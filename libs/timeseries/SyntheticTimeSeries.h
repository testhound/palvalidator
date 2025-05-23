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
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeries.h"
#include "VectorDecimal.h" 
#include "RandomMersenne.h"
#include "DecimalConstants.h"
#include "number.h"

namespace mkc_timeseries
{

// Forward declaration of the Pimpl interface and concrete implementations
template <class Decimal> class ISyntheticTimeSeriesImpl;
template <class Decimal> class EodSyntheticTimeSeriesImpl;
template <class Decimal> class IntradaySyntheticTimeSeriesImpl;

/**
 * @interface ISyntheticTimeSeriesImpl
 * @brief Defines the interface for synthetic time series generator implementations.
 *
 * This interface is part of a Pimpl idiom, allowing different strategies
 * (e.g., End-of-Day, Intraday) for synthetic time series generation to be
 * encapsulated and used by the main SyntheticTimeSeries class.
 *
 * @tparam Decimal The numeric type for price and factor data.
 */
template <class Decimal>
class ISyntheticTimeSeriesImpl {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes. */
    virtual ~ISyntheticTimeSeriesImpl() = default;

    /**
     * @brief Shuffles the internal factors used for synthetic series generation.
     * @param randGenerator A reference to a random number generator.
     */
    virtual void shuffleFactors(RandomMersenne& randGenerator) = 0;

    /**
     * @brief Builds and returns the synthetic time series based on the shuffled factors.
     * @return A shared pointer to the generated OHLCTimeSeries.
     */
    virtual std::shared_ptr<OHLCTimeSeries<Decimal>> buildSeries() = 0;
    
    /**
     * @brief Gets the opening price of the first bar of the original source time series.
     * @return The first open price.
     */
    virtual Decimal getFirstOpen() const = 0;

    /**
     * @brief Gets the number of entries in the original source time series.
     * @return The number of original elements.
     */
    virtual unsigned long getNumOriginalElements() const = 0;

    /**
     * @brief Gets the relative open factors. (Primarily for EOD implementation).
     * @return A vector of relative open factors. Intraday implementation may return an empty vector.
     */
    virtual std::vector<Decimal> getRelativeOpenFactors()  const = 0;

    /**
     * @brief Gets the relative high factors. (Primarily for EOD implementation).
     * @return A vector of relative high factors. Intraday implementation may return an empty vector.
     */
    virtual std::vector<Decimal> getRelativeHighFactors()  const = 0;

    /**
     * @brief Gets the relative low factors. (Primarily for EOD implementation).
     * @return A vector of relative low factors. Intraday implementation may return an empty vector.
     */
    virtual std::vector<Decimal> getRelativeLowFactors()   const = 0;

    /**
     * @brief Gets the relative close factors. (Primarily for EOD implementation).
     * @return A vector of relative close factors. Intraday implementation may return an empty vector.
     */
    virtual std::vector<Decimal> getRelativeCloseFactors() const = 0;
#ifdef SYNTHETIC_VOLUME
    /**
     * @brief Gets the relative volume factors. (Primarily for EOD implementation, if SYNTHETIC_VOLUME is defined).
     * @return A vector of relative volume factors. Intraday implementation may return an empty vector.
     */
    virtual std::vector<Decimal> getRelativeVolumeFactors() const = 0;
#endif

    /**
     * @brief Clones the implementation object.
     * @return A unique_ptr to a copy of this implementation.
     */
    virtual std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> clone() const = 0;
};


/**
 * @class EodSyntheticTimeSeriesImpl
 * @brief Implements synthetic time series generation for End-of-Day (EOD) data.
 *
 * This class calculates relative price changes (open-to-previous-close, and HLC-to-open)
 * from an original EOD time series. It then shuffles these sets of relative changes
 * independently and reconstructs a new synthetic time series. This process preserves
 * the final closing price of the original series.
 *
 * @tparam Decimal The numeric type for price and factor data.
 */
template <class Decimal>
class EodSyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal> {
public:
    /**
     * @brief Constructs an EodSyntheticTimeSeriesImpl.
     * @param sourceSeries The original EOD OHLCTimeSeries to base the synthetic series on.
     * @param minimumTick The minimum price movement tick for rounding.
     * @param minimumTickDiv2 Half of the minimum tick, used for rounding decisions.
     */
    EodSyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal>& sourceSeries,
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

