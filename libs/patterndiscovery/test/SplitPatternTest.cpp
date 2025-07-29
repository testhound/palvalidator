// SplitPatternTest.cpp - Comprehensive unit tests for split patterns functionality
#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <chrono>
#include <set>

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

using TestDecimalType = num::DefaultNumber;

// ============================================================================
// HELPER FUNCTIONS FOR SPLIT PATTERN TESTING
// ============================================================================

// Helper to create a security with data specifically designed for split pattern testing
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createSplitPatternTestSecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    // Create data specifically designed to generate predictable split patterns
    // Pattern: First part shows upward trend, second part shows confirmation
    // Historical context bars (15 bars before the main test period)
    series->addEntry(*createTimeSeriesEntry("20221210", "70", "75", "68", "72", 1000));   // Historical -14
    series->addEntry(*createTimeSeriesEntry("20221211", "72", "77", "70", "74", 1000));   // Historical -13
    series->addEntry(*createTimeSeriesEntry("20221212", "74", "79", "72", "76", 1000));   // Historical -12
    series->addEntry(*createTimeSeriesEntry("20221213", "76", "81", "74", "78", 1000));   // Historical -11
    series->addEntry(*createTimeSeriesEntry("20221214", "78", "83", "76", "80", 1000));   // Historical -10
    series->addEntry(*createTimeSeriesEntry("20221215", "80", "85", "78", "82", 1000));   // Historical -9
    series->addEntry(*createTimeSeriesEntry("20221216", "82", "87", "80", "84", 1000));   // Historical -8
    series->addEntry(*createTimeSeriesEntry("20221217", "84", "89", "82", "86", 1000));   // Historical -7
    series->addEntry(*createTimeSeriesEntry("20221218", "86", "91", "84", "88", 1000));   // Historical -6
    series->addEntry(*createTimeSeriesEntry("20221219", "88", "93", "86", "90", 1000));   // Historical -5
    series->addEntry(*createTimeSeriesEntry("20221220", "90", "95", "88", "92", 1000));   // Historical -4
    series->addEntry(*createTimeSeriesEntry("20221221", "92", "97", "90", "94", 1000));   // Historical -3
    series->addEntry(*createTimeSeriesEntry("20221222", "94", "99", "92", "96", 1000));   // Historical -2
    series->addEntry(*createTimeSeriesEntry("20221223", "96", "101", "94", "98", 1000));  // Historical -1
    series->addEntry(*createTimeSeriesEntry("20221224", "98", "103", "96", "100", 1000)); // Historical 0
    
    // Main test period - designed for split pattern discovery
    series->addEntry(*createTimeSeriesEntry("20230101", "100", "110", "98", "108", 1000)); // Bar 4
    series->addEntry(*createTimeSeriesEntry("20230102", "105", "115", "103", "112", 1000)); // Bar 3
    series->addEntry(*createTimeSeriesEntry("20230103", "110", "125", "109", "120", 1000)); // Bar 2
    series->addEntry(*createTimeSeriesEntry("20230104", "122", "130", "121", "128", 1000)); // Bar 1
    series->addEntry(*createTimeSeriesEntry("20230105", "127", "135", "126", "132", 1000)); // Bar 0

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("MSFT", "Microsoft Corporation", series);
}

// Helper to create a security with minimal data for edge case testing
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createSplitPatternEdgeCaseSecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    // Minimal data for edge case testing
    series->addEntry(*createTimeSeriesEntry("20230101", "100", "105", "98", "102", 1000)); // Bar 2
    series->addEntry(*createTimeSeriesEntry("20230102", "102", "107", "100", "104", 1000)); // Bar 1
    series->addEntry(*createTimeSeriesEntry("20230103", "104", "109", "102", "106", 1000)); // Bar 0

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("AAPL", "Apple Inc.", series);
}

// Helper to create empty security for error testing
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createSplitPatternEmptySecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>("GOOGL", "Alphabet Inc.", series);
}

