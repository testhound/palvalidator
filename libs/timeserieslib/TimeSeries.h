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
	  //return getInstance (newIndex);
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
    using Map = std::map<boost::gregorian::date, std::shared_ptr<NumericTimeSeriesEntry<Decimal>>>;

  public:
    typedef typename Map::iterator TimeSeriesIterator;
    typedef typename Map::const_iterator ConstTimeSeriesIterator;
    typedef typename Map::const_reverse_iterator ConstReverseTimeSeriesIterator;
    typedef std::map<boost::gregorian::date, ArrayTimeSeriesIndex>::iterator MappingIterator;
    typedef typename std::vector<std::shared_ptr<NumericTimeSeriesEntry<Decimal>>>::iterator RandomAccessIterator;
    typedef typename std::vector<std::shared_ptr<NumericTimeSeriesEntry<Decimal>>>::const_iterator ConstRandomAccessIterator;

    NumericTimeSeries (TimeFrame::Duration timeFrame) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true)
    {}

    NumericTimeSeries (TimeFrame::Duration timeFrame,  unsigned long numElements) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true)
    {
      mSequentialTimeSeries.reserve(numElements);
    }

    NumericTimeSeries (const NumericTimeSeries<Decimal>& rhs)
      :  mSortedTimeSeries(rhs.mSortedTimeSeries),
	 mDateToSequentialIndex(rhs.mDateToSequentialIndex),
	 mSequentialTimeSeries(rhs.mSequentialTimeSeries),
	 mTimeFrame (rhs.mTimeFrame),
	 mMapAndArrayInSync (rhs.mMapAndArrayInSync)
    {}

    NumericTimeSeries<Decimal>&
    operator=(const NumericTimeSeries<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
      mSortedTimeSeries = rhs.mSortedTimeSeries;
      mDateToSequentialIndex = rhs.mDateToSequentialIndex;
      mSequentialTimeSeries = rhs.mSequentialTimeSeries;
      mTimeFrame  = rhs.mTimeFrame;
      mMapAndArrayInSync = rhs.mMapAndArrayInSync;

      return *this;
    }

    void addEntry (std::shared_ptr<NumericTimeSeriesEntry<Decimal>> entry)
    {
      if (entry->getTimeFrame() != getTimeFrame())
	throw std::domain_error(std::string("NumericTimeSeries:addEntry " +boost::gregorian::to_simple_string(entry->getDate()) + std::string(" time frames do not match")));

      boost::gregorian::date d = (entry->getDate());
      TimeSeriesIterator pos = mSortedTimeSeries.find (d);

      if (pos == mSortedTimeSeries.end())
	{
	  mSortedTimeSeries.insert(std::make_pair(d, entry));
	  mMapAndArrayInSync = false;
	  // std::cout << "mMapAndArrayInSync set to false" << std::endl;
	}
      else
	throw std::domain_error(std::string("NumericTimeSeries:" +boost::gregorian::to_simple_string(d) + std::string(" date already exists")));
    }

    void addEntry (const NumericTimeSeriesEntry<Decimal>& entry)
    {
      addEntry (std::make_shared<NumericTimeSeriesEntry<Decimal>> (entry));
    }

    NumericTimeSeries::TimeSeriesIterator getTimeSeriesEntry (const boost::gregorian::date& timeSeriesDate)
    {
      return mSortedTimeSeries.find(timeSeriesDate);
    }

    NumericTimeSeries::ConstTimeSeriesIterator getTimeSeriesEntry (const boost::gregorian::date& timeSeriesDate) const
    {
      return mSortedTimeSeries.find(timeSeriesDate);
    }

    std::vector<Decimal> getTimeSeriesAsVector() const
    {
      std::vector<Decimal> series;
      series.reserve (getNumEntries());

      NumericTimeSeries<Decimal>::ConstTimeSeriesIterator it = beginSortedAccess();

      for (; it != endSortedAccess(); it++)
	{
	  series.push_back (it->second->getValue());

	}

      return series;
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    unsigned long getNumEntries() const
    {
      return mSortedTimeSeries.size();
    }

    NumericTimeSeries::RandomAccessIterator beginRandomAccess()
    {
     if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.begin();
    }

    NumericTimeSeries::RandomAccessIterator endRandomAccess()
    {
     if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.end();
    }

    NumericTimeSeries::ConstRandomAccessIterator beginRandomAccess() const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.begin();
    }

    NumericTimeSeries::ConstRandomAccessIterator endRandomAccess() const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.end();
    }

    NumericTimeSeries::ConstRandomAccessIterator
    getRandomAccessIterator(const boost::gregorian::date& d) const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      NumericTimeSeries::MappingIterator pos = getTimeSeriesIndex (d);
      if (pos != mDateToSequentialIndex.end())
	{
	  std::shared_ptr<ArrayTimeSeriesIndex> index = pos->second;
	  return (beginRandomAccess() + index->asIntegral());
	}
      else
	return endRandomAccess();
    }

    NumericTimeSeries::TimeSeriesIterator beginSortedAccess()
    {
      return mSortedTimeSeries.begin();
    }

    NumericTimeSeries::TimeSeriesIterator endSortedAccess()
    {
      return mSortedTimeSeries.end();
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

    const boost::gregorian::date& getFirstDate() const
    {
      if (getNumEntries() > 0)
	{
	  NumericTimeSeries::ConstTimeSeriesIterator it = beginSortedAccess();
	  return it->first;
	}
      else
	throw std::domain_error(std::string("NumericTimeSeries:getFirstDate: no entries in time series "));
    }

    const boost::gregorian::date& getLastDate() const
    {
      if (getNumEntries() > 0)
	{
	  NumericTimeSeries::ConstTimeSeriesIterator it = endSortedAccess();
	  it--;
	  return it->first;
	}
      else
	throw std::domain_error(std::string("NumericTimeSeries:getLastDate: no entries in time series "));
    }

    void ValidateVectorOffset (const ConstRandomAccessIterator& it,
			       unsigned long offset) const
    {
      ValidateVectorOffset (offset);

      if (it != endRandomAccess())
	{
	  if ((it - offset) >= beginRandomAccess())
	    ;
	  else
	    throw TimeSeriesException ("Offset " +std::to_string (offset) +" outside bounds of time series");
	}
      else
	throw TimeSeriesException ("Iterator is equal to end of internal data structure");
    }

    void ValidateVectorOffset (unsigned long offset) const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      if (offset > mSequentialTimeSeries.size())
	throw TimeSeriesException(std::string("NumericTimeSeries:ValidateVectorOffset ") +std::string(" offset ") +std::to_string (offset) +std::string(" > number of elements in time seres"));
    }

    const std::shared_ptr<NumericTimeSeriesEntry<Decimal>>& getTimeSeriesEntry (const RandomAccessIterator& it,
									     unsigned long offset)
    {
      ValidateVectorOffset(it, offset);
      NumericTimeSeries::RandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    const std::shared_ptr<NumericTimeSeriesEntry<Decimal>>& getTimeSeriesEntry (const ConstRandomAccessIterator& it,
								      unsigned long offset) const
    {
      ValidateVectorOffset(it, offset);
      NumericTimeSeries::ConstRandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    const std::shared_ptr<boost::gregorian::date>&
    getDate (const RandomAccessIterator& it, unsigned long offset)
    {
      return (getTimeSeriesEntry (it, offset)->getDate());
    }

    const boost::gregorian::date&
    getDateValue (const RandomAccessIterator& it, unsigned long offset)
    {
      return (*getDate(it, offset));
    }

    const std::shared_ptr<boost::gregorian::date>&
    getDate (const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      return (getTimeSeriesEntry (it, offset)->getDate());
    }

    const boost::gregorian::date&
    getDateValue (const ConstRandomAccessIterator& it, unsigned long offset) const
    {
      return (*getDate(it, offset));
    }

   const Decimal& getValue (const RandomAccessIterator& it,
				  unsigned long offset)
    {
      return (getTimeSeriesEntry (it, offset)->getValue());
    }

   const Decimal& getValue (const ConstRandomAccessIterator& it,
				  unsigned long offset) const
    {
      return (getTimeSeriesEntry (it, offset)->getValue());
    }

  private:
    bool isSynchronized() const
    {
      return (mMapAndArrayInSync);
    }

    void syncronizeMapAndArray() const
    {
      //std::cout << "syncronizeMapAndArray called" << std::endl;

      if (mSequentialTimeSeries.size() > 0)
	{
	  mSequentialTimeSeries.clear();
	  mDateToSequentialIndex.clear();
	}

      NumericTimeSeries<Decimal>::ConstTimeSeriesIterator pos;
      unsigned long index = 0;
      for (pos = mSortedTimeSeries.begin(); pos != mSortedTimeSeries.end(); ++pos)
	{
	  mDateToSequentialIndex.insert(std::make_pair(pos->first, ArrayTimeSeriesIndex(index)));
	  mSequentialTimeSeries.push_back (pos->second);
	  index++;

	}

      mMapAndArrayInSync = true;
    }

    NumericTimeSeries::MappingIterator
    getTimeSeriesIndex (const boost::gregorian::date& d) const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mDateToSequentialIndex.find (d);
    }

  private:
    Map mSortedTimeSeries;
    mutable std::map<boost::gregorian::date, ArrayTimeSeriesIndex> mDateToSequentialIndex;
    mutable std::vector<std::shared_ptr<NumericTimeSeriesEntry<Decimal>>> mSequentialTimeSeries;
    TimeFrame::Duration mTimeFrame;
    mutable bool mMapAndArrayInSync;
  };

  /*
    class TimeSeries

   */

