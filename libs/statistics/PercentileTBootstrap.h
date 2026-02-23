// PercentileTBootstrap.h
// Studentized percentile-t bootstrap with composable resampler and two run paths:
//  (1) caller-supplied RNG reference
//  (2) CRN/engine-provider supplying a deterministic engine per outer replicate
//
// Thread-safety hardening:
// - The caller RNG is never touched inside the parallel region (seeds precomputed).
// - The sampler is copied into each parallel task (safe even if sampler has state).
// - The resampler is also copied into each parallel task (safe even if resampler has state).
// - "Last run diagnostics" are stored behind a mutex, and getters return COPIES
//   (so a concurrent run() can't invalidate references).
//
// IMPORTANT behavioral note if the same PercentileTBootstrap instance is used concurrently:
// - It is safe (no data races).
// - "Last run diagnostics" are inherently racy in meaning: whichever run finishes last
//   becomes "the last run". This is expected; the goal here is memory safety.
//
// GENERALIZATION NOTE (trade-level bootstrap):
//   Both PercentileTBootstrap and BCaCompatibleTBootstrap now accept a SampleType
//   template parameter (default: Decimal) that controls the element type of the
//   input data vector and the internal resample buffers.
//
//   When SampleType = Decimal (the default):
//     Behaviour is 100% identical to the original. All existing instantiations
//     with fewer explicit template parameters compile unchanged.
//
//   When SampleType = Trade<Decimal>:
//     run() accepts std::vector<Trade<Decimal>> as x.
//     The resampler (e.g. IIDResampler<Trade<Decimal>>) produces
//     std::vector<Trade<Decimal>> for y_outer and y_inner.
//     The sampler (statistic) maps std::vector<Trade<Decimal>> -> Decimal.
//     All pivot arithmetic (theta_star_d, se_star, t_b) continues to operate
//     on double/Decimal throughout, completely unaffected by SampleType.
//
//   Trade<Decimal> has a default constructor (required for y_outer/y_inner
//   value-initialization via std::vector<SampleType>(m)) and copy semantics
//   (required by the resampler), so the generalization requires no changes to
//   the algorithmic core -- only to the types of x, y_outer, and y_inner.
//
//   NOTE on MIN_INNER with small trade populations:
//     With n ~ 9 trades (27 bars / 3-bar median holding period), m_outer ~ 9
//     and m_inner ~ 9. The MIN_INNER = 100 gate is still met because IID
//     resampling of 9 trades 100+ times is trivially fast. However, SE*
//     estimated from 100 inner resamples of 9 trades has higher uncertainty
//     than the bar-level equivalent. Document this at the call site.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "number.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "RngUtils.h"
#include "StatUtils.h"
#include "BootstrapTypes.h"

namespace palvalidator
{
  namespace analysis
  {
    using palvalidator::analysis::IntervalType;

    // PercentileT Bootstrap Constants
    // These constants are available to client code without requiring template instantiation
    namespace percentile_t_constants
    {
      /// Minimum inner bootstrap replications required for stable standard error estimation
      constexpr std::size_t MIN_INNER   = 100;

      /// Check stabilization every N inner replications during adaptive stopping
      constexpr std::size_t CHECK_EVERY = 16;

      /// Relative epsilon for SE* stabilization (1.5%) - stop when change is less than this
      constexpr double      REL_EPS     = 0.015;
    }

    /**
     * @class PercentileTBootstrap
     * @brief Studentized (percentile-t) bootstrap confidence intervals.
     *
     * @tparam Decimal     Numeric type for statistics and bounds (e.g., dec::decimal<8>).
     * @tparam Sampler     Statistic callable: maps std::vector<SampleType> -> Decimal.
     * @tparam Resampler   Resampling policy: fills std::vector<SampleType> from another.
     * @tparam Rng         Random engine type (default: std::mt19937_64).
     * @tparam Executor    Parallel execution policy (default: SingleThreadExecutor).
     * @tparam SampleType  Element type of the input data vector (default: Decimal).
     *                     Set to Trade<Decimal> for trade-level bootstrapping.
     *                     All existing code with 3-5 explicit parameters is unaffected.
     */
    template<
      class Decimal,
      class Sampler,
      class Resampler,
      class Rng        = std::mt19937_64,
      class Executor   = concurrency::SingleThreadExecutor,
      class SampleType = Decimal>
    class PercentileTBootstrap
    {
    public:
      struct Result
      {
        Decimal     mean;                  // theta-hat on original sample
        Decimal     lower;                 // lower CI (per-period)
        Decimal     upper;                 // upper CI (per-period)
        double      cl;                    // confidence level
        std::size_t B_outer;               // requested outer reps
        std::size_t B_inner;               // requested inner reps
        std::size_t effective_B;           // usable outer reps (finite pivots)
        std::size_t skipped_outer;         // outer reps skipped (degenerate theta* / SE*)
        std::size_t skipped_inner_total;   // total degenerate inner reps
        std::size_t inner_attempted_total; // total inner attempts across all outer reps (diagnostic)
        std::size_t n;                     // original sample size (in SampleType units)
        std::size_t m_outer;               // outer subsample size
        std::size_t m_inner;               // inner subsample size
        std::size_t L;                     // resampler L (diagnostic)
        double      se_hat;                // sd(theta*) over effective outer reps
      };

