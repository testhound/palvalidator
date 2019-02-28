// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __RELATIVE_TIME_SERIES_H
#define __RELATIVE_TIME_SERIES_H 1

#include <cassert>
#include "number.h"
#include "TimeSeries.h"
#include <boost/date_time.hpp>
#include "DecimalConstants.h"
#include "RandomMersenne.h"
#include <vector>

namespace mkc_timeseries
{

  template <class Decimal>
  class RelativeTimeSeries
  {
  public:
    typedef vector<Decimal>::const_iterator ConstRelativeTimeSeriesIterator;
    
  public:
    explicit RelativeTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries)
      : mDateSeries (),
	mRelativeOpen(),
	mRelativeHigh(),
	mRelativeLow(),
	mRelativeClose(),
	mRelativeVolume(),
	mNumElements (aTimeSeries.getNumEntries())
    {
      mDateSeries.reserve (aTimeSeries.getNumEntries()),
      mRelativeOpen.reserve(aTimeSeries.getNumEntries());
      mRelativeHigh.reserve(aTimeSeries.getNumEntries());
      mRelativeLow.reserve(aTimeSeries.getNumEntries());
      mRelativeClose.reserve(aTimeSeries.getNumEntries());

#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.reserve(aTimeSeries.getNumEntries());
#endif
      Decimal valueOfOne (DecimalConstants<Decimal>::DecimalOne);
      Decimal currentOpen (DecimalConstants<Decimal>::DecimalZero);

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = aTimeSeries.beginRandomAccess();

      mRelativeOpen.push_back(valueOfOne);
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.push_back(valueOfOne);
#endif      

      Decimal firstOpen (aTimeSeries.getOpenValue (it, 0));
      
#ifdef SYNTHETIC_VOLUME
      Decimal firstVolume (aTimeSeries.getVolumeValue (it, 0));
#endif
      
      mRelativeHigh.push_back(aTimeSeries.getHighValue (it, 0) / firstOpen);
      mRelativeLow.push_back(aTimeSeries.getLowValue (it, 0) / firstOpen);
      mRelativeClose.push_back(aTimeSeries.getCloseValue (it, 0) /firstOpen) ;
      mDateSeries.push_back (aTimeSeries.getDateValue(it, 0));

      it++;

      for (; it != aTimeSeries.endRandomAccess(); it++)
	{
	  currentOpen = aTimeSeries.getOpenValue (it, 0);

	  mRelativeOpen.push_back(currentOpen / aTimeSeries.getCloseValue (it, 1));
	  mRelativeHigh.push_back(aTimeSeries.getHighValue (it, 0) / currentOpen) ;
	  mRelativeLow.push_back(aTimeSeries.getLowValue (it, 0) / currentOpen) ;
	  mRelativeClose.push_back(aTimeSeries.getCloseValue (it, 0) / currentOpen) ;

#ifdef SYNTHETIC_VOLUME
	  if ((aTimeSeries.getVolumeValue (it, 0) > DecimalConstants<Decimal>::DecimalZero) &&
	      (aTimeSeries.getVolumeValue (it, 1) > DecimalConstants<Decimal>::DecimalZero))
	    {
	      mRelativeVolume.push_back (aTimeSeries.getVolumeValue (it, 0) /
					     aTimeSeries.getVolumeValue (it, 1));
	    }
	  else
	    {
	      mRelativeVolume.push_back (valueOfOne);
	    }
#endif
	  mDateSeries.push_back (aTimeSeries.getDateValue(it,0));
	}

    }

    unsigned long getNumElements() const
    {
      return mNumElements;
    }

    std::vector<Decimal> getOpenRelativeSeries() const
    {
      return mRelativeOpen;
    }
    
    ConstRelativeTimeSeriesIterator beginOpenRelativeSeries() const
    {
      return mRelativeOpen.begin();
    }

