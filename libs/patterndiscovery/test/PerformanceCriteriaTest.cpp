// test/PerformanceCriteriaTests.cpp
// Always use fixed include references, never relative paths
#include <catch2/catch_test_macros.hpp> // For Catch2 test macros
#include "PerformanceCriteria.h"       // Include the class to be tested
#include "number.h"                    // For num::DefaultNumber

// Define the decimal type to use for testing
using TestDecimalType = num::DefaultNumber;

// Test cases for PerformanceCriteria class
TEST_CASE("PerformanceCriteria construction and getters", "[PerformanceCriteria]")
{
    SECTION("Valid construction with typical values")
    {
        // Example values from the problem description, Table 7.4
        // Minimum profitability: 80%
        // Minimum number of trades: 28
        // Maximum consecutive losers: 5
        // Minimum Profit factor: 2
        PerformanceCriteria<TestDecimalType> criteria(TestDecimalType(80), 28, 5, TestDecimalType(2));

        // Perform direct DecimalType comparisons
        REQUIRE(criteria.getMinProfitability() == TestDecimalType(80));
        REQUIRE(criteria.getMinTrades() == 28);
        REQUIRE(criteria.getMaxConsecutiveLosers() == 5);
        REQUIRE(criteria.getMinProfitFactor() == TestDecimalType(2));
    }

    SECTION("Valid construction with edge case minimums")
    {
        PerformanceCriteria<TestDecimalType> criteria(TestDecimalType(0), 1, 0, TestDecimalType("0.0000001")); // Very small positive profit factor

        // Perform direct DecimalType comparisons
        REQUIRE(criteria.getMinProfitability() == TestDecimalType(0));
        REQUIRE(criteria.getMinTrades() == 1);
        REQUIRE(criteria.getMaxConsecutiveLosers() == 0);
        REQUIRE(criteria.getMinProfitFactor() == TestDecimalType("0.0000001"));
    }

    SECTION("Valid construction with edge case maximums")
    {
        PerformanceCriteria<TestDecimalType> criteria(TestDecimalType(100), 1000, 100, TestDecimalType(100));

        // Perform direct DecimalType comparisons
        REQUIRE(criteria.getMinProfitability() == TestDecimalType(100));
        REQUIRE(criteria.getMinTrades() == 1000);
        REQUIRE(criteria.getMaxConsecutiveLosers() == 100);
        REQUIRE(criteria.getMinProfitFactor() == TestDecimalType(100));
    }

    SECTION("Invalid minProfitability - below 0")
    {
        REQUIRE_THROWS_AS(PerformanceCriteria<TestDecimalType>(TestDecimalType(-0.1), 1, 0, TestDecimalType(1.0)), PerformanceCriteriaException);
    }

    SECTION("Invalid minProfitability - above 100")
    {
        REQUIRE_THROWS_AS(PerformanceCriteria<TestDecimalType>(TestDecimalType(100.1), 1, 0, TestDecimalType(1.0)), PerformanceCriteriaException);
    }

    SECTION("Invalid minTrades - zero")
    {
        REQUIRE_THROWS_AS(PerformanceCriteria<TestDecimalType>(TestDecimalType(50.0), 0, 0, TestDecimalType(1.0)), PerformanceCriteriaException);
    }

    SECTION("Invalid minProfitFactor - zero")
    {
        REQUIRE_THROWS_AS(PerformanceCriteria<TestDecimalType>(TestDecimalType(50.0), 1, 0, TestDecimalType(0.0)), PerformanceCriteriaException);
    }

    SECTION("Invalid minProfitFactor - negative")
    {
        REQUIRE_THROWS_AS(PerformanceCriteria<TestDecimalType>(TestDecimalType(50.0), 1, 0, TestDecimalType(-1.0)), PerformanceCriteriaException);
    }
}
