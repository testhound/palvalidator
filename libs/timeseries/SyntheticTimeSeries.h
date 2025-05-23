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
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeries.h"
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include "DecimalConstants.h"
#include "number.h" // Ensure Round2Tick is available

// DEBUG: Added for debugging output
#include <iostream>
#include <iomanip>


namespace mkc_timeseries
{

template <class Decimal>
class SyntheticTimeSeries
{
public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal>& aTimeSeries,
                                 const Decimal& minimumTick,
                                 const Decimal& minimumTickDiv2)
      : mTimeSeries(aTimeSeries)
      , mDateSeries(aTimeSeries.getNumEntries()) 
      , mRelativeOpen()     
      , mRelativeHigh()     
      , mRelativeLow()      
      , mRelativeClose()    
#ifdef SYNTHETIC_VOLUME
      , mRelativeVolume()   
#endif
      , mFirstOpen(DecimalConstants<Decimal>::DecimalZero) 
#ifdef SYNTHETIC_VOLUME
      , mFirstVolume(DecimalConstants<Decimal>::DecimalZero) 
#endif
      , mRandGenerator()
      , mIsIntraday(aTimeSeries.getTimeFrame() == TimeFrame::Duration::INTRADAY)
      , mSyntheticTimeSeries(
            std::make_shared<OHLCTimeSeries<Decimal>>(
                aTimeSeries.getTimeFrame(),
                aTimeSeries.getVolumeUnits(),
                aTimeSeries.getNumEntries())) 
      , mMinimumTick(minimumTick)
      , mMinimumTickDiv2(minimumTickDiv2)
    {
        if (!mIsIntraday)
            initEodData();
        else
            initIntradayData();
    }