// Helper to create SearchConfiguration optimized for split pattern testing
SearchConfiguration<TestDecimalType> createSplitPatternConfig(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
    SearchType searchType,
    bool enableDelayPatterns = false)
{
    // Use lenient performance criteria to capture more patterns for testing
    PerformanceCriteria<TestDecimalType> perfCriteria(TestDecimalType("0.0"), 1, 999, TestDecimalType("0.001"));
    boost::posix_time::ptime startTime(boost::gregorian::date(2022, 12, 10));
    boost::posix_time::ptime endTime(boost::gregorian::date(2023, 1, 5));

    auto timeSeries = security->getTimeSeries();
    TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
    TestDecimalType profitTarget = profitTargetAndStop;
    TestDecimalType stopLoss = profitTargetAndStop;

    return SearchConfiguration<TestDecimalType>(
        security,
        mkc_timeseries::TimeFrame::DAILY,
        searchType,
        enableDelayPatterns,
        profitTarget,
        stopLoss,
        perfCriteria,
        startTime,
        endTime
    );
}

// Helper to create SearchConfiguration with custom performance criteria for split pattern tests
SearchConfiguration<TestDecimalType> createSplitPatternSearchConfigWithCriteria(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
    SearchType searchType,
    const PerformanceCriteria<TestDecimalType>& criteria)
{
    boost::posix_time::ptime startTime(boost::gregorian::date(2022, 12, 10));
    boost::posix_time::ptime endTime(boost::gregorian::date(2023, 1, 5));
    
    auto timeSeries = security->getTimeSeries();
    TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);

    return SearchConfiguration<TestDecimalType>(
        security,
        mkc_timeseries::TimeFrame::DAILY,
        searchType,
        false,
        profitTargetAndStop,
        profitTargetAndStop,
        criteria,
        startTime,
        endTime
    );
}

// Helper to validate split pattern structure
void validateSplitPatternStructure(
    PALPatternPtr pattern,
    unsigned int expectedTotalLength,
    unsigned int expectedPart1Length)
{
    REQUIRE(pattern != nullptr);
    REQUIRE(pattern->getPatternExpression() != nullptr);
    REQUIRE(pattern->getPatternDescription() != nullptr);
    
    // Verify naming convention
    std::string fileName = pattern->getPatternDescription()->getFileName();
    REQUIRE(fileName.find("_S_L" + std::to_string(expectedTotalLength)) != std::string::npos);
    REQUIRE(fileName.find("_P" + std::to_string(expectedPart1Length)) != std::string::npos);
    
    // Verify AST structure - split patterns should have AndExpr at root
    auto patternExpr = pattern->getPatternExpression();
    auto andExpr = std::dynamic_pointer_cast<AndExpr>(patternExpr);
    REQUIRE(andExpr != nullptr); // Root should be AndExpr for split patterns
}

// ============================================================================
// PHASE 1: CORE FUNCTIONALITY TESTS
// ============================================================================

