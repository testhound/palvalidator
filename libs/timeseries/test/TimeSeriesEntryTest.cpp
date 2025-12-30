#include <catch2/catch_test_macros.hpp>
#include <utility>
#include "TimeSeriesEntry.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;
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

  SECTION ("Intraday Time Frame Tests")
    {
      auto entry1 = createTimeSeriesEntry ("20210405", "09:00", "105.99", "106.57", "105.93", "106.54", "0");
      auto entry2 = createTimeSeriesEntry ("20210405", "10:00", "106.54", "107.29", "106.38", "107.10", "0");
      REQUIRE (entry1->getTimeFrame() == TimeFrame::INTRADAY);
      boost::gregorian::date intradayDate(entry1->getDateValue());

      REQUIRE (intradayDate.year() == 2021);
      REQUIRE (intradayDate.month().as_number() == 4);
      REQUIRE (intradayDate.day().as_number() == 5);

      REQUIRE (*entry1 != *entry2);
      DecimalType open(dec::fromString<DecimalType>("106.54"));
      DecimalType high(dec::fromString<DecimalType>("107.29"));
      DecimalType low(dec::fromString<DecimalType>("106.38"));
      DecimalType close(dec::fromString<DecimalType>("107.10"));
      DecimalType vol(dec::fromString<DecimalType>("0"));
      ptime aDate(date(2021, Apr, 05), time_duration(10, 0, 0));
      EntryType entry3(aDate, open, high, low, close,  vol, TimeFrame::INTRADAY);
      REQUIRE (*entry2 == entry3);

      time_duration intradayTime(entry1->getBarTime());
      REQUIRE(intradayTime.hours() == 9);
      REQUIRE(intradayTime.minutes() == 0);
      REQUIRE(intradayTime.seconds() == 0);

      DecimalType openPrice (fromString<DecimalType>("105.99"));
      REQUIRE(entry1->getOpenValue() == openPrice);
	
      DecimalType highPrice (fromString<DecimalType>("106.57"));
      REQUIRE(entry1->getHighValue() == highPrice);

      DecimalType lowPrice (fromString<DecimalType>("105.93"));
      REQUIRE(entry1->getLowValue() == lowPrice);

      DecimalType closePrice (fromString<DecimalType>("106.54"));
      REQUIRE(entry1->getCloseValue() == closePrice);

      ptime referenceDateTime(date(2021, 4, 5), time_duration(9, 0, 0));
      REQUIRE(entry1->getDateTime() == referenceDateTime);
    }

  SECTION ("Monthlyw Time Frame Tests")
    {
	auto entry = createTimeSeriesEntry ("19930226", "44.23", "45.13", "42.82", "44.42", "0",
					TimeFrame::MONTHLY);
	REQUIRE (entry->getTimeFrame() == TimeFrame::MONTHLY);

	boost::gregorian::date monthlyDate = entry->getDateValue();
	//REQUIRE (is_first_of_month (monthlyDate));
	REQUIRE (monthlyDate.year() == 1993);
	REQUIRE (monthlyDate.month().as_number() == 2);
	REQUIRE (monthlyDate.day().as_number() == 26);
	
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

TEST_CASE ("NumericTimeSeriesEntry move operations", "[NumericTimeSeriesEntry][Move]")
{
  SECTION ("Move constructor")
  {
    DecimalType value1(fromString<DecimalType>("123.45"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    // Create original entry
    NumericTimeSeriesEntry<DecimalType> original(refDate, value1, TimeFrame::DAILY);
    
    // Capture original values
    auto originalDate = original.getDate();
    auto originalValue = original.getValue();
    auto originalTimeFrame = original.getTimeFrame();
    
    // Move construct
    NumericTimeSeriesEntry<DecimalType> moved(std::move(original));
    
    // Verify moved object has correct values
    REQUIRE(moved.getDate() == originalDate);
    REQUIRE(moved.getValue() == originalValue);
    REQUIRE(moved.getTimeFrame() == originalTimeFrame);
  }
  
  SECTION ("Move assignment operator")
  {
    DecimalType value1(fromString<DecimalType>("123.45"));
    DecimalType value2(fromString<DecimalType>("678.90"));
    boost::gregorian::date refDate1(2023, Jan, 15);
    boost::gregorian::date refDate2(2023, Feb, 20);
    
    // Create entries
    NumericTimeSeriesEntry<DecimalType> entry1(refDate1, value1, TimeFrame::DAILY);
    NumericTimeSeriesEntry<DecimalType> entry2(refDate2, value2, TimeFrame::WEEKLY);
    
    // Capture entry2 values before move
    auto originalDate = entry2.getDate();
    auto originalValue = entry2.getValue();
    auto originalTimeFrame = entry2.getTimeFrame();
    
    // Move assign
    entry1 = std::move(entry2);
    
    // Verify entry1 now has entry2's original values
    REQUIRE(entry1.getDate() == originalDate);
    REQUIRE(entry1.getValue() == originalValue);
    REQUIRE(entry1.getTimeFrame() == originalTimeFrame);
  }
  
  SECTION ("Move assignment to self (should handle gracefully)")
  {
    DecimalType value(fromString<DecimalType>("123.45"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    NumericTimeSeriesEntry<DecimalType> entry(refDate, value, TimeFrame::DAILY);
    
    // Self move-assignment - intentional test for robustness
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    entry = std::move(entry);
#pragma GCC diagnostic pop
    
    // Should still be valid
    REQUIRE(entry.getDate() == refDate);
    REQUIRE(entry.getValue() == value);
    REQUIRE(entry.getTimeFrame() == TimeFrame::DAILY);
  }
}

TEST_CASE ("OHLCTimeSeriesEntry move operations", "[OHLCTimeSeriesEntry][Move]")
{
  SECTION ("Move constructor")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    // Create original entry
    OHLCTimeSeriesEntry<DecimalType> original(refDate, open, high, low, close, 
                                               volume, TimeFrame::DAILY);
    
    // Capture original values
    auto originalDate = original.getDateValue();
    auto originalOpen = original.getOpenValue();
    auto originalHigh = original.getHighValue();
    auto originalLow = original.getLowValue();
    auto originalClose = original.getCloseValue();
    auto originalVolume = original.getVolumeValue();
    auto originalTimeFrame = original.getTimeFrame();
    
    // Move construct
    OHLCTimeSeriesEntry<DecimalType> moved(std::move(original));
    
    // Verify moved object has correct values
    REQUIRE(moved.getDateValue() == originalDate);
    REQUIRE(moved.getOpenValue() == originalOpen);
    REQUIRE(moved.getHighValue() == originalHigh);
    REQUIRE(moved.getLowValue() == originalLow);
    REQUIRE(moved.getCloseValue() == originalClose);
    REQUIRE(moved.getVolumeValue() == originalVolume);
    REQUIRE(moved.getTimeFrame() == originalTimeFrame);
  }
  
  SECTION ("Move assignment operator")
  {
    DecimalType open1(fromString<DecimalType>("100.00"));
    DecimalType high1(fromString<DecimalType>("105.00"));
    DecimalType low1(fromString<DecimalType>("99.00"));
    DecimalType close1(fromString<DecimalType>("103.00"));
    DecimalType volume1(fromString<DecimalType>("1000000"));
    boost::gregorian::date refDate1(2023, Jan, 15);
    
    DecimalType open2(fromString<DecimalType>("200.00"));
    DecimalType high2(fromString<DecimalType>("210.00"));
    DecimalType low2(fromString<DecimalType>("195.00"));
    DecimalType close2(fromString<DecimalType>("205.00"));
    DecimalType volume2(fromString<DecimalType>("2000000"));
    boost::gregorian::date refDate2(2023, Feb, 20);
    
    // Create entries
    OHLCTimeSeriesEntry<DecimalType> entry1(refDate1, open1, high1, low1, close1, 
                                             volume1, TimeFrame::DAILY);
    OHLCTimeSeriesEntry<DecimalType> entry2(refDate2, open2, high2, low2, close2, 
                                             volume2, TimeFrame::WEEKLY);
    
    // Capture entry2 values before move
    auto originalDate = entry2.getDateValue();
    auto originalOpen = entry2.getOpenValue();
    auto originalHigh = entry2.getHighValue();
    auto originalLow = entry2.getLowValue();
    auto originalClose = entry2.getCloseValue();
    auto originalVolume = entry2.getVolumeValue();
    auto originalTimeFrame = entry2.getTimeFrame();
    
    // Move assign
    entry1 = std::move(entry2);
    
    // Verify entry1 now has entry2's original values
    REQUIRE(entry1.getDateValue() == originalDate);
    REQUIRE(entry1.getOpenValue() == originalOpen);
    REQUIRE(entry1.getHighValue() == originalHigh);
    REQUIRE(entry1.getLowValue() == originalLow);
    REQUIRE(entry1.getCloseValue() == originalClose);
    REQUIRE(entry1.getVolumeValue() == originalVolume);
    REQUIRE(entry1.getTimeFrame() == originalTimeFrame);
  }
  
  SECTION ("Move assignment to self (should handle gracefully)")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    OHLCTimeSeriesEntry<DecimalType> entry(refDate, open, high, low, close,
                                            volume, TimeFrame::DAILY);
    
    // Self move-assignment - intentional test for robustness
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    entry = std::move(entry);
#pragma GCC diagnostic pop
    
    // Should still be valid
    REQUIRE(entry.getDateValue() == refDate);
    REQUIRE(entry.getOpenValue() == open);
    REQUIRE(entry.getHighValue() == high);
    REQUIRE(entry.getLowValue() == low);
    REQUIRE(entry.getCloseValue() == close);
    REQUIRE(entry.getVolumeValue() == volume);
    REQUIRE(entry.getTimeFrame() == TimeFrame::DAILY);
  }
}

TEST_CASE ("NumericTimeSeriesEntry comprehensive tests", "[NumericTimeSeriesEntry]")
{
  SECTION ("Copy constructor")
  {
    DecimalType value(fromString<DecimalType>("123.45"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    NumericTimeSeriesEntry<DecimalType> original(refDate, value, TimeFrame::DAILY);
    NumericTimeSeriesEntry<DecimalType> copy(original);
    
    REQUIRE(copy.getDate() == original.getDate());
    REQUIRE(copy.getValue() == original.getValue());
    REQUIRE(copy.getTimeFrame() == original.getTimeFrame());
    REQUIRE(copy == original);
  }
  
  SECTION ("Assignment operator")
  {
    DecimalType value1(fromString<DecimalType>("123.45"));
    DecimalType value2(fromString<DecimalType>("678.90"));
    boost::gregorian::date refDate1(2023, Jan, 15);
    boost::gregorian::date refDate2(2023, Feb, 20);
    
    NumericTimeSeriesEntry<DecimalType> entry1(refDate1, value1, TimeFrame::DAILY);
    NumericTimeSeriesEntry<DecimalType> entry2(refDate2, value2, TimeFrame::WEEKLY);
    
    entry1 = entry2;
    
    REQUIRE(entry1.getDate() == entry2.getDate());
    REQUIRE(entry1.getValue() == entry2.getValue());
    REQUIRE(entry1.getTimeFrame() == entry2.getTimeFrame());
    REQUIRE(entry1 == entry2);
  }
  
  SECTION ("Self-assignment")
  {
    DecimalType value(fromString<DecimalType>("123.45"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    NumericTimeSeriesEntry<DecimalType> entry(refDate, value, TimeFrame::DAILY);
    entry = entry;
    
    REQUIRE(entry.getDate() == refDate);
    REQUIRE(entry.getValue() == value);
    REQUIRE(entry.getTimeFrame() == TimeFrame::DAILY);
  }
  
  SECTION ("DateTime with time component")
  {
    DecimalType value(fromString<DecimalType>("123.45"));
    ptime dateTime(date(2023, Jan, 15), time_duration(14, 30, 0));
    
    NumericTimeSeriesEntry<DecimalType> entry(dateTime, value, TimeFrame::INTRADAY);
    
    REQUIRE(entry.getDateTime() == dateTime);
    REQUIRE(entry.getDate() == date(2023, Jan, 15));
    REQUIRE(entry.getBarTime().hours() == 14);
    REQUIRE(entry.getBarTime().minutes() == 30);
    REQUIRE(entry.getBarTime().seconds() == 0);
    REQUIRE(entry.getValue() == value);
    REQUIRE(entry.getTimeFrame() == TimeFrame::INTRADAY);
  }
  
  SECTION ("Different time frames")
  {
    DecimalType value(fromString<DecimalType>("123.45"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    NumericTimeSeriesEntry<DecimalType> dailyEntry(refDate, value, TimeFrame::DAILY);
    NumericTimeSeriesEntry<DecimalType> weeklyEntry(refDate, value, TimeFrame::WEEKLY);
    NumericTimeSeriesEntry<DecimalType> monthlyEntry(refDate, value, TimeFrame::MONTHLY);
    NumericTimeSeriesEntry<DecimalType> intradayEntry(refDate, value, TimeFrame::INTRADAY);
    
    REQUIRE(dailyEntry.getTimeFrame() == TimeFrame::DAILY);
    REQUIRE(weeklyEntry.getTimeFrame() == TimeFrame::WEEKLY);
    REQUIRE(monthlyEntry.getTimeFrame() == TimeFrame::MONTHLY);
    REQUIRE(intradayEntry.getTimeFrame() == TimeFrame::INTRADAY);
    
    // Entries with same date and value but different timeframes should not be equal
    REQUIRE(dailyEntry != weeklyEntry);
    REQUIRE(dailyEntry != monthlyEntry);
    REQUIRE(weeklyEntry != monthlyEntry);
  }
  
  SECTION ("Extreme decimal values")
  {
    boost::gregorian::date refDate(2023, Jan, 15);
    
    // Very small value
    DecimalType smallValue(fromString<DecimalType>("0.00000001"));
    NumericTimeSeriesEntry<DecimalType> smallEntry(refDate, smallValue, TimeFrame::DAILY);
    REQUIRE(smallEntry.getValue() == smallValue);
    
    // Very large value
    DecimalType largeValue(fromString<DecimalType>("999999999.99999999"));
    NumericTimeSeriesEntry<DecimalType> largeEntry(refDate, largeValue, TimeFrame::DAILY);
    REQUIRE(largeEntry.getValue() == largeValue);
    
    // Negative value
    DecimalType negValue(fromString<DecimalType>("-12345.6789"));
    NumericTimeSeriesEntry<DecimalType> negEntry(refDate, negValue, TimeFrame::DAILY);
    REQUIRE(negEntry.getValue() == negValue);
    
    // Zero
    DecimalType zeroValue(fromString<DecimalType>("0.0"));
    NumericTimeSeriesEntry<DecimalType> zeroEntry(refDate, zeroValue, TimeFrame::DAILY);
    REQUIRE(zeroEntry.getValue() == zeroValue);
  }
}

TEST_CASE ("OHLCTimeSeriesEntry comprehensive tests", "[OHLCTimeSeriesEntry]")
{
  SECTION ("Self-assignment")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    OHLCTimeSeriesEntry<DecimalType> entry(refDate, open, high, low, close, 
                                            volume, TimeFrame::DAILY);
    
    entry = entry;
    
    REQUIRE(entry.getDateValue() == refDate);
    REQUIRE(entry.getOpenValue() == open);
    REQUIRE(entry.getHighValue() == high);
    REQUIRE(entry.getLowValue() == low);
    REQUIRE(entry.getCloseValue() == close);
    REQUIRE(entry.getVolumeValue() == volume);
    REQUIRE(entry.getTimeFrame() == TimeFrame::DAILY);
  }
  
  SECTION ("DateTime with time component")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    ptime dateTime(date(2023, Jan, 15), time_duration(9, 30, 0));
    
    OHLCTimeSeriesEntry<DecimalType> entry(dateTime, open, high, low, close, 
                                            volume, TimeFrame::INTRADAY);
    
    REQUIRE(entry.getDateTime() == dateTime);
    REQUIRE(entry.getDateValue() == date(2023, Jan, 15));
    REQUIRE(entry.getBarTime().hours() == 9);
    REQUIRE(entry.getBarTime().minutes() == 30);
    REQUIRE(entry.getBarTime().seconds() == 0);
    REQUIRE(entry.getTimeFrame() == TimeFrame::INTRADAY);
  }
  
  SECTION ("Extreme decimal values")
  {
    boost::gregorian::date refDate(2023, Jan, 15);
    
    // Very small prices
    DecimalType smallOpen(fromString<DecimalType>("0.00000001"));
    DecimalType smallHigh(fromString<DecimalType>("0.00000002"));
    DecimalType smallLow(fromString<DecimalType>("0.000000005"));
    DecimalType smallClose(fromString<DecimalType>("0.000000015"));
    DecimalType smallVol(fromString<DecimalType>("100"));
    
    OHLCTimeSeriesEntry<DecimalType> smallEntry(refDate, smallOpen, smallHigh, 
                                                 smallLow, smallClose, smallVol, 
                                                 TimeFrame::DAILY);
    
    REQUIRE(smallEntry.getOpenValue() == smallOpen);
    REQUIRE(smallEntry.getHighValue() == smallHigh);
    REQUIRE(smallEntry.getLowValue() == smallLow);
    REQUIRE(smallEntry.getCloseValue() == smallClose);
    
    // Very large prices
    DecimalType largeOpen(fromString<DecimalType>("999999999.0"));
    DecimalType largeHigh(fromString<DecimalType>("999999999.99"));
    DecimalType largeLow(fromString<DecimalType>("999999998.0"));
    DecimalType largeClose(fromString<DecimalType>("999999999.50"));
    DecimalType largeVol(fromString<DecimalType>("999999999"));
    
    OHLCTimeSeriesEntry<DecimalType> largeEntry(refDate, largeOpen, largeHigh, 
                                                  largeLow, largeClose, largeVol, 
                                                  TimeFrame::DAILY);
    
    REQUIRE(largeEntry.getOpenValue() == largeOpen);
    REQUIRE(largeEntry.getHighValue() == largeHigh);
    REQUIRE(largeEntry.getLowValue() == largeLow);
    REQUIRE(largeEntry.getCloseValue() == largeClose);
  }
  
  SECTION ("Boundary validation - open equals high and low")
  {
    DecimalType price(fromString<DecimalType>("100.00"));
    DecimalType volume(fromString<DecimalType>("1000"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    // Open = High = Low = Close (valid)
    OHLCTimeSeriesEntry<DecimalType> entry1(refDate, price, price, price, price, 
                                             volume, TimeFrame::DAILY);
    REQUIRE(entry1.getOpenValue() == price);
    REQUIRE(entry1.getHighValue() == price);
    REQUIRE(entry1.getLowValue() == price);
    REQUIRE(entry1.getCloseValue() == price);
    
    // Open = High, Low < Open, Close = Open (valid)
    DecimalType low(fromString<DecimalType>("99.00"));
    OHLCTimeSeriesEntry<DecimalType> entry2(refDate, price, price, low, price, 
                                             volume, TimeFrame::DAILY);
    REQUIRE(entry2.getOpenValue() == price);
    REQUIRE(entry2.getHighValue() == price);
    REQUIRE(entry2.getLowValue() == low);
    
    // Open = Low, High > Open, Close = Open (valid)
    DecimalType high(fromString<DecimalType>("101.00"));
    OHLCTimeSeriesEntry<DecimalType> entry3(refDate, price, high, price, price, 
                                             volume, TimeFrame::DAILY);
    REQUIRE(entry3.getOpenValue() == price);
    REQUIRE(entry3.getHighValue() == high);
    REQUIRE(entry3.getLowValue() == price);
  }
  
  SECTION ("Different time frames")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    OHLCTimeSeriesEntry<DecimalType> dailyEntry(refDate, open, high, low, close, 
                                                 volume, TimeFrame::DAILY);
    OHLCTimeSeriesEntry<DecimalType> weeklyEntry(refDate, open, high, low, close, 
                                                  volume, TimeFrame::WEEKLY);
    OHLCTimeSeriesEntry<DecimalType> monthlyEntry(refDate, open, high, low, close, 
                                                   volume, TimeFrame::MONTHLY);
    OHLCTimeSeriesEntry<DecimalType> intradayEntry(refDate, open, high, low, close, 
                                                    volume, TimeFrame::INTRADAY);
    
    REQUIRE(dailyEntry.getTimeFrame() == TimeFrame::DAILY);
    REQUIRE(weeklyEntry.getTimeFrame() == TimeFrame::WEEKLY);
    REQUIRE(monthlyEntry.getTimeFrame() == TimeFrame::MONTHLY);
    REQUIRE(intradayEntry.getTimeFrame() == TimeFrame::INTRADAY);
    
    // Entries with same OHLC and date but different timeframes should not be equal
    REQUIRE(dailyEntry != weeklyEntry);
    REQUIRE(dailyEntry != monthlyEntry);
    REQUIRE(weeklyEntry != monthlyEntry);
  }
  
  SECTION ("Zero volume")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType zeroVolume(fromString<DecimalType>("0"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    OHLCTimeSeriesEntry<DecimalType> entry(refDate, open, high, low, close, 
                                            zeroVolume, TimeFrame::DAILY);
    
    REQUIRE(entry.getVolumeValue() == zeroVolume);
  }
}

TEST_CASE ("TimeSeriesEntry comparison edge cases", "[TimeSeriesEntry][Comparison]")
{
  SECTION ("NumericTimeSeriesEntry - same date different times")
  {
    DecimalType value(fromString<DecimalType>("123.45"));
    ptime dateTime1(date(2023, Jan, 15), time_duration(9, 0, 0));
    ptime dateTime2(date(2023, Jan, 15), time_duration(10, 0, 0));
    
    NumericTimeSeriesEntry<DecimalType> entry1(dateTime1, value, TimeFrame::INTRADAY);
    NumericTimeSeriesEntry<DecimalType> entry2(dateTime2, value, TimeFrame::INTRADAY);
    
    // Same value and date but different times should not be equal
    REQUIRE(entry1 != entry2);
  }
  
  SECTION ("OHLCTimeSeriesEntry - same date different times")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    ptime dateTime1(date(2023, Jan, 15), time_duration(9, 0, 0));
    ptime dateTime2(date(2023, Jan, 15), time_duration(10, 0, 0));
    
    OHLCTimeSeriesEntry<DecimalType> entry1(dateTime1, open, high, low, close, 
                                             volume, TimeFrame::INTRADAY);
    OHLCTimeSeriesEntry<DecimalType> entry2(dateTime2, open, high, low, close, 
                                             volume, TimeFrame::INTRADAY);
    
    // Same OHLCV and date but different times should not be equal
    REQUIRE(entry1 != entry2);
  }
  
  SECTION ("OHLCTimeSeriesEntry - one value differs")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType closeDiff(fromString<DecimalType>("103.01"));
    DecimalType volume(fromString<DecimalType>("1000000"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    OHLCTimeSeriesEntry<DecimalType> entry1(refDate, open, high, low, close, 
                                             volume, TimeFrame::DAILY);
    OHLCTimeSeriesEntry<DecimalType> entry2(refDate, open, high, low, closeDiff, 
                                             volume, TimeFrame::DAILY);
    
    REQUIRE(entry1 != entry2);
  }
  
  SECTION ("OHLCTimeSeriesEntry - volume differs")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("105.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("103.00"));
    DecimalType volume1(fromString<DecimalType>("1000000"));
    DecimalType volume2(fromString<DecimalType>("1000001"));
    boost::gregorian::date refDate(2023, Jan, 15);
    
    OHLCTimeSeriesEntry<DecimalType> entry1(refDate, open, high, low, close, 
                                             volume1, TimeFrame::DAILY);
    OHLCTimeSeriesEntry<DecimalType> entry2(refDate, open, high, low, close, 
                                             volume2, TimeFrame::DAILY);
    
    REQUIRE(entry1 != entry2);
  }
}

TEST_CASE ("TimeSeriesEntry exception validation edge cases", "[OHLCTimeSeriesEntry][Exception]")
{
  boost::gregorian::date refDate(2023, Jan, 15);
  DecimalType volume(fromString<DecimalType>("1000000"));
  
  SECTION ("High equals open (should be valid)")
  {
    DecimalType price(fromString<DecimalType>("100.00"));
    DecimalType low(fromString<DecimalType>("99.00"));
    
    REQUIRE_NOTHROW(OHLCTimeSeriesEntry<DecimalType>(refDate, price, price, low, 
                                                       price, volume, TimeFrame::DAILY));
  }
  
  SECTION ("High equals close (should be valid)")
  {
    DecimalType open(fromString<DecimalType>("99.00"));
    DecimalType high(fromString<DecimalType>("100.00"));
    DecimalType low(fromString<DecimalType>("98.00"));
    
    REQUIRE_NOTHROW(OHLCTimeSeriesEntry<DecimalType>(refDate, open, high, low, 
                                                       high, volume, TimeFrame::DAILY));
  }
  
  SECTION ("Low equals open (should be valid)")
  {
    DecimalType price(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("101.00"));
    
    REQUIRE_NOTHROW(OHLCTimeSeriesEntry<DecimalType>(refDate, price, high, price, 
                                                       price, volume, TimeFrame::DAILY));
  }
  
  SECTION ("Low equals close (should be valid)")
  {
    DecimalType open(fromString<DecimalType>("101.00"));
    DecimalType high(fromString<DecimalType>("102.00"));
    DecimalType low(fromString<DecimalType>("100.00"));
    
    REQUIRE_NOTHROW(OHLCTimeSeriesEntry<DecimalType>(refDate, open, high, low, 
                                                       low, volume, TimeFrame::DAILY));
  }
  
  SECTION ("High slightly less than open")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("99.99"));
    DecimalType low(fromString<DecimalType>("99.00"));
    DecimalType close(fromString<DecimalType>("99.50"));
    
    REQUIRE_THROWS_AS(OHLCTimeSeriesEntry<DecimalType>(refDate, open, high, low, 
                                                         close, volume, TimeFrame::DAILY),
                      TimeSeriesEntryException);
  }
  
  SECTION ("Low slightly greater than close")
  {
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("101.00"));
    DecimalType low(fromString<DecimalType>("99.51"));
    DecimalType close(fromString<DecimalType>("99.50"));
    
    REQUIRE_THROWS_AS(OHLCTimeSeriesEntry<DecimalType>(refDate, open, high, low, 
                                                         close, volume, TimeFrame::DAILY),
                      TimeSeriesEntryException);
  }
  
  SECTION ("Multiple constraint violations")
  {
    // High < open AND high < close
    DecimalType open(fromString<DecimalType>("100.00"));
    DecimalType high(fromString<DecimalType>("95.00"));
    DecimalType low(fromString<DecimalType>("94.00"));
    DecimalType close(fromString<DecimalType>("99.00"));
    
    REQUIRE_THROWS_AS(OHLCTimeSeriesEntry<DecimalType>(refDate, open, high, low, 
                                                         close, volume, TimeFrame::DAILY),
                      TimeSeriesEntryException);
  }
}

