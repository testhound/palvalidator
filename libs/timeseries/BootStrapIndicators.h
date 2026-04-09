// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
//
// New Bootstrapped Stop/Target functions

#ifndef __BOOTSTRAPPED_INDICATORS_H
#define __BOOTSTRAPPED_INDICATORS_H

#include <functional>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <iostream>

// Include your two existing headers
#include "TimeSeriesIndicators.h"
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"

namespace mkc_timeseries
{
  // Internal namespace for helper functions
  namespace detail
  {
    /**
     * @brief Helper struct to hold the four critical bounds from bootstrapping.
     */
    template <class Decimal>
    struct BootstrappedWidthBounds
    {
      Decimal upside_lower_bound;
      Decimal upside_upper_bound;
      Decimal downside_lower_bound;
      Decimal downside_upper_bound;
    };

    /**
     * @brief Internal helper to run BCa bootstrap on upside and downside widths.
     *
     * This is the statistical engine that powers both ComputeBootStrappedLongStopAndTarget
     * and ComputeBootStrappedShortStopAndTarget. It performs the core bootstrap analysis
     * on the distribution of width statistics.
     *
     * ## WHAT THIS FUNCTION DOES
     *
     * 1. **Validates Data:**
     *    - Checks if sample size ≥ kMinBootstrapSize (30)
     *    - Returns epsilon bounds if insufficient data
     *
     * 2. **Defines Width Statistics:**
     *    - Upside Width = q90 - q50 (profit potential for longs)
     *    - Downside Width = q50 - q10 (risk exposure for longs)
     *
     * 3. **Configures Block Resampler:**
     *    - Calculates block length adaptively:
     *      * For n >= 100: Uses ACF-based calculation (see BLOCK LENGTH SELECTION below)
     *      * For n < 100: Uses n^(1/3) heuristic
     *    - Creates StationaryBlockResampler with this length
     *
     * 4. **Runs Two Separate BCa Bootstraps:**
     *    - One for upside width distribution
     *    - One for downside width distribution
     *    - Each with 10,000 resamples and 90% confidence
     *
     * 5. **Extracts Four Critical Bounds:**
     *    - upside_lower_bound (5th percentile of upside widths)
     *    - upside_upper_bound (95th percentile of upside widths)
     *    - downside_lower_bound (5th percentile of downside widths)
     *    - downside_upper_bound (95th percentile of downside widths)
     *
     * ## WHY SEPARATE BOOTSTRAPS?
     *
     * We could bootstrap a single statistic that captures both upside and downside,
     * but running separate bootstraps provides:
     *
     * - **Independent uncertainty quantification:** Upside and downside may have
     *   different variability
     * - **Asymmetric distributions:** Financial returns are typically skewed
     * - **Flexible application:** Caller can combine bounds as needed for LONG/SHORT
     *
     * ## CONFIGURATION CONSTANTS
     *
     * These are the tuneable parameters that control bootstrap behavior:
     *
     * ```cpp
     * constexpr size_t kMinBootstrapSize = 30;
     * constexpr unsigned int kNumResamples = 10000;
     * constexpr double kConfidenceLevel = 0.90;
     * constexpr std::size_t kMaxACFLag = 20;
     * constexpr double kBonferroniZ = 3.05;
     * ```
     *
     * **kMinBootstrapSize = 30:**
     * - Based on Central Limit Theorem heuristics
     * - Below this, asymptotic properties of bootstrap may not hold
     * - Below this, BCa corrections become unreliable
     * - Trade-off: Lower = more lenient, Higher = more conservative
     *
     * **kNumResamples = 10,000:**
     * - More resamples = more stable CI estimates, slower computation
     * - Academic standard is often 2,000-5,000
     * - 10,000 is generous and provides very stable estimates
     * - Reducing to 2,000 gives ~5× speedup with minimal quality loss
     *
     * **kConfidenceLevel = 0.90:**
     * - Produces 90% CI (5th to 95th percentiles)
     * - Standard in industry practice
     * - 0.80 = less conservative (narrower CI)
     * - 0.95 = more conservative (wider CI)
     * - Don't go outside [0.80, 0.95] range
     *
     * **kMaxACFLag = 20:**
     * - Maximum lag used in ACF analysis for block length selection
     * - Must match M in the Bonferroni threshold formula (see below)
     * - 20 lags is sufficient to detect weekly/monthly return cycles in daily data
     *
     * **kBonferroniZ = 3.05:**
     * - z-critical for Bonferroni-corrected ACF significance test
     * - Derived from: qnorm(1 - 0.05 / (2 * kMaxACFLag)) = qnorm(0.99875) ≈ 3.023
     * - Rounded up to 3.05 for a small conservative margin
     * - Controls family-wise error rate at ≤5% across all kMaxACFLag lags
     * - Do not reduce below 3.0 or the multiple-testing protection is compromised
     *
     * ## BLOCK LENGTH SELECTION
     *
     * The block length L is computed adaptively:
     *
     * **For large series (n >= 100):** ACF-based calculation
     * ```cpp
     * logReturns = percentBarsToLogBars(rocVec / 100)
     * acf = computeACF(logReturns, maxLag=20)
     * threshold = kBonferroniZ / sqrt(n)          // Bonferroni-corrected band
     * L = minBlockL (=2) if no consecutive pair of lags exceeds threshold
     * L = last k+1 where both |rho[k]| and |rho[k+1]| exceed threshold, clamped to [2,12]
     * ```
     *
     * **For smaller series (n < 100):** n^(1/3) heuristic
     * ```cpp
     * L = max(2, floor(n^(1/3)))
     * ```
     *
     * ### Why log-returns for the ACF?
     *
     * The ACF is computed on log-returns rather than percent-ROC directly because
     * log-returns are additive and yield an unbiased ACF estimator. For the small
     * returns typical of daily financial data, log(1+r) ≈ r, so the autocorrelation
     * structures of the two series are numerically identical. The block length derived
     * from the log-return ACF is therefore fully valid for the percent-ROC bootstrap.
     *
     * ### Why the Bonferroni threshold — not 2/sqrt(n)?
     *
     * The naive ±2/√n pointwise band has a ~1.2% false-positive rate per lag. When
     * testing all kMaxACFLag=20 lags simultaneously, the family-wise false-positive
     * rate (the probability of any lag spuriously exceeding the band) is approximately
     * 1 - (1 - 0.012)^20 ≈ 22%. In practice this caused block lengths to be driven to
     * the cap (12) even for pure white-noise return series, making the bootstrap
     * artificially conservative.
     *
     * The Bonferroni threshold threshold = kBonferroniZ / sqrt(n) controls the
     * family-wise error rate at ≤5%, so a white-noise series almost never triggers a
     * spurious large block length.
     *
     * ### Why the consecutive-lag rule?
     *
     * Even with the Bonferroni threshold, a single isolated lag can occasionally
     * exceed the band by chance. The consecutive-lag rule requires both lag k and
     * lag k+1 to exceed the threshold before L is updated. Genuine autocorrelation
     * (AR, MA, GARCH volatility clustering) always produces a run of significant lags
     * starting from lag 1 and decaying monotonically. An isolated exceedance at a
     * high lag with no neighbours above the band is a noise artifact, not a real
     * dependence structure.
     *
     * ### Why L=2 minimum and not L=1 (IID)?
     *
     * Failing to reject ρ(k)=0 at all lags is not the same as proving independence.
     * Two practical reasons to keep L ≥ 2 even for apparently white-noise data:
     *
     * 1. **Test power:** With n=3000 and kBonferroniZ=3.05, autocorrelations below
     *    ~0.055 are statistically undetectable but still real. Daily returns routinely
     *    carry autocorrelations of 0.01-0.04 from microstructure effects.
     * 2. **Volatility clustering:** The ACF of raw returns being near zero is entirely
     *    consistent with significant autocorrelation in squared returns (ARCH/GARCH
     *    effects). Adjacent observations share a common variance environment even when
     *    their signs are independent. L=2 preserves same-day neighboring pairs and
     *    partially captures this structure. An IID bootstrap would destroy it entirely.
     *
     * ### Expected block lengths by sample size
     *
     * For essentially uncorrelated daily financial return data:
     * - n=50:   L=3  (n^(1/3) heuristic, ACF too noisy)
     * - n=100:  L=2  (Bonferroni threshold suppresses noise spikes)
     * - n=500:  L=2  (white noise → minimum; genuine AR → 3-8)
     * - n=1000: L=2  (white noise → minimum; genuine AR → 3-8)
     * - n=3000: L=2  (white noise → minimum; genuine AR → 3-12)
     *
     * ## WIDTH STATISTIC FUNCTIONS
     *
     * The lambda functions define how to compute widths from a resampled vector:
     *
     * **Upside Width (for a given resample):**
     * ```cpp
     * calc_upside_width = [](const vector<Decimal>& v) {
     *     median = quantile(v, 0.50);
     *     q90 = quantile(v, 0.90);
     *     width = q90 - median;
     *     return max(width, 0);  // Floor at zero
     * };
     * ```
     *
     * **Downside Width (for a given resample):**
     * ```cpp
     * calc_downside_width = [](const vector<Decimal>& v) {
     *     median = quantile(v, 0.50);
     *     q10 = quantile(v, 0.10);
     *     width = median - q10;
     *     return max(width, 0);  // Floor at zero
     * };
     * ```
     *
     * These use LinearInterpolationQuantile which:
     * - Sorts the vector (makes a copy internally)
     * - Interpolates between adjacent order statistics
     * - Handles fractional indices smoothly
     *
     * ## QUANTILE CHOICES: q10/q50/q90
     *
     * We use 10th/50th/90th percentiles rather than extremes (5th/95th) because:
     *
     * 1. **Stability:** Avoid highly variable tail estimates
     * 2. **Sample Size:** For n=100, q05 is ~5th observation (unstable)
     * 3. **Robustness:** Less sensitive to outliers
     * 4. **Coverage:** Still captures 80% of distribution
     * 5. **Conservatism:** CI itself (5th/95th of widths) adds conservatism
     *
     * The combination gives: 90% CI of 80% coverage = very robust estimates
     *
     * ## BCa BOOTSTRAP MECHANICS
     *
     * For each width statistic, the function creates a BCaBootStrap object:
     *
     * ```cpp
     * BCaBootStrap<Decimal, StationaryBlockResampler<Decimal>> bca(
     *     rocVec,              // Original data
     *     kNumResamples,       // 10,000
     *     kConfidenceLevel,    // 0.90
     *     calc_width_function, // Statistic to bootstrap
     *     blockSampler         // Resampling policy
     * );
     * ```
     *
     * This performs:
     * 1. **Resampling:** 10,000 block bootstrap samples
     * 2. **Statistic Computation:** calc_width_function on each sample
     * 3. **Bias Correction:** Compute z₀ (proportion of bootstrap stats < observed)
     * 4. **Acceleration:** Jackknife to compute 'a' (skewness correction)
     * 5. **Adjusted Percentiles:** Transform 5%/95% using BCa formula
     * 6. **Bounds Extraction:** nth_element to find adjusted percentiles
     *
     * ## RETURN VALUE STRUCTURE
     *
     * The BootstrappedWidthBounds struct contains four key values:
     *
     * ```cpp
     * return {
     *     upside_lower_bound,   // For LONG targets, SHORT stops
     *     upside_upper_bound,   // For SHORT stops, LONG (not used)
     *     downside_lower_bound, // For SHORT targets, LONG (not used)
     *     downside_upper_bound  // For LONG stops, SHORT targets
     * };
     * ```
     *
     * The calling functions select appropriate bounds:
     * - LONG: {upside_lower, downside_upper}
     * - SHORT: {downside_lower, upside_upper}
     *
     * ## ERROR HANDLING
     *
     * **Insufficient Data:**
     * ```cpp
     * if (rocVec.size() < kMinBootstrapSize)
     *     return {eps, eps, eps, eps};
     * ```
     * Returns near-zero values to signal failure without throwing.
     *
     * **Bootstrap Exception:**
     * ```cpp
     * catch (const std::exception& e) {
     *     return {eps, eps, eps, eps};
     * }
     * ```
     *
     * Possible causes:
     * - All ROC values identical (zero variance)
     * - Numerical overflow in BCa calculations
     * - Memory allocation failure
     *
     * ## PERFORMANCE PROFILE
     *
     * Typical execution time breakdown:
     * - Validation and setup: <1%
     * - Block resampling: 10-15%
     * - Quantile computation (sorting): 40-50%
     * - BCa jackknife: 30-40%
     * - Bounds extraction: <5%
     *
     * Total: 0.5-5 seconds depending on n
     *
     * ## THREAD SAFETY
     *
     * This function is **thread-safe** as long as:
     * - Input vector is not modified during execution
     * - Random number generation is properly seeded per thread
     * - No shared state is accessed
     *
     * Safe for parallel execution on different datasets.
     *
     * @tparam Decimal Numeric type for calculations
     *
     * @param rocVec Vector of Rate-of-Change values (typically from RocSeries)
     *               Should contain at least 30 values for reliable results.
     *
     * @return BootstrappedWidthBounds<Decimal> containing four bounds:
     *         - upside_lower_bound: Conservative estimate of upside potential
     *         - upside_upper_bound: Liberal estimate of upside potential
     *         - downside_lower_bound: Conservative estimate of downside risk
     *         - downside_upper_bound: Liberal estimate of downside risk
     *
     * @note Returns {eps, eps, eps, eps} where eps=1e-8 if:
     *       - rocVec.size() < 30
     *       - Bootstrap throws exception
     *       - Any other error occurs
     *
     * @warning This function can be computationally expensive (seconds).
     *          Not suitable for ultra-high-frequency applications without caching.
     *
     * @see BCaBootStrap Template class implementing BCa bootstrap
     * @see StationaryBlockResampler Block resampling for time series
     * @see LinearInterpolationQuantile Quantile computation with interpolation
     */
    template <class Decimal>
    BootstrappedWidthBounds<Decimal>
    ComputeBootstrappedWidths(const std::vector<Decimal>& rocVec)
    {
      // -----------------------------------------------------------------------
      // Configuration
      // -----------------------------------------------------------------------

      // Minimum sample size for a stable bootstrap (CLT / BCa reliability).
      constexpr size_t kMinBootstrapSize = 30;

      // Number of bootstrap resamples.  10 000 gives very stable CI estimates.
      constexpr unsigned int kNumResamples = 10000;

      /**
       * @brief Bootstrap Confidence Level (0.90 Two-Sided)
       *
       * ARCHITECTURE NOTE:
       * We use a 90% TWO-SIDED confidence interval here, which mathematically
       * yields the exact same lower bound as a 95% ONE-SIDED interval.
       *
       * 1. The Math (Tail Equivalence):
       * - A 90% Two-Sided CI excludes 10% of the distribution. This error is
       * split evenly: 5% in the left tail, 5% in the right tail. The bounds
       * sit exactly at the 5th and 95th percentiles.
       * - A 95% One-Sided CI excludes 5% of the distribution, entirely in one
       * tail. The bound sits exactly at the 5th (or 95th) percentile.
       *
       * 2. The Performance Justification (O(n^2) Avoidance):
       * - The calling functions (Long and Short strategy generators) require
       * both the 5th percentile (conservative target/stop) AND the 95th
       * percentile (liberal target/stop) from the same distribution.
       * - The BCa jackknife loop is computationally expensive.
       * - By running a single 90% TWO-SIDED bootstrap, the engine calculates
       * both the 5th and 95th percentiles simultaneously in one pass.
       * - If we used ONE-SIDED intervals, we would have to invoke the BCa
       * engine four separate times instead of two, doubling the runtime
       * cost with zero mathematical benefit.
       */
      constexpr double kConfidenceLevel = 0.90;

      // ACF lag range.  kMaxACFLag must match M used in the Bonferroni threshold.
      constexpr std::size_t  kMaxACFLag = 20;
      constexpr unsigned int kMinBlockL  = 2;
      constexpr unsigned int kMaxBlockL  = 12;

      // Bonferroni z-critical: α=0.05 family-wise, M=kMaxACFLag two-sided tests.
      //   z = qnorm(1 − 0.05 / (2 * 20)) = qnorm(0.99875) ≈ 3.023
      // Rounded up to 3.05 for a small conservative margin.
      constexpr double kBonferroniZ = 3.05;

      // Sentinel for degenerate / insufficient-data returns.
      const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-8");

      // -----------------------------------------------------------------------
      // Guard: need enough data for a meaningful bootstrap
      // -----------------------------------------------------------------------
      if (rocVec.size() < kMinBootstrapSize)
        return {eps, eps, eps, eps};

      // -----------------------------------------------------------------------
      // Width statistics (lambdas passed to BCaBootStrap)
      // -----------------------------------------------------------------------

      using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

      // Upside width = q90 − q50  (profit potential; used for LONG targets)
      StatFn calc_upside_width = [](const std::vector<Decimal>& v) -> Decimal {
        if (v.size() < 2) return DecimalConstants<Decimal>::DecimalZero;
        Decimal med   = LinearInterpolationQuantile(v, 0.50);
        Decimal q90   = LinearInterpolationQuantile(v, 0.90);
        Decimal width = q90 - med;
        return (width < DecimalConstants<Decimal>::DecimalZero)
               ? DecimalConstants<Decimal>::DecimalZero : width;
      };

      // Downside width = q50 − q10  (risk exposure; used for LONG stops)
      StatFn calc_downside_width = [](const std::vector<Decimal>& v) -> Decimal {
        if (v.size() < 2) return DecimalConstants<Decimal>::DecimalZero;
        Decimal med   = LinearInterpolationQuantile(v, 0.50);
        Decimal q10   = LinearInterpolationQuantile(v, 0.10);
        Decimal width = med - q10;
        return (width < DecimalConstants<Decimal>::DecimalZero)
               ? DecimalConstants<Decimal>::DecimalZero : width;
      };

      // -----------------------------------------------------------------------
      // Block length selection
      // -----------------------------------------------------------------------
      const size_t n = rocVec.size();
      size_t L;

      std::cout << "[BootStrapIndicators] Block Length Calculation for n=" << n
                << " observations:\n";

      if (n >= 100)
      {
        std::cout << "  Method: ACF-based (n >= 100)\n";
        try
        {
          const std::size_t maxACFLag = std::min<std::size_t>(kMaxACFLag, n - 1);

          // --- Convert percent-ROC → decimal → log-returns for ACF ---
          // Log-returns share the same autocorrelation structure as percent-ROC
          // for small returns and are the natural input for an unbiased ACF
          // estimator.  The block length derived here is applied to the
          // bootstrap of the original percent-ROC values.
          std::vector<Decimal> decimalReturns;
          decimalReturns.reserve(n);
          const Decimal hundred = DecimalConstants<Decimal>::createDecimal("100.0");
          for (const auto& roc_pct : rocVec)
            decimalReturns.push_back(roc_pct / hundred);

          auto logReturns = StatUtils<Decimal>::percentBarsToLogBars(decimalReturns);
          const auto acf  = StatUtils<Decimal>::computeACF(logReturns, maxACFLag);

          // --- Bonferroni-corrected significance threshold ---
          // Controls family-wise error rate at <=5% across all M=kMaxACFLag lags.
          // This eliminates the ~22% spurious-large-L rate of the raw 2/sqrt(n)
          // pointwise band when testing 20 lags simultaneously.
          const double threshold =
              kBonferroniZ / std::sqrt(static_cast<double>(n));

          // --- Consecutive-lag rule (secondary guard) ---
          // Update L only when two *adjacent* lags both exceed the Bonferroni
          // band.  Combined with the corrected threshold, the overall false-
          // positive probability is negligible.  Genuine autocorrelation always
          // produces runs starting at lag 1, not isolated high-lag pairs.
          unsigned int L_acf = kMinBlockL;   // default: minimum for white noise
          for (std::size_t k = 1; k + 1 < acf.size(); ++k)
          {
            const double rk  = std::fabs(acf[k    ].getAsDouble());
            const double rk1 = std::fabs(acf[k + 1].getAsDouble());
            if (rk > threshold && rk1 > threshold)
              L_acf = static_cast<unsigned int>(k + 1);
          }
          L_acf = std::max(kMinBlockL, std::min(kMaxBlockL, L_acf));
          L     = static_cast<std::size_t>(L_acf);

          // --- Diagnostics ---
          std::cout << "  Bonferroni threshold (" << kBonferroniZ << "/sqrt(n)): "
                    << threshold << "\n";
          std::cout << "  (z=" << kBonferroniZ << ", M=" << maxACFLag
                    << " lags, FWER=5%)\n";
          std::cout << "  ACF analysis: maxLag=" << maxACFLag
                    << ", block range=[" << kMinBlockL << "," << kMaxBlockL << "]\n";
          std::cout << "  ACF values:\n";
          for (std::size_t i = 0; i < acf.size(); ++i)
          {
            const double av = std::fabs(acf[i].getAsDouble());
            std::cout << "    rho[" << i << "] = " << acf[i]
                      << (i > 0 && av > threshold ? "  *** above Bonferroni band" : "")
                      << "\n";
          }
          std::cout << "  ACF-suggested block length: L=" << L << "\n";
        }
        catch (const std::exception& e)
        {
          // Fallback: n^(1/3) heuristic (Politis & White, 2004).
          L = std::max<size_t>(
                2, static_cast<size_t>(
                     std::pow(static_cast<double>(n), 1.0 / 3.0)));
          std::cout << "  ACF calculation failed (" << e.what() << ")\n";
          std::cout << "  Fallback to n^(1/3) heuristic: L=" << L << "\n";
        }
      }
      else
      {
        // For small series the ACF is too noisy; use the n^(1/3) heuristic.
        std::cout << "  Method: n^(1/3) heuristic (n < 100)\n";
        L = std::max<size_t>(
              2, static_cast<size_t>(
                   std::pow(static_cast<double>(n), 1.0 / 3.0)));
        std::cout << "  Calculated block length: L=" << L << "\n";
      }

      std::cout << "  Final block length used: L=" << L << "\n\n";

      // -----------------------------------------------------------------------
      // Bootstrap
      // -----------------------------------------------------------------------
      StationaryBlockResampler<Decimal> blockSampler(L);

      // 1. Define the Executor type (<0> dynamically uses std::thread::hardware_concurrency)
      using ThreadPool = concurrency::ThreadPoolExecutor<0>;

      // 2. Instantiate a single shared pool to prevent thread allocation thrashing.
      //    Both bca_up and bca_down are evaluated sequentially (lazy evaluation
      //    triggers on the first getLowerBound() / getUpperBound() call), so the
      //    pool is never accessed by both bootstraps concurrently.  Sharing the
      //    pool eliminates the overhead of constructing and destroying a second
      //    fixed-size thread pool for the downside bootstrap.
      auto sharedExecutor = std::make_shared<ThreadPool>();

      try
      {
        // 3. Fully qualify the BCaBootStrap template to reach the 6th parameter.
        //    Params 3–5 (Rng, Provider, SampleType) are spelled out explicitly
        //    because C++ has no way to skip defaulted template parameters when
        //    specifying a later one.
        using ParallelBCa = BCaBootStrap<
            Decimal,
            StationaryBlockResampler<Decimal>,
            randutils::mt19937_rng,              // Param 3: Rng  (default)
            void,                                // Param 4: Provider (default)
            Decimal,                             // Param 5: SampleType (default)
            ThreadPool>;                         // Param 6: concurrent executor

        // 4. Instantiate with the interval type and the injected shared pool.
        //    Constructor used: custom-statistic + custom-sampler overload
        //    (BiasCorrectedBootstrap.h, lines 1062-1083).
        ParallelBCa bca_up(
            rocVec, kNumResamples, kConfidenceLevel,
            calc_upside_width, blockSampler,
            palvalidator::analysis::IntervalType::TWO_SIDED,
            sharedExecutor);

        ParallelBCa bca_down(
            rocVec, kNumResamples, kConfidenceLevel,
            calc_downside_width, blockSampler,
            palvalidator::analysis::IntervalType::TWO_SIDED,
            sharedExecutor);

        return {
          bca_up.getLowerBound(),
          bca_up.getUpperBound(),
          bca_down.getLowerBound(),
          bca_down.getUpperBound()
        };
      }
      catch (const std::exception&)
      {
        return {eps, eps, eps, eps};
      }
    }
  } // namespace detail

