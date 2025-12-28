#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include "randutils.hpp"
#include "RngUtils.h"
#include "StatUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "AdaptiveRatioInternal.h"
#include "AdaptiveRatioPolicies.h"

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief m-out-of-n percentile bootstrap (stationary-block resampling aware).
     *
     * This class performs a conservative percentile bootstrap by drawing
     * \f$m=\lfloor \rho n \rfloor\f$ observations (with replacement via a provided
     * resampler) from the original sample of length \f$n\f$ for each of \f$B\f$ replicates,
     * computing a user-supplied statistic \f$\theta(\cdot)\f$ on each subsample, and
     * returning a \f$100 \cdot \mathrm{CL}\%\f$ confidence interval using type-7
     * quantiles of the bootstrap distribution \f$\{\theta^\*_b\}\f$.
     *
     * Design highlights:
     * - **Composable resampler**: Inject any resampler implementing
     *   `resampler(x, y, m, rng)` and `getL()`, e.g., a stationary block value resampler,
     *   enabling dependence-aware draws and synchronized resampling across strategies.
     * - **Small-n friendly**: Picking \f$m < n\f$ often improves coverage for small samples
     *   by reducing the influence of single outliers.
     * - **Numerically robust**: Degenerate/NaN replicates are skipped; too many degenerates
     *   raise an error to avoid misleading intervals.
     *
     * @note Thread safety: Concurrent calls to run() are not supported due to shared
     *       diagnostic storage. Use separate instances or external synchronization.
     *
     * @tparam Decimal
     *   Numeric value type (e.g., dec::decimal<8>).
     * @tparam Sampler
     *   Callable with signature `Decimal(const std::vector<Decimal>&)` that computes the
     *   statistic of interest on a series (e.g., GeoMeanStat).
     * @tparam Resampler
     *   Type that provides `void operator()(const std::vector<Decimal>& x,
     *   std::vector<Decimal>& y, std::size_t m, Rng& rng) const;` and `std::size_t getL() const;`.
     * @tparam Rng
     *   Random-number generator type. Defaults to `randutils::mt19937_rng`.
     */