      // For backward compatibility, expose constants as class static members
      static constexpr std::size_t MIN_INNER   = percentile_t_constants::MIN_INNER;
      static constexpr std::size_t CHECK_EVERY = percentile_t_constants::CHECK_EVERY;
      static constexpr double      REL_EPS     = percentile_t_constants::REL_EPS;

    public:
      PercentileTBootstrap(std::size_t      B_outer,
                           std::size_t      B_inner,
                           double           confidence_level,
                           const Resampler& resampler,
                           double           m_ratio_outer = 1.0,
                           double           m_ratio_inner = 1.0,
                           IntervalType     interval_type = IntervalType::TWO_SIDED)
        : m_B_outer(B_outer)
        , m_B_inner(B_inner)
        , m_CL(confidence_level)
        , m_resampler(resampler)
        , m_ratio_outer(m_ratio_outer)
        , m_ratio_inner(m_ratio_inner)
        , m_diagTValues()
        , m_diagThetaStars()
        , m_diagSeHat(0.0)
        , m_diagValid(false)
        , m_interval_type(interval_type)
      {
        if (m_B_outer < 400)
          throw std::invalid_argument("PercentileTBootstrap: B_outer must be >= 400");
        if (m_B_inner < 100)
          throw std::invalid_argument("PercentileTBootstrap: B_inner must be >= 100");
        if (!(m_CL > 0.5 && m_CL < 1.0))
          throw std::invalid_argument("PercentileTBootstrap: CL must be in (0.5,1)");
        if (!(m_ratio_outer > 0.0 && m_ratio_outer <= 1.0))
          throw std::invalid_argument("m_ratio_outer must be in (0,1]");
        if (!(m_ratio_inner > 0.0 && m_ratio_inner <= 1.0))
          throw std::invalid_argument("m_ratio_inner must be in (0,1]");
      }

      // Copy constructor
      PercentileTBootstrap(const PercentileTBootstrap& other)
        : m_B_outer(other.m_B_outer)
        , m_B_inner(other.m_B_inner)
        , m_CL(other.m_CL)
        , m_resampler(other.m_resampler)
        , m_ratio_outer(other.m_ratio_outer)
        , m_ratio_inner(other.m_ratio_inner)
        , m_diagMutex()
        , m_diagTValues()
        , m_diagThetaStars()
        , m_diagSeHat(0.0)
        , m_diagValid(false)
        , m_interval_type(other.m_interval_type)
      {}

      // Copy assignment operator
      PercentileTBootstrap& operator=(const PercentileTBootstrap& other)
      {
        if (this != &other) {
          const_cast<std::size_t&>(m_B_outer)      = other.m_B_outer;
          const_cast<std::size_t&>(m_B_inner)      = other.m_B_inner;
          const_cast<double&>(m_CL)                = other.m_CL;
          const_cast<Resampler&>(m_resampler)       = other.m_resampler;
          const_cast<double&>(m_ratio_outer)        = other.m_ratio_outer;
          const_cast<double&>(m_ratio_inner)        = other.m_ratio_inner;
          m_interval_type                           = other.m_interval_type;

          std::lock_guard<std::mutex> lock(m_diagMutex);
          m_diagTValues.clear();
          m_diagThetaStars.clear();
          m_diagSeHat = 0.0;
          m_diagValid = false;
        }
        return *this;
      }