    /** @brief Default copy constructor. Used for cloning. */
    EodSyntheticTimeSeriesImpl(const EodSyntheticTimeSeriesImpl& other) = default;

    /**
     * @brief Shuffles the calculated EOD relative factors.
     *
     * This involves two separate shuffles:
     * 1. Overnight changes (relative open factors, excluding the first).
     * 2. Trading day changes (relative high, low, and close factors, in unison).
     * @param randGenerator A reference to a random number generator.
     */
    void shuffleFactors(RandomMersenne& randGenerator) override {
        shuffleOverNightChangesInternal(randGenerator);
        shuffleTradingDayChangesInternal(randGenerator);
    }

    /**
     * @brief Builds the synthetic EOD time series from the shuffled factors.
     * * The series is reconstructed by starting with the original first open price and
     * iteratively applying the permuted relative open factor to get the day's open,
     * then applying the permuted relative HLC factors to that open.
     * OHLC values are rounded to the specified tick size.
     * The precise inter-day price chain is maintained using unrounded values.
     * This method guarantees that the final closing price of the synthetic series
     * will match the final closing price of the original series.
     *
     * @return A shared pointer to the generated OHLCTimeSeries.
     */
    std::shared_ptr<OHLCTimeSeries<Decimal>> buildSeries() override {
        return buildEodInternal();
    }

    /** @copydoc ISyntheticTimeSeriesImpl::getFirstOpen() */
    Decimal getFirstOpen() const override {
        return mFirstOpen;
    }
    
    /** @copydoc ISyntheticTimeSeriesImpl::getNumOriginalElements() */
    unsigned long getNumOriginalElements() const override {
        return mSourceTimeSeries.getNumEntries();
    }

    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeOpenFactors() */
    std::vector<Decimal> getRelativeOpenFactors() const override { return mRelativeOpen; }
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeHighFactors() */
    std::vector<Decimal> getRelativeHighFactors() const override { return mRelativeHigh; }
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeLowFactors() */
    std::vector<Decimal> getRelativeLowFactors()  const override { return mRelativeLow; }
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeCloseFactors() */
    std::vector<Decimal> getRelativeCloseFactors()const override { return mRelativeClose; }
#ifdef SYNTHETIC_VOLUME
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeVolumeFactors() */
    std::vector<Decimal> getRelativeVolumeFactors() const override { return mRelativeVolume; }
#endif

