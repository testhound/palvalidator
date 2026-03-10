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
#include "TimeSeries.h"
#include "VectorDecimal.h"
#include "RandomMersenne.h"
#include "ShuffleUtils.h"
#include "DecimalConstants.h"
#include "number.h"
#include "RoundingPolicies.h"

namespace mkc_timeseries
{

  // ============================================================
  // SyntheticNullModel
  // ============================================================

  /**
   * @enum SyntheticNullModel
   * @brief Defines the randomization strategy used to generate the synthetic series.
   *
   * These models determine how much of the original market structure is destroyed.
   */
  enum class SyntheticNullModel
  {
    N1_MaxDestruction = 0,  // independent shuffles of gaps and intraday shapes
    N0_PairedDay      = 1,  // shuffle day-units intact: (gap, H/L/C[, Volume]) together
    N2_BlockDays      = 2   // (reserved) shuffle blocks of day-units; not yet implemented
  };

  // ============================================================
  // EodFactors<Decimal>
  // ============================================================

  /**
   * @class EodFactors
   * @brief Immutable-after-construction container for EOD relative-return factor arrays.
   *
   * Holds the four (optionally five) normalised factor vectors that describe an EOD
   * price series: Open (overnight gap), High, Low, Close (all intraday ratios), and
   * optionally Volume.  No mutation is possible after construction; the class acts as
   * a value type that can be freely copied or moved.
   *
   * @tparam Decimal  Numeric type used throughout the series (e.g. double, dec::Decimal).
   */
  template <class Decimal>
  class EodFactors
  {
  public:
    /// Default-construct an empty factor set (required for member-variable initialisation).
    EodFactors() = default;

    /// Construct from four pre-built factor vectors (no-volume build).
    EodFactors(std::vector<Decimal> open,
               std::vector<Decimal> high,
               std::vector<Decimal> low,
               std::vector<Decimal> close)
      : mOpen (std::move(open)),
        mHigh (std::move(high)),
        mLow  (std::move(low)),
        mClose(std::move(close))
    {}

#ifdef SYNTHETIC_VOLUME
    /// Construct from five pre-built factor vectors (volume-enabled build).
    EodFactors(std::vector<Decimal> open,
               std::vector<Decimal> high,
               std::vector<Decimal> low,
               std::vector<Decimal> close,
               std::vector<Decimal> volume)
      : mOpen  (std::move(open)),
        mHigh  (std::move(high)),
        mLow   (std::move(low)),
        mClose (std::move(close)),
        mVolume(std::move(volume))
    {}
#endif

    // Copy and move are compiler-generated and correct.
    EodFactors(const EodFactors&)                = default;
    EodFactors& operator=(const EodFactors&)     = default;
    EodFactors(EodFactors&&) noexcept            = default;
    EodFactors& operator=(EodFactors&&) noexcept = default;

    // --- Accessors (read-only; no setters by design) ---

    const std::vector<Decimal>& getOpen()  const { return mOpen;  }
    const std::vector<Decimal>& getHigh()  const { return mHigh;  }
    const std::vector<Decimal>& getLow()   const { return mLow;   }
    const std::vector<Decimal>& getClose() const { return mClose; }
#ifdef SYNTHETIC_VOLUME
    const std::vector<Decimal>& getVolume() const { return mVolume; }
#endif

    size_t size()  const { return mOpen.size();  }
    bool   empty() const { return mOpen.empty(); }

  private:
    std::vector<Decimal> mOpen, mHigh, mLow, mClose;
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> mVolume;
#endif
  };

  // ============================================================
  // detail utilities
  // ============================================================

  namespace detail
  {
    /**
     * @brief Fisher-Yates in-place shuffle of v[firstShufflable .. v.size()-1].
     *
     * Elements at indices [0 .. firstShufflable-1] are left untouched (anchor).
     * Passing firstShufflable = 1 is the standard "keep index 0 fixed" case used
     * throughout this file.
     */
    template <class T>
    void fisherYatesSubrange(std::vector<T>& v,
                             size_t          firstShufflable,
                             RandomMersenne& rng)
    {
      const size_t n = v.size();
      if (n < firstShufflable + 2)
      {
        return; // nothing to shuffle
      }
      for (size_t i = n - 1; i > firstShufflable; --i)
      {
        // j uniformly in [firstShufflable .. i]
        const size_t j = rng.DrawNumberExclusive(i - firstShufflable + 1) + firstShufflable;
        std::swap(v[i], v[j]);
      }
    }

    /**
     * @brief Returns a uniformly random permutation of [0..n-1] that fixes
     *        the sub-range [0 .. firstShufflable-1].
     *
     * Useful when the same permutation must be applied to several vectors
     * simultaneously (e.g. keeping H/L/C tuples intact).
     */
    inline std::vector<size_t> generatePermutation(size_t          n,
                                                   size_t          firstShufflable,
                                                   RandomMersenne& rng)
    {
      std::vector<size_t> idx(n);
      std::iota(idx.begin(), idx.end(), size_t{0});
      if (n < firstShufflable + 2)
      {
        return idx;
      }
      for (size_t i = n - 1; i > firstShufflable; --i)
      {
        const size_t j = rng.DrawNumberExclusive(i - firstShufflable + 1) + firstShufflable;
        std::swap(idx[i], idx[j]);
      }
      return idx;
    }

  } // namespace detail

  // ============================================================
  // EOD Shuffle Policies
  // ============================================================