      // Move constructor
      PercentileTBootstrap(PercentileTBootstrap&& other) noexcept
        : m_B_outer(other.m_B_outer)
        , m_B_inner(other.m_B_inner)
        , m_CL(other.m_CL)
        , m_resampler(std::move(other.m_resampler))
        , m_ratio_outer(other.m_ratio_outer)
        , m_ratio_inner(other.m_ratio_inner)
        , m_diagTValues(std::move(other.m_diagTValues))
        , m_diagThetaStars(std::move(other.m_diagThetaStars))
        , m_diagSeHat(other.m_diagSeHat)
        , m_diagValid(other.m_diagValid)
        , m_interval_type(other.m_interval_type)
      {
        // m_diagMutex is default-constructed in the new object.
        // The moved-from object should not be used after move.
      }

      // Move assignment operator
      PercentileTBootstrap& operator=(PercentileTBootstrap&& other) noexcept
      {
        if (this != &other)
        {
          std::lock_guard<std::mutex> lock_this(m_diagMutex);
          std::lock_guard<std::mutex> lock_other(other.m_diagMutex);

          const_cast<std::size_t&>(m_B_outer)      = other.m_B_outer;
          const_cast<std::size_t&>(m_B_inner)      = other.m_B_inner;
          const_cast<double&>(m_CL)                = other.m_CL;
          const_cast<Resampler&>(m_resampler)       = std::move(other.m_resampler);
          const_cast<double&>(m_ratio_outer)        = other.m_ratio_outer;
          const_cast<double&>(m_ratio_inner)        = other.m_ratio_inner;
          m_interval_type                           = other.m_interval_type;

          m_diagTValues    = std::move(other.m_diagTValues);
          m_diagThetaStars = std::move(other.m_diagThetaStars);
          m_diagSeHat      = other.m_diagSeHat;
          m_diagValid      = other.m_diagValid;
        }
        return *this;
      }

      // -----------------------------------------------------------------------
      // (A) Run with caller-provided RNG (non-CRN path).
      //
      // @param x  Input sample of SampleType elements.
      //           When SampleType = Decimal:        std::vector<Decimal>
      //           When SampleType = Trade<Decimal>: std::vector<Trade<Decimal>>
      // -----------------------------------------------------------------------
      Result run(const std::vector<SampleType>& x,
                 Sampler                         sampler,
                 Rng&                            rng,
                 std::size_t                     m_outer_override = 0,
                 std::size_t                     m_inner_override = 0) const
      {
        // IMPORTANT: run_impl parallelizes the outer loop, so we must not touch
        // the caller-provided RNG from inside the parallel region. Precompute
        // per-outer-replicate seeds deterministically in the calling thread.
        std::vector<std::array<uint32_t, 4>> per_outer_seed_words(m_B_outer);
        for (std::size_t b = 0; b < m_B_outer; ++b) {
          const uint64_t s1 = mkc_timeseries::rng_utils::get_random_value(rng);
          const uint64_t s2 = mkc_timeseries::rng_utils::get_random_value(rng);
          per_outer_seed_words[b] = {
            static_cast<uint32_t>(s1),
            static_cast<uint32_t>(s1 >> 32),
            static_cast<uint32_t>(s2),
            static_cast<uint32_t>(s2 >> 32)
          };
        }

        auto engine_maker = [per_outer_seed_words = std::move(per_outer_seed_words)](std::size_t b) -> Rng {
          const auto& w = per_outer_seed_words[b];
          std::seed_seq seq{w[0], w[1], w[2], w[3]};
          return Rng(seq);
        };

        return run_impl(x, std::move(sampler), m_outer_override, m_inner_override,
                        std::move(engine_maker));
      }

      // -----------------------------------------------------------------------
      // (B) Run with a CRN/engine-provider (order/thread independent).
      //     Provider concept: Rng make_engine(std::size_t b) const;
      //
      //     Provider thread-safety is the provider's responsibility. We only
      //     call a const method; if Provider has internal mutable state, it
      //     must synchronize internally.
      // -----------------------------------------------------------------------
      template<class Provider>
      Result run(const std::vector<SampleType>& x,
                 Sampler                         sampler,
                 const Provider&                 provider,
                 std::size_t                     m_outer_override = 0,
                 std::size_t                     m_inner_override = 0) const
      {
        auto engine_maker = [&provider](std::size_t b) -> Rng {
          return provider.make_engine(b);
        };

        return run_impl(x, std::move(sampler), m_outer_override, m_inner_override,
                        std::move(engine_maker));
      }

