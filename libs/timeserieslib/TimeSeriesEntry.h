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

  template <int Prec> class NumericTimeSeriesEntry
  {
  public:
    NumericTimeSeriesEntry (const boost::gregorian::date& entryDate,
			    const decimal<Prec>& value,
			    TimeFrame::Duration timeFrame) :
      mDate(entryDate),
      mEntryValue(value),
      mTimeFrame(timeFrame)
    {}

    ~NumericTimeSeriesEntry()
    {}

    NumericTimeSeriesEntry (const NumericTimeSeriesEntry<Prec>& rhs) 
      : mDate (rhs.mDate),
	mEntryValue (rhs.mEntryValue),
	mTimeFrame(rhs.mTimeFrame)
    {}

    NumericTimeSeriesEntry<Prec>& 
    operator=(const NumericTimeSeriesEntry<Prec> &rhs)
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

    const decimal<Prec>& getValue() const
    {
      return mEntryValue;
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

  private:
    boost::gregorian::date mDate;
    decimal<Prec> mEntryValue;
    TimeFrame::Duration mTimeFrame;
  };


  template <int Prec>
  bool operator==(const NumericTimeSeriesEntry<Prec>& lhs, const NumericTimeSeriesEntry<Prec>& rhs)
  {
    return ((lhs.getDate() == rhs.getDate()) &&
	    (lhs.getValue() == rhs.getValue()) &&
	    (lhs.getTimeFrame() == rhs.getTimeFrame()));
  }

  template <int Prec>
  bool operator!=(const NumericTimeSeriesEntry<Prec>& lhs, const NumericTimeSeriesEntry<Prec>& rhs)
  { 
    return !(lhs == rhs); 
  }

  //
  // class OHLCTimeSeriesEntry
  //

  template <int Prec> class OHLCTimeSeriesEntry
  {
  public:
    OHLCTimeSeriesEntry (const std::shared_ptr<boost::gregorian::date> entryDate,
		     const std::shared_ptr<decimal<Prec>>& open,
		     const std::shared_ptr<decimal<Prec>>& high,
		     const std::shared_ptr<decimal<Prec>>& low,
		     const std::shared_ptr<decimal<Prec>>& close,
		     volume_t volume,
		     TimeFrame::Duration timeFrame) :
      mDate(entryDate),
      mOpen(open),
      mHigh(high),
      mLow(low),
      mClose(close),
      mVolume(volume),
      mTimeFrame(timeFrame)
    {
      if (*high < *open)
	throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (*mDate) +std::string ("high of ") +dec::toString (*high) +std::string(" is less that open of ") +dec::toString (*open));

      if (*high < *low)
	throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (*mDate) +std::string ("high of ") +dec::toString (*high) +std::string(" is less that low of ") +dec::toString (*low));

      if (*high < *close)
	throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (*mDate) +std::string ("high of ") +dec::toString (*high) +std::string(" is less that close of ") +dec::toString (*close));

      if (*low > *open)
	throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (*mDate) +std::string ("low of ") +dec::toString (*low) +std::string (" is greater than open of ") +dec::toString (*open));
      
      if (*low > *close)
	throw TimeSeriesEntryException(std::string ("TimeSeriesEntryException: on - ") +boost::gregorian::to_simple_string (*mDate) +std::string ("low of ") +dec::toString (*low) +std::string (" is greater than close of ") +dec::toString (*close));

      if(timeFrame == TimeFrame::WEEKLY)
	{
	  // For weekly time frame we want to normalize all dates
	  // to be the beginning of the week (Sunday)

	  if (is_first_of_week (*mDate) == false)
	    mDate = std::make_shared<boost::gregorian::date> (first_of_week (*mDate));
	}
      else if (timeFrame == TimeFrame::MONTHLY)
	{
	  // For monthly time frame we want to normalize all dates
	  // to be the beginning of the month
	  if (is_first_of_month (*mDate) == false)
	    {
	      mDate = std::make_shared<boost::gregorian::date> (first_of_month (*mDate));
	    }
	}
    }

      OHLCTimeSeriesEntry (const boost::gregorian::date& entryDate,
		       const decimal<Prec>& open,
		       const decimal<Prec>& high,
		       const decimal<Prec>& low,
		       const decimal<Prec>& close,
		       volume_t volume,
		       TimeFrame::Duration timeFrame) 
	: OHLCTimeSeriesEntry (std::make_shared<boost::gregorian::date> (entryDate),
			   std::make_shared<decimal<Prec>> (open),
			   std::make_shared<decimal<Prec>> (high),
			   std::make_shared<decimal<Prec>> (low),
			   std::make_shared<decimal<Prec>> (close),
			   volume,
			   timeFrame)
	{}


    ~OHLCTimeSeriesEntry()
      {}

    OHLCTimeSeriesEntry (const OHLCTimeSeriesEntry<Prec>& rhs) 
      : mDate (rhs.mDate),
	mOpen (rhs.mOpen),
	mHigh (rhs.mHigh),
	mLow (rhs.mLow),
	mClose (rhs.mClose),
	mVolume (rhs.mVolume),
	mTimeFrame(rhs.mTimeFrame)
    {}

    OHLCTimeSeriesEntry<Prec>& 
    operator=(const OHLCTimeSeriesEntry<Prec> &rhs)
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

    const std::shared_ptr<boost::gregorian::date>& getDate() const
    {
      return mDate;
    }

    const boost::gregorian::date& getDateValue() const
    {
      return *getDate();
    }

    const std::shared_ptr<decimal<Prec>>& getOpen() const
    {
      return mOpen;
    }

    const decimal<Prec>& getOpenValue() const
    {
      return *getOpen();
    }

    const std::shared_ptr<decimal<Prec>>& getHigh() const
    {
      return mHigh;
    }

    const decimal<Prec>& getHighValue() const
    {
      return *getHigh();
    }

    const std::shared_ptr<decimal<Prec>>& getLow() const
    {
      return mLow;
    }

    const decimal<Prec>& getLowValue() const
    {
      return *getLow();

    }
    const std::shared_ptr<decimal<Prec>>& getClose() const
    {
      return mClose;
    }

    const decimal<Prec>& getCloseValue() const
    {
      return *getClose();
    }

    const volume_t getVolume() const
    {
      return mVolume;
    }

  private:
    std::shared_ptr<boost::gregorian::date> mDate;
    std::shared_ptr<decimal<Prec>> mOpen;
    std::shared_ptr<decimal<Prec>> mHigh;
    std::shared_ptr<decimal<Prec>> mLow;
    std::shared_ptr<decimal<Prec>> mClose;
    volume_t mVolume;
    TimeFrame::Duration mTimeFrame;
  };

  template <int Prec>
  bool operator==(const OHLCTimeSeriesEntry<Prec>& lhs, const OHLCTimeSeriesEntry<Prec>& rhs)
  {
    return ((lhs.getDateValue() == rhs.getDateValue()) && 
	    (lhs.getOpenValue() == rhs.getOpenValue()) &&
	    (lhs.getHighValue() == rhs.getHighValue()) &&
	    (lhs.getLowValue() == rhs.getLowValue()) &&
	    (lhs.getCloseValue() == rhs.getCloseValue()) &&
	    (lhs.getTimeFrame() == rhs.getTimeFrame()) &&
	    (lhs.getVolume() == rhs.getVolume()));
  }

  template <int Prec>
  bool operator!=(const OHLCTimeSeriesEntry<Prec>& lhs, const OHLCTimeSeriesEntry<Prec>& rhs)
  { 
    return !(lhs == rhs); 
  }
}


#endif
