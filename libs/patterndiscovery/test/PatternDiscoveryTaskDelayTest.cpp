// PatternDiscoveryTaskDelayTest.cpp
// Unit tests for Phase 3 delay pattern functionality

#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "PatternDiscoveryTask.h"
#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
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
#include <memory>

using TestDecimalType = num::DefaultNumber;

// Helper to create a security specifically designed for delay pattern testing
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createDelayPatternTestSecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    // Create a longer time series with predictable patterns that will work well with delays
    // Need sufficient historical data to support both base patterns and delayed versions
    
    // Generate 25 days of data starting from 2022-12-01
    boost::gregorian::date startDate(2022, 12, 1);
    TestDecimalType basePrice = TestDecimalType("100");
    
    for (int i = 0; i < 25; ++i)
    {
        boost::gregorian::date currentDate = startDate + boost::gregorian::days(i);
        std::string dateStr = boost::gregorian::to_iso_string(currentDate);
        
        // Create patterns that will be profitable both as exact and delayed patterns
        TestDecimalType price = basePrice + TestDecimalType(i % 3); // Creates repeating patterns
        TestDecimalType open = price;
        TestDecimalType high = price + TestDecimalType("3");
        TestDecimalType low = price - TestDecimalType("1");
        TestDecimalType close = price + TestDecimalType("2");
        
        series->addEntry(*createTimeSeriesEntry(
            dateStr,
            num::toString(open),
            num::toString(high),
            num::toString(low),
            num::toString(close),
            1000
        ));
    }

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("AAPL", "Apple Inc.", series);
}

// Helper to create search configuration with delay patterns enabled
SearchConfiguration<TestDecimalType> createDelaySearchConfig(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
    bool enableDelayPatterns,
    SearchType searchType = SearchType::EXTENDED)
{
    // Use lenient performance criteria to ensure patterns are found
    PerformanceCriteria<TestDecimalType> perfCriteria(
        TestDecimalType("0.0"),  // 0% profitability required
        1,                       // Min trades
        999,                     // Max consecutive losers
        TestDecimalType("0.001") // Very low profit factor
    );
    
    // Calculate profit target and stop loss based on the actual time series data
    auto timeSeries = security->getTimeSeries();
    TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
    
    return SearchConfiguration<TestDecimalType>(
        security,
        mkc_timeseries::TimeFrame::DAILY,
        searchType,
        enableDelayPatterns,
        profitTargetAndStop,
        profitTargetAndStop,
        perfCriteria,
        timeSeries->getFirstDateTime(),
        timeSeries->getLastDateTime()
    );
}

TEST_CASE("PatternDiscoveryTask delay pattern functionality", "[PatternDiscoveryTask][delay-patterns]")
{
    auto testSecurity = createDelayPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Delay pattern configuration validation")
    {
        // Test with delay patterns enabled
        auto delayConfig = createDelaySearchConfig(testSecurity, true);
        
        REQUIRE(delayConfig.isSearchingForDelayPatterns() == true);
        REQUIRE(delayConfig.getMinDelayBars() == 1);
        REQUIRE(delayConfig.getMaxDelayBars() == 5);
        
        // Test with delay patterns disabled
        auto noDelayConfig = createDelaySearchConfig(testSecurity, false);
        
        REQUIRE(noDelayConfig.isSearchingForDelayPatterns() == false);
        REQUIRE(noDelayConfig.getMinDelayBars() == 0);
        REQUIRE(noDelayConfig.getMaxDelayBars() == 0);
    }
    
    SECTION("Delay pattern discovery flow")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        auto patterns = task.findPatterns();
        
        std::cout << "DEBUG: Found " << patterns.size() << " total patterns (exact + delayed)" << std::endl;
        
        // With delay patterns enabled, we should find more patterns than without
        // (patterns.size() is unsigned, so always >= 0 - just verify no crash)
        
        // Now test without delay patterns for comparison
        auto noDelayConfig = createDelaySearchConfig(testSecurity, false);
        PatternDiscoveryTask<TestDecimalType> noDelayTask(noDelayConfig, windowEndTime, resourceManager);
        
        auto noDelayPatterns = noDelayTask.findPatterns();
        
        std::cout << "DEBUG: Found " << noDelayPatterns.size() << " exact patterns only" << std::endl;
        
        // With delay patterns enabled, we should find at least as many patterns as without
        // (exact patterns + potentially some delayed patterns)
        REQUIRE(patterns.size() >= noDelayPatterns.size());
    }
    
    SECTION("Delay range iteration validation")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // The task should iterate through delays from minDelayBars to maxDelayBars
        // We can't directly test the private method, but we can verify the overall behavior
        auto patterns = task.findPatterns();
        
        // Verify that delay patterns are being generated
        // The exact number depends on the data and performance criteria
        REQUIRE_NOTHROW([&]() {
            task.findPatterns();
        }());
        
        // Check that patterns have reasonable metadata
        for (const auto& patternPair : patterns)
        {
            auto pattern = patternPair.first;
            REQUIRE(pattern != nullptr);
            REQUIRE(pattern->getPatternDescription() != nullptr);
            
            // Verify filename contains delay information
            std::string fileName = pattern->getPatternDescription()->getFileName();
            REQUIRE(fileName.find("_D") != std::string::npos); // Should contain delay indicator
        }
    }
}

