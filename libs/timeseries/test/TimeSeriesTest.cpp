#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <future>
#include <atomic>
#include <vector>
#include <random>
#include <chrono>
#include "TimeSeriesCsvReader.h"
#include "TimeSeries.h"
#include "TimeSeriesIndicators.h"
#include "TestUtils.h"
#include "number.h"
#include "IntradayIntervalCalculator.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef num::DefaultNumber EquityType;
typedef num::DefaultNumber DecimalType;

using boost::posix_time::ptime;
using boost::posix_time::hours;
using boost::posix_time::minutes;
using boost::posix_time::seconds;
using boost::gregorian::days;

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
  spySeries.addEntry (*entry4);
  spySeries.addEntry (*entry6);
  spySeries.addEntry (*entry2);
  spySeries.addEntry (*entry3);
  spySeries.addEntry (*entry1);
  spySeries.addEntry (*entry5);
  spySeries.addEntry (*entry0);


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

  RobustQn<DecimalType> qn(rocIndicatorSeries);

  REQUIRE (aVector.size() == lowSeries.getNumEntries());

  DecimalType dollarTickValue(createDecimal("0.005"));
  CSIExtendedFuturesCsvReader<DecimalType> dollarIndexCsvFile ("DX20060R.txt", TimeFrame::DAILY,
							       TradingVolume::CONTRACTS, dollarTickValue);
  dollarIndexCsvFile.readFile();


  std::shared_ptr<OHLCTimeSeries<DecimalType>> dollarIndexTimeSeries = dollarIndexCsvFile.getTimeSeries();

  // Intraday series

  OHLCTimeSeries<DecimalType> ssoSeries(TimeFrame::INTRADAY, TradingVolume::SHARES);
  auto intraday_entry1 = createTimeSeriesEntry ("20210405", "09:00", "105.99", "106.57", "105.93", "106.54", "0");
  auto intraday_entry2 = createTimeSeriesEntry ("20210405", "10:00", "106.54", "107.29", "106.38", "107.10", "0");
  auto intraday_entry3 = createTimeSeriesEntry ("20210405", "11:00", "107.10", "107.54", "107.03", "107.409", "0");
  auto intraday_entry4 = createTimeSeriesEntry ("20210405", "12:00", "107.42", "107.78", "107.375", "107.47", "0");
  auto intraday_entry5 = createTimeSeriesEntry ("20210405", "13:00", "107.47", "107.60", "107.34", "107.5712", "0");
  auto intraday_entry6 = createTimeSeriesEntry ("20210405", "14:00", "107.59", "107.7099", "107.34", "107.345", "0");
  auto intraday_entry7 = createTimeSeriesEntry ("20210405", "15:00", "107.35", "107.70", "107.16", "107.45", "0");

  auto intraday_entry8 = createTimeSeriesEntry ("20210406", "09:00", "107.14", "107.75", "107.02", "107.68", "0");
  auto intraday_entry9 = createTimeSeriesEntry ("20210406", "10:00", "107.73", "107.91", "107.58", "107.739", "0");
  auto intraday_entry10 = createTimeSeriesEntry ("20210406", "11:00", "107.71", "107.9225", "107.55", "107.92", "0");
  auto intraday_entry11 = createTimeSeriesEntry ("20210406", "12:00", "107.91", "107.91", "107.63", "107.71", "0");
  auto intraday_entry12 = createTimeSeriesEntry ("20210406", "13:00", "107.70", "107.70", "107.22", "107.60", "0");
  auto intraday_entry13 = createTimeSeriesEntry ("20210406", "14:00", "107.62", "107.71", "107.44", "107.59", "0");
  auto intraday_entry14 = createTimeSeriesEntry ("20210406", "15:00", "107.59", "107.64", "106.98", "107.33", "0");

  ssoSeries.addEntry(*intraday_entry9);
  ssoSeries.addEntry(*intraday_entry5);
  ssoSeries.addEntry(*intraday_entry12);
  ssoSeries.addEntry(*intraday_entry3);
  ssoSeries.addEntry(*intraday_entry13);
  ssoSeries.addEntry(*intraday_entry6);
  ssoSeries.addEntry(*intraday_entry1);
  ssoSeries.addEntry(*intraday_entry10);
  ssoSeries.addEntry(*intraday_entry2);
  ssoSeries.addEntry(*intraday_entry7);
  ssoSeries.addEntry(*intraday_entry11);
  ssoSeries.addEntry(*intraday_entry8);
  ssoSeries.addEntry(*intraday_entry4);
  ssoSeries.addEntry(*intraday_entry14);




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

      NumericTimeSeries<DecimalType>::ConstSortedIterator it = divideIndicatorSeries.beginSortedAccess();
      NumericTimeSeries<DecimalType>::ConstSortedIterator closeSeriesIterator = closeSeries.beginSortedAccess();
      NumericTimeSeries<DecimalType>::ConstSortedIterator openSeriesIterator = openSeries.beginSortedAccess();
      DecimalType temp;


      for (; ((closeSeriesIterator != closeSeries.endSortedAccess()) &&
       (openSeriesIterator != openSeries.endSortedAccess())); closeSeriesIterator++, openSeriesIterator++, it++)
 {
   temp = closeSeriesIterator->getValue() / openSeriesIterator->getValue();
   REQUIRE (it->getValue() == temp);
 }


    }

  SECTION ("Timeseries ROC Indicator test", "[TimeSeries]")
    {
      DecimalType rocVal, currVal, prevVal, calcVal;

      NumericTimeSeries<DecimalType>::ConstRandomAccessIterator closeSeriesIt = closeSeries.beginRandomAccess();
      closeSeriesIt++;

      NumericTimeSeries<DecimalType>::ConstSortedIterator it = rocIndicatorSeries.beginSortedAccess();
      rocVal = it->getValue();

      currVal = closeSeries.getValue (closeSeriesIt->getDateTime(), 0);
      prevVal = closeSeries.getValue (closeSeriesIt->getDateTime(), 1);
      calcVal = ((currVal / prevVal) - DecimalConstants<DecimalType>::DecimalOne) * DecimalConstants<DecimalType>::DecimalOneHundred;
      REQUIRE (rocVal == calcVal);

      closeSeriesIt++;
      it++;

      rocVal = it->getValue();

      currVal = closeSeries.getValue (closeSeriesIt->getDateTime(), 0);
      prevVal = closeSeries.getValue (closeSeriesIt->getDateTime(), 1);
      calcVal = ((currVal / prevVal) - DecimalConstants<DecimalType>::DecimalOne) * DecimalConstants<DecimalType>::DecimalOneHundred;
      REQUIRE (rocVal == calcVal);

    }

  SECTION ("TimeSeries getTimeSeriesEntry by date", "TimeSeries]")
    {
      auto entry = spySeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE (entry == *entry4);

      auto closeEntry = closeSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE (closeEntry.getValue() == entry4->getCloseValue());

      auto openEntry = openSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE (openEntry.getValue() == entry4->getOpenValue());

      auto highEntry = highSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE (highEntry.getValue() == entry4->getHighValue());

      auto lowEntry = lowSeries.getTimeSeriesEntry(date (2015, Dec, 30));
      REQUIRE (lowEntry.getValue() == entry4->getLowValue());
    }

  SECTION ("TimeSeries getTimeSeriesEntry by date const", "TimeSeries]")
    {
      auto entry = spySeries.getTimeSeriesEntry(date (2016, Jan, 4));
      REQUIRE (entry == *entry2);

      REQUIRE_THROWS_AS(spySeries.getTimeSeriesEntry(date (2016, Jan, 15)), mkc_timeseries::TimeSeriesDataNotFoundException);
    }

  SECTION ("TimeSeries getRandomAccessIterator by date const", "TimeSeries]")
    {
      auto it= spySeries.beginRandomAccess();
      // Find entry2 manually since getRandomAccessIterator is removed
      bool found = false;
      for (; it != spySeries.endRandomAccess(); ++it) {
        if (it->getDateTime().date() == date(2016, Jan, 4)) {
          REQUIRE ((*it) == *entry2);
          found = true;
          break;
        }
      }
      REQUIRE(found);

      // Test for non-existent date
      found = false;
      for (it = spySeries.beginRandomAccess(); it != spySeries.endRandomAccess(); ++it) {
        if (it->getDateTime().date() == date(2016, Jan, 18)) {
          found = true;
          break;
        }
      }
      REQUIRE_FALSE(found);

      // Test for entry0
      found = false;
      for (it = spySeries.beginRandomAccess(); it != spySeries.endRandomAccess(); ++it) {
        if (it->getDateTime().date() == date(2016, Jan, 6)) {
          REQUIRE ((*it) == *entry0);
          found = true;
          break;
        }
      }
      REQUIRE(found);
    }

  SECTION ("Timeseries date test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getFirstDate() == date (2015, Dec, 28));
      REQUIRE (spySeries.getLastDate() == date (2016, Jan, 06));

      REQUIRE (closeSeries.getFirstDate() == date (2015, Dec, 28));
      REQUIRE (closeSeries.getLastDate() == date (2016, Jan, 06));
    }

  SECTION ("Timeseries intraday date test", "[TimeSeries]")
    {
      date first(2021, Apr, 05);
      date last(2021, Apr, 06);

      ptime firstDateTime (first, time_duration(9, 0, 0));
      ptime lastDateTime (last, time_duration(15, 0, 0));

      REQUIRE (ssoSeries.getFirstDateTime() == firstDateTime);
      REQUIRE (ssoSeries.getLastDateTime() == lastDateTime);
    }

  SECTION ("Timeseries time frame test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getTimeFrame() == TimeFrame::DAILY);
      REQUIRE (closeSeries.getTimeFrame() == TimeFrame::DAILY);
      REQUIRE (ssoSeries.getTimeFrame() == TimeFrame::INTRADAY);
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
      REQUIRE_THROWS (spySeries.addEntry (*entry));
     }

  SECTION ("Timeseries RandomAccess Iterator test", "[TimeSeries]")
    {
      auto it = spySeries.beginRandomAccess();
      REQUIRE ((*it) == *entry6);
      it++;
      REQUIRE ((*it) == *entry5);
      it++;
      REQUIRE ((*it) == *entry4);
      it++;
      REQUIRE ((*it) == *entry3);
      it++;
      REQUIRE ((*it) == *entry2);
      it++;
      REQUIRE ((*it) == *entry1);
    }

  SECTION ("Timeseries Const RandomAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      REQUIRE ((*it) == *entry6);
      it++;
      REQUIRE ((*it) == *entry5);
      it++;
      REQUIRE ((*it) == *entry4);
      it++;
      REQUIRE ((*it) == *entry3);
      it++;
      REQUIRE ((*it) == *entry2);
      it++;
      REQUIRE ((*it) == *entry1);
    }


 SECTION ("Timeseries OHLC test", "[TimeSeries]")
    {
      // Use date-based access instead of iterator-based
      date testDate(2015, Dec, 31); // entry3 date

      DecimalType openRef2 = spySeries.getOpenValue(testDate, 2);
      REQUIRE (openRef2 == entry5->getOpenValue());

      DecimalType highRef3 = spySeries.getHighValue(testDate, 3);
      REQUIRE (highRef3 == entry6->getHighValue());

      // Test from entry2 date
      date testDate2(2016, Jan, 4); // entry2 date

      DecimalType lowRef1 = spySeries.getLowValue(testDate2, 1);
      REQUIRE (lowRef1 == entry3->getLowValue());

      DecimalType closeRef0 = spySeries.getCloseValue(testDate2, 0);
      REQUIRE (closeRef0 == entry2->getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue(testDate2, 2);
      REQUIRE (closeRef2 == entry4->getCloseValue());
    }

 SECTION ("Timeseries Const OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      // Use iterator dereferencing for offset 0, and date-based access for other offsets
      DecimalType openRef2 = spySeries.getOpenValue(it->getDateValue(), 2);
      REQUIRE (openRef2 == entry5->getOpenValue());

      boost::gregorian::date dateRef2 = spySeries.getDateValue(it->getDateValue(), 2);
      REQUIRE (dateRef2 == entry5->getDateValue());

      DecimalType highRef3 = spySeries.getHighValue(it->getDateValue(), 3);
      REQUIRE (highRef3 == entry6->getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue(it->getDateValue(), 1);
      REQUIRE (lowRef1 == entry3->getLowValue());

      DecimalType closeRef0 = it->getCloseValue(); // offset 0 - direct access
      REQUIRE (closeRef0 == entry2->getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue(it->getDateValue(), 2);
      REQUIRE (closeRef2 == entry4->getCloseValue());
    }

SECTION ("Timeseries Value OHLC test", "[TimeSeries]")
    {
      auto it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      // Use iterator dereferencing for offset 0, and date-based access for other offsets
      DecimalType openRef2 = spySeries.getOpenValue(it->getDateValue(), 2);
      REQUIRE (openRef2 == entry5->getOpenValue());

      boost::gregorian::date dateRef2 = spySeries.getDateValue(it->getDateValue(), 2);
      REQUIRE (dateRef2 == entry5->getDateValue());

      DecimalType highRef3 = spySeries.getHighValue(it->getDateValue(), 3);
      REQUIRE (highRef3 == entry6->getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue(it->getDateValue(), 1);
      REQUIRE (lowRef1 == entry3->getLowValue());

      DecimalType closeRef0 = it->getCloseValue(); // offset 0 - direct access
      REQUIRE (closeRef0 == entry2->getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue(it->getDateValue(), 2);
      REQUIRE (closeRef2 == entry4->getCloseValue());
    }

 SECTION ("Timeseries Const Value OHLC test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstRandomAccessIterator it = spySeries.beginRandomAccess();
      it++;
      it++;
      it++;

      // Use iterator dereferencing for offset 0, and date-based access for other offsets
      DecimalType openRef2 = spySeries.getOpenValue(it->getDateValue(), 2);
      REQUIRE (openRef2 == entry5->getOpenValue());

      DecimalType highRef3 = spySeries.getHighValue(it->getDateValue(), 3);
      REQUIRE (highRef3 == entry6->getHighValue());

      it++;

      DecimalType lowRef1 = spySeries.getLowValue(it->getDateValue(), 1);
      REQUIRE (lowRef1 == entry3->getLowValue());

      DecimalType closeRef0 = it->getCloseValue(); // offset 0 - direct access
      REQUIRE (closeRef0 == entry2->getCloseValue());

      DecimalType closeRef2 = spySeries.getCloseValue(it->getDateValue(), 2);
      REQUIRE (closeRef2 == entry4->getCloseValue());
    }

 SECTION ("Timeseries Const Value OHLC exception tests", "[TimeSeries]")
    {
      // Use date-based access instead of getRandomAccessIterator
      date testDate(2016, Jan, 4);

      DecimalType closeRef2 = spySeries.getCloseValue(testDate, 4);

      REQUIRE_THROWS (spySeries.getCloseValue(testDate, 5));
    }

 SECTION ("Timeseries SortedAccess Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstSortedIterator it = spySeries.beginSortedAccess();
      REQUIRE ((*it) == *entry6);
      it++;
      REQUIRE ((*it) == *entry5);
      it++;
      REQUIRE ((*it) == *entry4);
      it++;
      REQUIRE ((*it) == *entry3);
      it++;
      REQUIRE ((*it) == *entry2);
      it++;
      REQUIRE ((*it) == *entry1);
    }

 SECTION ("Timeseries SortedAccess Const Iterator test", "[TimeSeries]")
    {
      OHLCTimeSeries<DecimalType>::ConstSortedIterator it = spySeries.beginSortedAccess();
      REQUIRE ((*it) == *entry6);
      it++;
      REQUIRE ((*it) == *entry5);
      it++;
      REQUIRE ((*it) == *entry4);
      it++;
      REQUIRE ((*it) == *entry3);
      it++;
      REQUIRE ((*it) == *entry2);
      it++;
      REQUIRE ((*it) == *entry1);
    }


 SECTION("TimeSeries copy construction equality", "[TimeSeries]")
   {
     OHLCTimeSeries<DecimalType> spySeries2(spySeries);
     REQUIRE (spySeries == spySeries2);
   }

 SECTION("TimeSeries assignment operator", "[TimeSeries]")
   {
     OHLCTimeSeries<DecimalType> spySeries2(TimeFrame::DAILY, TradingVolume::SHARES);

     spySeries2.addEntry (*entry0);
     spySeries2.addEntry (*entry1);
     spySeries2.addEntry (*entry2);
     spySeries2.addEntry (*entry3);
     spySeries2.addEntry (*entry4);
     spySeries2.addEntry (*entry5);


     REQUIRE (spySeries != spySeries2);
     spySeries = spySeries2;
     REQUIRE (spySeries == spySeries2);
   }

SECTION("TimeSeries inequality", "[TimeSeries]")
   {
     OHLCTimeSeries<DecimalType>  spySeries2(TimeFrame::DAILY, TradingVolume::SHARES);

     spySeries2.addEntry (*entry0);
     spySeries2.addEntry (*entry1);
     spySeries2.addEntry (*entry2);
     spySeries2.addEntry (*entry3);
     spySeries2.addEntry (*entry4);
     spySeries2.addEntry (*entry5);

     REQUIRE (spySeries != spySeries2);
   }

  SECTION("TimeSeries getRandomAccessIterator by ptime", "[TimeSeries]") {
    // pick an existing 2021-04-05 12:00 bar
    ptime dt4 = intraday_entry4->getDateTime();

    // Test that the entry exists using the new API
    REQUIRE(ssoSeries.isDateFound(dt4));
    auto entry = ssoSeries.getTimeSeriesEntry(dt4);
    REQUIRE(entry == *intraday_entry4);

    // a timestamp not in the series should return false
    ptime missing(dt4.date(), time_duration(16, 0, 0));
    REQUIRE_FALSE(ssoSeries.isDateFound(missing));
  }

  SECTION("TimeSeries getDateTimeValue by ptime iterator", "[TimeSeries]") {
    // locate the 11:00 bar on 2021-04-05
    ptime dt3 = intraday_entry3->getDateTime();

    // Test using date-based access instead of iterator-based
    // offset 0 → exact same timestamp
    REQUIRE(ssoSeries.getDateTimeValue(dt3, 0) == dt3);
    // offset 2 → two bars earlier (09:00 bar)
    ptime dt1 = intraday_entry1->getDateTime();
    REQUIRE(ssoSeries.getDateTimeValue(dt3, 2) == dt1);

    // offset beyond available history should throw
    REQUIRE_THROWS_AS(ssoSeries.getDateTimeValue(dt3, 5), TimeSeriesOffsetOutOfRangeException);
  }
  SECTION ("Timeseries IBS1 Indicator test", "[TimeSeries]")
    {
      // Test IBS1 calculation with known values
      NumericTimeSeries<DecimalType> ibsIndicatorSeries (IBS1Series<DecimalType>(spySeries));
      
      // Verify the series has the same number of entries as the input
      REQUIRE (ibsIndicatorSeries.getNumEntries() == spySeries.getNumEntries());
      
      // Test specific IBS calculations
      // For entry6 (20151228): Open=204.86, High=205.26, Low=203.94, Close=205.21
      // IBS = (205.21 - 203.94) / (205.26 - 203.94) = 1.27 / 1.32 ≈ 0.9621
      DecimalType expectedIBS6 = (entry6->getCloseValue() - entry6->getLowValue()) /
                                 (entry6->getHighValue() - entry6->getLowValue());
      
      // Get the IBS value for the same date as entry6
      auto ibsEntry6 = ibsIndicatorSeries.getTimeSeriesEntry(entry6->getDateTime().date());
      REQUIRE (ibsEntry6.getValue() == expectedIBS6);
      
      // For entry0 (20160106): Open=198.34, High=200.06, Low=197.60, Close=198.82
      // IBS = (198.82 - 197.60) / (200.06 - 197.60) = 1.22 / 2.46 ≈ 0.4959
      DecimalType expectedIBS0 = (entry0->getCloseValue() - entry0->getLowValue()) /
                                 (entry0->getHighValue() - entry0->getLowValue());
      
      auto ibsEntry0 = ibsIndicatorSeries.getTimeSeriesEntry(entry0->getDateTime().date());
      REQUIRE (ibsEntry0.getValue() == expectedIBS0);
      
      // Verify all IBS values are between 0 and 1
      NumericTimeSeries<DecimalType>::ConstSortedIterator it = ibsIndicatorSeries.beginSortedAccess();
      for (; it != ibsIndicatorSeries.endSortedAccess(); it++)
      {
        DecimalType ibsValue = it->getValue();
        REQUIRE (ibsValue >= DecimalConstants<DecimalType>::DecimalZero);
        REQUIRE (ibsValue <= DecimalConstants<DecimalType>::DecimalOne);
      }
    }

  SECTION ("Timeseries IBS1 Indicator division by zero test", "[TimeSeries]")
    {
      // Create a test series with a bar where High == Low (division by zero case)
      OHLCTimeSeries<DecimalType> testSeries(TimeFrame::DAILY, TradingVolume::SHARES);
      
      // Add a normal entry
      auto normalEntry = createEquityEntry ("20160101", "100.00", "101.00", "99.00", "100.50", 1000000);
      testSeries.addEntry (*normalEntry);
      
      // Add an entry where High == Low (should result in IBS = 0)
      auto flatEntry = createEquityEntry ("20160102", "100.00", "100.00", "100.00", "100.00", 1000000);
      testSeries.addEntry (*flatEntry);
      
      // Add another normal entry
      auto normalEntry2 = createEquityEntry ("20160103", "100.00", "102.00", "98.00", "99.00", 1000000);
      testSeries.addEntry (*normalEntry2);
      
      NumericTimeSeries<DecimalType> ibsIndicatorSeries (IBS1Series<DecimalType>(testSeries));
      
      // Verify the series has the same number of entries
      REQUIRE (ibsIndicatorSeries.getNumEntries() == testSeries.getNumEntries());
      
      // Check the flat entry (should have IBS = 0)
      auto ibsFlatEntry = ibsIndicatorSeries.getTimeSeriesEntry(flatEntry->getDateTime().date());
      REQUIRE (ibsFlatEntry.getValue() == DecimalConstants<DecimalType>::DecimalZero);
      
      // Check the normal entries have valid IBS values
      auto ibsNormalEntry = ibsIndicatorSeries.getTimeSeriesEntry(normalEntry->getDateTime().date());
      DecimalType expectedIBS = (normalEntry->getCloseValue() - normalEntry->getLowValue()) /
                               (normalEntry->getHighValue() - normalEntry->getLowValue());
      REQUIRE (ibsNormalEntry.getValue() == expectedIBS);
      
      auto ibsNormalEntry2 = ibsIndicatorSeries.getTimeSeriesEntry(normalEntry2->getDateTime().date());
      DecimalType expectedIBS2 = (normalEntry2->getCloseValue() - normalEntry2->getLowValue()) /
                                 (normalEntry2->getHighValue() - normalEntry2->getLowValue());
      REQUIRE (ibsNormalEntry2.getValue() == expectedIBS2);
    }

  SECTION ("Timeseries IBS1 Indicator empty series test", "[TimeSeries]")
    {
      // Test with empty series
      OHLCTimeSeries<DecimalType> emptySeries(TimeFrame::DAILY, TradingVolume::SHARES);
      NumericTimeSeries<DecimalType> ibsIndicatorSeries (IBS1Series<DecimalType>(emptySeries));
      
      REQUIRE (ibsIndicatorSeries.getNumEntries() == 0);
    }

  SECTION ("Timeseries IBS1 Indicator edge cases test", "[TimeSeries]")
    {
      // Test edge cases: close at high, close at low, close in middle
      OHLCTimeSeries<DecimalType> edgeCaseSeries(TimeFrame::DAILY, TradingVolume::SHARES);
      
      // Close at low (IBS should be 0)
      auto closeAtLow = createEquityEntry ("20160101", "100.00", "102.00", "98.00", "98.00", 1000000);
      edgeCaseSeries.addEntry (*closeAtLow);
      
      // Close at high (IBS should be 1)
      auto closeAtHigh = createEquityEntry ("20160102", "100.00", "102.00", "98.00", "102.00", 1000000);
      edgeCaseSeries.addEntry (*closeAtHigh);
      
      // Close in middle (IBS should be 0.5)
      auto closeInMiddle = createEquityEntry ("20160103", "100.00", "102.00", "98.00", "100.00", 1000000);
      edgeCaseSeries.addEntry (*closeInMiddle);
      
      NumericTimeSeries<DecimalType> ibsIndicatorSeries (IBS1Series<DecimalType>(edgeCaseSeries));
      
      // Check close at low (IBS = 0)
      auto ibsCloseAtLow = ibsIndicatorSeries.getTimeSeriesEntry(closeAtLow->getDateTime().date());
      REQUIRE (ibsCloseAtLow.getValue() == DecimalConstants<DecimalType>::DecimalZero);
      
      // Check close at high (IBS = 1)
      auto ibsCloseAtHigh = ibsIndicatorSeries.getTimeSeriesEntry(closeAtHigh->getDateTime().date());
      REQUIRE (ibsCloseAtHigh.getValue() == DecimalConstants<DecimalType>::DecimalOne);
      
      // Check close in middle (IBS = 0.5)
      auto ibsCloseInMiddle = ibsIndicatorSeries.getTimeSeriesEntry(closeInMiddle->getDateTime().date());
      DecimalType expectedMiddle = DecimalConstants<DecimalType>::DecimalOne / DecimalConstants<DecimalType>::DecimalTwo;
      REQUIRE (ibsCloseInMiddle.getValue() == expectedMiddle);
    }

}

TEST_CASE ("OHLCTimeSeries with HashedLookupPolicy operations", "[TimeSeries][HashedLookupPolicy]")
{
  using HashedSeries = mkc_timeseries::OHLCTimeSeries<DecimalType, mkc_timeseries::HashedLookupPolicy<DecimalType>>;

  auto entry0 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82", 142662900);
  auto entry1 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36", 105999900);
  auto entry2 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02", 222353400);
  auto entry3 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87", 114877900);
  auto entry4 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93", 63317700);
  auto entry5 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40", 92640700);
  auto entry6 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21", 65899900);

  HashedSeries spySeriesHashed(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeriesHashed.addEntry (*entry4); // 20151230
  spySeriesHashed.addEntry (*entry6); // 20151228
  spySeriesHashed.addEntry (*entry2); // 20160104
  spySeriesHashed.addEntry (*entry3); // 20151231
  spySeriesHashed.addEntry (*entry1); // 20160105
  spySeriesHashed.addEntry (*entry5); // 20151229
  spySeriesHashed.addEntry (*entry0); // 20160106

  SECTION ("Basic operations and lookups with HashedLookupPolicy")
  {
    REQUIRE (spySeriesHashed.getNumEntries() == 7);
    REQUIRE (spySeriesHashed.getTimeFrame() == TimeFrame::DAILY);
    REQUIRE (spySeriesHashed.getVolumeUnits() == TradingVolume::SHARES);

    // Test getTimeSeriesEntry by ptime (should trigger index build if not already)
    auto it_e4_p = spySeriesHashed.getTimeSeriesEntry(entry4->getDateTime());
    REQUIRE (it_e4_p == *entry4);
    // Entry comparison already done above

    // Test getTimeSeriesEntry by date
    auto it_e2_d = spySeriesHashed.getTimeSeriesEntry(date(2016, Jan, 4));
    REQUIRE (it_e2_d == *entry2);

    // Test entry lookup by ptime using new API
    REQUIRE(spySeriesHashed.isDateFound(entry0->getDateTime()));
    auto retrieved_entry = spySeriesHashed.getTimeSeriesEntry(entry0->getDateTime());
    REQUIRE(retrieved_entry == *entry0);

    // Test lookup for non-existent entry
    REQUIRE_THROWS_AS(spySeriesHashed.getTimeSeriesEntry(date(2016, Jan, 15)), mkc_timeseries::TimeSeriesDataNotFoundException);

    // Test first/last dates/datetimes
    REQUIRE (spySeriesHashed.getFirstDateTime() == entry6->getDateTime()); // 20151228
    REQUIRE (spySeriesHashed.getLastDateTime() == entry0->getDateTime());  // 20160106
  }

  SECTION ("Index invalidation and rebuild with HashedLookupPolicy")
  {
    HashedSeries series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry6); // 20151228

    // First lookup, index built
    REQUIRE(series.getTimeSeriesEntry(entry6->getDateTime()) == *entry6);

    series.addEntry(*entry5); // 20151229 - Index should be cleared

    // Lookups should work, triggering index rebuild
    REQUIRE(series.getTimeSeriesEntry(entry6->getDateTime()) == *entry6);
    REQUIRE(series.getTimeSeriesEntry(entry5->getDateTime()) == *entry5);
    REQUIRE(series.getNumEntries() == 2);

    series.deleteEntryByDate(entry6->getDateTime()); // Index should be cleared
    REQUIRE(series.getNumEntries() == 1);
    REQUIRE_THROWS_AS(series.getTimeSeriesEntry(entry6->getDateTime()), mkc_timeseries::TimeSeriesDataNotFoundException); // entry6 deleted
    REQUIRE(series.getTimeSeriesEntry(entry5->getDateTime()) == *entry5); // entry5 still there, index rebuilt
  }

  SECTION ("Copy semantics with HashedLookupPolicy")
  {
    HashedSeries original(TimeFrame::DAILY, TradingVolume::SHARES);
    original.addEntry(*entry0);
    original.addEntry(*entry1);

    // Trigger index build on original
    REQUIRE(original.getTimeSeriesEntry(entry0->getDateTime()) == *entry0);

    HashedSeries copyConstructed(original);
    REQUIRE(copyConstructed.getNumEntries() == 2);
    REQUIRE(copyConstructed == original);
    REQUIRE(copyConstructed.getTimeSeriesEntry(entry0->getDateTime()) == *entry0);
    REQUIRE(copyConstructed.getTimeSeriesEntry(entry1->getDateTime()) == *entry1);

    // Modify original, copy should be unaffected
    original.addEntry(*entry2);
    REQUIRE(original.getNumEntries() == 3);
    REQUIRE(copyConstructed.getNumEntries() == 2); // Copy unaffected
    REQUIRE(copyConstructed.getTimeSeriesEntry(entry1->getDateTime()) == *entry1); // Copy's index still works

    HashedSeries copyAssigned(TimeFrame::DAILY, TradingVolume::SHARES);
    copyAssigned.addEntry(*entry3); // Add something to it first
    copyAssigned = original; // Assign from modified original

    REQUIRE(copyAssigned.getNumEntries() == 3);
    REQUIRE(copyAssigned == original);
    REQUIRE(copyAssigned.getTimeSeriesEntry(entry2->getDateTime()) == *entry2);
  }

  SECTION ("Move semantics with HashedLookupPolicy")
  {
    HashedSeries original(TimeFrame::DAILY, TradingVolume::SHARES);
    original.addEntry(*entry0);
    original.addEntry(*entry1);
    REQUIRE(original.getTimeSeriesEntry(entry0->getDateTime()) == *entry0); // Build index

    HashedSeries movedConstructed(std::move(original));
    REQUIRE(movedConstructed.getNumEntries() == 2);
    REQUIRE(movedConstructed.getTimeSeriesEntry(entry0->getDateTime()) == *entry0);
    REQUIRE(movedConstructed.getTimeSeriesEntry(entry1->getDateTime()) == *entry1);
    // 'original' is now in a valid but unspecified state, typically empty or small.
    // Depending on std::unordered_map move, its size might be 0 or its data pointers null.
    // For safety, let's assume it's not to be used for content without re-initialization.
    // REQUIRE(original.getNumEntries() == 0); // This might be true for std::vector move

    HashedSeries source2(TimeFrame::DAILY, TradingVolume::SHARES);
    source2.addEntry(*entry2);
    source2.addEntry(*entry3);
    REQUIRE(source2.getTimeSeriesEntry(entry2->getDateTime()) == *entry2); // Build index

    HashedSeries movedAssigned(TimeFrame::DAILY, TradingVolume::SHARES);
    movedAssigned.addEntry(*entry4); // Add something
    movedAssigned = std::move(source2);
    REQUIRE(movedAssigned.getNumEntries() == 2);
    REQUIRE(movedAssigned.getTimeSeriesEntry(entry2->getDateTime()) == *entry2);
    REQUIRE(movedAssigned.getTimeSeriesEntry(entry3->getDateTime()) == *entry3);
  }

  SECTION ("Constructor from range with HashedLookupPolicy")
  {
    std::vector<OHLCTimeSeriesEntry<DecimalType>> entries;
    entries.push_back(*entry0);
    entries.push_back(*entry1);
    entries.push_back(*entry2); // Entries will be sorted by constructor

    HashedSeries seriesFromRange(TimeFrame::DAILY, TradingVolume::SHARES, entries.begin(), entries.end());
    REQUIRE(seriesFromRange.getNumEntries() == 3);
    // Index should be built by on_construct_from_range via constructor
    REQUIRE(seriesFromRange.getTimeSeriesEntry(entry0->getDateTime()) == *entry0);
    REQUIRE(seriesFromRange.getTimeSeriesEntry(entry1->getDateTime()) == *entry1);
    REQUIRE(seriesFromRange.getTimeSeriesEntry(entry2->getDateTime()) == *entry2);
  }
   SECTION ("Hashed Policy - Edge case: Empty series lookup")
    {
        HashedSeries emptySeries(TimeFrame::DAILY, TradingVolume::SHARES);
        REQUIRE(emptySeries.getNumEntries() == 0);
        REQUIRE_THROWS_AS(emptySeries.getTimeSeriesEntry(entry0->getDateTime()), mkc_timeseries::TimeSeriesDataNotFoundException);
    }

    SECTION ("Hashed Policy - Edge case: Single entry series")
    {
        HashedSeries singleEntrySeries(TimeFrame::DAILY, TradingVolume::SHARES);
        singleEntrySeries.addEntry(*entry0);
        REQUIRE(singleEntrySeries.getNumEntries() == 1);
        REQUIRE(singleEntrySeries.getTimeSeriesEntry(entry0->getDateTime()) == *entry0);
        REQUIRE_THROWS_AS(singleEntrySeries.getTimeSeriesEntry(entry1->getDateTime()), mkc_timeseries::TimeSeriesDataNotFoundException);
    }
}

TEST_CASE ("FilterTimeSeries with Intraday Data (ptime precision)", "[TimeSeries][FilterTimeSeries][Intraday]")
{
  // Existing ssoSeries intraday data from TimeSeriesTest.cpp
  OHLCTimeSeries<DecimalType> ssoSeries(TimeFrame::INTRADAY, TradingVolume::SHARES);
  auto intraday_entry1 = createTimeSeriesEntry ("20210405", "09:00", "105.99", "106.57", "105.93", "106.54", "0"); // 2021-Apr-05 09:00:00
  auto intraday_entry2 = createTimeSeriesEntry ("20210405", "10:00", "106.54", "107.29", "106.38", "107.10", "0"); // 2021-Apr-05 10:00:00
  auto intraday_entry3 = createTimeSeriesEntry ("20210405", "11:00", "107.10", "107.54", "107.03", "107.409", "0"); // 2021-Apr-05 11:00:00
  auto intraday_entry4 = createTimeSeriesEntry ("20210405", "12:00", "107.42", "107.78", "107.375", "107.47", "0"); // 2021-Apr-05 12:00:00
  auto intraday_entry5 = createTimeSeriesEntry ("20210405", "13:00", "107.47", "107.60", "107.34", "107.5712", "0"); // 2021-Apr-05 13:00:00
  auto intraday_entry6 = createTimeSeriesEntry ("20210405", "14:00", "107.59", "107.7099", "107.34", "107.345", "0"); // 2021-Apr-05 14:00:00
  auto intraday_entry7 = createTimeSeriesEntry ("20210405", "15:00", "107.35", "107.70", "107.16", "107.45", "0");   // 2021-Apr-05 15:00:00

  auto intraday_entry8 = createTimeSeriesEntry ("20210406", "09:00", "107.14", "107.75", "107.02", "107.68", "0");   // 2021-Apr-06 09:00:00
  auto intraday_entry9 = createTimeSeriesEntry ("20210406", "10:00", "107.73", "107.91", "107.58", "107.739", "0");  // 2021-Apr-06 10:00:00
  auto intraday_entry10 = createTimeSeriesEntry ("20210406", "11:00", "107.71", "107.9225", "107.55", "107.92", "0"); // 2021-Apr-06 11:00:00
  // ... add all 14 entries to ssoSeries ...
  ssoSeries.addEntry(*intraday_entry1); ssoSeries.addEntry(*intraday_entry2); ssoSeries.addEntry(*intraday_entry3);
  ssoSeries.addEntry(*intraday_entry4); ssoSeries.addEntry(*intraday_entry5); ssoSeries.addEntry(*intraday_entry6);
  ssoSeries.addEntry(*intraday_entry7); ssoSeries.addEntry(*intraday_entry8); ssoSeries.addEntry(*intraday_entry9);
  ssoSeries.addEntry(*intraday_entry10);
  // Manually add remaining test entries to make it complete as per original test file
  auto intraday_entry11 = createTimeSeriesEntry ("20210406", "12:00", "107.91", "107.91", "107.63", "107.71", "0");
  auto intraday_entry12 = createTimeSeriesEntry ("20210406", "13:00", "107.70", "107.70", "107.22", "107.60", "0");
  auto intraday_entry13 = createTimeSeriesEntry ("20210406", "14:00", "107.62", "107.71", "107.44", "107.59", "0");
  auto intraday_entry14 = createTimeSeriesEntry ("20210406", "15:00", "107.59", "107.64", "106.98", "107.33", "0");
  ssoSeries.addEntry(*intraday_entry11); ssoSeries.addEntry(*intraday_entry12);
  ssoSeries.addEntry(*intraday_entry13); ssoSeries.addEntry(*intraday_entry14);

  using HashedSSOSeries = mkc_timeseries::OHLCTimeSeries<DecimalType, mkc_timeseries::HashedLookupPolicy<DecimalType>>;
  HashedSSOSeries ssoSeriesHashed(TimeFrame::INTRADAY, TradingVolume::SHARES);
   // Populate ssoSeriesHashed identically to ssoSeries
  ssoSeriesHashed.addEntry(*intraday_entry1); ssoSeriesHashed.addEntry(*intraday_entry2); ssoSeriesHashed.addEntry(*intraday_entry3);
  ssoSeriesHashed.addEntry(*intraday_entry4); ssoSeriesHashed.addEntry(*intraday_entry5); ssoSeriesHashed.addEntry(*intraday_entry6);
  ssoSeriesHashed.addEntry(*intraday_entry7); ssoSeriesHashed.addEntry(*intraday_entry8); ssoSeriesHashed.addEntry(*intraday_entry9);
  ssoSeriesHashed.addEntry(*intraday_entry10); ssoSeriesHashed.addEntry(*intraday_entry11); ssoSeriesHashed.addEntry(*intraday_entry12);
  ssoSeriesHashed.addEntry(*intraday_entry13); ssoSeriesHashed.addEntry(*intraday_entry14);


  SECTION ("Filter portion of a single day (LogN Policy)")
  {
    DateRange range(ptime(date(2021,Apr,5), hours(10)), ptime(date(2021,Apr,5), hours(14))); // 10:00 to 14:00 inclusive
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 5); // 10:00, 11:00, 12:00, 13:00, 14:00
    REQUIRE(filtered.getFirstDateTime() == intraday_entry2->getDateTime()); // 10:00
    REQUIRE(filtered.getLastDateTime() == intraday_entry6->getDateTime());  // 14:00
    REQUIRE(filtered.getTimeSeriesEntry(intraday_entry3->getDateTime()) == *intraday_entry3);
  }

  SECTION ("Filter portion of a single day (Hashed Policy)")
  {
    DateRange range(ptime(date(2021,Apr,5), hours(10)), ptime(date(2021,Apr,5), hours(14))); // 10:00 to 14:00 inclusive
    HashedSSOSeries filtered = FilterTimeSeries(ssoSeriesHashed, range);
    REQUIRE(filtered.getNumEntries() == 5);
    REQUIRE(filtered.getFirstDateTime() == intraday_entry2->getDateTime());
    REQUIRE(filtered.getLastDateTime() == intraday_entry6->getDateTime());
    REQUIRE(filtered.getTimeSeriesEntry(intraday_entry3->getDateTime()) == *intraday_entry3);
  }

  SECTION ("Filter full single day (LogN Policy)")
  {
    DateRange range(ptime(date(2021,Apr,6), hours(9)), ptime(date(2021,Apr,6), hours(15)));
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 7); // All 7 entries for 2021-04-06
    REQUIRE(filtered.getFirstDateTime() == intraday_entry8->getDateTime());
    REQUIRE(filtered.getLastDateTime() == intraday_entry14->getDateTime());
  }

  SECTION ("Filter full single day (Hashed Policy)")
  {
    DateRange range(ptime(date(2021,Apr,6), hours(9)), ptime(date(2021,Apr,6), hours(15)));
    HashedSSOSeries filtered = FilterTimeSeries(ssoSeriesHashed, range);
    REQUIRE(filtered.getNumEntries() == 7);
    REQUIRE(filtered.getFirstDateTime() == intraday_entry8->getDateTime());
    REQUIRE(filtered.getLastDateTime() == intraday_entry14->getDateTime());
  }

  SECTION ("Filter across midnight (LogN Policy)")
  {
    DateRange range(ptime(date(2021,Apr,5), hours(14)), ptime(date(2021,Apr,6), hours(10))); // 14:00, 15:00 from day 1; 09:00, 10:00 from day 2
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 4);
    REQUIRE(filtered.getFirstDateTime() == intraday_entry6->getDateTime()); // 2021-04-05 14:00
    REQUIRE(filtered.getLastDateTime() == intraday_entry9->getDateTime());   // 2021-04-06 10:00
  }

  SECTION ("Filter across midnight (Hashed Policy)")
  {
    DateRange range(ptime(date(2021,Apr,5), hours(14)), ptime(date(2021,Apr,6), hours(10)));
    HashedSSOSeries filtered = FilterTimeSeries(ssoSeriesHashed, range);
    REQUIRE(filtered.getNumEntries() == 4);
    REQUIRE(filtered.getFirstDateTime() == intraday_entry6->getDateTime());
    REQUIRE(filtered.getLastDateTime() == intraday_entry9->getDateTime());
  }

  SECTION ("Filter range resulting in empty series (LogN Policy)")
  {
    DateRange range(ptime(date(2021,Apr,5), boost::posix_time::hours(10) + boost::posix_time::minutes(5)),
                ptime(date(2021,Apr,5), boost::posix_time::hours(10) + boost::posix_time::minutes(55)));
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 0);
  }

  SECTION ("Filter range resulting in empty series (Hashed Policy)")
  {
    DateRange range(ptime(date(2021,Apr,5), hours(10) + minutes(5)),
                ptime(date(2021,Apr,5), hours(10) + minutes(55)));
    HashedSSOSeries filtered = FilterTimeSeries(ssoSeriesHashed, range);
    REQUIRE(filtered.getNumEntries() == 0);
  }

  SECTION ("Filter range matching a single bar (LogN Policy)")
  {
    DateRange range(intraday_entry4->getDateTime(), intraday_entry4->getDateTime()); // Exactly 2021-Apr-05 12:00:00
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 1);
    REQUIRE(filtered.getFirstDateTime() == intraday_entry4->getDateTime());
    REQUIRE(*(filtered.beginSortedAccess()) == *intraday_entry4);
  }

  SECTION ("Filter range matching a single bar (Hashed Policy)")
  {
    DateRange range(intraday_entry4->getDateTime(), intraday_entry4->getDateTime());
    HashedSSOSeries filtered = FilterTimeSeries(ssoSeriesHashed, range);
    REQUIRE(filtered.getNumEntries() == 1);
    REQUIRE(filtered.getFirstDateTime() == intraday_entry4->getDateTime());
    REQUIRE(*(filtered.beginSortedAccess()) == *intraday_entry4);
  }

  SECTION ("Filter range completely before series data (LogN Policy)") //
  {
    DateRange range(ptime(date(2021,Apr,4), hours(9)), ptime(date(2021,Apr,4), hours(10))); //
    // This range starts before ssoSeries.getFirstDateTime() (2021-Apr-05 09:00:00)
    // Expect TimeSeriesException based on FilterTimeSeries precondition in TimeSeries.h
    REQUIRE_THROWS_AS(FilterTimeSeries(ssoSeries, range), TimeSeriesException);
  }

  SECTION ("Filter range completely after series data (LogN Policy)")
  {
    DateRange range(ptime(date(2021,Apr,7), hours(9)), ptime(date(2021,Apr,7), hours(10)));
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 0);
  }

  SECTION ("Filter range partial start (LogN Policy)") // Starts before, ends within //
  {
    DateRange range(ptime(date(2021,Apr,4), hours(9)), intraday_entry2->getDateTime()); // ends 2021-04-05 10:00 //
    // This range starts before ssoSeries.getFirstDateTime() (2021-Apr-05 09:00:00)
    // Expect TimeSeriesException based on FilterTimeSeries precondition in TimeSeries.h
    REQUIRE_THROWS_AS(FilterTimeSeries(ssoSeries, range), TimeSeriesException);
  }

  SECTION ("Filter range partial end (LogN Policy)") // Starts within, ends after
  {
    DateRange range(intraday_entry13->getDateTime(), ptime(date(2021,Apr,7), hours(10))); // starts 2021-04-06 14:00
    OHLCTimeSeries<DecimalType> filtered = FilterTimeSeries(ssoSeries, range);
    REQUIRE(filtered.getNumEntries() == 2); // 14:00 and 15:00 on 2021-04-06
    REQUIRE(filtered.getFirstDateTime() == intraday_entry13->getDateTime());
    REQUIRE(filtered.getLastDateTime() == intraday_entry14->getDateTime());
  }
}


