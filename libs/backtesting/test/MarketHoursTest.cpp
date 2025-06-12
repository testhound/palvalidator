#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "MarketHours.h"

using namespace mkc_timeseries;
using namespace boost::posix_time;
using namespace boost::gregorian;

TEST_CASE("USEquitiesMarketHours functionality", "[MarketHours]")
{
    USEquitiesMarketHours marketHours;
    
    SECTION("Market hours validation - weekdays")
    {
        // Test Monday through Friday during market hours
        
        // Monday 9:30 AM ET - market open
        ptime mondayOpen(date(2023, 1, 2), time_duration(9, 30, 0));
        REQUIRE(marketHours.isMarketOpen(mondayOpen) == true);
        
        // Tuesday 10:00 AM ET - during market hours
        ptime tuesdayMid(date(2023, 1, 3), time_duration(10, 0, 0));
        REQUIRE(marketHours.isMarketOpen(tuesdayMid) == true);
        
        // Wednesday 12:00 PM ET - during market hours
        ptime wednesdayNoon(date(2023, 1, 4), time_duration(12, 0, 0));
        REQUIRE(marketHours.isMarketOpen(wednesdayNoon) == true);
        
        // Thursday 3:59 PM ET - just before close
        ptime thursdayBeforeClose(date(2023, 1, 5), time_duration(15, 59, 59));
        REQUIRE(marketHours.isMarketOpen(thursdayBeforeClose) == true);
        
        // Friday 4:00 PM ET - market close (should be false)
        ptime fridayClose(date(2023, 1, 6), time_duration(16, 0, 0));
        REQUIRE(marketHours.isMarketOpen(fridayClose) == false);
    }
    
    SECTION("Market hours validation - before market open")
    {
        // Monday 9:29 AM ET - just before open
        ptime mondayBeforeOpen(date(2023, 1, 2), time_duration(9, 29, 59));
        REQUIRE(marketHours.isMarketOpen(mondayBeforeOpen) == false);
        
        // Tuesday 6:00 AM ET - early morning
        ptime tuesdayEarly(date(2023, 1, 3), time_duration(6, 0, 0));
        REQUIRE(marketHours.isMarketOpen(tuesdayEarly) == false);
        
        // Wednesday midnight
        ptime wednesdayMidnight(date(2023, 1, 4), time_duration(0, 0, 0));
        REQUIRE(marketHours.isMarketOpen(wednesdayMidnight) == false);
    }
    
    SECTION("Market hours validation - after market close")
    {
        // Monday 4:00 PM ET - market close
        ptime mondayClose(date(2023, 1, 2), time_duration(16, 0, 0));
        REQUIRE(marketHours.isMarketOpen(mondayClose) == false);
        
        // Tuesday 5:00 PM ET - after hours
        ptime tuesdayAfterHours(date(2023, 1, 3), time_duration(17, 0, 0));
        REQUIRE(marketHours.isMarketOpen(tuesdayAfterHours) == false);
        
        // Wednesday 11:59 PM ET - late evening
        ptime wednesdayLate(date(2023, 1, 4), time_duration(23, 59, 59));
        REQUIRE(marketHours.isMarketOpen(wednesdayLate) == false);
    }
    
    SECTION("Weekend exclusion")
    {
        // Saturday during normal market hours
        ptime saturdayMorning(date(2023, 1, 7), time_duration(10, 0, 0));
        REQUIRE(marketHours.isMarketOpen(saturdayMorning) == false);
        
        ptime saturdayAfternoon(date(2023, 1, 7), time_duration(14, 0, 0));
        REQUIRE(marketHours.isMarketOpen(saturdayAfternoon) == false);
        
        // Sunday during normal market hours
        ptime sundayMorning(date(2023, 1, 8), time_duration(10, 0, 0));
        REQUIRE(marketHours.isMarketOpen(sundayMorning) == false);
        
        ptime sundayAfternoon(date(2023, 1, 8), time_duration(14, 0, 0));
        REQUIRE(marketHours.isMarketOpen(sundayAfternoon) == false);
        
        // Weekend at exact market open/close times
        ptime saturdayOpen(date(2023, 1, 7), time_duration(9, 30, 0));
        REQUIRE(marketHours.isMarketOpen(saturdayOpen) == false);
        
        ptime sundayClose(date(2023, 1, 8), time_duration(16, 0, 0));
        REQUIRE(marketHours.isMarketOpen(sundayClose) == false);
    }
    
    SECTION("Market boundary times - exact open and close")
    {
        // Exact market open time
        ptime exactOpen(date(2023, 1, 3), time_duration(9, 30, 0));
        REQUIRE(marketHours.isMarketOpen(exactOpen) == true);
        
        // One second before market open
        ptime beforeOpen(date(2023, 1, 3), time_duration(9, 29, 59));
        REQUIRE(marketHours.isMarketOpen(beforeOpen) == false);
        
        // Exact market close time (should be false - market is closed at 4:00 PM)
        ptime exactClose(date(2023, 1, 3), time_duration(16, 0, 0));
        REQUIRE(marketHours.isMarketOpen(exactClose) == false);
        
        // One second before market close
        ptime beforeClose(date(2023, 1, 3), time_duration(15, 59, 59));
        REQUIRE(marketHours.isMarketOpen(beforeClose) == true);
    }
}

