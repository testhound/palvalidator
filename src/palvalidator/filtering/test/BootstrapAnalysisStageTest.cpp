#include <catch2/catch_test_macros.hpp>
#include "filtering/FilteringTypes.h"
#include "filtering/stages/BootstrapAnalysisStage.h"
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;

TEST_CASE("BootstrapAnalysisStage: defensive behavior with no backtester", "[BootstrapAnalysisStage]")
{
  // Prepare a minimal context: null strategy and security so stage returns defaults.
  std::shared_ptr<mkc_timeseries::PalStrategy<Num>> strat = nullptr;
  std::shared_ptr<mkc_timeseries::Security<Num>> sec = nullptr;

  using boost::gregorian::date;
  date d1(2020,1,1), d2(2020,12,31);
  mkc_timeseries::DateRange in(d1, d2), oos(d1, d2);

  StrategyAnalysisContext ctx(strat, sec, in, oos, mkc_timeseries::TimeFrame::DAILY);

  BootstrapAnalysisStage stage(Num("0.95"), /*numResamples=*/100);
  std::ostringstream os;
  auto result = stage.execute(ctx, os);

  // Defensive stage should complete without throwing and return default result
  REQUIRE(result.medianHoldBars == 0);
  // annualized lower bounds should be default-initialized (== 0)
  REQUIRE(result.annualizedLowerBoundGeo == Num(0));
}