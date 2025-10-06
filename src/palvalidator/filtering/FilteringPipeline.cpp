#include "filtering/FilteringPipeline.h"
#include "analysis/DivergenceAnalyzer.h"
#include "DecimalConstants.h"
#include <sstream>

namespace palvalidator::filtering
{
  using mkc_timeseries::DecimalConstants;

  FilteringPipeline::FilteringPipeline(
    const TradingHurdleCalculator& hurdleCalc,
    const Num& confidenceLevel,
    unsigned int numResamples,
    const RobustnessChecksConfig& robustnessConfig,
    const PerformanceFilter::LSensitivityConfig& lSensitivityConfig,
    const FragileEdgePolicy& fragileEdgePolicy,
    bool applyFragileAdvice,
    FilteringSummary& summary)
    : mBacktestingStage()
    , mBootstrapStage(confidenceLevel, numResamples)
    , mHurdleStage(hurdleCalc)
    , mRobustnessStage(robustnessConfig, summary)
    , mLSensitivityStage(lSensitivityConfig, numResamples, confidenceLevel)
    , mRegimeMixStage(confidenceLevel, numResamples)
    , mFragileEdgeStage(fragileEdgePolicy, applyFragileAdvice)
    , mRobustnessConfig(robustnessConfig)
    , mLSensitivityConfig(lSensitivityConfig)
  {
  }

  FilterDecision FilteringPipeline::executeForStrategy(
    StrategyAnalysisContext& ctx,
    std::ostream& os)
  {
    // Stage 1: Backtesting (lines 67-82)
    // GATE: Requires >= 20 returns
    auto backtestDecision = mBacktestingStage.execute(ctx, os);
    if (!backtestDecision.passed())
    {
      return backtestDecision; // Fail-fast: insufficient data
    }

    // Stage 2: Bootstrap Analysis (lines 84-118)
    // NO GATE: Pure computation
    auto bootstrap = mBootstrapStage.execute(ctx, os);
    if (!bootstrap.isValid())
    {
      std::string reason = bootstrap.failureReason.empty()
        ? "Bootstrap analysis produced invalid results (unknown reason)"
        : bootstrap.failureReason;
      os << "✗ Strategy filtered out: Bootstrap analysis failed - " << reason << "\n";
      return FilterDecision::Fail(FilterDecisionType::FailInsufficientData, reason);
    }

    // Store computed values in context for later stages
    ctx.blockLength = bootstrap.blockLength;
    ctx.annualizationFactor = mBootstrapStage.computeAnnualizationFactor(ctx);

    // Stage 3: Hurdle Analysis (lines 122-157)
    // GATE: Must pass BOTH baseHurdle AND 1Qn stress
    auto hurdle = mHurdleStage.execute(ctx, bootstrap, os);
    ctx.finalRequiredReturn = hurdle.finalRequiredReturn;

    if (!hurdle.passed())
    {
      os << "      → Gate: FAIL vs cost-stressed hurdles.\n\n";
      return FilterDecision::Fail(FilterDecisionType::FailHurdle,
                                  "Failed cost-stressed hurdles");
    }

    os << "      → Gate: PASS vs cost-stressed hurdles.\n";

    // Stage 4: Robustness Analysis (lines 160-184)
    // GATE: If triggered (divergence/near-hurdle/small-N), must pass checks
    // Note: RobustnessStage updates FilteringSummary directly (matches legacy)

    // AM-GM divergence diagnostic (legacy line 160-162)
    const auto divergence =
      palvalidator::analysis::DivergenceAnalyzer::assessAMGMDivergence(
        bootstrap.annualizedLowerBoundGeo,
        bootstrap.annualizedLowerBoundMean,
        /*absThresh=*/0.05,
        /*relThresh=*/0.30);

    // Determine if robustness checks needed (legacy lines 165-168)
    const bool nearHurdle = (bootstrap.annualizedLowerBoundGeo <=
                            (ctx.finalRequiredReturn + mRobustnessConfig.borderlineAnnualMargin));
    const bool smallN = (ctx.highResReturns.size() < mRobustnessConfig.minTotalForSplit);
    const bool mustRobust = divergence.flagged || nearHurdle || smallN;

    if (mustRobust)
    {
      auto robustnessDecision = mRobustnessStage.execute(ctx, divergence, nearHurdle, smallN, os);
      if (!robustnessDecision.passed())
      {
        return robustnessDecision; // Fail-fast: robustness failed
      }
    }

    // Stage 5: L-Sensitivity Stress (lines 188-230)
    // GATE: If enabled, must pass fraction threshold and gap tolerance
    double lSensitivityRelVar = 0.0;
    auto lSensitivityDecision = executeLSensitivityStress(ctx, bootstrap, hurdle, lSensitivityRelVar, os);
    if (!lSensitivityDecision.passed())
    {
      return lSensitivityDecision; // Fail-fast: L-sensitivity failed
    }

    // Stage 6: Regime-Mix Stress (lines 233-249)
    // GATE: Must pass configured fraction of regime mixes
    auto regimeMixDecision = mRegimeMixStage.execute(ctx, bootstrap, hurdle, os);
    if (!regimeMixDecision.passed())
    {
      return regimeMixDecision; // Fail-fast: regime-mix failed
    }

    // Stage 7: Fragile Edge Advisory (lines 254-258)
    // GATE: May drop if policy action is Drop AND apply is enabled
    auto fragileEdgeDecision = mFragileEdgeStage.execute(ctx, bootstrap, hurdle, lSensitivityRelVar, os);
    if (!fragileEdgeDecision.passed())
    {
      return fragileEdgeDecision; // Fail-fast: fragile edge drop
    }

    // All gates passed - log success and return
    logPassedStrategy(ctx, bootstrap, hurdle, os);
    return FilterDecision::Pass();
  }

