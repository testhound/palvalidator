#include "filtering/stages/FragileEdgeStage.h"
#include "analysis/FragileEdgeAnalyzer.h"
#include "StatUtils.h"
#include "DecimalConstants.h"
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
    using palvalidator::analysis::FragileEdgeAction;
    using palvalidator::analysis::FragileEdgeAnalyzer;
    using mkc_timeseries::DecimalConstants;
    using Num = palvalidator::filtering::Num;

    const auto& r = ctx.highResReturns;
    const size_t n = r.size();

    if (n == 0) {
      os << "   [ADVISORY/FragileEdge] no returns; skipping tail analysis.\n";
      return FilterDecision::Pass("FragileEdge: no-op (empty series)");
    }

    // 1) Compute tails via your analyzer, then apply small-N ES shrink toward Q05.
    auto [q05_raw, es05_raw] = FragileEdgeAnalyzer::computeQ05_ES05(r, /*alpha=*/0.05); // returns Num, Num

    // Tuned to your context (~20 returns median)
    constexpr size_t nSmall = 20;     // max shrink at/below this N
    constexpr size_t nLarge = 35;     // no shrink at/above this N
    constexpr double lambdaMax = 0.60; // up to 60% shrink at tiny N

    double lambda = 0.0;
    if (n <= nSmall) {
      lambda = lambdaMax;
    } else if (n < nLarge) {
      const double t = double(nLarge - n) / double(nLarge - nSmall);
      lambda = lambdaMax * std::max(0.0, std::min(1.0, t));
    }
    const Num es05_smooth = Num(1.0 - lambda) * es05_raw + Num(lambda) * q05_raw;

    // 2) Run your analyzer with the smoothed ES05.
    const auto advice = FragileEdgeAnalyzer::analyzeFragileEdge(
								bootstrap.lbGeoPeriod,              // per-period GM LB
								bootstrap.annualizedLowerBoundGeo,  // annualized GM LB
								hurdle.finalRequiredReturn,         // hurdle (annual)
								lSensitivityRelVar,                 // relVar from L-sensitivity / robustness
								q05_raw,                            // tail quantile
								es05_smooth,                        // smoothed ES05
								n,                                  // sample size
								mPolicy);                           // thresholds

    // 3) Strict-mode composition:
    //    - Do not DROP on tails-only when N is tiny (unless compounded risks).
    //    - Always upgrade Downweight -> Drop when (Downweight + highRelVar + nearHurdle).
    constexpr size_t minNForTailDrop = 22;         // tails-only block when n < 22
    const bool tinyN = (n < minNForTailDrop);

    const bool highRelVar = (lSensitivityRelVar > 0.50);
    const Num margin = bootstrap.annualizedLowerBoundGeo - hurdle.finalRequiredReturn;
    const bool nearHurdle = (margin < Num(0.0025)); // within 0.25%/yr

    FragileEdgeAction finalAction = advice.action;
    std::string finalWhy = advice.rationale;

    if (finalAction == FragileEdgeAction::Drop && tinyN && !(highRelVar && nearHurdle)) {
      finalAction = FragileEdgeAction::Downweight;
      finalWhy += " | downgraded to Downweight due to tiny N without compounded risks";
    }

    if (finalAction == FragileEdgeAction::Downweight && highRelVar && nearHurdle) {
      finalAction = FragileEdgeAction::Drop;
      finalWhy += " | strict-mode: upgraded to Drop (Downweight + high relVar + near hurdle)";
    }

    // 4) Logging
    os << "   [ADVISORY/FragileEdge] action="
       << (finalAction == FragileEdgeAction::Keep ? "Keep" :
	   finalAction == FragileEdgeAction::Downweight ? "Downweight" : "Drop")
       << " (policy=" << (advice.action == FragileEdgeAction::Keep ? "Keep" :
			  advice.action == FragileEdgeAction::Downweight ? "Downweight" : "Drop") << ")"
       << ", n=" << n
       << ", Q05=" << (q05_raw * DecimalConstants<Num>::DecimalOneHundred) << "% "
       << ", ES05_raw=" << (es05_raw * DecimalConstants<Num>::DecimalOneHundred) << "% "
       << ", ES05_smooth=" << (es05_smooth * DecimalConstants<Num>::DecimalOneHundred) << "% "
       << ", λ=" << lambda
       << ", relVar=" << lSensitivityRelVar
       << ", lbAnn=" << (bootstrap.annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred) << "% "
       << ", hurdle=" << (hurdle.finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "% "
       << ", nearHurdle=" << (nearHurdle ? "true" : "false")
       << ", highRelVar=" << (highRelVar ? "true" : "false")
       << " → " << finalWhy << "\n";

    // 5) Gate if final action is Drop and applyAdvice is enabled
    if (finalAction == FragileEdgeAction::Drop && mApplyAdvice) {
      return FilterDecision::Fail(FilterDecisionType::FailFragileEdge, finalWhy);
    }

    return FilterDecision::Pass("FragileEdge: " + finalWhy);
  }
} // namespace palvalidator::filtering::stages
