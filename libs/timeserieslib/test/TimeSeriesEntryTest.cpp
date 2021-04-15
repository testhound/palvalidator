#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesEntry.h"
#include "../BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace dec;

TEST_CASE ("TimeSeriesEntry operations", "[TimeSeriesEntry]")
{
  

  DecimalType openPrice1 (fromString<DecimalType>("200.49"));
  auto open1 = std::make_shared<DecimalType> (openPrice1);

  DecimalType highPrice1 (fromString<DecimalType>("201.03"));
  auto high1 = std::make_shared<DecimalType> (highPrice1);

  DecimalType lowPrice1 (fromString<DecimalType>("198.59"));
  auto low1 = std::make_shared<DecimalType> (lowPrice1);

  DecimalType closePrice1 (fromString<DecimalType>("201.02"));
  auto close1 = std::make_shared<DecimalType> (closePrice1);

  boost::gregorian::date refDate1 (2016, Jan, 4);
  auto date1 = std::make_shared<boost::gregorian::date> (refDate1);

  DecimalType vol1(13990200);

  NumericTimeSeriesEntry<DecimalType> aNonOHLCTimeSeriesEntry  (refDate1, closePrice1,  
						   TimeFrame::DAILY);
  REQUIRE (aNonOHLCTimeSeriesEntry.getDate() == refDate1);
  REQUIRE (aNonOHLCTimeSeriesEntry.getValue() == closePrice1);
  REQUIRE (aNonOHLCTimeSeriesEntry.getTimeFrame() == TimeFrame::DAILY);

  NumericTimeSeriesEntry<DecimalType> aNonOHLCTimeSeriesEntry2  (refDate1, highPrice1,  
						       TimeFrame::DAILY);
  REQUIRE (aNonOHLCTimeSeriesEntry2.getDate() == refDate1);
  REQUIRE (aNonOHLCTimeSeriesEntry2.getValue() == highPrice1);
  REQUIRE (aNonOHLCTimeSeriesEntry2.getTimeFrame() == TimeFrame::DAILY);
  REQUIRE_FALSE (aNonOHLCTimeSeriesEntry == aNonOHLCTimeSeriesEntry2);
  REQUIRE (aNonOHLCTimeSeriesEntry != aNonOHLCTimeSeriesEntry2);

  auto entry1 = std::make_shared<EntryType>(refDate1, openPrice1, highPrice1, lowPrice1, 
							closePrice1, vol1, TimeFrame::DAILY);

  DecimalType openPrice2 (fromString<DecimalType>("205.13"));
  auto open2 = std::make_shared<DecimalType> (openPrice2);

  DecimalType highPrice2 (fromString<DecimalType>("205.89"));
  auto high2 = std::make_shared<DecimalType> (highPrice2);

  DecimalType lowPrice2 (fromString<DecimalType>("203.87"));
  auto low2 = std::make_shared<DecimalType> (lowPrice2);

  DecimalType closePrice2 (fromString<DecimalType>("203.87"));
  auto close2 = std::make_shared<DecimalType> (closePrice2);

  boost::gregorian::date refDate2 (2015, Dec, 31);
  auto date2 = std::make_shared<boost::gregorian::date> (refDate2);

  DecimalType vol2 (114877900);

  auto entry2 = std::make_shared<EntryType>(refDate2, openPrice2, highPrice2, lowPrice2, 
					    closePrice2, vol2, TimeFrame::DAILY);


  DecimalType openPrice3 (fromString<DecimalType>("205.13"));

  DecimalType highPrice3 (fromString<DecimalType>("205.89"));

  DecimalType lowPrice3 (fromString<DecimalType>("203.87"));

  DecimalType closePrice3 (fromString<DecimalType>("203.87"));

  boost::gregorian::date refDate3 (2015, Dec, 31);

  DecimalType vol3 (114877900);

  auto entry3 = std::make_shared<EntryType>(refDate3, openPrice3, highPrice3, 
							lowPrice3, 
							closePrice3, vol3, TimeFrame::DAILY);
  auto errorShareVolume = std::make_shared<TradingVolume>(114877900, 
							  TradingVolume::CONTRACTS);



  REQUIRE (entry1->getOpenValue() == openPrice1);
  REQUIRE (entry1->getHighValue() == highPrice1);
  REQUIRE (entry1->getLowValue() == lowPrice1);
  REQUIRE (entry1->getCloseValue() == closePrice1);
  REQUIRE (entry1->getDateValue() == refDate1);
  REQUIRE (entry1->getVolumeValue() == vol1);
  REQUIRE (entry1->getTimeFrame() == TimeFrame::DAILY);
  REQUIRE (entry2->getOpenValue() == openPrice2);
  REQUIRE (entry2->getHighValue() == highPrice2);
  REQUIRE (entry2->getLowValue() == lowPrice2);
  REQUIRE (entry2->getCloseValue() == closePrice2);
  REQUIRE (entry2->getDateValue() == refDate2);
  REQUIRE (entry2->getVolumeValue() == vol2);
  REQUIRE (entry2->getTimeFrame() == TimeFrame::DAILY);

  REQUIRE (entry3->getOpenValue() == entry2->getOpenValue());
  REQUIRE (entry3->getHighValue() == entry2->getHighValue());
  REQUIRE (entry3->getLowValue() == entry2->getLowValue());
  REQUIRE (entry3->getCloseValue() == entry2->getCloseValue());
  REQUIRE (entry3->getDateValue() == entry2->getDateValue());
  REQUIRE (entry3->getVolumeValue() == entry2->getVolumeValue());
  REQUIRE (entry3->getTimeFrame() == entry2->getTimeFrame());
  REQUIRE (*entry2 == *entry3);
 
  SECTION ("TimeSeriesEntry inequality tests")
  {
    REQUIRE (*entry1 != *entry2);
  }

  SECTION ("TimeSeriesEntry equality tests")
  {
    auto entry = std::make_shared<EntryType>(*entry1);
    REQUIRE (*entry == *entry1);
  }

  SECTION ("Monthly Time Frame Tests")
    {
	auto entry = createTimeSeriesEntry ("19930226", "44.23", "45.13", "42.82", "44.42", "0",
					TimeFrame::MONTHLY);
	REQUIRE (entry->getTimeFrame() == TimeFrame::MONTHLY);

	boost::gregorian::date monthlyDate = entry->getDateValue();
	//REQUIRE (is_first_of_month (monthlyDate));
	REQUIRE (monthlyDate.year() == 1993);
	REQUIRE (monthlyDate.month().as_number() == 2);
	REQUIRE (monthlyDate.day().as_number() ==26);
	
    }

  SECTION ("Weekly Time Frame Tests")
    {
	auto entry = createTimeSeriesEntry ("19990806", "132.75", "134.75", "128.84", "130.38", "0",
					TimeFrame::WEEKLY);
	REQUIRE (entry->getTimeFrame() == TimeFrame::WEEKLY);

	boost::gregorian::date weeklyDate = entry->getDateValue();
	//REQUIRE (is_first_of_week (weeklyDate));
	REQUIRE (weeklyDate.year() == 1999);
	REQUIRE (weeklyDate.month().as_number() == 8);
	REQUIRE (weeklyDate.day().as_number() == 6);
	
    }

  SECTION ("EntryType exception tests")
  {
    DecimalType lowPrice_temp1 (fromString<DecimalType>("206.87"));
    auto low_temp1 = std::make_shared<DecimalType> (lowPrice_temp1);

    DecimalType closePrice_temp1 (fromString<DecimalType>("208.31"));
    auto close_temp1 = std::make_shared<DecimalType> (closePrice_temp1);


    // high < open
    REQUIRE_THROWS (std::make_shared<EntryType>(refDate2, highPrice2, openPrice2, lowPrice2, 
							    closePrice2, vol2, TimeFrame::DAILY));

    // high < low
    REQUIRE_THROWS (std::make_shared<EntryType>(refDate2, openPrice2, highPrice2, lowPrice_temp1, 
							     closePrice2, vol2, TimeFrame::DAILY));

    // high < close

    REQUIRE_THROWS (std::make_shared<EntryType>(refDate2, openPrice2, highPrice2, lowPrice2, 
							     closePrice_temp1, vol2, 
							    TimeFrame::DAILY));
    

    // low > open
    DecimalType lowPrice_temp2 (fromString<DecimalType>("205.14"));
    auto low_temp2 = std::make_shared<DecimalType> (lowPrice_temp2);
  
    REQUIRE_THROWS (std::make_shared<EntryType>(refDate2, openPrice2, highPrice2, lowPrice_temp2, 
							    closePrice2, vol2, TimeFrame::DAILY));

    // low > close
    DecimalType lowPrice_temp3 (fromString<DecimalType>("203.88"));
    auto low_temp3 = std::make_shared<DecimalType> (lowPrice_temp3);
  
    REQUIRE_THROWS (std::make_shared<EntryType>(refDate2, openPrice2, highPrice2, lowPrice_temp3, 
							    closePrice2, vol2, TimeFrame::DAILY));
  }
}

