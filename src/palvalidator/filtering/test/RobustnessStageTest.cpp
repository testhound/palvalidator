#include <catch2/catch_test_macros.hpp>
#include "filtering/stages/RobustnessStage.h"
#include "filtering/FilteringTypes.h"
#include "filtering/TradingBootstrapFactory.h"
#include "ConfigSeeds.h"
#include "PalStrategy.h"
#include "TestUtils.h"
#include "Portfolio.h"
#include "Security.h"
#include "DecimalConstants.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <sstream>

using namespace palvalidator::filtering;
using namespace palvalidator::filtering::stages;
using namespace mkc_timeseries;
using palvalidator::analysis::DivergenceResult;
using palvalidator::bootstrap_cfg::BootstrapFactory;

// Helper functions copied from PalStrategyTestHelpers.cpp since we can't link to it
static std::shared_ptr<LongMarketEntryOnOpen> createLongOnOpen()
{
  return std::make_shared<LongMarketEntryOnOpen>();
}

static std::shared_ptr<LongSideProfitTargetInPercent> createLongProfitTarget(const std::string& targetPct)
{
  return std::make_shared<LongSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<LongSideStopLossInPercent> createLongStopLoss(const std::string& targetPct)
{
  return std::make_shared<LongSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

// Helper function to create a simple long pattern for testing
static std::shared_ptr<PriceActionLabPattern> createSimpleLongPattern()
{
  auto percentLong = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("TestPattern.txt", 1, 20200101,
                                                   percentLong, percentShort, 1, 1);

  // Simple pattern: Close of 0 bars ago > Close of 1 bar ago (upward momentum)
  auto close0 = std::make_shared<PriceBarClose>(0);
  auto close1 = std::make_shared<PriceBarClose>(1);
  auto longPattern = std::make_shared<GreaterThanExpr>(close0, close1);

  auto entry = createLongOnOpen();
  auto target = createLongProfitTarget("5.00");
  auto stop = createLongStopLoss("2.50");

  return std::make_shared<PriceActionLabPattern>(desc, longPattern, entry, target, stop);
}

TEST_CASE("RobustnessStage: passing and failing paths", "[RobustnessStage]")
{
  // Prepare config (defaults) and summary
  RobustnessChecksConfig cfg; // default tolerances
  FilteringSummary summary;

  // Create bootstrap factory for the test
  BootstrapFactory bootstrapFactory(palvalidator::config::kDefaultCrnMasterSeed);

  RobustnessStage stage(cfg, summary, bootstrapFactory);

  // Create a proper strategy and context
  auto pattern = createSimpleLongPattern();
  auto portfolio = std::make_shared<Portfolio<Num>>("Test Portfolio");
  
  // Create a simple security (using futures as in PalStrategyTest.cpp)
  DecimalType tickValue(createDecimal("0.25"));
  std::string futuresSymbol("@TEST");
  std::string futuresName("Test futures");
  DecimalType bigPointValue(createDecimal("50.0"));
  
  // Create a minimal time series with just a few data points
  auto timeSeries = std::make_shared<OHLCTimeSeries<Num>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  auto testSecurity = std::make_shared<FuturesSecurity<Num>>(futuresSymbol, futuresName,
                                                             bigPointValue, tickValue, timeSeries);
  portfolio->addSecurity(testSecurity);

  // Create strategy
  StrategyOptions options(false, 0, 0);
  auto strategy = std::make_shared<PalLongStrategy<Num>>("Test Strategy", pattern, portfolio, options);

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
    StrategyAnalysisContext ctx(strategy, testSecurity, inSample, oosSample, tf, std::nullopt);
    
    // Set the cloned strategy (this is what RobustnessStage needs)
    ctx.clonedStrategy = strategy->clone2(portfolio);
    
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
    StrategyAnalysisContext ctx(strategy, testSecurity, inSample, oosSample, tf, std::nullopt);
    
    // Set the cloned strategy (this is what RobustnessStage needs)
    ctx.clonedStrategy = strategy->clone2(portfolio);
    
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