  /**
   * @struct IndependentShufflePolicy
   * @brief N1 "Max Destruction" shuffle: overnight gaps and intraday shapes permuted independently.
   *
   * The RelativeOpen (gap) vector is shuffled with one independent permutation.
   * The (High, Low, Close[, Volume]) tuple is shuffled with a separate, independent
   * permutation, preserving the internal consistency of each day's intraday range
   * while destroying any link between the gap into a day and that day's behaviour.
   *
   * This is the default policy and reproduces the original N1 behaviour exactly.
   */
  struct IndependentShufflePolicy
  {
    template <class Decimal>
    static EodFactors<Decimal> apply(const EodFactors<Decimal>& orig,
                                     RandomMersenne&             rng)
    {
      const size_t n = orig.size();

      // Copy originals into mutable working vectors.
      auto open  = orig.getOpen();
      auto high  = orig.getHigh();
      auto low   = orig.getLow();
      auto close = orig.getClose();
#ifdef SYNTHETIC_VOLUME
      auto volume = orig.getVolume();
#endif

      // --- Pass 1: shuffle overnight gaps independently ---
      detail::fisherYatesSubrange(open, 1, rng);

      // --- Pass 2: shuffle intraday day-shape tuples (H/L/C[/V]) with one shared
      //             permutation so each day's internal OHLC relationship is preserved.
      //             Temporaries are taken once and all writes are fused into a single
      //             loop so perm[k] is dereferenced once per k rather than once per
      //             array -- important at 10,000 permutation-test iterations. ---
      if (n > 2)
      {
        const auto perm = detail::generatePermutation(n, 1, rng);

        const auto tmpHigh  = high;
        const auto tmpLow   = low;
        const auto tmpClose = close;
#ifdef SYNTHETIC_VOLUME
        const auto tmpVolume = volume;
#endif
        for (size_t k = 0; k < n; ++k)
        {
          const size_t src = perm[k];
          high[k]  = tmpHigh[src];
          low[k]   = tmpLow[src];
          close[k] = tmpClose[src];
#ifdef SYNTHETIC_VOLUME
          volume[k] = tmpVolume[src];
#endif
        }
      }

#ifdef SYNTHETIC_VOLUME
      return EodFactors<Decimal>(std::move(open),  std::move(high),
                                 std::move(low),   std::move(close),
                                 std::move(volume));
#else
      return EodFactors<Decimal>(std::move(open), std::move(high),
                                 std::move(low),  std::move(close));
#endif
    }
  };

  /**
   * @struct PairedDayShufflePolicy
   * @brief N0 "Paired-Day" shuffle: each trading day treated as an atomic unit.
   *
   * A single permutation is applied to ALL factor arrays simultaneously.  The
   * overnight gap into day k and that day's intraday (H, L, C[, V]) shape always
   * travel together, preserving the intraday structure of every individual day while
   * randomising the order in which days appear.
   */
  struct PairedDayShufflePolicy
  {
    template <class Decimal>
    static EodFactors<Decimal> apply(const EodFactors<Decimal>& orig,
                                     RandomMersenne&             rng)
    {
      const size_t n = orig.size();
      if (n <= 2)
      {
        return orig; // too few bars to permute; return a copy
      }

      // One permutation applied atomically to every factor array.
      // Output vectors are pre-allocated and all writes are fused into a single
      // loop: perm[k] is loaded once per k and reused for all arrays, avoiding
      // the repeated indirect loads of separate per-array passes.
      const auto perm = detail::generatePermutation(n, 1, rng);

      std::vector<Decimal> newOpen(n), newHigh(n), newLow(n), newClose(n);
#ifdef SYNTHETIC_VOLUME
      std::vector<Decimal> newVolume(n);
#endif
      const auto& srcOpen  = orig.getOpen();
      const auto& srcHigh  = orig.getHigh();
      const auto& srcLow   = orig.getLow();
      const auto& srcClose = orig.getClose();
#ifdef SYNTHETIC_VOLUME
      const auto& srcVolume = orig.getVolume();
#endif
      for (size_t k = 0; k < n; ++k)
      {
        const size_t src = perm[k];
        newOpen[k]  = srcOpen[src];
        newHigh[k]  = srcHigh[src];
        newLow[k]   = srcLow[src];
        newClose[k] = srcClose[src];
#ifdef SYNTHETIC_VOLUME
        newVolume[k] = srcVolume[src];
#endif
      }

      // FIX #3: generatePermutation(n, 1, rng) guarantees perm[0] == 0, so
      // newOpen[0] is always srcOpen[0] == DecimalOne by construction.
      // This explicit reset is kept as a cheap defensive guard but the comment
      // has been corrected: the permutation never places a non-unity value here.
      newOpen[0] = DecimalConstants<Decimal>::DecimalOne;

#ifdef SYNTHETIC_VOLUME
      return EodFactors<Decimal>(std::move(newOpen),  std::move(newHigh),
                                 std::move(newLow),   std::move(newClose),
                                 std::move(newVolume));
#else
      return EodFactors<Decimal>(std::move(newOpen), std::move(newHigh),
                                 std::move(newLow),  std::move(newClose));
#endif
    }
  };

  // ============================================================
  // Forward declarations
  // ============================================================

  template <class Decimal,
            class LookupPolicy                   = mkc_timeseries::LogNLookupPolicy<Decimal>,
            template<class> class RoundingPolicy = NoRounding>
  class ISyntheticTimeSeriesImpl;

  template <class Decimal,
            class LookupPolicy                   = mkc_timeseries::LogNLookupPolicy<Decimal>,
            template<class> class RoundingPolicy = NoRounding>
  class EodSyntheticTimeSeriesImplBase;

  /// Default ShufflePolicy = IndependentShufflePolicy preserves backward compatibility:
  /// existing callers using three explicit template arguments are unaffected.
  template <class Decimal,
            class LookupPolicy                   = mkc_timeseries::LogNLookupPolicy<Decimal>,
            template<class> class RoundingPolicy = NoRounding,
            class ShufflePolicy                  = IndependentShufflePolicy>
  class EodSyntheticTimeSeriesImpl;

  template <class Decimal,
            class LookupPolicy                   = mkc_timeseries::LogNLookupPolicy<Decimal>,
            template<class> class RoundingPolicy = NoRounding>
  class IntradaySyntheticTimeSeriesImpl;

  // ============================================================
  // ISyntheticTimeSeriesImpl — abstract interface
  // ============================================================

