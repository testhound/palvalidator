#include <catch2/catch_test_macros.hpp>
#include "TestUtils.h"
#include "DateRange.h"
#include "TimeSeriesEntry.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iterator>

using namespace mkc_timeseries;
using boost::gregorian::date;
using boost::posix_time::to_simple_string;
using boost::posix_time::time_from_string;
using boost::posix_time::time_duration;

TEST_CASE("DateRange: valid construction and getters", "[DateRange]") {
    date d1(2020, 1, 1);
    date d2(2020, 12, 31);
    DateRange range(d1, d2);
    REQUIRE(range.getFirstDate() == d1);
    REQUIRE(range.getLastDate() == d2);
}

TEST_CASE("DateRange: invalid construction throws", "[DateRange]") {
    date d1(2020, 12, 31);
    date d2(2020, 1, 1);
    REQUIRE_THROWS_AS(DateRange(d1, d2), DateRangeException);
}

TEST_CASE("DateRange: copy and assignment", "[DateRange]") {
    date d1(2019, 5, 5);
    date d2(2019, 6, 6);
    DateRange original(d1, d2);
    DateRange copyConstructed(original);
    REQUIRE(copyConstructed == original);

    DateRange assigned = DateRange(d1, d1);
    assigned = original;
    REQUIRE(assigned == original);

    // Self-assignment
    assigned = assigned;
    REQUIRE(assigned == original);
}

TEST_CASE("DateRange: equality and inequality", "[DateRange]") {
    date d1(2021, 7, 1);
    date d2(2021, 7, 31);
    DateRange a(d1, d2);
    DateRange b(d1, d2);
    DateRange c(d1, date(2021, 8, 1));
    REQUIRE(a == b);
    REQUIRE(!(a != b));
    REQUIRE(a != c);
}

TEST_CASE("DateRangeContainer: add and retrieve", "[DateRangeContainer]") {
    DateRangeContainer container;
    REQUIRE(container.getNumEntries() == 0);

    date d1(2022, 3, 1);
    date d2(2022, 3, 31);
    date d3(2021, 1, 1);
    date d4(2021, 1, 31);

    DateRange r1(d1, d2);
    DateRange r2(d3, d4);

    container.addDateRange(r1);
    REQUIRE(container.getNumEntries() == 1);
    container.addDateRange(r2);
    REQUIRE(container.getNumEntries() == 2);

    // getFirstDateRange returns the one with earliest firstDate (r2)
    DateRange firstRange = container.getFirstDateRange();
    REQUIRE(firstRange.getFirstDate() == d3);
    REQUIRE(firstRange.getLastDate() == d4);

    // Iteration covers both ranges
    std::vector<DateRange> ranges;
    for (auto it = container.beginDateRange(); it != container.endDateRange(); ++it) {
        ranges.push_back(it->second);
    }
    REQUIRE(ranges.size() == 2);
    REQUIRE(std::find(ranges.begin(), ranges.end(), r1) != ranges.end());
    REQUIRE(std::find(ranges.begin(), ranges.end(), r2) != ranges.end());
}

TEST_CASE("DateRangeContainer: duplicate add throws", "[DateRangeContainer]") {
    DateRangeContainer container;
    date d1(2020, 4, 1);
    date d2(2020, 4, 30);
    DateRange r1(d1, d2);
    container.addDateRange(r1);

    // Adding another with same firstDate
    DateRange rDuplicate(d1, date(2020, 5, 1));

    std::string expectedMsg =
      std::string("DateRangeContainer: (")
      + to_simple_string(rDuplicate.getFirstDateTime()) + ","
      + to_simple_string(rDuplicate.getLastDateTime())
      + ") date range already exists";
    try {
        container.addDateRange(rDuplicate);
        FAIL("Expected exception on duplicate date range addition");
    } catch (const std::exception& e) {
        REQUIRE(std::string(e.what()) == expectedMsg);
    }
}

TEST_CASE("DateRangeContainer: getFirstDateRange on empty throws", "[DateRangeContainer]") {
    DateRangeContainer container;
    REQUIRE_THROWS_AS((container.getFirstDateRange()), std::domain_error);
}

TEST_CASE("DateRange: ptime constructor and getters", "[DateRange]") {
    // build two distinct ptime values
    ptime p1 = time_from_string("2020-04-01 10:15:30");
    ptime p2 = time_from_string("2020-04-02 23:45:00");

    // construct via ptime ctor :contentReference[oaicite:0]{index=0}
    DateRange r(p1, p2);

    REQUIRE(r.getFirstDateTime() == p1);
    REQUIRE(r.getLastDateTime()  == p2);
}

TEST_CASE("DateRange: ptime constructor throws if last < first", "[DateRange]") {
    ptime early = time_from_string("2021-01-01 00:00:00");
    ptime later = time_from_string("2021-01-02 00:00:00");
    REQUIRE_THROWS_AS(DateRange(later, early), DateRangeException);
}

TEST_CASE("DateRange: date constructor uses default bar time", "[DateRange]") {
    date d1(2021, 8, 15);
    date d2(2021, 9, 15);

    // legacy date‐based ctor should embed getDefaultBarTime() :contentReference[oaicite:1]{index=1}
    DateRange r(d1, d2);

    auto dt1 = r.getFirstDateTime();
    auto dt2 = r.getLastDateTime();
    time_duration bar = getDefaultBarTime();

    // date portion matches…
    REQUIRE(dt1.date() == d1);
    REQUIRE(dt2.date() == d2);
    // …and time portion is getDefaultBarTime()
    REQUIRE(dt1.time_of_day() == bar);
    REQUIRE(dt2.time_of_day() == bar);
}

TEST_CASE("DateRange: operator== / operator!= with ptime‐based ranges", "[DateRange]") {
    ptime p1 = time_from_string("2020-01-01 00:00:00");
    ptime p2 = time_from_string("2020-12-31 23:59:59");
    ptime p3 = time_from_string("2020-12-30 23:59:59");

    DateRange a(p1, p2);
    DateRange b(p1, p2);
    DateRange c(p1, p3);

    REQUIRE(a == b);
    REQUIRE(!(a != b));
    REQUIRE(a != c);
}
