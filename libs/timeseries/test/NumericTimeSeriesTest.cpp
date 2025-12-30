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

TEST_CASE("NumericTimeSeries Additional Coverage Tests", "[NumericTimeSeries][Coverage]")
{
    SECTION("Constructor - range-based constructor")
    {
        std::vector<NumericTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY, entries.begin(), entries.end());
        
        REQUIRE(series.getNumEntries() == 3);
        REQUIRE(series.getTimeFrame() == TimeFrame::DAILY);
        
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getValue() == DecimalType("100.0"));
        
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getValue() == DecimalType("101.0"));
        
        auto entry3 = series.getTimeSeriesEntry(d3);
        REQUIRE(entry3.getValue() == DecimalType("102.0"));
    }
    
    SECTION("Constructor - range-based with unsorted entries")
    {
        std::vector<NumericTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        // Add entries out of order
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY, entries.begin(), entries.end());
        
        // Should be sorted after construction
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d3);
    }
    
    SECTION("Constructor - range-based with mismatched timeframe throws")
    {
        std::vector<NumericTimeSeriesEntry<DecimalType>> entries;
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        entries.push_back(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, hours(9)), DecimalType("101.0"), TimeFrame::INTRADAY)); // Wrong timeframe
        
        REQUIRE_THROWS_AS(
            NumericTimeSeries<DecimalType>(TimeFrame::DAILY, entries.begin(), entries.end()),
            TimeSeriesException);
    }
    
    SECTION("Constructor - with reserve count")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY, 100);
        
        REQUIRE(series.getNumEntries() == 0);
        REQUIRE(series.getTimeFrame() == TimeFrame::DAILY);
        
        // Should be able to add entries
        date d1(2021, Apr, 5);
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        REQUIRE(series.getNumEntries() == 1);
    }
    
    SECTION("Copy constructor")
    {
        NumericTimeSeries<DecimalType> original(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        original.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        original.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        
        NumericTimeSeries<DecimalType> copy(original);
        
        REQUIRE(copy.getNumEntries() == original.getNumEntries());
        REQUIRE(copy.getTimeFrame() == original.getTimeFrame());
        
        auto origEntry = original.getTimeSeriesEntry(d1);
        auto copyEntry = copy.getTimeSeriesEntry(d1);
        REQUIRE(origEntry == copyEntry);
    }
    
    SECTION("Copy assignment operator")
    {
        NumericTimeSeries<DecimalType> original(TimeFrame::DAILY);
        NumericTimeSeries<DecimalType> copy(TimeFrame::WEEKLY);
        
        date d1(2021, Apr, 5);
        
        original.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        copy = original;
        
        REQUIRE(copy.getNumEntries() == original.getNumEntries());
        REQUIRE(copy.getTimeFrame() == TimeFrame::DAILY); // Should be copied
        
        auto entry = copy.getTimeSeriesEntry(d1);
        REQUIRE(entry.getValue() == DecimalType("100.0"));
    }
    
    SECTION("Copy assignment - self-assignment")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        // Self-assignment should be safe
        series = series;
        
        REQUIRE(series.getNumEntries() == 1);
        auto entry = series.getTimeSeriesEntry(d1);
        REQUIRE(entry.getValue() == DecimalType("100.0"));
    }
    
    SECTION("Move constructor")
    {
        NumericTimeSeries<DecimalType> original(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        original.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        NumericTimeSeries<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getNumEntries() == 1);
        REQUIRE(moved.getTimeFrame() == TimeFrame::DAILY);
        
        auto entry = moved.getTimeSeriesEntry(d1);
        REQUIRE(entry.getValue() == DecimalType("100.0"));
    }
    
    SECTION("Move assignment operator")
    {
        NumericTimeSeries<DecimalType> original(TimeFrame::DAILY);
        NumericTimeSeries<DecimalType> moved(TimeFrame::WEEKLY);
        
        date d1(2021, Apr, 5);
        original.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        moved = std::move(original);
        
        REQUIRE(moved.getNumEntries() == 1);
        REQUIRE(moved.getTimeFrame() == TimeFrame::DAILY);
    }
    
    SECTION("Move assignment - self-assignment")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        // Test self-move-assignment protection by creating a reference and moving through it
        // This avoids the direct self-move which triggers compiler warnings
        NumericTimeSeries<DecimalType>& seriesRef = series;
        series = std::move(seriesRef);
        
        REQUIRE(series.getNumEntries() == 1);
    }
    
    SECTION("getDateValue with ptime and offset")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        
        ptime baseTime = ptime(d3, getDefaultBarTime());
        
        // Test offset 0
        REQUIRE(series.getDateValue(baseTime, 0) == d3);
        
        // Test offset 1
        REQUIRE(series.getDateValue(baseTime, 1) == d2);
        
        // Test offset 2
        REQUIRE(series.getDateValue(baseTime, 2) == d1);
    }
    
    SECTION("getDateValue with date and offset")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        
        // Test offset 0
        REQUIRE(series.getDateValue(d3, 0) == d3);
        
        // Test offset 1
        REQUIRE(series.getDateValue(d3, 1) == d2);
        
        // Test offset 2
        REQUIRE(series.getDateValue(d3, 2) == d1);
    }
    
    SECTION("getDateTimeValue with ptime and offset")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::INTRADAY);
        
        ptime pt1(date(2021, Apr, 5), hours(9));
        ptime pt2(date(2021, Apr, 5), hours(10));
        ptime pt3(date(2021, Apr, 5), hours(11));
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt2, DecimalType("101.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt3, DecimalType("102.0"), TimeFrame::INTRADAY));
        
        // Test offset 0
        REQUIRE(series.getDateTimeValue(pt3, 0) == pt3);
        
        // Test offset 1
        REQUIRE(series.getDateTimeValue(pt3, 1) == pt2);
        
        // Test offset 2
        REQUIRE(series.getDateTimeValue(pt3, 2) == pt1);
    }
    
    SECTION("getDateTimeValue with date and offset")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        ptime pt1(d1, getDefaultBarTime());
        ptime pt2(d2, getDefaultBarTime());
        ptime pt3(d3, getDefaultBarTime());
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt2, DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt3, DecimalType("102.0"), TimeFrame::DAILY));
        
        // Test offset 0
        REQUIRE(series.getDateTimeValue(d3, 0) == pt3);
        
        // Test offset 1
        REQUIRE(series.getDateTimeValue(d3, 1) == pt2);
        
        // Test offset 2
        REQUIRE(series.getDateTimeValue(d3, 2) == pt1);
    }
    
    SECTION("Boundary methods - comprehensive testing")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        ptime pt1(d1, getDefaultBarTime());
        ptime pt2(d2, getDefaultBarTime());
        ptime pt3(d3, getDefaultBarTime());
        
        // Add entries out of order
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt2, DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt3, DecimalType("102.0"), TimeFrame::DAILY));
        
        // Should be sorted, so first is d1, last is d3
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d3);
        REQUIRE(series.getFirstDateTime() == pt1);
        REQUIRE(series.getLastDateTime() == pt3);
    }
    
    SECTION("Boundary methods - empty series throws")
    {
        NumericTimeSeries<DecimalType> emptySeries(TimeFrame::DAILY);
        
        REQUIRE_THROWS_AS(emptySeries.getFirstDate(), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getLastDate(), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getFirstDateTime(), TimeSeriesDataNotFoundException);
        REQUIRE_THROWS_AS(emptySeries.getLastDateTime(), TimeSeriesDataNotFoundException);
    }
    
    SECTION("Comparison operators - equality")
    {
        NumericTimeSeries<DecimalType> series1(TimeFrame::DAILY);
        NumericTimeSeries<DecimalType> series2(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        auto entry1 = NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY);
        auto entry2 = NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY);
        
        series1.addEntry(entry1);
        series1.addEntry(entry2);
        
        series2.addEntry(entry1);
        series2.addEntry(entry2);
        
        REQUIRE(series1 == series2);
        REQUIRE_FALSE(series1 != series2);
    }
    
    SECTION("Comparison operators - inequality by different entries")
    {
        NumericTimeSeries<DecimalType> series1(TimeFrame::DAILY);
        NumericTimeSeries<DecimalType> series2(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        series1.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        
        series2.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        
        REQUIRE(series1 != series2);
        REQUIRE_FALSE(series1 == series2);
    }
    
    SECTION("Comparison operators - inequality by timeframe")
    {
        NumericTimeSeries<DecimalType> series1(TimeFrame::DAILY);
        NumericTimeSeries<DecimalType> series2(TimeFrame::WEEKLY);
        
        REQUIRE(series1 != series2);
        REQUIRE_FALSE(series1 == series2);
    }
    
    SECTION("Comparison operators - empty series equality")
    {
        NumericTimeSeries<DecimalType> series1(TimeFrame::DAILY);
        NumericTimeSeries<DecimalType> series2(TimeFrame::DAILY);
        
        REQUIRE(series1 == series2);
    }
    
    SECTION("deleteEntryByDate - ptime overload")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::INTRADAY);
        
        ptime pt1(date(2021, Apr, 5), hours(9));
        ptime pt2(date(2021, Apr, 5), hours(10));
        ptime pt3(date(2021, Apr, 5), hours(11));
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt2, DecimalType("101.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt3, DecimalType("102.0"), TimeFrame::INTRADAY));
        
        REQUIRE(series.getNumEntries() == 3);
        
        // Delete middle entry
        series.deleteEntryByDate(pt2);
        
        REQUIRE(series.getNumEntries() == 2);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(pt2), TimeSeriesDataNotFoundException);
        REQUIRE_NOTHROW(series.getTimeSeriesEntry(pt1));
        REQUIRE_NOTHROW(series.getTimeSeriesEntry(pt3));
    }
    
    SECTION("deleteEntryByDate - date overload")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        
        REQUIRE(series.getNumEntries() == 3);
        
        // Delete first entry
        series.deleteEntryByDate(d1);
        
        REQUIRE(series.getNumEntries() == 2);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1), TimeSeriesDataNotFoundException);
        REQUIRE(series.getFirstDate() == d2);
    }
    
    SECTION("Duplicate timestamp handling")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        ptime pt1(d1, getDefaultBarTime());
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::DAILY));
        
        // Attempt to add duplicate
        REQUIRE_THROWS_AS(
            series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("101.0"), TimeFrame::DAILY)),
            TimeSeriesException);
    }
    
    SECTION("Single entry series operations")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        ptime pt1(d1, getDefaultBarTime());
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::DAILY));
        
        REQUIRE(series.getNumEntries() == 1);
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d1);
        REQUIRE(series.getFirstDateTime() == pt1);
        REQUIRE(series.getLastDateTime() == pt1);
        
        auto entry = series.getTimeSeriesEntry(d1);
        REQUIRE(entry.getValue() == DecimalType("100.0"));
        
        // Offset 0 should work
        auto entryOffset0 = series.getTimeSeriesEntry(d1, 0);
        REQUIRE(entryOffset0.getValue() == DecimalType("100.0"));
        
        // Any non-zero offset should throw
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, 1), TimeSeriesOffsetOutOfRangeException);
        REQUIRE_THROWS_AS(series.getTimeSeriesEntry(d1, -1), TimeSeriesOffsetOutOfRangeException);
    }
    
    SECTION("getTimeSeriesAsVector")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        
        auto values = series.getTimeSeriesAsVector();
        
        REQUIRE(values.size() == 3);
        REQUIRE(values[0] == DecimalType("100.0"));
        REQUIRE(values[1] == DecimalType("101.0"));
        REQUIRE(values[2] == DecimalType("102.0"));
    }
    
    SECTION("getEntriesCopy")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        
        auto entries = series.getEntriesCopy();
        
        REQUIRE(entries.size() == 2);
        REQUIRE(entries[0].getValue() == DecimalType("100.0"));
        REQUIRE(entries[0].getDate() == d1);
        REQUIRE(entries[1].getValue() == DecimalType("101.0"));
        REQUIRE(entries[1].getDate() == d2);
    }
}

