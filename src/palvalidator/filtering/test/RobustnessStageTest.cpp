#include <catch2/catch_test_macros.hpp>
#include "filtering/stages/RobustnessStage.h"
#include "filtering/FilteringTypes.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <sstream>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;
using palvalidator::analysis::DivergenceResult;

TEST_CASE("RobustnessStage: passing and failing paths", "[RobustnessStage]")
{
  // Prepare config (defaults) and summary
  RobustnessChecksConfig cfg; // default tolerances
  FilteringSummary summary;

  RobustnessStage stage(cfg, summary);

  // Date range for context (required by StrategyAnalysisContext ctor)
  const boost::gregorian::date d1(2020,1,1);
  const boost::gregorian::date d2(2020,12,31);
  mkc_timeseries::DateRange inSample(d1, d2);
  mkc_timeseries::DateRange oosSample(d1, d2);
  auto tf = mkc_timeseries::TimeFrame::DAILY;

  // Common divergence (not flagged)
  DivergenceResult<Num> div;
  div.flagged = false;
  div.absDiff = 0.0;
  div.relDiff = 0.0;
  div.relState = palvalidator::analysis::DivergencePrintRel::NotDefined;

  // 1) Passing path: moderately positive returns -> should PASS
  {
    StrategyAnalysisContext ctx(nullptr, nullptr, inSample, oosSample, tf, std::nullopt);
    // Synthetic positive returns (0.5% per period)
    ctx.highResReturns = std::vector<Num>(100, Num("0.005"));
    ctx.blockLength = 2;
    ctx.annualizationFactor = 252.0; // daily -> annualization placeholder
    ctx.finalRequiredReturn = Num("0.001"); // small hurdle

    std::ostringstream os;
    const auto decision = stage.execute(ctx, div, /*nearHurdle*/ false, /*smallN*/ false, os);

    REQUIRE(decision.passed());
    // No robustness failure counters should be incremented
    REQUIRE(summary.getFailLBoundCount() == 0);
  }

  // 2) Failing path: strongly negative returns -> should FAIL robustness
  {
    StrategyAnalysisContext ctx(nullptr, nullptr, inSample, oosSample, tf, std::nullopt);
    // Synthetic negative returns to force low LB / tail risk
    ctx.highResReturns = std::vector<Num>(100, Num("-0.02"));
    ctx.blockLength = 2;
    ctx.annualizationFactor = 252.0;
    ctx.finalRequiredReturn = Num("0.001");

    std::ostringstream os;
    const auto decision = stage.execute(ctx, div, /*nearHurdle*/ true, /*smallN*/ false, os);

    REQUIRE(!decision.passed());
    // At least one failure counter should be incremented (split/tail/L-bound)
    const auto totalFails = summary.getFailLBoundCount() + summary.getFailLVarCount() +
                            summary.getFailSplitCount() + summary.getFailTailCount();
    REQUIRE(totalFails >= 1);
  }
}