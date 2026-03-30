/**
 * @file MOutOfNPercentileBootstrap.h
 *
 * M-out-of-N percentile bootstrap with adaptive subsample ratio.
 *
 * Draws B resamples of size m < n (where m/n is the adaptive ratio),
 * computes a percentile CI, and rescales to the full-sample scale. Supports
 * pluggable ratio policies, reliability flags, and trade-level bootstrapping.
 *
 * Copyright (C) MKC Associates, LLC — All Rights Reserved.
 */

#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include "randutils.hpp"
#include "RngUtils.h"
#include "StatUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "AdaptiveRatioInternal.h"
#include "AdaptiveRatioPolicies.h"
#include "BootstrapTypes.h"

namespace palvalidator
{
  namespace analysis
  {
    using palvalidator::analysis::IntervalType;

    /**
     * m-out-of-n percentile bootstrap (stationary-block resampling aware).
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
     * - **Rescaling mode**: Optional rescaling of CI bounds and diagnostics to target
     *   sample size n (rescale_to_n=true), providing theoretically correct M-out-of-N
     *   inference, or conservative subsample-based inference (rescale_to_n=false, default).
     * - **Numerically robust**: Degenerate/NaN replicates are skipped; too many degenerates
     *   raise an error to avoid misleading intervals.
     *
     * @note Thread safety: Concurrent calls to run() are not supported due to shared
     *       diagnostic storage. Use separate instances or external synchronization.
     *
     * @tparam Decimal
     *   Numeric value type for statistics and bounds (e.g., dec::decimal<8>).
     * @tparam Sampler
     *   Callable with signature `Decimal(const std::vector<SampleType>&)` that computes
     *   the statistic of interest on a series (e.g., GeoMeanStat).
     *   When SampleType = Decimal (default/bar-level): `Decimal(const std::vector<Decimal>&)`
     *   When SampleType = Trade<Decimal> (trade-level): `Decimal(const std::vector<Trade<Decimal>>&)`
     * @tparam Resampler
     *   Type that provides `void operator()(const std::vector<SampleType>& x,
     *   std::vector<SampleType>& y, std::size_t m, Rng& rng) const;` and
     *   `std::size_t getL() const;`.
     *   For trade-level use: IIDResampler<Trade<Decimal>>.
     * @tparam Rng
     *   Random-number generator type. Defaults to `std::mt19937_64`.
     * @tparam Executor
     *   Parallel execution policy. Defaults to `SingleThreadExecutor`.
     * @tparam SampleType
     *   Element type of the input data vector. Defaults to Decimal (bar-level).
     *   Set to Trade<Decimal> for trade-level bootstrapping.
     *   All existing code with 3-5 explicit template parameters is unaffected.
     *
     *   IMPORTANT — adaptive mode and runWithRefinement() are NOT supported when
     *   SampleType != Decimal. The Hill estimator, skewness, and kurtosis required
     *   by StatisticalContext need ~8+ scalar losses, which are unavailable from
     *   n~9 Trade objects. Use the fixed-ratio constructor or createFixedRatio()
     *   at trade level. A static_assert enforces this at compile time.
     */
    template <class Decimal,
              class Sampler,
              class Resampler,
              class Rng        = std::mt19937_64,
              class Executor   = concurrency::SingleThreadExecutor,
              class SampleType = Decimal>
    class MOutOfNPercentileBootstrap
    {
    public:
      /**
       * Holds the output of an M-out-of-N bootstrap run, including reliability flags.
       */
      struct Result
      {
        Decimal     mean;              /**< Statistic on original sample. */
        Decimal     lower;             /**< Percentile lower bound. */
        Decimal     upper;             /**< Percentile upper bound. */
        double      cl;                /**< Confidence level. */
        std::size_t B;                 /**< Requested bootstrap replicates. */
        std::size_t effective_B;       /**< Usable (finite) replicates. */
        std::size_t skipped;           /**< Degenerate replicates skipped. */
        std::size_t n;                 /**< Sample size in SampleType units (bars or trades). */
        std::size_t m_sub;             /**< Subsample size m. */
        std::size_t L;                 /**< Resampler block length. */
        double      computed_ratio;    /**< Logical m/n ratio reported to callers. */
        double      skew_boot;         /**< Skewness of usable bootstrap θ* values. */
        bool        degenerate_warning;/**< True if excessive NaN/non-finite replicates. */

        /**
         * @name Reliability flags
         *
         * Analogous to BCa's isReliable() but split into distinct failure modes
         * since M-out-of-N failures are distributed across the bootstrap
         * distribution rather than concentrated in a single scalar parameter.
         *
         * These flags are INDEPENDENT of degenerate_warning above.
         * degenerate_warning measures NaN/non-finite replicates (resampling
         * failures). The flags below assess whether the *valid* replicates
         * form a structurally meaningful bootstrap distribution. Both can
         * be set simultaneously and should be interpreted separately.
         * @{
         */

        /**
         * True when fewer than RELIABILITY_UNIQUE_RATIO_THRESHOLD of replicates
         * are distinct, indicating m_sub is too small for meaningful variation.
         */
        bool        distribution_degenerate;
        /**
         * True (rescale_to_n only) when mean_boot deviates from θ̂ by more than
         * RELIABILITY_BIAS_FRACTION_THRESHOLD, indicating the Var ∝ 1/n assumption fails.
         */
        bool        excessive_bias;
        /**
         * True when the coefficient of variation of the bootstrap distribution
         * falls below RELIABILITY_MIN_CV_THRESHOLD.
         */
        bool        insufficient_spread;
        /**
         * True when computed_ratio is within RELIABILITY_BOUNDARY_FRACTION of
         * either the lower (2/n) or upper ((n−1)/n) clamp limit.
         */
        bool        ratio_near_boundary;

