#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PatternPositionRegistry.h"
#include "PalAst.h"
#include "DecimalConstants.h"

using namespace mkc_timeseries;

// Helper function to create a mock pattern for testing
static std::shared_ptr<PriceActionLabPattern> createMockPattern()
{
  // Create minimal required components for a pattern
  auto description = std::make_shared<PatternDescription>(
    "test.txt",           // fileName
    1,                    // patternIndex
    20240101,             // indexDate
    std::make_shared<decimal7>(DecimalConstants<decimal7>::createDecimal("100.0")),  // percentLong
    std::make_shared<decimal7>(DecimalConstants<decimal7>::createDecimal("0.0")),    // percentShort
    10,                   // numTrades
    2                     // consecutiveLosses
  );
  
  // Create a simple pattern expression: Close[0] > Open[0]
  auto priceClose = std::make_shared<PriceBarClose>(0);
  auto priceOpen = std::make_shared<PriceBarOpen>(0);
  auto patternExpr = std::make_shared<GreaterThanExpr>(priceClose, priceOpen);
  
  // Create market entry
  auto entry = std::make_shared<LongMarketEntryOnOpen>();
  
  // Create profit target and stop loss
  auto profitTarget = std::make_shared<LongSideProfitTargetInPercent>(
    std::make_shared<decimal7>(DecimalConstants<decimal7>::createDecimal("2.0"))
  );
  auto stopLoss = std::make_shared<LongSideStopLossInPercent>(
    std::make_shared<decimal7>(DecimalConstants<decimal7>::createDecimal("1.0"))
  );
  
  return std::make_shared<PriceActionLabPattern>(
    description,
    patternExpr,
    entry,
    profitTarget,
    stopLoss
  );
}

TEST_CASE("PatternPositionRegistry: Basic functionality", "[PatternPositionRegistry]")
{
    // Clear registry before test
    PatternPositionRegistry::getInstance().clear();
    
    // Create mock pattern
    auto pattern = createMockPattern();
    
    uint32_t orderID = 12345;
    uint32_t positionID = 67890;
    
    // Test order registration
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID, pattern);
    
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForOrder(orderID));
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForOrder(orderID) == pattern);
    
    // Test pattern transfer from order to position
    PatternPositionRegistry::getInstance().transferOrderToPosition(orderID, positionID);
    
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForPosition(positionID));
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForPosition(positionID) == pattern);
    
    // Test reverse lookup
    auto positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern);
    REQUIRE(positions.size() == 1);
    REQUIRE(positions[0] == positionID);
}

TEST_CASE("PatternPositionRegistry: Multiple positions per pattern", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern = createMockPattern();
    
    uint32_t orderID1 = 111;
    uint32_t orderID2 = 222;
    uint32_t positionID1 = 333;
    uint32_t positionID2 = 444;
    
    // Register multiple orders with same pattern
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID1, pattern);
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID2, pattern);
    
    // Transfer to positions
    PatternPositionRegistry::getInstance().transferOrderToPosition(orderID1, positionID1);
    PatternPositionRegistry::getInstance().transferOrderToPosition(orderID2, positionID2);
    
    // Test reverse lookup
    auto positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern);
    REQUIRE(positions.size() == 2);
    REQUIRE(std::find(positions.begin(), positions.end(), positionID1) != positions.end());
    REQUIRE(std::find(positions.begin(), positions.end(), positionID2) != positions.end());
}

TEST_CASE("PatternPositionRegistry: Null pattern handling", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    uint32_t orderID = 12345;
    
    // Test registering null pattern (should be ignored)
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID, nullptr);
    
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForOrder(orderID));
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForOrder(orderID) == nullptr);
}

