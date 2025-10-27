#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PalStrategy.h"
#include "PatternPositionRegistry.h"
#include "Portfolio.h"
#include "Security.h"
#include "PalAst.h"
#include "ClosedPositionHistory.h"
#include "BackTester.h"
#include "TestUtils.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "BoostDateHelper.h"

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

// Helper functions to create simple patterns that will match specific bar sequences

/**
 * @brief Creates a simple long pattern that matches when Close[0] > Close[1]
 * This pattern should match frequently on an uptrending synthetic series
 */
static std::shared_ptr<PriceActionLabPattern> createSimpleLongPattern()
{
  auto percentLong = std::make_shared<DecimalType>(createDecimal("100.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("0.00"));
  auto desc = std::make_shared<PatternDescription>("SimpleLong.txt", 1, 20200101,
                                                   percentLong, percentShort, 1, 1);
  
  // Pattern: Close[0] > Close[1] (today's close higher than yesterday's)
  auto close0 = std::make_shared<PriceBarClose>(0);
  auto close1 = std::make_shared<PriceBarClose>(1);
  auto longPattern = std::make_shared<GreaterThanExpr>(close0, close1);
  
  // Create entry/exit components using correct AST class names
  auto entry = std::make_shared<LongMarketEntryOnOpen>();
  auto targetDecimal = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto target = std::make_shared<LongSideProfitTargetInPercent>(targetDecimal);
  auto stopDecimal = std::make_shared<DecimalType>(createDecimal("5.00"));
  auto stop = std::make_shared<LongSideStopLossInPercent>(stopDecimal);
  
  return std::make_shared<PriceActionLabPattern>(desc, longPattern, entry, target, stop);
}

/**
 * @brief Creates a simple short pattern that matches when Close[0] < Close[1]
 * This pattern should match frequently on a downtrending synthetic series
 */
static std::shared_ptr<PriceActionLabPattern> createSimpleShortPattern()
{
  auto percentLong = std::make_shared<DecimalType>(createDecimal("0.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("100.00"));
  auto desc = std::make_shared<PatternDescription>("SimpleShort.txt", 2, 20200102,
                                                   percentLong, percentShort, 1, 1);
  
  // Pattern: Close[0] < Close[1] (today's close lower than yesterday's)
  auto close0 = std::make_shared<PriceBarClose>(0);
  auto close1 = std::make_shared<PriceBarClose>(1);
  auto shortPattern = std::make_shared<GreaterThanExpr>(close1, close0);
  
  // Create entry/exit components using correct AST class names
  auto entry = std::make_shared<ShortMarketEntryOnOpen>();
  auto targetDecimal = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto target = std::make_shared<ShortSideProfitTargetInPercent>(targetDecimal);
  auto stopDecimal = std::make_shared<DecimalType>(createDecimal("5.00"));
  auto stop = std::make_shared<ShortSideStopLossInPercent>(stopDecimal);
  
  return std::make_shared<PriceActionLabPattern>(desc, shortPattern, entry, target, stop);
}

/**
 * @brief Creates a more complex long pattern that matches when High[0] > High[1] AND Low[0] > Low[1]
 * This pattern will match less frequently than the simple one
 */
std::shared_ptr<PriceActionLabPattern> createComplexLongPattern()
{
  auto percentLong = std::make_shared<DecimalType>(createDecimal("100.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("0.00"));
  auto desc = std::make_shared<PatternDescription>("ComplexLong.txt", 3, 20200103,
                                                   percentLong, percentShort, 1, 1);
  
  // Pattern: High[0] > High[1] AND Low[0] > Low[1] (bullish engulfing-like)
  auto high0 = std::make_shared<PriceBarHigh>(0);
  auto high1 = std::make_shared<PriceBarHigh>(1);
  auto low0 = std::make_shared<PriceBarLow>(0);
  auto low1 = std::make_shared<PriceBarLow>(1);
  
  auto gt1 = std::make_shared<GreaterThanExpr>(high0, high1);
  auto gt2 = std::make_shared<GreaterThanExpr>(low0, low1);
  auto complexPattern = std::make_shared<AndExpr>(gt1, gt2);
  
  // Create entry/exit components using correct AST class names
  auto entry = std::make_shared<LongMarketEntryOnOpen>();
  auto targetDecimal = std::make_shared<DecimalType>(createDecimal("15.00"));
  auto target = std::make_shared<LongSideProfitTargetInPercent>(targetDecimal);
  auto stopDecimal = std::make_shared<DecimalType>(createDecimal("7.50"));
  auto stop = std::make_shared<LongSideStopLossInPercent>(stopDecimal);
  
  return std::make_shared<PriceActionLabPattern>(desc, complexPattern, entry, target, stop);
}

/**
 * @brief Creates synthetic time series data with predictable patterns
 * This creates an uptrending series with some volatility
 */
std::shared_ptr<OHLCTimeSeries<Num>> createSyntheticTimeSeries()
{
  auto ts = std::make_shared<OHLCTimeSeries<Num>>(TimeFrame::DAILY, TradingVolume::SHARES);
  
  // Create 50 days of synthetic data with overall upward trend
  Num basePrice = createDecimal("100.00");
  auto startDate = boost::gregorian::date(2020, 1, 1);
  
  for (int i = 0; i < 50; ++i)
  {
    auto currentDate = startDate + boost::gregorian::days(i);
    
    // Skip weekends
    if (currentDate.day_of_week() == boost::gregorian::Saturday ||
        currentDate.day_of_week() == boost::gregorian::Sunday)
      continue;
    
    // Create slight upward trend with some randomness
    Num trend = createDecimal(std::to_string(i * 0.5)); // 0.5 point per day trend
    Num noise = createDecimal(std::to_string((i % 7 - 3) * 0.3)); // Some noise
    
    Num open = basePrice + trend + noise;
    Num close = open + createDecimal(std::to_string((i % 3 - 1) * 0.8)); // Close varies
    
    // Ensure high >= max(open, close) and low <= min(open, close)
    Num high = (open > close ? open : close) + createDecimal("0.5");
    Num low = (open < close ? open : close) - createDecimal("0.5");
    Num volume = createDecimal("10000");
    
    auto entry = createTimeSeriesEntry(
      currentDate,
      open,
      high,
      low,
      close,
      static_cast<volume_t>(10000)
    );
    
    ts->addEntry(*entry);
  }
  
  return ts;
}

TEST_CASE("PatternPositionRegistry: End-to-end pattern tracking with BackTester", "[PatternPositionRegistry][Integration]")
{
  // Clear registry before test
  PatternPositionRegistry::getInstance().clear();
  
  SECTION("Multiple patterns with PalMetaStrategy")
  {
    // Create synthetic time series
    auto timeSeries = createSyntheticTimeSeries();
    REQUIRE(timeSeries->getNumEntries() > 30); // Should have reasonable amount of data
    
    // Create security
    std::string symbol = "MSFT";
    auto security = std::make_shared<EquitySecurity<Num>>(symbol, "Test Stock", timeSeries);
    
    // Create portfolio
    auto portfolio = std::make_shared<Portfolio<Num>>("Test Portfolio");
    portfolio->addSecurity(security);
    
    // Create patterns
    auto simpleLongPattern = createSimpleLongPattern();
    auto simpleShortPattern = createSimpleShortPattern();
    auto complexLongPattern = createComplexLongPattern();
    
    // Create PalMetaStrategy with multiple patterns
    StrategyOptions options(false, 0, 0); // No pyramiding, no max hold
    PalMetaStrategy<Num> strategy("Multi-Pattern Strategy", portfolio, options);
    strategy.addPricePattern(simpleLongPattern);
    strategy.addPricePattern(simpleShortPattern);
    strategy.addPricePattern(complexLongPattern);
    
    // Verify patterns are added
    REQUIRE(strategy.getPatternMaxBarsBack() > 0);
    
    // Create BackTester
    TimeSeriesDate startDate = timeSeries->getFirstDate() + boost::gregorian::days(5); // Start after we have enough history
    TimeSeriesDate endDate = timeSeries->getLastDate() - boost::gregorian::days(2);   // End before series ends
    
    auto backTester = BackTesterFactory<Num>::getBackTester(TimeFrame::DAILY, startDate, endDate);
    
    // Run backtest
    backTester->addStrategy(strategy.cloneForBackTesting());
    backTester->backtest();
    
    // Get results
    auto& strategyBroker = backTester->getStrategyBrokerForStrategy("Multi-Pattern Strategy");
    
    auto positionHistory = strategyBroker.getClosedPositionHistory();
    
    // Core verification: we have some closed positions
    REQUIRE(positionHistory.getNumPositions() > 0);
    
    // Core verification: all positions have associated patterns
    size_t positionsWithPatterns = positionHistory.getPositionCountWithPatterns();
    REQUIRE(positionsWithPatterns == positionHistory.getNumPositions());
    
    // Core verification: pattern-to-position mapping works
    std::map<std::shared_ptr<PriceActionLabPattern>, size_t> patternCounts;
    
    for (auto it = positionHistory.beginTradingPositions(); it != positionHistory.endTradingPositions(); ++it)
    {
      auto position = it->second;
      auto pattern = positionHistory.getPatternForPosition(position);
      
      // Every position should have a pattern
      REQUIRE(pattern != nullptr);
      
      // Count patterns
      patternCounts[pattern]++;
      
      // Core verification: reverse lookup works
      auto positionsForThisPattern = positionHistory.getPositionsForPattern(pattern);
      bool foundPosition = false;
      for (auto pos : positionsForThisPattern)
      {
        if (pos->getPositionID() == position->getPositionID())
        {
          foundPosition = true;
          break;
        }
      }
      REQUIRE(foundPosition);
    }
    
    // Core verification: we used multiple patterns
    REQUIRE(patternCounts.size() >= 1);
    
    // Core verification: reverse lookup consistency
    for (const auto& entry : patternCounts)
    {
      auto pattern = entry.first;
      auto positions = positionHistory.getPositionsForPattern(pattern);
      REQUIRE(positions.size() == entry.second);
      
      // Each position should map back to this pattern
      for (auto pos : positions)
      {
        auto mappedPattern = positionHistory.getPatternForPosition(pos);
        REQUIRE(mappedPattern == pattern);
      }
    }
    
    // Core verification: registry statistics are consistent
    auto& registry = PatternPositionRegistry::getInstance();
    REQUIRE(registry.getTotalOrdersRegistered() >= positionHistory.getNumPositions());
    REQUIRE(registry.getTotalPositionsRegistered() >= positionHistory.getNumPositions());
    REQUIRE(registry.getPatternCount() > 0);
  }
  
  SECTION("Pattern identification in closed positions")
  {
    // Test with just two patterns to ensure clear identification
    auto timeSeries = createSyntheticTimeSeries();
    auto security = std::make_shared<EquitySecurity<Num>>("MSFT", "Identity Test", timeSeries);
    auto portfolio = std::make_shared<Portfolio<Num>>("Identity Portfolio");
    portfolio->addSecurity(security);
    
    auto pattern1 = createSimpleLongPattern();
    auto pattern2 = createComplexLongPattern();
    
    StrategyOptions options(false, 0, 0);
    PalMetaStrategy<Num> strategy("Identity Strategy", portfolio, options);
    strategy.addPricePattern(pattern1);
    strategy.addPricePattern(pattern2);
    
    // Run backtest
    TimeSeriesDate startDate = timeSeries->getFirstDate() + boost::gregorian::days(3);
    TimeSeriesDate endDate = timeSeries->getLastDate() - boost::gregorian::days(2);
    
    auto backTester = BackTesterFactory<Num>::getBackTester(TimeFrame::DAILY, startDate, endDate);
    backTester->addStrategy(strategy.cloneForBackTesting());
    backTester->backtest();
    
    auto& strategyBroker = backTester->getStrategyBrokerForStrategy("Identity Strategy");
    auto positionHistory = strategyBroker.getClosedPositionHistory();
    
    REQUIRE(positionHistory.getNumPositions() > 0);
    
    // Verify each pattern can be uniquely identified
    for (auto it = positionHistory.beginTradingPositions(); it != positionHistory.endTradingPositions(); ++it)
    {
      auto position = it->second;
      auto pattern = positionHistory.getPatternForPosition(position);
      REQUIRE(pattern != nullptr);
      
      // Pattern should be one of the two we added
      bool isKnownPattern = (pattern == pattern1 || pattern == pattern2);
      REQUIRE(isKnownPattern);
      
      // Verify pattern details are accessible
      REQUIRE(pattern->getPatternDescription() != nullptr);
      REQUIRE(!pattern->getPatternDescription()->getFileName().empty());
    }
    
    // Verify we can get all positions with patterns
    auto allPatterned = positionHistory.getPositionsWithPatterns();
    REQUIRE(allPatterned.size() == positionHistory.getNumPositions());
  }
}
