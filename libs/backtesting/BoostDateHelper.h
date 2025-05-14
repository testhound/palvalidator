// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __BOOST_DATE_HELPER_H
#define __BOOST_DATE_HELPER_H 1

#include <boost/date_time.hpp>

namespace mkc_timeseries
{
  typedef boost::gregorian::date TimeSeriesDate;
  using boost::gregorian::date_duration;

  inline bool isWeekend (const boost::gregorian::date& aDate)
  {
    return (aDate.day_of_week() == boost::date_time::Saturday ||
	    aDate.day_of_week() == boost::date_time::Sunday);
  }

  inline bool isWeekday (const boost::gregorian::date& aDate)
  {
    return !isWeekend(aDate);
  }


  inline TimeSeriesDate boost_previous_weekday(const boost::gregorian::date& d)
{
    // Sunday==0, Monday==1, …, Saturday==6
    int dow = d.day_of_week().as_number();

    date_duration offset;
    if      (dow == 1)      // Monday → go back 3 days to Friday
        offset = date_duration(3);
    else if (dow == 0)      // Sunday → go back 2 days to Friday
        offset = date_duration(2);
    else                    // any other weekday → go back exactly one day
        offset = date_duration(1);

    return d - offset;
}

  /*
  inline TimeSeriesDate boost_previous_weekday (const boost::gregorian::date& aDate)
  {
    date_duration dur(1);
    TimeSeriesDate temp;

    temp = aDate - dur;
    if (isWeekday (temp))
      return temp;

    while (1)
      {
	temp = temp - dur;
	if (isWeekday (temp))
	  return temp;
      }
  }
  */

  inline TimeSeriesDate boost_next_weekday(const TimeSeriesDate& d)
  {
    int dow = d.day_of_week().as_number();

    date_duration offset;
    if (dow == 5)      // Friday → advance 3 days to Monday
      offset = date_duration(3);
    else if (dow == 6)      // Saturday → advance 2 days to Monday
      offset = date_duration(2);
    else                    // any other weekday → advance exactly one day
      offset = date_duration(1);

    return d + offset;
  }

  /*
  inline TimeSeriesDate boost_next_weekday (const boost::gregorian::date& aDate)
  {
    date_duration dur(1);
    TimeSeriesDate temp;

    temp = aDate + dur;
    if (isWeekday (temp))
      return temp;

    while (1)
      {
	temp = temp + dur;
	if (isWeekday (temp))
	  return temp;
      }
  }
  */
  
  inline TimeSeriesDate boost_next_month (const boost::gregorian::date& aDate)
  {
    return aDate + boost::gregorian::months(1);
  }

  inline TimeSeriesDate boost_previous_month (const boost::gregorian::date& aDate)
  {
    return aDate - boost::gregorian::months(1);
  }

  inline TimeSeriesDate first_of_month (const boost::gregorian::date& aDate)
  {
    if (aDate.day().as_number() != 1)
      {
	return boost::gregorian::date (aDate.year(), aDate.month(),boost::gregorian:: greg_day (1));
      }
    else
      return aDate;
  }

  /**
   * @brief   Test whether a date is the first day of the week (Sunday).
   * @param   aDate   The date to examine.
   * @returns `true` if `aDate.day_of_week() == Sunday`, `false` otherwise.
   * @note    Week is defined Sunday→Saturday.
   */
  inline bool is_first_of_week (const boost::gregorian::date& aDate)
  {
    return (aDate.day_of_week() == boost::date_time::Sunday);
  }

  /**
   * @brief   Find the first day of the week (Sunday) that contains the given date.
   * @param   aDate   Any calendar date.
   * @returns The same date if it’s already Sunday; otherwise the most recent prior Sunday.
   * @note    Runs in O(1) if rewritten with arithmetic, O(weeks) in the current looped form.
   */

  inline TimeSeriesDate first_of_week (const boost::gregorian::date& aDate)
  {
    if (is_first_of_week (aDate))
      return aDate;

    date_duration dur(1);
    TimeSeriesDate temp(aDate);

    while (temp.day_of_week() != boost::date_time::Sunday)
      {
	temp = temp - dur;
      }

    return temp;
  }

  /**
   * @brief   Advance the given date by exactly one calendar week (7 days).
   * @param   aDate   A date (typically aligned to the week boundary).
   * @returns `aDate + weeks(1)`.
   * @pre      If you want to step through week-boundaries, feed it dates returned by `first_of_week`.
   */
  inline TimeSeriesDate boost_next_week (const boost::gregorian::date& aDate)
  {
    return aDate + boost::gregorian::weeks(1);
  }

  /**
   * @brief   Move the given date back by exactly one calendar week (7 days).
   * @param   aDate   A date (typically aligned to the week boundary).
   * @returns `aDate - weeks(1)`.
   * @pre      If you want to step through week-boundaries, feed it dates returned by `first_of_week`.
   */
  inline TimeSeriesDate boost_previous_week (const boost::gregorian::date& aDate)
  {
    return aDate - boost::gregorian::weeks(1);
  }

  inline bool is_first_of_month (const boost::gregorian::date& aDate)
  {
    return (aDate.day().as_number() == 1);
  }
}


#endif
