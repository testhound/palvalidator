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
#include "Annualizer.h"
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

  bool BootstrapAnalysisStage::initializeBacktester(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    if (ctx.backtester)
    {
      return true;
    }

    try
    {
      if (!ctx.portfolio && ctx.strategy && ctx.baseSecurity)
      {
        ctx.portfolio = std::make_shared<mkc_timeseries::Portfolio<Num>>(
          ctx.strategy->getStrategyName() + " Portfolio");
        ctx.portfolio->addSecurity(ctx.baseSecurity);
        ctx.clonedStrategy = ctx.strategy->clone2(ctx.portfolio);
        ctx.backtester = mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(
          ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
        ctx.highResReturns = ctx.backtester->getAllHighResReturns(ctx.clonedStrategy.get());
      }
      return true;
    }
    catch (const std::exception& e)
    {
      os << "Warning: BootstrapAnalysisStage failed to initialize backtester: " << e.what() << "\n";
      return false;
    }
  }

  BootstrapAnalysisStage::DistributionDiagnostics
  BootstrapAnalysisStage::analyzeDistribution(const StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    using mkc_timeseries::StatUtils;
    using palvalidator::bootstrap_helpers::has_heavy_tails_wide;

    const std::size_t n = ctx.highResReturns.size();
    const auto [skew, exkurt] = StatUtils<Num>::computeSkewAndExcessKurtosis(ctx.highResReturns);
    const bool heavy_tails = has_heavy_tails_wide(skew, exkurt);

    // Routing (pragmatic): m/n for small N; percentile-t through medium N; extend m/n if heavy tails.
    bool run_mn = palvalidator::bootstrap_helpers::should_run_smallN(n, heavy_tails);
    bool run_pt = (n <= 80);
    if (!run_mn && n <= 60 && heavy_tails) run_mn = true;
    if (n <= 24) run_pt = true; // tiny n → keep t as a second view

    os << "   [Bootstrap] n=" << n
       << "  skew=" << skew << "  exkurt=" << exkurt
       << "  heavy_tails=" << (heavy_tails ? "yes" : "no") << "\n";

    return DistributionDiagnostics(skew, exkurt, heavy_tails, run_mn, run_pt);
  }

  std::optional<BootstrapAnalysisStage::SmallNResult>
  BootstrapAnalysisStage::runSmallNBootstrap(const StrategyAnalysisContext& ctx,
                                              double confidenceLevel,
                                              double annualizationFactor,
                                              size_t blockLength,
                                              bool heavyTails,
                                              std::ostream& os) const
  {
    using palvalidator::bootstrap_helpers::conservative_smallN_lower_bound;
    using mkc_timeseries::DecimalConstants;

    // If heavy tails detected, force block resampler; otherwise let heuristics/MC guard decide.
    std::optional<bool> heavy_override = heavyTails ? std::optional<bool>(true) : std::nullopt;
    
    auto smallN = conservative_smallN_lower_bound<Num, mkc_timeseries::GeoMeanStat<Num>>(
      ctx.highResReturns,
      blockLength,
      annualizationFactor,
      confidenceLevel,
      mNumResamples,
      /*rho_m<=0 → auto*/ -1.0,
      *ctx.clonedStrategy,
      mBootstrapFactory,
      &os, /*stage*/1, /*fold*/0,
      /*heavy_tails_override=*/heavy_override);

    os << "   [Bootstrap] m-out-of-n ∧ BCa (conservative small-N):"
       << "  resampler=" << (smallN.resampler_name ? smallN.resampler_name : "n/a")
       << "  m_sub=" << smallN.m_sub
       << "  L=" << smallN.L_used
       << "  effB(mn)=" << smallN.effB_mn
       << "  LB(per)=" << smallN.per_lower
       << "  LB(ann)=" << smallN.ann_lower << "\n";

    return SmallNResult(
      smallN.per_lower,
      smallN.ann_lower,
      smallN.resampler_name ? smallN.resampler_name : "n/a",
      smallN.m_sub,
      smallN.L_used,
      smallN.effB_mn);
  }

  std::optional<BootstrapAnalysisStage::PercentileTResult>
  BootstrapAnalysisStage::runPercentileTBootstrap(const StrategyAnalysisContext& ctx,
						  double confidenceLevel,
						  size_t blockLength,
						  std::ostream& os) const
  {
    using PTExec = concurrency::ThreadPoolExecutor<0>;
    using ResamplerT = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    /*
      --------------------------------------------------------------------------------
      Why we run the Percentile-t Bootstrap (in addition to m-out-of-n and BCa)
      --------------------------------------------------------------------------------

      The percentile-t bootstrap provides a variance-stabilized benchmark for cases
      where the statistic of interest (e.g., geometric mean of mark-to-market returns)
      is skewed or exhibits heteroscedasticity.  It complements the other bootstrap
      methods rather than replacing them.

      Summary of roles:
      
      • m-out-of-n Bootstrap:
      - Very conservative and theoretically robust for small sample sizes (small n)
      or heavy-tailed data.
      - Protects against non-normality and dependent samples.
      - Limitation: often produces overly wide confidence intervals.

      • BCa Bootstrap (Bias-Corrected and Accelerated):
      - Corrects both bias and skewness in smooth, nearly normal statistics.
      - Works best with moderate-to-large n.
      - Limitation: requires accurate jackknife acceleration, which can be unstable
        for small samples or fat-tailed data.
	
	• Percentile-t Bootstrap:
      - "Studentizes" each bootstrap replicate by its estimated standard error.
      - Stabilizes variance and improves coverage accuracy for moderate n.
      - Bridges the gap between conservative m-out-of-n and efficient BCa.
      - Performs well with 5–15 trades or medium-size samples with mild skew.

      In BootstrapAnalysisStage we run all three methods because:
      1. m-out-of-n + BCa handle the extremes (small n, heavy tails, large n, smooth).
      2. Percentile-t acts as a precision cross-check for the middle regime.
      3. The final lower-bound estimate is derived by combining their results
      (typically the median of present bounds) to reduce sensitivity to any one
     method’s bias.
     
     In short:
     Percentile-t is the “bridge” method that validates and stabilizes inference
     when neither purely robust (m-out-of-n) nor purely efficient (BCa) assumptions
     hold—ensuring our bootstrap-based lower bounds remain both conservative and
     statistically meaningful across all sample sizes.
     --------------------------------------------------------------------------------
    */

    const std::size_t n     = ctx.highResReturns.size();
    const std::size_t B_out = std::max<std::size_t>(mNumResamples, 400);
    const std::size_t B_in  = std::max<std::size_t>(mNumResamples / 5, 100ul);
    const double rho_o = 1.0;
    const double rho_i = (n <= 24) ? 0.85 : 0.95;

    ResamplerT resampler(blockLength);
    auto [ptBoot, ptCrn] =
      mBootstrapFactory.template makePercentileT<Num,
                                                 mkc_timeseries::GeoMeanStat<Num>,
                                                 ResamplerT,
                                                 PTExec>(
							 B_out, B_in, confidenceLevel, resampler, *ctx.clonedStrategy,
							 /*stage*/1, blockLength, /*fold*/0, rho_o, rho_i);

    auto r = ptBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), ptCrn);

    os << "   [Bootstrap] Percentile-t:"
       << "  resampler=StationaryBlockResampler"
       << "  m_outer=" << r.m_outer
       << "  m_inner=" << r.m_inner
       << "  L=" << r.L
       << "  effB=" << r.effective_B
       << "  LB=" << r.lower << "\n";

    return PercentileTResult(r.lower, "StationaryBlockResampler",
			     r.m_outer, r.m_inner, r.L, r.effective_B);
  }
  
  BootstrapAnalysisStage::BCaMeanResult
  BootstrapAnalysisStage::runBCaMeanBootstrap(const StrategyAnalysisContext& ctx,
                                               double confidenceLevel,
                                               double annualizationFactor,
                                               size_t blockLength,
                                               std::ostream& os) const
  {
    using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
    BCaResampler bcaResampler(blockLength);

    std::function<Num(const std::vector<Num>&)> meanFn = &mkc_timeseries::StatUtils<Num>::computeMean;

    auto bcaMean = mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
                                                  mNumResamples,
                                                  confidenceLevel,
                                                  meanFn,
                                                  bcaResampler,
                                                  *ctx.clonedStrategy,
                                                  /*stage*/1, blockLength, /*fold*/0);

    const Num lbMean_BCa = bcaMean.getLowerBound();
    const Num annualizedLB = mkc_timeseries::BCaAnnualizer<Num>(bcaMean, annualizationFactor)
                               .getAnnualizedLowerBound();

    return BCaMeanResult(lbMean_BCa, annualizedLB);
  }

  Num BootstrapAnalysisStage::combineGeometricLowerBounds(
    const std::optional<SmallNResult>& smallN,
    const std::optional<PercentileTResult>& percentileT,
    std::ostream& os) const
  {
    using mkc_timeseries::DecimalConstants;

    // Neutral, hurdle-agnostic combine INSIDE the stage:
    // - If both present → median-of-present
    // - If one present → that one
    // - Else → fall back to zero (caller should handle this case)
    auto median_of_present = [](std::vector<Num> v) -> Num {
      if (v.empty()) return Num(0);
      if (v.size() == 1) return v[0];
      std::sort(v.begin(), v.end(), [](const Num& a, const Num& b) { return a < b; });
      if (v.size() == 2) return v[0] + (v[1] - v[0]) / Num(2);
      return v[1]; // size()==3
    };

    std::vector<Num> geo_parts;
    if (smallN.has_value())
      geo_parts.push_back(smallN->getLowerBoundPeriod());
    
    if (percentileT.has_value())
      geo_parts.push_back(percentileT->getLowerBoundPeriod());

    return median_of_present(geo_parts);
  }

  void BootstrapAnalysisStage::logFinalPolicy(
    const std::optional<SmallNResult>& smallN,
    const std::optional<PercentileTResult>& percentileT,
    size_t n,
    size_t blockLength,
    double skew,
    double excessKurtosis,
    bool heavyTails,
    std::ostream& os) const
  {
    namespace bhi = palvalidator::bootstrap_helpers::internal;

    constexpr bool kUseVoteOfTwoMedian = true;
    const char* policyLabel = nullptr;
    
    if (smallN.has_value() && percentileT.has_value())
    {
      policyLabel = kUseVoteOfTwoMedian 
        ? "smallN(min of m/n,BCa) ⊕ percentile-t (median of present)"
        : "min( smallN(min of m/n,BCa), percentile-t )";
    }
    else if (smallN.has_value())
    {
      policyLabel = "smallN(min of m/n,BCa)";
    }
    else if (percentileT.has_value())
    {
      policyLabel = "percentile-t (geo)";
    }
    else
    {
      policyLabel = "BCa (fallback)";
    }

    // For logging, prefer the small-N resampler name if available.
    const char* resamplerNameForLog = smallN.has_value() 
      ? smallN->getResamplerName().c_str() 
      : "StationaryBlockResampler";

    bhi::log_policy_line(os, policyLabel, n, blockLength, skew, excessKurtosis, 
                         heavyTails, resamplerNameForLog, std::min<std::size_t>(blockLength, 3));
  }

  BootstrapAnalysisResult
  BootstrapAnalysisStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    using mkc_timeseries::DecimalConstants;

    BootstrapAnalysisResult R;

    // --- Step 1: Ensure backtester & high-res returns exist ----------------
    if (!initializeBacktester(ctx, os))
      {
	R.failureReason = "Failed to initialize backtester";
	return R;
      }

    const std::size_t n = ctx.highResReturns.size();
    if (n < 2)
      {
	R.failureReason = "Insufficient returns (need at least 2, have " + std::to_string(n) + ")";
	os << "   [Bootstrap] Skipped (" << R.failureReason << ")\n";
	return R;
      }

    // --- Step 2: Compute block length and *bars/year* annualization --------
    const std::size_t L = computeBlockLength(ctx);
    R.blockLength = L;

    const unsigned int medianHoldBars =
      ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
    R.medianHoldBars = medianHoldBars;
    os << "Strategy Median holding period = " << medianHoldBars << "\n";

    // Base calendar factor (e.g., 252 daily, etc.) retained for fallback only
    const double baseAnnFactor = computeAnnualizationFactor(ctx);

    // λ = trades/year from the backtester (preferred)
    double lambdaTradesPerYear = 0.0;
    if (ctx.backtester) {
      try {
	lambdaTradesPerYear = ctx.backtester->getEstimatedAnnualizedTrades();
      } catch (...) {
	lambdaTradesPerYear = 0.0;
      }
    }

    // New: Annualize M2M bar statistics using bars/year = λ × medianHoldBars.
    // If λ is unavailable or medianHoldBars == 0, fall back to base calendar factor.
    double barsPerYear = lambdaTradesPerYear * static_cast<double>(medianHoldBars);
    if (!(barsPerYear > 0.0)) {
      barsPerYear = baseAnnFactor;
      os << "   [Bootstrap] Warning: trades/year (λ) or medianHoldBars unavailable; "
	"falling back to base calendar factor = " << baseAnnFactor << "\n";
    }

    // Publish for downstream stages (interpretation: *bars/year* on M2M series)
    R.annFactorUsed = barsPerYear;

    const double CL = mConfidenceLevel.getAsDouble();

    try
      {
	// --- Step 3: Analyze distribution and determine which methods to run ----
	const auto diagnostics = analyzeDistribution(ctx, os);
	os << "   [Bootstrap] L=" << L << "\n";

	// --- Step 4: Run small-N conservative bootstrap (if applicable) ---------
	std::optional<SmallNResult> smallNResult;
	if (diagnostics.shouldRunSmallN())
	  {
	    smallNResult = runSmallNBootstrap(ctx, CL, barsPerYear, L,
					      diagnostics.hasHeavyTails(), os);
	  }

	// --- Step 5: Run percentile-t bootstrap (if applicable) -----------------
	std::optional<PercentileTResult> percentileTResult;
	if (diagnostics.shouldRunPercentileT())
	  {
	    percentileTResult = runPercentileTBootstrap(ctx, CL, L, os);
	  }

	// --- Step 6: Run BCa mean bootstrap (always for compatibility) ----------
	const auto bcaMeanResult = runBCaMeanBootstrap(ctx, CL, barsPerYear, L, os);
	R.lbMeanPeriod = bcaMeanResult.getLowerBoundPeriod();
	R.annualizedLowerBoundMean = bcaMeanResult.getLowerBoundAnnualized();

	// --- Step 7: Combine geometric lower bounds -----------------------------
	const Num lbGeoPer_neutral = combineGeometricLowerBounds(smallNResult, percentileTResult, os);
	R.lbGeoPeriod = lbGeoPer_neutral;

	// Annualize geometric lower bound using barsPerYear (λ × medianHoldBars)
	R.annualizedLowerBoundGeo =
	  mkc_timeseries::Annualizer<Num>::annualize_one(R.lbGeoPeriod, barsPerYear);

	// Publish the parts for downstream near-hurdle refinement
	R.lbGeoSmallNPeriod = smallNResult.has_value()
	  ? std::optional<Num>(smallNResult->getLowerBoundPeriod())
	  : std::nullopt;
	R.lbGeoPTPeriod = percentileTResult.has_value()
	  ? std::optional<Num>(percentileTResult->getLowerBoundPeriod())
	  : std::nullopt;

	// --- Step 8: Log final policy and finish --------------------------------
	logFinalPolicy(smallNResult, percentileTResult, n, L,
		       diagnostics.getSkew(), diagnostics.getExcessKurtosis(),
		       diagnostics.hasHeavyTails(), os);

	os << "   [Bootstrap] Annualization factor (bars/year via λ×medianHoldBars) = "
	   << barsPerYear
	   << "  [λ=" << lambdaTradesPerYear
	   << ", medianHoldBars=" << medianHoldBars << "]\n";

	R.computationSucceeded = true;
	return R;
      }
    catch (const std::exception& e)
      {
	R.failureReason = std::string("Bootstrap computation failed: ") + e.what();
	os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
	return R;
      }
  }
} // namespace palvalidator::filtering::stages
