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
#include <sstream>
#include <cmath>


namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::BackTester;
  using mkc_timeseries::ClosedPositionHistory;
  using mkc_timeseries::Security;

  BootstrapAnalysisStage::BootstrapAnalysisStage(const Num& confidenceLevel, unsigned int numResamples)
    : mConfidenceLevel(confidenceLevel)
    , mNumResamples(numResamples)
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

  BootstrapAnalysisResult BootstrapAnalysisStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    BootstrapAnalysisResult R;

    // Ensure backtest & high-res returns exist (defensive)
    if (!ctx.backtester)
      {
	try
	  {
	    if (!ctx.portfolio && ctx.strategy && ctx.baseSecurity)
	      {
		ctx.portfolio = std::make_shared<mkc_timeseries::Portfolio<Num>>(ctx.strategy->getStrategyName() + " Portfolio");
		ctx.portfolio->addSecurity(ctx.baseSecurity);
		ctx.clonedStrategy = ctx.strategy->clone2(ctx.portfolio);
		ctx.backtester = mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(
											  ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
		ctx.highResReturns = ctx.backtester->getAllHighResReturns(ctx.clonedStrategy.get());
	      }
	  }
	catch (const std::exception& e)
	  {
	    R.failureReason = std::string("Failed to initialize backtester: ") + e.what();
	    os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
	    return R;
	  }
      }

    const std::size_t n = ctx.highResReturns.size();
    if (n < 2)
      {
	R.failureReason = "Insufficient returns (need at least 2, have " + std::to_string(n) + ")";
	os << "   [Bootstrap] Skipped (" << R.failureReason << ")\n";
	return R;
      }

    // Block length & basic diagnostics
    const std::size_t L = computeBlockLength(ctx);
    R.blockLength = L;

    const unsigned int medianHoldBars = ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
    R.medianHoldBars = medianHoldBars;
    os << "Strategy Median holding period = " << medianHoldBars << "\n";

    // Use the mask-based stationary resamplers for everything
    using MaskValueResampler = palvalidator::resampling::StationaryMaskValueResampler<Num>;
    using BCaResampler       = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    BCaResampler       bcaResampler(L);
    MaskValueResampler smallNResampler(L);

    // Statistic samplers & helpers
    mkc_timeseries::GeoMeanStat<Num> geoStat; // log-aware; ruin/winsor handling inside
    using BlockBCA = mkc_timeseries::BCaBootStrap<Num, BCaResampler>;
    randutils::mt19937_rng rng;

    try
      {
	// ===== 1) Primary BCa on GeoMean and Mean =====
	BlockBCA bcaGeo(
			ctx.highResReturns,
			mNumResamples,
			mConfidenceLevel.getAsDouble(),
			geoStat,
			bcaResampler);

	BlockBCA bcaMean(
			 ctx.highResReturns,
			 mNumResamples,
			 mConfidenceLevel.getAsDouble(),
			 &mkc_timeseries::StatUtils<Num>::computeMean,
			 bcaResampler);

	const Num lbGeo_BCa  = bcaGeo.getLowerBound();
	const Num lbMean_BCa = bcaMean.getLowerBound();

	// ===== 2) Small-n fallbacks (computed, but gate happens in pipeline) =====
	const bool run_pt = (n <= 29);
	const bool run_mn = (n <= 24);

	Num lbGeo_pt = lbGeo_BCa;
	Num lbGeo_mn = lbGeo_BCa;

	if (run_mn)
	  {
	    const double      CL     = mConfidenceLevel.getAsDouble();
	    const double      rho_m  = 0.70; // m ≈ floor(0.7 n)
	    const std::size_t B      = std::max<std::size_t>(mNumResamples, 400);

	    palvalidator::analysis::MOutOfNPercentileBootstrap<
	      Num,
	      mkc_timeseries::GeoMeanStat<Num>,
	      MaskValueResampler
	      > mn(B, CL, rho_m, smallNResampler);

	    auto mnRes = mn.run(ctx.highResReturns, geoStat, rng);
	    lbGeo_mn   = mnRes.lower;

	    os << "   [Bootstrap] m-out-of-n percentile: "
	       << "m_sub=" << mnRes.m_sub
	       << " L="    << mnRes.L
	       << " effB=" << mnRes.effective_B
	       << " LB="   << lbGeo_mn << "\n";
	  }

	if (run_pt)
	  {
	    const double      CL    = mConfidenceLevel.getAsDouble();
	    const std::size_t B_out = std::max<std::size_t>(mNumResamples, 400);
	    const std::size_t B_in  = std::max<std::size_t>(mNumResamples / 5, 100ul);
	    const double      rho_o = 1.0;                     // full-size outer
	    const double      rho_i = (n <= 24) ? 0.85 : 0.95; // slightly smaller inner at tiny n

	    palvalidator::analysis::PercentileTBootstrap<
	      Num,
	      mkc_timeseries::GeoMeanStat<Num>,
	      MaskValueResampler
	      > pT(B_out, B_in, CL, smallNResampler, rho_o, rho_i);

	    auto ptRes = pT.run(ctx.highResReturns, geoStat, rng);
	    lbGeo_pt   = ptRes.lower;

	    os << "   [Bootstrap] Percentile-t: "
	       << "m_outer=" << ptRes.m_outer
	       << " m_inner=" << ptRes.m_inner
	       << " L="       << ptRes.L
	       << " effB="    << ptRes.effective_B
	       << " LB="      << lbGeo_pt << "\n";
	  }

	// ===== 3) Per-period reporting =====
	// Conservative Geo LB = min of available (BCa; plus pt and/or m/n when enabled)
	Num lbGeo_conservative = lbGeo_BCa;
	if (run_mn) { lbGeo_conservative = std::min(lbGeo_conservative, lbGeo_mn); }
	if (run_pt) { lbGeo_conservative = std::min(lbGeo_conservative, lbGeo_pt); }

	R.lbGeoPeriod  = lbGeo_conservative;
	R.lbMeanPeriod = lbMean_BCa;

	// ===== 4) Annualize bounds for reporting =====
	const double annFactor = computeAnnualizationFactor(ctx);

	// Annualized mean LB from BCa wrapper (kept as-is)
	mkc_timeseries::BCaAnnualizer<Num> annualizerMean_BCa(bcaMean, annFactor);

	// Use the new generic Annualizer for all single-value transforms
	using A = mkc_timeseries::Annualizer<Num>;

	const Num annGeo_BCa          = A::annualize_one(lbGeo_BCa,         annFactor);
	const Num annGeo_mn           = run_mn ? A::annualize_one(lbGeo_mn, annFactor) : annGeo_BCa;
	const Num annGeo_pt           = run_pt ? A::annualize_one(lbGeo_pt, annFactor) : annGeo_BCa;
	const Num annGeo_conservative = A::annualize_one(R.lbGeoPeriod,     annFactor);

	R.annualizedLowerBoundGeo  = annGeo_conservative;
	R.annualizedLowerBoundMean = annualizerMean_BCa.getAnnualizedLowerBound();

	// ===== 5) No gate here — pipeline performs the single gate after Hurdle =====
	R.gatePassedHurdle   = false;   // not evaluated here
	R.gateIsAnnualized   = true;    // we report annualized LBs
	R.gateComparedLB     = Num(0);  // not set here
	R.gateComparedHurdle = Num(0);  // not set here
	R.gatePolicy.clear();
	if (n <= 24)
	  {
	    R.gatePolicy = "conservative LB built from BCa ∧ (m/n, t) components (<=24)";
	  }
	else if (run_pt)
	  {
	    R.gatePolicy = "conservative LB = min(BCa, t) (25..29)";
	  }
	else
	  {
	    R.gatePolicy = "conservative LB from BCa only (>=30)";
	  }

	// Success
	R.computationSucceeded = true;

	// ===== 6) Diagnostics =====
	os << "   [Bootstrap] Per-period bounds: "
	   << "GeoMean (BCa)=" << lbGeo_BCa;
	if (run_mn) { os << ", GeoMean (m/n)=" << lbGeo_mn; }
	if (run_pt) { os << ", GeoMean (t)="   << lbGeo_pt; }
	os << ", Mean (BCa)=" << lbMean_BCa << "\n";

	os << "   [Bootstrap] Annualization factor = " << annFactor << "\n";

	os << "   [Bootstrap] Annualized bounds: "
	   << "GeoMean (BCa)=" << annGeo_BCa;
	if (run_mn) { os << ", GeoMean (m/n)=" << annGeo_mn; }
	if (run_pt) { os << ", GeoMean (t)="   << annGeo_pt; }
	os << ", GeoMean (conservative)=" << annGeo_conservative
	   << ", Mean (BCa)=" << R.annualizedLowerBoundMean << "\n";

	os << "   [Bootstrap] Conservative LB construction policy = " << R.gatePolicy << "\n";

	return R;
      }
    catch (const std::exception& e)
      {
	R.failureReason = std::string("Bootstrap computation failed: ") + e.what();
	os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
	// computationSucceeded remains false
	return R;
      }
  }
} // namespace palvalidator::filtering::stages