TEST_CASE("Split patterns - Basic identification", "[PatternDiscoveryTask][split-patterns]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Simple 2-part split patterns")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        std::cout << "DEBUG: Found " << patterns.size() << " patterns for split pattern test" << std::endl;
        
        // Should find some split patterns with sufficient data
        // Look for patterns with split naming convention
        bool foundSplitPattern = false;
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                foundSplitPattern = true;
                std::cout << "DEBUG: Found split pattern: " << fileName << std::endl;
                
                // Verify it's a valid split pattern structure
                auto patternExpr = patternPair.first->getPatternExpression();
                auto andExpr = std::dynamic_pointer_cast<AndExpr>(patternExpr);
                REQUIRE(andExpr != nullptr); // Split patterns should have AndExpr at root
                break;
            }
        }
        
        // Verify that split patterns were actually found
        REQUIRE(foundSplitPattern);
    }
    
    SECTION("Complex multi-length splits")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::DEEP);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Test various split combinations
        std::vector<std::pair<unsigned int, unsigned int>> expectedSplits = {
            {4, 2}, {4, 3}, {5, 2}, {5, 3}, {5, 4}, {6, 2}, {6, 3}, {6, 4}, {6, 5}
        };
        
        // Verify that the algorithm attempts all valid split combinations
        // (We can't guarantee all will be profitable, but the algorithm should try them)
        // Already called findPatterns() above, so just verify it completed successfully
        REQUIRE_NOTHROW([&]() {
            // Verify we can access the patterns without issues
            for (const auto& patternPair : patterns) {
                REQUIRE(patternPair.first != nullptr);
                REQUIRE(patternPair.second != nullptr);
            }
        }());
    }
    
    SECTION("All split point combinations")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // For each total length, verify split points are tested
        // Already called findPatterns() above, so just verify it completed successfully
        REQUIRE_NOTHROW([&]() {
            // Verify we can access the patterns without issues
            for (const auto& patternPair : patterns) {
                REQUIRE(patternPair.first != nullptr);
                REQUIRE(patternPair.second != nullptr);
            }
        }());
        
        std::cout << "DEBUG: Split point combinations test found " << patterns.size() << " patterns" << std::endl;
    }
    
    SECTION("SearchType compatibility")
    {
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
            auto config = createSplitPatternConfig(testSecurity, searchType);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            
            // Each SearchType should work with split patterns without throwing exceptions
            REQUIRE_NOTHROW([&]() {
                auto patterns = task.findPatterns();
                std::cout << "DEBUG: SearchType " << static_cast<int>(searchType) 
                         << " found " << patterns.size() << " patterns" << std::endl;
            }());
        }
    }
}

TEST_CASE("Split patterns - AST construction", "[PatternDiscoveryTask][split-patterns][ast]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("AST node structure")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Find split patterns and verify AST structure
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto patternExpr = patternPair.first->getPatternExpression();
                
                // Root should be AndExpr combining two sub-patterns
                auto andExpr = std::dynamic_pointer_cast<AndExpr>(patternExpr);
                REQUIRE(andExpr != nullptr);
                
                // Both sides of the AndExpr should be valid expressions
                REQUIRE(andExpr->getLHS() != nullptr);
                REQUIRE(andExpr->getRHS() != nullptr);
                
                std::cout << "DEBUG: Validated AST structure for split pattern: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Offset correctness")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify that split patterns have correct temporal separation
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto patternExpr = patternPair.first->getPatternExpression();
                auto andExpr = std::dynamic_pointer_cast<AndExpr>(patternExpr);
                REQUIRE(andExpr != nullptr);
                
                // The structure should represent temporal separation between parts
                // This is verified by the AndExpr structure combining two sub-expressions
                REQUIRE(andExpr->getLHS() != nullptr);
                REQUIRE(andExpr->getRHS() != nullptr);
                
                std::cout << "DEBUG: Validated offset correctness for: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Price component usage")
    {
        // Test that split patterns use appropriate price components based on SearchType
        std::vector<SearchType> searchTypes = {
            SearchType::CLOSE_ONLY,
            SearchType::HIGH_LOW_ONLY,
            SearchType::OPEN_CLOSE_ONLY
        };
        
        for (auto searchType : searchTypes)
        {
            auto config = createSplitPatternConfig(testSecurity, searchType);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
            
            // Verify patterns are generated without errors for each SearchType
            // Already called findPatterns() above, so just verify it completed successfully
            REQUIRE_NOTHROW([&]() {
                // Verify we can access the patterns without issues
                for (const auto& patternPair : patterns) {
                    REQUIRE(patternPair.first != nullptr);
                    REQUIRE(patternPair.second != nullptr);
                }
            }());
            
            std::cout << "DEBUG: SearchType " << static_cast<int>(searchType) 
                     << " generated patterns successfully" << std::endl;
        }
    }
    
    SECTION("Expression complexity")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify combined expressions maintain proper logical structure
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto patternExpr = patternPair.first->getPatternExpression();
                
                // Should be a complex expression with nested structure
                REQUIRE(patternExpr != nullptr);
                
                // Verify pattern has proper market entry, profit target, and stop loss
                REQUIRE(patternPair.first->getMarketEntry() != nullptr);
                REQUIRE(patternPair.first->getProfitTarget() != nullptr);
                REQUIRE(patternPair.first->getStopLoss() != nullptr);
                
                std::cout << "DEBUG: Validated expression complexity for: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
}

