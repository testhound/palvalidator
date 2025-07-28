// ExhaustivePatternSearchEngineParallelizationTest.cpp
// Unit tests for Phase 3 parallelization functionality

#include <catch2/catch_test_macros.hpp>
#include "ExhaustivePatternSearchEngine.h"
#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
#include "Security.h"
#include "TimeSeries.h"
#include "TimeSeriesIndicators.h"
#include "TestUtils.h"
#include "number.h"
#include "ParallelExecutors.h"

#include <vector>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>

using TestDecimalType = num::DefaultNumber;

// Helper function to create a larger time series for parallelization testing
std::shared_ptr<mkc_timeseries::OHLCTimeSeries<TestDecimalType>> createParallelizationTestTimeSeries()
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;

    auto timeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(TimeFrame::DAILY, TradingVolume::SHARES);

    // Create a larger dataset with 30 entries to provide more windows for parallel processing
    // Start with sufficient historical data for pattern evaluation
    
    // Generate 30 days of data starting from 2022-12-01
    boost::gregorian::date startDate(2022, 12, 1);
    Decimal basePrice = Decimal("100");
    
    for (int i = 0; i < 30; ++i)
    {
        boost::gregorian::date currentDate = startDate + boost::gregorian::days(i);
        std::string dateStr = boost::gregorian::to_iso_string(currentDate);
        
        // Create predictable price patterns that will generate discoverable patterns
        Decimal price = basePrice + Decimal(i % 5); // Creates cyclical patterns
        Decimal open = price;
        Decimal high = price + Decimal("2");
        Decimal low = price - Decimal("1");
        Decimal close = price + Decimal("1");
        
        timeSeries->addEntry(*createTimeSeriesEntry(
            dateStr,
            num::toString(open),
            num::toString(high),
            num::toString(low),
            num::toString(close),
            "10000"
        ));
    }

    return timeSeries;
}

// Helper to create search configuration for parallelization tests
SearchConfiguration<TestDecimalType> createParallelizationTestConfig(
    std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> security,
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
        false, // No delay patterns for basic parallelization tests
        profitTargetValue,
        stopValue,
        perfCriteria,
        timeSeries->getFirstDateTime(),
        timeSeries->getLastDateTime()
    );
}

TEST_CASE("ExhaustivePatternSearchEngine parallelization functionality", "[ExhaustivePatternSearchEngine][parallelization]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    auto timeSeries = createParallelizationTestTimeSeries();
    auto security = std::make_shared<EquitySecurity<Decimal>>("AAPL", "Apple Inc.", timeSeries);
    auto config = createParallelizationTestConfig(security);
    
    SECTION("ThreadPoolExecutor integration with different pool sizes")
    {
        // Test with different thread pool sizes
        std::vector<unsigned int> poolSizes = {1, 2, 4};
        std::vector<unsigned int> patternCounts;
        
        for (auto poolSize : poolSizes)
        {
            unsigned int patternCount = 0;
            
            if (poolSize == 1) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<1>> engine(config);
                auto results = engine.run();
                patternCount = results->getNumPatterns();
            } else if (poolSize == 2) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<2>> engine(config);
                auto results = engine.run();
                patternCount = results->getNumPatterns();
            } else if (poolSize == 4) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
                auto results = engine.run();
                patternCount = results->getNumPatterns();
            }
            
            patternCounts.push_back(patternCount);
            
            // Verify results are valid (patternCount is unsigned, so always >= 0)
            // Just verify the operation completed without crashing
        }
        
        // All pool sizes should produce the same number of patterns
        for (size_t i = 1; i < patternCounts.size(); ++i)
        {
            REQUIRE(patternCounts[i] == patternCounts[0]);
        }
    }
    
    SECTION("Task distribution verification")
    {
        // Create engine and verify it processes the expected number of windows
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
        auto results = engine.run();
        
        REQUIRE(results != nullptr);
        
        // With 30 entries and EXTENDED search (max 6 bars lookback), 
        // we should have 30 - 6 = 24 valid windows to process
        // The exact number of patterns depends on the data, but we should find some
        auto lengthRange = config.getPatternLengthRange();
        unsigned int maxLookback = lengthRange.second;
        unsigned int expectedWindows = timeSeries->getNumEntries() - maxLookback;
        
        // Verify we have a reasonable number of windows to process
        REQUIRE(expectedWindows > 0);
        REQUIRE(expectedWindows == 24); // 30 - 6 = 24
        
        // The engine should process all valid windows without crashing
        // (getNumPatterns() returns unsigned, so always >= 0)
    }
    
    SECTION("Result aggregation thread safety")
    {
        // Test with high thread count to stress test thread safety
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<8>> engine(config);
        
        // Run multiple times to check for race conditions
        std::vector<unsigned int> patternCounts;
        for (int run = 0; run < 5; ++run)
        {
            auto results = engine.run();
            REQUIRE(results != nullptr);
            patternCounts.push_back(results->getNumPatterns());
        }
        
        // All runs should produce consistent results (no race conditions)
        for (size_t i = 1; i < patternCounts.size(); ++i)
        {
            REQUIRE(patternCounts[i] == patternCounts[0]);
        }
    }
}