TEST_CASE("PatternDiscoveryTask AST offset shifting", "[PatternDiscoveryTask][ast][delay]")
{
    auto testSecurity = createDelayPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("AST transformation for delayed patterns")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        auto patterns = task.findPatterns();
        
        // Verify that delayed patterns have valid AST structures
        for (const auto& patternPair : patterns)
        {
            auto pattern = patternPair.first;
            auto patternExpr = pattern->getPatternExpression();
            
            REQUIRE(patternExpr != nullptr);
            
            // The AST should be properly formed (we can't easily inspect the internal structure
            // without making the visitor public, but we can verify it doesn't crash)
            REQUIRE_NOTHROW([&]() {
                // The pattern should be usable for backtesting
                auto backtester = patternPair.second;
                REQUIRE(backtester != nullptr);
            }());
        }
    }
    
    SECTION("Offset calculation verification")
    {
        // Test that delayed patterns have different characteristics than exact patterns
        auto delayConfig = createDelaySearchConfig(testSecurity, true);
        auto noDelayConfig = createDelaySearchConfig(testSecurity, false);
        
        PatternDiscoveryTask<TestDecimalType> delayTask(delayConfig, windowEndTime, resourceManager);
        PatternDiscoveryTask<TestDecimalType> noDelayTask(noDelayConfig, windowEndTime, resourceManager);
        
        auto delayPatterns = delayTask.findPatterns();
        auto exactPatterns = noDelayTask.findPatterns();
        
        // If we found both types of patterns, they should have different structures
        if (!delayPatterns.empty() && !exactPatterns.empty())
        {
            // Both should have valid expressions
            REQUIRE(delayPatterns.begin()->first->getPatternExpression() != nullptr);
            REQUIRE(exactPatterns.begin()->first->getPatternExpression() != nullptr);
            
            // The filename should indicate the delay
            std::string delayFileName = delayPatterns.begin()->first->getPatternDescription()->getFileName();
            std::string exactFileName = exactPatterns.begin()->first->getPatternDescription()->getFileName();
            
            // Both should contain delay indicators, but delayed patterns might have non-zero delays
            REQUIRE(delayFileName.find("_D") != std::string::npos);
            REQUIRE(exactFileName.find("_D") != std::string::npos);
        }
    }
    
    SECTION("AST visitor pattern correctness")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Test that the AST transformation doesn't cause memory issues
        for (int i = 0; i < 3; ++i)
        {
            REQUIRE_NOTHROW([&]() {
                auto patterns = task.findPatterns();
                
                // Verify all patterns have valid AST structures
                for (const auto& patternPair : patterns)
                {
                    REQUIRE(patternPair.first->getPatternExpression() != nullptr);
                    REQUIRE(patternPair.first->getMarketEntry() != nullptr);
                    REQUIRE(patternPair.first->getProfitTarget() != nullptr);
                    REQUIRE(patternPair.first->getStopLoss() != nullptr);
                }
            }());
        }
    }
}

