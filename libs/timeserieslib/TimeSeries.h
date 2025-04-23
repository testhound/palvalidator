// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIMESERIES_H
#define __TIMESERIES_H 1

#include <iostream>
#include <map>
#include <boost/container/flat_map.hpp>
#include <vector>
#include <boost/thread/mutex.hpp>
#include "TimeSeriesEntry.h"
#include "DateRange.h"

namespace mkc_timeseries
{
  using boost::posix_time::ptime;
  using boost::posix_time::time_duration;

  class TimeSeriesException : public std::runtime_error
  {
  public:
    TimeSeriesException(const std::string msg)
      : std::runtime_error(msg)
    {}

    ~TimeSeriesException()
    {}

  };

  class TimeSeriesOffset
  {
  public:
    unsigned long asIntegral() const
    {
      return mOffset;
    }

  private:
    TimeSeriesOffset (unsigned long offset) : mOffset(offset)
    {}

  private:
    typedef std::map<unsigned long, std::shared_ptr<TimeSeriesOffset>>::iterator CacheIterator;

  public:
    static std::shared_ptr<TimeSeriesOffset> createOffset (unsigned long offset)
    {
      boost::mutex::scoped_lock lock(mOffsetCacheMutex);

      CacheIterator pos = mOffsetCache.find (offset);
      if (pos != mOffsetCache.end())
	return pos->second;
      else
	{
	  std::shared_ptr<TimeSeriesOffset> p (new TimeSeriesOffset (offset));
	  mOffsetCache.insert(std::make_pair(offset, p));
	  return p;
	}
    }

  private:
    unsigned long mOffset;
    static boost::mutex  mOffsetCacheMutex;
    static std::map<unsigned long, std::shared_ptr<TimeSeriesOffset>> mOffsetCache;
  };

  inline bool operator< (const TimeSeriesOffset& lhs, const TimeSeriesOffset& rhs)
  {
    return lhs.asIntegral() < rhs.asIntegral();
  }

  inline bool operator> (const TimeSeriesOffset& lhs, const TimeSeriesOffset& rhs){ return rhs < lhs; }
  inline bool operator<=(const TimeSeriesOffset& lhs, const TimeSeriesOffset& rhs){ return !(lhs > rhs); }
  inline bool operator>=(const TimeSeriesOffset& lhs, const TimeSeriesOffset& rhs){ return !(lhs < rhs); }

  inline bool operator==(const TimeSeriesOffset& lhs, const TimeSeriesOffset& rhs)
  {
    return (lhs.asIntegral() == rhs.asIntegral());
  }

  inline bool operator!=(const TimeSeriesOffset& lhs, const TimeSeriesOffset& rhs){ return !(lhs == rhs); }

  class ArrayTimeSeriesIndex
  {
  private:
    typedef std::map<unsigned long, std::shared_ptr<ArrayTimeSeriesIndex>>::iterator CacheIterator;

  public:
    ArrayTimeSeriesIndex fromOffset (const std::shared_ptr<TimeSeriesOffset>& offset)
    {
      if (mArrayIndex >= offset->asIntegral())
	{
	  unsigned long newIndex = mArrayIndex - offset->asIntegral();

	  return ArrayTimeSeriesIndex(newIndex);
	}
      else
	throw std::out_of_range("ArrayTimeSeriesIndex: offset cannot be larger than array index");
    }

    unsigned long asIntegral() const
    {
      return mArrayIndex;
    }

    ArrayTimeSeriesIndex (unsigned long arrayIndex) : mArrayIndex(arrayIndex)
    {}

    ArrayTimeSeriesIndex()
      : mArrayIndex(0) {}

  private:
    unsigned long mArrayIndex;
  };


  inline bool operator< (const ArrayTimeSeriesIndex& lhs, const ArrayTimeSeriesIndex& rhs)
  {
    return lhs.asIntegral() < rhs.asIntegral();
  }