  /**
   * @brief Computes robust LONG-side profit target and stop widths using BCa bootstrap.
   *
   * This function calculates statistically robust stop-loss and profit-target widths
   * for LONG (buy) positions using a sophisticated second-order bootstrap approach.
   * Rather than computing simple historical quantiles (which can overfit), it bootstraps
   * the **distribution of width statistics** to account for sampling uncertainty and
   * provide conservative, reliable estimates.
   *
   * ## METHODOLOGY OVERVIEW
   *
   * The function performs a **meta-bootstrap** in three stages:
   *
   * 1. **Data Preparation:**
   *    - Computes Rate-of-Change (ROC) over the specified period
   *    - ROC(t) = ((Close(t) / Close(t-period)) - 1) × 100
   *    - Provides percentage-normalized returns suitable for bootstrap
   *
   * 2. **Width Definition:**
   *    - Upside Width = q90 - q50 (distance from median to 90th percentile)
   *    - Downside Width = q50 - q10 (distance from 10th percentile to median)
   *    - These represent typical profit potential and risk exposure
   *
   * 3. **Bootstrap Analysis:**
   *    - Creates 10,000 resampled datasets using stationary block bootstrap
   *    - Computes upside/downside widths for each resample
   *    - Builds distributions of these widths (not just single values)
   *    - Uses BCa (Bias-Corrected and Accelerated) method for confidence intervals
   *
   * ## WHY THIS APPROACH IS SUPERIOR
   *
   * Traditional Method (prone to overfitting):
   *   ROC data → compute q90-q50 once → use this single number
   *
   * Bootstrap Method (robust):
   *   ROC data → resample 10,000 times → 10,000 width estimates →
   *   distribution of widths → 90% confidence interval on width
   *
   * This answers: "What width can I be confident about, accounting for
   * sampling uncertainty?" rather than "What width did I observe in this
   * particular dataset?"
   *
   * ## STATIONARY BLOCK BOOTSTRAP
   *
   * Financial time series exhibit autocorrelation (momentum, mean reversion,
   * volatility clustering). Standard bootstrap assumes independence and fails
   * for such data. This implementation uses Stationary Block Bootstrap
   * (Politis & Romano, 1994) which:
   *
   * - Resamples contiguous blocks of observations
   * - Block length L chosen adaptively:
   *   * ACF-based for n≥100 (captures actual dependencies)
   *   * n^(1/3) heuristic for n<100 (Politis & White, 2004)
   * - Preserves short-term dependencies within blocks
   * - Provides valid inference for time series data
   *
   * ## BCa CONFIDENCE INTERVALS
   *
   * The Bias-Corrected and Accelerated (BCa) bootstrap (Efron, 1987) improves
   * on standard percentile bootstrap by correcting for:
   *
   * 1. **Bias Correction (z₀):** Accounts for median bias in bootstrap distribution
   * 2. **Acceleration (a):** Accounts for skewness via jackknife estimation
   *
   * This provides more accurate coverage probabilities, especially for
   * skewed distributions (common in financial returns).
   *
   * ## CONSERVATIVE ASYMMETRIC BOUNDS
   *
   * The function uses a 90% confidence interval (5th to 95th percentiles) but
   * applies them asymmetrically for robustness:
   *
   * For LONG positions:
   * - **Profit Target Width** = 5th percentile of upside width distribution
   *   → Conservative estimate of achievable profit (lower bound)
   *   → We're 90% confident profit potential is AT LEAST this much
   *
   * - **Stop Loss Width** = 95th percentile of downside width distribution
   *   → Conservative estimate of required risk tolerance (upper bound)
   *   → We're 90% confident risk exposure is AT MOST this much
   *
   * This ensures:
   * - Targets are realistic and achievable (not overly optimistic)
   * - Stops are sufficiently wide (won't be prematurely triggered)
   *
   * ## QUANTILE SELECTION RATIONALE
   *
   * Uses q10/q50/q90 rather than extremes (q5/q95):
   * - Avoids unstable tail estimates
   * - More robust to outliers
   * - Still captures bulk of distribution
   * - Confidence interval provides additional conservatism
   *
   * ## MINIMUM DATA REQUIREMENTS
   *
   * - Absolute minimum: 30 ROC values (enforced by kMinBootstrapSize)
   * - Practical minimum: period + 30 + period ≈ 2×period + 30 bars
   * - Example: period=20 requires ≥70 price bars
   * - Returns epsilon (1e-8) values if insufficient data
   *
   * ## COMPUTATIONAL COMPLEXITY
   *
   * - Time: O(B × n log n) where B=10,000 resamples, n=sample size
   * - Space: O(n² + B) dominated by BCa jackknife calculation
   * - Typical runtime: 0.5-5 seconds for 100-1000 bars on modern CPU
   *
   * ## PARAMETER GUIDANCE
   *
   * **Period Selection:**
   * - 5-10:   Short-term (day trading, scalping)
   * - 10-20:  Medium-term (swing trading) - RECOMMENDED DEFAULT
   * - 20-60:  Long-term (position trading)
   * - >60:    Strategic allocation
   *
   * Align period with your trading timeframe. Longer periods yield:
   * - Smoother estimates
   * - Larger width values
   * - Fewer observations (require more data)
   *
   * @tparam Decimal Numeric type (e.g., num::DefaultNumber, double)
   *
   * @param series The OHLC time series to analyze. Must contain at least
   *               (2×period + 30) entries for reliable results.
   *
   * @param period The lookback period for ROC calculation. Typical values:
   *               10 (short-term), 20 (medium-term), 50 (long-term).
   *               Must be positive and significantly less than series length.
   *
   * @return std::pair<Decimal, Decimal> containing {profit_width, stop_width}
   *         Both values are in decimal form (0.05 = 5%). These represent:
   *         - profit_width: Conservative estimate of achievable profit potential
   *         - stop_width: Conservative estimate of necessary risk tolerance
   *
   * @throws std::domain_error If series has fewer than 3 entries
   * @throws std::domain_error If ROC series has fewer than 3 entries (after period offset)
   *
   * @note Returns {eps, eps} where eps=1e-8 in degenerate cases:
   *       - Insufficient data (< 30 ROC values)
   *       - Bootstrap failure (all identical values, numerical issues)
   *       - Non-positive width results
   *
   * ## USAGE EXAMPLE
   *
   * @code
   * // Load daily price data
   * OHLCTimeSeries<num::DefaultNumber> prices = loadOHLCData("AAPL.csv");
   *
   * // Compute stop and target widths using 10-period ROC
   * auto [target_width, stop_width] =
   *     ComputeBootStrappedLongStopAndTarget(prices, 10);
   *
   * // Validate results (check for degenerate epsilon returns)
   * const Decimal min_viable = Decimal(0.001); // 0.1%
   * if (target_width < min_viable || stop_width < min_viable) {
   *     std::cerr << "Warning: Insufficient data for reliable bootstrap" << std::endl;
   *     // Fall back to default risk management or gather more data
   * }
   *
   * // Convert widths to actual price levels
   * Decimal entry_price = prices.getLastEntry().getCloseValue();
   * Decimal target_price = entry_price * (Decimal(1.0) + target_width);
   * Decimal stop_price = entry_price * (Decimal(1.0) - stop_width);
   *
   * std::cout << "Entry: $" << entry_price << std::endl;
   * std::cout << "Target: $" << target_price
   *           << " (+" << (target_width * 100) << "%)" << std::endl;
   * std::cout << "Stop: $" << stop_price
   *           << " (-" << (stop_width * 100) << "%)" << std::endl;
   * std::cout << "Risk/Reward: " << (target_width / stop_width) << std::endl;
   * @endcode
   *
   * ## TYPICAL OUTPUT INTERPRETATION
   *
   * Example result for 10-period ROC on daily data:
   *   profit_width = 0.03 (3.0%)
   *   stop_width = 0.045 (4.5%)
   *   Risk/Reward = 0.67
   *
   * Interpretation:
   * - Conservative profit target is 3.0% above entry
   * - Conservative stop loss is 4.5% below entry
   * - Risking $1.50 to make $1.00 (slightly unfavorable, but robust)
   * - These are 90% confidence bounds, not point estimates
   *
   * ## ADVANCED: POSITION SIZING APPLICATION
   *
   * @code
   * Decimal account_equity = Decimal(100000.0);
   * Decimal max_risk_pct = Decimal(0.02); // Risk 2% per trade
   *
   * auto [target, stop] = ComputeBootStrappedLongStopAndTarget(series, 20);
   *
   * Decimal entry_price = series.getLastEntry().getCloseValue();
   * Decimal dollars_at_risk_per_share = entry_price * stop;
   * Decimal max_dollar_risk = account_equity * max_risk_pct;
   *
   * // Calculate position size
   * Decimal shares = max_dollar_risk / dollars_at_risk_per_share;
   *
   * std::cout << "Position size: " << shares << " shares" << std::endl;
   * std::cout << "Max loss: $" << max_dollar_risk << std::endl;
   * @endcode
   *
   * ## KNOWN LIMITATIONS AND CAVEATS
   *
   * 1. **Stationarity Assumption:** Assumes recent price behavior is representative
   *    of future behavior. May fail during regime changes (crashes, policy shifts).
   *    Mitigation: Use recent data (6-12 months) or regime-detection methods.
   *
   * 2. **Block Bootstrap Limitations:** While better than IID bootstrap, still
   *    assumes stationarity within blocks. Very long-range dependencies may not
   *    be fully captured.
   *
   * 3. **Computational Cost:** 10,000 resamples with BCa jackknife is expensive.
   *    Not suitable for ultra-high-frequency applications without optimization.
   *
   * 4. **Parameter Sensitivity:** Results depend on period selection. No single
   *    "correct" period exists. Test multiple periods or use ensemble methods.
   *
   * 5. **No Market Microstructure:** Ignores bid-ask spreads, slippage, liquidity.
   *    Add appropriate margins for real-world execution.
   *
   * ## VALIDATION AND TESTING
   *
   * Before deploying in production:
   * 1. Verify sufficient data (≥ 2×period + 30 bars)
   * 2. Check for epsilon returns (indicates failure)
   * 3. Validate widths are reasonable (0.5% - 50% for most assets)
   * 4. Backtest on out-of-sample data
   * 5. Monitor in paper trading before live deployment
   *
   * ## CONFIGURATION CONSTANTS (in ComputeBootstrappedWidths)
   *
   * Current settings:
   * - kMinBootstrapSize = 30 (minimum ROC observations)
   * - kNumResamples = 10,000 (bootstrap iterations)
   * - kConfidenceLevel = 0.90 (90% CI)
   * - Block length L: ACF-based for n≥100, n^(1/3) for n<100 (automatic)
   *
   * These are well-validated defaults. Modify only if you have specific
   * requirements and understand the statistical implications.
   *
   * ## REFERENCES
   *
   * - Efron, B. (1987). "Better Bootstrap Confidence Intervals."
   *   Journal of the American Statistical Association, 82(397), 171-185.
   *
   * - Politis, D.N., & Romano, J.P. (1994). "The Stationary Bootstrap."
   *   Journal of the American Statistical Association, 89(428), 1303-1313.
   *
   * - Politis, D.N., & White, H. (2004). "Automatic Block-Length Selection
   *   for the Dependent Bootstrap." Econometric Reviews, 23(1), 53-70.
   *
   * @see ComputeBootStrappedShortStopAndTarget For SHORT position equivalent
   * @see detail::ComputeBootstrappedWidths Internal bootstrap implementation
   * @see BCaBootStrap BCa bootstrap class (BiasCorrectedBootstrap.h)
   * @see StationaryBlockResampler Block bootstrap implementation
   * @see RocSeries Rate-of-change calculation (TimeSeriesIndicators.h)
   */
  template <typename Decimal>
  std::pair<Decimal, Decimal>
  ComputeBootStrappedLongStopAndTarget(const OHLCTimeSeries<Decimal>& series,
                                     uint32_t period)
  {
    if (series.getNumEntries() < 3)
      throw std::domain_error("ComputeBootStrappedLongStopAndTarget: input series too small");

    // RocSeries is in TimeSeriesIndicators.h
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec    = rocSeries.getTimeSeriesAsVector();
    if (rocVec.size() < 3)
      throw std::domain_error("ComputeBootStrappedLongStopAndTarget: ROC series too small");

    // Get all 4 bounds from the bootstrap helper
    auto bounds = detail::ComputeBootstrappedWidths<Decimal>(rocVec);

    // For LONG trades:
    // Profit = Conservative Upside = Lower Bound of Upside Width
    // Stop   = Conservative Downside = Upper Bound of Downside Width
    Decimal profitWidth = bounds.upside_lower_bound;
    Decimal stopWidth   = bounds.downside_upper_bound;

    // Ensure non-degenerate widths
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-8");
    if (profitWidth <= DecimalConstants<Decimal>::DecimalZero)
      profitWidth = eps;
    if (stopWidth   <= DecimalConstants<Decimal>::DecimalZero)
      stopWidth   = eps;

    return {profitWidth, stopWidth};
  }

