// BoundedDrawdownsTest.cpp
// Unit tests for the magnitude-only BoundedDrawdowns class
// Uses Catch2

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cmath>

#include "TradeResampling.h"
#include "BoundedDrawdowns.h"
#include "BiasCorrectedBootstrap.h"
#include "TestUtils.h"
#include "number.h"
#include "BootstrapTypes.h"

using namespace mkc_timeseries;

// --------------------------- helpers ---------------------------

/**
 * @brief Constructs a Trade<D> from an initializer-style vector of string literals.
 *
 * Each string is passed to createDecimal() so the helper integrates with the
 * same decimal infrastructure used everywhere else in these test files.
 *
 * Example:
 *   auto t = makeTrade<D>({"-0.01", "0.02", "-0.005"});
 *   // t.getDailyReturns() == { -0.01, 0.02, -0.005 }
 */
template <class D>
mkc_timeseries::Trade<D> makeTrade(const std::vector<const char*>& returnStrs)
{
    std::vector<D> rets;
    rets.reserve(returnStrs.size());
    for (const char* s : returnStrs)
        rets.push_back(createDecimal(s));
    return mkc_timeseries::Trade<D>(std::move(rets));
}

/**
 * @brief Replicates the maxDrawdown walk over a flat return sequence.
 *
 * Used to compute analytic expected values for hand-crafted trade sequences
 * without relying on the code under test.
 */
template <class D>
double dd_from_flat_sequence(const std::vector<D>& flat)
{
    const D one = createDecimal("1.0");
    D equity = one;
    D peak   = one;
    D maxDD  = createDecimal("0.0");
    for (const auto& r : flat)
    {
        equity *= (one + r);
        if (equity > peak)
            peak = equity;
        else
        {
            const D dd = (peak - equity) / peak;
            if (dd > maxDD) maxDD = dd;
        }
    }
    return num::to_double(maxDD);
}

template <class D>
double expected_dd_constant_return_decimal(const D& r, int nTrades) {
    const D one = createDecimal("1.0");
    D equity = one;
    for (int i = 0; i < nTrades; ++i) equity *= (one + r);
    // magnitude = 1 - equity since peak remains 1
    return num::to_double(one - equity);
}

/**
 * @brief Deterministic expected max drawdown for nTrades of the same constant return.
 *
 * When r < 0 the equity falls monotonically from 1, so the drawdown magnitude
 * equals 1 - (1+r)^nTrades.  When r >= 0 there is no drawdown (returns 0).
 */
template <class D>
double expected_dd_constant_return(const D& r, int nTrades)
{
    const D one = createDecimal("1.0");
    if (r >= createDecimal("0.0")) return 0.0;
    D equity = one;
    for (int i = 0; i < nTrades; ++i)
        equity *= (one + r);
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

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary basic behavior", "[BoundedDrawdowns][stationary][fractile]") {
    using D = DecimalType;

    SECTION("Input validation") {
        std::vector<D> empty;
        REQUIRE_THROWS_AS(
            BoundedDrawdowns<D>::drawdownFractileStationary(empty, 10, 100, 0.5, 3),
            std::invalid_argument
        );
        
        std::vector<D> rets = { createDecimal("0.0") };
        REQUIRE_THROWS_AS(
            BoundedDrawdowns<D>::drawdownFractileStationary(rets, 0, 100, 0.5, 3),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            BoundedDrawdowns<D>::drawdownFractileStationary(rets, 10, 0, 0.5, 3),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            BoundedDrawdowns<D>::drawdownFractileStationary(rets, 10, 100, -0.1, 3),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            BoundedDrawdowns<D>::drawdownFractileStationary(rets, 10, 100, 1.1, 3),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            BoundedDrawdowns<D>::drawdownFractileStationary(rets, 10, 100, 0.5, 0),
            std::invalid_argument
        );
    }

    SECTION("All-zero returns => zero fractile regardless of settings") {
        std::vector<D> rets = { createDecimal("0.0"), createDecimal("0.0") };
        const int nTrades = 50;
        const int nReps   = 500;
        const double p    = 0.95;
        const std::size_t L = 3;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) == Catch::Approx(0.0));
    }

    SECTION("All non-negative returns => zero or very small fractile") {
        std::vector<D> rets = {
            createDecimal("0.02"), createDecimal("0.01"), createDecimal("0.03"),
            createDecimal("0.015"), createDecimal("0.025")
        };
        const int nTrades = 40;
        const int nReps   = 800;
        const double p    = 0.90;
        const std::size_t L = 3;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        // With all positive returns, max drawdown should be essentially 0
        REQUIRE(num::to_double(q) == Catch::Approx(0.0).margin(0.001));
    }
}