    /** @copydoc ISyntheticTimeSeriesImpl::clone() */
    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> clone() const override {
        return std::make_unique<EodSyntheticTimeSeriesImpl<Decimal>>(*this);
    }

private:
    /**
     * @brief Initializes the EOD factors from the source time series.
     *
     * Algorithm:
     * 1. Stores the open of the first bar in `mFirstOpen`.
     * 2. For each bar in the source series:
     * a. `mRelativeOpen`: Ratio of the current bar's open to the previous bar's close.
     * The first bar's `mRelativeOpen` is set to 1.0.
     * b. `mRelativeHigh`: Ratio of the current bar's high to its open.
     * c. `mRelativeLow`: Ratio of the current bar's low to its open.
     * d. `mRelativeClose`: Ratio of the current bar's close to its open.
     * e. (If SYNTHETIC_VOLUME is defined) `mRelativeVolume`: Ratio of current bar's volume to previous bar's volume.
     * 3. Dates are stored in `mDateSeries`.
     * Handles cases where divisor prices (previous close, current open) are zero by using a factor of 1.0.
     */
    void initEodDataInternal()
    {
        using Iter = typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator;
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
        // mDateSeries is already constructed with capacity via its constructor

        mFirstOpen = mSourceTimeSeries.getOpenValue(it, 0);
#ifdef SYNTHETIC_VOLUME
        mFirstVolume = mSourceTimeSeries.getVolumeValue(it, 0);
#endif
        mRelativeOpen.push_back(one); 
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.push_back(one); 
#endif

        if (mFirstOpen != DecimalConstants<Decimal>::DecimalZero) {
            mRelativeHigh.push_back(mSourceTimeSeries.getHighValue(it,0) / mFirstOpen);
            mRelativeLow.push_back(mSourceTimeSeries.getLowValue (it,0) / mFirstOpen);
            mRelativeClose.push_back(mSourceTimeSeries.getCloseValue(it,0) / mFirstOpen);
        } else { 
            mRelativeHigh.push_back(one);
            mRelativeLow.push_back(one);
            mRelativeClose.push_back(one);
        }
        mDateSeries.addElement(mSourceTimeSeries.getDateValue(it,0));
        
        if (mSourceTimeSeries.getNumEntries() > 1) {
            Iter prev_it = it; 
            ++it;
            for (; it != mSourceTimeSeries.endRandomAccess(); ++it, ++prev_it)
            {
                Decimal currOpen  = mSourceTimeSeries.getOpenValue(it,0);
                Decimal prevClose = mSourceTimeSeries.getCloseValue(prev_it,0); 

                if (prevClose != DecimalConstants<Decimal>::DecimalZero) {
                    mRelativeOpen.push_back(currOpen / prevClose);
                } else {
                    mRelativeOpen.push_back(one); 
                }

                if (currOpen != DecimalConstants<Decimal>::DecimalZero) {
                    mRelativeHigh.push_back(mSourceTimeSeries.getHighValue(it,0) / currOpen);
                    mRelativeLow.push_back(mSourceTimeSeries.getLowValue (it,0) / currOpen);
                    mRelativeClose.push_back(mSourceTimeSeries.getCloseValue(it,0) / currOpen);
                } else {
                    mRelativeHigh.push_back(one);
                    mRelativeLow.push_back(one);
                    mRelativeClose.push_back(one);
                }
#ifdef SYNTHETIC_VOLUME
                Decimal v0 = mSourceTimeSeries.getVolumeValue(it,0);
                Decimal v1 = mSourceTimeSeries.getVolumeValue(prev_it,0);
                if (v1 > DecimalConstants<Decimal>::DecimalZero) {
                     mRelativeVolume.push_back(v0 / v1);
                } else {
                     mRelativeVolume.push_back(one);
                }
#endif
                mDateSeries.addElement(mSourceTimeSeries.getDateValue(it,0));
            }
        }
    }

    /**
     * @brief Shuffles the overnight change factors (mRelativeOpen).
     * The first element (always 1.0) is not included in the shuffle.
     * @param randGenerator Random number generator.
     */
    void shuffleOverNightChangesInternal(RandomMersenne& randGenerator)
    {
        if (mRelativeOpen.size() <= 1) return; 
        // Fisher-Yates shuffle for elements from index 1 to end.
        for (size_t i = mRelativeOpen.size() - 1; i > 1; --i) { 
            // Pick a random index j from [1, i] (inclusive) to swap with current i.
            // randGenerator.DrawNumberExclusive(i) generates in [0, i-1].
            // We want to pick from i elements (from index 1 to i).
            // So, draw from i elements, then add 1 to shift the range.
            size_t j = randGenerator.DrawNumberExclusive(i) + 1; 
            std::swap(mRelativeOpen[i], mRelativeOpen[j]);
        }
    }

    /**
     * @brief Shuffles the trading day change factors (mRelativeHigh, Low, Close, Volume) in unison.
     * @param randGenerator Random number generator.
     */
    void shuffleTradingDayChangesInternal(RandomMersenne& randGenerator)
    {
        if (mRelativeHigh.size() <= 1) return; 
        size_t i = mRelativeHigh.size();
        while (i > 1)
        {
            size_t j = randGenerator.DrawNumberExclusive(i); // j is in [0, i-1]
            i--; // i is now the last index to consider
            std::swap(mRelativeHigh[i],  mRelativeHigh[j]);
            std::swap(mRelativeLow [i],  mRelativeLow [j]);
            std::swap(mRelativeClose[i], mRelativeClose[j]);
#ifdef SYNTHETIC_VOLUME
            std::swap(mRelativeVolume[i], mRelativeVolume[j]);
#endif
        }
    }

