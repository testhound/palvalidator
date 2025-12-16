#include "filtering/FilteringPipeline.h"
#include "analysis/DivergenceAnalyzer.h"
#include "DecimalConstants.h"
#include <sstream>
#include <fstream>
#include <iomanip> // Included for formatting if needed

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
    auto sign = [&](const NumT& v)
    {
      return (v > DecimalConstants<NumT>::DecimalZero)
               ? 1
               : (v < DecimalConstants<NumT>::DecimalZero ? -1 : 0);
    };
    int last = sign(r[0]);
    std::size_t curRun = 1, bestRun = 1;
    for (std::size_t i = 1; i < n; ++i)
      {
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
    if (showHead)
      {
        os << "   [Diag] head(" << showHead << "):";
        for (std::size_t i = 0; i < showHead; ++i)
          os << " [" << i << "]=" << r[i];
        os << "\n";
      }
    if (showTail)
      {
        os << "   [Diag] tail(" << showTail << "):";
        for (std::size_t i = n - showTail; i < n; ++i)
          os << " [" << i << "]=" << r[i];
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

  FilterDecision FilteringPipeline::executeForStrategy(StrategyAnalysisContext& ctx,
						       std::ostream& os)
  {
    using mkc_timeseries::DecimalConstants;
    using Num = palvalidator::filtering::Num;

    bool runAdvisoryStages = false;

    // Stage 1: Backtesting
    auto backtestDecision = mBacktestingStage.execute(ctx, os);
    if (!backtestDecision.passed())
      {
	return backtestDecision;
      }

    if (false)
    // [TEMPORARY] Dump highResReturns for integration testing
    {
      // Create ticker-specific filename
      const std::string ticker = ctx.baseSecurity ? ctx.baseSecurity->getSymbol() : "UNKNOWN";
      const std::string tempFileName =
	"/tmp/palvalidator_highres_returns_" + ticker + ".txt";

      std::ofstream tempFile(tempFileName, std::ios::app);
      if (tempFile.is_open())
	{
	  const std::string strategyName =
	    ctx.strategy ? ctx.strategy->getStrategyName() : "<unknown>";
	  tempFile << "\n=== STRATEGY: " << strategyName << " ===\n";
	  tempFile << "Ticker: " << ticker << "\n";
	  tempFile << "Returns count: " << ctx.highResReturns.size() << "\n";

	  // Use existing dump function to write detailed analysis to temp file
	  dumpHighResReturns_(ctx.highResReturns, tempFile);

	  // Add raw data for easy copy/paste into unit tests
	  tempFile << "   [Raw Data] Returns vector:\n   {";
	  for (size_t i = 0; i < ctx.highResReturns.size(); ++i)
	    {
	      if (i > 0) tempFile << ", ";
	      if (i % 10 == 0 && i > 0) tempFile << "\n    ";  // line break every 10 values
	      tempFile << ctx.highResReturns[i];
	    }
	  tempFile << "}\n";
	  tempFile << "=== END STRATEGY: " << strategyName << " ===\n\n";
	  tempFile.close();
	}
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

    // Stage 3: Hurdle Analysis
    auto hurdle = mHurdleStage.execute(ctx, os);

    // --------------------------------------------------------------------
    // Gate metadata wiring (introspectable single source of truth)
    // We define the "primary gate" as annualized Geo LB vs hurdle.finalRequiredReturn.
    // This is *metadata only*; the veto logic is implemented just below.
    // --------------------------------------------------------------------
    {
      const Num lbAnnGeo  = bootstrap.annualizedLowerBoundGeo;
      const Num hurdleAnn = hurdle.finalRequiredReturn;

      // Use existing gate fields: gateLBGeo and gatePassedHurdle + gatePolicy.
      bootstrap.gateLBGeo       = lbAnnGeo;
      bootstrap.gatePassedHurdle = (lbAnnGeo >= hurdleAnn);
      bootstrap.gatePolicy       = "AutoCI GeoLB >= hurdle";

      os << "   [Gate] Policy='" << bootstrap.gatePolicy << "'  "
	 << (bootstrap.gatePassedHurdle ? "PASSED" : "FAILED")
	 << "  LB=" << (lbAnnGeo * 100).getAsDouble()
	 << "% vs Hurdle=" << (hurdleAnn * 100).getAsDouble() << "%\n";
    }

    // Stage 4: Centralized Validation (Gate: LB Geo > 0 AND LB PF > Hurdle, if PF available)
    ValidationPolicy policy(hurdle.finalRequiredReturn);

    // Hurdle 1: Geometric Mean Lower Bound must be positive (per-period)
    const bool isEdgePositive =
      (bootstrap.lbGeoPeriod > DecimalConstants<Num>::DecimalZero);

    // Hurdle 2: Profit Factor Lower Bound must clear the minimum threshold
    // We use 0.95 instead of 1.0 to allow for slight statistical noise in the ratio statistic,
    // but effectively enforce a profitability check.
    //
    // Note: If Percentile-T creates a negative LB, it fails this check (<=0 < 0.95),
    // acting as a strict quality filter against extreme variance.
    const Num requiredPFHurdle = DecimalConstants<Num>::createDecimal("0.95");
    bool isProfitFactorStrong  = false;
    bool pfUsable              = false;

    if (bootstrap.lbProfitFactor.has_value())
      {
        pfUsable = true;
        const Num lbPF = *bootstrap.lbProfitFactor;
        
        // Strict check: LB must be >= 0.95.
        // A negative or zero LB automatically fails here.
        isProfitFactorStrong = (lbPF >= requiredPFHurdle);

        // Warn if the interval is degenerate (<= 0), which implies high instability
        if (lbPF <= DecimalConstants<Num>::DecimalZero)
          {
            os << "   [HurdleAnalysis] Warning: PF Lower Bound (" << lbPF 
               << ") is non-positive (likely due to Percentile-T volatility).\n";
          }
      }

    // --- Combined Veto Check ---
    if (!isEdgePositive)
      {
        os << "✗ Strategy filtered out: Per-period geometric lower bound is not positive.\n"
           << "   ↳ LB(Geo Period) = " << (bootstrap.lbGeoPeriod * 100).getAsDouble() << "%\n";
        
        if (bootstrap.lbProfitFactor.has_value())
          {
            os << "   ↳ LB(PF)         = " << *bootstrap.lbProfitFactor << "\n";
          }
        else
          {
            os << "   ↳ LB(PF)         = N/A\n";
          }

        return FilterDecision::Fail(FilterDecisionType::FailHurdle,
                                    "Per-period geometric LB not > 0");
      }

    // PF veto is applied strictly if computed
    if (pfUsable && !isProfitFactorStrong)
      {
	const Num currentPF = *bootstrap.lbProfitFactor;
        os << "✗ Strategy filtered out: Robust Profit Factor lower bound failed to clear hurdle.\n"
           << "   ↳ LB(PF)         = " << currentPF << " (Required: " << requiredPFHurdle << ")\n"
           << "   ↳ LB(Geo Period) = " << (bootstrap.lbGeoPeriod * 100).getAsDouble() << "%\n";

	return FilterDecision::Fail(FilterDecisionType::FailHurdle,
				    "Robust Profit Factor LB failed hurdle");
      }

    if (pfUsable)
      {
	os << "✓ Strategy passed primary validation gate (LB Geo > 0 and "
	   << "LB PF >= " << requiredPFHurdle << ").\n";
      }
    else
      {
	os << "✓ Strategy passed primary validation gate (LB Geo > 0; "
	   << "Profit Factor veto not applied).\n";
      }

    // --- Supplemental Analysis Stages (for passed strategies) ---
    if (runAdvisoryStages)
      {
	// Stage 5: Robustness Analysis
	const auto divergence =
	  palvalidator::analysis::DivergenceAnalyzer::assessAMGMDivergence(
									   bootstrap.annualizedLowerBoundGeo,
									   bootstrap.annualizedLowerBoundMean,
									   0.05, 0.30);

	const bool nearHurdle = false; // Set to false to remove this veto trigger for specialists
	const bool smallN     = (ctx.highResReturns.size() < mRobustnessConfig.minTotalForSplit);
	const bool mustRobust = divergence.flagged || nearHurdle || smallN;

	if (mustRobust)
	  {
	    auto robustnessDecision =
	      mRobustnessStage.execute(ctx, divergence, nearHurdle, smallN, os, policy);
	    if (!robustnessDecision.passed())
	      {
		// NOTE: Veto is intentionally converted to an advisory for specialists
		os << "   [INFO] Robustness check failed, but passing to MetaStrategy for final decision.\n";
	      }
	  }

	// Stage 6: L-Sensitivity Stress
	double lSensitivityRelVar = 0.0;

	// Use zero hurdle for L-Sensitivity to enforce LB > 0 under stress
	auto lSensitivityDecision =
	  executeLSensitivityStress(ctx, bootstrap, hurdle, lSensitivityRelVar, Num(0.0), os);

	if (!lSensitivityDecision.passed())
	  {
	    // NOTE: Veto is intentionally converted to an advisory for specialists
	    os << "   [INFO] L-Sensitivity failed, but passing to MetaStrategy for final decision.\n";
	  }

	// Stage 7: Regime-Mix Stress
	auto regimeMixDecision = mRegimeMixStage.execute(ctx, bootstrap, hurdle, os);
	if (!regimeMixDecision.passed())
	  {
	    // NOTE: Veto is intentionally converted to an advisory for specialists
	    os << "   [INFO] Regime-Mix failed, but passing to MetaStrategy for final decision.\n";
	  }

	// Stage 8: Fragile Edge Advisory
	auto fragileEdgeDecision =
	  mFragileEdgeStage.execute(ctx, bootstrap, hurdle, lSensitivityRelVar, os);
	if (!fragileEdgeDecision.passed())
	  {
	    // NOTE: Veto is intentionally converted to an advisory for specialists
	    os << "   [INFO] FragileEdge check failed, but passing to MetaStrategy for final decision.\n";
	  }
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
      const Num& requiredReturn,
      std::ostream& os) const
  {
    outRelVar = 0.0;
    if (!mLSensitivityConfig.enabled)
      {
        return FilterDecision::Pass();
      }

    size_t L_cap = mLSensitivityConfig.maxL;
    if (mLSensitivityConfig.capByMaxHold)
      {
        const size_t n = ctx.highResReturns.size();
        size_t medHold =
          (ctx.backtester)
            ? static_cast<size_t>(ctx.backtester->getClosedPositionHistory()
                                    .getMedianHoldingPeriod())
            : 0;
        if (medHold == 0) medHold = (ctx.blockLength > 0) ? ctx.blockLength : 3;

        size_t base = std::max<size_t>(2, medHold);
        const size_t gentleBuf =
          std::min(mLSensitivityConfig.capBuffer,
                   std::max<size_t>(1, base / 2));
        const size_t sampleCap    = (n > 0) ? std::max<size_t>(2, n - 1) : mLSensitivityConfig.maxL;
        const size_t growthCap    = 2 * base;
        const size_t neighborhoodCap = base + 2;
        const size_t desiredCap =
          std::max(base + gentleBuf, neighborhoodCap);
        L_cap = std::min({mLSensitivityConfig.maxL, sampleCap, growthCap, desiredCap});
        L_cap = std::max<size_t>(2, L_cap);
      }

    const double annUsed =
      (bootstrap.annFactorUsed > 0.0) ? bootstrap.annFactorUsed : ctx.annualizationFactor;
    const auto Lres =
      mLSensitivityStage.execute(ctx, L_cap, annUsed, requiredReturn, os);

    if (!Lres.ran)
      {
        return FilterDecision::Pass();
      }

    outRelVar = Lres.relVar;
    if (!Lres.pass)
      {
        const Num gap = (requiredReturn - Lres.minLbAnn);
        const bool catastrophic =
          (gap > Num(std::max(0.0, mLSensitivityConfig.minGapTolerance)));
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
					    const ValidationPolicy&        policy,
					    std::ostream&                  os) const
  {
    // Default: log using the canonical annualized Geo LB and the policy hurdle.
    Num lbForLog     = bootstrap.annualizedLowerBoundGeo;
    Num hurdleForLog = policy.getRequiredReturn();

    // If gatePolicy is set, prefer the gate's LB Geo value for the message.
    if (!bootstrap.gatePolicy.empty())
      {
	lbForLog = bootstrap.gateLBGeo;
	// We still take the hurdle from the ValidationPolicy; if in the future
	// you want to log a different hurdle, you can store it in the result.
      }

    os << "✓ Strategy passed: "
       << (ctx.strategy ? ctx.strategy->getStrategyName() : "<unknown>")
       << " (Lower Bound = "
       << (lbForLog * 100).getAsDouble()
       << "% > Required Return = "
       << (hurdleForLog * 100).getAsDouble() << "%)"
       << "  [Block L=" << bootstrap.blockLength << "]";

    if (!bootstrap.gatePolicy.empty())
      {
	os << "  [GatePolicy=" << bootstrap.gatePolicy
	   << ", gatePassed=" << (bootstrap.gatePassedHurdle ? "true" : "false") << "]";
      }

    os << "\n";

    os << "   ↳ Lower bounds (annualized): "
       << "GeoMean = " << (bootstrap.annualizedLowerBoundGeo * 100).getAsDouble() << "%, "
       << "Mean = "  << (bootstrap.annualizedLowerBoundMean  * 100).getAsDouble() << "%";
    
    // Explicitly add Profit Factor if available
    if (bootstrap.lbProfitFactor.has_value())
      {
        os << ", PF = " << *bootstrap.lbProfitFactor;
      }
    os << "\n";

    // If you want to see AutoCI diagnostics in the log, you can uncomment/extend this:
    //
     if (!bootstrap.geoAutoCIChosenMethod.empty())
       {
         os << "   [AutoCI-Geo] method=" << bootstrap.geoAutoCIChosenMethod
            << " score=" << bootstrap.geoAutoCIChosenScore
            << " stab_penalty=" << bootstrap.geoAutoCIStabilityPenalty
            << " len_penalty=" << bootstrap.geoAutoCILengthPenalty
            << " hasBCa=" << (bootstrap.geoAutoCIHasBCaCandidate ? "true" : "false")
            << " BCaChosen=" << (bootstrap.geoAutoCIBCaChosen ? "true" : "false")
            << " BCaRejected(stability)="
            << (bootstrap.geoAutoCIBCaRejectedForInstability ? "true" : "false")
            << " BCaRejected(length)="
            << (bootstrap.geoAutoCIBCaRejectedForLength ? "true" : "false")
            << " numCandidates=" << bootstrap.geoAutoCINumCandidates
            << "\n";
       }
    
     if (!bootstrap.pfAutoCIChosenMethod.empty())
       {
         os << "   [AutoCI-PF] method=" << bootstrap.pfAutoCIChosenMethod
            << " score=" << bootstrap.pfAutoCIChosenScore
            << " stab_penalty=" << bootstrap.pfAutoCIStabilityPenalty
            << " len_penalty=" << bootstrap.pfAutoCILengthPenalty
            << " hasBCa=" << (bootstrap.pfAutoCIHasBCaCandidate ? "true" : "false")
            << " BCaChosen=" << (bootstrap.pfAutoCIBCaChosen ? "true" : "false")
            << " BCaRejected(stability)="
            << (bootstrap.pfAutoCIBCaRejectedForInstability ? "true" : "false")
            << " BCaRejected(length)="
            << (bootstrap.pfAutoCIBCaRejectedForLength ? "true" : "false")
            << " numCandidates=" << bootstrap.pfAutoCINumCandidates
            << "\n";
       }

    os << "\n";
  }
  
} // namespace palvalidator::filtering
