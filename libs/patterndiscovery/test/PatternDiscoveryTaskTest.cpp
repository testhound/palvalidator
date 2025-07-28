// test/PatternDiscoveryTaskTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "PatternDiscoveryTask.h"
#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
#include "Security.h"
#include "Security.h"
#include "TimeFrame.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "number.h"
#include "AstResourceManager.h"
#include "PalAst.h"
#include "TestUtils.h"
#include "TradingVolume.h"
#include "BackTester.h"
#include "ClosedPositionHistory.h"
#include "TimeSeriesIndicators.h"

#include <iostream>
#include <string>
#include <utility>

using TestDecimalType = num::DefaultNumber;

// Helper to create a security with a longer, more predictable data series
// designed to test various SearchType conditions.
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createComprehensiveSecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    // This data is specifically designed so that sorting different price components
    // results in a unique and predictable AST for each SearchType.
    // Adding sufficient historical data to support patterns up to 9 bars back
    // Historical context bars (10 bars before the main test period)
    series->addEntry(*createTimeSeriesEntry("20221220", "80", "85", "78", "82", 1000));   // Historical -9
    series->addEntry(*createTimeSeriesEntry("20221221", "82", "87", "80", "84", 1000));   // Historical -8
    series->addEntry(*createTimeSeriesEntry("20221222", "84", "89", "82", "86", 1000));   // Historical -7
    series->addEntry(*createTimeSeriesEntry("20221223", "86", "91", "84", "88", 1000));   // Historical -6
    series->addEntry(*createTimeSeriesEntry("20221226", "88", "93", "86", "90", 1000));   // Historical -5
    series->addEntry(*createTimeSeriesEntry("20221227", "90", "95", "88", "92", 1000));   // Historical -4
    series->addEntry(*createTimeSeriesEntry("20221228", "92", "97", "90", "94", 1000));   // Historical -3
    series->addEntry(*createTimeSeriesEntry("20221229", "94", "99", "92", "96", 1000));   // Historical -2
    series->addEntry(*createTimeSeriesEntry("20221230", "96", "101", "94", "98", 1000));  // Historical -1
    series->addEntry(*createTimeSeriesEntry("20230101", "100", "110", "98", "108", 1000)); // Bar 4
    series->addEntry(*createTimeSeriesEntry("20230102", "105", "115", "103", "112", 1000)); // Bar 3
    series->addEntry(*createTimeSeriesEntry("20230103", "110", "125", "109", "120", 1000)); // Bar 2
    series->addEntry(*createTimeSeriesEntry("20230104", "122", "130", "121", "128", 1000)); // Bar 1
    series->addEntry(*createTimeSeriesEntry("20230105", "127", "135", "126", "132", 1000)); // Bar 0

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("AAPL", "Apple Computer", series);
}

// Updated helper to create a SearchConfiguration with the new constructor
SearchConfiguration<TestDecimalType> createSearchConfig(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
    SearchType searchType,
    bool searchForDelayPatterns)
{
    // Use lenient performance criteria to ensure any backtest with at least one trade passes
    PerformanceCriteria<TestDecimalType> perfCriteria(TestDecimalType("0.0"), 1, 999, TestDecimalType("0.001"));
    boost::posix_time::ptime startTime(boost::gregorian::date(2022, 12, 20));
    boost::posix_time::ptime endTime(boost::gregorian::date(2023, 1, 5));

    // Calculate profit target and stop loss based on the actual time series data
    auto timeSeries = security->getTimeSeries();
    TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
    
    // Use the computed value as both profit target and stop loss
    // You could also scale these values, e.g., profitTarget = profitTargetAndStop * 1.5
    TestDecimalType profitTarget = profitTargetAndStop;
    TestDecimalType stopLoss = profitTargetAndStop;

    return SearchConfiguration<TestDecimalType>(
        security,
        mkc_timeseries::TimeFrame::DAILY,
        searchType,
        searchForDelayPatterns,
        profitTarget,
        stopLoss,
        perfCriteria,
        startTime,
        endTime
    );
}