    /** @brief Internal method to build the EOD synthetic series. */
    std::shared_ptr<OHLCTimeSeries<Decimal>> buildEodInternal()
    {
        if (mSourceTimeSeries.getNumEntries() == 0) {
             return std::make_shared<OHLCTimeSeries<Decimal>>(
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
            // For i=0, mRelativeOpen[0] is 1.0, so preciseOpenOfDay = preciseChainPrice (which is mFirstOpen).
            // For i>0, preciseChainPrice is the previous day's unrounded close.
            Decimal preciseOpenOfDay = (i == 0) ? preciseChainPrice : preciseChainPrice * mRelativeOpen[i];
            Decimal preciseCloseOfDay = preciseOpenOfDay * mRelativeClose[i];
            
            Decimal open  = num::Round2Tick(preciseOpenOfDay, mMinimumTick, mMinimumTickDiv2);
            Decimal high  = num::Round2Tick(preciseOpenOfDay * mRelativeHigh[i], mMinimumTick, mMinimumTickDiv2);
            Decimal low   = num::Round2Tick(preciseOpenOfDay * mRelativeLow[i],  mMinimumTick, mMinimumTickDiv2);
            Decimal close = num::Round2Tick(preciseCloseOfDay, mMinimumTick, mMinimumTickDiv2);

	    // **clamp to guard against rounding inversion**
	    high = std::max(high, std::max(open, close));
	    low  = std::min(low,  std::min(open, close));

            preciseChainPrice = preciseCloseOfDay; // Update precise chain with unrounded close of current day

#ifdef SYNTHETIC_VOLUME
            Decimal currentDayVolume;
            if (i == 0) { 
                 currentDayVolume = preciseChainVolume; // mFirstVolume
            } else { 
                 if (mRelativeVolume.size() > i) { // Check bounds
                    currentDayVolume = preciseChainVolume * mRelativeVolume[i];
                 } else { // Fallback if sizes mismatch (should not happen with proper init)
                    currentDayVolume = preciseChainVolume;
                 }
            }
            // Assuming volume might also need rounding, e.g., to whole numbers
            Decimal volume = num::Round2Tick(currentDayVolume, DecimalConstants<Decimal>::DecimalOne, DecimalConstants<Decimal>::DecimalZero); 
            preciseChainVolume = currentDayVolume; // Update precise volume chain

            bars.emplace_back(
                mDateSeries.getDate(i), open, high, low, close, volume, mSourceTimeSeries.getTimeFrame()
            );
#else
            bars.emplace_back(
                mDateSeries.getDate(i), open, high, low, close, DecimalConstants<Decimal>::DecimalZero, mSourceTimeSeries.getTimeFrame()
            );
#endif
        }
        return std::make_shared<OHLCTimeSeries<Decimal>>(
            mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), bars.begin(), bars.end());
    }

private:
    OHLCTimeSeries<Decimal> mSourceTimeSeries; 
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
 * @brief Implements synthetic time series generation for Intraday data.
 *
 * This algorithm involves multiple levels of permutation:
 * 1. Bars within each day (excluding a basis day) are permuted.
 * 2. Overnight gap factors (previous day's close to current day's open) are permuted.
 * 3. The order of the (permuted) days themselves is permuted.
 *
 * The reconstruction process chains these permuted components together.
 * Due to the intra-day bar permutations and the final chronological sorting of the
 * generated series, this algorithm does not guarantee that the final synthetic closing
 * price will match the original series' final closing price.
 *
 * @tparam Decimal The numeric type for price and factor data.
 */
template <class Decimal>
class IntradaySyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal> {
public:
    /**
     * @brief Constructs an IntradaySyntheticTimeSeriesImpl.
     * @param sourceSeries The original intraday OHLCTimeSeries.
     * @param minimumTick The minimum price movement tick for rounding.
     * @param minimumTickDiv2 Half of the minimum tick, used for rounding decisions.
     */
    IntradaySyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal>& sourceSeries,
                                    const Decimal& minimumTick,
                                    const Decimal& minimumTickDiv2)
      : mSourceTimeSeries(sourceSeries), 
        mMinimumTick(minimumTick),
        mMinimumTickDiv2(minimumTickDiv2),
        mFirstOpen(DecimalConstants<Decimal>::DecimalZero)
    {
        initIntradayDataInternal();
    }

