#ifndef __SYNTHETIC_TIME_SERIES_H
#define __SYNTHETIC_TIME_SERIES_H 1

// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016

#include <cassert>
#include <map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <memory>
#include <mutex> 
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeries.h" // OHLCTimeSeries is policy-based
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include "ShuffleUtils.h"
#include "DecimalConstants.h"
#include "number.h"
#include "RoundingPolicies.h"

namespace mkc_timeseries
{

  /**
   * @enum SyntheticNullModel
   * @brief Defines the randomization strategy used to generate the synthetic series.
   * * These models determine how much of the original market structure is destroyed.
   */
  enum class SyntheticNullModel {
    N1_MaxDestruction = 0,  // current behavior (independent shuffles)
    N0_PairedDay      = 1,  // shuffle day-units intact: (gap, H/L/C[, Volume]) together
    N2_BlockDays      = 2   // (reserved) shuffle blocks of day-units; not implemented here
  };

// Forward declaration of the Pimpl interface and concrete implementations
// Pimpl interface and classes are now templated on LookupPolicy, with a default

  template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>,
	    template<class> class RoundingPolicy = NoRounding>
  class ISyntheticTimeSeriesImpl;

  template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>,
	    template<class> class RoundingPolicy = NoRounding>
  class EodSyntheticTimeSeriesImpl;

  template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>,
	    template<class> class RoundingPolicy = NoRounding>
  class IntradaySyntheticTimeSeriesImpl;

  /**
   * @interface ISyntheticTimeSeriesImpl
   * @brief Abstract base class (Interface) for synthetic time series generator implementations.
   *
   * This interface abstracts the specific logic used to shuffle and reconstruct 
   * End-of-Day (EOD) vs. Intraday data, allowing the main SyntheticTimeSeries 
   * class to use the Pimpl idiom.
   *
   * @tparam Decimal The numeric type for price and factor data (e.g. double, float).
   * @tparam LookupPolicy The lookup policy for the OHLCTimeSeries to be generated.
   */
  template <class Decimal, class LookupPolicy, template<class> class RoundingPolicy>
  class ISyntheticTimeSeriesImpl
  {
  public:
    virtual ~ISyntheticTimeSeriesImpl() = default;
    virtual void shuffleFactors(RandomMersenne& randGenerator) = 0;
    virtual std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildSeries() = 0;
    virtual Decimal getFirstOpen() const = 0;
    virtual unsigned long getNumOriginalElements() const = 0;
    virtual std::vector<Decimal> getRelativeOpenFactors()  const = 0;
    virtual std::vector<Decimal> getRelativeHighFactors()  const = 0;
    virtual std::vector<Decimal> getRelativeLowFactors()   const = 0;
    virtual std::vector<Decimal> getRelativeCloseFactors() const = 0;
#ifdef SYNTHETIC_VOLUME
    virtual std::vector<Decimal> getRelativeVolumeFactors() const = 0;
#endif
    virtual std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>> clone() const = 0;
  };

