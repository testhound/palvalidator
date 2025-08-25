#pragma once

#include <string>
#include "number.h"

namespace palvalidator
{
namespace analysis
{

using Num = num::DefaultNumber;

/**
 * @brief Configuration parameters for robustness analysis checks
 */
template<typename Num>
struct RobustnessChecksConfig
{
    unsigned int B = 1200;
    double cl = 0.95;

    // L-sensitivity
    double relVarTol = 0.25;        // keep 0.25
    size_t minL = 2;

    // Split-sample eligibility
    size_t minTotalForSplit = 40;
    size_t minHalfForSplit  = 20;

    // Tail risk
    double tailAlpha = 0.05;
    Num    tailMultiple = Num("3.0");
    Num    borderlineAnnualMargin = Num("0.02");

    // NEW: only fail on variability if we're close to the hurdle
    Num    varOnlyMarginAbs = Num("0.02");  // within +2% annual of hurdle
    double varOnlyMarginRel = 0.25;         // or within +25% relative to hurdle
};

/**
 * @brief Verdict from robustness analysis
 */
enum class RobustnessVerdict 
{ 
    ThumbsUp, 
    ThumbsDown 
};

/**
 * @brief Reasons for robustness analysis failure
 */
enum class RobustnessFailReason
{
    None = 0,
    LSensitivityBound,        ///< a bound at {L-1,L,L+1} ≤ 0 or ≤ hurdle
    LSensitivityVarNearHurdle,///< variability too high AND base near hurdle
    SplitSample,              ///< a half ≤ 0 or ≤ hurdle
    TailRisk                  ///< severe tails + borderline base
};

/**
 * @brief Result of robustness analysis
 */
struct RobustnessResult
{
    RobustnessVerdict verdict;
    RobustnessFailReason reason;
    double relVar; ///< for logging/diagnostics
};

/**
 * @brief State for divergence analysis relative difference printing
 */
enum class DivergencePrintRel 
{ 
    Defined, 
    NotDefined 
};

/**
 * @brief Result of AM vs GM divergence analysis
 */
template<typename Num>
struct DivergenceResult
{
    bool flagged;
    double absDiff;       ///< absolute annualized difference (as a fraction, not %)
    double relDiff;       ///< relative annualized difference (abs/max), undefined if max<=0
    DivergencePrintRel relState;
};

/**
 * @brief Actions that can be taken for fragile edge strategies
 */
enum class FragileEdgeAction 
{ 
    Keep, 
    Downweight, 
    Drop 
};

/**
 * @brief Decision result for fragile edge analysis
 */
template<typename Num>
struct FragileDecision
{
    FragileEdgeAction action;
    double weightMultiplier;        ///< 1.0 for Keep; <1.0 for Downweight; 0 for Drop
    std::string rationale;
};

/**
 * @brief Policy configuration for fragile edge analysis
 */
struct FragileEdgePolicy
{
    double relVarDown    = 0.35;    ///< if L-sensitivity relVar > this → consider downweight
    double relVarDrop    = 0.60;    ///< if relVar is huge and near hurdle → consider drop
    double tailMultiple  = 3.0;     ///< "severe tail" if |q05| > tailMultiple × per-period GM LB
    double nearAbs       = 0.02;    ///< "near hurdle" if |LB_ann - hurdle| ≤ nearAbs (2pp)
    double nearRel       = 0.10;    ///< …or within 10% of hurdle
    size_t minNDown      = 30;      ///< small n → consider downweight (never drop on size alone)
};

} // namespace analysis
} // namespace palvalidator