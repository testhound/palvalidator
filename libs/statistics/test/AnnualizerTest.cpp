// AnnualizerTest.cpp
//
// Unit tests for mkc_timeseries::Annualizer:
//  - single-value annualization matches analytic exp(K*log1p(r)) - 1
//  - guards keep outputs finite and > -1 even near ruin (r <= -1)
//  - ordering preserved for (lower, mean, upper) triplets
//  - triplet = element-wise application of annualize_one
//  - monotonicity with K for small positive returns
//
// Reference style & intent is consistent with the BCa integration test:
// :contentReference[oaicite:0]{index=0}

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <memory>
#include "number.h"               // DecimalType alias (dec::decimal<8>)
#include "Annualizer.h"           // mkc_timeseries::Annualizer

using DecimalType = num::DefaultNumber;
using A = mkc_timeseries::Annualizer<DecimalType>;

namespace
{
    // Minimal fake time series type used to test
    // computeAnnualizationFactorForSeries. It only needs to expose
    // getIntradayTimeFrameDurationInMinutes().
    struct DummyIntradaySeries
    {
        explicit DummyIntradaySeries(int minutesPerBar)
            : m_minutesPerBar(minutesPerBar)
        {
        }

        int getIntradayTimeFrameDurationInMinutes() const
        {
            return m_minutesPerBar;
        }

    private:
        int m_minutesPerBar;
    };
}

static inline double round_to_decimal8(double x)
{
    return std::round(x * 1e8) / 1e8;
}

static inline double annualize_expect(double r_per_period, long double K)
{
    // Analytic (1+r)^K - 1 computed stably in long double
    const long double g = std::log1pl(static_cast<long double>(r_per_period));
    const long double a = std::exp(K * g) - 1.0L;
    return static_cast<double>(a);
}

TEST_CASE("Annualizer::annualize_one matches analytic and is finite", "[Annualizer][single]")
{
    using D = DecimalType;

    // A mix of negative, zero, and positive per-period returns (all > -1)
    const std::vector<double> rs = { -0.35, -0.01, 0.0, 0.0005, 0.01, 0.05 };
    const std::vector<long double> Ks = { 12.0L, 252.0L, 504.0L };

    for (double r : rs)
    {
        for (auto K : Ks)
        {
            const D ann = A::annualize_one(D(r), static_cast<double>(K));
            const double got = num::to_double(ann);
            const double expd = round_to_decimal8(annualize_expect(r, K));

            REQUIRE(std::isfinite(got));
            REQUIRE(got > -1.0); // strictly > -1 for r > -1
            REQUIRE(got == Catch::Approx(expd).margin(1e-12));
        }
    }
}

TEST_CASE("Annualizer guards near ruin (r <= -1) and remains > -1 after transform", "[Annualizer][guards]")
{
    using D = DecimalType;

    // Exact -1 and less-than -1 should be clamped internally for log1p
    const std::vector<double> rs = { -1.0, -1.0000001, -10.0 };
    const long double K = 252.0L;

    for (double r : rs)
    {
        const D ann = A::annualize_one(D(r), static_cast<double>(K));
        const double got = num::to_double(ann);

        REQUIRE(std::isfinite(got));
        // Our implementation bumps results that would land exactly at -1
        REQUIRE(got > -1.0);
    }
}

TEST_CASE("Annualizer::annualize_triplet preserves ordering and equals element-wise", "[Annualizer][triplet]")
{
    using D = DecimalType;

    // Construct a strictly ordered triplet inside (-1, +inf)
    const D lo = D(-0.01);
    const D mu = D( 0.003);
    const D hi = D( 0.02);

    const double K = 252.0;

    // Triplet API
    auto trip = A::annualize_triplet(lo, mu, hi, K);

    // Element-wise for cross-check
    const D lo1 = A::annualize_one(lo, K);
    const D mu1 = A::annualize_one(mu, K);
    const D hi1 = A::annualize_one(hi, K);

    // Ordering preserved
    REQUIRE(num::to_double(trip.lower) <= num::to_double(trip.mean));
    REQUIRE(num::to_double(trip.mean)  <= num::to_double(trip.upper));

    // Equality with element-wise calls (exact in Decimal<8> rounding)
    REQUIRE(num::to_double(trip.lower) == Catch::Approx(num::to_double(lo1)).margin(1e-12));
    REQUIRE(num::to_double(trip.mean)  == Catch::Approx(num::to_double(mu1)).margin(1e-12));
    REQUIRE(num::to_double(trip.upper) == Catch::Approx(num::to_double(hi1)).margin(1e-12));
}

TEST_CASE("Annualizer: larger K weakly increases annualized mean for small positive r", "[Annualizer][monotonicity]")
{
    using D = DecimalType;

    const D r = D(0.001); // 0.1% per period
    const double K1 = 252.0;
    const double K2 = 504.0;

    const double a1 = num::to_double(A::annualize_one(r, K1));
    const double a2 = num::to_double(A::annualize_one(r, K2));

    REQUIRE(a2 >= a1 - 1e-12);
}

