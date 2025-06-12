// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#include <catch2/catch_test_macros.hpp>
#include "TimeFrameDiscovery.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "TimeFrame.h"
#include "TradingVolume.h"
#include "TestUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace mkc_timeseries;
using namespace boost::posix_time;
using namespace boost::gregorian;

TEST_CASE("TimeFrameDiscovery operations", "[TimeFrameDiscovery]")
{
    SECTION("5-minute bars discovery")
    {
        // Test with 5-minute bars similar to the example data
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Add 5-minute bar entries (09:35, 09:40, 09:45, etc.)
        // These represent bars at 9:35 AM, 9:40 AM, 9:45 AM, etc.
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:35:00", "65.10", "65.82", "64.86", "65.68", "2644523"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:40:00", "65.68", "66.09", "65.62", "65.76", "1635393"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:45:00", "65.75", "65.92", "65.33", "65.64", "1136110"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:50:00", "65.65", "65.68", "65.03", "65.22", "1100238"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:55:00", "65.21", "65.66", "65.09", "65.35", "1409552"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "10:00:00", "65.34", "65.60", "65.25", "65.47", "823288"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        REQUIRE(discovery.isDiscovered());
        REQUIRE(discovery.numTimeFrames() == 6);
        
        // Check that the time-of-day patterns are correctly identified
        // Note: time_duration here represents time-of-day, not duration
        auto timeFrames = discovery.getTimeFrames();
        REQUIRE(timeFrames[0].hours() == 9);
        REQUIRE(timeFrames[0].minutes() == 35);
        REQUIRE(timeFrames[1].hours() == 9);
        REQUIRE(timeFrames[1].minutes() == 40);
        REQUIRE(timeFrames[2].hours() == 9);
        REQUIRE(timeFrames[2].minutes() == 45);
        REQUIRE(timeFrames[3].hours() == 9);
        REQUIRE(timeFrames[3].minutes() == 50);
        REQUIRE(timeFrames[4].hours() == 9);
        REQUIRE(timeFrames[4].minutes() == 55);
        REQUIRE(timeFrames[5].hours() == 10);
        REQUIRE(timeFrames[5].minutes() == 0);
        
        // Test common interval calculation - this should be 5 minutes duration
        auto commonInterval = discovery.getCommonInterval();
        REQUIRE(commonInterval.total_seconds() == 5 * 60); // 5 minutes in seconds
    }

    SECTION("90-minute bars discovery with incomplete first day")
    {
        // Test with 90-minute bars similar to the example data
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Add 90-minute bar entries for first day (incomplete - starts at 12:00)
        timeSeries->addEntry(*createTimeSeriesEntry("20250425", "12:00:00", "51.77", "52.77", "51.49", "52.52", "9136553"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250425", "13:30:00", "52.53", "54.07", "52.41", "54.02", "9293851"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250425", "15:00:00", "54.01", "54.04", "52.60", "53.62", "10294009"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250425", "16:00:00", "53.62", "53.99", "53.18", "53.87", "5963116"));
        
        // Add 90-minute bar entries for second day (complete - starts at 10:30)
        timeSeries->addEntry(*createTimeSeriesEntry("20250428", "10:30:00", "53.96", "54.64", "53.07", "53.54", "11335531"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250428", "12:00:00", "53.54", "53.94", "52.06", "52.36", "10830408"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250428", "13:30:00", "52.35", "52.44", "51.64", "51.76", "5890848"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250428", "15:00:00", "51.77", "53.13", "51.69", "53.04", "6957659"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        REQUIRE(discovery.isDiscovered());
        REQUIRE(discovery.numTimeFrames() == 5); // Should find all unique time-of-day values
        
        // Check that all time-of-day patterns are discovered (including the missing 10:30 from first day)
        auto timeFrames = discovery.getTimeFrames();
        REQUIRE(timeFrames[0].hours() == 10);
        REQUIRE(timeFrames[0].minutes() == 30);
        REQUIRE(timeFrames[1].hours() == 12);
        REQUIRE(timeFrames[1].minutes() == 0);
        REQUIRE(timeFrames[2].hours() == 13);
        REQUIRE(timeFrames[2].minutes() == 30);
        REQUIRE(timeFrames[3].hours() == 15);
        REQUIRE(timeFrames[3].minutes() == 0);
        REQUIRE(timeFrames[4].hours() == 16);
        REQUIRE(timeFrames[4].minutes() == 0);
        
        // Test common interval calculation - this should be 90 minutes duration
        auto commonInterval = discovery.getCommonInterval();
        REQUIRE(commonInterval.total_seconds() == 90 * 60); // 90 minutes in seconds
    }

    SECTION("60-minute bars discovery")
    {
        // Test with 60-minute bars similar to the example data
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Add 60-minute bar entries for first day (starts at 13:00)
        timeSeries->addEntry(*createTimeSeriesEntry("20210415", "13:00:00", "70.00", "71.51", "66.69", "70.14", "3170892"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210415", "14:00:00", "70.04", "70.36", "65.10", "65.61", "686151"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210415", "15:00:00", "65.51", "67.64", "65.12", "65.28", "789138"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210415", "16:00:00", "65.30", "65.55", "63.00", "65.20", "997590"));
        
        // Add 60-minute bar entries for second day (starts at 10:00)
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "10:00:00", "64.40", "65.20", "63.25", "63.98", "189072"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "11:00:00", "63.80", "64.00", "61.65", "63.80", "330281"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "12:00:00", "63.80", "64.00", "61.18", "61.93", "195508"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "13:00:00", "61.92", "63.79", "61.92", "62.97", "175624"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "14:00:00", "62.97", "63.06", "60.01", "61.00", "137948"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "15:00:00", "61.00", "61.27", "58.65", "60.02", "244901"));
        timeSeries->addEntry(*createTimeSeriesEntry("20210416", "16:00:00", "60.00", "61.35", "59.96", "61.01", "383815"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        REQUIRE(discovery.isDiscovered());
        REQUIRE(discovery.numTimeFrames() == 7); // Should find all unique time-of-day values
        
        // Check that all time-of-day patterns are discovered
        auto timeFrames = discovery.getTimeFrames();
        REQUIRE(timeFrames[0].hours() == 10);
        REQUIRE(timeFrames[0].minutes() == 0);
        REQUIRE(timeFrames[1].hours() == 11);
        REQUIRE(timeFrames[1].minutes() == 0);
        REQUIRE(timeFrames[2].hours() == 12);
        REQUIRE(timeFrames[2].minutes() == 0);
        REQUIRE(timeFrames[3].hours() == 13);
        REQUIRE(timeFrames[3].minutes() == 0);
        REQUIRE(timeFrames[4].hours() == 14);
        REQUIRE(timeFrames[4].minutes() == 0);
        REQUIRE(timeFrames[5].hours() == 15);
        REQUIRE(timeFrames[5].minutes() == 0);
        REQUIRE(timeFrames[6].hours() == 16);
        REQUIRE(timeFrames[6].minutes() == 0);
        
        // Test common interval calculation - this should be 60 minutes duration
        auto commonInterval = discovery.getCommonInterval();
        REQUIRE(commonInterval.total_seconds() == 60 * 60); // 60 minutes in seconds
    }

    SECTION("Timestamp navigation methods")
    {
        // Test the getPreviousTimestamp and getNextTimestamp methods
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Add some entries
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:35:00", "65.10", "65.82", "64.86", "65.68", "2644523"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:40:00", "65.68", "66.09", "65.62", "65.76", "1635393"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:45:00", "65.75", "65.92", "65.33", "65.64", "1136110"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:50:00", "65.65", "65.68", "65.03", "65.22", "1100238"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        // Create ptime objects for the actual timestamps in the data
        date testDate(2025, 5, 23);
        ptime t1(testDate, time_duration(9, 35, 0));  // 9:35 AM
        ptime t2(testDate, time_duration(9, 40, 0));  // 9:40 AM
        ptime t3(testDate, time_duration(9, 45, 0));  // 9:45 AM
        ptime t4(testDate, time_duration(9, 50, 0));  // 9:50 AM
        
        // Test getPreviousTimestamp
        REQUIRE(discovery.getPreviousTimestamp(t2) == t1);
        REQUIRE(discovery.getPreviousTimestamp(t3) == t2);
        REQUIRE(discovery.getPreviousTimestamp(t4) == t3);
        
        // Test getNextTimestamp
        REQUIRE(discovery.getNextTimestamp(t1) == t2);
        REQUIRE(discovery.getNextTimestamp(t2) == t3);
        REQUIRE(discovery.getNextTimestamp(t3) == t4);
        
        // Test boundary conditions
        REQUIRE(discovery.getPreviousTimestamp(t1).is_not_a_date_time());
        REQUIRE(discovery.getNextTimestamp(t4).is_not_a_date_time());
        
        // Test with timestamp not in data
        ptime tMissing(testDate, time_duration(9, 37, 0)); // 9:37 AM - Between t1 and t2
        REQUIRE(discovery.getPreviousTimestamp(tMissing) == t1);
        REQUIRE(discovery.getNextTimestamp(tMissing) == t2);
    }

    SECTION("hasTimestamp method")
    {
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:35:00", "65.10", "65.82", "64.86", "65.68", "2644523"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:40:00", "65.68", "66.09", "65.62", "65.76", "1635393"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        date testDate(2025, 5, 23);
        ptime t1(testDate, time_duration(9, 35, 0));  // 9:35 AM
        ptime t2(testDate, time_duration(9, 40, 0));  // 9:40 AM
        ptime tMissing(testDate, time_duration(9, 37, 0)); // 9:37 AM - not in data
        
        REQUIRE(discovery.hasTimestamp(t1));
        REQUIRE(discovery.hasTimestamp(t2));
        REQUIRE_FALSE(discovery.hasTimestamp(tMissing));
    }

    SECTION("getTimestampsForDate method")
    {
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:35:00", "65.10", "65.82", "64.86", "65.68", "2644523"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:40:00", "65.68", "66.09", "65.62", "65.76", "1635393"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250524", "09:35:00", "65.75", "65.92", "65.33", "65.64", "1136110"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        date testDate1(2025, 5, 23);
        date testDate2(2025, 5, 24);
        date testDate3(2025, 5, 25); // No data for this date
        
        ptime t1(testDate1, time_duration(9, 35, 0));  // 9:35 AM on 5/23
        ptime t2(testDate1, time_duration(9, 40, 0));  // 9:40 AM on 5/23
        ptime t3(testDate2, time_duration(9, 35, 0));  // 9:35 AM on 5/24
        
        auto timestamps1 = discovery.getTimestampsForDate(testDate1);
        REQUIRE(timestamps1.size() == 2);
        REQUIRE(timestamps1[0] == t1);
        REQUIRE(timestamps1[1] == t2);
        
        auto timestamps2 = discovery.getTimestampsForDate(testDate2);
        REQUIRE(timestamps2.size() == 1);
        REQUIRE(timestamps2[0] == t3);
        
        auto timestamps3 = discovery.getTimestampsForDate(testDate3);
        REQUIRE(timestamps3.size() == 0);
    }

    SECTION("Empty time series exception")
    {
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        
        REQUIRE_THROWS_AS(discovery.inferTimeFrames(), TimeFrameDiscoveryException);
    }

    SECTION("Methods before inference exception")
    {
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:35:00", "65.10", "65.82", "64.86", "65.68", "2644523"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        
        date testDate(2025, 5, 23);
        ptime t1(testDate, time_duration(9, 35, 0));  // 9:35 AM
        
        // Methods should throw before inferTimeFrames() is called
        REQUIRE_THROWS_AS(discovery.getPreviousTimestamp(t1), TimeFrameDiscoveryException);
        REQUIRE_THROWS_AS(discovery.getNextTimestamp(t1), TimeFrameDiscoveryException);
        REQUIRE_THROWS_AS(discovery.getCommonInterval(), TimeFrameDiscoveryException);
        REQUIRE_THROWS_AS(discovery.hasTimestamp(t1), TimeFrameDiscoveryException);
        REQUIRE_THROWS_AS(discovery.getTimestampsForDate(testDate), TimeFrameDiscoveryException);
        REQUIRE_THROWS_AS(discovery.getTimeFrame(0), TimeFrameDiscoveryException);
    }

    SECTION("Legacy interface compatibility")
    {
        // Test the legacy interface methods for backward compatibility
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
            TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:35:00", "65.10", "65.82", "64.86", "65.68", "2644523"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:40:00", "65.68", "66.09", "65.62", "65.76", "1635393"));
        timeSeries->addEntry(*createTimeSeriesEntry("20250523", "09:45:00", "65.75", "65.92", "65.33", "65.64", "1136110"));
        
        TimeFrameDiscovery<DecimalType> discovery(timeSeries);
        discovery.inferTimeFrames();
        
        // Test legacy methods
        REQUIRE(discovery.numTimeFrames() == 3);
        
        // Check time-of-day values (stored as time_duration representing time-of-day)
        REQUIRE(discovery.getTimeFrame(0).hours() == 9);
        REQUIRE(discovery.getTimeFrame(0).minutes() == 35);
        REQUIRE(discovery.getTimeFrame(1).hours() == 9);
        REQUIRE(discovery.getTimeFrame(1).minutes() == 40);
        REQUIRE(discovery.getTimeFrame(2).hours() == 9);
        REQUIRE(discovery.getTimeFrame(2).minutes() == 45);
        
        // Test out of bounds
        REQUIRE_THROWS_AS(discovery.getTimeFrame(3), TimeFrameDiscoveryException);
        
        // Test iterators
        auto begin = discovery.getTimeFramesBegin();
        auto end = discovery.getTimeFramesEnd();
        REQUIRE(std::distance(begin, end) == 3);
        
        // Test getTimeFrames
        auto timeFrames = discovery.getTimeFrames();
        REQUIRE(timeFrames.size() == 3);
        REQUIRE(timeFrames[0].hours() == 9);
        REQUIRE(timeFrames[0].minutes() == 35);
        REQUIRE(timeFrames[1].hours() == 9);
        REQUIRE(timeFrames[1].minutes() == 40);
        REQUIRE(timeFrames[2].hours() == 9);
        REQUIRE(timeFrames[2].minutes() == 45);
    }
}