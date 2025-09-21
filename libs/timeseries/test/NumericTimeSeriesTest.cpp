#include <catch2/catch_test_macros.hpp>
#include "TimeSeries.h"
#include "TestUtils.h"
#include "number.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::ptime;
using boost::posix_time::hours;
using boost::posix_time::minutes;

typedef num::DefaultNumber DecimalType;

TEST_CASE("NumericTimeSeries New Interface Comprehensive Tests", "[NumericTimeSeries][NewInterface]")
{
    SECTION("getTimeSeriesEntry with ptime - comprehensive coverage")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::INTRADAY);
        
        ptime dt1(date(2021, Apr, 5), hours(9));
        ptime dt2(date(2021, Apr, 5), hours(10));
        ptime dt3(date(2021, Apr, 5), hours(11));
        
        // Add entries using the corrected interface
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(dt1, DecimalType("100.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(dt2, DecimalType("101.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(dt3, DecimalType("102.0"), TimeFrame::INTRADAY));
        
        // Test getTimeSeriesEntry by ptime (new interface)
        auto entry1 = series.getTimeSeriesEntry(dt1);
        REQUIRE(entry1.getValue() == DecimalType("100.0"));
        REQUIRE(entry1.getDateTime() == dt1);
        
        auto entry2 = series.getTimeSeriesEntry(dt2);
        REQUIRE(entry2.getValue() == DecimalType("101.0"));
        REQUIRE(entry2.getDateTime() == dt2);
        
        auto entry3 = series.getTimeSeriesEntry(dt3);
        REQUIRE(entry3.getValue() == DecimalType("102.0"));
        REQUIRE(entry3.getDateTime() == dt3);
        
        // Test exception for non-existent ptime
        ptime nonExistent(date(2021, Apr, 5), hours(12));
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(nonExistent), TimeSeriesDataNotFoundException);
    }
    
    SECTION("getTimeSeriesEntry with date - comprehensive coverage")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        // Add entries using date constructor (gets default bar time automatically)
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d1, DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d2, DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d3, DecimalType("102.0"), TimeFrame::DAILY));
        
        // Test getTimeSeriesEntry by date
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getValue() == DecimalType("100.0"));
        REQUIRE(entry1.getDateTime().date() == d1);
        
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getValue() == DecimalType("101.0"));
        REQUIRE(entry2.getDateTime().date() == d2);
        
        // Test exception for non-existent date
        date nonExistent(2021, Apr, 10);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(nonExistent), TimeSeriesDataNotFoundException);
    }
    
    SECTION("getTimeSeriesEntry with offset - new functionality")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        date d4(2021, Apr, 8);
        
        // Add entries in chronological order using date constructor
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d1, DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d2, DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d3, DecimalType("102.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d4, DecimalType("103.0"), TimeFrame::DAILY));
        
        // Test getTimeSeriesEntry with ptime and offset
        ptime baseTime = ptime(d3, getDefaultBarTime()); // April 7th with default time
        
        // Offset 0 should return the same entry
        auto entry0 = series.getTimeSeriesEntry(baseTime, 0);
        REQUIRE(entry0.getValue() == DecimalType("102.0"));
        REQUIRE(entry0.getDateTime().date() == d3);
        
        // Offset 1 should return previous entry (April 6th)
        auto entry1 = series.getTimeSeriesEntry(baseTime, 1);
        REQUIRE(entry1.getValue() == DecimalType("101.0"));
        REQUIRE(entry1.getDateTime().date() == d2);
        
        // Offset 2 should return entry from April 5th
        auto entry2 = series.getTimeSeriesEntry(baseTime, 2);
        REQUIRE(entry2.getValue() == DecimalType("100.0"));
        REQUIRE(entry2.getDateTime().date() == d1);
        
        // Test getTimeSeriesEntry with date and offset
        auto entryDateOffset1 = series.getTimeSeriesEntry(d4, 1); // From April 8th, go back 1
        REQUIRE(entryDateOffset1.getValue() == DecimalType("102.0"));
        REQUIRE(entryDateOffset1.getDateTime().date() == d3);
        
        // Test offset beyond available data
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(baseTime, 5), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d2, 3), TimeSeriesOffsetOutOfRangeException);
    }
    
    SECTION("getValue with ptime and offset - comprehensive coverage")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d1, DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d2, DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d3, DecimalType("102.0"), TimeFrame::DAILY));
        
        ptime baseTime = ptime(d3, getDefaultBarTime());
        
        // Test getValue with different offsets
        REQUIRE(series.getValue(baseTime, 0) == DecimalType("102.0"));
        REQUIRE(series.getValue(baseTime, 1) == DecimalType("101.0"));
        REQUIRE(series.getValue(baseTime, 2) == DecimalType("100.0"));
        
        // Test getValue with date and offset
        REQUIRE(series.getValue(d3, 0) == DecimalType("102.0"));
        REQUIRE(series.getValue(d3, 1) == DecimalType("101.0"));
        REQUIRE(series.getValue(d3, 2) == DecimalType("100.0"));
        
        // Test exception for invalid offset
        REQUIRE_THROWS_AS(series.getValue(baseTime, 5), TimeSeriesOffsetOutOfRangeException);
    }
    
    SECTION("Entry object functionality")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        DecimalType value("123.45");
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d1, value, TimeFrame::DAILY));
        ptime dt1(d1, getDefaultBarTime()); // Expected datetime with default bar time
        
        // Test Entry object returned by getTimeSeriesEntry
        auto entry = series.getTimeSeriesEntry(d1);
        
        // Test Entry methods
        REQUIRE(entry.getValue() == value);
        REQUIRE(entry.getDateTime() == dt1);
        REQUIRE(entry.getDateTime().date() == d1);
        REQUIRE(entry.getTimeFrame() == TimeFrame::DAILY);
        
        // Test Entry equality
        NumericTimeSeriesEntry<DecimalType> expectedEntry(dt1, value, TimeFrame::DAILY);
        REQUIRE(entry == expectedEntry);
    }
    
    SECTION("Iterator interface with new Entry access")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d1, DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d2, DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(d3, DecimalType("102.0"), TimeFrame::DAILY));
        
        // Test ConstSortedIterator with direct Entry access
        auto it = series.beginSortedAccess();
        REQUIRE(it->getValue() == DecimalType("100.0")); // First entry chronologically
        REQUIRE(it->getDateTime().date() == d1);
        
        ++it;
        REQUIRE(it->getValue() == DecimalType("101.0"));
        REQUIRE(it->getDateTime().date() == d2);
        
        ++it;
        REQUIRE(it->getValue() == DecimalType("102.0"));
        REQUIRE(it->getDateTime().date() == d3);
        
        ++it;
        REQUIRE(it == series.endSortedAccess());
        
        // Test ConstRandomAccessIterator
        auto randomIt = series.beginRandomAccess();
        REQUIRE(randomIt->getValue() == DecimalType("100.0"));
        
        randomIt += 2;
        REQUIRE(randomIt->getValue() == DecimalType("102.0"));
        
        randomIt--;
        REQUIRE(randomIt->getValue() == DecimalType("101.0"));
    }
    
    SECTION("Edge cases and error conditions")
    {
        NumericTimeSeries<DecimalType> emptySeries(TimeFrame::DAILY);
        
        // Test operations on empty series
        date testDate(2021, Apr, 5);
        ptime testTime(testDate, getDefaultBarTime());
        
        REQUIRE_THROWS_AS(emptySeries.getTimeSeriesEntry(testDate), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getTimeSeriesEntry(testTime), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getTimeSeriesEntry(testTime, 0), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getValue(testTime, 0), TimeSeriesDataNotFoundException);
        
        // Test single entry series
        NumericTimeSeries<DecimalType> singleSeries(TimeFrame::DAILY);
        singleSeries.addEntry(NumericTimeSeriesEntry<DecimalType>(testDate, DecimalType("100.0"), TimeFrame::DAILY));
        
        // Valid operations
        REQUIRE_NOTHROW(singleSeries.getTimeSeriesEntry(testDate));
        REQUIRE_NOTHROW(singleSeries.getTimeSeriesEntry(testTime, 0));
        REQUIRE_NOTHROW(singleSeries.getValue(testTime, 0));
        
        // Invalid offset
        REQUIRE_THROWS_AS(singleSeries.getTimeSeriesEntry(testTime, 1), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(singleSeries.getValue(testTime, 1), TimeSeriesOffsetOutOfRangeException);
    }
    
    SECTION("Performance and consistency checks")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        // Add multiple entries
        std::vector<date> dates;
        std::vector<DecimalType> values;
        
        for (int i = 0; i < 100; ++i) {
            date d(2021, Apr, 1 + i % 30); // Cycle through April dates
            DecimalType value(100.0 + i);
            
            if (std::find(dates.begin(), dates.end(), d) == dates.end()) {
                dates.push_back(d);
                values.push_back(value);
                series.addEntry(NumericTimeSeriesEntry<DecimalType>(d, value, TimeFrame::DAILY));
            }
        }
        
        // Test consistency between different access methods
        for (size_t i = 0; i < dates.size(); ++i) {
            auto entryByDate = series.getTimeSeriesEntry(dates[i]);
            auto entryByPtime = series.getTimeSeriesEntry(ptime(dates[i], getDefaultBarTime()));
            auto valueByDate = series.getValue(dates[i], 0);
            auto valueByPtime = series.getValue(ptime(dates[i], getDefaultBarTime()), 0);
            
            REQUIRE(entryByDate == entryByPtime);
            REQUIRE(entryByDate.getValue() == valueByDate);
            REQUIRE(entryByDate.getValue() == valueByPtime);
        }
    }
}