TEST_CASE("OHLCTimeSeries with HashedLookupPolicy concurrent access (Adds and Reads)", "[TimeSeries][HashedLookupPolicy][Concurrency]") {
    using SeriesType = mkc_timeseries::OHLCTimeSeries<DecimalType, mkc_timeseries::HashedLookupPolicy<DecimalType>>;

    SECTION("Concurrent addEntry and getTimeSeriesEntry operations") {
        auto sharedSeries = std::make_shared<SeriesType>(TimeFrame::DAILY, TradingVolume::SHARES);

        std::atomic<int> ptime_day_offset_counter(0);

        auto generate_unique_entry = [&](int thread_id_for_value, int entry_idx_in_thread) {
            int day_offset = ptime_day_offset_counter.fetch_add(1);
            ptime dt = ptime(date(2020, Jan, 1) + days(day_offset),
                             hours(12));

            DecimalType val_open = DecimalType(100 + thread_id_for_value * 200 + entry_idx_in_thread);
            DecimalType val_high = val_open + DecimalType(5);
            DecimalType val_low  = val_open - DecimalType(5);
            DecimalType val_close= val_open + DecimalType(2);
            DecimalType val_vol  = DecimalType(1000 + thread_id_for_value * 100 + entry_idx_in_thread);

            return OHLCTimeSeriesEntry<DecimalType>(
                dt, val_open, val_high, val_low, val_close, val_vol, TimeFrame::DAILY
            );
        };

        const int num_threads = std::max(4u, std::thread::hardware_concurrency());
        const int entries_per_thread = 200;

        std::vector<std::future<std::vector<OHLCTimeSeriesEntry<DecimalType>>>> futures;
        std::vector<std::vector<OHLCTimeSeriesEntry<DecimalType>>> all_added_entries_from_threads(num_threads);

        std::random_device rd;

        for (int i = 0; i < num_threads; ++i) {
            futures.emplace_back(std::async(std::launch::async, [&, i, seed = rd()]() {
                std::vector<OHLCTimeSeriesEntry<DecimalType>> thread_added_entries;
                thread_added_entries.reserve(entries_per_thread);
                std::mt19937 local_rng(seed);

                for (int j = 0; j < entries_per_thread; ++j) {
                    auto entry_to_add = generate_unique_entry(i, j);

                    REQUIRE_NOTHROW(sharedSeries->addEntry(entry_to_add));
                    thread_added_entries.push_back(entry_to_add);

                    if (j % 5 == 0) {
                        // 1. Lookup the entry just added by this thread
                        try {
                            auto retrieved_entry = sharedSeries->getTimeSeriesEntry(entry_to_add.getDateTime());
                            REQUIRE(retrieved_entry.getCloseValue() == entry_to_add.getCloseValue());
                        } catch (const mkc_timeseries::TimeSeriesDataNotFoundException&) {
                            // If it's not found immediately after adding, it might be a timing issue
                            // The final check after all threads join is more definitive.
                        }

                        // 2. Lookup a ptime that might exist due to other threads or might not.
                        int current_max_offset = ptime_day_offset_counter.load();
                        if (current_max_offset > 0) {
                           std::uniform_int_distribution<> distrib(0, current_max_offset -1);
                           ptime lookup_dt = ptime(date(2020, Jan, 1) + days(distrib(local_rng)), hours(12));
                           try {
                               sharedSeries->getTimeSeriesEntry(lookup_dt);
                           } catch (const mkc_timeseries::TimeSeriesDataNotFoundException&) {
                               // Expected for non-existent entries
                           }
                        }
                    }
                }
                return thread_added_entries;
            }));
        }

        size_t total_successfully_added_count = 0;
        for (int i = 0; i < num_threads; ++i) {
            REQUIRE_NOTHROW(all_added_entries_from_threads[i] = futures[i].get());
            total_successfully_added_count += all_added_entries_from_threads[i].size();
        }

        size_t expected_total_entries = static_cast<size_t>(num_threads) * entries_per_thread;
        REQUIRE(sharedSeries->getNumEntries() == expected_total_entries);
        REQUIRE(total_successfully_added_count == expected_total_entries);

        for (int i = 0; i < num_threads; ++i) {
            for (const auto& expected_entry : all_added_entries_from_threads[i]) {
                auto retrieved_entry = sharedSeries->getTimeSeriesEntry(expected_entry.getDateTime());
                INFO("Final Check: ptime: " << boost::posix_time::to_simple_string(expected_entry.getDateTime()) << " from thread " << i);
                REQUIRE(retrieved_entry == expected_entry);
            }
        }

        ptime non_existent_dt = ptime(date(1999, Dec, 31), hours(12));
        REQUIRE_THROWS_AS(sharedSeries->getTimeSeriesEntry(non_existent_dt), mkc_timeseries::TimeSeriesDataNotFoundException);

        WARN("Concurrent add and read test completed. Final verification passed.");
    }
}

