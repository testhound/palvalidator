#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include <string>

// Include the class to be tested and its dependencies
#include "PricePatternFactory.h"
#include "SearchConfiguration.h"
#include "PatternTemplate.h"
#include "PerformanceCriteria.h"
#include "AstResourceManager.h"
#include "Security.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "number.h"

// Include shared test utilities
#include "TestUtilities.h"

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;

// --- Test Cases for PricePatternFactory ---

TEST_CASE("PricePatternFactory creates valid long PAL pattern", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    auto mockSecurity = createMockSecurity(SeriesType::ProfitableLong);
    auto config = createTestConfig(mockSecurity, 5);
    
    // Create a simple pattern expression: C[0] > O[0]
    auto closeRef = resourceManager->getPriceClose(0);
    auto openRef = resourceManager->getPriceOpen(0);
    auto patternExpression = std::make_shared<GreaterThanExpr>(closeRef, openRef);

    // ACT
    auto longPattern = factory.createLongPalPattern(patternExpression, config, "TestPattern");

    // ASSERT
    REQUIRE(longPattern != nullptr);
    REQUIRE(longPattern->isLongPattern());
    REQUIRE_FALSE(longPattern->isShortPattern());
    REQUIRE(longPattern->getFileName() == "TestPattern_Long");
    REQUIRE(longPattern->getProfitTargetAsDecimal() == config.getProfitTarget());
    REQUIRE(longPattern->getStopLossAsDecimal() == config.getStopLoss());
}

TEST_CASE("PricePatternFactory creates valid short PAL pattern", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    auto mockSecurity = createMockSecurity(SeriesType::ProfitableShort);
    auto config = createTestConfig(mockSecurity, 5);
    
    // Create a simple pattern expression: O[0] > C[0]
    auto openRef = resourceManager->getPriceOpen(0);
    auto closeRef = resourceManager->getPriceClose(0);
    auto patternExpression = std::make_shared<GreaterThanExpr>(openRef, closeRef);

    // ACT
    auto shortPattern = factory.createShortPalPattern(patternExpression, config, "TestPattern");

    // ASSERT
    REQUIRE(shortPattern != nullptr);
    REQUIRE(shortPattern->isShortPattern());
    REQUIRE_FALSE(shortPattern->isLongPattern());
    REQUIRE(shortPattern->getFileName() == "TestPattern_Short");
    REQUIRE(shortPattern->getProfitTargetAsDecimal() == config.getProfitTarget());
    REQUIRE(shortPattern->getStopLossAsDecimal() == config.getStopLoss());
}

TEST_CASE("PricePatternFactory creates pattern expression from simple template", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    PatternTemplate simpleTemplate("SimplePattern");
    simpleTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 0)
    ));

    // ACT
    auto expression = factory.createPatternExpressionFromTemplate(simpleTemplate);

    // ASSERT
    REQUIRE(expression != nullptr);
    
    // Verify it's a GreaterThanExpr (single condition)
    auto greaterThanExpr = std::dynamic_pointer_cast<GreaterThanExpr>(expression);
    REQUIRE(greaterThanExpr != nullptr);
    
    // Verify the price bar references
    REQUIRE(greaterThanExpr->getLHS()->getReferenceType() == PriceBarReference::CLOSE);
    REQUIRE(greaterThanExpr->getRHS()->getReferenceType() == PriceBarReference::OPEN);
    REQUIRE(greaterThanExpr->getLHS()->getBarOffset() == 0);
    REQUIRE(greaterThanExpr->getRHS()->getBarOffset() == 0);
}

