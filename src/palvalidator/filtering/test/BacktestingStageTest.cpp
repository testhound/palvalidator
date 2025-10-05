#include <catch2/catch_test_macros.hpp>
#include "filtering/FilteringTypes.h"
#include "filtering/stages/BacktestingStage.h"
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;

TEST_CASE("BacktestingStage: null strategy yields Fail", "[BacktestingStage]")
{
  // Prepare a context with null strategy/security and valid DateRange
  std::shared_ptr<mkc_timeseries::PalStrategy<Num>> strat = nullptr;
  std::shared_ptr<mkc_timeseries::Security<Num>> sec = nullptr;

  using boost::gregorian::date;
  date d1(2020,1,1), d2(2020,12,31);
  mkc_timeseries::DateRange in(d1, d2), oos(d1, d2);

  StrategyAnalysisContext ctx(strat, sec, in, oos, mkc_timeseries::TimeFrame::DAILY);

  BacktestingStage stage;
  std::ostringstream os;
  auto decision = stage.execute(ctx, os);

  REQUIRE_FALSE(decision.passed());
  REQUIRE(decision.decision == FilterDecisionType::FailInsufficientData);
  // Rationale should contain "Backtest error" per implementation
  REQUIRE(decision.rationale.find("Backtest error") != std::string::npos);
}