  /**
   * @interface ISyntheticTimeSeriesImpl
   * @brief Abstract base class for synthetic time series generator implementations.
   *
   * Abstracts the specific logic used to shuffle and reconstruct EOD vs. Intraday
   * data, enabling the outer SyntheticTimeSeries class to use the Pimpl idiom.
   *
   * @tparam Decimal        Numeric type for price and factor data.
   * @tparam LookupPolicy   Lookup policy for the generated OHLCTimeSeries.
   * @tparam RoundingPolicy Policy to enforce tick-size validity.
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
    virtual std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>
    clone() const = 0;
  };

  // ============================================================
  // EodSyntheticTimeSeriesImplBase — shared EOD state and logic
  // ============================================================

  /**
   * @class EodSyntheticTimeSeriesImplBase
   * @brief Shared base class for all EOD synthetic time series implementations.
   *
   * @details
   * Owns and initialises the EOD factor data (via EodFactors), implements series
   * reconstruction (buildEodInternal), and provides all public accessors.
   *
   * The only things left abstract are shuffleFactors() and clone(), which are
   * provided by the policy-parameterised derived class EodSyntheticTimeSeriesImpl.
   * This eliminates the duplication that previously existed between
   * EodSyntheticTimeSeriesImpl (N1) and EodSyntheticTimeSeriesImpl_N0 (N0), which
   * shared byte-for-byte identical init and build logic with only the shuffle
   * strategy differing.
   *
   * @tparam Decimal        Numeric type for price and factor data.
   * @tparam LookupPolicy   Lookup policy for the generated OHLCTimeSeries.
   * @tparam RoundingPolicy Policy to enforce tick-size validity.
   */
  template <class Decimal, class LookupPolicy, template<class> class RoundingPolicy>
  class EodSyntheticTimeSeriesImplBase
    : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>
  {
  public:
    EodSyntheticTimeSeriesImplBase(const OHLCTimeSeries<Decimal, LookupPolicy>& sourceSeries,
                                   const Decimal& minimumTick,
                                   const Decimal& minimumTickDiv2)
      : mSourceTimeSeries(sourceSeries),
        mMinimumTick     (minimumTick),
        mMinimumTickDiv2 (minimumTickDiv2),
        mDateSeries      (sourceSeries.getNumEntries()),
        mFirstOpen       (DecimalConstants<Decimal>::DecimalZero)
#ifdef SYNTHETIC_VOLUME
      , mFirstVolume     (DecimalConstants<Decimal>::DecimalZero)
#endif
    {
      initEodDataInternal();
    }

    EodSyntheticTimeSeriesImplBase(const EodSyntheticTimeSeriesImplBase&)                = default;
    EodSyntheticTimeSeriesImplBase& operator=(const EodSyntheticTimeSeriesImplBase&)     = default;
    EodSyntheticTimeSeriesImplBase(EodSyntheticTimeSeriesImplBase&&) noexcept            = default;
    EodSyntheticTimeSeriesImplBase& operator=(EodSyntheticTimeSeriesImplBase&&) noexcept = default;

    // shuffleFactors() and clone() remain pure virtual — provided by derived class.

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildSeries() override
    {
      return buildEodInternal();
    }

    Decimal getFirstOpen() const override
    {
      return mFirstOpen;
    }

    unsigned long getNumOriginalElements() const override
    {
      return mSourceTimeSeries.getNumEntries();
    }

    /// Accessors return the current working (permuted) state, not the immutable original.
    std::vector<Decimal> getRelativeOpenFactors()  const override { return mWorking.getOpen();  }
    std::vector<Decimal> getRelativeHighFactors()  const override { return mWorking.getHigh();  }
    std::vector<Decimal> getRelativeLowFactors()   const override { return mWorking.getLow();   }
    std::vector<Decimal> getRelativeCloseFactors() const override { return mWorking.getClose(); }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return mWorking.getVolume(); }
#endif

  protected:
    /// Source data retained for buildEodInternal (time-frame, volume-units, date sequence).
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeries;
    Decimal    mMinimumTick;
    Decimal    mMinimumTickDiv2;
    VectorDate mDateSeries;
    Decimal    mFirstOpen;
#ifdef SYNTHETIC_VOLUME
    Decimal    mFirstVolume;
#endif

    /// Immutable empirical baseline — populated once in initEodDataInternal(), never mutated.
    EodFactors<Decimal> mOriginal;
    /// Current permuted state — replaced on every shuffleFactors() call.
    EodFactors<Decimal> mWorking;

