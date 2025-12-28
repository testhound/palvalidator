#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <mutex>

#include "randutils.hpp"
#include "RngUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "StatUtils.h"
#include "NormalDistribution.h"

namespace palvalidator
{
  namespace analysis
  {

    /**
     * @brief Normal (Wald) bootstrap confidence interval using bootstrap SD.
     *
     * CI construction:
     *   1) θ̂ = sampler(x)
     *   2) Generate B bootstrap replicates θ*_b from resampled series of length n
     *   3) Compute bootstrap standard deviation se_boot = sd({θ*_b})
     *   4) CI = [ θ̂ - z_{α/2} * se_boot , θ̂ + z_{α/2} * se_boot ]
     *
     * Degenerate (non-finite) replicates are skipped. If fewer than B/2 usable
     * replicates remain, an exception is thrown.
     *
     * Thread-safety: This class uses internal mutexes to protect mutable state.
     * Multiple threads can safely call run() concurrently on the same instance,
     * and diagnostic getters are protected against concurrent modification.
     *
     * @tparam Decimal
     *   Numeric value type.
     * @tparam Sampler
     *   Callable with signature `Decimal(const std::vector<Decimal>&)`.
     * @tparam Resampler
     *   Type that provides
     *     `void operator()(const std::vector<Decimal>& x,
     *                       std::vector<Decimal>&       y,
     *                       std::size_t                 m,
     *                       Rng&                        rng) const;`
     *   and `std::size_t getL() const;`.
     * @tparam Rng
     *   Random-number generator type. Defaults to `std::mt19937_64`.
     * @tparam Executor
     *   Parallel executor type used by `concurrency::parallel_for_chunked`.
     */
    template<
      class Decimal,
      class Sampler,
      class Resampler,
      class Rng      = std::mt19937_64,
      class Executor = concurrency::SingleThreadExecutor
      >
    class NormalBootstrap
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
        double      se_boot;      // bootstrap standard error
      };

    public:
      NormalBootstrap(std::size_t   B,
                      double        confidence_level,
                      const Resampler& resampler)
        : m_B(B),
          m_CL(confidence_level),
          m_resampler(resampler),
          m_exec(std::make_shared<Executor>()),
          m_chunkHint(0),
          m_diagBootstrapStats(),
          m_diagMeanBoot(0.0),
          m_diagVarBoot(0.0),
          m_diagSeBoot(0.0),
          m_diagValid(false),
          m_diagMutex(),
          m_chunkMutex()
      {
        if (m_B < 400) {
          throw std::invalid_argument("NormalBootstrap: B should be >= 400");
        }
        if (!(m_CL > 0.5 && m_CL < 1.0)) {
          throw std::invalid_argument("NormalBootstrap: CL must be in (0.5,1)");
        }
      }

      // Copy constructor
      NormalBootstrap(const NormalBootstrap& other)
        : m_B(other.m_B),
          m_CL(other.m_CL),
          m_resampler(other.m_resampler),
          m_exec(std::make_shared<Executor>()),
          m_chunkHint(0),
          m_diagBootstrapStats(),
          m_diagMeanBoot(0.0),
          m_diagVarBoot(0.0),
          m_diagSeBoot(0.0),
          m_diagValid(false),
          m_diagMutex(),
          m_chunkMutex()
      {
      }

      // Move constructor
      NormalBootstrap(NormalBootstrap&& other) noexcept
        : m_B(other.m_B),
          m_CL(other.m_CL),
          m_resampler(std::move(other.m_resampler)),
          m_exec(std::move(other.m_exec)),
          m_chunkHint(other.m_chunkHint),
          m_diagBootstrapStats(std::move(other.m_diagBootstrapStats)),
          m_diagMeanBoot(other.m_diagMeanBoot),
          m_diagVarBoot(other.m_diagVarBoot),
          m_diagSeBoot(other.m_diagSeBoot),
          m_diagValid(other.m_diagValid),
          m_diagMutex(),
          m_chunkMutex()
      {
        // Reset moved-from object
        other.m_diagValid = false;
      }

      // Copy assignment operator
      NormalBootstrap& operator=(const NormalBootstrap& other)
      {
        if (this != &other) {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_resampler = other.m_resampler;
          m_exec = std::make_shared<Executor>();
          m_chunkHint = 0;
          
          // Clear diagnostic data
          std::lock_guard<std::mutex> lock(m_diagMutex);
          m_diagBootstrapStats.clear();
          m_diagMeanBoot = 0.0;
          m_diagVarBoot = 0.0;
          m_diagSeBoot = 0.0;
          m_diagValid = false;
        }
        return *this;
      }