  inline bool operator> (const ArrayTimeSeriesIndex& lhs, const ArrayTimeSeriesIndex& rhs){ return rhs < lhs; }
  inline bool operator<=(const ArrayTimeSeriesIndex& lhs, const ArrayTimeSeriesIndex& rhs){ return !(lhs > rhs); }
  inline bool operator>=(const ArrayTimeSeriesIndex& lhs, const ArrayTimeSeriesIndex& rhs){ return !(lhs < rhs); }

  inline bool operator==(const ArrayTimeSeriesIndex& lhs, const ArrayTimeSeriesIndex& rhs)
  {
    return (lhs.asIntegral() == rhs.asIntegral());
  }

  inline bool operator!=(const ArrayTimeSeriesIndex& lhs, const ArrayTimeSeriesIndex& rhs){ return !(lhs == rhs); }


  //
  //  class NumericTimeSeries
  //

  template <class Decimal> class NumericTimeSeries
  {
    using Map = boost::container::flat_map<ptime, std::shared_ptr<NumericTimeSeriesEntry<Decimal>>>;

  public:
    typedef typename Map::const_iterator ConstTimeSeriesIterator;
    typedef typename Map::const_reverse_iterator ConstReverseTimeSeriesIterator;
    typedef typename std::vector<std::shared_ptr<NumericTimeSeriesEntry<Decimal>>>::const_iterator ConstRandomAccessIterator;

    NumericTimeSeries (TimeFrame::Duration timeFrame) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true),
      mMutex()
    {}