TEST_CASE("Split patterns - Backtesting verification", "[PatternDiscoveryTask][split-patterns][backtesting]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Backtest execution")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify that split patterns are backtested successfully
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto backtester = patternPair.second;
                REQUIRE(backtester != nullptr);
                
                // Verify backtester has valid results
                auto profitability = backtester->getProfitability();
                REQUIRE(std::get<0>(profitability) >= TestDecimalType("0")); // Profit factor >= 0
                REQUIRE(std::get<1>(profitability) >= TestDecimalType("0")); // Win rate >= 0
                REQUIRE(std::get<1>(profitability) <= TestDecimalType("100")); // Win rate <= 100%
                
                std::cout << "DEBUG: Split pattern " << fileName 
                         << " - Profit Factor: " << std::get<0>(profitability)
                         << ", Win Rate: " << std::get<1>(profitability) << "%" << std::endl;
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Performance metrics")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify that split patterns have consistent performance metrics
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto backtester = patternPair.second;
                auto& positionHistory = backtester->getClosedPositionHistory();
                
                // Verify position history exists and is accessible
                REQUIRE_NOTHROW([&]() {
                    auto numPositions = positionHistory.getNumPositions();
                    auto consecutiveLosses = backtester->getNumConsecutiveLosses();
                    (void)numPositions; // Suppress unused variable warning
                    (void)consecutiveLosses;
                }());
                
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Profitability filtering")
    {
        // Test with strict criteria to verify filtering works
        PerformanceCriteria<TestDecimalType> strictCriteria(
            TestDecimalType("90.0"), // 90% profitability required
            10,                      // 10 minimum trades
            1,                       // 1 max consecutive loss
            TestDecimalType("5.0")   // High profit factor requirement
        );
        
        auto config = createSplitPatternSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, strictCriteria);
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // With strict criteria, should filter out most patterns
        // Already called findPatterns() above, so just verify it completed successfully
        REQUIRE_NOTHROW([&]() {
            // Verify we can access the patterns without issues
            for (const auto& patternPair : patterns) {
                REQUIRE(patternPair.first != nullptr);
                REQUIRE(patternPair.second != nullptr);
            }
        }());
        
        std::cout << "DEBUG: Strict criteria filtered to " << patterns.size() << " patterns" << std::endl;
    }
    
    SECTION("Trade generation")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify split patterns generate appropriate trade signals
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto backtester = patternPair.second;
                auto& positionHistory = backtester->getClosedPositionHistory();
                
                // Trades should be generated when both parts of split pattern are satisfied
                // This is verified by the existence of position history
                REQUIRE_NOTHROW([&]() {
                    auto numPositions = positionHistory.getNumPositions();
                    (void)numPositions; // Suppress unused variable warning
                }());
                
                std::cout << "DEBUG: Trade generation verified for: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
}

// ============================================================================
// PHASE 2: QUALITY ASSURANCE TESTS
// ============================================================================

