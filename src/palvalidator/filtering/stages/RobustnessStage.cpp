#include "filtering/stages/RobustnessStage.h"
#include "analysis/RobustnessAnalyzer.h"
#include "PalStrategy.h"
#include "DecimalConstants.h"
#include <sstream>
#include <stdexcept>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  FilterDecision RobustnessStage::execute(StrategyAnalysisContext& ctx,
                                         const palvalidator::analysis::DivergenceResult<Num>& divergence,
                                         bool nearHurdle,
                                         bool smallN,
                                         std::ostream& os) const
  {
    // Defensive: ensure we have the inputs we need
    const std::string strategyName = ctx.strategy ? ctx.strategy->getStrategyName() : std::string("<unknown>");
    if (ctx.highResReturns.empty())
    {
      throw std::runtime_error("RobustnessStage::execute - empty returns");
    }

    // Logging similar to original implementation
    if (divergence.flagged)
    {
      mSummary.incrementFlaggedCount();
      os << "   [FLAG] Large AM vs GM divergence (abs="
         << (Num(divergence.absDiff) * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%, rel=";
      if (divergence.relState == palvalidator::analysis::DivergencePrintRel::Defined)
        os << divergence.relDiff;
      else
        os << "n/a";
      os << "); running robustness checks";

      if (nearHurdle || smallN)
      {
        os << " (also triggered by ";
        if (nearHurdle) os << "near-hurdle";
        if (nearHurdle && smallN) os << " & ";
        if (smallN) os << "small-sample";
        os << ")";
      }
      os << "...\n";
    }
    else
    {
      os << "   [CHECK] Running robustness checks due to "
         << (nearHurdle ? "near-hurdle" : "")
         << ((nearHurdle && smallN) ? " & " : "")
         << (smallN ? "small-sample" : "")
         << " condition(s)...\n";
    }

    // Execute robustness analyzer (returns a RobustnessResults or similar)
    // Ensure we have a valid cloned strategy for the bootstrap factory
    if (!ctx.clonedStrategy) {
        throw std::runtime_error("RobustnessStage::execute - clonedStrategy is null - cannot proceed with robustness analysis");
    }
    
    // Pass the cached L-grid result if available (avoids redundant computation)
    const auto& gridOpt = ctx.lgrid_result;
    
    const auto rob = palvalidator::analysis::RobustnessAnalyzer::runFlaggedStrategyRobustness(
        strategyName,
        ctx.highResReturns,
        ctx.blockLength,
        ctx.annualizationFactor,
        ctx.finalRequiredReturn,
        mCfg,
        *ctx.clonedStrategy,
        mBootstrapFactory,
        os,
        gridOpt);

    if (rob.verdict == palvalidator::analysis::RobustnessVerdict::ThumbsDown)
    {
      // Map failure reason to summary counters (preserve original mapping)
      switch (rob.reason)
      {
        case palvalidator::analysis::RobustnessFailReason::LSensitivityBound:
          mSummary.incrementFailLBoundCount();
          break;
        case palvalidator::analysis::RobustnessFailReason::LSensitivityVarNearHurdle:
          mSummary.incrementFailLVarCount();
          break;
        case palvalidator::analysis::RobustnessFailReason::SplitSample:
          mSummary.incrementFailSplitCount();
          break;
        case palvalidator::analysis::RobustnessFailReason::TailRisk:
          mSummary.incrementFailTailCount();
          break;
        default:
          break;
      }

      os << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]") << " Robustness checks FAILED → excluding strategy.\n\n";
      return FilterDecision::Fail(FilterDecisionType::FailRobustness, "Robustness failed");
    }
    else
    {
      if (divergence.flagged)
        mSummary.incrementFlagPassCount();

      os << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]") << " Robustness checks PASSED.\n";
      return FilterDecision::Pass();
    }
  }

} // namespace palvalidator::filtering::stages