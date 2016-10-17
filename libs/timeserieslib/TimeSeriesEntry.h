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
#include "BoostDateHelper.h"
#include "TradingVolume.h"
#include "TimeFrame.h"
#include "decimal.h"
#include "number.h"

namespace mkc_timeseries
{
  using namespace dec;
  typedef boost::gregorian::date TimeSeriesDate;

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
    NumericTimeSeriesEntry (const boost::gregorian::date& entryDate,
			    const Decimal& value,
			    TimeFrame::Duration timeFrame) :
      mDate(entryDate),
      mEntryValue(value),
      mTimeFrame(timeFrame)
    {}

    ~NumericTimeSeriesEntry()
    {}

    NumericTimeSeriesEntry (const NumericTimeSeriesEntry<Decimal>& rhs) 
      : mDate (rhs.mDate),
	mEntryValue (rhs.mEntryValue),
	mTimeFrame(rhs.mTimeFrame)
    {}

    NumericTimeSeriesEntry<Decimal>& 
    operator=(const NumericTimeSeriesEntry<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mDate = rhs.mDate;
      mEntryValue = rhs.mEntryValue;
      mTimeFrame = rhs.mTimeFrame;
      return *this;
    }

    const boost::gregorian::date& getDate() const
    {
      return mDate;
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
    boost::gregorian::date mDate;
    Decimal mEntryValue;
    TimeFrame::Duration mTimeFrame;
  };


  template <class Decimal>
  bool operator==(const NumericTimeSeriesEntry<Decimal>& lhs, const NumericTimeSeriesEntry<Decimal>& rhs)
  {
    return ((lhs.getDate() == rhs.getDate()) &&
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
      OHLCTimeSeriesEntry (const boost::gregorian::date& entryDate,
		       const Decimal& open,
		       const Decimal& high,
		       const Decimal& low,
		       const Decimal& close,
		       volume_t volume,
		       TimeFrame::Duration timeFrame)
        : mDate(entryDate),
          mOpen(Decimal(open)),
          mHigh(Decimal(high)),
          mLow(Decimal(low)),
          mClose(close),
          mVolume(volume),
          mTimeFrame(timeFrame)
      {
        if (high < open)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (mDate) +std::string ("high of ") +num::toString (high) +std::string(" is less that open of ") +num::toString (open));

        if (high < low)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (mDate) +std::string ("high of ") +num::toString (high) +std::string(" is less that low of ") +num::toString (low));

        if (high < close)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (mDate) +std::string ("high of ") +num::toString (high) +std::string(" is less that close of ") +num::toString (close));

        if (low > open)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (mDate) +std::string ("low of ") +num::toString (low) +std::string (" is greater than open of ") +num::toString (open));

        if (low > close)
        throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (mDate) +std::string ("low of ") +num::toString (low) +std::string (" is greater than close of ") +num::toString (close));

        if(timeFrame == TimeFrame::WEEKLY)
        {
          // For weekly time frame we want to normalize all dates
          // to be the beginning of the week (Sunday)

          if (is_first_of_week (mDate) == false)
            mDate = first_of_week (mDate);
        }
        else if (timeFrame == TimeFrame::MONTHLY)
        {
          // For monthly time frame we want to normalize all dates
          // to be the beginning of the month
          if (is_first_of_month (mDate) == false)
            {
              mDate = first_of_month (mDate);
            }
        }
      }


    ~OHLCTimeSeriesEntry()
      {}

    OHLCTimeSeriesEntry (const OHLCTimeSeriesEntry<Decimal>& rhs)
      : mDate (rhs.mDate),
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

      mDate = rhs.mDate;
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

    const volume_t getVolume() const
    {
      return mVolume;
    }

  private:
    boost::gregorian::date mDate;
    Decimal mOpen;
    Decimal mHigh;
    Decimal mLow;
    Decimal mClose;
    volume_t mVolume;
    TimeFrame::Duration mTimeFrame;
  };

  template <class Decimal>
  bool operator==(const OHLCTimeSeriesEntry<Decimal>& lhs, const OHLCTimeSeriesEntry<Decimal>& rhs)
  {
    return ((lhs.getDateValue() == rhs.getDateValue()) && 
	    (lhs.getOpenValue() == rhs.getOpenValue()) &&
	    (lhs.getHighValue() == rhs.getHighValue()) &&
	    (lhs.getLowValue() == rhs.getLowValue()) &&
	    (lhs.getCloseValue() == rhs.getCloseValue()) &&
	    (lhs.getTimeFrame() == rhs.getTimeFrame()) &&
	    (lhs.getVolume() == rhs.getVolume()));
  }

  template <class Decimal>
  bool operator!=(const OHLCTimeSeriesEntry<Decimal>& lhs, const OHLCTimeSeriesEntry<Decimal>& rhs)
  { 
    return !(lhs == rhs); 
  }
}


#endif