TEST_CASE("PatternPositionRegistry: Non-existent lookups", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    uint32_t nonExistentOrderID = 99999;
    uint32_t nonExistentPositionID = 88888;
    
    // Test lookups for non-existent IDs
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForOrder(nonExistentOrderID));
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForPosition(nonExistentPositionID));
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForOrder(nonExistentOrderID) == nullptr);
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForPosition(nonExistentPositionID) == nullptr);
    
    // Test reverse lookup for non-existent pattern
    auto nonExistentPattern = createMockPattern();
    auto positions = PatternPositionRegistry::getInstance().getPositionsForPattern(nonExistentPattern);
    REQUIRE(positions.empty());
    
    // Test reverse lookup for null pattern
    auto nullPositions = PatternPositionRegistry::getInstance().getPositionsForPattern(nullptr);
    REQUIRE(nullPositions.empty());
}

TEST_CASE("PatternPositionRegistry: Statistics tracking", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern1 = createMockPattern();
    auto pattern2 = createMockPattern();
    
    // Initial state
    REQUIRE(PatternPositionRegistry::getInstance().getOrderCount() == 0);
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == 0);
    REQUIRE(PatternPositionRegistry::getInstance().getPatternCount() == 0);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalOrdersRegistered() == 0);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalPositionsRegistered() == 0);
    
    // Register orders
    PatternPositionRegistry::getInstance().registerOrderPattern(100, pattern1);
    PatternPositionRegistry::getInstance().registerOrderPattern(200, pattern2);
    
    REQUIRE(PatternPositionRegistry::getInstance().getOrderCount() == 2);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalOrdersRegistered() == 2);
    
    // Transfer to positions
    PatternPositionRegistry::getInstance().transferOrderToPosition(100, 300);
    PatternPositionRegistry::getInstance().transferOrderToPosition(200, 400);
    
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == 2);
    REQUIRE(PatternPositionRegistry::getInstance().getPatternCount() == 2);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalPositionsRegistered() == 2);
}

TEST_CASE("PatternPositionRegistry: Cleanup operations", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern = createMockPattern();
    uint32_t orderID = 123;
    uint32_t positionID = 456;
    
    // Set up data
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID, pattern);
    PatternPositionRegistry::getInstance().transferOrderToPosition(orderID, positionID);
    
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForOrder(orderID));
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForPosition(positionID));
    
    // Test removing order
    PatternPositionRegistry::getInstance().removeOrder(orderID);
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForOrder(orderID));
    REQUIRE(PatternPositionRegistry::getInstance().hasPatternForPosition(positionID)); // Position should remain
    
    // Re-register order for position cleanup test
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID, pattern);
    PatternPositionRegistry::getInstance().transferOrderToPosition(orderID, positionID + 1);
    
    // Test removing position
    PatternPositionRegistry::getInstance().removePosition(positionID);
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForPosition(positionID));
    
    // Verify pattern still exists for other position
    auto positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern);
    REQUIRE_FALSE(positions.empty());
    
    // Remove last position for this pattern
    PatternPositionRegistry::getInstance().removePosition(positionID + 1);
    positions = PatternPositionRegistry::getInstance().getPositionsForPattern(pattern);
    REQUIRE(positions.empty());
}

TEST_CASE("PatternPositionRegistry: getAllPatterns functionality", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern1 = createMockPattern();
    auto pattern2 = createMockPattern();
    auto pattern3 = createMockPattern();
    
    // Initially no patterns
    auto patterns = PatternPositionRegistry::getInstance().getAllPatterns();
    REQUIRE(patterns.empty());
    
    // Register orders and transfer to positions (patterns appear in getAllPatterns only after transferOrderToPosition)
    PatternPositionRegistry::getInstance().registerOrderPattern(100, pattern1);
    PatternPositionRegistry::getInstance().transferOrderToPosition(100, 200);
    
    PatternPositionRegistry::getInstance().registerOrderPattern(101, pattern2);
    PatternPositionRegistry::getInstance().transferOrderToPosition(101, 201);
    
    PatternPositionRegistry::getInstance().registerOrderPattern(102, pattern3);
    PatternPositionRegistry::getInstance().transferOrderToPosition(102, 202);
    
    patterns = PatternPositionRegistry::getInstance().getAllPatterns();
    REQUIRE(patterns.size() == 3);
    
    // Verify all patterns are present
    REQUIRE(std::find(patterns.begin(), patterns.end(), pattern1) != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), pattern2) != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), pattern3) != patterns.end());
}