// --------------------------- fallback to IID behavior tests ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary fallback to IID", "[BoundedDrawdowns][stationary][fallback]") {
    using D = DecimalType;

    SECTION("Single-element returns vector falls back to IID (drawdownFractile)") {
        const D r = createDecimal("-0.01");
        std::vector<D> rets = { r }; // size == 1, should trigger fallback
        const int nTrades = 100;
        const int nReps   = 1000;
        const double p    = 0.90;
        const std::size_t L = 5;

        const double expected = expected_dd_constant_return_decimal(r, nTrades);
        
        D q_stationary = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        D q_iid = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);
        
        // Both should give the same deterministic result
        REQUIRE(num::to_double(q_stationary) == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(q_iid) == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(q_stationary) == Catch::Approx(num::to_double(q_iid)).epsilon(1e-12));
    }

    SECTION("nTrades == 1 falls back to IID") {
        std::vector<D> rets = {
            createDecimal("-0.02"), createDecimal("-0.01"), createDecimal("0.03")
        };
        const int nTrades = 1; // should trigger fallback
        const int nReps   = 500;
        const double p    = 0.95;
        const std::size_t L = 3;

        D q_stationary = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        D q_iid = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);
        
        // With 1 trade, both methods should sample single returns and find max DD from that
        // Results should be identical (both use same IID path when nTrades == 1)
        REQUIRE(num::to_double(q_stationary) == Catch::Approx(num::to_double(q_iid)).epsilon(1e-10));
    }

    SECTION("Both returns.size() < 2 AND nTrades < 2 falls back") {
        const D r = createDecimal("-0.005");
        std::vector<D> rets = { r }; // size == 1
        const int nTrades = 1; // also == 1
        const int nReps   = 300;
        const double p    = 0.90;
        const std::size_t L = 3;

        // Single trade with single return is completely deterministic
        const double expected = num::to_double(DecimalConstants<D>::DecimalZero - r);
        if (expected < 0.0) {
            // If return is positive, dd should be 0
            REQUIRE(expected == Catch::Approx(0.0));
        }
        
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) == Catch::Approx(std::max(0.0, expected)).epsilon(1e-12));
    }
}

// --------------------------- deterministic cases ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary deterministic cases", "[BoundedDrawdowns][stationary][deterministic]") {
    using D = DecimalType;

    SECTION("Two identical constant returns => deterministic fractile") {
        const D r = createDecimal("-0.005"); // -0.5% each trade
        std::vector<D> rets = { r, r }; // ensure >= 2 for stationary resampling
        const int nTrades = 120;
        const int nReps   = 800;
        const double p    = 0.90;
        const std::size_t L = 3;

        const double expected = expected_dd_constant_return_decimal(r, nTrades);
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        
        // All resamples will produce identical sequences, so fractile == deterministic value
        REQUIRE(num::to_double(q) == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("Multiple identical constant negative returns") {
        const D r = createDecimal("-0.01");
        std::vector<D> rets = { r, r, r, r }; // all identical
        const int nTrades = 80;
        const int nReps   = 1000;
        const double p    = 0.95;
        const std::size_t L = 4;

        const double expected = expected_dd_constant_return_decimal(r, nTrades);
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        
        REQUIRE(num::to_double(q) == Catch::Approx(expected).epsilon(1e-12));
    }
}

// --------------------------- mean block length variation ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary with varying block lengths", "[BoundedDrawdowns][stationary][blocklen]") {
    using D = DecimalType;

    SECTION("Results are reasonable across different mean block lengths") {
        std::vector<D> rets = {
            createDecimal("0.01"), createDecimal("-0.02"), createDecimal("0.03"),
            createDecimal("0.015"), createDecimal("-0.005"), createDecimal("0.025"),
            createDecimal("-0.01"), createDecimal("0.02"), createDecimal("-0.015"),
            createDecimal("0.01")
        };
        const int nTrades = 60;
        const int nReps   = 1000;
        const double p    = 0.95;

        // Test with different block lengths
        D q_L1 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, 1);
        D q_L3 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, 3);
        D q_L5 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, 5);
        D q_L10 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, 10);

        // All results should be non-negative
        REQUIRE(num::to_double(q_L1) >= 0.0);
        REQUIRE(num::to_double(q_L3) >= 0.0);
        REQUIRE(num::to_double(q_L5) >= 0.0);
        REQUIRE(num::to_double(q_L10) >= 0.0);

        // L=1 should behave like IID (random restarts every position)
        // Larger L should preserve more dependence structure
        // We can't assert exact ordering due to Monte Carlo variance,
        // but we can verify results are in a reasonable range
        
        // For comparison, get the IID result
        D q_iid = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);
        
        // L=1 should be close to IID (though not identical due to implementation differences)
        // Just verify it's in a reasonable range
        const double ratio_L1_to_iid = num::to_double(q_L1) / std::max(1e-10, num::to_double(q_iid));
        REQUIRE(ratio_L1_to_iid > 0.5);  // Not too different
        REQUIRE(ratio_L1_to_iid < 2.0);  // Not too different
    }

    SECTION("meanBlockLength = returns.size() creates one long block per resample") {
        std::vector<D> rets = {
            createDecimal("-0.01"), createDecimal("0.02"), createDecimal("-0.015"),
            createDecimal("0.01"), createDecimal("-0.005")
        };
        const int nTrades = 30;
        const int nReps   = 800;
        const double p    = 0.90;
        const std::size_t L = rets.size(); // one block length = entire series

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        
        // Should still produce valid results
        REQUIRE(num::to_double(q) >= 0.0);
        
        // Result should be reasonable (hard to test exact value due to randomness,
        // but it should be in a plausible range)
        REQUIRE(num::to_double(q) < 1.0); // Not catastrophic
    }
}

