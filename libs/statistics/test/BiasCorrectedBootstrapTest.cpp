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
#include <cmath>   // For std::abs, std::pow

#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h" // GeoMeanStat + StatUtils
#include "TestUtils.h" // Assumed to provide DecimalType, createDecimal
#include "number.h"    // For num::to_double

using namespace mkc_timeseries;

// --------------------------- Existing Tests ---------------------------

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
        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

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

// --------------------------- New Tests: BCa + GeoMeanStat ---------------------------

TEST_CASE("BCaBootStrap with GeoMeanStat (geometric mean of returns)", "[BCaBootStrap][GeoMean]") {
    // Absolute tolerance to accommodate Decimal<->double rounding differences.
    constexpr double kTol = 5e-8;

    // A convenience helper to compute the geometric mean in double for cross-checks.
    auto expected_geo = [](const std::vector<double>& rs) -> double {
        if (rs.empty()) return 0.0;
        long double s = 0.0L;
        for (double r : rs) s += std::log1p(r);   // valid for r > -1
        return std::expm1(s / static_cast<long double>(rs.size()));
    };

    SECTION("Basic geometric bootstrap: stat equals direct GeoMean and CI contains it") {
        std::vector<DecimalType> returns = {
            DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
            DecimalType("-0.02"), DecimalType("0.00"), DecimalType("0.04")
        };

        // Build BCA using geometric mean as the statistic
        GeoMeanStat<DecimalType> stat; // default clip=false
        BCaBootStrap<DecimalType> bca(returns, /*num_resamples*/2000, /*CL*/0.95, stat);

        // Direct geometric mean using the same stat functor (exactly what BCA uses)
        DecimalType direct = stat(returns);

        // Cross-check against a pure double implementation (sanity only)
        double expd = expected_geo({0.10, -0.05, 0.20, -0.02, 0.00, 0.04});

        // Properties
        REQUIRE(num::to_double(bca.getStatistic()) == Catch::Approx(num::to_double(direct)).margin(kTol));
        REQUIRE(num::to_double(bca.getStatistic()) == Catch::Approx(expd).margin(kTol));

        // Ordering and inclusion
        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(bca.getStatistic()  >= bca.getLowerBound());
        REQUIRE(bca.getStatistic()  <= bca.getUpperBound());
    }

    SECTION("GeoMeanStat: r <= -1 throws by default inside BCA") {
        std::vector<DecimalType> returns = { DecimalType("0.02"), DecimalType("-1.0"), DecimalType("0.03") };

        // With clip = false, evaluating the statistic should throw
        GeoMeanStat<DecimalType> stat(/*clip_ruin=*/false);
        BCaBootStrap<DecimalType> bca(returns, 200, 0.95, stat);

        // The exception surfaces when the statistic is first computed
        REQUIRE_THROWS_AS((void)bca.getStatistic(), std::domain_error);
    }

    SECTION("GeoMeanStat: clipping mode handles r <= -1 and BCA completes") {
        std::vector<DecimalType> returns = { DecimalType("0.02"), DecimalType("-1.0"), DecimalType("0.03") };

        // In clipping mode, the statistic becomes finite (winsorize -1 to -1+eps)
        GeoMeanStat<DecimalType> stat(/*clip_ruin=*/true, /*eps=*/1e-6);
        BCaBootStrap<DecimalType> bca(returns, 200, 0.95, stat);

        // Should not throw and should produce ordered bounds
        DecimalType theta = bca.getStatistic();
        REQUIRE(theta > DecimalType("-1.0"));

        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(theta >= bca.getLowerBound());
        REQUIRE(theta <= bca.getUpperBound());
    }

    SECTION("Annualization with GeoMean-based per-period returns") {
        std::vector<DecimalType> returns = {
            DecimalType("0.01"), DecimalType("-0.02"), DecimalType("0.03"),
            DecimalType("0.007"), DecimalType("-0.004"), DecimalType("0.015")
        };

        GeoMeanStat<DecimalType> stat;
        BCaBootStrap<DecimalType> bca(returns, 1500, 0.95, stat);

        // Annualize via the BCaAnnualizer (geometric compounding)
        double k = 252.0;
        BCaAnnualizer<DecimalType> annualizer(bca, k);

        // Expected: (1 + g)^{k} - 1
        const DecimalType one = DecimalConstants<DecimalType>::DecimalOne;
        double mean_d = (one + bca.getStatistic()).getAsDouble();
        DecimalType expected_annual = DecimalType(std::pow(mean_d, k)) - one;

        REQUIRE(num::to_double(annualizer.getAnnualizedMean()) == Catch::Approx(num::to_double(expected_annual)).margin(1e-10));
        REQUIRE(annualizer.getAnnualizedLowerBound() <= annualizer.getAnnualizedUpperBound());
    }
}

// --------------------------- Annualization Tests (Existing + Keep) ---------------------------

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
        // Intentionally no-op in the mock.
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
