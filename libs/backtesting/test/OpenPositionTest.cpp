#include <catch2/catch_test_macros.hpp>
#include "TradingPosition.h"
#include "TestUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>
using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("OpenPosition operations", "[OpenPosition]")
{
  auto entry0 = createTimeSeriesEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry1 = createTimeSeriesEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry2 = createTimeSeriesEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry3 = createTimeSeriesEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry4 = createTimeSeriesEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createTimeSeriesEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry6 = createTimeSeriesEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);

  TradingVolume oneShare (1, TradingVolume::SHARES);
  
  OpenLongPosition<DecimalType> longPosition1(createDecimal("206.51"),  *entry5, oneShare);
  longPosition1.addBar(*entry4);
  longPosition1.addBar(*entry3);
  longPosition1.addBar(*entry2);

  OpenShortPosition<DecimalType> shortPosition1(createDecimal("206.51"),  *entry5, oneShare);
  shortPosition1.addBar(*entry4);
  shortPosition1.addBar(*entry3);
  shortPosition1.addBar(*entry2);

  REQUIRE (longPosition1.isPositionOpen());
  REQUIRE_FALSE (longPosition1.isPositionClosed());

  REQUIRE (longPosition1.getEntryDate() == TimeSeriesDate (2015, Dec, 29));
  REQUIRE (longPosition1.getEntryPrice() ==  DecimalType (dec::fromString<DecimalType>("206.51")));
  REQUIRE (longPosition1.getTradingUnits() == oneShare);
  
  REQUIRE (longPosition1.getNumBarsInPosition() == 4);
  REQUIRE (longPosition1.getNumBarsSinceEntry() == 3);
  REQUIRE (longPosition1.getLastClose() == DecimalType (dec::fromString<DecimalType>("201.02")));

  REQUIRE (shortPosition1.isPositionOpen());
  REQUIRE_FALSE (shortPosition1.isPositionClosed());

  REQUIRE (shortPosition1.getEntryDate() == TimeSeriesDate (2015, Dec, 29));
  REQUIRE (shortPosition1.getEntryPrice() ==  DecimalType (dec::fromString<DecimalType>("206.51")));
  REQUIRE (shortPosition1.getTradingUnits() == oneShare);
  
  REQUIRE (shortPosition1.getNumBarsInPosition() == 4);
  REQUIRE (shortPosition1.getNumBarsSinceEntry() == 3);
  REQUIRE (shortPosition1.getLastClose() == DecimalType (dec::fromString<DecimalType>("201.02")));

  SECTION ("OpenPosition getPercentReturn()")
  {
    REQUIRE (longPosition1.getPercentReturn() == DecimalType (dec::fromString<DecimalType>("-2.65846700")));
    REQUIRE_FALSE (longPosition1.isWinningPosition());
    REQUIRE (longPosition1.isLosingPosition());
    REQUIRE (shortPosition1.getPercentReturn() == DecimalType (dec::fromString<DecimalType>("2.65846700")));
    REQUIRE (shortPosition1.isWinningPosition());
    REQUIRE_FALSE (shortPosition1.isLosingPosition());
  }

  SECTION ("OpenPosition getTradeReturn()")
  {
    DecimalType longReturn(dec::fromString<DecimalType>("-2.65846700")/DecimalConstants<DecimalType>::DecimalOneHundred);

      REQUIRE (longPosition1.getTradeReturn() == longReturn);

      DecimalType shortReturn(dec::fromString<DecimalType>("2.65846700")/DecimalConstants<DecimalType>::DecimalOneHundred);
      REQUIRE (shortPosition1.getTradeReturn() == shortReturn);
  }

  SECTION ("OpenPosition getTradeMultiplier()")
  {
    DecimalType longMult(longPosition1.getTradeReturn() + DecimalConstants<DecimalType>::DecimalOne);

    REQUIRE (longPosition1.getTradeReturnMultiplier() == longMult);

    DecimalType shortMult(shortPosition1.getTradeReturn() + DecimalConstants<DecimalType>::DecimalOne);

    REQUIRE (shortPosition1.getTradeReturnMultiplier() == shortMult);
  }

  SECTION ("OpenLongPosition Iterator tests")
  {
    OpenPosition<DecimalType>::PositionBarIterator it = longPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first.date() ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry2);
  }

  SECTION ("OpenLongPosition ConstIterator tests")
  {
    OpenPosition<DecimalType>::ConstPositionBarIterator it = longPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first.date() ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry2);
  }

  SECTION ("OpenShortPosition Iterator tests")
  {
    OpenPosition<DecimalType>::PositionBarIterator it = shortPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first.date() ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry2);
  }

  SECTION ("OpenShortPosition Iterator tests")
  {
    OpenPosition<DecimalType>::ConstPositionBarIterator it = shortPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first.date() ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry2);
  }
  
  SECTION ("Throw exception on long getExitPrice")
    {
      REQUIRE_THROWS (longPosition1.getExitPrice());
    }

  SECTION ("Throw exception on long getExitDate")
    {
      REQUIRE_THROWS (longPosition1.getExitDate());
    }

  SECTION ("Throw exception on short getExitPrice")
    {
      REQUIRE_THROWS (shortPosition1.getExitPrice());
    }

  SECTION ("Throw exception on short getExitDate")
    {
      REQUIRE_THROWS (shortPosition1.getExitDate());
    }

  SECTION("OpenPosition getEntryDateTime returns underlying ptime", "[OpenPosition][ptime]") {
    // entry5 was used to construct both positions
    const auto& expected = entry5->getDateTime();
    REQUIRE(longPosition1.getEntryDateTime() == expected);
    REQUIRE(shortPosition1.getEntryDateTime() == expected);
  }

  SECTION("OpenPosition getExitDateTime throws on open", "[OpenPosition][ptime]") {
    REQUIRE_THROWS_AS(longPosition1.getExitDateTime(), TradingPositionException);
    REQUIRE_THROWS_AS(shortPosition1.getExitDateTime(), TradingPositionException);
  }

  SECTION("OpenPosition intraday entryDateTime matches explicit ptime", "[OpenPosition][ptime]") {
    TradingVolume oneShare(1, TradingVolume::SHARES);

    // 1) Build an intraday bar at 2025-05-26 09:45:00
    auto intraEntry = createTimeSeriesEntry(
        "20250526", "09:45:00",
        "100.00", "101.00", "99.50", "100.75", "12345"
    );

    OpenLongPosition<DecimalType> longPos(
        intraEntry->getOpenValue(),
        *intraEntry,
        oneShare
    );
    ptime expectedLong = time_from_string("2025-05-26 09:45:00");
    REQUIRE(longPos.getEntryDateTime() == expectedLong);
    REQUIRE(longPos.getEntryDate()     == expectedLong.date());

    // 2) Build a short position at 2025-05-27 14:20:15
    auto intraEntry2 = createTimeSeriesEntry(
        "20250527", "14:20:15",
        "200.00", "205.00", "195.00", "202.00", "500"
    );

    OpenShortPosition<DecimalType> shortPos(
        intraEntry2->getOpenValue(),
        *intraEntry2,
        oneShare
    );
    ptime expectedShort = time_from_string("2025-05-27 14:20:15");
    REQUIRE(shortPos.getEntryDateTime() == expectedShort);
    REQUIRE(shortPos.getEntryDate()     == expectedShort.date());
  }
}