TEST_CASE("NumericTimeSeries with NumericLogNLookupPolicy", "[NumericTimeSeries][LogNPolicy]")
{
    using LogNNumericSeries = NumericTimeSeries<DecimalType, NumericLogNLookupPolicy<DecimalType>>;
    
    SECTION("Basic operations with LogN policy")
    {
        LogNNumericSeries series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        
        auto entry1 = series.getTimeSeriesEntry(d1);
        REQUIRE(entry1.getValue() == DecimalType("100.0"));
        
        auto entry2 = series.getTimeSeriesEntry(d2);
        REQUIRE(entry2.getValue() == DecimalType("101.0"));
        
        auto entry3 = series.getTimeSeriesEntry(d3);
        REQUIRE(entry3.getValue() == DecimalType("102.0"));
    }
    
    SECTION("Sorted insertion maintains order")
    {
        LogNNumericSeries series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        
        // Add out of order
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        
        // Verify sorted order
        REQUIRE(series.getFirstDate() == d1);
        REQUIRE(series.getLastDate() == d3);
        
        auto it = series.beginSortedAccess();
        REQUIRE(it->getValue() == DecimalType("100.0"));
        ++it;
        REQUIRE(it->getValue() == DecimalType("101.0"));
        ++it;
        REQUIRE(it->getValue() == DecimalType("102.0"));
    }
}

