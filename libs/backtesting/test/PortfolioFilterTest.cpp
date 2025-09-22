// PortfolioFilterTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <stdexcept>

#include "PortfolioFilter.h"            // PortfolioFilter / AdaptiveVolatilityPortfolioFilter
#include "TimeSeriesIndicators.h"       // AdaptiveVolatilityPercentRankAnnualizedSeries
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "BiasCorrectedBootstrap.h"     // calculateAnnualizationFactor
#include "TestUtils.h"                  // DecimalType alias, helpers
#include "DecimalConstants.h"
#include "number.h"
#include "BoostDateHelper.h"            // getDefaultBarTime()

using namespace mkc_timeseries;
using num::fromString;
using boost::posix_time::ptime;

// ======= DecimalApproxMatcher (same style as project tests) =======
template<typename Decimal>
struct DecimalApproxMatcher {
    Decimal expected;
    Decimal tolerance;
    DecimalApproxMatcher(const Decimal& e, const Decimal& t) : expected(e), tolerance(t) {}
};
template<typename Decimal>
bool operator==(const Decimal& actual, const DecimalApproxMatcher<Decimal>& approx) {
    if (actual > approx.expected) return (actual - approx.expected).abs() <= approx.tolerance;
    else                          return (approx.expected - actual).abs() <= approx.tolerance;
}
template<typename Decimal>
auto decimalApprox(const Decimal& expected, const Decimal& tolerance) {
    return DecimalApproxMatcher<Decimal>(expected, tolerance);
}
// =================================================================

typedef DecimalType Dec; // from TestUtils.h
static const Dec TOL = fromString<Dec>("0.00001");

// Build a synthetic OHLC series by applying a deterministic return pattern
static OHLCTimeSeries<Dec> makeSeriesWithPattern(std::size_t nBars)
{
    OHLCTimeSeries<Dec> series(TimeFrame::DAILY, TradingVolume::SHARES);

    // Start date
    auto day = boost::gregorian::date(2023, 1, 1);
    double close = 100.0;

    // Repeating return pattern to create varying volatility
    // (units are simple returns, e.g., 0.01 = +1%)
    // Enhanced pattern with more extreme variations to create lower percentile ranks
    const std::vector<double> pattern = {
        0.001, -0.001, 0.0,   0.05,   0.0,   -0.04,  0.0,  0.008, 0.0,  -0.012,
        0.000, 0.000,  0.005, -0.005, 0.0,    0.08,  0.0, -0.06,  0.0,   0.004,
        0.002, -0.002, 0.001, 0.10,  -0.08,  0.001, 0.0,  0.015, 0.0,  -0.025,
        0.000, 0.001,  0.003, -0.003, 0.0,    0.12, -0.10, 0.02,  0.0,   0.006
    };

    for (std::size_t i = 0; i < nBars; ++i) {
        double r = pattern[i % pattern.size()];
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.001;
        double low  = std::min(open, newClose) * 0.999;

        // Use the OHLCTimeSeriesEntry ctor with a date (consistent with your tests)
        series.addEntry(OHLCTimeSeriesEntry<Dec>(
            day, fromString<Dec>(std::to_string(open)),
            fromString<Dec>(std::to_string(high)),
            fromString<Dec>(std::to_string(low)),
            fromString<Dec>(std::to_string(newClose)),
            fromString<Dec>("1000"),
            TimeFrame::DAILY));

        // next day
        day = day + boost::gregorian::days(1);
        close = newClose;
    }

    return series;
}

