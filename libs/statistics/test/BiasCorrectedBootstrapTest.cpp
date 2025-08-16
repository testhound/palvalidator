// BCaBootStrapTest.cpp
//
// Unit tests for the BCaBootStrap class and resampling policies.
// Uses Catch2.
//
// New in this version:
//  - Tests for StationaryBlockResampler (policy-only & via BCaBootStrap)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

#include "BiasCorrectedBootstrap.h" // Includes policy classes
#include "TestUtils.h"              // DecimalType, createDecimal
#include "number.h"                 // num::to_double
#include "randutils.hpp"            // for seeded rng in policy tests

using namespace mkc_timeseries;

// --------------------------- Existing BCa tests ---------------------------

TEST_CASE("BCaBootStrap Tests", "[BCaBootStrap]") {

    SECTION("Constructor validation") {
        std::vector<DecimalType> valid_returns = {DecimalType("0.1")};

        std::vector<DecimalType> empty_returns;
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(empty_returns, 1000), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 50), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 1000, 0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 1000, 1.0), std::invalid_argument);
    }

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

        DecimalType expected_mean =
            std::accumulate(returns.begin(), returns.end(), DecimalType(0)) / DecimalType(returns.size());
        REQUIRE(num::to_double(bca.getMean()) == Catch::Approx(num::to_double(expected_mean)));

        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(bca.getMean() >= bca.getLowerBound());
        REQUIRE(bca.getMean() <= bca.getUpperBound());
    }

    SECTION("Symmetric data should produce a roughly symmetric interval") {
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

        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

        REQUIRE(num::to_double(lower_dist / upper_dist) == Catch::Approx(1.0).margin(0.35));
    }

    SECTION("Skewed data should produce an asymmetric interval") {
        std::vector<DecimalType> skewed_returns = {
            DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
            DecimalType("-0.05"), DecimalType("0.03"), DecimalType("-0.04"),
            DecimalType("0.025"), DecimalType("0.15"),
            DecimalType("0.01"), DecimalType("0.02"), DecimalType("-0.03"),
            DecimalType("0.18")
        };

        BCaBootStrap<DecimalType> bca(skewed_returns, 3000, 0.95);

        DecimalType mean = bca.getMean();
        DecimalType lower = bca.getLowerBound();
        DecimalType upper = bca.getUpperBound();

        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

        REQUIRE(upper_dist > lower_dist);
    }

    SECTION("Larger dataset behavior") {
        std::vector<DecimalType> returns;
        for (int i = 0; i < 100; ++i) {
            if (i % 5 == 0)
                returns.push_back(DecimalType("-0.03") + DecimalType(i) / DecimalType(2000));
            else
                returns.push_back(DecimalType("0.01") + DecimalType(i) / DecimalType(5000));
        }

        BCaBootStrap<DecimalType> bca(returns, 5000, 0.99);

        DecimalType expected_mean =
            std::accumulate(returns.begin(), returns.end(), DecimalType(0)) / DecimalType(returns.size());
        REQUIRE(num::to_double(bca.getMean()) == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(bca.getMean() >= bca.getLowerBound());
        REQUIRE(bca.getMean() <= bca.getUpperBound());
    }
}

// --------------------------- New: Policy tests ---------------------------