        /** @} */ // end Reliability flags

        /**
         * Bias fraction computed during excessive_bias evaluation.
         * Always 0.0 when rescale_to_n=false. Used by the downstream tournament
         * to calibrate the lower-bound haircut proportionally to bias severity.
         */
        double      bias_fraction;

        /**
         * Returns true if none of the four reliability flags are set.
         *
         * Downstream selectors (e.g., AutoBootstrapSelector) use this as a
         * single gate; diagnostic logging can inspect individual flags for
         * the specific failure reason.
         */
        bool isReliable() const noexcept
        {
          return !distribution_degenerate
              && !excessive_bias
              && !insufficient_spread
              && !ratio_near_boundary;
        }
      };

      /** Configuration constant: maximum allowed fraction of degenerate replicates. */
      static constexpr double MAX_DEGENERATE_FRACTION_WARN  = 0.10; /**< Warn at >10% degenerate. */
      static constexpr double MAX_DEGENERATE_FRACTION_ERROR = 0.25; /**< Throw at >25% degenerate. */

      /**
       * @name Reliability thresholds
       *
       * Govern the four failure-mode flags in Result. These are intentionally
       * conservative defaults: they flag pathological cases without triggering
       * on normal bootstrap variance.
       * @{
       */

      /**
       * Bootstrap distribution is flagged as degenerate when fewer than this
       * fraction of replicates produce distinct values. At 0.05, a distribution
       * with only 5% distinct values (e.g., 20 unique from B=400) triggers.
       */
      static constexpr double RELIABILITY_UNIQUE_RATIO_THRESHOLD = 0.05;

      /**
       * Bootstrap mean is flagged as excessively biased when it deviates from
       * theta_hat by more than this fraction of |theta_hat|. Only meaningful
       * when rescale_to_n=true. At 0.20, a 20% relative deviation triggers.
       */
      static constexpr double RELIABILITY_BIAS_FRACTION_THRESHOLD = 0.20;

      /**
       * Bootstrap distribution is flagged as insufficiently spread when its
       * coefficient of variation falls below this value. At 0.01, a distribution
       * where SE is less than 1% of the mean triggers.
       */
      static constexpr double RELIABILITY_MIN_CV_THRESHOLD = 0.01;

      /**
       * Minimum absolute value of theta_hat used as denominator when computing
       * the excessive_bias fraction. Prevents the fraction from becoming
       * arbitrarily large when theta_hat is near zero (e.g., near-breakeven
       * strategies), where tiny absolute differences in mean_boot produce
       * enormous relative fractions that do not reflect genuine bootstrap
       * pathology. Set to 0.001 (10 basis points).
       */
      static constexpr double RELIABILITY_BIAS_MIN_ABS_THETA = 0.001;

      /** @} */

    public:
      // ====================================================================
      // CONSTRUCTOR 1: Fixed Ratio
      // ====================================================================
      /**
       * Constructs an M-out-of-N bootstrap with fixed subsample ratio.
       *
       * @param B Number of bootstrap replicates (must be >= 400).
       * @param confidence_level Confidence level (must be in [0.90, 0.999]).
       * @param m_ratio Subsample ratio m/n (must be in (0, 1)).
       * @param resampler Resampling strategy.
       * @param rescale_to_n If true, rescale CI bounds and diagnostics to target
       *        sample size n (theoretically correct M-out-of-N). If false (default),
       *        provide conservative subsample-based inference.
       */
      MOutOfNPercentileBootstrap(std::size_t B,
                                 double      confidence_level,
                                 double      m_ratio,
                                 const Resampler& resampler,
                                 bool        rescale_to_n = false,
				 IntervalType interval_type = IntervalType::TWO_SIDED)
        : m_B(B)
        , m_CL(confidence_level)
        , m_ratio(m_ratio)
        , m_resampler(resampler)
        , m_rescale_to_n(rescale_to_n)
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
	, m_interval_type(interval_type)
      {
        validateParameters();
        if (!(m_ratio > 0.0 && m_ratio < 1.0))
        {
          throw std::invalid_argument("MOutOfNPercentileBootstrap: m_ratio must be in (0,1)");
        }
      }

      /**
       * Copy constructor. Resets executor, chunk hint, and diagnostics.
       */
      MOutOfNPercentileBootstrap(const MOutOfNPercentileBootstrap& other)
        : m_B(other.m_B)
        , m_CL(other.m_CL)
        , m_ratio(other.m_ratio)
        , m_resampler(other.m_resampler)
        , m_rescale_to_n(other.m_rescale_to_n)
        , m_exec(std::make_shared<Executor>())
        , m_chunkHint(0)
        , m_ratioPolicy(other.m_ratioPolicy)
        , m_diagMutex(std::make_unique<std::mutex>())
        , m_diagBootstrapStats()
        , m_diagMeanBoot(0.0)
        , m_diagVarBoot(0.0)
        , m_diagSeBoot(0.0)
        , m_diagSkewBoot(0.0)
        , m_diagValid(false)
	, m_interval_type(other.m_interval_type)
      {
      }

