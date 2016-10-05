#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TradingPosition.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef dec::decimal<7> DecimalType;
typedef OHLCTimeSeriesEntry<7> EntryType;

std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString)
{
  return std::make_shared<DecimalType> (fromString<DecimalType>(valueString));
}

DecimalType
createDecimal(const std::string& valueString)
{
  return fromString<DecimalType>(valueString);
}

std::shared_ptr<EntryType>
    createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    auto date1 = std::make_shared<date> (from_undelimited_string(dateString));
    auto open1 = std::make_shared<DecimalType> (fromString<DecimalType>(openPrice));
    auto high1 = std::make_shared<DecimalType> (fromString<DecimalType>(highPrice));
    auto low1 = std::make_shared<DecimalType> (fromString<DecimalType>(lowPrice));
    auto close1 = std::make_shared<DecimalType> (fromString<DecimalType>(closePrice));
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }


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
  
  OpenLongPosition<7> longPosition1(createDecimal("206.51"),  entry5, oneShare);
  longPosition1.addBar(entry4);
  longPosition1.addBar(entry3);
  longPosition1.addBar(entry2);

  OpenShortPosition<7> shortPosition1(createDecimal("206.51"),  entry5, oneShare);
  shortPosition1.addBar(entry4);
  shortPosition1.addBar(entry3);
  shortPosition1.addBar(entry2);

  REQUIRE (longPosition1.isPositionOpen());
  REQUIRE_FALSE (longPosition1.isPositionClosed());

  REQUIRE (longPosition1.getEntryDate() == TimeSeriesDate (2015, Dec, 29));
  REQUIRE (longPosition1.getEntryPrice() ==  DecimalType (fromString<DecimalType>("206.51")));
  REQUIRE (longPosition1.getTradingUnits() == oneShare);
  
  REQUIRE (longPosition1.getNumBarsInPosition() == 4);
  REQUIRE (longPosition1.getNumBarsSinceEntry() == 3);
  REQUIRE (longPosition1.getLastClose() == DecimalType (fromString<DecimalType>("201.02")));

  REQUIRE (shortPosition1.isPositionOpen());
  REQUIRE_FALSE (shortPosition1.isPositionClosed());

  REQUIRE (shortPosition1.getEntryDate() == TimeSeriesDate (2015, Dec, 29));
  REQUIRE (shortPosition1.getEntryPrice() ==  DecimalType (fromString<DecimalType>("206.51")));
  REQUIRE (shortPosition1.getTradingUnits() == oneShare);
  
  REQUIRE (shortPosition1.getNumBarsInPosition() == 4);
  REQUIRE (shortPosition1.getNumBarsSinceEntry() == 3);
  REQUIRE (shortPosition1.getLastClose() == DecimalType (fromString<DecimalType>("201.02")));

  SECTION ("OpenPosition getPercentReturn()")
  {
    REQUIRE (longPosition1.getPercentReturn() == DecimalType (fromString<DecimalType>("-2.6584700")));
    REQUIRE_FALSE (longPosition1.isWinningPosition());
    REQUIRE (longPosition1.isLosingPosition());
    REQUIRE (shortPosition1.getPercentReturn() == DecimalType (fromString<DecimalType>("2.6584700")));
    REQUIRE (shortPosition1.isWinningPosition());
    REQUIRE_FALSE (shortPosition1.isLosingPosition());
  }

  SECTION ("OpenPosition getTradeReturn()")
  {
    DecimalType longReturn(fromString<DecimalType>("-2.6584700")/DecimalConstants<7>::DecimalOneHundred);

      REQUIRE (longPosition1.getTradeReturn() == longReturn);

      DecimalType shortReturn(fromString<DecimalType>("2.6584700")/DecimalConstants<7>::DecimalOneHundred);
      REQUIRE (shortPosition1.getTradeReturn() == shortReturn);
  }

  SECTION ("OpenPosition getTradeMultiplier()")
  {
    DecimalType longMult(longPosition1.getTradeReturn() + DecimalConstants<7>::DecimalOne);

    REQUIRE (longPosition1.getTradeReturnMultiplier() == longMult);

    DecimalType shortMult(shortPosition1.getTradeReturn() + DecimalConstants<7>::DecimalOne);

    REQUIRE (shortPosition1.getTradeReturnMultiplier() == shortMult);
  }

  SECTION ("OpenLongPosition Iterator tests")
  {
    OpenPosition<7>::PositionBarIterator it = longPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry2);
  }

  SECTION ("OpenLongPosition ConstIterator tests")
  {
    OpenPosition<7>::ConstPositionBarIterator it = longPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry2);
  }

  SECTION ("OpenShortPosition Iterator tests")
  {
    OpenPosition<7>::PositionBarIterator it = shortPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry2);
  }

  SECTION ("OpenShortPosition Iterator tests")
  {
    OpenPosition<7>::ConstPositionBarIterator it = shortPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (2015, Dec, 30));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry4);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (2016, Jan, 4));
    REQUIRE (*it->second->getTimeSeriesEntry() == *entry2);
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
}