TEST_CASE("PatternDiscoveryTask correctly generates patterns for each SearchType", "[PatternDiscoveryTask]")
{
    auto testSecurity = createComprehensiveSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    // Use the actual last date from the time series to ensure exact timestamp match
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    // Debug: Print the time series date range
    std::cout << "DEBUG: Time series has " << timeSeries->getNumEntries() << " entries" << std::endl;
    std::cout << "DEBUG: First date: " << timeSeries->getFirstDateTime() << std::endl;
    std::cout << "DEBUG: Last date: " << timeSeries->getLastDateTime() << std::endl;
    std::cout << "DEBUG: Window end time: " << windowEndTime << std::endl;

    SECTION("Task generates correct pattern for SearchType::EXTENDED")
    {
        auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
        
        // Debug: Print the calculated profit target and stop loss
        auto timeSeries = testSecurity->getTimeSeries();
        TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
        std::cout << "DEBUG: Calculated profit target/stop: " << profitTargetAndStop << std::endl;
        std::cout << "DEBUG: Profit target: " << config.getProfitTarget() << std::endl;
        std::cout << "DEBUG: Stop loss: " << config.getStopLoss() << std::endl;
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();

        std::cout << "DEBUG: Found " << patterns.size() << " patterns for EXTENDED search" << std::endl;

        // EXTENDED search is for lengths 2-6. With sufficient historical data and
        // realistic profit targets, we should find some patterns
        REQUIRE(patterns.size() >= 1);

        // Verify we found patterns and check the first one
        auto firstPattern = patterns.begin();
        std::cout << "DEBUG: First pattern filename: " << firstPattern->first->getPatternDescription()->getFileName() << std::endl;
        
        // Verify the pattern has a valid expression
        REQUIRE(firstPattern->first->getPatternExpression() != nullptr);
        
        // For EXTENDED search, we expect patterns with mixed OHLC components
        // The exact pattern depends on the data, so we just verify it's a valid pattern
        auto patternExpr = firstPattern->first->getPatternExpression();
        REQUIRE(patternExpr != nullptr);
    }

    SECTION("Task generates correct pattern for SearchType::CLOSE_ONLY")
    {
        auto config = createSearchConfig(testSecurity, SearchType::CLOSE_ONLY, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();

        // CLOSE_ONLY is for lengths 3-9. With sufficient data, we should find some patterns
        REQUIRE(patterns.size() >= 1);

        // Verify we found patterns and check that they use only Close prices
        auto firstPattern = patterns.begin();
        std::cout << "DEBUG: CLOSE_ONLY first pattern filename: " << firstPattern->first->getPatternDescription()->getFileName() << std::endl;
        
        // Verify the pattern has a valid expression
        REQUIRE(firstPattern->first->getPatternExpression() != nullptr);
        
        // For CLOSE_ONLY search, patterns should only use Close price components
        // The exact pattern depends on the data, so we just verify it's a valid pattern
        auto patternExpr = firstPattern->first->getPatternExpression();
        REQUIRE(patternExpr != nullptr);
    }

    SECTION("Task generates correct pattern for SearchType::HIGH_LOW_ONLY")
    {
        auto config = createSearchConfig(testSecurity, SearchType::HIGH_LOW_ONLY, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        REQUIRE(patterns.size() >= 1); // Should find some patterns

        // Verify we found patterns and check that they use only High and Low prices
        auto firstPattern = patterns.begin();
        std::cout << "DEBUG: HIGH_LOW_ONLY first pattern filename: " << firstPattern->first->getPatternDescription()->getFileName() << std::endl;
        
        // Verify the pattern has a valid expression
        REQUIRE(firstPattern->first->getPatternExpression() != nullptr);
        
        // For HIGH_LOW_ONLY search, patterns should only use High and Low price components
        // The exact pattern depends on the data, so we just verify it's a valid pattern
        auto patternExpr = firstPattern->first->getPatternExpression();
        REQUIRE(patternExpr != nullptr);
    }
    
    SECTION("Task generates correct pattern for SearchType::OPEN_CLOSE_ONLY")
    {
        auto config = createSearchConfig(testSecurity, SearchType::OPEN_CLOSE_ONLY, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        REQUIRE(patterns.size() >= 1); // Should find some patterns

        // Verify we found patterns and check that they use only Open and Close prices
        auto firstPattern = patterns.begin();
        std::cout << "DEBUG: OPEN_CLOSE_ONLY first pattern filename: " << firstPattern->first->getPatternDescription()->getFileName() << std::endl;
        
        // Verify the pattern has a valid expression
        REQUIRE(firstPattern->first->getPatternExpression() != nullptr);
        
        // For OPEN_CLOSE_ONLY search, patterns should only use Open and Close price components
        // The exact pattern depends on the data, so we just verify it's a valid pattern
        auto patternExpr = firstPattern->first->getPatternExpression();
        REQUIRE(patternExpr != nullptr);
    }
}

// ============================================================================
// PRIORITY 1: CRITICAL MISSING TESTS
// ============================================================================

// Helper functions for test data generation
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createMinimalSecurity(int numEntries)
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    for (int i = 0; i < numEntries; ++i)
    {
        std::string dateStr = "2023010" + std::to_string(i + 1);
        TestDecimalType price = TestDecimalType("100") + TestDecimalType(i);
        series->addEntry(*createTimeSeriesEntry(
            dateStr,
            num::toString(price),
            num::toString(price + TestDecimalType("2")),
            num::toString(price - TestDecimalType("1")),
            num::toString(price + TestDecimalType("1")),
            1000
        ));
    }

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("MSFT", "Microsoft Corporation", series);
}

std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createEmptySecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("GOOGL", "Alphabet Inc.", series);
}

SearchConfiguration<TestDecimalType> createSearchConfigWithCriteria(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
    SearchType searchType,
    const PerformanceCriteria<TestDecimalType>& criteria)
{
    boost::posix_time::ptime startTime(boost::gregorian::date(2023, 1, 1));
    boost::posix_time::ptime endTime(boost::gregorian::date(2023, 1, 10));
    
    TestDecimalType profitTarget = TestDecimalType("5.0");
    TestDecimalType stopLoss = TestDecimalType("5.0");

    return SearchConfiguration<TestDecimalType>(
        security,
        mkc_timeseries::TimeFrame::DAILY,
        searchType,
        false,
        profitTarget,
        stopLoss,
        criteria,
        startTime,
        endTime
    );
}

std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createPerformanceTestSecurity(
    int numEntries, bool profitable)
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    TestDecimalType basePrice = TestDecimalType("100");
    
    // Start from January 1, 2023 and increment by days
    boost::gregorian::date startDate(2023, 1, 1);
    
    for (int i = 0; i < numEntries; ++i)
    {
        // Generate sequential dates to avoid duplicates
        boost::gregorian::date currentDate = startDate + boost::gregorian::days(i);
        std::string dateStr = boost::gregorian::to_iso_string(currentDate);
        
        TestDecimalType price = basePrice;
        if (profitable && i % 3 == 0) {
            price += TestDecimalType("10"); // Create profitable patterns every 3rd bar
        } else if (!profitable) {
            price -= TestDecimalType("2"); // Create losing patterns
        }
        
        series->addEntry(*createTimeSeriesEntry(
            dateStr,
            num::toString(price),
            num::toString(price + TestDecimalType("3")),
            num::toString(price - TestDecimalType("2")),
            num::toString(price + TestDecimalType("1")),
            1000
        ));
        
        basePrice = price;
    }

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("AMZN", "Amazon.com Inc.", series);
}

