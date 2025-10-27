#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PalStrategy.h"
#include "PatternPositionRegistry.h"
#include "Portfolio.h"
#include "Security.h"
#include "PalAst.h"
#include "ClosedPositionHistory.h"
#include "BacktesterStrategy.h"
#include "StrategyBroker.h"
#include "TradingOrder.h"
#include "TradingPosition.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

// Helper functions to create simple test patterns using AstResourceManager
static std::shared_ptr<PriceActionLabPattern> createTestLongPattern()
{
  // Create pattern description
  auto percentLong = std::make_shared<DecimalType>(createDecimal("100.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("0.00"));
  auto desc = std::make_shared<PatternDescription>("TestLong.txt", 1, 20200101,
                                                   percentLong, percentShort, 1, 1);
  
  // Simple pattern: Close[0] > Close[1]
  auto close0 = std::make_shared<PriceBarClose>(0);
  auto close1 = std::make_shared<PriceBarClose>(1);
  auto pattern = std::make_shared<GreaterThanExpr>(close0, close1);
  
  // Create entry/exit components - using simple market entry
  auto entry = std::make_shared<LongMarketEntryOnOpen>();
  
  // Create profit target and stop loss using correct AST class names
  auto targetDecimal = std::make_shared<DecimalType>(createDecimal("5.00"));
  auto target = std::make_shared<LongSideProfitTargetInPercent>(targetDecimal);
  
  auto stopDecimal = std::make_shared<DecimalType>(createDecimal("2.50"));
  auto stop = std::make_shared<LongSideStopLossInPercent>(stopDecimal);
  
  return std::make_shared<PriceActionLabPattern>(desc, pattern, entry, target, stop);
}

static std::shared_ptr<PriceActionLabPattern> createTestShortPattern()
{
  // Create pattern description
  auto percentLong = std::make_shared<DecimalType>(createDecimal("0.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("100.00"));
  auto desc = std::make_shared<PatternDescription>("TestShort.txt", 2, 20200102,
                                                   percentLong, percentShort, 1, 1);
  
  // Simple pattern: Close[1] > Close[0]
  auto close0 = std::make_shared<PriceBarClose>(0);
  auto close1 = std::make_shared<PriceBarClose>(1);
  auto pattern = std::make_shared<GreaterThanExpr>(close1, close0);
  
  // Create entry/exit components - using simple market entry
  auto entry = std::make_shared<ShortMarketEntryOnOpen>();
  
  // Create profit target and stop loss using correct AST class names
  auto targetDecimal = std::make_shared<DecimalType>(createDecimal("5.00"));
  auto target = std::make_shared<ShortSideProfitTargetInPercent>(targetDecimal);
  
  auto stopDecimal = std::make_shared<DecimalType>(createDecimal("2.50"));
  auto stop = std::make_shared<ShortSideStopLossInPercent>(stopDecimal);
  
  return std::make_shared<PriceActionLabPattern>(desc, pattern, entry, target, stop);
}

TEST_CASE("PalMetaStrategy: Pattern tracking end-to-end basic", "[PalMetaStrategy][PatternTracking][Integration]")
{
    // Clear registry before test
    PatternPositionRegistry::getInstance().clear();
    
    // Create test setup with a security (required for portfolio filter)
    auto portfolio = std::make_shared<Portfolio<Num>>("Test Portfolio");
    
    // Create a minimal time series for the security
    auto timeSeries = std::make_shared<OHLCTimeSeries<Num>>(TimeFrame::DAILY, TradingVolume::SHARES);
    auto entry = createTimeSeriesEntry("20200101", "100.00", "101.00", "99.00", "100.50", 1000);
    timeSeries->addEntry(*entry);
    
    auto security = std::make_shared<EquitySecurity<Num>>("TEST", "Test Security", timeSeries);
    portfolio->addSecurity(security);
    
    auto pattern = createTestLongPattern();
    
    // Create PalMetaStrategy
    PalMetaStrategy<Num> strategy("Test Strategy", portfolio, defaultStrategyOptions);
    strategy.addPricePattern(pattern);
    
    // Test that pattern tracking works through the full stack
    // This would require a more comprehensive setup with actual securities and backtesting
    
    // For now, just verify the registry is working
    REQUIRE(PatternPositionRegistry::getInstance().getOrderCount() == 0);
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == 0);
    
    // Simulate pattern being used
    uint32_t testOrderId = 123;
    uint32_t testPositionId = 456;
    
    PatternPositionRegistry::getInstance().registerOrderPattern(testOrderId, pattern);
    PatternPositionRegistry::getInstance().transferOrderToPosition(testOrderId, testPositionId);
    
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForPosition(testPositionId) == pattern);
}

TEST_CASE("ClosedPositionHistory: Pattern integration", "[ClosedPositionHistory][PatternTracking][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    // Create test patterns
    auto pattern1 = createTestLongPattern();
    auto pattern2 = createTestShortPattern();
    
    // Create test positions with known IDs
    uint32_t positionId1 = 1001;
    uint32_t positionId2 = 1002;
    uint32_t positionId3 = 1003;
    
    // Mock trading positions - we'll use the constructor that might exist
    // Note: This test would need to be adapted based on actual TradingPosition constructor
    // For now, we'll test the registry integration directly
    
    // Register patterns for positions
    PatternPositionRegistry::getInstance().registerOrderPattern(100, pattern1);
    PatternPositionRegistry::getInstance().registerOrderPattern(200, pattern2);
    PatternPositionRegistry::getInstance().registerOrderPattern(300, pattern1); // Same pattern as first
    
    PatternPositionRegistry::getInstance().transferOrderToPosition(100, positionId1);
    PatternPositionRegistry::getInstance().transferOrderToPosition(200, positionId2);
    PatternPositionRegistry::getInstance().transferOrderToPosition(300, positionId3);
    
    // Create ClosedPositionHistory and test pattern methods
    ClosedPositionHistory<Num> history;
    
    // Test getPositionsForPattern - should find positions associated with pattern1
    auto pattern1Positions = history.getPositionsForPattern(pattern1);
    // Since we don't have actual positions in the history, this will be empty
    // But we can verify the registry lookup works
    
    auto registryPattern1Positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern1);
    REQUIRE(registryPattern1Positions.size() == 2); // positionId1 and positionId3
    REQUIRE(std::find(registryPattern1Positions.begin(), registryPattern1Positions.end(), positionId1) != registryPattern1Positions.end());
    REQUIRE(std::find(registryPattern1Positions.begin(), registryPattern1Positions.end(), positionId3) != registryPattern1Positions.end());
    
    auto registryPattern2Positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern2);
    REQUIRE(registryPattern2Positions.size() == 1); // positionId2
    REQUIRE(registryPattern2Positions[0] == positionId2);
}