    SyntheticTimeSeries(const SyntheticTimeSeries& rhs)
      : mTimeSeries(rhs.mTimeSeries)
      , mDateSeries(rhs.mDateSeries)
      , mRelativeOpen(rhs.mRelativeOpen)
      , mRelativeHigh(rhs.mRelativeHigh)
      , mRelativeLow(rhs.mRelativeLow)
      , mRelativeClose(rhs.mRelativeClose)
#ifdef SYNTHETIC_VOLUME
      , mRelativeVolume(rhs.mRelativeVolume)
#endif
      , mFirstOpen(rhs.mFirstOpen) 
#ifdef SYNTHETIC_VOLUME
      , mFirstVolume(rhs.mFirstVolume)
#endif
      , mRandGenerator(rhs.mRandGenerator)
      , mIsIntraday(rhs.mIsIntraday)
      , mDailyNormalizedBars(rhs.mDailyNormalizedBars)
      , mBasisDayBars(rhs.mBasisDayBars)
      , mOvernightGaps(rhs.mOvernightGaps)
      , mDayIndices(rhs.mDayIndices)
      , mSyntheticTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries))
      , mMinimumTick(rhs.mMinimumTick)
      , mMinimumTickDiv2(rhs.mMinimumTickDiv2)
    {
    }

    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs)
    {
        if (this != &rhs) {
            boost::mutex::scoped_lock current_lock(mMutex); 

            mTimeSeries          = rhs.mTimeSeries;
            mDateSeries          = rhs.mDateSeries;
            mRelativeOpen        = rhs.mRelativeOpen;
            mRelativeHigh        = rhs.mRelativeHigh;
            mRelativeLow         = rhs.mRelativeLow;
            mRelativeClose       = rhs.mRelativeClose;
#ifdef SYNTHETIC_VOLUME
            mRelativeVolume      = rhs.mRelativeVolume;
#endif
            mFirstOpen           = rhs.mFirstOpen; 
#ifdef SYNTHETIC_VOLUME
            mFirstVolume         = rhs.mFirstVolume;
#endif
            mRandGenerator       = rhs.mRandGenerator;
            mIsIntraday          = rhs.mIsIntraday;
            mDailyNormalizedBars = rhs.mDailyNormalizedBars;
            mBasisDayBars        = rhs.mBasisDayBars;
            mOvernightGaps       = rhs.mOvernightGaps;
            mDayIndices          = rhs.mDayIndices;
            mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(*rhs.mSyntheticTimeSeries);
            mMinimumTick         = rhs.mMinimumTick;
            mMinimumTickDiv2     = rhs.mMinimumTickDiv2;
        }
        return *this;
    }

    void createSyntheticSeries()
    {
        boost::mutex::scoped_lock lock(mMutex);
        if (!mIsIntraday)
        {
            shuffleOverNightChanges();
            shuffleTradingDayChanges();
            buildEod();
        }
        else
        {
            shuffleIntraday();
            buildIntraday();
        }
    }

    std::shared_ptr<const OHLCTimeSeries<Decimal>> getSyntheticTimeSeries() const
    {
        boost::mutex::scoped_lock lock(mMutex);
        return mSyntheticTimeSeries;
    }

    Decimal getFirstOpen() const { return mFirstOpen; } 
    unsigned long getNumElements() const { return mTimeSeries.getNumEntries(); } 
    const Decimal& getTick() const { return mMinimumTick; }
    const Decimal& getTickDiv2() const { return mMinimumTickDiv2; }
    
    std::vector<Decimal> getRelativeOpen()  const { boost::mutex::scoped_lock lk(mMutex); return mRelativeOpen; }
    std::vector<Decimal> getRelativeHigh()  const { boost::mutex::scoped_lock lk(mMutex); return mRelativeHigh; }
    std::vector<Decimal> getRelativeLow()   const { boost::mutex::scoped_lock lk(mMutex); return mRelativeLow; }
    std::vector<Decimal> getRelativeClose() const { boost::mutex::scoped_lock lk(mMutex); return mRelativeClose; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolume()const { boost::mutex::scoped_lock lk(mMutex); return mRelativeVolume; }
#endif

private:
    OHLCTimeSeries<Decimal>           mTimeSeries; 
    VectorDate                        mDateSeries; // Initialized with numEntries, internal vector is reserved
    std::vector<Decimal>              mRelativeOpen, mRelativeHigh, mRelativeLow, mRelativeClose; 
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal>              mRelativeVolume; 
#endif
    Decimal                           mFirstOpen;   
#ifdef SYNTHETIC_VOLUME
    Decimal                           mFirstVolume; 
#endif
    RandomMersenne                    mRandGenerator;

    bool                                           mIsIntraday;
    std::vector<std::vector<OHLCTimeSeriesEntry<Decimal>>> mDailyNormalizedBars; 
    std::vector<OHLCTimeSeriesEntry<Decimal>>      mBasisDayBars;        
    std::vector<Decimal>                           mOvernightGaps;       
    std::vector<size_t>                            mDayIndices;          

    std::shared_ptr<OHLCTimeSeries<Decimal>>      mSyntheticTimeSeries; 
    Decimal                                        mMinimumTick, mMinimumTickDiv2;
    mutable boost::mutex                          mMutex;


    void initEodData()
    {
        using Iter = typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator;
        if (mTimeSeries.getNumEntries() == 0) {
            mFirstOpen = DecimalConstants<Decimal>::DecimalZero;
#ifdef SYNTHETIC_VOLUME
            mFirstVolume = DecimalConstants<Decimal>::DecimalZero;
#endif
            return;
        }

        Iter it = mTimeSeries.beginRandomAccess(); 
        Decimal one = DecimalConstants<Decimal>::DecimalOne;

        mRelativeOpen.reserve(mTimeSeries.getNumEntries());
        mRelativeHigh.reserve(mTimeSeries.getNumEntries());
        mRelativeLow.reserve(mTimeSeries.getNumEntries());
        mRelativeClose.reserve(mTimeSeries.getNumEntries());
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.reserve(mTimeSeries.getNumEntries());
#endif
        // mDateSeries is already constructed with numEntries, its internal vector is reserved.
        // Remove: mDateSeries.reserve(mTimeSeries.getNumEntries());


        mFirstOpen = mTimeSeries.getOpenValue(it, 0);
#ifdef SYNTHETIC_VOLUME
        mFirstVolume = mTimeSeries.getVolumeValue(it, 0);
#endif
        mRelativeOpen.push_back(one); 
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.push_back(one); 
#endif

        if (mFirstOpen != DecimalConstants<Decimal>::DecimalZero) {
            mRelativeHigh.push_back(mTimeSeries.getHighValue(it,0) / mFirstOpen);
            mRelativeLow.push_back(mTimeSeries.getLowValue (it,0) / mFirstOpen);
            mRelativeClose.push_back(mTimeSeries.getCloseValue(it,0) / mFirstOpen);
        } else { 
            mRelativeHigh.push_back(one);
            mRelativeLow.push_back(one);
            mRelativeClose.push_back(one);
        }
        mDateSeries.addElement(mTimeSeries.getDateValue(it,0));
        
        if (mTimeSeries.getNumEntries() > 1) {
            Iter prev_it = it; 
            ++it;
            for (; it != mTimeSeries.endRandomAccess(); ++it, ++prev_it)
            {
                Decimal currOpen  = mTimeSeries.getOpenValue(it,0);
                Decimal prevClose = mTimeSeries.getCloseValue(prev_it,0); 

                if (prevClose != DecimalConstants<Decimal>::DecimalZero) {
                    mRelativeOpen.push_back(currOpen / prevClose);
                } else {
                    mRelativeOpen.push_back(one); 
                }

                if (currOpen != DecimalConstants<Decimal>::DecimalZero) {
                    mRelativeHigh.push_back(mTimeSeries.getHighValue(it,0) / currOpen);
                    mRelativeLow.push_back(mTimeSeries.getLowValue (it,0) / currOpen);
                    mRelativeClose.push_back(mTimeSeries.getCloseValue(it,0) / currOpen);
                } else {
                    mRelativeHigh.push_back(one);
                    mRelativeLow.push_back(one);
                    mRelativeClose.push_back(one);
                }
#ifdef SYNTHETIC_VOLUME
                Decimal v0 = mTimeSeries.getVolumeValue(it,0);
                Decimal v1 = mTimeSeries.getVolumeValue(prev_it,0);
                if (v1 > DecimalConstants<Decimal>::DecimalZero) {
                     mRelativeVolume.push_back(v0 / v1);
                } else {
                     mRelativeVolume.push_back(one);
                }
#endif
                mDateSeries.addElement(mTimeSeries.getDateValue(it,0));
            }
        }
    }

    void shuffleOverNightChanges()
    {
        if (mRelativeOpen.size() <= 1) return; 
        
        for (size_t i = mRelativeOpen.size() - 1; i > 1; --i) {
            size_t j = mRandGenerator.DrawNumberExclusive(i) + 1; 
            if (j > i) j = i; 
            std::swap(mRelativeOpen[i], mRelativeOpen[j]);
        }
    }


    void shuffleTradingDayChanges()
    {
        if (mRelativeHigh.size() <= 1) return; 
        size_t i = mRelativeHigh.size();
        while (i > 1)
        {
            size_t j = mRandGenerator.DrawNumberExclusive(i);
            i--;
            std::swap(mRelativeHigh[i],  mRelativeHigh[j]);
            std::swap(mRelativeLow [i],  mRelativeLow [j]);
            std::swap(mRelativeClose[i], mRelativeClose[j]);
#ifdef SYNTHETIC_VOLUME
            std::swap(mRelativeVolume[i], mRelativeVolume[j]);
#endif
        }
    }

    void buildEod()
    {
        if (mTimeSeries.getNumEntries() == 0) {
             mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
                mTimeSeries.getTimeFrame(), mTimeSeries.getVolumeUnits());
            return;
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
            
            Decimal open  = num::Round2Tick(preciseOpenOfDay, getTick(), getTickDiv2());
            Decimal high  = num::Round2Tick(preciseOpenOfDay * mRelativeHigh[i], getTick(), getTickDiv2());
            Decimal low   = num::Round2Tick(preciseOpenOfDay * mRelativeLow[i],  getTick(), getTickDiv2());
            Decimal close = num::Round2Tick(preciseCloseOfDay, getTick(), getTickDiv2());
            
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
                mDateSeries.getDate(i), open, high, low, close, volume, mTimeSeries.getTimeFrame()
            );
#else
            bars.emplace_back(
                mDateSeries.getDate(i), open, high, low, close, DecimalConstants<Decimal>::DecimalZero, mTimeSeries.getTimeFrame()
            );
#endif
        }

        mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
            mTimeSeries.getTimeFrame(), mTimeSeries.getVolumeUnits(), bars.begin(), bars.end());
    }


    void initIntradayData()
    {
        std::cout << std::fixed << std::setprecision(7); 
        std::cout << "DEBUG: initIntradayData() called." << std::endl;

        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::map<boost::gregorian::date, std::vector<Entry>> dayMap;
        
        if (mTimeSeries.getNumEntries() == 0) {
            mFirstOpen = DecimalConstants<Decimal>::DecimalZero; 
            std::cout << "DEBUG: initIntradayData - Input timeseries is empty." << std::endl;
            return;
        }

        mFirstOpen = mTimeSeries.beginRandomAccess()->getOpenValue();
        std::cout << "DEBUG: initIntradayData - mFirstOpen set to: " << mFirstOpen << " from date: " << mTimeSeries.beginRandomAccess()->getDateTime() << std::endl;

        for (auto it = mTimeSeries.beginRandomAccess(); it != mTimeSeries.endRandomAccess(); ++it) {
            dayMap[it->getDateTime().date()].push_back(*it);
        }
        std::cout << "DEBUG: initIntradayData - dayMap populated with " << dayMap.size() << " days." << std::endl;


        if (dayMap.empty()) { 
            std::cout << "DEBUG: initIntradayData - dayMap is empty after population (should not happen if mTimeSeries not empty)." << std::endl;
            return; 
        }

        auto dayMapIt = dayMap.begin();
        mBasisDayBars = dayMapIt->second;
        std::cout << "DEBUG: initIntradayData - Basis day (" << dayMapIt->first << ") has " << mBasisDayBars.size() << " bars." << std::endl;


        if (mBasisDayBars.empty()) {
            std::cout << "DEBUG: initIntradayData - Basis day is empty. Cannot proceed." << std::endl;
            return; 
        }
        Decimal prevDayActualClose = mBasisDayBars.back().getCloseValue();
        std::cout << "DEBUG: initIntradayData - Basis day last close (prevDayActualClose initial): " << prevDayActualClose << std::endl;
        
        ++dayMapIt; 

        Decimal one = DecimalConstants<Decimal>::DecimalOne;
        int dayCounter = 0;
        for (; dayMapIt != dayMap.end(); ++dayMapIt)
        {
            dayCounter++;
            std::cout << "DEBUG: initIntradayData - Processing permutable day " << dayCounter << " (Date: " << dayMapIt->first << ")" << std::endl;
            const auto& currentDayBars = dayMapIt->second;
            if (currentDayBars.empty()) {
                std::cout << "DEBUG: initIntradayData - Day " << dayMapIt->first << " is empty. Adding gap factor 1.0." << std::endl;
                mOvernightGaps.push_back(one); 
                mDailyNormalizedBars.emplace_back(); 
                continue;
            }

            Decimal currentDayOriginalOpen = currentDayBars.front().getOpenValue();
            std::cout << "DEBUG: initIntradayData - Day " << dayMapIt->first << ": OriginalOpen = " << currentDayOriginalOpen << ", PrevDayActualClose = " << prevDayActualClose << std::endl;
            
            Decimal gapFactor;
            if (prevDayActualClose != DecimalConstants<Decimal>::DecimalZero) {
                 gapFactor = currentDayOriginalOpen / prevDayActualClose;
                 mOvernightGaps.push_back(gapFactor);
            } else {
                 std::cout << "DEBUG: initIntradayData - Warning: Previous day close is zero for day " << dayMapIt->first << ", using gap factor 1.0" << std::endl;
                 gapFactor = one;
                 mOvernightGaps.push_back(one); 
            }
            std::cout << "DEBUG: initIntradayData - Day " << dayMapIt->first << ": Calculated GapFactor = " << gapFactor << std::endl;
            
            prevDayActualClose = currentDayBars.back().getCloseValue(); 
            std::cout << "DEBUG: initIntradayData - Day " << dayMapIt->first << ": Updated prevDayActualClose (for next gap) to: " << prevDayActualClose << std::endl;


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
                        std::cout << "DEBUG: initIntradayData - Day " << dayMapIt->first << (firstBar ? " First Bar Norm:" : " Last Bar Norm:")
                                  << " OrigO=" << bar.getOpenValue() << " NormO=" << normO
                                  << " OrigC=" << bar.getCloseValue() << " NormC=" << normC << std::endl;
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
                        volumeFactor, mTimeSeries.getTimeFrame());
#else
                    normalizedBarsForThisDay.emplace_back(
                        bar.getDateTime(), normO, normH, normL, normC,
                        DecimalConstants<Decimal>::DecimalZero, mTimeSeries.getTimeFrame());
#endif
                }
            } else { 
                 std::cout << "DEBUG: initIntradayData - Warning: Current day open is zero on " << dayMapIt->first << ", using unit factors for normalization." << std::endl;
                for (const auto& bar : currentDayBars) { 
                     normalizedBarsForThisDay.emplace_back(
                        bar.getDateTime(), one, one, one, one, 
#ifdef SYNTHETIC_VOLUME
                        one, 
#else
                        DecimalConstants<Decimal>::DecimalZero,
#endif
                        mTimeSeries.getTimeFrame());
                }
            }
            mDailyNormalizedBars.push_back(std::move(normalizedBarsForThisDay));
        }

        mDayIndices.resize(mDailyNormalizedBars.size());
        std::iota(mDayIndices.begin(), mDayIndices.end(), 0u); 
        std::cout << "DEBUG: initIntradayData - Finished. mOvernightGaps size: " << mOvernightGaps.size() 
                  << ", mDailyNormalizedBars size: " << mDailyNormalizedBars.size() 
                  << ", mDayIndices size: " << mDayIndices.size() << std::endl;
    }

    void shuffleIntraday()
    {
        for (auto& dayBarsVec : mDailyNormalizedBars) {
            if (dayBarsVec.size() > 1) {
                size_t n = dayBarsVec.size();
                while (n > 1) { 
                    size_t j = mRandGenerator.DrawNumberExclusive(n); 
                    n--; 
                    std::swap(dayBarsVec[n], dayBarsVec[j]); 
                }
            }
        }
        if (mOvernightGaps.size() > 1) {
            size_t g = mOvernightGaps.size();
            while (g > 1) { 
                size_t j = mRandGenerator.DrawNumberExclusive(g); 
                g--; 
                std::swap(mOvernightGaps[g], mOvernightGaps[j]); 
            }
        }
        if (mDayIndices.size() > 1) {
            size_t d = mDayIndices.size();
            while (d > 1) { 
                size_t j = mRandGenerator.DrawNumberExclusive(d); 
                d--; 
                std::swap(mDayIndices[d], mDayIndices[j]); 
            }
        }
    }

    void buildIntraday()
    {
        std::cout << std::fixed << std::setprecision(7);
        std::cout << "DEBUG: buildIntraday() called." << std::endl;

        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::vector<Entry> constructedBars;
        size_t totalReserve = mBasisDayBars.size();
        for (const auto& v : mDailyNormalizedBars) totalReserve += v.size();
        if (totalReserve == 0 && mBasisDayBars.empty()) { 
             std::cout << "DEBUG: buildIntraday - No basis bars and no normalized bars. Creating empty series." << std::endl;
             mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
                mTimeSeries.getTimeFrame(), mTimeSeries.getVolumeUnits());
            return;
        }
        constructedBars.reserve(totalReserve);

        std::cout << "DEBUG: buildIntraday - Adding " << mBasisDayBars.size() << " basis day bars." << std::endl;
        for (const auto& bar : mBasisDayBars) { 
            constructedBars.push_back(bar);
        }

        if (mDayIndices.empty() || mBasisDayBars.empty()) { 
             std::cout << "DEBUG: buildIntraday - No permutable days or no basis day. Synthetic series will only contain basis day (if any)." << std::endl;
             mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
                mTimeSeries.getTimeFrame(), mTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
            return;
        }
        
        Decimal preciseInterDayChainClose = mBasisDayBars.back().getCloseValue(); 
        std::cout << "DEBUG: buildIntraday - Initial preciseInterDayChainClose (from basis day): " << preciseInterDayChainClose << std::endl;


        for (size_t i = 0; i < mDayIndices.size(); ++i) 
        {
            std::cout << "DEBUG: buildIntraday - Processing permuted day sequence " << i << std::endl;
            if (i >= mOvernightGaps.size()) {
                std::cout << "DEBUG: buildIntraday - Error: 'i' (" << i << ") is out of bounds for mOvernightGaps (size " << mOvernightGaps.size() << ")." << std::endl;
                break; 
            }
            Decimal currentGapFactor = mOvernightGaps[i];
            std::cout << "DEBUG: buildIntraday - Using permutedGap[" << i << "] = " << currentGapFactor << std::endl;

            Decimal preciseDayOpenAnchor = preciseInterDayChainClose * currentGapFactor; 
            std::cout << "DEBUG: buildIntraday - preciseDayOpenAnchor for this day = " << preciseInterDayChainClose << " * " << currentGapFactor << " = " << preciseDayOpenAnchor << std::endl;
            
            size_t currentOriginalDayIndex = mDayIndices[i]; 
            std::cout << "DEBUG: buildIntraday - Original day index from mDayIndices[" << i << "] = " << currentOriginalDayIndex << std::endl;

            if (currentOriginalDayIndex >= mDailyNormalizedBars.size()) {
                 std::cout << "DEBUG: buildIntraday - Error: currentOriginalDayIndex (" << currentOriginalDayIndex 
                           << ") is out of bounds for mDailyNormalizedBars (size " << mDailyNormalizedBars.size() << ")." << std::endl;
                break;
            }
            const auto& selectedNormalizedDayBars = mDailyNormalizedBars[currentOriginalDayIndex];
            std::cout << "DEBUG: buildIntraday - Selected day (original index " << currentOriginalDayIndex << ") has " << selectedNormalizedDayBars.size() << " normalized bars." << std::endl;


            if (selectedNormalizedDayBars.empty())
            {
                std::cout << "DEBUG: buildIntraday - Selected day is empty. preciseInterDayChainClose becomes preciseDayOpenAnchor: " << preciseDayOpenAnchor << std::endl;
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

                Decimal open  = num::Round2Tick(actualOpen,  getTick(), getTickDiv2());
                Decimal high  = num::Round2Tick(actualHigh,  getTick(), getTickDiv2());
                Decimal low   = num::Round2Tick(actualLow,   getTick(), getTickDiv2());
                Decimal close = num::Round2Tick(actualClose, getTick(), getTickDiv2());

                if (barIdx == 0 || barIdx == selectedNormalizedDayBars.size() - 1) {
                     std::cout << "DEBUG: buildIntraday - Day (orig idx " << currentOriginalDayIndex << "), Bar " << barIdx 
                               << ": NormO=" << normalizedBar.getOpenValue() << " NormC=" << normalizedBar.getCloseValue()
                               << " -> ActualO=" << actualOpen << " ActualC=" << actualClose
                               << " -> RoundedO=" << open << " RoundedC=" << close << std::endl;
                }
                
#ifdef SYNTHETIC_VOLUME
                Decimal volume = DecimalConstants<Decimal>::DecimalZero; 
                constructedBars.emplace_back(originalDateTime, open, high, low, close, volume, mTimeSeries.getTimeFrame());
#else
                constructedBars.emplace_back(originalDateTime, open, high, low, close, 
                                             DecimalConstants<Decimal>::DecimalZero, mTimeSeries.getTimeFrame());
#endif
            }
            preciseInterDayChainClose = lastUnroundedCloseForThisDay; 
            std::cout << "DEBUG: buildIntraday - End of day (orig idx " << currentOriginalDayIndex 
                      << "). Updated preciseInterDayChainClose (for next day) to: " << preciseInterDayChainClose << std::endl;
        }

        mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
            mTimeSeries.getTimeFrame(), mTimeSeries.getVolumeUnits(), constructedBars.begin(), constructedBars.end());
        
        // Fix for second compiler error: Use getNumEntries() instead of isEmpty()
        if (mSyntheticTimeSeries->getNumEntries() > 0) {
             std::cout << "DEBUG: buildIntraday - Finished. Synthetic series last close: " << mSyntheticTimeSeries->CloseTimeSeries().getTimeSeriesAsVector().back() << std::endl;
        } else {
            std::cout << "DEBUG: buildIntraday - Finished. Synthetic series is empty." << std::endl;
        }
    }
};

} // namespace mkc_timeseries

#endif // __SYNTHETIC_TIME_SERIES_H
