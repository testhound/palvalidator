#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <atomic>

#include "randutils.hpp"
#include "RngUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "number.h"
#include "StatUtils.h"
#include "BootstrapTypes.h"

namespace palvalidator
{
  namespace analysis
  {
    // Using declaration for IntervalType
    using palvalidator::analysis::IntervalType;

    /**
     * @brief Basic bootstrap confidence interval (reverse percentile).
     *
     * CI construction:
     *   1) θ̂ = sampler(x)
     *   2) Generate B bootstrap replicates θ*_b from resampled series of length n
     *   3) Compute percentile quantiles q_{α/2}, q_{1-α/2} of {θ*_b}
     *   4) Basic CI = [ 2 θ̂ - q_{1-α/2} , 2 θ̂ - q_{α/2} ]
     *
     * Degenerate (non-finite) replicates are skipped. If fewer than B/2 usable
     * replicates remain, an exception is thrown.
     *
     * @tparam Decimal
     *   Numeric value type.
     * @tparam Sampler
     *   Callable with signature `Decimal(const std::vector<SampleType>&)`.
     *   At bar level (SampleType = Decimal) this is `Decimal(const std::vector<Decimal>&)`.
     *   At trade level (SampleType = Trade<Decimal>) this is
     *   `Decimal(const std::vector<Trade<Decimal>>&)`.
     * @tparam Resampler
     *   Type that provides
     *     `void operator()(const std::vector<SampleType>& x,
     *                       std::vector<SampleType>&       y,
     *                       std::size_t                    m,
     *                       Rng&                           rng) const;`
     *   and `std::size_t getL() const;`.
     * @tparam Rng
     *   Random-number generator type. Defaults to `std::mt19937_64`.
     * @tparam Executor
     *   Parallel executor type used by `concurrency::parallel_for_chunked`.
     * @tparam SampleType
     *   Element type of the input vector passed to run(). Defaults to `Decimal`
     *   for bar-level bootstrapping. Set to `Trade<Decimal>` for trade-level
     *   bootstrapping, where Sampler accepts `const std::vector<Trade<Decimal>>&`
     *   and Resampler operates on `std::vector<Trade<Decimal>>`.
     *
     * @note Thread Safety:
     *   - The run() methods are NOT thread-safe when called concurrently on the
     *     same instance because they update diagnostic member variables.
     *   - Multiple threads may safely call run() on different instances.
     *   - setChunkSizeHint() may be called concurrently with run() as it uses
     *     an atomic variable.
     */
    template<
      class Decimal,
      class Sampler,
      class Resampler,
      class Rng      = std::mt19937_64,
      class Executor = concurrency::SingleThreadExecutor,
      class SampleType = Decimal
      >
    class BasicBootstrap
    {
    public:
      struct Result
      {
        Decimal     mean;         // θ̂ on original sample
        Decimal     lower;        // lower CI bound
        Decimal     upper;        // upper CI bound
        double      cl;           // confidence level
        std::size_t B;            // requested bootstrap reps
        std::size_t effective_B;  // usable (finite) reps
        std::size_t skipped;      // degenerate reps skipped
        std::size_t n;            // original sample size
        std::size_t L;            // resampler L (diagnostic)
      };

    public:
      /**
       * @brief Construct a basic bootstrap CI engine.
       *
       * @param B
       *   Number of bootstrap replicates (B >= 400 recommended).
       * @param confidence_level
       *   Confidence level CL ∈ (0.5, 1), e.g. 0.95.
       * @param resampler
       *   Resampler instance used to generate each length-n bootstrap sample.
       * @param interval_type
       *   Type of confidence interval (TWO_SIDED, ONE_SIDED_LOWER, or ONE_SIDED_UPPER).
       *   Defaults to TWO_SIDED for backward compatibility.
       *
       * @throws std::invalid_argument
       *   If B < 400 or confidence_level not in (0.5,1).
       */
      BasicBootstrap(std::size_t   B,
                     double        confidence_level,
                     const Resampler& resampler,
                     IntervalType  interval_type = IntervalType::TWO_SIDED)
        : m_B(B),
          m_CL(confidence_level),
          m_resampler(resampler),
          m_interval_type(interval_type),
          m_exec(std::make_shared<Executor>()),
          m_chunkHint(0),
          m_diagBootstrapStats(),
          m_diagMeanBoot(0.0),
          m_diagVarBoot(0.0),
          m_diagSeBoot(0.0),
          m_diagValid(false)
      {
        if (m_B < 400) {
          throw std::invalid_argument("BasicBootstrap: B should be >= 400");
        }
        if (!(m_CL > 0.5 && m_CL < 1.0)) {
          throw std::invalid_argument("BasicBootstrap: CL must be in (0.5,1)");
        }
      }

      // Move constructor
      BasicBootstrap(BasicBootstrap&& other) noexcept
        : m_B(other.m_B),
          m_CL(other.m_CL),
          m_resampler(std::move(other.m_resampler)),
          m_interval_type(other.m_interval_type),
          m_exec(std::move(other.m_exec)),
          m_chunkHint(static_cast<uint32_t>(other.m_chunkHint.load())),
          m_diagBootstrapStats(std::move(other.m_diagBootstrapStats)),
          m_diagMeanBoot(other.m_diagMeanBoot),
          m_diagVarBoot(other.m_diagVarBoot),
          m_diagSeBoot(other.m_diagSeBoot),
          m_diagValid(other.m_diagValid)
      {
      }