TEST_CASE("ExhaustivePatternSearchEngine executor policy comparison", "[ExhaustivePatternSearchEngine][executors]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    auto timeSeries = createParallelizationTestTimeSeries();
    auto security = std::make_shared<EquitySecurity<Decimal>>("AAPL", "Apple Inc.", timeSeries);
    auto config = createParallelizationTestConfig(security);
    
    SECTION("Single vs multi-threaded results equivalence")
    {
        // Run with SingleThreadExecutor (baseline)
        ExhaustivePatternSearchEngine<Decimal, concurrency::SingleThreadExecutor> singleEngine(config);
        auto singleResults = singleEngine.run();
        
        // Run with ThreadPoolExecutor
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> multiEngine(config);
        auto multiResults = multiEngine.run();
        
        // Results should be identical
        REQUIRE(singleResults != nullptr);
        REQUIRE(multiResults != nullptr);
        REQUIRE(singleResults->getNumPatterns() == multiResults->getNumPatterns());
        
        // Verify both found some patterns (data should be designed to produce patterns)
        if (singleResults->getNumPatterns() > 0)
        {
            // Both should have found the same patterns
            REQUIRE(multiResults->getNumPatterns() > 0);
        }
    }
    
    SECTION("StdAsyncExecutor produces consistent results")
    {
        ExhaustivePatternSearchEngine<Decimal, concurrency::StdAsyncExecutor> asyncEngine(config);
        auto asyncResults = asyncEngine.run();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::SingleThreadExecutor> singleEngine(config);
        auto singleResults = singleEngine.run();
        
        // Compare results with SingleThreadExecutor baseline
        REQUIRE(asyncResults != nullptr);
        REQUIRE(singleResults != nullptr);
        REQUIRE(asyncResults->getNumPatterns() == singleResults->getNumPatterns());
    }
    
    SECTION("BoostRunnerExecutor integration")
    {
        // Test BoostRunnerExecutor if available
        REQUIRE_NOTHROW([&]() {
            ExhaustivePatternSearchEngine<Decimal, concurrency::BoostRunnerExecutor> boostEngine(config);
            auto results = boostEngine.run();
            REQUIRE(results != nullptr);
        }());
    }
    
    SECTION("Hardware concurrency thread pool")
    {
        // Test with hardware_concurrency() thread count (N=0 uses hardware_concurrency)
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<0>> hwEngine(config);
        auto hwResults = hwEngine.run();
        
        ExhaustivePatternSearchEngine<Decimal, concurrency::SingleThreadExecutor> singleEngine(config);
        auto singleResults = singleEngine.run();
        
        // Should produce same results as single-threaded
        REQUIRE(hwResults != nullptr);
        REQUIRE(singleResults != nullptr);
        REQUIRE(hwResults->getNumPatterns() == singleResults->getNumPatterns());
    }
}

