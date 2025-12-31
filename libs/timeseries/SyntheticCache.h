#pragma once
#include <memory>
#include <utility>
#include "Security.h"
#include "SyntheticTimeSeries.h"  // SyntheticNullModel, EOD/Intraday impls & interface

namespace mkc_timeseries
{

  /**
   * @brief Per-thread cache for efficient synthetic time series generation.
   *
   * This cache maintains a single Security object and swaps its time series
   * pointer on each shuffle, avoiding repeated allocations. Automatically
   * selects the appropriate implementation (EOD vs Intraday) based on the
   * base time series TimeFrame.
   *
   * @tparam Decimal Numeric type (e.g., num::DefaultNumber)
   * @tparam LookupPolicy Time series lookup policy
   * @tparam RoundingPolicy Tick rounding policy (default: NoRounding)
   * @tparam NullModel Synthetic null model strategy (default: N1_MaxDestruction)
   *
   * @note This class is NOT thread-safe. Use one instance per worker thread.
   * @note The returned Security reference remains valid until the next
   *       shuffleAndRebuild() call or cache destruction.
   *
   * @par Usage Example:
   * @code
   * auto baseSec = createSecurityFromData();
   * SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding> cache(baseSec);
   *
   * RandomMersenne rng;
   * for (int i = 0; i < numPermutations; ++i) {
   *   auto& syntheticSec = cache.shuffleAndRebuild(rng);
   *   // Use syntheticSec for testing...
   * }
   * @endcode
   */
  template<
    class Decimal,
    class LookupPolicy,
    template<class> class RoundingPolicy,
    SyntheticNullModel NullModel = SyntheticNullModel::N1_MaxDestruction
    >
  class SyntheticCache
  {
  public:
    using SeriesT = OHLCTimeSeries<Decimal, LookupPolicy>;
    using SecPtr  = std::shared_ptr<Security<Decimal>>;

    explicit SyntheticCache(const SecPtr& baseSec)
      : m_sec(baseSec->clone(baseSec->getTimeSeries()))
    {
      if (!m_sec) {
        throw SecurityException("SyntheticCache: failed to clone base security");
      }
      initImplFrom(*baseSec->getTimeSeries(), baseSec->getTick(), baseSec->getTickDiv2());
    }

    /**
     * @brief Shuffle factors (this permutation), rebuild series, and swap into reusable Security.
     * @return Reference to the reusable Security configured with the new synthetic series.
     */
    SecPtr& shuffleAndRebuild(RandomMersenne& rng)
    {
      m_impl->shuffleFactors(rng);
      auto synSeries = m_impl->buildSeries();
      m_sec->resetTimeSeries(std::move(synSeries));
      return m_sec;
    }
    
    /**
     * @brief Re-initialize from a different base security.
     *
     * Replaces the internal implementation and resets the cached Security's
     * time series to match the new base. The new base security should have
     * the same symbol and tick parameters for consistent behavior.
     *
     * @param baseSec New base security to use for future shuffles
     *
     * @note This resets internal state; previous permutations are discarded.
     * @note The Security object is reused but its time series is replaced.
     */
    void resetFromBase(const SecPtr& baseSec)
    {
      initImplFrom(*baseSec->getTimeSeries(), baseSec->getTick(), baseSec->getTickDiv2());
      m_sec->resetTimeSeries(baseSec->getTimeSeries());
    }

    const SecPtr& security() const noexcept
    {
      return m_sec;
    }

  private:
    // Polymorphic impl type-erasure — matches the interface used by SyntheticTimeSeries
    struct ImplIface {
      virtual ~ImplIface() = default;
      virtual void shuffleFactors(RandomMersenne& rng) = 0;
      virtual std::shared_ptr<const SeriesT> buildSeries() = 0;
    };

    // EOD adapter (N1: legacy independent shuffles)
    class EodImpl final : public ImplIface
    {
    public:
      EodImpl(const SeriesT& base, const Decimal& tick, const Decimal& tickDiv2)
        : m_impl(base, tick, tickDiv2)
      {}

      void shuffleFactors(RandomMersenne& rng) override
      {
        m_impl.shuffleFactors(rng);
      }

      std::shared_ptr<const SeriesT> buildSeries() override
      {
        return m_impl.buildSeries();
      }

    private:
      EodSyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy> m_impl;
    };

    // EOD adapter (N0: paired-day shuffle — gap + H/L/C permuted together)
    class EodImplN0 final : public ImplIface
    {
    public:
      EodImplN0(const SeriesT& base, const Decimal& tick, const Decimal& tickDiv2)
        : m_impl(base, tick, tickDiv2)
      {}

      void shuffleFactors(RandomMersenne& rng) override
      {
        m_impl.shuffleFactors(rng);
      }

      std::shared_ptr<const SeriesT> buildSeries() override
      {
        return m_impl.buildSeries();
      }

    private:
      EodSyntheticTimeSeriesImpl_N0<Decimal, LookupPolicy, RoundingPolicy> m_impl;
    };

    // Intraday adapter (unchanged)
    class IntradayImpl final : public ImplIface {
    public:
      IntradayImpl(const SeriesT& base, const Decimal& tick, const Decimal& tickDiv2)
        : m_impl(base, tick, tickDiv2)
      {}

      void shuffleFactors(RandomMersenne& rng) override
      {
        m_impl.shuffleFactors(rng);
      }

      std::shared_ptr<const SeriesT> buildSeries() override
      {
        return m_impl.buildSeries();
      }

    private:
      IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy> m_impl;
    };

    void initImplFrom(const SeriesT& base, const Decimal& tick, const Decimal& tickDiv2)
    {
      const auto tf = base.getTimeFrame();
      if (tf != TimeFrame::INTRADAY)
	{
	  if constexpr (NullModel == SyntheticNullModel::N0_PairedDay)
	    m_impl = std::make_unique<EodImplN0>(base, tick, tickDiv2);
	  else if constexpr (NullModel == SyntheticNullModel::N2_BlockDays)
	    throw std::logic_error("SyntheticCache: N2_BlockDays not yet implemented");
	  else
	    m_impl = std::make_unique<EodImpl>(base, tick, tickDiv2);
	}
      else
	{
	  m_impl = std::make_unique<IntradayImpl>(base, tick, tickDiv2);
	}
    }
  private:
    std::unique_ptr<ImplIface> m_impl;  // chosen at runtime (EOD or Intraday)
    SecPtr                      m_sec;  // one reusable Security; series swapped per permutation
  };

} // namespace mkc_timeseries