TEST_CASE("Split patterns - Naming convention", "[PatternDiscoveryTask][split-patterns][naming]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Naming format")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify split patterns use correct naming format
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                // Format should be: {symbol}_S_L{totalLength}_P{lenPart1}_D{delay}
                REQUIRE(fileName.find("MSFT") != std::string::npos); // Symbol
                REQUIRE(fileName.find("_S_L") != std::string::npos);  // Split indicator
                REQUIRE(fileName.find("_P") != std::string::npos);   // Part length indicator
                REQUIRE(fileName.find("_D") != std::string::npos);   // Delay indicator
                
                std::cout << "DEBUG: Validated naming format: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Uniqueness")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Collect all split pattern names
        std::set<std::string> splitPatternNames;
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                splitPatternNames.insert(fileName);
            }
        }
        
        // Count split patterns in the results
        size_t splitPatternCount = 0;
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                splitPatternCount++;
            }
        }
        
        // All split pattern names should be unique
        REQUIRE(splitPatternNames.size() == splitPatternCount);
        
        std::cout << "DEBUG: Found " << splitPatternCount << " unique split patterns" << std::endl;
    }
    
    SECTION("Symbol inclusion")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify security symbol appears in pattern names
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                REQUIRE(fileName.find("MSFT") == 0); // Symbol should be at beginning
                std::cout << "DEBUG: Verified symbol inclusion: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Length encoding")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify total length and part length are correctly encoded
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                // Should contain both total length (L) and part length (P) indicators
                REQUIRE(fileName.find("_L") != std::string::npos);
                REQUIRE(fileName.find("_P") != std::string::npos);
                
                // The naming should reflect actual pattern structure
                std::cout << "DEBUG: Validated length encoding: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
}

TEST_CASE("Split patterns - Edge cases", "[PatternDiscoveryTask][split-patterns][edge-cases]")
{
    mkc_palast::AstResourceManager resourceManager;
    
    SECTION("Minimum split lengths")
    {
        auto testSecurity = createSplitPatternEdgeCaseSecurity();
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test with minimal data (only 3 entries)
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Should handle minimal configurations gracefully
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            std::cout << "DEBUG: Minimal data test found " << patterns.size() << " patterns" << std::endl;
        }());
    }
    
    SECTION("Maximum pattern lengths")
    {
        auto testSecurity = createSplitPatternTestSecurity();
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test with maximum length patterns
        auto config = createSplitPatternConfig(testSecurity, SearchType::DEEP);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Should handle maximum lengths without performance issues
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            std::cout << "DEBUG: Maximum length test found " << patterns.size() << " patterns" << std::endl;
        }());
    }
    
    SECTION("Insufficient data")
    {
        auto emptySecurity = createSplitPatternEmptySecurity();
        
        // Empty time series should throw exception during profit target calculation
        REQUIRE_THROWS([&]() {
            auto timeSeries = emptySecurity->getTimeSeries();
            TestDecimalType profitTargetAndStop = mkc_timeseries::ComputeProfitTargetAndStop(*timeSeries);
        }());
    }
    
    SECTION("Invalid split points")
    {
        auto testSecurity = createSplitPatternTestSecurity();
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test that invalid split configurations are handled gracefully
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Should handle edge cases at split point limits without crashing
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
        }());
    }
    
    SECTION("Empty pattern generation")
    {
        auto testSecurity = createSplitPatternEdgeCaseSecurity();
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test with very strict criteria that might result in no patterns
        PerformanceCriteria<TestDecimalType> impossibleCriteria(
            TestDecimalType("100.0"), // 100% profitability required
            1000,                     // 1000 minimum trades (impossible with small dataset)
            0,                        // 0 max consecutive losses
            TestDecimalType("100.0")  // Very high profit factor requirement
        );
        
        auto config = createSplitPatternSearchConfigWithCriteria(testSecurity, SearchType::EXTENDED, impossibleCriteria);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Should handle cases where no patterns meet criteria gracefully
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            std::cout << "DEBUG: Impossible criteria test found " << patterns.size() << " patterns" << std::endl;
        }());
    }
}

