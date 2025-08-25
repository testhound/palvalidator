#include "FragileEdgeAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "DecimalConstants.h"

using namespace mkc_timeseries;

namespace palvalidator
{
namespace analysis
{

FragileDecision<FragileEdgeAnalyzer::Num> FragileEdgeAnalyzer::analyzeFragileEdge(
    const Num& lbPer_GM,
    const Num& lbAnn_GM,
    const Num& hurdleAnn,
    double relVarL,
    const Num& q05,
    const Num& es05,
    size_t n,
    const FragileEdgePolicy& pol)
{
    const double edge  = lbPer_GM.getAsDouble();
    const double q     = q05.getAsDouble();
    (void)es05; // ES05 is logged elsewhere; not used in the simple rule below.

    const double lbAnn = lbAnn_GM.getAsDouble();
    const double hdAnn = hurdleAnn.getAsDouble();

    const double absGap = std::fabs(lbAnn - hdAnn);
    const bool nearHurdle = (absGap <= pol.nearAbs)
                         || ((hdAnn > 0.0) && (absGap / hdAnn <= pol.nearRel));

    const bool severeTail = (q < 0.0) && (edge > 0.0) && (std::fabs(q) > pol.tailMultiple * edge);

    // --- Heuristic tree (advisory) ---
    // 1) Severe tails + near hurdle → DROP (too fragile to rely on)
    if (severeTail && nearHurdle) {
        return {FragileEdgeAction::Drop, 0.0,
                "Severe downside tails and LB near hurdle → drop"};
    }

    // 2) Very large L-variability and near hurdle → DROP
    if (relVarL > pol.relVarDrop && nearHurdle) {
        return {FragileEdgeAction::Drop, 0.0,
                "High L-sensitivity and LB near hurdle → drop"};
    }

    // 3) Otherwise "soft" reasons to reduce exposure
    if (severeTail || relVarL > pol.relVarDown || n < pol.minNDown)
    {
        std::string why;
        if (severeTail)
            why += "severe tails; ";

        if (relVarL > pol.relVarDown)
            why += "high L-variability; ";

        if (n < pol.minNDown)
            why += "small sample; ";
        return {FragileEdgeAction::Downweight, 0.50,
                "Advisory downweight: " + (why.empty() ? std::string("weak signal") : why)};
    }

    // 4) Default keep
    return {FragileEdgeAction::Keep, 1.0, "Robust enough to keep at full weight"};
}

std::pair<FragileEdgeAnalyzer::Num, FragileEdgeAnalyzer::Num> FragileEdgeAnalyzer::computeQ05_ES05(
    const std::vector<Num>& returns, 
    double alpha)
{
    if (returns.empty())
        return {DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero};

    std::vector<Num> sorted = returns;
    std::sort(sorted.begin(), sorted.end(), [](const Num& a, const Num& b){ return a < b; });

    const size_t n = sorted.size();
    size_t k = static_cast<size_t>(std::floor(alpha * static_cast<double>(n)));
    if (k >= n)
        k = n - 1;

    const Num q05 = sorted[k];

    Num sumTail = DecimalConstants<Num>::DecimalZero;
    for (size_t i = 0; i <= k; ++i)
        sumTail += sorted[i];

    const Num es05 = (k + 1 > 0) ? (sumTail / Num(static_cast<int>(k + 1))) : q05;

    return {q05, es05};
}

} // namespace analysis
} // namespace palvalidator