// --------------------------- comparison with non-stationary version ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary vs non-stationary", "[BoundedDrawdowns][stationary][comparison]") {
    using D = DecimalType;

    SECTION("Both methods produce non-negative results") {
        std::vector<D> rets = {
            createDecimal("0.02"), createDecimal("-0.03"), createDecimal("0.01"),
            createDecimal("-0.015"), createDecimal("0.025"), createDecimal("-0.01"),
            createDecimal("0.015"), createDecimal("-0.005")
        };
        const int nTrades = 50;
        const int nReps   = 1000;
        const double p    = 0.95;
        const std::size_t L = 3;

        D q_stationary = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        D q_iid = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);

        REQUIRE(num::to_double(q_stationary) >= 0.0);
        REQUIRE(num::to_double(q_iid) >= 0.0);
        
        // Both should be in a reasonable range (not wildly different)
        // Due to Monte Carlo variance and different resampling strategies,
        // we just verify they're both plausible
        REQUIRE(num::to_double(q_stationary) < 1.0);
        REQUIRE(num::to_double(q_iid) < 1.0);
    }

    SECTION("High autocorrelation: stationary should preserve structure better") {
        // Create a series with strong autocorrelation (alternating runs)
        std::vector<D> rets = {
            createDecimal("-0.02"), createDecimal("-0.02"), createDecimal("-0.02"),
            createDecimal("0.03"), createDecimal("0.03"), createDecimal("0.03"),
            createDecimal("-0.015"), createDecimal("-0.015"), createDecimal("-0.015"),
            createDecimal("0.02"), createDecimal("0.02"), createDecimal("0.02")
        };
        const int nTrades = 60;
        const int nReps   = 1500;
        const double p    = 0.90;
        const std::size_t L = 3; // matches run length

        D q_stationary = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        D q_iid = BoundedDrawdowns<D>::drawdownFractile(rets, nTrades, nReps, p);

        // Both should be valid
        REQUIRE(num::to_double(q_stationary) >= 0.0);
        REQUIRE(num::to_double(q_iid) >= 0.0);
        
        // With structured data, stationary bootstrap should preserve runs,
        // potentially leading to different (often larger) drawdowns than IID
        // But we can't assert a specific ordering without many more reps
        // Just verify both are reasonable
        REQUIRE(num::to_double(q_stationary) < 1.0);
        REQUIRE(num::to_double(q_iid) < 1.0);
    }
}

// --------------------------- edge cases for fractile parameter ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary fractile parameter edge cases", "[BoundedDrawdowns][stationary][fractile]") {
    using D = DecimalType;

    std::vector<D> rets = {
        createDecimal("-0.01"), createDecimal("0.02"), createDecimal("-0.015"),
        createDecimal("0.01"), createDecimal("-0.005"), createDecimal("0.015")
    };
    const int nTrades = 40;
    const int nReps   = 800;
    const std::size_t L = 3;

    SECTION("p = 0.0 returns minimum drawdown") {
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, 0.0, L);
        REQUIRE(num::to_double(q) >= 0.0);
        // Should be the minimum (or close to it)
    }

    SECTION("p = 1.0 returns maximum drawdown") {
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, 1.0, L);
        REQUIRE(num::to_double(q) >= 0.0);
        // Should be the maximum (or close to it)
    }

    SECTION("p = 0.5 returns median drawdown") {
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, 0.5, L);
        REQUIRE(num::to_double(q) >= 0.0);
    }

    SECTION("Fractiles should be ordered: 0.1 < 0.5 < 0.9") {
        D q_10 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, 0.1, L);
        D q_50 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, 0.5, L);
        D q_90 = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, 0.9, L);

        // Allow for some Monte Carlo variance, but generally should be ordered
        REQUIRE(num::to_double(q_10) <= num::to_double(q_90) * 1.1); // Allow 10% tolerance
        REQUIRE(num::to_double(q_50) <= num::to_double(q_90) * 1.05);
    }
}

