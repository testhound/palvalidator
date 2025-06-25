// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>
#include <future>
#include <random>
#include "PermutationTestObserver.h"
#include "PermutationTestSubject.h"
#include "UuidStrategyPermutationStatsAggregator.h"
#include "StrategyIdentificationHelper.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "TestUtils.h"
#include "Security.h"

using namespace mkc_timeseries;

namespace {
    // Helper function to create a BackTester with real PAL strategy and proper date range
    std::shared_ptr<BackTester<DecimalType>> createTestBackTester() {
        auto strategy = getRandomPalStrategy();
        auto timeSeries = getRandomPriceSeries();
        
        // Get the actual date range from the time series
        auto startDate = timeSeries->getFirstDate();
        auto endDate = timeSeries->getLastDate();
        
        auto bt = std::make_shared<DailyBackTester<DecimalType>>();
        bt->addDateRange(DateRange(startDate, endDate));
        bt->addStrategy(strategy);
        bt->backtest();
        return bt;
    }

    // Simple test observer for integration testing
    class TestObserver : public PermutationTestObserver<DecimalType> {
    private:
        std::shared_ptr<UuidStrategyPermutationStatsAggregator<DecimalType>> aggregator_;
        
    public:
        TestObserver(std::shared_ptr<UuidStrategyPermutationStatsAggregator<DecimalType>> aggregator)
            : aggregator_(aggregator) {}
            
        void update(const BackTester<DecimalType>& permutedBacktester,
                   const DecimalType& permutedTestStatistic) override {
            
            // Extract strategy identification using centralized hash computation
            unsigned long long strategyHash = StrategyIdentificationHelper<DecimalType>::extractStrategyHash(permutedBacktester);
            const PalStrategy<DecimalType>* strategy = StrategyIdentificationHelper<DecimalType>::extractPalStrategy(permutedBacktester);
            
            if (!strategy) {
                return; // Skip non-PAL strategies
            }
            
            // Extract statistics
            uint32_t numTrades = StrategyIdentificationHelper<DecimalType>::extractNumTrades(permutedBacktester);
            uint32_t numBarsInTrades = StrategyIdentificationHelper<DecimalType>::extractNumBarsInTrades(permutedBacktester);
            
            // Store metrics using centralized hash computation (now consistent with getters)
            aggregator_->addValue(strategyHash, strategy, MetricType::PERMUTED_TEST_STATISTIC, permutedTestStatistic);
            aggregator_->addValue(strategyHash, strategy, MetricType::NUM_TRADES, DecimalType(numTrades));
            aggregator_->addValue(strategyHash, strategy, MetricType::NUM_BARS_IN_TRADES, DecimalType(numBarsInTrades));
        }

        void updateMetric(const PalStrategy<DecimalType>* strategy,
                         MetricType metricType,
                         const DecimalType& metricValue) override {
            if (!strategy) {
                return; // Skip null strategies
            }
            
            // Extract strategy identification using centralized hash computation
            unsigned long long strategyHash = StrategyIdentificationHelper<DecimalType>::extractCombinedHash(strategy);
            
            // Store the metric using centralized hash computation
            aggregator_->addValue(strategyHash, strategy, metricType, metricValue);
        }
        
        std::optional<DecimalType> getMinMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return aggregator_->getMin(strategy, metric);
        }
        
        std::optional<DecimalType> getMaxMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return aggregator_->getMax(strategy, metric);
        }
        
        std::optional<double> getMedianMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return aggregator_->getMedian(strategy, metric);
        }
        
        std::optional<double> getStdDevMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return aggregator_->getStdDev(strategy, metric);
        }
        
        void clear() override {
            aggregator_->clear();
        }
    };

    class MockPermutationTestSubject : public PermutationTestSubject<DecimalType> {
    private:
        std::vector<std::shared_ptr<BackTester<DecimalType>>> backtesters_;
        
    public:
        void addBackTester(std::shared_ptr<BackTester<DecimalType>> backTester) {
            backtesters_.push_back(backTester);
        }
        
        void simulatePermutationRun() {
            // Simulate a permutation test run by notifying observers with each BackTester
            for (auto& backTester : backtesters_) {
                // Use a dummy test statistic for simulation
                DecimalType dummyTestStat = createDecimal("0.5");
                notifyObservers(*backTester, dummyTestStat);
            }
        }
        
        void simulateMultiplePermutations(int numPermutations) {
            for (int i = 0; i < numPermutations; ++i) {
                simulatePermutationRun();
            }
        }
    };
}

/**
 * @brief Integration tests for Phase 2: Complete Observer pattern system validation
 * 
 * These tests validate the entire Observer pattern implementation working together
 * in realistic scenarios, including multi-threaded permutation testing simulation.
 */

