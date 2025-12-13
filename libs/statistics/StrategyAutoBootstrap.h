#pragma once

#include <vector>
#include <optional>
#include <ostream>
#include <stdexcept>

#include "TradingBootstrapFactory.h"
#include "AutoBootstrapSelector.h"
#include "MOutOfNPercentileBootstrap.h"
#include "PercentileTBootstrap.h"
#include "BasicBootstrap.h"
#include "NormalBootstrap.h"
#include "PercentileBootstrap.h"
#include "BiasCorrectedBootstrap.h"
#include "BacktesterStrategy.h"
#include "StatUtils.h"
#include "StationaryMaskResamplers.h"  // for typical Resampler choice

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Core numerical / CRN configuration for the strategy bootstrap.
     *
     *  - numBootStrapReplications: B for all single-level bootstraps (Normal / Basic /
     *                              Percentile / (adaptive) M-out-of-N) and also the
     *                              outer B for Percentile-T.
     *  - blockSize               : Stationary block bootstrap length L used by resamplers.
     *  - confidenceInterval      : Confidence level (e.g. 0.95).
     *  - stageTag / fold         : CRN domain tags (stage, fold) passed into the factory.
     *
     * Internally:
     *  - Percentile-T uses outer B = numBootStrapReplications.
     *  - Inner B is derived from the fixed ratio percentileTRatioOuterToInner = 10,
     *    so B_inner = max(1, B_outer / 10).
     */
    class BootstrapConfiguration
    {
    public:
      BootstrapConfiguration(std::size_t numBootStrapReplications,
                             std::size_t blockSize,
                             double      confidenceInterval,
                             std::uint64_t stageTag,
                             std::uint64_t fold)
        : m_numBootStrapReplications(numBootStrapReplications),
	m_blockSize(blockSize),
	m_confidenceInterval(confidenceInterval),
	m_stageTag(stageTag),
	m_fold(fold),
	m_percentileTRatioOuterToInner(10u)
      {}

      std::size_t getNumBootStrapReplications() const
      {
        return m_numBootStrapReplications;
      }

      std::size_t getBlockSize() const
      {
        return m_blockSize;
      }

      double getConfidenceInterval() const
      {
        return m_confidenceInterval;
      }

      std::uint64_t getStageTag() const
      {
        return m_stageTag;
      }

      std::uint64_t getFold() const
      {
        return m_fold;
      }

      /// Outer B for Percentile-T bootstrap.
      std::size_t getPercentileTNumOuterReplications() const
      {
        return m_numBootStrapReplications;
      }

      /// Inner B for Percentile-T bootstrap (outer / ratio, at least 1).
      std::size_t getPercentileTNumInnerReplications() const
      {
        std::size_t outer = getPercentileTNumOuterReplications();
        std::size_t ratio = m_percentileTRatioOuterToInner;
        if (ratio == 0)
          {
            ratio = 1; // safety: avoid division by zero
          }
        std::size_t inner = outer / ratio;
        return inner == 0 ? std::size_t(1) : inner;
      }

      /// Ratio "outer / inner" for Percentile-T.
      std::size_t getPercentileTRatioOuterToInner() const
      {
        return m_percentileTRatioOuterToInner;
      }

    private:
      std::size_t   m_numBootStrapReplications;
      std::size_t   m_blockSize;
      double        m_confidenceInterval;
      std::uint64_t m_stageTag;
      std::uint64_t m_fold;
      std::size_t   m_percentileTRatioOuterToInner;
    };

    /**
     * @brief Configuration of which bootstrap algorithms are enabled.
     *
     * All flags default to true, and there are no setters (immutable after construction).
     */
    class BootstrapAlgorithmsConfiguration
    {
    public:
      explicit BootstrapAlgorithmsConfiguration(bool enableNormal     = true,
                                                bool enableBasic      = true,
                                                bool enablePercentile = true,
                                                bool enableMOutOfN    = true,
                                                bool enablePercentileT = true,
                                                bool enableBCa        = true)
        : m_enableNormal(enableNormal),
	  m_enableBasic(enableBasic),
	  m_enablePercentile(enablePercentile),
	  m_enableMOutOfN(enableMOutOfN),
	  m_enablePercentileT(enablePercentileT),
	  m_enableBCa(enableBCa)
      {}

      bool enableNormal() const
      {
        return m_enableNormal;
      }

      bool enableBasic() const
      {
        return m_enableBasic;
      }

      bool enablePercentile() const
      {
        return m_enablePercentile;
      }

      bool enableMOutOfN() const
      {
        return m_enableMOutOfN;
      }

      bool enablePercentileT() const
      {
        return m_enablePercentileT;
      }

      bool enableBCa() const
      {
        return m_enableBCa;
      }

    private:
      bool m_enableNormal;
      bool m_enableBasic;
      bool m_enablePercentile;
      bool m_enableMOutOfN;
      bool m_enablePercentileT;
      bool m_enableBCa;
    };

    /**
     * @brief High-level helper to compute an automatically-selected bootstrap CI
     *        for a trading strategy, using all available bootstrap engines.
     *
     * The class:
     *   - Uses TradingBootstrapFactory to construct engines with CRN-safe RNGs.
     *   - Runs Normal, Basic, Percentile, adaptive M-out-of-N, Percentile-t, and BCa
     *     (subject to BootstrapAlgorithmsConfiguration flags).
     *   - Converts each engine's result into an AutoBootstrapSelector::Candidate.
     *   - Calls AutoBootstrapSelector<Decimal>::select(...) and returns AutoCIResult<Decimal>.
     *
     * Template parameters:
     *   Decimal   : numeric type (e.g., mkc_timeseries::Decimal)
     *   Sampler   : statistic functor (e.g., mkc_timeseries::GeoMeanStat<Decimal>)
     *   Resampler : resampling policy for the percentile-like and Percentile-t engines
     *               (e.g., palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal>)
     */
    template <class Decimal,
              class Sampler,
              class Resampler>
    class StrategyAutoBootstrap
    {
    public:
      using Num        = Decimal;
      using Result     = AutoCIResult<Decimal>;
      using Selector   = AutoBootstrapSelector<Decimal>;
      using MethodId   = typename Result::MethodId;
      using Candidate  = typename Result::Candidate;
      using Factory    = ::TradingBootstrapFactory<>;

      // BCa resampler is always a stationary block bootstrap in the current design.
      using BCaResampler = mkc_timeseries::StationaryBlockResampler<Decimal>;

      StrategyAutoBootstrap(Factory& factory,
                            const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                            const BootstrapConfiguration& bootstrapConfiguration,
                            const BootstrapAlgorithmsConfiguration& algorithmsConfiguration)
        : m_factory(factory),
	  m_strategy(strategy),
	  m_bootstrapConfiguration(bootstrapConfiguration),
	  m_algorithmsConfiguration(algorithmsConfiguration)
      {}

      /**
       * @brief Run all configured bootstrap engines on @p returns and select the best CI.
       *
       * @param returns  Return series (typically high-res mark-to-market returns).
       * @param os       Optional logging stream. If non-null, engine failures are logged.
       *
       * @return AutoCIResult<Decimal> encapsulating the chosen method and all candidates.
       *
       * @throws std::runtime_error if no engine produced a usable candidate.
       */
      Result run(const std::vector<Decimal>& returns, std::ostream* os = nullptr)
      {
        std::vector<Candidate> candidates;
        candidates.reserve(6);

        if (returns.size() < 2)
          {
            throw std::runtime_error(
              "StrategyAutoBootstrap::run: need at least 2 returns to bootstrap.");
          }

        const std::size_t B_single   = m_bootstrapConfiguration.getNumBootStrapReplications();
        const double      cl         = m_bootstrapConfiguration.getConfidenceInterval();
        const std::size_t blockSize  = m_bootstrapConfiguration.getBlockSize();
        const std::uint64_t stageTag = m_bootstrapConfiguration.getStageTag();
        const std::uint64_t fold     = m_bootstrapConfiguration.getFold();

        const std::size_t B_outer = m_bootstrapConfiguration.getPercentileTNumOuterReplications();
        const std::size_t B_inner = m_bootstrapConfiguration.getPercentileTNumInnerReplications();

        // Shared resampler for percentile-like / Percentile-t engines.
        Resampler resampler(blockSize);

        // 1) Normal bootstrap
        if (m_algorithmsConfiguration.enableNormal())
          {
            try
              {
                auto [engine, crn] =
                  m_factory.template makeNormal<Decimal, Sampler, Resampler>(
                    B_single,
                    cl,
                    resampler,
                    m_strategy,
                    stageTag,
                    static_cast<uint64_t>(blockSize),
                    fold);

                auto res = engine.run(returns, Sampler(), crn);
                candidates.push_back(
                  Selector::template summarizePercentileLike(
                    MethodId::Normal, engine, res));
              }
            catch (const std::exception& e)
              {
                if (os)
                  {
                    (*os) << "   [AutoCI] NormalBootstrap failed: "
                          << e.what() << "\n";
                  }
              }
          }

        // 2) Basic bootstrap
        if (m_algorithmsConfiguration.enableBasic())
          {
            try
              {
                auto [engine, crn] =
                  m_factory.template makeBasic<Decimal, Sampler, Resampler>(
                    B_single,
                    cl,
                    resampler,
                    m_strategy,
                    stageTag,
                    static_cast<uint64_t>(blockSize),
                    fold);

                auto res = engine.run(returns, Sampler(), crn);
                candidates.push_back(
                  Selector::template summarizePercentileLike(
                    MethodId::Basic, engine, res));
              }
            catch (const std::exception& e)
              {
                if (os)
                  {
                    (*os) << "   [AutoCI] BasicBootstrap failed: "
                          << e.what() << "\n";
                  }
              }
          }

        // 3) Percentile bootstrap
        if (m_algorithmsConfiguration.enablePercentile())
          {
            try
              {
                auto [engine, crn] =
                  m_factory.template makePercentile<Decimal, Sampler, Resampler>(
                    B_single,
                    cl,
                    resampler,
                    m_strategy,
                    stageTag,
                    static_cast<uint64_t>(blockSize),
                    fold);

                auto res = engine.run(returns, Sampler(), crn);
                candidates.push_back(
                  Selector::template summarizePercentileLike(
                    MethodId::Percentile, engine, res));
              }
            catch (const std::exception& e)
              {
                if (os)
                  {
                    (*os) << "   [AutoCI] PercentileBootstrap failed: "
                          << e.what() << "\n";
                  }
              }
          }

        // 4) Adaptive M-out-of-n Percentile bootstrap
        if (m_algorithmsConfiguration.enableMOutOfN())
          {
            try
              {
                auto [engine, crn] =
                  m_factory.template makeAdaptiveMOutOfN<Decimal, Sampler, Resampler>(
                    B_single,
                    cl,
                    resampler,
                    m_strategy,
                    stageTag,
                    static_cast<uint64_t>(blockSize),
                    fold);

                auto res = engine.run(returns, Sampler(), crn);
                candidates.push_back(
                  Selector::template summarizePercentileLike(
                    MethodId::MOutOfN, engine, res));
              }
            catch (const std::exception& e)
              {
                if (os)
                  {
                    (*os) << "   [AutoCI] Adaptive MOutOfNPercentileBootstrap failed: "
                          << e.what() << "\n";
                  }
              }
          }

        // 5) Percentile-t (double bootstrap)
        if (m_algorithmsConfiguration.enablePercentileT())
          {
            try
              {
                auto [engine, crn] =
                  m_factory.template makePercentileT<Decimal, Sampler, Resampler>(
                    B_outer,
                    B_inner,
                    cl,
                    resampler,
                    m_strategy,
                    stageTag,
                    static_cast<uint64_t>(blockSize),
                    fold /* use default m_ratio_outer/inner = 1.0 */);

                auto res = engine.run(returns, Sampler(), crn);
                candidates.push_back(
                  Selector::template summarizePercentileT(engine, res));
              }
            catch (const std::exception& e)
              {
                if (os)
                  {
                    (*os) << "   [AutoCI] PercentileTBootstrap failed: "
                          << e.what() << "\n";
                  }
              }
          }

        // 6) BCa (Bias-Corrected and Accelerated)
        if (m_algorithmsConfiguration.enableBCa())
          {
            try
              {
                BCaResampler bcaResampler(blockSize);

                // Use the same statistic as Sampler, but expressed as a std::function.
                std::function<Decimal(const std::vector<Decimal>&)> statFn =
                  [](const std::vector<Decimal>& r) { return Sampler()(r); };

                auto bcaEngine =
                  m_factory.template makeBCa<Decimal, BCaResampler>(
                    returns,
                    static_cast<unsigned>(B_single),
                    cl,
                    statFn,
                    bcaResampler,
                    m_strategy,
                    stageTag,
                    static_cast<uint64_t>(blockSize),
                    fold);

                // BCaBootStrap computes its statistics during construction; no run() needed.
                candidates.push_back(Selector::template summarizeBCa(bcaEngine));
              }
            catch (const std::exception& e)
              {
                if (os)
                  {
                    (*os) << "   [AutoCI] BCaBootstrap failed: "
                          << e.what() << "\n";
                  }
              }
          }

        if (candidates.empty())
          {
            throw std::runtime_error(
              "StrategyAutoBootstrap::run: no bootstrap candidate succeeded.");
          }

        // Let AutoBootstrapSelector perform the hierarchy-of-trust selection.
        return Selector::select(candidates);
      }

    private:
      Factory& m_factory;
      const mkc_timeseries::BacktesterStrategy<Decimal>& m_strategy;
      BootstrapConfiguration             m_bootstrapConfiguration;
      BootstrapAlgorithmsConfiguration   m_algorithmsConfiguration;
    };
  } // namespace analysis
} // namespace palvalidator