  /**
   * @class EodSyntheticTimeSeriesImpl_N0
   * @brief Implements the "Paired-Day" (N0) Null Model for EOD data.
   *
   * @details
   * In this model, the specific data of a single trading day (Open, High, Low, Close, Volume)
   * is treated as an atomic unit. The algorithm shuffles the *order* in which these days appear
   * but does not alter the intraday relationship (e.g., a massive gap down followed by a rally
   * stays intact).
   *
   * Logic:
   * 1. Generate a permutation of day indices.
   * 2. Reorder all relative factor arrays (Open, High, Low, Close) using this single permutation.
   */
  template <class Decimal, class LookupPolicy, template<class> class RoundingPolicy>
  class EodSyntheticTimeSeriesImpl_N0
    : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>
  {
  public:
    EodSyntheticTimeSeriesImpl_N0(const OHLCTimeSeries<Decimal, LookupPolicy>& sourceSeries,
				  const Decimal& minimumTick,
				  const Decimal& minimumTickDiv2)
      : mSourceTimeSeries(sourceSeries),
	mMinimumTick(minimumTick),
	mMinimumTickDiv2(minimumTickDiv2),
	mDateSeries(sourceSeries.getNumEntries()),
	mFirstOpen(DecimalConstants<Decimal>::DecimalZero)
#ifdef SYNTHETIC_VOLUME
      , mFirstVolume(DecimalConstants<Decimal>::DecimalZero)
#endif
    {
      initEodDataInternal();
    }

    EodSyntheticTimeSeriesImpl_N0(const EodSyntheticTimeSeriesImpl_N0& other) = default;
    EodSyntheticTimeSeriesImpl_N0& operator=(const EodSyntheticTimeSeriesImpl_N0& other) = default;
    EodSyntheticTimeSeriesImpl_N0(EodSyntheticTimeSeriesImpl_N0&& other) noexcept = default;
    EodSyntheticTimeSeriesImpl_N0& operator=(EodSyntheticTimeSeriesImpl_N0&& other) noexcept = default;

    // Paired-day shuffle: permute indices {1..n-1} once and apply to all day-factor arrays
    void shuffleFactors(RandomMersenne& randGenerator) override
    {
      const size_t n = mRelativeOpen.size();
      if (n <= 2) return;

      // Build day index permutation; keep index 0 fixed as anchor
      std::vector<size_t> idx(n);
      std::iota(idx.begin(), idx.end(), size_t{0});

      // Fisher–Yates over subrange [1..n-1]
      for (size_t i = n - 1; i > 1; --i) {
	// draw j in [1, i]
	size_t j = randGenerator.DrawNumberExclusive(i) + 1; // DrawNumberExclusive(i) yields [0..i-1]
	std::swap(idx[i], idx[j]);
      }

      auto apply_perm = [&](const std::vector<Decimal>& src, std::vector<Decimal>& dst) {
	dst.resize(n);
	for (size_t k = 0; k < n; ++k) dst[k] = src[idx[k]];
      };

      std::vector<Decimal> newOpen, newHigh, newLow, newClose;
#ifdef SYNTHETIC_VOLUME
      std::vector<Decimal> newVolume;
#endif

      // Apply the same permutation to all day-factor arrays
      apply_perm(mRelativeOpen,  newOpen);
      apply_perm(mRelativeHigh,  newHigh);
      apply_perm(mRelativeLow,   newLow);
      apply_perm(mRelativeClose, newClose);
#ifdef SYNTHETIC_VOLUME
      apply_perm(mRelativeVolume, newVolume);
#endif

      // Ensure the anchor day keeps open factor = 1
      if (!newOpen.empty()) newOpen[0] = DecimalConstants<Decimal>::DecimalOne;

      mRelativeOpen.swap(newOpen);
      mRelativeHigh.swap(newHigh);
      mRelativeLow.swap(newLow);
      mRelativeClose.swap(newClose);
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.swap(newVolume);
#endif
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildSeries() override {
      return buildEodInternal();
    }

    Decimal        getFirstOpen()               const override { return mFirstOpen; }
    unsigned long  getNumOriginalElements()     const override { return mSourceTimeSeries.getNumEntries(); }
    std::vector<Decimal> getRelativeOpenFactors()  const override { return mRelativeOpen;  }
    std::vector<Decimal> getRelativeHighFactors()  const override { return mRelativeHigh;  }
    std::vector<Decimal> getRelativeLowFactors()   const override { return mRelativeLow;   }
    std::vector<Decimal> getRelativeCloseFactors() const override { return mRelativeClose; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return mRelativeVolume; }
#endif

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>> clone() const override {
      return std::make_unique<EodSyntheticTimeSeriesImpl_N0<Decimal, LookupPolicy, RoundingPolicy>>(*this);
    }

  private:
    // Copied from EodSyntheticTimeSeriesImpl with identical logic
    void initEodDataInternal()
    {
      using SourceSeriesType = OHLCTimeSeries<Decimal, LookupPolicy>;
      using Iter = typename SourceSeriesType::ConstRandomAccessIterator;

      if (mSourceTimeSeries.getNumEntries() == 0) {
	mFirstOpen = DecimalConstants<Decimal>::DecimalZero;
#ifdef SYNTHETIC_VOLUME
	mFirstVolume = DecimalConstants<Decimal>::DecimalZero;
#endif
	return;
      }

      Iter it = mSourceTimeSeries.beginRandomAccess();
      Decimal one = DecimalConstants<Decimal>::DecimalOne;

      mRelativeOpen.reserve(mSourceTimeSeries.getNumEntries());
      mRelativeHigh.reserve(mSourceTimeSeries.getNumEntries());
      mRelativeLow.reserve(mSourceTimeSeries.getNumEntries());
      mRelativeClose.reserve(mSourceTimeSeries.getNumEntries());
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.reserve(mSourceTimeSeries.getNumEntries());
#endif

      mFirstOpen = it->getOpenValue();
#ifdef SYNTHETIC_VOLUME
      mFirstVolume = it->getVolumeValue();
#endif

      mRelativeOpen.push_back(one);
#ifdef SYNTHETIC_VOLUME
      mRelativeVolume.push_back(one);
#endif

      if (mFirstOpen != DecimalConstants<Decimal>::DecimalZero) {
	mRelativeHigh.push_back(it->getHighValue()  / mFirstOpen);
	mRelativeLow.push_back(it->getLowValue()    / mFirstOpen);
	mRelativeClose.push_back(it->getCloseValue()/ mFirstOpen);
      } else {
	mRelativeHigh.push_back(one);
	mRelativeLow.push_back(one);
	mRelativeClose.push_back(one);
      }
      mDateSeries.addElement(it->getDateValue());

      if (mSourceTimeSeries.getNumEntries() > 1) {
	Iter prev_it = it;
	++it;
	for (; it != mSourceTimeSeries.endRandomAccess(); ++it, ++prev_it) {
	  Decimal currOpen  = it->getOpenValue();
	  Decimal prevClose = prev_it->getCloseValue();

	  mRelativeOpen.push_back(
				  (prevClose != DecimalConstants<Decimal>::DecimalZero)
				  ? currOpen / prevClose
				  : one
				  );

	  if (currOpen != DecimalConstants<Decimal>::DecimalZero) {
	    mRelativeHigh.push_back(it->getHighValue()  / currOpen);
	    mRelativeLow.push_back(it->getLowValue()    / currOpen);
	    mRelativeClose.push_back(it->getCloseValue()/ currOpen);
	  } else {
	    mRelativeHigh.push_back(one);
	    mRelativeLow.push_back(one);
	    mRelativeClose.push_back(one);
	  }

#ifdef SYNTHETIC_VOLUME
	  Decimal v0 = it->getVolumeValue();
	  Decimal v1 = prev_it->getVolumeValue();
	  mRelativeVolume.push_back(
				    (v1 > DecimalConstants<Decimal>::DecimalZero) ? (v0 / v1) : one
				    );
#endif
	  mDateSeries.addElement(it->getDateValue());
	}
      }
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildEodInternal()
    {
      if (mSourceTimeSeries.getNumEntries() == 0) {
	return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
								       mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
      }

      Decimal preciseChainPrice = mFirstOpen;
#ifdef SYNTHETIC_VOLUME
      Decimal preciseChainVolume = mFirstVolume;
#endif
      std::vector<OHLCTimeSeriesEntry<Decimal>> bars;
      bars.reserve(mRelativeOpen.size());

      for (size_t i = 0; i < mRelativeOpen.size(); ++i) {
	Decimal preciseOpenOfDay  = (i == 0) ? preciseChainPrice
	  : preciseChainPrice * mRelativeOpen[i];
	Decimal preciseCloseOfDay = preciseOpenOfDay * mRelativeClose[i];

	Decimal open  = RoundingPolicy<Decimal>::round(preciseOpenOfDay,               mMinimumTick, mMinimumTickDiv2);
	Decimal high  = RoundingPolicy<Decimal>::round(preciseOpenOfDay*mRelativeHigh[i],  mMinimumTick, mMinimumTickDiv2);
	Decimal low   = RoundingPolicy<Decimal>::round(preciseOpenOfDay*mRelativeLow[i],   mMinimumTick, mMinimumTickDiv2);
	Decimal close = RoundingPolicy<Decimal>::round(preciseCloseOfDay,              mMinimumTick, mMinimumTickDiv2);

	high = std::max({high, open, close});
	low  = std::min({low, open, close});

	preciseChainPrice = preciseCloseOfDay;

#ifdef SYNTHETIC_VOLUME
	Decimal currentDayVolume;
	if (i == 0) {
	  currentDayVolume = preciseChainVolume;
	} else {
	  currentDayVolume = (mRelativeVolume.size() > i)
            ? preciseChainVolume * mRelativeVolume[i]
            : preciseChainVolume;
	}
	Decimal volume = num::Round2Tick(currentDayVolume,
					 DecimalConstants<Decimal>::DecimalOne,
					 DecimalConstants<Decimal>::DecimalZero);
	preciseChainVolume = currentDayVolume;

	bars.emplace_back(mDateSeries.getDate(i), open, high, low, close,
			  volume, mSourceTimeSeries.getTimeFrame());
#else
	bars.emplace_back(mDateSeries.getDate(i), open, high, low, close,
			  DecimalConstants<Decimal>::DecimalZero, mSourceTimeSeries.getTimeFrame());
#endif
      }

      return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
								     mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(),
								     bars.begin(), bars.end());
    }

  private:
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeries;
    Decimal        mMinimumTick;
    Decimal        mMinimumTickDiv2;
    VectorDate     mDateSeries;
    std::vector<Decimal> mRelativeOpen, mRelativeHigh, mRelativeLow, mRelativeClose;
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> mRelativeVolume;
#endif
    Decimal        mFirstOpen;
#ifdef SYNTHETIC_VOLUME
    Decimal        mFirstVolume;
#endif
  };

  /**
   * @class EodSyntheticTimeSeriesImpl
   * @brief Implements the "Max Destruction" (N1) Null Model for EOD data.
   *
   * @details
   * This model performs independent shuffling of:
   * 1. Overnight Gaps (Relative Open)
   * 2. Intraday Volatility (Relative High/Low/Close)
   *
   * By breaking the link between the overnight gap and the subsequent trading day's
   * behavior, this creates the most rigorous test for a strategy.
   */
  template <class Decimal, class LookupPolicy, template<class> class RoundingPolicy>
  class EodSyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy> {
public:
    EodSyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal, LookupPolicy>& sourceSeries, 
                               const Decimal& minimumTick,
                               const Decimal& minimumTickDiv2)
      : mSourceTimeSeries(sourceSeries),
        mMinimumTick(minimumTick),
        mMinimumTickDiv2(minimumTickDiv2),
        mDateSeries(sourceSeries.getNumEntries()),
        mFirstOpen(DecimalConstants<Decimal>::DecimalZero)
#ifdef SYNTHETIC_VOLUME
        , mFirstVolume(DecimalConstants<Decimal>::DecimalZero)
#endif
    {
        initEodDataInternal();
    }

    EodSyntheticTimeSeriesImpl(const EodSyntheticTimeSeriesImpl& other) = default;
    EodSyntheticTimeSeriesImpl& operator=(const EodSyntheticTimeSeriesImpl& other) = default;
    EodSyntheticTimeSeriesImpl(EodSyntheticTimeSeriesImpl&& other) noexcept = default;
    EodSyntheticTimeSeriesImpl& operator=(EodSyntheticTimeSeriesImpl&& other) noexcept = default;

    void shuffleFactors(RandomMersenne& randGenerator) override {
        shuffleOverNightChangesInternal(randGenerator);
        shuffleTradingDayChangesInternal(randGenerator);
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildSeries() override {
        return buildEodInternal();
    }

    Decimal getFirstOpen() const override { return mFirstOpen; }
    unsigned long getNumOriginalElements() const override { return mSourceTimeSeries.getNumEntries(); }
    std::vector<Decimal> getRelativeOpenFactors() const override { return mRelativeOpen; }
    std::vector<Decimal> getRelativeHighFactors() const override { return mRelativeHigh; }
    std::vector<Decimal> getRelativeLowFactors()  const override { return mRelativeLow; }
    std::vector<Decimal> getRelativeCloseFactors()const override { return mRelativeClose; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return mRelativeVolume; }
#endif

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>> clone() const override {
        return std::make_unique<EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>(*this);
    }

private:
    void initEodDataInternal() {
        using SourceSeriesType = OHLCTimeSeries<Decimal, LookupPolicy>;
        using Iter = typename SourceSeriesType::ConstRandomAccessIterator;
        if (mSourceTimeSeries.getNumEntries() == 0) {
            mFirstOpen = DecimalConstants<Decimal>::DecimalZero;
#ifdef SYNTHETIC_VOLUME
            mFirstVolume = DecimalConstants<Decimal>::DecimalZero;
#endif
            return;
        }

        Iter it = mSourceTimeSeries.beginRandomAccess(); 
        Decimal one = DecimalConstants<Decimal>::DecimalOne;

        mRelativeOpen.reserve(mSourceTimeSeries.getNumEntries());
        mRelativeHigh.reserve(mSourceTimeSeries.getNumEntries());
        mRelativeLow.reserve(mSourceTimeSeries.getNumEntries());
        mRelativeClose.reserve(mSourceTimeSeries.getNumEntries());
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.reserve(mSourceTimeSeries.getNumEntries());
#endif
        mFirstOpen = it->getOpenValue();
#ifdef SYNTHETIC_VOLUME
        mFirstVolume = it->getVolumeValue();
#endif
        mRelativeOpen.push_back(one);
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.push_back(one);
#endif

        if (mFirstOpen != DecimalConstants<Decimal>::DecimalZero) {
            mRelativeHigh.push_back(it->getHighValue() / mFirstOpen);
            mRelativeLow.push_back(it->getLowValue() / mFirstOpen);
            mRelativeClose.push_back(it->getCloseValue() / mFirstOpen);
        } else {
            mRelativeHigh.push_back(one);
            mRelativeLow.push_back(one);
            mRelativeClose.push_back(one);
        }
        mDateSeries.addElement(it->getDateValue());
        
        if (mSourceTimeSeries.getNumEntries() > 1) {
            Iter prev_it = it;
            ++it;
            for (; it != mSourceTimeSeries.endRandomAccess(); ++it, ++prev_it)
            {
                Decimal currOpen  = it->getOpenValue();
                Decimal prevClose = prev_it->getCloseValue();

                if (prevClose != DecimalConstants<Decimal>::DecimalZero) {
                    mRelativeOpen.push_back(currOpen / prevClose);
                } else {
                    mRelativeOpen.push_back(one);
                }

                if (currOpen != DecimalConstants<Decimal>::DecimalZero) {
                    mRelativeHigh.push_back(it->getHighValue() / currOpen);
                    mRelativeLow.push_back(it->getLowValue() / currOpen);
                    mRelativeClose.push_back(it->getCloseValue() / currOpen);
                } else {
                    mRelativeHigh.push_back(one);
                    mRelativeLow.push_back(one);
                    mRelativeClose.push_back(one);
                }
#ifdef SYNTHETIC_VOLUME
                Decimal v0 = it->getVolumeValue();
                Decimal v1 = prev_it->getVolumeValue();
                if (v1 > DecimalConstants<Decimal>::DecimalZero) {
                     mRelativeVolume.push_back(v0 / v1);
                } else {
                     mRelativeVolume.push_back(one);
                }
#endif
                mDateSeries.addElement(it->getDateValue());
            }
        }
    }

    void shuffleOverNightChangesInternal(RandomMersenne& randGenerator) {
        if (mRelativeOpen.size() <= 1) return; 
        for (size_t i = mRelativeOpen.size() - 1; i > 1; --i) { 
            size_t j = randGenerator.DrawNumberExclusive(i) + 1; 
            std::swap(mRelativeOpen[i], mRelativeOpen[j]);
        }
    }

    void shuffleTradingDayChangesInternal(RandomMersenne& randGenerator) {
        if (mRelativeHigh.size() <= 1) return; 
        size_t i = mRelativeHigh.size();
        while (i > 1)
        {
            size_t j = randGenerator.DrawNumberExclusive(i);
            i--; 
            std::swap(mRelativeHigh[i],  mRelativeHigh[j]);
            std::swap(mRelativeLow [i],  mRelativeLow [j]);
            std::swap(mRelativeClose[i], mRelativeClose[j]);
#ifdef SYNTHETIC_VOLUME
            std::swap(mRelativeVolume[i], mRelativeVolume[j]);
#endif
        }
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildEodInternal() {
        if (mSourceTimeSeries.getNumEntries() == 0) {
             return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
                mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
        }

        Decimal preciseChainPrice = mFirstOpen; 
#ifdef SYNTHETIC_VOLUME
        Decimal preciseChainVolume = mFirstVolume; 
#endif
        std::vector<OHLCTimeSeriesEntry<Decimal>> bars;
        bars.reserve(mRelativeOpen.size());

        for (size_t i = 0; i < mRelativeOpen.size(); ++i)
        {
            Decimal preciseOpenOfDay = (i == 0) ? preciseChainPrice : preciseChainPrice * mRelativeOpen[i];
            Decimal preciseCloseOfDay = preciseOpenOfDay * mRelativeClose[i];
            
	    Decimal open  = RoundingPolicy<Decimal>::round(preciseOpenOfDay, mMinimumTick, mMinimumTickDiv2);
	    Decimal high  = RoundingPolicy<Decimal>::round(preciseOpenOfDay * mRelativeHigh[i], mMinimumTick, mMinimumTickDiv2);
	    Decimal low   = RoundingPolicy<Decimal>::round(preciseOpenOfDay * mRelativeLow[i],  mMinimumTick, mMinimumTickDiv2);
	    Decimal close = RoundingPolicy<Decimal>::round(preciseCloseOfDay, mMinimumTick, mMinimumTickDiv2);

	    high = std::max({high, open, close});
	    low  = std::min({low, open, close}); 

            preciseChainPrice = preciseCloseOfDay;

#ifdef SYNTHETIC_VOLUME
            Decimal currentDayVolume;
            if (i == 0) { 
                 currentDayVolume = preciseChainVolume; 
            } else { 
                 if (mRelativeVolume.size() > i) {
                    currentDayVolume = preciseChainVolume * mRelativeVolume[i];
                 } else { 
                    currentDayVolume = preciseChainVolume;
                 }
            }
            Decimal volume = num::Round2Tick(currentDayVolume, DecimalConstants<Decimal>::DecimalOne, DecimalConstants<Decimal>::DecimalZero); 
            preciseChainVolume = currentDayVolume;

            bars.emplace_back(
                mDateSeries.getDate(i), open, high, low, close, volume, mSourceTimeSeries.getTimeFrame()
            );
#else
            bars.emplace_back(
                mDateSeries.getDate(i), open, high, low, close, DecimalConstants<Decimal>::DecimalZero, mSourceTimeSeries.getTimeFrame()
            );
#endif
        }
        return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
            mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), bars.begin(), bars.end());
    }

private:
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
    VectorDate mDateSeries;
    std::vector<Decimal> mRelativeOpen, mRelativeHigh, mRelativeLow, mRelativeClose;
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> mRelativeVolume;
#endif
    Decimal mFirstOpen;
#ifdef SYNTHETIC_VOLUME
    Decimal mFirstVolume;
#endif
};

