#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TradingPosition.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("OpenPositionBar operations", "[OpenPositionBar]")
{
  using namespace dec;
  typedef decimal<2> EquityType;
  typedef OHLCTimeSeriesEntry<2> EquityTimeSeriesEntry;

  EquityType openPrice1 (fromString<decimal<2>>("200.49"));
  auto open1 = std::make_shared<EquityType> (openPrice1);

  EquityType highPrice1 (fromString<decimal<2>>("201.03"));
  auto high1 = std::make_shared<EquityType> (highPrice1);

  EquityType lowPrice1 (fromString<decimal<2>>("198.59"));
  auto low1 = std::make_shared<EquityType> (lowPrice1);

  EquityType closePrice1 (fromString<decimal<2>>("201.02"));
  auto close1 = std::make_shared<EquityType> (closePrice1);

  boost::gregorian::date refDate1 (2016, Jan, 4);
  auto date1 = std::make_shared<boost::gregorian::date> (refDate1);

  volume_t vol1 = 213990200;

  auto entry1 = std::make_shared<EquityTimeSeriesEntry>(date1, open1, high1, low1, 
							close1, vol1, TimeFrame::DAILY);

  OpenPositionBar<2> bar1 (entry1);

  EquityType openPrice2 (fromString<decimal<2>>("205.13"));
  auto open2 = std::make_shared<EquityType> (openPrice2);

  EquityType highPrice2 (fromString<decimal<2>>("205.89"));
  auto high2 = std::make_shared<EquityType> (highPrice2);

  EquityType lowPrice2 (fromString<decimal<2>>("203.87"));
  auto low2 = std::make_shared<EquityType> (lowPrice2);

  EquityType closePrice2 (fromString<decimal<2>>("203.87"));
  auto close2 = std::make_shared<EquityType> (closePrice2);

  boost::gregorian::date refDate2 (2015, Dec, 31);
  auto date2 = std::make_shared<boost::gregorian::date> (refDate2);

  volume_t vol2 = 114877900;

  auto entry2 = std::make_shared<EquityTimeSeriesEntry>(date2, open2, high2, low2, 
							close2, vol2, TimeFrame::DAILY);
  OpenPositionBar<2> bar2 (entry2);

  EquityType openPrice3 (fromString<decimal<2>>("205.13"));

  EquityType highPrice3 (fromString<decimal<2>>("205.89"));

  EquityType lowPrice3 (fromString<decimal<2>>("203.87"));

  EquityType closePrice3 (fromString<decimal<2>>("203.87"));

  boost::gregorian::date refDate3 (2015, Dec, 31);

  volume_t vol3 = 114877900;

  auto entry3 = std::make_shared<EquityTimeSeriesEntry>(refDate3, openPrice3, highPrice3, 
							lowPrice3, 
							closePrice3, vol3, TimeFrame::DAILY);
  OpenPositionBar<2> bar3 (entry3);

  auto errorShareVolume = std::make_shared<TradingVolume>(114877900, 
							  TradingVolume::CONTRACTS);

  REQUIRE (bar1.getOpenValue() == openPrice1);
  REQUIRE (bar1.getHighValue() == highPrice1);
  REQUIRE (bar1.getLowValue() == lowPrice1);
  REQUIRE (bar1.getCloseValue() == closePrice1);
  REQUIRE (bar1.getDate() == refDate1);
  REQUIRE (bar1.getVolume() == vol1);
  REQUIRE (bar2.getOpenValue() == openPrice2);
  REQUIRE (bar2.getHighValue() == highPrice2);
  REQUIRE (bar2.getLowValue() == lowPrice2);
  REQUIRE (bar2.getCloseValue() == closePrice2);
  REQUIRE (bar2.getDate() == refDate2);
  REQUIRE (bar2.getVolume() == vol2);

  REQUIRE (bar3.getOpenValue() == bar2.getOpenValue());
  REQUIRE (bar3.getHighValue() == bar2.getHighValue());
  REQUIRE (bar3.getLowValue() == bar2.getLowValue());
  REQUIRE (bar3.getCloseValue() == bar2.getCloseValue());
  REQUIRE (bar3.getDate() == bar2.getDate());
  REQUIRE (bar3.getVolume() == bar2.getVolume());
 
  SECTION ("TimeSeriesEntry inequality tests");
  {
    REQUIRE (bar1 != bar2);
  }

  SECTION ("TimeSeriesEntry equality tests");
  {
    auto entry = std::make_shared<EquityTimeSeriesEntry>(*entry1);
    REQUIRE (bar2 == bar3);
  }

  
}

