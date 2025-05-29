// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __DATE_RANGE_H
#define __DATE_RANGE_H 1

#include <exception>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <map>
#include <stdexcept>
#include <string>
#include "TimeSeriesEntry.h"

namespace mkc_timeseries
{
  using std::map;
  using boost::posix_time::ptime;

  class DateRangeException : public std::runtime_error
  {
  public:
  DateRangeException(const std::string& msg) 
    : std::runtime_error(msg)
      {}

    ~DateRangeException()
      {}
  };

  class DateRange
  {
  public:
    DateRange(const boost::gregorian::date& firstDate, const boost::gregorian::date& lastDate)
      : DateRange(ptime(firstDate, getDefaultBarTime()),
		  ptime(lastDate, getDefaultBarTime()))
    {}

    DateRange(const ptime& firstDate, const ptime& lastDate)
      : mFirstDate(firstDate),
	mLastDate(lastDate)
    {
      if (lastDate < firstDate)
	throw DateRangeException ("DateRange::DateRange - Second date cannot occur before first date");
    }

    DateRange(const DateRange&) = default;
    DateRange& operator=(const DateRange&) = default;
    ~DateRange() noexcept = default;

    boost::gregorian::date getFirstDate() const
    {
      return mFirstDate.date();
    }

    const ptime& getFirstDateTime() const
    {
      return mFirstDate;
    }

    boost::gregorian::date getLastDate() const
    {
      return mLastDate.date();
    }

    const ptime& getLastDateTime() const
    {
      return mLastDate;
    }

  private:
    ptime mFirstDate;
    ptime mLastDate;
  };

  inline bool operator==(const DateRange& lhs, const DateRange& rhs)
    {
      return ((lhs.getFirstDateTime() == rhs.getFirstDateTime()) &&
	      (lhs.getLastDateTime() == rhs.getLastDateTime()));
    }

  inline bool operator!=(const DateRange& lhs, const DateRange& rhs)
    {
      return !(lhs == rhs);
    }

  class DateRangeContainer
  {
    using Map = map<ptime, DateRange>;
  public:
    typedef typename Map::const_iterator ConstDateRangeIterator;

    DateRangeContainer()
      : mDateRangeMap()
    {}

    DateRangeContainer(const DateRangeContainer&) = default;
    DateRangeContainer& operator=(const DateRangeContainer&) = default;
    ~DateRangeContainer() noexcept = default;

    void addDateRange(const DateRange& r)
    {
      const auto& key = r.getFirstDateTime();
      auto [it, ok] = mDateRangeMap.emplace(key, r);
      if (!ok)
	{
	  auto msg = "("
	    + boost::posix_time::to_simple_string(key)
	    + ","
	    + boost::posix_time::to_simple_string(r.getLastDateTime())
	    + ") date range already exists";
	  throw std::domain_error("DateRangeContainer: " + msg);
	}
    }

    DateRange getFirstDateRange() const
    {
      if (mDateRangeMap.size() > 0)
	{
	  auto it = beginDateRange();
	  return it->second;
	}
      else
	throw std::domain_error(std::string("DateRangeContainer::getFirstDateRange: no entries in container"));
    }

    unsigned long getNumEntries() const
    {
      return mDateRangeMap.size();
    }

    DateRangeContainer::ConstDateRangeIterator beginDateRange() const
    {
      return mDateRangeMap.begin();
    }

    DateRangeContainer::ConstDateRangeIterator endDateRange() const
    {
      return mDateRangeMap.end();
    }

  private:
    Map mDateRangeMap;
  };
}

#endif

