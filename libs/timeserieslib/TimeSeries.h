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

  /**
   * @brief Represents a time series of Open, High, Low, Close (OHLC) and Volume data.
   * @tparam Decimal The numeric type used for price and volume data (e.g., double, float).
   *
   * This class is central to financial backtesting systems, holding historical
   * price and volume information for instruments like equities or futures.
   *
   * It maintains data internally in two synchronized structures:
   *
   * 1. A `boost::container::flat_map` (`mSortedTimeSeries`) mapping `ptime` timestamps
   * to `OHLCTimeSeriesEntry` objects, ensuring time-sorted order and efficient
   * lookup/insertion/deletion by date/time.
   * 2. A `std::vector` (`mSequentialTimeSeries`) storing the `OHLCTimeSeriesEntry`
   * objects in sequential time order, allowing for fast random access by index.
   *
   * A `boost::mutex` (`mMutex`) is used to ensure thread safety for concurrent access.
   * Operations that modify the series (`addEntry`, `deleteEntryByDate`) or require
   * the vector view (`beginRandomAccess`, `getRandomAccessIterator`, etc.) handle
   * synchronization between the map and vector representations via the `mMapAndArrayInSync` flag.
   */  
  template <class Decimal> class OHLCTimeSeries
  {
  public:
    typedef typename boost::container::flat_map<ptime, OHLCTimeSeriesEntry<Decimal>>::const_iterator ConstTimeSeriesIterator;
    typedef typename std::vector<OHLCTimeSeriesEntry<Decimal>>::const_iterator ConstRandomAccessIterator;

    /**
     * @brief Extracts the Open prices into a separate NumericTimeSeries.
     * @return A NumericTimeSeries<Decimal> containing only the Open values,
     * sharing the same time frame and dates as this OHLC series.
     */
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

     /**
     * @brief Extracts the High prices into a separate NumericTimeSeries.
     * @return A NumericTimeSeries<Decimal> containing only the High values,
     * sharing the same time frame and dates as this OHLC series.
     */
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

    /**
     * @brief Extracts the Low prices into a separate NumericTimeSeries.
     * @return A NumericTimeSeries<Decimal> containing only the Low values,
     * sharing the same time frame and dates as this OHLC series.
     */
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

    /**
     * @brief Extracts the Close prices into a separate NumericTimeSeries.
     * @return A NumericTimeSeries<Decimal> containing only the Close values,
     * sharing the same time frame and dates as this OHLC series.
     */
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

    /**
     * @brief Constructs an empty OHLCTimeSeries.
     * @param timeFrame The time interval between data points (e.g., daily, hourly).
     * @param unitsOfVolume The units used for the volume data (e.g., shares, contracts).
     */
    OHLCTimeSeries (TimeFrame::Duration timeFrame, TradingVolume::VolumeUnit unitsOfVolume) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true),
      mUnitsOfVolume(unitsOfVolume),
      mMutex()
    {}

    /**
     * @brief Constructs an empty OHLCTimeSeries, reserving space for entries.
     * @param timeFrame The time interval between data points.
     * @param unitsOfVolume The units used for the volume data.
     * @param numElements A hint for reserving space in the internal vector for performance.
     */
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

    /**
     * @brief Move constructor.
     * @param rhs The OHLCTimeSeries object to move from.
     * @note Transfers ownership of internal data structures efficiently.
     * `rhs` is left in a valid but unspecified state (likely empty).
     * A new mutex is created for the moved-to object. Locks `rhs`.
     */
    OHLCTimeSeries(OHLCTimeSeries<Decimal>&& rhs) noexcept
      : mSortedTimeSeries(std::move(rhs.mSortedTimeSeries)),
	mDateToSequentialIndex(std::move(rhs.mDateToSequentialIndex)),
	mSequentialTimeSeries(std::move(rhs.mSequentialTimeSeries)),
	mTimeFrame(rhs.mTimeFrame),
	mMapAndArrayInSync(rhs.mMapAndArrayInSync),
	mUnitsOfVolume(rhs.mUnitsOfVolume),
	mMutex() // Fresh mutex
    {}

    /**
     * @brief Move assignment operator.
     * @param rhs The OHLCTimeSeries object to move assign from.
     * @return A reference to this object (`*this`).
     * @note Transfers ownership of internal data. Handles self-assignment.
     * `rhs` is left in a valid but unspecified state. Locks both `this` and `rhs`.
     * The mutex of `this` is not assigned.
     */
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

    /**
     * @brief Adds a new OHLC entry to the time series.
     * @param entry The OHLCTimeSeriesEntry to add. The entry is moved into the series.
     * @throws std::domain_error If the entry's time frame doesn't match the series'
     * time frame, or if an entry for the given timestamp
     * already exists in the sorted map.
     *
     * @note This operation locks the mutex. It invalidates the sequential (vector)
     * representation, setting `mMapAndArrayInSync` to false. Subsequent
     * operations requiring the vector view will trigger synchronization.
     */
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

    /**
     * @brief Retrieves an iterator pointing to the time series entry for a specific date.
     * @param timeSeriesDate The date (`boost::gregorian::date`) to find.
     * The search uses the date combined with the result of `getDefaultBarTime()`.
     * @return A `ConstTimeSeriesIterator` pointing to the entry if found in the sorted map,
     * otherwise returns `endSortedAccess()`.
     * @note This operation may lock the mutex briefly if a synchronization check is needed,
     * but primarily accesses the underlying map.
     */
    ConstTimeSeriesIterator getTimeSeriesEntry(const boost::gregorian::date& timeSeriesDate) const
    {
      ptime dateTime(timeSeriesDate, getDefaultBarTime());

      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }

      return mSortedTimeSeries.find(dateTime);
    }

    /**
     * @brief Retrieves an iterator pointing to the time series entry for a specific datetime.
     * @param timeSeriesDateTime The exact `boost::posix_time::ptime` to find.
     * @return A `ConstTimeSeriesIterator` pointing to the entry if found in the sorted map,
     * otherwise returns `endSortedAccess()`.
     * @note This operation may lock the mutex briefly if a synchronization check is needed,
     * but primarily accesses the underlying map.
     */
    ConstTimeSeriesIterator getTimeSeriesEntry(const ptime& timeSeriesDate) const
    {
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }
      
      return mSortedTimeSeries.find(timeSeriesDate);
    }

    /**
     * @brief Gets the time frame duration associated with this series.
     * @return The `TimeFrame::Duration` (e.g., Daily, Hourly).
     */
    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    /**
     * @brief Gets the total number of entries currently in the time series.
     * @return The number of entries as an `unsigned long`.
     * @note Locks the mutex to safely access the size of the underlying map.
     */
    unsigned long getNumEntries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mSortedTimeSeries.size();
    }

    /**
     * @brief Gets the units used for the volume data in this series.
     * @return The `TradingVolume::VolumeUnit` (e.g., Shares, Contracts).
     */
    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return mUnitsOfVolume;
    }

    std::vector<OHLCTimeSeriesEntry<Decimal>> getEntriesCopy() const
    {
      synchronize();
      return mSequentialTimeSeries; // safe copy
    }

    /**
     * @brief Gets a constant random access iterator pointing to the beginning of the sequential time series vector.
     * A random access iterator is need when accessing something like high from 5 bars ago.
     * @return A `ConstRandomAccessIterator` to the first element.
     * @note Ensures the internal vector is synchronized with the map before returning the iterator.
     * Acquires a lock if synchronization is necessary.
     */
    OHLCTimeSeries::ConstRandomAccessIterator beginRandomAccess() const
    {
      if (!mMapAndArrayInSync)
	{
	  boost::mutex::scoped_lock lock(mMutex);
	  if (!mMapAndArrayInSync)
	    synchronize_unlocked();
	}

      return mSequentialTimeSeries.begin();
    }

    /**
     * @brief Gets a constant random access iterator pointing past the end of the sequential time series vector.
     * @return A `ConstRandomAccessIterator` referring to the past-the-end element.
     * @note Ensures the internal vector is synchronized with the map before returning the iterator.
     * Acquires a lock if synchronization is necessary.
     */
    OHLCTimeSeries::ConstRandomAccessIterator endRandomAccess() const
    {
      if (!mMapAndArrayInSync)
	{
	  boost::mutex::scoped_lock lock(mMutex);
	  if (!mMapAndArrayInSync)
	    synchronize_unlocked();
	}

      return mSequentialTimeSeries.end();
    }

    /**
     * @brief Gets a random access iterator pointing to the entry for a specific date within the sequential vector.
     * @param d The `boost::gregorian::date` to find. The search uses the date combined with `getDefaultBarTime()`.
     * @return A `ConstRandomAccessIterator` pointing to the entry if found, otherwise `endRandomAccess()`.
     * @note Requires internal synchronization (locks if necessary) to ensure the date-to-index map
     * and sequential vector are up-to-date.
     */
    OHLCTimeSeries::ConstRandomAccessIterator getRandomAccessIterator(const boost::gregorian::date& d) const
    {
      ptime dateTime(d, getDefaultBarTime());

      if (!mMapAndArrayInSync)
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

    /**
     * @brief Gets a constant iterator pointing to the beginning of the time-sorted map.
     * @return A `ConstTimeSeriesIterator` to the earliest entry.
     * @note Accesses the map directly, does not require synchronization itself, but other
     * concurrent operations might affect the map.
     */
    OHLCTimeSeries::ConstTimeSeriesIterator beginSortedAccess() const
    {
      return mSortedTimeSeries.begin();
    }

    /**
     * @brief Gets a constant iterator pointing past the end of the time-sorted map.
     * @return A `ConstTimeSeriesIterator` referring to the past-the-end element.
     */
    OHLCTimeSeries::ConstTimeSeriesIterator endSortedAccess() const
    {
      return mSortedTimeSeries.end();
    }

     /**
     * @brief Gets the date part of the earliest entry in the time series.
     * @return The `boost::gregorian::date` of the first entry.
     * @throws std::domain_error If the time series is empty.
     * @note Accesses the sorted map. May lock briefly for synchronization status check.
     */
    const boost::gregorian::date getFirstDate() const
    {
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }
 
      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getFirstDate: no entries in time series");

      return mSortedTimeSeries.begin()->first.date();
    }

    /**
     * @brief Gets the full timestamp (`ptime`) of the earliest entry in the time series.
     * @return The `boost::posix_time::ptime` of the first entry.
     * @throws std::domain_error If the time series is empty.
     * @note Accesses the sorted map. May lock briefly for synchronization status check.
     */
    const ptime getFirstDateTime() const
    {
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }      

      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getFirstDateTime: no entries in time series");

      return mSortedTimeSeries.begin()->first;
    }

    /**
     * @brief Gets the date part of the latest entry in the time series.
     * @return The `boost::gregorian::date` of the last entry.
     * @throws std::domain_error If the time series is empty.
     * @note Accesses the sorted map using reverse iterators. May lock briefly for synchronization status check.
     */
    const boost::gregorian::date getLastDate() const
    {
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }

      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getLastDate: no entries in time series");

      return mSortedTimeSeries.rbegin()->first.date();
    }

     /**
     * @brief Gets the full timestamp (`ptime`) of the latest entry in the time series.
     * @return The `boost::posix_time::ptime` of the last entry.
     * @throws std::domain_error If the time series is empty.
     * @note Accesses the sorted map using reverse iterators. May lock briefly for synchronization status check.
     */
    const ptime getLastDateTime() const
    {
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }

      if (mSortedTimeSeries.empty())
	throw std::domain_error("OHLCTimeSeries:getLastDateTime: no entries in time series");

      return mSortedTimeSeries.rbegin()->first;
    }

    /**
     * @brief Retrieves a specific entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator` pointing to a position in the sequential vector.
     * @param offset The number of entries to look back from the iterator's position
     * (0 means the entry at `it`, 1 means the previous entry, etc.).
     * @return A constant reference to the `OHLCTimeSeriesEntry<Decimal>` at the calculated position (`it - offset`).
     * @throws TimeSeriesException If the iterator `it` is the `endRandomAccess` iterator,
     * or if the calculated position (`it - offset`) falls before
     * the beginning of the sequential vector.
     * @note Requires the sequential vector to be synchronized. Ensures synchronization internally (locks if needed).
     */
    const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const ConstRandomAccessIterator& it,
							    unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);
      OHLCTimeSeries::ConstRandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    /**
     * @brief Gets the date value of an entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator`.
     * @param offset The lookback offset (0 for current `it`, 1 for previous, etc.).
     * @return A constant reference to the `boost::gregorian::date` of the specified entry.
     * @throws TimeSeriesException If access is out of bounds (checked via `getTimeSeriesEntry`).
     * @note Requires synchronization.
     */
    const boost::gregorian::date&
    getDateValue (const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getDateValue();
    }

    /**
     * @brief Gets the Open value of an entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator`.
     * @param offset The lookback offset.
     * @return A constant reference to the Open `Decimal` value.
     * @throws TimeSeriesException If access is out of bounds.
     * @note Requires synchronization.
     */
    const Decimal& getOpenValue (const ConstRandomAccessIterator& it,
				 unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getOpenValue();
    }

    /**
     * @brief Gets the High value of an entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator`.
     * @param offset The lookback offset.
     * @return A constant reference to the High `Decimal` value.
     * @throws TimeSeriesException If access is out of bounds.
     * @note Requires synchronization.
     */
    const Decimal& getHighValue (const ConstRandomAccessIterator& it,
				 unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getHighValue();
    }

    /**
     * @brief Gets the Low value of an entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator`.
     * @param offset The lookback offset.
     * @return A constant reference to the Low `Decimal` value.
     * @throws TimeSeriesException If access is out of bounds.
     * @note Requires synchronization.
     */
    const Decimal& getLowValue (const ConstRandomAccessIterator& it,
				unsigned long offset) const
    {
      return (getTimeSeriesEntry (it, offset).getLowValue());
    }

    /**
     * @brief Gets the Close value of an entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator`.
     * @param offset The lookback offset.
     * @return A constant reference to the Close `Decimal` value.
     * @throws TimeSeriesException If access is out of bounds.
     * @note Requires synchronization.
     */
    const Decimal& getCloseValue (const ConstRandomAccessIterator& it,
				  unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getCloseValue();
    }

    /**
     * @brief Gets the Volume value of an entry relative to a random access iterator using an offset.
     * @param it A `ConstRandomAccessIterator`.
     * @param offset The lookback offset.
     * @return A constant reference to the Volume `Decimal` value.
     * @throws TimeSeriesException If access is out of bounds.
     * @note Requires synchronization.
     */
    const Decimal& getVolumeValue (const ConstRandomAccessIterator& it,
				   unsigned long offset) const
    {
      return getTimeSeriesEntry (it, offset).getVolumeValue();
    }

    /**
     * @brief Checks if an entry exists for a specific date (using `getDefaultBarTime()`).
     * @param date The `boost::gregorian::date` to check.
     * @return `true` if an entry exists for that date at the default bar time in the sorted map, `false` otherwise.
     * @note This function specifically checks the map for an entry at `ptime(date, getDefaultBarTime())`.
     * It may lock briefly to check synchronization status.
     */
    bool isDateFound(const boost::gregorian::date& date)
    {
      ptime dateTime(date, getDefaultBarTime());

      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
        }

      return (mSortedTimeSeries.find(dateTime) != mSortedTimeSeries.end());
    }

    /**
     * @brief Deletes all entries associated with a specific calendar date from the time series.
     * @param date The `boost::gregorian::date` for which to delete all entries.
     * @note This method iterates through the sorted map and removes *all* entries whose
     * `ptime` key has the specified date part, regardless of the time component.
     * This is useful for clearing all data for a day (e.g., all hourly bars).
     * Locks the mutex during the deletion process. Marks the sequential vector
     * representation as out-of-sync (`mMapAndArrayInSync = false`).
     */
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
    /**
     * @brief Ensures the sequential vector (`mSequentialTimeSeries`) and index map
     * (`mDateToSequentialIndex`) are synchronized with the sorted map
     * (`mSortedTimeSeries`).
     * @note Acquires a unique lock on `mMutex` if synchronization is needed (`!mMapAndArrayInSync`).
     * Calls `synchronize_unlocked()` internally if synchronization is performed.
     * This is called by methods needing the vector view.
     * @internal
     */
    void ensureSynchronized() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (!mMapAndArrayInSync)
        synchronize_unlocked();
    }

    /**
     * @brief Public interface to trigger synchronization if needed.
     * @note Acquires a unique lock on `mMutex`. If the series is already synchronized,
     * it returns quickly. Otherwise, it calls `synchronize_unlocked()`.
     * @internal
     */
    void synchronize() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mMapAndArrayInSync)
	return;

      synchronize_unlocked();
    }

     /**
     * @brief Performs the synchronization of the vector and index map from the sorted map.
     * @note This function *must* be called while holding a unique lock on `mMutex`.
     * It clears the vector and index map, then rebuilds them by iterating
     * through the sorted map. Sets `mMapAndArrayInSync` to true upon completion.
     * @internal
     */
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

    /**
     * @brief Validates offset access relative to a random access iterator.
     * @param it The `ConstRandomAccessIterator` used as the base.
     * @param offset The lookback offset.
     * @throws TimeSeriesException If the iterator `it` is `endRandomAccess()` or if
     * `it - offset` points before `beginRandomAccess()`.
     * @note Ensures synchronization before performing validation (locks if needed).
     * @internal
     */
    void ValidateVectorOffset (const ConstRandomAccessIterator& it,
			       unsigned long offset) const
    {      
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
	  if (!mMapAndArrayInSync)
	    synchronize_unlocked();
        }

      if (it == mSequentialTimeSeries.end())
        throw TimeSeriesException("Iterator is at end of time series");

      if ((it - offset) < mSequentialTimeSeries.begin())
        throw TimeSeriesException("Offset " + std::to_string(offset) + " outside bounds of time series");
    }

    /**
     * @brief Validates an offset against the total size of the sequential time series vector.
     * @param offset The offset value to check.
     * @throws TimeSeriesException If the `offset` is greater than the number of elements
     * in the sequential time series vector (i.e., `offset > size()`).
     * @note Ensures synchronization before performing validation (locks if needed). This check
     * seems intended to ensure `offset` can be used as a 1-based index or size,
     * as `offset == size()` would be a valid lookback from `end() - 1`.
     * @internal
     */
    void ValidateVectorOffset (unsigned long offset) const
    {
      if (!mMapAndArrayInSync)
        {
	  boost::mutex::scoped_lock lock(mMutex);
	  if (!mMapAndArrayInSync)
	    synchronize_unlocked();
        }

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

   /**
   * @brief Compares two OHLCTimeSeries for equality.
   * @tparam Decimal The numeric type used in the series.
   * @param lhs The left-hand side series.
   * @param rhs The right-hand side series.
   * @return true if both series have the same time frame, volume units, number of entries,
   * and all corresponding entries are equal (checked via `operator==` for OHLCTimeSeriesEntry),
   * false otherwise.
   * @note This comparison iterates through the sorted map view of both series. It acquires locks
   * briefly to get sizes and potentially iterators safely, depending on implementation details
   * of the iterator accessors.
   */
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

  /**
   * @brief Compares two OHLCTimeSeries for inequality.
   * @tparam Decimal The numeric type used in the series.
   * @param lhs The left-hand side series.
   * @param rhs The right-hand side series.
   * @return true if the series are not equal (based on the `operator==` definition), false otherwise.
   */
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
  
  /**
   * @brief Creates a new OHLCTimeSeries containing only the entries within a specified date range.
   * @tparam Decimal The numeric type of the series.
   * @param series The source OHLCTimeSeries to filter.
   * @param dates The `DateRange` specifying the start and end dates (inclusive).
   * @return A new `OHLCTimeSeries` containing copies of the entries from the source
   * series whose `ptime` keys fall within the specified date range (using
   * `getDefaultBarTime()` for boundary times).
   * @throws TimeSeriesException If the requested date range starts before the source series' first date.
   * @note The function iterates through the sorted map view of the source series. It locks
   * briefly on the source series to get start/end dates and potentially iterators.
   * The resulting series is a separate copy.
   */
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