template <class Decimal> class OHLCTimeSeries
  {
  public:
    typedef typename boost::container::flat_map<boost::gregorian::date, OHLCTimeSeriesEntry<Decimal>>::iterator TimeSeriesIterator;
    typedef typename boost::container::flat_map<boost::gregorian::date, OHLCTimeSeriesEntry<Decimal>>::const_iterator ConstTimeSeriesIterator;
    typedef boost::container::flat_map<boost::gregorian::date, ArrayTimeSeriesIndex>::iterator MappingIterator;
    typedef typename std::vector<OHLCTimeSeriesEntry<Decimal>>::iterator RandomAccessIterator;
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
      mUnitsOfVolume(unitsOfVolume)
    {}

    OHLCTimeSeries (TimeFrame::Duration timeFrame, TradingVolume::VolumeUnit unitsOfVolume,
		unsigned long numElements) :
      mSortedTimeSeries(),
      mDateToSequentialIndex(),
      mSequentialTimeSeries(),
      mTimeFrame (timeFrame),
      mMapAndArrayInSync (true),
      mUnitsOfVolume(unitsOfVolume)
    {
      mSequentialTimeSeries.reserve(numElements);
    }

    OHLCTimeSeries (const OHLCTimeSeries<Decimal>& rhs)
      :  mSortedTimeSeries(rhs.mSortedTimeSeries),
	 mDateToSequentialIndex(rhs.mDateToSequentialIndex),
	 mSequentialTimeSeries(rhs.mSequentialTimeSeries),
	 mTimeFrame (rhs.mTimeFrame),
	 mMapAndArrayInSync (rhs.mMapAndArrayInSync),
	 mUnitsOfVolume(rhs.mUnitsOfVolume)
    {}

    OHLCTimeSeries<Decimal>&
    operator=(const OHLCTimeSeries<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
      mSortedTimeSeries = rhs.mSortedTimeSeries;
      mDateToSequentialIndex = rhs.mDateToSequentialIndex;
      mSequentialTimeSeries = rhs.mSequentialTimeSeries;
      mTimeFrame  = rhs.mTimeFrame;
      mMapAndArrayInSync = rhs.mMapAndArrayInSync;
      mUnitsOfVolume = rhs.mUnitsOfVolume;

      return *this;
    }

    void addEntry (OHLCTimeSeriesEntry<Decimal> entry)
    {
      if (entry.getTimeFrame() != getTimeFrame())
	throw std::domain_error(std::string("OHLCTimeSeries:addEntry " +boost::gregorian::to_simple_string(entry.getDateValue()) + std::string(" time frames do not match")));

      boost::gregorian::date d = entry.getDateValue();
      TimeSeriesIterator pos = mSortedTimeSeries.find (d);

      if (pos == mSortedTimeSeries.end())
	{
	  mSortedTimeSeries.insert(std::make_pair(d, std::move(entry)));
	  mMapAndArrayInSync = false;
	  // std::cout << "mMapAndArrayInSync set to false" << std::endl;
	}
      else
	throw std::domain_error(std::string("OHLCTimeSeries:" +boost::gregorian::to_simple_string(d) + std::string(" date already exists")));
    }

    TimeSeriesIterator getTimeSeriesEntry (const boost::gregorian::date& timeSeriesDate)
    {
      return mSortedTimeSeries.find(timeSeriesDate);
    }

    ConstTimeSeriesIterator getTimeSeriesEntry (const boost::gregorian::date& timeSeriesDate) const
    {
      return mSortedTimeSeries.find(timeSeriesDate);
    }

    TimeFrame::Duration getTimeFrame() const
    {
      return mTimeFrame;
    }

    unsigned long getNumEntries() const
    {
      return mSortedTimeSeries.size();
    }

    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return mUnitsOfVolume;
    }

    OHLCTimeSeries::RandomAccessIterator beginRandomAccess()
    {
     if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.begin();
    }

    OHLCTimeSeries::RandomAccessIterator endRandomAccess()
    {
     if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.end();
    }

    OHLCTimeSeries::ConstRandomAccessIterator beginRandomAccess() const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.begin();
    }

    OHLCTimeSeries::ConstRandomAccessIterator endRandomAccess() const
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mSequentialTimeSeries.end();
    }

    OHLCTimeSeries::ConstRandomAccessIterator getRandomAccessIterator(const boost::gregorian::date& d)
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      OHLCTimeSeries::MappingIterator pos = getTimeSeriesIndex (d);
      if (pos != mDateToSequentialIndex.end())
	{
	  const ArrayTimeSeriesIndex& index = pos->second;
	  return (beginRandomAccess() + index.asIntegral());
	}
      else
	return endRandomAccess();
    }

    OHLCTimeSeries::TimeSeriesIterator beginSortedAccess()
    {
      return mSortedTimeSeries.begin();
    }

    OHLCTimeSeries::TimeSeriesIterator endSortedAccess()
    {
      return mSortedTimeSeries.end();
    }

    OHLCTimeSeries::ConstTimeSeriesIterator beginSortedAccess() const
    {
      return mSortedTimeSeries.begin();
    }

    OHLCTimeSeries::ConstTimeSeriesIterator endSortedAccess() const
    {
      return mSortedTimeSeries.end();
    }

    const boost::gregorian::date& getFirstDate() const
    {
      if (getNumEntries() > 0)
	{
	  OHLCTimeSeries::ConstTimeSeriesIterator it = beginSortedAccess();
	  return it->first;
	}
      else
	throw std::domain_error(std::string("OHLCTimeSeries:getFirstDate: no entries in time series "));
    }

    const boost::gregorian::date& getLastDate() const
    {
      if (getNumEntries() > 0)
	{
	  OHLCTimeSeries::ConstTimeSeriesIterator it = endSortedAccess();
	  it--;
	  return it->first;
	}
      else
	throw std::domain_error(std::string("OHLCTimeSeries:getLastDate: no entries in time series "));
    }

    void ValidateVectorOffset (const ConstRandomAccessIterator& it,
			       unsigned long offset)
    {
      ValidateVectorOffset (offset);

      if (it != endRandomAccess())
	{
	  if ((it - offset) >= beginRandomAccess())
	    ;
	  else
	    throw TimeSeriesException ("Offset " +std::to_string (offset) +" outside bounds of time series");
	}
      else
	throw TimeSeriesException ("Iterator is equal to end of internal data structure");
    }

    void ValidateVectorOffset (unsigned long offset)
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      if (offset > mSequentialTimeSeries.size())
	throw TimeSeriesException(std::string("OHLCTimeSeries:ValidateVectorOffset ") +std::string(" offset ") +std::to_string (offset) +std::string(" > number of elements in time seres"));
    }

    const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const RandomAccessIterator& it,
								     unsigned long offset)
    {
      ValidateVectorOffset(it, offset);
      OHLCTimeSeries::RandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const ConstRandomAccessIterator& it,
								      unsigned long offset)
    {
      ValidateVectorOffset(it, offset);
      OHLCTimeSeries::ConstRandomAccessIterator new_it = it - offset;
      return *new_it;
    }

    const boost::gregorian::date&
    getDateValue (const RandomAccessIterator& it, unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getDateValue();
    }

    const boost::gregorian::date&
    getDateValue (const ConstRandomAccessIterator& it, unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getDateValue();
    }

    const Decimal& getOpenValue (const RandomAccessIterator& it,
				      unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getOpenValue();
    }

    const Decimal& getOpenValue (const ConstRandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getOpenValue();
    }

    const Decimal& getHighValue (const RandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getHighValue();
    }

    const Decimal& getHighValue (const ConstRandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getHighValue();
    }

    const Decimal& getLowValue (const RandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getLowValue();
    }

    const Decimal& getLowValue (const ConstRandomAccessIterator& it,
				       unsigned long offset)
    {
      return (getTimeSeriesEntry (it, offset).getLowValue());
    }

    const Decimal& getCloseValue (const RandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getCloseValue();
    }

    const Decimal& getCloseValue (const ConstRandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getCloseValue();
    }
    ///

    const Decimal& getVolumeValue (const RandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getVolumeValue();
    }

    const Decimal& getVolumeValue (const ConstRandomAccessIterator& it,
				       unsigned long offset)
    {
      return getTimeSeriesEntry (it, offset).getVolumeValue();
    }

    ////
    
    bool isSynchronized()
    {
      return (mMapAndArrayInSync);
    }

    void syncronizeMapAndArray()
    {
      //std::cout << "syncronizeMapAndArray called" << std::endl;

      if (mSequentialTimeSeries.size() > 0)
	{
	  mSequentialTimeSeries.clear();
	  mDateToSequentialIndex.clear();
	}

      OHLCTimeSeries::TimeSeriesIterator pos;
      unsigned long index = 0;
      for (pos = mSortedTimeSeries.begin(); pos != mSortedTimeSeries.end(); ++pos)
	{
	  mDateToSequentialIndex.insert(std::make_pair(pos->first, ArrayTimeSeriesIndex(index)));
	  mSequentialTimeSeries.push_back(pos->second);
	  index++;

	}

      mMapAndArrayInSync = true;
    }

    bool isDateFound(const boost::gregorian::date& date)
    {
      return (mSortedTimeSeries.find(date) != mSortedTimeSeries.end());
    }

  private:

    OHLCTimeSeries::MappingIterator
    getTimeSeriesIndex (const boost::gregorian::date& d)
    {
      if (isSynchronized() == false)
	syncronizeMapAndArray();

      return mDateToSequentialIndex.find (d);
    }

    private:
    boost::container::flat_map<boost::gregorian::date, OHLCTimeSeriesEntry<Decimal>> mSortedTimeSeries;
    boost::container::flat_map<boost::gregorian::date, ArrayTimeSeriesIndex> mDateToSequentialIndex;
    std::vector<OHLCTimeSeriesEntry<Decimal>> mSequentialTimeSeries;
    TimeFrame::Duration mTimeFrame;
    bool mMapAndArrayInSync;
    TradingVolume::VolumeUnit mUnitsOfVolume;
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

  // Create a new time series containing entries covered by date range
  template <class Decimal>
  OHLCTimeSeries<Decimal> FilterTimeSeries (const OHLCTimeSeries<Decimal>& series, const DateRange& dates)
  {
    boost::gregorian::date firstDate (dates.getFirstDate());
    boost::gregorian::date lastDate (dates.getLastDate());

    if ((series.getFirstDate() == firstDate) && (series.getLastDate() == lastDate))
      return series;

    // Cannot create a new series that starts before the first date of the reference series
    if (firstDate < series.getFirstDate())
      throw TimeSeriesException("FilterTimeSeries: Cannot create new series that starts before reference series");

    // Cannot create a new series that starts before the first date of the reference series
    if (lastDate < series.getFirstDate())
      throw TimeSeriesException("FilterTimeSeries: Cannot create new series that starts before reference series");

    OHLCTimeSeries<Decimal> resultSeries (series.getTimeFrame(), series.getVolumeUnits(),
				       series.getNumEntries());

    typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = series.beginSortedAccess();
    if (series.getFirstDate() < firstDate)
      {
	for (; it != series.endSortedAccess(); it++)
	  {
	    if (it->first >= firstDate)
	      break;
	  }
      }

    for (; ((it != series.endSortedAccess()) && (it->first <= lastDate)) ; it++)
      {
	resultSeries.addEntry (it->second);
      }

    return resultSeries;
  }
}

#endif
