// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <boost/uuid/uuid_io.hpp>
#include "ThreadSafeAccumulator.h"
#include "PermutationTestObserver.h"
#include "PermutationTestSubject.h"
#include "StrategyIdentificationHelper.h"
#include "UuidStrategyPermutationStatsAggregator.h"
#include "PALMastersMonteCarloValidationObserver.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

TEST_CASE("ThreadSafeAccumulator basic functionality", "[observer][accumulator]") {
    ThreadSafeAccumulator<DecimalType> accumulator;
    
    SECTION("Empty accumulator returns nullopt") {
        REQUIRE_FALSE(accumulator.getMin().has_value());
        REQUIRE_FALSE(accumulator.getMax().has_value());
        REQUIRE_FALSE(accumulator.getMedian().has_value());
        REQUIRE_FALSE(accumulator.getStdDev().has_value());
        REQUIRE(accumulator.getCount() == 0);
    }
    
    SECTION("Single value statistics") {
        accumulator.addValue(createDecimal("5.0"));
        
        REQUIRE(accumulator.getMin().value() == createDecimal("5.0"));
        REQUIRE(accumulator.getMax().value() == createDecimal("5.0"));
        // Note: Boost.Accumulators median behavior with single value may vary
        REQUIRE(accumulator.getMedian().has_value());
        REQUIRE_FALSE(accumulator.getStdDev().has_value()); // Need at least 2 values
        REQUIRE(accumulator.getCount() == 1);
    }
    
    SECTION("Multiple values statistics") {
        accumulator.addValue(createDecimal("1.0"));
        accumulator.addValue(createDecimal("2.0"));
        accumulator.addValue(createDecimal("3.0"));
        accumulator.addValue(createDecimal("4.0"));
        accumulator.addValue(createDecimal("5.0"));
        
        REQUIRE(accumulator.getMin().value() == createDecimal("1.0"));
        REQUIRE(accumulator.getMax().value() == createDecimal("5.0"));
        REQUIRE(accumulator.getMedian().value() == 3.0);
        REQUIRE(accumulator.getStdDev().has_value());
        REQUIRE(accumulator.getCount() == 5);
        
        // Standard deviation for [1,2,3,4,5] should be approximately sqrt(2) ≈ 1.414
        double stddev = accumulator.getStdDev().value();
        REQUIRE(stddev > 1.3);
        REQUIRE(stddev < 1.5);
    }
    
    SECTION("Clear functionality") {
        accumulator.addValue(createDecimal("1.0"));
        accumulator.addValue(createDecimal("2.0"));
        
        REQUIRE(accumulator.getCount() == 2);
        
        accumulator.clear();
        
        REQUIRE(accumulator.getCount() == 0);
        REQUIRE_FALSE(accumulator.getMin().has_value());
    }
}

TEST_CASE("UUID generation in BacktesterStrategy", "[observer][uuid]") {
    // This test would require creating actual strategy instances
    // For now, we'll test the concept with a mock
    
    SECTION("UUID uniqueness") {
        // Create multiple UUIDs and verify they're different
        boost::uuids::random_generator gen;
        auto uuid1 = gen();
        auto uuid2 = gen();
        auto uuid3 = gen();
        
        REQUIRE(uuid1 != uuid2);
        REQUIRE(uuid2 != uuid3);
        REQUIRE(uuid1 != uuid3);
        
        // Test hash generation
        boost::hash<boost::uuids::uuid> hasher;
        auto hash1 = hasher(uuid1);
        auto hash2 = hasher(uuid2);
        
        REQUIRE(hash1 != hash2);
    }
}

TEST_CASE("UuidStrategyPermutationStatsAggregator functionality", "[observer][aggregator]") {
    UuidStrategyPermutationStatsAggregator<DecimalType> aggregator;
    
    SECTION("Empty aggregator") {
        REQUIRE(aggregator.getStrategyCount() == 0);
    }
    
    SECTION("Basic statistics collection") {
        // For now, just verify the aggregator can be created and cleared
        // Note: Full testing would require actual PalStrategy instances
        aggregator.clear();
        REQUIRE(aggregator.getStrategyCount() == 0);
    }
}

TEST_CASE("PermutationTestSubject observer management", "[observer][subject]") {
    // Create a simple test subject
    class TestSubject : public PermutationTestSubject<DecimalType> {
    public:
        void triggerNotification(const BackTester<DecimalType>& bt, const DecimalType& stat) {
            notifyObservers(bt, stat);
        }
    };
    
    // Create a simple test observer
    class TestObserver : public PermutationTestObserver<DecimalType> {
    public:
        int updateCount = 0;
        DecimalType lastStatistic = createDecimal("0.0");
        
        void update(const BackTester<DecimalType>& permutedBacktester,
                   const DecimalType& permutedTestStatistic) override {
            updateCount++;
            lastStatistic = permutedTestStatistic;
        }
        
        std::optional<DecimalType> getMinMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return std::nullopt;
        }
        
        std::optional<DecimalType> getMaxMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return std::nullopt;
        }
        
        std::optional<double> getMedianMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return std::nullopt;
        }
        
        std::optional<double> getStdDevMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return std::nullopt;
        }
        
        void clear() override {}
    };
    
    TestSubject subject;
    TestObserver observer1, observer2;
    
    SECTION("Observer attachment and notification") {
        subject.attach(&observer1);
        subject.attach(&observer2);
        
        // Create a mock BackTester (in real usage, this would be a proper instance)
        DailyBackTester<DecimalType> mockBackTester;
        
        subject.triggerNotification(mockBackTester, createDecimal("1.5"));
        
        REQUIRE(observer1.updateCount == 1);
        REQUIRE(observer1.lastStatistic == createDecimal("1.5"));
        REQUIRE(observer2.updateCount == 1);
        REQUIRE(observer2.lastStatistic == createDecimal("1.5"));
    }
    
    SECTION("Observer detachment") {
        subject.attach(&observer1);
        subject.attach(&observer2);
        
        DailyBackTester<DecimalType> mockBackTester;
        subject.triggerNotification(mockBackTester, createDecimal("1.0"));
        
        REQUIRE(observer1.updateCount == 1);
        REQUIRE(observer2.updateCount == 1);
        
        subject.detach(&observer1);
        subject.triggerNotification(mockBackTester, createDecimal("2.0"));
        
        REQUIRE(observer1.updateCount == 1); // Should not have increased
        REQUIRE(observer2.updateCount == 2); // Should have increased
        REQUIRE(observer2.lastStatistic == createDecimal("2.0"));
    }
}

TEST_CASE("BackTester new methods compilation", "[observer][backtester]") {
    // Test that the new methods compile and can be called
    // Note: This test focuses on compilation rather than full functionality
    // since we'd need a complete setup with strategies and portfolios for full testing
    
    SECTION("Method signatures exist") {
        DailyBackTester<DecimalType> backTester;
        
        // These calls will throw exceptions since no strategies are added,
        // but they verify the methods exist and compile correctly
        REQUIRE_THROWS_AS(backTester.getNumTrades(), BackTesterException);
        REQUIRE_THROWS_AS(backTester.getNumBarsInTrades(), BackTesterException);
    }
}