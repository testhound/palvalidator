#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include <memory>

#include "randutils.hpp"
#include "RngUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "number.h"

namespace palvalidator
{
  namespace analysis
  {

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
      BasicBootstrap(std::size_t   B,
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
          m_diagValid(false)
      {
        if (m_B < 400) {
          throw std::invalid_argument("BasicBootstrap: B should be >= 400");
        }
        if (!(m_CL > 0.5 && m_CL < 1.0)) {
          throw std::invalid_argument("BasicBootstrap: CL must be in (0.5,1)");
        }
      }

      /**
       * @brief Run the basic-bootstrap CI using a caller-supplied RNG.
       *
       * After this call, diagnostic getters (getBootstrapStatistics,
       * getBootstrapMean/Variance/Se) refer to this run's results.
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
       * After this call, diagnostic getters (getBootstrapStatistics,
       * getBootstrapMean/Variance/Se) refer to this run's results.
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

      /// Hint for chunk size in parallel_for_chunked.
      void setChunkSizeHint(uint32_t c) const
      {
        m_chunkHint = c;
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
      Result run_core_(const std::vector<Decimal>& x,
                       Sampler                      sampler,
                       EngineMaker&&                make_engine) const
      {
        const std::size_t n = x.size();
        if (n < 3) {
          m_diagValid = false;
          throw std::invalid_argument("BasicBootstrap: n must be >= 3");
        }

        const Decimal theta_hat = sampler(x);

        std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

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
          /*chunkSizeHint=*/m_chunkHint
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

        const double alpha = 1.0 - m_CL;
        const double pl    = alpha / 2.0;
        const double pu    = 1.0 - alpha / 2.0;

        const double q_lo = quantile_type7_via_nth(thetas_d, pl);
        const double q_hi = quantile_type7_via_nth(thetas_d, pu);

        const double center = num::to_double(theta_hat);
        const double lb_d   = 2.0 * center - q_hi; // 2θ̂ - q_{1-α/2}
        const double ub_d   = 2.0 * center - q_lo; // 2θ̂ - q_{α/2}

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
      mutable std::shared_ptr<Executor> m_exec;
      mutable uint32_t                  m_chunkHint;

      // Diagnostics from most recent run(...)
      mutable std::vector<double> m_diagBootstrapStats;
      mutable double              m_diagMeanBoot;
      mutable double              m_diagVarBoot;
      mutable double              m_diagSeBoot;
      mutable bool                m_diagValid;
    };

  } // namespace analysis
} // namespace palvalidator