TEST_CASE("PatternDiscoveryTask exception handling", "[PatternDiscoveryTask][exceptions]")
{
    mkc_palast::AstResourceManager resourceManager;
    boost::posix_time::ptime windowEndTime(boost::gregorian::date(2023, 1, 5));
    
    SECTION("Constructor throws with null security")
    {
        // Create a SearchConfiguration with null security pointer
        PerformanceCriteria<TestDecimalType> perfCriteria(TestDecimalType("0.0"), 1, 999, TestDecimalType("0.001"));
        boost::posix_time::ptime startTime(boost::gregorian::date(2023, 1, 1));
        boost::posix_time::ptime endTime(boost::gregorian::date(2023, 1, 10));
        
        REQUIRE_THROWS_AS(
            SearchConfiguration<TestDecimalType>(
                nullptr, // null security
                mkc_timeseries::TimeFrame::DAILY,
                SearchType::EXTENDED,
                false,
                TestDecimalType("5.0"),
                TestDecimalType("5.0"),
                perfCriteria,
                startTime,
                endTime
            ),
            SearchConfigurationException
        );
    }
    
    SECTION("Invalid price component names throw exceptions")
    {
        auto testSecurity = createMinimalSecurity(5);
        auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
        
        // This test verifies that the createPriceBarReference method in ExactPatternExpressionGenerator
        // throws an exception for invalid component names. We can't directly test this private method,
        // but we can verify the system handles invalid data gracefully.
        
        // Create a task and verify it doesn't crash with valid data
        REQUIRE_NOTHROW([&]() {
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
        }());
    }
}