  /**
   * @brief Computes robust SHORT-side profit target and stop widths using BCa bootstrap.
   *
   * This function calculates statistically robust stop-loss and profit-target widths
   * for SHORT (sell) positions using the same sophisticated bootstrap methodology as
   * ComputeBootStrappedLongStopAndTarget, but with **inverted application** of the
   * width distributions appropriate for short selling.
   *
   * ## KEY DIFFERENCE FROM LONG VERSION
   *
   * The bootstrap process is identical (same ROC calculation, same width definitions,
   * same statistical methodology), but the **interpretation and assignment** of widths
   * is reversed to match short position mechanics:
   *
   * **For SHORT positions:**
   * - **Profit Target Width** = 5th percentile of DOWNSIDE width distribution
   *   → Profit comes from price decline
   *   → Conservative estimate of achievable downside movement
   *
   * - **Stop Loss Width** = 95th percentile of UPSIDE width distribution
   *   → Risk comes from price increase
   *   → Conservative estimate of potential adverse movement
   *
   * **For LONG positions (for comparison):**
   * - Profit Target Width = 5th percentile of UPSIDE width distribution
   * - Stop Loss Width = 95th percentile of DOWNSIDE width distribution
   *
   * ## WHY THE INVERSION IS CORRECT
   *
   * When shorting:
   * 1. **Entry:** Sell at current price (e.g., $100)
   * 2. **Profit Target:** Buy back BELOW entry (e.g., $95 = -5% downside)
   * 3. **Stop Loss:** Buy back ABOVE entry (e.g., $105 = +5% upside)
   *
   * The downside width (median - q10) represents typical downward movement,
   * which is profit for shorts. The upside width (q90 - median) represents
   * typical upward movement, which is risk for shorts.
   *
   * ## IDENTICAL BOOTSTRAP METHODOLOGY
   *
   * This function uses the exact same statistical approach as the LONG version:
   *
   * 1. Computes Rate-of-Change (ROC) series over specified period
   * 2. Defines upside width = q90 - q50, downside width = q50 - q10
   * 3. Bootstraps width distributions using stationary block resampling
   * 4. Applies BCa corrections for bias and skewness
   * 5. Extracts 90% confidence interval (5th to 95th percentiles)
   *
   * The only difference is which bounds are used for target vs. stop.
   *
   * ## CONSERVATIVE ASYMMETRIC BOUNDS (SHORT VERSION)
   *
   * For SHORT positions, we want:
   * - **Realistic profit targets:** Use lower bound of downside width
   *   → Don't expect more downside than we can be confident about
   *
   * - **Wide enough stops:** Use upper bound of upside width
   *   → Protect against larger-than-typical rallies
   *
   * This ensures short positions are:
   * - Not chasing unrealistic downside targets
   * - Protected against adverse rallies with adequate room
   *
   * ## SYMMETRY VS. ASYMMETRY IN RETURNS
   *
   * Important note on return asymmetry in short selling:
   *
   * LONG position:
   * - Max gain: Unlimited (price → ∞)
   * - Max loss: 100% (price → 0)
   *
   * SHORT position:
   * - Max gain: 100% (price → 0)
   * - Max loss: Unlimited (price → ∞)
   *
   * However, for practical stop/target setting over typical timeframes
   * (period = 5-60), this asymmetry is negligible. The bootstrap widths
   * already account for the actual distribution observed in the data.
   *
   * For very long holding periods or extreme volatility, consider:
   * - Adjusting stop_width upward for shorts (more conservative)
   * - Using options or other defined-risk strategies
   *
   * ## TYPICAL USAGE PATTERN
   *
   * @code
   * // Load price data
   * OHLCTimeSeries<num::DefaultNumber> prices = loadOHLCData("TSLA.csv");
   *
   * // Compute stop and target for SHORT position
   * auto [target_width, stop_width] =
   *     ComputeBootStrappedShortStopAndTarget(prices, 15);
   *
   * // Convert to actual price levels for short entry
   * Decimal entry_price = prices.getLastEntry().getCloseValue();
   *
   * // For shorts: target is BELOW entry, stop is ABOVE entry
   * Decimal target_price = entry_price * (Decimal(1.0) - target_width);
   * Decimal stop_price = entry_price * (Decimal(1.0) + stop_width);
   *
   * std::cout << "SHORT Entry: $" << entry_price << std::endl;
   * std::cout << "Target (buy back): $" << target_price
   *           << " (-" << (target_width * 100) << "%)" << std::endl;
   * std::cout << "Stop (buy back): $" << stop_price
   *           << " (+" << (stop_width * 100) << "%)" << std::endl;
   * @endcode
   *
   * ## COMPARISON: LONG VS SHORT ON SAME DATA
   *
   * For the same asset and period, you might observe:
   *
   * @code
   * auto [long_target, long_stop] =
   *     ComputeBootStrappedLongStopAndTarget(prices, 20);
   * auto [short_target, short_stop] =
   *     ComputeBootStrappedShortStopAndTarget(prices, 20);
   *
   * // Example output:
   * // LONG:  target = 3.5%, stop = 4.2%
   * // SHORT: target = 3.8%, stop = 4.5%
   * @endcode
   *
   * Observations:
   * - Values are similar but not identical (different CI bounds)
   * - SHORT widths often slightly larger (reflects return asymmetry)
   * - Both are conservative estimates from same underlying distribution
   *
   * ## DIRECTIONAL BIAS CONSIDERATIONS
   *
   * If the underlying asset has a strong directional bias (e.g., equity indices
   * have long-term upward drift), the width distributions will reflect this:
   *
   * - **Upward-biased asset:** Upside widths > downside widths
   *   → LONG positions: favorable risk/reward
   *   → SHORT positions: unfavorable risk/reward
   *
   * - **Downward-biased asset:** Downside widths > upside widths
   *   → SHORT positions: favorable risk/reward
   *   → LONG positions: unfavorable risk/reward
   *
   * The bootstrap automatically captures this from the data. You don't need to
   * manually adjust for bias—it's embedded in the width distributions.
   *
   * ## RISK MANAGEMENT FOR SHORT POSITIONS
   *
   * Additional considerations when using these widths for short selling:
   *
   * 1. **Margin Requirements:** Factor in broker margin requirements and
   *    interest costs (not captured by stop/target widths).
   *
   * 2. **Dividend Risk:** Short sellers pay dividends. Adjust target downward
   *    or avoid shorts around ex-dividend dates.
   *
   * 3. **Borrow Costs:** Hard-to-borrow stocks have significant costs that
   *    eat into profit targets.
   *
   * 4. **Short Squeeze Risk:** Low-float stocks with high short interest can
   *    experience violent squeezes. Consider wider stops.
   *
   * 5. **Regulatory Risk:** Short sale restrictions (uptick rule, SSR) can
   *    make execution difficult.
   *
   * ## POSITION SIZING FOR SHORTS
   *
   * @code
   * Decimal account_equity = Decimal(100000.0);
   * Decimal max_risk_pct = Decimal(0.015); // 1.5% for shorts (more conservative)
   *
   * auto [target, stop] = ComputeBootStrappedShortStopAndTarget(series, 20);
   *
   * Decimal entry_price = series.getLastEntry().getCloseValue();
   * Decimal dollars_at_risk_per_share = entry_price * stop; // Stop is upside for shorts
   * Decimal max_dollar_risk = account_equity * max_risk_pct;
   *
   * // Calculate short position size
   * Decimal shares = max_dollar_risk / dollars_at_risk_per_share;
   *
   * std::cout << "SHORT size: " << shares << " shares" << std::endl;
   * std::cout << "Entry value: $" << (shares * entry_price) << std::endl;
   * std::cout << "Max loss: $" << max_dollar_risk << std::endl;
   * @endcode
   *
   * Note: Many traders use smaller position sizes for shorts (1.5% vs 2% risk)
   * due to unlimited loss potential and typically lower win rates.
   *
   * @tparam Decimal Numeric type (e.g., num::DefaultNumber, double)
   *
   * @param series The OHLC time series to analyze. Must contain at least
   *               (2×period + 30) entries for reliable results.
   *
   * @param period The lookback period for ROC calculation. Same considerations
   *               as LONG version. Typical values: 10-20 for swing trading.
   *
   * @return std::pair<Decimal, Decimal> containing {profit_width, stop_width}
   *         Both values are in decimal form (0.04 = 4%). For SHORT positions:
   *         - profit_width: Conservative estimate of achievable downside (profit)
   *         - stop_width: Conservative estimate of potential upside (risk)
   *
   * @throws std::domain_error If series has fewer than 3 entries
   * @throws std::domain_error If ROC series has fewer than 3 entries
   *
   * @note Returns {eps, eps} where eps=1e-8 in degenerate cases (same as LONG version)
   *
   * ## DATA REQUIREMENTS
   *
   * Identical to LONG version:
   * - Minimum 30 ROC values after period offset
   * - Practical minimum: ~70 bars for period=20
   * - More data = more reliable estimates
   * - Use recent data (6-12 months) for relevance
   *
   * ## COMPUTATIONAL COST
   *
   * Identical to LONG version:
   * - 10,000 bootstrap resamples
   * - BCa bias correction and acceleration
   * - Typical runtime: 0.5-5 seconds
   *
   * ## VALIDATION CHECKLIST
   *
   * Before using SHORT widths in production:
   *
   * 1. ✓ Sufficient data (check series length)
   * 2. ✓ Non-degenerate results (widths > 0.001)
   * 3. ✓ Reasonable values (0.5% - 50% for most assets)
   * 4. ✓ Backtest with proper short mechanics (dividends, borrow costs)
   * 5. ✓ Paper trade before live deployment
   * 6. ✓ Monitor actual stop/target hit rates
   * 7. ✓ Consider asset-specific risks (squeeze, hard-to-borrow)
   *
   * ## DEBUGGING COMMON ISSUES
   *
   * **Issue:** Stop width >> target width (e.g., stop=10%, target=2%)
   * **Cause:** Asset has strong upward bias
   * **Action:** SHORT is fighting the trend. Consider avoiding or use wider targets.
   *
   * **Issue:** Both widths near zero (epsilon)
   * **Cause:** Insufficient data or zero variance
   * **Action:** Check series length, ensure period is appropriate, verify data quality.
   *
   * **Issue:** Extremely large widths (>50%)
   * **Cause:** High volatility asset or inappropriate period
   * **Action:** Reduce period, check for data errors, consider volatility-adjusted sizing.
   *
   * ## ADVANCED: REGIME-AWARE USAGE
   *
   * For more sophisticated implementations, consider regime detection:
   *
   * @code
   * // Detect market regime
   * bool is_bear_market = detectBearMarket(prices);
   *
   * if (is_bear_market) {
   *     // In bear markets, SHORT conditions are more favorable
   *     auto [target, stop] = ComputeBootStrappedShortStopAndTarget(prices, 15);
   *     // Potentially more aggressive: smaller stops, larger targets
   * } else {
   *     // In bull markets, LONG conditions are more favorable
   *     auto [target, stop] = ComputeBootStrappedLongStopAndTarget(prices, 15);
   * }
   * @endcode
   *
   * ## REFERENCES
   *
   * Same statistical references as LONG version:
   * - Efron (1987): BCa bootstrap methodology
   * - Politis & Romano (1994): Stationary block bootstrap
   * - Politis & White (2004): Block length selection
   *
   * Additional short selling references:
   * - Jones, C.M., & Lamont, O.A. (2002). "Short-sale constraints and stock returns."
   *   Journal of Financial Economics, 66(2-3), 207-239.
   *
   * @see ComputeBootStrappedLongStopAndTarget For LONG position equivalent
   * @see detail::ComputeBootstrappedWidths Shared bootstrap implementation
   * @see BCaBootStrap BCa bootstrap class (BiasCorrectedBootstrap.h)
   * @see StationaryBlockResampler Block bootstrap implementation
   */
  template <typename Decimal>
  std::pair<Decimal, Decimal>
  ComputeBootStrappedShortStopAndTarget(const OHLCTimeSeries<Decimal>& series,
                                      uint32_t period)
  {
    if (series.getNumEntries() < 3)
      throw std::domain_error("ComputeBootStrappedShortStopAndTarget: input series too small");

    // RocSeries is in TimeSeriesIndicators.h
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec    = rocSeries.getTimeSeriesAsVector();
    if (rocVec.size() < 3)
      throw std::domain_error("ComputeBootStrappedShortStopAndTarget: ROC series too small");

    // Get all 4 bounds from the bootstrap helper
    auto bounds = detail::ComputeBootstrappedWidths<Decimal>(rocVec);

    // For SHORT trades:
    // Profit = Conservative Downside = Lower Bound of Downside Width
    // Stop   = Conservative Upside   = Upper Bound of Upside Width
    Decimal profitWidth = bounds.downside_lower_bound;
    Decimal stopWidth   = bounds.upside_upper_bound;

    // Ensure non-degenerate widths
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-8");
    if (profitWidth <= DecimalConstants<Decimal>::DecimalZero)
      profitWidth = eps;
    if (stopWidth   <= DecimalConstants<Decimal>::DecimalZero)
      stopWidth   = eps;

    return {profitWidth, stopWidth};
  }

} // namespace mkc_timeseries

#endif // __BOOTSTRAPPED_INDICATORS_H
