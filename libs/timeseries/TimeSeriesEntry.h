// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIMESERIES_ENTRY_H
#define __TIMESERIES_ENTRY_H 1

#include <exception>
#include <memory>
#include <string>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "BoostDateHelper.h"
#include "TradingVolume.h"
#include "TimeFrame.h"
#include "number.h"

namespace mkc_timeseries
{
  typedef boost::gregorian::date TimeSeriesDate;
  using boost::posix_time::ptime;
  using boost::posix_time::time_duration;

  extern time_duration getDefaultBarTime();

  //
  // class TimeSeriesEntryException
  //

  class TimeSeriesEntryException : public std::domain_error
  {
  public:
    TimeSeriesEntryException(const std::string msg) 
      : std::domain_error(msg)
    {}
    
    ~TimeSeriesEntryException()
    {}
    
  };

  //
  // class NumericTimeSeriesEntry
  //

  template <class Decimal> class NumericTimeSeriesEntry
  {
  public:
    NumericTimeSeriesEntry (const ptime& entryDateTime,
			    const Decimal& value,
			    TimeFrame::Duration timeFrame) :
      mDateTime(entryDateTime),
      mDate(entryDateTime.date()),
      mTime(entryDateTime.time_of_day()),
      mEntryValue(value),
      mTimeFrame(timeFrame)
    {}

    NumericTimeSeriesEntry (const boost::gregorian::date& entryDate,
			    const Decimal& value,
			    TimeFrame::Duration timeFrame) :
      NumericTimeSeriesEntry(ptime(entryDate, getDefaultBarTime()),
			     value, timeFrame)
    {}

    ~NumericTimeSeriesEntry()
    {}

    NumericTimeSeriesEntry (const NumericTimeSeriesEntry<Decimal>& rhs) 
      : mDateTime(rhs.mDateTime),
	mDate (rhs.mDate),
	mTime(rhs.mTime),
	mEntryValue (rhs.mEntryValue),
	mTimeFrame(rhs.mTimeFrame)
    {}

    NumericTimeSeriesEntry<Decimal>& 
    operator=(const NumericTimeSeriesEntry<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mDateTime = rhs.mDateTime;
      mTime = rhs.mTime;
      mDate = rhs.mDate;
      mEntryValue = rhs.mEntryValue;
      mTimeFrame = rhs.mTimeFrame;
      return *this;
    }

    const boost::gregorian::date& getDate() const
    {
      return mDate;
    }

    const time_duration& getBarTime() const
    {
      return mTime;
    }

    const ptime& getDateTime() const
    {
      return mDateTime;
    }

    const Decimal& getValue() const
    {
      return mEntryValue;
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

  private:
    ptime mDateTime;
    boost::gregorian::date mDate;
    time_duration mTime;
    Decimal mEntryValue;
    TimeFrame::Duration mTimeFrame;
  };


  template <class Decimal>
  bool operator==(const NumericTimeSeriesEntry<Decimal>& lhs, const NumericTimeSeriesEntry<Decimal>& rhs)
  {
    return ((lhs.getDateTime() == rhs.getDateTime()) &&
	    (lhs.getValue() == rhs.getValue()) &&
	    (lhs.getTimeFrame() == rhs.getTimeFrame()));
  }

  template <class Decimal>
  bool operator!=(const NumericTimeSeriesEntry<Decimal>& lhs, const NumericTimeSeriesEntry<Decimal>& rhs)
  { 
    return !(lhs == rhs); 
  }


  //
  // class OHLCTimeSeriesEntry
  //

  template <class Decimal> class OHLCTimeSeriesEntry
  {
  public:
    OHLCTimeSeriesEntry (const ptime& entryDateTime,
			 const Decimal& open,
			 const Decimal& high,
			 const Decimal& low,
			 const Decimal& close,
			 const Decimal& volumeForEntry,
			 TimeFrame::Duration timeFrame)
        : mDateTime(entryDateTime),
	  mDate(entryDateTime.date()),
	  mTime(entryDateTime.time_of_day()),
          mOpen(open),
          mHigh(high),
          mLow(low),
          mClose(close),
          mVolume(volumeForEntry),
          mTimeFrame(timeFrame)
    {
        if (high < open)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::posix_time::to_simple_string (this->getDateTime()) +std::string ("high of ") +num::toString (high) +std::string(" is less that open of ") +num::toString (open));

        if (high < low)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::posix_time::to_simple_string (this->getDateTime()) +std::string ("high of ") +num::toString (high) +std::string(" is less that low of ") +num::toString (low));

        if (high < close)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::posix_time::to_simple_string (this->getDateTime()) +std::string ("high of ") +num::toString (high) +std::string(" is less that close of ") +num::toString (close));

