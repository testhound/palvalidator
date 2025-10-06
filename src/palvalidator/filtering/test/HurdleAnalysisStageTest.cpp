#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "filtering/stages/HurdleAnalysisStage.h"
#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <iostream>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;

TEST_CASE("HurdleAnalysisStage: basic execution returns sensible hurdle", "[HurdleAnalysis]")
{
  // Build simple risk parameters required by TradingHurdleCalculator
  palvalidator::utils::RiskParameters rp;
  rp.riskFreeRate = Num("0.01");  // 1%
  rp.riskPremium  = Num("0.02");  // 2%

  // Construct TradingHurdleCalculator with required parameters
  TradingHurdleCalculator calc(rp);

  HurdleAnalysisStage stage(calc);

  // Prepare a minimal context. DateRange requires two dates.
  const boost::gregorian::date d1(2020, 1, 1);
  const boost::gregorian::date d2(2020, 12, 31);
  mkc_timeseries::DateRange inSample(d1, d2);
  mkc_timeseries::DateRange oosSample(d1, d2);

  auto tf = mkc_timeseries::TimeFrame::DAILY; // use DAILY timeframe for the test

  StrategyAnalysisContext ctx(
    nullptr,
    nullptr,
    inSample,
    oosSample,
    tf,
    std::nullopt
  );

  BootstrapAnalysisResult boot;
  boot.annualizedLowerBoundGeo = Num("0.01");   // 1%
  boot.annualizedLowerBoundMean = Num("0.015"); // 1.5%

  std::ostringstream os;
  const auto result = stage.execute(ctx, boot, os);

  // Basic sanity checks: finalRequiredReturn must be a numeric value (non-NaN).
  REQUIRE(result.finalRequiredReturn >= Num(0));

  // Ensure the stage wrote something to the log stream (concise cost-stress output)
  REQUIRE(!os.str().empty());
}