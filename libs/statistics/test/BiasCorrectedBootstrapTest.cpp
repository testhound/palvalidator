// BCaBootStrapTest.cpp
//
// Unit tests for the BCaBootStrap class.
// This file uses the Catch2 testing framework to validate the correctness
// and statistical properties of the Bias-Corrected and Accelerated (BCa)
// bootstrap implementation.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp> // For Catch::Approx
#include <vector>
#include <numeric> // For std::accumulate
#include <cmath>   // For std::abs

#include "BiasCorrectedBootstrap.h"
#include "TestUtils.h" // Assumed to provide DecimalType
#include "number.h"    // For num::to_double

using namespace mkc_timeseries;

// Test suite for the BCaBootStrap class
TEST_CASE("BCaBootStrap Tests", "[BCaBootStrap]") {

    // SECTION 1: Test constructor with invalid arguments
    SECTION("Constructor validation") {
        std::vector<DecimalType> valid_returns = {DecimalType("0.1")};

        // Test with empty returns vector
        std::vector<DecimalType> empty_returns;
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(empty_returns, 1000), std::invalid_argument);

        // Test with too few resamples
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 50), std::invalid_argument);

        // Test with invalid confidence level (<= 0)
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 1000, 0.0), std::invalid_argument);

        // Test with invalid confidence level (>= 1)
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 1000, 1.0), std::invalid_argument);
    }

    // SECTION 2: Basic functionality and sanity checks
    SECTION("Basic functionality with a simple dataset") {
        std::vector<DecimalType> returns = {
            DecimalType("0.01"), DecimalType("-0.02"), DecimalType("0.03"),
            DecimalType("0.015"), DecimalType("-0.005"), DecimalType("0.025"),
            DecimalType("0.01"), DecimalType("0.00"), DecimalType("-0.01"),
            DecimalType("0.02")
        };
        
        unsigned int num_resamples = 2000;
        double confidence_level = 0.95;

        BCaBootStrap<DecimalType> bca(returns, num_resamples, confidence_level);
        
        // Check if the calculated mean is correct
        DecimalType expected_mean = std::accumulate(returns.begin(), returns.end(), DecimalType(0)) / DecimalType(returns.size());
        REQUIRE(num::to_double(bca.getMean()) == Catch::Approx(num::to_double(expected_mean)));

        // Sanity check: lower bound should be less than or equal to upper bound
        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());

        // Sanity check: the original sample mean should fall within the confidence interval
        REQUIRE(bca.getMean() >= bca.getLowerBound());
        REQUIRE(bca.getMean() <= bca.getUpperBound());
    }

    // SECTION 3: Test with symmetric (normally distributed) data
    SECTION("Symmetric data should produce a roughly symmetric interval") {
        // Data sampled from a normal distribution with mean ~0.05
        std::vector<DecimalType> symmetric_returns = {
            DecimalType("0.055"), DecimalType("0.047"), DecimalType("0.062"),
            DecimalType("0.051"), DecimalType("0.038"), DecimalType("0.069"),
            DecimalType("0.050"), DecimalType("0.042"), DecimalType("0.058"),
            DecimalType("0.031"), DecimalType("0.075"), DecimalType("0.045")
        };

        BCaBootStrap<DecimalType> bca(symmetric_returns, 2000, 0.95);
        
        DecimalType mean = bca.getMean();
        DecimalType lower = bca.getLowerBound();
        DecimalType upper = bca.getUpperBound();

        // For symmetric data, the distance from the mean to each bound should be similar.
        // The bias-correction factor z0 should be close to zero.
        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

        // Allow for some stochastic variation, but they should be in the same ballpark.
        // We check if the ratio of the distances is close to 1.
        REQUIRE(num::to_double(lower_dist / upper_dist) == Catch::Approx(1.0).margin(0.35));
    }

    // SECTION 4: Test with skewed data
    SECTION("Skewed data should produce an asymmetric interval") {
        // Data with a positive skew (long tail of positive returns)
        std::vector<DecimalType> skewed_returns = {
            DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
            DecimalType("-0.05"), DecimalType("0.03"), DecimalType("-0.04"),
            DecimalType("0.025"), DecimalType("0.15"), // outlier
            DecimalType("0.01"), DecimalType("0.02"), DecimalType("-0.03"),
            DecimalType("0.18") // outlier
        };

        BCaBootStrap<DecimalType> bca(skewed_returns, 3000, 0.95);

        DecimalType mean = bca.getMean();
        DecimalType lower = bca.getLowerBound();
        DecimalType upper = bca.getUpperBound();

        // For positively skewed data, the upper tail of the CI should be longer.
        // The distance from the mean to the upper bound should be greater than
        // the distance from the mean to the lower bound.
        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

        REQUIRE(upper_dist > lower_dist);
    }
    
    // SECTION 5: Test with a larger, more realistic dataset
    SECTION("Larger dataset behavior") {
        std::vector<DecimalType> returns;
        // Generate a slightly more realistic set of returns
        for(int i = 0; i < 100; ++i) {
            if (i % 5 == 0)
                returns.push_back(DecimalType("-0.03") + DecimalType(i) / DecimalType(2000));
            else
                returns.push_back(DecimalType("0.01") + DecimalType(i) / DecimalType(5000));
        }

        BCaBootStrap<DecimalType> bca(returns, 5000, 0.99);

        // Perform the same sanity checks as the basic case
        DecimalType expected_mean = std::accumulate(returns.begin(), returns.end(), DecimalType(0)) / DecimalType(returns.size());
        REQUIRE(num::to_double(bca.getMean()) == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(bca.getMean() >= bca.getLowerBound());
        REQUIRE(bca.getMean() <= bca.getUpperBound());
    }
}