      /**
       * Move constructor. Transfers all state; marks moved-from diagnostics invalid.
       */
      MOutOfNPercentileBootstrap(MOutOfNPercentileBootstrap&& other) noexcept
        : m_B(other.m_B)
        , m_CL(other.m_CL)
        , m_ratio(other.m_ratio)
        , m_resampler(std::move(other.m_resampler))
        , m_rescale_to_n(other.m_rescale_to_n)
        , m_exec(std::move(other.m_exec))
        , m_chunkHint(other.m_chunkHint)
        , m_ratioPolicy(std::move(other.m_ratioPolicy))
        , m_diagMutex(std::move(other.m_diagMutex))
        , m_diagBootstrapStats(std::move(other.m_diagBootstrapStats))
        , m_diagMeanBoot(other.m_diagMeanBoot)
        , m_diagVarBoot(other.m_diagVarBoot)
        , m_diagSeBoot(other.m_diagSeBoot)
        , m_diagSkewBoot(other.m_diagSkewBoot)
        , m_diagValid(other.m_diagValid)
	, m_interval_type(other.m_interval_type)
      {
        // Reset moved-from object
        other.m_diagValid = false;
      }

      /**
       * Copy assignment operator. Clears diagnostics under lock.
       */
      MOutOfNPercentileBootstrap& operator=(const MOutOfNPercentileBootstrap& other)
      {
        if (this != &other) {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_ratio = other.m_ratio;
          m_resampler = other.m_resampler;
          m_rescale_to_n = other.m_rescale_to_n;
          m_exec = std::make_shared<Executor>();
          m_chunkHint = 0;
          m_ratioPolicy = other.m_ratioPolicy;
          m_diagMutex = std::make_unique<std::mutex>();
	  m_interval_type = other.m_interval_type;

          // Clear diagnostic data
          if (m_diagMutex) {
            std::lock_guard<std::mutex> lock(*m_diagMutex);
            m_diagBootstrapStats.clear();
            m_diagMeanBoot = 0.0;
            m_diagVarBoot = 0.0;
            m_diagSeBoot = 0.0;
            m_diagSkewBoot = 0.0;
            m_diagValid = false;
          }
        }
        return *this;
      }

      /**
       * Move assignment operator. Transfers diagnostics; marks moved-from invalid.
       */
      MOutOfNPercentileBootstrap& operator=(MOutOfNPercentileBootstrap&& other) noexcept
      {
        if (this != &other) {
          m_B = other.m_B;
          m_CL = other.m_CL;
          m_ratio = other.m_ratio;
          m_resampler = std::move(other.m_resampler);
          m_rescale_to_n = other.m_rescale_to_n;
          m_exec = std::move(other.m_exec);
          m_chunkHint = other.m_chunkHint;
          m_ratioPolicy = std::move(other.m_ratioPolicy);
          m_diagMutex = std::move(other.m_diagMutex);
	  m_interval_type = other.m_interval_type;

          // Transfer diagnostic data
          m_diagBootstrapStats = std::move(other.m_diagBootstrapStats);
          m_diagMeanBoot = other.m_diagMeanBoot;
          m_diagVarBoot = other.m_diagVarBoot;
          m_diagSeBoot = other.m_diagSeBoot;
          m_diagSkewBoot = other.m_diagSkewBoot;
          m_diagValid = other.m_diagValid;

          // Reset moved-from object
          other.m_diagValid = false;
        }
        return *this;
      }

      /**
       * Fixed-ratio factory (thin wrapper over the existing constructor).
       */
      static MOutOfNPercentileBootstrap
      createFixedRatio(std::size_t B,
                       double      confidence_level,
                       double      m_ratio,
                       const Resampler& resampler,
                       bool        rescale_to_n = false,
		       IntervalType interval_type = IntervalType::TWO_SIDED)
      {
        return MOutOfNPercentileBootstrap(B, confidence_level, m_ratio, resampler,
					  rescale_to_n, interval_type);
      }

      /**
       * Adaptive-ratio factory using a caller-supplied policy.
       */
      template<typename BootstrapStatistic>
      static MOutOfNPercentileBootstrap
      createAdaptiveWithPolicy( std::size_t B,
				double      confidence_level,
				const Resampler& resampler,
				std::shared_ptr<IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>> policy,
				bool        rescale_to_n = false,
				IntervalType interval_type = IntervalType::TWO_SIDED)
      {
        if (!policy)
        {
          throw std::invalid_argument(
            "MOutOfNPercentileBootstrap::createAdaptiveWithPolicy: policy cannot be null");
        }

        // Start from any valid fixed-ratio instance (ratio is ignored in adaptive mode).
        MOutOfNPercentileBootstrap instance(B,
					    confidence_level,
					    /*m_ratio=*/0.5,
					    resampler,
					    rescale_to_n,
					    interval_type);

        instance.m_ratio      = -1.0;  // switch to adaptive mode
        instance.m_ratioPolicy =
          std::static_pointer_cast<void>(policy);

        return instance;
      }

      /**
       * Adaptive-ratio factory using the default TailVolatilityAdaptivePolicy.
       */
      template<typename BootstrapStatistic>
      static MOutOfNPercentileBootstrap
      createAdaptive(std::size_t B,
                     double      confidence_level,
                     const Resampler& resampler,
                     bool        rescale_to_n = false,
		     IntervalType interval_type = IntervalType::TWO_SIDED)
      {
        auto defaultPolicy = std::make_shared<
          TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic>>();

        return createAdaptiveWithPolicy<BootstrapStatistic>(
          B, confidence_level, resampler, defaultPolicy, rescale_to_n, interval_type);
      }

      // ====================================================================
      // RUN METHODS
      // ====================================================================

