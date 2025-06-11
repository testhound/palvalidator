#include <catch2/catch_test_macros.hpp>
#include "BackTester.h"
#include "TestUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

TEST_CASE("BackTesterFactory creates DailyBackTester correctly", "[BackTesterFactory][Daily]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    DateRange dateRange(startDate, endDate);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::DAILY, dateRange);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isDailyBackTester() == true);
    REQUIRE(backTester->isWeeklyBackTester() == false);
    REQUIRE(backTester->isMonthlyBackTester() == false);
    REQUIRE(backTester->isIntradayBackTester() == false);
    REQUIRE(backTester->getStartDate() == startDate);
    REQUIRE(backTester->getEndDate() == endDate);
    REQUIRE(backTester->numBackTestRanges() == 1);
}

TEST_CASE("BackTesterFactory creates WeeklyBackTester correctly", "[BackTesterFactory][Weekly]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    DateRange dateRange(startDate, endDate);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::WEEKLY, dateRange);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isDailyBackTester() == false);
    REQUIRE(backTester->isWeeklyBackTester() == true);
    REQUIRE(backTester->isMonthlyBackTester() == false);
    REQUIRE(backTester->isIntradayBackTester() == false);
    REQUIRE(backTester->numBackTestRanges() == 1);
}

TEST_CASE("BackTesterFactory creates MonthlyBackTester correctly", "[BackTesterFactory][Monthly]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    DateRange dateRange(startDate, endDate);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::MONTHLY, dateRange);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isDailyBackTester() == false);
    REQUIRE(backTester->isWeeklyBackTester() == false);
    REQUIRE(backTester->isMonthlyBackTester() == true);
    REQUIRE(backTester->isIntradayBackTester() == false);
    REQUIRE(backTester->numBackTestRanges() == 1);
}

TEST_CASE("BackTesterFactory creates IntradayBackTester correctly with DateRange", "[BackTesterFactory][INTRADAY]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(9, 30, 0));
    ptime endDateTime = ptime(date(2021, 4, 15), time_duration(16, 0, 0));
    DateRange dateRange(startDateTime, endDateTime);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::INTRADAY, dateRange);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isDailyBackTester() == false);
    REQUIRE(backTester->isWeeklyBackTester() == false);
    REQUIRE(backTester->isMonthlyBackTester() == false);
    REQUIRE(backTester->isIntradayBackTester() == true);
    REQUIRE(backTester->numBackTestRanges() == 1);
}

TEST_CASE("BackTesterFactory getBackTester works with INTRADAY using ptime", "[BackTesterFactory][IntradayBackTester][ptime]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(9, 30, 0));
    ptime endDateTime = ptime(date(2021, 4, 15), time_duration(16, 0, 0));
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(
        TimeFrame::INTRADAY, startDateTime, endDateTime);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isDailyBackTester() == false);
    REQUIRE(backTester->isWeeklyBackTester() == false);
    REQUIRE(backTester->isMonthlyBackTester() == false);
    REQUIRE(backTester->isIntradayBackTester() == true);
    REQUIRE(backTester->numBackTestRanges() == 1);
}

TEST_CASE("BackTesterFactory getBackTester works with INTRADAY single day", "[BackTesterFactory][IntradayBackTester][SingleDay]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(9, 30, 0));
    ptime endDateTime = ptime(date(2021, 4, 15), time_duration(16, 0, 0));
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(
        TimeFrame::INTRADAY, startDateTime, endDateTime);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isIntradayBackTester() == true);
}

TEST_CASE("BackTesterFactory getBackTester works with INTRADAY full day", "[BackTesterFactory][IntradayBackTester][FullDay]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(0, 0, 0));
    ptime endDateTime = ptime(date(2021, 4, 15), time_duration(23, 59, 59));
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(
        TimeFrame::INTRADAY, startDateTime, endDateTime);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isIntradayBackTester() == true);
}

TEST_CASE("BackTesterFactory getBackTester works with INTRADAY multi-day range", "[BackTesterFactory][IntradayBackTester][MultiDay]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(9, 30, 0));
    ptime endDateTime = ptime(date(2021, 4, 16), time_duration(16, 0, 0));
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(
        TimeFrame::INTRADAY, startDateTime, endDateTime);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isIntradayBackTester() == true);
}

