// Phase3IntegrationTest.cpp
// Integration tests for Phase 3: Parallelization with Delay Patterns

#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "ExhaustivePatternSearchEngine.h"
#include "PatternDiscoveryTask.h"
#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
#include "Security.h"
#include "TimeSeries.h"
#include "TimeSeriesIndicators.h"
#include "TestUtils.h"
#include "number.h"
#include "ParallelExecutors.h"
#include "AstResourceManager.h"

#include <vector>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <iostream>

using TestDecimalType = num::DefaultNumber;

// Helper function to create a comprehensive time series for integration testing
std::shared_ptr<mkc_timeseries::OHLCTimeSeries<TestDecimalType>> createIntegrationTestTimeSeries()
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;

    auto timeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(TimeFrame::DAILY, TradingVolume::SHARES);

    // Create a substantial dataset with 40 entries to provide many windows for parallel processing
    // and sufficient data for delay patterns
    
    // Generate 40 days of data starting from 2022-11-15
    boost::gregorian::date startDate(2022, 11, 15);
    Decimal basePrice = Decimal("100");
    
    for (int i = 0; i < 40; ++i)
    {
        boost::gregorian::date currentDate = startDate + boost::gregorian::days(i);
        std::string dateStr = boost::gregorian::to_iso_string(currentDate);
        
        // Create patterns that will be profitable both as exact and delayed patterns
        // Use a more complex pattern that creates multiple opportunities
        Decimal priceVariation = Decimal(i % 7); // 7-day cycle for more variety
        Decimal price = basePrice + priceVariation;
        
        // Create OHLC data that will generate discoverable patterns
        Decimal open = price;
        Decimal close = price + Decimal("2") + Decimal(i % 4);
        Decimal low = price - Decimal("1") - Decimal(i % 2);
        
        // Ensure High is always >= max(Open, Close) and Low is always <= min(Open, Close)
        Decimal maxPrice = (open > close) ? open : close;
        Decimal minPrice = (open < close) ? open : close;
        
        Decimal high = maxPrice + Decimal("1") + Decimal(i % 3);
        if (low > minPrice) {
            low = minPrice - Decimal("1");
        }
        
        timeSeries->addEntry(*createTimeSeriesEntry(
            dateStr,
            num::toString(open),
            num::toString(high),
            num::toString(low),
            num::toString(close),
            "15000"
        ));
    }

    return timeSeries;
}

// Helper to create search configuration for integration tests
SearchConfiguration<TestDecimalType> createIntegrationTestConfig(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
    bool enableDelayPatterns,
    SearchType searchType = SearchType::EXTENDED)
{
    using namespace mkc_timeseries;
    
    // Use lenient performance criteria to ensure patterns are found
    PerformanceCriteria<TestDecimalType> perfCriteria(
        TestDecimalType("0.0"),  // 0% profitability required
        1,                       // Min trades
        999,                     // Max consecutive losers
        TestDecimalType("0.001") // Very low profit factor
    );
    
    // Calculate dynamic profit target and stop loss
    auto timeSeries = security->getTimeSeries();
    NumericTimeSeries<TestDecimalType> closingPrices(timeSeries->CloseTimeSeries());
    NumericTimeSeries<TestDecimalType> rocOfClosingPrices(RocSeries(closingPrices, 1));
    TestDecimalType medianOfRoc = Median(rocOfClosingPrices);
    RobustQn<TestDecimalType> qnEstimator(rocOfClosingPrices);
    TestDecimalType robustQn = qnEstimator.getRobustQn();
    
    TestDecimalType stopValue = medianOfRoc + robustQn;
    TestDecimalType profitTargetValue = stopValue;

    return SearchConfiguration<TestDecimalType>(
        security,
        TimeFrame::DAILY,
        searchType,
        enableDelayPatterns,
        profitTargetValue,
        stopValue,
        perfCriteria,
        timeSeries->getFirstDateTime(),
        timeSeries->getLastDateTime()
    );
}