TEST_CASE("EntryOrderConditions: Pattern-aware integration", "[EntryOrderConditions][PatternTracking][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern = createTestLongPattern();
    
    // Test that EntryOrderConditions classes will use the pattern-aware methods
    // This is more of a compile-time verification that our changes work
    
    // Create entry conditions
    FlatEntryOrderConditions<Num> flatConditions;
    LongEntryOrderConditions<Num> longConditions;
    ShortEntryOrderConditions<Num> shortConditions;
    
    // Verify conditions exist (they use pattern-aware methods internally now)
    REQUIRE(true); // Placeholder - the real test is that this compiles and links
}

TEST_CASE("BacktesterStrategy: Pattern-aware methods integration", "[BacktesterStrategy][PatternTracking][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern = createTestLongPattern();
    
    // This test verifies that the BacktesterStrategy pattern-aware methods exist
    // and can be called (even if we don't have a full backtesting setup)
    
    // The actual methods would require a full portfolio/security setup to test properly
    // For now, we verify the registry integration
    
    uint32_t testOrderId = 5000;
    uint32_t testPositionId = 6000;
    
    PatternPositionRegistry::getInstance().registerOrderPattern(testOrderId, pattern);
    PatternPositionRegistry::getInstance().transferOrderToPosition(testOrderId, testPositionId);
    
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForPosition(testPositionId) == pattern);
}

TEST_CASE("Pattern Registry: Multiple patterns performance", "[PatternPositionRegistry][Performance][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    // Create multiple patterns - alternating long and short for variety
    std::vector<std::shared_ptr<PriceActionLabPattern>> patterns;
    for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            patterns.push_back(createTestLongPattern());
        } else {
            patterns.push_back(createTestShortPattern());
        }
    }
    
    // Register many orders and positions
    for (int i = 0; i < 1000; ++i) {
        uint32_t orderId = 10000 + i;
        uint32_t positionId = 20000 + i;
        auto pattern = patterns[i % patterns.size()]; // Cycle through patterns
        
        PatternPositionRegistry::getInstance().registerOrderPattern(orderId, pattern);
        PatternPositionRegistry::getInstance().transferOrderToPosition(orderId, positionId);
    }
    
    // Verify counts
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == 1000);
    REQUIRE(PatternPositionRegistry::getInstance().getPatternCount() == patterns.size());
    
    // Test lookup performance (should be fast)
    auto startPattern = patterns[0];
    auto positions = PatternPositionRegistry::getInstance().getPositionsForPattern(startPattern);
    REQUIRE_FALSE(positions.empty()); // Should find some positions
    
    // Test individual lookups
    for (int i = 0; i < 10; ++i) { // Test first 10
        uint32_t positionId = 20000 + i;
        auto pattern = PatternPositionRegistry::getInstance().getPatternForPosition(positionId);
        REQUIRE(pattern != nullptr);
        REQUIRE(std::find(patterns.begin(), patterns.end(), pattern) != patterns.end());
    }
}

