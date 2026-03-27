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

      // -----------------------------------------------------------------------
      // Reliability thresholds — govern the three flags in Result::isReliable().
      // Values are kept here (self-contained in the bootstrap header) so the
      // engine does not depend on AutoBootstrapConfiguration. The tournament
      // mirrors these values in kPercentileT* constants.
      // -----------------------------------------------------------------------

      /// Minimum fraction of outer replicates that must yield a finite pivot.
      /// Mirrors AutoBootstrapConfiguration::kPercentileTMinEffectiveFraction.
      constexpr double MIN_EFFECTIVE_FRACTION  = 0.70;

      /// Inner SE* failure rate above which high_inner_skip_rate fires.
      /// Mirrors AutoBootstrapConfiguration::kPercentileTInnerFailThreshold.
      constexpr double INNER_FAIL_THRESHOLD    = 0.05;

      /// |skew(t*)| above which extreme_pivot_skewness fires (hard reliability flag).
      /// Mirrors AutoBootstrapConfiguration::kPercentileTSkewHardThreshold.
      constexpr double PIVOT_SKEW_THRESHOLD    = 3.0;

      /// |skew(t*)| above which the soft stability penalty begins to accrue.
      /// Mirrors AutoBootstrapConfiguration::kPercentileTSkewSoftThreshold.
      constexpr double PIVOT_SKEW_SOFT_THRESHOLD = 2.0;
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
        double      skew_pivot;            // skewness of the t* (pivot) distribution

        // ----------------------------------------------------------------
        // Reliability flags — analogous to BCa's AccelerationReliability
        // and M-out-of-N's four-flag decomposition. The three flags capture
        // distinct failure modes of the double-bootstrap machinery:
        //
        // low_effective_replicates:
        //   effective_B / B_outer < MIN_EFFECTIVE_FRACTION (0.70).
        //   The empirical t-distribution is too thinly populated for
        //   reliable quantile inversion. Hard gate in the tournament.
        //
        // high_inner_skip_rate:
        //   skipped_inner / inner_attempted > INNER_FAIL_THRESHOLD (0.05).
        //   Too many inner resamples produced non-finite statistics, so
        //   the SE* denominators feeding the pivots are corrupted. Hard
        //   gate in the tournament (mirrors the existing rejection mask).
        //
        // extreme_pivot_skewness:
        //   |skew(t*)| > PIVOT_SKEW_THRESHOLD (3.0).
        //   The pivot distribution is too skewed for the CI inversion
        //   to be reliable. Soft penalty in the tournament (calibrated
        //   haircut on stability score, not an outright hard gate).
        //
        // isReliable():
        //   Convenience AND-gate over all three flags. Downstream
        //   selectors (AutoBootstrapSelector) use this as algorithmIsReliable.
        //
        // API CONTRACT — proceed threshold vs. reliability threshold:
        //   run() throws only when effective_B falls below the hard-abort
        //   floor (max(16, B_outer/25), i.e. ~4%). A Result is therefore
        //   returned — and isReliable() may return false — for any run
        //   that clears the 4% floor but does not clear the 70%
        //   MIN_EFFECTIVE_FRACTION threshold. This is intentional: the
        //   algorithm returns the best available estimate rather than
        //   discarding it, and labels it unreliable so callers can
        //   decide. Callers must not assume a returned Result is reliable
        //   without checking isReliable().
        // ----------------------------------------------------------------
        bool        low_effective_replicates;  // effective_B fraction too low
        bool        high_inner_skip_rate;      // inner SE* failures too frequent
        bool        extreme_pivot_skewness;    // t* distribution too skewed

        // The interval type used to produce lower/upper.
        //
        // TWO_SIDED:       Both bounds are computed finite values.
        //
        // ONE_SIDED_LOWER [L, +sentinel]:
        //   lower is the finite, meaningful CI bound.
        //   upper is a finite surrogate for +infinity: the value at quantile
        //   (1 - 1e-10) of the empirical pivot distribution, i.e. the most
        //   extreme observed upper tail. It is always a valid Decimal value.
        //
        // ONE_SIDED_UPPER [-sentinel, U]:
        //   lower is a finite surrogate for -infinity: the value at quantile
        //   1e-10 of the empirical pivot distribution.
        //   upper is the finite, meaningful CI bound.
        //
        // Floating-point infinity is NOT used because Decimal may be a
        // fixed-point type (e.g. dec::decimal<8>) that cannot represent it
        // and for which std::numeric_limits<Decimal>::max() may be 0.
        // Callers MUST inspect interval_type to identify which bound is the
        // true constraint and which is the surrogate sentinel.
        IntervalType interval_type;

        bool isReliable() const noexcept
        {
          return !low_effective_replicates
              && !high_inner_skip_rate
              && !extreme_pivot_skewness;
        }
      };

      // For backward compatibility, expose constants as class static members
      static constexpr std::size_t MIN_INNER              = percentile_t_constants::MIN_INNER;
      static constexpr std::size_t CHECK_EVERY            = percentile_t_constants::CHECK_EVERY;
      static constexpr double      REL_EPS                = percentile_t_constants::REL_EPS;
      static constexpr double      MIN_EFFECTIVE_FRACTION = percentile_t_constants::MIN_EFFECTIVE_FRACTION;
      static constexpr double      INNER_FAIL_THRESHOLD   = percentile_t_constants::INNER_FAIL_THRESHOLD;
      static constexpr double      PIVOT_SKEW_THRESHOLD   = percentile_t_constants::PIVOT_SKEW_THRESHOLD;
      static constexpr double      PIVOT_SKEW_SOFT_THRESHOLD = percentile_t_constants::PIVOT_SKEW_SOFT_THRESHOLD;

    public:
      /**
       * @param B_outer        Number of outer bootstrap replicates (>= 400).
       * @param B_inner        Number of inner bootstrap replicates per outer
       *                       replicate (>= 100). Used to estimate SE*.
       * @param confidence_level  Nominal coverage in (0.5, 1.0).
       * @param resampler      Resampling policy (copied per parallel task).
       * @param m_ratio_outer  Fraction of n used for each outer resample, in
       *                       (0, 1]. Default 1.0 gives the standard full-sample
       *                       percentile-t bootstrap.
       *                       @warning Values < 1.0 activate the m-out-of-n
       *                       bootstrap variant. Standard percentile-t asymptotic
       *                       theory does not automatically apply: validity
       *                       requires separate theoretical justification (e.g.
       *                       Bickel, Götze & van Zwet 1997). Use sub-unity ratios
       *                       only when deliberately targeting the m-out-of-n
       *                       studentized bootstrap and document the choice.
       * @param m_ratio_inner  Fraction of m_outer used for each inner resample,
       *                       in (0, 1]. Default 1.0 is the standard choice.
       *                       @warning Values < 1.0 compound the m-out-of-n
       *                       concern above and are rarely justified.
       * @param interval_type  TWO_SIDED (default), ONE_SIDED_LOWER, or
       *                       ONE_SIDED_UPPER. For one-sided intervals the
       *                       unused Result bound receives a finite extreme-
       *                       quantile surrogate (quantile 1e-10 or 1-1e-10
       *                       of the empirical pivot distribution) rather
       *                       than floating-point infinity, which is not
       *                       representable in all Decimal types. The
       *                       Result::interval_type field identifies which
       *                       bound is the true constraint and which is the
       *                       surrogate. Callers must not treat a one-sided
       *                       result as a two-sided interval.
       */
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
        , m_diagSkewPivot(0.0)
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
        , m_diagSkewPivot(0.0)
        , m_diagValid(false)
        , m_interval_type(other.m_interval_type)
      {}

      // Copy assignment operator
      PercentileTBootstrap& operator=(const PercentileTBootstrap& other)
      {
        if (this != &other) {
          m_B_outer      = other.m_B_outer;
          m_B_inner      = other.m_B_inner;
          m_CL           = other.m_CL;
          m_resampler    = other.m_resampler;
          m_ratio_outer  = other.m_ratio_outer;
          m_ratio_inner  = other.m_ratio_inner;
          m_interval_type = other.m_interval_type;

          std::lock_guard<std::mutex> lock(m_diagMutex);
          m_diagTValues.clear();
          m_diagThetaStars.clear();
          m_diagSeHat     = 0.0;
          m_diagSkewPivot = 0.0;
          m_diagValid     = false;
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
        , m_diagSkewPivot(other.m_diagSkewPivot)
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
          // Acquire both mutexes simultaneously to prevent lock-ordering
          // deadlock if two threads concurrently move-assign a pair of objects
          // in opposite directions (e.g. a=move(b) vs b=move(a)).
          std::scoped_lock guard(m_diagMutex, other.m_diagMutex);

          m_B_outer      = other.m_B_outer;
          m_B_inner      = other.m_B_inner;
          m_CL           = other.m_CL;
          m_resampler    = std::move(other.m_resampler);
          m_ratio_outer  = other.m_ratio_outer;
          m_ratio_inner  = other.m_ratio_inner;
          m_interval_type = other.m_interval_type;

          m_diagTValues    = std::move(other.m_diagTValues);
          m_diagThetaStars = std::move(other.m_diagThetaStars);
          m_diagSeHat      = other.m_diagSeHat;
          m_diagSkewPivot  = other.m_diagSkewPivot;
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
      // Each getter holds the lock for the entire duration of the validity check
      // and the copy, eliminating the TOCTOU gap that existed when
      // ensureDiagnosticsAvailable() released the lock before the caller
      // re-acquired it.
      std::vector<double> getTStatistics() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        if (!m_diagValid)
          throw std::logic_error(
            "PercentileTBootstrap diagnostics are not available: "
            "run() has not been called successfully on this instance.");
        return m_diagTValues;
      }

      std::vector<double> getThetaStarStatistics() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        if (!m_diagValid)
          throw std::logic_error(
            "PercentileTBootstrap diagnostics are not available: "
            "run() has not been called successfully on this instance.");
        return m_diagThetaStars;
      }

      double getSeHat() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        if (!m_diagValid)
          throw std::logic_error(
            "PercentileTBootstrap diagnostics are not available: "
            "run() has not been called successfully on this instance.");
        return m_diagSeHat;
      }

      /// Returns the skewness of the t* (pivot) distribution from the most
      /// recent successful run. Positive values indicate right-skewed pivots;
      /// negative values indicate left-skewed pivots. Values with |skew| > 3.0
      /// trigger extreme_pivot_skewness in Result::isReliable().
      double getSkewPivot() const
      {
        std::lock_guard<std::mutex> lock(m_diagMutex);
        if (!m_diagValid)
          throw std::logic_error(
            "PercentileTBootstrap diagnostics are not available: "
            "run() has not been called successfully on this instance.");
        return m_diagSkewPivot;
      }

    private:
      void clearDiagnostics_unsafe() const noexcept
      {
        // Caller must hold m_diagMutex.
        m_diagTValues.clear();
        m_diagThetaStars.clear();
        m_diagSeHat     = 0.0;
        m_diagSkewPivot = 0.0;
        m_diagValid     = false;
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
                // Use sample variance (÷ N-1) for an unbiased SE* estimate.
                const double se_now = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner - 1)));
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

            // Use sample variance (÷ N-1, Bessel-corrected) for an unbiased SE*.
            // eff_inner >= MIN_INNER >= 2 here, so eff_inner - 1 >= 1 is safe.
            const double se_star = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner - 1)));
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

        // Guard: if all effective outer replicates produced the same statistic
        // (e.g. constant data), se_hat will be 0, which would collapse both CI
        // bounds to theta_hat. Treat this as a degenerate / un-bootstrappable
        // dataset and surface a clear error rather than silently returning a
        // zero-width interval.
        if (!std::isfinite(se_hat) || se_hat == 0.0) {
          {
            std::lock_guard<std::mutex> lock(m_diagMutex);
            clearDiagnostics_unsafe();
          }
          throw std::runtime_error(
            "PercentileTBootstrap: se_hat is zero or non-finite. "
            "All effective outer replicates produced identical statistics; "
            "the data may be constant or nearly so.");
        }

        const double alpha = 1.0 - m_CL;

        // -----------------------------------------------------------------------
        // Quantile selection and CI inversion.
        //
        // The pivot inversion is:
        //   lower = theta_hat - t_hi * se_hat
        //   upper = theta_hat - t_lo * se_hat
        //
        // For two-sided intervals both quantiles are computed normally.
        //
        // For one-sided intervals the unused bound receives a finite extreme-
        // quantile surrogate:
        //
        // ONE_SIDED_LOWER  [L, surrogate_upper]:
        //   Tight lower bound: t_hi = quantile(t*, 1-alpha)
        //     → lower = theta_hat - t_hi * se_hat  (full alpha on lower side)
        //   Surrogate upper:   t_lo = quantile(t*, 1e-10)
        //     → upper = theta_hat - t_lo * se_hat  (near-zero t_lo → very large upper)
        //   Result.upper is a valid Decimal value, not infinity.
        //   Callers must use Result::interval_type to identify it as a surrogate.
        //
        // ONE_SIDED_UPPER  [surrogate_lower, U]:
        //   Tight upper bound: t_lo = quantile(t*, alpha)
        //     → upper = theta_hat - t_lo * se_hat  (full alpha on upper side)
        //   Surrogate lower:   t_hi = quantile(t*, 1-1e-10)
        //     → lower = theta_hat - t_hi * se_hat  (very large t_hi → very small lower)
        //   Symmetric reasoning to ONE_SIDED_LOWER.
        //
        // Floating-point infinity is deliberately avoided: Decimal may be a
        // fixed-point type (e.g. dec::decimal<8>) that cannot represent it,
        // and std::numeric_limits<Decimal>::max() may be 0 for such types.
        // -----------------------------------------------------------------------
        double lower_quantile, upper_quantile;
        switch (m_interval_type)
          {
          case IntervalType::ONE_SIDED_LOWER:
            // lower = theta_hat - t_hi * se_hat  →  tight lower bound needs
            //   t_hi at the (1-alpha) quantile (full alpha on one side).
            // upper = theta_hat - t_lo * se_hat  →  surrogate +∞ upper needs
            //   t_lo at the 1e-10 quantile (near-zero → very large upper).
            lower_quantile = 1e-10;
            upper_quantile = 1.0 - alpha;
            break;

          case IntervalType::ONE_SIDED_UPPER:
            // upper = theta_hat - t_lo * se_hat  →  tight upper bound needs
            //   t_lo at the alpha quantile (full alpha on one side).
            // lower = theta_hat - t_hi * se_hat  →  surrogate -∞ lower needs
            //   t_hi at the (1-1e-10) quantile (very large → very small lower).
            lower_quantile = alpha;
            upper_quantile = 1.0 - 1e-10;
            break;

          case IntervalType::TWO_SIDED:
          default:
            lower_quantile = alpha / 2.0;
            upper_quantile = 1.0 - alpha / 2.0;
            break;
          }

        const double t_lo = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(
                              t_eff, lower_quantile);
        const double t_hi = mkc_timeseries::StatUtils<double>::quantileType7Unsorted(
                              t_eff, upper_quantile);

        const double lower_d = theta_hat_d - t_hi * se_hat;
        const double upper_d = theta_hat_d - t_lo * se_hat;

        // -------------------------------------------------------------------------
        // Pivot (t*) skewness — computed from t_eff, the same vector used for
        // quantile inversion. This is deliberately distinct from the θ* skewness
        // (computed by the selector from getThetaStarStatistics()), which describes
        // the data distribution. The pivot skewness describes whether the
        // studentisation is working: a symmetric pivot distribution indicates a
        // well-behaved statistic; a heavily skewed one indicates the t-distribution
        // approximation is degrading.
        // -------------------------------------------------------------------------
        double skew_pivot = 0.0;
        if (t_eff.size() >= 3) {
          double sum_t = 0.0;
          for (double v : t_eff) sum_t += v;
          const double mean_t = sum_t / static_cast<double>(t_eff.size());
          const double sd_t   = mkc_timeseries::StatUtils<double>::computeStdDev(t_eff);
          if (sd_t > 0.0)
            skew_pivot = mkc_timeseries::StatUtils<double>::computeSkewness(
                           t_eff, mean_t, sd_t);
        }

        // -------------------------------------------------------------------------
        // Reliability flags — mirror the M-out-of-N four-flag pattern.
        // Flags 1 and 2 are also enforced as hard gates in the tournament rejection
        // mask (PercentileTLowEffB, PercentileTInnerFails); they are duplicated
        // here so callers who only hold a Result can inspect them without
        // recomputing against AutoBootstrapConfiguration thresholds.
        // Flag 3 drives a soft stability penalty in the tournament (not a hard gate).
        // -------------------------------------------------------------------------
        const double eff_fraction =
          static_cast<double>(effective_B) / static_cast<double>(m_B_outer);
        const double inner_attempted_d =
          static_cast<double>(inner_attempted_total.load(std::memory_order_relaxed));
        const double inner_fail_rate =
          (inner_attempted_d > 0.0)
          ? static_cast<double>(skipped_inner_total.load(std::memory_order_relaxed))
            / inner_attempted_d
          : 0.0;

        const bool low_effective_replicates =
          eff_fraction < percentile_t_constants::MIN_EFFECTIVE_FRACTION;
        const bool high_inner_skip_rate =
          inner_fail_rate > percentile_t_constants::INNER_FAIL_THRESHOLD;
        const bool extreme_pivot_skewness =
          std::fabs(skew_pivot) > percentile_t_constants::PIVOT_SKEW_THRESHOLD;

        // Store diagnostics for the most recent successful run (thread-safe).
        {
          std::lock_guard<std::mutex> lock(m_diagMutex);
          m_diagTValues    = t_eff;
          m_diagThetaStars = theta_eff;
          m_diagSeHat      = se_hat;
          m_diagSkewPivot  = skew_pivot;
          m_diagValid      = true;
        }

        Result R;
        R.mean  = theta_hat;
        R.lower = Decimal(lower_d);
        R.upper = Decimal(upper_d);
        R.cl                        = m_CL;
        R.B_outer                   = m_B_outer;
        R.B_inner                   = m_B_inner;
        R.effective_B               = effective_B;
        R.skipped_outer             = skipped_outer.load(std::memory_order_relaxed);
        R.skipped_inner_total       = skipped_inner_total.load(std::memory_order_relaxed);
        R.inner_attempted_total     = inner_attempted_total.load(std::memory_order_relaxed);
        R.n                         = n;
        R.m_outer                   = m_outer;
        R.m_inner                   = m_inner;
        R.L                         = Ldiag;
        R.se_hat                    = se_hat;
        R.skew_pivot                = skew_pivot;
        R.low_effective_replicates  = low_effective_replicates;
        R.high_inner_skip_rate      = high_inner_skip_rate;
        R.extreme_pivot_skewness    = extreme_pivot_skewness;
        R.interval_type             = m_interval_type;
        return R;
      }

    private:
      // Configuration set at construction. Not declared const so that copy/move
      // assignment operators can assign them directly without undefined-behaviour
      // const_cast tricks (writing through a const_cast to a genuinely const
      // object is UB in C++).
      std::size_t  m_B_outer;
      std::size_t  m_B_inner;
      double       m_CL;
      Resampler    m_resampler;
      double       m_ratio_outer;
      double       m_ratio_inner;

      // Diagnostics for most recent successful run (protected by m_diagMutex).
      // Getters return copies to avoid reference invalidation by concurrent run().
      mutable std::mutex          m_diagMutex;
      mutable std::vector<double> m_diagTValues;
      mutable std::vector<double> m_diagThetaStars;
      mutable double              m_diagSeHat;
      mutable double              m_diagSkewPivot;   // skewness of t* (pivot) distribution
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
      //
      // num_resamples must be >= 400 to match PercentileTBootstrap's own
      // B_outer >= 400 requirement. Passing a value in [100, 400) would pass
      // this guard and then immediately throw inside the wrapped object; the
      // threshold is kept consistent here to give a clear error at the point
      // of construction.
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
        if (m_returns.empty() || num_resamples < 400u ||
            confidence_level <= 0.0 || confidence_level >= 1.0)
          throw std::invalid_argument(
            "BCaCompatibleTBootstrap: Invalid construction arguments.");
      }

      // -----------------------------------------------------------------------
      // Constructor for Provider != void (CRN path).
      //
      // Enabled only when Provider != void. Uses provider.make_engine(b) to
      // supply a deterministic RNG per outer replicate.
      //
      // num_resamples must be >= 400 for the same reason as the Provider=void
      // constructor above; see that constructor's comment.
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
        if (m_returns.empty() || num_resamples < 400u ||
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