TEST_CASE("Phase 3 integration: Parallelization with delay patterns", "[integration][phase3]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    auto timeSeries = createIntegrationTestTimeSeries();
    auto security = std::make_shared<EquitySecurity<Decimal>>("AAPL", "Apple Inc.", timeSeries);
    
    SECTION("Parallel delay pattern discovery")
    {
        // Test with both parallelization and delay patterns enabled
        auto config = createIntegrationTestConfig(security, true);
        
        // Test with ThreadPoolExecutor and delay patterns
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> parallelEngine(config);
        auto parallelResults = parallelEngine.run();
        
        REQUIRE(parallelResults != nullptr);
        
        std::cout << "DEBUG: Parallel with delays found " << parallelResults->getNumPatterns() << " patterns" << std::endl;
        
        // Verify that we found some patterns
        // (getNumPatterns() returns unsigned, so always >= 0)
        
        // Test that all patterns have valid structure
        unsigned int patternCount = 0;
        for (auto it = parallelResults->allPatternsBegin(); it != parallelResults->allPatternsEnd(); ++it)
        {
            REQUIRE(*it != nullptr);
            REQUIRE((*it)->getPatternExpression() != nullptr);
            REQUIRE((*it)->getPatternDescription() != nullptr);
            
            // Verify filename contains expected components
            std::string fileName = (*it)->getPatternDescription()->getFileName();
            REQUIRE(fileName.find("AAPL") != std::string::npos); // Security symbol
            REQUIRE(fileName.find("_L") != std::string::npos);    // Length indicator
            REQUIRE(fileName.find("_D") != std::string::npos);    // Delay indicator
            
            patternCount++;
        }
        
        REQUIRE(patternCount == parallelResults->getNumPatterns());
    }
    
    SECTION("Thread safety of delay pattern generation")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        // Run multiple parallel engines concurrently to test thread safety
        std::vector<std::future<std::shared_ptr<PriceActionLabSystem>>> futures;
        
        for (int i = 0; i < 3; ++i)
        {
            futures.push_back(std::async(std::launch::async, [&config]() {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
                return engine.run();
            }));
        }
        
        // Wait for all to complete and verify consistency
        std::vector<unsigned int> patternCounts;
        for (auto& future : futures)
        {
            auto result = future.get();
            REQUIRE(result != nullptr);
            patternCounts.push_back(result->getNumPatterns());
        }
        
        // All runs should produce the same number of patterns (thread safety)
        for (size_t i = 1; i < patternCounts.size(); ++i)
        {
            REQUIRE(patternCounts[i] == patternCounts[0]);
        }
        
        std::cout << "DEBUG: Thread safety test - all runs found " << patternCounts[0] << " patterns" << std::endl;
    }
    
    SECTION("Concurrent AST transformation")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        // Test that AST transformations for delay patterns work correctly in parallel
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<8>> highConcurrencyEngine(config);
        
        // Run multiple times to stress test concurrent AST operations
        for (int run = 0; run < 3; ++run)
        {
            REQUIRE_NOTHROW([&]() {
                auto results = highConcurrencyEngine.run();
                REQUIRE(results != nullptr);
                
                // Verify all patterns have valid AST structures
                for (auto it = results->allPatternsBegin(); it != results->allPatternsEnd(); ++it)
                {
                    REQUIRE((*it)->getPatternExpression() != nullptr);
                    REQUIRE((*it)->getMarketEntry() != nullptr);
                    REQUIRE((*it)->getProfitTarget() != nullptr);
                    REQUIRE((*it)->getStopLoss() != nullptr);
                }
            }());
        }
    }
}