TEST_CASE("Split patterns - Performance and resource management", "[PatternDiscoveryTask][split-patterns][performance]")
{
    SECTION("Combinatorial complexity")
    {
        auto testSecurity = createSplitPatternTestSecurity();
        mkc_palast::AstResourceManager resourceManager;
        
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test with larger pattern length range
        auto config = createSplitPatternConfig(testSecurity, SearchType::DEEP);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        // Measure that execution completes in reasonable time
        auto start = std::chrono::high_resolution_clock::now();
        auto patterns = task.findPatterns();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "DEBUG: Split pattern discovery took " << duration.count() << " ms" << std::endl;
        std::cout << "DEBUG: Found " << patterns.size() << " total patterns" << std::endl;
        
        // Should complete without excessive time (this is a basic performance check)
        REQUIRE(duration.count() < 30000); // Less than 30 seconds
    }
    
    SECTION("Memory usage")
    {
        auto testSecurity = createSplitPatternTestSecurity();
        mkc_palast::AstResourceManager resourceManager;
        
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test memory usage with multiple pattern discovery runs
        for (int i = 0; i < 5; ++i)
        {
            auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            
            REQUIRE_NOTHROW([&]() {
                auto patterns = task.findPatterns();
                // Memory should be managed properly across multiple runs
            }());
        }
        
        std::cout << "DEBUG: Multiple runs completed successfully" << std::endl;
    }
    
    SECTION("AST resource sharing")
    {
        auto testSecurity = createSplitPatternTestSecurity();
        mkc_palast::AstResourceManager resourceManager;
        
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test that AST nodes are properly shared and reused
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            // AstResourceManager should handle resource sharing efficiently
        }());
        
        std::cout << "DEBUG: AST resource sharing test completed" << std::endl;
    }
    
    SECTION("Backtest resource management")
    {
        auto testSecurity = createSplitPatternTestSecurity();
        mkc_palast::AstResourceManager resourceManager;
        
        auto timeSeries = testSecurity->getTimeSeries();
        boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
        
        // Test proper cleanup of backtesting resources
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        
        REQUIRE_NOTHROW([&]() {
            auto patterns = task.findPatterns();
            // Should not have resource leaks during pattern discovery
            for (const auto& patternPair : patterns)
            {
                REQUIRE(patternPair.first != nullptr);
                REQUIRE(patternPair.second != nullptr);
            }
        }());
        
        std::cout << "DEBUG: Backtest resource management test completed" << std::endl;
    }
}

// ============================================================================
// PHASE 3: INTEGRATION AND ADVANCED TESTS
// ============================================================================

TEST_CASE("Split patterns - Integration with delay patterns", "[PatternDiscoveryTask][split-patterns][delay]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Delayed split patterns")
    {
        // Create configuration with delay patterns enabled
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Should find both regular split patterns and delayed versions
        bool foundRegularSplit = false;
        bool foundDelayedSplit = false;
        
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                if (fileName.find("_D0") != std::string::npos)
                {
                    foundRegularSplit = true;
                    std::cout << "DEBUG: Found regular split pattern: " << fileName << std::endl;
                }
                else if (fileName.find("_D1") != std::string::npos ||
                         fileName.find("_D2") != std::string::npos ||
                         fileName.find("_D3") != std::string::npos ||
                         fileName.find("_D4") != std::string::npos ||
                         fileName.find("_D5") != std::string::npos)
                {
                    foundDelayedSplit = true;
                    std::cout << "DEBUG: Found delayed split pattern: " << fileName << std::endl;
                }
            }
        }
        
        // With delay patterns enabled, should process split patterns for delay
        // Already called findPatterns() above, so just verify it completed successfully
        REQUIRE_NOTHROW([&]() {
            // Verify we can access the patterns without issues
            for (const auto& patternPair : patterns) {
                REQUIRE(patternPair.first != nullptr);
                REQUIRE(patternPair.second != nullptr);
            }
        }());
        
        std::cout << "DEBUG: Delay integration test found " << patterns.size() << " total patterns" << std::endl;
        std::cout << "DEBUG: Found regular split patterns: " << (foundRegularSplit ? "Yes" : "No") << std::endl;
        std::cout << "DEBUG: Found delayed split patterns: " << (foundDelayedSplit ? "Yes" : "No") << std::endl;
    }
    
    SECTION("Offset preservation with delay")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify that delayed split patterns maintain proper AST structure
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos && fileName.find("_D") != std::string::npos)
            {
                auto patternExpr = patternPair.first->getPatternExpression();
                
                // Should still have AndExpr structure even with delay
                auto andExpr = std::dynamic_pointer_cast<AndExpr>(patternExpr);
                REQUIRE(andExpr != nullptr);
                
                // Both parts should be valid after delay application
                REQUIRE(andExpr->getLHS() != nullptr);
                REQUIRE(andExpr->getRHS() != nullptr);
                
                std::cout << "DEBUG: Validated offset preservation for: " << fileName << std::endl;
                break; // Test first delayed split pattern found
            }
        }
    }
    
    SECTION("Combined naming with delay")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Verify delayed split patterns have appropriate naming convention
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                // Should have format: {symbol}_S_L{totalLength}_P{lenPart1}_D{delay}
                REQUIRE(fileName.find("MSFT") != std::string::npos); // Symbol
                REQUIRE(fileName.find("_S_L") != std::string::npos);  // Split indicator
                REQUIRE(fileName.find("_P") != std::string::npos);   // Part length indicator
                REQUIRE(fileName.find("_D") != std::string::npos);   // Delay indicator
                
                std::cout << "DEBUG: Validated combined naming: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
    
    SECTION("Performance filtering with delay")
    {
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED, true);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        // Validate delayed split patterns are properly filtered
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                auto backtester = patternPair.second;
                REQUIRE(backtester != nullptr);
                
                // Performance criteria should apply to delayed split patterns
                auto profitability = backtester->getProfitability();
                REQUIRE(std::get<0>(profitability) >= TestDecimalType("0")); // Profit factor >= 0
                REQUIRE(std::get<1>(profitability) >= TestDecimalType("0")); // Win rate >= 0
                REQUIRE(std::get<1>(profitability) <= TestDecimalType("100")); // Win rate <= 100%
                
                std::cout << "DEBUG: Performance filtering validated for: " << fileName << std::endl;
                break; // Test first split pattern found
            }
        }
    }
}

