// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIMESERIES_H
#define __TIMESERIES_H 1

#include "TimeSeriesEntry.h"
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
#include "DateRange.h"

namespace std
{
  /**
   * @brief Hash functor specialization for Boost.PosixTime ptime.
   *
   * Packs the calendar date and minute-of-day into a 64-bit integer key,
   * then feeds that key into the standard integer hasher.  Designed for
   * daily and intraday bar timestamps (minute resolution), yielding
   * a perfect, collision-free mapping within those constraints.
   *
   * Key layout (64 bits total):
   * - High 53+ bits: days since epoch (as returned by day_number())
   * - Low 11 bits:   minute of day (0–1439)
   *
   * @note If your time series never has sub-minute bars, this guarantees
   *       unique hash keys for each bar timestamp.
   */
  template<>
  struct hash<boost::posix_time::ptime> {
    static inline size_t computeKey(const boost::posix_time::ptime& t) noexcept
    {
      // days since some epoch
      uint64_t days = t.date().day_number();
      
      // minute of day (0–1439)
      uint64_t minuteOfDay =
        static_cast<uint64_t>(t.time_of_day().hours()) * 60 +
        static_cast<uint64_t>(t.time_of_day().minutes());

      // pack into one 64-bit key: high bits = days, low bits = minuteOfDay
      uint64_t key = (days << 11) | (minuteOfDay & 0x7FF);

      // now feed it to the integer hasher
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

  /**
   * @brief Represents a time series of Open, High, Low, Close (OHLC) and Volume data.
   * @tparam Decimal The numeric type used for price and volume data (e.g., double, float).
   *
   * This class is central to financial backtesting systems, holding historical
   * price and volume information for instruments like equities or futures.

   * Maintains a single sorted-invariant vector of entries (`mData`) of type `OHLCTimeSeriesEntry<Decimal>`.
   * - Insertion via `addEntry(...)` keeps the data sorted (binary search + insert).
   * - Rejects duplicate timestamps.
   * - Offers both sorted and random access without any synchronization or finalize steps.
   */
  template <class Decimal>
  class OHLCTimeSeries
  {
  public:
    using Entry = OHLCTimeSeriesEntry<Decimal>;
    using ConstTimeSeriesIterator   = typename std::vector<Entry>::const_iterator;
    using ConstRandomAccessIterator = ConstTimeSeriesIterator;

    /** @name Constructors & Assignment */
    ///@{

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
	mIndex()
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
	mIndex()
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
     * @note only enable if InputIt::value_type is OHLCTimeSeriesEntry<Decimal>
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
	mIndex()
    {
      // 1) Optional: verify every entry has the correct timeFrame
      for (auto& e : mData) {
	if (e.getTimeFrame() != tf)
	  throw TimeSeriesException("ctor: time frame mismatch");
      }

      std::sort(mData.begin(), mData.end(),
		[](auto const &a, auto const &b)
		{
		  return a.getDateTime() < b.getDateTime();
		});
    }

    /** @brief Default copy constructor. */
    OHLCTimeSeries(const OHLCTimeSeries& rhs) = default;

    /** @brief Default copy assignment operator. */
    OHLCTimeSeries& operator=(const OHLCTimeSeries& rhs) = default;

    /** @brief Default move constructor. */
    OHLCTimeSeries(OHLCTimeSeries&& rhs) noexcept = default;

     /** @brief Default move assignment operator. */
    OHLCTimeSeries& operator=(OHLCTimeSeries&& rhs) noexcept = default;
    ///@}

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
     * @brief Number of entries in the series.
     */
    unsigned long getNumEntries() const
    {
      return static_cast<unsigned long>(mData.size());
    }

    /**
     * @brief Inserts a new OHLC entry into the time series.
     *
     * The entry is inserted in a way that maintains the time-sorted order of the
     * internal vector. If an entry with the same timestamp already exists,
     * or if the entry's time frame does not match the series' time frame,
     * an exception is thrown. This operation invalidates the internal ptime index.
     *
     * @param entry The OHLCTimeSeriesEntry to add (passed by value, potentially moved).
     * @throws std::domain_error If the entry's time frame does not match the series' time frame.
     * @throws std::domain_error If an entry with the same timestamp already exists.
     */
    void addEntry(Entry entry)
    {
      if (entry.getTimeFrame() != getTimeFrame())
	throw std::domain_error("addEntry: time frame mismatch");

      auto it = std::lower_bound(
				 mData.begin(), mData.end(), entry,
				 [](auto const& a, auto const& b){ return a.getDateTime() < b.getDateTime(); });

      if (it != mData.end() && it->getDateTime() == entry.getDateTime())
	throw std::domain_error("addEntry: duplicate timestamp");

      mData.insert(it, std::move(entry));
      if (!mIndex.empty())
        mIndex.clear();
    }

    /**
     * @brief Creates a NumericTimeSeries containing only the Open prices from this series.
     * @return A new NumericTimeSeries object holding the Open prices and corresponding timestamps.
     */
    NumericTimeSeries<Decimal> OpenTimeSeries() const
    {
      NumericTimeSeries<Decimal> out(getTimeFrame(), getNumEntries());
      for (auto it = beginSortedAccess(); it != endSortedAccess(); ++it)
	out.addEntry(
		     NumericTimeSeriesEntry<Decimal>(it->getDateTime(),
						     it->getOpenValue(),
						     it->getTimeFrame()));
      return out;
    }

    /**
     * @brief Creates a NumericTimeSeries containing only the High prices from this series.
     * @return A new NumericTimeSeries object holding the High prices and corresponding timestamps.
     */
    NumericTimeSeries<Decimal> HighTimeSeries() const
    {
      NumericTimeSeries<Decimal> out(getTimeFrame(), getNumEntries());
      for (auto it = beginSortedAccess(); it != endSortedAccess(); ++it)
	out.addEntry(
		     NumericTimeSeriesEntry<Decimal>(it->getDateTime(),
						     it->getHighValue(),
						     it->getTimeFrame()));
      return out;
    }

    /**
     * @brief Creates a NumericTimeSeries containing only the Low prices from this series.
     * @return A new NumericTimeSeries object holding the Low prices and corresponding timestamps.
     */
    NumericTimeSeries<Decimal> LowTimeSeries() const
    {
      NumericTimeSeries<Decimal> out(getTimeFrame(), getNumEntries());
      for (auto it = beginSortedAccess(); it != endSortedAccess(); ++it)
	out.addEntry(
		     NumericTimeSeriesEntry<Decimal>(it->getDateTime(),
						     it->getLowValue(),
						     it->getTimeFrame()));
      return out;
    }

    /**
     * @brief Creates a NumericTimeSeries containing only the Close prices from this series.
     * @return A new NumericTimeSeries object holding the Close prices and corresponding timestamps.
     */
    NumericTimeSeries<Decimal> CloseTimeSeries() const
    {
      NumericTimeSeries<Decimal> out(getTimeFrame(), getNumEntries());
      for (auto it = beginSortedAccess(); it != endSortedAccess(); ++it)
	out.addEntry(
		     NumericTimeSeriesEntry<Decimal>(it->getDateTime(),
						     it->getCloseValue(),
						     it->getTimeFrame()));
      return out;
    }

    /** @brief Return a copy of all entries. */
    std::vector<Entry> getEntriesCopy() const
    {
      return mData;
    }

    /** @name Sorted-access iteration (by time) */
    ///@{
    ConstTimeSeriesIterator beginSortedAccess() const { return mData.begin(); }
    ConstTimeSeriesIterator endSortedAccess()   const { return mData.end();   }
    ///@}

    /**
     * @brief Lookup entry by date (default bar time).
     * @return endSortedAccess() if not found.
     */
    ConstTimeSeriesIterator getTimeSeriesEntry(const boost::gregorian::date& d) const
    {
      return getTimeSeriesEntry(boost::posix_time::ptime(d, getDefaultBarTime()));
    }

    /**
     * @brief Lookup entry by exact datetime.
     */
    ConstTimeSeriesIterator getTimeSeriesEntry(const boost::posix_time::ptime& dt) const
    {
      /*       if (mIndex.empty())
	buildIndex();
      
       auto it = mIndex.find(dt);
       if (it == mIndex.end())
	 return mData.end();

       return mData.begin() + it->second;  */
      
      
       auto it = std::lower_bound(
				 mData.begin(), mData.end(), dt,
				 [](auto const& e, auto const& t){ return e.getDateTime() < t; });
      return (it != mData.end() && it->getDateTime() == dt)
      ? it : mData.end();
    }

    /** @name Random-access iteration (by index) */

    ConstRandomAccessIterator beginRandomAccess() const { return mData.begin(); }
    ConstRandomAccessIterator endRandomAccess()   const { return mData.end();   }

    /**
     * @brief Get iterator for a specific ptime date in the random-access array.
     */
    ConstRandomAccessIterator getRandomAccessIterator(const boost::posix_time::ptime& d) const
    {
      return getTimeSeriesEntry(d);
    }

    /**
     * @brief Get iterator for a specific gregorian date in the random-access array.
     */
    ConstRandomAccessIterator getRandomAccessIterator(const boost::gregorian::date& d) const
    {
      return getTimeSeriesEntry(d);
    }

    /** @brief First date in series. */
    boost::gregorian::date getFirstDate() const
    {
      if (mData.empty()) throw std::domain_error("getFirstDate: empty series");
      return mData.front().getDateTime().date();
    }

    /** @brief First full timestamp. */
    boost::posix_time::ptime getFirstDateTime() const
    {
      if (mData.empty()) throw std::domain_error("getFirstDateTime: empty series");
      return mData.front().getDateTime();
    }

    /** @brief Last date in series. */
    boost::gregorian::date getLastDate() const
    {
      if (mData.empty()) throw std::domain_error("getLastDate: empty series");
      return mData.back().getDateTime().date();
    }

    /** @brief Last full timestamp. */
    boost::posix_time::ptime getLastDateTime() const
    {
      if (mData.empty()) throw std::domain_error("getLastDateTime: empty series");
      return mData.back().getDateTime();
    }

    /**
     * @brief Get entry by iterator and offset.
     */
    const Entry& getTimeSeriesEntry(const ConstRandomAccessIterator& it,
				    unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);
      return *(it - offset);
    }

    /**
     * @brief Retrieve date value by iterator offset.
     */
    const boost::posix_time::ptime& getDateTimeValue(const ConstRandomAccessIterator& it,
						     unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getDateTime();
    }

    /**
     * @brief Retrieve date value by iterator offset.
     */
    const boost::gregorian::date& getDateValue(const ConstRandomAccessIterator& it,
					       unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getDateValue();
    }

    /**
     * @brief Retrieve Open value by iterator offset.
     */
    const Decimal& getOpenValue(const ConstRandomAccessIterator& it,
				unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getOpenValue();
    }

    /**
     * @brief Retrieve High value by iterator offset.
     */
    const Decimal& getHighValue(const ConstRandomAccessIterator& it,
				unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getHighValue();
    }

    /**
     * @brief Retrieve Low value by iterator offset.
     */
    const Decimal& getLowValue(const ConstRandomAccessIterator& it,
			       unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getLowValue();
    }

    /**
     * @brief Retrieve Close value by iterator offset.
     */
    const Decimal& getCloseValue(const ConstRandomAccessIterator& it,
				 unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getCloseValue();
    }

    /**
     * @brief Retrieve Volume value by iterator offset.
     */
    const Decimal& getVolumeValue(const ConstRandomAccessIterator& it,
				  unsigned long offset) const
    {
      return getTimeSeriesEntry(it, offset).getVolumeValue();
    }

    /**
     * @brief Check if a date exists in series.
     */
    bool isDateFound(const boost::gregorian::date& d) const
    {
      return getTimeSeriesEntry(d) != mData.end();
    }

    /**
     * @brief Remove all entries matching a date (ignoring time).
     */
    void deleteEntryByDate(const boost::gregorian::date& d)
    {
      mData.erase(
		  std::remove_if(mData.begin(), mData.end(),
				 [&](auto const& e){ return e.getDateTime().date() == d; }),
		  mData.end());

      if (!mIndex.empty())
	mIndex.clear();
    }

  private:
    void buildIndex() const
    {
      mIndex.clear();
      for (size_t i = 0; i < mData.size(); ++i)
	mIndex[mData[i].getDateTime()] = i;
    }

    // Validate iterator-based offset.
    void ValidateVectorOffset(const ConstRandomAccessIterator& it,
			      unsigned long offset) const
    {
      if (it == mData.end())
	throw TimeSeriesException("Iterator at end");
      auto idx = std::distance(mData.begin(), it);
      if (static_cast<unsigned long>(idx) < offset)
	throw TimeSeriesException("Offset out of bounds");
    }

    // Validate index-based offset.
    void ValidateVectorOffset(unsigned long offset) const
    {
      if (offset >= mData.size())
	throw TimeSeriesException("Offset exceeds series size");
    }

    // ---- Data Members ----
    std::vector<Entry>         mData;
    TimeFrame::Duration        mTimeFrame;
    TradingVolume::VolumeUnit  mUnitsOfVolume;
    mutable std::unordered_map<ptime,size_t> mIndex;
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
	if (*it1 != *it2)
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
	  const auto& entry = *it; // Get the OHLC entry value
	  const ptime& dateTime = entry.getDateTime();                // Get the boost::posix_time::ptime key


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
  OHLCTimeSeries<Decimal> FilterTimeSeries(const OHLCTimeSeries<Decimal>& series,
					   const DateRange& dates)
  {
    using TS = OHLCTimeSeries<Decimal>;

    auto firstDate = dates.getFirstDate();
    auto lastDate  = dates.getLastDate();
    ptime firstP(firstDate, getDefaultBarTime());
    ptime lastP (lastDate,  getDefaultBarTime());

    // quick-path: full range
    if (firstDate == series.getFirstDate() && lastDate == series.getLastDate())
        return series;

    if (firstDate < series.getFirstDate())
        throw TimeSeriesException("FilterTimeSeries: Cannot start before reference series");
    if (lastDate  < series.getFirstDate())
        throw TimeSeriesException("FilterTimeSeries: Cannot end before reference series");

    // reserve to avoid reallocations
    TS result(series.getTimeFrame(),
              series.getVolumeUnits(),
              series.getNumEntries());

    // now iterate the vector of Entry objects
    for (auto const& entry : series.getEntriesCopy()) {
        auto dt = entry.getDateTime();
        if (dt < firstP)  continue;
        if (dt > lastP)   break;
        result.addEntry(entry);
    }

    return result;
  }
}

#endif