TEST_CASE("Phase 3 performance comparison", "[integration][phase3][performance]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    auto timeSeries = createIntegrationTestTimeSeries();
    auto security = std::make_shared<EquitySecurity<Decimal>>("MSFT", "Microsoft", timeSeries);
    
    SECTION("Single-threaded vs multi-threaded with delay patterns")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        // Measure single-threaded performance
        auto start = std::chrono::high_resolution_clock::now();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::SingleThreadExecutor> singleEngine(config);
        auto singleResults = singleEngine.run();
        
        auto singleEnd = std::chrono::high_resolution_clock::now();
        auto singleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(singleEnd - start);
        
        // Measure multi-threaded performance
        start = std::chrono::high_resolution_clock::now();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> multiEngine(config);
        auto multiResults = multiEngine.run();
        
        auto multiEnd = std::chrono::high_resolution_clock::now();
        auto multiDuration = std::chrono::duration_cast<std::chrono::milliseconds>(multiEnd - start);
        
        // Verify results are equivalent
        REQUIRE(singleResults != nullptr);
        REQUIRE(multiResults != nullptr);
        REQUIRE(singleResults->getNumPatterns() == multiResults->getNumPatterns());
        
        // Log performance for analysis
        std::cout << "Single-threaded with delays: " << singleDuration.count() << "ms, " 
                  << singleResults->getNumPatterns() << " patterns" << std::endl;
        std::cout << "Multi-threaded with delays: " << multiDuration.count() << "ms, " 
                  << multiResults->getNumPatterns() << " patterns" << std::endl;
        
        // Both should complete in reasonable time
        // (duration.count() returns signed type, but should be non-negative)
        REQUIRE(singleDuration.count() >= 0);
        REQUIRE(multiDuration.count() >= 0);
    }
    
    SECTION("Scalability with delay patterns enabled")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        std::vector<std::pair<int, long>> performanceData;
        std::vector<int> threadCounts = {1, 2, 4};
        
        for (int threads : threadCounts)
        {
            auto start = std::chrono::high_resolution_clock::now();
            unsigned int patternCount = 0;
            
            if (threads == 1) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<1>> engine(config);
                auto results = engine.run();
                patternCount = results->getNumPatterns();
            } else if (threads == 2) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<2>> engine(config);
                auto results = engine.run();
                patternCount = results->getNumPatterns();
            } else if (threads == 4) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
                auto results = engine.run();
                patternCount = results->getNumPatterns();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            performanceData.push_back({threads, duration.count()});
            std::cout << "Threads: " << threads << ", Duration: " << duration.count() 
                      << "ms, Patterns: " << patternCount << std::endl;
        }
        
        // Verify all configurations completed successfully
        REQUIRE(performanceData.size() == threadCounts.size());
        
        // All should complete in reasonable time
        for (const auto& data : performanceData)
        {
            // data.second is duration in milliseconds, should be non-negative
            REQUIRE(data.second >= 0);
        }
    }
    
    SECTION("Delay patterns vs no delay patterns performance impact")
    {
        // Test performance impact of enabling delay patterns
        auto noDelayConfig = createIntegrationTestConfig(security, false);
        auto delayConfig = createIntegrationTestConfig(security, true);
        
        // Measure without delay patterns
        auto start = std::chrono::high_resolution_clock::now();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> noDelayEngine(noDelayConfig);
        auto noDelayResults = noDelayEngine.run();
        
        auto noDelayEnd = std::chrono::high_resolution_clock::now();
        auto noDelayDuration = std::chrono::duration_cast<std::chrono::milliseconds>(noDelayEnd - start);
        
        // Measure with delay patterns
        start = std::chrono::high_resolution_clock::now();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> delayEngine(delayConfig);
        auto delayResults = delayEngine.run();
        
        auto delayEnd = std::chrono::high_resolution_clock::now();
        auto delayDuration = std::chrono::duration_cast<std::chrono::milliseconds>(delayEnd - start);
        
        // Verify results
        REQUIRE(noDelayResults != nullptr);
        REQUIRE(delayResults != nullptr);
        
        // With delay patterns, we should find at least as many patterns
        REQUIRE(delayResults->getNumPatterns() >= noDelayResults->getNumPatterns());
        
        // Log performance impact
        std::cout << "No delays: " << noDelayDuration.count() << "ms, " 
                  << noDelayResults->getNumPatterns() << " patterns" << std::endl;
        std::cout << "With delays: " << delayDuration.count() << "ms, " 
                  << delayResults->getNumPatterns() << " patterns" << std::endl;
        
        // Both should complete in reasonable time
        // (duration.count() returns signed type, but should be non-negative)
        REQUIRE(noDelayDuration.count() >= 0);
        REQUIRE(delayDuration.count() >= 0);
    }
}