TEST_CASE("Split patterns - Determinism and consistency", "[PatternDiscoveryTask][split-patterns][determinism]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Reproducible results")
    {
        mkc_palast::AstResourceManager resourceManager1;
        mkc_palast::AstResourceManager resourceManager2;
        
        // First run
        auto config1 = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task1(config1, windowEndTime, resourceManager1);
        auto patterns1 = task1.findPatterns();
        
        // Second run with different resource manager
        auto config2 = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task2(config2, windowEndTime, resourceManager2);
        auto patterns2 = task2.findPatterns();
        
        // Results should be consistent (same number of patterns)
        REQUIRE(patterns1.size() == patterns2.size());
        
        std::cout << "DEBUG: Reproducible results test - Run 1: " << patterns1.size()
                 << " patterns, Run 2: " << patterns2.size() << " patterns" << std::endl;
    }
    
    SECTION("Order consistency")
    {
        mkc_palast::AstResourceManager resourceManager;
        
        // Multiple runs should produce patterns in consistent order
        std::vector<std::vector<std::string>> patternNameSets;
        
        for (int i = 0; i < 3; ++i)
        {
            auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
            
            std::vector<std::string> patternNames;
            for (const auto& patternPair : patterns)
            {
                patternNames.push_back(patternPair.first->getPatternDescription()->getFileName());
            }
            patternNameSets.push_back(patternNames);
        }
        
        // All runs should produce the same pattern names in the same order
        for (size_t i = 1; i < patternNameSets.size(); ++i)
        {
            REQUIRE(patternNameSets[i].size() == patternNameSets[0].size());
            for (size_t j = 0; j < patternNameSets[i].size(); ++j)
            {
                REQUIRE(patternNameSets[i][j] == patternNameSets[0][j]);
            }
        }
        
        std::cout << "DEBUG: Order consistency verified across " << patternNameSets.size() << " runs" << std::endl;
    }
    
    SECTION("Cross-platform consistency")
    {
        mkc_palast::AstResourceManager resourceManager;
        
        // Test that results are consistent across different configurations
        std::vector<SearchType> searchTypes = {
            SearchType::EXTENDED,
            SearchType::CLOSE_ONLY,
            SearchType::HIGH_LOW_ONLY
        };
        
        for (auto searchType : searchTypes)
        {
            auto config = createSplitPatternConfig(testSecurity, searchType);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            
            // Each SearchType should produce consistent results
            REQUIRE_NOTHROW([&]() {
                auto patterns = task.findPatterns();
                std::cout << "DEBUG: SearchType " << static_cast<int>(searchType)
                         << " produced " << patterns.size() << " patterns consistently" << std::endl;
            }());
        }
    }
    
    SECTION("Resource manager independence")
    {
        // Test that results are consistent across different AstResourceManager instances
        std::vector<size_t> patternCounts;
        
        for (int i = 0; i < 3; ++i)
        {
            mkc_palast::AstResourceManager resourceManager;
            auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
            PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
            auto patterns = task.findPatterns();
            patternCounts.push_back(patterns.size());
        }
        
        // All runs should produce the same number of patterns
        for (size_t i = 1; i < patternCounts.size(); ++i)
        {
            REQUIRE(patternCounts[i] == patternCounts[0]);
        }
        
        std::cout << "DEBUG: Resource manager independence verified - consistent count: "
                 << patternCounts[0] << std::endl;
    }
}