  /**
   * @class IntradaySyntheticTimeSeriesImpl
   * @brief Implements Intraday synthetic time series generation.
   * * @details
   * This implementation performs a hierarchical "Deep Shuffle" suitable for intraday data:
   * 1. Shuffles the order of trading days.
   * 2. Shuffles the overnight gaps between days.
   * 3. Shuffles the order of intraday bars WITHIN each day.
   * * This effectively destroys both intraday serial correlation (trends within the day)
   * and inter-day correlation (trends across days).
   */
  template <class Decimal, class LookupPolicy, template<class> class RoundingPolicy>
  class IntradaySyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy> {
public:
    IntradaySyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal, LookupPolicy>& sourceSeries, 
                                    const Decimal& minimumTick,
                                    const Decimal& minimumTickDiv2)
      : mSourceTimeSeries(sourceSeries),
        mMinimumTick(minimumTick),
        mMinimumTickDiv2(minimumTickDiv2),
        mFirstOpen(DecimalConstants<Decimal>::DecimalZero)
    {
        initIntradayDataInternal();
    }

    IntradaySyntheticTimeSeriesImpl(const IntradaySyntheticTimeSeriesImpl& other) = default;
    IntradaySyntheticTimeSeriesImpl& operator=(const IntradaySyntheticTimeSeriesImpl& other) = default;
    IntradaySyntheticTimeSeriesImpl(IntradaySyntheticTimeSeriesImpl&& other) noexcept = default;
    IntradaySyntheticTimeSeriesImpl& operator=(IntradaySyntheticTimeSeriesImpl&& other) noexcept = default;

