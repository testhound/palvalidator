#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesEntry.h"
#include "../BoostDateHelper.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace dec;
typedef decimal<7> EquityType;
typedef OHLCTimeSeriesEntry<7> EquityTimeSeriesEntry;

std::shared_ptr<OHLCTimeSeriesEntry<7>>
    createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol,
		       TimeFrame::Duration frame)
  {
    auto date1 = std::make_shared<date> (from_undelimited_string(dateString));
    auto open1 = std::make_shared<EquityType> (fromString<decimal<7>>(openPrice));
    auto high1 = std::make_shared<EquityType> (fromString<decimal<7>>(highPrice));
    auto low1 = std::make_shared<EquityType> (fromString<decimal<7>>(lowPrice));
    auto close1 = std::make_shared<EquityType> (fromString<decimal<7>>(closePrice));
    return std::make_shared<OHLCTimeSeriesEntry<7>>(date1, open1, high1, low1, 
						close1, vol, frame);
  }

TEST_CASE ("TimeSeriesEntry operations", "[TimeSeriesEntry]")
{
  

  EquityType openPrice1 (fromString<decimal<7>>("200.49"));
  auto open1 = std::make_shared<EquityType> (openPrice1);

  EquityType highPrice1 (fromString<decimal<7>>("201.03"));
  auto high1 = std::make_shared<EquityType> (highPrice1);

  EquityType lowPrice1 (fromString<decimal<7>>("198.59"));
  auto low1 = std::make_shared<EquityType> (lowPrice1);

  EquityType closePrice1 (fromString<decimal<7>>("201.02"));
  auto close1 = std::make_shared<EquityType> (closePrice1);

  boost::gregorian::date refDate1 (2016, Jan, 4);
  auto date1 = std::make_shared<boost::gregorian::date> (refDate1);

  volume_t vol1 = 213990200;

  NumericTimeSeriesEntry<7> aNonOHLCTimeSeriesEntry  (refDate1, closePrice1,  
						   TimeFrame::DAILY);
  REQUIRE (aNonOHLCTimeSeriesEntry.getDate() == refDate1);
  REQUIRE (aNonOHLCTimeSeriesEntry.getValue() == closePrice1);
  REQUIRE (aNonOHLCTimeSeriesEntry.getTimeFrame() == TimeFrame::DAILY);

  NumericTimeSeriesEntry<7> aNonOHLCTimeSeriesEntry2  (refDate1, highPrice1,  
						       TimeFrame::DAILY);
  REQUIRE (aNonOHLCTimeSeriesEntry2.getDate() == refDate1);
  REQUIRE (aNonOHLCTimeSeriesEntry2.getValue() == highPrice1);
  REQUIRE (aNonOHLCTimeSeriesEntry2.getTimeFrame() == TimeFrame::DAILY);
  REQUIRE_FALSE (aNonOHLCTimeSeriesEntry == aNonOHLCTimeSeriesEntry2);
  REQUIRE (aNonOHLCTimeSeriesEntry != aNonOHLCTimeSeriesEntry2);

  auto entry1 = std::make_shared<EquityTimeSeriesEntry>(date1, open1, high1, low1, 
							close1, vol1, TimeFrame::DAILY);

  EquityType openPrice2 (fromString<decimal<7>>("205.13"));
  auto open2 = std::make_shared<EquityType> (openPrice2);

  EquityType highPrice2 (fromString<decimal<7>>("205.89"));
  auto high2 = std::make_shared<EquityType> (highPrice2);

  EquityType lowPrice2 (fromString<decimal<7>>("203.87"));
  auto low2 = std::make_shared<EquityType> (lowPrice2);

  EquityType closePrice2 (fromString<decimal<7>>("203.87"));
  auto close2 = std::make_shared<EquityType> (closePrice2);

  boost::gregorian::date refDate2 (2015, Dec, 31);
  auto date2 = std::make_shared<boost::gregorian::date> (refDate2);

  volume_t vol2 = 114877900;

  auto entry2 = std::make_shared<EquityTimeSeriesEntry>(date2, open2, high2, low2, 
							close2, vol2, TimeFrame::DAILY);


  EquityType openPrice3 (fromString<decimal<7>>("205.13"));

  EquityType highPrice3 (fromString<decimal<7>>("205.89"));

  EquityType lowPrice3 (fromString<decimal<7>>("203.87"));

  EquityType closePrice3 (fromString<decimal<7>>("203.87"));

  boost::gregorian::date refDate3 (2015, Dec, 31);

  volume_t vol3 = 114877900;

  auto entry3 = std::make_shared<EquityTimeSeriesEntry>(refDate3, openPrice3, highPrice3, 
							lowPrice3, 
							closePrice3, vol3, TimeFrame::DAILY);
  auto errorShareVolume = std::make_shared<TradingVolume>(114877900, 
							  TradingVolume::CONTRACTS);



  REQUIRE (*entry1->getOpen() == openPrice1);
  REQUIRE (*entry1->getHigh() == highPrice1);
  REQUIRE (*entry1->getLow() == lowPrice1);
  REQUIRE (*entry1->getClose() == closePrice1);
  REQUIRE (*entry1->getDate() == refDate1);
  REQUIRE (entry1->getVolume() == vol1);
  REQUIRE (entry1->getTimeFrame() == TimeFrame::DAILY);
  REQUIRE (*entry2->getOpen() == openPrice2);
  REQUIRE (*entry2->getHigh() == highPrice2);
  REQUIRE (*entry2->getLow() == lowPrice2);
  REQUIRE (*entry2->getClose() == closePrice2);
  REQUIRE (*entry2->getDate() == refDate2);
  REQUIRE (entry2->getVolume() == vol2);
  REQUIRE (entry2->getTimeFrame() == TimeFrame::DAILY);

  REQUIRE (entry3->getOpenValue() == entry2->getOpenValue());
  REQUIRE (entry3->getHighValue() == entry2->getHighValue());
  REQUIRE (entry3->getLowValue() == entry2->getLowValue());
  REQUIRE (entry3->getCloseValue() == entry2->getCloseValue());
  REQUIRE (entry3->getDateValue() == entry2->getDateValue());
  REQUIRE (entry3->getVolume() == entry2->getVolume());
  REQUIRE (entry3->getTimeFrame() == entry2->getTimeFrame());
  REQUIRE (*entry2 == *entry3);
 
  SECTION ("TimeSeriesEntry inequality tests")
  {
    REQUIRE (*entry1 != *entry2);
  }

  SECTION ("TimeSeriesEntry equality tests")
  {
    auto entry = std::make_shared<EquityTimeSeriesEntry>(*entry1);
    REQUIRE (*entry == *entry1);
  }

  SECTION ("Monthly Time Frame Tests")
    {
	auto entry = createEquityEntry ("19930226", "44.23", "45.13", "42.82", "44.42", 0,
					TimeFrame::MONTHLY);
	REQUIRE (entry->getTimeFrame() == TimeFrame::MONTHLY);

	boost::gregorian::date monthlyDate = entry->getDateValue();
	REQUIRE (is_first_of_month (monthlyDate));
	REQUIRE (monthlyDate.year() == 1993);
	REQUIRE (monthlyDate.month().as_number() == 2);
	REQUIRE (monthlyDate.day().as_number() == 1);
	
    }

  SECTION ("Weekly Time Frame Tests")
    {
	auto entry = createEquityEntry ("19990806", "132.75", "134.75", "128.84", "130.38", 0,
					TimeFrame::WEEKLY);
	REQUIRE (entry->getTimeFrame() == TimeFrame::WEEKLY);

	boost::gregorian::date weeklyDate = entry->getDateValue();
	REQUIRE (is_first_of_week (weeklyDate));
	REQUIRE (weeklyDate.year() == 1999);
	REQUIRE (weeklyDate.month().as_number() == 8);
	REQUIRE (weeklyDate.day().as_number() == 1);
	
    }

  SECTION ("EquityTimeSeriesEntry exception tests")
  {
    EquityType lowPrice_temp1 (fromString<decimal<7>>("206.87"));
    auto low_temp1 = std::make_shared<EquityType> (lowPrice_temp1);

    EquityType closePrice_temp1 (fromString<decimal<7>>("208.31"));
    auto close_temp1 = std::make_shared<EquityType> (closePrice_temp1);


    // high < open
    REQUIRE_THROWS (std::make_shared<EquityTimeSeriesEntry>(date2, high2, open2, low2, 
							    close2, vol2, TimeFrame::DAILY));

    // high < low
    REQUIRE_THROWS (std::make_shared<EquityTimeSeriesEntry>(date2, open2, high2, low_temp1, 
							     close2, vol2, TimeFrame::DAILY));

    // high < close

    REQUIRE_THROWS (std::make_shared<EquityTimeSeriesEntry>(date2, open2, high2, low2, 
							     close_temp1, vol2, 
							    TimeFrame::DAILY));
    

    // low > open
    EquityType lowPrice_temp2 (fromString<decimal<7>>("205.14"));
    auto low_temp2 = std::make_shared<EquityType> (lowPrice_temp2);
  
    REQUIRE_THROWS (std::make_shared<EquityTimeSeriesEntry>(date2, open2, high2, low_temp2, 
							    close2, vol2, TimeFrame::DAILY));

    // low > close
    EquityType lowPrice_temp3 (fromString<decimal<7>>("203.88"));
    auto low_temp3 = std::make_shared<EquityType> (lowPrice_temp3);
  
    REQUIRE_THROWS (std::make_shared<EquityTimeSeriesEntry>(date2, open2, high2, low_temp3, 
							    close2, vol2, TimeFrame::DAILY));
  }
}

