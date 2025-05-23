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
#include <numeric> // For std::iota
#include <memory>  // For std::unique_ptr, std::make_unique
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeries.h"       // Assumed to contain OHLCTimeSeries, TimeFrame, TradingVolume, OHLCTimeSeriesEntry
#include "VectorDecimal.h"    // Assumed to contain VectorDate
#include "RandomMersenne.h"
#include "DecimalConstants.h"
#include "number.h"           // Assumed to contain num::Round2Tick

// DEBUG: Added for debugging output
#include <iostream>
#include <iomanip>


namespace mkc_timeseries
{

// Forward declaration of the Pimpl interface and concrete implementations
template <class Decimal> class ISyntheticTimeSeriesImpl;
template <class Decimal> class EodSyntheticTimeSeriesImpl;
template <class Decimal> class IntradaySyntheticTimeSeriesImpl;

// Pimpl Interface
template <class Decimal>
class ISyntheticTimeSeriesImpl {
public:
    virtual ~ISyntheticTimeSeriesImpl() = default;

    virtual void shuffleFactors(RandomMersenne& randGenerator) = 0;
    virtual std::shared_ptr<OHLCTimeSeries<Decimal>> buildSeries() = 0;
    
    virtual Decimal getFirstOpen() const = 0;
    virtual unsigned long getNumOriginalElements() const = 0;

    // For EOD-specific getters, to satisfy "don't remove public methods"
    // IntradayImpl will return empty vectors for these.
    virtual std::vector<Decimal> getRelativeOpenFactors()  const = 0;
    virtual std::vector<Decimal> getRelativeHighFactors()  const = 0;
    virtual std::vector<Decimal> getRelativeLowFactors()   const = 0;
    virtual std::vector<Decimal> getRelativeCloseFactors() const = 0;
#ifdef SYNTHETIC_VOLUME
    virtual std::vector<Decimal> getRelativeVolumeFactors() const = 0;
#endif

    virtual std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> clone() const = 0;
};


// EOD Implementation
template <class Decimal>
class EodSyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal> {
public:
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

    EodSyntheticTimeSeriesImpl(const EodSyntheticTimeSeriesImpl& other) = default;


    void shuffleFactors(RandomMersenne& randGenerator) override {
        shuffleOverNightChangesInternal(randGenerator);
        shuffleTradingDayChangesInternal(randGenerator);
    }

    std::shared_ptr<OHLCTimeSeries<Decimal>> buildSeries() override {
        return buildEodInternal();
    }

    Decimal getFirstOpen() const override {
        return mFirstOpen;
    }
    
    unsigned long getNumOriginalElements() const override {
        return mSourceTimeSeries.getNumEntries();
    }

    std::vector<Decimal> getRelativeOpenFactors() const override { return mRelativeOpen; }
    std::vector<Decimal> getRelativeHighFactors() const override { return mRelativeHigh; }
    std::vector<Decimal> getRelativeLowFactors()  const override { return mRelativeLow; }
    std::vector<Decimal> getRelativeCloseFactors()const override { return mRelativeClose; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return mRelativeVolume; }
#endif

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> clone() const override {
        return std::make_unique<EodSyntheticTimeSeriesImpl<Decimal>>(*this);
    }

private:
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

    void shuffleOverNightChangesInternal(RandomMersenne& randGenerator)
    {
        if (mRelativeOpen.size() <= 1) return; 
        for (size_t i = mRelativeOpen.size() - 1; i > 1; --i) { // Corrected loop condition for Fisher-Yates on relevant part
            size_t j = randGenerator.DrawNumberExclusive(i) + 1; // j is in [1, i] (if DrawNumberExclusive(i) is [0, i-1])
                                                                  // This ensures mRelativeOpen[0] is not touched.
            std::swap(mRelativeOpen[i], mRelativeOpen[j]);
        }
    }

