#pragma once

#include <vector>
#include <utility>
#include "StatisticalTypes.h"
#include "number.h"

namespace palvalidator
{
namespace analysis
{

/**
 * @brief Analyzer for fragile edge detection and advisory recommendations
 * 
 * This class provides analysis of strategy fragility based on various risk factors
 * including L-sensitivity variability, tail risk, sample size, and proximity to
 * performance hurdles. It provides advisory recommendations for strategy treatment.
 */
class FragileEdgeAnalyzer
{
public:
    using Num = num::DefaultNumber;

    /**
     * @brief Analyze fragile edge characteristics and provide action recommendation
     * 
     * Advises what to do with an otherwise-PASSing strategy that looks "fragile".
     * Uses GM-centric numbers to make recommendations about keeping, downweighting,
     * or dropping strategies based on various risk factors.
     * 
     * @param lbPer_GM Baseline per-period GM BCa lower bound
     * @param lbAnn_GM Baseline annualized GM BCa LB
     * @param hurdleAnn Annualized hurdle used in main filter
     * @param relVarL L-sensitivity variability (0..1)
     * @param q05 Per-period 5% quantile (raw returns)
     * @param es05 Per-period ES05
     * @param n Sample size
     * @param pol Policy thresholds for decision making
     * @return FragileDecision with recommended action and rationale
     */
    static FragileDecision<Num> analyzeFragileEdge(
        const Num& lbPer_GM,
        const Num& lbAnn_GM,
        const Num& hurdleAnn,
        double relVarL,
        const Num& q05,
        const Num& es05,
        size_t n,
        const FragileEdgePolicy& pol);

    /**
     * @brief Compute Q05 and ES05 tail risk metrics from return series
     * 
     * @param returns Vector of per-period returns
     * @param alpha Tail quantile level (default: 0.05 for 5%)
     * @return Pair of (q05, es05) values
     */
    static std::pair<Num, Num> computeQ05_ES05(
        const std::vector<Num>& returns, 
        double alpha = 0.05);
};

} // namespace analysis
} // namespace palvalidator