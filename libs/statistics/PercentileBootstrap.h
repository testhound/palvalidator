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
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "StatUtils.h"
#include "BootstrapTypes.h"

namespace palvalidator
{
  namespace analysis
  {
    // Using declaration for IntervalType
    using palvalidator::analysis::IntervalType;

    /**
     * @brief Standard n-out-of-n percentile bootstrap confidence interval.
     *
     * This class implements the classical nonparametric percentile bootstrap:
     *
     *  - Given a statistic θ = sampler(x) on an input sample x of size n,
     *  - Draw B bootstrap samples y_b of length n via the injected Resampler
     *    (i.e., m = n),
     *  - Compute θ*_b = sampler(y_b),
     *  - Form the (1 - α) percentile CI by taking type-7 empirical quantiles of
     *    {θ*_b} at α/2 and 1 - α/2.
     *
     * Degenerate / non-finite replicates are skipped; if fewer than B/2 usable
     * replicates remain, an exception is thrown to avoid misleading intervals.
     *
     * Thread safety: This class is thread-safe for concurrent calls to run().
     * All mutable state is protected by internal mutexes.
     *
     * @tparam Decimal
     *   Numeric value type (e.g., dec::decimal<8>).
     * @tparam Sampler
     *   Callable with signature `Decimal(const std::vector<SampleType>&)` that computes
     *   the statistic of interest on a series.
     *   At bar level (SampleType = Decimal) this is `Decimal(const std::vector<Decimal>&)`.
     *   At trade level (SampleType = Trade<Decimal>) this is
     *   `Decimal(const std::vector<Trade<Decimal>>&)`.
     * @tparam Resampler
     *   Type that provides
     *     `void operator()(const std::vector<SampleType>& x,
     *                       std::vector<SampleType>&       y,
     *                       std::size_t                    m,
     *                       Rng&                           rng) const;`
     *   and `std::size_t getL() const;`. For i.i.d. bootstrap, this typically draws
     *   with replacement from x into y with m = n.
     * @tparam Rng
     *   Random-number generator type. Defaults to `std::mt19937_64`.
     * @tparam Executor
     *   Parallel executor type used by `concurrency::parallel_for_chunked`.
     * @tparam SampleType
     *   Element type of the input vector passed to run(). Defaults to `Decimal`
     *   for bar-level bootstrapping. Set to `Trade<Decimal>` for trade-level
     *   bootstrapping, where Sampler accepts `const std::vector<Trade<Decimal>>&`
     *   and Resampler operates on `std::vector<Trade<Decimal>>`.
     */
    template<
      class Decimal,
      class Sampler,
      class Resampler,
      class Rng      = std::mt19937_64,
      class Executor = concurrency::SingleThreadExecutor,
      class SampleType = Decimal
      >
    class PercentileBootstrap
    {
    public:
      struct Result
      {
        Decimal     mean;         // θ̂ on original sample
        Decimal     lower;        // percentile lower bound
        Decimal     upper;        // percentile upper bound
        double      cl;           // confidence level
        std::size_t B;            // requested bootstrap reps
        std::size_t effective_B;  // usable (finite) reps
        std::size_t skipped;      // degenerate reps skipped
        std::size_t n;            // original sample size
        std::size_t L;            // resampler L (diagnostic)
      };