    /** @brief Default copy constructor. Used for cloning. */
    IntradaySyntheticTimeSeriesImpl(const IntradaySyntheticTimeSeriesImpl& other) = default;

    /**
     * @brief Shuffles the intraday factors.
     *
     * Algorithm for shuffling:
     * 1. For each day in `mDailyNormalizedBars` (which are day structures excluding the basis day),
     * the bars within that day are permuted.
     * 2. The vector of `mOvernightGaps` is permuted.
     * 3. The vector of `mDayIndices` (which dictates the order of permutable days) is permuted.
     * @param randGenerator A reference to a random number generator.
     */
    void shuffleFactors(RandomMersenne& randGenerator) override {
        shuffleIntradayInternal(randGenerator);
    }

    /**
     * @brief Builds the synthetic intraday time series.
     *
     * Algorithm for building:
     * 1. The `mBasisDayBars` are added to the synthetic series first, unchanged.
     * 2. `preciseInterDayChainClose` is initialized with the close of the last bar of the basis day.
     * 3. The method iterates `mDayIndices.size()` times (for each permutable day slot). In each iteration `i`:
     * a. A permuted overnight gap factor is selected: `currentGapFactor = mOvernightGaps[i]`.
     * b. The anchor price for the start of the current synthetic day is calculated:
     * `preciseDayOpenAnchor = preciseInterDayChainClose * currentGapFactor`.
     * c. An original day's (internally permuted) normalized bar structure is selected using the
     * permuted day index: `selectedNormalizedDayBars = mDailyNormalizedBars[mDayIndices[i]]`.
     * d. For each `normalizedBar` in `selectedNormalizedDayBars`:
     * i.  Actual OHLC values are calculated: `actualOHLC = preciseDayOpenAnchor * normalizedBar.OHLCFactor`.
     * (where `normalizedBar.OHLCFactor` means `normalizedBar.getOpenValue()`, etc., which are
     * `OriginalBarOHLC / OriginalDayOpen_for_that_structure`).
     * ii. These `actualOHLC` values are rounded using `num::Round2Tick`.
     * iii.The bar is added to the `constructedBars` vector using its original timestamp.
     * e. `preciseInterDayChainClose` is updated with the *unrounded* close of the last bar constructed for this day.
     * 4. The final `OHLCTimeSeries` is created from `constructedBars`, which sorts all entries by timestamp.
     *
     * @note This process does not guarantee that the final closing price of the synthetic series
     * will match the original series' final closing price.
     * @return A shared pointer to the generated OHLCTimeSeries.
     */
    std::shared_ptr<OHLCTimeSeries<Decimal>> buildSeries() override {
        return buildIntradayInternal();
    }

    /** @copydoc ISyntheticTimeSeriesImpl::getFirstOpen() */
    Decimal getFirstOpen() const override {
        return mFirstOpen;
    }

    /** @copydoc ISyntheticTimeSeriesImpl::getNumOriginalElements() */
    unsigned long getNumOriginalElements() const override {
        return mSourceTimeSeries.getNumEntries();
    }

    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeOpenFactors() */
    std::vector<Decimal> getRelativeOpenFactors()  const override { return {}; }
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeHighFactors() */
    std::vector<Decimal> getRelativeHighFactors()  const override { return {}; }
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeLowFactors() */
    std::vector<Decimal> getRelativeLowFactors()   const override { return {}; }
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeCloseFactors() */
    std::vector<Decimal> getRelativeCloseFactors() const override { return {}; }
#ifdef SYNTHETIC_VOLUME
    /** @copydoc ISyntheticTimeSeriesImpl::getRelativeVolumeFactors() */
    std::vector<Decimal> getRelativeVolumeFactors() const override { return {}; }
#endif