    NumericTimeSeries (TimeFrame::Duration timeFrame,  unsigned long numElements) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true),
      mMutex()
    {
      mSequentialTimeSeries.reserve(numElements);
    }

    NumericTimeSeries(const NumericTimeSeries<Decimal>& rhs)
      : mSortedTimeSeries(),
	mDateToSequentialIndex(),
	mSequentialTimeSeries(),
	mTimeFrame(rhs.mTimeFrame),
	mMapAndArrayInSync(rhs.mMapAndArrayInSync),
	mMutex()
    {
      boost::mutex::scoped_lock lock(rhs.mMutex);
      mSortedTimeSeries = rhs.mSortedTimeSeries;
      mDateToSequentialIndex = rhs.mDateToSequentialIndex;
      mSequentialTimeSeries = rhs.mSequentialTimeSeries;
    }

    NumericTimeSeries(NumericTimeSeries<Decimal>&& rhs) noexcept
      : mSortedTimeSeries(),
	mDateToSequentialIndex(),
	mSequentialTimeSeries(),
	mTimeFrame(rhs.mTimeFrame),
	mMapAndArrayInSync(rhs.mMapAndArrayInSync),
	mMutex()
    {
      boost::mutex::scoped_lock lock(rhs.mMutex);
      mSortedTimeSeries = std::move(rhs.mSortedTimeSeries);
      mDateToSequentialIndex = std::move(rhs.mDateToSequentialIndex);
      mSequentialTimeSeries = std::move(rhs.mSequentialTimeSeries);
    }
    
    NumericTimeSeries<Decimal>& operator=(const NumericTimeSeries<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;

      boost::mutex::scoped_lock lock(mMutex);
      boost::mutex::scoped_lock rhsLock(rhs.mMutex);

      mSortedTimeSeries = rhs.mSortedTimeSeries;
      mDateToSequentialIndex = rhs.mDateToSequentialIndex;
      mSequentialTimeSeries = rhs.mSequentialTimeSeries;
      mTimeFrame = rhs.mTimeFrame;
      mMapAndArrayInSync = rhs.mMapAndArrayInSync;

      return *this;
    }

    // Move assignment
    NumericTimeSeries<Decimal>& operator=(NumericTimeSeries<Decimal>&& rhs) noexcept
    {
      if (this != &rhs)
	{
	  boost::mutex::scoped_lock lock(mMutex);
	  boost::mutex::scoped_lock rhsLock(rhs.mMutex);

	  mSortedTimeSeries = std::move(rhs.mSortedTimeSeries);
	  mDateToSequentialIndex = std::move(rhs.mDateToSequentialIndex);
	  mSequentialTimeSeries = std::move(rhs.mSequentialTimeSeries);
	  mTimeFrame = rhs.mTimeFrame;
	  mMapAndArrayInSync = rhs.mMapAndArrayInSync;
	}
      
      return *this;
    }

    void addEntry (std::shared_ptr<NumericTimeSeriesEntry<Decimal>> entry)
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (entry->getTimeFrame() != getTimeFrame())
	throw std::domain_error(std::string("NumericTimeSeries:addEntry " +boost::posix_time::to_simple_string(entry->getDateTime()) + std::string(" time frames do not match")));

      auto result = mSortedTimeSeries.emplace(entry->getDateTime(), entry);
      if (!result.second)
	throw std::domain_error("NumericTimeSeries:addEntry: entry for time already exists");

      mMapAndArrayInSync = false;
    }

    void addEntry (const NumericTimeSeriesEntry<Decimal>& entry)
    {
      addEntry (std::make_shared<NumericTimeSeriesEntry<Decimal>> (entry));
    }

    NumericTimeSeries::ConstTimeSeriesIterator getTimeSeriesEntry (const boost::gregorian::date& timeSeriesDate) const
    {
      ptime dateTime(timeSeriesDate, getDefaultBarTime());
      boost::mutex::scoped_lock lock(mMutex);

      return mSortedTimeSeries.find(dateTime);
    }

    std::vector<Decimal> getTimeSeriesAsVector() const
    {
      std::vector<Decimal> series;
      boost::mutex::scoped_lock lock(mMutex);

      series.reserve(mSortedTimeSeries.size());
      for (const auto& kv : mSortedTimeSeries)
	{
	  series.push_back(kv.second->getValue());
	}

      return series;
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    unsigned long getNumEntries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mSortedTimeSeries.size();
    }

    NumericTimeSeries::ConstRandomAccessIterator
    getRandomAccessIterator(const boost::gregorian::date& d) const
    {
      ptime dateTime(d, getDefaultBarTime());
      ensureSynchronized();

      auto pos = mDateToSequentialIndex.find(dateTime);
      if (pos != mDateToSequentialIndex.end())
	{
	  return mSequentialTimeSeries.begin() + pos->second.asIntegral();
	}
      return mSequentialTimeSeries.end();
    }

    ConstRandomAccessIterator beginRandomAccess() const
    {
      ensureSynchronized();
      return mSequentialTimeSeries.begin();
    }

    ConstRandomAccessIterator endRandomAccess() const
    {
      ensureSynchronized();
      return mSequentialTimeSeries.end();
    }
  
    NumericTimeSeries::ConstTimeSeriesIterator beginSortedAccess() const
    {
      return mSortedTimeSeries.begin();

    }

    NumericTimeSeries::ConstReverseTimeSeriesIterator beginReverseSortedAccess() const
    {
      return mSortedTimeSeries.rbegin();
    }

    NumericTimeSeries::ConstTimeSeriesIterator endSortedAccess() const
    {
      return mSortedTimeSeries.end();
    }

    NumericTimeSeries::ConstReverseTimeSeriesIterator endReverseSortedAccess() const
    {
      return mSortedTimeSeries.rend();
    }

    const boost::gregorian::date getFirstDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      
      if (mSortedTimeSeries.empty())
	throw std::domain_error("NumericTimeSeries:getFirstDate: no entries in time series");

      return mSortedTimeSeries.begin()->first.date();
    }

    const boost::gregorian::date getLastDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      
      if (mSortedTimeSeries.empty())
	throw std::domain_error("NumericTimeSeries:getLastDate: no entries in time series");
      
      return mSortedTimeSeries.rbegin()->first.date();
    }

    const std::shared_ptr<NumericTimeSeriesEntry<Decimal>>& getTimeSeriesEntry (const ConstRandomAccessIterator& it,
										unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);
      NumericTimeSeries::ConstRandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    const std::shared_ptr<boost::gregorian::date>&
    getDate (const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      return (getTimeSeriesEntry (it, offset)->getDate());
    }

    const boost::gregorian::date&
    getDateValue (const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);      
      return (*getDate(it, offset));
    }

    const Decimal& getValue (const ConstRandomAccessIterator& it,
			     unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);      
      return (getTimeSeriesEntry (it, offset)->getValue());
    }

  private:
    void ensureSynchronized() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (!mMapAndArrayInSync)
	synchronize_unlocked();
    }

    void synchronize_unlocked() const
    {
      mSequentialTimeSeries.clear();
      mDateToSequentialIndex.clear();

      unsigned long index = 0;
      for (const auto& kv : mSortedTimeSeries)
	{
	  mDateToSequentialIndex[kv.first] = ArrayTimeSeriesIndex(index);
	  mSequentialTimeSeries.push_back(kv.second);
	  ++index;
	}
      mMapAndArrayInSync = true;
    }

    void ValidateVectorOffset(const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      ensureSynchronized();
      if (it == mSequentialTimeSeries.end())
	throw TimeSeriesException("Iterator is at end of time series");
      if ((it - offset) < mSequentialTimeSeries.begin())
	throw TimeSeriesException("Offset " + std::to_string(offset) + " outside bounds of time series");
    }

    void ValidateVectorOffset(unsigned long offset) const
    {
      ensureSynchronized();
      if (offset > mSequentialTimeSeries.size())
	throw TimeSeriesException("Offset " + std::to_string(offset) + " exceeds size of time series");
    }
  
    bool isSynchronized() const
    {
      return (mMapAndArrayInSync);
    }

  private:
    Map mSortedTimeSeries;
    mutable boost::container::flat_map<ptime, ArrayTimeSeriesIndex> mDateToSequentialIndex;
    mutable std::vector<std::shared_ptr<NumericTimeSeriesEntry<Decimal>>> mSequentialTimeSeries;
    TimeFrame::Duration mTimeFrame;
    mutable bool mMapAndArrayInSync;
    mutable boost::mutex mMutex;
  };

  /*
    class TimeSeries

   */