TEST_CASE("IntradayIntervalCalculator Tests", "[TimeSeries][IntradayCalculator]")
{
  using boost::posix_time::minutes;
  using boost::posix_time::hours;
  using boost::posix_time::ptime;
  using boost::gregorian::date;

  SECTION("Calculate from vector of timestamps")
    {
      std::vector<ptime> timestamps = {
	ptime(date(2021, 4, 5), hours(9)),
	ptime(date(2021, 4, 5), hours(10)),
	ptime(date(2021, 4, 5), hours(11)),
	ptime(date(2021, 4, 5), hours(12))
      };

      auto duration = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonInterval(timestamps);
      REQUIRE(duration == hours(1));
      REQUIRE(duration.total_seconds() / 60 == 60);
    }

  SECTION("Calculate with irregular intervals")
    {
      std::vector<ptime> timestamps = {
	ptime(date(2021, 4, 5), hours(9)),      // 60 min gap
	ptime(date(2021, 4, 5), hours(10)),     // 60 min gap
	ptime(date(2021, 4, 5), hours(11)),     // 60 min gap
	ptime(date(2021, 4, 5), hours(12)),     // 120 min gap (holiday early close)
	ptime(date(2021, 4, 5), hours(14)),     // 60 min gap
	ptime(date(2021, 4, 5), hours(15))
      };

      auto duration = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonInterval(timestamps);
      REQUIRE(duration == hours(1)); // Most common is 60 minutes (4 occurrences vs 1)
    }

  SECTION("Exception tests")
    {
      std::vector<ptime> emptyTimestamps;
      REQUIRE_THROWS_AS(mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonInterval(emptyTimestamps),
			mkc_timeseries::TimeSeriesException);

      std::vector<ptime> singleTimestamp = { ptime(date(2021, 4, 5), hours(9)) };
      REQUIRE_THROWS_AS(mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonInterval(singleTimestamp),
			mkc_timeseries::TimeSeriesException);
    }

  SECTION("Calculate from vector of timestamps - minutes")
    {
      std::vector<ptime> timestamps = {
	ptime(date(2021, 4, 5), hours(9)),
	ptime(date(2021, 4, 5), hours(10)),
	ptime(date(2021, 4, 5), hours(11)),
	ptime(date(2021, 4, 5), hours(12))
      };

      auto durationMinutes = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(timestamps);
      REQUIRE(durationMinutes == 60);

      // Verify consistency with time_duration method
      auto duration = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonInterval(timestamps);
      REQUIRE(durationMinutes == duration.total_seconds() / 60);
    }

  SECTION("Calculate with irregular intervals - minutes")
    {
      std::vector<ptime> timestamps = {
	ptime(date(2021, 4, 5), hours(9)),      // 60 min gap
	ptime(date(2021, 4, 5), hours(10)),     // 60 min gap
	ptime(date(2021, 4, 5), hours(11)),     // 60 min gap
	ptime(date(2021, 4, 5), hours(12)),     // 120 min gap (holiday early close)
	ptime(date(2021, 4, 5), hours(14)),     // 60 min gap
	ptime(date(2021, 4, 5), hours(15))
      };

      auto durationMinutes = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(timestamps);
      REQUIRE(durationMinutes == 60); // Most common is 60 minutes (4 occurrences vs 1)

      // Verify consistency with time_duration method
      auto duration = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonInterval(timestamps);
      REQUIRE(durationMinutes == duration.total_seconds() / 60);
    }

  SECTION("Exception tests - minutes")
    {
      std::vector<ptime> emptyTimestamps;
      REQUIRE_THROWS_AS(mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(emptyTimestamps),
			mkc_timeseries::TimeSeriesException);

      std::vector<ptime> singleTimestamp = { ptime(date(2021, 4, 5), hours(9)) };
      REQUIRE_THROWS_AS(mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(singleTimestamp),
			mkc_timeseries::TimeSeriesException);
    }

  SECTION("Various minute intervals")
    {
      // Test 5-minute intervals
      {
	std::vector<ptime> timestamps = {
	  ptime(date(2021, 4, 5), hours(9) + minutes(0)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(5)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(10)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(15))
	};

	auto durationMinutes = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(timestamps);
	REQUIRE(durationMinutes == 5);
      }

      // Test 15-minute intervals
      {
	std::vector<ptime> timestamps = {
	  ptime(date(2021, 4, 5), hours(9) + minutes(0)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(15)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(30)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(45))
	};

	auto durationMinutes = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(timestamps);
	REQUIRE(durationMinutes == 15);
      }

      // Test 30-minute intervals
      {
	std::vector<ptime> timestamps = {
	  ptime(date(2021, 4, 5), hours(9) + minutes(0)),
	  ptime(date(2021, 4, 5), hours(9) + minutes(30)),
	  ptime(date(2021, 4, 5), hours(10) + minutes(0)),
	  ptime(date(2021, 4, 5), hours(10) + minutes(30))
	};

	auto durationMinutes = mkc_timeseries::IntradayIntervalCalculator::calculateMostCommonIntervalInMinutes(timestamps);
	REQUIRE(durationMinutes == 30);
      }
    }
}