        if (low > open)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::posix_time::to_simple_string (this->getDateTime()) +std::string ("low of ") +num::toString (low) +std::string (" is greater than open of ") +num::toString (open));

        if (low > close)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::posix_time::to_simple_string (this->getDateTime()) +std::string ("low of ") +num::toString (low) +std::string (" is greater than close of ") +num::toString (close));

#if 0
        if(timeFrame == TimeFrame::WEEKLY)
        {
          // For weekly time frame we want to normalize all dates
          // to be the beginning of the week (Sunday)

          if (is_first_of_week (this->getDateValue()) == false)
            mDateTime = first_of_week (this->getDateValue());
        }
        else if (timeFrame == TimeFrame::MONTHLY)
        {
          // For monthly time frame we want to normalize all dates
          // to be the beginning of the month
          if (is_first_of_month (this->getDateValue()) == false)
            {
              mDate = first_of_month (this->getDateValue());
            }
        }
#endif
    }

      OHLCTimeSeriesEntry (const boost::gregorian::date& entryDate,
			   const Decimal& open,
			   const Decimal& high,
			   const Decimal& low,
			   const Decimal& close,
			   const Decimal& volumeForEntry,
			   TimeFrame::Duration timeFrame)
        :  OHLCTimeSeriesEntry (ptime(entryDate, getDefaultBarTime()),
				open, high, low, close,
				volumeForEntry, timeFrame)
      {
      }


    ~OHLCTimeSeriesEntry()
      {}

    OHLCTimeSeriesEntry (const OHLCTimeSeriesEntry<Decimal>& rhs)
      : mDateTime (rhs.mDateTime),
	mDate(rhs.mDate),
	mTime(rhs.mTime),
	mOpen (rhs.mOpen),
	mHigh (rhs.mHigh),
	mLow (rhs.mLow),
	mClose (rhs.mClose),
	mVolume (rhs.mVolume),
	mTimeFrame(rhs.mTimeFrame)
    {}

    OHLCTimeSeriesEntry<Decimal>&
    operator=(const OHLCTimeSeriesEntry<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mDateTime = rhs.mDateTime;
      mDate = rhs.mDate;
      mTime = rhs.mTime;
      mOpen = rhs.mOpen;
      mHigh = rhs.mHigh;
      mLow = rhs.mLow;
      mClose = rhs.mClose;
      mVolume = rhs.mVolume;
      mTimeFrame = rhs.mTimeFrame;
      return *this;
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    const boost::gregorian::date& getDateValue() const
    {
      return mDate;
    }

    const time_duration& getBarTime() const
    {
      return mTime;
    }

    const ptime& getDateTime() const
    {
      return mDateTime;
    }

    const Decimal& getOpenValue() const
    {
      return mOpen;
    }

    const Decimal& getHighValue() const
    {
      return mHigh;
    }

    const Decimal& getLowValue() const
    {
      return mLow;
    }

    const Decimal& getCloseValue() const
    {
      return mClose;
    }

    const Decimal& getVolumeValue() const
    {
      return mVolume;
    }

  private:
    ptime mDateTime;
    boost::gregorian::date mDate;
    time_duration mTime;
    Decimal mOpen;
    Decimal mHigh;
    Decimal mLow;
    Decimal mClose;
    Decimal mVolume;
    TimeFrame::Duration mTimeFrame;
  };

  template <class Decimal>
  bool operator==(const OHLCTimeSeriesEntry<Decimal>& lhs, const OHLCTimeSeriesEntry<Decimal>& rhs)
  {
    return ((lhs.getDateTime() == rhs.getDateTime()) && 
	    (lhs.getOpenValue() == rhs.getOpenValue()) &&
	    (lhs.getHighValue() == rhs.getHighValue()) &&
	    (lhs.getLowValue() == rhs.getLowValue()) &&
	    (lhs.getCloseValue() == rhs.getCloseValue()) &&
	    (lhs.getTimeFrame() == rhs.getTimeFrame()) &&
	    (lhs.getVolumeValue() == rhs.getVolumeValue()));
  }

  template <class Decimal>
  bool operator!=(const OHLCTimeSeriesEntry<Decimal>& lhs, const OHLCTimeSeriesEntry<Decimal>& rhs)
  { 
    return !(lhs == rhs); 
  }
}


#endif