TEST_CASE("PortfolioFilter / AdaptiveVolatilityPortfolioFilter", "[PortfolioFilter]") {
    const TimeFrame::Duration tf = TimeFrame::DAILY;

    // A reasonably sized sample to ensure R^2 (20) and pr windows fill
    auto ohlc = makeSeriesWithPattern(/*nBars=*/120);

    // Percent-rank window we’ll use for both filter and reference series
    const uint32_t prPeriod = 10;

    // Build the filter (constructor uses r2Period=20 and calculateAnnualizationFactor)
    AdaptiveVolatilityPortfolioFilter<Dec> filter(ohlc, prPeriod);

    // Build a REFERENCE percent-rank series the exact same way as the filter:
    // annualization via calculateAnnualizationFactor(TimeFrame::DAILY)
    const double ann = calculateAnnualizationFactor(tf);
    auto ref = AdaptiveVolatilityPercentRankAnnualizedSeries<Dec>(ohlc, /*r2=*/20, prPeriod, ann);

    REQUIRE(ref.getNumEntries() > 0);  // sanity

    // Find any date with rank < 0.75 (should exist with our varied pattern)
    const Dec thr = DecimalConstants<Dec>::createDecimal("0.75");
    boost::optional<boost::gregorian::date> dateAllow;
    boost::optional<boost::gregorian::date> dateDeny;

    for (auto it = ref.beginSortedAccess(); it != ref.endSortedAccess(); ++it) {
        const Dec v = it->getValue();
        if (!dateAllow && (v < thr)) dateAllow = it->getDate();
        if (!dateDeny  && (v >= thr)) dateDeny  = it->getDate();
        if (dateAllow && dateDeny) break;
    }

    SECTION("areEntriesAllowed == true when percent-rank < 0.75") {
        REQUIRE(dateAllow.has_value()); // pattern should create some sub-75th values
        const ptime when(*dateAllow, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(when) == true);
    }

    SECTION("areEntriesAllowed == false when percent-rank >= 0.75") {
        REQUIRE(dateDeny.has_value()); // and some >= 75th too
        const ptime when(*dateDeny, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(when) == false);
    }

    SECTION("areEntriesAllowed == false when timestamp not present") {
        // Choose a date before the first available in ref (or safely earlier)
        auto missing = boost::gregorian::date(2022, 12, 15);
        const ptime when(missing, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(when) == false);
    }

    SECTION("Sanity: reference series spans the expected dates") {
        // First usable output index for percent-rank: r2Period + prPeriod - 1
        const std::size_t n = ohlc.getNumEntries();
        const std::size_t expected_vol_len = (n >= 20) ? (n - 20 + 1) : 0;      // after R^2 warm-up
        const std::size_t expected_pr_len  = (expected_vol_len >= prPeriod)
                                             ? (expected_vol_len - prPeriod + 1)
                                             : 0;
        REQUIRE(ref.getNumEntries() == expected_pr_len);
    }
}

TEST_CASE("PortfolioFilter / NoPortfolioFilter", "[PortfolioFilter]") {
    // Create a NoPortfolioFilter instance
    NoPortfolioFilter<Dec> noFilter;

    SECTION("areEntriesAllowed always returns true for any valid date") {
        // Test with a specific date
        auto testDate = boost::gregorian::date(2023, 6, 15);
        const ptime when(testDate, getDefaultBarTime());
        REQUIRE(noFilter.areEntriesAllowed(when) == true);
    }

    SECTION("areEntriesAllowed always returns true for different dates") {
        // Test with multiple different dates to ensure consistency
        std::vector<boost::gregorian::date> testDates = {
            boost::gregorian::date(2020, 1, 1),
            boost::gregorian::date(2023, 12, 31),
            boost::gregorian::date(2025, 7, 4),
            boost::gregorian::date(1990, 2, 14)
        };

        for (const auto& date : testDates) {
            const ptime when(date, getDefaultBarTime());
            REQUIRE(noFilter.areEntriesAllowed(when) == true);
        }
    }

    SECTION("areEntriesAllowed always returns true for different times of day") {
        // Test with same date but different times
        auto testDate = boost::gregorian::date(2023, 6, 15);
        
        std::vector<boost::posix_time::time_duration> testTimes = {
            boost::posix_time::time_duration(9, 30, 0),   // Market open
            boost::posix_time::time_duration(12, 0, 0),   // Noon
            boost::posix_time::time_duration(16, 0, 0),   // Market close
            boost::posix_time::time_duration(23, 59, 59)  // End of day
        };

        for (const auto& time : testTimes) {
            const ptime when(testDate, time);
            REQUIRE(noFilter.areEntriesAllowed(when) == true);
        }
    }

    SECTION("Constructor creates valid instance") {
        // Test that the constructor works and creates a usable instance
        NoPortfolioFilter<Dec> filter1;
        NoPortfolioFilter<Dec> filter2{};
        
        auto testDate = boost::gregorian::date(2023, 1, 1);
        const ptime when(testDate, getDefaultBarTime());
        
        REQUIRE(filter1.areEntriesAllowed(when) == true);
        REQUIRE(filter2.areEntriesAllowed(when) == true);
    }

    SECTION("Polymorphic usage through base class pointer") {
        // Test that NoPortfolioFilter works correctly when used polymorphically
        std::unique_ptr<PortfolioFilter<Dec>> filter = std::make_unique<NoPortfolioFilter<Dec>>();
        
        auto testDate = boost::gregorian::date(2023, 6, 15);
        const ptime when(testDate, getDefaultBarTime());
        
        REQUIRE(filter->areEntriesAllowed(when) == true);
    }
}