  FilterDecision FilteringPipeline::executeLSensitivityStress(
    const StrategyAnalysisContext& ctx,
    const BootstrapAnalysisResult& bootstrap,
    const HurdleAnalysisResult& hurdle,
    double& outRelVar,
    std::ostream& os) const
  {
    // Initialize output parameter
    outRelVar = 0.0;

    // Skip if L-sensitivity not enabled
    if (!mLSensitivityConfig.enabled)
    {
      return FilterDecision::Pass();
    }

    size_t L_cap = mLSensitivityConfig.maxL;
    if (mLSensitivityConfig.capByMaxHold)
    {
      unsigned int maxHoldBars = 0;
      if (ctx.strategy)
        maxHoldBars = ctx.strategy->getMaxHoldingPeriod();

      if (maxHoldBars == 0 && ctx.clonedStrategy)
        maxHoldBars = ctx.clonedStrategy->getMaxHoldingPeriod();

      // Fallbacks: bootstrap block length (median-hold proxy), then legacy default of 8 bars.
      if (maxHoldBars == 0)
	{
	  if (ctx.blockLength > 0)
	    maxHoldBars = static_cast<unsigned int>(ctx.blockLength);
	  else
	    maxHoldBars = 8;
	}

      const size_t byHold = static_cast<size_t>(std::max<unsigned int>(2, maxHoldBars + mLSensitivityConfig.capBuffer));
      L_cap = std::min(mLSensitivityConfig.maxL, byHold);
    }

    // Run L-sensitivity stress with the computed cap
    const auto Lres = mLSensitivityStage.execute(ctx, L_cap, ctx.annualizationFactor, hurdle.finalRequiredReturn, os);

    // If stress didn't run (e.g., n<20), just pass
    if (!Lres.ran)
    {
      return FilterDecision::Pass();
    }

    // One-line summary (legacy lines 208-213)
    const double frac = (Lres.numTested == 0) ? 0.0 : double(Lres.numPassed) / double(Lres.numTested);
    os << "      [L-grid] pass fraction = " << (100.0 * frac) << "%, "
       << "min LB at L=" << Lres.L_at_min
       << ", min LB = " << (Lres.minLbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%, "
       << "relVar = " << Lres.relVar << " → decision: "
       << (Lres.pass ? "PASS" : "FAIL") << "\n";

    // Feed relVar for fragile edge (legacy line 216)
    outRelVar = Lres.relVar;

    // Check if stress passed
    if (!Lres.pass)
    {
      // Determine failure type: catastrophic vs variability (legacy lines 220-223)
      // NOTE: This is where pipeline determines failure type since stage doesn't update summary
      const bool catastrophic =
        (hurdle.finalRequiredReturn - Lres.minLbAnn) > Num(std::max(0.0, mLSensitivityConfig.minGapTolerance));

      os << "   ✗ Strategy filtered out due to L-sensitivity: "
         "insufficient robustness across block lengths (capped).\n\n";

      return FilterDecision::Fail(
        FilterDecisionType::FailLSensitivity,
        catastrophic ? "L-sensitivity: catastrophic gap" : "L-sensitivity: high variability");
    }

    return FilterDecision::Pass();
  }

  void FilteringPipeline::logPassedStrategy(
    const StrategyAnalysisContext& ctx,
    const BootstrapAnalysisResult& bootstrap,
    const HurdleAnalysisResult& hurdle,
    std::ostream& os) const
  {
    // Legacy lines 263-272
    os << "✓ Strategy passed: " << (ctx.strategy ? ctx.strategy->getStrategyName() : std::string("<unknown>"))
       << " (Lower Bound = "
       << (bootstrap.annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
       << "% > Required Return = "
       << (hurdle.finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
       << "  [Block L=" << bootstrap.blockLength << "]\n";

    os << "   ↳ Lower bounds (annualized): "
       << "GeoMean = " << (bootstrap.annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred) << "%, "
       << "Mean = " << (bootstrap.annualizedLowerBoundMean * DecimalConstants<Num>::DecimalOneHundred) << "%\n\n";
  }

} // namespace palvalidator::filtering