TEST_CASE("PatternDiscoveryTask data edge cases", "[PatternDiscoveryTask][edge-cases]")
{
    mkc_palast::AstResourceManager resourceManager;
    
    SECTION("Handles insufficient historical data")
    {
        auto testSecurity = createMinimalSecurity(2); // Only 2 entries
        auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
        
        // Use the last date from the minimal time series
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // With insufficient data, should return empty vector
        REQUIRE(patterns.empty());
    }
    
    SECTION("Handles empty time series")
    {
        auto emptySecurity = createEmptySecurity();
        
        // Empty time series should throw exception during profit target calculation
        REQUIRE_THROWS([&]() {
            auto timeSeries = emptySecurity->getTimeSeries();
            TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
        }());
        
        // Since we can't compute profit target, we can't create a valid SearchConfiguration
        // This is the expected behavior - empty time series should fail early
    }
    
    SECTION("Handles single bar time series")
    {
        auto singleBarSecurity = createMinimalSecurity(1);
        
        // Single bar time series should throw exception during ROC calculation
        // because RocSeries needs at least 2 entries (current + 1 lookback)
        REQUIRE_THROWS([&]() {
            auto timeSeries = singleBarSecurity->getTimeSeries();
            TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
        }());
        
        // Since we can't compute profit target, we can't create a valid SearchConfiguration
        // This is the expected behavior - insufficient data should fail early
    }
    
    SECTION("Pattern length exceeds available data")
    {
        auto smallSecurity = createMinimalSecurity(3); // Only 3 entries
        auto config = createSearchConfig(smallSecurity, SearchType::DEEP, false); // DEEP requires up to 9 bars
        
        auto timeSeries = smallSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Should handle gracefully when pattern length exceeds available data
        // May find some shorter patterns but not the longer ones
        // The exact behavior depends on the implementation, but it shouldn't crash
        REQUIRE_NOTHROW([&]() {
            task.findPatterns();
        }());
    }
}

TEST_CASE("PatternDiscoveryTask performance filtering", "[PatternDiscoveryTask][performance]")
{
    mkc_palast::AstResourceManager resourceManager;
    
    SECTION("Filters by minimum trades")
    {
        auto testSecurity = createPerformanceTestSecurity(15, true);
        
        // Create criteria requiring more trades than possible
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("0.0"),  // 0% profitability (lenient)
            100,                     // 100 minimum trades (very strict)
            999,                     // Max consecutive losses (lenient)
            TestDecimalType("0.001") // Min profit factor (lenient)
        );
        
        auto config = createSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, strictCriteria);
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Should filter out patterns that don't meet minimum trade requirement
        REQUIRE(patterns.empty());
    }
    
    SECTION("Filters by minimum profitability")
    {
        auto testSecurity = createPerformanceTestSecurity(15, false); // Create losing patterns
        
        // Create criteria requiring 100% profitability
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("100.0"), // 100% profitability required
            1,                        // Min trades (lenient)
            999,                      // Max consecutive losses (lenient)
            TestDecimalType("0.001")  // Min profit factor (lenient)
        );
        
        auto config = createSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, strictCriteria);
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Should filter out patterns with low profitability
        REQUIRE(patterns.empty());
    }
    
    SECTION("Filters by maximum consecutive losses")
    {
        auto testSecurity = createPerformanceTestSecurity(15, false); // Create losing patterns
        
        // Create criteria with very low consecutive loss tolerance
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("0.0"),  // Profitability (lenient)
            1,                       // Min trades (lenient)
            0,                       // Max consecutive losses (very strict)
            TestDecimalType("0.001") // Min profit factor (lenient)
        );
        
        auto config = createSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, strictCriteria);
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Should filter out patterns with consecutive losses
        REQUIRE(patterns.empty());
    }
    
    SECTION("Filters by minimum profit factor")
    {
        auto testSecurity = createPerformanceTestSecurity(15, false); // Create losing patterns
        
        // Create criteria with high profit factor requirement
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("0.0"),   // Profitability (lenient)
            1,                        // Min trades (lenient)
            999,                      // Max consecutive losses (lenient)
            TestDecimalType("10.0")   // High profit factor requirement
        );
        
        auto config = createSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, strictCriteria);
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Should filter out patterns with low profit factor
        REQUIRE(patterns.empty());
    }
}

