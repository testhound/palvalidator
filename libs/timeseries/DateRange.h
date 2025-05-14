// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __DATE_RANGE_H
#define __DATE_RANGE_H 1

#include <exception>
#include <boost/date_time.hpp>
#include <map>

namespace mkc_timeseries
{
  using std::map;
  using boost::gregorian::to_simple_string;

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

  inline bool operator==(const DateRange& lhs, const DateRange& rhs)
    {
      return ((lhs.getFirstDate() == rhs.getFirstDate()) &&
	      (lhs.getLastDate() == rhs.getLastDate()));
    }

  inline bool operator!=(const DateRange& lhs, const DateRange& rhs)
    {
      return !(lhs == rhs);
    }

  class DateRangeContainer
  {
    using Map = map<boost::gregorian::date, DateRange>;
  public:
    typedef typename Map::const_iterator ConstDateRangeIterator;
    typedef typename Map::const_iterator DateRangeIterator;

    DateRangeContainer()
      : mDateRangeMap()
    {}

    ~DateRangeContainer()
    {}

    void addDateRange(const DateRange& range)
      {
	boost::gregorian::date firstDate = range.getFirstDate();
	auto it = mDateRangeMap.find(firstDate);

	if (it == mDateRangeMap.end())
	  {
	    mDateRangeMap.insert(std::make_pair(firstDate, std::move(range)));
	  }
	else
	  {
	    std::string date1_as_string(to_simple_string (firstDate));
	    std::string date2_as_string(to_simple_string (range.getLastDate()));
	    std::string range_as_string = std::string("(") + date1_as_string + std::string(",") + date2_as_string + std::string(")");
	    throw std::domain_error(std::string("DateRangeContainer: " +range_as_string  +std::string(" date range already exists")));
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
    map<boost::gregorian::date, DateRange> mDateRangeMap;
  };
}

#endif

