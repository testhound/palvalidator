// BoundedDrawdownsTest.cpp
// Unit tests for the magnitude-only BoundedDrawdowns class
// Uses Catch2

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cmath>

#include "BoundedDrawdowns.h"
#include "BiasCorrectedBootstrap.h" // ensure StationaryBlockResampler is available
#include "TestUtils.h"              // DecimalType, createDecimal
#include "number.h"                 // num::to_double

using namespace mkc_timeseries;

// --------------------------- helpers ---------------------------

template <class D>
double expected_dd_constant_return_decimal(const D& r, int nTrades) {
    const D one = createDecimal("1.0");
    D equity = one;
    for (int i = 0; i < nTrades; ++i) equity *= (one + r);
    // magnitude = 1 - equity since peak remains 1
    return num::to_double(one - equity);
}

template <class D>
double dd_magnitude_from_sequence_decimal(const std::vector<D>& rets) {
    const D one = createDecimal("1.0");
    D equity = one;
    D peak   = one;
    D maxDD  = createDecimal("0.0");
    for (const auto& r : rets) {
        equity *= (one + r);
        if (equity > peak) {
            peak = equity;
        } else {
            const D dd = (peak - equity) / peak; // >= 0
            if (dd > maxDD) maxDD = dd;
        }
    }
    return num::to_double(maxDD);
}

// --------------------------- maxDrawdown (magnitude) tests ---------------------------

TEST_CASE("BoundedDrawdowns::maxDrawdown magnitude basic behavior", "[BoundedDrawdowns][maxDrawdown]") {
    using D = DecimalType;

    SECTION("Empty input returns 0") {
        std::vector<D> x;
        auto dd = BoundedDrawdowns<D>::maxDrawdown(x);
        REQUIRE(num::to_double(dd) == Catch::Approx(0.0));
    }

    SECTION("All non-negative returns -> no drawdown") {
        std::vector<D> x = { createDecimal("0.02"), createDecimal("0.00"), createDecimal("0.03") };
        auto dd = BoundedDrawdowns<D>::maxDrawdown(x);
        REQUIRE(num::to_double(dd) == Catch::Approx(0.0));
    }

    SECTION("Single loss produces that loss as drawdown magnitude") {
        std::vector<D> x = { createDecimal("-0.10") };
        auto dd = BoundedDrawdowns<D>::maxDrawdown(x);
        REQUIRE(num::to_double(dd) == Catch::Approx(0.10));
    }

    SECTION("Rise then fall: +10% then -20% => 20% drawdown from peak") {
        std::vector<D> x = { createDecimal("0.10"), createDecimal("-0.20") };
        auto dd = BoundedDrawdowns<D>::maxDrawdown(x);
        REQUIRE(num::to_double(dd) == Catch::Approx(0.20).epsilon(1e-12));
    }

    SECTION("Multiple peaks and declines (Decimal expected)") {
        // Sequence: +20%, -10%, -10%, +5%, -30%
        std::vector<D> x = {
            createDecimal("0.20"), createDecimal("-0.10"), createDecimal("-0.10"),
            createDecimal("0.05"), createDecimal("-0.30")
        };
        const double expected = dd_magnitude_from_sequence_decimal(x);
        auto dd = BoundedDrawdowns<D>::maxDrawdown(x);
        REQUIRE(num::to_double(dd) == Catch::Approx(expected).epsilon(1e-12));
    }
}