    /** @copydoc ISyntheticTimeSeriesImpl::clone() */
    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> clone() const override {
        return std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal>>(*this);
    }

private:
    /**
     * @brief Initializes data structures for intraday permutation.
     *
     * Algorithm:
     * 1. Sets `mFirstOpen` to the open of the very first bar of the `mSourceTimeSeries`.
     * 2. Segments `mSourceTimeSeries` into daily blocks using a `std::map<date, std::vector<Entry>>`.
     * 3. The first day's bars are stored in `mBasisDayBars`. These are not normalized or internally permuted later.
     * 4. For each subsequent day (permutable days):
     * a. Calculates the overnight gap factor: `currentDayOriginalOpen / prevDayActualClose`. This is stored in `mOvernightGaps`.
     * `prevDayActualClose` is the close of the last bar of the previously processed day (or basis day).
     * b. Normalizes the bars of the current day: Each OHLC value of each bar in the current day is divided
     * by `currentDayOriginalOpen`. These normalized day structures are stored in `mDailyNormalizedBars`.
     * If `currentDayOriginalOpen` is zero, unit factors (1.0) are used for normalization.
     * 5. Initializes `mDayIndices` as a sequence `0, 1, ..., (num_permutable_days - 1)`.
     */
    void initIntradayDataInternal()
    {
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

    /**
     * @brief Internal method to shuffle intraday factors.
     * @param randGenerator Random number generator.
     */
    void shuffleIntradayInternal(RandomMersenne& randGenerator)
    {
        // Shuffle intraday bars within each day
        for (auto& dayBarsVec : mDailyNormalizedBars) {
            if (dayBarsVec.size() > 1) {
                size_t n = dayBarsVec.size();
                while (n > 1) { 
                    size_t j = randGenerator.DrawNumberExclusive(n); 
                    n--; 
                    std::swap(dayBarsVec[n], dayBarsVec[j]); 
                }
            }
        }
        // Shuffle overnight gaps
        if (mOvernightGaps.size() > 1) {
            size_t g = mOvernightGaps.size();
            while (g > 1) { 
                size_t j = randGenerator.DrawNumberExclusive(g); 
                g--; 
                std::swap(mOvernightGaps[g], mOvernightGaps[j]); 
            }
        }
        // Shuffle day order
        if (mDayIndices.size() > 1) {
            size_t d = mDayIndices.size();
            while (d > 1) { 
                size_t j = randGenerator.DrawNumberExclusive(d); 
                d--; 
                std::swap(mDayIndices[d], mDayIndices[j]); 
            }
        }
    }

    /** @brief Internal method to build the intraday synthetic series. */
    std::shared_ptr<OHLCTimeSeries<Decimal>> buildIntradayInternal()
    {
        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::vector<Entry> constructedBars;
        size_t totalReserve = mBasisDayBars.size();
        for (const auto& v : mDailyNormalizedBars) totalReserve += v.size();
        if (totalReserve == 0 && mBasisDayBars.empty()) { 
             return std::make_shared<OHLCTimeSeries<Decimal>>(
                mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
        }
        constructedBars.reserve(totalReserve);

        for (const auto& bar : mBasisDayBars) { 
            constructedBars.push_back(bar);
        }

        if (mDayIndices.empty() || mBasisDayBars.empty()) { 
             return std::make_shared<OHLCTimeSeries<Decimal>>(
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
                
                lastUnroundedCloseForThisDay = actualClose; 

                Decimal open  = num::Round2Tick(actualOpen,  mMinimumTick, mMinimumTickDiv2);
                Decimal high  = num::Round2Tick(actualHigh,  mMinimumTick, mMinimumTickDiv2);
                Decimal low   = num::Round2Tick(actualLow,   mMinimumTick, mMinimumTickDiv2);
                Decimal close = num::Round2Tick(actualClose, mMinimumTick, mMinimumTickDiv2);
                
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

        auto finalSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
            mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
        
        return finalSeries;
    }

private:
    OHLCTimeSeries<Decimal> mSourceTimeSeries; 
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
    Decimal mFirstOpen;

    std::vector<std::vector<OHLCTimeSeriesEntry<Decimal>>> mDailyNormalizedBars; 
    std::vector<OHLCTimeSeriesEntry<Decimal>>      mBasisDayBars;        
    std::vector<Decimal>                           mOvernightGaps;       
    std::vector<size_t>                            mDayIndices;
};


/**
 * @class SyntheticTimeSeries
 * @brief Public-facing class for generating synthetic OHLC time series.
 *
 * This class uses the Pimpl (Pointer to Implementation) idiom to delegate the actual
 * generation logic to either an EOD (End-of-Day) or Intraday implementation based
 * on the time frame of the input series.
 *
 * The EOD algorithm typically preserves the final closing price of the original series,
 * while the Intraday algorithm, due to its more complex multi-level permutations
 * (including intra-day bar shuffling), does not guarantee this property but aims to
 * preserve other statistical characteristics like the distribution of overnight gaps
 * and daily bar count distributions.
 *
 * @tparam Decimal The numeric type for price and factor data.
 */
template <class Decimal>
class SyntheticTimeSeries
{
public:
    /**
     * @brief Constructs a SyntheticTimeSeries object.
     *
     * Based on the `timeFrame` of `aTimeSeries`, it internally creates either an
     * `EodSyntheticTimeSeriesImpl` or an `IntradaySyntheticTimeSeriesImpl`.
     *
     * @param aTimeSeries The original OHLCTimeSeries to use as a basis. A copy is made.
     * @param minimumTick The minimum price movement (tick size) for rounding final values.
     * @param minimumTickDiv2 Half of the minimumTick, used in the rounding logic.
     */
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries,
                                 const Decimal& minimumTick,
                                 const Decimal& minimumTickDiv2)
      : mSourceTimeSeriesCopy(aTimeSeries), 
        mMinimumTick(minimumTick),
        mMinimumTickDiv2(minimumTickDiv2),
        mRandGenerator() 
    {
        bool isIntraday = (aTimeSeries.getTimeFrame() == TimeFrame::Duration::INTRADAY);
        if (!isIntraday) {
            mPimpl = std::make_unique<EodSyntheticTimeSeriesImpl<Decimal>>(mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
        } else {
            mPimpl = std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal>>(mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
        }
    }

    /** @brief Default destructor. Manages the Pimpl lifetime via std::unique_ptr. */
    ~SyntheticTimeSeries() = default; 

    /**
     * @brief Copy constructor. Performs a deep copy of the implementation.
     * @param rhs The SyntheticTimeSeries object to copy.
     */
    SyntheticTimeSeries(const SyntheticTimeSeries& rhs)
      : mSourceTimeSeriesCopy(rhs.mSourceTimeSeriesCopy),
        mMinimumTick(rhs.mMinimumTick),
        mMinimumTickDiv2(rhs.mMinimumTickDiv2),
        mRandGenerator(rhs.mRandGenerator), 
        mPimpl(rhs.mPimpl ? rhs.mPimpl->clone() : nullptr),
        mSyntheticTimeSeries(rhs.mSyntheticTimeSeries ? std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries) : nullptr)
    {
    }

    /**
     * @brief Copy assignment operator. Performs a deep copy of the implementation.
     * @param rhs The SyntheticTimeSeries object to assign from.
     * @return A reference to this object.
     */
    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs)
    {
        if (this != &rhs) {
            boost::mutex::scoped_lock current_lock(mMutex); // Ensure thread safety during assignment
            // Copy data members
            mSourceTimeSeriesCopy = rhs.mSourceTimeSeriesCopy;
            mMinimumTick          = rhs.mMinimumTick;
            mMinimumTickDiv2      = rhs.mMinimumTickDiv2;
            mRandGenerator        = rhs.mRandGenerator; // Assuming RandomMersenne is copyable
            
            // Clone Pimpl
            mPimpl                = rhs.mPimpl ? rhs.mPimpl->clone() : nullptr;
            
            // Copy the resulting synthetic series (if it exists)
            mSyntheticTimeSeries  = rhs.mSyntheticTimeSeries ? std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries) : nullptr;
        }
        return *this;
    }
    
    /** @brief Default move constructor. */
    SyntheticTimeSeries(SyntheticTimeSeries&& rhs) noexcept = default;
    /** @brief Default move assignment operator. */
    SyntheticTimeSeries& operator=(SyntheticTimeSeries&& rhs) noexcept = default;


    /**
     * @brief Creates the synthetic time series.
     *
     * This method delegates to the Pimpl's `shuffleFactors` and `buildSeries` methods.
     * The generated series is stored internally and can be retrieved via `getSyntheticTimeSeries()`.
     * This operation is guarded by a mutex for thread safety.
     */
    void createSyntheticSeries()
    {
        boost::mutex::scoped_lock lock(mMutex);
        if (!mPimpl) return; 

        mPimpl->shuffleFactors(mRandGenerator); 
        mSyntheticTimeSeries = mPimpl->buildSeries();
    }

    /**
     * @brief Retrieves the generated synthetic time series.
     * @return A const shared_ptr to the synthetic OHLCTimeSeries. Returns nullptr if `createSyntheticSeries()` has not been called.
     * This operation is guarded by a mutex for thread safety.
     */
    std::shared_ptr<const OHLCTimeSeries<Decimal>> getSyntheticTimeSeries() const
    {
        boost::mutex::scoped_lock lock(mMutex);
        return mSyntheticTimeSeries;
    }

    /** * @brief Gets the opening price of the first bar of the original source time series.
     * @return The first open price. Returns zero if the Pimpl is not initialized.
     */
    Decimal getFirstOpen() const { 
        return mPimpl ? mPimpl->getFirstOpen() : DecimalConstants<Decimal>::DecimalZero; 
    } 

    /** * @brief Gets the number of entries in the original source time series.
     * @return The number of original elements. Returns 0 if the Pimpl is not initialized.
     */
    unsigned long getNumElements() const { 
        return mPimpl ? mPimpl->getNumOriginalElements() : 0;
    } 

    /** @brief Gets the minimum tick size used for rounding. */
    const Decimal& getTick() const { return mMinimumTick; }
    /** @brief Gets half of the minimum tick size. */
    const Decimal& getTickDiv2() const { return mMinimumTickDiv2; }
    
    /** * @brief Gets the relative open factors (EOD specific).
     * @return Vector of factors. Empty if Intraday or Pimpl not initialized.
     * Guarded by a mutex.
     */
    std::vector<Decimal> getRelativeOpen()  const { 
        boost::mutex::scoped_lock lk(mMutex); 
        return mPimpl ? mPimpl->getRelativeOpenFactors() : std::vector<Decimal>(); 
    }
    /** * @brief Gets the relative high factors (EOD specific).
     * @return Vector of factors. Empty if Intraday or Pimpl not initialized.
     * Guarded by a mutex.
     */
    std::vector<Decimal> getRelativeHigh()  const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeHighFactors() : std::vector<Decimal>(); 
    }
    /** * @brief Gets the relative low factors (EOD specific).
     * @return Vector of factors. Empty if Intraday or Pimpl not initialized.
     * Guarded by a mutex.
     */
    std::vector<Decimal> getRelativeLow()   const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeLowFactors() : std::vector<Decimal>(); 
    }
    /** * @brief Gets the relative close factors (EOD specific).
     * @return Vector of factors. Empty if Intraday or Pimpl not initialized.
     * Guarded by a mutex.
     */
    std::vector<Decimal> getRelativeClose() const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeCloseFactors() : std::vector<Decimal>(); 
    }
#ifdef SYNTHETIC_VOLUME
    /** * @brief Gets the relative volume factors (EOD specific).
     * @return Vector of factors. Empty if Intraday or Pimpl not initialized.
     * Guarded by a mutex.
     */
    std::vector<Decimal> getRelativeVolume()const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeVolumeFactors() : std::vector<Decimal>(); 
    }
#endif

private:
    OHLCTimeSeries<Decimal>           mSourceTimeSeriesCopy; 
    Decimal                           mMinimumTick;
    Decimal                           mMinimumTickDiv2;
    RandomMersenne                    mRandGenerator; 

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> mPimpl;
    
    std::shared_ptr<OHLCTimeSeries<Decimal>>      mSyntheticTimeSeries;
    mutable boost::mutex                          mMutex;
};

} // namespace mkc_timeseries

#endif // __SYNTHETIC_TIME_SERIES_H
