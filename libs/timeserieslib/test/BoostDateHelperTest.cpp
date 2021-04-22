#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include <string>
#include <iostream>
#include "../BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;


TEST_CASE ("BoostDateHelper operations", "[BoostDateHelper]")
{
  date tradeDate1(createDate ("19851118"));
  using boost::gregorian::date_duration;

  date_duration OneDay (1);

  REQUIRE (isWeekday (tradeDate1));
  REQUIRE_FALSE (isWeekend (tradeDate1));

  date aDate = tradeDate1 - OneDay;
  REQUIRE (isWeekend (aDate));
  REQUIRE_FALSE (isWeekday (aDate));

  date previousWeekday = boost_previous_weekday (tradeDate1);
  REQUIRE (isWeekday (previousWeekday));
  REQUIRE_FALSE (isWeekend (previousWeekday));
  REQUIRE (previousWeekday == date (1985, 11, 15));
  REQUIRE (previousWeekday.day_of_week() == boost::date_time::Friday);

  date orderDate1(createDate ("19851115"));
  date executionDate1 = boost_next_weekday (orderDate1);
  REQUIRE (isWeekday (executionDate1));
  REQUIRE_FALSE (isWeekend (executionDate1));
  REQUIRE (executionDate1 == tradeDate1);
  REQUIRE (executionDate1.day_of_week() == boost::date_time::Monday);

  date nextOrderDate(boost_next_weekday (tradeDate1));
  REQUIRE (isWeekday (nextOrderDate));
  REQUIRE_FALSE (isWeekend (nextOrderDate));
  REQUIRE (nextOrderDate ==  date (1985, 11, 19));
  REQUIRE (nextOrderDate.day_of_week() == boost::date_time::Tuesday);

  date aSpecialYear (createDate("19630101"));
  date monthAfter = boost_next_month (aSpecialYear);
  REQUIRE_FALSE (aSpecialYear.month().as_number() == monthAfter.month().as_number());
   REQUIRE (aSpecialYear.month().as_number() < monthAfter.month().as_number());

   date monthBefore = boost_previous_month (aSpecialYear);
   REQUIRE (monthBefore.month().as_number() == 12);
   REQUIRE (monthBefore.year() < aSpecialYear.year());

   date aSpecialDate (createDate("19631218"));

   REQUIRE (aSpecialDate.month().as_number() == 12);
   REQUIRE (aSpecialDate.year() == 1963);
   REQUIRE (aSpecialDate.day().as_number() == 18);
   
   date begOfMonthDate = first_of_month (aSpecialDate);
   REQUIRE_FALSE (aSpecialDate == begOfMonthDate);
   REQUIRE (begOfMonthDate.month().as_number() == 12);
   REQUIRE (begOfMonthDate.year() == 1963);
   REQUIRE (begOfMonthDate.day().as_number() == 1);

   date aSpecialDate2 (createDate("19990801"));
   date begOfMonthDate2 = first_of_month (aSpecialDate2);

   REQUIRE (aSpecialDate2 == begOfMonthDate2);

   REQUIRE_FALSE (is_first_of_month (aSpecialDate));
   REQUIRE (is_first_of_month (begOfMonthDate));
   REQUIRE (is_first_of_month (aSpecialDate2));

   date aWeeklyDate1 (createDate("20160624"));
   date aWeeklyStart = first_of_week (aWeeklyDate1);
   std::cout << "For date  " << aWeeklyDate1 << ", week start: " << aWeeklyStart << std::endl;

   date prevWeek = boost_previous_week (aWeeklyStart);
   REQUIRE (prevWeek.day().as_number() == 12);
   REQUIRE (prevWeek.month().as_number() == 6);
   REQUIRE (prevWeek.year() == 2016);

   date nextWeek = boost_next_week (prevWeek);
   REQUIRE (nextWeek == aWeeklyStart);
}

