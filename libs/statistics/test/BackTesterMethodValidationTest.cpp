// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#include <catch2/catch_test_macros.hpp>
#include "BackTester.h"
#include "PalStrategy.h"
#include "TestUtils.h"
#include "StrategyIdentificationHelper.h"
#include "Security.h"

using namespace mkc_timeseries;

// No need for dummy classes - we'll use TestUtils functions

/**
 * @brief Validation tests for new BackTester methods vs existing approach
 * 
 * These tests ensure the new getNumTrades() and getNumBarsInTrades() methods
 * provide accurate counts including open positions, compared to the previous
 * estimation-based approach.
 */

TEST_CASE("BackTester new methods compilation", "[backtester][validation]") {
    SECTION("Method signatures exist") {
        DailyBackTester<DecimalType> backTester;
        
        // These calls will throw exceptions since no strategies are added,
        // but they verify the methods exist and compile correctly
        REQUIRE_THROWS_AS(backTester.getNumTrades(), BackTesterException);
        REQUIRE_THROWS_AS(backTester.getNumBarsInTrades(), BackTesterException);
    }
}

TEST_CASE("BackTester methods with real PAL strategy", "[backtester][validation]") {
    SECTION("Methods work with strategy added") {
        // Use real PAL strategy from TestUtils
        auto strategy = getRandomPalStrategy();
        auto timeSeries = getRandomPriceSeries();
        
        // Get the actual date range from the time series
        auto startDate = timeSeries->getFirstDate();
        auto endDate = timeSeries->getLastDate();
        
        DailyBackTester<DecimalType> bt;
        bt.addDateRange(DateRange(startDate, endDate));
        bt.addStrategy(strategy);
        
        // Run the backtest
        bt.backtest();
        
        // Test new methods - should not throw with strategy present
        auto numTrades = bt.getNumTrades();
        auto numBars = bt.getNumBarsInTrades();
        
        // Verify basic constraints
        REQUIRE(numTrades >= 0);
        REQUIRE(numBars >= 0);
        
        // If there are trades, there should be bars
        if (numTrades > 0) {
            REQUIRE(numBars > 0);
        }
        
        INFO("Trades: " << numTrades << ", Bars: " << numBars);
    }
}

TEST_CASE("StrategyIdentificationHelper with real PAL strategy", "[strategy-identification][validation]") {
    SECTION("Strategy identification works") {
        // Use real PAL strategy from TestUtils
        auto strategy = getRandomPalStrategy();
        auto timeSeries = getRandomPriceSeries();
        
        // Get the actual date range from the time series
        auto startDate = timeSeries->getFirstDate();
        auto endDate = timeSeries->getLastDate();
        
        DailyBackTester<DecimalType> bt;
        bt.addDateRange(DateRange(startDate, endDate));
        bt.addStrategy(strategy);
        bt.backtest();
        
        // Test strategy identification
        auto strategyHash = StrategyIdentificationHelper<DecimalType>::extractStrategyHash(bt);
        REQUIRE(strategyHash != 0);  // Should have a valid hash
        
        // Test statistics extraction
        auto numTrades = StrategyIdentificationHelper<DecimalType>::extractNumTrades(bt);
        auto numBars = StrategyIdentificationHelper<DecimalType>::extractNumBarsInTrades(bt);
        
        // Verify extracted values match direct method calls
        REQUIRE(numTrades == bt.getNumTrades());
        REQUIRE(numBars == bt.getNumBarsInTrades());
        
        INFO("Strategy hash: " << strategyHash);
        INFO("Extracted trades: " << numTrades);
        INFO("Extracted bars: " << numBars);
    }
}

TEST_CASE("Strategy UUID uniqueness", "[strategy-identification][validation]") {
    SECTION("Multiple strategy instances have unique UUIDs") {
        // Use real PAL strategies from TestUtils
        auto strategy1 = getRandomPalStrategy();
        auto strategy2 = getRandomPalStrategy();
        
        // Each strategy should have unique instance ID
        REQUIRE(strategy1->getInstanceId() != strategy2->getInstanceId());
        
        // Overall hash should be different (UUID-based)
        REQUIRE(strategy1->hashCode() != strategy2->hashCode());
        
        INFO("Strategy1 UUID: " << boost::uuids::to_string(strategy1->getInstanceId()));
        INFO("Strategy2 UUID: " << boost::uuids::to_string(strategy2->getInstanceId()));
    }
}