// --------------------------- stress tests with larger data ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary stress tests", "[BoundedDrawdowns][stationary][stress]") {
    using D = DecimalType;

    SECTION("Large number of trades") {
        std::vector<D> rets = {
            createDecimal("-0.01"), createDecimal("0.015"), createDecimal("-0.02"),
            createDecimal("0.025"), createDecimal("-0.005"), createDecimal("0.01")
        };
        const int nTrades = 500; // large path
        const int nReps   = 500;
        const double p    = 0.95;
        const std::size_t L = 4;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) >= 0.0);
        REQUIRE(num::to_double(q) < 1.0);
    }

    SECTION("Large number of Monte Carlo reps") {
        std::vector<D> rets = {
            createDecimal("0.01"), createDecimal("-0.02"), createDecimal("0.015"),
            createDecimal("-0.01"), createDecimal("0.02")
        };
        const int nTrades = 40;
        const int nReps   = 5000; // many reps for stability
        const double p    = 0.95;
        const std::size_t L = 3;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) >= 0.0);
        REQUIRE(num::to_double(q) < 1.0);
    }

    SECTION("Very large mean block length (>> data length)") {
        std::vector<D> rets = {
            createDecimal("-0.01"), createDecimal("0.02"), createDecimal("-0.015"),
            createDecimal("0.01")
        };
        const int nTrades = 30;
        const int nReps   = 500;
        const double p    = 0.90;
        const std::size_t L = 1000; // much larger than data

        // Should still work (essentially one block per resample)
        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) >= 0.0);
        REQUIRE(num::to_double(q) < 1.0);
    }
}

// --------------------------- sanity check: results should vary with more losses ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary sanity check", "[BoundedDrawdowns][stationary][sanity]") {
    using D = DecimalType;

    SECTION("More negative returns should generally lead to larger drawdowns") {
        std::vector<D> mild_losses = {
            createDecimal("-0.005"), createDecimal("0.01"), createDecimal("-0.003"),
            createDecimal("0.008"), createDecimal("-0.002")
        };
        std::vector<D> severe_losses = {
            createDecimal("-0.03"), createDecimal("0.01"), createDecimal("-0.02"),
            createDecimal("0.008"), createDecimal("-0.025")
        };
        
        const int nTrades = 50;
        const int nReps   = 1000;
        const double p    = 0.95;
        const std::size_t L = 3;

        D q_mild = BoundedDrawdowns<D>::drawdownFractileStationary(mild_losses, nTrades, nReps, p, L);
        D q_severe = BoundedDrawdowns<D>::drawdownFractileStationary(severe_losses, nTrades, nReps, p, L);

        // Severe losses should generally produce larger drawdowns
        // (allowing for Monte Carlo variance)
        REQUIRE(num::to_double(q_severe) > num::to_double(q_mild) * 0.5);
    }
}

// --------------------------- minimum viable inputs ---------------------------

TEST_CASE("BoundedDrawdowns::drawdownFractileStationary minimum viable inputs", "[BoundedDrawdowns][stationary][minimal]") {
    using D = DecimalType;

    SECTION("Exactly 2 returns, exactly 2 trades, L=1") {
        std::vector<D> rets = { createDecimal("-0.01"), createDecimal("0.02") };
        const int nTrades = 2;
        const int nReps   = 100;
        const double p    = 0.90;
        const std::size_t L = 1;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) >= 0.0);
    }

    SECTION("Exactly 2 returns, exactly 2 trades, L=2") {
        std::vector<D> rets = { createDecimal("-0.01"), createDecimal("0.02") };
        const int nTrades = 2;
        const int nReps   = 100;
        const double p    = 0.50;
        const std::size_t L = 2;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) >= 0.0);
    }

    SECTION("Minimum viable: 2 returns, 2 trades, 1 rep, L=1") {
        std::vector<D> rets = { createDecimal("-0.02"), createDecimal("0.01") };
        const int nTrades = 2;
        const int nReps   = 1; // minimum
        const double p    = 0.0; // will select first (and only) sample
        const std::size_t L = 1;

        D q = BoundedDrawdowns<D>::drawdownFractileStationary(rets, nTrades, nReps, p, L);
        REQUIRE(num::to_double(q) >= 0.0);
        REQUIRE(num::to_double(q) < 1.0);
    }
}

// =============================================================================
// maxDrawdown(vector<Trade<D>>) — value overload
// =============================================================================

