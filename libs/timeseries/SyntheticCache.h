#pragma once
#include <memory>
#include <utility>
#include "Security.h"
#include "SyntheticTimeSeries.h"  // for Eod/Intraday impls & interface

namespace mkc_timeseries
{

  /**
   * @brief Per-thread cache to build synthetic series cheaply across permutations.
   *
   * Chooses the correct synthetic implementation (EOD vs Intraday) automatically
   * from the base time series, mirroring SyntheticTimeSeries' behavior.
   *
   * Keep one instance per worker thread. Not thread-safe.
   *
   * Template params:
   *  Decimal        : numeric type
   *  LookupPolicy   : OHLCTimeSeries lookup policy
   *  RoundingPolicy : tick rounding policy
   */
  template<
    class Decimal,
    class LookupPolicy,
    template<class> class RoundingPolicy
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
    template <class Rng>
    SecPtr& shuffleAndRebuild(Rng& rng)
    {
      m_impl->shuffleFactors(rng);                    // permute overnight/day factors
      auto synSeries = m_impl->buildSeries();         // build a new OHLCTimeSeries from those factors
      m_sec->resetTimeSeries(std::move(synSeries));   // swap series pointer into the reusable Security
      return m_sec;
    }

    /// Re-initialize from a different base security (same symbol/tick shape expected).
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

    // EOD adapter
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

    // Intraday adapter
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
      IntradaySyntheticTimeSeriesImpl<Decimal, LookupPolicy, RoundingPolicy> m_impl; // :contentReference[oaicite:2]{index=2}
    };

    void initImplFrom(const SeriesT& base, const Decimal& tick, const Decimal& tickDiv2)
    {
      // Decide EOD vs Intraday from the base series, same as SyntheticTimeSeries does.
      // (Uses your series' time-frame or an equivalent indicator.)
      const auto tf = base.getTimeFrame();
      if (tf == TimeFrame::DAILY)
	m_impl = std::make_unique<EodImpl>(base, tick, tickDiv2);
      else
	m_impl = std::make_unique<IntradayImpl>(base, tick, tickDiv2);
    }

  private:
    std::unique_ptr<ImplIface> m_impl;  // chosen at runtime (EOD or Intraday)
    SecPtr                      m_sec;  // one reusable Security; series swapped per permutation
  };
} // namespace mkc_timeseries
