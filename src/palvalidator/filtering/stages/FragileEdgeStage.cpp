#include "filtering/stages/FragileEdgeStage.h"
#include "analysis/FragileEdgeAnalyzer.h"
#include <sstream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  FilterDecision FragileEdgeStage::execute(
    const StrategyAnalysisContext& ctx,
    const BootstrapAnalysisResult& bootstrap,
    const HurdleAnalysisResult& hurdle,
    double lSensitivityRelVar,
    std::ostream& os) const
  {
    // Compute tail risk metrics (Q05 and ES05)
    const auto [q05, es05] = palvalidator::analysis::FragileEdgeAnalyzer::computeQ05_ES05(
      ctx.highResReturns, /*alpha=*/0.05);

    // Analyze fragile edge characteristics and get advisory
    const auto advice = palvalidator::analysis::FragileEdgeAnalyzer::analyzeFragileEdge(
      bootstrap.lbGeoPeriod,              // per-period GM LB
      bootstrap.annualizedLowerBoundGeo,  // annualized GM LB
      hurdle.finalRequiredReturn,         // hurdle (annual)
      lSensitivityRelVar,                 // relVar from robustness; 0.0 if unrun
      q05,                                // tail quantile
      es05,                               // ES05 (logged elsewhere)
      ctx.highResReturns.size(),          // n
      mPolicy                             // thresholds
    );

    // Helper to convert action enum to text
    auto fragileActionToText = [](palvalidator::analysis::FragileEdgeAction a) -> const char*
    {
      switch (a)
      {
        case palvalidator::analysis::FragileEdgeAction::Keep:       return "Keep";
        case palvalidator::analysis::FragileEdgeAction::Downweight: return "Downweight";
        case palvalidator::analysis::FragileEdgeAction::Drop:       return "Drop";
        default:                                                     return "Keep";
      }
    };

    // Log advisory
    os << "   [ADVISORY] Fragile edge assessment: action="
       << fragileActionToText(advice.action)
       << ", weight×=" << advice.weightMultiplier
       << " — " << advice.rationale << "\n";

    // Apply advisory if enabled
    if (mApplyAdvice)
    {
      if (advice.action == palvalidator::analysis::FragileEdgeAction::Drop)
      {
        os << "   [ADVISORY] Apply=ON → dropping strategy per fragile-edge policy.\n\n";
        return FilterDecision::Fail(FilterDecisionType::FailFragileEdge,
                                    "Dropped by fragile edge policy: " + advice.rationale);
      }
      if (advice.action == palvalidator::analysis::FragileEdgeAction::Downweight)
      {
        os << "   [ADVISORY] Apply=ON → (not implemented here) would downweight this strategy in meta.\n";
      }
    }

    return FilterDecision::Pass();
  }

} // namespace palvalidator::filtering::stages