TEST_CASE("BoundedDrawdowns::maxDrawdown(Trade) basic behavior",
          "[BoundedDrawdowns][maxDrawdown][Trade]")
{
    using D = DecimalType;

    SECTION("Empty trade vector returns zero")
    {
        std::vector<mkc_timeseries::Trade<D>> trades;
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.0));
    }

    SECTION("Single trade, single positive return -> no drawdown")
    {
        // Equity goes from 1 -> 1.05, never falls below peak.
        std::vector<mkc_timeseries::Trade<D>> trades = { makeTrade<D>({"0.05"}) };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.0));
    }

    SECTION("Single trade, single negative return -> magnitude equals that loss")
    {
        // Equity goes from 1 -> 0.90, so drawdown = (1 - 0.90) / 1 = 0.10.
        std::vector<mkc_timeseries::Trade<D>> trades = { makeTrade<D>({"-0.10"}) };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.10).epsilon(1e-12));
    }

    SECTION("Single trade, multiple positive returns -> no drawdown")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.02", "0.03", "0.01"})
        };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.0));
    }

    SECTION("Single trade with intra-trade peak-to-trough: +5% then -10%")
    {
        // After +5%: equity=1.05 (new peak).  After -10%: equity=1.05*0.90=0.945.
        // DD = (1.05 - 0.945) / 1.05 = 0.10.
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.05", "-0.10"})
        };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.10).epsilon(1e-12));
    }

    SECTION("Equity curve is continuous across trade boundaries")
    {
        // trade1: +10%  -> equity peaks at 1.10
        // trade2: -20%  -> equity = 1.10 * 0.80 = 0.88
        // DD = (1.10 - 0.88) / 1.10 = 0.22 / 1.10 = 0.20
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.10"}),
            makeTrade<D>({"-0.20"})
        };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.20).epsilon(1e-12));
    }

    SECTION("Largest drawdown can span multiple trades")
    {
        // trade1: +20%, +5%  -> equity=1.26, peak=1.26
        // trade2: -15%, -10% -> equity=1.26*0.85*0.90 = 0.9639
        // trade3: +30%       -> equity=1.253 (new peak after recovery)
        // trade4: -25%       -> equity=0.940
        // First trough: DD = (1.26 - 0.9639) / 1.26 ≈ 0.2350
        // Second trough off second peak: DD = (1.253 - 0.940) / 1.253 ≈ 0.2498
        // Expected: maximum of the two
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.20", "0.05"}),
            makeTrade<D>({"-0.15", "-0.10"}),
            makeTrade<D>({"0.30"}),
            makeTrade<D>({"-0.25"})
        };

        // Build the flat sequence and compute analytically via the helper.
        std::vector<D> flat = {
            createDecimal("0.20"), createDecimal("0.05"),
            createDecimal("-0.15"), createDecimal("-0.10"),
            createDecimal("0.30"),
            createDecimal("-0.25")
        };
        const double expected = dd_from_flat_sequence(flat);

        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("All-zero daily returns produce zero drawdown")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.0", "0.0"}),
            makeTrade<D>({"0.0"})
        };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades))
                == Catch::Approx(0.0));
    }
}


// =============================================================================
// maxDrawdown(vector<const Trade<D>*>) — pointer overload
// =============================================================================

TEST_CASE("BoundedDrawdowns::maxDrawdown(Trade*) pointer overload",
          "[BoundedDrawdowns][maxDrawdown][TradePtr]")
{
    using D = DecimalType;

    SECTION("Empty pointer vector returns zero")
    {
        std::vector<const mkc_timeseries::Trade<D>*> ptrs;
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(ptrs))
                == Catch::Approx(0.0));
    }

    SECTION("Pointer overload produces the same result as value overload")
    {
        // Build a set of trades and compare both overloads side by side.
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.10"}),
            makeTrade<D>({"-0.20"}),
            makeTrade<D>({"0.05", "-0.08"}),
            makeTrade<D>({"0.03"})
        };

        // Value overload
        const double dd_value =
            num::to_double(BoundedDrawdowns<D>::maxDrawdown(trades));

        // Pointer overload — build pointer vector from the same trades
        std::vector<const mkc_timeseries::Trade<D>*> ptrs;
        for (const auto& t : trades) ptrs.push_back(&t);
        const double dd_ptr =
            num::to_double(BoundedDrawdowns<D>::maxDrawdown(ptrs));

        REQUIRE(dd_ptr == Catch::Approx(dd_value).epsilon(1e-12));
    }

    SECTION("Single trade pointer: known drawdown")
    {
        // Identical to the value overload single-trade test; verifies pointer
        // dereference path specifically.
        auto trade = makeTrade<D>({"-0.10"});
        std::vector<const mkc_timeseries::Trade<D>*> ptrs = { &trade };
        REQUIRE(num::to_double(BoundedDrawdowns<D>::maxDrawdown(ptrs))
                == Catch::Approx(0.10).epsilon(1e-12));
    }
}


// =============================================================================
// drawdownFractile(vector<Trade<D>>, ...) — input validation
// =============================================================================

TEST_CASE("BoundedDrawdowns::drawdownFractile(Trade) input validation",
          "[BoundedDrawdowns][fractile][Trade][validation]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    SECTION("Empty trade vector throws invalid_argument")
    {
        std::vector<mkc_timeseries::Trade<D>> empty;
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(empty, 10, 100, 0.95, exec),
            std::invalid_argument);
    }

    SECTION("nTrades <= 0 throws invalid_argument")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = { makeTrade<D>({"0.01"}) };
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(trades, 0, 100, 0.95, exec),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(trades, -1, 100, 0.95, exec),
            std::invalid_argument);
    }

    SECTION("nReps <= 0 throws invalid_argument")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = { makeTrade<D>({"0.01"}) };
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(trades, 10, 0, 0.95, exec),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(trades, 10, -1, 0.95, exec),
            std::invalid_argument);
    }

    SECTION("ddConf out of [0,1] throws invalid_argument")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = { makeTrade<D>({"0.01"}) };
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(trades, 10, 100, -0.01, exec),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            BD::drawdownFractile(trades, 10, 100,  1.01, exec),
            std::invalid_argument);
    }
}


// =============================================================================
// drawdownFractile(vector<Trade<D>>, ...) — deterministic cases
// =============================================================================

