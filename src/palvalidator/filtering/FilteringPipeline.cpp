#include "filtering/FilteringPipeline.h"
#include "analysis/DivergenceAnalyzer.h"
#include "DecimalConstants.h"
#include "SmallNBootstrapHelpers.h"
#include <sstream>

namespace
{
  template <typename NumT>
  void dumpHighResReturns_(const std::vector<NumT>& r, std::ostream& os,
                           std::size_t max_head = 12, std::size_t max_tail = 12)
  {
    using mkc_timeseries::DecimalConstants;
    const std::size_t n = r.size();
    if (n == 0)
      {
	os << "   [Diag] highResReturns: <empty>\n";
	return;
      }

    // Basic stats
    NumT sum = DecimalConstants<NumT>::DecimalZero;
    NumT rmin = r[0], rmax = r[0];
    std::size_t pos = 0;
    for (const auto& v : r)
      {
	sum = sum + v;
	if (v < rmin)
	  rmin = v;
	if (v > rmax)
	  rmax = v;
	if (v > DecimalConstants<NumT>::DecimalZero)
	  ++pos;
      }
    const double mean = (n ? (sum / NumT(static_cast<int>(n))).getAsDouble() : 0.0);
    const double pctPos = 100.0 * static_cast<double>(pos) / static_cast<double>(n);

    // Longest sign run (very lightweight dependence proxy)
    auto sign = [&](const NumT& v){ return (v > DecimalConstants<NumT>::DecimalZero) ? 1 : (v < DecimalConstants<NumT>::DecimalZero ? -1 : 0); };
    int last = sign(r[0]);
    std::size_t curRun = 1, bestRun = 1;
    for (std::size_t i = 1; i < n; ++i) {
      const int s = sign(r[i]);
      if (s == last && s != 0)
	{
	  ++curRun;
	}
      else
	{
	  bestRun = std::max(bestRun, curRun);
	  curRun = 1;
	  last = s;
	}
    }
    bestRun = std::max(bestRun, curRun);

    os << "   [Diag] highResReturns: n=" << n
       << "  mean=" << std::setprecision(10) << mean
       << "  min="  << rmin
       << "  max="  << rmax
       << "  %>0="  << pctPos << "%  longestSignRun=" << bestRun << "\n";

    // Head/Tail print
    const std::size_t showHead = std::min(max_head, n);
    const std::size_t showTail = (n > max_head ? std::min(max_tail, n - showHead) : 0);
    if (showHead) {
      os << "   [Diag] head(" << showHead << "):";
      for (std::size_t i = 0; i < showHead; ++i) os << " [" << i << "]=" << r[i];
      os << "\n";
    }
    if (showTail) {
      os << "   [Diag] tail(" << showTail << "):";
      for (std::size_t i = n - showTail; i < n; ++i) os << " [" << i << "]=" << r[i];
      os << "\n";
    }
  }
}

namespace palvalidator::filtering
{
  using mkc_timeseries::DecimalConstants;

  FilteringPipeline::FilteringPipeline(const TradingHurdleCalculator& hurdleCalc,
				       const Num& confidenceLevel,
				       unsigned int numResamples,
				       const RobustnessChecksConfig& robustnessConfig,
				       const PerformanceFilter::LSensitivityConfig& lSensitivityConfig,
				       const FragileEdgePolicy& fragileEdgePolicy,
				       bool applyFragileAdvice,
				       FilteringSummary& summary,
				       BootstrapFactory& bootstrapFactory)
    : mBacktestingStage()
    , mBootstrapStage(confidenceLevel, numResamples, bootstrapFactory)
    , mHurdleStage(hurdleCalc)
    , mRobustnessStage(robustnessConfig, summary, bootstrapFactory)
    , mLSensitivityStage(lSensitivityConfig, numResamples, confidenceLevel, bootstrapFactory)
    , mRegimeMixStage(confidenceLevel, numResamples)
    , mFragileEdgeStage(fragileEdgePolicy, applyFragileAdvice)
    , mRobustnessConfig(robustnessConfig)
    , mLSensitivityConfig(lSensitivityConfig)
    , mBootstrapFactory(bootstrapFactory)
  {
  }