TEST_CASE("StationaryBlockResampler basic behavior", "[Resampler][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // Build a simple monotone sequence so we can infer indices from values
    const size_t n = 200;
    std::vector<D> x; x.reserve(n);
    for (size_t i = 0; i < n; ++i) x.push_back(D(static_cast<int>(i))); // values 0..n-1

    // Fixed-seed RNG for determinism in this policy-only test
    randutils::seed_seq_fe128 seed{12345u, 67890u, 13579u, 24680u};
    randutils::mt19937_rng rng(seed);

    SECTION("Throws on empty input") {
        Policy pol(4);
        std::vector<D> empty;
        REQUIRE_THROWS_AS(pol(empty, 10, rng), std::invalid_argument);
    }

    SECTION("Output size and domain are correct; contiguity is substantial") {
        const size_t L = 4;
        Policy pol(L);

        std::vector<D> y = pol(x, n, rng);

        // size
        REQUIRE(y.size() == n);

        // all values are from the domain 0..n-1
        for (const auto& v : y) {
            const double vd = num::to_double(v);
            REQUIRE(vd >= 0.0);
            REQUIRE(vd < static_cast<double>(n));
        }

        // contiguity: fraction of (y[t+1] == (y[t]+1) mod n) should be high (~ 1 - 1/L)
        // With L=4, expectation is ~0.75. Allow a safe lower bound.
        size_t adjacent = 0;
        for (size_t t = 0; t + 1 < y.size(); ++t) {
            int cur = static_cast<int>(num::to_double(y[t]));
            int nxt = static_cast<int>(num::to_double(y[t + 1]));
            if (nxt == (cur + 1) % static_cast<int>(n)) adjacent++;
        }
        const double frac_adjacent = static_cast<double>(adjacent) / static_cast<double>(y.size() - 1);
        REQUIRE(frac_adjacent > 0.60); // conservative threshold
    }

    SECTION("Mean block length is coerced to >= 2") {
        Policy pol1(1);  // should coerce to 2
        Policy pol2(2);  // stays 2
        Policy pol5(5);  // stays 5
        REQUIRE(pol1.meanBlockLen() == 2);
        REQUIRE(pol2.meanBlockLen() == 2);
        REQUIRE(pol5.meanBlockLen() == 5);
    }
}

TEST_CASE("BCaBootStrap works with StationaryBlockResampler", "[BCaBootStrap][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // Build a small, autocorrelated-ish series: clusters of positives and negatives
    std::vector<D> returns;
    for (int k = 0; k < 30; ++k) {        // 180 points total
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
    }

    // Geometric mean statistic to exercise the path with a custom stat as well
    GeoMeanStat<D> gstat;
    const unsigned int B = 1500;
    const double cl = 0.95;

    // IID baseline (default policy)
    BCaBootStrap<D> bca_iid(returns, B, cl, gstat);
    REQUIRE(bca_iid.getLowerBound() <= bca_iid.getUpperBound());
    REQUIRE(bca_iid.getStatistic()  >= bca_iid.getLowerBound());
    REQUIRE(bca_iid.getStatistic()  <= bca_iid.getUpperBound());

    // Stationary blocks with mean L = 3 (close to the run length we used)
    Policy pol(3);
    BCaBootStrap<D, Policy> bca_blk(returns, B, cl, gstat, pol);
    REQUIRE(bca_blk.getLowerBound() <= bca_blk.getUpperBound());
    REQUIRE(bca_blk.getStatistic()  >= bca_blk.getLowerBound());
    REQUIRE(bca_blk.getStatistic()  <= bca_blk.getUpperBound());

    // It's common (not guaranteed) that block bootstrap yields a wider CI than IID when dependence exists.
    // We assert a weak property: both intervals are non-degenerate and the block interval is
    // at least not smaller by a *large* margin. This avoids flakiness while still exercising the path.
    const D wid_iid = bca_iid.getUpperBound() - bca_iid.getLowerBound();
    const D wid_blk = bca_blk.getUpperBound() - bca_blk.getLowerBound();

    REQUIRE(num::to_double(wid_iid) > 0.0);
    REQUIRE(num::to_double(wid_blk) > 0.0);

    // Soft check: block width should not be dramatically smaller than IID width.
    REQUIRE(num::to_double(wid_blk) >= 0.50 * num::to_double(wid_iid));
}

// --------------------------- Annualizer tests (existing) ---------------------------

