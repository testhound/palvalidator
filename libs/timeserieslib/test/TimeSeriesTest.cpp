#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../TimeSeries.h"
#include "../TimeSeriesIndicators.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef decimal<7> EquityType;

std::shared_ptr<OHLCTimeSeriesEntry<7>>
    createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    auto date1 = std::make_shared<date> (from_undelimited_string(dateString));
    auto open1 = std::make_shared<EquityType> (fromString<decimal<7>>(openPrice));
    auto high1 = std::make_shared<EquityType> (fromString<decimal<7>>(highPrice));
    auto low1 = std::make_shared<EquityType> (fromString<decimal<7>>(lowPrice));
    auto close1 = std::make_shared<EquityType> (fromString<decimal<7>>(closePrice));
    return std::make_shared<OHLCTimeSeriesEntry<7>>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }

std::shared_ptr<OHLCTimeSeriesEntry<7>>
    createWeeklyEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    auto date1 = std::make_shared<date> (from_undelimited_string(dateString));
    auto open1 = std::make_shared<EquityType> (fromString<decimal<7>>(openPrice));
    auto high1 = std::make_shared<EquityType> (fromString<decimal<7>>(highPrice));
    auto low1 = std::make_shared<EquityType> (fromString<decimal<7>>(lowPrice));
    auto close1 = std::make_shared<EquityType> (fromString<decimal<7>>(closePrice));
    return std::make_shared<OHLCTimeSeriesEntry<7>>(date1, open1, high1, low1, 
						   close1, vol, TimeFrame::WEEKLY);
  }