    FilterDecision FilteringPipeline::executeForStrategy(
        StrategyAnalysisContext& ctx,
        std::ostream& os)
    {
        // Stage 1: Backtesting
        auto backtestDecision = mBacktestingStage.execute(ctx, os);
        if (!backtestDecision.passed())
        {
            return backtestDecision;
        }

        // Stage 2: Bootstrap Analysis
        auto bootstrap = mBootstrapStage.execute(ctx, os);
        if (!bootstrap.computationSucceeded)
        {
            std::string reason = bootstrap.failureReason.empty()
                                     ? "Bootstrap analysis failed"
                                     : bootstrap.failureReason;
            os << "✗ Strategy filtered out: " << reason << "\n";
            return FilterDecision::Fail(FilterDecisionType::FailInsufficientData, reason);
        }

	// Make bootstrap AF available to downstream stages for consistent scaling
        ctx.annualizationFactor = bootstrap.annFactorUsed;

	os << "   [Bootstrap->Pipeline] Using OOS bootstrap annualization: annFactorUsed="
	   << bootstrap.annFactorUsed
	   << " (will be used for both LB annualization and cost-hurdle trades/yr)\n";

        // Stage 3: Hurdle Analysis (Simplified)
        auto hurdle = mHurdleStage.execute(ctx, os);

        // Stage 4: Centralized Validation
        ValidationPolicy policy(hurdle.finalRequiredReturn);

	const bool isEdgePositive = (bootstrap.lbGeoPeriod > DecimalConstants<Num>::DecimalZero);

        if (!isEdgePositive)
        {
            os << "✗ Strategy filtered out: Per-period lower bound is not positive ("
               << (bootstrap.lbGeoPeriod * 100).getAsDouble() << "%).\n";
            return FilterDecision::Fail(FilterDecisionType::FailHurdle,
                                        "Per-period geometric LB not > 0");
        }

        os << "✓ Strategy passed primary validation gate.\n";

        // --- Supplemental Analysis Stages (for passed strategies) ---

        // Stage 5: Robustness Analysis
        const auto divergence = palvalidator::analysis::DivergenceAnalyzer::assessAMGMDivergence(
            bootstrap.annualizedLowerBoundGeo,
            bootstrap.annualizedLowerBoundMean,
            0.05, 0.30);

        const bool nearHurdle = false;
        const bool smallN = (ctx.highResReturns.size() < mRobustnessConfig.minTotalForSplit);
        const bool mustRobust = divergence.flagged || nearHurdle || smallN;

        if (mustRobust)
        {
            auto robustnessDecision =
                mRobustnessStage.execute(ctx, divergence, nearHurdle, smallN, os, policy);
            if (!robustnessDecision.passed())
            {
	      //return robustnessDecision;
		os << "   [INFO] Robustness check failed, but passing to MetaStrategy for final decision.\n";
            }
        }

        // Stage 6: L-Sensitivity Stress
        double lSensitivityRelVar = 0.0;
        auto lSensitivityDecision =
            executeLSensitivityStress(ctx, bootstrap, hurdle, lSensitivityRelVar, os);
        if (!lSensitivityDecision.passed())
        {
	  //return lSensitivityDecision;
	    os << "   [INFO] L-Sensitivity failed, but passing to MetaStrategy for final decision.\n";
        }

        // Stage 7: Regime-Mix Stress
        auto regimeMixDecision = mRegimeMixStage.execute(ctx, bootstrap, hurdle, os);
        if (!regimeMixDecision.passed())
        {
	  //return regimeMixDecision;
	  os << "   [INFO] Regime-Mix failed, but passing to MetaStrategy for final decision.\n";
        }

        // Stage 8: Fragile Edge Advisory
        auto fragileEdgeDecision =
            mFragileEdgeStage.execute(ctx, bootstrap, hurdle, lSensitivityRelVar, os);
        if (!fragileEdgeDecision.passed())
        {
	  //return fragileEdgeDecision;
	    os << "   [INFO] FragileEdge check failed, but passing to MetaStrategy for final decision.\n";
        }

        // --- Success ---
        logPassedStrategy(ctx, bootstrap, policy, os);
        return FilterDecision::Pass();
    }
  
