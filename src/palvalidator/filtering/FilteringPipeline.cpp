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
    // ──────────────────────────────────────────────────────────────────────────
    // Stage 1: Backtesting
    // GATE: Requires sufficient data (legacy behavior)
    // ──────────────────────────────────────────────────────────────────────────
    auto backtestDecision = mBacktestingStage.execute(ctx, os);
    if (!backtestDecision.passed())
      {
        return backtestDecision; // Fail-fast: insufficient data
      }

    // ──────────────────────────────────────────────────────────────────────────
    // Stage 2: Bootstrap Analysis
    // Computes per-period and annualized bounds. DO NOT gate here because
    // the hurdle is produced by the next stage (depends on bootstrap too).
    // ──────────────────────────────────────────────────────────────────────────
    auto bootstrap = mBootstrapStage.execute(ctx, os);

    if (!bootstrap.computationSucceeded)
      {
        std::string reason = bootstrap.failureReason.empty()
	  ? "Bootstrap analysis produced invalid results (unknown reason)"
	  : bootstrap.failureReason;
        os << "✗ Strategy filtered out: Bootstrap analysis failed - " << reason << "\n";
        return FilterDecision::Fail(FilterDecisionType::FailInsufficientData, reason);
      }

    // Persist diagnostics in context for later stages
    ctx.blockLength = bootstrap.blockLength;
    ctx.annualizationFactor = mBootstrapStage.computeAnnualizationFactor(ctx);

    // ──────────────────────────────────────────────────────────────────────────
    // Stage 3: Hurdle Analysis
    // GATE: Must pass cost-stressed base + 1Qn hurdle computation.
    // Produces ctx.finalRequiredReturn (ANNUALIZED).
    // ──────────────────────────────────────────────────────────────────────────
    auto hurdle = mHurdleStage.execute(ctx, bootstrap, os);
    ctx.finalRequiredReturn = hurdle.finalRequiredReturn;

    if (!hurdle.passed())
      {
        os << "      → Gate: FAIL vs cost-stressed hurdles.\n\n";
        return FilterDecision::Fail(
				    FilterDecisionType::FailHurdle,
				    "Failed cost-stressed hurdles");
      }

    os << "      → Gate: PASS vs cost-stressed hurdles.\n";

    // ──────────────────────────────────────────────────────────────────────────
    // Single Source-of-Truth Bootstrap Gate (Pipeline-level)
    //
    // Compare the bootstrap’s conservative ANNUALIZED Geo LB against the
    // ANNUALIZED hurdle produced by the Hurdle stage. We avoid any per-period
    // vs annualized ambiguity, and we avoid double-gating.
    //
    // Note: if you later expose per-method annualized LBs (BCa, m/n, t),
    // you can optionally implement AND-gate for n<=24 here. For now,
    // the conservative LB equals min-of-LBs and is a solid, simple gate.
    // ──────────────────────────────────────────────────────────────────────────
    const bool bootstrapPasses =
      (bootstrap.annualizedLowerBoundGeo > hurdle.finalRequiredReturn);

    os << "   [Pipeline] Bootstrap gate (annualized): "
       << "GeoLB=" << (bootstrap.annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred) << "% "
       << "vs Hurdle=" << (hurdle.finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "% → "
       << (bootstrapPasses ? "PASS" : "FAIL") << "\n";

    if (!bootstrapPasses)
      {
        os << "✗ Strategy filtered out: Bootstrap lower bound failed to exceed hurdle\n\n";
        return FilterDecision::Fail(
				    FilterDecisionType::FailHurdle,
				    "Bootstrap lower bound below required return");
      }

    // ──────────────────────────────────────────────────────────────────────────
    // Stage 4: Robustness Analysis
    // GATE: If divergence/near-hurdle/small-N, must pass robustness checks.
    // ──────────────────────────────────────────────────────────────────────────
    const auto divergence =
      palvalidator::analysis::DivergenceAnalyzer::assessAMGMDivergence(
								       bootstrap.annualizedLowerBoundGeo,
								       bootstrap.annualizedLowerBoundMean,
								       /*absThresh=*/0.05,
								       /*relThresh=*/0.30);

    const bool nearHurdle =
      (bootstrap.annualizedLowerBoundGeo
       <= (ctx.finalRequiredReturn + mRobustnessConfig.borderlineAnnualMargin));

    const bool smallN =
      (ctx.highResReturns.size() < mRobustnessConfig.minTotalForSplit);

    const bool mustRobust = divergence.flagged || nearHurdle || smallN;

    if (mustRobust)
      {
        auto robustnessDecision =
	  mRobustnessStage.execute(ctx, divergence, nearHurdle, smallN, os);

        if (!robustnessDecision.passed())
	  {
            return robustnessDecision; // Fail-fast: robustness failed
	  }
      }

    // ──────────────────────────────────────────────────────────────────────────
    // Stage 5: L-Sensitivity Stress
    // GATE: If enabled, must pass fraction threshold and gap tolerance.
    // (Uses ctx.blockLength and ctx.annualizationFactor.)
    // ──────────────────────────────────────────────────────────────────────────
    double lSensitivityRelVar = 0.0;
    auto lSensitivityDecision =
      executeLSensitivityStress(ctx, bootstrap, hurdle, lSensitivityRelVar, os);

    if (!lSensitivityDecision.passed())
      {
        return lSensitivityDecision; // Fail-fast: L-sensitivity failed
      }

    // ──────────────────────────────────────────────────────────────────────────
    // Stage 6: Regime-Mix Stress
    // GATE: Must pass configured fraction of regime mixes.
    // ──────────────────────────────────────────────────────────────────────────
    auto regimeMixDecision = mRegimeMixStage.execute(ctx, bootstrap, hurdle, os);
    if (!regimeMixDecision.passed())
      {
        return regimeMixDecision; // Fail-fast: regime-mix failed
      }

    // ──────────────────────────────────────────────────────────────────────────
    // Stage 7: Fragile Edge Advisory
    // GATE: May drop if policy action is Drop AND apply is enabled.
    // ──────────────────────────────────────────────────────────────────────────
    auto fragileEdgeDecision =
      mFragileEdgeStage.execute(ctx, bootstrap, hurdle, lSensitivityRelVar, os);
    if (!fragileEdgeDecision.passed())
      {
        return fragileEdgeDecision; // Fail-fast: fragile edge drop
      }

    // ──────────────────────────────────────────────────────────────────────────
    // Success: all gates passed
    // ──────────────────────────────────────────────────────────────────────────
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