template <class Decimal,
              class Sampler,
              class Resampler,
              class Rng      = std::mt19937_64,
              class Executor = concurrency::SingleThreadExecutor>
    class MOutOfNPercentileBootstrap
    {
    public:
      struct Result
      {
        Decimal     mean;              // stat on original sample
        Decimal     lower;             // percentile lower bound
        Decimal     upper;             // percentile upper bound
        double      cl;
        std::size_t B;
        std::size_t effective_B;
        std::size_t skipped;
        std::size_t n;
        std::size_t m_sub;
        std::size_t L;
        double      computed_ratio;    // logical ratio reported to callers
        double      skew_boot;         // skewness of usable bootstrap θ*'s
      };

      /// Configuration constant: maximum allowed fraction of degenerate replicates
      static constexpr double MAX_DEGENERATE_FRACTION = 0.5;

    public:
      // ====================================================================
      // CONSTRUCTOR 1: Fixed Ratio
      // ====================================================================
      MOutOfNPercentileBootstrap(std::size_t B,
                                 double      confidence_level,
                                 double      m_ratio,
                                 const Resampler& resampler)
        : m_B(B)
        , m_CL(confidence_level)
        , m_ratio(m_ratio)
        , m_resampler(resampler)
        , m_exec(std::make_shared<Executor>())
        , m_chunkHint(0)
        , m_ratioPolicy(nullptr)
        , m_diagMutex(std::make_unique<std::mutex>())
        , m_diagBootstrapStats()
        , m_diagMeanBoot(0.0)
        , m_diagVarBoot(0.0)
        , m_diagSeBoot(0.0)
        , m_diagSkewBoot(0.0)
        , m_diagValid(false)
      {
        validateParameters();
        if (!(m_ratio > 0.0 && m_ratio < 1.0))
        {
          throw std::invalid_argument("MOutOfNPercentileBootstrap: m_ratio must be in (0,1)");
        }
      }

      /// Fixed-ratio factory (thin wrapper over the existing constructor).
      static MOutOfNPercentileBootstrap
      createFixedRatio(std::size_t B,
                       double      confidence_level,
                       double      m_ratio,
                       const Resampler& resampler)
      {
        return MOutOfNPercentileBootstrap(B, confidence_level, m_ratio, resampler);
      }

      /// Adaptive-ratio factory using a caller-supplied policy.
      template<typename BootstrapStatistic>
      static MOutOfNPercentileBootstrap
      createAdaptiveWithPolicy(
        std::size_t B,
        double      confidence_level,
        const Resampler& resampler,
        std::shared_ptr<IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>> policy)
      {
        if (!policy)
        {
          throw std::invalid_argument(
            "MOutOfNPercentileBootstrap::createAdaptiveWithPolicy: policy cannot be null");
        }

        // Start from any valid fixed-ratio instance (ratio is ignored in adaptive mode).
        MOutOfNPercentileBootstrap instance(
          B,
          confidence_level,
          /*m_ratio=*/0.5,
          resampler);

        instance.m_ratio      = -1.0;  // switch to adaptive mode
        instance.m_ratioPolicy =
          std::static_pointer_cast<void>(policy);

        return instance;
      }

      /// Adaptive-ratio factory using the default TailVolatilityAdaptivePolicy.
      template<typename BootstrapStatistic>
      static MOutOfNPercentileBootstrap
      createAdaptive(std::size_t B,
                     double      confidence_level,
                     const Resampler& resampler)
      {
        auto defaultPolicy = std::make_shared<
          TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic>>();

        return createAdaptiveWithPolicy<BootstrapStatistic>(
          B, confidence_level, resampler, defaultPolicy);
      }

      // ====================================================================
      // RUN METHODS
      // ====================================================================
      Result run(const std::vector<Decimal>& x,
                 Sampler                      sampler,
                 Rng&                         rng,
                 std::size_t                  m_sub_override = 0,
                 std::ostream*                diagnosticLog  = nullptr) const
      {
        // IMPORTANT: run_core_ parallelizes the loop, so we must not touch the
        // caller-provided RNG from inside the parallel region (std::* RNGs are not thread-safe).
        // Precompute per-replicate seeds deterministically in the calling thread.
        std::vector<uint64_t> per_replicate_seeds(m_B);
        for (std::size_t b = 0; b < m_B; ++b) {
          per_replicate_seeds[b] = mkc_timeseries::rng_utils::get_random_value(rng);
        }

        auto make_engine = [&per_replicate_seeds](std::size_t b) {
          auto seq = mkc_timeseries::rng_utils::make_seed_seq(per_replicate_seeds[b]);
          return mkc_timeseries::rng_utils::construct_seeded_engine<Rng>(seq);
        };

        return run_core_(x, sampler, m_sub_override, make_engine, diagnosticLog);
      }

      template <class Provider>
      Result run(const std::vector<Decimal>& x,
                 Sampler                      sampler,
                 const Provider&              provider,
                 std::size_t                  m_sub_override = 0,
                 std::ostream*                diagnosticLog  = nullptr) const
      {
        auto make_engine = [&provider](std::size_t b) {
          // CRN: 1 engine per replicate index
          return provider.make_engine(b);
        };

        return run_core_(x, sampler, m_sub_override, make_engine, diagnosticLog);
      }

      // ====================================================================
      // Advanced refinement (two-tier API)
      // ====================================================================
      template <typename BootstrapStatistic, typename StrategyT, typename BootstrapFactoryT>
      Result runWithRefinement(
        const std::vector<Decimal>& x,
        Sampler sampler,
        StrategyT& strategy,
        BootstrapFactoryT& factory,
        int stageTag,
        int fold,
        std::ostream* diagnosticLog = nullptr) const
      {
        const std::size_t n = x.size();
        if (n < 3)
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          m_diagValid = false;
          throw std::invalid_argument("MOutOfNPercentileBootstrap::runWithRefinement: n must be >= 3");
        }

        // 1. Compute Statistical Context
        detail::StatisticalContext<Decimal> ctx(x);

        // 2. Setup Probe Maker (capturing CRN state)
        const std::size_t L_small = m_resampler.getL();

        detail::ConcreteProbeEngineMaker<Decimal, BootstrapStatistic,
                                         StrategyT, BootstrapFactoryT, Resampler>
          probeMaker(strategy, factory, stageTag, fold,
                     m_resampler, L_small, m_CL);

        // 3. Resolve Ratio using Refinement Policy
        double actual_ratio;

        if (!m_ratioPolicy)
        {
          TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic> defaultPolicy;
          actual_ratio = defaultPolicy.computeRatioWithRefinement(
            x, ctx, m_CL, m_B, probeMaker, diagnosticLog);
        }
        else
        {
          auto policy = std::static_pointer_cast<
            IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>>(m_ratioPolicy);

          if (policy)
          {
            actual_ratio = policy->computeRatioWithRefinement(
              x, ctx, m_CL, m_B, probeMaker, diagnosticLog);
          }
          else
          {
            TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic> defaultPolicy;
            actual_ratio = defaultPolicy.computeRatioWithRefinement(
              x, ctx, m_CL, m_B, probeMaker, diagnosticLog);
          }
        }

        // 4. Compute m_sub and clamp to valid range
        std::size_t m_sub = static_cast<std::size_t>(std::floor(actual_ratio * n));
        if (m_sub < 2) m_sub = 2;
        if (m_sub >= n) m_sub = n - 1;
        actual_ratio = static_cast<double>(m_sub) / static_cast<double>(n);

        // 5. Create CRN provider for main bootstrap execution
        auto [mainEngine, crnProvider] = factory.template makeMOutOfN<Decimal, BootstrapStatistic, Resampler>(
          m_B, m_CL, actual_ratio, m_resampler, strategy, stageTag, static_cast<int>(L_small), fold);

        auto make_engine = [&crnProvider](std::size_t b) {
          return crnProvider.make_engine(b);
        };

        // Use run_core_ with the computed m_sub
        return run_core_(x, sampler, m_sub, make_engine, diagnosticLog);
      }

      // ====================================================================
      // POLICY CONFIGURATION
      // ====================================================================
      template<typename BootstrapStatistic>
      void setAdaptiveRatioPolicy(
        std::shared_ptr<IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>> policy)
      {
        if (!policy)
          throw std::invalid_argument("Policy cannot be null");

        m_ratioPolicy = std::static_pointer_cast<void>(policy);
        m_ratio       = -1.0;  // switch to adaptive mode
      }

      bool isAdaptiveMode() const { return m_ratio < 0.0; }

      void setChunkSizeHint(uint32_t c) { m_chunkHint = c; }

      // Introspection
      std::size_t B()        const { return m_B; }
      double      CL()       const { return m_CL; }
      double      mratio()   const { return m_ratio; }
      const Resampler& resampler() const { return m_resampler; }

      // ====================================================================
      // Diagnostics for AutoBootstrapSelector
      // ====================================================================
      bool hasDiagnostics() const noexcept
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        return m_diagValid;
      }

      const std::vector<double>& getBootstrapStatistics() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagBootstrapStats;
        }

      double getBootstrapMean() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagMeanBoot;
        }

      double getBootstrapVariance() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagVarBoot;
        }

      double getBootstrapSe() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagSeBoot;
        }

      double getBootstrapSkewness() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagSkewBoot;
        }
      
    private:
      void ensureDiagnosticsAvailable() const
      {
        // Note: caller must hold m_diagMutex
        if (!m_diagValid)
        {
          throw std::logic_error(
            "MOutOfNPercentileBootstrap diagnostics are not available: run() has not been called on this instance.");
        }
      }

      // ====================================================================
      // INTERNAL HELPERS
      // ====================================================================
      void validateParameters() const
      {
        if (m_B == 0)
        {
          throw std::invalid_argument("MOutOfNPercentileBootstrap: B must be > 0");
        }
        if (m_B < 400)
        {
          throw std::invalid_argument("MOutOfNPercentileBootstrap: B should be >= 400 for reliable intervals");
        }
        if (!(m_CL > 0.5 && m_CL < 1.0))
        {
          throw std::invalid_argument("MOutOfNPercentileBootstrap: CL must be in (0.5,1)");
        }
      }

      // ====================================================================
      // CORE BOOTSTRAP IMPLEMENTATION
      // ====================================================================
      template <class EngineMaker>
      Result run_core_(const std::vector<Decimal>& x,
                       Sampler                      sampler,
                       std::size_t                  m_sub_override,
                       EngineMaker&&                make_engine,
                       std::ostream*                diagnosticLog = nullptr) const
      {
        const std::size_t n = x.size();
        if (n < 3)
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          m_diagValid = false;
          throw std::invalid_argument("MOutOfNPercentileBootstrap: n must be >= 3");
        }

        // Determine m_sub and ratios
        std::size_t m_sub;
        double      actual_ratio;    // ratio used internally to derive m_sub
        double      reported_ratio;  // ratio exposed via Result::computed_ratio

        if (m_sub_override > 0)
        {
          m_sub          = m_sub_override;
          actual_ratio   = static_cast<double>(m_sub) / static_cast<double>(n);
          reported_ratio = actual_ratio;
        }
        else if (isAdaptiveMode())
        {
          if (!m_ratioPolicy)
          {
            std::lock_guard<std::mutex> lock(*m_diagMutex);
            m_diagValid = false;
            throw std::runtime_error("Adaptive mode enabled but no policy set");
          }

          detail::StatisticalContext<Decimal> ctx(x);
          actual_ratio = computeAdaptiveRatio(x, ctx, diagnosticLog);
          m_sub        = static_cast<std::size_t>(std::floor(actual_ratio * n));
          reported_ratio = actual_ratio;
        }
        else
        {
          m_sub        = static_cast<std::size_t>(std::floor(m_ratio * static_cast<double>(n)));
          actual_ratio = static_cast<double>(m_sub) / static_cast<double>(n);
          reported_ratio = m_ratio;  // report configured fixed ratio, not m_sub/n
        }

        // Clamp to valid range [2, n-1]
        if (m_sub < 2)
          m_sub = 2;
        if (m_sub >= n)
          m_sub = n - 1;

        const Decimal theta_hat = sampler(x);

        // Pre-allocate; NaN marks skipped/invalid replicates
        std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

        concurrency::parallel_for_chunked(
          static_cast<uint32_t>(m_B),
          *m_exec,
          [&](uint32_t b) {
            auto rng = make_engine(b);
            std::vector<Decimal> y;
            y.resize(m_sub);
            m_resampler(x, y, m_sub, rng);
            const double v = num::to_double(sampler(y));
            if (std::isfinite(v))
              thetas_d[b] = v;
          },
          /*chunkSizeHint=*/m_chunkHint);

        // Compact NaNs and compute skipped count
        std::size_t skipped = 0;
        {
          auto it = std::remove_if(
            thetas_d.begin(),
            thetas_d.end(),
            [](double v) { return !std::isfinite(v); });
          skipped = static_cast<std::size_t>(std::distance(it, thetas_d.end()));
          thetas_d.erase(it, thetas_d.end());
        }

        // Check if too many replicates are degenerate
        // MAX_DEGENERATE_FRACTION = 0.5 means we require at least 50% valid replicates
        if (thetas_d.size() < static_cast<std::size_t>(m_B * (1.0 - MAX_DEGENERATE_FRACTION)))
        {
           std::lock_guard<std::mutex> lock(*m_diagMutex);
          m_diagValid = false;
          throw std::runtime_error(
            "MOutOfNPercentileBootstrap: too many degenerate replicates (>" + 
            std::to_string(static_cast<int>(MAX_DEGENERATE_FRACTION * 100)) + "% failed)");
        }

        // Diagnostics: mean, variance, se, skewness over usable replicates
        const std::size_t m = thetas_d.size();

        double mean_boot = 0.0;
        for (double v : thetas_d)
        {
          mean_boot += v;
        }
        mean_boot /= static_cast<double>(m);

        double var_boot = 0.0;
        if (m > 1)
        {
          for (double v : thetas_d)
          {
            const double d = v - mean_boot;
            var_boot += d * d;
          }
          var_boot /= static_cast<double>(m - 1);
        }

        const double se_boot = std::sqrt(var_boot);

        double skew_boot = 0.0;
        if (m > 2 && se_boot > 0.0)
        {
          double m3 = 0.0;
          for (double v : thetas_d)
          {
            const double d = v - mean_boot;
            m3 += d * d * d;
          }
          m3 /= static_cast<double>(m);
          skew_boot = m3 / (se_boot * se_boot * se_boot);
        }

        // Percentile CI (type-7) at CL
        const double alpha = 1.0 - m_CL;
        const double pl    = alpha / 2.0;
        const double pu    = 1.0 - alpha / 2.0;

        // Sort once and use efficient sorted quantile function
        std::sort(thetas_d.begin(), thetas_d.end());
        
        // Convert to Decimal vector for quantile function
        std::vector<Decimal> thetas_sorted;
        thetas_sorted.reserve(thetas_d.size());
        for (double v : thetas_d)
        {
          thetas_sorted.push_back(Decimal(v));
        }

        const Decimal lb = mkc_timeseries::StatUtils<Decimal>::quantileType7Sorted(thetas_sorted, pl);
        const Decimal ub = mkc_timeseries::StatUtils<Decimal>::quantileType7Sorted(thetas_sorted, pu);

        // Store diagnostics for the most recent run (with thread safety)
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          m_diagBootstrapStats = thetas_d;
          m_diagMeanBoot       = mean_boot;
          m_diagVarBoot        = var_boot;
          m_diagSeBoot         = se_boot;
          m_diagSkewBoot       = skew_boot;
          m_diagValid          = true;
        }

        return Result{
          /*mean          =*/ theta_hat,
          /*lower         =*/ lb,
          /*upper         =*/ ub,
          /*cl            =*/ m_CL,
          /*B             =*/ m_B,
          /*effective_B   =*/ thetas_d.size(),
          /*skipped       =*/ skipped,
          /*n             =*/ n,
          /*m_sub         =*/ m_sub,
          /*L             =*/ m_resampler.getL(),
          /*computed_ratio=*/ reported_ratio,
          /*skew_boot     =*/ skew_boot
        };
      }

      // ====================================================================
      // ADAPTIVE RATIO DISPATCH
      // ====================================================================
      template<typename BootstrapStatistic = Sampler>
      double computeAdaptiveRatio(const std::vector<Decimal>&                x,
                                  const detail::StatisticalContext<Decimal>& ctx,
                                  std::ostream*                              diagnosticLog) const
      {
        auto policy = std::static_pointer_cast<
          IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>>(m_ratioPolicy);

        if (policy)
        {
          return policy->computeRatio(x, ctx, m_CL, m_B, diagnosticLog);
        }

        TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic> defaultPolicy;
        return defaultPolicy.computeRatio(x, ctx, m_CL, m_B, diagnosticLog);
      }

    private:
      std::size_t  m_B;
      double       m_CL;
      double       m_ratio;       // -1.0 = adaptive mode, else fixed ratio
      Resampler    m_resampler;
      mutable std::shared_ptr<Executor> m_exec;
      mutable uint32_t           m_chunkHint{0};
      std::shared_ptr<void>      m_ratioPolicy;  // type-erased policy pointer

      // Diagnostics from most recent run (protected by mutex for thread safety)
      mutable std::unique_ptr<std::mutex> m_diagMutex;
      mutable std::vector<double> m_diagBootstrapStats;
      mutable double              m_diagMeanBoot;
      mutable double              m_diagVarBoot;
      mutable double              m_diagSeBoot;
      mutable double              m_diagSkewBoot;
      mutable bool                m_diagValid;
    };
  }
} // namespace palvalidator::analysis