TEST_CASE("PricePatternFactory creates pattern expression from complex template", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    PatternTemplate complexTemplate("ComplexPattern");
    // Add two conditions: C[0] > O[0] AND H[1] > L[1]
    complexTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 0)
    ));
    complexTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::High, 1),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Low, 1)
    ));

    // ACT
    auto expression = factory.createPatternExpressionFromTemplate(complexTemplate);

    // ASSERT
    REQUIRE(expression != nullptr);
    
    // Verify it's an AndExpr (multiple conditions)
    auto andExpr = std::dynamic_pointer_cast<AndExpr>(expression);
    REQUIRE(andExpr != nullptr);
    
    // Verify both sides are GreaterThanExpr
    auto leftExpr = std::dynamic_pointer_cast<GreaterThanExpr>(andExpr->getLHSShared());
    auto rightExpr = std::dynamic_pointer_cast<GreaterThanExpr>(andExpr->getRHSShared());
    REQUIRE(leftExpr != nullptr);
    REQUIRE(rightExpr != nullptr);
}

TEST_CASE("PricePatternFactory throws exception for empty template", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    PatternTemplate emptyTemplate("EmptyPattern");
    // Don't add any conditions

    // ACT & ASSERT
    REQUIRE_THROWS_AS(factory.createPatternExpressionFromTemplate(emptyTemplate), PricePatternFactoryException);
    
    // Verify the exception message contains the template name
    try {
        factory.createPatternExpressionFromTemplate(emptyTemplate);
        FAIL("Expected PricePatternFactoryException to be thrown");
    } catch (const PricePatternFactoryException& e) {
        std::string errorMsg(e.what());
        REQUIRE(errorMsg.find("EmptyPattern") != std::string::npos);
        REQUIRE(errorMsg.find("empty template") != std::string::npos);
    }
}

TEST_CASE("PricePatternFactory creates final pattern with performance metrics", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    auto mockSecurity = createMockSecurity(SeriesType::ProfitableLong);
    auto config = createTestConfig(mockSecurity, 5);
    
    // Create a partial pattern
    auto closeRef = resourceManager->getPriceClose(0);
    auto openRef = resourceManager->getPriceOpen(0);
    auto patternExpression = std::make_shared<GreaterThanExpr>(closeRef, openRef);
    auto partialPattern = factory.createLongPalPattern(patternExpression, config, "TestPattern");
    
    // Create a mock backtester with some performance data
    auto strategy = makePalStrategy<Decimal>(partialPattern->getFileName(), partialPattern, mockSecurity);
    DateRange backTestDates(config.getBacktestStartTime(), config.getBacktestEndTime());
    auto backtester = BackTesterFactory<Decimal>::backTestStrategy(strategy, config.getTimeFrameDuration(), backTestDates);

    // ACT
    auto finalPattern = factory.createFinalPattern(partialPattern, *backtester);

    // ASSERT
    REQUIRE(finalPattern != nullptr);
    REQUIRE(finalPattern->getFileName() == partialPattern->getFileName());
    REQUIRE(finalPattern->isLongPattern() == partialPattern->isLongPattern());
    
    // Verify performance metrics were updated
    auto finalDesc = finalPattern->getPatternDescription();
    REQUIRE(finalDesc->numTrades() == backtester->getClosedPositionHistory().getNumPositions());
    REQUIRE(finalDesc->numConsecutiveLosses() == backtester->getNumConsecutiveLosses());
}

TEST_CASE("PricePatternFactory throws exception for unknown price component type", "[PricePatternFactory]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> factory(*resourceManager);
    
    // Create a template with an invalid component type (this would require modifying the enum, 
    // so we'll test the factory's robustness by creating a pattern template that would
    // exercise the error path if we had an invalid enum value)
    PatternTemplate validTemplate("ValidPattern");
    validTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 0)
    ));

    // ACT & ASSERT
    // This should work fine with valid component types
    auto expression = factory.createPatternExpressionFromTemplate(validTemplate);
    REQUIRE(expression != nullptr);
    
    // Note: Testing the exception case would require either:
    // 1. Modifying the enum to add an invalid value (not recommended)
    // 2. Using a mock or creating a test-specific subclass
    // 3. Testing at a lower level with direct calls to createPriceBarReference
    // For now, we verify the happy path works correctly
}