TEST_CASE("Annualizer rejects non-positive or non-finite K", "[Annualizer][validation]")
{
    using D = DecimalType;

    // K must be positive and finite
    REQUIRE_THROWS_AS(A::annualize_one(D(0.01), 0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(A::annualize_one(D(0.01), -5.0), std::invalid_argument);

    // Simulate a non-finite K without relying on platform-specific INFINITY macro
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_THROWS_AS(A::annualize_one(D(0.01), NaN), std::invalid_argument);
}

TEST_CASE("computeAnnualizationFactor returns expected factors for standard time frames",
          "[Annualizer][factor][TimeFrame]")
{
    using mkc_timeseries::TimeFrame;
    using mkc_timeseries::computeAnnualizationFactor;

    // Daily / Weekly / Monthly / Quarterly / Yearly
    REQUIRE(computeAnnualizationFactor(TimeFrame::DAILY)
            == Catch::Approx(252.0));
    REQUIRE(computeAnnualizationFactor(TimeFrame::WEEKLY)
            == Catch::Approx(52.0));
    REQUIRE(computeAnnualizationFactor(TimeFrame::MONTHLY)
            == Catch::Approx(12.0));
    REQUIRE(computeAnnualizationFactor(TimeFrame::QUARTERLY)
            == Catch::Approx(4.0));
    REQUIRE(computeAnnualizationFactor(TimeFrame::YEARLY)
            == Catch::Approx(1.0));

    // INTRADAY with a concrete minutes-per-bar
    // Expected: trading_hours_per_day * (60 / minutesPerBar) * trading_days_per_year
    const int    minutesPerBar = 5;
    const double expected_intraday =
        6.5 * (60.0 / static_cast<double>(minutesPerBar)) * 252.0;

    const double got_intraday =
        computeAnnualizationFactor(TimeFrame::INTRADAY, minutesPerBar);

    REQUIRE(got_intraday == Catch::Approx(expected_intraday).margin(1e-12));

    // INTRADAY with minutesPerBar == 0 should throw
    REQUIRE_THROWS_AS(
        computeAnnualizationFactor(TimeFrame::INTRADAY, 0),
        std::invalid_argument);

    // INTRADAY with non-positive trading_days_per_year or trading_hours_per_day
    REQUIRE_THROWS_AS(
        computeAnnualizationFactor(TimeFrame::INTRADAY,
                                   minutesPerBar,
                                   0.0,     // trading_days_per_year
                                   6.5),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        computeAnnualizationFactor(TimeFrame::INTRADAY,
                                   minutesPerBar,
                                   252.0,
                                   0.0),   // trading_hours_per_day
        std::invalid_argument);
}

TEST_CASE("computeAnnualizationFactorForSeries uses intraday minutes-per-bar from series",
          "[Annualizer][factor][TimeSeries][INTRADAY]")
{
    using mkc_timeseries::TimeFrame;
    using mkc_timeseries::computeAnnualizationFactor;
    using mkc_timeseries::computeAnnualizationFactorForSeries;

    const int minutesPerBar = 15;
    auto ts = std::make_shared<DummyIntradaySeries>(minutesPerBar);

    const double expected =
        computeAnnualizationFactor(TimeFrame::INTRADAY, minutesPerBar);

    const double got =
        computeAnnualizationFactorForSeries(TimeFrame::INTRADAY, ts);

    REQUIRE(got == Catch::Approx(expected).margin(1e-12));
}

TEST_CASE("computeAnnualizationFactorForSeries falls back to TimeFrame-only variant for non-intraday",
          "[Annualizer][factor][TimeSeries][non-INTRADAY]")
{
    using mkc_timeseries::TimeFrame;
    using mkc_timeseries::computeAnnualizationFactor;
    using mkc_timeseries::computeAnnualizationFactorForSeries;

    auto ts = std::make_shared<DummyIntradaySeries>(5);

    // For non-INTRADAY, the series is ignored and the TimeFrame-only
    // overload is used internally.
    const double daily_expected  = computeAnnualizationFactor(TimeFrame::DAILY);
    const double daily_from_ts   = computeAnnualizationFactorForSeries(TimeFrame::DAILY, ts);

    const double monthly_expected = computeAnnualizationFactor(TimeFrame::MONTHLY);
    const double monthly_from_ts  = computeAnnualizationFactorForSeries(TimeFrame::MONTHLY, ts);

    REQUIRE(daily_from_ts   == Catch::Approx(daily_expected).margin(1e-12));
    REQUIRE(monthly_from_ts == Catch::Approx(monthly_expected).margin(1e-12));
}

TEST_CASE("computeAnnualizationFactorForSeries throws for INTRADAY without series",
          "[Annualizer][factor][TimeSeries][error]")
{
    using mkc_timeseries::TimeFrame;
    using mkc_timeseries::computeAnnualizationFactorForSeries;

    std::shared_ptr<DummyIntradaySeries> nullTs;

    // For INTRADAY and a null series, the helper falls through to the
    // TimeFrame-only overload with intraday_minutes_per_bar = 0,
    // which must throw std::invalid_argument.
    REQUIRE_THROWS_AS(
        computeAnnualizationFactorForSeries(TimeFrame::INTRADAY, nullTs),
        std::invalid_argument);
}