      // Move assignment operator
      BasicBootstrap& operator=(BasicBootstrap&& other) noexcept
      {
        if (this != &other)
        {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_resampler = std::move(other.m_resampler);
          m_interval_type = other.m_interval_type;
          m_exec = std::move(other.m_exec);
          m_chunkHint.store(static_cast<uint32_t>(other.m_chunkHint.load()));
          m_diagBootstrapStats = std::move(other.m_diagBootstrapStats);
          m_diagMeanBoot = other.m_diagMeanBoot;
          m_diagVarBoot = other.m_diagVarBoot;
          m_diagSeBoot = other.m_diagSeBoot;
          m_diagValid = other.m_diagValid;
        }
        return *this;
      }

      // Delete copy constructor and copy assignment operator
      BasicBootstrap(const BasicBootstrap&) = delete;
      BasicBootstrap& operator=(const BasicBootstrap&) = delete;

      /**
       * @brief Run the basic-bootstrap CI using a caller-supplied RNG.
       *
       * After this call, diagnostic getters (getBootstrapStatistics,
       * getBootstrapMean/Variance/Se) refer to this run's results.
       *
       * @note This method is NOT thread-safe when called concurrently on the
       *       same instance. The RNG is protected by a mutex for use within
       *       parallel bootstrap iterations, but diagnostic members are updated
       *       without synchronization.
       */
      Result run(const std::vector<SampleType>& x,
                 Sampler                         sampler,
                 Rng&                            rng)
      {
        // Pre-generate seeds sequentially to ensure determinism.
        // Even with a mutex, grabbing seeds inside the parallel loop 
        // makes the assignment of seeds to replicates dependent on thread scheduling.
        std::vector<uint64_t> seeds;
        seeds.reserve(m_B);
        for (std::size_t i = 0; i < m_B; ++i)
	  seeds.push_back(mkc_timeseries::rng_utils::get_random_value(rng));

        // Capture seeds by move
        auto make_engine = [seeds = std::move(seeds)](std::size_t b) {
          const uint64_t seed = seeds[b];
          auto           seq  = mkc_timeseries::rng_utils::make_seed_seq(seed);
          return mkc_timeseries::rng_utils::construct_seeded_engine<Rng>(seq);
        };

        // Note: run_core_ handles the parallel execution using these deterministic seeds
        return run_core_(x, sampler, make_engine);
      }

      /**
       * @brief Run with an engine provider (CRN-friendly).
       *
       * After this call, diagnostic getters (getBootstrapStatistics,
       * getBootstrapMean/Variance/Se) refer to this run's results.
       *
       * @note This method is NOT thread-safe when called concurrently on the
       *       same instance due to updates to diagnostic members.
       */
      template<class Provider>
      Result run(const std::vector<SampleType>& x,
                 Sampler                         sampler,
                 const Provider&                 provider)
      {
        auto make_engine = [&provider](std::size_t b) {
          return provider.make_engine(b);
        };

        return run_core_(x, sampler, make_engine);
      }

      /**
       * @brief Hint for chunk size in parallel_for_chunked.
       *
       * @note This method is thread-safe and may be called concurrently with run().
       */
      void setChunkSizeHint(uint32_t c) const
      {
        m_chunkHint.store(c, std::memory_order_relaxed);
      }

      std::size_t      B()         const { return m_B; }
      double           CL()        const { return m_CL; }
      const Resampler& resampler() const { return m_resampler; }

      // ------------------------------------------------------------------
      // Diagnostics for AutoBootstrapSelector
      // ------------------------------------------------------------------

      /**
       * @brief Returns true if this instance has diagnostics from a previous run().
       */
      bool hasDiagnostics() const noexcept
      {
        return m_diagValid;
      }