    void shuffleTradingDayChangesInternal(RandomMersenne& randGenerator)
    {
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
            Decimal preciseOpenOfDay = (i == 0) ? preciseChainPrice : preciseChainPrice * mRelativeOpen[i];
            Decimal preciseCloseOfDay = preciseOpenOfDay * mRelativeClose[i];
            
            Decimal open  = num::Round2Tick(preciseOpenOfDay, mMinimumTick, mMinimumTickDiv2);
            Decimal high  = num::Round2Tick(preciseOpenOfDay * mRelativeHigh[i], mMinimumTick, mMinimumTickDiv2);
            Decimal low   = num::Round2Tick(preciseOpenOfDay * mRelativeLow[i],  mMinimumTick, mMinimumTickDiv2);
            Decimal close = num::Round2Tick(preciseCloseOfDay, mMinimumTick, mMinimumTickDiv2);
            
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


// Intraday Implementation
template <class Decimal>
class IntradaySyntheticTimeSeriesImpl : public ISyntheticTimeSeriesImpl<Decimal> {
public:
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

    IntradaySyntheticTimeSeriesImpl(const IntradaySyntheticTimeSeriesImpl& other) = default;


    void shuffleFactors(RandomMersenne& randGenerator) override {
        shuffleIntradayInternal(randGenerator);
    }

    std::shared_ptr<OHLCTimeSeries<Decimal>> buildSeries() override {
        return buildIntradayInternal();
    }

    Decimal getFirstOpen() const override {
        return mFirstOpen;
    }

    unsigned long getNumOriginalElements() const override {
        return mSourceTimeSeries.getNumEntries();
    }

    std::vector<Decimal> getRelativeOpenFactors()  const override { return {}; }
    std::vector<Decimal> getRelativeHighFactors()  const override { return {}; }
    std::vector<Decimal> getRelativeLowFactors()   const override { return {}; }
    std::vector<Decimal> getRelativeCloseFactors() const override { return {}; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return {}; }
#endif

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal>> clone() const override {
        return std::make_unique<IntradaySyntheticTimeSeriesImpl<Decimal>>(*this);
    }

private:
    void initIntradayDataInternal()
    {
      //std::cout << std::fixed << std::setprecision(7); 
      //std::cout << "DEBUG: (Impl) initIntradayData() called." << std::endl;

        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::map<boost::gregorian::date, std::vector<Entry>> dayMap;
        
        if (mSourceTimeSeries.getNumEntries() == 0) {
            mFirstOpen = DecimalConstants<Decimal>::DecimalZero; 
            //std::cout << "DEBUG: (Impl) initIntradayData - Input timeseries is empty." << std::endl;
            return;
        }

        mFirstOpen = mSourceTimeSeries.beginRandomAccess()->getOpenValue();
        //std::cout << "DEBUG: (Impl) initIntradayData - mFirstOpen set to: " << mFirstOpen << " from date: " << mSourceTimeSeries.beginRandomAccess()->getDateTime() << std::endl;

        for (auto it = mSourceTimeSeries.beginRandomAccess(); it != mSourceTimeSeries.endRandomAccess(); ++it) {
            dayMap[it->getDateTime().date()].push_back(*it);
        }
        //std::cout << "DEBUG: (Impl) initIntradayData - dayMap populated with " << dayMap.size() << " days." << std::endl;


        if (dayMap.empty()) { 
	  //std::cout << "DEBUG: (Impl) initIntradayData - dayMap is empty after population." << std::endl;
            return; 
        }

        auto dayMapIt = dayMap.begin();
        mBasisDayBars = dayMapIt->second; 
        //std::cout << "DEBUG: (Impl) initIntradayData - Basis day (" << dayMapIt->first << ") has " << mBasisDayBars.size() << " bars." << std::endl;


        if (mBasisDayBars.empty()) {
            std::cout << "DEBUG: (Impl) initIntradayData - Basis day is empty. Cannot proceed." << std::endl;
            return; 
        }
        Decimal prevDayActualClose = mBasisDayBars.back().getCloseValue();
        //std::cout << "DEBUG: (Impl) initIntradayData - Basis day last close (prevDayActualClose initial): " << prevDayActualClose << std::endl;
        
        ++dayMapIt; 

        Decimal one = DecimalConstants<Decimal>::DecimalOne;
        int dayCounter = 0;
        for (; dayMapIt != dayMap.end(); ++dayMapIt)
        {
            dayCounter++;
            //std::cout << "DEBUG: (Impl) initIntradayData - Processing permutable day " << dayCounter << " (Date: " << dayMapIt->first << ")" << std::endl;
            const auto& currentDayBars = dayMapIt->second;
            if (currentDayBars.empty()) {
	      //std::cout << "DEBUG: (Impl) initIntradayData - Day " << dayMapIt->first << " is empty. Adding gap factor 1.0." << std::endl;
                mOvernightGaps.push_back(one); 
                mDailyNormalizedBars.emplace_back(); 
                continue;
            }

            Decimal currentDayOriginalOpen = currentDayBars.front().getOpenValue();
            //std::cout << "DEBUG: (Impl) initIntradayData - Day " << dayMapIt->first << ": OriginalOpen = " << currentDayOriginalOpen << ", PrevDayActualClose = " << prevDayActualClose << std::endl;
            
            Decimal gapFactor;
            if (prevDayActualClose != DecimalConstants<Decimal>::DecimalZero) {
                 gapFactor = currentDayOriginalOpen / prevDayActualClose;
                 mOvernightGaps.push_back(gapFactor);
            } else {
	      //std::cout << "DEBUG: (Impl) initIntradayData - Warning: Previous day close is zero for day " << dayMapIt->first << ", using gap factor 1.0" << std::endl;
                 gapFactor = one;
                 mOvernightGaps.push_back(one); 
            }
            //std::cout << "DEBUG: (Impl) initIntradayData - Day " << dayMapIt->first << ": Calculated GapFactor = " << gapFactor << std::endl;
            
            prevDayActualClose = currentDayBars.back().getCloseValue(); 
            //std::cout << "DEBUG: (Impl) initIntradayData - Day " << dayMapIt->first << ": Updated prevDayActualClose (for next gap) to: " << prevDayActualClose << std::endl;


            std::vector<Entry> normalizedBarsForThisDay;
            normalizedBarsForThisDay.reserve(currentDayBars.size());

            if (currentDayOriginalOpen != DecimalConstants<Decimal>::DecimalZero) {
                bool firstBar = true;
                for (const auto& bar : currentDayBars)
                {
                    Decimal normO = bar.getOpenValue()  / currentDayOriginalOpen;
                    Decimal normH = bar.getHighValue()  / currentDayOriginalOpen;
                    Decimal normL = bar.getLowValue()   / currentDayOriginalOpen;
                    Decimal normC = bar.getCloseValue() / currentDayOriginalOpen;

                    if (firstBar || &bar == &currentDayBars.back()) {
		      //std::cout << "DEBUG: (Impl) initIntradayData - Day " << dayMapIt->first << (firstBar ? " First Bar Norm:" : " Last Bar Norm:")
		      //                                  << " OrigO=" << bar.getOpenValue() << " NormO=" << normO
		      //          << " OrigC=" << bar.getCloseValue() << " NormC=" << normC << std::endl;
                        firstBar = false;
                    }
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
	      //std::cout << "DEBUG: (Impl) initIntradayData - Warning: Current day open is zero on " << dayMapIt->first << ", using unit factors for normalization." << std::endl;
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
        //std::cout << "DEBUG: (Impl) initIntradayData - Finished. mOvernightGaps size: " << mOvernightGaps.size() 
	//                  << ", mDailyNormalizedBars size: " << mDailyNormalizedBars.size() 
	//        << ", mDayIndices size: " << mDayIndices.size() << std::endl;
    }

    void shuffleIntradayInternal(RandomMersenne& randGenerator)
    {
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
        if (mOvernightGaps.size() > 1) {
            size_t g = mOvernightGaps.size();
            while (g > 1) { 
                size_t j = randGenerator.DrawNumberExclusive(g); 
                g--; 
                std::swap(mOvernightGaps[g], mOvernightGaps[j]); 
            }
        }
        if (mDayIndices.size() > 1) {
            size_t d = mDayIndices.size();
            while (d > 1) { 
                size_t j = randGenerator.DrawNumberExclusive(d); 
                d--; 
                std::swap(mDayIndices[d], mDayIndices[j]); 
            }
        }
    }

    std::shared_ptr<OHLCTimeSeries<Decimal>> buildIntradayInternal()
    {
      //std::cout << std::fixed << std::setprecision(7);
      //std::cout << "DEBUG: (Impl) buildIntraday() called." << std::endl;

        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::vector<Entry> constructedBars;
        size_t totalReserve = mBasisDayBars.size();
        for (const auto& v : mDailyNormalizedBars) totalReserve += v.size();
        if (totalReserve == 0 && mBasisDayBars.empty()) { 
	  //std::cout << "DEBUG: (Impl) buildIntraday - No basis bars and no normalized bars. Creating empty series." << std::endl;
             return std::make_shared<OHLCTimeSeries<Decimal>>(
                mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
        }
        constructedBars.reserve(totalReserve);

        //std::cout << "DEBUG: (Impl) buildIntraday - Adding " << mBasisDayBars.size() << " basis day bars." << std::endl;
        for (const auto& bar : mBasisDayBars) { 
            constructedBars.push_back(bar);
        }

        if (mDayIndices.empty() || mBasisDayBars.empty()) { 
	  //std::cout << "DEBUG: (Impl) buildIntraday - No permutable days or no basis day. Synthetic series will only contain basis day (if any)." << std::endl;
             return std::make_shared<OHLCTimeSeries<Decimal>>(
                mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
        }
        
        Decimal preciseInterDayChainClose = mBasisDayBars.back().getCloseValue(); 
        //std::cout << "DEBUG: (Impl) buildIntraday - Initial preciseInterDayChainClose (from basis day): " << preciseInterDayChainClose << std::endl;


        for (size_t i = 0; i < mDayIndices.size(); ++i) 
        {
	  //std::cout << "DEBUG: (Impl) buildIntraday - Processing permuted day sequence " << i << std::endl;
            if (i >= mOvernightGaps.size()) {
	      //std::cout << "DEBUG: (Impl) buildIntraday - Error: 'i' (" << i << ") is out of bounds for mOvernightGaps (size " << mOvernightGaps.size() << ")." << std::endl;
                break; 
            }
            Decimal currentGapFactor = mOvernightGaps[i];
            //std::cout << "DEBUG: (Impl) buildIntraday - Using permutedGap[" << i << "] = " << currentGapFactor << std::endl;

            Decimal preciseDayOpenAnchor = preciseInterDayChainClose * currentGapFactor; 
            //std::cout << "DEBUG: (Impl) buildIntraday - preciseDayOpenAnchor for this day = " << preciseInterDayChainClose << " * " << currentGapFactor << " = " << preciseDayOpenAnchor << std::endl;
            
            size_t currentOriginalDayIndex = mDayIndices[i]; 
            //std::cout << "DEBUG: (Impl) buildIntraday - Original day index from mDayIndices[" << i << "] = " << currentOriginalDayIndex << std::endl;

            if (currentOriginalDayIndex >= mDailyNormalizedBars.size()) {
	      //std::cout << "DEBUG: (Impl) buildIntraday - Error: currentOriginalDayIndex (" << currentOriginalDayIndex 
              //             << ") is out of bounds for mDailyNormalizedBars (size " << mDailyNormalizedBars.size() << ")." << std::endl;
                break;
            }
            const auto& selectedNormalizedDayBars = mDailyNormalizedBars[currentOriginalDayIndex];
            //std::cout << "DEBUG: (Impl) buildIntraday - Selected day (original index " << currentOriginalDayIndex << ") has " << selectedNormalizedDayBars.size() << " normalized bars." << std::endl;


            if (selectedNormalizedDayBars.empty())
            {
	      //std::cout << "DEBUG: (Impl) buildIntraday - Selected day is empty. preciseInterDayChainClose becomes preciseDayOpenAnchor: " << preciseDayOpenAnchor << std::endl;
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

                if (barIdx == 0 || barIdx == selectedNormalizedDayBars.size() - 1) {
		  /*
		  std::cout << "DEBUG: (Impl) buildIntraday - Day (orig idx " << currentOriginalDayIndex << "), Bar " << barIdx 
                               << ": NormO=" << normalizedBar.getOpenValue() << " NormC=" << normalizedBar.getCloseValue()
                               << " -> ActualO=" << actualOpen << " ActualC=" << actualClose
                               << " -> RoundedO=" << open << " RoundedC=" << close << std::endl; */
                }
                
#ifdef SYNTHETIC_VOLUME
                Decimal volume = DecimalConstants<Decimal>::DecimalZero; 
                constructedBars.emplace_back(originalDateTime, open, high, low, close, volume, mSourceTimeSeries.getTimeFrame());
#else
                constructedBars.emplace_back(originalDateTime, open, high, low, close, 
                                             DecimalConstants<Decimal>::DecimalZero, mSourceTimeSeries.getTimeFrame());
#endif
            }
            preciseInterDayChainClose = lastUnroundedCloseForThisDay; 
            //std::cout << "DEBUG: (Impl) buildIntraday - End of day (orig idx " << currentOriginalDayIndex 
	    //                      << "). Updated preciseInterDayChainClose (for next day) to: " << preciseInterDayChainClose << std::endl;
        }

        auto finalSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
            mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
        
        if (finalSeries->getNumEntries() > 0) { // Use getNumEntries for OHLCTimeSeries
	  //std::cout << "DEBUG: (Impl) buildIntraday - Finished. Synthetic series last close: " << finalSeries->CloseTimeSeries().getTimeSeriesAsVector().back() << std::endl;
        } else {
	  //std::cout << "DEBUG: (Impl) buildIntraday - Finished. Synthetic series is empty." << std::endl;
        }
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


// Main SyntheticTimeSeries class using Pimpl
template <class Decimal>
class SyntheticTimeSeries
{
public:
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

    ~SyntheticTimeSeries() = default; 

    SyntheticTimeSeries(const SyntheticTimeSeries& rhs)
      : mSourceTimeSeriesCopy(rhs.mSourceTimeSeriesCopy),
        mMinimumTick(rhs.mMinimumTick),
        mMinimumTickDiv2(rhs.mMinimumTickDiv2),
        mRandGenerator(rhs.mRandGenerator), 
        mPimpl(rhs.mPimpl ? rhs.mPimpl->clone() : nullptr),
        mSyntheticTimeSeries(rhs.mSyntheticTimeSeries ? std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries) : nullptr)
    {
    }

    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs)
    {
        if (this != &rhs) {
            boost::mutex::scoped_lock current_lock(mMutex);
            mSourceTimeSeriesCopy = rhs.mSourceTimeSeriesCopy;
            mMinimumTick          = rhs.mMinimumTick;
            mMinimumTickDiv2      = rhs.mMinimumTickDiv2;
            mRandGenerator        = rhs.mRandGenerator;
            mPimpl                = rhs.mPimpl ? rhs.mPimpl->clone() : nullptr;
            mSyntheticTimeSeries  = rhs.mSyntheticTimeSeries ? std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries) : nullptr;
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

    std::shared_ptr<const OHLCTimeSeries<Decimal>> getSyntheticTimeSeries() const
    {
        boost::mutex::scoped_lock lock(mMutex);
        return mSyntheticTimeSeries;
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
        boost::mutex::scoped_lock lk(mMutex); 
        return mPimpl ? mPimpl->getRelativeOpenFactors() : std::vector<Decimal>(); 
    }
    std::vector<Decimal> getRelativeHigh()  const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeHighFactors() : std::vector<Decimal>(); 
    }
    std::vector<Decimal> getRelativeLow()   const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeLowFactors() : std::vector<Decimal>(); 
    }
    std::vector<Decimal> getRelativeClose() const { 
        boost::mutex::scoped_lock lk(mMutex);
        return mPimpl ? mPimpl->getRelativeCloseFactors() : std::vector<Decimal>(); 
    }
#ifdef SYNTHETIC_VOLUME
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
