// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SYNTHETIC_TIME_SERIES_H
#define __SYNTHETIC_TIME_SERIES_H 1

#include <cassert>
#include "TimeSeries.h"
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include <iostream>
#include <ostream>
#include <fstream>
#include "TimeSeriesCsvWriter.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{

  template <class Decimal>
  class SyntheticTimeSeries
  {
    //using Decimal = decimal<Prec>;
    //using Decimal = BloombergLP::bdldfp::Decimal64;

  public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries,
				 const Decimal& minimumTick,
				 const Decimal& minimumTickDiv2)
      : mTimeSeries(aTimeSeries),
	mDateSeries (aTimeSeries.getNumEntries()),
	mRelativeOpen(),
	mRelativeHigh(),
	mRelativeLow(),
	mRelativeClose(),
	mRelativeVolume(),
	mFirstOpen(),
	mFirstVolume(),
	mNumElements (aTimeSeries.getNumEntries()),
	mRandGenerator(),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (aTimeSeries.getTimeFrame(),
									aTimeSeries.getVolumeUnits(),
									aTimeSeries.getNumEntries())),
      mMinimumTick(minimumTick),
      mMinimumTickDiv2(minimumTickDiv2)
    {
      //std::cout << "SyntheticTimeSeries:: minimum tick = " << mMinimumTick << std::endl;
      //std::cout << "SyntheticTimeSeries: creating new times series with start date = " << aTimeSeries.getFirstDate() <<
      //" and last date = " << aTimeSeries.getLastDate() << std::endl << std::endl;
      mRelativeOpen.reserve(aTimeSeries.getNumEntries());
      mRelativeHigh.reserve(aTimeSeries.getNumEntries());
      mRelativeLow.reserve(aTimeSeries.getNumEntries());
      mRelativeClose.reserve(aTimeSeries.getNumEntries());
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.reserve(aTimeSeries.getNumEntries());
#endif
      Decimal valueOfOne (DecimalConstants<Decimal>::DecimalOne);
      Decimal currentOpen (DecimalConstants<Decimal>::DecimalZero);
      //Decimal valueOfOne (num::fromString<Decimal>("1.0"));
      //Decimal currentOpen(num::fromString<Decimal>("0.0"));

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = mTimeSeries.beginRandomAccess();

      mRelativeOpen.push_back(valueOfOne);
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.push_back(valueOfOne);
#endif      
      mFirstOpen = mTimeSeries.getOpenValue (it, 0);
#ifdef SYNTHETIC_VOLUME
      mFirstVolume = mTimeSeries.getVolumeValue (it, 0);
#endif
      mRelativeHigh.push_back(mTimeSeries.getHighValue (it, 0) / mFirstOpen);
      mRelativeLow.push_back(mTimeSeries.getLowValue (it, 0) / mFirstOpen);
      mRelativeClose.push_back(mTimeSeries.getCloseValue (it, 0) /mFirstOpen) ;
      mDateSeries.addElement (mTimeSeries.getDateValue(it, 0));

      it++;

      for (; it != mTimeSeries.endRandomAccess(); it++)
	{
	  currentOpen = mTimeSeries.getOpenValue (it, 0);

	   mRelativeOpen.push_back(currentOpen /
				   mTimeSeries.getCloseValue (it, 1));
	  mRelativeHigh.push_back(mTimeSeries.getHighValue (it, 0) /
				   currentOpen) ;
	  mRelativeLow.push_back(mTimeSeries.getLowValue (it, 0) /
				  currentOpen) ;
	  mRelativeClose.push_back(mTimeSeries.getCloseValue (it, 0) /
	  currentOpen) ;


#ifdef SYNTHETIC_VOLUME
	  if ((mTimeSeries.getVolumeValue (it, 0) > DecimalConstants<Decimal>::DecimalZero) &&
	      (mTimeSeries.getVolumeValue (it, 1) > DecimalConstants<Decimal>::DecimalZero))
	    {
	      mRelativeVolume.push_back (mTimeSeries.getVolumeValue (it, 0) /
					     mTimeSeries.getVolumeValue (it, 1));
	      //std::cout << "Relative indicator1 value = " << mTimeSeries.getVolumeValue (it, 0) /
	      //mTimeSeries.getVolumeValue (it, 1) << std::endl;
	    }
	  else
	    {
	      mRelativeVolume.push_back (valueOfOne);
	    }
#endif
	  
	  mDateSeries.addElement (mTimeSeries.getDateValue(it,0));
	}
    }

    SyntheticTimeSeries (const SyntheticTimeSeries& rhs)
      : mTimeSeries(rhs.mTimeSeries),
	mDateSeries (rhs.mDateSeries),
	mRelativeOpen (rhs.mRelativeOpen),
	mRelativeHigh (rhs.mRelativeHigh),
	mRelativeLow (rhs.mRelativeLow),
	mRelativeClose (rhs.mRelativeClose),
	mRelativeVolume (rhs.mRelativeVolume),
	mFirstOpen (rhs.mFirstOpen),
	mFirstVolume (rhs.mFirstVolume),
	mNumElements (rhs.mNumElements),
	mRandGenerator(rhs.mRandGenerator),
	mSyntheticTimeSeries (rhs.mSyntheticTimeSeries),
	mMinimumTick(rhs.mMinimumTick),
	mMinimumTickDiv2(rhs.mMinimumTickDiv2)
    {}

    SyntheticTimeSeries<Decimal>&
    operator=(const SyntheticTimeSeries<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mTimeSeries = rhs.mTimeSeries;
      mDateSeries = rhs.mDateSeries;
      mRelativeOpen = rhs.mRelativeOpen;
      mRelativeHigh = rhs.mRelativeHigh;
      mRelativeLow = rhs.mRelativeLow;
      mRelativeClose = rhs.mRelativeClose;
      mRelativeVolume = rhs.mRelativeVolume;
      mFirstOpen = rhs.mFirstOpen;
      mFirstVolume = rhs.mFirstVolume;
      mNumElements = rhs.mNumElements;
      mRandGenerator = rhs.mRandGenerator;
      mSyntheticTimeSeries = rhs.mSyntheticTimeSeries;
      mMinimumTick = rhs.mMinimumTick;
      mMinimumTickDiv2 = rhs.mMinimumTickDiv2;
      
      return *this;
    }

    void createSyntheticSeries()
    {
      shuffleOverNightChanges();
      shuffleTradingDayChanges();

      // Shuffle is done. Integrate to recreate the market

      Decimal xPrice = mFirstOpen;

#ifdef SYNTHETIC_VOLUME
      Decimal xVolume = mFirstVolume;
#endif
      Decimal syntheticOpen, syntheticHigh;
      Decimal syntheticClose, syntheticLow;

      for (unsigned long i = 0; i < mNumElements; i++)
	{
	  xPrice *= mRelativeOpen[i];
	  //xPrice = num::Round2Tick (xPrice, getTick(), getTickDiv2());
	  syntheticOpen = xPrice;

	  xPrice *= mRelativeClose[i];
	  //xPrice = num::Round2Tick (xPrice, getTick(), getTickDiv2());
	  syntheticClose = xPrice;

	  syntheticHigh = num::Round2Tick (syntheticOpen * mRelativeHigh[i], getTick(), getTickDiv2());
	  syntheticLow = num::Round2Tick (syntheticOpen * mRelativeLow[i], getTick(), getTickDiv2());
#ifdef SYNTHETIC_VOLUME
	  xVolume *= mRelativeVolume[i];
#endif

#if 0
	  if ((syntheticLow > syntheticOpen) && ((syntheticLow - syntheticOpen) <= getTick()))
	    syntheticLow = syntheticOpen;
	  else if ((syntheticLow > syntheticClose) && ((syntheticLow - syntheticClose) <= getTick()))
	    syntheticLow = syntheticClose;

	  if ((syntheticOpen > syntheticHigh) && ((syntheticOpen - syntheticHigh) <= getTick()))
	    syntheticHigh = syntheticOpen;
	  else if ((syntheticClose > syntheticHigh) && ((syntheticClose - syntheticHigh) <= getTick()))
	    syntheticHigh = syntheticClose;
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
    void shuffleOverNightChanges()
    {
      unsigned long i = mNumElements;
      unsigned long j;

      while (i > 1)
	{
	  //j = mRandGenerator.DrawNumber (0, i - 1);
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
	  //j = mRandGenerator.DrawNumber (0, i - 1);
	  // std::cout << "shuffleTradingDayChanges: random number: " << j << std::endl;
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
    }

  private:
    OHLCTimeSeries<Decimal> mTimeSeries;
    VectorDate mDateSeries;
    vector<Decimal> mRelativeOpen;
    vector<Decimal> mRelativeHigh;
    vector<Decimal> mRelativeLow;
    vector<Decimal> mRelativeClose;
    vector<Decimal> mRelativeVolume;
    Decimal mFirstOpen;
    Decimal mFirstVolume;
    unsigned long mNumElements;
    RandomMersenne mRandGenerator;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSyntheticTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
  };

}
#endif
