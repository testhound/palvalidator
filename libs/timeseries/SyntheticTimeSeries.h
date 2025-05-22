// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SYNTHETIC_TIME_SERIES_H
#define __SYNTHETIC_TIME_SERIES_H 1

#include <cassert>
#include <boost/thread/mutex.hpp>
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
/**
 * @class SyntheticTimeSeries
 * @brief Generates a synthetic OHLC time series by reordering the relative price changes of an input time series.
 *
 * This class takes an existing OHLCTimeSeries and computes relative price factors such as:
 * - RelativeOpen: the ratio of the current day's open to the previous day's close.
 * - RelativeClose: the ratio of the current day's close to the current day's open.
 * - RelativeHigh and RelativeLow: the ratios of the current day's high and low to the current day's open.
 *
 * The computed ratios are stored in corresponding vectors. Two separate shuffling methods are applied:
 * - @ref shuffleOverNightChanges() randomizes the order of the relative open factors.
 * - @ref shuffleTradingDayChanges() randomizes the order of the relative high, low, and close factors.
 *
 * Despite the random shuffling of these factors, the overall final closing price remains the same. This is
 * by design since the synthetic series is constructed by applying multiplicative factors iteratively; the
 * cumulative product, which determines the final closing price, is invariant under a permutation of its factors.
 *
 * @tparam Decimal The numeric type used to represent prices and relative changes.
 */
  template <class Decimal>
  class SyntheticTimeSeries
  {
  public:
    /**
     * @brief Constructs a SyntheticTimeSeries based on the provided OHLCTimeSeries.
     *
     * Computes the relative price changes from the input time series and stores them for later use.
     * The relative factors include mRelativeOpen, mRelativeHigh, mRelativeLow, and mRelativeClose.
     * These factors are used to recreate a synthetic price evolution that preserves the global, cumulative
     * price movement of the original data.
     *
     * @param aTimeSeries The original OHLC time series.
     * @param minimumTick The minimum tick size used for rounding prices.
     * @param minimumTickDiv2 Half of the minimum tick size used for rounding.
     */
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
	mRandGenerator(),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (aTimeSeries.getTimeFrame(),
									aTimeSeries.getVolumeUnits(),
									aTimeSeries.getNumEntries())),
      mMinimumTick(minimumTick),
      mMinimumTickDiv2(minimumTickDiv2)
    {
      mRelativeOpen.reserve(aTimeSeries.getNumEntries());
      mRelativeHigh.reserve(aTimeSeries.getNumEntries());
      mRelativeLow.reserve(aTimeSeries.getNumEntries());
      mRelativeClose.reserve(aTimeSeries.getNumEntries());
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.reserve(aTimeSeries.getNumEntries());
#endif
      Decimal valueOfOne (DecimalConstants<Decimal>::DecimalOne);
      Decimal currentOpen (DecimalConstants<Decimal>::DecimalZero);

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

    SyntheticTimeSeries(const SyntheticTimeSeries& rhs)
      : mTimeSeries(rhs.mTimeSeries),
	mDateSeries(rhs.mDateSeries),
	mRelativeOpen(rhs.mRelativeOpen),
	mRelativeHigh(rhs.mRelativeHigh),
	mRelativeLow(rhs.mRelativeLow),
	mRelativeClose(rhs.mRelativeClose),
#ifdef SYNTHETIC_VOLUME
	mRelativeVolume(rhs.mRelativeVolume),
#endif
	mFirstOpen(rhs.mFirstOpen),
#ifdef SYNTHETIC_VOLUME
	mFirstVolume(rhs.mFirstVolume),
#endif
	mRandGenerator(rhs.mRandGenerator),
	mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries)),
	mMinimumTick(rhs.mMinimumTick),
	mMinimumTickDiv2(rhs.mMinimumTickDiv2)
    {
      boost::mutex::scoped_lock lock(rhs.mMutex);
    }
        
    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs)
    {
      if (this != &rhs)
	{
	  boost::mutex::scoped_lock lock(mMutex);
	  boost::mutex::scoped_lock rhsLock(rhs.mMutex);

	  mTimeSeries = rhs.mTimeSeries;
	  mDateSeries = rhs.mDateSeries;
	  mRelativeOpen = rhs.mRelativeOpen;
	  mRelativeHigh = rhs.mRelativeHigh;
	  mRelativeLow = rhs.mRelativeLow;
	  mRelativeClose = rhs.mRelativeClose;
	  mFirstOpen = rhs.mFirstOpen;
	  mRandGenerator = rhs.mRandGenerator;
	  mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries);
	  mMinimumTick = rhs.mMinimumTick;
	  mMinimumTickDiv2 = rhs.mMinimumTickDiv2;
	}
      return *this;
    }
    
    /**
     * @brief Creates the synthetic time series by integrating the shuffled relative factors.
     *
     * The synthetic series is generated by iteratively multiplying an initial price (mFirstOpen) with the
     * corresponding relative factors:
     * - First, the open price for the synthetic series is computed by multiplying the current price by the
     *   relative open factor.
     * - Then, the price is further updated by applying the relative close factor.
     * - The high and low values are computed using the synthetic open price and the relative high and low factors,
     *   followed by a tick-adjustment rounding.
     *
     * Despite the random shuffling, the final synthetic closing price matches that of the original series
     * because the overall product of the relative open and close factors remains unchanged regardless of
     * their order (multiplication is commutative).
     *
     * @exception TimeSeriesEntryException Thrown if an inconsistency is encountered when creating an OHLC entry.
     */
    void createSyntheticSeries()
    {
      boost::mutex::scoped_lock lock(mMutex);

      shuffleOverNightChanges();
      shuffleTradingDayChanges();

      // Shuffle is done. Integrate to recreate the market

      Decimal xPrice = mFirstOpen;

#ifdef SYNTHETIC_VOLUME
      Decimal xVolume = mFirstVolume;
#endif
      Decimal syntheticOpen, syntheticHigh;
      Decimal syntheticClose, syntheticLow;

      std::vector<OHLCTimeSeriesEntry<Decimal>> bars;
      bars.reserve(mTimeSeries.getNumEntries());
      
      for (unsigned long i = 0; i < getNumElements(); i++)
	{
	  xPrice *= mRelativeOpen[i];
	  syntheticOpen = xPrice;

	  xPrice *= mRelativeClose[i];
	  syntheticClose = xPrice;

	  syntheticHigh = num::Round2Tick (syntheticOpen * mRelativeHigh[i], getTick(), getTickDiv2());
	  syntheticLow = num::Round2Tick (syntheticOpen * mRelativeLow[i], getTick(), getTickDiv2());
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

	      bars.emplace_back(entry);
	      //mSyntheticTimeSeries->addEntry(std::move(entry));
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

      mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(mTimeSeries.getTimeFrame(),
								       mTimeSeries.getVolumeUnits(),
								       bars.begin(),
								       bars.end());
    }

    void dumpRelative()
    {
      boost::mutex::scoped_lock lock(mMutex);

      std::ofstream f;
      f.open("relative1.csv");

      for (unsigned int i = 0; i < getNumElements(); i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpRelative2()
    {
      boost::mutex::scoped_lock lock(mMutex);

      std::ofstream f;
      f.open("relative2.csv");

      for (unsigned int i = 0; i < getNumElements(); i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpRelative3()
    {
      boost::mutex::scoped_lock lock(mMutex);

      std::ofstream f;
      f.open("relative3.csv");

      for (unsigned int i = 0; i < getNumElements(); i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpRelative4()
    {
      boost::mutex::scoped_lock lock(mMutex);

      std::ofstream f;
      f.open("relative4.csv");

      for (unsigned int i = 0; i < getNumElements(); i++)
	{
	  f << mDateSeries.getDate(i) << "," << mRelativeOpen[i] << "," <<
	    mRelativeHigh[i] << "," << mRelativeLow[i] << "," <<
	    mRelativeClose[i] << std::endl;
	}
    }

    void dumpSyntheticSeries ()
    {
      boost::mutex::scoped_lock lock(mMutex);

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
      return mTimeSeries.getNumEntries(); 
    }

    std::shared_ptr<const OHLCTimeSeries<Decimal>> getSyntheticTimeSeries() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mSyntheticTimeSeries;
    }

    const std::vector<Decimal> getRelativeOpen() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mRelativeOpen;
    }

    const std::vector<Decimal> getRelativeHigh() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mRelativeHigh;
    }

    const std::vector<Decimal> getRelativeLow() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mRelativeLow;
    }

    const std::vector<Decimal> getRelativeClose() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mRelativeClose;
    }

#ifdef SYNTHETIC_VOLUME
    const std::vector<Decimal> getRelativeVolume() const
    {
      boost::mutex::scoped_lock lock(mMutex);
      return mRelativeVolume;
    }
#endif

  private:
    /**
     * @brief Randomizes the order of the relative overnight price changes.
     *
     * This method shuffles the mRelativeOpen vector using a Fisher–Yates-like
     * algorithm (sampling without replacement).
     * The overnight changes affect the synthetic open prices. Although their order is randomized,
     * when paired with the subsequent relative close factors, the final cumulative product
     * (and hence the final closing price) remains invariant.
     */
    void shuffleOverNightChanges()
    {
      unsigned long i = getNumElements();
      unsigned long j;

      while (i > 1)
	{
	  // Sample without replacement
	  
	  j = mRandGenerator.DrawNumberExclusive (i);
	  i = i - 1;

	  std::swap(mRelativeOpen[i], mRelativeOpen[j]);
	}
    }

     /**
     * @brief Randomizes the order of the trading day price changes.
     *
     * This method shuffles the mRelativeHigh, mRelativeLow, and mRelativeClose vectors independently (and
     * mRelativeVolume, if applicable) using a sampling without replacement algorithm.
     * These shuffles rearrange the intra-day price behavior (highs, lows, and closes) but do not affect the
     * overall
     * cumulative product of relative changes. As a result, the final synthetic closing price is preserved.
     */
    void shuffleTradingDayChanges()
    {
      int i = getNumElements();
      int j;

      while (i > 1)
	{
	  // Sample without replacement
	  
	  j = mRandGenerator.DrawNumberExclusive (i);

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
    RandomMersenne mRandGenerator;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSyntheticTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
    mutable boost::mutex mMutex;    
  };

}
#endif