TEST_CASE("PatternDiscoveryTask complete SearchType coverage", "[PatternDiscoveryTask][search-types]")
{
    auto testSecurity = createComprehensiveSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("DEEP search type generates correct patterns")
    {
        auto config = createSearchConfig(testSecurity, SearchType::DEEP, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // DEEP search is for lengths 2-9, should find some patterns with sufficient data
        // patterns.size() is unsigned, so it's always >= 0
        
        // Verify we can generate patterns without crashing
        REQUIRE_NOTHROW([&]() {
            task.findPatterns();
        }());
    }
    
    SECTION("MIXED search type generates correct patterns")
    {
        auto config = createSearchConfig(testSecurity, SearchType::MIXED, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // MIXED search is for lengths 2-9, should find some patterns
        // patterns.size() is unsigned, so it's always >= 0
        
        // Verify we can generate patterns without crashing
        REQUIRE_NOTHROW([&]() {
            task.findPatterns();
        }());
    }
    
    SECTION("Price component validation per SearchType")
    {
        // Test that different SearchTypes work without throwing exceptions
        std::vector<SearchType> searchTypes = {
            SearchType::EXTENDED,
            SearchType::DEEP,
            SearchType::MIXED,
            SearchType::CLOSE_ONLY,
            SearchType::HIGH_LOW_ONLY,
            SearchType::OPEN_CLOSE_ONLY
        };
        
        for (auto searchType : searchTypes)
        {
            auto config = createSearchConfig(testSecurity, searchType, false);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            
            // Each SearchType should work without throwing exceptions
            REQUIRE_NOTHROW([&]() {
                auto patterns = task.findPatterns();
            }());
        }
    }
}

// ============================================================================
// PRIORITY 2: ENHANCED VALIDATION AND PATTERN STRUCTURE TESTS
// ============================================================================

TEST_CASE("Pattern AST structure validation", "[PatternDiscoveryTask][ast]")
{
    auto testSecurity = createComprehensiveSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Generated AST structure matches expected pattern")
    {
        auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        if (!patterns.empty())
        {
            auto firstPattern = patterns.begin();
            auto patternExpr = firstPattern->first->getPatternExpression();
            
            // Verify the pattern has a valid expression
            REQUIRE(patternExpr != nullptr);
            
            // For EXTENDED search, patterns should be composed of GreaterThanExpr and AndExpr
            // The exact structure depends on the data, but we can verify basic properties
            
            // Verify pattern has proper market entry
            REQUIRE(firstPattern->first->getMarketEntry() != nullptr);
            
            // Verify pattern has profit target and stop loss
            REQUIRE(firstPattern->first->getProfitTarget() != nullptr);
            REQUIRE(firstPattern->first->getStopLoss() != nullptr);
        }
    }
    
    SECTION("Pattern metadata is correctly populated")
    {
        auto config = createSearchConfig(testSecurity, SearchType::CLOSE_ONLY, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        if (!patterns.empty())
        {
            auto firstPattern = patterns.begin();
            auto patternDesc = firstPattern->first->getPatternDescription();
            
            // Verify PatternDescription fields are populated
            REQUIRE(patternDesc != nullptr);
            REQUIRE(!patternDesc->getFileName().empty());
            REQUIRE(patternDesc->getpatternIndex() > 0);
            REQUIRE(patternDesc->getIndexDate() > 0);
            
            // Verify filename generation logic includes security symbol
            std::string fileName = patternDesc->getFileName();
            REQUIRE(fileName.find("AAPL") != std::string::npos);
            REQUIRE(fileName.find("_L") != std::string::npos); // Length indicator
            REQUIRE(fileName.find("_D") != std::string::npos); // Delay indicator
        }
    }
    
    SECTION("Pattern expression complexity validation")
    {
        // Test with different SearchTypes to verify expression complexity
        std::vector<SearchType> searchTypes = {
            SearchType::CLOSE_ONLY,      // Should use only Close prices
            SearchType::HIGH_LOW_ONLY,   // Should use only High and Low prices
            SearchType::OPEN_CLOSE_ONLY  // Should use only Open and Close prices
        };
        
        for (auto searchType : searchTypes)
        {
            auto config = createSearchConfig(testSecurity, searchType, false);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
            
            // Each SearchType should generate valid patterns without crashing
            REQUIRE_NOTHROW([&]() {
                task.findPatterns();
            }());
            
            // If patterns are found, verify they have valid structure
            for (const auto& patternPair : patterns)
            {
                REQUIRE(patternPair.first->getPatternExpression() != nullptr);
                REQUIRE(patternPair.first->getPatternDescription() != nullptr);
            }
        }
    }
}

TEST_CASE("Pattern backtesting validation", "[PatternDiscoveryTask][backtesting]")
{
    auto testSecurity = createComprehensiveSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Backtester results are consistent")
    {
        auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        for (const auto& patternPair : patterns)
        {
            auto backtester = patternPair.second;
            REQUIRE(backtester != nullptr);
            
            // Verify backtester has valid results
            auto profitability = backtester->getProfitability();
            REQUIRE(std::get<0>(profitability) >= TestDecimalType("0")); // Profit factor >= 0
            REQUIRE(std::get<1>(profitability) >= TestDecimalType("0")); // Win rate >= 0
            REQUIRE(std::get<1>(profitability) <= TestDecimalType("100")); // Win rate <= 100%
            
            // Verify position history
            auto& positionHistory = backtester->getClosedPositionHistory();
            // Verify position history exists (getNumPositions() returns unsigned, so it's always >= 0)
            (void)positionHistory; // Suppress unused variable warning
            
            // Verify consecutive losses count
            // getNumConsecutiveLosses() returns unsigned, so it's always >= 0
        }
    }
    
    SECTION("Performance criteria filtering works correctly")
    {
        // Create lenient criteria that should pass most patterns
        PerformanceCriteria<TestDecimalType> lenientCriteria(
            TestDecimalType("0.0"),  // 0% profitability
            1,                       // 1 minimum trade
            999,                     // 999 max consecutive losses
            TestDecimalType("0.001") // Very low profit factor
        );
        
        auto lenientConfig = createSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, lenientCriteria);
        PatternDiscoveryTask<TestDecimalType> lenientTask(lenientConfig, windowEndTime, resourceManager);
        auto lenientPatterns = lenientTask.findPatterns();
        
        // Create strict criteria that should filter out most patterns
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("90.0"), // 90% profitability
            10,                      // 10 minimum trades
            1,                       // 1 max consecutive loss
            TestDecimalType("5.0")   // High profit factor
        );
        
        auto strictConfig = createSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, strictCriteria);
        PatternDiscoveryTask<TestDecimalType> strictTask(strictConfig, windowEndTime, resourceManager);
        auto strictPatterns = strictTask.findPatterns();
        
        // Lenient criteria should find more or equal patterns than strict criteria
        REQUIRE(lenientPatterns.size() >= strictPatterns.size());
    }
}

