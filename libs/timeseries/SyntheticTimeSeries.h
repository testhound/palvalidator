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
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeries.h"
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include "DecimalConstants.h"

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

    // Copy constructor (skip mutex)
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
        // each instance has its own mutex
    }

    // Copy‐assignment (skip mutex)
    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs)
    {
        if (this != &rhs) {
            boost::mutex::scoped_lock lk(mMutex);
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
            mSyntheticTimeSeries = rhs.mSyntheticTimeSeries;
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

    // Getters for unit tests
    Decimal getFirstOpen() const { return mFirstOpen; }
    unsigned long getNumElements() const { return mTimeSeries.getNumEntries(); }
    const Decimal& getTick() const
    {
      return mMinimumTick;
    }

    const Decimal& getTickDiv2() const
    {
      return mMinimumTickDiv2;
    }

    std::vector<Decimal> getRelativeOpen()  const { boost::mutex::scoped_lock lk(mMutex); return mRelativeOpen; }
    std::vector<Decimal> getRelativeHigh()  const { boost::mutex::scoped_lock lk(mMutex); return mRelativeHigh; }
    std::vector<Decimal> getRelativeLow()   const { boost::mutex::scoped_lock lk(mMutex); return mRelativeLow; }
    std::vector<Decimal> getRelativeClose() const { boost::mutex::scoped_lock lk(mMutex); return mRelativeClose; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolume()const { boost::mutex::scoped_lock lk(mMutex); return mRelativeVolume; }
#endif

private:
    // ---- Original EOD members ----
    OHLCTimeSeries<Decimal>           mTimeSeries;
    VectorDate                        mDateSeries;
    std::vector<Decimal>              mRelativeOpen,
                                      mRelativeHigh,
                                      mRelativeLow,
                                      mRelativeClose;
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal>              mRelativeVolume;
#endif
    Decimal                           mFirstOpen;
#ifdef SYNTHETIC_VOLUME
    Decimal                           mFirstVolume;
#endif
    RandomMersenne                    mRandGenerator;

    // ---- Intraday extension members ----
    bool                                           mIsIntraday;
    std::vector<std::vector<OHLCTimeSeriesEntry<Decimal>>> mDailyNormalizedBars;
    std::vector<OHLCTimeSeriesEntry<Decimal>>      mBasisDayBars;
    std::vector<Decimal>                           mOvernightGaps;
    std::vector<size_t>                            mDayIndices;

    // ---- Resulting synthetic series ----
    std::shared_ptr<OHLCTimeSeries<Decimal>>      mSyntheticTimeSeries;
    Decimal                                        mMinimumTick, mMinimumTickDiv2;
    mutable boost::mutex                          mMutex;

    // === EOD methods ===

    void initEodData()
    {
        using Iter = typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator;
        Iter it = mTimeSeries.beginRandomAccess();

        Decimal one = DecimalConstants<Decimal>::DecimalOne;
        mRelativeOpen.reserve(mTimeSeries.getNumEntries());
        mRelativeHigh.reserve(mTimeSeries.getNumEntries());
        mRelativeLow .reserve(mTimeSeries.getNumEntries());
        mRelativeClose.reserve(mTimeSeries.getNumEntries());
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.reserve(mTimeSeries.getNumEntries());
#endif

        // first bar
        mRelativeOpen.push_back(one);
#ifdef SYNTHETIC_VOLUME
        mRelativeVolume.push_back(one);
#endif
        mFirstOpen = mTimeSeries.getOpenValue(it, 0);
#ifdef SYNTHETIC_VOLUME
        mFirstVolume = mTimeSeries.getVolumeValue(it, 0);
#endif
        mRelativeHigh.push_back(mTimeSeries.getHighValue(it,0)/mFirstOpen);
        mRelativeLow .push_back(mTimeSeries.getLowValue (it,0)/mFirstOpen);
        mRelativeClose.push_back(mTimeSeries.getCloseValue(it,0)/mFirstOpen);
        mDateSeries.addElement(mTimeSeries.getDateValue(it,0));
        ++it;

        // remaining bars
        for (; it!=mTimeSeries.endRandomAccess(); ++it)
        {
            Decimal currOpen  = mTimeSeries.getOpenValue(it,0);
            Decimal prevClose = mTimeSeries.getCloseValue(it,1);

            mRelativeOpen.push_back(currOpen/prevClose);
            mRelativeHigh.push_back(mTimeSeries.getHighValue(it,0)/currOpen);
            mRelativeLow .push_back(mTimeSeries.getLowValue (it,0)/currOpen);
            mRelativeClose.push_back(mTimeSeries.getCloseValue(it,0)/currOpen);
#ifdef SYNTHETIC_VOLUME
            Decimal v0 = mTimeSeries.getVolumeValue(it,0),
                    v1 = mTimeSeries.getVolumeValue(it,1);
            mRelativeVolume.push_back((v0>DecimalConstants<Decimal>::DecimalZero && v1>DecimalConstants<Decimal>::DecimalZero)
                                           ? (v0/v1)
                                           : one);
#endif
            mDateSeries.addElement(mTimeSeries.getDateValue(it,0));
        }
    }

    void shuffleOverNightChanges()
    {
        size_t i = mRelativeOpen.size();
        while (i>1)
        {
            size_t j = mRandGenerator.DrawNumberExclusive(i--);
            std::swap(mRelativeOpen[i], mRelativeOpen[j]);
        }
    }

    void shuffleTradingDayChanges()
    {
        size_t i = mRelativeHigh.size();
        while (i>1)
        {
            size_t j = mRandGenerator.DrawNumberExclusive(i--);
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
        Decimal xPrice = mFirstOpen;
#ifdef SYNTHETIC_VOLUME
        Decimal xVolume = mFirstVolume;
#endif
        std::vector<OHLCTimeSeriesEntry<Decimal>> bars;
        bars.reserve(mRelativeOpen.size());

        for (size_t i = 0; i < mRelativeOpen.size(); ++i)
        {
            // apply open ratio
            xPrice *= mRelativeOpen[i];
            Decimal rawOpen = xPrice;

            // apply close ratio
            xPrice *= mRelativeClose[i];
            Decimal rawClose = xPrice;

            // compute high/low from rawOpen
            Decimal high =
              num::Round2Tick(rawOpen * mRelativeHigh[i],
                              getTick(), getTickDiv2());
            Decimal low  =
              num::Round2Tick(rawOpen * mRelativeLow [i],
                              getTick(), getTickDiv2());

            // now round the open and close
            Decimal open  = num::Round2Tick(rawOpen,  getTick(), getTickDiv2());
            Decimal close = num::Round2Tick(rawClose, getTick(), getTickDiv2());

#ifdef SYNTHETIC_VOLUME
            xVolume *= mRelativeVolume[i];
            bars.emplace_back(
                mDateSeries.getDate(i),
                open, high, low, close,
                xVolume,
                mTimeSeries.getTimeFrame()
            );
#else
            bars.emplace_back(
                mDateSeries.getDate(i),
                open, high, low, close,
                DecimalConstants<Decimal>::DecimalZero,
                mTimeSeries.getTimeFrame()
            );
#endif
        }

        mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
            mTimeSeries.getTimeFrame(),
            mTimeSeries.getVolumeUnits(),
            bars.begin(), bars.end());
    }

    // === Intraday methods ===

    void initIntradayData()
    {
        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::map<boost::gregorian::date, std::vector<Entry>> dayMap;
        for (auto it=mTimeSeries.beginRandomAccess(); it!=mTimeSeries.endRandomAccess(); ++it)
            dayMap[it->getDateTime().date()].push_back(*it);

        if (dayMap.empty()) return;

        // basis day
        auto bit = dayMap.begin();
        mBasisDayBars = bit->second;
        Decimal prevClose = mBasisDayBars.back().getCloseValue();
        ++bit;

        // subsequent days
        for (; bit!=dayMap.end(); ++bit)
        {
            auto& dayBars = bit->second;
            if (dayBars.empty()) continue;

            Decimal open0 = dayBars.front().getOpenValue();
            mOvernightGaps.push_back(open0 / prevClose);
            prevClose = dayBars.back().getCloseValue();

            std::vector<Entry> norm;
            norm.reserve(dayBars.size());
            for (auto& e : dayBars)
            {
#ifdef SYNTHETIC_VOLUME
                Decimal v0  = dayBars.front().getVolumeValue();
                Decimal vol = (v0>DecimalConstants<Decimal>::DecimalZero)
                              ? (e.getVolumeValue()/v0)
                              : DecimalConstants<Decimal>::DecimalZero;
                norm.emplace_back(
                    e.getDateTime(),
                    e.getOpenValue()  / open0,
                    e.getHighValue()  / open0,
                    e.getLowValue()   / open0,
                    e.getCloseValue() / open0,
                    vol,
                    mTimeSeries.getTimeFrame()
                );
#else
                norm.emplace_back(
                    e.getDateTime(),
                    e.getOpenValue()  / open0,
                    e.getHighValue()  / open0,
                    e.getLowValue()   / open0,
                    e.getCloseValue() / open0,
                    DecimalConstants<Decimal>::DecimalZero,
                    mTimeSeries.getTimeFrame()
                );
#endif
            }
            mDailyNormalizedBars.push_back(std::move(norm));
        }

        mDayIndices.resize(mDailyNormalizedBars.size());
        std::iota(mDayIndices.begin(), mDayIndices.end(), 0u);
    }

    void shuffleIntraday()
    {
        // shuffle intraday bars
        for (auto& day : mDailyNormalizedBars)
        {
            size_t n = day.size();
            while (n>1)
            {
                size_t j = mRandGenerator.DrawNumberExclusive(n--);
                std::swap(day[n], day[j]);
            }
        }
        // shuffle overnight gaps
        size_t g = mOvernightGaps.size();
        while (g>1)
        {
            size_t j = mRandGenerator.DrawNumberExclusive(g--);
            std::swap(mOvernightGaps[g], mOvernightGaps[j]);
        }
        // shuffle day order
        size_t d = mDayIndices.size();
        while (d>1)
        {
            size_t j = mRandGenerator.DrawNumberExclusive(d--);
            std::swap(mDayIndices[d], mDayIndices[j]);
        }
    }

    void buildIntraday()
    {
        using Entry = OHLCTimeSeriesEntry<Decimal>;
        std::vector<Entry> bars;
        size_t total = mBasisDayBars.size();
        for (auto& v : mDailyNormalizedBars) total += v.size();
        bars.reserve(total);

        // basis day unchanged
        for (auto& e : mBasisDayBars) bars.push_back(e);

        Decimal lastClose = mBasisDayBars.back().getCloseValue();
        for (size_t idx=0; idx<mDayIndices.size(); ++idx)
        {
            lastClose = lastClose * mOvernightGaps[idx];
            auto& day = mDailyNormalizedBars[mDayIndices[idx]];
            for (auto& ne : day)
            {
                auto dt = ne.getDateTime();
                // raw open for this bar
                Decimal rawOpen = lastClose * ne.getOpenValue();
                // raw close
                Decimal rawClose = rawOpen * ne.getCloseValue();
                // high/low from rawOpen
                Decimal high = num::Round2Tick(rawOpen * ne.getHighValue(), getTick(), getTickDiv2());
                Decimal low  = num::Round2Tick(rawOpen * ne.getLowValue(),  getTick(), getTickDiv2());
                // now round open/close
                Decimal open  = num::Round2Tick(rawOpen,  getTick(), getTickDiv2());
                Decimal close = num::Round2Tick(rawClose, getTick(), getTickDiv2());
                lastClose = rawClose;  // ensure continuity in ratio space

                bars.emplace_back(
                    dt,
                    open, high, low, close,
                    DecimalConstants<Decimal>::DecimalZero,
                    mTimeSeries.getTimeFrame()
                );
            }
        }

        mSyntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
            mTimeSeries.getTimeFrame(),
            mTimeSeries.getVolumeUnits(),
            bars.begin(), bars.end());
    }
};

} // namespace mkc_timeseries

#endif // __SYNTHETIC_TIME_SERIES_H
