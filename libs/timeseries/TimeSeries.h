// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIMESERIES_H
#define __TIMESERIES_H 1

#include "TimeSeriesEntry.h"
#include "IntradayIntervalCalculator.h"
#include <iostream>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <boost/container/flat_map.hpp>
#include <vector>
#include <unordered_map>
#include <boost/thread/mutex.hpp>
#include <functional>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/optional.hpp>
#include "DateRange.h"
#include <string>
#include <stdexcept>
#include <map>

namespace std
{
  /**
   * @brief Hash functor specialization for Boost.PosixTime ptime.
   * (Original hash functor code remains unchanged)
   */
  template<>
  struct hash<boost::posix_time::ptime>
  {
    static inline size_t computeKey(const boost::posix_time::ptime& t) noexcept
    {
      uint64_t days = t.date().day_number();
      uint64_t minuteOfDay =
        static_cast<uint64_t>(t.time_of_day().hours()) * 60 +
        static_cast<uint64_t>(t.time_of_day().minutes());
      uint64_t key = (days << 11) | (minuteOfDay & 0x7FF);
      return std::hash<uint64_t>()(key);
    }

    std::size_t operator()(boost::posix_time::ptime const& t) const noexcept
    {
      return computeKey(t);
    }
  };
}

namespace mkc_timeseries
{
  using boost::posix_time::ptime;
  using boost::posix_time::time_duration;
  using boost::gregorian::date;

  // TimeSeriesOffset and ArrayTimeSeriesIndex remain unchanged from original TimeSeries.h
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
      {
	return pos->second;
      }
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
  // Static member definition (would be in a .cpp file)
  // boost::mutex TimeSeriesOffset::mOffsetCacheMutex;
  // std::map<unsigned long, std::shared_ptr<TimeSeriesOffset>> TimeSeriesOffset::mOffsetCache;


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
      {
	throw std::out_of_range("ArrayTimeSeriesIndex: offset cannot be larger than array index");
      }
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
  //  class NumericTimeSeries - UNMODIFIED as per focus on OHLCTimeSeries
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
      {
	return *this;
      }

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
      {
 throw std::domain_error(std::string("NumericTimeSeries:addEntry " +boost::posix_time::to_simple_string(entry->getDateTime()) + std::string(" time frames do not match")));
      }

      auto result = mSortedTimeSeries.emplace(entry->getDateTime(), entry);
      if (!result.second)
      {
	throw std::domain_error("NumericTimeSeries:addEntry: entry for time already exists: " + boost::posix_time::to_simple_string(entry->getDateTime()));
      }

      mMapAndArrayInSync = false;