TEST_CASE("USEquitiesMarketHours getNextTradingTime functionality", "[MarketHours]")
{
    USEquitiesMarketHours marketHours;
    
    SECTION("Next trading time during market hours")
    {
        // Start at 10:00 AM on Tuesday, add 30 minutes
        ptime start(date(2023, 1, 3), time_duration(10, 0, 0));
        time_duration interval = minutes(30);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        ptime expected(date(2023, 1, 3), time_duration(10, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Next trading time crossing market close")
    {
        // Start at 3:45 PM on Tuesday, add 30 minutes (would go to 4:15 PM)
        ptime start(date(2023, 1, 3), time_duration(15, 45, 0));
        time_duration interval = minutes(30);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to next day's market open (Wednesday 9:30 AM)
        ptime expected(date(2023, 1, 4), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Next trading time from before market open")
    {
        // Start at 8:00 AM on Tuesday, add 1 hour (would go to 9:00 AM)
        ptime start(date(2023, 1, 3), time_duration(8, 0, 0));
        time_duration interval = hours(1);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to market open (Tuesday 9:30 AM)
        ptime expected(date(2023, 1, 3), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Next trading time from after market close")
    {
        // Start at 5:00 PM on Tuesday, add 1 hour
        ptime start(date(2023, 1, 3), time_duration(17, 0, 0));
        time_duration interval = hours(1);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to next day's market open (Wednesday 9:30 AM)
        ptime expected(date(2023, 1, 4), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Next trading time crossing weekend - Friday to Monday")
    {
        // Start at 3:45 PM on Friday, add 30 minutes
        ptime start(date(2023, 1, 6), time_duration(15, 45, 0));
        time_duration interval = minutes(30);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to Monday's market open
        ptime expected(date(2023, 1, 9), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Next trading time from Saturday")
    {
        // Start at 10:00 AM on Saturday, add 1 hour
        ptime start(date(2023, 1, 7), time_duration(10, 0, 0));
        time_duration interval = hours(1);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to Monday's market open
        ptime expected(date(2023, 1, 9), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Next trading time from Sunday")
    {
        // Start at 2:00 PM on Sunday, add 2 hours
        ptime start(date(2023, 1, 8), time_duration(14, 0, 0));
        time_duration interval = hours(2);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to Monday's market open
        ptime expected(date(2023, 1, 9), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Multiple small intervals during market hours")
    {
        // Test multiple 5-minute intervals during market hours
        ptime current(date(2023, 1, 3), time_duration(10, 0, 0));
        time_duration interval = minutes(5);
        
        for (int i = 0; i < 10; ++i) {
            ptime next = marketHours.getNextTradingTime(current, interval);
            ptime expected = current + interval;
            
            REQUIRE(next == expected);
            REQUIRE(marketHours.isMarketOpen(next) == true);
            
            current = next;
        }
        
        // Final time should be 10:50 AM
        ptime expectedFinal(date(2023, 1, 3), time_duration(10, 50, 0));
        REQUIRE(current == expectedFinal);
    }
    
    SECTION("Large interval spanning multiple days")
    {
        // Start at 10:00 AM on Monday, add 25 hours
        ptime start(date(2023, 1, 2), time_duration(10, 0, 0));
        time_duration interval = hours(25);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // 25 hours from Monday 10:00 AM would be Tuesday 11:00 AM
        ptime expected(date(2023, 1, 3), time_duration(11, 0, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Zero interval")
    {
        // Test with zero interval during market hours
        ptime start(date(2023, 1, 3), time_duration(11, 0, 0));
        time_duration interval = minutes(0);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        
        REQUIRE(next == start);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
    
    SECTION("Zero interval outside market hours")
    {
        // Test with zero interval outside market hours
        ptime start(date(2023, 1, 3), time_duration(8, 0, 0)); // Before market open
        time_duration interval = minutes(0);
        
        ptime next = marketHours.getNextTradingTime(start, interval);
        // Should jump to market open
        ptime expected(date(2023, 1, 3), time_duration(9, 30, 0));
        
        REQUIRE(next == expected);
        REQUIRE(marketHours.isMarketOpen(next) == true);
    }
}

TEST_CASE("USEquitiesMarketHours edge cases and integration", "[MarketHours]")
{
    USEquitiesMarketHours marketHours;
    
    SECTION("Market hours integration with different time zones")
    {
        // Note: These tests assume the input times are already in ET
        // In a real implementation, time zone conversion would be handled elsewhere
        
        // Test various times throughout the day
        std::vector<std::pair<ptime, bool>> testCases = {
            {ptime(date(2023, 1, 3), time_duration(0, 0, 0)), false},    // Midnight
            {ptime(date(2023, 1, 3), time_duration(6, 0, 0)), false},    // 6 AM
            {ptime(date(2023, 1, 3), time_duration(9, 29, 59)), false},  // Just before open
            {ptime(date(2023, 1, 3), time_duration(9, 30, 0)), true},    // Market open
            {ptime(date(2023, 1, 3), time_duration(12, 0, 0)), true},    // Noon
            {ptime(date(2023, 1, 3), time_duration(15, 59, 59)), true},  // Just before close
            {ptime(date(2023, 1, 3), time_duration(16, 0, 0)), false},   // Market close
            {ptime(date(2023, 1, 3), time_duration(20, 0, 0)), false},   // 8 PM
            {ptime(date(2023, 1, 3), time_duration(23, 59, 59)), false}  // End of day
        };
        
        for (const auto& testCase : testCases) {
            REQUIRE(marketHours.isMarketOpen(testCase.first) == testCase.second);
        }
    }
    
    SECTION("Trading time generation across market boundaries")
    {
        // Test sequence of trading times across market close
        ptime start(date(2023, 1, 3), time_duration(15, 50, 0)); // 3:50 PM Tuesday
        time_duration interval = minutes(15);
        
        // First interval: 3:50 PM + 15 min = 4:05 PM (after close)
        ptime next1 = marketHours.getNextTradingTime(start, interval);
        ptime expected1(date(2023, 1, 4), time_duration(9, 30, 0)); // Wednesday open
        REQUIRE(next1 == expected1);
        
        // Second interval: Wednesday 9:30 AM + 15 min = 9:45 AM
        ptime next2 = marketHours.getNextTradingTime(next1, interval);
        ptime expected2(date(2023, 1, 4), time_duration(9, 45, 0));
        REQUIRE(next2 == expected2);
        
        // Third interval: Wednesday 9:45 AM + 15 min = 10:00 AM
        ptime next3 = marketHours.getNextTradingTime(next2, interval);
        ptime expected3(date(2023, 1, 4), time_duration(10, 0, 0));
        REQUIRE(next3 == expected3);
    }
    
    SECTION("Weekend handling with various intervals")
    {
        // Test different intervals starting from Friday afternoon
        ptime fridayAfternoon(date(2023, 1, 6), time_duration(15, 0, 0));
        
        // 30 minutes - should stay within Friday
        ptime next30min = marketHours.getNextTradingTime(fridayAfternoon, minutes(30));
        ptime expected30min(date(2023, 1, 6), time_duration(15, 30, 0));
        REQUIRE(next30min == expected30min);
        
        // 2 hours - should jump to Monday
        ptime next2hours = marketHours.getNextTradingTime(fridayAfternoon, hours(2));
        ptime expected2hours(date(2023, 1, 9), time_duration(9, 30, 0));
        REQUIRE(next2hours == expected2hours);
        
        // 1 minute from Friday close - should jump to Monday
        ptime fridayClose(date(2023, 1, 6), time_duration(16, 0, 0));
        ptime nextFromClose = marketHours.getNextTradingTime(fridayClose, minutes(1));
        ptime expectedFromClose(date(2023, 1, 9), time_duration(9, 30, 0));
        REQUIRE(nextFromClose == expectedFromClose);
    }
    
    SECTION("Consistency validation")
    {
        // Verify that getNextTradingTime always returns a time when market is open
        std::vector<ptime> testTimes = {
            ptime(date(2023, 1, 1), time_duration(0, 0, 0)),    // Sunday midnight
            ptime(date(2023, 1, 2), time_duration(8, 0, 0)),    // Monday morning
            ptime(date(2023, 1, 3), time_duration(17, 0, 0)),   // Tuesday evening
            ptime(date(2023, 1, 6), time_duration(16, 30, 0)),  // Friday after close
            ptime(date(2023, 1, 7), time_duration(12, 0, 0)),   // Saturday noon
            ptime(date(2023, 1, 8), time_duration(20, 0, 0))    // Sunday evening
        };
        
        std::vector<time_duration> intervals = {
            minutes(1), minutes(15), minutes(30), hours(1), hours(6), hours(24)
        };
        
        for (const auto& startTime : testTimes) {
            for (const auto& interval : intervals) {
                ptime nextTime = marketHours.getNextTradingTime(startTime, interval);
                
                // The returned time must always be during market hours
                REQUIRE(marketHours.isMarketOpen(nextTime) == true);
                
                // The returned time must be >= startTime + interval (unless market closed)
                if (marketHours.isMarketOpen(startTime + interval)) {
                    REQUIRE(nextTime == startTime + interval);
                } else {
                    REQUIRE(nextTime >= startTime + interval);
                }
            }
        }
    }
    
    SECTION("Performance with rapid successive calls")
    {
        // Test that the class can handle many successive calls efficiently
        ptime current(date(2023, 1, 3), time_duration(9, 30, 0));
        time_duration interval = minutes(1);
        
        // Simulate a full trading day of 1-minute intervals (6.5 hours = 390 minutes)
        // But stop before market close to avoid jumping to next day
        int tradingMinutes = (6 * 60) + 29; // 6 hours 29 minutes = 389 minutes (stops at 15:59)
        
        for (int i = 0; i < tradingMinutes; ++i) {
            ptime next = marketHours.getNextTradingTime(current, interval);
            
            // Should always be market hours
            REQUIRE(marketHours.isMarketOpen(next) == true);
            
            // Should be exactly 1 minute later during market hours
            REQUIRE(next == current + interval);
            
            current = next;
        }
        
        // Final time should be just before market close (15:59)
        ptime expectedFinal(date(2023, 1, 3), time_duration(15, 59, 0));
        REQUIRE(current == expectedFinal);
        
        // Test one more interval that would cross market close
        ptime nextAfterClose = marketHours.getNextTradingTime(current, interval);
        // Should jump to next day's market open
        ptime expectedNextDay(date(2023, 1, 4), time_duration(9, 30, 0));
        REQUIRE(nextAfterClose == expectedNextDay);
        REQUIRE(marketHours.isMarketOpen(nextAfterClose) == true);
    }
}