TEST_CASE("NumericTimeSeries negative offset comprehensive", "[NumericTimeSeries][NegativeOffset]")
{
    SECTION("Negative offsets with intraday data")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::INTRADAY);
        
        ptime pt1(date(2021, Apr, 5), hours(9));
        ptime pt2(date(2021, Apr, 5), hours(10));
        ptime pt3(date(2021, Apr, 5), hours(11));
        ptime pt4(date(2021, Apr, 5), hours(12));
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt1, DecimalType("100.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt2, DecimalType("101.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt3, DecimalType("102.0"), TimeFrame::INTRADAY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(pt4, DecimalType("103.0"), TimeFrame::INTRADAY));
        
        // From first entry, look forward
        auto entry1 = series.getTimeSeriesEntry(pt1, -1);
        REQUIRE(entry1.getValue() == DecimalType("101.0"));
        REQUIRE(entry1.getDateTime() == pt2);
        
        auto entry2 = series.getTimeSeriesEntry(pt1, -2);
        REQUIRE(entry2.getValue() == DecimalType("102.0"));
        REQUIRE(entry2.getDateTime() == pt3);
        
        auto entry3 = series.getTimeSeriesEntry(pt1, -3);
        REQUIRE(entry3.getValue() == DecimalType("103.0"));
        REQUIRE(entry3.getDateTime() == pt4);
        
        // From middle entry, look forward
        auto entry4 = series.getTimeSeriesEntry(pt2, -1);
        REQUIRE(entry4.getValue() == DecimalType("102.0"));
        
        auto entry5 = series.getTimeSeriesEntry(pt2, -2);
        REQUIRE(entry5.getValue() == DecimalType("103.0"));
    }
    
    SECTION("Mixed positive and negative offsets")
    {
        NumericTimeSeries<DecimalType> series(TimeFrame::DAILY);
        
        date d1(2021, Apr, 5);
        date d2(2021, Apr, 6);
        date d3(2021, Apr, 7);
        date d4(2021, Apr, 8);
        date d5(2021, Apr, 9);
        
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d1, getDefaultBarTime()), DecimalType("100.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d2, getDefaultBarTime()), DecimalType("101.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d3, getDefaultBarTime()), DecimalType("102.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d4, getDefaultBarTime()), DecimalType("103.0"), TimeFrame::DAILY));
        series.addEntry(NumericTimeSeriesEntry<DecimalType>(
            ptime(d5, getDefaultBarTime()), DecimalType("104.0"), TimeFrame::DAILY));
        
        // From middle, go backward and forward
        REQUIRE(series.getValue(d3, 0) == DecimalType("102.0"));  // Middle entry
        REQUIRE(series.getValue(d3, 1) == DecimalType("101.0"));  // One back
        REQUIRE(series.getValue(d3, 2) == DecimalType("100.0"));  // Two back
        REQUIRE(series.getValue(d3, -1) == DecimalType("103.0")); // One forward
        REQUIRE(series.getValue(d3, -2) == DecimalType("104.0")); // Two forward
    }
}