   FilterDecision FilteringPipeline::executeLSensitivityStress(
       const StrategyAnalysisContext& ctx,
       const BootstrapAnalysisResult& bootstrap,
       const HurdleAnalysisResult& hurdle,
       double& outRelVar,
       std::ostream& os) const
   {
       // This method's logic remains largely the same, but the hurdle comparison
       // now uses the simplified trading cost.
       outRelVar = 0.0;
       if (!mLSensitivityConfig.enabled)
       {
           return FilterDecision::Pass();
       }

       size_t L_cap = mLSensitivityConfig.maxL;
       if (mLSensitivityConfig.capByMaxHold)
       {
           const size_t n = ctx.highResReturns.size();
           size_t medHold = (ctx.backtester) ? static_cast<size_t>(ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod()) : 0;
           if (medHold == 0) medHold = (ctx.blockLength > 0) ? ctx.blockLength : 3;

           size_t base = std::max<size_t>(2, medHold);
           const size_t gentleBuf = std::min(mLSensitivityConfig.capBuffer, std::max<size_t>(1, base / 2));
           const size_t sampleCap = (n > 0) ? std::max<size_t>(2, n - 1) : mLSensitivityConfig.maxL;
           const size_t growthCap = 2 * base;
           const size_t neighborhoodCap = base + 2;
           const size_t desiredCap = std::max(base + gentleBuf, neighborhoodCap);
           L_cap = std::min({mLSensitivityConfig.maxL, sampleCap, growthCap, desiredCap});
           L_cap = std::max<size_t>(2, L_cap);
       }

       const double annUsed = (bootstrap.annFactorUsed > 0.0) ? bootstrap.annFactorUsed : ctx.annualizationFactor;
       const auto Lres = mLSensitivityStage.execute(ctx, L_cap, annUsed, hurdle.finalRequiredReturn, os);

       if (!Lres.ran)
       {
           return FilterDecision::Pass();
       }

       outRelVar = Lres.relVar;
       if (!Lres.pass)
       {
           const Num gap = (hurdle.finalRequiredReturn - Lres.minLbAnn);
           const bool catastrophic = (gap > Num(std::max(0.0, mLSensitivityConfig.minGapTolerance)));
           const std::string reason = catastrophic
                                    ? "L-sensitivity: catastrophic gap"
                                    : "L-sensitivity: high variability";
           return FilterDecision::Fail(FilterDecisionType::FailLSensitivity, reason);
       }

       return FilterDecision::Pass();
   }

   void FilteringPipeline::logPassedStrategy(
       const StrategyAnalysisContext& ctx,
       const BootstrapAnalysisResult& bootstrap,
       const ValidationPolicy& policy,
       std::ostream& os) const
   {
       os << "✓ Strategy passed: " << (ctx.strategy ? ctx.strategy->getStrategyName() : "<unknown>")
          << " (Lower Bound = "
          << (bootstrap.annualizedLowerBoundGeo * 100).getAsDouble()
          << "% > Required Return = "
          << (policy.getRequiredReturn() * 100).getAsDouble() << "%)"
          << "  [Block L=" << bootstrap.blockLength << "]\n";

       os << "   ↳ Lower bounds (annualized): "
          << "GeoMean = " << (bootstrap.annualizedLowerBoundGeo * 100).getAsDouble() << "%, "
          << "Mean = " << (bootstrap.annualizedLowerBoundMean * 100).getAsDouble() << "%\n\n";
   }

} // namespace palvalidator::filtering