      // ------------------------------------------------------------------
      // Diagnostics for AutoBootstrapSelector (thread-safe)
      // ------------------------------------------------------------------

      bool hasDiagnostics() const noexcept
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        return m_diagValid;
      }

      // Return COPIES to avoid returning references that could be invalidated
      // by a concurrent run().
      std::vector<double> getTStatistics() const
      {
        ensureDiagnosticsAvailable();
        std::lock_guard<std::mutex> lock(m_diagMutex);
        return m_diagTValues;
      }

      std::vector<double> getThetaStarStatistics() const
      {
        ensureDiagnosticsAvailable();
        std::lock_guard<std::mutex> lock(m_diagMutex);
        return m_diagThetaStars;
      }

      double getSeHat() const
      {
        ensureDiagnosticsAvailable();
        std::lock_guard<std::mutex> lock(m_diagMutex);
        return m_diagSeHat;
      }

    private:
      void ensureDiagnosticsAvailable() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        if (!m_diagValid) {
          throw std::logic_error(
            "PercentileTBootstrap diagnostics are not available: "
            "run() has not been called successfully on this instance.");
        }
      }

      void clearDiagnostics_unsafe() const noexcept
      {
        // Caller must hold m_diagMutex.
        m_diagTValues.clear();
        m_diagThetaStars.clear();
        m_diagSeHat = 0.0;
        m_diagValid = false;
      }

      // -----------------------------------------------------------------------
      // run_impl: core bootstrap algorithm.
      //
      // The only lines that changed from the original are the declarations of
      // x (const std::vector<SampleType>&), y_outer, and y_inner
      // (std::vector<SampleType>). All pivot arithmetic operates on
      // double/Decimal and is completely unaffected by SampleType.
      //
      // Trade<Decimal> satisfies both requirements imposed on SampleType here:
      //   (1) Default-constructible: std::vector<SampleType>(m) value-initializes
      //       m Trade objects using Trade::Trade() = default.
      //   (2) Copyable: the resampler copies Trade objects by index, which uses
      //       Trade's implicitly-generated copy constructor.
      // -----------------------------------------------------------------------
      template<class EngineMaker>
      Result run_impl(const std::vector<SampleType>& x,
                      Sampler                         sampler,
                      std::size_t                     m_outer_override,
                      std::size_t                     m_inner_override,
                      EngineMaker                     make_engine) const
      {
        const std::size_t n = x.size();
        if (n < 3) {
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            clearDiagnostics_unsafe();
          }
          throw std::invalid_argument("PercentileTBootstrap.run: n must be >= 3");
        }

        // Decide m_outer and m_inner.
        // When SampleType = Trade<Decimal>, n is the trade count (~9), not the
        // bar count (~27). m_outer and m_inner are therefore in trade units.
        std::size_t m_outer = (m_outer_override > 0)
          ? m_outer_override
          : static_cast<std::size_t>(std::floor(m_ratio_outer * static_cast<double>(n)));
        if (m_outer < 2) m_outer = 2;
        if (m_outer > n) m_outer = n;

        std::size_t m_inner = (m_inner_override > 0)
          ? m_inner_override
          : static_cast<std::size_t>(std::floor(m_ratio_inner * static_cast<double>(m_outer)));
        if (m_inner < 2)       m_inner = 2;
        if (m_inner > m_outer) m_inner = m_outer;

        // Baseline statistic on original sample.
        // sampler maps std::vector<SampleType> -> Decimal in both paths.
        const Decimal theta_hat   = sampler(x);
        const double  theta_hat_d = num::to_double(theta_hat);

        // Output buffers (Decimal statistics and pivot values are always scalar)
        std::vector<double> theta_star_ds(m_B_outer, std::numeric_limits<double>::quiet_NaN());
        std::vector<double> tvals         (m_B_outer, std::numeric_limits<double>::quiet_NaN());

        // Diagnostics counters
        std::atomic<std::size_t> skipped_outer{0};
        std::atomic<std::size_t> skipped_inner_total{0};
        std::atomic<std::size_t> inner_attempted_total{0};

        const std::size_t Ldiag = m_resampler.getL();

        // Copy resampler per task to avoid shared mutation of any internal state.
        const Resampler resampler_base_copy = m_resampler;

