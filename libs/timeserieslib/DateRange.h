// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __DATE_RANGE_H
#define __DATE_RANGE_H 1

#include <exception>
#include <boost/date_time.hpp>

namespace mkc_timeseries
{
  class DateRangeException : public std::runtime_error
  {
  public:
  DateRangeException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~DateRangeException()
      {}
  };

  class DateRange
  {
  public:
    DateRange(const boost::gregorian::date& firstDate, const boost::gregorian::date& lastDate)
      : mFirstDate(firstDate),
	mLastDate(lastDate)
    {
      if (lastDate < firstDate)
	throw DateRangeException ("DateRange::DateRange - Second date cannot occur before first date");
    }

    DateRange (const DateRange& rhs)
      : mFirstDate(rhs.mFirstDate),
	mLastDate(rhs.mLastDate)
    {}

    DateRange& 
    operator=(const DateRange& rhs)
    {
      if (this == &rhs)
	return *this;

      mFirstDate = rhs.mFirstDate;
      mLastDate = rhs.mLastDate;

      return *this;
    }

    ~DateRange()
    {}

    const boost::gregorian::date& getFirstDate() const
    {
      return mFirstDate;
    }

    const boost::gregorian::date& getLastDate() const
    {
      return mLastDate;
    }

  private:
    boost::gregorian::date mFirstDate;
    boost::gregorian::date mLastDate;
  };
}

#endif

