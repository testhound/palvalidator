#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../TimeSeries.h"
#include "../TimeSeriesIndicators.h"
#include "TestUtils.h"
#include "number.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef num::DefaultNumber EquityType;

OHLCTimeSeriesEntry<EquityType>
    createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    return *createTimeSeriesEntry(dateString, openPrice, highPrice, lowPrice, closePrice, vol);
  }

OHLCTimeSeriesEntry<EquityType>
    createWeeklyEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       const std::string&  vol)
  {
    return *createTimeSeriesEntry(dateString, openPrice, highPrice, lowPrice, closePrice, vol, TimeFrame::WEEKLY);
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
  OHLCTimeSeries<DecimalType> spySeries(TimeFrame::DAILY, TradingVolume::SHARES);
  //TimeSeries<2> spySeries();
  spySeries.addEntry (entry4);
  spySeries.addEntry (entry6);
  spySeries.addEntry (entry2);
  spySeries.addEntry (entry3);
  spySeries.addEntry (entry1);
  spySeries.addEntry (entry5);
  spySeries.addEntry (entry0);


  NumericTimeSeries<DecimalType> closeSeries (spySeries.CloseTimeSeries());
  NumericTimeSeries<DecimalType> openSeries (spySeries.OpenTimeSeries());
  NumericTimeSeries<DecimalType> highSeries (spySeries.HighTimeSeries());
  NumericTimeSeries<DecimalType> lowSeries (spySeries.LowTimeSeries());
  std::vector<DecimalType> aVector(lowSeries.getTimeSeriesAsVector());

  NumericTimeSeries<DecimalType> rocIndicatorSeries (RocSeries<DecimalType>(closeSeries, 1));

  DecimalType medianValue (mkc_timeseries::Median (closeSeries));

  std::vector<unsigned int> aIntVec;
  aIntVec.push_back(2);
  aIntVec.push_back(5);
  aIntVec.push_back(2);

  double dev (MedianAbsoluteDeviation<unsigned int> (aIntVec));
  double dev2 (StandardDeviation<unsigned int> (aIntVec));

  RobustQn<DecimalType> qn(rocIndicatorSeries);

  REQUIRE (aVector.size() == lowSeries.getNumEntries());

  DecimalType dollarTickValue(createDecimal("0.005"));
  CSIExtendedFuturesCsvReader<DecimalType> dollarIndexCsvFile ("DX20060R.txt", TimeFrame::DAILY,
							       TradingVolume::CONTRACTS, dollarTickValue);
  dollarIndexCsvFile.readFile();


  std::shared_ptr<OHLCTimeSeries<DecimalType>> dollarIndexTimeSeries = dollarIndexCsvFile.getTimeSeries();

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
      REQUIRE (medianValue == entry3.getCloseValue());
    }

  SECTION ("Timeseries Robust Qn Indicator test", "[TimeSeries]")
    {
      DecimalType result = qn.getRobustQn();
      REQUIRE (result > DecimalConstants<DecimalType>::DecimalZero);
    }

  SECTION ("TimeSeries Date Filtering test", "[TimeSeries]")
    {
      boost::gregorian::date firstDate (1986, Dec, 18);
      boost::gregorian::date lastDate (1987, Dec, 20);
      boost::gregorian::date actualLastDate (1987, Dec, 18);

      DateRange range(firstDate, lastDate);

      OHLCTimeSeries<DecimalType> filteredSeries (FilterTimeSeries<DecimalType>(*dollarIndexTimeSeries, range));
      REQUIRE (filteredSeries.getFirstDate() == firstDate);
      REQUIRE (filteredSeries.getLastDate() == actualLastDate);

    }

  SECTION ("TimeSeries Divide test", "[TimeSeries]")
    {
      NumericTimeSeries<DecimalType> divideIndicatorSeries (DivideSeries<DecimalType> (closeSeries, openSeries));

      NumericTimeSeries<DecimalType>::ConstTimeSeriesIterator it = divideIndicatorSeries.beginSortedAccess();
      NumericTimeSeries<DecimalType>::ConstTimeSeriesIterator closeSeriesIterator = closeSeries.beginSortedAccess();
      NumericTimeSeries<DecimalType>::ConstTimeSeriesIterator openSeriesIterator = openSeries.beginSortedAccess();
      DecimalType temp;

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
      DecimalType rocVal, currVal, prevVal, calcVal;

      NumericTimeSeries<DecimalType>::ConstRandomAccessIterator closeSeriesIt = closeSeries.beginRandomAccess();
      closeSeriesIt++;

      NumericTimeSeries<DecimalType>::ConstTimeSeriesIterator it = rocIndicatorSeries.beginSortedAccess();
      rocVal = it->second->getValue();

      currVal = closeSeries.getValue (closeSeriesIt, 0);
      prevVal = closeSeries.getValue (closeSeriesIt, 1);
      calcVal = ((currVal / prevVal) - DecimalConstants<DecimalType>::DecimalOne) * DecimalConstants<DecimalType>::DecimalOneHundred;
      REQUIRE (rocVal == calcVal);

      closeSeriesIt++;
      it++;

      rocVal = it->second->getValue();

      currVal = closeSeries.getValue (closeSeriesIt, 0);
      prevVal = closeSeries.getValue (closeSeriesIt, 1);
      calcVal = ((currVal / prevVal) - DecimalConstants<DecimalType>::DecimalOne) * DecimalConstants<DecimalType>::DecimalOneHundred;
      REQUIRE (rocVal == calcVal);

    }

  SECTION ("TimeSeries getTimeSeriesEntry by date", "TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::TimeSeriesIterator it= spySeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it != spySeries.endSortedAccess());
      REQUIRE (it->second == entry4);

      NumericTimeSeries<DecimalType>::TimeSeriesIterator it2 = closeSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it2 != closeSeries.endSortedAccess());
      REQUIRE (it2->second->getValue() == entry4.getCloseValue());

      NumericTimeSeries<DecimalType>::TimeSeriesIterator it3 = openSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it3 != openSeries.endSortedAccess());
      REQUIRE (it3->second->getValue() == entry4.getOpenValue());

      NumericTimeSeries<DecimalType>::TimeSeriesIterator it4 = highSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it4 != highSeries.endSortedAccess());
      REQUIRE (it4->second->getValue() == entry4.getHighValue());

      NumericTimeSeries<DecimalType>::TimeSeriesIterator it5 = lowSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE  (it5 != lowSeries.endSortedAccess());
      REQUIRE (it5->second->getValue() == entry4.getLowValue());
    }

  SECTION ("TimeSeries getTimeSeriesEntry by date const", "TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstTimeSeriesIterator it= spySeries.getTimeSeriesEntry(date (2016, Jan, 4));
      REQUIRE  (it != spySeries.endSortedAccess());
      REQUIRE (it->second == entry2);

      it = spySeries.getTimeSeriesEntry(date (2016, Jan, 15));
      REQUIRE  (it == spySeries.endSortedAccess());
    }

  SECTION ("TimeSeries getRandomAccessIterator by date const", "TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it= spySeries.getRandomAccessIterator (date (2016, Jan, 4));
      REQUIRE  (it != spySeries.endRandomAccess());
      REQUIRE ((*it) == entry2);

      it= spySeries.getRandomAccessIterator (date (2016, Jan, 18));
      REQUIRE  (it == spySeries.endRandomAccess());

      it= spySeries.getRandomAccessIterator (date (2016, Jan, 6));
      REQUIRE  (it != spySeries.endRandomAccess());
      REQUIRE ((*it) == entry0);
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
					    "198.82", "151566880");
      REQUIRE_THROWS (spySeries.addEntry (decltype(entry)(entry)));
     }

  SECTION ("Timeseries addEntry existing entry exception test", "[TimeSeries]")
    {
      auto entry = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
      REQUIRE_THROWS (spySeries.addEntry (entry));
     }

  SECTION ("Timeseries RandomAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::RandomAccessIterator it = spySeries.beginRandomAccess();
      REQUIRE ((*it) == entry6);
      it++;
      REQUIRE ((*it) == entry5);
      it++;
      REQUIRE ((*it) == entry4);
      it++;
      REQUIRE ((*it) == entry3);
      it++;
      REQUIRE ((*it) == entry2);
      it++;
      REQUIRE ((*it) == entry1);
    }

  SECTION ("Timeseries Const RandomAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      REQUIRE ((*it) == entry6);
      it++;
      REQUIRE ((*it) == entry5);
      it++;
      REQUIRE ((*it) == entry4);
      it++;
      REQUIRE ((*it) == entry3);
      it++;
      REQUIRE ((*it) == entry2);
      it++;
      REQUIRE ((*it) == entry1);
    }


 SECTION ("Timeseries OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::RandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      DecimalType openRef2 = spySeries.getOpenValue (it, 2);
      REQUIRE (openRef2 == entry5.getOpenValue());

      DecimalType highRef3 = spySeries.getHighValue (it, 3);
      REQUIRE (highRef3 == entry6.getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue (it, 1);
      REQUIRE (lowRef1 == entry3.getLowValue());

      DecimalType closeRef0 = spySeries.getCloseValue (it, 0);
      REQUIRE (closeRef0 == entry2.getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue (it, 2);
      REQUIRE (closeRef2 == entry4.getCloseValue());
    }

 SECTION ("Timeseries Const OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      DecimalType openRef2 = spySeries.getOpenValue (it, 2);
      REQUIRE (openRef2 == entry5.getOpenValue());

      boost::gregorian::date dateRef2 = spySeries.getDateValue(it, 2);
      REQUIRE (dateRef2 == entry5.getDateValue());

      DecimalType highRef3 = spySeries.getHighValue (it, 3);
      REQUIRE (highRef3 == entry6.getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue (it, 1);
      REQUIRE (lowRef1 == entry3.getLowValue());

      DecimalType closeRef0 = spySeries.getCloseValue (it, 0);
      REQUIRE (closeRef0 == entry2.getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue (it, 2);
      REQUIRE (closeRef2 == entry4.getCloseValue());
    }

SECTION ("Timeseries Value OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::RandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      DecimalType openRef2 = spySeries.getOpenValue (it, 2);
      REQUIRE (openRef2 == entry5.getOpenValue());

      boost::gregorian::date dateRef2 = spySeries.getDateValue(it, 2);
      REQUIRE (dateRef2 == entry5.getDateValue());

      DecimalType highRef3 = spySeries.getHighValue (it, 3);
      REQUIRE (highRef3 == entry6.getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue (it, 1);
      REQUIRE (lowRef1 == entry3.getLowValue());

      DecimalType closeRef0 = spySeries.getCloseValue (it, 0);
      REQUIRE (closeRef0 == entry2.getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue (it, 2);
      REQUIRE (closeRef2 == entry4.getCloseValue());
    }

 SECTION ("Timeseries Const Value OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      DecimalType openRef2 = spySeries.getOpenValue (it, 2);
      REQUIRE (openRef2 == entry5.getOpenValue());

      DecimalType highRef3 = spySeries.getHighValue (it, 3);
      REQUIRE (highRef3 == entry6.getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue (it, 1);
      REQUIRE (lowRef1 == entry3.getLowValue());

      DecimalType closeRef0 = spySeries.getCloseValue (it, 0);
      REQUIRE (closeRef0 == entry2.getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue (it, 2);
      REQUIRE (closeRef2 == entry4.getCloseValue());
    }

 SECTION ("Timeseries Const Value OHLC exception tests", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it =
	spySeries.getRandomAccessIterator (date (2016, Jan, 4));

      DecimalType closeRef2 = spySeries.getCloseValue (it, 4);

      REQUIRE_THROWS (spySeries.getCloseValue (it, 5));
    }

 SECTION ("Timeseries SortedAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::TimeSeriesIterator it = spySeries.beginSortedAccess();
      REQUIRE ((it->second) == entry6);
      it++;
      REQUIRE ((it->second) == entry5);
      it++;
      REQUIRE ((it->second) == entry4);
      it++;
      REQUIRE ((it->second) == entry3);
      it++;
      REQUIRE ((it->second) == entry2);
      it++;
      REQUIRE ((it->second) == entry1);
    }

 SECTION ("Timeseries SortedAccess Const Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstTimeSeriesIterator it = spySeries.beginSortedAccess();
      REQUIRE ((it->second) == entry6);
      it++;
      REQUIRE ((it->second) == entry5);
      it++;
      REQUIRE ((it->second) == entry4);
      it++;
      REQUIRE ((it->second) == entry3);
      it++;
      REQUIRE ((it->second) == entry2);
      it++;
      REQUIRE ((it->second) == entry1);
    }


 SECTION("TimeSeries copy construction equality", "[TimeSeries]")
   {
     OHLCTimeSeries<DecimalType> spySeries2(spySeries);
     REQUIRE (spySeries == spySeries2);
   }

 SECTION("TimeSeries assignment operator", "[TimeSeries]")
   {
     OHLCTimeSeries<DecimalType> spySeries2(TimeFrame::DAILY, TradingVolume::SHARES);

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
     OHLCTimeSeries<DecimalType>  spySeries2(TimeFrame::DAILY, TradingVolume::SHARES);

     spySeries2.addEntry (entry0);
     spySeries2.addEntry (entry1);
     spySeries2.addEntry (entry2);
     spySeries2.addEntry (entry3);
     spySeries2.addEntry (entry4);
     spySeries2.addEntry (entry5);

     REQUIRE (spySeries != spySeries2);
   }
}