      /**
       * Run the M-out-of-N bootstrap with a caller-supplied RNG.
       *
       * Seeds are precomputed in the calling thread so the caller RNG is never
       * touched inside the parallel region.
       *
       * @param x              Input sample (bars or trades).
       * @param sampler        Statistic callable mapping a sample vector to Decimal.
       * @param rng            Caller-owned random engine (used only for seed generation).
       * @param m_sub_override If > 0, overrides the adaptive/fixed subsample size.
       * @param diagnosticLog  Optional stream for diagnostic output.
       * @return Result containing bounds, diagnostics, and reliability flags.
       * @throws std::invalid_argument If x.size() < 3.
       */
      Result run(const std::vector<SampleType>& x,
                 Sampler                        sampler,
                 Rng&                           rng,
                 std::size_t                    m_sub_override = 0,
                 std::ostream*                  diagnosticLog  = nullptr) const
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

      /**
       * Run the M-out-of-N bootstrap with a CRN engine provider.
       *
       * Provider concept: `Rng make_engine(std::size_t b) const`.
       *
       * @tparam Provider  Engine provider type satisfying the make_engine concept.
       * @param x              Input sample (bars or trades).
       * @param sampler        Statistic callable.
       * @param provider       CRN engine provider (const ref).
       * @param m_sub_override If > 0, overrides the adaptive/fixed subsample size.
       * @param diagnosticLog  Optional stream for diagnostic output.
       * @return Result containing bounds, diagnostics, and reliability flags.
       * @throws std::invalid_argument If x.size() < 3.
       */
      template <class Provider>
      Result run(const std::vector<SampleType>& x,
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

      /**
       * Run with adaptive ratio refinement via probe-engine probing.
       *
       * Two-tier API: first resolves the subsample ratio using the adaptive
       * policy's refinement loop (which may run probe bootstraps internally),
       * then executes the main bootstrap with the resolved ratio and a CRN
       * provider from the factory.
       *
       * Only available when SampleType == Decimal (bar-level). A static_assert
       * enforces this at compile time.
       *
       * @tparam BootstrapStatistic  Statistic functor type for the probe engine.
       * @tparam StrategyT           Strategy type providing hashCode() for CRN tagging.
       * @tparam BootstrapFactoryT   Factory type providing makeMOutOfN().
       * @param x              Input sample.
       * @param sampler        Statistic callable.
       * @param strategy       Strategy reference for CRN key derivation.
       * @param factory        Bootstrap factory for CRN provider construction.
       * @param stageTag       CRN stage tag (metric identifier).
       * @param fold           CRN fold identifier (0 = no cross-validation).
       * @param diagnosticLog  Optional stream for diagnostic output.
       * @return Result containing bounds, diagnostics, and reliability flags.
       * @throws std::invalid_argument If x.size() < 3.
       */
      template <typename BootstrapStatistic, typename StrategyT, typename BootstrapFactoryT>
      Result runWithRefinement(
        const std::vector<SampleType>& x,
        Sampler sampler,
        StrategyT& strategy,
        BootstrapFactoryT& factory,
        int stageTag,
        int fold,
        std::ostream* diagnosticLog = nullptr) const
      {
        // TIER 3 GUARD: runWithRefinement() is not supported at trade level.
        //
        // Rationale: the refinement pipeline (ConcreteProbeEngineMaker, IProbeEngineMaker,
        // StatisticalContext, TailVolatilityAdaptivePolicy) is wired to std::vector<Decimal>
        // throughout. More fundamentally, the adaptive ratio heuristics (Hill estimator,
        // skewness, kurtosis) require ~8+ scalar observations to produce reliable estimates.
        // With n~9 Trade objects, those estimates are too noisy to drive a useful ratio
        // decision. Use the fixed-ratio run() overload with IIDResampler<Trade<Decimal>>.
        static_assert(std::is_same_v<SampleType, Decimal>,
          "MOutOfNPercentileBootstrap::runWithRefinement() is not supported for "
          "trade-level bootstrapping (SampleType != Decimal). "
          "Use the fixed-ratio run() overload with IIDResampler<Trade<Decimal>> instead. "
          "See class documentation for the rationale.");
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

      /**
       * Installs an adaptive ratio policy and switches to adaptive mode.
       *
       * @tparam BootstrapStatistic Statistic functor type used by the policy.
       * @param  policy             Shared pointer to the policy (must not be null).
       * @throws std::invalid_argument If policy is null.
       */
      template<typename BootstrapStatistic>
      void setAdaptiveRatioPolicy(
        std::shared_ptr<IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>> policy)
      {
        if (!policy)
          throw std::invalid_argument("Policy cannot be null");

        m_ratioPolicy = std::static_pointer_cast<void>(policy);
        m_ratio       = -1.0;  // switch to adaptive mode
      }

      /**
       * Returns true if the instance is configured for adaptive ratio selection.
       */
      bool isAdaptiveMode() const { return m_ratio < 0.0; }

      /**
       * Returns true if rescaling from subsample size m to full size n is enabled.
       */
      bool rescalesToN() const { return m_rescale_to_n; }

      /**
       * Hint for parallel_for_chunked granularity (0 = auto).
       */
      void setChunkSizeHint(uint32_t c) { m_chunkHint = c; }

      /** Returns the number of requested bootstrap replicates B. */
      std::size_t B()        const { return m_B; }
      /** Returns the confidence level. */
      double      CL()       const { return m_CL; }
      /** Returns the configured fixed ratio (negative if adaptive mode). */
      double      mratio()   const { return m_ratio; }
      /** Returns a const reference to the resampler. */
      const Resampler& resampler() const { return m_resampler; }

      // ====================================================================
      // Diagnostics for AutoBootstrapSelector
      // ====================================================================

      /**
       * Returns true if diagnostics from a successful run are available.
       */
      bool hasDiagnostics() const noexcept
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        return m_diagValid;
      }

      /**
       * Returns the vector of usable bootstrap statistics {θ*_b}.
       * @throws std::logic_error If run() has not been called successfully.
       */
      const std::vector<double>& getBootstrapStatistics() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagBootstrapStats;
        }

