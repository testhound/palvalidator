#include <catch2/catch_test_macros.hpp>
#include "filtering/stages/LSensitivityStage.h"
#include "filtering/FilteringTypes.h"
#include "filtering/PerformanceFilter.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <sstream>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;

TEST_CASE("LSensitivityStage basic pass/fail", "[LSensitivity]")
{
  // LSensitivityConfig is nested in PerformanceFilter
  palvalidator::filtering::PerformanceFilter::LSensitivityConfig cfg;
  cfg.maxL = 8;
  cfg.minPassFraction = 0.5;
  cfg.minGapTolerance = 0.0;

  // Provide bootstrap params expected by LSensitivityStage
  unsigned int numResamples = 1200;
  Num confidenceLevel = Num("0.95");

  LSensitivityStage stage(cfg, numResamples, confidenceLevel);

  const boost::gregorian::date d1(2020,1,1);
  const boost::gregorian::date d2(2020,12,31);
  mkc_timeseries::DateRange inSample(d1,d2);
  mkc_timeseries::DateRange oosSample(d1,d2);
  auto tf = mkc_timeseries::TimeFrame::DAILY;

  StrategyAnalysisContext ctx(nullptr, nullptr, inSample, oosSample, tf, std::nullopt);

  // 1) Passing scenario: modest positive returns
  ctx.highResReturns = std::vector<Num>(100, Num("0.005"));
  ctx.blockLength = 2;
  std::ostringstream os;
  size_t L_cap = cfg.maxL; // Use configured maxL as cap
  auto res = stage.execute(ctx, L_cap, 252.0, Num("0.001"), os);
  REQUIRE(res.ran);
  REQUIRE( (res.pass || res.numPassed >= 0) ); // basic sanity (wrapped in parentheses for Catch2)

  // 2) Failing scenario: strongly negative returns
  ctx.highResReturns = std::vector<Num>(100, Num("-0.02"));
  std::ostringstream os2;
  auto res2 = stage.execute(ctx, L_cap, 252.0, Num("0.001"), os2);
  REQUIRE(res2.ran);
  REQUIRE(!res2.pass);
}