    ConstRelativeTimeSeriesIterator endOpenRelativeSeries() const
    {
      return mRelativeOpen.end();
    }

    
    std::vector<Decimal> getHighRelativeSeries() const
    {
      return mRelativeHigh;
    }
    
    ConstRelativeTimeSeriesIterator beginHighRelativeSeries() const
    {
      return mRelativeHigh.begin();
    }

    ConstRelativeTimeSeriesIterator endHighRelativeSeries() const
    {
      return mRelativeHigh.end();
    }

    std::vector<Decimal> getLowRelativeSeries() const
    {
      return mRelativeLow;
    }
    
    ConstRelativeTimeSeriesIterator beginLowRelativeSeries() const
    {
      return mRelativeLow.begin();
    }

    ConstRelativeTimeSeriesIterator endLowRelativeSeries() const
    {
      return mRelativeLow.end();
    }

    std::vector<Decimal> getCloseRelativeSeries() const
    {
      return mRelativeLow;
    }

    ConstRelativeTimeSeriesIterator beginCloseRelativeSeries() const
    {
      return mRelativeClose.begin();
    }

    ConstRelativeTimeSeriesIterator endCloseRelativeSeries() const
    {
      return mRelativeClose.end();
    }

    std::vector<boost::gregorian::date> getDateRelativeSeries() const
    {
      return mDateSeries;
    }
    
    ConstRelativeTimeSeriesIterator beginDateRelativeSeries() const
    {
      return mDateSeries.begin();
    }

    ConstRelativeTimeSeriesIterator endDateRelativeSeries() const
    {
      return mDateSeries.end();
    }

