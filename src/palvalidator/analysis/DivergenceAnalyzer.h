#pragma once

#include "StatisticalTypes.h"
#include "number.h"

namespace palvalidator
{
namespace analysis
{

/**
 * @brief Analyzer for AM vs GM lower-bound divergence detection
 * 
 * This class provides diagnostic analysis of divergence between arithmetic mean (AM)
 * and geometric mean (GM) bootstrap confidence intervals. Large divergences can indicate
 * volatility drag, outlier sensitivity, or other statistical issues that warrant
 * additional robustness testing.
 */
class DivergenceAnalyzer
{
public:
    using Num = num::DefaultNumber;

    /**
     * @brief Assess AM vs GM lower-bound divergence
     * 
     * We make decisions using the GEOMETRIC mean (GM) because it matches compounding.
     * However, we also compare the BCa *annualized* lower bounds of the ARITHMETIC mean (AM)
     * and GM as a cheap, informative warning signal. This function computes:
     * 
     *   abs_gap = | LB_ann(GM) - LB_ann(AM) |
     *   rel_gap = abs_gap / max(LB_ann(GM), LB_ann(AM))   // guarded against 0
     * 
     * and returns them so the caller can decide whether to flag the strategy and run
     * deeper robustness checks (L-sensitivity, split-sample when n>=40, tail-risk).
     * 
     * Why keep AM if we filter on GM?
     *  - Volatility drag proxy: for small returns, GM ≈ AM − ½·Var(r). A large AM–GM gap
     *    is a red flag that variance and/or skew/fat tails are hurting true compounding.
     *  - Different influence functions: AM (linear) and GM (log-domain) react differently
     *    to outliers, zeros, and serial dependence; disagreement is a useful "smoke detector"
     *    for shape/resampling sensitivity or data/plumbing mistakes.
     *  - Sanity & transparency: printing AM alongside GM helps diagnose unexpected shifts
     *    (e.g., slippage handling, transform errors) without changing the pass/fail rule.
     * 
     * Important:
     *  - This divergence is **diagnostic only**—it does NOT accept or reject a strategy.
     *    It merely gates the robustness suite. Final acceptance remains GM-LB vs hurdle.
     *  - Thresholds for abs/rel gaps are heuristics; tune per risk tolerance and sample size.
     *    Near-hurdle strategies deserve extra scrutiny even for modest gaps.
     *  - Divergences can occur legitimately (finite samples, BCa asymmetry, block resampling),
     *    so never drop solely on AM–GM gap—always confirm with the robustness checks.
     * 
     * @param gmAnn Annualized geometric mean lower bound
     * @param amAnn Annualized arithmetic mean lower bound
     * @param absThresh Absolute threshold for flagging (default: 0.05 = 5pp)
     * @param relThresh Relative threshold for flagging (default: 0.30 = 30%)
     * @return DivergenceResult indicating whether strategy should be flagged for robustness testing
     */
    static DivergenceResult<Num> assessAMGMDivergence(
        const Num& gmAnn, 
        const Num& amAnn,
        double absThresh = 0.05, 
        double relThresh = 0.30);
};

} // namespace analysis
} // namespace palvalidator