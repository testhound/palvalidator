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
                                        BootstrapFactory& bootstrapFactory,
                                        std::shared_ptr<palvalidator::diagnostics::IBootstrapObserver> observer)
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
    // If an observer is provided, set it on the relevant stage(s)
    if (observer) {
      // BootstrapAnalysisStage will need to hold the observer; we set it here
      mBootstrapStage.setObserver(observer);
    }
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

	  dumpHighResReturns_(ctx.highResReturns, tempFile);

	  tempFile << "   [Raw Data] Returns vector:\n   {";
	  for (size_t i = 0; i < ctx.highResReturns.size(); ++i)
	    {
	      if (i > 0) tempFile << ", ";
	      if (i % 10 == 0 && i > 0) tempFile << "\n    ";
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

    // Make bootstrap AF available to downstream stages
    ctx.annualizationFactor = bootstrap.annFactorUsed;

    os << "   [Bootstrap->Pipeline] Using OOS bootstrap annualization: annFactorUsed="
       << bootstrap.annFactorUsed
       << " (will be used for both LB annualization and cost-hurdle trades/yr)\n";

    // Stage 3: Hurdle Analysis
    auto hurdle = mHurdleStage.execute(ctx, os);

    // Gate metadata wiring
    {
      const Num lbAnnGeo  = bootstrap.annualizedLowerBoundGeo;
      const Num hurdleAnn = hurdle.finalRequiredReturn;

      bootstrap.gateLBGeo       = lbAnnGeo;
      bootstrap.gatePassedHurdle = (lbAnnGeo >= hurdleAnn);
      bootstrap.gatePolicy       = "AutoCI GeoLB >= hurdle";
    }

    // ============================================================================
    // ADAPTIVE PROFIT FACTOR HURDLE - FDR CONTROL MECHANISM
    // ============================================================================
    //
    // OVERVIEW:
    // The profit factor (PF) hurdle is calculated as: PF_hurdle = exp(k * se_boot)
    // where k is a "skepticism factor" that adapts based on sample size.
    //
    // WHY ADAPTIVE k?
    // Larger sample sizes reduce bootstrap instability, making false positives
    // appear more consistent. To maintain FDR <0.1% across all sample sizes,
    // we increase k (raising the hurdle) as n increases.
    //
    // MECHANISM:
    // The hurdle is set in log-space as: log(PF) must exceed k * se_boot
    // This means the strategy must be at least k standard errors above breakeven.
    //
    // Example with n=27, se_boot=0.20, k=0.5:
    //   Hurdle = exp(0.5 × 0.20) = exp(0.10) = 1.105
    //   Strategy needs PF > 1.105 (10.5% above breakeven)
    //
    // Example with n=100, se_boot=0.104, k=1.0:
    //   Hurdle = exp(1.0 × 0.104) = exp(0.104) = 1.110
    //   Strategy needs PF > 1.110 (11% above breakeven)
    //
    // SKEPTICISM FACTOR SCHEDULE:
    //   n ≤ 60:  k = 0.5  (mild - instability provides control)
    //   n ≤ 80:  k = 0.75 (moderate - compensating for stability)
    //   n ≤ 100: k = 1.0  (standard - requires 1 SE above breakeven)
    //   n > 100: k = 1.5  (strict - strong FDR control needed)
    //
    // INTERACTION WITH GEOMETRIC MEAN:
    // Both metrics must pass in ALL 10/10 bootstrap trials:
    //   1. Geometric mean lower bound > 0 (economic viability)
    //   2. Profit factor > exp(k * se_boot) (statistical significance)
    //
    // For a false positive (no real edge):
    //   P(GM > 0 in single trial) ≈ 60-85% (depending on n)
    //   P(PF > hurdle in single trial) ≈ 30-60% (depending on n and k)
    //   P(both pass in single trial) ≈ 25-50%
    //   P(both pass 10/10 trials) ≈ 0.0006-0.3% (depending on n)
    //
    // EMPIRICAL VALIDATION:
    // With mixed sample size distribution (median n=27):
    //   - Expected false positives in 112 survivors: ~0.1-0.15 strategies
    //   - Overall FDR: ~0.1% (essentially zero false positives)
    //
    // This has been empirically validated (e.g., EWZ ticker analysis):
    //   - 112/116 survivors (96.6%) passed at 100% rate (10/10)
    //   - Only 4 survivors (3.4%) passed at 90% rate (9/10)
    //   - Bimodal distribution confirms excellent signal/noise separation
    //
    // FUTURE MAINTAINERS:
    // - The k schedule in getSkepticismFactor() is calibrated for FDR <0.1%
    // - Reducing k will increase FDR (more false positives)
    // - Increasing k will decrease power (reject more true positives)
    // - The current schedule is empirically validated - change with caution
    // - If you modify, re-run FDR simulations to validate
    //
    // ============================================================================
    // Stage 4: Centralized Validation (Gate: LB Geo > 0 AND Strict PF Validation)
    ValidationPolicy policy(hurdle.finalRequiredReturn);

    // Define skepticism factor k based on sample size
    const double k = getSkepticismFactor(ctx.highResReturns.size());
    const double se = bootstrap.pfAutoCIChosenSeBoot;

    /**
     * @section PROFIT_FACTOR_VALIDATION Rationale for Dynamic Volatility Hurdle
     *
     * The Profit Factor (PF) validation stage uses a dynamic hurdle rather than a 
     * static 1.10 floor to account for "Estimation Risk" inherent in daily 
     * mark-to-market (MTM) returns.
     *
     * 1. Log-Space Bootstrap Mechanics:
     * Our bootstrap engine (LogProfitFactorFromLogBarsStat_LogPF) operates in 
     * logarithmic space: log(PF) = log(GrossWins) - log(GrossLosses)[cite: 6]. 
     * In this space, a breakeven strategy has a mean of 0.0[cite: 6].
     *
     * 2. The Standard Error (SE) as a Risk Metric:
     * The 'se_boot' captured from the bootstrap tournament is the standard 
     * deviation of the bootstrap replicates. It represents the 
     * uncertainty of the Profit Factor estimate given the sample size (n) and 
     * the volatility of the daily returns.
     *
     * 3. Why a Dynamic Hurdle?
     * A fixed 1.10 hurdle is "blind" to volatility. A strategy with 
     * high daily MTM noise will have a wide bootstrap distribution (high SE). 
     * The dynamic hurdle requires the Lower Bound (LB) to be at least 'k' 
     * standard errors above the breakeven line.
     *
     * Hurdle Formula: exp(k * se_boot)
     *
     * - If se_boot is low (consistent returns): The hurdle stays near 1.10.
     * - If se_boot is high (noisy/lumpy returns): The hurdle rises (e.g., to 1.30+), 
     * forcing a higher margin of safety to compensate for fragility.
     *
     * 4. Implementation Details:
     * - Skepticism Factor (k = 0.5): A balanced multiplier that ensures the 
     * strategy's edge is statistically significant relative to its noise.
     * - Hard Floor (1.10): Prevents the filter from accepting "smooth" strategies 
     * with paper-thin margins that might be eaten by slippage.
     *
     * This dual-gate approach (max(1.10, exp(k*se))) ensures that we only discover 
     * strategies that are both profitable and robust against the specific path of 
     * daily returns observed in the backtest.
     */

    // 1. Calculate the dynamic hurdle based on bootstrap volatility
    const double dynamicHurdleVal = std::exp(k * se);

    // 2. Set a hard minimum floor to ensure we don't accept 'smooth' marginals
    const Num minRequiredFloor = DecimalConstants<Num>::createDecimal("1.10");
  
    // The dynamic hurdle is the linear equivalent of being k sigmas above breakeven (0) in log-space
    const Num dynamicPFHurdle = Num(std::max(num::to_double(minRequiredFloor), dynamicHurdleVal));

    {
      os << "Sample size: " << ctx.highResReturns.size()
	 << ", Skepticism factor k: " << k
       << ", Bootstrap SE: " << se
	 << ", Dynamic hurdle: " << dynamicHurdleVal << "\n";
    }

    // --- Helper Lambda for Consistent Logging ---
    auto logGateMetrics = [&](const std::string& status) {
      os << "   [" << status << "] Gate Validation Metrics:\n"
         << "      1. Annualized Geo LB: " << (bootstrap.annualizedLowerBoundGeo * 100).getAsDouble() << "%\n";
      
      if (bootstrap.lbProfitFactor.has_value()) {
          os << "      2. Profit Factor LB:  " << *bootstrap.lbProfitFactor 
             << " (Hurdle: " << dynamicPFHurdle << ")\n";
      } else {
          os << "      2. Profit Factor LB:  N/A\n";
      }

      if (bootstrap.medianProfitFactor.has_value())
          os << "      3. Profit Factor Med: " << *bootstrap.medianProfitFactor << "\n";
      else
          os << "      3. Profit Factor Med: N/A\n";
    };

    // --- Hurdle 1: Geometric Mean Lower Bound must be positive ---
    if (bootstrap.lbGeoPeriod <= DecimalConstants<Num>::DecimalZero)
      {
        os << "✗ Strategy filtered out: Per-period geometric lower bound is not positive.\n";
        logGateMetrics("FAIL");
        return FilterDecision::Fail(FilterDecisionType::FailHurdle,
                                    "Per-period geometric LB not > 0");
      }

    // --- Hurdle 2: Profit Factor Availability ---
    if (!bootstrap.lbProfitFactor.has_value())
      {
         os << "✗ Strategy filtered out: Profit Factor statistics are unavailable.\n"
            << "   ↳ Validation strictly requires Profit Factor metrics (likely insufficient trade count).\n";
         
         logGateMetrics("FAIL");
         return FilterDecision::Fail(FilterDecisionType::FailInsufficientData,
                                     "Profit Factor statistics unavailable");
      }

    // --- Hurdle 3: Profit Factor Thresholds ---
    //const Num requiredPFHurdle       = DecimalConstants<Num>::createDecimal("0.995");
    const Num requiredPFMedianHurdle = DecimalConstants<Num>::createDecimal("1.10");

    const Num lbPF = *bootstrap.lbProfitFactor;
    
    // Check A: Lower Bound
    if (lbPF <= DecimalConstants<Num>::DecimalZero)
      {
        os << "   [HurdleAnalysis] Warning: PF Lower Bound (" << lbPF 
           << ") is non-positive.\n";
      }

    //const bool isProfitFactorStrong = (lbPF >= requiredPFHurdle);
    const bool isProfitFactorStrong = (lbPF >= dynamicPFHurdle);
    
    // Check B: Median
    bool isMedianPFStrong = false;
    if (bootstrap.medianProfitFactor.has_value())
      isMedianPFStrong = (*bootstrap.medianProfitFactor >= requiredPFMedianHurdle);

    // Apply PF Veto
    if (!isProfitFactorStrong || !isMedianPFStrong)
      {
	os << "✗ Strategy filtered out: Profit Factor validation failed.\n";
  
	if (!isProfitFactorStrong)
          os << "   ↳ Failure: PF Lower Bound " << lbPF << " < Volatility Hurdle " << dynamicPFHurdle << "\n";
      
	if (!isMedianPFStrong && bootstrap.medianProfitFactor.has_value())
          os << "   ↳ Failure: PF Median " << *bootstrap.medianProfitFactor << " < " << requiredPFMedianHurdle << "\n";

	logGateMetrics("FAIL");
	return FilterDecision::Fail(FilterDecisionType::FailHurdle,
				    "Robust Profit Factor validation failed (LB or Median)");
      }

    // Success Path
    os << "✓ Strategy passed primary validation gate.\n";
    logGateMetrics("PASS");

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