TEST_CASE("BoundedDrawdowns::drawdownFractile(Trade) deterministic cases",
          "[BoundedDrawdowns][fractile][Trade][deterministic]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    SECTION("All-zero daily returns -> zero fractile regardless of parameters")
    {
        // Every resample of the pool will be a zero-return trade, so every
        // max drawdown is zero, and every fractile is zero.
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.0", "0.0"}),
            makeTrade<D>({"0.0"})
        };
        D q = BD::drawdownFractile(trades, 50, 500, 0.95, exec);
        REQUIRE(num::to_double(q) == Catch::Approx(0.0));
    }

    SECTION("Pool of one trade with constant negative single-bar return: deterministic result")
    {
        // Pool contains exactly one trade whose only daily return is -1%.
        // Every resample must pick that trade nTrades times.
        // Each trade contributes one bar of -1%, so the equity walks down
        // as (0.99)^nTrades giving a deterministic max drawdown of
        // 1 - (0.99)^nTrades.
        const D r = createDecimal("-0.01");
        const int nTrades = 100;

        std::vector<mkc_timeseries::Trade<D>> trades = { makeTrade<D>({"-0.01"}) };

        const double expected = expected_dd_constant_return(r, nTrades);
        D q = BD::drawdownFractile(
            trades, nTrades, /*nReps=*/1000, /*ddConf=*/0.90, exec);

        REQUIRE(num::to_double(q) == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("Pool of one trade with multi-bar constant negative return: deterministic result")
    {
        // Each trade in the pool has two daily bars of -0.5%.
        // Every resample of nTrades=50 picks the same trade each time.
        // Flat sequence = 100 bars of -0.5%, deterministic DD = 1-(0.995)^100.
        const D r = createDecimal("-0.005");
        const int barsPerTrade = 2;
        const int nTrades = 50;

        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"-0.005", "-0.005"})
        };

        // Expected: nTrades * barsPerTrade bars of r, monotonically falling.
        const double expected =
            expected_dd_constant_return(r, nTrades * barsPerTrade);

        D q = BD::drawdownFractile(
            trades, nTrades, /*nReps=*/800, /*ddConf=*/0.90, exec);

        REQUIRE(num::to_double(q) == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("All-positive single-bar trades -> zero fractile")
    {
        // No resample can ever produce a drawdown because all returns are positive.
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.01"}),
            makeTrade<D>({"0.02"}),
            makeTrade<D>({"0.005"})
        };
        D q = BD::drawdownFractile(
            trades, 30, 500, 0.95, exec);
        REQUIRE(num::to_double(q) == Catch::Approx(0.0));
    }
}


// =============================================================================
// drawdownFractile(vector<Trade<D>>, ...) — sanity / ordering checks
// =============================================================================

TEST_CASE("BoundedDrawdowns::drawdownFractile(Trade) sanity checks",
          "[BoundedDrawdowns][fractile][Trade][sanity]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    SECTION("Result is non-negative and < 1 for realistic mixed-return trades")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.02", "-0.01"}),
            makeTrade<D>({"-0.03", "0.01", "0.02"}),
            makeTrade<D>({"0.01"}),
            makeTrade<D>({"-0.02", "-0.01"}),
            makeTrade<D>({"0.03", "0.01"})
        };
        D q = BD::drawdownFractile(
            trades, 40, 600, 0.95, exec);
        REQUIRE(num::to_double(q) >= 0.0);
        REQUIRE(num::to_double(q) <  1.0);
    }

    SECTION("Higher fractile >= lower fractile (ordering property)")
    {
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.01", "-0.02"}),
            makeTrade<D>({"-0.015", "0.01"}),
            makeTrade<D>({"0.02"}),
            makeTrade<D>({"-0.01", "-0.005"}),
            makeTrade<D>({"0.015", "-0.01"})
        };

        D q50 = BD::drawdownFractile(trades, 40, 1000, 0.50, exec);
        D q95 = BD::drawdownFractile(trades, 40, 1000, 0.95, exec);

        // The 95th percentile drawdown must be >= the 50th percentile drawdown.
        REQUIRE(num::to_double(q95) >= num::to_double(q50));
    }

    SECTION("Trades with larger losses produce larger drawdown fractile")
    {
        std::vector<mkc_timeseries::Trade<D>> mild = {
            makeTrade<D>({"0.01", "-0.005"}),
            makeTrade<D>({"-0.003", "0.008"}),
            makeTrade<D>({"0.005"})
        };
        std::vector<mkc_timeseries::Trade<D>> severe = {
            makeTrade<D>({"0.01", "-0.04"}),
            makeTrade<D>({"-0.03", "0.008"}),
            makeTrade<D>({"-0.02"})
        };

        D q_mild   = BD::drawdownFractile(mild,   40, 800, 0.95, exec);
        D q_severe = BD::drawdownFractile(severe, 40, 800, 0.95, exec);

        // Severe pool should generally produce larger drawdowns.
        // Use a conservative ratio check rather than strict > to absorb MC variance.
        REQUIRE(num::to_double(q_severe) > num::to_double(q_mild) * 0.5);
    }
}


// =============================================================================
// bcaBoundsForDrawdownFractile(vector<Trade<D>>, ...) — degenerate cases
// =============================================================================