    void shuffleFactors(RandomMersenne& randGenerator) override {
        for (auto& dayBars : mDailyNormalizedBars) {
            inplaceShuffle(dayBars, randGenerator);
        }
        inplaceShuffle(mOvernightGaps, randGenerator);
        inplaceShuffle(mDayIndices, randGenerator);
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildSeries() override {
        return buildIntradayInternal();
    }

    Decimal getFirstOpen() const override { return mFirstOpen; }
    unsigned long getNumOriginalElements() const override { return mSourceTimeSeries.getNumEntries(); }
    std::vector<Decimal> getRelativeOpenFactors()  const override { return {}; }
    std::vector<Decimal> getRelativeHighFactors()  const override { return {}; }
    std::vector<Decimal> getRelativeLowFactors()   const override { return {}; }
    std::vector<Decimal> getRelativeCloseFactors() const override { return {}; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return {}; }
#endif

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>> clone() const override {
        return std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>(*this);
    }

private:
    void initIntradayDataInternal() {
        using Entry = OHLCTimeSeriesEntry<Decimal>;

        std::map<boost::gregorian::date, std::vector<Entry>> dayMap;
        
        if (mSourceTimeSeries.getNumEntries() == 0) {
            mFirstOpen = DecimalConstants<Decimal>::DecimalZero; 
            return;
        }

        mFirstOpen = mSourceTimeSeries.beginRandomAccess()->getOpenValue();

        for (auto it = mSourceTimeSeries.beginRandomAccess(); it != mSourceTimeSeries.endRandomAccess(); ++it) {
            dayMap[it->getDateTime().date()].push_back(*it);
        }

        if (dayMap.empty()) { 
            return; 
        }

        auto dayMapIt = dayMap.begin();
        mBasisDayBars = dayMapIt->second; 

        if (mBasisDayBars.empty()) {
            return; 
        }
        Decimal prevDayActualClose = mBasisDayBars.back().getCloseValue();
        
        ++dayMapIt; 

        Decimal one = DecimalConstants<Decimal>::DecimalOne;
        for (; dayMapIt != dayMap.end(); ++dayMapIt)
        {
            const auto& currentDayBars = dayMapIt->second;
            if (currentDayBars.empty()) {
                mOvernightGaps.push_back(one); 
                mDailyNormalizedBars.emplace_back(); 
                continue;
            }

            Decimal currentDayOriginalOpen = currentDayBars.front().getOpenValue();
            
            Decimal gapFactor;
            if (prevDayActualClose != DecimalConstants<Decimal>::DecimalZero) {
                 gapFactor = currentDayOriginalOpen / prevDayActualClose;
                 mOvernightGaps.push_back(gapFactor);
            } else {
                 gapFactor = one;
                 mOvernightGaps.push_back(one); 
            }
            
            prevDayActualClose = currentDayBars.back().getCloseValue(); 

            std::vector<Entry> normalizedBarsForThisDay;
            normalizedBarsForThisDay.reserve(currentDayBars.size());

            if (currentDayOriginalOpen != DecimalConstants<Decimal>::DecimalZero) {
                for (const auto& bar : currentDayBars)
                {
                    Decimal normO = bar.getOpenValue()  / currentDayOriginalOpen;
                    Decimal normH = bar.getHighValue()  / currentDayOriginalOpen;
                    Decimal normL = bar.getLowValue()   / currentDayOriginalOpen;
                    Decimal normC = bar.getCloseValue() / currentDayOriginalOpen;
#ifdef SYNTHETIC_VOLUME
                    Decimal dayFirstVolume = currentDayBars.front().getVolumeValue();
                    Decimal volumeFactor = one;
                    if (dayFirstVolume > DecimalConstants<Decimal>::DecimalZero) {
                        volumeFactor = bar.getVolumeValue() / dayFirstVolume;
                    } else if (bar.getVolumeValue() > DecimalConstants<Decimal>::DecimalZero) {
                        volumeFactor = one; 
                    }
                    normalizedBarsForThisDay.emplace_back(
                        bar.getDateTime(), normO, normH, normL, normC,
                        volumeFactor, mSourceTimeSeries.getTimeFrame());
#else
                    normalizedBarsForThisDay.emplace_back(
                        bar.getDateTime(), normO, normH, normL, normC,
                        DecimalConstants<Decimal>::DecimalZero, mSourceTimeSeries.getTimeFrame());
#endif
                }
            } else { 
                for (const auto& bar : currentDayBars) { 
                     normalizedBarsForThisDay.emplace_back(
                        bar.getDateTime(), one, one, one, one, 
#ifdef SYNTHETIC_VOLUME
                        one, 
#else
                        DecimalConstants<Decimal>::DecimalZero,
#endif
                        mSourceTimeSeries.getTimeFrame());
                }
            }
            mDailyNormalizedBars.push_back(std::move(normalizedBarsForThisDay));
        }

        mDayIndices.resize(mDailyNormalizedBars.size());
        std::iota(mDayIndices.begin(), mDayIndices.end(), 0u); 
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildIntradayInternal() {
        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::vector<Entry> constructedBars;
        size_t totalReserve = mBasisDayBars.size();
        for (const auto& v : mDailyNormalizedBars) totalReserve += v.size();
        if (totalReserve == 0 && mBasisDayBars.empty()) { 
             return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
                mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
        }
        constructedBars.reserve(totalReserve);

        //for (const auto& bar : mBasisDayBars) { 
        //    constructedBars.push_back(bar);
        //}

	// Apply rounding policy to basis-day bars and enforce OHLC invariants
	for (const auto& bar : mBasisDayBars)
	  {
	    Decimal o = RoundingPolicy<Decimal>::round(bar.getOpenValue(),  mMinimumTick, mMinimumTickDiv2);
	    Decimal h = RoundingPolicy<Decimal>::round(bar.getHighValue(),  mMinimumTick, mMinimumTickDiv2);
	    Decimal l = RoundingPolicy<Decimal>::round(bar.getLowValue(),   mMinimumTick, mMinimumTickDiv2);
	    Decimal c = RoundingPolicy<Decimal>::round(bar.getCloseValue(), mMinimumTick, mMinimumTickDiv2);
	    
	    // Ensure rounded OHLC satisfy invariants expected by OHLCTimeSeriesEntry
	    h = std::max({h, o, c});
	    l = std::min({l, o, c});

	    constructedBars.emplace_back(
					 bar.getDateTime(),
					 o, h, l, c,
					 bar.getVolumeValue(),                       // keep existing volume for basis bars
					 mSourceTimeSeries.getTimeFrame()
					 );
	  }
	
        if (mDayIndices.empty() || mBasisDayBars.empty()) { 
             return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
                mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
        }
        
        Decimal preciseInterDayChainClose = mBasisDayBars.back().getCloseValue(); 

        for (size_t i = 0; i < mDayIndices.size(); ++i) 
        {
            if (i >= mOvernightGaps.size()) {
                break; 
            }
            Decimal currentGapFactor = mOvernightGaps[i];
            Decimal preciseDayOpenAnchor = preciseInterDayChainClose * currentGapFactor; 
            
            size_t currentOriginalDayIndex = mDayIndices[i]; 
            if (currentOriginalDayIndex >= mDailyNormalizedBars.size()) {
                break;
            }
            const auto& selectedNormalizedDayBars = mDailyNormalizedBars[currentOriginalDayIndex];

            if (selectedNormalizedDayBars.empty())
            {
                preciseInterDayChainClose = preciseDayOpenAnchor; 
                continue;
            }
            
            Decimal lastUnroundedCloseForThisDay = preciseDayOpenAnchor; 

            for (size_t barIdx = 0; barIdx < selectedNormalizedDayBars.size(); ++barIdx) 
            {
                const auto& normalizedBar = selectedNormalizedDayBars[barIdx];
                auto originalDateTime = normalizedBar.getDateTime(); 

                Decimal actualOpen  = preciseDayOpenAnchor * normalizedBar.getOpenValue();
                Decimal actualHigh  = preciseDayOpenAnchor * normalizedBar.getHighValue();
                Decimal actualLow   = preciseDayOpenAnchor * normalizedBar.getLowValue();
                Decimal actualClose = preciseDayOpenAnchor * normalizedBar.getCloseValue();

		Decimal open  = RoundingPolicy<Decimal>::round(actualOpen,  mMinimumTick, mMinimumTickDiv2);
		Decimal high  = RoundingPolicy<Decimal>::round(actualHigh,  mMinimumTick, mMinimumTickDiv2);
		Decimal low   = RoundingPolicy<Decimal>::round(actualLow,   mMinimumTick, mMinimumTickDiv2);
		Decimal close = RoundingPolicy<Decimal>::round(actualClose, mMinimumTick, mMinimumTickDiv2);

		high = std::max({high, open, close});
		low  = std::min({low,  open, close});

                lastUnroundedCloseForThisDay = actualClose;
                
#ifdef SYNTHETIC_VOLUME
                Decimal volume = DecimalConstants<Decimal>::DecimalZero; 
                constructedBars.emplace_back(originalDateTime, open, high, low, close, volume, mSourceTimeSeries.getTimeFrame());
#else
                constructedBars.emplace_back(originalDateTime, open, high, low, close, 
                                             DecimalConstants<Decimal>::DecimalZero, mSourceTimeSeries.getTimeFrame());
#endif
            }
            preciseInterDayChainClose = lastUnroundedCloseForThisDay; 
        }
        
        return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
            mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
    }

private:
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
    Decimal mFirstOpen;

    std::vector<std::vector<OHLCTimeSeriesEntry<Decimal>>> mDailyNormalizedBars; 
    std::vector<OHLCTimeSeriesEntry<Decimal>> mBasisDayBars;
    std::vector<Decimal> mOvernightGaps;
    std::vector<size_t> mDayIndices;
};


  /**
   * @class SyntheticTimeSeries
   * @brief Main public wrapper for generating synthetic OHLC time series.
   * * @details
   * This class uses the Pimpl (Pointer to Implementation) idiom to select the correct 
   * shuffling algorithm (EOD vs. Intraday) based on the source data time frame and the 
   * selected Null Model.
   * * @note Implements algorithms described by Timothy Masters for Monte Carlo 
   * Permutation Testing of trading strategies.
   * * @tparam Decimal The numeric type for price and factor data.
   * @tparam LookupPolicy The lookup policy for the OHLCTimeSeries to be generated.
   * @tparam RoundingPolicy The policy to enforce tick-size validity.
   * @tparam NullModel The destruction strategy (default: N1_MaxDestruction).
   */
  template <class Decimal,
	    class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>,
	    template<class> class RoundingPolicy = NoRounding,
	    SyntheticNullModel NullModel = SyntheticNullModel::N1_MaxDestruction>
  class SyntheticTimeSeries
  {
  public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal, LookupPolicy>& aTimeSeries, 
                                 const Decimal& minimumTick,
                                 const Decimal& minimumTickDiv2)
      : mSourceTimeSeriesCopy(aTimeSeries),
        mMinimumTick(minimumTick),
        mMinimumTickDiv2(minimumTickDiv2),
        mRandGenerator()
    {
      bool isIntraday = (aTimeSeries.getTimeFrame() == TimeFrame::Duration::INTRADAY);

      if (!isIntraday)
	{
	  // EOD: choose implementation by NullModel at compile time
	  if constexpr (NullModel == SyntheticNullModel::N0_PairedDay)
	    {
	      mPimpl = std::make_unique<EodSyntheticTimeSeriesImpl_N0<Decimal, LookupPolicy, RoundingPolicy>>(mSourceTimeSeriesCopy,
													      mMinimumTick,
													      mMinimumTickDiv2);
	    }
	  else
	    {
	      // N1 (current) or N2 (defer to N1 for now)
	      mPimpl = std::make_unique<EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>(mSourceTimeSeriesCopy,
													   mMinimumTick,
													   mMinimumTickDiv2);
	    }
	}
      else
	{
	  // Intraday unchanged for now
	  mPimpl = std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>(mSourceTimeSeriesCopy,
													    mMinimumTick,
													    mMinimumTickDiv2);
	}
    }

    ~SyntheticTimeSeries() = default;

    SyntheticTimeSeries(const SyntheticTimeSeries& rhs)
      : mSourceTimeSeriesCopy(rhs.mSourceTimeSeriesCopy),
	mMinimumTick(rhs.mMinimumTick),
	mMinimumTickDiv2(rhs.mMinimumTickDiv2),
	mRandGenerator(rhs.mRandGenerator),
	mPimpl(rhs.mPimpl ? rhs.mPimpl->clone() : nullptr),
	mSyntheticTimeSeries(rhs.mSyntheticTimeSeries
			     ? std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(*rhs.mSyntheticTimeSeries)
			     : nullptr)
    {}

    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs) {
      if (this == &rhs) return *this;
      std::scoped_lock lock(mMutex, rhs.mMutex); // only if you truly need thread-safe copying
      mSourceTimeSeriesCopy = rhs.mSourceTimeSeriesCopy;
      mMinimumTick = rhs.mMinimumTick;
      mMinimumTickDiv2 = rhs.mMinimumTickDiv2;
      mRandGenerator = rhs.mRandGenerator;
      mPimpl = rhs.mPimpl ? rhs.mPimpl->clone() : nullptr;
      mSyntheticTimeSeries = rhs.mSyntheticTimeSeries
	? std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(*rhs.mSyntheticTimeSeries)
	: nullptr;
      return *this;
    }
  