  private:
    std::vector<boost::gregorian::date> mDateSeries;
    std::vector<Decimal> mRelativeOpen;
    std::vector<Decimal> mRelativeHigh;
    std::vector<Decimal> mRelativeLow;
    std::vector<Decimal> mRelativeClose;
    std::vector<Decimal> mRelativeVolume;
    unsigned long mNumElements;
  };

  template <class Decimal>
  bool operator==(const RelativeTimeSeries<Decimal>& lhs, const RelativeTimeSeries<Decimal>& rhs)
  {
    if (lhs.getNumElements() != rhs.getNumElements())
      return false;

    bool bDate = std::equal (lhs.beginDateRelativeSeries(), lhs.endDateRelativeSeries(),
			     rhs.beginDateRelativeSeries());

    if (!bDate)
      return false;

    bool bOpen = std::equal (lhs.beginOpenRelativeSeries(), lhs.endOpenRelativeSeries(),
			     rhs.beginOpenRelativeSeries());

    if (!bOpen)
      return false;

    bool bHigh = std::equal (lhs.beginHighRelativeSeries(), lhs.endHighRelativeSeries(),
			     rhs.beginHighRelativeSeries());

    if (!bHigh)
      return false;

    bool bLow = std::equal (lhs.beginLowRelativeSeries(), lhs.endLowRelativeSeries(),
			     rhs.beginLowRelativeSeries());

    if (!bLow)
      return false;

    bool bClose = std::equal (lhs.beginCloseRelativeSeries(), lhs.endCloseRelativeSeries(),
			     rhs.beginCloseRelativeSeries());

    if (!bClose)
      return false;

    return true;
  }
    
  template <class Decimal>
  bool operator!=(const RelativeTimeSeries<Decimal>& lhs, const RelativeTimeSeries<Decimal>& rhs)
  {
    return !(lhs == rhs);
  }
  
  template <class Decimal>
  class SyntheticRelativeTimeSeries
  {
  public:
    typedef vector<Decimal>::const_iterator ConstRelativeTimeSeriesIterator;
    
  public:
    explicit SyntheticRelativeTimeSeries(const RelativeTimeSeries<Decimal>& aRelativeTimeSeries)
      : mRelativeOpen(aRelativeTimeSeries.getOpenRelativeSeries()),
	mRelativeHigh(aRelativeTimeSeries.getHighRelativeSeries()),
	mRelativeLow(aRelativeTimeSeries.getLowRelativeSeries()),
	mRelativeClose(aRelativeTimeSeries.getCloseRelativeSeries()),
	mRelativeVolume(),
	mNumElements(aRelativeTimeSeries.getNumEntries()),
	mRandGenerator()
    {
    }

    void createSyntheticRelativeSeries()
    {
      shuffleOverNightChanges();
      shuffleTradingDayChanges()
    }
    
    Decimal getRelativeOpen (unsigned long index) const
    {
      return mRelativeOpen[index];
    }

    Decimal getRelativeHigh (unsigned long index) const
    {
      return mRelativeHigh[index];
    }

    Decimal getRelativeLow (unsigned long index) const
    {
      return mRelativeLow[index];
    }

    Decimal getRelativeClose (unsigned long index) const
    {
      return mRelativeClose[index];
    }

    std::vector<Decimal> getOpenRelativeSeries() const
    {
      return mRelativeOpen;
    }
    
    ConstRelativeTimeSeriesIterator beginOpenRelativeSeries() const
    {
      return mRelativeOpen.begin();
    }

    ConstRelativeTimeSeriesIterator endOpenRelativeSeries() const
    {
      return mRelativeOpen.end();
    }

    
    std::vector<Decimal> getHighRelativeSeries() const
    {
      return mRelativeHigh;
    }
    
    ConstRelativeTimeSeriesIterator beginHighRelativeSeries() const
    {
      return mRelativeHigh.begin();
    }

    ConstRelativeTimeSeriesIterator endHighRelativeSeries() const
    {
      return mRelativeHigh.end();
    }

    std::vector<Decimal> getLowRelativeSeries() const
    {
      return mRelativeLow;
    }
    
    ConstRelativeTimeSeriesIterator beginLowRelativeSeries() const
    {
      return mRelativeLow.begin();
    }

    ConstRelativeTimeSeriesIterator endLowRelativeSeries() const
    {
      return mRelativeLow.end();
    }

    std::vector<Decimal> getCloseRelativeSeries() const
    {
      return mRelativeLow;
    }

    ConstRelativeTimeSeriesIterator beginCloseRelativeSeries() const
    {
      return mRelativeClose.begin();
    }

    ConstRelativeTimeSeriesIterator endCloseRelativeSeries() const
    {
      return mRelativeClose.end();
    }
    
  private:
    void shuffleOverNightChanges()
    {
      unsigned long i = mNumElements;
      unsigned long j;

      while (i > 1)
	{
	  // Sample without replacement
	  
	  j = mRandGenerator.DrawNumber (i - 1);
	  //	std::cout << "shuffleOverNightChanges: random number: " << j << std::endl;
	  if (j >= i)
	    j = i - 1;
	  i = i - 1;

        std::swap(mRelativeOpen[i], mRelativeOpen[j]);
	}
    }

    void shuffleTradingDayChanges()
    {
      int i = mNumElements;
      int j;

      while (i > 1)
	{
	  // Sample without replacement
	  
	  j = mRandGenerator.DrawNumber (i - 1);
	  if (j >= i)
	    j = i - 1;
	  i = i - 1;

        std::swap(mRelativeHigh[i], mRelativeHigh[j]);
        std::swap(mRelativeLow[i], mRelativeLow[j]);
        std::swap(mRelativeClose[i], mRelativeClose[j]);
#ifdef SYNTHETIC_VOLUME       
	std::swap(mRelativeVolume[i], mRelativeVolume[j]);
#endif
	}

  private:
    std::vector<Decimal> mRelativeOpen;
    std::vector<Decimal> mRelativeHigh;
    std::vector<Decimal> mRelativeLow;
    std::vector<Decimal> mRelativeClose;
    std::vector<Decimal> mRelativeVolume;
    unsigned long mNumElements;
    RandomMersenne mRandGenerator;
    };
}
