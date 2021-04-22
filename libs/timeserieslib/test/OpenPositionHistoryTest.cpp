#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TradingPosition.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;


TEST_CASE ("OpenPositionHistory operations", "[OpenPositionHistory]")
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

  
  auto bar1 = std::make_shared<OpenPositionBar<DecimalType>>(*entry6);
  auto bar2 = std::make_shared<OpenPositionBar<DecimalType>>(*entry5);
  auto bar3 = std::make_shared<OpenPositionBar<DecimalType>>(*entry4);
  auto bar4 = std::make_shared<OpenPositionBar<DecimalType>>(*entry3);
  auto bar5 = std::make_shared<OpenPositionBar<DecimalType>>(*entry2);
  auto bar6 = std::make_shared<OpenPositionBar<DecimalType>>(*entry1);
  auto bar7 = std::make_shared<OpenPositionBar<DecimalType>>(*entry0);

  OpenPositionHistory<DecimalType> positionHistory(*entry6);
  positionHistory.addBar (*bar2);
  positionHistory.addBar (*bar3);
  positionHistory.addBar (*bar4);
  positionHistory.addBar (*bar5);
  positionHistory.addBar (*bar6);
  positionHistory.addBar (*bar7);
   
  OpenPositionHistory<DecimalType> positionHistory2(*entry5);
  positionHistory2.addBar (*bar3);

  REQUIRE (positionHistory.numBarsInPosition() == 7);

  SECTION ("OpenPositionHistory getFirstDate()");
  {
    REQUIRE (positionHistory.getFirstDate() == TimeSeriesDate (2015, Dec, 28));
  }

  SECTION ("OpenPositionHistory getLastDate()");
  {
    REQUIRE (positionHistory.getLastDate() == TimeSeriesDate (2016, Jan, 6));
  }

   SECTION ("OpenPositionHistory getLastClose()");
  {
    DecimalType num(dec::fromString<DecimalType>("198.82"));
    REQUIRE (positionHistory.getLastClose() == num);;
  }

  SECTION ("OpenPositionHistory Copy Constructor and assignment operator");
  {
    OpenPositionHistory<DecimalType> history(positionHistory);
    REQUIRE (history.numBarsInPosition() == 7);
    REQUIRE (history.getFirstDate() == TimeSeriesDate (2015, Dec, 28));
    REQUIRE (history.getLastDate() == TimeSeriesDate (2016, Jan, 6));

    DecimalType num(dec::fromString<DecimalType>("198.82"));
    REQUIRE (history.getLastClose() == num);

    history = positionHistory2;
    REQUIRE (history.numBarsInPosition() == 2);
    REQUIRE (history.getFirstDate() == TimeSeriesDate (2015, Dec, 29));
    REQUIRE (history.getLastDate() == TimeSeriesDate (2015, Dec, 30));

    DecimalType num2(dec::fromString<DecimalType>("205.93"));
    REQUIRE (history.getLastClose() == num2);
  }


  SECTION ("OpenPositionHistory Iterator operations");
  {
    OpenPositionHistory<DecimalType>::PositionBarIterator it = positionHistory.beginPositionBarHistory();

    REQUIRE (it->first == TimeSeriesDate (2015, Dec, 28));
    REQUIRE (it->second == *bar1);
    it++;

    REQUIRE (it->first == TimeSeriesDate (2015, Dec, 29));
    REQUIRE (it->second == *bar2);
    it++;

    REQUIRE (it->first == TimeSeriesDate (2015, Dec, 30));
    REQUIRE (it->second == *bar3);

    it = positionHistory.endPositionBarHistory();
    it--;

    REQUIRE (it->first == TimeSeriesDate (2016, Jan, 6));
    REQUIRE (it->second == *bar7);

  }

SECTION ("OpenPositionHistory ConstIterator operations");
  {
    OpenPositionHistory<DecimalType>::ConstPositionBarIterator it = positionHistory.beginPositionBarHistory();

    REQUIRE (it->first == TimeSeriesDate (2015, Dec, 28));
    REQUIRE (it->second == *bar1);
    it++;

    REQUIRE (it->first == TimeSeriesDate (2015, Dec, 29));
    REQUIRE (it->second == *bar2);
    it++;

    REQUIRE (it->first == TimeSeriesDate (2015, Dec, 30));
    REQUIRE (it->second == *bar3);

    it = positionHistory.endPositionBarHistory();
    it--;

    REQUIRE (it->first == TimeSeriesDate (2016, Jan, 6));
    REQUIRE (it->second == *bar7);

  }
}