        // Parallelize outer loop only.
        Executor exec{};
        concurrency::parallel_for_chunked(
          static_cast<uint32_t>(m_B_outer),
          exec,
          [&, sampler, make_engine, resampler_base_copy](uint32_t b32)
          {
            Resampler resampler_local = resampler_base_copy;

            const std::size_t b = static_cast<std::size_t>(b32);
            Rng rng_b = make_engine(b);

            // y_outer and y_inner are std::vector<SampleType>.
            // When SampleType = Decimal:        vectors of Decimal (original behaviour).
            // When SampleType = Trade<Decimal>: vectors of Trade objects.
            // Value-initialization is safe because Trade has a default constructor.
            std::vector<SampleType> y_outer(m_outer);
            std::vector<SampleType> y_inner(m_inner);

            // OUTER resample: fill y_outer from x
            resampler_local(x, y_outer, m_outer, rng_b);

            // theta* on OUTER resample
            const Decimal theta_star   = sampler(y_outer);
            const double  theta_star_d = num::to_double(theta_star);
            if (!std::isfinite(theta_star_d)) {
              skipped_outer.fetch_add(1, std::memory_order_relaxed);
              return;
            }

            // Inner loop: estimate SE* via Welford online variance.
            // Adaptive stopping: halt once SE* stabilizes to within REL_EPS.
            double mean = 0.0, m2 = 0.0;
            std::size_t eff_inner = 0;

            auto push_inner = [&](double v) noexcept {
              ++eff_inner;
              const double delta = v - mean;
              mean += delta / static_cast<double>(eff_inner);
              m2   += delta * (v - mean);
            };

            double last_se = std::numeric_limits<double>::infinity();

            for (std::size_t j = 0; j < m_B_inner; ++j) {
              inner_attempted_total.fetch_add(1, std::memory_order_relaxed);

              // INNER resample: fill y_inner from y_outer
              resampler_local(y_outer, y_inner, m_inner, rng_b);

              const double v = num::to_double(sampler(y_inner));
              if (!std::isfinite(v)) {
                skipped_inner_total.fetch_add(1, std::memory_order_relaxed);
                continue;
              }
              push_inner(v);

              if (eff_inner >= MIN_INNER && ((eff_inner % CHECK_EVERY) == 0)) {
                const double se_now = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner)));
                if (std::isfinite(se_now) &&
                    std::fabs(se_now - last_se) <= REL_EPS * std::max(se_now, 1e-300)) {
                  break;
                }
                last_se = se_now;
              }
            }

            if (eff_inner < MIN_INNER) {
              skipped_outer.fetch_add(1, std::memory_order_relaxed);
              return;
            }

            const double se_star = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner)));
            if (!(se_star > 0.0) || !std::isfinite(se_star)) {
              skipped_outer.fetch_add(1, std::memory_order_relaxed);
              return;
            }

            const double t_b = (theta_star_d - theta_hat_d) / se_star;

            theta_star_ds[b] = theta_star_d;
            tvals[b]         = t_b;
          });

        // Collect effective outer replicates (finite pivot values only)
        std::vector<double> t_eff;     t_eff.reserve(m_B_outer);
        std::vector<double> theta_eff; theta_eff.reserve(m_B_outer);

        for (std::size_t b = 0; b < m_B_outer; ++b) {
          const double tb = tvals[b];
          const double th = theta_star_ds[b];
          if (std::isfinite(tb) && std::isfinite(th)) {
            t_eff.push_back(tb);
            theta_eff.push_back(th);
          }
        }

        const std::size_t effective_B = t_eff.size();

        // Require at least 4% of requested outer replicates, with a floor of 16.
        const std::size_t min_effective = std::max<std::size_t>(16u, m_B_outer / 25u);
        if (effective_B < min_effective) {
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            clearDiagnostics_unsafe();
          }
          throw std::runtime_error(
            "PercentileTBootstrap: insufficient valid outer replicates. "
            "Got " + std::to_string(effective_B) + " valid out of " +
            std::to_string(m_B_outer) + " (minimum required: " +
            std::to_string(min_effective) + ", i.e., 4% or 16, whichever is larger). "
            "The data may be too pathological for Percentile-t bootstrap.");
        }

        const double se_hat = mkc_timeseries::StatUtils<double>::computeStdDev(theta_eff);

        const double alpha = 1.0 - m_CL;

        double lower_quantile, upper_quantile;
        switch (m_interval_type)
          {
          case IntervalType::TWO_SIDED:
          default:
            lower_quantile = alpha / 2.0;
            upper_quantile = 1.0 - alpha / 2.0;
            break;

          case IntervalType::ONE_SIDED_LOWER:
            lower_quantile = 1e-10;
            upper_quantile = 1.0 - alpha;
            break;

          case IntervalType::ONE_SIDED_UPPER:
            lower_quantile = alpha;
            upper_quantile = 1.0 - 1e-10;
            break;
          }

        const double t_lo = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(t_eff, lower_quantile);
        const double t_hi = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(t_eff, upper_quantile);

        const double lower_d = theta_hat_d - t_hi * se_hat;
        const double upper_d = theta_hat_d - t_lo * se_hat;

        // Store diagnostics for the most recent successful run (thread-safe).
        {
          std::lock_guard<std::mutex> lock(m_diagMutex);
          m_diagTValues    = t_eff;
          m_diagThetaStars = theta_eff;
          m_diagSeHat      = se_hat;
          m_diagValid      = true;
        }

        Result R;
        R.mean                  = theta_hat;
        R.lower                 = Decimal(lower_d);
        R.upper                 = Decimal(upper_d);
        R.cl                    = m_CL;
        R.B_outer               = m_B_outer;
        R.B_inner               = m_B_inner;
        R.effective_B           = effective_B;
        R.skipped_outer         = skipped_outer.load(std::memory_order_relaxed);
        R.skipped_inner_total   = skipped_inner_total.load(std::memory_order_relaxed);
        R.inner_attempted_total = inner_attempted_total.load(std::memory_order_relaxed);
        R.n                     = n;
        R.m_outer               = m_outer;
        R.m_inner               = m_inner;
        R.L                     = Ldiag;
        R.se_hat                = se_hat;
        return R;
      }

    private:
      // Immutable configuration after construction
      const std::size_t  m_B_outer;
      const std::size_t  m_B_inner;
      const double       m_CL;
      const Resampler    m_resampler;
      const double       m_ratio_outer;
      const double       m_ratio_inner;

      // Diagnostics for most recent successful run (protected by m_diagMutex).
      // Getters return copies to avoid reference invalidation by concurrent run().
      mutable std::mutex          m_diagMutex;
      mutable std::vector<double> m_diagTValues;
      mutable std::vector<double> m_diagThetaStars;
      mutable double              m_diagSeHat;
      mutable bool                m_diagValid;
      IntervalType                m_interval_type;
    };


    // --------------------------------------------------------------------------
    // BCaCompatibleTBootstrap
    //
    // A BCaBootStrap-compatible wrapper around PercentileTBootstrap.
    // Presents the same constructor signatures and accessor API as BCaBootStrap
    // so the two can be used interchangeably in AutoBootstrapSelector and at
    // any other call site that is templated on the bootstrap type.
    //
    // GENERALIZATION: SampleType (default: Decimal) propagates through to
    // PercentileTBootstrap, m_returns, StatFn, and m_cached_result in exactly
    // the same way as in BCaBootStrap. All existing code with 2-4 explicit
    // template parameters is 100% backward-compatible.
    //
    // Thread-safety: cached_result access is protected by m_cacheMutex.
    // --------------------------------------------------------------------------
    template <class Decimal,
              class Sampler,          // Resampler policy (called "Sampler" to
                                      // match BCaBootStrap naming convention)
              class Rng        = std::mt19937_64,
              class Provider   = void,
              class SampleType = Decimal>
    class BCaCompatibleTBootstrap
    {
    public:
      /**
       * @brief Statistic function type.
       *
       * When SampleType = Decimal (default/bar-level):
       *   StatFn = std::function<Decimal(const std::vector<Decimal>&)>
       *   -- identical to the original; all existing code compiles unchanged.
       *
       * When SampleType = Trade<Decimal> (trade-level):
       *   StatFn = std::function<Decimal(const std::vector<Trade<Decimal>>&)>
       *   -- statistics such as GeoMeanStat provide
       *      operator()(const std::vector<Trade<Decimal>>&) directly.
       */
      using StatFn = std::function<Decimal(const std::vector<SampleType>&)>;

      // -----------------------------------------------------------------------
      // Constructor for Provider = void (default, no CRN provider).
      //
      // This is the primary constructor for bar-level and trade-level use
      // when deterministic per-replicate seeds are not required.
      // -----------------------------------------------------------------------
      template <class P = Provider, std::enable_if_t<std::is_void_v<P>, int> = 0>
      BCaCompatibleTBootstrap(const std::vector<SampleType>& returns,
                              unsigned int                    num_resamples,
                              double                          confidence_level,
                              StatFn                          statistic,
                              Sampler                         sampler)
        : m_internal_pt(num_resamples, m_B_inner_default, confidence_level,
                        std::move(sampler), 1.0, 1.0)
        , m_returns(returns)
        , m_statistic(std::move(statistic))
        , m_cached_result()
      {
        if (m_returns.empty() || num_resamples < 100u ||
            confidence_level <= 0.0 || confidence_level >= 1.0)
          throw std::invalid_argument(
            "BCaCompatibleTBootstrap: Invalid construction arguments.");
      }

      // -----------------------------------------------------------------------
      // Constructor for Provider != void (CRN path).
      //
      // Enabled only when Provider != void. Uses provider.make_engine(b) to
      // supply a deterministic RNG per outer replicate.
      // -----------------------------------------------------------------------
      template <class P = Provider, std::enable_if_t<!std::is_void_v<P>, int> = 0>
      BCaCompatibleTBootstrap(const std::vector<SampleType>& returns,
                              unsigned int                    num_resamples,
                              double                          confidence_level,
                              StatFn                          statistic,
                              Sampler                         sampler,
                              const P&                        provider)
        : m_internal_pt(num_resamples, m_B_inner_default, confidence_level,
                        std::move(sampler), 1.0, 1.0)
        , m_returns(returns)
        , m_statistic(std::move(statistic))
        , m_provider(provider)
        , m_cached_result()
      {
        if (m_returns.empty() || num_resamples < 100u ||
            confidence_level <= 0.0 || confidence_level >= 1.0)
          throw std::invalid_argument(
            "BCaCompatibleTBootstrap: Invalid construction arguments.");
      }

      // BCaBootStrap-compatible accessors
      Decimal getLowerBound()
      {
        ensureCalculated();
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        return m_cached_result.value().lower;
      }

      Decimal getUpperBound()
      {
        ensureCalculated();
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        return m_cached_result.value().upper;
      }

      Decimal getStatistic()
      {
        ensureCalculated();
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        return m_cached_result.value().mean;
      }

      Decimal getMean() { return getStatistic(); }

      /**
       * @brief Returns the sample size in SampleType units.
       *
       * For bar-level bootstrapping (SampleType = Decimal): number of bars.
       * For trade-level bootstrapping (SampleType = Trade<Decimal>): number of trades.
       */
      std::size_t getSampleSize() const { return m_returns.size(); }

    private:
      static constexpr std::size_t m_B_inner_default = 200;

      // Internal PercentileTBootstrap carries SampleType all the way through.
      PercentileTBootstrap<Decimal, StatFn, Sampler, Rng,
                           concurrency::SingleThreadExecutor,
                           SampleType>       m_internal_pt;

      const std::vector<SampleType>&         m_returns;
      StatFn                                 m_statistic;

      // Provider storage: materialized only when Provider != void.
      [[no_unique_address]]
      std::conditional_t<std::is_void_v<Provider>, char, Provider> m_provider{};

      mutable std::mutex m_cacheMutex;

      // m_cached_result stores the PercentileTBootstrap::Result, which always
      // contains Decimal bounds regardless of SampleType.
      std::optional<
        typename PercentileTBootstrap<Decimal, StatFn, Sampler, Rng,
                                      concurrency::SingleThreadExecutor,
                                      SampleType>::Result>
        m_cached_result;

      void ensureCalculated()
      {
        {
          std::lock_guard<std::mutex> lock(m_cacheMutex);
          if (m_cached_result.has_value()) return;
        }

        // Compute outside lock to avoid holding mutex during heavy work.
        typename PercentileTBootstrap<Decimal, StatFn, Sampler, Rng,
                                      concurrency::SingleThreadExecutor,
                                      SampleType>::Result computed;

        if constexpr (std::is_void_v<Provider>) {
          // Default path: use a thread-local RNG (matches BCaBootStrap behaviour)
          thread_local static Rng tl_rng;
          computed = m_internal_pt.run(m_returns, m_statistic, tl_rng);
        } else {
          computed = m_internal_pt.run(m_returns, m_statistic, m_provider);
        }

        {
          std::lock_guard<std::mutex> lock(m_cacheMutex);
          if (!m_cached_result.has_value())
            m_cached_result = std::move(computed);
        }
      }
    };

  } // namespace analysis
} // namespace palvalidator