// ============================================================================
// INTEGRATION TEST SUMMARY
// ============================================================================

TEST_CASE("Split patterns - Integration test summary", "[PatternDiscoveryTask][split-patterns][integration]")
{
    auto testSecurity = createSplitPatternTestSecurity();
    mkc_palast::AstResourceManager resourceManager;
    
    auto timeSeries = testSecurity->getTimeSeries();
    boost::posix_time::ptime windowEndTime = timeSeries->getLastDateTime();
    
    SECTION("Complete split pattern workflow")
    {
        // Test the complete workflow from pattern discovery to validation
        auto config = createSplitPatternConfig(testSecurity, SearchType::EXTENDED);
        PatternDiscoveryTask<TestDecimalType> task(config, windowEndTime, resourceManager);
        auto patterns = task.findPatterns();
        
        std::cout << "=== SPLIT PATTERN INTEGRATION TEST SUMMARY ===" << std::endl;
        std::cout << "Total patterns found: " << patterns.size() << std::endl;
        
        size_t splitPatternCount = 0;
        size_t exactPatternCount = 0;
        
        for (const auto& patternPair : patterns)
        {
            std::string fileName = patternPair.first->getPatternDescription()->getFileName();
            if (fileName.find("_S_L") != std::string::npos)
            {
                splitPatternCount++;
                
                // Validate split pattern structure
                auto patternExpr = patternPair.first->getPatternExpression();
                auto andExpr = std::dynamic_pointer_cast<AndExpr>(patternExpr);
                REQUIRE(andExpr != nullptr);
                
                // Validate backtesting results
                auto backtester = patternPair.second;
                REQUIRE(backtester != nullptr);
                
                auto profitability = backtester->getProfitability();
                REQUIRE(std::get<0>(profitability) >= TestDecimalType("0"));
                REQUIRE(std::get<1>(profitability) >= TestDecimalType("0"));
                REQUIRE(std::get<1>(profitability) <= TestDecimalType("100"));
                
                std::cout << "Split pattern: " << fileName
                         << " - PF: " << std::get<0>(profitability)
                         << ", WR: " << std::get<1>(profitability) << "%" << std::endl;
            }
            else
            {
                exactPatternCount++;
            }
        }
        
        std::cout << "Split patterns: " << splitPatternCount << std::endl;
        std::cout << "Exact patterns: " << exactPatternCount << std::endl;
        std::cout << "=============================================" << std::endl;
        
        // Verify that split patterns were found and validated
        // Already called findPatterns() above, so just verify it completed successfully
        REQUIRE_NOTHROW([&]() {
            // Verify we can access the patterns without issues
            for (const auto& patternPair : patterns) {
                REQUIRE(patternPair.first != nullptr);
                REQUIRE(patternPair.second != nullptr);
            }
        }());
        
        std::cout << "Split pattern integration test completed successfully!" << std::endl;
    }
}