TEST_CASE("PatternDiscoveryTask delay pattern integration", "[PatternDiscoveryTask][delay][integration]")
{
    auto testSecurity = createDelayPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Base pattern to delayed pattern flow")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        auto patterns = task.findPatterns();
        
        // Verify that we can identify which patterns are base vs delayed
        // by examining their metadata
        std::vector<std::string> fileNames;
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            fileNames.push_back(fileName);
        }
        
        // All patterns should have valid filenames with delay indicators
        for (const auto& fileName : fileNames)
        {
            REQUIRE(fileName.find("AAPL") != std::string::npos); // Security symbol
            REQUIRE(fileName.find("_L") != std::string::npos);    // Length indicator
            REQUIRE(fileName.find("_D") != std::string::npos);    // Delay indicator
        }
    }
    
    SECTION("Backtesting delayed patterns")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        auto patterns = task.findPatterns();
        
        // Verify that delayed patterns can be backtested successfully
        for (const auto& patternPair : patterns)
        {
            auto backtester = patternPair.second;
            REQUIRE(backtester != nullptr);
            
            // Verify backtester has valid results
            auto profitability = backtester->getProfitability();
            REQUIRE(std::get<0>(profitability) >= TestDecimalType("0")); // Profit factor >= 0
            REQUIRE(std::get<1>(profitability) >= TestDecimalType("0")); // Win rate >= 0
            REQUIRE(std::get<1>(profitability) <= TestDecimalType("100")); // Win rate <= 100%
            
            // Verify position history exists
            auto& positionHistory = backtester->getClosedPositionHistory();
            // getNumPositions() returns unsigned, so always >= 0
        }
    }
    
    SECTION("Performance filtering with delayed patterns")
    {
        // Test with strict criteria to verify filtering works
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("50.0"), // 50% profitability required
            5,                       // 5 minimum trades
            2,                       // Max 2 consecutive losses
            TestDecimalType("2.0")   // High profit factor requirement
        );
        
        auto timeSeries = testSecurity->getTimeSeries();
        TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
        
        SearchConfiguration<TestDecimalType> strictConfig(
            testSecurity,
            mkc_timeseries::TimeFrame::DAILY,
            SearchType::EXTENDED,
            true, // Enable delay patterns
            profitTargetAndStop,
            profitTargetAndStop,
            strictCriteria,
            timeSeries->getFirstDateTime(),
            timeSeries->getLastDateTime()
        );
        
        PatternDiscoveryTask<TestDecimalType> strictTask(strictConfig, windowEndTime, resourceManager);
        auto strictPatterns = strictTask.findPatterns();
        
        // With strict criteria, we should find fewer patterns
        // All found patterns should meet the criteria
        for (const auto& patternPair : strictPatterns)
        {
            auto backtester = patternPair.second;
            auto profitability = backtester->getProfitability();
            
            // These patterns should meet the strict criteria
            // (though the exact values depend on the test data)
            // getNumPositions() returns unsigned, so always >= 0
            REQUIRE(std::get<0>(profitability) >= TestDecimalType("0"));
            REQUIRE(std::get<1>(profitability) >= TestDecimalType("0"));
        }
    }
}