TEST_CASE("BackTesterFactory date-based constructor works for DAILY", "[BackTesterFactory][DateConstructor][Daily]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::DAILY, startDate, endDate);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isDailyBackTester() == true);
    REQUIRE(backTester->getStartDate() == startDate);
    REQUIRE(backTester->getEndDate() == endDate);
}

TEST_CASE("BackTesterFactory date-based constructor throws for INTRADAY", "[BackTesterFactory][DateConstructor][INTRADAY][Exception]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    
    REQUIRE_THROWS_AS(
        BackTesterFactory<DecimalType>::getBackTester(TimeFrame::INTRADAY, startDate, endDate),
        BackTesterException
    );
}

TEST_CASE("BackTesterFactory ptime-based constructor throws for non-INTRADAY", "[BackTesterFactory][PtimeConstructor][NonINTRADAY][Exception]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(9, 30, 0));
    ptime endDateTime = ptime(date(2021, 4, 15), time_duration(16, 0, 0));
    
    REQUIRE_THROWS_AS(
        BackTesterFactory<DecimalType>::getBackTester(TimeFrame::DAILY, startDateTime, endDateTime),
        BackTesterException
    );
    
    REQUIRE_THROWS_AS(
        BackTesterFactory<DecimalType>::getBackTester(TimeFrame::WEEKLY, startDateTime, endDateTime),
        BackTesterException
    );
    
    REQUIRE_THROWS_AS(
        BackTesterFactory<DecimalType>::getBackTester(TimeFrame::MONTHLY, startDateTime, endDateTime),
        BackTesterException
    );
}

TEST_CASE("BackTesterFactory throws for unsupported timeframe", "[BackTesterFactory][UnsupportedTimeFrame]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    DateRange dateRange(startDate, endDate);
    
    // Assuming there's an invalid timeframe value we can test with
    // This test verifies the "else" clause in the factory method
    REQUIRE_THROWS_AS(
        BackTesterFactory<DecimalType>::getBackTester(static_cast<TimeFrame::Duration>(999), dateRange),
        BackTesterException
    );
}

TEST_CASE("BackTesterFactory clone functionality works", "[BackTesterFactory][Clone]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    DateRange dateRange(startDate, endDate);
    
    auto originalBackTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::DAILY, dateRange);
    auto clonedBackTester = originalBackTester->clone();
    
    REQUIRE(clonedBackTester != nullptr);
    REQUIRE(clonedBackTester->isDailyBackTester() == originalBackTester->isDailyBackTester());
    REQUIRE(clonedBackTester->getStartDate() == originalBackTester->getStartDate());
    REQUIRE(clonedBackTester->getEndDate() == originalBackTester->getEndDate());
    REQUIRE(clonedBackTester->numBackTestRanges() == originalBackTester->numBackTestRanges());
    
    // Verify they are different instances
    REQUIRE(clonedBackTester.get() != originalBackTester.get());
}

TEST_CASE("BackTesterFactory edge case - same start and end date", "[BackTesterFactory][EdgeCase][SameDate]")
{
    date sameDate = date(2021, 4, 15);
    DateRange dateRange(sameDate, sameDate);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::DAILY, dateRange);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->getStartDate() == sameDate);
    REQUIRE(backTester->getEndDate() == sameDate);
}

TEST_CASE("BackTesterFactory edge case - very short intraday interval", "[BackTesterFactory][EdgeCase][ShortInterval]")
{
    ptime startDateTime = ptime(date(2021, 4, 15), time_duration(9, 30, 0));
    ptime endDateTime = ptime(date(2021, 4, 15), time_duration(9, 31, 0));  // 1 minute total
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(
        TimeFrame::INTRADAY, startDateTime, endDateTime);
    
    REQUIRE(backTester != nullptr);
    REQUIRE(backTester->isIntradayBackTester() == true);
}

TEST_CASE("BackTesterFactory getNumClosedTrades static method works", "[BackTesterFactory][StaticMethods]")
{
    date startDate = date(2021, 4, 15);
    date endDate = date(2021, 4, 20);
    DateRange dateRange(startDate, endDate);
    
    auto backTester = BackTesterFactory<DecimalType>::getBackTester(TimeFrame::DAILY, dateRange);
    
    // This should not throw, even with no strategies added
    // The method should handle empty strategy lists gracefully
    REQUIRE_NOTHROW(BackTesterFactory<DecimalType>::getNumClosedTrades(backTester));
}