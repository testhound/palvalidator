#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../DateRange.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("DateRange operations", "[DateRange]")
{
  using namespace dec;
  typedef DecimalType PercentType;

  boost::gregorian::date range1FirstDate (2002, Jan, 01);
  boost::gregorian::date range1LastDate (2007, Dec, 31);

  boost::gregorian::date range2FirstDate (2009, Jan, 01);
  boost::gregorian::date range2LastDate (2013, Dec, 31);

  boost::gregorian::date range3FirstDate (2018, Jan, 01);
  boost::gregorian::date range3LastDate (2021, Dec, 31);

  DateRange range1(range1FirstDate, range1LastDate);
  DateRange range2(range2FirstDate, range2LastDate);
  DateRange range3(range3FirstDate, range3LastDate);

  SECTION ("DateRange tests");
  {
    DateRangeContainer dateRanges;
    REQUIRE (dateRanges.getNumEntries() == 0);
    REQUIRE(dateRanges.beginDateRange() == dateRanges.endDateRange());
    REQUIRE_THROWS (dateRanges.getFirstDateRange());

    dateRanges.addDateRange(range2);
    REQUIRE (dateRanges.getNumEntries() == 1);

    // Adding a duplicate range should throw an exception
    REQUIRE_THROWS(dateRanges.addDateRange(range2));
    dateRanges.addDateRange(range1);
    REQUIRE (dateRanges.getNumEntries() == 2);

    // Adding a duplicate range should throw an exception
    REQUIRE_THROWS(dateRanges.addDateRange(range1));
    dateRanges.addDateRange(range3);
    REQUIRE (dateRanges.getNumEntries() == 3);

    DateRange r(dateRanges.getFirstDateRange());
    REQUIRE(r == range1);

    auto it = dateRanges.beginDateRange();
    DateRange r1(it->second);
    REQUIRE(r1 == range1);

    it++;
    DateRange r2(it->second);
    REQUIRE(r2 == range2);

    it++;
    DateRange r3(it->second);
    REQUIRE(r3 == range3);

    it++;
    REQUIRE(it == dateRanges.endDateRange());

  }
}