    public:
      /**
       * @brief Construct an n-out-of-n percentile bootstrap engine.
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
      PercentileBootstrap(std::size_t   B,
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
          throw std::invalid_argument("PercentileBootstrap: B should be >= 400");
        }
        if (!(m_CL > 0.5 && m_CL < 1.0)) {
          throw std::invalid_argument("PercentileBootstrap: CL must be in (0.5,1)");
        }
      }

      // Copy constructor
      PercentileBootstrap(const PercentileBootstrap& other)
        : m_B(other.m_B),
          m_CL(other.m_CL),
          m_resampler(other.m_resampler),
          m_interval_type(other.m_interval_type),
          m_exec(std::make_shared<Executor>()),
          m_chunkHint(0),
          m_chunkHintMutex(),
          m_diagBootstrapStats(),
          m_diagMeanBoot(0.0),
          m_diagVarBoot(0.0),
          m_diagSeBoot(0.0),
          m_diagValid(false),
          m_diagMutex(),
          m_rngMutex()
      {
      }

      // Move constructor
      PercentileBootstrap(PercentileBootstrap&& other) noexcept
        : m_B(other.m_B),
          m_CL(other.m_CL),
          m_resampler(std::move(other.m_resampler)),
          m_interval_type(other.m_interval_type),
          m_exec(std::move(other.m_exec)),
          m_chunkHint(other.m_chunkHint),
          m_chunkHintMutex(),
          m_diagBootstrapStats(std::move(other.m_diagBootstrapStats)),
          m_diagMeanBoot(other.m_diagMeanBoot),
          m_diagVarBoot(other.m_diagVarBoot),
          m_diagSeBoot(other.m_diagSeBoot),
          m_diagValid(other.m_diagValid),
          m_diagMutex(),
          m_rngMutex()
      {
        // Reset moved-from object
        other.m_diagValid = false;
      }

      // Copy assignment operator
      PercentileBootstrap& operator=(const PercentileBootstrap& other)
      {
        if (this != &other) {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_resampler = other.m_resampler;
          m_interval_type = other.m_interval_type;
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
      PercentileBootstrap& operator=(PercentileBootstrap&& other) noexcept
      {
        if (this != &other) {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_resampler = std::move(other.m_resampler);
          m_interval_type = other.m_interval_type;
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
       * @brief Run the percentile bootstrap using a caller-supplied RNG.
       *
       * For each replicate b = 0..B-1:
       *   1. Construct a per-replicate engine from @p rng,
       *   2. Draw y_b of length n using the resampler (m = n),
       *   3. Compute θ*_b = sampler(y_b),
       *   4. Skip non-finite θ*_b.
       *
       * Thread safety: The supplied RNG is protected by an internal mutex.
       * Multiple threads can safely call run() concurrently on the same instance.
       *
       * @param x
       *   Original sample (size n >= 3).
       * @param sampler
       *   Statistic functor.
       * @param rng
       *   Random number generator.
       *
       * @return Result with mean, CI bounds, and diagnostics.
       *
       * After this call, diagnostic getters (getBootstrapStatistics,
       * getBootstrapMean/Variance/Se) refer to this run's results.
       */
      Result run(const std::vector<SampleType>& x,
                 Sampler                         sampler,
                 Rng&                            rng) const
      {
        // Derive per-replicate engines from the supplied RNG.
        // Protect RNG access with mutex to prevent data races.
        auto make_engine = [&rng, this](std::size_t /*b*/) {
          uint64_t seed;
          {
            std::lock_guard<std::mutex> lock(m_rngMutex);
            seed = mkc_timeseries::rng_utils::get_random_value(rng);
          }
          auto seq = mkc_timeseries::rng_utils::make_seed_seq(seed);
          return mkc_timeseries::rng_utils::construct_seeded_engine<Rng>(seq);
        };

        return run_core_(x, sampler, make_engine);
      }

      /**
       * @brief Run the percentile bootstrap using an engine provider (CRN-friendly).
       *
       * The provider is expected to offer `Rng make_engine(std::size_t b) const`,
       * returning a deterministic engine for each replicate index b. This allows
       * common random numbers across multiple strategies.
       *
       * Thread safety: Multiple threads can safely call run() concurrently on the
       * same instance as long as the provider is thread-safe.
       *
       * @tparam Provider
       *   Type with `Rng make_engine(std::size_t) const`.
       *
       * After this call, diagnostic getters (getBootstrapStatistics,
       * getBootstrapMean/Variance/Se) refer to this run's results.
       */
      template<class Provider>
      Result run(const std::vector<SampleType>& x,
                 Sampler                         sampler,
                 const Provider&                 provider) const
      {
        auto make_engine = [&provider](std::size_t b) {
          return provider.make_engine(b);  // CRN: 1 engine per replicate index
        };

        return run_core_(x, sampler, make_engine);
      }

      /// Hint for chunk size in parallel_for_chunked.
      /// Thread-safe: can be called concurrently with other methods.
      void setChunkSizeHint(uint32_t c) const
      {
        std::lock_guard<std::mutex> lock(m_chunkHintMutex);
        m_chunkHint = c;
      }

      // Introspection helpers
      std::size_t      B()         const { return m_B; }
      double           CL()        const { return m_CL; }
      const Resampler& resampler() const { return m_resampler; }

      // ------------------------------------------------------------------
      // Diagnostics for AutoBootstrapSelector
      // ------------------------------------------------------------------

      /**
       * @brief Diagnostic data structure for atomic retrieval.
       */
      struct DiagnosticData
      {
        std::vector<double> bootstrapStats;
        double meanBoot;
        double varBoot;
        double seBoot;
        bool valid;
      };

      /**
       * @brief Returns true if this instance has diagnostics from a previous run().
       *
       * Thread-safe: can be called concurrently with other methods.
       */
      bool hasDiagnostics() const noexcept
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        return m_diagValid;
      }

      /**
       * @brief Atomically retrieve all diagnostic data in a single lock.
       *
       * This method ensures that all diagnostic values are from the same
       * bootstrap run, preventing inconsistencies that could occur when
       * calling individual getter methods separately under concurrent access.
       *
       * Thread-safe: can be called concurrently with other methods.
       *
       * @throws std::logic_error if run(...) has not been called yet.
       */
      DiagnosticData getAllDiagnostics() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        ensureDiagnosticsAvailable();
        