TEST_CASE("PatternPositionRegistry: Transfer non-existent order", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    uint32_t nonExistentOrderID = 99999;
    uint32_t positionID = 12345;
    
    // Try to transfer pattern from non-existent order
    PatternPositionRegistry::getInstance().transferOrderToPosition(nonExistentOrderID, positionID);
    
    // Should not create a position entry
    REQUIRE_FALSE(PatternPositionRegistry::getInstance().hasPatternForPosition(positionID));
    REQUIRE(PatternPositionRegistry::getInstance().getPositionCount() == 0);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalPositionsRegistered() == 0);
}

TEST_CASE("PatternPositionRegistry: Integration test with PalMetaStrategy flow", "[PatternPositionRegistry][Integration]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern = createMockPattern();
    uint32_t orderID = 100;
    uint32_t positionID = 200;
    
    // Simulate the full flow:
    // 1. PalMetaStrategy calls EntryOrderConditions::createEntryOrders
    // 2. This calls BacktesterStrategy::EnterLongOnOpenWithPattern
    // 3. This calls StrategyBroker::EnterLongOnOpenWithPattern  
    // 4. This registers the pattern with the order
    PatternPositionRegistry::getInstance().registerOrderPattern(orderID, pattern);
    
    // 5. Order gets filled, StrategyBroker::createLongTradingPosition called
    // 6. This transfers the pattern from order to position
    PatternPositionRegistry::getInstance().transferOrderToPosition(orderID, positionID);
    
    // 7. Position eventually ends up in ClosedPositionHistory
    // 8. User calls history.getPatternForPosition(position)
    auto retrievedPattern = PatternPositionRegistry::getInstance().getPatternForPosition(positionID);
    
    REQUIRE(retrievedPattern == pattern);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalOrdersRegistered() == 1);
    REQUIRE(PatternPositionRegistry::getInstance().getTotalPositionsRegistered() == 1);
}

TEST_CASE("PatternPositionRegistry: Thread safety basic verification", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    // This test would need to be expanded with actual threading tests
    // For now, just verify basic operations work
    auto pattern = createMockPattern();
    
    PatternPositionRegistry::getInstance().registerOrderPattern(1, pattern);
    PatternPositionRegistry::getInstance().transferOrderToPosition(1, 2);
    
    REQUIRE(PatternPositionRegistry::getInstance().getPatternForPosition(2) == pattern);
}

TEST_CASE("PatternPositionRegistry: Debug report functionality", "[PatternPositionRegistry]")
{
    PatternPositionRegistry::getInstance().clear();
    
    auto pattern1 = createMockPattern();
    auto pattern2 = createMockPattern();
    
    // Set up some test data
    PatternPositionRegistry::getInstance().registerOrderPattern(100, pattern1);
    PatternPositionRegistry::getInstance().registerOrderPattern(200, pattern2);
    PatternPositionRegistry::getInstance().transferOrderToPosition(100, 300);
    PatternPositionRegistry::getInstance().transferOrderToPosition(200, 400);
    
    // Test debug report generation (should not throw)
    std::ostringstream output;
    REQUIRE_NOTHROW(PatternPositionRegistry::getInstance().generateDebugReport(output));
    
    // Verify report contains some expected content
    std::string report = output.str();
    REQUIRE(report.find("PatternPositionRegistry Debug Report") != std::string::npos);
    REQUIRE(report.find("Orders tracked: 2") != std::string::npos);
    REQUIRE(report.find("Positions tracked: 2") != std::string::npos);
    REQUIRE(report.find("Patterns tracked: 2") != std::string::npos);
}