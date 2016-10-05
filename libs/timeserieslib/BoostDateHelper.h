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

  inline bool is_first_of_week (const boost::gregorian::date& aDate)
  {
    return (aDate.day_of_week() == boost::date_time::Sunday);
  }

  // We assume that the first day of the week is a Sunday

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

  inline TimeSeriesDate boost_next_week (const boost::gregorian::date& aDate)
  {
    return aDate + boost::gregorian::weeks(1);
  }

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