TEST_CASE ("TimeSeries operations", "[TimeSeries]")
{
  auto entry0 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry1 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry2 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry3 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry4 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry6 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  OHLCTimeSeries<7> spySeries(TimeFrame::DAILY, TradingVolume::SHARES);
  //TimeSeries<2> spySeries();
  spySeries.addEntry (entry4);
  spySeries.addEntry (entry6);
  spySeries.addEntry (entry2);
  spySeries.addEntry (entry3);
  spySeries.addEntry (entry1);
  spySeries.addEntry (entry5);
  spySeries.addEntry (entry0);


  NumericTimeSeries<7> closeSeries (spySeries.CloseTimeSeries());
  NumericTimeSeries<7> openSeries (spySeries.OpenTimeSeries());
  NumericTimeSeries<7> highSeries (spySeries.HighTimeSeries());
  NumericTimeSeries<7> lowSeries (spySeries.LowTimeSeries());
  std::vector<decimal<7>> aVector(lowSeries.getTimeSeriesAsVector());

  NumericTimeSeries<7> rocIndicatorSeries (RocSeries<7>(closeSeries, 1));
  
  decimal<7> medianValue (mkc_timeseries::Median (closeSeries));

  std::vector<unsigned int> aIntVec;
  aIntVec.push_back(2);
  aIntVec.push_back(5);
  aIntVec.push_back(2);

  double dev (MedianAbsoluteDeviation<unsigned int> (aIntVec));
  double dev2 (StandardDeviation<unsigned int> (aIntVec));

  RobustQn<7> qn(rocIndicatorSeries);
  
  REQUIRE (aVector.size() == lowSeries.getNumEntries());

  CSIExtendedFuturesCsvReader<7> dollarIndexCsvFile ("DX20060R.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS);
  dollarIndexCsvFile.readFile();
  

  std::shared_ptr<OHLCTimeSeries<7>> dollarIndexTimeSeries = dollarIndexCsvFile.getTimeSeries();
  
  SECTION ("Timeseries size test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getNumEntries() == 7);
      REQUIRE (closeSeries.getNumEntries() == 7);
      REQUIRE (openSeries.getNumEntries() == 7);
      REQUIRE (highSeries.getNumEntries() == 7);
      REQUIRE (lowSeries.getNumEntries() == 7);
    }

  SECTION ("Timeseries Median Indicator test", "[TimeSeries]")
    {
      REQUIRE (medianValue == entry3->getCloseValue());
    }

  SECTION ("Timeseries Robust Qn Indicator test", "[TimeSeries]")
    {
      decimal<7> result = qn.getRobustQn();
      REQUIRE (result > DecimalConstants<7>::DecimalZero);
    }

  SECTION ("TimeSeries Date Filtering test", "[TimeSeries]")
    {
      boost::gregorian::date firstDate (1986, Dec, 18);
      boost::gregorian::date lastDate (1987, Dec, 20);
      boost::gregorian::date actualLastDate (1987, Dec, 18);

      DateRange range(firstDate, lastDate);

      OHLCTimeSeries<7> filteredSeries (FilterTimeSeries<7>(*dollarIndexTimeSeries, range));
      REQUIRE (filteredSeries.getFirstDate() == firstDate);
      REQUIRE (filteredSeries.getLastDate() == actualLastDate);
      
    }

  SECTION ("TimeSeries Divide test", "[TimeSeries]")
    {
      NumericTimeSeries<7> divideIndicatorSeries (DivideSeries<7> (closeSeries, openSeries));

      NumericTimeSeries<7>::ConstTimeSeriesIterator it = divideIndicatorSeries.beginSortedAccess();
      NumericTimeSeries<7>::ConstTimeSeriesIterator closeSeriesIterator = closeSeries.beginSortedAccess();
      NumericTimeSeries<7>::ConstTimeSeriesIterator openSeriesIterator = openSeries.beginSortedAccess();
      dec::decimal<7> temp;

      std::cout << "SANITY CHECK!" << std::endl << std::endl;

      for (; ((closeSeriesIterator != closeSeries.endSortedAccess()) && 
	      (openSeriesIterator != openSeries.endSortedAccess())); closeSeriesIterator++, openSeriesIterator++, it++)
	{
	  std::cout << "On " << boost::gregorian::to_simple_string (it->first);
	  std::cout << " Dividing " << closeSeriesIterator->second->getValue() << " by " << openSeriesIterator->second->getValue() << std::endl;
	  temp = closeSeriesIterator->second->getValue() / openSeriesIterator->second->getValue();
	  std::cout << "Result = " << temp << std::endl;
	  REQUIRE (it->second->getValue() == temp);
	}


    }

  SECTION ("Timeseries ROC Indicator test", "[TimeSeries]")
    {
      decimal<7> rocVal, currVal, prevVal, calcVal;

      NumericTimeSeries<7>::ConstRandomAccessIterator closeSeriesIt = closeSeries.beginRandomAccess();
      closeSeriesIt++;

      NumericTimeSeries<7>::ConstTimeSeriesIterator it = rocIndicatorSeries.beginSortedAccess();
      rocVal = it->second->getValue();

      currVal = closeSeries.getValue (closeSeriesIt, 0);
      prevVal = closeSeries.getValue (closeSeriesIt, 1);
      calcVal = ((currVal / prevVal) - DecimalConstants<7>::DecimalOne) * DecimalConstants<7>::DecimalOneHundred;
      REQUIRE (rocVal == calcVal);

      closeSeriesIt++;
      it++;

      rocVal = it->second->getValue();

      currVal = closeSeries.getValue (closeSeriesIt, 0);
      prevVal = closeSeries.getValue (closeSeriesIt, 1);
      calcVal = ((currVal / prevVal) - DecimalConstants<7>::DecimalOne) * DecimalConstants<7>::DecimalOneHundred;
      REQUIRE (rocVal == calcVal);

    }

  SECTION ("TimeSeries getTimeSeriesEntry by date", "TimeSeries]")
    {
      OHLCTimeSeries<7>::TimeSeriesIterator it= spySeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it != spySeries.endSortedAccess());
      REQUIRE (*it->second == *entry4);

      NumericTimeSeries<7>::TimeSeriesIterator it2 = closeSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it2 != closeSeries.endSortedAccess());
      REQUIRE (it2->second->getValue() == entry4->getCloseValue());

      NumericTimeSeries<7>::TimeSeriesIterator it3 = openSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it3 != openSeries.endSortedAccess());
      REQUIRE (it3->second->getValue() == entry4->getOpenValue());

      NumericTimeSeries<7>::TimeSeriesIterator it4 = highSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it4 != highSeries.endSortedAccess());
      REQUIRE (it4->second->getValue() == entry4->getHighValue());

      NumericTimeSeries<7>::TimeSeriesIterator it5 = lowSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it5 != lowSeries.endSortedAccess());
      REQUIRE (it5->second->getValue() == entry4->getLowValue());
    }

  SECTION ("TimeSeries getTimeSeriesEntry by date const", "TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstTimeSeriesIterator it= spySeries.getTimeSeriesEntry(date (2016, Jan, 4));
      REQUIRE  (it != spySeries.endSortedAccess());
      REQUIRE (*it->second == *entry2);

      it = spySeries.getTimeSeriesEntry(date (2016, Jan, 15));
      REQUIRE  (it == spySeries.endSortedAccess());
    }

  SECTION ("TimeSeries getRandomAccessIterator by date const", "TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstRandomAccessIterator it= spySeries.getRandomAccessIterator (date (2016, Jan, 4));
      REQUIRE  (it != spySeries.endRandomAccess());
      REQUIRE (*(*it) == *entry2);

      it= spySeries.getRandomAccessIterator (date (2016, Jan, 18));
      REQUIRE  (it == spySeries.endRandomAccess());

      it= spySeries.getRandomAccessIterator (date (2016, Jan, 6));
      REQUIRE  (it != spySeries.endRandomAccess());
      REQUIRE (*(*it) == *entry0);
    }

  SECTION ("Timeseries date test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getFirstDate() == date (2015, Dec, 28));
      REQUIRE (spySeries.getLastDate() == date (2016, Jan, 06));

      REQUIRE (closeSeries.getFirstDate() == date (2015, Dec, 28));
      REQUIRE (closeSeries.getLastDate() == date (2016, Jan, 06));
    }

  SECTION ("Timeseries time frame test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getTimeFrame() == TimeFrame::DAILY); 
      REQUIRE (closeSeries.getTimeFrame() == TimeFrame::DAILY); 
    }

  SECTION ("Timeseries addEntry timeframe exception test", "[TimeSeries]")
    {
      auto entry = createWeeklyEquityEntry ("20160106", "198.34", "200.06", "197.60",
					    "198.82", 151566880);
      REQUIRE_THROWS (spySeries.addEntry (entry));
     }

  SECTION ("Timeseries addEntry existing entry exception test", "[TimeSeries]")
    {
      auto entry = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
      REQUIRE_THROWS (spySeries.addEntry (entry));
     }

  SECTION ("Timeseries RandomAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::RandomAccessIterator it = spySeries.beginRandomAccess();
      REQUIRE (*(*it) == *entry6);
      it++;
      REQUIRE (*(*it) == *entry5);
      it++;
      REQUIRE (*(*it) == *entry4);
      it++;
      REQUIRE (*(*it) == *entry3);
      it++;
      REQUIRE (*(*it) == *entry2);
      it++;
      REQUIRE (*(*it) == *entry1);
    }

  SECTION ("Timeseries Const RandomAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      REQUIRE (*(*it) == *entry6);
      it++;
      REQUIRE (*(*it) == *entry5);
      it++;
      REQUIRE (*(*it) == *entry4);
      it++;
      REQUIRE (*(*it) == *entry3);
      it++;
      REQUIRE (*(*it) == *entry2);
      it++;
      REQUIRE (*(*it) == *entry1);
    }


 SECTION ("Timeseries OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::RandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      std::shared_ptr<decimal<7>> openRef2 = spySeries.getOpen (it, 2);
      REQUIRE (*openRef2 == *entry5->getOpen());

      std::shared_ptr<decimal<7>> highRef3 = spySeries.getHigh (it, 3);
      REQUIRE (*highRef3 == *entry6->getHigh());

      it++;

      std::shared_ptr<decimal<7>> lowRef1 = spySeries.getLow (it, 1);
      REQUIRE (*lowRef1 == *entry3->getLow());

      std::shared_ptr<decimal<7>> closeRef0 = spySeries.getClose (it, 0);
      REQUIRE (*closeRef0 == *entry2->getClose());

      std::shared_ptr<decimal<7>> closeRef2 = spySeries.getClose (it, 2);
      REQUIRE (*closeRef2 == *entry4->getClose());
    }

 SECTION ("Timeseries Const OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      std::shared_ptr<decimal<7>> openRef2 = spySeries.getOpen (it, 2);
      REQUIRE (*openRef2 == *entry5->getOpen());

      boost::gregorian::date dateRef2 = spySeries.getDateValue(it, 2);
      REQUIRE (dateRef2 == entry5->getDateValue());

     std::shared_ptr<decimal<7>> highRef3 = spySeries.getHigh (it, 3);
      REQUIRE (*highRef3 == *entry6->getHigh());

      it++;

      std::shared_ptr<decimal<7>> lowRef1 = spySeries.getLow (it, 1);
      REQUIRE (*lowRef1 == *entry3->getLow());

      std::shared_ptr<decimal<7>> closeRef0 = spySeries.getClose (it, 0);
      REQUIRE (*closeRef0 == *entry2->getClose());

      std::shared_ptr<decimal<7>> closeRef2 = spySeries.getClose (it, 2);
      REQUIRE (*closeRef2 == *entry4->getClose());
    }

