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
    using mkc_timeseries::DecimalConstants;
    using mkc_timeseries::StatUtils;
    using palvalidator::bootstrap_helpers::conservative_smallN_lower_bound;
    using palvalidator::bootstrap_helpers::dispatch_smallN_resampler;
    using palvalidator::bootstrap_helpers::MNRunSimple;
    using palvalidator::bootstrap_helpers::PTRunSimple;
    namespace bh = palvalidator::bootstrap_helpers;
    namespace bhi = palvalidator::bootstrap_helpers::internal;

    BootstrapAnalysisResult R;

    // --- Ensure backtester & high-res returns exist (defensive) ----------------
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

    // --- Block length (economic/statistical hybrid) & annualization ------------
    const std::size_t L = computeBlockLength(ctx);
    R.blockLength = L;

    const unsigned int medianHoldBars =
      ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
    R.medianHoldBars = medianHoldBars;
    os << "Strategy Median holding period = " << medianHoldBars << "\n";

    const double annFactor = computeAnnualizationFactor(ctx);

    // --- Moments / diagnostics --------------------------------------------------
    const auto [skew, exkurt] = StatUtils<Num>::computeSkewAndExcessKurtosis(ctx.highResReturns);
    const bool heavy_tails    = palvalidator::bootstrap_helpers::has_heavy_tails_wide(skew, exkurt);

    // Routing (pragmatic): m/n for small N; percentile-t through medium N; extend m/n if heavy tails.
    bool run_mn = (n <= 40);
    bool run_pt = (n <= 80);
    if (!run_mn && n <= 60 && heavy_tails) run_mn = true;
    if (n <= 24) run_pt = true; // tiny n → keep t as a second view

    os << "   [Bootstrap] n=" << n
       << "  skew=" << skew << "  exkurt=" << exkurt
       << "  heavy_tails=" << (heavy_tails ? "yes" : "no")
       << "  L=" << L << "\n";

    try {
      // =========================================================================
      // 1) CONSERVATIVE SMALL-N ENGINE (min of m/n & BCa on the SAME resampler)
      //    Uses SmallNBootstrapHelpers to select IID vs Block, choose m (≈0.8n),
      //    run both engines, and return per-period and annualized LBs.
      // =========================================================================
      const double CL = mConfidenceLevel.getAsDouble();

      // We always compute the conservative small-N LB if run_mn is true; if false,
      // we still compute BCa-only below for mean/geo comparability.
      Num lbGeo_smallN_per = DecimalConstants<Num>::DecimalZero;
      Num lbGeo_smallN_ann = DecimalConstants<Num>::DecimalZero;
      std::string smallN_resampler = "n/a";
      std::size_t smallN_m_sub = 0, smallN_L_used = 0, smallN_effB_mn = 0;

      if (run_mn) {
	auto smallN = conservative_smallN_lower_bound<Num, mkc_timeseries::GeoMeanStat<Num>>(
											     ctx.highResReturns,
											     L,
											     annFactor,
											     CL,
											     mNumResamples,
											     /*rho_m<=0 → auto*/ -1.0,
											     *ctx.clonedStrategy,
											     mBootstrapFactory,
											     &os, /*stage*/1, /*fold*/0);

	lbGeo_smallN_per = smallN.per_lower;
	lbGeo_smallN_ann = smallN.ann_lower;
	smallN_resampler = smallN.resampler_name ? smallN.resampler_name : "n/a";
	smallN_m_sub     = smallN.m_sub;
	smallN_L_used    = smallN.L_used;
	smallN_effB_mn   = smallN.effB_mn;

	os << "   [Bootstrap] m-out-of-n ∧ BCa (conservative small-N):"
	   << "  resampler=" << smallN_resampler
	   << "  m_sub="     << smallN_m_sub
	   << "  L="         << smallN_L_used
	   << "  effB(mn)="  << smallN_effB_mn
	   << "  LB(per)="   << lbGeo_smallN_per
	   << "  LB(ann)="   << lbGeo_smallN_ann << "\n";
      }

      // =========================================================================
      // 2) Percentile-t (optional second view; same resampler dispatch)
      // =========================================================================
      Num lbGeo_pt_per = DecimalConstants<Num>::DecimalZero;
      bool have_pt = false;

      if (run_pt) {
	using PTExec = concurrency::ThreadPoolExecutor<0>;
	const std::size_t B_out = std::max<std::size_t>(mNumResamples, 400);
	const std::size_t B_in  = std::max<std::size_t>(mNumResamples / 5, 100ul);
	const double rho_o = 1.0;                       // outer uses full size
	const double rho_i = (n <= 24) ? 0.85 : 0.95;   // inner smaller at tiny n

	const char* chosen_name = nullptr;
	std::size_t L_small = 0;

	auto ptRes = dispatch_smallN_resampler(
					       ctx.highResReturns, L,
					       [&](auto& resampler, double, bool, std::size_t) {
						 using ResamplerT = std::decay_t<decltype(resampler)>;
						 auto [ptBoot, ptCrn] =
						   mBootstrapFactory.template makePercentileT<Num,
											      mkc_timeseries::GeoMeanStat<Num>,
											      ResamplerT,
											      PTExec>(
												      B_out, B_in, CL, resampler, *ctx.clonedStrategy,
												      /*stage*/1, L, /*fold*/0, rho_o, rho_i);
						 auto r = ptBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), ptCrn);
						 PTRunSimple<Num> out;
						 out.lower       = r.lower;
						 out.m_outer     = r.m_outer;
						 out.m_inner     = r.m_inner;
						 out.L           = r.L;
						 out.effective_B = r.effective_B;
						 return out;
					       },
					       &chosen_name, &L_small);

	lbGeo_pt_per = ptRes.lower;
	have_pt = true;

	os << "   [Bootstrap] Percentile-t:"
	   << "  resampler=" << (chosen_name ? chosen_name : "n/a")
	   << "  m_outer="   << ptRes.m_outer
	   << "  m_inner="   << ptRes.m_inner
	   << "  L="         << ptRes.L
	   << "  effB="      << ptRes.effective_B
	   << "  LB="        << lbGeo_pt_per << "\n";
      }

      // =========================================================================
      // 3) BCa (mean) for reporting/compatibility; also get mean annualized LB
      //    (Use stationary block resampler with full L for BCa mean.)
      // =========================================================================
      using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
      BCaResampler bcaResampler(L);

      std::function<Num(const std::vector<Num>&)> geoFn  = mkc_timeseries::GeoMeanStat<Num>();
      std::function<Num(const std::vector<Num>&)> meanFn = &mkc_timeseries::StatUtils<Num>::computeMean;

      auto bcaMean = mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
						    mNumResamples,
						    CL,
						    meanFn,
						    bcaResampler,
						    *ctx.clonedStrategy,
						    /*stage*/1, L, /*fold*/0);

      const Num lbMean_BCa = bcaMean.getLowerBound();
      R.lbMeanPeriod = lbMean_BCa;
      R.annualizedLowerBoundMean = mkc_timeseries::BCaAnnualizer<Num>(bcaMean, annFactor).getAnnualizedLowerBound();

      // =========================================================================
      // 4) Combine per-period GEO LBs:
      //    - If we ran small-N conservative only: that’s the GEO per-period LB.
      //    - If we also ran t: combine {small-N-conservative, pt} via median-of-present.
      // =========================================================================
      constexpr bool kUseVoteOfTwoMedian = true; // set false to fall back to strict min

      std::vector<Num> geo_parts;
      const bool have_smallN = run_mn; // we computed it only when run_mn=true
      if (have_smallN)
	geo_parts.push_back(lbGeo_smallN_per);
      
      if (have_pt)
	geo_parts.push_back(lbGeo_pt_per);

      // Neutral, hurdle-agnostic combine INSIDE the stage:
      // - If both present → median-of-present
      // - If one present → that one
      // - Else → fall back to BCa(geo) you already compute
      auto median_of_present = [](std::vector<Num> v)->Num {
	if (v.empty()) return Num(0);
	if (v.size() == 1) return v[0];
	std::sort(v.begin(), v.end(), [](const Num& a, const Num& b){ return a < b; });
	if (v.size() == 2) return v[0] + (v[1] - v[0]) / Num(2);
	return v[1]; // size()==3
      };

      Num lbGeoPer_neutral;
      if (!geo_parts.empty())
	lbGeoPer_neutral = median_of_present(geo_parts);
      else
	// fallback: if no smallN/t parts were produced, use your BCa(geo) per-period LB (already computed)
	lbGeoPer_neutral = R.lbGeoPeriod; // or whichever variable holds BCa(geo) per-period LB
     
      // Publish the neutral pick as the stage’s GEO result
      R.lbGeoPeriod              = lbGeoPer_neutral;
      
      // Annualize chosen GEO LB
      R.annualizedLowerBoundGeo = mkc_timeseries::Annualizer<Num>::annualize_one(R.lbGeoPeriod, annFactor);

      // Also publish the parts for downstream near-hurdle refinement
      R.lbGeoSmallNPeriod = run_mn ? std::optional<Num>(lbGeo_smallN_per) : std::nullopt;
      R.lbGeoPTPeriod     = run_pt ? std::optional<Num>(lbGeo_pt_per)     : std::nullopt;
      R.annFactorUsed     = annFactor;

      // =========================================================================
      // 5) Log chosen policy line (shared helper) and finish
      // =========================================================================
      const char* policyLabel = nullptr;
      if (have_smallN && have_pt) {
	policyLabel = kUseVoteOfTwoMedian ? "smallN(min of m/n,BCa) ⊕ percentile-t (median of present)"
	  : "min( smallN(min of m/n,BCa), percentile-t )";
      } else if (have_smallN) {
	policyLabel = "smallN(min of m/n,BCa)";
      } else if (have_pt) {
	policyLabel = "percentile-t (geo)";
      } else {
	policyLabel = "BCa (fallback)";
      }

      // For logging, prefer the small-N resampler name if available.
      const char* resamplerNameForLog = have_smallN ? smallN_resampler.c_str() : "StationaryBlockResampler";

      bhi::log_policy_line(os, policyLabel, n, L, skew, exkurt, heavy_tails,
			   resamplerNameForLog, std::min<std::size_t>(L, 3));

      os << "   [Bootstrap] Annualization factor = " << annFactor << "\n";

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
