#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/ValidationPolicy.h"
#include "filtering/TradingHurdleCalculator.h"
#include "filtering/PerformanceFilter.h"
#include "filtering/stages/BacktestingStage.h"
#include "filtering/stages/BootstrapAnalysisStage.h"
#include "filtering/stages/HurdleAnalysisStage.h"
#include "filtering/stages/RobustnessStage.h"
#include "filtering/stages/LSensitivityStage.h"
#include "filtering/stages/RegimeMixStage.h"
#include "filtering/stages/FragileEdgeStage.h"
#include "filtering/BootstrapConfig.h"
#include <ostream>
#include <memory>

namespace palvalidator::filtering
{
    /**
     * @brief Orchestrates the complete filtering pipeline for a single strategy.
     *
     * This class executes a sequence of validation stages to determine if a
     * trading strategy is viable. The pipeline is designed to be "fail-fast,"
     * stopping at the first stage that indicates a failure.
     *
     * The new execution order is as follows:
     * 1. Backtesting: Ensures the strategy produces a sufficient number of trades.
     * 2. Bootstrap Analysis: Calculates the lower bound of returns.
     * 3. Hurdle Calculation: Determines the trading spread cost.
     * 4. Centralized Validation: Applies the `ValidationPolicy` to make a definitive
     *    pass/fail decision.
     * 5. Supplemental Analysis (if passed):
     *    - Robustness Checks
     *    - L-Sensitivity Stress Test
     *    - Regime-Mix Analysis
     *    - Fragile Edge Advisory
     * 6. Final Logging: Records the outcome for the strategy.
     */
  class FilteringPipeline
  {
  public:
    /**
     * @brief Constructor with all necessary configuration.
     * @param hurdleCalc Trading hurdle calculator for cost hurdles.
     * @param confidenceLevel Confidence level for bootstrap (e.g., 0.95).
     * @param numResamples Number of bootstrap resamples (e.g., 2000)
     * @param robustnessConfig Configuration for robustness checks.
     * @param lSensitivityConfig Configuration for L-sensitivity stress.
     * @param fragileEdgePolicy Policy for fragile edge analysis.
     * @param applyFragileAdvice Whether to apply fragile edge recommendations.
     * @param summary Reference to FilteringSummary for tracking results.
     * @param bootstrapFactory Factory for creating bootstrap instances.
     */
    FilteringPipeline(const TradingHurdleCalculator& hurdleCalc,
                          const Num& confidenceLevel,
                          unsigned int numResamples,
                          const RobustnessChecksConfig& robustnessConfig,
                           const PerformanceFilter::LSensitivityConfig& lSensitivityConfig,
                           const FragileEdgePolicy& fragileEdgePolicy,
                           bool applyFragileAdvice,
                           FilteringSummary& summary,
                           BootstrapFactory& bootstrapFactory,
                           std::shared_ptr<palvalidator::diagnostics::IBootstrapObserver> observer);

    /**
     * @brief Execute complete filtering pipeline for a single strategy
     * @param ctx Strategy analysis context (input/output).
     * @param os Output stream for logging.
     * @return FilterDecision with overall pass/fail status.
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

    // Configuration references (must be before mBootstrapFactory since they're initialized first)
    const RobustnessChecksConfig& mRobustnessConfig;
    const PerformanceFilter::LSensitivityConfig& mLSensitivityConfig;
    BootstrapFactory& mBootstrapFactory;

    /**
     * @brief Execute L-sensitivity stress testing if enabled.
     * @param ctx Strategy analysis context.
     * @param bootstrap Bootstrap results (for block length).
     * @param hurdle Hurdle results (for trading spread cost).
     * @param outRelVar Output parameter for relative variance.
     * @param os Output stream for logging.
     * @return FilterDecision (Pass or FailLSensitivity).
     */
    FilterDecision executeLSensitivityStress(
        const StrategyAnalysisContext& ctx,
        const BootstrapAnalysisResult& bootstrap,
        const HurdleAnalysisResult& hurdle,
        double& outRelVar,
	const Num& requiredReturn,
        std::ostream& os) const;

    /**
     * @brief Log final success message for a passed strategy.
     * @param ctx Strategy analysis context.
     * @param bootstrap Bootstrap results for logging bounds.
     * @param policy The validation policy containing the required return.
     * @param os Output stream.
     */
    void logPassedStrategy(
        const StrategyAnalysisContext& ctx,
        const BootstrapAnalysisResult& bootstrap,
        const ValidationPolicy& policy,
        std::ostream& os) const;
  };

} // namespace palvalidator::filtering