SECTION ("Timeseries Value OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::RandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      decimal<7> openRef2 = spySeries.getOpenValue (it, 2);
      REQUIRE (openRef2 == entry5->getOpenValue());

      boost::gregorian::date dateRef2 = spySeries.getDateValue(it, 2);
      REQUIRE (dateRef2 == entry5->getDateValue());

      decimal<7> highRef3 = spySeries.getHighValue (it, 3);
      REQUIRE (highRef3 == entry6->getHighValue());

      it++;

      decimal<7> lowRef1 = spySeries.getLowValue (it, 1);
      REQUIRE (lowRef1 == entry3->getLowValue());

      decimal<7> closeRef0 = spySeries.getCloseValue (it, 0);
      REQUIRE (closeRef0 == entry2->getCloseValue());

      decimal<7> closeRef2 = spySeries.getCloseValue (it, 2);
      REQUIRE (closeRef2 == entry4->getCloseValue());
    }

 SECTION ("Timeseries Const Value OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      decimal<7> openRef2 = spySeries.getOpenValue (it, 2);
      REQUIRE (openRef2 == entry5->getOpenValue());

      decimal<7> highRef3 = spySeries.getHighValue (it, 3);
      REQUIRE (highRef3 == entry6->getHighValue());

      it++;

      decimal<7> lowRef1 = spySeries.getLowValue (it, 1);
      REQUIRE (lowRef1 == entry3->getLowValue());

      decimal<7> closeRef0 = spySeries.getCloseValue (it, 0);
      REQUIRE (closeRef0 == entry2->getCloseValue());

      decimal<7> closeRef2 = spySeries.getCloseValue (it, 2);
      REQUIRE (closeRef2 == entry4->getCloseValue());
    }

 SECTION ("Timeseries Const Value OHLC exception tests", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstRandomAccessIterator it = 
	spySeries.getRandomAccessIterator (date (2016, Jan, 4));

      decimal<7> closeRef2 = spySeries.getCloseValue (it, 4);

      REQUIRE_THROWS (spySeries.getCloseValue (it, 5));
    }

 SECTION ("Timeseries SortedAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::TimeSeriesIterator it = spySeries.beginSortedAccess();
      REQUIRE (*(it->second) == *entry6);
      it++;
      REQUIRE (*(it->second) == *entry5);
      it++;
      REQUIRE (*(it->second) == *entry4);
      it++;
      REQUIRE (*(it->second) == *entry3);
      it++;
      REQUIRE (*(it->second) == *entry2);
      it++;
      REQUIRE (*(it->second) == *entry1);
    }

 SECTION ("Timeseries SortedAccess Const Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<7>::ConstTimeSeriesIterator it = spySeries.beginSortedAccess();
      REQUIRE (*(it->second) == *entry6);
      it++;
      REQUIRE (*(it->second) == *entry5);
      it++;
      REQUIRE (*(it->second) == *entry4);
      it++;
      REQUIRE (*(it->second) == *entry3);
      it++;
      REQUIRE (*(it->second) == *entry2);
      it++;
      REQUIRE (*(it->second) == *entry1);
    }


 SECTION("TimeSeries copy construction equality", "[TimeSeries]")
   {
     OHLCTimeSeries<7> spySeries2(spySeries);
     REQUIRE (spySeries == spySeries2);
   }

 SECTION("TimeSeries assignment operator", "[TimeSeries]")
   {
     OHLCTimeSeries<7> spySeries2(TimeFrame::DAILY, TradingVolume::SHARES);

     spySeries2.addEntry (entry0);
     spySeries2.addEntry (entry1);
     spySeries2.addEntry (entry2);
     spySeries2.addEntry (entry3);
     spySeries2.addEntry (entry4);
     spySeries2.addEntry (entry5);

     
     REQUIRE (spySeries != spySeries2);
     spySeries = spySeries2;
     REQUIRE (spySeries == spySeries2);
   }

SECTION("TimeSeries inequality", "[TimeSeries]")
   {
     OHLCTimeSeries<7>  spySeries2(TimeFrame::DAILY, TradingVolume::SHARES);

     spySeries2.addEntry (entry0);
     spySeries2.addEntry (entry1);
     spySeries2.addEntry (entry2);
     spySeries2.addEntry (entry3);
     spySeries2.addEntry (entry4);
     spySeries2.addEntry (entry5);

     REQUIRE (spySeries != spySeries2);
   }
}