TEST_CASE("calculateAnnualizationFactor functionality", "[BCaAnnualizer]") {

    SECTION("Standard time frames") {
        REQUIRE(calculateAnnualizationFactor(TimeFrame::DAILY) == Catch::Approx(252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::WEEKLY) == Catch::Approx(52.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::MONTHLY) == Catch::Approx(12.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::QUARTERLY) == Catch::Approx(4.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::YEARLY) == Catch::Approx(1.0));
    }

    SECTION("Intraday time frames with standard US stock market hours") {
        // 6.5 hours/day, 252 days/year
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 1) == Catch::Approx(6.5 * 60.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 15) == Catch::Approx(6.5 * 4.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 60) == Catch::Approx(6.5 * 1.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 90) == Catch::Approx(6.5 * (60.0 / 90.0) * 252.0));
    }

    SECTION("Intraday time frames with custom hours (e.g., 24-hour Forex)") {
        double forex_hours = 24.0;
        double trading_days = 252.0;
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 1, trading_days, forex_hours) == Catch::Approx(24.0 * 60.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 60, trading_days, forex_hours) == Catch::Approx(24.0 * 1.0 * 252.0));
    }

    SECTION("Invalid arguments throw exceptions") {
        REQUIRE_THROWS_AS(calculateAnnualizationFactor(TimeFrame::INTRADAY, 0), std::invalid_argument);
        // Assuming TimeFrame::UNKNOWN exists and is not a valid choice for annualization
        // If it doesn't exist, this test can be adapted or removed.
        // REQUIRE_THROWS_AS(calculateAnnualizationFactor(TimeFrame::UNKNOWN), std::invalid_argument);
    }
}

// Mock BCaBootStrap class to provide deterministic results for testing the annualizer.
template<class Decimal>
class MockBCaBootStrapForAnnualizer : public BCaBootStrap<Decimal>
{
public:
    // Constructor that calls the base but avoids the expensive calculation.
    MockBCaBootStrapForAnnualizer()
      // Provide a minimal valid vector to satisfy the base constructor's checks.
      : BCaBootStrap<Decimal>(std::vector<Decimal>{Decimal("0.0"), Decimal("0.0")}, 100)
    {}

