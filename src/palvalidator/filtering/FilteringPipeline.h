#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"
#include "filtering/PerformanceFilter.h"
#include "filtering/stages/BacktestingStage.h"
#include "filtering/stages/BootstrapAnalysisStage.h"
#include "filtering/stages/HurdleAnalysisStage.h"
#include "filtering/stages/RobustnessStage.h"
#include "filtering/stages/LSensitivityStage.h"
#include "filtering/stages/RegimeMixStage.h"
#include "filtering/stages/FragileEdgeStage.h"
#include <ostream>
#include <memory>

namespace palvalidator::filtering
{
  /**
   * @brief Orchestrates the complete filtering pipeline for a single strategy
   *
   * This class ensures stages execute in the same order as the original
   * monolithic method, preserving all output, gating logic, and behavior.
   *
   * FAIL-FAST: Pipeline stops at first gate failure, just like original
   *
   * Execution order (matching legacy lines 67-272):
   * 1. Backtesting (lines 67-82) - GATE: ≥20 returns
   * 2. Bootstrap Analysis (lines 84-118) - no gate
   * 3. Hurdle Analysis (lines 122-157) - GATE: pass base AND 1Qn
   * 4. Robustness if needed (lines 160-184) - GATE: thumbs up required
   * 5. L-Sensitivity if enabled (lines 188-230) - GATE: pass fraction & gap
   * 6. Regime-Mix (lines 233-249) - GATE: pass fraction of mixes
   * 7. Fragile Edge (lines 254-258) - GATE: not dropped by policy
   * 8. Final logging if passed (lines 263-272)
   */
  class FilteringPipeline
  {
  public:
    /**
     * @brief Constructor with all necessary configuration
     * @param hurdleCalc Trading hurdle calculator for cost and risk hurdles
     * @param confidenceLevel Confidence level for bootstrap (e.g., 0.95)
     * @param numResamples Number of bootstrap resamples (e.g., 2000)
     * @param robustnessConfig Configuration for robustness checks
     * @param lSensitivityConfig Configuration for L-sensitivity stress
     * @param fragileEdgePolicy Policy for fragile edge analysis
     * @param applyFragileAdvice Whether to apply fragile edge recommendations
     * @param summary Reference to FilteringSummary for tracking results
     */
    FilteringPipeline(
      const TradingHurdleCalculator& hurdleCalc,
      const Num& confidenceLevel,
      unsigned int numResamples,
      const RobustnessChecksConfig& robustnessConfig,
      const PerformanceFilter::LSensitivityConfig& lSensitivityConfig,
      const FragileEdgePolicy& fragileEdgePolicy,
      bool applyFragileAdvice,
      FilteringSummary& summary);

    /**
     * @brief Execute complete filtering pipeline for a single strategy
     * @param ctx Strategy analysis context (input/output)
     * @param os Output stream for logging
     * @return FilterDecision with overall pass/fail status
     *
     * STOPS IMMEDIATELY at first failure (fail-fast)
     */
    FilterDecision executeForStrategy(
      StrategyAnalysisContext& ctx,
      std::ostream& os);

  private:
    // Stage instances
    stages::BacktestingStage mBacktestingStage;
    stages::BootstrapAnalysisStage mBootstrapStage;
    stages::HurdleAnalysisStage mHurdleStage;
    stages::RobustnessStage mRobustnessStage;
    stages::LSensitivityStage mLSensitivityStage;
    stages::RegimeMixStage mRegimeMixStage;
    stages::FragileEdgeStage mFragileEdgeStage;

    // Configuration references
    const RobustnessChecksConfig& mRobustnessConfig;
    const PerformanceFilter::LSensitivityConfig& mLSensitivityConfig;

    /**
     * @brief Execute L-sensitivity stress testing if enabled
     * @param ctx Strategy analysis context
     * @param bootstrap Bootstrap results (for block length)
     * @param hurdle Hurdle results (for finalRequiredReturn)
     * @param outRelVar Output parameter for relative variance
     * @param os Output stream for logging
     * @return FilterDecision (Pass or FailLSensitivity)
     */
    FilterDecision executeLSensitivityStress(
      const StrategyAnalysisContext& ctx,
      const BootstrapAnalysisResult& bootstrap,
      const HurdleAnalysisResult& hurdle,
      double& outRelVar,
      std::ostream& os) const;

    /**
     * @brief Log final success message for passed strategy
     * @param ctx Strategy analysis context
     * @param bootstrap Bootstrap results for logging bounds
     * @param hurdle Hurdle results for logging required return
     * @param os Output stream
     */
    void logPassedStrategy(
      const StrategyAnalysisContext& ctx,
      const BootstrapAnalysisResult& bootstrap,
      const HurdleAnalysisResult& hurdle,
      std::ostream& os) const;
  };

} // namespace palvalidator::filtering