      /**
       * Returns the mean of usable bootstrap replicates.
       * @throws std::logic_error If run() has not been called successfully.
       */
      double getBootstrapMean() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagMeanBoot;
        }

      /**
       * Returns the sample variance of usable bootstrap replicates.
       * @throws std::logic_error If run() has not been called successfully.
       */
      double getBootstrapVariance() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagVarBoot;
        }

      /**
       * Returns the standard error of usable bootstrap replicates.
       * @throws std::logic_error If run() has not been called successfully.
       */
      double getBootstrapSe() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagSeBoot;
        }

      /**
       * Returns the skewness of usable bootstrap replicates.
       * @throws std::logic_error If run() has not been called successfully.
       */
      double getBootstrapSkewness() const
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          ensureDiagnosticsAvailable();
          return m_diagSkewBoot;
        }

      /**
       * @name Reliability diagnostic accessors
       *
       * Mirror the flags in Result, allowing callers to inspect reliability
       * without retaining the Result object.
       * @{
       */

      /**
       * Returns true if the bootstrap distribution had too few distinct values.
       * @throws std::logic_error If run() has not been called successfully.
       */
      bool isDistributionDegenerate() const
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagDistributionDegenerate;
      }

      /**
       * Returns true if bootstrap mean deviated excessively from θ̂ (rescale_to_n only).
       * @throws std::logic_error If run() has not been called successfully.
       */
      bool isExcessiveBias() const
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagExcessiveBias;
      }

      /**
       * Returns true if the bootstrap distribution's CV was below threshold.
       * @throws std::logic_error If run() has not been called successfully.
       */
      bool isInsufficientSpread() const
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagInsufficientSpread;
      }

      /**
       * Returns true if the computed ratio was clamped to a boundary.
       * @throws std::logic_error If run() has not been called successfully.
       */
      bool isRatioNearBoundary() const
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagRatioNearBoundary;
      }

      /**
       * Convenience: returns true if none of the four reliability flags are set.
       */
      bool isReliable() const
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        ensureDiagnosticsAvailable();
        return !m_diagDistributionDegenerate
            && !m_diagExcessiveBias
            && !m_diagInsufficientSpread
            && !m_diagRatioNearBoundary;
      }

      /**
       * Returns the bias fraction computed during the most recent run.
       * Always 0.0 when rescale_to_n=false. When rescale_to_n=true, reflects
       * |mean_boot - theta_hat| / max(|theta_hat|, RELIABILITY_BIAS_MIN_ABS_THETA).
       * Used by the downstream tournament to calibrate the lower bound haircut
       * proportionally to the severity of the bias rather than a fixed fraction.
       */
      double getBiasFraction() const
      {
        std::lock_guard<std::mutex> lock(*m_diagMutex);
        ensureDiagnosticsAvailable();
        return m_diagBiasFraction;
      }

      /** @} */ // end Reliability diagnostic accessors

    private:
      /**
       * Throws std::logic_error if diagnostics are not yet populated.
       * Caller must hold m_diagMutex.
       */
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

      /**
       * Validates B and CL constructor parameters.
       * @throws std::invalid_argument If B < 400 or CL is not in (0.5, 1).
       */
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

      /**
       * Core resampling loop shared by all run() overloads.
       *
       * Resolves the subsample size (from override, adaptive policy, or fixed ratio),
       * runs B resamples in parallel via the executor, computes percentile quantiles,
       * evaluates reliability flags, and populates diagnostics.
       *
       * @tparam EngineMaker  Callable with signature Rng(std::size_t b).
       * @param  x              Input sample.
       * @param  sampler        Statistic callable.
       * @param  m_sub_override If > 0, overrides the subsample size.
       * @param  make_engine    Per-replicate engine factory.
       * @param  diagnosticLog  Optional stream for diagnostic output.
       * @return Fully populated Result.
       * @throws std::invalid_argument If x.size() < 3.
       * @throws std::runtime_error    If the degenerate replicate fraction exceeds the error threshold.
       */
      template <class EngineMaker>
      Result run_core_(const std::vector<SampleType>& x,
                       Sampler                         sampler,
                       std::size_t                     m_sub_override,
                       EngineMaker&&                   make_engine,
                       std::ostream*                   diagnosticLog = nullptr) const
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
          // TIER 2: StatisticalContext<Decimal> requires a flat vector<Decimal>.
          // When SampleType = Trade<Decimal> this branch is unreachable at runtime
          // (the static_assert in computeAdaptiveRatio() fires first at compile
          // time if someone attempts it), but we use if constexpr here so the
          // compiler never attempts to instantiate the Decimal-specific code with
          // Trade-typed x, which would be a type error even before the assert.
          if constexpr (std::is_same_v<SampleType, Decimal>)
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
            // Trade-level with adaptive mode is a programming error.
            // A static_assert cannot be used here: even though this else-branch
            // is inside an outer `if constexpr (std::is_same_v<SampleType, Decimal>)`
            // block, the INNER runtime `if (isAdaptiveMode())` means this else-branch
            // is the *active* (non-discarded) branch when SampleType != Decimal.
            // A static_assert in an active branch always fires at compile time,
            // regardless of runtime reachability.  A runtime throw is correct.
            throw std::logic_error(
              "MOutOfNPercentileBootstrap: adaptive ratio mode is not supported "
              "for trade-level bootstrapping (SampleType != Decimal). "
              "Use the fixed-ratio constructor instead of createAdaptive() or "
              "createAdaptiveWithPolicy() at trade level.");
          }
        }
        else
        {
          m_sub        = static_cast<std::size_t>(std::floor(m_ratio * static_cast<double>(n)));
          actual_ratio = static_cast<double>(m_sub) / static_cast<double>(n);
          reported_ratio = m_ratio;  // report configured fixed ratio, not m_sub/n
        }

        // Clamp to valid range [2, n-1]
	// WITH this:
	const std::size_t m_sub_before_clamp = m_sub;
	if (m_sub < 2)
	  m_sub = 2;
	if (m_sub >= n)
	  m_sub = n - 1;
	
	if (m_sub != m_sub_before_clamp)
	  {
	    actual_ratio   = static_cast<double>(m_sub) / static_cast<double>(n);
	    reported_ratio = actual_ratio;
	  }
	
        const Decimal theta_hat = sampler(x);

        // Pre-allocate; NaN marks skipped/invalid replicates
        std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

        concurrency::parallel_for_chunked(
          static_cast<uint32_t>(m_B),
          *m_exec,
          [&](uint32_t b) {
            auto rng = make_engine(b);
            // y is std::vector<SampleType>.
            // When SampleType = Decimal:        vector of bar returns (original behaviour).
            // When SampleType = Trade<Decimal>: vector of Trade objects.
            // SampleType must be default-constructible; Trade<Decimal> satisfies this.
            std::vector<SampleType> y;
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

	const double degenerate_fraction =
	  static_cast<double>(skipped) / static_cast<double>(m_B);

	if (degenerate_fraction > MAX_DEGENERATE_FRACTION_ERROR)
	  {
	    std::lock_guard<std::mutex> lock(*m_diagMutex);
	    m_diagValid = false;
	    throw std::runtime_error(
				     "MOutOfNPercentileBootstrap: too many degenerate replicates ("
				     + std::to_string(static_cast<int>(degenerate_fraction * 100))
				     + "% failed, error threshold is "
				     + std::to_string(static_cast<int>(MAX_DEGENERATE_FRACTION_ERROR * 100))
				     + "%)");
	  }

	const bool degenerate_warning = (degenerate_fraction > MAX_DEGENERATE_FRACTION_WARN);

	if (degenerate_warning && diagnosticLog)
	  {
	    (*diagnosticLog)
	      << "[M-out-of-N WARNING] "
	      << static_cast<int>(degenerate_fraction * 100)
	      << "% of replicates were degenerate (threshold: "
	      << static_cast<int>(MAX_DEGENERATE_FRACTION_WARN * 100)
	      << "%). Interval reliability may be reduced.\n";
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

        // ====================================================================
        // RESCALING LOGIC (NEW)
        // ====================================================================
        // If rescale_to_n is enabled, we rescale the bootstrap statistics from
        // subsample size m_sub to target sample size n. This provides theoretically
        // correct M-out-of-N inference (coverage for size n) rather than conservative
        // subsample-based inference (coverage for size m_sub).
        //
        // The rescaling factor sqrt(m_sub/n) contracts the m-subsample
        // fluctuations (O(1/sqrt(m_sub))) down to n-sample scale (O(1/sqrt(n))),
        // assuming variance scales as 1/sample_size. Note: this is sqrt(m_sub/n),
        // NOT sqrt(n/m_sub) — the contraction shrinks deviations toward theta_hat.
        //
        // Note: Skewness is a scale-invariant statistic, so it doesn't change.
        // ====================================================================
        if (m_rescale_to_n)
        {
	  const double scale_factor = std::sqrt(static_cast<double>(m_sub) / static_cast<double>(n));
          const double theta_hat_d = num::to_double(theta_hat);
          
          if (diagnosticLog)
          {
            (*diagnosticLog) << "[M-out-of-N Rescaling] n=" << n << ", m_sub=" << m_sub 
                           << ", scale_factor=" << scale_factor << "\n";
            (*diagnosticLog) << "  Before rescaling: mean_boot=" << mean_boot 
                           << ", se_boot=" << se_boot << "\n";
          }
          
	  // Rescale the m-out-of-n bootstrap replicates to the full-sample scale.
	  // Each bootstrap statistic is centered at theta_hat and then contracted
	  // by sqrt(m_sub / n), because the m-subsample fluctuation is O(1/sqrt(m_sub))
	  // while the target n-sample fluctuation is O(1/sqrt(n)).
          for (double& v : thetas_d)
          {
	    const double centered = v - theta_hat_d;   // standard pivot: θ̂*_m - θ̂_n
            v = theta_hat_d + centered * scale_factor;
          }
          
          // Recompute mean and variance on rescaled data
          mean_boot = 0.0;
          for (double v : thetas_d) mean_boot += v;
          mean_boot /= static_cast<double>(m);
          
          var_boot = 0.0;
          if (m > 1)
          {
            for (double v : thetas_d)
            {
              const double d = v - mean_boot;
              var_boot += d * d;
            }
            var_boot /= static_cast<double>(m - 1);
          }
          
          if (diagnosticLog)
          {
            (*diagnosticLog) << "  After rescaling: mean_boot=" << mean_boot 
                           << ", se_boot=" << std::sqrt(var_boot) << "\n";
          }
          
          // Note: skewness remains unchanged (scale-invariant)
          // We don't need to recompute it
        }

        // Percentile CI (type-7) at CL
        const double alpha = 1.0 - m_CL;

	// Compute quantile probabilities based on interval type
        double pl, pu;
        switch (m_interval_type)
	  {
          case IntervalType::TWO_SIDED:
	  default:
            pl = alpha / 2.0;        // 2.5% for CL=0.95
            pu = 1.0 - alpha / 2.0;  // 97.5% for CL=0.95
            break;
          
          case IntervalType::ONE_SIDED_LOWER:
            pl = alpha;              // 5% for CL=0.95 - less conservative lower bound
            pu = 1.0 - 1e-10;        // extreme upper empirical quantile (not truly unbounded)
            break;
          
          case IntervalType::ONE_SIDED_UPPER:
            pl = 1e-10;              // extreme lower empirical quantile (not truly unbounded)
            pu = 1.0 - alpha;        // 95% for CL=0.95 - less conservative upper bound
            break;
        }

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

        // ====================================================================
        // RELIABILITY CHECKS
        // ====================================================================
        // Compute four structural failure-mode flags. These are distinct from
        // the degenerate_warning (which measures NaN/non-finite replicates) —
        // they assess whether the *valid* replicates form a meaningful
        // bootstrap distribution. See Result struct documentation for the
        // interpretation of each flag.
        // ====================================================================

        // Flag 1: Distribution degeneracy — too few distinct values.
        // Uses the post-rescaling thetas_d so the check reflects the actual
        // distribution used for quantile computation.
        const std::size_t uniqueCount =
            std::set<double>(thetas_d.begin(), thetas_d.end()).size();
        const double uniqueRatio =
            static_cast<double>(uniqueCount) / static_cast<double>(m);
        const bool distribution_degenerate =
            (uniqueRatio < RELIABILITY_UNIQUE_RATIO_THRESHOLD);

        // Flag 2: Excessive bias — bootstrap mean far from theta_hat.
        // Only meaningful when rescale_to_n=true; the rescaling assumes
        // Var ∝ 1/sample_size, and a large bias gap indicates this assumption
        // is being violated. When rescale_to_n=false the bias is expected and
        // the flag is suppressed to avoid spurious warnings.
        const double theta_hat_d_rel = num::to_double(theta_hat);
        // Use max(|theta_hat|, RELIABILITY_BIAS_MIN_ABS_THETA) as denominator
        // to prevent the fraction exploding when theta_hat is near zero.
        const double bias_denom = std::max(std::abs(theta_hat_d_rel),
                                           RELIABILITY_BIAS_MIN_ABS_THETA);
        const double biasFraction =
            std::abs(mean_boot - theta_hat_d_rel) / bias_denom;
        const bool excessive_bias =
            m_rescale_to_n && (biasFraction > RELIABILITY_BIAS_FRACTION_THRESHOLD);

        // Flag 3: Insufficient spread — CV of the bootstrap distribution near zero.
        // A near-zero CV means the bootstrap distribution is too concentrated
        // to produce a meaningful interval, regardless of the interval width.
        const double cvBoot =
            (std::abs(mean_boot) > 1e-10)
            ? (std::sqrt(var_boot) / std::abs(mean_boot))
            : std::sqrt(var_boot);
        const bool insufficient_spread =
            (m > 1) && (cvBoot < RELIABILITY_MIN_CV_THRESHOLD);

        // Flag 4: Ratio near boundary — computed_ratio very close to either
        // the lower limit (2/n) or upper limit ((n-1)/n). This indicates that
        // clamping fired or the adaptive policy pushed the ratio into a
        // degenerate region where subsample behaviour is unreliable.
        //
        // The guard (isAdaptiveMode() || clamping fired || m_sub_override > 0)
        // is intentional: a caller who explicitly chose a small fixed ratio
        // should NOT be flagged — they made a deliberate choice. The flag
        // is only meaningful when the ratio was computed or adjusted
        // internally (adaptive policy, clamping, or override), where an
        // extreme value signals a pathological internal outcome rather than
        // a deliberate user decision.
        const double lower_boundary = 3.0 / static_cast<double>(n);
        const double upper_boundary =
            static_cast<double>(n - 2) / static_cast<double>(n);
        const bool ratio_near_boundary =
            (isAdaptiveMode() || (m_sub != m_sub_before_clamp) || (m_sub_override > 0))
            && ((reported_ratio < lower_boundary) || (reported_ratio > upper_boundary));

        // Log reliability issues if a diagnostic stream is available
        if (diagnosticLog)
        {
          if (distribution_degenerate)
            (*diagnosticLog) << "[M-out-of-N RELIABILITY] distribution_degenerate: "
                             << uniqueCount << " unique values from " << m
                             << " replicates (ratio=" << uniqueRatio
                             << " < threshold=" << RELIABILITY_UNIQUE_RATIO_THRESHOLD
                             << "). m_sub=" << m_sub << " may be too small.\n";

          if (excessive_bias)
            (*diagnosticLog) << "[M-out-of-N RELIABILITY] excessive_bias: "
                             << "mean_boot=" << mean_boot
                             << " vs theta_hat=" << theta_hat_d_rel
                             << " (bias fraction=" << biasFraction
                             << " > threshold=" << RELIABILITY_BIAS_FRACTION_THRESHOLD
                             << "). rescale_to_n assumption may be violated.\n";

          if (insufficient_spread)
            (*diagnosticLog) << "[M-out-of-N RELIABILITY] insufficient_spread: "
                             << "CV=" << cvBoot
                             << " < threshold=" << RELIABILITY_MIN_CV_THRESHOLD
                             << ". Bootstrap distribution too concentrated.\n";

          if (ratio_near_boundary)
            (*diagnosticLog) << "[M-out-of-N RELIABILITY] ratio_near_boundary: "
                             << "computed_ratio=" << reported_ratio
                             << " is within boundary margins ["
                             << lower_boundary << ", " << upper_boundary
                             << "]. Clamping may have fired.\n";
        }

        // Store diagnostics for the most recent run (with thread safety)
        {
          std::lock_guard<std::mutex> lock(*m_diagMutex);
          m_diagBootstrapStats          = thetas_d;
          m_diagMeanBoot                = mean_boot;
          m_diagVarBoot                 = var_boot;
          m_diagSeBoot                  = std::sqrt(var_boot);
          m_diagSkewBoot                = skew_boot;
          m_diagDistributionDegenerate  = distribution_degenerate;
          m_diagExcessiveBias           = excessive_bias;
          m_diagInsufficientSpread      = insufficient_spread;
          m_diagRatioNearBoundary       = ratio_near_boundary;
          m_diagBiasFraction            = biasFraction;
          m_diagValid                   = true;
        }

        return Result{
          /*mean                    =*/ theta_hat,
          /*lower                   =*/ lb,
          /*upper                   =*/ ub,
          /*cl                      =*/ m_CL,
          /*B                       =*/ m_B,
          /*effective_B             =*/ thetas_d.size(),
          /*skipped                 =*/ skipped,
          /*n                       =*/ n,
          /*m_sub                   =*/ m_sub,
          /*L                       =*/ m_resampler.getL(),
          /*computed_ratio          =*/ reported_ratio,
          /*skew_boot               =*/ skew_boot,
          /*degenerate_warning      =*/ degenerate_warning,
          /*distribution_degenerate =*/ distribution_degenerate,
          /*excessive_bias          =*/ excessive_bias,
          /*insufficient_spread     =*/ insufficient_spread,
          /*ratio_near_boundary     =*/ ratio_near_boundary,
          /*bias_fraction           =*/ biasFraction
        };
      }

      // ====================================================================
      // ADAPTIVE RATIO DISPATCH
      // ====================================================================

      /**
       * Dispatches to the installed adaptive ratio policy to compute m/n.
       *
       * Falls back to a default TailVolatilityAdaptivePolicy if no policy was
       * explicitly installed. Only available for bar-level (SampleType == Decimal).
       *
       * @tparam BootstrapStatistic Statistic functor type for the policy (defaults to Sampler).
       * @param  x             Input sample.
       * @param  ctx           Pre-computed distributional context (tail index, skewness, etc.).
       * @param  diagnosticLog Optional stream for diagnostic output.
       * @return The adaptive subsample ratio in (0, 1).
       */
      template<typename BootstrapStatistic = Sampler>
      double computeAdaptiveRatio(const std::vector<Decimal>&                x,
                                  const detail::StatisticalContext<Decimal>& ctx,
                                  std::ostream*                              diagnosticLog) const
      {
        // TIER 3 GUARD: adaptive ratio computation is not supported at trade level.
        // This function is only reachable from the SampleType=Decimal branch of
        // run_core_() (guarded by if constexpr), but the static_assert provides
        // an explicit compile-time error if called directly on a trade-level instance.
        static_assert(std::is_same_v<SampleType, Decimal>,
          "MOutOfNPercentileBootstrap::computeAdaptiveRatio() is not supported for "
          "trade-level bootstrapping (SampleType != Decimal). "
          "Use the fixed-ratio run() overload with IIDResampler<Trade<Decimal>> instead.");
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
      std::size_t  m_B;            /**< Number of bootstrap replicates. */
      double       m_CL;           /**< Confidence level. */
      double       m_ratio;        /**< Fixed ratio (−1.0 = adaptive mode). */
      Resampler    m_resampler;    /**< Injected resampling strategy. */
      bool         m_rescale_to_n; /**< If true, rescale CI from m to n. */
      mutable std::shared_ptr<Executor> m_exec;   /**< Parallel executor. */
      mutable uint32_t           m_chunkHint{0};  /**< Chunk size hint for parallel_for. */
      std::shared_ptr<void>      m_ratioPolicy;   /**< Type-erased adaptive ratio policy. */

      /** @name Diagnostics from most recent run (protected by m_diagMutex)
       *  @{ */
      mutable std::unique_ptr<std::mutex> m_diagMutex;
      mutable std::vector<double> m_diagBootstrapStats;  /**< Usable θ* values. */
      mutable double              m_diagMeanBoot;        /**< Mean of bootstrap distribution. */
      mutable double              m_diagVarBoot;         /**< Variance of bootstrap distribution. */
      mutable double              m_diagSeBoot;          /**< Standard error of bootstrap distribution. */
      mutable double              m_diagSkewBoot;        /**< Skewness of bootstrap distribution. */
      mutable bool                m_diagValid;           /**< True after a successful run(). */
      /** @} */

      /** @name Reliability flags from most recent run (protected by same mutex)
       *  @{ */
      mutable bool                m_diagDistributionDegenerate{false};
      mutable bool                m_diagExcessiveBias{false};
      mutable bool                m_diagInsufficientSpread{false};
      mutable bool                m_diagRatioNearBoundary{false};
      mutable double              m_diagBiasFraction{0.0};
      /** @} */
      IntervalType m_interval_type; /**< Interval type (two-sided or one-sided). */
    };
  }
} // namespace palvalidator::analysis
