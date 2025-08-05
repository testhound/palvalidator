#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

// Include the class to be tested and its direct dependencies
#include "PatternEvaluationTask.h"
#include "PricePatternFactory.h"
#include "SearchConfiguration.h"
#include "PatternTemplate.h"
#include "PerformanceCriteria.h"
#include "AstResourceManager.h"
#include "Security.h"
#include "TimeSeries.h" // OHLCTimeSeries is defined here
#include "TimeSeriesEntry.h" // OHLCTimeSeriesEntry is defined here
#include "number.h"

// Include shared test utilities
#include "TestUtilities.h"

// --- Helper Functions and Test Fixtures ---

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;


// --- Test Cases ---

TEST_CASE("PatternEvaluationTask correctly identifies a profitable long pattern", "[PatternEvaluationTask]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> patternFactory(*resourceManager);
    auto mockSecurity = createMockSecurity(SeriesType::ProfitableLong);
    auto config = createTestConfig(mockSecurity, 5);

    // Create a simple "up-day" pattern template: C[0] > O[0]
    PatternTemplate upDayTemplate("UpDay_C0_gt_O0");
    upDayTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 0)
    ));

    // ACT
    PatternEvaluationTask<Decimal> task(config, upDayTemplate, patternFactory);
    auto profitablePatterns = task.evaluateAndBacktest();

    // ASSERT
    REQUIRE(profitablePatterns.size() == 1);
    
    auto thePattern = profitablePatterns[0];
    REQUIRE(thePattern->isLongPattern());
    REQUIRE_FALSE(thePattern->isShortPattern());
    REQUIRE(thePattern->getFileName().find("_Long") != std::string::npos);
    REQUIRE(thePattern->getPatternDescription()->numTrades() > 0);
}

TEST_CASE("PatternEvaluationTask correctly identifies a profitable short pattern", "[PatternEvaluationTask]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> patternFactory(*resourceManager);
    auto mockSecurity = createMockSecurity(SeriesType::ProfitableShort);
    auto config = createTestConfig(mockSecurity, 5);

    // Create a simple "down-day" pattern template: O[0] > C[0]
    PatternTemplate downDayTemplate("DownDay_O0_gt_C0");
    downDayTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Open, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Close, 0)
    ));

    // ACT
    PatternEvaluationTask<Decimal> task(config, downDayTemplate, patternFactory);
    auto profitablePatterns = task.evaluateAndBacktest();

    // ASSERT
    REQUIRE(profitablePatterns.size() == 1);

    auto thePattern = profitablePatterns[0];
    REQUIRE(thePattern->isShortPattern());
    REQUIRE_FALSE(thePattern->isLongPattern());
    REQUIRE(thePattern->getFileName().find("_Short") != std::string::npos);
    REQUIRE(thePattern->getPatternDescription()->numTrades() > 0);
}

TEST_CASE("PatternEvaluationTask returns no patterns for an unprofitable template", "[PatternEvaluationTask]")
{
    // ARRANGE
    auto resourceManager = std::make_unique<mkc_palast::AstResourceManager>();
    PricePatternFactory<Decimal> patternFactory(*resourceManager);
    auto mockSecurity = createMockSecurity(SeriesType::Unprofitable);
    
    // Use very strict criteria that are unlikely to be met
    auto strictCriteria = std::make_shared<PerformanceCriteria<Decimal>>(Decimal("99.9"), 5, 0, Decimal("5.0"));
    
    // FIX: Corrected constructor call for SearchConfiguration
    auto config = SearchConfiguration<Decimal>(
        mockSecurity,
        TimeFrame::DAILY,
        SearchType::EXTENDED, // Added missing argument
        false,                // Added missing argument
        Decimal("1.0"),
        Decimal("1.0"),
        *strictCriteria,      // Dereference pointer
        boost::posix_time::time_from_string("2025-01-02 09:30:00.000"),
        boost::posix_time::time_from_string("2025-01-20 09:30:00.000")
    );

    // A pattern that will always trigger but be unprofitable: C[0] > L[0] (always true, but unprofitable with declining data)
    PatternTemplate sidewaysTemplate("Sideways_C0_gt_L0");
    sidewaysTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Low, 0)
    ));

    // ACT
    PatternEvaluationTask<Decimal> task(config, sidewaysTemplate, patternFactory);
    auto profitablePatterns = task.evaluateAndBacktest();

    // DEBUG: Print information about found patterns
    std::cout << "\n=== DEBUG INFO FOR UNPROFITABLE TEST ===" << std::endl;
    std::cout << "Number of patterns found: " << profitablePatterns.size() << std::endl;
    
    for (size_t i = 0; i < profitablePatterns.size(); ++i) {
        auto pattern = profitablePatterns[i];
        auto desc = pattern->getPatternDescription();
        std::cout << "Pattern " << i << ":" << std::endl;
        std::cout << "  File name: " << pattern->getFileName() << std::endl;
        std::cout << "  Is Long: " << pattern->isLongPattern() << std::endl;
        std::cout << "  Is Short: " << pattern->isShortPattern() << std::endl;
        std::cout << "  Num trades: " << desc->numTrades() << std::endl;
        std::cout << "  Consecutive losses: " << desc->numConsecutiveLosses() << std::endl;
        std::cout << "  Percent Long: " << *desc->getPercentLong() << std::endl;
        std::cout << "  Percent Short: " << *desc->getPercentShort() << std::endl;
    }
    std::cout << "==========================================\n" << std::endl;

    // ASSERT
    REQUIRE(profitablePatterns.empty());
}