      // Move assignment operator
      NormalBootstrap& operator=(NormalBootstrap&& other) noexcept
      {
        if (this != &other) {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_resampler = std::move(other.m_resampler);
          m_exec = std::move(other.m_exec);
          m_chunkHint = other.m_chunkHint;
          
          // Transfer diagnostic data
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            m_diagBootstrapStats = std::move(other.m_diagBootstrapStats);
            m_diagMeanBoot = other.m_diagMeanBoot;
            m_diagVarBoot = other.m_diagVarBoot;
            m_diagSeBoot = other.m_diagSeBoot;
            m_diagValid = other.m_diagValid;
          }
          
          // Reset moved-from object
          other.m_diagValid = false;
        }
        return *this;
      }

      /**
       * @brief Run the normal-bootstrap CI using a caller-supplied RNG.
       *
       * After this call, diagnostic getters (getBootstrapStatistics, getBootstrapMean,
       * getBootstrapVariance, getBootstrapSe) will refer to this run's results.
       *
       * Thread-safe: Multiple threads can call run() concurrently on the same instance.
       */
      Result run(const std::vector<Decimal>& x,
                 Sampler                      sampler,
                 Rng&                         rng) const
      {
        auto make_engine = [&rng](std::size_t /*b*/) {
          const uint64_t seed = mkc_timeseries::rng_utils::get_random_value(rng);
          auto           seq  = mkc_timeseries::rng_utils::make_seed_seq(seed);
          return mkc_timeseries::rng_utils::construct_seeded_engine<Rng>(seq);
        };

        return run_core_(x, sampler, make_engine);
      }

      /**
       * @brief Run with an engine provider (CRN-friendly).
       *
       * After this call, diagnostic getters (getBootstrapStatistics, getBootstrapMean,
       * getBootstrapVariance, getBootstrapSe) will refer to this run's results.
       *
       * Thread-safe: Multiple threads can call run() concurrently on the same instance.
       */
      template<class Provider>
      Result run(const std::vector<Decimal>& x,
                 Sampler                      sampler,
                 const Provider&              provider) const
      {
        auto make_engine = [&provider](std::size_t b) {
          return provider.make_engine(b);
        };

        return run_core_(x, sampler, make_engine);
      }

      /**
       * @brief Hint for chunk size in parallel_for_chunked.
       *
       * Thread-safe: Can be called concurrently with run() and other methods.
       */
      void setChunkSizeHint(uint32_t c) const
      {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        m_chunkHint = c;
      }

      std::size_t      B()         const { return m_B; }
      double           CL()        const { return m_CL; }
      const Resampler& resampler() const { return m_resampler; }

      // --------------------------------------------------------------------
      // Diagnostics for AutoBootstrapSelector
      // --------------------------------------------------------------------

      /**
       * @brief Returns true if this instance has diagnostics from a previous run().
       *
       * The diagnostics below (bootstrap stats, mean, variance, se) all refer to
       * the *most recent* successful call to run(...) on this instance.
       *
       * Thread-safe: Protected by mutex.
       */
      bool hasDiagnostics() const noexcept
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        return m_diagValid;
      }

      /**
       * @brief Returns the usable bootstrap statistics {θ*_b} from the last run.
       *
       * Values are stored after removal of non-finite replicates. The size of
       * this vector equals the effective_B of the last Result.
       *
       * Thread-safe: Returns a copy of the diagnostic data protected by mutex.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      std::vector<double> getBootstrapStatistics() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagBootstrapStats;
      }

      /**
       * @brief Returns the bootstrap mean of θ* from the last run.
       *
       * Thread-safe: Protected by mutex.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      double getBootstrapMean() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagMeanBoot;
      }

      /**
       * @brief Returns the bootstrap variance of θ* from the last run.
       *
       * Thread-safe: Protected by mutex.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      double getBootstrapVariance() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagVarBoot;
      }

      /**
       * @brief Returns the bootstrap standard error (sqrt(variance)) from the last run.
       *
       * Equivalent to the se_boot field in the last Result.
       *
       * Thread-safe: Protected by mutex.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      double getBootstrapSe() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagSeBoot;
      }

    private:
      void ensureDiagnosticsAvailable() const
      {
        // NOTE: This method must be called while holding m_diagMutex
        if (!m_diagValid) {
          throw std::logic_error(
            "NormalBootstrap diagnostics are not available: run() has not been called on this instance.");
        }
      }

      template<class EngineMaker>
      Result run_core_(const std::vector<Decimal>& x,
                       Sampler                      sampler,
                       EngineMaker&&                make_engine) const
      {
        const std::size_t n = x.size();
        if (n < 3) {
          throw std::invalid_argument("NormalBootstrap: n must be >= 3");
        }

        const Decimal theta_hat = sampler(x);

        // Pre-allocate; NaN marks skipped/invalid replicates
        std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

        // Read chunk hint under lock
        uint32_t chunkHint;
        {
          std::lock_guard<std::mutex> lock(m_chunkMutex);
          chunkHint = m_chunkHint;
        }

        // Parallel over B using the internally constructed Executor
        concurrency::parallel_for_chunked(
          static_cast<uint32_t>(m_B),
          *m_exec,
          [&](uint32_t b) {
            auto rng_b = make_engine(b);
            std::vector<Decimal> y;
            y.resize(n);
            // n-out-of-n: m = n
            m_resampler(x, y, n, rng_b);
            const double v = num::to_double(sampler(y));
            if (std::isfinite(v)) {
              thetas_d[b] = v;
            }
          },
          /*chunkSizeHint=*/chunkHint
        );

        // Compact away NaNs and count skipped
        std::size_t skipped = 0;
        {
          auto it = std::remove_if(
            thetas_d.begin(), thetas_d.end(),
            [](double v) { return !std::isfinite(v); });
          skipped = static_cast<std::size_t>(std::distance(it, thetas_d.end()));
          thetas_d.erase(it, thetas_d.end());
        }

        if (thetas_d.size() < m_B / 2) {
          // Invalidate diagnostics in case of error
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            m_diagValid = false;
          }
          throw std::runtime_error("NormalBootstrap: too many degenerate replicates");
        }

        const std::size_t m = thetas_d.size();

        // Bootstrap mean and variance of θ*
        double mean_boot = 0.0;
        for (double v : thetas_d) {
          mean_boot += v;
        }
        mean_boot /= static_cast<double>(m);

        double var_boot = 0.0;
        for (double v : thetas_d) {
          const double d = v - mean_boot;
          var_boot += d * d;
        }
        if (m > 1) {
          var_boot /= static_cast<double>(m - 1);
        }

        const double se_boot = std::sqrt(var_boot);
        const double alpha   = 1.0 - m_CL;
        const double z       = mkc_timeseries::NormalDistribution::inverseNormalCdf(1.0 - alpha / 2.0);

        const double center  = num::to_double(theta_hat);
        const double lb_d    = center - z * se_boot;
        const double ub_d    = center + z * se_boot;

        // Store diagnostics for the most recent run (protected by mutex)
        {
          std::lock_guard<std::mutex> lock(m_diagMutex);
          m_diagBootstrapStats = thetas_d;
          m_diagMeanBoot       = mean_boot;
          m_diagVarBoot        = var_boot;
          m_diagSeBoot         = se_boot;
          m_diagValid          = true;
        }

        return Result{
          /*mean        =*/ theta_hat,
          /*lower       =*/ Decimal(lb_d),
          /*upper       =*/ Decimal(ub_d),
          /*cl          =*/ m_CL,
          /*B           =*/ m_B,
          /*effective_B =*/ thetas_d.size(),
          /*skipped     =*/ skipped,
          /*n           =*/ n,
          /*L           =*/ m_resampler.getL(),
          /*se_boot     =*/ se_boot
        };
      }

    private:
      std::size_t                       m_B;
      double                            m_CL;
      Resampler                         m_resampler;
      mutable std::shared_ptr<Executor> m_exec;
      mutable uint32_t                  m_chunkHint;

      // Diagnostics from the most recent run(...)
      mutable std::vector<double> m_diagBootstrapStats;
      mutable double              m_diagMeanBoot;
      mutable double              m_diagVarBoot;
      mutable double              m_diagSeBoot;
      mutable bool                m_diagValid;

      // Mutexes for thread-safety
      mutable std::mutex m_diagMutex;   // Protects all diagnostic members
      mutable std::mutex m_chunkMutex;  // Protects m_chunkHint
    };

  } // namespace analysis
} // namespace palvalidator