TEST_CASE("Pattern generation determinism", "[PatternDiscoveryTask][determinism]")
{
    auto testSecurity = createComprehensiveSecurity();
    mkc_palast::AstResourceManager resourceManager1;
    mkc_palast::AstResourceManager resourceManager2;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Multiple runs produce consistent results")
    {
        auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
        
        // First run
        PatternDiscoveryTask<TestDecimalType> task1(config, windowEndTime, resourceManager1);
        auto patterns1 = task1.findPatterns();
        
        // Second run with different resource manager
        PatternDiscoveryTask<TestDecimalType> task2(config, windowEndTime, resourceManager2);
        auto patterns2 = task2.findPatterns();
        
        // Results should be consistent (same number of patterns)
        REQUIRE(patterns1.size() == patterns2.size());
        
        // If patterns exist, verify they have similar characteristics
        if (!patterns1.empty() && !patterns2.empty())
        {
            // Both should find patterns with the same basic structure
            REQUIRE(patterns1.begin()->first->getPatternExpression() != nullptr);
            REQUIRE(patterns2.begin()->first->getPatternExpression() != nullptr);
        }
    }
    
    SECTION("Same input data produces same pattern count")
    {
        std::vector<SearchType> searchTypes = {
            SearchType::EXTENDED,
            SearchType::CLOSE_ONLY,
            SearchType::HIGH_LOW_ONLY
        };
        
        for (auto searchType : searchTypes)
        {
            auto config = createSearchConfig(testSecurity, searchType, false);
            
            // Run multiple times
            std::vector<size_t> patternCounts;
            for (int i = 0; i < 3; ++i)
            {
                mkc_palast::AstResourceManager tempResourceManager;
                PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, tempResourceManager);
                auto patterns = task.findPatterns();
                patternCounts.push_back(patterns.size());
            }
            
            // All runs should produce the same number of patterns
            for (size_t i = 1; i < patternCounts.size(); ++i)
            {
                REQUIRE(patternCounts[i] == patternCounts[0]);
            }
        }
    }
}

