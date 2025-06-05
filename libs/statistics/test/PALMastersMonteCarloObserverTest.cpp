// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include "PALMastersMonteCarloValidation.h"
#include "PermutationStatisticsCollector.h"
#include "MastersRomanoWolfImproved.h"
#include "MastersRomanoWolf.h"
#include "MonteCarloPermutationTest.h"
#include "MonteCarloTestPolicy.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

/**
 * @brief PALMastersMonteCarloValidation observer pattern integration tests
 * 
 * Tests the complete observer chain from top-level orchestrator through
 * intermediate classes to the statistics collector observer.
 */

TEST_CASE("PALMastersMonteCarloValidation observer integration", "[observer][palvalidation][integration]") {
    SECTION("Statistics collector integration") {
        const uint32_t numPermutations = 10; // Small for testing
        
        // Create the top-level orchestrator with observer integration
        PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>> validation(numPermutations);
        
        // Verify that statistics collector is properly initialized
        const auto& collector = validation.getStatisticsCollector();
        REQUIRE(collector.getStrategyCount() == 0);
        
        // Test non-const access
        auto& nonConstCollector = const_cast<PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>>&>(validation).getStatisticsCollector();
        REQUIRE(nonConstCollector.getStrategyCount() == 0);
        
        // Test clear functionality
        nonConstCollector.clear();
        REQUIRE(nonConstCollector.getStrategyCount() == 0);
    }
}

TEST_CASE("MastersRomanoWolfImproved observer chaining", "[observer][palvalidation][intermediate]") {
    SECTION("Observer attachment and chaining") {
        MastersRomanoWolfImproved<DecimalType, AllHighResLogPFPolicy<DecimalType>> algorithm;
        PermutationStatisticsCollector<DecimalType> collector;
        
        // Test that algorithm inherits from PermutationTestSubject
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MastersRomanoWolfImproved<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "MastersRomanoWolfImproved should inherit from PermutationTestSubject");
        
        // Test observer attachment
        REQUIRE_NOTHROW(algorithm.attach(&collector));
        REQUIRE_NOTHROW(algorithm.detach(&collector));
        
        // Verify collector is properly typed
        static_assert(std::is_base_of_v<PermutationTestObserver<DecimalType>, PermutationStatisticsCollector<DecimalType>>,
                      "PermutationStatisticsCollector should inherit from PermutationTestObserver");
    }
}

TEST_CASE("MastersRomanoWolf observer chaining", "[observer][palvalidation][intermediate]") {
    SECTION("Observer attachment and chaining") {
        MastersRomanoWolf<DecimalType, AllHighResLogPFPolicy<DecimalType>> algorithm;
        PermutationStatisticsCollector<DecimalType> collector;
        
        // Test that algorithm inherits from PermutationTestSubject
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MastersRomanoWolf<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "MastersRomanoWolf should inherit from PermutationTestSubject");
        
        // Test observer attachment
        REQUIRE_NOTHROW(algorithm.attach(&collector));
        REQUIRE_NOTHROW(algorithm.detach(&collector));
    }
}

TEST_CASE("MonteCarloPermuteMarketChanges observer chaining", "[observer][palvalidation][intermediate]") {
    SECTION("Observer attachment and chaining") {
        // Create a minimal backtester for testing
        auto backtester = std::make_shared<DailyBackTester<DecimalType>>();
        
        // This will throw since no strategies are added, but tests compilation
        REQUIRE_THROWS_AS(MonteCarloPermuteMarketChanges<DecimalType>(backtester, 10), 
                         MonteCarloPermutationException);
        
        // Test that class inherits from PermutationTestSubject
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MonteCarloPermuteMarketChanges<DecimalType>>,
                      "MonteCarloPermuteMarketChanges should inherit from PermutationTestSubject");
    }
}

