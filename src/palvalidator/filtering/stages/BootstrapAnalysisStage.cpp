#include "BootstrapAnalysisStage.h"
#include "BiasCorrectedBootstrap.h"
#include "MOutOfNPercentileBootstrap.h"
#include "PercentileTBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"
#include "ClosedPositionHistory.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "ParallelExecutors.h"
#include "filtering/BootstrapConfig.h" 
#include "SmallNBootstrapHelpers.h"
#include <sstream>
#include <cmath>


namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::BackTester;
  using mkc_timeseries::ClosedPositionHistory;
  using mkc_timeseries::Security;
  using palvalidator::bootstrap_cfg::BootstrapFactory;
  
  BootstrapAnalysisStage::BootstrapAnalysisStage(const Num& confidenceLevel,
  				 unsigned int numResamples,
  				 BootstrapFactory& bootstrapFactory)
    : mConfidenceLevel(confidenceLevel)
    , mNumResamples(numResamples)
    , mBootstrapFactory(bootstrapFactory)
  {}

  size_t BootstrapAnalysisStage::computeBlockLength(const StrategyAnalysisContext& ctx) const
  {
    // 1) Median holding period (economic horizon)
    unsigned int medianHoldBars = 2;
    if (ctx.backtester)
      medianHoldBars = ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();

    // 2) n^(1/3) heuristic (statistical horizon), prefer already-prepared returns
    size_t n = ctx.highResReturns.size();
    size_t lCube = (n > 0) ? static_cast<size_t>(std::lround(std::pow(static_cast<double>(n), 1.0/3.0))) : 0;

    // 3) Hybrid: max of the two, with a minimum of 2
    size_t L = std::max<size_t>(2, std::max(static_cast<size_t>(medianHoldBars), lCube));

    const size_t L_cap = 12;
    L = std::min(L, L_cap);

    return L;
  }

  double BootstrapAnalysisStage::computeAnnualizationFactor(const StrategyAnalysisContext& ctx) const
  {
    // Mirror original logic: use intraday minutes when appropriate
    if (ctx.timeFrame == mkc_timeseries::TimeFrame::INTRADAY)
      {
	if (ctx.baseSecurity && ctx.baseSecurity->getTimeSeries())
	  {
	    return calculateAnnualizationFactor(ctx.timeFrame,
						ctx.baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes());
	  }
      }
    return calculateAnnualizationFactor(ctx.timeFrame);
  }

  BootstrapAnalysisResult
  BootstrapAnalysisStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    BootstrapAnalysisResult R;

    // --- Ensure backtester & high-res returns exist (defensive) ---
    if (!ctx.backtester) {
      try {
	if (!ctx.portfolio && ctx.strategy && ctx.baseSecurity) {
	  ctx.portfolio = std::make_shared<mkc_timeseries::Portfolio<Num>>(ctx.strategy->getStrategyName() + " Portfolio");
	  ctx.portfolio->addSecurity(ctx.baseSecurity);
	  ctx.clonedStrategy = ctx.strategy->clone2(ctx.portfolio);
	  ctx.backtester = mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(
										    ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
	  ctx.highResReturns = ctx.backtester->getAllHighResReturns(ctx.clonedStrategy.get());
	}
      } catch (const std::exception& e) {
	R.failureReason = std::string("Failed to initialize backtester: ") + e.what();
	os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
	return R;
      }
    }

    const std::size_t n = ctx.highResReturns.size();
    if (n < 2) {
      R.failureReason = "Insufficient returns (need at least 2, have " + std::to_string(n) + ")";
      os << "   [Bootstrap] Skipped (" << R.failureReason << ")\n";
      return R;
    }

    // --- Block length & basic diagnostics ---
    const std::size_t L = computeBlockLength(ctx);
    R.blockLength = L;

    const unsigned int medianHoldBars =
      ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
    R.medianHoldBars = medianHoldBars;
    os << "Strategy Median holding period = " << medianHoldBars << "\n";

    // --- Resampler types & BCa resampler (unchanged) ---
    using IIDResampler   = mkc_timeseries::IIDResampler<Num>;
    using BlockValueRes  = palvalidator::resampling::StationaryBlockValueResampler<Num>;
    using BCaResampler   = mkc_timeseries::StationaryBlockResampler<Num>;

    BCaResampler bcaResampler(L);

    // --- Choose SmallNResampler by a simple, stable sign-imbalance heuristic ---
    const std::size_t num_pos = std::count_if(ctx.highResReturns.begin(),
					      ctx.highResReturns.end(),
					      [](const Num& r){ return num::to_double(r) > 0.0; });
    const double ratio_pos = static_cast<double>(num_pos) / static_cast<double>(n);
    const bool use_block_smallN = (ratio_pos > 0.80) || (ratio_pos < 0.20);

    // Keep tiny L for individual M2M level; clamp to [2,3] and ≤ L
    const std::size_t L_small = std::max<std::size_t>(2, std::min<std::size_t>(3, L));

    IIDResampler  iidResampler;
    BlockValueRes blockResampler(L_small);

    const char* smallN_resampler_name = use_block_smallN
      ? "StationaryBlockValueResampler(small L)"
      : "IIDResampler";

    os << "   [Bootstrap] SmallNResampler=" << smallN_resampler_name
       << " (ratio_pos=" << ratio_pos << ", L_small=" << L_small << ")\n";

    // --- Moment diagnostics for routing ---
    const auto [skew, exkurt] =
      mkc_timeseries::StatUtils<Num>::computeSkewAndExcessKurtosis(ctx.highResReturns);
    const bool heavy_tails =
      mkc_timeseries::StatUtils<Num>::hasHeavyTails(ctx.highResReturns);

    // Baseline routing (pragmatic, conservative):
    bool run_mn  = (n <= 40);  // m-out-of-n for small n
    bool run_pt  = (n <= 80);  // percentile-t through medium n

    // Heavy-tail override: extend m-out-of-n to n<=60 when tails look risky
    if (!run_mn && n <= 60 && heavy_tails) run_mn = true;

    // For the tiniest samples, also run Percentile-t for a second look
    const bool run_pt_also_for_tiny = (n <= 24);
    if (run_pt_also_for_tiny) run_pt = true;

    os << "   [Bootstrap] n=" << n
       << "  skew=" << skew << "  exkurt=" << exkurt
       << "  heavy_tails=" << (heavy_tails ? "yes" : "no")
       << "  L=" << L << "\n";

    try {
      // ===== Baseline BCa (for comparability) =====
      std::function<Num(const std::vector<Num>&)> geoFn  = mkc_timeseries::GeoMeanStat<Num>();
      std::function<Num(const std::vector<Num>&)> meanFn = &mkc_timeseries::StatUtils<Num>::computeMean;

      auto bcaGeo = mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
						   mNumResamples,
						   mConfidenceLevel.getAsDouble(),
						   geoFn,
						   bcaResampler,
						   *ctx.clonedStrategy,
						   /*stage*/1, L, /*fold*/0);

      auto bcaMean = mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
						    mNumResamples,
						    mConfidenceLevel.getAsDouble(),
						    meanFn,
						    bcaResampler,
						    *ctx.clonedStrategy,
						    /*stage*/1, L, /*fold*/0);

      const Num lbGeo_BCa  = bcaGeo.getLowerBound();
      const Num lbMean_BCa = bcaMean.getLowerBound();

      // ===== Optional small-n engines =====
      Num lbGeo_mn = lbGeo_BCa;
      Num lbGeo_pt = lbGeo_BCa;

      if (run_mn)
	{
	  const double CL    = mConfidenceLevel.getAsDouble();
	  const double rho_m = 0.70; // m ≈ floor(n^0.70)

	  if (use_block_smallN)
	    {
	      auto [mnBoot, mnCrn] =
		mBootstrapFactory.template makeMOutOfN<Num,
						       mkc_timeseries::GeoMeanStat<Num>,
						       BlockValueRes>(
								      mNumResamples,
								      CL,
								      rho_m,
								      blockResampler,
								      *ctx.clonedStrategy,
								      /*stage*/1, L, /*fold*/0);

	      auto mnRes = mnBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), mnCrn);
	      lbGeo_mn = mnRes.lower;

	      os << "   [Bootstrap] m-out-of-n percentile:"
		 << "  resampler=" << smallN_resampler_name
		 << "  m_sub=" << mnRes.m_sub
		 << "  L="     << mnRes.L
		 << "  effB="  << mnRes.effective_B
		 << "  LB="    << lbGeo_mn << "\n";
	    }
	  else
	    {
	      auto [mnBoot, mnCrn] =
		mBootstrapFactory.template makeMOutOfN<Num,
						       mkc_timeseries::GeoMeanStat<Num>,
						       IIDResampler>(
								     mNumResamples,
								     CL,
								     rho_m,
								     iidResampler,
								     *ctx.clonedStrategy,
								     /*stage*/1, L, /*fold*/0);

	      auto mnRes = mnBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), mnCrn);
	      lbGeo_mn = mnRes.lower;

	      os << "   [Bootstrap] m-out-of-n percentile:"
		 << "  resampler=" << smallN_resampler_name
		 << "  m_sub=" << mnRes.m_sub
		 << "  L="     << mnRes.L
		 << "  effB="  << mnRes.effective_B
		 << "  LB="    << lbGeo_mn << "\n";
	    }
	}

      if (run_pt)
	{
	  const std::size_t B_out = std::max<std::size_t>(mNumResamples, 400);
	  const std::size_t B_in  = std::max<std::size_t>(mNumResamples / 5, 100ul);
	  const double rho_o = 1.0;                     // outer uses full size
	  const double rho_i = (n <= 24) ? 0.85 : 0.95; // smaller inner at tiny n

	  using PTExec = concurrency::ThreadPoolExecutor<0>;

	  if (use_block_smallN)
	    {
	      auto [ptBoot, ptCrn] =
		mBootstrapFactory.template makePercentileT<Num,
							   mkc_timeseries::GeoMeanStat<Num>,
							   BlockValueRes,
							   PTExec>(
								   B_out,
								   B_in,
								   mConfidenceLevel.getAsDouble(),
								   blockResampler,
								   *ctx.clonedStrategy,
								   /*stage*/1, L, /*fold*/0,
								   rho_o, rho_i);

	      auto ptRes = ptBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), ptCrn);
	      lbGeo_pt = ptRes.lower;

	      os << "   [Bootstrap] Percentile-t:"
		 << "  resampler=" << smallN_resampler_name
		 << "  m_outer=" << ptRes.m_outer
		 << "  m_inner=" << ptRes.m_inner
		 << "  L="       << ptRes.L
		 << "  effB="    << ptRes.effective_B
		 << "  LB="      << lbGeo_pt << "\n";
	    }
	  else
	    {
	      auto [ptBoot, ptCrn] =
		mBootstrapFactory.template makePercentileT<Num,
							   mkc_timeseries::GeoMeanStat<Num>,
							   IIDResampler,
							   PTExec>(
								   B_out,
								   B_in,
								   mConfidenceLevel.getAsDouble(),
								   iidResampler,
								   *ctx.clonedStrategy,
								   /*stage*/1, L, /*fold*/0,
								   rho_o, rho_i);

	      auto ptRes = ptBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), ptCrn);
	      lbGeo_pt = ptRes.lower;

	      os << "   [Bootstrap] Percentile-t:"
		 << "  resampler=" << smallN_resampler_name
		 << "  m_outer=" << ptRes.m_outer
		 << "  m_inner=" << ptRes.m_inner
		 << "  L="       << ptRes.L
		 << "  effB="    << ptRes.effective_B
		 << "  LB="      << lbGeo_pt << "\n";
	    }
	}

      // ===== Conservative combination =====
      Num lbGeo_conservative = lbGeo_BCa;
      if (run_mn)
	lbGeo_conservative = std::min(lbGeo_conservative, lbGeo_mn);

      if (run_pt)
	lbGeo_conservative = std::min(lbGeo_conservative, lbGeo_pt);

      R.lbGeoPeriod  = lbGeo_conservative;
      R.lbMeanPeriod = lbMean_BCa;

      // ===== Annualization =====
      const double annFactor = computeAnnualizationFactor(ctx);
      mkc_timeseries::BCaAnnualizer<Num> annualizerMean_BCa(bcaMean, annFactor);

      using A = mkc_timeseries::Annualizer<Num>;
      const Num annGeo_conservative = A::annualize_one(R.lbGeoPeriod, annFactor);

      R.annualizedLowerBoundGeo  = annGeo_conservative;
      R.annualizedLowerBoundMean = annualizerMean_BCa.getAnnualizedLowerBound();

      // ===== Policy/diagnostics =====
      {
	std::ostringstream pol;
	pol << "policy: BCa";
	if (run_mn) pol << " ∧ m/n";
	if (run_pt) pol << " ∧ percentile-t";
	pol << "  | n=" << n << " L=" << L
	    << "  skew=" << skew << " exkurt=" << exkurt
	    << "  heavy_tails=" << (heavy_tails ? "yes" : "no")
	    << "  SmallNResampler=" << smallN_resampler_name
	    << "  ratio_pos=" << ratio_pos << " L_small=" << L_small;
	R.gatePolicy = pol.str();
      }

      os << "   [Bootstrap] Annualization factor = " << annFactor << "\n";
      os << "   [Bootstrap] Conservative LB construction policy = " << R.gatePolicy << "\n";

      R.computationSucceeded = true;
      return R;
    }
    catch (const std::exception& e) {
      R.failureReason = std::string("Bootstrap computation failed: ") + e.what();
      os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
      return R;
    }
  }
} // namespace palvalidator::filtering::stages
