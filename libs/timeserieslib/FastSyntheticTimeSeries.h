// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __FAST_SYNTHETIC_TIME_SERIES_H
#define __FAST_SYNTHETIC_TIME_SERIES_H 1

#include <cassert>
#include "TimeSeries.h"
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include <iostream>
#include <ostream>
#include <fstream>
#include "TimeSeriesCsvWriter.h"
#include "DecimalConstants.h"
#include "RelativeTimeSeries.h"

namespace mkc_timeseries
{

  template <class Decimal>
  class FastSyntheticTimeSeries
  {

  public:
    explicit FastSyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries,
				     const Decimal& minimumTick,
				     const Decimal& minimumTickDiv2)
      : mTimeSeries(aTimeSeries),
	mFirstOpen(),
	mFirstVolume(),
	mNumElements (aTimeSeries.getNumEntries()),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (aTimeSeries.getTimeFrame(),
									aTimeSeries.getVolumeUnits(),
									aTimeSeries.getNumEntries())),
	mMinimumTick(minimumTick),
	mMinimumTickDiv2(minimumTickDiv2),
	mSyntheticRelativeTimeSeries (RelativeTimeSeries<Decimal>(aTimeSeries))
    {
      mFirstOpen = mTimeSeries.getOpenValue (it, 0);
#ifdef SYNTHETIC_VOLUME
      mFirstVolume = mTimeSeries.getVolumeValue (it, 0);
#endif
    }

    explicit FastSyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries,
				     const RelativeTimeSeries<Decimal>& relativeTimeSeries,
				     const Decimal& minimumTick,
				     const Decimal& minimumTickDiv2)
      : mTimeSeries(aTimeSeries),
	mFirstOpen(),
	mFirstVolume(),
	mNumElements (aTimeSeries.getNumEntries()),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (aTimeSeries.getTimeFrame(),
									aTimeSeries.getVolumeUnits(),
									aTimeSeries.getNumEntries())),
	mMinimumTick(minimumTick),
	mMinimumTickDiv2(minimumTickDiv2),
	mSyntheticRelativeTimeSeries (relativeTimeSeries)
    {
      mFirstOpen = mTimeSeries.getOpenValue (it, 0);
#ifdef SYNTHETIC_VOLUME
      mFirstVolume = mTimeSeries.getVolumeValue (it, 0);
#endif
    }

    FastSyntheticTimeSeries (const FastSyntheticTimeSeries& rhs)
      : mTimeSeries(rhs.mTimeSeries),
	mFirstOpen (rhs.mFirstOpen),
	mFirstVolume (rhs.mFirstVolume),
	mNumElements (rhs.mNumElements),
	mSyntheticTimeSeries (rhs.mSyntheticTimeSeries),
	mMinimumTick(rhs.mMinimumTick),
	mMinimumTickDiv2(rhs.mMinimumTickDiv2),
	mSyntheticRelativeTimeSeries(rhs.mSyntheticRelativeTimeSeries)
    {}

    FastSyntheticTimeSeries<Decimal>&
    operator=(const FastSyntheticTimeSeries<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mFirstOpen = rhs.mFirstOpen;
      mFirstVolume = rhs.mFirstVolume;
      mNumElements = rhs.mNumElements;
      mSyntheticTimeSeries = rhs.mSyntheticTimeSeries;
      mMinimumTick = rhs.mMinimumTick;
      mMinimumTickDiv2 = rhs.mMinimumTickDiv2;
      mSyntheticRelativeTimeSeries = rhs.mSyntheticRelativeTimeSeries;
      
      return *this;
    }

    void createSyntheticSeries()
    {
      mSyntheticRelativeTimeSeries.createSyntheticRelativeSeries();

      // Shuffle is done. Integrate to recreate the market

      Decimal xPrice = mFirstOpen;

#ifdef SYNTHETIC_VOLUME
      Decimal xVolume = mFirstVolume;
#endif
      Decimal syntheticOpen, syntheticHigh;
      Decimal syntheticClose, syntheticLow;

      for (unsigned long i = 0; i < mNumElements; i++)
	{
	  xPrice *= mSyntheticRelativeTimeSeries.getRelativeOpen(i);

	  syntheticOpen = xPrice;

	  xPrice *= mSyntheticRelativeTimeSeries.getRelativeClose(i);;
	  syntheticClose = xPrice;

	  syntheticHigh = num::Round2Tick (syntheticOpen * mSyntheticRelativeTimeSeries.getRelativeHIgh(i),
					   getTick(),
					   getTickDiv2());
	  syntheticLow = num::Round2Tick (syntheticOpen * mSyntheticRelativeTimeSeries.getRelativeLow(i),
					  getTick(),
					  getTickDiv2());
#ifdef SYNTHETIC_VOLUME
	  xVolume *= mRelativeVolume[i];
#endif

	  
	  try
	    {
	      OHLCTimeSeriesEntry<Decimal> entry (mDateSeries.getDate(i),
						  num::Round2Tick (syntheticOpen, getTick(), getTickDiv2()),
						  syntheticHigh,
						  syntheticLow,
						  num::Round2Tick (syntheticClose, getTick(), getTickDiv2()),
#ifdef SYNTHETIC_VOLUME
						  xVolume,
#else
						  DecimalConstants<Decimal>::DecimalZero,
#endif
						  mSyntheticTimeSeries->getTimeFrame());
	      mSyntheticTimeSeries->addEntry(std::move(entry));
	    }
	  catch (const TimeSeriesEntryException& e)
	    {
	      std::cout << "TimeSeriesEntryException found with relative OHLC = ";
	      std::cout << mRelativeOpen[i] << ", " << mRelativeHigh[i] << ", ";
	      std::cout << mRelativeLow[i] << ", " << mRelativeClose[i] << std::endl;
	      std::cout << "synthetic OHLC = " << syntheticOpen << ", ";
	      std::cout <<  num::Round2Tick (syntheticOpen * mRelativeHigh[i], getTick(), getTickDiv2()) << ", ";
	      std::cout <<  num::Round2Tick (syntheticOpen * mRelativeLow[i], getTick(), getTickDiv2())  << ", ";
	      std::cout << syntheticClose << std::endl;

	      std::cout << "First open = " << mFirstOpen << std::endl;
	      std::cout << "Index = " << i << std::endl;

	      std::cout << "Exception = " << e.what() << std::endl;
	      dumpRelative();
	      dumpSyntheticSeries ();
	      throw;
	    }
	}
    }

    void dumpRelative()
    {
      std::ofstream f;
      f.open("relative1.csv");

      for (unsigned int i = 0; i < mNumElements; i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpRelative2()
    {
      std::ofstream f;
      f.open("relative2.csv");

      for (unsigned int i = 0; i < mNumElements; i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpRelative3()
    {
      std::ofstream f;
      f.open("relative3.csv");

      for (unsigned int i = 0; i < mNumElements; i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpRelative4()
    {
      std::ofstream f;
      f.open("relative4.csv");

      for (unsigned int i = 0; i < mNumElements; i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpSyntheticSeries ()
    {
      PalTimeSeriesCsvWriter<Decimal> dumpFile("SyntheticSeriesDump.csv", *mSyntheticTimeSeries);
      dumpFile.writeFile();

    }

    Decimal getFirstOpen () const
    {
      return mFirstOpen;
    }

    const Decimal& getTick() const
    {
      return mMinimumTick;
    }

    const Decimal& getTickDiv2() const
    {
      return mMinimumTickDiv2;
    }

    unsigned long getNumElements() const
    {
      return mNumElements;
    }

    std::shared_ptr<OHLCTimeSeries<Decimal>> getSyntheticTimeSeries() const
    {
      return mSyntheticTimeSeries;
    }

  private:

  private:
    OHLCTimeSeries<Decimal> mTimeSeries;
    Decimal mFirstOpen;
    Decimal mFirstVolume;
    unsigned long mNumElements;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSyntheticTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
    SyntheticRelativeTimeSeries<Decimal> mSyntheticRelativeTimeSeries;
  };

}
#endif