      /**
       * @brief Returns the usable bootstrap statistics {θ*_b} from the last run.
       *
       * Values are stored after removal of non-finite replicates. The size of
       * this vector equals the effective_B of the last Result.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      const std::vector<double>& getBootstrapStatistics() const
      {
        ensureDiagnosticsAvailable();
        return m_diagBootstrapStats;
      }

      /**
       * @brief Returns the bootstrap mean of θ* from the last run.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      double getBootstrapMean() const
      {
        ensureDiagnosticsAvailable();
        return m_diagMeanBoot;
      }

      /**
       * @brief Returns the bootstrap variance of θ* from the last run.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      double getBootstrapVariance() const
      {
        ensureDiagnosticsAvailable();
        return m_diagVarBoot;
      }

      /**
       * @brief Returns the bootstrap standard error (sqrt(variance)) from the last run.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      double getBootstrapSe() const
      {
        ensureDiagnosticsAvailable();
        return m_diagSeBoot;
      }

    private:
      void ensureDiagnosticsAvailable() const
      {
        if (!m_diagValid) {
          throw std::logic_error(
            "BasicBootstrap diagnostics are not available: run() has not been called on this instance.");
        }
      }

      template<class EngineMaker>
      Result run_core_(const std::vector<SampleType>& x,
                       Sampler                         sampler,
                       EngineMaker&&                   make_engine)
      {
        const std::size_t n = x.size();
        if (n < 3) {
          m_diagValid = false;
          throw std::invalid_argument("BasicBootstrap: n must be >= 3");
        }

        const Decimal theta_hat = sampler(x);

        std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

        // Load chunk hint once before parallel region
        const uint32_t chunk_hint = m_chunkHint.load(std::memory_order_relaxed);

        concurrency::parallel_for_chunked(
          static_cast<uint32_t>(m_B),
          *m_exec,
          [&](uint32_t b) {
            auto rng_b = make_engine(b);
            std::vector<SampleType> y;
            y.resize(n);
            // n-out-of-n: m = n
            m_resampler(x, y, n, rng_b);
            const double v = num::to_double(sampler(y));
            if (std::isfinite(v)) {
              thetas_d[b] = v;
            }
          },
          /*chunkSizeHint=*/chunk_hint
        );

        std::size_t skipped = 0;
        {
          auto it = std::remove_if(
            thetas_d.begin(), thetas_d.end(),
            [](double v) { return !std::isfinite(v); });
          skipped = static_cast<std::size_t>(std::distance(it, thetas_d.end()));
          thetas_d.erase(it, thetas_d.end());
        }

        if (thetas_d.size() < m_B / 2) {
          m_diagValid = false;
          throw std::runtime_error("BasicBootstrap: too many degenerate replicates");
        }

        // Compute diagnostics: mean, variance, se over usable replicates
        const std::size_t m = thetas_d.size();

        double mean_boot = 0.0;
        for (double v : thetas_d) {
          mean_boot += v;
        }
        mean_boot /= static_cast<double>(m);

        double var_boot = 0.0;
        if (m > 1) {
          for (double v : thetas_d) {
            const double d = v - mean_boot;
            var_boot += d * d;
          }
          var_boot /= static_cast<double>(m - 1);
        }

        const double se_boot = std::sqrt(var_boot);

        // Calculate quantiles based on interval type
        const double alpha = 1.0 - m_CL;
        double pl = 0.0;  // Initialize to suppress compiler warning
        double pu = 1.0;  // Initialize to suppress compiler warning

	switch (m_interval_type) {
          case IntervalType::TWO_SIDED:
            pl = alpha / 2.0;
            pu = 1.0 - alpha / 2.0;
            break;
            
          case IntervalType::ONE_SIDED_LOWER:
            // Basic Bootstrap Lower Bound = 2*theta - q_{1-alpha}
            // Code uses: lower = 2*theta - q_hi (where q_hi is quantile at pu)
            // Therefore: pu must be 1.0 - alpha.
            pl = 1e-10;       // Upper bound becomes effectively unbounded (2*theta - min)
            pu = 1.0 - alpha; // Finite lower bound
            break;
            
          case IntervalType::ONE_SIDED_UPPER:
            // Basic Bootstrap Upper Bound = 2*theta - q_{alpha}
            // Code uses: upper = 2*theta - q_lo (where q_lo is quantile at pl)
            // Therefore: pl must be alpha.
            pl = alpha;       // Finite upper bound
            pu = 1.0 - 1e-10; // Lower bound becomes effectively unbounded (2*theta - max)
            break;
        }
	
        const double q_lo = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(thetas_d, pl);
        const double q_hi = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(thetas_d, pu);

        const double center = num::to_double(theta_hat);
        const double lb_d   = 2.0 * center - q_hi; // 2θ̂ - q_{pu} (reverse)
        const double ub_d   = 2.0 * center - q_lo; // 2θ̂ - q_{pl} (reverse)

        // Store diagnostics for last run
        m_diagBootstrapStats = thetas_d;
        m_diagMeanBoot       = mean_boot;
        m_diagVarBoot        = var_boot;
        m_diagSeBoot         = se_boot;
        m_diagValid          = true;

        return Result{
          /*mean        =*/ theta_hat,
          /*lower       =*/ Decimal(lb_d),
          /*upper       =*/ Decimal(ub_d),
          /*cl          =*/ m_CL,
          /*B           =*/ m_B,
          /*effective_B =*/ thetas_d.size(),
          /*skipped     =*/ skipped,
          /*n           =*/ n,
          /*L           =*/ m_resampler.getL()
        };
      }

    private:
      std::size_t                       m_B;
      double                            m_CL;
      Resampler                         m_resampler;
      IntervalType                      m_interval_type;
      mutable std::shared_ptr<Executor> m_exec;
      mutable std::atomic<uint32_t>     m_chunkHint;

      // Diagnostics from most recent run(...)
      // Note: These are updated without synchronization, so run() methods
      // are not thread-safe when called concurrently on the same instance.
      std::vector<double> m_diagBootstrapStats;
      double              m_diagMeanBoot;
      double              m_diagVarBoot;
      double              m_diagSeBoot;
      bool                m_diagValid;
    };

  } // namespace analysis
} // namespace palvalidator
