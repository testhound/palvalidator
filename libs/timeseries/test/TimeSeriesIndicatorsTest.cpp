
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <utility>
#include <ostream>
#include <stdexcept>  // for std::domain_error

// Project headers
#include "TimeSeriesIndicators.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "TestUtils.h"
#include "DecimalConstants.h"
#include "number.h"
#include "BoostDateHelper.h"

// Using directives
using namespace mkc_timeseries;
// DecimalType is defined in TestUtils.h as typedef num::DefaultNumber DecimalType;
using num::fromString; // bring fromString<DecimalType> into scope
using namespace Catch;

// ======= DecimalApproxMatcher for Catch2 =======
template<typename Decimal>
struct DecimalApproxMatcher {
    Decimal expected;
    Decimal tolerance;
    DecimalApproxMatcher(const Decimal& e, const Decimal& t)
      : expected(e), tolerance(t) {}
};

template<typename Decimal>
bool operator==(const Decimal& actual, const DecimalApproxMatcher<Decimal>& approx) {
    if (actual > approx.expected)
        return (actual - approx.expected).abs() <= approx.tolerance;
    else
        return (approx.expected - actual).abs() <= approx.tolerance;
}

template<typename Decimal>
auto decimalApprox(const Decimal& expected, const Decimal& tolerance) {
    return DecimalApproxMatcher<Decimal>(expected, tolerance);
}

template<typename Decimal>
std::ostream& operator<<(std::ostream& os,
                         const DecimalApproxMatcher<Decimal>& approx)
{
    os << "expected " << approx.expected
       << " ± " << approx.tolerance;
    return os;
}
// ======= End DecimalApproxMatcher =======


// Helpers to build test series
NumericTimeSeries<DecimalType> createNumericTimeSeriesForTest(
    TimeFrame::Duration tf,
    const std::vector<std::pair<std::string, std::string>>& dateValuePairs)
{
    NumericTimeSeries<DecimalType> ts(tf, dateValuePairs.size());
    for (auto const &p : dateValuePairs) {
        ts.addEntry(NumericTimeSeriesEntry<DecimalType>(
            boost::gregorian::from_simple_string(p.first),
            fromString<DecimalType>(p.second),
            tf
        ));
    }
    return ts;
}

NumericTimeSeries<DecimalType> createNumericTimeSeriesPTimeForTest(
    TimeFrame::Duration tf,
    const std::vector<std::pair<ptime, DecimalType>>& ptimeValuePairs)
{
    NumericTimeSeries<DecimalType> ts(tf, ptimeValuePairs.size());
    for (auto const &pv : ptimeValuePairs) {
        ts.addEntry(NumericTimeSeriesEntry<DecimalType>(
            pv.first,
            pv.second,
            tf
        ));
    }
    return ts;
}

const DecimalType TEST_DEC_TOL_INDICATORS = fromString<DecimalType>("0.00001");
const DecimalType ROBUST_QN_TOL_INDICATORS = fromString<DecimalType>("0.001");
const DecimalType ROC_TOL_INDICATORS = fromString<DecimalType>("0.0001");