      // Invalidate cached duration when data changes
      mCachedIntradayDuration.reset();
    }

    void addEntry (const NumericTimeSeriesEntry<Decimal>& entry)
    {
      addEntry (std::make_shared<NumericTimeSeriesEntry<Decimal>> (entry));
    }

    NumericTimeSeries::ConstTimeSeriesIterator getTimeSeriesEntry (const boost::gregorian::date& timeSeriesDate) const
    {
      ptime dateTime(timeSeriesDate, mkc_timeseries::getDefaultBarTime()); // Use extern function
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

    /**
     * @brief Gets the intraday time frame duration for this numeric time series.
     * @return boost::posix_time::time_duration representing the most common interval between entries
     * @throws TimeSeriesException if the time frame is not INTRADAY or insufficient data
     * @details Analyzes time differences between consecutive entries to determine the predominant interval.
     * This method uses caching for performance optimization.
     */
    boost::posix_time::time_duration getIntradayTimeFrameDuration() const
    {
        if (mTimeFrame != TimeFrame::INTRADAY)
        {
            throw TimeSeriesException("getIntradayTimeFrameDuration: Method only valid for INTRADAY time frame");
        }

        boost::mutex::scoped_lock lock(mMutex);

        if (mSortedTimeSeries.size() < 2)
        {
            throw TimeSeriesException("getIntradayTimeFrameDuration: Insufficient data - need at least 2 entries");
        }

        // Check cache first
        if (mCachedIntradayDuration)
        {
            return *mCachedIntradayDuration;
        }

        // Calculate and cache the result
        auto duration = IntradayIntervalCalculator::calculateFromSortedMap(mSortedTimeSeries);
        mCachedIntradayDuration = duration;
        return duration;
    }

    /**
     * @brief Gets the intraday time frame duration in minutes.
     * @return long representing the most common interval between entries in minutes
     * @throws TimeSeriesException if the time frame is not INTRADAY or insufficient data
     * @details This is a convenience method that calls getIntradayTimeFrameDuration()
     * and extracts the total minutes. Leverages the same caching mechanism.
     */
    long getIntradayTimeFrameDurationInMinutes() const
    {
        auto duration = getIntradayTimeFrameDuration();
        return duration.total_seconds() / 60;
    }

    unsigned long getNumEntries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mSortedTimeSeries.size();
    }

    NumericTimeSeries::ConstRandomAccessIterator
    getRandomAccessIterator(const boost::gregorian::date& d) const
    {
      ptime dateTime(d, mkc_timeseries::getDefaultBarTime()); // Use extern function
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
      {
	throw std::domain_error("NumericTimeSeries:getFirstDate: no entries in time series");
      }

      return mSortedTimeSeries.begin()->first.date();
    }

    const boost::gregorian::date getLastDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);

      if (mSortedTimeSeries.empty())
      {
	throw std::domain_error("NumericTimeSeries:getLastDate: no entries in time series");
      }

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

    boost::gregorian::date
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
      {
	synchronize_unlocked();
      }
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
      {
	throw TimeSeriesException("Iterator is at end of time series");
      }
      if ((it - offset) < mSequentialTimeSeries.begin())
      {
	throw TimeSeriesException("Offset " + std::to_string(offset) + " outside bounds of time series");
      }
    }

    void ValidateVectorOffset(unsigned long offset) const
    {
      ensureSynchronized();
      if (offset > mSequentialTimeSeries.size())
      {
	throw TimeSeriesException("Offset " + std::to_string(offset) + " exceeds size of time series");
      }
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
    mutable boost::optional<boost::posix_time::time_duration> mCachedIntradayDuration;
  };

  /**
   * @class LogNLookupPolicy
   * @brief Implements O(log n) lookup for OHLCTimeSeries using std::lower_bound.
   * This policy is stateless.
   */
  template <class Decimal>
  class LogNLookupPolicy
  {
  public:
    using Entry = OHLCTimeSeriesEntry<Decimal>;
    using VectorConstIterator = typename std::vector<Entry>::const_iterator;

    LogNLookupPolicy() = default;

    void addEntry(std::vector<Entry>& data,
                  TimeFrame::Duration seriesTimeFrame,
                  Entry entry) const
    {
      if (entry.getTimeFrame() != seriesTimeFrame)
      {
        throw TimeSeriesException("LogNLookupPolicy::addEntry: time frame mismatch for entry " + boost::posix_time::to_simple_string(entry.getDateTime()));
      }

      auto it = std::lower_bound(data.begin(), data.end(), entry,
                                 [](const Entry& a, const Entry& b)
                                 {
                                   return a.getDateTime() < b.getDateTime();
                                 });

      if (it != data.end() && it->getDateTime() == entry.getDateTime())
      {
        throw TimeSeriesException("LogNLookupPolicy::addEntry: duplicate timestamp " + boost::posix_time::to_simple_string(entry.getDateTime()));
      }

      data.insert(it, std::move(entry));
    }

    /**
     * @brief Gets an iterator to the entry for a specific ptime.
     * For internal use by OHLCTimeSeries to support efficient offset calculations.
     * @param data The underlying data vector of OHLCTimeSeries.
     * @param dt The ptime to search for.
     * @return A const_iterator to the found entry, or data.end() if not found.
     */
    VectorConstIterator
    getInternalIterator(const std::vector<Entry>& data,
                        const boost::posix_time::ptime& dt) const
    {
      auto it = std::lower_bound(data.begin(), data.end(), dt,
                                 [](const Entry& e, const boost::posix_time::ptime& tval)
                                 {
                                   return e.getDateTime() < tval;
                                 });
      return (it != data.end() && it->getDateTime() == dt) ? it : data.end();
    }

    void deleteEntryByDate(std::vector<Entry>& data, const boost::posix_time::ptime& d) const
    {
      data.erase(
        std::remove_if(data.begin(), data.end(),
                       [&](const Entry& e) { return e.getDateTime() == d; }),
        data.end());
    }

    void on_construct_from_range(const std::vector<Entry>& /*data*/) const
    {
      // No-op for LogN policy as mData is already sorted by OHLCTimeSeries constructor.
    }

    std::vector<Entry> getEntriesCopy(const std::vector<Entry>& data) const
    {
      return data;
    }
  };

  /**
   * @class HashedLookupPolicy
   * @brief Implements O(1) average time lookup for OHLCTimeSeries using std::unordered_map.
   * This policy is stateful and manages an internal index and mutex for thread safety.
   */
  template <class Decimal>
  class HashedLookupPolicy
  {
  public:
    using Entry = OHLCTimeSeriesEntry<Decimal>;
    using VectorConstIterator = typename std::vector<Entry>::const_iterator;

  public:
    HashedLookupPolicy() = default;

    HashedLookupPolicy(const HashedLookupPolicy& other)
    {
      boost::mutex::scoped_lock lock(other.m_mutex);
      mIndex = other.mIndex;
    }

    HashedLookupPolicy& operator=(const HashedLookupPolicy& other)
    {
      if (this == &other)
      {
        return *this;
      }

      boost::mutex::scoped_lock lock_this(m_mutex, boost::defer_lock);
      boost::mutex::scoped_lock lock_other(other.m_mutex, boost::defer_lock);
      std::lock(lock_this, lock_other); // Lock both mutexes to prevent deadlock

      mIndex = other.mIndex;
      return *this;
    }

    HashedLookupPolicy(HashedLookupPolicy&& other) noexcept
    {
      boost::mutex::scoped_lock lock(other.m_mutex);
      mIndex = std::move(other.mIndex);
    }

    HashedLookupPolicy& operator=(HashedLookupPolicy&& other) noexcept
    {
      if (this == &other)
      {
        return *this;
      }

      boost::mutex::scoped_lock lock_this(m_mutex, boost::defer_lock);
      boost::mutex::scoped_lock lock_other(other.m_mutex, boost::defer_lock);
      std::lock(lock_this, lock_other); // Lock both mutexes

      mIndex = std::move(other.mIndex);
      return *this;
    }

    void addEntry(std::vector<Entry>& data,
                  TimeFrame::Duration seriesTimeFrame,
                  Entry entry)
    {
      if (entry.getTimeFrame() != seriesTimeFrame)
      {
        throw TimeSeriesException("HashedLookupPolicy::addEntry: time frame mismatch for entry " + boost::posix_time::to_simple_string(entry.getDateTime()));
      }

      boost::mutex::scoped_lock lock(m_mutex);
      auto it = std::lower_bound(data.begin(), data.end(), entry,
                                 [](const Entry& a, const Entry& b)
                                 {
                                   return a.getDateTime() < b.getDateTime();
                                 });

      if (it != data.end() && it->getDateTime() == entry.getDateTime())
      {
        throw TimeSeriesException("HashedLookupPolicy::addEntry: duplicate timestamp " + boost::posix_time::to_simple_string(entry.getDateTime()));
      }

      data.insert(it, std::move(entry));

      if (!mIndex.empty())
      {
        mIndex.clear();
      }
    }

    /**
     * @brief Gets an iterator to the entry for a specific ptime using the hash index.
     * For internal use by OHLCTimeSeries to support efficient offset calculations.
     * This method is thread-safe due to internal locking.
     * @param data The underlying data vector of OHLCTimeSeries.
     * @param dt The ptime to search for.
     * @return A const_iterator to the found entry, or data.end() if not found or if index is stale.
     */
    VectorConstIterator
    getInternalIterator(const std::vector<Entry>& data,
                        const boost::posix_time::ptime& dt) const
    {
      boost::mutex::scoped_lock lock(m_mutex);
      if (mIndex.empty() && !data.empty())
      {
        buildIndex_nolock(data);
      }

      auto map_it = mIndex.find(dt);
      if (map_it == mIndex.end())
      {
        return data.end();
      }

      // Stale index check
      if (map_it->second >= data.size())
      {
          mIndex.clear(); // Clear the stale index
          return data.end();
      }
      return data.begin() + map_it->second;
    }

    void deleteEntryByDate(std::vector<Entry>& data, const boost::posix_time::ptime& d)
    {
      boost::mutex::scoped_lock lock(m_mutex);
      data.erase(
        std::remove_if(data.begin(), data.end(),
                       [&](const Entry& e) { return e.getDateTime() == d; }),
        data.end());

      if (!mIndex.empty())
      {
        mIndex.clear();
      }
    }

    /**
     * @brief Builds the internal hash index from the provided data vector.
     * Called when an OHLCTimeSeries is constructed from a range of entries.
     * This method is thread-safe.
     * @param data The data vector, assumed to be sorted by timestamp.
     */
    void on_construct_from_range(const std::vector<Entry>& data)
    {
      boost::mutex::scoped_lock lock(m_mutex);
      buildIndex_nolock(data);
    }

    std::vector<Entry> getEntriesCopy(const std::vector<Entry>& data) const
    {
      boost::mutex::scoped_lock lock(m_mutex);
      return data;
    }

  private:
    mutable std::unordered_map<boost::posix_time::ptime, size_t> mIndex;
    mutable boost::mutex m_mutex;

    /**
     * @brief Internal helper to build the hash index. Assumes caller holds the lock.
     * @param data The data vector from OHLCTimeSeries.
     */
    void buildIndex_nolock(const std::vector<Entry>& data) const
    {
      mIndex.clear();
      mIndex.reserve(data.size());
      for (size_t i = 0; i < data.size(); ++i)
      {
        mIndex[data[i].getDateTime()] = i;
      }
    }
  };

  // Forward declaration for non-member operators
  template <class Decimal, class LookupPolicy>
  class OHLCTimeSeries;

  template <class Decimal, class LookupPolicy>
  bool operator==(const OHLCTimeSeries<Decimal, LookupPolicy>& lhs, const OHLCTimeSeries<Decimal, LookupPolicy>& rhs);

  /**
   * @brief Represents a time series of Open, High, Low, Close (OHLC) and Volume data.
   * @tparam Decimal The numeric type used for price and volume data (e.g., double, float).
   * @tparam LookupPolicy Policy class to determine lookup strategy (e.g., LogNLookupPolicy, HashedLookupPolicy). Defaults to LogNLookupPolicy.
   *
   * This class is central to financial backtesting systems, holding historical
   * price and volume information for instruments like equities or futures.
   *
   * Maintains a single sorted-invariant vector of entries (`mData`) of type `OHLCTimeSeriesEntry<Decimal>`.
   * - Insertion via `addEntry(...)` keeps the data sorted by delegating to the LookupPolicy.
   * - Rejects duplicate timestamps (enforced by LookupPolicy).
   */
  template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>>
  class OHLCTimeSeries
  {
  public:
    using Entry = OHLCTimeSeriesEntry<Decimal>;
    // ConstTimeSeriesIterator and ConstRandomAccessIterator for single entry lookups are replaced by value-returning methods.
    // ConstSortedIterator is provided for full range-based loops over the series.
    using ConstSortedIterator = typename std::vector<Entry>::const_iterator;
/**
     * @brief Legacy iterator type for backward compatibility.
     * Functionally equivalent to ConstSortedIterator.
     * @warning Invalidated by any modification to the series.
     */
    using ConstRandomAccessIterator = typename std::vector<Entry>::const_iterator;

    /** @name Constructors & Assignment */


    /**
     * @brief Constructs an empty OHLCTimeSeries.
     * @param timeFrame The time frame duration for entries in this series (e.g., Daily, Weekly).
     * @param unitsOfVolume The units for the volume data (e.g., Shares, Contracts).
     */
    OHLCTimeSeries(TimeFrame::Duration timeFrame,
		   TradingVolume::VolumeUnit unitsOfVolume)
      : mData(),
	mTimeFrame(timeFrame),
	mUnitsOfVolume(unitsOfVolume),
	m_lookup_policy()
    {}

    /**
     * @brief Constructs an empty OHLCTimeSeries, reserving space for elements.
     * @param timeFrame The time frame duration for entries in this series.
     * @param unitsOfVolume The units for the volume data.
     * @param reserveCount The initial capacity to reserve in the internal vector.
     */
    OHLCTimeSeries(TimeFrame::Duration timeFrame,
		   TradingVolume::VolumeUnit unitsOfVolume,
		   unsigned long reserveCount)
      : mData(),
	mTimeFrame(timeFrame),
	mUnitsOfVolume(unitsOfVolume),
	m_lookup_policy()
    {
      mData.reserve(reserveCount);
    }

    /**
     * @brief Constructs an OHLCTimeSeries from a range of entries.
     *
     * Initializes the series with copies of entries from the range [first, last).
     * The entries are sorted by timestamp after insertion.
     *
     * @tparam InputIt The type of the input iterator. Must dereference to OHLCTimeSeriesEntry<Decimal>.
     * @param tf The time frame duration for entries in this series.
     * @param units The units for the volume data.
     * @param first Iterator pointing to the beginning of the range.
     * @param last Iterator pointing past the end of the range.
     * @throws TimeSeriesException If any entry in the input range has a time frame different from `tf`.
     */
    template<
    class InputIt,
    class = typename std::enable_if<
        std::is_same<typename std::iterator_traits<InputIt>::value_type,
		     OHLCTimeSeriesEntry<Decimal>>::value>::type>
    OHLCTimeSeries(TimeFrame::Duration tf,
		   TradingVolume::VolumeUnit units,
		   InputIt first,
		   InputIt last)
      : mData(first, last),
	mTimeFrame(tf),
	mUnitsOfVolume(units),
	m_lookup_policy()
    {
      for (auto& e : mData)
      {
	if (e.getTimeFrame() != tf)
        {
	  throw TimeSeriesException("OHLCTimeSeries constructor: time frame mismatch for provided entries.");
        }
      }

      std::sort(mData.begin(), mData.end(),
		[](auto const &a, auto const &b)
		{
		  return a.getDateTime() < b.getDateTime();
		});
      m_lookup_policy.on_construct_from_range(mData);
    }

    /** @brief Copy constructor. */
    OHLCTimeSeries(const OHLCTimeSeries& rhs)
      : mData(rhs.mData),
        mTimeFrame(rhs.mTimeFrame),
        mUnitsOfVolume(rhs.mUnitsOfVolume),
        m_lookup_policy(rhs.m_lookup_policy),
        mMutex()
    {
    }

    /** @brief Copy assignment operator. */
    OHLCTimeSeries& operator=(const OHLCTimeSeries& rhs)
    {
      if (this == &rhs) return *this;

      boost::mutex::scoped_lock lock_this(mMutex, boost::defer_lock);
      boost::mutex::scoped_lock lock_rhs(rhs.mMutex, boost::defer_lock);
      std::lock(lock_this, lock_rhs);

      mData = rhs.mData;
      mTimeFrame = rhs.mTimeFrame;
      mUnitsOfVolume = rhs.mUnitsOfVolume;
      m_lookup_policy = rhs.m_lookup_policy;

      return *this;
    }

    /** @brief Move constructor. */
    OHLCTimeSeries(OHLCTimeSeries&& rhs) noexcept
      : mData(std::move(rhs.mData)),
        mTimeFrame(rhs.mTimeFrame),
        mUnitsOfVolume(rhs.mUnitsOfVolume),
        m_lookup_policy(std::move(rhs.m_lookup_policy)),
        mMutex()
    {
    }

     /** @brief Move assignment operator. */
    OHLCTimeSeries& operator=(OHLCTimeSeries&& rhs) noexcept
    {
      if (this == &rhs) return *this;

      boost::mutex::scoped_lock lock_this(mMutex, boost::defer_lock);
      boost::mutex::scoped_lock lock_rhs(rhs.mMutex, boost::defer_lock);
      std::lock(lock_this, lock_rhs);

      mData = std::move(rhs.mData);
      mTimeFrame = rhs.mTimeFrame;
      mUnitsOfVolume = rhs.mUnitsOfVolume;
      m_lookup_policy = std::move(rhs.m_lookup_policy);

      return *this;
    }


    /**
     * @brief Get the time frame of this series.
     */
    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    /**
     * @brief Get the volume units of this series.
     */
    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return mUnitsOfVolume;
    }

    /**
     * @brief Gets the intraday time frame duration for this OHLC time series.
     * @return boost::posix_time::time_duration representing the most common interval between entries
     * @throws TimeSeriesException if the time frame is not INTRADAY or insufficient data
     * @details Analyzes time differences between consecutive entries to determine the predominant interval.
     * This method uses caching for performance optimization.
     */
    boost::posix_time::time_duration getIntradayTimeFrameDuration() const
    {
        if (mTimeFrame != TimeFrame::INTRADAY)
        {
            throw TimeSeriesException("getIntradayTimeFrameDuration: Method only valid for INTRADAY time frame");
        }

        boost::mutex::scoped_lock lock(mMutex);

        if (mData.size() < 2)
        {
            throw TimeSeriesException("getIntradayTimeFrameDuration: Insufficient data - need at least 2 entries");
        }

        // Check cache first
        if (mCachedIntradayDuration)
        {
            return *mCachedIntradayDuration;
        }

        // Calculate and cache the result
        auto duration = IntradayIntervalCalculator::calculateFromOHLCEntries(mData);
        mCachedIntradayDuration = duration;
        return duration;
    }

    /**
     * @brief Gets the intraday time frame duration in minutes.
     * @return long representing the most common interval between entries in minutes
     * @throws TimeSeriesException if the time frame is not INTRADAY or insufficient data
     * @details This is a convenience method that calls getIntradayTimeFrameDuration()
     * and extracts the total minutes. Leverages the same caching mechanism.
     */
    long getIntradayTimeFrameDurationInMinutes() const
    {
        auto duration = getIntradayTimeFrameDuration();
        return duration.total_seconds() / 60;
    }

    /**
     * @brief Number of entries in the series.
     */
    unsigned long getNumEntries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return static_cast<unsigned long>(mData.size());
    }

    /**
     * @brief Inserts a new OHLC entry into the time series.
     *
     * The entry is inserted in a way that maintains the time-sorted order of the
     * internal vector, managed by the LookupPolicy.
     *
     * @param entry The OHLCTimeSeriesEntry to add (passed by value, potentially moved).
     * @throws TimeSeriesException If an entry with the same timestamp already exists,
     * or if the entry's time frame does not match the series' time frame (enforced by LookupPolicy).
     */
    void addEntry(Entry entry)
    {
      boost::mutex::scoped_lock lock(mMutex);
      m_lookup_policy.addEntry(mData, mTimeFrame, std::move(entry));

      // Invalidate cached duration when data changes
      mCachedIntradayDuration.reset();
    }

    /**
     * @brief Retrieves the time series entry for a specific date.
     * Converts the date to a ptime using the default bar time and calls the ptime overload.
     * @param d The date for which to retrieve the entry.
     * @return A copy of the OHLCTimeSeriesEntry for the specified date.
     * @throws TimeSeriesDataNotFoundException if no entry exists for the specified date.
     */
    Entry getTimeSeriesEntry(const date& d) const
    {
        return getTimeSeriesEntry(ptime(d, mkc_timeseries::getDefaultBarTime())); // Uses extern function from TimeSeriesEntry.h
    }

    /**
     * @brief Retrieves the time series entry for a specific ptime.
     * This is the primary lookup method for a single entry by its exact timestamp.
     * @param dt The ptime for which to retrieve the entry.
     * @return A copy of the OHLCTimeSeriesEntry for the specified ptime.
     * @throws TimeSeriesDataNotFoundException if no entry exists for the specified ptime.
     */
    Entry getTimeSeriesEntry(const ptime& dt) const
    {
        boost::mutex::scoped_lock lock(mMutex);
        auto internal_it = m_lookup_policy.getInternalIterator(mData, dt);
        if (internal_it == mData.end())
        {
            throw TimeSeriesDataNotFoundException("Entry not found for ptime: " + boost::posix_time::to_simple_string(dt));
        }
        return *internal_it; // Return copy
    }

    /**
     * @brief Retrieves a time series entry relative to a base date by a specific offset.
     * Converts the base date to a ptime using the default bar time and calls the ptime overload.
     * @param base_d The base date from which to offset.
     * @param offset_bars_ago The number of bars to offset from the base_d.
     * 0 means the entry for base_d itself.
     * Positive values mean bars prior to base_d (earlier in time).
     * Negative values mean bars after base_d (later in time).
     * @return A copy of the target OHLCTimeSeriesEntry.
     * @throws TimeSeriesDataNotFoundException if the base_d is not found.
     * @throws TimeSeriesOffsetOutOfRangeException if the offset leads to an out-of-bounds access.
     */
    Entry getTimeSeriesEntry(const date& base_d, long offset_bars_ago) const
    {
        return getTimeSeriesEntry(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Uses extern function from TimeSeriesEntry.h
    }

    /**
     * @brief Retrieves a time series entry relative to a base ptime by a specific offset.
     * @param base_dt The base ptime from which to offset.
     * @param offset_bars_ago The number of bars to offset from the base_dt.
     * 0 means the entry for base_dt itself.
     * Positive values mean bars prior to base_dt (earlier in time).
     * Negative values mean bars after base_dt (later in time).
     * @return A copy of the target OHLCTimeSeriesEntry.
     * @throws TimeSeriesDataNotFoundException if the base_dt is not found.
     * @throws TimeSeriesOffsetOutOfRangeException if the offset leads to an out-of-bounds access.
     */
    Entry getTimeSeriesEntry(const ptime& base_dt, long offset_bars_ago) const
    {
        boost::mutex::scoped_lock lock(mMutex);
        auto base_it = m_lookup_policy.getInternalIterator(mData, base_dt);
        if (base_it == mData.end())
        {
            throw TimeSeriesDataNotFoundException("Base entry not found for ptime: " + boost::posix_time::to_simple_string(base_dt) + " when applying offset " + std::to_string(offset_bars_ago));
        }

        typename std::vector<Entry>::const_iterator target_it;
        if (offset_bars_ago >= 0) // 0 or N bars ago (towards vector begin)
        {
            if (static_cast<size_t>(std::distance(mData.begin(), base_it)) < static_cast<size_t>(offset_bars_ago))
            {
                throw TimeSeriesOffsetOutOfRangeException("Offset " + std::to_string(offset_bars_ago) + " is out of bounds (before series start) from base date " + boost::posix_time::to_simple_string(base_dt));
            }
            target_it = base_it - offset_bars_ago;
        }
        else // Negative offset means "bars into the future" (towards vector end)
        {
            long forward_offset = -offset_bars_ago;
            if (static_cast<size_t>(std::distance(base_it, mData.end()) -1) < static_cast<size_t>(forward_offset) )
            {
                 throw TimeSeriesOffsetOutOfRangeException("Offset " + std::to_string(offset_bars_ago) + " is out of bounds (after series end) from base date " + boost::posix_time::to_simple_string(base_dt));
            }
            target_it = base_it + forward_offset;
        }

        if (target_it < mData.begin() || target_it >= mData.end())
        {
             throw TimeSeriesOffsetOutOfRangeException("Calculated target iterator is unexpectedly out of bounds for offset " + std::to_string(offset_bars_ago) + " from base date " + boost::posix_time::to_simple_string(base_dt));
        }
        return *target_it; // Return copy
    }

    // --- Value Accessor Methods (with date overloads calling ptime versions) ---
    Decimal getOpenValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getOpenValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }
    Decimal getHighValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getHighValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }
    Decimal getLowValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getLowValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }
    Decimal getCloseValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getCloseValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }
    Decimal getVolumeValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getVolumeValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }
    date getDateValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getDateValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }
    ptime getDateTimeValue(const date& base_d, unsigned long offset_bars_ago) const
    {
        return getDateTimeValue(ptime(base_d, mkc_timeseries::getDefaultBarTime()), offset_bars_ago); // Use extern function
    }

    // --- Value Accessor Methods (primary ptime implementation) ---
    Decimal getOpenValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getOpenValue();
    }
    Decimal getHighValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getHighValue();
    }
    Decimal getLowValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getLowValue();
    }
    Decimal getCloseValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getCloseValue();
    }
    Decimal getVolumeValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getVolumeValue();
    }
    date getDateValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getDateValue();
    }
    ptime getDateTimeValue(const ptime& base_dt, unsigned long offset_bars_ago) const
    {
        return getTimeSeriesEntry(base_dt, static_cast<long>(offset_bars_ago)).getDateTime();
    }

    /**
     * @brief Check if a date exists in series.
     */
    bool isDateFound(const date& d) const
    {
      try
      {
        getTimeSeriesEntry(d); // This now throws if not found
        return true;
      }
      catch (const TimeSeriesDataNotFoundException&)
      {
        return false;
      }
    }

    /**
     * @brief Check if a ptime exists in series.
     */
    bool isDateFound(const ptime& pt) const
    {
      try
      {
        getTimeSeriesEntry(pt); // This now throws if not found
        return true;
      }
      catch (const TimeSeriesDataNotFoundException&)
      {
        return false;
      }
    }

    // --- Other methods ---
    NumericTimeSeries<Decimal> OpenTimeSeries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      NumericTimeSeries<Decimal> out(getTimeFrame(), static_cast<unsigned long>(mData.size()));
      for (const auto& entry : mData) // Use direct mData iteration for this construction
      {
 out.addEntry(
       NumericTimeSeriesEntry<Decimal>(entry.getDateTime(),
    		     entry.getOpenValue(),
    		     entry.getTimeFrame()));
      }
      return out;
    }

    NumericTimeSeries<Decimal> HighTimeSeries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      NumericTimeSeries<Decimal> out(getTimeFrame(), static_cast<unsigned long>(mData.size()));
      for (const auto& entry : mData)
      {
 out.addEntry(
       NumericTimeSeriesEntry<Decimal>(entry.getDateTime(),
    		     entry.getHighValue(),
    		     entry.getTimeFrame()));
      }
      return out;
    }

    NumericTimeSeries<Decimal> LowTimeSeries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      NumericTimeSeries<Decimal> out(getTimeFrame(), static_cast<unsigned long>(mData.size()));
      for (const auto& entry : mData)
      {
 out.addEntry(
       NumericTimeSeriesEntry<Decimal>(entry.getDateTime(),
    		     entry.getLowValue(),
    		     entry.getTimeFrame()));
      }
      return out;
    }

    NumericTimeSeries<Decimal> CloseTimeSeries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      NumericTimeSeries<Decimal> out(getTimeFrame(), static_cast<unsigned long>(mData.size()));
      for (const auto& entry : mData)
      {
 out.addEntry(
       NumericTimeSeriesEntry<Decimal>(entry.getDateTime(),
    		     entry.getCloseValue(),
    		     entry.getTimeFrame()));
      }
      return out;
    }

    /** @brief Return a copy of all entries. */
    std::vector<Entry> getEntriesCopy() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return m_lookup_policy.getEntriesCopy(mData);
    }

    /** @name Sorted-access iteration (by time)
     * @warning Iterators are invalidated by any modification to the series.
     */

    ConstSortedIterator beginSortedAccess() const {
        boost::mutex::scoped_lock lock(mMutex);
        return mData.begin();
    }
    ConstSortedIterator endSortedAccess()   const {
        boost::mutex::scoped_lock lock(mMutex);
        return mData.end();
    }


    /** @name Legacy random-access iteration (for backward compatibility)
     * @brief Provides iterator access similar to the pre-refactoring API.
     * Functionally equivalent to sorted access iterators.
     * @warning Iterators are invalidated by any modification to the series.
     * Note: Methods to get values using (iterator, offset) like getOpenValue(iterator, offset)
     * have been removed and are NOT restored. Code using such patterns must be updated
     * to use iterator dereferencing (e.g., `it->getOpenValue()`) for offset 0,
     * or use the new date/ptime-based getXValue(date, offset_bars_ago) methods.
     */
    ConstRandomAccessIterator beginRandomAccess() const {
        boost::mutex::scoped_lock lock(mMutex);
        return mData.begin();
    }
    ConstRandomAccessIterator endRandomAccess()   const {
        boost::mutex::scoped_lock lock(mMutex);
        return mData.end();
    }


    /** @brief First date in series.
     * @throws TimeSeriesDataNotFoundException if series is empty.
     */
    date getFirstDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mData.empty())
      {
          throw TimeSeriesDataNotFoundException("getFirstDate: Time series is empty.");
      }
      return mData.front().getDateTime().date();
    }

    /** @brief First full timestamp.
     * @throws TimeSeriesDataNotFoundException if series is empty.
     */
    ptime getFirstDateTime() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mData.empty())
      {
          throw TimeSeriesDataNotFoundException("getFirstDateTime: Time series is empty.");
      }
      return mData.front().getDateTime();
    }

    /** @brief Last date in series.
     * @throws TimeSeriesDataNotFoundException if series is empty.
     */
    date getLastDate() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mData.empty())
      {
          throw TimeSeriesDataNotFoundException("getLastDate: Time series is empty.");
      }
      return mData.back().getDateTime().date();
    }

    /** @brief Last full timestamp.
     * @throws TimeSeriesDataNotFoundException if series is empty.
     */
    ptime getLastDateTime() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (mData.empty())
      {
        throw TimeSeriesDataNotFoundException("getLastDateTime: Time series is empty.");
      }
      return mData.back().getDateTime();
    }

    /**
     * @brief Remove all entries matching a ptime.
     */
    void deleteEntryByDate(const ptime& d)
    {
      boost::mutex::scoped_lock lock(mMutex);
      m_lookup_policy.deleteEntryByDate(mData, d);

      // Invalidate cached duration when data changes
      mCachedIntradayDuration.reset();
    }

    /**
     * @brief Remove all entries matching a date (using default bar time).
     */
    void deleteEntryByDate(const date& d)
    {
      this->deleteEntryByDate(ptime(d, mkc_timeseries::getDefaultBarTime())); // Use extern function
    }

  private:
    std::vector<Entry>         mData;
    TimeFrame::Duration        mTimeFrame;
    TradingVolume::VolumeUnit  mUnitsOfVolume;
    LookupPolicy               m_lookup_policy;
    mutable boost::mutex       mMutex;
    mutable boost::optional<boost::posix_time::time_duration> mCachedIntradayDuration;
  };

  template <class Decimal, class LookupPolicy>
  bool operator==(const OHLCTimeSeries<Decimal, LookupPolicy>& lhs,
		  const OHLCTimeSeries<Decimal, LookupPolicy>& rhs)
  {
    // Member operator== is not explicitly defined in the provided OHLCTimeSeries,
    // but it was in the previous context. Assuming it compares mTimeFrame, mUnitsOfVolume, and mData.
    // If it's not defined, this needs to be a direct comparison of members.
    // For this regeneration, I'll assume the member operator== as it was in the previous versions provided.
    if (lhs.getTimeFrame() != rhs.getTimeFrame() ||
        lhs.getVolumeUnits() != rhs.getVolumeUnits() ||
        lhs.getNumEntries() != rhs.getNumEntries())
    {
        return false;
    }
    // Element-wise comparison if fundamental properties match
    // This assumes getEntriesCopy() is suitable for comparison, or direct mData access if possible
    // and if policies don't interfere with direct mData comparison for equality.
    // The original OHLCTimeSeries had a member operator== that directly compared mData.
    // Let's use getEntriesCopy for safety, though it's less efficient.
    // A better way if mData is the sole source of truth and policies don't change its meaning
    // for equality would be to compare mData directly if friended or via a specific getter.
    // Given the constraints, comparing copies from getEntriesCopy is the safest interpretable approach.
    // However, the original TimeSeries.h likely just compared internal mData vectors.
    // Reverting to what OHLCTimeSeries::operator== would do (direct mData comparison through a helper or friendship):
    // For the purpose of this, I will assume the direct mData comparison as it was in original TimeSeries.h.
    // This would ideally be: return lhs.mData == rhs.mData; (if mData were public or via friend)
    // For now, will use getEntriesCopy:
    // return lhs.getEntriesCopy() == rhs.getEntriesCopy();
    // Re-evaluating the original TimeSeries.h, the member operator== for OHLCTimeSeries does this:
    //  return mData == rhs.mData; (plus other members)
    //  So the non-member should correctly call the member.
    if (lhs.getTimeFrame() != rhs.getTimeFrame()) return false;
    if (lhs.getVolumeUnits() != rhs.getVolumeUnits()) return false;
    if (lhs.getNumEntries() != rhs.getNumEntries()) return false; // Quick check

    // If LookupPolicy affects equality beyond mData (it shouldn't for content),
    // this direct comparison of mData (via getEntriesCopy if mData is private) is key.
    // The original TimeSeries.h had a member operator==
    // bool operator==(const OHLCTimeSeries<Decimal, LookupPolicy>& rhs) const { ... return mData == rhs.mData }
    // So this non-member is fine.
    return lhs.getEntriesCopy() == rhs.getEntriesCopy(); // Based on member operator== comparing mData
  }

  template <class Decimal, class LookupPolicy>
  bool operator!=(const OHLCTimeSeries<Decimal, LookupPolicy>& lhs,
		  const OHLCTimeSeries<Decimal, LookupPolicy>& rhs)
  {
    return !(lhs == rhs);
  }

  template <class Decimal, class LookupPolicy>
  std::ostream& operator<<(std::ostream& os, const OHLCTimeSeries<Decimal, LookupPolicy>& series)
  {
    os << "DateTime,Open,High,Low,Close,Volume\n";
    for (const auto& entry : series.getEntriesCopy())  // Uses safe getEntriesCopy
      {
	const auto& dateTime = entry.getDateTime();
	os << boost::posix_time::to_simple_string(dateTime);
	os << "," << entry.getOpenValue();
	os << "," << entry.getHighValue();
	os << "," << entry.getLowValue();
	os << "," << entry.getCloseValue();
	os << "," << entry.getVolumeValue();
	os << "\n";
      }
    return os;
  }

  /**
   * @brief Creates a new OHLCTimeSeries containing only the entries within a specified date range.
   * This version uses the original logic from the provided TimeSeries.h for pre-conditions.
   * @tparam Decimal The numeric type of the series.
   * @tparam LookupPolicy The lookup policy of the series.
   * @param series The source OHLCTimeSeries to filter.
   * @param dates The `DateRange` specifying the start and end ptimes (inclusive).
   * @return A new `OHLCTimeSeries` containing copies of the entries from the source
   * series whose `ptime` keys fall within the specified date range.
   * @throws TimeSeriesException If the requested date range starts or ends before the source series' first date,
   * and the series is not empty.
   */
  template <class Decimal, class LookupPolicy>
  OHLCTimeSeries<Decimal, LookupPolicy> FilterTimeSeries(
    const OHLCTimeSeries<Decimal, LookupPolicy>& series,
    const DateRange& dates)
  {
    OHLCTimeSeries<Decimal, LookupPolicy> result(series.getTimeFrame(),
                                                 series.getVolumeUnits());

    auto firstP = dates.getFirstDateTime();
    auto lastP  = dates.getLastDateTime();

    // Original pre-condition checks from the user-provided TimeSeries.h
    if (series.getNumEntries() > 0)
      {
        // getFirstDateTime() now throws if series is empty, so getNumEntries() check is vital.
        if (firstP < series.getFirstDateTime())
            {
                throw TimeSeriesException("FilterTimeSeries: Cannot start filter before reference series' first date");
            }
        if (lastP < series.getFirstDateTime())
            {
                 throw TimeSeriesException("FilterTimeSeries: Cannot end filter before reference series' first date");
            }
      }
    // If series is empty, the loop below won't run, and an empty result is correctly returned.

    for (const auto& entry : series.getEntriesCopy()) // Uses safe getEntriesCopy
      {
        const auto& dt = entry.getDateTime();

        if (dt < firstP)
        {
	  continue;
        }

        if (dt > lastP)
        {
	  break;
        }

        result.addEntry(entry);
      }

    return result;
  }

} // namespace mkc_timeseries

#endif // __TIMESERIES_H