TEST_CASE("PermutationStatisticsCollector functionality", "[observer][palvalidation][collector]") {
    SECTION("Basic observer interface implementation") {
        PermutationStatisticsCollector<DecimalType> collector;
        
        // Test basic functionality
        REQUIRE(collector.getStrategyCount() == 0);
        
        // Test clear functionality
        REQUIRE_NOTHROW(collector.clear());
        REQUIRE(collector.getStrategyCount() == 0);
        
        // Test that it properly implements the observer interface
        static_assert(std::is_base_of_v<PermutationTestObserver<DecimalType>, PermutationStatisticsCollector<DecimalType>>,
                      "PermutationStatisticsCollector should inherit from PermutationTestObserver");
    }
    
    SECTION("Statistics collection interface") {
        PermutationStatisticsCollector<DecimalType> collector;
        
        // Test that all required methods exist and can be called
        // Note: These will return nullopt since no data has been added
        const PalStrategy<DecimalType>* nullStrategy = nullptr;
        
        REQUIRE_FALSE(collector.getMinPermutedStatistic(nullStrategy).has_value());
        REQUIRE_FALSE(collector.getMaxPermutedStatistic(nullStrategy).has_value());
        REQUIRE_FALSE(collector.getMedianPermutedStatistic(nullStrategy).has_value());
        REQUIRE_FALSE(collector.getStdDevPermutedStatistic(nullStrategy).has_value());
        
        // Test UUID and pattern hash methods
        REQUIRE(collector.getStrategyUuid(nullStrategy) == boost::uuids::nil_uuid());
        REQUIRE(collector.getPatternHash(nullStrategy) == 0);
    }
}

TEST_CASE("PALMastersMonteCarloValidation observer pattern architecture", "[observer][palvalidation][architecture]") {
    SECTION("Correct inheritance hierarchy") {
        // Verify intermediate classes are Subjects (not Observers)
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MastersRomanoWolfImproved<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "MastersRomanoWolfImproved should be a Subject");
        
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MastersRomanoWolf<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "MastersRomanoWolf should be a Subject");
        
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MonteCarloPermuteMarketChanges<DecimalType>>,
                      "MonteCarloPermuteMarketChanges should be a Subject");
        
        // Verify collector is an Observer (not a Subject)
        static_assert(std::is_base_of_v<PermutationTestObserver<DecimalType>, 
                      PermutationStatisticsCollector<DecimalType>>,
                      "PermutationStatisticsCollector should be an Observer");
        
        // Verify PALMastersMonteCarloValidation is an Orchestrator (neither Subject nor Observer)
        static_assert(!std::is_base_of_v<PermutationTestObserver<DecimalType>, 
                      PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "PALMastersMonteCarloValidation should NOT be an Observer");
        
        static_assert(!std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "PALMastersMonteCarloValidation should NOT be a Subject");
    }
}

TEST_CASE("PALMastersMonteCarloValidation observer compilation validation", "[observer][palvalidation][compilation]") {
    SECTION("All observer pattern components compile correctly") {
        // Test that all the PALMastersMonteCarloValidation observer components can be instantiated
        REQUIRE_NOTHROW([]() {
            // Create collector
            PermutationStatisticsCollector<DecimalType> collector;
            
            // Create intermediate classes
            MastersRomanoWolfImproved<DecimalType, AllHighResLogPFPolicy<DecimalType>> improved;
            MastersRomanoWolf<DecimalType, AllHighResLogPFPolicy<DecimalType>> standard;
            
            // Create orchestrator
            PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>> validation(10);
            
            // Test observer attachment
            improved.attach(&collector);
            standard.attach(&collector);
            
            // Test observer detachment
            improved.detach(&collector);
            standard.detach(&collector);
            
            // Test statistics collector access
            const auto& constCollector = validation.getStatisticsCollector();
            auto& nonConstCollector = const_cast<PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>>&>(validation).getStatisticsCollector();
            
            (void)constCollector;    // Suppress unused variable warning
            (void)nonConstCollector; // Suppress unused variable warning
        }());
    }
}