        return DiagnosticData{
          m_diagBootstrapStats,  // Copy the vector
          m_diagMeanBoot,
          m_diagVarBoot,
          m_diagSeBoot,
          m_diagValid
        };
      }

      /**
       * @brief Returns the usable bootstrap statistics {θ*_b} from the last run.
       *
       * Values are stored after removal of non-finite replicates. The size of
       * this vector equals the effective_B of the last Result.
       *
       * Thread-safe: can be called concurrently with other methods.
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
       * Thread-safe: can be called concurrently with other methods.
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
       * Thread-safe: can be called concurrently with other methods.
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
       * Thread-safe: can be called concurrently with other methods.
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
        // Caller must hold m_diagMutex
        if (!m_diagValid) {
          throw std::logic_error(
            "PercentileBootstrap diagnostics are not available: run() has not been called on this instance.");
        }
      }

      // Hyndman–Fan type-7 quantile using two nth_element passes (unsorted input).
      static double quantile_type7_via_nth(const std::vector<double>& s, double p)
      {
        if (s.empty())
          throw std::invalid_argument("quantile_type7_via_nth: empty input");
        if (p <= 0.0)
          return *std::min_element(s.begin(), s.end());
        if (p >= 1.0)
          return *std::max_element(s.begin(), s.end());

        const double nd = static_cast<double>(s.size());
        const double h  = (nd - 1.0) * p + 1.0;
        std::size_t  i1 = static_cast<std::size_t>(std::floor(h));
        if (i1 < 1)         i1 = 1;
        if (i1 >= s.size()) i1 = s.size() - 1;
        const double frac = h - static_cast<double>(i1);

        std::vector<double> w0(s.begin(), s.end());
        std::nth_element(w0.begin(),
                         w0.begin() + static_cast<std::ptrdiff_t>(i1 - 1),
                         w0.end());
        const double x0 = w0[i1 - 1];

        std::vector<double> w1(s.begin(), s.end());
        std::nth_element(w1.begin(),
                         w1.begin() + static_cast<std::ptrdiff_t>(i1),
                         w1.end());
        const double x1 = w1[i1];

        return x0 + (x1 - x0) * frac;
      }

      template<class EngineMaker>
      Result run_core_(const std::vector<SampleType>& x,
                       Sampler                         sampler,
                       EngineMaker&&                   make_engine) const
      {
        const std::size_t n = x.size();
        if (n < 3) {
          // Update diagnostics validity under lock
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            m_diagValid = false;
          }
          throw std::invalid_argument("PercentileBootstrap: n must be >= 3");
        }

        const Decimal theta_hat = sampler(x);

        // Pre-allocate; NaN marks skipped/invalid replicates
        std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

        // Get chunk hint under lock
        uint32_t chunkHint;
        {
          std::lock_guard<std::mutex> lock(m_chunkHintMutex);
          chunkHint = m_chunkHint;
        }

        // Parallel over B using the internally constructed Executor
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
          // Update diagnostics validity under lock
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            m_diagValid = false;
          }
          throw std::runtime_error("PercentileBootstrap: too many degenerate replicates");
        }

        // Diagnostics: mean, variance, se over usable replicates
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

        // Percentile CI (type-7) at CL
        // Calculate quantiles based on interval type
        const double alpha = 1.0 - m_CL;
        double pl = 0.0;
	double pu = 1.0;
        
        switch (m_interval_type) {
          case IntervalType::TWO_SIDED:
            pl = alpha / 2.0;
            pu = 1.0 - alpha / 2.0;
            break;
            
          case IntervalType::ONE_SIDED_LOWER:
            pl = alpha;
            pu = 1.0 - 1e-10;  // Effectively unbounded upper
            break;
            
          case IntervalType::ONE_SIDED_UPPER:
            pl = 1e-10;        // Effectively unbounded lower
            pu = 1.0 - alpha;
            break;
        }

	const double lb_d = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(thetas_d, pl);
        const double ub_d = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(thetas_d, pu);

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
          /*L           =*/ m_resampler.getL()
        };
      }

    private:
      std::size_t                       m_B;
      double                            m_CL;
      Resampler                         m_resampler;
      IntervalType                      m_interval_type;
      mutable std::shared_ptr<Executor> m_exec;
      mutable uint32_t                  m_chunkHint;
      mutable std::mutex                m_chunkHintMutex;

      // Diagnostics from most recent run(...)
      mutable std::vector<double> m_diagBootstrapStats;
      mutable double              m_diagMeanBoot;
      mutable double              m_diagVarBoot;
      mutable double              m_diagSeBoot;
      mutable bool                m_diagValid;
      mutable std::mutex          m_diagMutex;

      // Mutex to protect RNG access in run() with caller-supplied RNG
      mutable std::mutex          m_rngMutex;
    };

  } // namespace analysis
} // namespace palvalidator