TEST_CASE("Intraday Time Frame Duration Tests", "[TimeSeries][IntradayDuration]")
{
  using boost::posix_time::minutes;
  using boost::posix_time::hours;

  SECTION("OHLCTimeSeries - 60 minute intervals")
    {
      OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);

      // Add hourly entries
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "10:00", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "11:00", "101.0", "103.0", "100.5", "102.0", "2000");

      series.addEntry(*entry1);
      series.addEntry(*entry2);
      series.addEntry(*entry3);

      auto duration = series.getIntradayTimeFrameDuration();
      REQUIRE(duration == hours(1));
      REQUIRE(duration.total_seconds() / 60 == 60);
    }

  SECTION("OHLCTimeSeries - Various intervals (1, 5, 15, 30, 60, 90, 135 minutes)")
    {
      // Test 1-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:01", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "09:02", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	auto duration = series.getIntradayTimeFrameDuration();
	REQUIRE(duration == minutes(1));
	REQUIRE(duration.total_seconds() / 60 == 1);
      }

      // Test 5-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:05", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "09:10", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	auto duration = series.getIntradayTimeFrameDuration();
	REQUIRE(duration == minutes(5));
	REQUIRE(duration.total_seconds() / 60 == 5);
      }

      // Test 15-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:15", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "09:30", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	auto duration = series.getIntradayTimeFrameDuration();
	REQUIRE(duration == minutes(15));
	REQUIRE(duration.total_seconds() / 60 == 15);
      }

      // Test 30-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
	auto entry4 = createTimeSeriesEntry("20210405", "10:30", "102.0", "104.0", "101.5", "103.0", "2500");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);
	series.addEntry(*entry4);

	auto duration = series.getIntradayTimeFrameDuration();
	REQUIRE(duration == minutes(30));
	REQUIRE(duration.total_seconds() / 60 == 30);
      }

      // Test 90-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "10:30", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "12:00", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	auto duration = series.getIntradayTimeFrameDuration();
	REQUIRE(duration == minutes(90));
	REQUIRE(duration.total_seconds() / 60 == 90);
      }

      // Test 135-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "11:15", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "13:30", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	auto duration = series.getIntradayTimeFrameDuration();
	REQUIRE(duration == minutes(135));
	REQUIRE(duration.total_seconds() / 60 == 135);
      }
    }

  SECTION("NumericTimeSeries - 60 minute intervals")
    {
      NumericTimeSeries<DecimalType> series(TimeFrame::INTRADAY);

      ptime dt1(date(2021, Apr, 5), hours(9));
      ptime dt2(date(2021, Apr, 5), hours(10));
      ptime dt3(date(2021, Apr, 5), hours(11));

      series.addEntry(*std::make_shared<NumericTimeSeriesEntry<DecimalType>>(dt1, DecimalType("100.0"), TimeFrame::INTRADAY));
      series.addEntry(*std::make_shared<NumericTimeSeriesEntry<DecimalType>>(dt2, DecimalType("101.0"), TimeFrame::INTRADAY));
      series.addEntry(*std::make_shared<NumericTimeSeriesEntry<DecimalType>>(dt3, DecimalType("102.0"), TimeFrame::INTRADAY));

      auto duration = series.getIntradayTimeFrameDuration();
      REQUIRE(duration == hours(1));
      REQUIRE(duration.total_seconds() / 60 == 60);
    }

  SECTION("Exception tests - Non-INTRADAY time frame")
    {
      OHLCTimeSeries<DecimalType> dailySeries(TimeFrame::DAILY, TradingVolume::SHARES);
      REQUIRE_THROWS_AS(dailySeries.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);

      OHLCTimeSeries<DecimalType> weeklySeries(TimeFrame::WEEKLY, TradingVolume::SHARES);
      REQUIRE_THROWS_AS(weeklySeries.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);

      OHLCTimeSeries<DecimalType> monthlySeries(TimeFrame::MONTHLY, TradingVolume::SHARES);
      REQUIRE_THROWS_AS(monthlySeries.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);

      NumericTimeSeries<DecimalType> dailyNumericSeries(TimeFrame::DAILY);
      REQUIRE_THROWS_AS(dailyNumericSeries.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);
    }

  SECTION("Exception tests - Insufficient data")
    {
      OHLCTimeSeries<DecimalType> emptySeries(TimeFrame::INTRADAY, TradingVolume::SHARES);
      REQUIRE_THROWS_AS(emptySeries.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);

      OHLCTimeSeries<DecimalType> singleEntrySeries(TimeFrame::INTRADAY, TradingVolume::SHARES);
      auto entry = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      singleEntrySeries.addEntry(*entry);
      REQUIRE_THROWS_AS(singleEntrySeries.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);
    }

  SECTION("Irregular intervals with holiday gaps")
    {
      OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);

      // Normal 60-minute intervals
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "10:00", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "11:00", "101.0", "103.0", "100.5", "102.0", "2000");
      auto entry4 = createTimeSeriesEntry("20210405", "12:00", "102.0", "104.0", "101.5", "103.0", "2500");

      // Holiday early close - missing 13:00 bar, next bar at 14:00 (120-minute gap)
      auto entry5 = createTimeSeriesEntry("20210405", "14:00", "103.0", "105.0", "102.5", "104.0", "3000");

      // Back to normal 60-minute intervals
      auto entry6 = createTimeSeriesEntry("20210405", "15:00", "104.0", "106.0", "103.5", "105.0", "3500");

      series.addEntry(*entry1);
      series.addEntry(*entry2);
      series.addEntry(*entry3);
      series.addEntry(*entry4);
      series.addEntry(*entry5);
      series.addEntry(*entry6);

      // Should return 60 minutes as it's the most common interval (4 occurrences vs 1 occurrence of 120)
      auto duration = series.getIntradayTimeFrameDuration();
      REQUIRE(duration == hours(1));
      REQUIRE(duration.total_seconds() / 60 == 60);
    }

  SECTION("Duration flexibility tests")
    {
      OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);

      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");

      series.addEntry(*entry1);
      series.addEntry(*entry2);
      series.addEntry(*entry3);

      auto duration = series.getIntradayTimeFrameDuration();

      // Test various ways to extract time information
      REQUIRE(duration.total_seconds() / 60 == 30);
      REQUIRE(duration.total_seconds() == 1800);
      REQUIRE(duration.hours() == 0);
      REQUIRE(duration.minutes() == 30);

      // Test comparison with boost time_duration objects
      REQUIRE(duration == minutes(30));
      REQUIRE(duration < hours(1));
      REQUIRE(duration > minutes(15));
    }
}

