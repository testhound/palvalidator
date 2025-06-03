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
#include "TimeSeries.h" // OHLCTimeSeries is policy-based
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include "ShuffleUtils.h"
#include "DecimalConstants.h"
#include "number.h"

namespace mkc_timeseries
{

// Forward declaration of the Pimpl interface and concrete implementations
// Pimpl interface and classes are now templated on LookupPolicy, with a default
template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>> class ISyntheticTimeSeriesImpl;
template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>> class EodSyntheticTimeSeriesImpl;
template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>> class IntradaySyntheticTimeSeriesImpl;

/**
 * @interface ISyntheticTimeSeriesImpl
 * @brief Defines the interface for synthetic time series generator implementations.
 *
 * @tparam Decimal The numeric type for price and factor data.
 * @tparam LookupPolicy The lookup policy for the OHLCTimeSeries to be generated.
 */
template <class Decimal, class LookupPolicy> // Default removed from definition
class ISyntheticTimeSeriesImpl {
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
    virtual std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy>> clone() const = 0;
};


/**
 * @class EodSyntheticTimeSeriesImpl
 * @brief Implements EOD synthetic time series generation.
 * @tparam Decimal The numeric type for price and factor data.
 * @tparam LookupPolicy The lookup policy for the OHLCTimeSeries to be generated.
 */
template <class Decimal, class LookupPolicy> // Default removed from definition
class EodSyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy> {
public:
    EodSyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>>& sourceSeries, 
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

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy>> clone() const override {
        return std::make_unique<EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy>>(*this);
    }

private:
    void initEodDataInternal() {
        using SourceSeriesType = OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>>;
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
            
            Decimal open  = num::Round2Tick(preciseOpenOfDay, mMinimumTick, mMinimumTickDiv2);
            Decimal high  = num::Round2Tick(preciseOpenOfDay * mRelativeHigh[i], mMinimumTick, mMinimumTickDiv2);
            Decimal low   = num::Round2Tick(preciseOpenOfDay * mRelativeLow[i],  mMinimumTick, mMinimumTickDiv2);
            Decimal close = num::Round2Tick(preciseCloseOfDay, mMinimumTick, mMinimumTickDiv2);

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
    OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>> mSourceTimeSeries;
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
 * @tparam Decimal The numeric type for price and factor data.
 * @tparam LookupPolicy The lookup policy for the OHLCTimeSeries to be generated.
 */
template <class Decimal, class LookupPolicy> // Default removed from definition
class IntradaySyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy> {
public:
    IntradaySyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>>& sourceSeries, 
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

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy>> clone() const override {
        return std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy>>(*this);
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

        for (const auto& bar : mBasisDayBars) { 
            constructedBars.push_back(bar);
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

                Decimal open  = num::Round2Tick(actualOpen,  mMinimumTick, mMinimumTickDiv2);
                Decimal high  = num::Round2Tick(actualHigh,  mMinimumTick, mMinimumTickDiv2);
                Decimal low   = num::Round2Tick(actualLow,   mMinimumTick, mMinimumTickDiv2);
                Decimal close = num::Round2Tick(actualClose, mMinimumTick, mMinimumTickDiv2);

                lastUnroundedCloseForThisDay = close;
                
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
    OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>> mSourceTimeSeries;
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
 * @brief Public-facing class for generating synthetic OHLC time series.
 * @tparam Decimal The numeric type for price and factor data.
 * @tparam LookupPolicy The lookup policy for the OHLCTimeSeries to be generated by this instance.
 */
template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>>
class SyntheticTimeSeries
{
public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>>& aTimeSeries, 
                                 const Decimal& minimumTick,
                                 const Decimal& minimumTickDiv2)
      : mSourceTimeSeriesCopy(aTimeSeries),
        mMinimumTick(minimumTick),
        mMinimumTickDiv2(minimumTickDiv2),
        mRandGenerator()
    {
        bool isIntraday = (aTimeSeries.getTimeFrame() == TimeFrame::Duration::INTRADAY);
        if (!isIntraday) {
            mPimpl = std::make_unique<EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy>>(
                mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
        } else {
            mPimpl = std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy>>(
                mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
        }
    }

    ~SyntheticTimeSeries() = default;

    SyntheticTimeSeries(const SyntheticTimeSeries<Decimal, LookupPolicy>& rhs)
      : mSourceTimeSeriesCopy(rhs.mSourceTimeSeriesCopy),
        mMinimumTick(rhs.mMinimumTick),
        mMinimumTickDiv2(rhs.mMinimumTickDiv2),
        mRandGenerator(rhs.mRandGenerator),
        mPimpl(rhs.mPimpl ? rhs.mPimpl->clone() : nullptr)
    {
        if (rhs.mSyntheticTimeSeries) {
             mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(*rhs.mSyntheticTimeSeries);
        }
    }

    SyntheticTimeSeries& operator=(const SyntheticTimeSeries<Decimal, LookupPolicy>& rhs)
    {
        if (this != &rhs) {
            boost::mutex::scoped_lock current_lock(mMutex);
            mSourceTimeSeriesCopy = rhs.mSourceTimeSeriesCopy;
            mMinimumTick          = rhs.mMinimumTick;
            mMinimumTickDiv2      = rhs.mMinimumTickDiv2;
            mRandGenerator        = rhs.mRandGenerator;
            mPimpl                = rhs.mPimpl ? rhs.mPimpl->clone() : nullptr;
            if (rhs.mSyntheticTimeSeries) {
                 mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(*rhs.mSyntheticTimeSeries);
            } else {
                 mSyntheticTimeSeries.reset();
            }
        }
        return *this;
    }
    
    SyntheticTimeSeries(SyntheticTimeSeries&& rhs) noexcept = default;
    SyntheticTimeSeries& operator=(SyntheticTimeSeries&& rhs) noexcept = default;


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
    OHLCTimeSeries<Decimal, LogNLookupPolicy<Decimal>> mSourceTimeSeriesCopy;
    Decimal                           mMinimumTick;
    Decimal                           mMinimumTickDiv2;
    RandomMersenne                    mRandGenerator;
    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy>> mPimpl;
    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> mSyntheticTimeSeries;
    mutable boost::mutex              mMutex;
};

} // namespace mkc_timeseries

#endif // __SYNTHETIC_TIME_SERIES_H