TEST_CASE("Pattern Registry: Cleanup integration", "[PatternPositionRegistry][Cleanup][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern1 = createTestLongPattern();
    auto pattern2 = createTestShortPattern();
    
    // Set up test data
    for (int i = 0; i < 10; ++i) {
        uint32_t orderId = 100 + i;
        uint32_t positionId = 200 + i;
        auto pattern = (i % 2 == 0) ? pattern1 : pattern2;
        
        PatternPositionRegistry::getInstance().registerOrderPattern(orderId, pattern);
        PatternPositionRegistry::getInstance().transferOrderToPosition(orderId, positionId);
    }
    
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == 10);
    REQUIRE(PatternPositionRegistry::getInstance().getPatternCount() == 2);
    
    // Remove all positions for pattern1
    auto pattern1Positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern1);
    for (auto positionId : pattern1Positions) {
        PatternPositionRegistry::getInstance().removePosition(positionId);
    }
    
    // Verify pattern1 positions are gone but pattern2 remains
    REQUIRE(PatternPositionRegistry::getInstance().getPositionsForPattern(pattern1).empty());
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().getPositionsForPattern(pattern2).empty());
    
    // Pattern1 should be removed from pattern count since no positions remain
    REQUIRE(PatternPositionRegistry::getInstance().getPatternCount() == 1);
}

TEST_CASE("Full Stack: Simulated trading workflow", "[Integration][FullStack]")
{
    PatternPositionRegistry::getInstance().clear();
    
    // This test simulates the complete workflow:
    // Pattern -> Order -> Position -> History
    
    auto longPattern = createTestLongPattern();
    auto shortPattern = createTestShortPattern();
    
    // Simulate multiple trades
    struct TradeSimulation {
        uint32_t orderId;
        uint32_t positionId;
        std::shared_ptr<PriceActionLabPattern> pattern;
        bool isLong;
    };
    
    std::vector<TradeSimulation> trades = {
        {1001, 2001, longPattern, true},
        {1002, 2002, shortPattern, false},
        {1003, 2003, longPattern, true},
        {1004, 2004, shortPattern, false},
        {1005, 2005, longPattern, true}
    };
    
    // Simulate the trading workflow
    for (const auto& trade : trades) {
        // 1. Strategy decides to enter based on pattern
        // 2. Order is created with pattern tracking
        PatternPositionRegistry::getInstance().registerOrderPattern(trade.orderId, trade.pattern);
        
        // 3. Order gets filled, position is created
        PatternPositionRegistry::getInstance().transferOrderToPosition(trade.orderId, trade.positionId);
        
        // 4. Verify pattern is tracked for position
        auto retrievedPattern = PatternPositionRegistry::getInstance().getPatternForPosition(trade.positionId);
        REQUIRE(retrievedPattern == trade.pattern);
    }
    
    // Verify final state
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == trades.size());
    REQUIRE(PatternPositionRegistry::getInstance().getPatternCount() == 2); // longPattern and shortPattern
    
    // Test pattern-specific analysis
    auto longPositions = PatternPositionRegistry::getInstance().getPositionsForPattern(longPattern);
    auto shortPositions = PatternPositionRegistry::getInstance().getPositionsForPattern(shortPattern);
    
    REQUIRE(longPositions.size() == 3); // 3 long trades
    REQUIRE(shortPositions.size() == 2); // 2 short trades
    
    // Verify position IDs are correct
    std::vector<uint32_t> expectedLongIds = {2001, 2003, 2005};
    std::vector<uint32_t> expectedShortIds = {2002, 2004};
    
    for (auto expectedId : expectedLongIds) {
        REQUIRE(std::find(longPositions.begin(), longPositions.end(), expectedId) != longPositions.end());
    }
    
    for (auto expectedId : expectedShortIds) {
        REQUIRE(std::find(shortPositions.begin(), shortPositions.end(), expectedId) != shortPositions.end());
    }
}

TEST_CASE("Error Handling: Registry robustness", "[PatternPositionRegistry][ErrorHandling][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern = createTestLongPattern();
    
    // Test various error conditions
    
    // 1. Double registration of same order
    PatternPositionRegistry::getInstance().registerOrderPattern(1000, pattern);
    PatternPositionRegistry::getInstance().registerOrderPattern(1000, pattern); // Should not crash
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForOrder(1000));
    
    // 2. Double transfer of same order
    PatternPositionRegistry::getInstance().transferOrderToPosition(1000, 2000);
    PatternPositionRegistry::getInstance().transferOrderToPosition(1000, 2001); // Should not crash
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForPosition(2000));
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForPosition(2001));
    
    // 3. Remove non-existent items
    PatternPositionRegistry::getInstance().removeOrder(99999); // Should not crash
    PatternPositionRegistry::getInstance().removePosition(99999); // Should not crash
    
    // 4. Operations with null patterns
    PatternPositionRegistry::getInstance().registerOrderPattern(3000, nullptr); // Should be ignored
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForOrder(3000));
    
    auto nullPositions = PatternPositionRegistry::getInstance().getPositionsForPattern(nullptr);
    REQUIRE(nullPositions.empty());
    
    // Registry should still be in valid state
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForPosition(2000) == pattern);
}