#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <stdexcept>

// Project headers
#include "TimeSeriesIndicators.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "TestUtils.h"
#include "DecimalConstants.h"
#include "number.h"
#include "BoostDateHelper.h"

using namespace mkc_timeseries;
using num::fromString;

// ======= DecimalApproxMatcher (mirrors existing style) =======
template<typename Decimal>
struct DecimalApproxMatcher {
    Decimal expected;
    Decimal tolerance;
    DecimalApproxMatcher(const Decimal& e, const Decimal& t) : expected(e), tolerance(t) {}
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
std::ostream& operator<<(std::ostream& os, const DecimalApproxMatcher<Decimal>& approx) {
    os << "expected " << approx.expected << " ± " << approx.tolerance;
    return os;
}
// ======= End DecimalApproxMatcher =======

typedef num::DefaultNumber DecimalType;
static const DecimalType TOL_SMALL = fromString<DecimalType>("0.00001");
static const DecimalType TOL_MED   = fromString<DecimalType>("0.0001");

static std::string to_yyyymmdd(const std::string& iso_date)
{
    // strip '-' from "YYYY-MM-DD" → "YYYYMMDD"
    std::string out; out.reserve(iso_date.size());
    for (char ch : iso_date) if (ch != '-') out.push_back(ch);
    return out;
}

// Local helper to build a NumericTimeSeries from (date, value) pairs
static NumericTimeSeries<DecimalType> makeNumTS(
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

// Build an OHLC series with provided closes (opens=prev close, hi/lo ~ close), using TestUtils helper
static OHLCTimeSeries<DecimalType> makeOhlcFromCloses(
    const std::vector<std::pair<std::string,double>>& dateClose,
    TimeFrame::Duration tf = TimeFrame::DAILY)
{
    OHLCTimeSeries<DecimalType> series(tf, TradingVolume::SHARES);
    double prevClose = dateClose.front().second;
    for (size_t i = 0; i < dateClose.size(); ++i) {
        const auto& d  = dateClose[i].first;
        const double c = dateClose[i].second;
        const double o = (i == 0) ? c : prevClose;
        
        // Ensure OHLC constraints: high >= max(open, close), low <= min(open, close)
        const double hi = std::max(o, c) * 1.001;  // Add small margin to ensure high >= close
        const double lo = std::min(o, c) * 0.999;  // Subtract small margin to ensure low <= close
        
        auto e = createEquityEntry( to_yyyymmdd(d),
                                   std::to_string(o),
                                   std::to_string(hi),
                                   std::to_string(lo),
                                   std::to_string(c),
                                   1000);
        series.addEntry(*e);
        prevClose = c;
    }
    return series;
}

TEST_CASE("Adaptive Volatility – New Indicator Functions", "[AdaptiveVolatility]") {
    TimeFrame::Duration daily = TimeFrame::DAILY;

    SECTION("RollingRSquaredSeries — perfect linear and constant cases") {
        // y = 2*x + 3 over 10 days → in any 5-bar window, R² should be 1
        std::vector<std::pair<std::string,std::string>> lin;
        for (int i = 1; i <= 10; ++i) {
            double y = 2.0 * i + 3.0;
            lin.push_back({ "2023-01-" + std::string(i<10 ? "0":"") + std::to_string(i), std::to_string(y) });
        }
        auto yts = makeNumTS(daily, lin);
        auto r2 = RollingRSquaredSeries(yts, /*lookback=*/5);
        REQUIRE(r2.getNumEntries() == 6);
        for (auto it = r2.beginRandomAccess(); it != r2.endRandomAccess(); ++it) {
            auto v = it->getValue();
            REQUIRE(v == decimalApprox(fromString<DecimalType>("1.0"), TOL_SMALL));
        }

        // Constant series → R² should be 0
        auto constTs = makeNumTS(daily, {
            {"2023-02-01","5"},{"2023-02-02","5"},{"2023-02-03","5"},{"2023-02-04","5"},{"2023-02-05","5"}
        });
        auto r2c = RollingRSquaredSeries(constTs, 4);
        REQUIRE(r2c.getNumEntries() == 2);
        for (auto it = r2c.beginRandomAccess(); it != r2c.endRandomAccess(); ++it) {
            REQUIRE(it->getValue() == decimalApprox(DecimalConstants<DecimalType>::DecimalZero, TOL_SMALL));
        }

        // Not enough data → empty
        auto shortTs = makeNumTS(daily, {{"2023-03-01","1"},{"2023-03-02","2"}});
        auto r2short = RollingRSquaredSeries(shortTs, 5);
        REQUIRE(r2short.getNumEntries() == 0);

        // lookback < 2 → throws
        REQUIRE_THROWS_AS(RollingRSquaredSeries(yts, 1), std::domain_error);
    }

    SECTION("PercentRankSeries — basic correctness and edge cases") {
        // Mixed values to produce nontrivial ranks
        auto s = makeNumTS(daily, {
            {"2023-01-01","10"},
            {"2023-01-02","20"},
            {"2023-01-03","15"},
            {"2023-01-04","30"},
            {"2023-01-05","25"}
        });
        auto pr = PercentRankSeries(s, /*window=*/3);
        REQUIRE(pr.getNumEntries() == 3);
        // Windows (inclusive of current):
        // i=2 -> [10,20,15], current=15 => rank = 2/3
        // i=3 -> [20,15,30], current=30 => rank = 3/3
        // i=4 -> [15,30,25], current=25 => rank = 2/3
        auto it = pr.beginRandomAccess();
        REQUIRE(it->getValue() == decimalApprox(fromString<DecimalType>("0.6666667"), TOL_MED)); ++it;
        REQUIRE(it->getValue() == decimalApprox(fromString<DecimalType>("1.0"),        TOL_SMALL)); ++it;
        REQUIRE(it->getValue() == decimalApprox(fromString<DecimalType>("0.6666667"), TOL_MED));

        // window < 2 → throws
        REQUIRE_THROWS_AS(PercentRankSeries(s, 1), std::domain_error);

        // window > length → empty
        auto emptyOut = PercentRankSeries(s, 10);
        REQUIRE(emptyOut.getNumEntries() == 0);

        // empty input → empty output
        NumericTimeSeries<DecimalType> emptyTs(daily);
        auto prEmpty = PercentRankSeries(emptyTs, 3);
        REQUIRE(prEmpty.getNumEntries() == 0);
    }

    SECTION("AdaptiveVolatilityAnnualizedSeries — constant daily return yields constant annualized vol") {
        // Build a 40-bar geometric series with constant simple return r = 1%
        const double r = 0.01;
        std::vector<std::pair<std::string,double>> dx;
        double c = 100.0;
        
        // Generate 40 valid dates across multiple months
        std::vector<std::string> dates = {
            "2023-01-01", "2023-01-02", "2023-01-03", "2023-01-04", "2023-01-05",
            "2023-01-06", "2023-01-07", "2023-01-08", "2023-01-09", "2023-01-10",
            "2023-01-11", "2023-01-12", "2023-01-13", "2023-01-14", "2023-01-15",
            "2023-01-16", "2023-01-17", "2023-01-18", "2023-01-19", "2023-01-20",
            "2023-01-21", "2023-01-22", "2023-01-23", "2023-01-24", "2023-01-25",
            "2023-01-26", "2023-01-27", "2023-01-28", "2023-01-29", "2023-01-30",
            "2023-01-31", "2023-02-01", "2023-02-02", "2023-02-03", "2023-02-04",
            "2023-02-05", "2023-02-06", "2023-02-07", "2023-02-08", "2023-02-09"
        };
        
        for (int i = 0; i < 40; ++i) {
            dx.push_back({ dates[i], c });
            c *= (1.0 + r);
        }
        auto ohlc = makeOhlcFromCloses(dx);
        // Any r2Period works; EMA of constant squared returns stays constant.
        auto vol = AdaptiveVolatilityAnnualizedSeries<DecimalType>(ohlc, /*r2Period=*/10, /*annFactor=*/252.0);

        // Output length should be n - (r2Period - 1)
        REQUIRE(vol.getNumEntries() == 40 - (10 - 1));

        // Expected annualized vol = sqrt( (r^2) * 252 ) = |r| * sqrt(252)
        // sqrt(252) ≈ 15.874507, so r=0.01 → ≈ 0.15874507
        const DecimalType expected = fromString<DecimalType>("0.1587451");
        for (auto it2 = vol.beginRandomAccess(); it2 != vol.endRandomAccess(); ++it2) {
            REQUIRE(it2->getValue() == decimalApprox(expected, TOL_MED));
        }

        // r2Period < 2 → throws
        REQUIRE_THROWS_AS(AdaptiveVolatilityAnnualizedSeries<DecimalType>(ohlc, 1, 252.0),
                          std::domain_error);

        // Too few bars → empty
        OHLCTimeSeries<DecimalType> shortOhlc(daily, TradingVolume::SHARES);
        auto e1 = createEquityEntry("20230101","100","100.1","99.9","100",1000);
        auto e2 = createEquityEntry("20230102","100","101.1","99.9","101",1000);
        shortOhlc.addEntry(*e1); shortOhlc.addEntry(*e2);
        auto volShort = AdaptiveVolatilityAnnualizedSeries<DecimalType>(shortOhlc, 5, 252.0);
        REQUIRE(volShort.getNumEntries() == 0);

        // Division by zero in previous close (c_{t-1} == 0) → throws
        OHLCTimeSeries<DecimalType> zeroPrev(daily, TradingVolume::SHARES);
        auto z1 = createEquityEntry("20230101","0","0","0","0",1000);
        auto z2 = createEquityEntry("20230102","0","1","0","1",1000);
        zeroPrev.addEntry(*z1); zeroPrev.addEntry(*z2);
        REQUIRE_THROWS_AS(AdaptiveVolatilityAnnualizedSeries<DecimalType>(zeroPrev, 2, 252.0),
                          std::domain_error);
    }

    SECTION("AdaptiveVolatilityPercentRankAnnualizedSeries — shape & bounds") {
        // Mostly flat price (zero returns) then a single large up-move → rank near/at the top at the end
        std::vector<std::pair<std::string,double>> dx;
        
        // Generate valid dates for 34 trading days across multiple months
        std::vector<std::string> dates = {
            "2023-03-01", "2023-03-02", "2023-03-03", "2023-03-04", "2023-03-05",
            "2023-03-06", "2023-03-07", "2023-03-08", "2023-03-09", "2023-03-10",
            "2023-03-11", "2023-03-12", "2023-03-13", "2023-03-14", "2023-03-15",
            "2023-03-16", "2023-03-17", "2023-03-18", "2023-03-19", "2023-03-20",
            "2023-03-21", "2023-03-22", "2023-03-23", "2023-03-24", "2023-03-25",
            "2023-03-26", "2023-03-27", "2023-03-28", "2023-03-29", "2023-03-30",
            "2023-03-31", "2023-04-01", "2023-04-02", "2023-04-03"
        };
        
        // First 24 days with flat price
        for (int i = 0; i < 24; ++i) {
            dx.push_back({ dates[i], 100.0 });
        }
        
        // 25th day with spike
        dx.push_back({ dates[24], 110.0 }); // +10% spike
        
        // Remaining days flat again
        for (int i = 25; i < 34; ++i) {
            dx.push_back({ dates[i], 110.0 });
        }

        auto ohlc = makeOhlcFromCloses(dx);
        const uint32_t r2Period = 5;
        const uint32_t prPeriod = 5;

        auto pr = AdaptiveVolatilityPercentRankAnnualizedSeries<DecimalType>(ohlc, r2Period, prPeriod, 252.0);

        // Length check: first the vol has (n - (r2Period - 1)) points, then percent-rank reduces by (prPeriod - 1)
        const size_t n = dx.size();
        const size_t volLen = n - (r2Period - 1);
        const size_t expectedLen = (volLen >= prPeriod) ? (volLen - prPeriod + 1) : 0;
        REQUIRE(pr.getNumEntries() == expectedLen);

        // Bounds: all ranks in [0,1]
        for (auto it = pr.beginRandomAccess(); it != pr.endRandomAccess(); ++it) {
            const auto v = it->getValue();
            REQUIRE(v >= DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(v <= fromString<DecimalType>("1.0"));
        }

        // prPeriod < 2 → throws
        REQUIRE_THROWS_AS(AdaptiveVolatilityPercentRankAnnualizedSeries<DecimalType>(ohlc, r2Period, 1, 252.0),
                          std::domain_error);
    }
}