    SyntheticTimeSeries(SyntheticTimeSeries&& rhs) noexcept
      : mSourceTimeSeriesCopy(std::move(rhs.mSourceTimeSeriesCopy)),
	mMinimumTick(std::move(rhs.mMinimumTick)),
	mMinimumTickDiv2(std::move(rhs.mMinimumTickDiv2)),
	mRandGenerator(std::move(rhs.mRandGenerator)),
	mPimpl(std::move(rhs.mPimpl)),
	mSyntheticTimeSeries(std::move(rhs.mSyntheticTimeSeries))
    {
      // mMutex is default-constructed (cannot be moved)
    }

    SyntheticTimeSeries& operator=(SyntheticTimeSeries&& rhs) noexcept
    {
      if (this == &rhs) return *this;

      boost::unique_lock<boost::mutex> lock1(mMutex, boost::defer_lock);
      boost::unique_lock<boost::mutex> lock2(rhs.mMutex, boost::defer_lock);
      std::lock(lock1, lock2);

      mSourceTimeSeriesCopy = std::move(rhs.mSourceTimeSeriesCopy);
      mMinimumTick          = std::move(rhs.mMinimumTick);
      mMinimumTickDiv2      = std::move(rhs.mMinimumTickDiv2);
      mRandGenerator        = std::move(rhs.mRandGenerator);
      mPimpl                = std::move(rhs.mPimpl);
      mSyntheticTimeSeries  = std::move(rhs.mSyntheticTimeSeries);

      return *this;
    }

