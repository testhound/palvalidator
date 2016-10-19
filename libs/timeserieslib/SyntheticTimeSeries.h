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

namespace mkc_timeseries
{

  template <class Decimal>
  class SyntheticTimeSeries
  {
    //using Decimal = decimal<Prec>;
    //using Decimal = BloombergLP::bdldfp::Decimal64;

  public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries)
      : mTimeSeries(aTimeSeries),
	mDateSeries (aTimeSeries.getNumEntries()),
	mNumElements (aTimeSeries.getNumEntries()),
	mRandGenerator(),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (aTimeSeries.getTimeFrame(),
								 aTimeSeries.getVolumeUnits(),
								 aTimeSeries.getNumEntries()))
    {
      mRelativeOpen .reserve(aTimeSeries.getNumEntries());
      mRelativeHigh .reserve(aTimeSeries.getNumEntries());
      mRelativeLow  .reserve(aTimeSeries.getNumEntries());
	mRelativeClose.reserve(aTimeSeries.getNumEntries());

      Decimal valueOfOne (1.0);
      Decimal currentOpen(0.0);

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = mTimeSeries.beginRandomAccess();

      mRelativeOpen.push_back(valueOfOne);
      mFirstOpen = mTimeSeries.getOpenValue (it, 0);

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
	mFirstOpen (rhs.mFirstOpen),
	mNumElements (rhs.mNumElements),
	mRandGenerator(rhs.mRandGenerator),
	mSyntheticTimeSeries (rhs.mSyntheticTimeSeries)
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
      mFirstOpen = rhs.mFirstOpen;
      mNumElements = rhs.mNumElements;
      mRandGenerator = rhs.mRandGenerator;
      mSyntheticTimeSeries = rhs.mSyntheticTimeSeries;

      return *this;
    }

    void createSyntheticSeries()
    {
      shuffleOverNightChanges();
      shuffleTradingDayChanges();

      // Shuffle is done. Integrate to recreate the market

      Decimal xPrice = mFirstOpen;
      Decimal syntheticOpen;
      Decimal syntheticClose;

      for (unsigned long i = 0; i < mNumElements; i++)
	{
	  xPrice *= mRelativeOpen[i];
	  syntheticOpen = xPrice;

	  xPrice *= mRelativeClose[i];
	  syntheticClose = xPrice;

	  try
	    {
	      OHLCTimeSeriesEntry<Decimal> entry (mDateSeries.getDate(i),
					       syntheticOpen,
					       syntheticOpen * mRelativeHigh[i],
					       syntheticOpen * mRelativeLow[i],
					       syntheticClose,
					       0,
					       mSyntheticTimeSeries->getTimeFrame());
	      mSyntheticTimeSeries->addEntry(std::move(entry));
	    }
	  catch (const TimeSeriesEntryException& e)
	    {
	      std::cout << "TimeSeriesEntryException found with relative OHLC = ";
	      std::cout << mRelativeOpen[i] << ", " << mRelativeHigh[i] << ", ";
	      std::cout << mRelativeLow[i] << ", " << mRelativeClose[i] << std::endl;
	      std::cout << "synthetic OHLC = " << syntheticOpen << ", ";
	      std::cout << syntheticOpen * mRelativeHigh[i] << ", ";
	      std::cout << syntheticOpen * mRelativeLow[i] << ", ";
	      std::cout << syntheticClose << std::endl;

	      std::cout << "First open = " << mFirstOpen << std::endl;
	      std::cout << "Index = " << i << std::endl;

	      dumpRelative();
	      dumpSyntheticSeries ();
	      throw;
	    }
	}
    }

    void dumpRelative()
    {
      std::ofstream f;
      f.open("relative.csv");

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
	  j = mRandGenerator.DrawNumber (0, i - 1);
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
	  j = mRandGenerator.DrawNumber (0, i - 1);
	  // std::cout << "shuffleTradingDayChanges: random number: " << j << std::endl;
	  if (j >= i)
	    j = i - 1;
	  i = i - 1;

        std::swap(mRelativeHigh[i], mRelativeHigh[j]);
        std::swap(mRelativeLow[i], mRelativeLow[j]);
        std::swap(mRelativeClose[i], mRelativeClose[j]);
	}
    }

  private:
    OHLCTimeSeries<Decimal> mTimeSeries;
    VectorDate mDateSeries;
    vector<Decimal> mRelativeOpen;
    vector<Decimal> mRelativeHigh;
    vector<Decimal> mRelativeLow;
    vector<Decimal> mRelativeClose;
    Decimal mFirstOpen;
    unsigned long mNumElements;
    RandomMersenne mRandGenerator;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSyntheticTimeSeries;
  };

  //typedef VectorDecimal<2> TimeSeriesPrec2;
  //typedef VectorDecimal<5> TimeSeriesPrec5;

  //typedef SyntheticTimeSeries<2> SyntheticTimeSeriesPrec2;
}
#endif