TEST_CASE("Intraday Time Frame Duration In Minutes Tests", "[TimeSeries][IntradayDurationMinutes]")
{
  using boost::posix_time::minutes;
  using boost::posix_time::hours;

  SECTION("OHLCTimeSeries - 60 minute intervals")
    {
      OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);

      // Add hourly entries
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "10:00", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "11:00", "101.0", "103.0", "100.5", "102.0", "2000");

      series.addEntry(*entry1);
      series.addEntry(*entry2);
      series.addEntry(*entry3);

      auto durationMinutes = series.getIntradayTimeFrameDurationInMinutes();
      REQUIRE(durationMinutes == 60);

      // Verify consistency with time_duration method
      auto duration = series.getIntradayTimeFrameDuration();
      REQUIRE(durationMinutes == duration.total_seconds() / 60);
    }

  SECTION("OHLCTimeSeries - Various intervals in minutes")
    {
      // Test 1-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:01", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "09:02", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	REQUIRE(series.getIntradayTimeFrameDurationInMinutes() == 1);
      }

      // Test 5-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:05", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "09:10", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	REQUIRE(series.getIntradayTimeFrameDurationInMinutes() == 5);
      }

      // Test 15-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:15", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "09:30", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	REQUIRE(series.getIntradayTimeFrameDurationInMinutes() == 15);
      }

      // Test 30-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	REQUIRE(series.getIntradayTimeFrameDurationInMinutes() == 30);
      }

      // Test 90-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "10:30", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "12:00", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	REQUIRE(series.getIntradayTimeFrameDurationInMinutes() == 90);
      }

      // Test 135-minute intervals
      {
	OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
	auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
	auto entry2 = createTimeSeriesEntry("20210405", "11:15", "100.5", "102.0", "100.0", "101.0", "1500");
	auto entry3 = createTimeSeriesEntry("20210405", "13:30", "101.0", "103.0", "100.5", "102.0", "2000");

	series.addEntry(*entry1);
	series.addEntry(*entry2);
	series.addEntry(*entry3);

	REQUIRE(series.getIntradayTimeFrameDurationInMinutes() == 135);
      }
    }

  SECTION("NumericTimeSeries - 60 minute intervals")
    {
      NumericTimeSeries<DecimalType> series(TimeFrame::INTRADAY);

      ptime dt1(date(2021, Apr, 5), hours(9));
      ptime dt2(date(2021, Apr, 5), hours(10));
      ptime dt3(date(2021, Apr, 5), hours(11));

      series.addEntry(*std::make_shared<NumericTimeSeriesEntry<DecimalType>>(dt1, DecimalType("100.0"), TimeFrame::INTRADAY));
      series.addEntry(*std::make_shared<NumericTimeSeriesEntry<DecimalType>>(dt2, DecimalType("101.0"), TimeFrame::INTRADAY));
      series.addEntry(*std::make_shared<NumericTimeSeriesEntry<DecimalType>>(dt3, DecimalType("102.0"), TimeFrame::INTRADAY));

      auto durationMinutes = series.getIntradayTimeFrameDurationInMinutes();
      REQUIRE(durationMinutes == 60);

      // Verify consistency with time_duration method
      auto duration = series.getIntradayTimeFrameDuration();
      REQUIRE(durationMinutes == duration.total_seconds() / 60);
    }

  SECTION("Exception tests - Non-INTRADAY time frame")
    {
      OHLCTimeSeries<DecimalType> dailySeries(TimeFrame::DAILY, TradingVolume::SHARES);
      REQUIRE_THROWS_AS(dailySeries.getIntradayTimeFrameDurationInMinutes(), mkc_timeseries::TimeSeriesException);

      NumericTimeSeries<DecimalType> dailyNumericSeries(TimeFrame::DAILY);
      REQUIRE_THROWS_AS(dailyNumericSeries.getIntradayTimeFrameDurationInMinutes(), mkc_timeseries::TimeSeriesException);
    }

  SECTION("Exception tests - Insufficient data")
    {
      OHLCTimeSeries<DecimalType> emptySeries(TimeFrame::INTRADAY, TradingVolume::SHARES);
      REQUIRE_THROWS_AS(emptySeries.getIntradayTimeFrameDurationInMinutes(), mkc_timeseries::TimeSeriesException);

      OHLCTimeSeries<DecimalType> singleEntrySeries(TimeFrame::INTRADAY, TradingVolume::SHARES);
      auto entry = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      singleEntrySeries.addEntry(*entry);
      REQUIRE_THROWS_AS(singleEntrySeries.getIntradayTimeFrameDurationInMinutes(), mkc_timeseries::TimeSeriesException);
    }

  SECTION("Consistency between duration methods")
    {
      OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);

      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");

      series.addEntry(*entry1);
      series.addEntry(*entry2);
      series.addEntry(*entry3);

      auto duration = series.getIntradayTimeFrameDuration();
      auto durationMinutes = series.getIntradayTimeFrameDurationInMinutes();

      // Both methods should return consistent results
      REQUIRE(durationMinutes == duration.total_seconds() / 60);
      REQUIRE(durationMinutes == 30);
    }

  SECTION("Irregular intervals with holiday gaps")
    {
      OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);

      // Normal 30-minute intervals
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
      auto entry4 = createTimeSeriesEntry("20210405", "10:30", "102.0", "104.0", "101.5", "103.0", "2500");

      // Holiday early close - missing 11:00 bar, next bar at 12:00 (90-minute gap)
      auto entry5 = createTimeSeriesEntry("20210405", "12:00", "103.0", "105.0", "102.5", "104.0", "3000");

      // Back to normal 30-minute intervals
      auto entry6 = createTimeSeriesEntry("20210405", "12:30", "104.0", "106.0", "103.5", "105.0", "3500");

      series.addEntry(*entry1);
      series.addEntry(*entry2);
      series.addEntry(*entry3);
      series.addEntry(*entry4);
      series.addEntry(*entry5);
      series.addEntry(*entry6);

      // Should return 30 minutes as it's the most common interval (4 occurrences vs 1 occurrence of 90)
      auto durationMinutes = series.getIntradayTimeFrameDurationInMinutes();
      REQUIRE(durationMinutes == 30);

      // Verify consistency with time_duration method
      auto duration = series.getIntradayTimeFrameDuration();
      REQUIRE(durationMinutes == duration.total_seconds() / 60);
    }
}