template<class Decimal>
class MockBCaBootStrapForAnnualizer : public BCaBootStrap<Decimal>
{
public:
    MockBCaBootStrapForAnnualizer()
      : BCaBootStrap<Decimal>(std::vector<Decimal>{Decimal("0.0"), Decimal("0.0")}, 100) {}

    void setTestResults(const Decimal& mean, const Decimal& lower, const Decimal& upper)
    {
        this->setMean(mean);
        this->setLowerBound(lower);
        this->setUpperBound(upper);
        this->m_is_calculated = true;
    }

protected:
    void calculateBCaBounds() override {
        // no-op for mock
    }
};

TEST_CASE("calculateAnnualizationFactor functionality", "[BCaAnnualizer]") {

    SECTION("Standard time frames") {
        REQUIRE(calculateAnnualizationFactor(TimeFrame::DAILY) == Catch::Approx(252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::WEEKLY) == Catch::Approx(52.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::MONTHLY) == Catch::Approx(12.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::QUARTERLY) == Catch::Approx(4.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::YEARLY) == Catch::Approx(1.0));
    }

    SECTION("Intraday time frames with standard US stock market hours") {
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
    }
}

TEST_CASE("BCaAnnualizer functionality", "[BCaAnnualizer]") {

    MockBCaBootStrapForAnnualizer<DecimalType> mock_bca;

    SECTION("Annualizing positive returns") {
        DecimalType per_bar_mean  = createDecimal("0.001");
        DecimalType per_bar_lower = createDecimal("0.0005");
        DecimalType per_bar_upper = createDecimal("0.0015");
        mock_bca.setTestResults(per_bar_mean, per_bar_lower, per_bar_upper);

        double k = 252.0;
        BCaAnnualizer<DecimalType> annualizer(mock_bca, k);

        DecimalType expected_mean  = DecimalType(pow((DecimalType("1.0") + per_bar_mean ).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_lower = DecimalType(pow((DecimalType("1.0") + per_bar_lower).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_upper = DecimalType(pow((DecimalType("1.0") + per_bar_upper).getAsDouble(), k)) - DecimalType("1.0");

        REQUIRE(num::to_double(annualizer.getAnnualizedMean())       == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(num::to_double(annualizer.getAnnualizedLowerBound()) == Catch::Approx(num::to_double(expected_lower)));
        REQUIRE(num::to_double(annualizer.getAnnualizedUpperBound()) == Catch::Approx(num::to_double(expected_upper)));
    }

    SECTION("Annualizing negative returns") {
        DecimalType per_bar_mean  = createDecimal("-0.0005");
        DecimalType per_bar_lower = createDecimal("-0.001");
        DecimalType per_bar_upper = createDecimal("-0.0002");
        mock_bca.setTestResults(per_bar_mean, per_bar_lower, per_bar_upper);

        double k = 252.0;
        BCaAnnualizer<DecimalType> annualizer(mock_bca, k);

        DecimalType expected_mean  = DecimalType(pow((DecimalType("1.0") + per_bar_mean ).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_lower = DecimalType(pow((DecimalType("1.0") + per_bar_lower).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_upper = DecimalType(pow((DecimalType("1.0") + per_bar_upper).getAsDouble(), k)) - DecimalType("1.0");

        REQUIRE(num::to_double(annualizer.getAnnualizedMean())       == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(num::to_double(annualizer.getAnnualizedLowerBound()) == Catch::Approx(num::to_double(expected_lower)));
        REQUIRE(num::to_double(annualizer.getAnnualizedUpperBound()) == Catch::Approx(num::to_double(expected_upper)));
    }

    SECTION("Invalid annualization factor throws exception") {
        mock_bca.setTestResults(createDecimal("0.01"), createDecimal("0.0"), createDecimal("0.02"));
        REQUIRE_THROWS_AS(BCaAnnualizer<DecimalType>(mock_bca, 0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaAnnualizer<DecimalType>(mock_bca, -252.0), std::invalid_argument);
    }
}
