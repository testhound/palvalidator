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
// DecimalType is defined in TestUtils.h as typedef dec::decimal<7> DecimalType;
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
}