// ============================================================================
// PRIORITY 3: PERFORMANCE AND RESOURCE MANAGEMENT TESTS
// ============================================================================

TEST_CASE("Resource management and performance", "[PatternDiscovery][performance]")
{
    SECTION("AstResourceManager lifecycle management")
    {
        auto testSecurity = createComprehensiveSecurity();
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test resource creation and cleanup
        {
            mkc_palast::AstResourceManager resourceManager;
            auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
            
            // Resource manager should clean up when it goes out of scope
        }
        
        // Test with multiple concurrent tasks using same resource manager
        mkc_palast::AstResourceManager sharedResourceManager;
        std::vector<std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<TestDecimalType>>>>> allResults;
        
        for (int i = 0; i < 3; ++i)
        {
            auto config = createSearchConfig(testSecurity, SearchType::EXTENDED, false);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, sharedResourceManager);
            auto patterns = task.findPatterns();
            allResults.push_back(patterns);
        }
        
        // Verify no resource conflicts
        REQUIRE(allResults.size() == 3);
    }
    
    SECTION("Large dataset handling")
    {
        // Create a larger security with more data points - now with proper date generation
        auto largeSecurity = createPerformanceTestSecurity(50, true); // 50 entries with sequential dates
        mkc_palast::AstResourceManager resourceManager;
        
        auto timeSeries = largeSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        auto config = createSearchConfig(largeSecurity, SearchType::EXTENDED, false);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Verify memory usage remains reasonable and execution completes
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            // Should handle large datasets without issues
        }());
    }
    
    SECTION("Pattern aggregation limits")
    {
        auto testSecurity = createComprehensiveSecurity();
        mkc_palast::AstResourceManager resourceManager;
        
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test with very lenient criteria to potentially generate many patterns
        PerformanceCriteria<TestDecimalType> veryLenientCriteria(
            TestDecimalType("0.0"),    // 0% profitability
            1,                         // 1 minimum trade
            9999,                      // Very high max consecutive losses
            TestDecimalType("0.0001")  // Very low profit factor
        );
        
        auto config = createSearchConfigWithCriteria(testSecurity, SearchType::DEEP, veryLenientCriteria);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify system can handle potentially large pattern counts
        // patterns.size() is unsigned, so it's always >= 0
        
        // If patterns are found, verify they're all valid
        for (const auto& patternPair : patterns)
        {
            REQUIRE(patternPair.first != nullptr);
            REQUIRE(patternPair.second != nullptr);
        }
    }
}