TEST_CASE("PortfolioFilter / AdaptiveVolatilityPortfolioFilter (Simons policy)", "[PortfolioFilter]") {
    using mkc_timeseries::SimonsHLCVolatilityPolicy;  // policy under test

    const TimeFrame::Duration tf = TimeFrame::DAILY;

    // Same synthetic series helper used above
    auto ohlc = makeSeriesWithPattern(/*nBars=*/120);

    // Percent-rank window (same as the default-policy test)
    const uint32_t prPeriod = 10;

    // Build a filter that uses SimonsHLCVolatilityPolicy
    // (Assumes AdaptiveVolatilityPortfolioFilter is templated on Decimal, VolPolicy with
    //  default = CloseToCloseVolatilityPolicy.)
    AdaptiveVolatilityPortfolioFilter<Dec, SimonsHLCVolatilityPolicy> filterSimons(ohlc, prPeriod);

    // Reference percent-rank series computed the *same way* the filter does, but using Simons policy
    const double ann = calculateAnnualizationFactor(tf);
    auto refSimons = AdaptiveVolatilityPercentRankAnnualizedSeries<Dec, SimonsHLCVolatilityPolicy>(
                        ohlc, /*r2=*/20, prPeriod, ann);

    REQUIRE(refSimons.getNumEntries() > 0); // sanity

    // Find a date with rank < 0.75 (should exist with our varied pattern), and one >= 0.75
    const Dec thr = DecimalConstants<Dec>::createDecimal("0.75");
    boost::optional<boost::gregorian::date> dateAllow;
    boost::optional<boost::gregorian::date> dateDeny;

    for (auto it = refSimons.beginSortedAccess(); it != refSimons.endSortedAccess(); ++it) {
        const Dec v = it->getValue();
        if (!dateAllow && (v < thr))  dateAllow = it->getDate();
        if (!dateDeny  && (v >= thr)) dateDeny  = it->getDate();
        if (dateAllow && dateDeny) break;
    }

    SECTION("areEntriesAllowed == true when percent-rank < 0.75 (Simons)") {
        REQUIRE(dateAllow.has_value());
        const ptime when(*dateAllow, getDefaultBarTime());
        REQUIRE(filterSimons.areEntriesAllowed(when) == true);
    }

    SECTION("areEntriesAllowed == false when percent-rank >= 0.75 (Simons)") {
        REQUIRE(dateDeny.has_value());
        const ptime when(*dateDeny, getDefaultBarTime());
        REQUIRE(filterSimons.areEntriesAllowed(when) == false);
    }

    SECTION("Sanity: reference (Simons) series spans the expected dates") {
        const std::size_t n = ohlc.getNumEntries();
        const std::size_t expected_vol_len = (n >= 20) ? (n - 20 + 1) : 0; // after R² warm-up
        const std::size_t expected_pr_len  = (expected_vol_len >= prPeriod)
                                             ? (expected_vol_len - prPeriod + 1)
                                             : 0;
        REQUIRE(refSimons.getNumEntries() == expected_pr_len);
    }
}