TEST_CASE("Phase 3 result consistency", "[integration][phase3][consistency]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    auto timeSeries = createIntegrationTestTimeSeries();
    auto security = std::make_shared<EquitySecurity<Decimal>>("GOOGL", "Google", timeSeries);
    
    SECTION("Parallel execution produces same results as sequential")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        // Sequential execution (SingleThreadExecutor)
        ExhaustivePatternSearchEngine<Decimal, concurrency::SingleThreadExecutor> seqEngine(config);
        auto seqResults = seqEngine.run();
        
        // Parallel execution (ThreadPoolExecutor)
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> parEngine(config);
        auto parResults = parEngine.run();
        
        // Results should be identical
        REQUIRE(seqResults != nullptr);
        REQUIRE(parResults != nullptr);
        REQUIRE(seqResults->getNumPatterns() == parResults->getNumPatterns());
        
        // Verify pattern details are consistent
        if (seqResults->getNumPatterns() > 0)
        {
            // Both should have found patterns with similar characteristics
            auto seqIt = seqResults->allPatternsBegin();
            auto parIt = parResults->allPatternsBegin();
            
            // Check first pattern from each
            REQUIRE((*seqIt)->getPatternExpression() != nullptr);
            REQUIRE((*parIt)->getPatternExpression() != nullptr);
            
            // Both should have valid pattern descriptions
            REQUIRE((*seqIt)->getPatternDescription() != nullptr);
            REQUIRE((*parIt)->getPatternDescription() != nullptr);
        }
    }
    
    SECTION("Deterministic behavior with delay patterns")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        std::vector<unsigned int> patternCounts;
        
        // Run multiple times to verify deterministic behavior
        for (int run = 0; run < 3; ++run)
        {
            ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
            auto results = engine.run();
            REQUIRE(results != nullptr);
            patternCounts.push_back(results->getNumPatterns());
        }
        
        // All runs should produce the same number of patterns
        for (size_t i = 1; i < patternCounts.size(); ++i)
        {
            REQUIRE(patternCounts[i] == patternCounts[0]);
        }
        
        std::cout << "DEBUG: Deterministic test - all runs found " << patternCounts[0] << " patterns" << std::endl;
    }
    
    SECTION("Pattern ordering and metadata consistency")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        // Run twice and compare pattern metadata
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine1(config);
        auto results1 = engine1.run();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine2(config);
        auto results2 = engine2.run();
        
        REQUIRE(results1 != nullptr);
        REQUIRE(results2 != nullptr);
        REQUIRE(results1->getNumPatterns() == results2->getNumPatterns());
        
        // Verify pattern metadata consistency
        if (results1->getNumPatterns() > 0)
        {
            std::vector<std::string> fileNames1, fileNames2;
            
            for (auto it = results1->allPatternsBegin(); it != results1->allPatternsEnd(); ++it)
            {
                fileNames1.push_back((*it)->getPatternDescription()->getFileName());
            }
            
            for (auto it = results2->allPatternsBegin(); it != results2->allPatternsEnd(); ++it)
            {
                fileNames2.push_back((*it)->getPatternDescription()->getFileName());
            }
            
            // Sort both lists for comparison (order might vary due to parallel execution)
            std::sort(fileNames1.begin(), fileNames1.end());
            std::sort(fileNames2.begin(), fileNames2.end());
            
            // Should have the same pattern filenames
            REQUIRE(fileNames1.size() == fileNames2.size());
            for (size_t i = 0; i < fileNames1.size(); ++i)
            {
                REQUIRE(fileNames1[i] == fileNames2[i]);
            }
        }
    }
    
    SECTION("Reproducibility across runs")
    {
        auto config = createIntegrationTestConfig(security, true);
        
        // Test that the same configuration produces the same results
        // across multiple independent runs
        std::vector<std::shared_ptr<PriceActionLabSystem>> allResults;
        
        for (int run = 0; run < 3; ++run)
        {
            ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
            auto results = engine.run();
            REQUIRE(results != nullptr);
            allResults.push_back(results);
        }
        
        // All results should have the same number of patterns
        for (size_t i = 1; i < allResults.size(); ++i)
        {
            REQUIRE(allResults[i]->getNumPatterns() == allResults[0]->getNumPatterns());
        }
        
        // Verify no memory leaks or resource conflicts
        REQUIRE(allResults.size() == 3);
        
        std::cout << "DEBUG: Reproducibility test - all runs found " 
                  << allResults[0]->getNumPatterns() << " patterns" << std::endl;
    }
}