TEST_CASE("BoundedDrawdowns::bcaBoundsForDrawdownFractile(Trade) degenerate cases",
          "[BoundedDrawdowns][bca][Trade][degenerate]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    SECTION("All-zero daily returns -> degenerate [0, 0, 0] interval")
    {
        // Every bootstrap resample evaluates to zero drawdown; BCa produces a
        // collapsed interval.
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"0.0", "0.0"}),
            makeTrade<D>({"0.0"})
        };

        auto res = BD::bcaBoundsForDrawdownFractile(
            trades,
            /*numResamples=*/500,
            /*confidenceLevel=*/0.95,
            /*nTrades=*/40,
            /*nReps=*/200,
            /*ddConf=*/0.95,
            exec,
            IntervalType::ONE_SIDED_UPPER);

        REQUIRE(num::to_double(res.statistic)  == Catch::Approx(0.0));
        REQUIRE(num::to_double(res.lowerBound) == Catch::Approx(0.0));
        REQUIRE(num::to_double(res.upperBound) == Catch::Approx(0.0));
    }

    SECTION("Pool of two identical constant-return trades -> degenerate interval at known value")
    {
        // With a homogeneous pool the bootstrap distribution is degenerate:
        // every resample gives the same statistic, so all three outputs collapse
        // to that single deterministic value.
        const D r = createDecimal("-0.005");
        const int nTrades = 80;

        // Two identical trades, each with one bar of -0.5%.
        std::vector<mkc_timeseries::Trade<D>> trades = {
            makeTrade<D>({"-0.005"}),
            makeTrade<D>({"-0.005"})
        };

        const double expected = expected_dd_constant_return(r, nTrades);

        auto res = BD::bcaBoundsForDrawdownFractile(
            trades,
            /*numResamples=*/600,
            /*confidenceLevel=*/0.95,
            /*nTrades=*/nTrades,
            /*nReps=*/300,
            /*ddConf=*/0.90,
            exec,
            IntervalType::ONE_SIDED_UPPER);

        REQUIRE(num::to_double(res.statistic)  == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(res.lowerBound) == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(res.upperBound) == Catch::Approx(expected).epsilon(1e-12));
    }
}


// =============================================================================
// bcaBoundsForDrawdownFractile(vector<Trade<D>>, ...) — structural properties
// =============================================================================

TEST_CASE("BoundedDrawdowns::bcaBoundsForDrawdownFractile(Trade) structural properties",
          "[BoundedDrawdowns][bca][Trade][structure]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    // A realistic mixed-return trade pool used across multiple sections.
    std::vector<mkc_timeseries::Trade<D>> trades = {
        makeTrade<D>({"0.02",  "-0.01", "0.015"}),
        makeTrade<D>({"-0.02", "0.03"}),
        makeTrade<D>({"0.01"}),
        makeTrade<D>({"-0.015","0.01",  "-0.005"}),
        makeTrade<D>({"0.025", "-0.01"}),
        makeTrade<D>({"-0.01", "-0.008","0.02"}),
        makeTrade<D>({"0.01",  "0.005"}),
        makeTrade<D>({"-0.03", "0.02"})
    };

    SECTION("Bounds ordered and contain statistic (ONE_SIDED_UPPER)")
    {
        auto res = BD::bcaBoundsForDrawdownFractile(
            trades, 800, 0.95, 50, 400, 0.95, exec,
            IntervalType::ONE_SIDED_UPPER);

        REQUIRE(res.lowerBound <= res.upperBound);
        REQUIRE(res.statistic  >= res.lowerBound);
        REQUIRE(res.statistic  <= res.upperBound);
    }

    SECTION("All output values are finite and non-negative (drawdown magnitudes)")
    {
        auto res = BD::bcaBoundsForDrawdownFractile(
            trades, 700, 0.95, 50, 400, 0.95, exec,
            IntervalType::ONE_SIDED_UPPER);

        REQUIRE(std::isfinite(num::to_double(res.statistic)));
        REQUIRE(std::isfinite(num::to_double(res.lowerBound)));
        REQUIRE(std::isfinite(num::to_double(res.upperBound)));

        REQUIRE(num::to_double(res.statistic)  >= 0.0);
        REQUIRE(num::to_double(res.lowerBound) >= 0.0);
        REQUIRE(num::to_double(res.upperBound) >= 0.0);
    }

    SECTION("Interval is non-degenerate with high probability for mixed returns")
    {
        auto res = BD::bcaBoundsForDrawdownFractile(
            trades, 900, 0.95, 50, 400, 0.95, exec,
            IntervalType::ONE_SIDED_UPPER);

        REQUIRE(num::to_double(res.upperBound - res.lowerBound) > 0.0);
    }

    SECTION("Upper bound >= statistic for ONE_SIDED_UPPER")
    {
        auto res = BD::bcaBoundsForDrawdownFractile(
            trades, 700, 0.95, 50, 400, 0.95, exec,
            IntervalType::ONE_SIDED_UPPER);

        REQUIRE(num::to_double(res.upperBound) >= num::to_double(res.statistic));
    }

    SECTION("TWO_SIDED ordering (lower <= statistic <= upper)")
    {
        auto res = BD::bcaBoundsForDrawdownFractile(
            trades, 700, 0.90, 50, 400, 0.95, exec,
            IntervalType::TWO_SIDED);

        REQUIRE(res.lowerBound <= res.upperBound);
        REQUIRE(res.statistic  >= res.lowerBound);
        REQUIRE(res.statistic  <= res.upperBound);
    }
}