TEST_CASE("OHLCTimeSeries Comprehensive Coverage Tests", "[OHLCTimeSeries][Coverage]")
{
    SECTION("Constructor - Range-based constructor")
    {
        // Create a vector of entries
        std::vector<OHLCTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        // Test range-based constructor
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES, 
                                           entries.begin(), entries.end());
        
        REQUIRE(series.getNumEntries() == 3);
        REQUIRE(series.getTimeFrame() == TimeFrame::DAILY);
        REQUIRE(series.getVolumeUnits() == TradingVolume::SHARES);
        
        // Verify entries are sorted and accessible
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getCloseValue() == DecimalType("103.0"));
        
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getCloseValue() == DecimalType("106.0"));
        
        auto entry3 = series.getTimeSeriesEntry(d3);
        REQUIRE(entry3.getCloseValue() == DecimalType("109.0"));
    }
    
    SECTION("Constructor - Range-based with unsorted entries")
    {
        std::vector<OHLCTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        // Add entries out of order
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES,
                                           entries.begin(), entries.end());
        
        // Should be sorted after construction
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d3);
    }
    
    SECTION("Constructor - Range-based with mismatched timeframe throws")
    {
        std::vector<OHLCTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, hours(9)),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::INTRADAY)); // Wrong timeframe
        
        REQUIRE_THROWS_AS(
            OHLCTimeSeries<DecimalType>(TimeFrame::DAILY, TradingVolume::SHARES,
                                        entries.begin(), entries.end()),
            TimeSeriesException);
    }
    
    SECTION("Copy constructor")
    {
        OHLCTimeSeries<DecimalType> original(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        original.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        original.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        // Copy construct
        OHLCTimeSeries<DecimalType> copy(original);
        
        REQUIRE(copy.getNumEntries() == original.getNumEntries());
        REQUIRE(copy.getTimeFrame() == original.getTimeFrame());
        REQUIRE(copy.getVolumeUnits() == original.getVolumeUnits());
        
        // Verify deep copy - entries are independent
        auto origEntry = original.getTimeSeriesEntry(d1);
        auto copyEntry = copy.getTimeSeriesEntry(d1);
        REQUIRE(origEntry == copyEntry);
    }
    
    SECTION("Copy assignment operator")
    {
        OHLCTimeSeries<DecimalType> original(TimeFrame::DAILY, TradingVolume::SHARES);
        OHLCTimeSeries<DecimalType> copy(TimeFrame::WEEKLY, TradingVolume::CONTRACTS);
        
        date d1(2021, Apr, 5);
        
        original.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        // Copy assign
        copy = original;
        
        REQUIRE(copy.getNumEntries() == original.getNumEntries());
        REQUIRE(copy.getTimeFrame() == TimeFrame::DAILY); // Should be copied
        REQUIRE(copy.getVolumeUnits() == TradingVolume::SHARES); // Should be copied
        
        auto entry = copy.getTimeSeriesEntry(d1);
        REQUIRE(entry.getCloseValue() == DecimalType("103.0"));
    }
    
    SECTION("Move constructor")
    {
        OHLCTimeSeries<DecimalType> original(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        
        original.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        // Move construct
        OHLCTimeSeries<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getNumEntries() == 1);
        REQUIRE(moved.getTimeFrame() == TimeFrame::DAILY);
        REQUIRE(moved.getVolumeUnits() == TradingVolume::SHARES);
        
        auto entry = moved.getTimeSeriesEntry(d1);
        REQUIRE(entry.getCloseValue() == DecimalType("103.0"));
    }
    
    SECTION("Move assignment operator")
    {
        OHLCTimeSeries<DecimalType> original(TimeFrame::DAILY, TradingVolume::SHARES);
        OHLCTimeSeries<DecimalType> moved(TimeFrame::WEEKLY, TradingVolume::CONTRACTS);
        
        date d1(2021, Apr, 5);
        
        original.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        // Move assign
        moved = std::move(original);
        
        REQUIRE(moved.getNumEntries() == 1);
        REQUIRE(moved.getTimeFrame() == TimeFrame::DAILY);
        REQUIRE(moved.getVolumeUnits() == TradingVolume::SHARES);
    }
    
    SECTION("getTimeSeriesEntry with offset - positive offsets")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        date d4(2021, Apr, 8);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d4, getDefaultBarTime()),
            DecimalType("109.0"), DecimalType("112.0"), DecimalType("108.0"), DecimalType("111.0"),
            DecimalType("2500"), TimeFrame::DAILY));
        
        // Test offset 0 (same entry)
        auto entry0 = series.getTimeSeriesEntry(d4, 0);
        REQUIRE(entry0.getCloseValue() == DecimalType("111.0"));
        REQUIRE(entry0.getDateValue() == d4);
        
        // Test offset 1 (one bar ago)
        auto entry1 = series.getTimeSeriesEntry(d4, 1);
        REQUIRE(entry1.getCloseValue() == DecimalType("109.0"));
        REQUIRE(entry1.getDateValue() == d3);
        
        // Test offset 2 (two bars ago)
        auto entry2 = series.getTimeSeriesEntry(d4, 2);
        REQUIRE(entry2.getCloseValue() == DecimalType("106.0"));
        REQUIRE(entry2.getDateValue() == d2);
        
        // Test offset 3 (three bars ago - first entry)
        auto entry3 = series.getTimeSeriesEntry(d4, 3);
        REQUIRE(entry3.getCloseValue() == DecimalType("103.0"));
        REQUIRE(entry3.getDateValue() == d1);
        
        // Test with ptime overload
        ptime pt4(d4, getDefaultBarTime());
        auto entryPt = series.getTimeSeriesEntry(pt4, 2);
        REQUIRE(entryPt.getCloseValue() == DecimalType("106.0"));
    }
    
    SECTION("getTimeSeriesEntry with offset - negative offsets (forward in time)")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        date d4(2021, Apr, 8);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d4, getDefaultBarTime()),
            DecimalType("109.0"), DecimalType("112.0"), DecimalType("108.0"), DecimalType("111.0"),
            DecimalType("2500"), TimeFrame::DAILY));
        
        // Test negative offset -1 (one bar forward)
        auto entry1 = series.getTimeSeriesEntry(d1, -1);
        REQUIRE(entry1.getCloseValue() == DecimalType("106.0"));
        REQUIRE(entry1.getDateValue() == d2);
        
        // Test negative offset -2 (two bars forward)
        auto entry2 = series.getTimeSeriesEntry(d1, -2);
        REQUIRE(entry2.getCloseValue() == DecimalType("109.0"));
        REQUIRE(entry2.getDateValue() == d3);
        
        // Test negative offset -3 (three bars forward - last entry)
        auto entry3 = series.getTimeSeriesEntry(d1, -3);
        REQUIRE(entry3.getCloseValue() == DecimalType("111.0"));
        REQUIRE(entry3.getDateValue() == d4);
    }
    
    SECTION("getTimeSeriesEntry with offset - out of bounds exceptions")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        // Test offset beyond start
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, 1), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d2, 2), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, 10), TimeSeriesOffsetOutOfRangeException);
        
        // Test negative offset beyond end
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d3, -1), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d2, -2), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, -10), TimeSeriesOffsetOutOfRangeException);
    }
    
    SECTION("getTimeSeriesEntry with offset - base date not found")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date nonExistent(2021, Apr, 10);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(nonExistent, 0), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(nonExistent, 1), TimeSeriesDataNotFoundException);
    }
    
    SECTION("Boundary methods - getFirstDate/getLastDate")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d3);
    }
    
    SECTION("Boundary methods - getFirstDateTime/getLastDateTime")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        ptime pt1(d1, getDefaultBarTime());
        ptime pt2(d2, getDefaultBarTime());
        ptime pt3(d3, getDefaultBarTime());
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt2, DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt1, DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt3, DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        REQUIRE(series.getFirstDateTime() == pt1);
        REQUIRE(series.getLastDateTime() == pt3);
    }
    
    SECTION("Boundary methods - empty series throws")
    {
        OHLCTimeSeries<DecimalType> emptySeries(TimeFrame::DAILY, TradingVolume::SHARES);
        
        REQUIRE_THROWS_AS(emptySeries.getFirstDate(), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getLastDate(), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getFirstDateTime(), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getLastDateTime(), TimeSeriesDataNotFoundException);
    }
    
    SECTION("isDateFound - date overload")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date nonExistent(2021, Apr, 10);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        REQUIRE(series.isDateFound(d1) == true);
        REQUIRE(series.isDateFound(d2) == true);
        REQUIRE(series.isDateFound(nonExistent) == false);
    }
    
    SECTION("isDateFound - ptime overload")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        ptime pt1(date(2021, Apr, 5), hours(9));
        ptime pt2(date(2021, Apr, 5), hours(10));
        ptime nonExistent(date(2021, Apr, 5), hours(15));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt1, DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::INTRADAY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt2, DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::INTRADAY));
        
        REQUIRE(series.isDateFound(pt1) == true);
        REQUIRE(series.isDateFound(pt2) == true);
        REQUIRE(series.isDateFound(nonExistent) == false);
    }
    
    SECTION("deleteEntryByDate - ptime overload")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        ptime pt1(date(2021, Apr, 5), hours(9));
        ptime pt2(date(2021, Apr, 5), hours(10));
        ptime pt3(date(2021, Apr, 5), hours(11));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt1, DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::INTRADAY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt2, DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::INTRADAY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            pt3, DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::INTRADAY));
        
        REQUIRE(series.getNumEntries() == 3);
        
        // Delete middle entry
        series.deleteEntryByDate(pt2);
        
        REQUIRE(series.getNumEntries() == 2);
        REQUIRE(series.isDateFound(pt1) == true);
        REQUIRE(series.isDateFound(pt2) == false);
        REQUIRE(series.isDateFound(pt3) == true);
    }
    
    SECTION("deleteEntryByDate - date overload")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        REQUIRE(series.getNumEntries() == 3);
        
        // Delete first entry
        series.deleteEntryByDate(d1);
        
        REQUIRE(series.getNumEntries() == 2);
        REQUIRE(series.isDateFound(d1) == false);
        REQUIRE(series.isDateFound(d2) == true);
        REQUIRE(series.getFirstDate() == d2);
    }
    
    SECTION("Comparison operators - equality")
    {
        OHLCTimeSeries<DecimalType> series1(TimeFrame::DAILY, TradingVolume::SHARES);
        OHLCTimeSeries<DecimalType> series2(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        auto entry1 = OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY);
        
        auto entry2 = OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY);
        
        series1.addEntry(entry1);
        series1.addEntry(entry2);
        
        series2.addEntry(entry1);
        series2.addEntry(entry2);
        
        REQUIRE(series1 == series2);
        REQUIRE_FALSE(series1 != series2);
    }
    
    SECTION("Comparison operators - inequality by different entries")
    {
        OHLCTimeSeries<DecimalType> series1(TimeFrame::DAILY, TradingVolume::SHARES);
        OHLCTimeSeries<DecimalType> series2(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        series1.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series2.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        REQUIRE(series1 != series2);
        REQUIRE_FALSE(series1 == series2);
    }
    
    SECTION("Comparison operators - inequality by timeframe")
    {
        OHLCTimeSeries<DecimalType> series1(TimeFrame::DAILY, TradingVolume::SHARES);
        OHLCTimeSeries<DecimalType> series2(TimeFrame::WEEKLY, TradingVolume::SHARES);
        
        REQUIRE(series1 != series2);
        REQUIRE_FALSE(series1 == series2);
    }
    
    SECTION("Comparison operators - inequality by volume units")
    {
        OHLCTimeSeries<DecimalType> series1(TimeFrame::DAILY, TradingVolume::SHARES);
        OHLCTimeSeries<DecimalType> series2(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        
        REQUIRE(series1 != series2);
        REQUIRE_FALSE(series1 == series2);
    }
    
    SECTION("Stream output operator")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        std::ostringstream oss;
        oss << series;
        
        std::string output = oss.str();
        
        // Check that output contains header and data
        REQUIRE(output.find("DateTime,Open,High,Low,Close,Volume") != std::string::npos);
        REQUIRE(output.find("100") != std::string::npos);
        REQUIRE(output.find("105") != std::string::npos);
        REQUIRE(output.find("103") != std::string::npos);
    }
    
    SECTION("Duplicate timestamp handling")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        // Attempt to add duplicate
        REQUIRE_THROWS_AS(
            series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
                ptime(d1, getDefaultBarTime()),
                DecimalType("101.0"), DecimalType("106.0"), DecimalType("100.0"), DecimalType("104.0"),
                DecimalType("1100"), TimeFrame::DAILY)),
            TimeSeriesException);
    }
    
    SECTION("Single entry series operations")
    {
        OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        REQUIRE(series.getNumEntries() == 1);
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d1);
        REQUIRE(series.isDateFound(d1) == true);
        
        auto entry = series.getTimeSeriesEntry(d1);
        REQUIRE(entry.getCloseValue() == DecimalType("103.0"));
        
        // Offset 0 should work
        auto entryOffset0 = series.getTimeSeriesEntry(d1, 0);
        REQUIRE(entryOffset0.getCloseValue() == DecimalType("103.0"));
        
        // Any non-zero offset should throw
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, 1), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, -1), TimeSeriesOffsetOutOfRangeException);
    }
}

