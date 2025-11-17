#include "filtering/stages/RobustnessStage.h"
#include "analysis/RobustnessAnalyzer.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "DecimalConstants.h"
#include "Annualizer.h"
#include <sstream>
#include <stdexcept>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

FilterDecision RobustnessStage::execute(StrategyAnalysisContext& ctx,
                                        const palvalidator::analysis::DivergenceResult<Num>& divergence,
                                        bool nearHurdle,
                                        bool smallN,
                                        std::ostream& os,
                                        const ValidationPolicy& validationPolicy) const
{
  using mkc_timeseries::DecimalConstants;

  const std::string strategyName = ctx.strategy ? ctx.strategy->getStrategyName() : std::string("<unknown>");
  if (ctx.highResReturns.empty())
    throw std::runtime_error("RobustnessStage::execute - empty returns");

  if (divergence.flagged) {
    mSummary.incrementFlaggedCount();
    os << "   [FLAG] Large AM vs GM divergence (abs="
       << (Num(divergence.absDiff) * DecimalConstants<Num>::DecimalOneHundred)
       << "%, rel=" << (divergence.relState == palvalidator::analysis::DivergencePrintRel::Defined ? divergence.relDiff : 0)
       << "); running robustness checks";
    if (nearHurdle || smallN) {
      os << " (also triggered by ";
      if (nearHurdle) os << "near-hurdle";
      if (nearHurdle && smallN) os << " & ";
      if (smallN) os << "small-sample";
      os << ")";
    }
    os << "...\n";
  } else {
    os << "   [CHECK] Running robustness checks due to "
       << (nearHurdle ? "near-hurdle" : "")
       << ((nearHurdle && smallN) ? " & " : "")
       << (smallN ? "small-sample" : "")
       << " condition(s)...\n";
  }

  if (!ctx.clonedStrategy)
    throw std::runtime_error("RobustnessStage::execute - clonedStrategy is null - cannot proceed with robustness analysis");

  const auto& gridOpt = ctx.lgrid_result;

  // ── Annualization policy (NEW) ───────────────────────────────────────────
  double annUsed = 0.0;

  if (ctx.annualizationFactor > 0.0) {
    annUsed = ctx.annualizationFactor; // bars/year from Bootstrap stage
    os << "   [Robustness] annualization (bars/year from pipeline) = "
       << annUsed << "\n";
  } else {
    double lambdaTradesPerYear = 0.0;
    unsigned int medianHoldBars = 0U;

    try {
      if (ctx.backtester) {
        lambdaTradesPerYear = ctx.backtester->getEstimatedAnnualizedTrades(); // λ
        medianHoldBars = ctx.backtester
                           ->getClosedPositionHistory()
                           .getMedianHoldingPeriod();                         // bars/trade
      }
    } catch (...) {
      // swallow; handled by fallback
    }

    const double barsPerYear = lambdaTradesPerYear * static_cast<double>(medianHoldBars);
    if (barsPerYear > 0.0) {
      annUsed = barsPerYear;
      os << "   [Robustness] annualization (bars/year via λ×medianHoldBars) = "
         << annUsed
         << "  [λ=" << lambdaTradesPerYear
         << ", medianHoldBars=" << medianHoldBars << "]\n";
    } else {
      annUsed = 252.0;
      os << "   [Robustness] Warning: λ×medianHoldBars unavailable; defaulting to 252.\n";
    }
  }
  // ─────────────────────────────────────────────────────────────────────────

  const auto rob = palvalidator::analysis::RobustnessAnalyzer::runFlaggedStrategyRobustness(
      strategyName,
      ctx.highResReturns,
      ctx.blockLength,
      annUsed,
      validationPolicy,
      mCfg,
      *ctx.clonedStrategy,
      mBootstrapFactory,
      os,
      gridOpt);

  if (rob.verdict == palvalidator::analysis::RobustnessVerdict::ThumbsDown) {
    switch (rob.reason) {
      case palvalidator::analysis::RobustnessFailReason::LSensitivityBound:
        mSummary.incrementFailLBoundCount(); break;
      case palvalidator::analysis::RobustnessFailReason::LSensitivityVarNearHurdle:
        mSummary.incrementFailLVarCount();   break;
      case palvalidator::analysis::RobustnessFailReason::SplitSample:
        mSummary.incrementFailSplitCount();  break;
      case palvalidator::analysis::RobustnessFailReason::TailRisk:
        mSummary.incrementFailTailCount();   break;
      default: break;
    }
    os << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]")
       << " Robustness checks FAILED → excluding strategy.\n\n";
    return FilterDecision::Fail(FilterDecisionType::FailRobustness, "Robustness failed");
  } else {
    if (divergence.flagged) mSummary.incrementFlagPassCount();
    os << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]")
       << " Robustness checks PASSED.\n";
    return FilterDecision::Pass();
  }
}
  
} // namespace palvalidator::filtering::stages