// =============================================================================
// bcaBoundsForDrawdownFractile(vector<Trade<D>>, ...) — confidence level monotonicity
// =============================================================================

TEST_CASE("BoundedDrawdowns::bcaBoundsForDrawdownFractile(Trade) confidence level monotonicity",
          "[BoundedDrawdowns][bca][Trade][confidence]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    std::vector<mkc_timeseries::Trade<D>> trades = {
        makeTrade<D>({"0.01",  "-0.02"}),
        makeTrade<D>({"-0.03", "0.015"}),
        makeTrade<D>({"0.02",  "-0.01"}),
        makeTrade<D>({"-0.01", "0.025"}),
        makeTrade<D>({"0.015", "-0.015"}),
        makeTrade<D>({"-0.02", "0.01"})
    };

    auto res90 = BD::bcaBoundsForDrawdownFractile(
        trades, 700, 0.90, 40, 350, 0.95, exec,
        IntervalType::ONE_SIDED_UPPER);

    auto res99 = BD::bcaBoundsForDrawdownFractile(
        trades, 700, 0.99, 40, 350, 0.95, exec,
        IntervalType::ONE_SIDED_UPPER);

    SECTION("Both intervals are valid")
    {
        REQUIRE(res90.lowerBound <= res90.upperBound);
        REQUIRE(res99.lowerBound <= res99.upperBound);
    }

    SECTION("Higher confidence level produces wider or equal upper bound")
    {
        // At 99% confidence the upper bound should be >= the 90% upper bound
        // (allowing a small stochastic margin for separate MC runs).
        const double ub90 = num::to_double(res90.upperBound);
        const double ub99 = num::to_double(res99.upperBound);
        REQUIRE(ub99 >= ub90 - 0.03); // 3% stochastic margin
    }
}


// =============================================================================
// Cross-overload consistency: drawdownFractile(Trade) vs drawdownFractile(Decimal)
// =============================================================================

TEST_CASE("BoundedDrawdowns::drawdownFractile Trade/Decimal cross-overload consistency",
          "[BoundedDrawdowns][fractile][Trade][consistency]")
{
    using D = DecimalType;
    concurrency::ThreadPoolExecutor<> exec;
    using BD = mkc_timeseries::BoundedDrawdowns<D, concurrency::ThreadPoolExecutor<>>;

    // When each trade contains exactly one daily bar, the trade-level overload
    // is mathematically equivalent to the scalar overload: sampling nTrades
    // trades from the pool is identical to sampling nTrades bar returns from
    // the flat return pool.  With a single-element pool (deterministic), both
    // overloads must produce exactly the same result.

    SECTION("Single-element pool, single-bar trades: both overloads agree exactly")
    {
        const D r = createDecimal("-0.01");
        const int nTrades = 80;
        const int nReps   = 1000;
        const double p    = 0.90;

        // Scalar overload: pool of one return value.
        std::vector<D> scalarPool = { r };
        D q_scalar = BD::drawdownFractile(
            scalarPool, nTrades, nReps, p, exec);

        // Trade overload: pool of one Trade with a single bar.
        std::vector<mkc_timeseries::Trade<D>> tradePool = { makeTrade<D>({"-0.01"}) };
        D q_trade = BD::drawdownFractile(
            tradePool, nTrades, nReps, p, exec);

        // Both must equal the analytic value and each other.
        const double expected = expected_dd_constant_return(r, nTrades);
        REQUIRE(num::to_double(q_scalar) == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(q_trade)  == Catch::Approx(expected).epsilon(1e-12));
    }

    SECTION("Two-element constant pool, single-bar trades: both overloads agree exactly")
    {
        // Two identical trades/returns -> still deterministic, still must agree.
        const D r = createDecimal("-0.005");
        const int nTrades = 60;
        const int nReps   = 800;
        const double p    = 0.90;

        std::vector<D> scalarPool = { r, r };
        D q_scalar = BD::drawdownFractile(
            scalarPool, nTrades, nReps, p, exec);

        std::vector<mkc_timeseries::Trade<D>> tradePool = {
            makeTrade<D>({"-0.005"}),
            makeTrade<D>({"-0.005"})
        };
        D q_trade = BD::drawdownFractile(
            tradePool, nTrades, nReps, p, exec);

        const double expected = expected_dd_constant_return(r, nTrades);
        REQUIRE(num::to_double(q_scalar) == Catch::Approx(expected).epsilon(1e-12));
        REQUIRE(num::to_double(q_trade)  == Catch::Approx(expected).epsilon(1e-12));
    }
}