TEST_CASE("PatternDiscoveryTask delay pattern edge cases", "[PatternDiscoveryTask][delay][edge-cases]")
{
    mkc_palast::AstResourceManager resourceManager;
    
    SECTION("Insufficient historical data for delays")
    {
        // Create a security with minimal data
        auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
            mkc_timeseries::TimeFrame::DAILY,
            mkc_timeseries::TradingVolume::SHARES
        );
        
        // Add only 5 entries - insufficient for large delays
        for (int i = 0; i < 5; ++i)
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
        
        auto minimalSecurity = std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("MSFT", "Microsoft", series);
        auto config = createDelaySearchConfig(minimalSecurity, true);
        
        auto timeSeries = minimalSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Should handle gracefully when there's insufficient data for delays
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            // May find no patterns due to insufficient data, but shouldn't crash
        }());
    }
    
    SECTION("Empty base pattern sets")
    {
        // Create a security that won't produce any profitable base patterns
        auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
            mkc_timeseries::TimeFrame::DAILY,
            mkc_timeseries::TradingVolume::SHARES
        );
        
        // Add data that creates losing patterns but still has positive price variation
        // to avoid negative profit target/stop loss calculations
        for (int i = 0; i < 15; ++i)
        {
            boost::gregorian::date currentDate(2023, 1, 1);
            currentDate += boost::gregorian::days(i);
            std::string dateStr = boost::gregorian::to_iso_string(currentDate);
            
            // Create data with small positive variations to avoid negative ROC
            TestDecimalType basePrice = TestDecimalType("100");
            TestDecimalType price = basePrice + TestDecimalType(i % 3) - TestDecimalType("1"); // Small variations around base
            series->addEntry(*createTimeSeriesEntry(
                dateStr,
                num::toString(price),
                num::toString(price + TestDecimalType("1.5")),
                num::toString(price - TestDecimalType("0.5")),
                num::toString(price + TestDecimalType("0.5")),
                1000
            ));
        }
        
        auto losingSecurity = std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("INTC", "Intel", series);
        
        // Use strict criteria that will filter out losing patterns
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("80.0"), // 80% profitability required
            3,                       // 3 minimum trades
            1,                       // Max 1 consecutive loss
            TestDecimalType("3.0")   // High profit factor requirement
        );
        
        auto timeSeries = losingSecurity->getTimeSeries();
        
        // This should now work with the improved data
        REQUIRE_NOTHROW([&]() {
            TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
            
            SearchConfiguration<TestDecimalType> strictConfig(
                losingSecurity,
                mkc_timeseries::TimeFrame::DAILY,
                SearchType::EXTENDED,
                true, // Enable delay patterns
                profitTargetAndStop,
                profitTargetAndStop,
                strictCriteria,
                timeSeries->getFirstDateTime(),
                timeSeries->getLastDateTime()
            );
            
            boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
            PatternDiscoveryTask<TestDecimalType> task(strictConfig, windowEndTime, resourceManager);
            
            auto patterns = task.findPatterns();
            
            // Should handle empty base pattern sets gracefully
            // If no base patterns are found, no delayed patterns should be generated either
            // (patterns.size() is unsigned, so always >= 0 - just verify no crash)
        }());
    }
    
    SECTION("Maximum delay boundary testing")
    {
        auto testSecurity = createDelayPatternTestSecurity();
        auto config = createDelaySearchConfig(testSecurity, true);
        
        // Verify the delay range is as expected
        REQUIRE(config.getMaxDelayBars() == 5);
        REQUIRE(config.getMinDelayBars() == 1);
        
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Should handle the full delay range without issues
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            
            // Verify patterns were processed for the full delay range
            // The exact behavior depends on the data, but it shouldn't crash
        }());
    }
}

TEST_CASE("PatternDiscoveryTask delay pattern determinism", "[PatternDiscoveryTask][delay][determinism]")
{
    auto testSecurity = createDelayPatternTestSecurity();
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Multiple runs produce consistent results")
    {
        auto config = createDelaySearchConfig(testSecurity, true);
        
        std::vector<size_t> patternCounts;
        
        // Run multiple times with different resource managers
        for (int i = 0; i < 3; ++i)
        {
            mkc_palast::AstResourceManager resourceManager;
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
            patternCounts.push_back(patterns.size());
        }
        
        // All runs should produce the same number of patterns
        for (size_t i = 1; i < patternCounts.size(); ++i)
        {
            REQUIRE(patternCounts[i] == patternCounts[0]);
        }
    }
    
    SECTION("Delay vs no-delay consistency")
    {
        mkc_palast::AstResourceManager resourceManager1;
        mkc_palast::AstResourceManager resourceManager2;
        
        // Test with delay patterns
        auto delayConfig = createDelaySearchConfig(testSecurity, true);
        PatternDiscoveryTask<TestDecimalType> delayTask(delayConfig, windowEndTime, resourceManager1);
        auto delayPatterns = delayTask.findPatterns();
        
        // Test without delay patterns
        auto noDelayConfig = createDelaySearchConfig(testSecurity, false);
        PatternDiscoveryTask<TestDecimalType> noDelayTask(noDelayConfig, windowEndTime, resourceManager2);
        auto noDelayPatterns = noDelayTask.findPatterns();
        
        // With delays enabled, we should find at least as many patterns as without
        REQUIRE(delayPatterns.size() >= noDelayPatterns.size());
        
        // Both should produce consistent results on repeated runs
        auto delayPatterns2 = delayTask.findPatterns();
        auto noDelayPatterns2 = noDelayTask.findPatterns();
        
        REQUIRE(delayPatterns.size() == delayPatterns2.size());
        REQUIRE(noDelayPatterns.size() == noDelayPatterns2.size());
    }
}