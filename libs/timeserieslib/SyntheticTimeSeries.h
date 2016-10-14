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

  template <int Prec>
  class SyntheticTimeSeries
  {
    using Decimal = decimal<Prec>;

  public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries) 
      : mTimeSeries(aTimeSeries),
	mDateSeries (aTimeSeries.getNumEntries()),
	mRelativeOpen (aTimeSeries.getNumEntries()),
	mRelativeHigh (aTimeSeries.getNumEntries()),
	mRelativeLow (aTimeSeries.getNumEntries()),
	mRelativeClose (aTimeSeries.getNumEntries()),
	mNumElements (aTimeSeries.getNumEntries()),
	mRandGenerator(),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (aTimeSeries.getTimeFrame(),
								 aTimeSeries.getVolumeUnits(),
								 aTimeSeries.getNumEntries()))
    {
    
      decimal<Prec> valueOfOne (1.0);
      decimal<Prec> currentOpen(0.0);
      
      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = mTimeSeries.beginRandomAccess();

      mRelativeOpen.addElement(valueOfOne);
      mFirstOpen = mTimeSeries.getOpenValue (it, 0);

      mRelativeHigh.addElement(mTimeSeries.getHighValue (it, 0) / mFirstOpen);
      mRelativeLow.addElement(mTimeSeries.getLowValue (it, 0) / mFirstOpen);
      mRelativeClose.addElement(mTimeSeries.getCloseValue (it, 0) /mFirstOpen) ;
      mDateSeries.addElement (mTimeSeries.getDateValue(it, 0));

      it++;

      for (; it != mTimeSeries.endRandomAccess(); it++)
	{
	  currentOpen = mTimeSeries.getOpenValue (it, 0);

	  mRelativeOpen.addElement(currentOpen / 
				   mTimeSeries.getCloseValue (it, 1));
	  mRelativeHigh.addElement(mTimeSeries.getHighValue (it, 0) / 
				   currentOpen) ;
	  mRelativeLow.addElement(mTimeSeries.getLowValue (it, 0) / 
				  currentOpen) ;
	  mRelativeClose.addElement(mTimeSeries.getCloseValue (it, 0) / 
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

    SyntheticTimeSeries<Prec>& 
    operator=(const SyntheticTimeSeries<Prec> &rhs)
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
	  xPrice *= mRelativeOpen.getElement(i);
	  syntheticOpen = xPrice;

	  xPrice *= mRelativeClose.getElement(i);
	  syntheticClose = xPrice;

	  try
	    {
	      OHLCTimeSeriesEntry<Decimal> entry (mDateSeries.getDate(i), 
					       syntheticOpen, 
					       syntheticOpen * mRelativeHigh.getElement(i),
					       syntheticOpen * mRelativeLow.getElement(i),
					       syntheticClose,
					       0,
					       mSyntheticTimeSeries->getTimeFrame());
	      mSyntheticTimeSeries->addEntry(std::move(entry));
	    }
	  catch (const TimeSeriesEntryException& e)
	    {
	      std::cout << "TimeSeriesEntryException found with relative OHLC = ";
	      std::cout << mRelativeOpen.getElement(i) << ", " << mRelativeHigh.getElement(i) << ", ";
	      std::cout << mRelativeLow.getElement(i) << ", " << mRelativeClose.getElement(i) << std::endl;
	      std::cout << "synthetic OHLC = " << syntheticOpen << ", ";
	      std::cout << syntheticOpen * mRelativeHigh.getElement(i) << ", ";
	      std::cout << syntheticOpen * mRelativeLow.getElement(i) << ", ";
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
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen.getElement (i) << "," <<
	    mRelativeHigh.getElement (i) << "," << mRelativeLow.getElement (i) << "," <<
	    mRelativeClose.getElement (i) << std::endl;
	}
    }

    void dumpSyntheticSeries ()
    {
      PalTimeSeriesCsvWriter<Prec> dumpFile("SyntheticSeriesDump.csv", *mSyntheticTimeSeries);
      dumpFile.writeFile();

    }

    decimal<Prec> getFirstOpen () const
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
	
	  mRelativeOpen.swap(i,j);
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

        mRelativeHigh.swap(i, j);
        mRelativeLow.swap(i, j);
        mRelativeClose.swap(i, j);
	}
    }

  private:
    OHLCTimeSeries<Decimal> mTimeSeries;
    VectorDate mDateSeries;
    VectorDecimal<Prec> mRelativeOpen;
    VectorDecimal<Prec> mRelativeHigh;
    VectorDecimal<Prec> mRelativeLow;
    VectorDecimal<Prec> mRelativeClose;
    decimal<Prec> mFirstOpen;
    unsigned long mNumElements;
    RandomMersenne mRandGenerator;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSyntheticTimeSeries;
  };

  typedef VectorDecimal<2> TimeSeriesPrec2;
  typedef VectorDecimal<5> TimeSeriesPrec5;

  typedef SyntheticTimeSeries<2> SyntheticTimeSeriesPrec2;
}
#endif