template <class Decimal> class OHLCTimeSeries
  {
  public:
    typedef typename boost::container::flat_map<ptime, OHLCTimeSeriesEntry<Decimal>>::const_iterator ConstTimeSeriesIterator;
    typedef typename std::vector<OHLCTimeSeriesEntry<Decimal>>::const_iterator ConstRandomAccessIterator;

    NumericTimeSeries<Decimal> OpenTimeSeries() const
    {
      NumericTimeSeries<Decimal> openSeries(getTimeFrame(), getNumEntries());
      OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = beginSortedAccess();

      for (; it != endSortedAccess(); it++)
    {
      openSeries.addEntry (NumericTimeSeriesEntry<Decimal> (it->first,
                                  it->second.getOpenValue(),
                                  it->second.getTimeFrame()));
    }

      return openSeries;
    }

    NumericTimeSeries<Decimal> HighTimeSeries() const
    {
      NumericTimeSeries<Decimal> highSeries(getTimeFrame(), getNumEntries());
      OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = beginSortedAccess();

      for (; it != endSortedAccess(); it++)
    {
      highSeries.addEntry (NumericTimeSeriesEntry<Decimal> (it->first,
                                  it->second.getHighValue(),
                                  it->second.getTimeFrame()));
    }

      return highSeries;
    }

    NumericTimeSeries<Decimal> LowTimeSeries() const
    {
      NumericTimeSeries<Decimal> lowSeries(getTimeFrame(), getNumEntries());
      OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = beginSortedAccess();

      for (; it != endSortedAccess(); it++)
    {
      lowSeries.addEntry (NumericTimeSeriesEntry<Decimal> (it->first,
                                  it->second.getLowValue(),
                                  it->second.getTimeFrame()));
    }

      return lowSeries;
    }

    NumericTimeSeries<Decimal> CloseTimeSeries() const
    {
      NumericTimeSeries<Decimal> closeSeries(getTimeFrame(), getNumEntries());
      OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = beginSortedAccess();

      for (; it != endSortedAccess(); it++)
    {
      closeSeries.addEntry (NumericTimeSeriesEntry<Decimal> (it->first,
                                  it->second.getCloseValue(),
                                  it->second.getTimeFrame()));
    }

      return closeSeries;
    }

    OHLCTimeSeries (TimeFrame::Duration timeFrame, TradingVolume::VolumeUnit unitsOfVolume) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true),
      mUnitsOfVolume(unitsOfVolume),
      mMutex()
    {}

    OHLCTimeSeries (TimeFrame::Duration timeFrame, TradingVolume::VolumeUnit unitsOfVolume,
		    unsigned long numElements) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true),
      mUnitsOfVolume(unitsOfVolume),
      mMutex()
    {
      mSequentialTimeSeries.reserve(numElements);
    }

    OHLCTimeSeries(const OHLCTimeSeries<Decimal>& rhs)
      : mSortedTimeSeries(),
	mDateToSequentialIndex(),
	mSequentialTimeSeries(),
	mTimeFrame(rhs.mTimeFrame),
	mMapAndArrayInSync(rhs.mMapAndArrayInSync),
	mUnitsOfVolume(rhs.mUnitsOfVolume),
	mMutex() // Create a new mutex for this instance.
    {
      // Lock rhs to ensure we copy a consistent state.
      boost::mutex::scoped_lock lock(rhs.mMutex);
      mSortedTimeSeries = rhs.mSortedTimeSeries;
      mDateToSequentialIndex = rhs.mDateToSequentialIndex;
      mSequentialTimeSeries = rhs.mSequentialTimeSeries;
    }
    
    OHLCTimeSeries<Decimal>&
    operator=(const OHLCTimeSeries<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      boost::mutex::scoped_lock lock(mMutex);           // Lock this instance
      boost::mutex::scoped_lock rhsLock(rhs.mMutex);    // Lock rhs to safely read shared state

      // Copy all member *except* the mutex
      // boost::mutex (and std::mutex) is non-copyable and non-assignable
      
      mSortedTimeSeries = rhs.mSortedTimeSeries;
      mDateToSequentialIndex = rhs.mDateToSequentialIndex;
      mSequentialTimeSeries = rhs.mSequentialTimeSeries;
      mTimeFrame  = rhs.mTimeFrame;
      mMapAndArrayInSync = rhs.mMapAndArrayInSync;
      mUnitsOfVolume = rhs.mUnitsOfVolume;

      return *this;
    }

    // Move copy constructor
    OHLCTimeSeries(OHLCTimeSeries<Decimal>&& rhs) noexcept
      : mSortedTimeSeries(std::move(rhs.mSortedTimeSeries)),
	mDateToSequentialIndex(std::move(rhs.mDateToSequentialIndex)),
	mSequentialTimeSeries(std::move(rhs.mSequentialTimeSeries)),
	mTimeFrame(rhs.mTimeFrame),
	mMapAndArrayInSync(rhs.mMapAndArrayInSync),
	mUnitsOfVolume(rhs.mUnitsOfVolume),
	mMutex() // Fresh mutex
    {}

    OHLCTimeSeries<Decimal>& operator=(OHLCTimeSeries<Decimal>&& rhs) noexcept
    {
      if (this != &rhs)
	{
	  boost::mutex::scoped_lock lock(mMutex);
	  boost::mutex::scoped_lock rhsLock(rhs.mMutex);

	  mSortedTimeSeries = std::move(rhs.mSortedTimeSeries);
	  mDateToSequentialIndex = std::move(rhs.mDateToSequentialIndex);
	  mSequentialTimeSeries = std::move(rhs.mSequentialTimeSeries);
	  mTimeFrame = rhs.mTimeFrame;
	  mMapAndArrayInSync = rhs.mMapAndArrayInSync;
	  mUnitsOfVolume = rhs.mUnitsOfVolume;
	  // Note: mMutex remains unchanged (fresh per object)
	}

      return *this;
    }
    
    void addEntry (OHLCTimeSeriesEntry<Decimal> entry)
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (entry.getTimeFrame() != getTimeFrame())
	throw std::domain_error(std::string("OHLCTimeSeries:addEntry " +boost::posix_time::to_simple_string(entry.getDateTime()) + std::string(" time frames do not match")));

      auto result = mSortedTimeSeries.emplace(entry.getDateTime(), std::move(entry));
      if (!result.second)
	throw std::domain_error("OHLCTimeSeries: entry for time already exists: " + boost::posix_time::to_simple_string(entry.getDateTime()));
	
      mMapAndArrayInSync = false;
    }

    ConstTimeSeriesIterator getTimeSeriesEntry(const boost::gregorian::date& timeSeriesDate) const
    {
      ptime dateTime(timeSeriesDate, getDefaultBarTime());
      boost::mutex::scoped_lock lock(mMutex);
      return mSortedTimeSeries.find(dateTime);
    }

    ConstTimeSeriesIterator getTimeSeriesEntry(const ptime& timeSeriesDate) const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mSortedTimeSeries.find(timeSeriesDate);
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    unsigned long getNumEntries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mSortedTimeSeries.size();
    }

    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return mUnitsOfVolume;
    }

    std::vector<OHLCTimeSeriesEntry<Decimal>> getEntriesCopy() const
    {
      synchronize();
      return mSequentialTimeSeries; // safe copy
    }
    
    OHLCTimeSeries::ConstRandomAccessIterator beginRandomAccess() const
    {
      synchronize();
      return mSequentialTimeSeries.begin();
    }

    OHLCTimeSeries::ConstRandomAccessIterator endRandomAccess() const
    {
      synchronize();
      return mSequentialTimeSeries.end();
    }

    OHLCTimeSeries::ConstRandomAccessIterator getRandomAccessIterator(const boost::gregorian::date& d) const
    {
      ptime dateTime(d, getDefaultBarTime());

      // Lock and synchronize only once
      {
        boost::mutex::scoped_lock lock(mMutex);
        if (!mMapAndArrayInSync)
            synchronize_unlocked();  // internal version assumes lock is already held
      }

      auto pos = mDateToSequentialIndex.find(dateTime);
      if (pos != mDateToSequentialIndex.end())
	{
	  const ArrayTimeSeriesIndex& index = pos->second;
	  return mSequentialTimeSeries.begin() + index.asIntegral();
	}

      return mSequentialTimeSeries.end();
    }

    OHLCTimeSeries::ConstTimeSeriesIterator beginSortedAccess() const
    {
      return mSortedTimeSeries.begin();
    }

    OHLCTimeSeries::ConstTimeSeriesIterator endSortedAccess() const
    {
      return mSortedTimeSeries.end();
    }

    const boost::gregorian::date getFirstDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);
 
      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getFirstDate: no entries in time series");

      return mSortedTimeSeries.begin()->first.date();
    }
   
    const ptime getFirstDateTime() const
    {
      boost::mutex::scoped_lock lock(mMutex);

      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getFirstDateTime: no entries in time series");

      return mSortedTimeSeries.begin()->first;
    }
    
    const boost::gregorian::date getLastDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getLastDate: no entries in time series");

      return mSortedTimeSeries.rbegin()->first.date();
    }

    const ptime getLastDateTime() const
    {
      boost::mutex::scoped_lock lock(mMutex);

      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getLastDateTime: no entries in time series");

      return mSortedTimeSeries.rbegin()->first;
    }

    const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const ConstRandomAccessIterator& it,
                                      unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);
      OHLCTimeSeries::ConstRandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    const boost::gregorian::date&
    getDateValue (const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getDateValue();
    }

    const Decimal& getOpenValue (const ConstRandomAccessIterator& it,
                       unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getOpenValue();
    }

    const Decimal& getHighValue (const ConstRandomAccessIterator& it,
                       unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getHighValue();
    }

    const Decimal& getLowValue (const ConstRandomAccessIterator& it,
                       unsigned long offset) const
    {
      return (getTimeSeriesEntry (it, offset).getLowValue());
    }

    const Decimal& getCloseValue (const ConstRandomAccessIterator& it,
                       unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getCloseValue();
    }
    ///

    const Decimal& getVolumeValue (const ConstRandomAccessIterator& it,
                       unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getVolumeValue();
    }

    bool isDateFound(const boost::gregorian::date& date)
    {
      ptime dateTime(date, getDefaultBarTime());

      boost::mutex::scoped_lock lock(mMutex);      
      return (mSortedTimeSeries.find(dateTime) != mSortedTimeSeries.end());
    }

    void deleteEntryByDate(const boost::gregorian::date& date)
    {
      boost::mutex::scoped_lock lock(mMutex);
      
      auto getDateEntryFromDate = [=](auto date)
      {
        for(auto it = mSortedTimeSeries.begin(); it != mSortedTimeSeries.end(); it++)
	  {
	    boost::gregorian::date mapDate(it->first.date().year(), it->first.date().month(), it->first.date().day());
	    if(mapDate == date)
	      return it;
	  }
        return mSortedTimeSeries.end();
      };

      // isDateFound only looks for dates at 15:00, we need to delete all times for hourly
      // time series and the 0:00 time for daily time series
      auto mapIterator = getDateEntryFromDate(date);
      while(mapIterator != mSortedTimeSeries.end())
      {
        mSortedTimeSeries.erase(mapIterator);
        mapIterator = getDateEntryFromDate(date);
      }
      
      mMapAndArrayInSync = false;
    }

  private:
    void ensureSynchronized() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (!mMapAndArrayInSync)
        synchronize_unlocked();
    }

    void synchronize() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mMapAndArrayInSync)
	return;

      synchronize_unlocked();
    }

    void synchronize_unlocked() const
    {
      mSequentialTimeSeries.clear();
      mDateToSequentialIndex.clear();
    
      unsigned long index = 0;
      for (const auto& kv : mSortedTimeSeries)
	{
	  mDateToSequentialIndex[kv.first] = ArrayTimeSeriesIndex(index);
	  mSequentialTimeSeries.push_back(kv.second);
	  ++index;
	}
      mMapAndArrayInSync = true;
    }

    void ValidateVectorOffset (const ConstRandomAccessIterator& it,
			       unsigned long offset) const
    {      
      ensureSynchronized() ;

      if (it == mSequentialTimeSeries.end())
        throw TimeSeriesException("Iterator is at end of time series");

      if ((it - offset) < mSequentialTimeSeries.begin())
        throw TimeSeriesException("Offset " + std::to_string(offset) + " outside bounds of time series");
    }

    void ValidateVectorOffset (unsigned long offset) const
    {
      ensureSynchronized();

      if (offset > mSequentialTimeSeries.size())
	  throw TimeSeriesException(std::string("OHLCTimeSeries:ValidateVectorOffset ") +std::string(" offset ") +std::to_string (offset) +std::string(" > number of elements in time seres"));
    }

  private:
    boost::container::flat_map<ptime, OHLCTimeSeriesEntry<Decimal>> mSortedTimeSeries;
    mutable boost::container::flat_map<ptime, ArrayTimeSeriesIndex> mDateToSequentialIndex;
    mutable std::vector<OHLCTimeSeriesEntry<Decimal>> mSequentialTimeSeries;
    TimeFrame::Duration mTimeFrame;
    mutable bool mMapAndArrayInSync;
    TradingVolume::VolumeUnit mUnitsOfVolume;
    mutable boost::mutex mMutex;
  };

  template <class Decimal>
  bool operator==(const OHLCTimeSeries<Decimal>& lhs, const OHLCTimeSeries<Decimal>& rhs)
  {
    if (lhs.getNumEntries() != rhs.getNumEntries())
      return false;

    if (lhs.getTimeFrame() != rhs.getTimeFrame())
      return false;

    if (lhs.getVolumeUnits() != rhs.getVolumeUnits())
      return false;

    typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it1 = lhs.beginSortedAccess();
    typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it2 = rhs.beginSortedAccess();

    for (; it1 != lhs.endSortedAccess() && it2 != rhs.endSortedAccess(); it1++, it2++)
      {
    if (it1->second != it2->second)
      return false;
      }

    return true;
  }

  template <class Decimal>
  bool operator!=(const OHLCTimeSeries<Decimal>& lhs, const OHLCTimeSeries<Decimal>& rhs)
  {
    return !(lhs == rhs);
  }

  template <class Decimal>
    std::ostream& operator<<(std::ostream& os, const OHLCTimeSeries<Decimal>& series)
    {
        // Optional: Set precision for Decimal types if needed
        // os << std::fixed << std::setprecision(4); // Example: 4 decimal places

        // Optional: Print a header line
        os << "DateTime,Open,High,Low,Close,Volume\n";

        // Iterate through the time series using the sorted access iterator
        for (typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = series.beginSortedAccess();
             it != series.endSortedAccess(); ++it)
        {
            const ptime& dateTime = it->first;                // Get the boost::posix_time::ptime key
            const OHLCTimeSeriesEntry<Decimal>& entry = it->second; // Get the OHLC entry value

            // Output the date and time (adjust formatting as desired)
            // Using to_simple_string for combined date and time, or dateTime.date() for just the date
            os << boost::posix_time::to_simple_string(dateTime);

            // Output the OHLCV values, separated by commas (or tabs, spaces, etc.)
            os << "," << entry.getOpenValue();
            os << "," << entry.getHighValue();
            os << "," << entry.getLowValue();
            os << "," << entry.getCloseValue();
            os << "," << entry.getVolumeValue(); // Assuming volume is also desired

            // Add a newline character for the next entry
            os << "\n";
        }

        return os; // Return the ostream reference to allow chaining (e.g., std::cout << series << " done";)
    }
  
  // Create a new time series containing entries covered by date range
  template <class Decimal>
  OHLCTimeSeries<Decimal> FilterTimeSeries (const OHLCTimeSeries<Decimal>& series, const DateRange& dates)
  {
    boost::gregorian::date firstDate(dates.getFirstDate());
    boost::gregorian::date lastDate(dates.getLastDate());

    ptime firstDateAsPtime(firstDate, getDefaultBarTime());
    ptime lastDateAsPtime(lastDate, getDefaultBarTime());

    const boost::gregorian::date seriesFirstDate = series.getFirstDate();
    const boost::gregorian::date seriesLastDate = series.getLastDate();

    if ((seriesFirstDate == firstDate) && (seriesLastDate == lastDate))
      return series;

    if (firstDate < seriesFirstDate)
      throw TimeSeriesException("FilterTimeSeries: Cannot create new series that starts before reference series");

    if (lastDate < seriesFirstDate)
      throw TimeSeriesException("FilterTimeSeries: Cannot create new series that starts before reference series");

    OHLCTimeSeries<Decimal> resultSeries(series.getTimeFrame(), series.getVolumeUnits(), series.getNumEntries());

    typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = series.beginSortedAccess();

    // Advance to the first relevant point (if needed)
    while (it != series.endSortedAccess() && it->first < firstDateAsPtime)
      ++it;

    // Add entries in range
    for (; it != series.endSortedAccess() && it->first <= lastDateAsPtime; ++it)
      resultSeries.addEntry(it->second);

    return resultSeries;
  }
}

#endif