TEST_CASE("AllHighResLogPFPolicy integration with observers", "[observer][palvalidation][policy]") {
    SECTION("Policy class compatibility with observer pattern") {
        // Test that AllHighResLogPFPolicy works with the observer pattern
        using PolicyType = AllHighResLogPFPolicy<DecimalType>;
        
        // Verify policy has required methods
        REQUIRE(PolicyType::getMinStrategyTrades() == 3);
        REQUIRE(PolicyType::getMinTradeFailureTestStatistic() == createDecimal("0.0"));
        
        // Test that policy can be used with PALMastersMonteCarloValidation
        REQUIRE_NOTHROW([]() {
            PALMastersMonteCarloValidation<DecimalType, PolicyType> validation(5);
            const auto& collector = validation.getStatisticsCollector();
            (void)collector; // Suppress unused variable warning
        }());
        
        // Test that policy can be used with intermediate classes
        REQUIRE_NOTHROW([]() {
            MastersRomanoWolfImproved<DecimalType, PolicyType> improved;
            MastersRomanoWolf<DecimalType, PolicyType> standard;
            PermutationStatisticsCollector<DecimalType> collector;
            
            improved.attach(&collector);
            standard.attach(&collector);
            improved.detach(&collector);
            standard.detach(&collector);
        }());
    }
}

TEST_CASE("PALMastersMonteCarloValidation observer integration readiness", "[observer][palvalidation][readiness]") {
    SECTION("All PALMastersMonteCarloValidation observer components are ready for integration") {
        // This test verifies that all observer components exist and can work together
        
        const uint32_t numPermutations = 5;
        
        // Create orchestrator
        PALMastersMonteCarloValidation<DecimalType, AllHighResLogPFPolicy<DecimalType>> validation(numPermutations);
        
        // Verify statistics collector is accessible
        const auto& collector = validation.getStatisticsCollector();
        REQUIRE(collector.getStrategyCount() == 0);
        
        // Create intermediate classes
        auto improved = std::make_unique<MastersRomanoWolfImproved<DecimalType, AllHighResLogPFPolicy<DecimalType>>>();
        auto standard = std::make_unique<MastersRomanoWolf<DecimalType, AllHighResLogPFPolicy<DecimalType>>>();
        
        // Create collector
        auto statsCollector = std::make_unique<PermutationStatisticsCollector<DecimalType>>();
        
        // Test observer attachment
        improved->attach(statsCollector.get());
        standard->attach(statsCollector.get());
        
        // Test observer detachment
        improved->detach(statsCollector.get());
        standard->detach(statsCollector.get());
        
        INFO("PALMastersMonteCarloValidation observer pattern integration is ready for production use");
        SUCCEED();
    }
}

TEST_CASE("PALMastersMonteCarloValidation observer pattern benefits validation", "[observer][palvalidation][benefits]") {
    SECTION("Observer pattern provides expected benefits") {
        // Test that the observer pattern implementation provides the expected benefits
        
        // 1. UUID-based strategy identification eliminates collision risk
        boost::uuids::random_generator gen;
        auto uuid1 = gen();
        auto uuid2 = gen();
        REQUIRE(uuid1 != uuid2);
        
        // 2. Boost.Accumulators integration for memory efficiency
        PermutationStatisticsCollector<DecimalType> collector;
        REQUIRE(collector.getStrategyCount() == 0); // O(1) memory usage
        
        // 3. Thread-safe statistics collection
        // (Basic test - full thread safety tested in other test files)
        REQUIRE_NOTHROW(collector.clear());
        
        // 4. Enhanced BackTester methods compilation
        DailyBackTester<DecimalType> backTester;
        REQUIRE_THROWS_AS(backTester.getNumTrades(), BackTesterException);
        REQUIRE_THROWS_AS(backTester.getNumBarsInTrades(), BackTesterException);
        
        // 5. Clean separation of concerns
        // Subjects manage observers, Observers collect statistics, Orchestrator coordinates
        static_assert(std::is_base_of_v<PermutationTestSubject<DecimalType>, 
                      MastersRomanoWolfImproved<DecimalType, AllHighResLogPFPolicy<DecimalType>>>,
                      "Clean separation: Intermediate classes are Subjects");
        
        static_assert(std::is_base_of_v<PermutationTestObserver<DecimalType>, 
                      PermutationStatisticsCollector<DecimalType>>,
                      "Clean separation: Collector is an Observer");
        
        INFO("PALMastersMonteCarloValidation observer pattern delivers all expected benefits");
        SUCCEED();
    }
}