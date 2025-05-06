#include <catch2/catch_test_macros.hpp>
#include "TradingPosition.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("OpenPositionBar operations", "[OpenPositionBar]")
{
  DecimalType openPrice1 (dec::fromString<DecimalType>("200.49"));
  auto open1 = std::make_shared<DecimalType> (openPrice1);

  DecimalType highPrice1 (dec::fromString<DecimalType>("201.03"));
  auto high1 = std::make_shared<DecimalType> (highPrice1);

  DecimalType lowPrice1 (dec::fromString<DecimalType>("198.59"));
  auto low1 = std::make_shared<DecimalType> (lowPrice1);

  DecimalType closePrice1 (dec::fromString<DecimalType>("201.02"));
  auto close1 = std::make_shared<DecimalType> (closePrice1);

  boost::gregorian::date refDate1 (2016, Jan, 4);
  auto date1 = std::make_shared<boost::gregorian::date> (refDate1);

  volume_t vol1 = 213990200;

  auto entry1 = std::make_shared<EntryType>(*date1, openPrice1, highPrice1, lowPrice1, 
							closePrice1, DecimalType((dec::int64) vol1), TimeFrame::DAILY);

  OpenPositionBar<DecimalType> bar1 (*entry1);

  DecimalType openPrice2 (dec::fromString<DecimalType>("205.13"));
  auto open2 = std::make_shared<DecimalType> (openPrice2);

  DecimalType highPrice2 (dec::fromString<DecimalType>("205.89"));
  auto high2 = std::make_shared<DecimalType> (highPrice2);

  DecimalType lowPrice2 (dec::fromString<DecimalType>("203.87"));
  auto low2 = std::make_shared<DecimalType> (lowPrice2);

  DecimalType closePrice2 (dec::fromString<DecimalType>("203.87"));
  auto close2 = std::make_shared<DecimalType> (closePrice2);

  boost::gregorian::date refDate2 (2015, Dec, 31);
  auto date2 = std::make_shared<boost::gregorian::date> (refDate2);

  volume_t vol2 = 114877900;

  auto entry2 = std::make_shared<EntryType>(*date2, openPrice2, highPrice2, lowPrice2, 
							closePrice2, DecimalType((dec::int64) vol2), TimeFrame::DAILY);
  OpenPositionBar<DecimalType> bar2 (*entry2);

  DecimalType openPrice3 (dec::fromString<DecimalType>("205.13"));

  DecimalType highPrice3 (dec::fromString<DecimalType>("205.89"));

  DecimalType lowPrice3 (dec::fromString<DecimalType>("203.87"));

  DecimalType closePrice3 (dec::fromString<DecimalType>("203.87"));

  boost::gregorian::date refDate3 (2015, Dec, 31);

  volume_t vol3 = 114877900;

  auto entry3 = std::make_shared<EntryType>(refDate3, openPrice3, highPrice3, 
							lowPrice3, 
							closePrice3, DecimalType((dec::int64) vol3), TimeFrame::DAILY);
  OpenPositionBar<DecimalType> bar3 (*entry3);

  auto errorShareVolume = std::make_shared<TradingVolume>(114877900, 
							  TradingVolume::CONTRACTS);

  REQUIRE (bar1.getOpenValue() == openPrice1);
  REQUIRE (bar1.getHighValue() == highPrice1);
  REQUIRE (bar1.getLowValue() == lowPrice1);
  REQUIRE (bar1.getCloseValue() == closePrice1);
  REQUIRE (bar1.getDate() == refDate1);
  REQUIRE (bar1.getVolumeValue() == DecimalType((dec::int64) vol1));
  REQUIRE (bar2.getOpenValue() == openPrice2);
  REQUIRE (bar2.getHighValue() == highPrice2);
  REQUIRE (bar2.getLowValue() == lowPrice2);
  REQUIRE (bar2.getCloseValue() == closePrice2);
  REQUIRE (bar2.getDate() == refDate2);
  REQUIRE (bar2.getVolumeValue() == DecimalType((dec::int64) vol2));

  REQUIRE (bar3.getOpenValue() == bar2.getOpenValue());
  REQUIRE (bar3.getHighValue() == bar2.getHighValue());
  REQUIRE (bar3.getLowValue() == bar2.getLowValue());
  REQUIRE (bar3.getCloseValue() == bar2.getCloseValue());
  REQUIRE (bar3.getDate() == bar2.getDate());
  REQUIRE (bar3.getVolumeValue() == bar2.getVolumeValue());
 
  SECTION ("TimeSeriesEntry inequality tests");
  {
    REQUIRE (bar1 != bar2);
  }

  SECTION ("TimeSeriesEntry equality tests");
  {
    auto entry = std::make_shared<EntryType>(*entry1);
    REQUIRE (bar2 == bar3);
  }

  
}

