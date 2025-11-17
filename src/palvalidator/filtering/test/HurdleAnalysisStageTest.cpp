#include <catch2/catch_test_macros.hpp>

#include "filtering/stages/HurdleAnalysisStage.h"
#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <sstream>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;

TEST_CASE("HurdleAnalysisStage: basic execution returns sensible trading-spread hurdle", "[HurdleAnalysis]")
{
  // Simplified TradingHurdleCalculator (per-side slippage only; default 0.10% per side).
  TradingHurdleCalculator calc;

  HurdleAnalysisStage stage(calc);

  // Minimal context (no backtester needed for this smoke test).
  const boost::gregorian::date d1(2020, 1, 1);
  const boost::gregorian::date d2(2020, 12, 31);
  mkc_timeseries::DateRange isRange(d1, d2);
  mkc_timeseries::DateRange oosRange(d1, d2);
  auto tf = mkc_timeseries::TimeFrame::DAILY;

  StrategyAnalysisContext ctx(
      /*backtester*/ nullptr,
      /*baseSecurity*/ nullptr,
      isRange,
      oosRange,
      tf,
      /*oosSpreadStats*/ std::nullopt);

  std::ostringstream os;

  // New API: execute(ctx, os) only (no BootstrapAnalysisResult parameter)
  const auto result = stage.execute(ctx, os);

  // Sanity checks: hurdle is computed and non-negative; log has content.
  REQUIRE(result.finalRequiredReturn >= Num(0));
  REQUIRE(!os.str().empty());
}