    // Manually set the results for testing purposes.
    void setTestResults(const Decimal& mean, const Decimal& lower, const Decimal& upper)
    {
        this->setMean(mean);
        this->setLowerBound(lower);
        this->setUpperBound(upper);
        // Mark as calculated to prevent base class calculation
        this->m_is_calculated = true;
    }

protected:
    // Override the calculation method to do nothing, preventing the expensive bootstrap.
    void calculateBCaBounds() override {
        std::cout << "In MockBCaBootStrapForAnnualizer::calculateBCaBounds" << std::endl;
        // This is intentionally left empty for the mock.
    }
};


TEST_CASE("BCaAnnualizer functionality", "[BCaAnnualizer]") {

    // Create a mock BCaBootStrap object
    MockBCaBootStrapForAnnualizer<DecimalType> mock_bca;

    SECTION("Annualizing positive returns") {
        DecimalType per_bar_mean = createDecimal("0.001");
        DecimalType per_bar_lower = createDecimal("0.0005");
        DecimalType per_bar_upper = createDecimal("0.0015");
        mock_bca.setTestResults(per_bar_mean, per_bar_lower, per_bar_upper);

        double annualization_factor = 252.0;
        BCaAnnualizer<DecimalType> annualizer(mock_bca, annualization_factor);

        // Expected results from geometric compounding: (1+r)^N - 1
        DecimalType expected_mean = DecimalType(pow((DecimalType("1.0") + per_bar_mean).getAsDouble(), annualization_factor)) - DecimalType("1.0");
        DecimalType expected_lower = DecimalType(pow((DecimalType("1.0") + per_bar_lower).getAsDouble(), annualization_factor)) - DecimalType("1.0");
        DecimalType expected_upper = DecimalType(pow((DecimalType("1.0") + per_bar_upper).getAsDouble(), annualization_factor)) - DecimalType("1.0");

        REQUIRE(num::to_double(annualizer.getAnnualizedMean()) == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(num::to_double(annualizer.getAnnualizedLowerBound()) == Catch::Approx(num::to_double(expected_lower)));
        REQUIRE(num::to_double(annualizer.getAnnualizedUpperBound()) == Catch::Approx(num::to_double(expected_upper)));
    }

    SECTION("Annualizing negative returns") {
        DecimalType per_bar_mean = createDecimal("-0.0005");
        DecimalType per_bar_lower = createDecimal("-0.001");
        DecimalType per_bar_upper = createDecimal("-0.0002");
        mock_bca.setTestResults(per_bar_mean, per_bar_lower, per_bar_upper);

        double annualization_factor = 252.0;
        BCaAnnualizer<DecimalType> annualizer(mock_bca, annualization_factor);

        DecimalType expected_mean = DecimalType(pow((DecimalType("1.0") + per_bar_mean).getAsDouble(), annualization_factor)) - DecimalType("1.0");
        DecimalType expected_lower = DecimalType(pow((DecimalType("1.0") + per_bar_lower).getAsDouble(), annualization_factor)) - DecimalType("1.0");
        DecimalType expected_upper = DecimalType(pow((DecimalType("1.0") + per_bar_upper).getAsDouble(), annualization_factor)) - DecimalType("1.0");

        REQUIRE(num::to_double(annualizer.getAnnualizedMean()) == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(num::to_double(annualizer.getAnnualizedLowerBound()) == Catch::Approx(num::to_double(expected_lower)));
        REQUIRE(num::to_double(annualizer.getAnnualizedUpperBound()) == Catch::Approx(num::to_double(expected_upper)));
    }

    SECTION("Invalid annualization factor throws exception") {
        mock_bca.setTestResults(createDecimal("0.01"), createDecimal("0.0"), createDecimal("0.02"));
        
        REQUIRE_THROWS_AS(BCaAnnualizer<DecimalType>(mock_bca, 0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaAnnualizer<DecimalType>(mock_bca, -252.0), std::invalid_argument);
    }
}