TEST_CASE("TimeSeriesIndicators Tests", "[TimeSeriesIndicators]") {
    TimeFrame::Duration daily_tf = TimeFrame::DAILY;

    SECTION("ComputeRobustStopAndTargetFromSeries") {
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // Synthetic OHLC time series with positive skew - oscillating around 100 with occasional large positive moves
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-01"),
        fromString<DecimalType>("100"), fromString<DecimalType>("100.5"),
        fromString<DecimalType>("99.5"), fromString<DecimalType>("100"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-02"),
        fromString<DecimalType>("100"), fromString<DecimalType>("100.8"),
        fromString<DecimalType>("99.2"), fromString<DecimalType>("99.8"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-03"),
        fromString<DecimalType>("99.8"), fromString<DecimalType>("101.2"),
        fromString<DecimalType>("99.5"), fromString<DecimalType>("100.1"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-04"),
        fromString<DecimalType>("100.1"), fromString<DecimalType>("100.5"),
        fromString<DecimalType>("99.0"), fromString<DecimalType>("99.5"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-05"),
        fromString<DecimalType>("99.5"), fromString<DecimalType>("100.2"),
        fromString<DecimalType>("98.8"), fromString<DecimalType>("100.2"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-06"),
        fromString<DecimalType>("100.2"), fromString<DecimalType>("103.5"),
        fromString<DecimalType>("100.0"), fromString<DecimalType>("103.0"), fromString<DecimalType>("100"), TimeFrame::DAILY)); // Large positive move
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-07"),
        fromString<DecimalType>("103.0"), fromString<DecimalType>("103.5"),
        fromString<DecimalType>("102.2"), fromString<DecimalType>("102.8"), fromString<DecimalType>("100"), TimeFrame::DAILY));

    auto [profitTarget, stopLoss] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series);

    // We can't predict exact values because skew/Qn are dynamic,
    // but we can assert expected relationships
    REQUIRE(profitTarget > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(stopLoss > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(profitTarget > stopLoss);  // positive skew → wider profit, tighter stop

    // Optional: print values for debugging
    INFO("Profit Target: " << profitTarget);
    INFO("Stop Loss: " << stopLoss);
}

SECTION("ComputeRobustStopAndTargetFromSeries - negative skew") {
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // Synthetic OHLC series with negative skew - oscillating around 100 with occasional large negative moves
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-01"),
        fromString<DecimalType>("100"), fromString<DecimalType>("100.5"),
        fromString<DecimalType>("99.5"), fromString<DecimalType>("100"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-02"),
        fromString<DecimalType>("100"), fromString<DecimalType>("100.8"),
        fromString<DecimalType>("99.2"), fromString<DecimalType>("100.2"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-03"),
        fromString<DecimalType>("100.2"), fromString<DecimalType>("100.5"),
        fromString<DecimalType>("99.8"), fromString<DecimalType>("99.9"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-04"),
        fromString<DecimalType>("99.9"), fromString<DecimalType>("100.5"),
        fromString<DecimalType>("99.0"), fromString<DecimalType>("100.3"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-05"),
        fromString<DecimalType>("100.3"), fromString<DecimalType>("100.8"),
        fromString<DecimalType>("99.5"), fromString<DecimalType>("99.8"), fromString<DecimalType>("100"), TimeFrame::DAILY));
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-06"),
        fromString<DecimalType>("99.8"), fromString<DecimalType>("100.2"),
        fromString<DecimalType>("96.5"), fromString<DecimalType>("97.0"), fromString<DecimalType>("100"), TimeFrame::DAILY)); // Large negative move
    series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        boost::gregorian::from_simple_string("2023-01-07"),
        fromString<DecimalType>("97.0"), fromString<DecimalType>("97.8"),
        fromString<DecimalType>("96.8"), fromString<DecimalType>("97.2"), fromString<DecimalType>("100"), TimeFrame::DAILY));

    auto [profitTarget, stopLoss] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series);

    // With negative skew, we expect:
    // - profit target to shrink
    // - stop loss to widen
    REQUIRE(profitTarget > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(stopLoss > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(profitTarget < stopLoss);  // negative skew → tighter target, wider stop

    // Optional: print values for review
    INFO("Profit Target (neg skew): " << profitTarget);
    INFO("Stop Loss (neg skew): " << stopLoss);
}

        SECTION("RobustSkewMedcouple") {
        using mkc_timeseries::RobustSkewMedcouple;

        SECTION("Symmetric distribution") {
            auto ts = createNumericTimeSeriesForTest(daily_tf, {
                {"2023-01-01", "1"},
                {"2023-01-02", "2"},
                {"2023-01-03", "3"},
                {"2023-01-04", "4"},
                {"2023-01-05", "5"}
            });
            auto result = RobustSkewMedcouple(ts);
            REQUIRE(result == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Positive skew") {
            auto ts = createNumericTimeSeriesForTest(daily_tf, {
                {"2023-01-01", "1"},
                {"2023-01-02", "2"},
                {"2023-01-03", "3"},
                {"2023-01-04", "6"},
                {"2023-01-05", "12"}
            });
            auto result = RobustSkewMedcouple(ts);
            REQUIRE(result > DecimalConstants<DecimalType>::DecimalZero);
        }

        SECTION("Negative skew") {
            auto ts = createNumericTimeSeriesForTest(daily_tf, {
                {"2023-01-01", "1"},
                {"2023-01-02", "2"},
                {"2023-01-03", "3"},
                {"2023-01-04", "-1"},
                {"2023-01-05", "-4"}
            });
            auto result = RobustSkewMedcouple(ts);
            REQUIRE(result < DecimalConstants<DecimalType>::DecimalZero);
        }

        SECTION("Flat series (zero skew)") {
            auto ts = createNumericTimeSeriesForTest(daily_tf, {
                {"2023-01-01", "5"},
                {"2023-01-02", "5"},
                {"2023-01-03", "5"},
                {"2023-01-04", "5"},
                {"2023-01-05", "5"}
            });
            auto result = RobustSkewMedcouple(ts);
            REQUIRE(result == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Too few values") {
            auto ts = createNumericTimeSeriesForTest(daily_tf, {
                {"2023-01-01", "1"},
                {"2023-01-02", "2"}
            });
            REQUIRE_THROWS_AS(RobustSkewMedcouple(ts), std::domain_error);
        }
    }

    SECTION("ComputeRobustStopAndTargetFromSeries (3-arg overload) — anchors cap/floor widths (pos skew)")
{
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // Build 25 bars with mild drift and two large positive spikes → positive skew
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    double close = 100.0;
    for (int i = 1; i <= 25; ++i) {
        double r = 0.001;                // +0.1% typical
        if (i == 10 || i == 20) r = 0.05; // +5% spikes
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.002;
        double low  = std::min(open, newClose) * 0.998;

        std::string day = (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
        auto e = createEquityEntry("202301" + day,
                                   std::to_string(open),
                                   std::to_string(high),
                                   std::to_string(low),
                                   std::to_string(newClose),
                                   1000);
        series.addEntry(*e);
        close = newClose;
    }

    // period=1 so rocVec.size() = 24 ≥ kMinSample(20) → anchors path is eligible
    auto [pt_no, sl_no] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, false);
    auto [pt_an, sl_an] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, true);

    // Basic sanity
    REQUIRE(pt_no > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(sl_no > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(pt_an > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(sl_an > DecimalConstants<DecimalType>::DecimalZero);

    // With positive skew, anchors should CAP target and FLOOR stop
    REQUIRE(pt_an <= pt_no);
    REQUIRE(sl_an >= sl_no);
}

SECTION("ComputeRobustStopAndTargetFromSeries (3-arg overload) — anchors cap/floor widths (neg skew)")
{
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // Build 25 bars with two large negative moves → negative skew
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    double close = 100.0;
    for (int i = 1; i <= 25; ++i) {
        double r = 0.001;                 // +0.1% typical
        if (i == 12 || i == 22) r = -0.05; // -5% shocks
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.002;
        double low  = std::min(open, newClose) * 0.998;

        std::string day = (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
        auto e = createEquityEntry("202302" + day,
                                   std::to_string(open),
                                   std::to_string(high),
                                   std::to_string(low),
                                   std::to_string(newClose),
                                   1000);
        series.addEntry(*e);
        close = newClose;
    }

    auto [pt_no, sl_no] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, false);
    auto [pt_an, sl_an] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, true);

    // Anchors should still CAP target and FLOOR stop
    REQUIRE(pt_an <= pt_no);
    REQUIRE(sl_an >= sl_no);
}

SECTION("ComputeRobustStopAndTargetFromSeries (3-arg overload) — period parameter matters")
{
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // Reuse a medium-size series (above)
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    double close = 100.0;
    for (int i = 1; i <= 25; ++i) {
        double r = (i % 5 == 0 ? 0.02 : 0.001);
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.001;
        double low  = std::min(open, newClose) * 0.999;

        std::string day = (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
        auto e = createEquityEntry("202303" + day,
                                   std::to_string(open),
                                   std::to_string(high),
                                   std::to_string(low),
                                   std::to_string(newClose),
                                   1000);
        series.addEntry(*e);
        close = newClose;
    }

    auto [pt_p1, sl_p1] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, false);
    auto [pt_p3, sl_p3] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 3, false);

    // Expect a change when period changes
    REQUIRE( ((pt_p1 - pt_p3).abs() > TEST_DEC_TOL_INDICATORS
          || (sl_p1 - sl_p3).abs() > TEST_DEC_TOL_INDICATORS) );
}

SECTION("ComputeRobustStopAndTargetFromSeries (3-arg overload) — anchors disabled under small sample")
{
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // Only 15 bars → rocVec.size() = 14 < kMinSample(20), so anchors path is skipped
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    double close = 50.0;
    for (int i = 1; i <= 15; ++i) {
        double r = (i % 7 == 0 ? 0.03 : 0.002);
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.001;
        double low  = std::min(open, newClose) * 0.999;

        std::string day = (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
        auto e = createEquityEntry("202304" + day,
                                   std::to_string(open),
                                   std::to_string(high),
                                   std::to_string(low),
                                   std::to_string(newClose),
                                   1000);
        series.addEntry(*e);
        close = newClose;
    }

    auto [pt_no, sl_no] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, false);
    auto [pt_an, sl_an] = ComputeRobustStopAndTargetFromSeries<DecimalType>(series, 1, true);

    // Should be effectively identical because anchors are not applied
    REQUIRE(pt_an == decimalApprox(pt_no, TEST_DEC_TOL_INDICATORS));
    REQUIRE(sl_an == decimalApprox(sl_no, TEST_DEC_TOL_INDICATORS));
}

SECTION("ComputeRobustStopAndTargetFromSeries (3-arg overload) — error conditions")
{
    using mkc_timeseries::ComputeRobustStopAndTargetFromSeries;

    // < 3 bars → throws
    {
        OHLCTimeSeries<DecimalType> s(TimeFrame::DAILY, TradingVolume::SHARES);
        auto e1 = createEquityEntry("20230501","100","101","99","100",1000);
        auto e2 = createEquityEntry("20230502","100","101","99","101",1000);
        s.addEntry(*e1);
        s.addEntry(*e2);
        REQUIRE_THROWS_AS(ComputeRobustStopAndTargetFromSeries<DecimalType>(s, 1, false), std::domain_error);
    }

    // ROC series too small (e.g., 4 bars, period = 3) → throws
    {
        OHLCTimeSeries<DecimalType> s(TimeFrame::DAILY, TradingVolume::SHARES);
        auto e1 = createEquityEntry("20230601","100","101","99","100",1000);
        auto e2 = createEquityEntry("20230602","100","101","99","101",1000);
        auto e3 = createEquityEntry("20230603","101","102","100","102",1000);
        auto e4 = createEquityEntry("20230604","102","103","101","103",1000);
        s.addEntry(*e1); s.addEntry(*e2); s.addEntry(*e3); s.addEntry(*e4);
        REQUIRE_THROWS_AS(ComputeRobustStopAndTargetFromSeries<DecimalType>(s, 3, false), std::domain_error);
    }
}

    SECTION("DivideSeries") {
        ptime d1(boost::gregorian::date(2023,1,1), getDefaultBarTime());
        ptime d2(boost::gregorian::date(2023,1,2), getDefaultBarTime());
        ptime d3(boost::gregorian::date(2023,1,3), getDefaultBarTime());

        auto s1_ds = createNumericTimeSeriesPTimeForTest(daily_tf, {
            {d1, fromString<DecimalType>("10")},
            {d2, fromString<DecimalType>("20")},
            {d3, fromString<DecimalType>("30")}
        });
        auto s2_ds = createNumericTimeSeriesPTimeForTest(daily_tf, {
            {d1, fromString<DecimalType>("2")},
            {d2, fromString<DecimalType>("4")},
            {d3, fromString<DecimalType>("5")}
        });

        SECTION("Basic division") {
            auto result = DivideSeries(s1_ds, s2_ds);
            REQUIRE(result.getNumEntries() == 3);
            REQUIRE(result.getTimeSeriesEntry(d1.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("5.0"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(result.getTimeSeriesEntry(d2.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("5.0"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(result.getTimeSeriesEntry(d3.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("6.0"), TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Different lengths (s1 shorter)") {
            auto s1_short = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("10")},
                {d2, fromString<DecimalType>("20")}
            });
            auto s2_long = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("2")},
                {d2, fromString<DecimalType>("4")},
                {d3, fromString<DecimalType>("5")}
            });

	    REQUIRE_THROWS_AS(DivideSeries(s1_short, s2_long), std::domain_error);
}

        SECTION("Different lengths (s2 shorter, end dates match)") {
            auto s1_long = s1_ds;
            auto s2_short = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d2, fromString<DecimalType>("4")},
                {d3, fromString<DecimalType>("5")}
            });

	    REQUIRE_THROWS_AS(DivideSeries(s1_long, s2_short), std::domain_error);
        }

        SECTION("Denominator has zero") {
            auto s2_zero = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("2")},
                {d2, fromString<DecimalType>("0")},
                {d3, fromString<DecimalType>("5")}
            });
            auto result = DivideSeries(s1_ds, s2_zero);
            REQUIRE(result.getTimeSeriesEntry(d2.date())->second->getValue()
                == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Empty series") {
            NumericTimeSeries<DecimalType> empty(daily_tf);
            REQUIRE(DivideSeries(empty, s2_ds).getNumEntries() == 0);
            REQUIRE(DivideSeries(s1_ds, empty).getNumEntries() == 0);
        }

        SECTION("Mismatched time frames") {
            NumericTimeSeries<DecimalType> weekly(TimeFrame::WEEKLY);
            weekly.addEntry(NumericTimeSeriesEntry<DecimalType>(
                d3.date(), fromString<DecimalType>("5"), TimeFrame::WEEKLY));
            REQUIRE_THROWS_AS(DivideSeries(s1_ds, weekly), std::domain_error);
        }

        SECTION("Mismatched end dates") {
            auto s2_diff_end = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("2")},
                {d2, fromString<DecimalType>("4")}
            });
            REQUIRE_THROWS_AS(DivideSeries(s1_ds, s2_diff_end), std::domain_error);
        }
    }


    SECTION("RocSeries") {
        ptime d1(boost::gregorian::date(2023,1,1), getDefaultBarTime());
        ptime d2(boost::gregorian::date(2023,1,2), getDefaultBarTime());
        ptime d3(boost::gregorian::date(2023,1,3), getDefaultBarTime());
        ptime d4(boost::gregorian::date(2023,1,4), getDefaultBarTime());

        SECTION("Basic ROC (period 1)") {
            auto s = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("100")},
                {d2, fromString<DecimalType>("102")},
                {d3, fromString<DecimalType>("105")},
                {d4, fromString<DecimalType>("103")}
            });
            auto result = RocSeries(s, 1);
            REQUIRE(result.getNumEntries() == 3);
            REQUIRE(result.getTimeSeriesEntry(d2.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("2.0"), ROC_TOL_INDICATORS));
            REQUIRE(result.getTimeSeriesEntry(d3.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("2.941176"), ROC_TOL_INDICATORS));
            REQUIRE(result.getTimeSeriesEntry(d4.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("-1.904762"), ROC_TOL_INDICATORS));
        }

        SECTION("ROC (period 0)") {
            auto s = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("100")},
                {d2, fromString<DecimalType>("102")}
            });
            auto result = RocSeries(s, 0);
            REQUIRE(result.getNumEntries() == 2);
            REQUIRE(result.getTimeSeriesEntry(d1.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("0.0"), ROC_TOL_INDICATORS));
        }

        SECTION("ROC (period 2)") {
            auto s = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("100")},
                {d2, fromString<DecimalType>("110")},
                {d3, fromString<DecimalType>("121")},
                {d4, fromString<DecimalType>("133.1")}
            });
            auto r = RocSeries(s, 2);
            REQUIRE(r.getNumEntries() == 2);
            REQUIRE(r.getTimeSeriesEntry(d3.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("21.0"), ROC_TOL_INDICATORS));
            REQUIRE(r.getTimeSeriesEntry(d4.date())->second->getValue()
                == decimalApprox(fromString<DecimalType>("21.0"), ROC_TOL_INDICATORS));
        }

        SECTION("ROC with division by zero in prevValue") {
            auto s = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("0")},
                {d2, fromString<DecimalType>("102")}
            });
            REQUIRE_THROWS_AS(RocSeries(s, 1), std::domain_error);
        }

        SECTION("Series shorter than period + 1") {
            auto s = createNumericTimeSeriesPTimeForTest(daily_tf, {
                {d1, fromString<DecimalType>("100")}
            });
            REQUIRE(RocSeries(s,1).getNumEntries() == 0);
        }

        SECTION("Empty series") {
            NumericTimeSeries<DecimalType> empty(daily_tf);
            REQUIRE(RocSeries(empty,1).getNumEntries() == 0);
        }
    }


    SECTION("Median (NumericTimeSeries)") {
        auto ts_odd = createNumericTimeSeriesForTest(daily_tf, {
            {"2023-01-01","10"}, {"2023-01-02","20"}, {"2023-01-03","5"}
        });
        REQUIRE(Median(ts_odd) == decimalApprox(fromString<DecimalType>("10"), TEST_DEC_TOL_INDICATORS));

        auto ts_even = createNumericTimeSeriesForTest(daily_tf, {
            {"2023-01-01","10"}, {"2023-01-02","20"},
            {"2023-01-03","5"},  {"2023-01-04","30"}
        });
        REQUIRE(Median(ts_even) == decimalApprox(fromString<DecimalType>("15"), TEST_DEC_TOL_INDICATORS));

        NumericTimeSeries<DecimalType> ts_empty(daily_tf);
        REQUIRE_THROWS_AS(Median(ts_empty), std::domain_error);
    }

    SECTION("Median (std::vector<DecimalType>) using Median template") {
        std::vector<DecimalType> vec_odd = {
            fromString<DecimalType>("10"),
            fromString<DecimalType>("20"),
            fromString<DecimalType>("5")
        };
        REQUIRE(Median(vec_odd) == decimalApprox(fromString<DecimalType>("10"), TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_even = {
            fromString<DecimalType>("10"),
            fromString<DecimalType>("20"),
            fromString<DecimalType>("5"),
            fromString<DecimalType>("30")
        };
        REQUIRE(Median(vec_even) == decimalApprox(fromString<DecimalType>("15"), TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_empty;
        REQUIRE_THROWS_AS(Median(vec_empty), std::domain_error);

        std::vector<DecimalType> vec_single = {fromString<DecimalType>("42")};
        REQUIRE(Median(vec_single) == decimalApprox(fromString<DecimalType>("42"), TEST_DEC_TOL_INDICATORS));
    }

    SECTION("MedianOfVec") {
        std::vector<DecimalType> vec_odd = {
            fromString<DecimalType>("10"),
            fromString<DecimalType>("5"),
            fromString<DecimalType>("20")
        };
        REQUIRE(MedianOfVec(vec_odd) == decimalApprox(fromString<DecimalType>("10"), TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_even = {
            fromString<DecimalType>("10"),
            fromString<DecimalType>("5"),
            fromString<DecimalType>("20"),
            fromString<DecimalType>("30")
        };
        REQUIRE(MedianOfVec(vec_even) == decimalApprox(fromString<DecimalType>("15"), TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_empty;
        REQUIRE_THROWS_AS(MedianOfVec(vec_empty), std::domain_error);
    }

    SECTION("StandardDeviation (std::vector<DecimalType>)") {
        std::vector<DecimalType> vec_sd = {
            fromString<DecimalType>("1"),
            fromString<DecimalType>("2"),
            fromString<DecimalType>("3"),
            fromString<DecimalType>("4"),
            fromString<DecimalType>("5")
        };
        REQUIRE(StandardDeviation(vec_sd) == decimalApprox(fromString<DecimalType>("1.41421"), TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_same = {
            fromString<DecimalType>("3"),
            fromString<DecimalType>("3"),
            fromString<DecimalType>("3")
        };
        REQUIRE(StandardDeviation(vec_same) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_empty;
        REQUIRE(StandardDeviation(vec_empty) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
    }

    SECTION("StandardDeviation (arithmetic types)") {
        std::vector<int> vi = {1,2,3};
        REQUIRE(StandardDeviation(vi) == Approx(0.8164965809).epsilon(1e-6));

        std::vector<double> vd_empty;
        REQUIRE(StandardDeviation(vd_empty) == 0.0);
    }

    SECTION("MedianAbsoluteDeviation (std::vector<DecimalType>)") {
        std::vector<DecimalType> vec_mad = {
            fromString<DecimalType>("1"),
            fromString<DecimalType>("2"),
            fromString<DecimalType>("3"),
            fromString<DecimalType>("4"),
            fromString<DecimalType>("5")
        };
        REQUIRE(MedianAbsoluteDeviation(vec_mad) == decimalApprox(fromString<DecimalType>("1.4826"), TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_same = {
            fromString<DecimalType>("3"),
            fromString<DecimalType>("3"),
            fromString<DecimalType>("3")
        };
        REQUIRE(MedianAbsoluteDeviation(vec_same) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));

        std::vector<DecimalType> vec_empty;
        REQUIRE(MedianAbsoluteDeviation(vec_empty) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
    }

    SECTION("MedianAbsoluteDeviation (arithmetic types)") {
        std::vector<int> vi = {1,2,3,4,5};
        REQUIRE(MedianAbsoluteDeviation(vi) == Approx(1.4826).epsilon(1e-6));

        std::vector<double> vd_empty;
        REQUIRE(MedianAbsoluteDeviation(vd_empty) == 0.0);
    }

    SECTION("RobustQn") {
        RobustQn<DecimalType> qn_estimator;

        SECTION("n < 2") {
            std::vector<DecimalType> vec0;
            REQUIRE(qn_estimator.getRobustQn(vec0) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
            std::vector<DecimalType> vec1 = {fromString<DecimalType>("10")};
            REQUIRE(qn_estimator.getRobustQn(vec1) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
        }

        SECTION("n = 2") {
            std::vector<DecimalType> vec2 = {
                fromString<DecimalType>("10"),
                fromString<DecimalType>("12")
            };
            REQUIRE(qn_estimator.getRobustQn(vec2) == decimalApprox(fromString<DecimalType>("0.798"), ROBUST_QN_TOL_INDICATORS));
        }

        SECTION("n = 3") {
            std::vector<DecimalType> vec3 = {
                fromString<DecimalType>("10"),
                fromString<DecimalType>("12"),
                fromString<DecimalType>("15")
            };
            REQUIRE(qn_estimator.getRobustQn(vec3) == decimalApprox(fromString<DecimalType>("1.988"), ROBUST_QN_TOL_INDICATORS));
        }

        SECTION("n = 4") {
            std::vector<DecimalType> vec4 = {
                fromString<DecimalType>("10"),
                fromString<DecimalType>("12"),
                fromString<DecimalType>("15"),
                fromString<DecimalType>("18")
            };
            REQUIRE(qn_estimator.getRobustQn(vec4) == decimalApprox(fromString<DecimalType>("1.536"), ROBUST_QN_TOL_INDICATORS));
        }

        SECTION("n = 5") {
            std::vector<DecimalType> vec5 = {
                fromString<DecimalType>("1"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("6"),
                fromString<DecimalType>("8"),
                fromString<DecimalType>("9")
            };
            REQUIRE(qn_estimator.getRobustQn(vec5) == decimalApprox(fromString<DecimalType>("1.688"), ROBUST_QN_TOL_INDICATORS));
        }

        SECTION("n > 9 (asymptotic)") {
            std::vector<DecimalType> vec11;
            for (int i = 1; i <= 11; ++i)
                vec11.push_back(fromString<DecimalType>(std::to_string(i)));
            auto expected = fromString<DecimalType>("3.942081");
            REQUIRE(qn_estimator.getRobustQn(vec11) == decimalApprox(expected, ROBUST_QN_TOL_INDICATORS));
        }

        SECTION("Using RobustQn with NumericTimeSeries constructor") {
            auto ts = createNumericTimeSeriesForTest(daily_tf, {
                {"2023-01-01","10"},
                {"2023-01-02","12"},
                {"2023-01-03","15"}
            });
            RobustQn<DecimalType> from_ts(ts);
            REQUIRE(from_ts.getRobustQn() == decimalApprox(fromString<DecimalType>("1.988"), ROBUST_QN_TOL_INDICATORS));
        }
    }

    SECTION("SampleQuantile") {
        using mkc_timeseries::SampleQuantile;

        SECTION("Basic quantile calculations") {
            std::vector<DecimalType> vec = {
                fromString<DecimalType>("1"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("3"),
                fromString<DecimalType>("4"),
                fromString<DecimalType>("5")
            };
            
            // Test median (0.5 quantile)
            auto vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, 0.5) == decimalApprox(fromString<DecimalType>("3"), TEST_DEC_TOL_INDICATORS));
            
            // Test first quartile (0.25 quantile)
            vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, 0.25) == decimalApprox(fromString<DecimalType>("2"), TEST_DEC_TOL_INDICATORS));
            
            // Test third quartile (0.75 quantile)
            vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, 0.75) == decimalApprox(fromString<DecimalType>("4"), TEST_DEC_TOL_INDICATORS));
            
            // Test minimum (0.0 quantile)
            vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, 0.0) == decimalApprox(fromString<DecimalType>("1"), TEST_DEC_TOL_INDICATORS));
            
            // Test maximum (1.0 quantile)
            vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, 1.0) == decimalApprox(fromString<DecimalType>("5"), TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Edge cases") {
            // Empty vector
            std::vector<DecimalType> empty_vec;
            REQUIRE(SampleQuantile(empty_vec, 0.5) == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TEST_DEC_TOL_INDICATORS));
            
            // Single element
            std::vector<DecimalType> single = {fromString<DecimalType>("42")};
            REQUIRE(SampleQuantile(single, 0.5) == decimalApprox(fromString<DecimalType>("42"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(SampleQuantile(single, 0.0) == decimalApprox(fromString<DecimalType>("42"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(SampleQuantile(single, 1.0) == decimalApprox(fromString<DecimalType>("42"), TEST_DEC_TOL_INDICATORS));
            
            // Two elements
            std::vector<DecimalType> two = {
                fromString<DecimalType>("10"),
                fromString<DecimalType>("20")
            };
            auto two_copy = two;
            REQUIRE(SampleQuantile(two_copy, 0.5) == decimalApprox(fromString<DecimalType>("10"), TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Out of range quantile values") {
            std::vector<DecimalType> vec = {
                fromString<DecimalType>("1"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("3")
            };
            
            // Values outside [0,1] should be clamped
            auto vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, -0.5) == decimalApprox(fromString<DecimalType>("1"), TEST_DEC_TOL_INDICATORS));
            
            vec_copy = vec;
            REQUIRE(SampleQuantile(vec_copy, 1.5) == decimalApprox(fromString<DecimalType>("3"), TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Unsorted input") {
            std::vector<DecimalType> unsorted = {
                fromString<DecimalType>("5"),
                fromString<DecimalType>("1"),
                fromString<DecimalType>("3"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("4")
            };
            
            // Should work correctly even with unsorted input
            REQUIRE(SampleQuantile(unsorted, 0.5) == decimalApprox(fromString<DecimalType>("3"), TEST_DEC_TOL_INDICATORS));
        }
    }

    SECTION("WinsorizeInPlace") {
        using mkc_timeseries::WinsorizeInPlace;

        SECTION("Basic winsorization") {
            std::vector<DecimalType> vec = {
                fromString<DecimalType>("1"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("3"),
                fromString<DecimalType>("4"),
                fromString<DecimalType>("5"),
                fromString<DecimalType>("6"),
                fromString<DecimalType>("7"),
                fromString<DecimalType>("8"),
                fromString<DecimalType>("9"),
                fromString<DecimalType>("10")
            };
            
            auto original = vec;
            
            // Winsorize at 10% (should cap at 10th and 90th percentiles)
            WinsorizeInPlace(vec, 0.1);
            
            // With 10 elements and tau=0.1, the 10th percentile is at index 1 (value 2)
            // and 90th percentile is at index 8 (value 9)
            // So values below 2 should be set to 2, and values above 9 should be set to 9
            REQUIRE(vec[0] == decimalApprox(fromString<DecimalType>("2"), TEST_DEC_TOL_INDICATORS)); // 1 -> 2
            REQUIRE(vec[9] == decimalApprox(fromString<DecimalType>("9"), TEST_DEC_TOL_INDICATORS)); // 10 -> 9
            
            // Middle values should be unchanged
            for (size_t i = 1; i < 9; ++i) {
                REQUIRE(vec[i] == decimalApprox(original[i], TEST_DEC_TOL_INDICATORS));
            }
        }

        SECTION("Edge cases") {
            // Empty vector
            std::vector<DecimalType> empty_vec;
            WinsorizeInPlace(empty_vec, 0.1);
            REQUIRE(empty_vec.empty());
            
            // Single element
            std::vector<DecimalType> single = {fromString<DecimalType>("42")};
            auto single_orig = single;
            WinsorizeInPlace(single, 0.1);
            REQUIRE(single[0] == decimalApprox(fromString<DecimalType>("42"), TEST_DEC_TOL_INDICATORS));
            
            // Two elements - with tau=0.1, both quantiles will be the same values
            std::vector<DecimalType> two = {
                fromString<DecimalType>("10"),
                fromString<DecimalType>("20")
            };
            auto two_orig = two;
            WinsorizeInPlace(two, 0.1);
            // With only 2 elements, the 10th and 90th percentiles are the same as the min and max
            // So no winsorization should occur
            REQUIRE(two[0] == decimalApprox(two_orig[0], TEST_DEC_TOL_INDICATORS));
            REQUIRE(two[1] == decimalApprox(two_orig[1], TEST_DEC_TOL_INDICATORS));
        }

        SECTION("Tau parameter validation") {
            std::vector<DecimalType> vec = {
                fromString<DecimalType>("1"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("3"),
                fromString<DecimalType>("4"),
                fromString<DecimalType>("5")
            };
            auto original = vec;
            
            // tau = 0 should do nothing
            WinsorizeInPlace(vec, 0.0);
            REQUIRE(vec == original);
            
            // Negative tau should be clamped to 0
            vec = original;
            WinsorizeInPlace(vec, -0.1);
            REQUIRE(vec == original);
            
            // tau > 0.25 should be clamped to 0.25
            vec = original;
            WinsorizeInPlace(vec, 0.5);
            // Should behave as if tau = 0.25
            // With 5 elements and tau=0.25, should cap at 25th and 75th percentiles
        }

        SECTION("Extreme outliers") {
            std::vector<DecimalType> vec = {
                fromString<DecimalType>("-1000"),
                fromString<DecimalType>("1"),
                fromString<DecimalType>("2"),
                fromString<DecimalType>("3"),
                fromString<DecimalType>("4"),
                fromString<DecimalType>("5"),
                fromString<DecimalType>("1000")
            };
            
            WinsorizeInPlace(vec, 0.1);
            
            // With 7 elements and tau=0.1, the 10th percentile should be around the 1st element (1)
            // and 90th percentile should be around the 5th element (5)
            // So -1000 should be capped to 1, and 1000 should be capped to 5
            REQUIRE(vec[0] == decimalApprox(fromString<DecimalType>("1"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(vec[6] == decimalApprox(fromString<DecimalType>("5"), TEST_DEC_TOL_INDICATORS));
            
            // Middle values should remain unchanged
            REQUIRE(vec[1] == decimalApprox(fromString<DecimalType>("1"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(vec[2] == decimalApprox(fromString<DecimalType>("2"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(vec[3] == decimalApprox(fromString<DecimalType>("3"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(vec[4] == decimalApprox(fromString<DecimalType>("4"), TEST_DEC_TOL_INDICATORS));
            REQUIRE(vec[5] == decimalApprox(fromString<DecimalType>("5"), TEST_DEC_TOL_INDICATORS));
        }
    }

    SECTION("ComputeQuantileStopAndTargetFromSeries") {
        using mkc_timeseries::ComputeQuantileStopAndTargetFromSeries;

        SECTION("Basic functionality") {
            // Create a synthetic OHLC series with some volatility using helper functions
            OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
            
            // Add entries with varying price movements using createEquityEntry
            auto entry1 = createEquityEntry("20230101", "100", "102", "99", "101", 1000);
            auto entry2 = createEquityEntry("20230102", "101", "103", "100", "102", 1000);
            auto entry3 = createEquityEntry("20230103", "102", "104", "101", "103", 1000);
            auto entry4 = createEquityEntry("20230104", "103", "105", "102", "104", 1000);
            auto entry5 = createEquityEntry("20230105", "104", "106", "103", "105", 1000);
            
            series.addEntry(*entry1);
            series.addEntry(*entry2);
            series.addEntry(*entry3);
            series.addEntry(*entry4);
            series.addEntry(*entry5);
            
            // Add more entries to get above minimum sample size
            for (int i = 6; i <= 25; ++i) {
                std::string dateStr = std::string("202301") + (i < 10 ? "0" : "") + std::to_string(i);
                std::string base = std::to_string(100 + i - 1);
                std::string high = std::to_string(100 + i - 1 + 2);
                std::string low = std::to_string(100 + i - 1 - 1);
                std::string close = std::to_string(100 + i - 1 + 1);
                
                auto entry = createEquityEntry(dateStr, base, high, low, close, 1000);
                series.addEntry(*entry);
            }
            
            auto [profitWidth, stopWidth] = ComputeQuantileStopAndTargetFromSeries(series);
            
            // Basic sanity checks
            REQUIRE(profitWidth >= DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(stopWidth >= DecimalConstants<DecimalType>::DecimalZero);
            
            // Should have reasonable values for this synthetic data
            REQUIRE(profitWidth > DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(stopWidth > DecimalConstants<DecimalType>::DecimalZero);
            
            INFO("Profit Width: " << profitWidth);
            INFO("Stop Width: " << stopWidth);
        }

        SECTION("Different periods") {
            // Create a longer series for multi-period testing
            OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
            
            for (int i = 1; i <= 30; ++i) {
                std::string dateStr = std::string("202301") + (i < 10 ? "0" : "") + std::to_string(i);
                double baseVal = 100 + i * 0.5;
                std::string base = std::to_string(baseVal);
                std::string high = std::to_string(baseVal + 1);
                std::string low = std::to_string(baseVal - 1);
                std::string close = std::to_string(baseVal + 0.5);
                
                auto entry = createEquityEntry(dateStr, base, high, low, close, 1000);
                series.addEntry(*entry);
            }
            
            // Test different periods
            auto [profit1, stop1] = ComputeQuantileStopAndTargetFromSeries(series, 1);
            auto [profit2, stop2] = ComputeQuantileStopAndTargetFromSeries(series, 2);
            
            REQUIRE(profit1 >= DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(stop1 >= DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(profit2 >= DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(stop2 >= DecimalConstants<DecimalType>::DecimalZero);
            
            INFO("Period 1 - Profit: " << profit1 << ", Stop: " << stop1);
            INFO("Period 2 - Profit: " << profit2 << ", Stop: " << stop2);
        }

        SECTION("Error conditions") {
            // Too few entries
            OHLCTimeSeries<DecimalType> small_series(TimeFrame::DAILY, TradingVolume::SHARES);
            auto entry1 = createEquityEntry("20230101", "100", "101", "99", "100", 1000);
            auto entry2 = createEquityEntry("20230102", "100", "101", "99", "100", 1000);
            
            small_series.addEntry(*entry1);
            small_series.addEntry(*entry2);
            
            REQUIRE_THROWS_AS(ComputeQuantileStopAndTargetFromSeries(small_series), std::domain_error);
            
            // Empty series
            OHLCTimeSeries<DecimalType> empty_series(TimeFrame::DAILY, TradingVolume::SHARES);
            REQUIRE_THROWS_AS(ComputeQuantileStopAndTargetFromSeries(empty_series), std::domain_error);
        }

        SECTION("Degenerate case handling") {
            // Create a series with constant prices (no volatility)
            OHLCTimeSeries<DecimalType> flat_series(TimeFrame::DAILY, TradingVolume::SHARES);
            
            for (int i = 1; i <= 25; ++i) {
                std::string dateStr = std::string("202301") + (i < 10 ? "0" : "") + std::to_string(i);
                auto entry = createEquityEntry(dateStr, "100", "100", "100", "100", 1000);
                flat_series.addEntry(*entry);
            }
            
            auto [profitWidth, stopWidth] = ComputeQuantileStopAndTargetFromSeries(flat_series);
            
            // Should handle degenerate case with fallback values
            REQUIRE(profitWidth > DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(stopWidth > DecimalConstants<DecimalType>::DecimalZero);
            
            // Should use the epsilon fallback
            DecimalType eps = DecimalConstants<DecimalType>::createDecimal("1e-6");
            REQUIRE(profitWidth == decimalApprox(eps, TEST_DEC_TOL_INDICATORS));
            REQUIRE(stopWidth == decimalApprox(eps, TEST_DEC_TOL_INDICATORS));
        }

        SECTION("High volatility scenario") {
            // Create a series with high volatility
            OHLCTimeSeries<DecimalType> volatile_series(TimeFrame::DAILY, TradingVolume::SHARES);
            
            for (int i = 1; i <= 25; ++i) {
                std::string dateStr = std::string("202301") + (i < 10 ? "0" : "") + std::to_string(i);
                std::string base = "100";
                int volatility = (i % 2 == 0) ? 10 : -10;
                int closeVal = 100 + volatility;
                std::string close = std::to_string(closeVal);
                std::string high = std::to_string(std::max(100, closeVal));
                std::string low = std::to_string(std::min(100, closeVal));
                
                auto entry = createEquityEntry(dateStr, base, high, low, close, 1000);
                volatile_series.addEntry(*entry);
            }
            
            auto [profitWidth, stopWidth] = ComputeQuantileStopAndTargetFromSeries(volatile_series);
            
            // High volatility should result in wider bands
            REQUIRE(profitWidth > fromString<DecimalType>("5"));
            REQUIRE(stopWidth > fromString<DecimalType>("5"));
            
            INFO("High volatility - Profit: " << profitWidth << ", Stop: " << stopWidth);
        }
    }
}