    /**
     * @brief Generates a new synthetic series.
     * * Triggers the shuffling process via the implementation pointer and
     * stores the result in mSyntheticTimeSeries. Thread-safe.
     */
    void createSyntheticSeries()
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (!mPimpl) return; 

      mPimpl->shuffleFactors(mRandGenerator); 
      mSyntheticTimeSeries = mPimpl->buildSeries();
    }

    void reseedRNG() {
      boost::mutex::scoped_lock lock(mMutex);
      mRandGenerator.seed();
    }

    std::shared_ptr<const OHLCTimeSeries<Decimal, LookupPolicy>> getSyntheticTimeSeries() const
    {
      std::shared_ptr<const OHLCTimeSeries<Decimal, LookupPolicy>> result;
      {
	boost::mutex::scoped_lock lock(mMutex);
	result = mSyntheticTimeSeries;
      } // Lock is released here before returning
      return result;
    }

    Decimal getFirstOpen() const { 
      return mPimpl ? mPimpl->getFirstOpen() : DecimalConstants<Decimal>::DecimalZero; 
    } 

    unsigned long getNumElements() const { 
      return mPimpl ? mPimpl->getNumOriginalElements() : 0;
    } 

    const Decimal& getTick() const { return mMinimumTick; }
    const Decimal& getTickDiv2() const { return mMinimumTickDiv2; }
    
    std::vector<Decimal> getRelativeOpen()  const {
      std::vector<Decimal> result;
      {
	boost::mutex::scoped_lock lk(mMutex);
	result = mPimpl ? mPimpl->getRelativeOpenFactors() : std::vector<Decimal>();
      }
      return result;
    }
    std::vector<Decimal> getRelativeHigh()  const {
      std::vector<Decimal> result;
      {
	boost::mutex::scoped_lock lk(mMutex);
	result = mPimpl ? mPimpl->getRelativeHighFactors() : std::vector<Decimal>();
      }
      return result;
    }
    std::vector<Decimal> getRelativeLow()   const {
      std::vector<Decimal> result;
      {
	boost::mutex::scoped_lock lk(mMutex);
	result = mPimpl ? mPimpl->getRelativeLowFactors() : std::vector<Decimal>();
      }
      return result;
    }
    std::vector<Decimal> getRelativeClose() const {
      std::vector<Decimal> result;
      {
	boost::mutex::scoped_lock lk(mMutex);
	result = mPimpl ? mPimpl->getRelativeCloseFactors() : std::vector<Decimal>();
      }
      return result;
    }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolume()const {
      std::vector<Decimal> result;
      {
	boost::mutex::scoped_lock lk(mMutex);
	result = mPimpl ? mPimpl->getRelativeVolumeFactors() : std::vector<Decimal>();
      }
      return result;
    }
#endif

  private:
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeriesCopy;
    Decimal                           mMinimumTick;
    Decimal                           mMinimumTickDiv2;
    RandomMersenne                    mRandGenerator;
    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>> mPimpl;
    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> mSyntheticTimeSeries;
    mutable boost::mutex              mMutex;
  };
} // namespace mkc_timeseries

#endif // __SYNTHETIC_TIME_SERIES_H