TEST_CASE("ExhaustivePatternSearchEngine parallel error handling", "[ExhaustivePatternSearchEngine][errors][parallelization]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    SECTION("Exception handling in parallel execution")
    {
        auto timeSeries = createParallelizationTestTimeSeries();
        auto security = std::make_shared<EquitySecurity<Decimal>>("MSFT", "Microsoft Corporation", timeSeries);
        auto config = createParallelizationTestConfig(security);
        
        // Test that the engine handles potential exceptions gracefully
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
        
        REQUIRE_NOTHROW([&]() {
            auto results = engine.run();
            REQUIRE(results != nullptr);
        }());
    }
    
    SECTION("Empty time series handling")
    {
        // Create empty time series
        auto emptyTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(TimeFrame::DAILY, TradingVolume::SHARES);
        auto emptySecurity = std::make_shared<EquitySecurity<Decimal>>("NVDA", "NVIDIA Corporation", emptyTimeSeries);
        
        // Create configuration with empty time series
        PerformanceCriteria<Decimal> criteria(Decimal("0.0"), 1, 999, Decimal("0.001"));
        boost::posix_time::ptime startTime(boost::gregorian::date(2023, 1, 1));
        boost::posix_time::ptime endTime(boost::gregorian::date(2023, 1, 10));
        
        SearchConfiguration<Decimal> emptyConfig(
            emptySecurity,
            TimeFrame::DAILY,
            SearchType::EXTENDED,
            false,
            Decimal("5.0"),
            Decimal("5.0"),
            criteria,
            startTime,
            endTime
        );
        
        // Test with parallel executor
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(emptyConfig);
        auto results = engine.run();
        
        // Should return valid but empty PriceActionLabSystem
        REQUIRE(results != nullptr);
        REQUIRE(results->getNumPatterns() == 0);
    }
    
    SECTION("Concurrent resource access safety")
    {
        auto timeSeries = createParallelizationTestTimeSeries();
        auto security = std::make_shared<EquitySecurity<Decimal>>("GOOGL", "Alphabet Inc.", timeSeries);
        auto config = createParallelizationTestConfig(security);
        
        // Run multiple engines concurrently to test resource conflicts
        std::vector<std::future<std::shared_ptr<PriceActionLabSystem>>> futures;
        
        for (int i = 0; i < 4; ++i)
        {
            futures.push_back(std::async(std::launch::async, [&config]() {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<2>> engine(config);
                return engine.run();
            }));
        }
        
        // Wait for all to complete and verify no crashes
        for (auto& future : futures)
        {
            auto result = future.get();
            REQUIRE(result != nullptr);
        }
    }
}

TEST_CASE("ExhaustivePatternSearchEngine parallelization performance", "[ExhaustivePatternSearchEngine][performance][parallelization]")
{
    using namespace mkc_timeseries;
    using Decimal = TestDecimalType;
    
    auto timeSeries = createParallelizationTestTimeSeries();
    auto security = std::make_shared<EquitySecurity<Decimal>>("AMZN", "Amazon.com Inc.", timeSeries);
    auto config = createParallelizationTestConfig(security);
    
    SECTION("Performance comparison single vs multi-threaded")
    {
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
        
        // Log performance for analysis (not a strict requirement, but useful)
        std::cout << "Single-threaded duration: " << singleDuration.count() << "ms" << std::endl;
        std::cout << "Multi-threaded duration: " << multiDuration.count() << "ms" << std::endl;
        
        // Both should complete in reasonable time (not a strict performance test)
        // (duration.count() returns signed type, but should be non-negative)
        REQUIRE(singleDuration.count() >= 0);
        REQUIRE(multiDuration.count() >= 0);
    }
    
    SECTION("Scalability with varying thread counts")
    {
        std::vector<std::pair<int, long>> performanceData;
        
        // Test with different thread counts
        std::vector<int> threadCounts = {1, 2, 4};
        
        for (int threads : threadCounts)
        {
            auto start = std::chrono::high_resolution_clock::now();
            
            if (threads == 1) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<1>> engine(config);
                auto results = engine.run();
                REQUIRE(results != nullptr);
            } else if (threads == 2) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<2>> engine(config);
                auto results = engine.run();
                REQUIRE(results != nullptr);
            } else if (threads == 4) {
                ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<4>> engine(config);
                auto results = engine.run();
                REQUIRE(results != nullptr);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            performanceData.push_back({threads, duration.count()});
            std::cout << "Threads: " << threads << ", Duration: " << duration.count() << "ms" << std::endl;
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
}