#pragma once

#include <vector>
#include <ostream>
#include <string>
#include "StatisticalTypes.h"
#include "number.h"

namespace palvalidator
{
namespace analysis
{

/**
 * @brief Analyzer for strategy robustness using various statistical tests
 * 
 * This class provides comprehensive robustness analysis for trading strategies,
 * including L-sensitivity testing, split-sample validation, and tail-risk assessment.
 * All tests are based on geometric mean (GM) bootstrap confidence intervals.
 */
class RobustnessAnalyzer
{
public:
    using Num = num::DefaultNumber;

    /**
     * @brief Run comprehensive robustness checks on a strategy
     * 
     * This function performs GM-only robustness checks for strategies flagged by AM–GM divergence.
     * It includes L-sensitivity testing, split-sample stability checks, and tail-risk assessment.
     * 
     * @param label Strategy name for logging
     * @param returns Per-period net returns after costs/slippage
     * @param L_in Block length for the block sampler (median holding period)
     * @param annualizationFactor Multiplier to annualize per-period LBs
     * @param finalRequiredReturn Annualized hurdle used in the main filter
     * @param cfg Configuration parameters for the robustness checks
     * @param os Output stream for diagnostic logging
     * @return RobustnessResult indicating pass/fail and reason
     */
    static RobustnessResult runFlaggedStrategyRobustness(
        const std::string& label,
        const std::vector<Num>& returns,
        size_t L_in,
        double annualizationFactor,
        const Num& finalRequiredReturn,
        const RobustnessChecksConfig<Num>& cfg,
        std::ostream& os);

private:
    /**
     * @brief Annualize a per-period lower bound
     * @param perPeriodLB Per-period lower bound
     * @param k Annualization factor
     * @return Annualized lower bound
     */
    static Num annualizeLB_(const Num& perPeriodLB, double k);

    /**
     * @brief Get absolute value of a number
     * @param x Input number
     * @return Absolute value
     */
    static Num absNum_(const Num& x);

    /**
     * @brief Log tail risk explanation to output stream
     * @param os Output stream
     * @param perPeriodGMLB Per-period geometric mean lower bound
     * @param q05 5% quantile
     * @param es05 Expected shortfall at 5%
     * @param severeMultiple Threshold multiple for severe classification
     */
    static void logTailRiskExplanation(
        std::ostream& os,
        const Num& perPeriodGMLB,
        const Num& q05,
        const Num& es05,
        double severeMultiple);
};

} // namespace analysis
} // namespace palvalidator