TEST_CASE("Basic Observer pattern integration", "[observer][integration]") {
    SECTION("Single observer, single strategy") {
        // Create the aggregator and observer
        auto aggregator = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        TestObserver observer(aggregator);
        
        // Create mock subject
        MockPermutationTestSubject subject;
        subject.attach(&observer);
        
        // Create a strategy using helper function
        auto bt = createTestBackTester();
        subject.addBackTester(bt);
        
        // Simulate permutation runs
        const int NUM_PERMUTATIONS = 10;
        subject.simulateMultiplePermutations(NUM_PERMUTATIONS);
        
        // Verify aggregation results
        REQUIRE(aggregator->getStrategyCount() == 1);
        
        subject.detach(&observer);
    }
    
    SECTION("Storage and retrieval consistency validation") {
        // This test specifically validates that stored statistics can be retrieved
        // It would have caught the hash mismatch bug
        
        auto aggregator = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        TestObserver observer(aggregator);
        
        MockPermutationTestSubject subject;
        subject.attach(&observer);
        
        // Create a strategy
        auto bt = createTestBackTester();
        subject.addBackTester(bt);
        
        // Get the strategy pointer for later retrieval
        auto strategy = StrategyIdentificationHelper<DecimalType>::extractPalStrategy(*bt);
        REQUIRE(strategy != nullptr);
        
        // Simulate permutation runs with known test statistics
        const DecimalType testStat1 = createDecimal("1.5");
        const DecimalType testStat2 = createDecimal("2.0");
        const DecimalType testStat3 = createDecimal("1.0");
        
        // Manually trigger updates with known values
        observer.update(*bt, testStat1);
        observer.update(*bt, testStat2);
        observer.update(*bt, testStat3);
        
        // CRITICAL TEST: Verify that stored statistics can be retrieved
        auto minStat = observer.getMinMetric(strategy, PermutationTestObserver<DecimalType>::MetricType::PERMUTED_TEST_STATISTIC);
        auto maxStat = observer.getMaxMetric(strategy, PermutationTestObserver<DecimalType>::MetricType::PERMUTED_TEST_STATISTIC);
        
        // These should NOT be nullopt if hash computation is consistent
        REQUIRE(minStat.has_value());
        REQUIRE(maxStat.has_value());
        
        // Verify the actual values
        REQUIRE(minStat.value() == testStat3);  // 1.0 is minimum
        REQUIRE(maxStat.value() == testStat2);  // 2.0 is maximum
        
        // Verify permutation count
        auto permCount = aggregator->getPermutationCount(strategy, PermutationTestObserver<DecimalType>::MetricType::PERMUTED_TEST_STATISTIC);
        REQUIRE(permCount == 3);
        
        subject.detach(&observer);
    }
    
    SECTION("Strategy clone hash consistency validation") {
        // This test verifies that cloned strategies have the same combined hash
        // even though they have different UUIDs - critical for permutation testing
        
        auto aggregator = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        TestObserver observer(aggregator);
        
        MockPermutationTestSubject subject;
        subject.attach(&observer);
        
        // Create original strategy
        auto originalBt = createTestBackTester();
        auto originalStrategy = StrategyIdentificationHelper<DecimalType>::extractPalStrategy(*originalBt);
        REQUIRE(originalStrategy != nullptr);
        
        // Get original combined hash
        unsigned long long originalHash = StrategyIdentificationHelper<DecimalType>::extractCombinedHash(originalStrategy);
        
        // Clone the strategy (this happens during permutation testing)
        auto clonedStrategy = originalStrategy->clone(originalStrategy->getPortfolio());
        auto clonedPalStrategy = dynamic_cast<const PalStrategy<DecimalType>*>(clonedStrategy.get());
        REQUIRE(clonedPalStrategy != nullptr);
        
        // Get cloned combined hash
        unsigned long long clonedHash = StrategyIdentificationHelper<DecimalType>::extractCombinedHash(clonedPalStrategy);
        
        // CRITICAL TEST: Combined hashes should be identical even though UUIDs differ
        REQUIRE(originalHash == clonedHash);
        
        // Verify UUIDs are different (confirming they are actually different instances)
        REQUIRE(originalStrategy->getInstanceId() != clonedPalStrategy->getInstanceId());
        
        // Verify pattern hashes are the same
        REQUIRE(originalStrategy->getPatternHash() == clonedPalStrategy->getPatternHash());
        
        // Verify strategy names are the same
        REQUIRE(originalStrategy->getStrategyName() == clonedPalStrategy->getStrategyName());
        
        // Test that statistics stored with original can be retrieved with clone
        const DecimalType testStat = createDecimal("2.5");
        
        // Store with original strategy hash
        observer.update(*originalBt, testStat);
        
        // Retrieve with cloned strategy - should work because combined hash is the same
        auto retrievedStat = observer.getMinMetric(clonedPalStrategy, PermutationTestObserver<DecimalType>::MetricType::PERMUTED_TEST_STATISTIC);
        
        REQUIRE(retrievedStat.has_value());
        REQUIRE(retrievedStat.value() == testStat);
        
        subject.detach(&observer);
    }
}

