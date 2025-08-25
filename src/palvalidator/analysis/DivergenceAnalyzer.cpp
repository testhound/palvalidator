#include "DivergenceAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace palvalidator
{
namespace analysis
{

DivergenceResult<DivergenceAnalyzer::Num> DivergenceAnalyzer::assessAMGMDivergence(
    const Num& gmAnn, 
    const Num& amAnn,
    double absThresh, 
    double relThresh)
{
    const double g = gmAnn.getAsDouble();
    const double a = amAnn.getAsDouble();
    const double absd = std::fabs(g - a);

    const double denom = std::max(g, a);
    DivergenceResult<Num> out{};
    out.absDiff = absd;

    if (denom > 0.0)
    {
        out.relDiff = absd / denom;
        out.relState = DivergencePrintRel::Defined;
        out.flagged = (absd > absThresh) || (out.relDiff > relThresh);
    }
    else
    {
        out.relDiff = 0.0; // meaningless; we'll print "n/a"
        out.relState = DivergencePrintRel::NotDefined;
        // Still allow flagging by absolute gap even if relative is undefined.
        out.flagged = (absd > absThresh);
    }
    return out;
}

} // namespace analysis
} // namespace palvalidator