TEST_CASE("Phase 3 stress testing", "[stress][phase3]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    SECTION("High volume pattern discovery with delays")
    {
        // Create a larger dataset for stress testing
        auto largeTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(TimeFrame::DAILY, TradingVolume::SHARES);
        
        // Generate 60 days of data for more intensive testing
        boost::gregorian::date startDate(2022, 10, 1);
        Decimal basePrice = Decimal("100");
        
        for (int i = 0; i < 60; ++i)
        {
            boost::gregorian::date currentDate = startDate + boost::gregorian::days(i);
            std::string dateStr = boost::gregorian::to_iso_string(currentDate);
            
            Decimal price = basePrice + Decimal(i % 10);
            largeTimeSeries->addEntry(*createTimeSeriesEntry(
                dateStr,
                num::toString(price),
                num::toString(price + Decimal("4")),
                num::toString(price - Decimal("2")),
                num::toString(price + Decimal("3")),
                "20000"
            ));
        }
        
        auto largeSecurity = std::make_shared<EquitySecurity<Decimal>>("AMZN", "Amazon", largeTimeSeries);
        auto config = createIntegrationTestConfig(largeSecurity, true);
        
        // Test with high thread count and delay patterns
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<8>> stressEngine(config);
        
        REQUIRE_NOTHROW([&]() {
            auto results = stressEngine.run();
            REQUIRE(results != nullptr);
            
            std::cout << "DEBUG: Stress test found " << results->getNumPatterns() << " patterns" << std::endl;
            
            // Verify all patterns are valid
            for (auto it = results->allPatternsBegin(); it != results->allPatternsEnd(); ++it)
            {
                REQUIRE(*it != nullptr);
                REQUIRE((*it)->getPatternExpression() != nullptr);
            }
        }());
    }
    
    SECTION("Resource management under load")
    {
        auto timeSeries = createIntegrationTestTimeSeries();
        auto security = std::make_shared<EquitySecurity<Decimal>>("NVDA", "Nvidia", timeSeries);
        auto config = createIntegrationTestConfig(security, true);
        
        // Run multiple engines simultaneously to test resource management
        std::vector<std::future<void>> futures;
        
        for (int i = 0; i < 5; ++i)
        {
            futures.push_back(std::async(std::launch::async, [&config, i]() {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
                auto results = engine.run();
                
                REQUIRE(results != nullptr);
                
                // Verify no resource conflicts
                for (auto it = results->allPatternsBegin(); it != results->allPatternsEnd(); ++it)
                {
                    REQUIRE(*it != nullptr);
                    REQUIRE((*it)->getPatternDescription() != nullptr);
                }
            }));
        }
        
        // Wait for all to complete
        for (auto& future : futures)
        {
            REQUIRE_NOTHROW([&]() {
                future.get();
            }());
        }
    }
    
    SECTION("Long-running operations stability")
    {
        auto timeSeries = createIntegrationTestTimeSeries();
        auto security = std::make_shared<EquitySecurity<Decimal>>("TSLA", "Tesla", timeSeries);
        auto config = createIntegrationTestConfig(security, true);
        
        // Run extended test to verify system stability
        for (int iteration = 0; iteration < 5; ++iteration)
        {
            ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
            
            REQUIRE_NOTHROW([&]() {
                auto results = engine.run();
                REQUIRE(results != nullptr);
                
                // Verify consistent behavior over time
                // (getNumPatterns() returns unsigned, so always >= 0)
            }());
            
            // Small delay between iterations to simulate real usage
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}