TEST_CASE("Multi-strategy Observer pattern integration", "[observer][integration]") {
    SECTION("Multiple strategies") {
        // Create the aggregator and observer
        auto aggregator = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        TestObserver observer(aggregator);
        
        // Create mock subject
        MockPermutationTestSubject subject;
        subject.attach(&observer);
        
        // Create multiple strategies
        std::vector<std::shared_ptr<BackTester<DecimalType>>> backtesters;
        
        for (int i = 0; i < 3; ++i) {
            auto bt = createTestBackTester();
            backtesters.push_back(bt);
            subject.addBackTester(bt);
        }
        
        // Simulate multiple permutation runs
        const int NUM_PERMUTATIONS = 5;
        subject.simulateMultiplePermutations(NUM_PERMUTATIONS);
        
        // Verify aggregation results
        REQUIRE(aggregator->getStrategyCount() == 3);
        
        subject.detach(&observer);
    }
}

TEST_CASE("Observer pattern thread safety", "[observer][thread-safety][integration]") {
    SECTION("Concurrent permutation testing simulation") {
        // Create shared aggregator
        auto aggregator = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        
        const int NUM_THREADS = 2;
        const int STRATEGIES_PER_THREAD = 2;
        const int PERMUTATIONS_PER_THREAD = 5;
        
        std::vector<std::future<void>> futures;
        
        // Launch multiple threads, each running permutation tests
        for (int t = 0; t < NUM_THREADS; ++t) {
            futures.push_back(std::async(std::launch::async, [=]() {
                // Each thread creates its own observer but shares the aggregator
                TestObserver observer(aggregator);
                MockPermutationTestSubject subject;
                subject.attach(&observer);
                
                // Create strategies for this thread
                for (int s = 0; s < STRATEGIES_PER_THREAD; ++s) {
                    auto bt = createTestBackTester();
                    subject.addBackTester(bt);
                }
                
                // Run permutations
                subject.simulateMultiplePermutations(PERMUTATIONS_PER_THREAD);
                subject.detach(&observer);
            }));
        }
        
        // Wait for all threads to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        // Verify final aggregation state
        size_t expectedStrategies = NUM_THREADS * STRATEGIES_PER_THREAD;
        REQUIRE(aggregator->getStrategyCount() == expectedStrategies);
        
        INFO("Successfully processed " << expectedStrategies << " strategies across " 
             << NUM_THREADS << " threads");
    }
}

TEST_CASE("Observer pattern error handling", "[observer][error-handling][integration]") {
    SECTION("Observer detachment during operation") {
        auto aggregator = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        auto observer = std::make_unique<TestObserver>(aggregator);
        MockPermutationTestSubject subject;
        
        subject.attach(observer.get());
        
        auto bt = createTestBackTester();
        subject.addBackTester(bt);
        
        // Run some permutations
        subject.simulateMultiplePermutations(3);
        REQUIRE(aggregator->getStrategyCount() == 1);
        
        // Detach observer
        subject.detach(observer.get());
        
        // Run more permutations - should not crash
        subject.simulateMultiplePermutations(3);
        
        // Strategy count should remain the same (no new observations)
        REQUIRE(aggregator->getStrategyCount() == 1);
        
        // Destroy observer - should not crash
        observer.reset();
        
        // Subject should still work
        subject.simulateMultiplePermutations(2);
    }
    
    SECTION("Multiple observer attachment/detachment") {
        auto aggregator1 = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        auto aggregator2 = std::make_shared<UuidStrategyPermutationStatsAggregator<DecimalType>>();
        
        TestObserver observer1(aggregator1);
        TestObserver observer2(aggregator2);
        
        MockPermutationTestSubject subject;
        
        // Attach both observers
        subject.attach(&observer1);
        subject.attach(&observer2);
        
        auto bt = createTestBackTester();
        subject.addBackTester(bt);
        
        // Run permutations - both observers should receive notifications
        subject.simulateMultiplePermutations(5);
        
        // Both aggregators should have the same data
        REQUIRE(aggregator1->getStrategyCount() == 1);
        REQUIRE(aggregator2->getStrategyCount() == 1);
        
        // Detach one observer
        subject.detach(&observer1);
        
        // Run more permutations
        subject.simulateMultiplePermutations(3);
        
        // Only observer2 should have received the additional data
        // (We can't easily verify this without exposing internal counts,
        // but the test ensures no crashes occur)
        
        subject.detach(&observer2);
    }
}