// --------------------------- drawdownFractile (magnitude) tests ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractile deterministic cases", "[BoundedDrawdowns][fractile]") {
    using D = DecimalType;

    SECTION("All-zero returns => zero fractile regardless of settings") {
        std::vector<D> rets = { createDecimal("0.0") }; // single zero => any resample is zeroes
        const int nTrades = 50;
        const int nReps   = 500; // any B
        const double p    = 0.95;

        D q = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);
        REQUIRE(num::to_double(q) == Catch::Approx(0.0));
    }

    SECTION("Single constant negative return gives deterministic fractile (Decimal expected)") {
        const D r        = createDecimal("-0.01"); // -1% each trade
        const int nTrades = 100;
        const int nReps   = 1000; // any B
        const double p    = 0.90; // any p
        std::vector<D> rets = { r }; // single element -> all resamples identical

        const double expected = expected_dd_constant_return_decimal(r, nTrades);
        D q = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);
        REQUIRE(num::to_double(q) == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("Input validation") {
        std::vector<D> empty;
        REQUIRE_THROWS_AS(BoundedDrawdowns<D>::drawdownFractile(empty, 10, 100, 0.5), std::invalid_argument);
        std::vector<D> rets = { createDecimal("0.0") };
        REQUIRE_THROWS_AS(BoundedDrawdowns<D>::drawdownFractile(rets, 0, 100, 0.5), std::invalid_argument);
        REQUIRE_THROWS_AS(BoundedDrawdowns<D>::drawdownFractile(rets, 10, 0,   0.5), std::invalid_argument);
        REQUIRE_THROWS_AS(BoundedDrawdowns<D>::drawdownFractile(rets, 10, 100, -0.1), std::invalid_argument);
        REQUIRE_THROWS_AS(BoundedDrawdowns<D>::drawdownFractile(rets, 10, 100,  1.1), std::invalid_argument);
    }
}

// --------------------------- BCa bounds wrapper tests ---------------------------

TEST_CASE("BoundedDrawdowns::bcaBoundsForDrawdownFractile deterministic cases", "[BoundedDrawdowns][bca]") {
    using D = DecimalType;

    SECTION("Zero returns => degenerate [0,0,0] interval") {
        std::vector<D> rets = { createDecimal("0.0"), createDecimal("0.0") }; // ensure >=2 for BCa
        const unsigned int B = 500;   // bootstrap resamples
        const double cl      = 0.95;  // confidence level
        const int nTrades    = 40;
        const int nReps      = 200;   // MC reps inside statistic
        const double p       = 0.95;  // dd fractile

        auto res = BoundedDrawdowns<D>::bcaBoundsForDrawdownFractile(rets, B, cl, nTrades, nReps, p, /*L=*/3);

        REQUIRE(num::to_double(res.statistic)  == Catch::Approx(0.0));
        REQUIRE(num::to_double(res.lowerBound) == Catch::Approx(0.0));
        REQUIRE(num::to_double(res.upperBound) == Catch::Approx(0.0));
    }

    SECTION("Two constant negative returns => degenerate interval at known value (Decimal expected)") {
        const D r = createDecimal("-0.005"); // -0.5% each trade
        std::vector<D> rets = { r, r };       // ensure >=2 for BCa
        const unsigned int B = 800;  // bootstrap resamples
        const double cl      = 0.95;
        const int nTrades    = 120;
        const int nReps      = 300;  // MC reps inside statistic
        const double p       = 0.90;

        const double expected = expected_dd_constant_return_decimal(r, nTrades);

        auto res = BoundedDrawdowns<D>::bcaBoundsForDrawdownFractile(rets, B, cl, nTrades, nReps, p, /*L=*/3);

        // All three should match the deterministic statistic (within conversion noise)
        REQUIRE(num::to_double(res.statistic)  == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(res.lowerBound) == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(res.upperBound) == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("Basic sanity: bounds ordered and contain statistic (non-degenerate case)") {
        // Mixed returns to exercise a non-degenerate path; Monte-Carlo inside is random,
        // so we only assert order-based properties.
        std::vector<D> rets = {
            createDecimal("0.01"), createDecimal("-0.02"), createDecimal("0.03"),
            createDecimal("0.015"), createDecimal("-0.005"), createDecimal("0.025"),
            createDecimal("0.01"), createDecimal("0.00"), createDecimal("-0.01"),
            createDecimal("0.02")
        };

        const unsigned int B = 1200;
        const double cl      = 0.90;
        const int nTrades    = 30;
        const int nReps      = 400;
        const double p       = 0.95;

        auto res = BoundedDrawdowns<D>::bcaBoundsForDrawdownFractile(rets, B, cl, nTrades, nReps, p, /*L=*/3);

        REQUIRE(res.lowerBound <= res.upperBound);
        REQUIRE(res.statistic  >= res.lowerBound);
        REQUIRE(res.statistic  <= res.upperBound);

        // Intervals should be non-degenerate with high probability
        REQUIRE(num::to_double(res.upperBound - res.lowerBound) > 0.0);
    }
}