TEST_CASE("OHLCTimeSeries with HashedLookupPolicy", "[OHLCTimeSeries][HashedPolicy]")
{
    using HashedOHLCTimeSeries = OHLCTimeSeries<DecimalType, HashedLookupPolicy<DecimalType>>;
    
    SECTION("Basic operations with hashed policy")
    {
        HashedOHLCTimeSeries series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        // Test lookups work with hashed policy
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getCloseValue() == DecimalType("103.0"));
        
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getCloseValue() == DecimalType("106.0"));
        
        auto entry3 = series.getTimeSeriesEntry(d3);
        REQUIRE(entry3.getCloseValue() == DecimalType("109.0"));
    }
    
    SECTION("Index invalidation after addEntry")
    {
        HashedOHLCTimeSeries series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        // Access to build index
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getCloseValue() == DecimalType("103.0"));
        
        // Add new entry (should invalidate index)
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        // Should still work (index rebuilt)
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getCloseValue() == DecimalType("106.0"));
        
        // Original entry should still be accessible
        auto entry1Again = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1Again.getCloseValue() == DecimalType("103.0"));
    }
    
    SECTION("Index invalidation after deleteEntryByDate")
    {
        HashedOHLCTimeSeries series(TimeFrame::DAILY, TradingVolume::SHARES);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        series.addEntry(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()),
            DecimalType("106.0"), DecimalType("110.0"), DecimalType("105.0"), DecimalType("109.0"),
            DecimalType("2000"), TimeFrame::DAILY));
        
        // Access to build index
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getCloseValue() == DecimalType("106.0"));
        
        // Delete entry (should invalidate index)
        series.deleteEntryByDate(d2);
        
        // Should still work with remaining entries
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getCloseValue() == DecimalType("103.0"));
        
        auto entry3 = series.getTimeSeriesEntry(d3);
        REQUIRE(entry3.getCloseValue() == DecimalType("109.0"));
        
        // Deleted entry should not be found
        REQUIRE(series.isDateFound(d2) == false);
    }
    
    SECTION("Range-based constructor builds index")
    {
        std::vector<OHLCTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()),
            DecimalType("100.0"), DecimalType("105.0"), DecimalType("99.0"), DecimalType("103.0"),
            DecimalType("1000"), TimeFrame::DAILY));
        
        entries.push_back(OHLCTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()),
            DecimalType("103.0"), DecimalType("108.0"), DecimalType("102.0"), DecimalType("106.0"),
            DecimalType("1500"), TimeFrame::DAILY));
        
        HashedOHLCTimeSeries series(TimeFrame::DAILY, TradingVolume::SHARES,
                                    entries.begin(), entries.end());
        
        // Index should be built during construction
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getCloseValue() == DecimalType("103.0"));
        
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getCloseValue() == DecimalType("106.0"));
    }
}