  private:
    void initEodDataInternal()
    {
      using SourceSeriesType = OHLCTimeSeries<Decimal, LookupPolicy>;
      using Iter = typename SourceSeriesType::ConstRandomAccessIterator;

      if (mSourceTimeSeries.getNumEntries() == 0)
      {
        mFirstOpen = DecimalConstants<Decimal>::DecimalZero;
#ifdef SYNTHETIC_VOLUME
        mFirstVolume = DecimalConstants<Decimal>::DecimalZero;
#endif
        return;
      }

      const size_t  n   = mSourceTimeSeries.getNumEntries();
      const Decimal one = DecimalConstants<Decimal>::DecimalOne;

      std::vector<Decimal> relOpen, relHigh, relLow, relClose;
#ifdef SYNTHETIC_VOLUME
      std::vector<Decimal> relVolume;
#endif
      relOpen.reserve(n);  relHigh.reserve(n);
      relLow.reserve(n);   relClose.reserve(n);
#ifdef SYNTHETIC_VOLUME
      relVolume.reserve(n);
#endif

      Iter it = mSourceTimeSeries.beginRandomAccess();

      mFirstOpen = it->getOpenValue();
#ifdef SYNTHETIC_VOLUME
      mFirstVolume = it->getVolumeValue();
#endif

      // --- Anchor bar (index 0) ---
      relOpen.push_back(one);
#ifdef SYNTHETIC_VOLUME
      relVolume.push_back(one);
#endif
      if (mFirstOpen != DecimalConstants<Decimal>::DecimalZero)
      {
        relHigh.push_back(it->getHighValue()   / mFirstOpen);
        relLow.push_back (it->getLowValue()    / mFirstOpen);
        relClose.push_back(it->getCloseValue() / mFirstOpen);
      }
      else
      {
        relHigh.push_back(one);
        relLow.push_back(one);
        relClose.push_back(one);
      }
      mDateSeries.addElement(it->getDateValue());

      // --- Subsequent bars ---
      if (n > 1)
      {
        Iter prev_it = it;
        ++it;
        for (; it != mSourceTimeSeries.endRandomAccess(); ++it, ++prev_it)
        {
          const Decimal currOpen  = it->getOpenValue();
          const Decimal prevClose = prev_it->getCloseValue();

          relOpen.push_back(
            (prevClose != DecimalConstants<Decimal>::DecimalZero)
            ? currOpen / prevClose : one);

          if (currOpen != DecimalConstants<Decimal>::DecimalZero)
          {
            relHigh.push_back(it->getHighValue()   / currOpen);
            relLow.push_back (it->getLowValue()    / currOpen);
            relClose.push_back(it->getCloseValue() / currOpen);
          }
          else
          {
            relHigh.push_back(one);
            relLow.push_back(one);
            relClose.push_back(one);
          }

#ifdef SYNTHETIC_VOLUME
          const Decimal v0 = it->getVolumeValue();
          const Decimal v1 = prev_it->getVolumeValue();
          relVolume.push_back(
            (v1 > DecimalConstants<Decimal>::DecimalZero) ? (v0 / v1) : one);
#endif
          mDateSeries.addElement(it->getDateValue());
        }
      }

      // Construct the immutable baseline; working copy starts as an exact copy.
#ifdef SYNTHETIC_VOLUME
      mOriginal = EodFactors<Decimal>(std::move(relOpen),  std::move(relHigh),
                                      std::move(relLow),   std::move(relClose),
                                      std::move(relVolume));
#else
      mOriginal = EodFactors<Decimal>(std::move(relOpen),  std::move(relHigh),
                                      std::move(relLow),   std::move(relClose));
#endif
      mWorking = mOriginal;
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildEodInternal()
    {
      if (mSourceTimeSeries.getNumEntries() == 0)
      {
        return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
          mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
      }

      const auto& relOpen  = mWorking.getOpen();
      const auto& relHigh  = mWorking.getHigh();
      const auto& relLow   = mWorking.getLow();
      const auto& relClose = mWorking.getClose();
#ifdef SYNTHETIC_VOLUME
      const auto& relVolume = mWorking.getVolume();
#endif

      Decimal preciseChainPrice = mFirstOpen;
#ifdef SYNTHETIC_VOLUME
      Decimal preciseChainVolume = mFirstVolume;
#endif
      std::vector<OHLCTimeSeriesEntry<Decimal>> bars;
      bars.reserve(relOpen.size());

      for (size_t i = 0; i < relOpen.size(); ++i)
      {
        const Decimal preciseOpenOfDay  = (i == 0) ? preciseChainPrice
                                                   : preciseChainPrice * relOpen[i];
        const Decimal preciseCloseOfDay = preciseOpenOfDay * relClose[i];

        Decimal open  = RoundingPolicy<Decimal>::round(preciseOpenOfDay,
                          mMinimumTick, mMinimumTickDiv2);
        Decimal high  = RoundingPolicy<Decimal>::round(preciseOpenOfDay * relHigh[i],
                          mMinimumTick, mMinimumTickDiv2);
        Decimal low   = RoundingPolicy<Decimal>::round(preciseOpenOfDay * relLow[i],
                          mMinimumTick, mMinimumTickDiv2);
        Decimal close = RoundingPolicy<Decimal>::round(preciseCloseOfDay,
                          mMinimumTick, mMinimumTickDiv2);

        high = std::max({high, open, close});
        low  = std::min({low,  open, close});

        preciseChainPrice = preciseCloseOfDay;

#ifdef SYNTHETIC_VOLUME
        Decimal currentDayVolume;
        if (i == 0)
        {
          currentDayVolume = preciseChainVolume;
        }
        else
        {
          currentDayVolume = (relVolume.size() > i)
            ? preciseChainVolume * relVolume[i]
            : preciseChainVolume;
        }
        const Decimal volume = num::Round2Tick(currentDayVolume,
                                               DecimalConstants<Decimal>::DecimalOne,
                                               DecimalConstants<Decimal>::DecimalZero);
        preciseChainVolume = currentDayVolume;
        bars.emplace_back(mDateSeries.getDate(i), open, high, low, close,
                          volume, mSourceTimeSeries.getTimeFrame());
#else
        bars.emplace_back(mDateSeries.getDate(i), open, high, low, close,
                          DecimalConstants<Decimal>::DecimalZero,
                          mSourceTimeSeries.getTimeFrame());
#endif
      }

      return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
        mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(),
        bars.begin(), bars.end());
    }
  };

  // ============================================================
  // EodSyntheticTimeSeriesImpl — policy-parameterised EOD implementation
  // ============================================================

  /**
   * @class EodSyntheticTimeSeriesImpl
   * @brief Concrete EOD synthetic time series implementation, parameterised on ShufflePolicy.
   *
   * @details
   * Inherits all state and logic from EodSyntheticTimeSeriesImplBase and provides
   * the single missing piece: the shuffle strategy, supplied as a compile-time policy.
   *
   * The default policy (IndependentShufflePolicy) reproduces the previous N1
   * "Max Destruction" behaviour exactly, so all existing callers that pass three
   * explicit template arguments are completely unaffected.
   *
   * @tparam Decimal        Numeric type for price and factor data.
   * @tparam LookupPolicy   Lookup policy for the generated OHLCTimeSeries.
   * @tparam RoundingPolicy Policy to enforce tick-size validity.
   * @tparam ShufflePolicy  Stateless struct exposing
   *                        `static EodFactors<Decimal> apply(const EodFactors<Decimal>&,
   *                                                          RandomMersenne&)`.
   *                        Defaults to IndependentShufflePolicy (N1).
   */
  template <class Decimal,
            class LookupPolicy,
            template<class> class RoundingPolicy,
            class ShufflePolicy>   // default established in the forward declaration above
  class EodSyntheticTimeSeriesImpl
    : public EodSyntheticTimeSeriesImplBase<Decimal, LookupPolicy, RoundingPolicy>
  {
    using Base = EodSyntheticTimeSeriesImplBase<Decimal, LookupPolicy, RoundingPolicy>;

  public:
    using Base::Base; // inherit the three-argument constructor

    EodSyntheticTimeSeriesImpl(const EodSyntheticTimeSeriesImpl&)                = default;
    EodSyntheticTimeSeriesImpl& operator=(const EodSyntheticTimeSeriesImpl&)     = default;
    EodSyntheticTimeSeriesImpl(EodSyntheticTimeSeriesImpl&&) noexcept            = default;
    EodSyntheticTimeSeriesImpl& operator=(EodSyntheticTimeSeriesImpl&&) noexcept = default;

    /**
     * @brief Replace the working factor set with a fresh permutation drawn from
     *        the immutable empirical baseline via ShufflePolicy.
     *
     * Each call is an independent draw: the baseline is never modified, so
     * consecutive calls produce independent permutations regardless of RNG history.
     */
    void shuffleFactors(RandomMersenne& rng) override
    {
      this->mWorking = ShufflePolicy::template apply<Decimal>(this->mOriginal, rng);
    }

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>
    clone() const override
    {
      return std::make_unique<EodSyntheticTimeSeriesImpl>(*this);
    }
  };

  /**
   * @brief Backward-compatible alias for the N0 "Paired-Day" EOD implementation.
   *
   * Existing code using `EodSyntheticTimeSeriesImpl_N0<D, L, R>` continues to
   * compile and behave identically without modification.
   */
  template <class Decimal,
            class LookupPolicy                   = mkc_timeseries::LogNLookupPolicy<Decimal>,
            template<class> class RoundingPolicy = NoRounding>
  using EodSyntheticTimeSeriesImpl_N0 =
    EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy, PairedDayShufflePolicy>;

  // ============================================================
  // IntradaySyntheticTimeSeriesImpl
  // ============================================================

  /**
   * @class IntradaySyntheticTimeSeriesImpl
   * @brief Implements Intraday synthetic time series generation.
   *
   * @details
   * This implementation performs a hierarchical "Deep Shuffle" suitable for intraday data:
   * 1. Shuffles the order of intraday bars WITHIN each day.
   * 2. Shuffles the overnight gaps between days.
   * 3. Shuffles the order of trading days.
   *
   * This effectively destroys both intraday serial correlation (trends within the day)
   * and inter-day correlation (trends across days).
   */
  template <class Decimal, class LookupPolicy, template<class> class RoundingPolicy>
  class IntradaySyntheticTimeSeriesImpl
    : public ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>
  {
  public:
    IntradaySyntheticTimeSeriesImpl(const OHLCTimeSeries<Decimal, LookupPolicy>& sourceSeries,
                                    const Decimal& minimumTick,
                                    const Decimal& minimumTickDiv2)
      : mSourceTimeSeries(sourceSeries),
        mMinimumTick     (minimumTick),
        mMinimumTickDiv2 (minimumTickDiv2),
        mFirstOpen       (DecimalConstants<Decimal>::DecimalZero)
    {
      initIntradayDataInternal();
    }

    IntradaySyntheticTimeSeriesImpl(const IntradaySyntheticTimeSeriesImpl&)                = default;
    IntradaySyntheticTimeSeriesImpl& operator=(const IntradaySyntheticTimeSeriesImpl&)     = default;
    IntradaySyntheticTimeSeriesImpl(IntradaySyntheticTimeSeriesImpl&&) noexcept            = default;
    IntradaySyntheticTimeSeriesImpl& operator=(IntradaySyntheticTimeSeriesImpl&&) noexcept = default;

    void shuffleFactors(RandomMersenne& randGenerator) override
    {
      // Reset all working state to the empirical baseline so each call is
      // an independent draw rather than a permutation of the previous one.
      mDailyNormalizedBars = mOriginalDailyNormalizedBars;
      mOvernightGaps       = mOriginalOvernightGaps;
      std::iota(mDayIndices.begin(), mDayIndices.end(), 0u);

      // Shuffle from the clean baseline.
      for (auto& dayBars : mDailyNormalizedBars)
      {
        inplaceShuffle(dayBars, randGenerator);
      }
      inplaceShuffle(mOvernightGaps, randGenerator);
      inplaceShuffle(mDayIndices,    randGenerator);
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildSeries() override
    {
      return buildIntradayInternal();
    }

    Decimal getFirstOpen() const override
    {
      return mFirstOpen;
    }

    unsigned long getNumOriginalElements() const override
    {
      return mSourceTimeSeries.getNumEntries();
    }

    // Intraday does not expose flat factor vectors via the EOD accessor interface.
    std::vector<Decimal> getRelativeOpenFactors()  const override { return {}; }
    std::vector<Decimal> getRelativeHighFactors()  const override { return {}; }
    std::vector<Decimal> getRelativeLowFactors()   const override { return {}; }
    std::vector<Decimal> getRelativeCloseFactors() const override { return {}; }
#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolumeFactors() const override { return {}; }
#endif

    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>
    clone() const override
    {
      return std::make_unique<IntradaySyntheticTimeSeriesImpl>(*this);
    }

  private:
    void initIntradayDataInternal()
    {
      using Entry = OHLCTimeSeriesEntry<Decimal>;

      std::map<boost::gregorian::date, std::vector<Entry>> dayMap;

      if (mSourceTimeSeries.getNumEntries() == 0)
      {
        mFirstOpen = DecimalConstants<Decimal>::DecimalZero;
        return;
      }

      mFirstOpen = mSourceTimeSeries.beginRandomAccess()->getOpenValue();

      for (auto it = mSourceTimeSeries.beginRandomAccess();
           it != mSourceTimeSeries.endRandomAccess(); ++it)
      {
        dayMap[it->getDateTime().date()].push_back(*it);
      }

      if (dayMap.empty())
      {
        return;
      }

      auto dayMapIt = dayMap.begin();
      mBasisDayBars = dayMapIt->second;
      if (mBasisDayBars.empty())
      {
        return;
      }

      Decimal prevDayActualClose = mBasisDayBars.back().getCloseValue();
      ++dayMapIt;

      const Decimal one = DecimalConstants<Decimal>::DecimalOne;
      for (; dayMapIt != dayMap.end(); ++dayMapIt)
      {
        const auto& currentDayBars = dayMapIt->second;
        if (currentDayBars.empty())
        {
          mOvernightGaps.push_back(one);
          mDailyNormalizedBars.emplace_back();
          continue;
        }

        const Decimal currentDayOriginalOpen = currentDayBars.front().getOpenValue();

        if (prevDayActualClose != DecimalConstants<Decimal>::DecimalZero)
        {
          mOvernightGaps.push_back(currentDayOriginalOpen / prevDayActualClose);
        }
        else
        {
          mOvernightGaps.push_back(one);
        }

        prevDayActualClose = currentDayBars.back().getCloseValue();

        std::vector<Entry> normalizedBarsForThisDay;
        normalizedBarsForThisDay.reserve(currentDayBars.size());

        if (currentDayOriginalOpen != DecimalConstants<Decimal>::DecimalZero)
        {
          for (const auto& bar : currentDayBars)
          {
            const Decimal normO = bar.getOpenValue()  / currentDayOriginalOpen;
            const Decimal normH = bar.getHighValue()  / currentDayOriginalOpen;
            const Decimal normL = bar.getLowValue()   / currentDayOriginalOpen;
            const Decimal normC = bar.getCloseValue() / currentDayOriginalOpen;
#ifdef SYNTHETIC_VOLUME
            const Decimal dayFirstVolume = currentDayBars.front().getVolumeValue();
            Decimal volumeFactor = one;
            if (dayFirstVolume > DecimalConstants<Decimal>::DecimalZero)
            {
              volumeFactor = bar.getVolumeValue() / dayFirstVolume;
            }
            else if (bar.getVolumeValue() > DecimalConstants<Decimal>::DecimalZero)
            {
              volumeFactor = one;
            }
            normalizedBarsForThisDay.emplace_back(
              bar.getDateTime(), normO, normH, normL, normC,
              volumeFactor, mSourceTimeSeries.getTimeFrame());
#else
            normalizedBarsForThisDay.emplace_back(
              bar.getDateTime(), normO, normH, normL, normC,
              DecimalConstants<Decimal>::DecimalZero,
              mSourceTimeSeries.getTimeFrame());
#endif
          }
        }
        else
        {
          for (const auto& bar : currentDayBars)
          {
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

      // Snapshot the empirical baseline so every shuffle draws from the
      // same original state rather than from the previous permutation.
      mOriginalDailyNormalizedBars = mDailyNormalizedBars;
      mOriginalOvernightGaps       = mOvernightGaps;
    }

    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> buildIntradayInternal()
    {
      using Entry = OHLCTimeSeriesEntry<Decimal>;
      std::vector<Entry> constructedBars;

      size_t totalReserve = mBasisDayBars.size();
      for (const auto& v : mDailyNormalizedBars)
      {
        totalReserve += v.size();
      }

      if (totalReserve == 0 && mBasisDayBars.empty())
      {
        return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
          mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits());
      }

      constructedBars.reserve(totalReserve);

      // Apply rounding policy to basis-day bars and enforce OHLC invariants.
      for (const auto& bar : mBasisDayBars)
      {
        Decimal o = RoundingPolicy<Decimal>::round(bar.getOpenValue(),  mMinimumTick, mMinimumTickDiv2);
        Decimal h = RoundingPolicy<Decimal>::round(bar.getHighValue(),  mMinimumTick, mMinimumTickDiv2);
        Decimal l = RoundingPolicy<Decimal>::round(bar.getLowValue(),   mMinimumTick, mMinimumTickDiv2);
        Decimal c = RoundingPolicy<Decimal>::round(bar.getCloseValue(), mMinimumTick, mMinimumTickDiv2);
        h = std::max({h, o, c});
        l = std::min({l, o, c});
        constructedBars.emplace_back(bar.getDateTime(), o, h, l, c,
                                     bar.getVolumeValue(),
                                     mSourceTimeSeries.getTimeFrame());
      }

      if (mDayIndices.empty() || mBasisDayBars.empty())
      {
        return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
          mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(),
          constructedBars.begin(), constructedBars.end());
      }

      Decimal preciseInterDayChainClose = mBasisDayBars.back().getCloseValue();

      for (size_t i = 0; i < mDayIndices.size(); ++i)
      {
        if (i >= mOvernightGaps.size())
        {
          break;
        }

        const Decimal currentGapFactor     = mOvernightGaps[i];
        const Decimal preciseDayOpenAnchor = preciseInterDayChainClose * currentGapFactor;

        const size_t currentOriginalDayIndex = mDayIndices[i];
        if (currentOriginalDayIndex >= mDailyNormalizedBars.size())
        {
          break;
        }

        const auto& selectedNormalizedDayBars =
          mDailyNormalizedBars[currentOriginalDayIndex];

        if (selectedNormalizedDayBars.empty())
        {
          preciseInterDayChainClose = preciseDayOpenAnchor;
          continue;
        }

        Decimal lastUnroundedCloseForThisDay = preciseDayOpenAnchor;

        for (const auto& normalizedBar : selectedNormalizedDayBars)
        {
          const Decimal actualOpen  = preciseDayOpenAnchor * normalizedBar.getOpenValue();
          const Decimal actualHigh  = preciseDayOpenAnchor * normalizedBar.getHighValue();
          const Decimal actualLow   = preciseDayOpenAnchor * normalizedBar.getLowValue();
          const Decimal actualClose = preciseDayOpenAnchor * normalizedBar.getCloseValue();

          Decimal open  = RoundingPolicy<Decimal>::round(actualOpen,  mMinimumTick, mMinimumTickDiv2);
          Decimal high  = RoundingPolicy<Decimal>::round(actualHigh,  mMinimumTick, mMinimumTickDiv2);
          Decimal low   = RoundingPolicy<Decimal>::round(actualLow,   mMinimumTick, mMinimumTickDiv2);
          Decimal close = RoundingPolicy<Decimal>::round(actualClose, mMinimumTick, mMinimumTickDiv2);

          high = std::max({high, open, close});
          low  = std::min({low,  open, close});

          lastUnroundedCloseForThisDay = actualClose;

          // FIX #2: Reconstruct volume from the normalised factor stored in the bar.
          // Previously DecimalZero was unconditionally emitted here even when
          // SYNTHETIC_VOLUME was defined, silently discarding all intraday volume.
          // The normalised bar carries volumeFactor = bar.volume / dayFirstVolume,
          // so actual volume = preciseDayOpenAnchor-scaled anchor * volumeFactor.
          // For intraday we use the first bar of the selected day as the volume
          // anchor, consistent with how initIntradayDataInternal() normalises it.
#ifdef SYNTHETIC_VOLUME
          const Decimal firstBarVolume =
            selectedNormalizedDayBars.front().getVolumeValue();
          const Decimal actualVolume =
            (firstBarVolume > DecimalConstants<Decimal>::DecimalZero)
            ? normalizedBar.getVolumeValue() * firstBarVolume
            : DecimalConstants<Decimal>::DecimalZero;
          const Decimal roundedVolume =
            num::Round2Tick(actualVolume,
                            DecimalConstants<Decimal>::DecimalOne,
                            DecimalConstants<Decimal>::DecimalZero);
          constructedBars.emplace_back(normalizedBar.getDateTime(), open, high, low, close,
                                       roundedVolume,
                                       mSourceTimeSeries.getTimeFrame());
#else
          constructedBars.emplace_back(normalizedBar.getDateTime(), open, high, low, close,
                                       DecimalConstants<Decimal>::DecimalZero,
                                       mSourceTimeSeries.getTimeFrame());
#endif
        }
        preciseInterDayChainClose = lastUnroundedCloseForThisDay;
      }

      return std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(
        mSourceTimeSeries.getTimeFrame(), mSourceTimeSeries.getVolumeUnits(),
        constructedBars.begin(), constructedBars.end());
    }

  private:
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
    Decimal mFirstOpen;

    // Working (permuted) state — overwritten on each shuffleFactors() call.
    std::vector<std::vector<OHLCTimeSeriesEntry<Decimal>>> mDailyNormalizedBars;
    std::vector<OHLCTimeSeriesEntry<Decimal>>              mBasisDayBars;
    std::vector<Decimal>                                   mOvernightGaps;
    std::vector<size_t>                                    mDayIndices;

    // Immutable empirical baseline — set once in initIntradayDataInternal(), never mutated.
    std::vector<std::vector<OHLCTimeSeriesEntry<Decimal>>> mOriginalDailyNormalizedBars;
    std::vector<Decimal>                                   mOriginalOvernightGaps;
  };

  // ============================================================
  // SyntheticTimeSeries — public wrapper
  // ============================================================

  /**
   * @class SyntheticTimeSeries
   * @brief Main public wrapper for generating synthetic OHLC time series.
   *
   * @details
   * This class uses the Pimpl (Pointer to Implementation) idiom to select the correct
   * shuffling algorithm (EOD vs. Intraday) based on the source data time frame and the
   * selected Null Model.
   *
   * @note Implements algorithms described by Timothy Masters for Monte Carlo
   * Permutation Testing of trading strategies.
   *
   * @tparam Decimal        The numeric type for price and factor data.
   * @tparam LookupPolicy   The lookup policy for the OHLCTimeSeries to be generated.
   * @tparam RoundingPolicy The policy to enforce tick-size validity.
   * @tparam NullModel      The destruction strategy (default: N1_MaxDestruction).
   */
  template <class Decimal,
            class LookupPolicy                   = mkc_timeseries::LogNLookupPolicy<Decimal>,
            template<class> class RoundingPolicy = NoRounding,
            SyntheticNullModel NullModel         = SyntheticNullModel::N1_MaxDestruction>
  class SyntheticTimeSeries
  {
  public:
    explicit SyntheticTimeSeries(const OHLCTimeSeries<Decimal, LookupPolicy>& aTimeSeries,
                                 const Decimal& minimumTick,
                                 const Decimal& minimumTickDiv2)
      : mSourceTimeSeriesCopy(aTimeSeries),
        mMinimumTick         (minimumTick),
        mMinimumTickDiv2     (minimumTickDiv2),
        mRandGenerator       ()
    {
      const bool isIntraday =
        (aTimeSeries.getTimeFrame() == TimeFrame::Duration::INTRADAY);

      if (!isIntraday)
      {
        // FIX #1: N2_BlockDays previously fell through silently to N1 behaviour.
        // Now it triggers a hard compile-time error so callers know the model they
        // requested is not yet implemented rather than getting N1 results silently.
        static_assert(NullModel != SyntheticNullModel::N2_BlockDays,
                      "SyntheticNullModel::N2_BlockDays is not yet implemented. "
                      "Use N1_MaxDestruction or N0_PairedDay.");

        if constexpr (NullModel == SyntheticNullModel::N0_PairedDay)
        {
          mPimpl = std::make_unique<
            EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy,
                                       PairedDayShufflePolicy>>(
              mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
        }
        else
        {
          // N1_MaxDestruction (default).
          mPimpl = std::make_unique<
            EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>(
              mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
        }
      }
      else
      {
        mPimpl = std::make_unique<
          IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>>(
            mSourceTimeSeriesCopy, mMinimumTick, mMinimumTickDiv2);
      }
    }

    ~SyntheticTimeSeries() = default;

    // FIX #5: Copy constructor default-constructs the RNG rather than copying it.
    //
    // Copying the RNG state would cause all copies to produce identical permutation
    // sequences — silently collapsing a parallel Monte Carlo run of N workers into
    // N repetitions of the same single permutation. Default-constructing seeds each
    // copy independently from the OS entropy pool via randutils::auto_seed_256.
    //
    // If exact RNG-state reproduction is needed for debugging a specific permutation,
    // use seed_u64() / seed_seq() on the target instance explicitly.
    SyntheticTimeSeries(const SyntheticTimeSeries& rhs)
      : mSourceTimeSeriesCopy(rhs.mSourceTimeSeriesCopy),
        mMinimumTick         (rhs.mMinimumTick),
        mMinimumTickDiv2     (rhs.mMinimumTickDiv2),
        mRandGenerator       (),                          // independent entropy seed
        mPimpl               (rhs.mPimpl ? rhs.mPimpl->clone() : nullptr),
        mSyntheticTimeSeries (rhs.mSyntheticTimeSeries
          ? std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(*rhs.mSyntheticTimeSeries)
          : nullptr)
    {}

    // FIX #4: Copy and move assignment now use the same locking strategy.
    // Both use boost::unique_lock + boost::lock() (deadlock-safe simultaneous
    // acquisition) for consistency. The original copy assignment used std::scoped_lock
    // on boost::mutex objects, which happened to work but mixed std/boost lock types.
    SyntheticTimeSeries& operator=(const SyntheticTimeSeries& rhs)
    {
      if (this == &rhs)
      {
        return *this;
      }
      boost::unique_lock<boost::mutex> lock1(mMutex,     boost::defer_lock);
      boost::unique_lock<boost::mutex> lock2(rhs.mMutex, boost::defer_lock);
      std::lock(lock1, lock2);
      mSourceTimeSeriesCopy = rhs.mSourceTimeSeriesCopy;
      mMinimumTick          = rhs.mMinimumTick;
      mMinimumTickDiv2      = rhs.mMinimumTickDiv2;
      // FIX #5 (copy-assignment): reseed rather than copy RNG state for the same
      // reason as the copy constructor — each assigned-to instance should be
      // statistically independent.
      mRandGenerator.reseed();
      mPimpl                = rhs.mPimpl ? rhs.mPimpl->clone() : nullptr;
      mSyntheticTimeSeries  = rhs.mSyntheticTimeSeries
        ? std::make_shared<OHLCTimeSeries<Decimal, LookupPolicy>>(*rhs.mSyntheticTimeSeries)
        : nullptr;
      return *this;
    }

    SyntheticTimeSeries(SyntheticTimeSeries&& rhs) noexcept
      : mSourceTimeSeriesCopy(std::move(rhs.mSourceTimeSeriesCopy)),
        mMinimumTick         (std::move(rhs.mMinimumTick)),
        mMinimumTickDiv2     (std::move(rhs.mMinimumTickDiv2)),
        mRandGenerator       (std::move(rhs.mRandGenerator)),
        mPimpl               (std::move(rhs.mPimpl)),
        mSyntheticTimeSeries (std::move(rhs.mSyntheticTimeSeries))
    {
      // mMutex is default-constructed (cannot be moved)
    }

    SyntheticTimeSeries& operator=(SyntheticTimeSeries&& rhs) noexcept
    {
      if (this == &rhs)
      {
        return *this;
      }
      boost::unique_lock<boost::mutex> lock1(mMutex,     boost::defer_lock);
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
     *
     * Triggers the shuffling process via the implementation pointer and
     * stores the result in mSyntheticTimeSeries. Thread-safe.
     */
    void createSyntheticSeries()
    {
      boost::mutex::scoped_lock lock(mMutex);
      if (!mPimpl)
      {
        return;
      }
      mPimpl->shuffleFactors(mRandGenerator);
      mSyntheticTimeSeries = mPimpl->buildSeries();
    }

    void reseedRNG()
    {
      boost::mutex::scoped_lock lock(mMutex);
      mRandGenerator.seed();
    }

    std::shared_ptr<const OHLCTimeSeries<Decimal, LookupPolicy>>
    getSyntheticTimeSeries() const
    {
      std::shared_ptr<const OHLCTimeSeries<Decimal, LookupPolicy>> result;
      {
        boost::mutex::scoped_lock lock(mMutex);
        result = mSyntheticTimeSeries;
      }
      return result;
    }

    Decimal getFirstOpen() const
    {
      return mPimpl ? mPimpl->getFirstOpen()
                    : DecimalConstants<Decimal>::DecimalZero;
    }

    unsigned long getNumElements() const
    {
      return mPimpl ? mPimpl->getNumOriginalElements() : 0;
    }

    const Decimal& getTick()     const { return mMinimumTick;     }
    const Decimal& getTickDiv2() const { return mMinimumTickDiv2; }

    std::vector<Decimal> getRelativeOpen() const
    {
      boost::mutex::scoped_lock lk(mMutex);
      return mPimpl ? mPimpl->getRelativeOpenFactors() : std::vector<Decimal>();
    }

    std::vector<Decimal> getRelativeHigh() const
    {
      boost::mutex::scoped_lock lk(mMutex);
      return mPimpl ? mPimpl->getRelativeHighFactors() : std::vector<Decimal>();
    }

    std::vector<Decimal> getRelativeLow() const
    {
      boost::mutex::scoped_lock lk(mMutex);
      return mPimpl ? mPimpl->getRelativeLowFactors() : std::vector<Decimal>();
    }

    std::vector<Decimal> getRelativeClose() const
    {
      boost::mutex::scoped_lock lk(mMutex);
      return mPimpl ? mPimpl->getRelativeCloseFactors() : std::vector<Decimal>();
    }

#ifdef SYNTHETIC_VOLUME
    std::vector<Decimal> getRelativeVolume() const
    {
      boost::mutex::scoped_lock lk(mMutex);
      return mPimpl ? mPimpl->getRelativeVolumeFactors() : std::vector<Decimal>();
    }
#endif

  private:
    OHLCTimeSeries<Decimal, LookupPolicy> mSourceTimeSeriesCopy;
    Decimal                               mMinimumTick;
    Decimal                               mMinimumTickDiv2;
    RandomMersenne                        mRandGenerator;
    std::unique_ptr<ISyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy>> mPimpl;
    std::shared_ptr<OHLCTimeSeries<Decimal, LookupPolicy>> mSyntheticTimeSeries;
    mutable boost::mutex                  mMutex;
  };

} // namespace mkc_timeseries

#endif // __SYNTHETIC_TIME_SERIES_H
