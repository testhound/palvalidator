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
 * This is the statistical engine that powers both
 * ComputeBootStrappedLongStopAndTarget() and
 * ComputeBootStrappedShortStopAndTarget(). It performs the core bootstrap
 * analysis on the distribution of width statistics and returns four critical
 * bounds describing the uncertainty in upside and downside movement widths.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS FUNCTION DOES
 * ---------------------------------------------------------------------------
 *
 * 1. Validates Data
 *    - Requires sample size >= kMinBootstrapSize (30)
 *    - Returns epsilon bounds if insufficient data
 *
 * 2. Defines Width Statistics
 *    - Upside Width   = q90 - q50
 *    - Downside Width = q50 - q10
 *
 * 3. Chooses a Stationary Bootstrap Block Length
 *    - For n >= 100: uses a smooth ACF dependence-mass estimator
 *    - For n < 100:  uses the n^(1/3) heuristic
 *
 * 4. Runs Two Separate BCa Bootstraps
 *    - One bootstrap for upside width
 *    - One bootstrap for downside width
 *    - Each uses 10,000 resamples and a 90% confidence level
 *
 * 5. Returns Four Bounds
 *    - upside_lower_bound
 *    - upside_upper_bound
 *    - downside_lower_bound
 *    - downside_upper_bound
 *
 * ---------------------------------------------------------------------------
 * WHY SEPARATE BOOTSTRAPS?
 * ---------------------------------------------------------------------------
 *
 * Upside and downside widths often have different variability, skewness, and
 * sampling uncertainty. Running separate BCa bootstraps provides:
 *
 * - independent uncertainty quantification
 * - asymmetric treatment of upside and downside risk
 * - flexibility for LONG and SHORT applications
 *
 * ---------------------------------------------------------------------------
 * CONFIGURATION CONSTANTS
 * ---------------------------------------------------------------------------
 *
 * constexpr size_t kMinBootstrapSize = 30;
 * constexpr unsigned int kNumResamples = 10000;
 * constexpr double kConfidenceLevel = 0.90;
 * constexpr std::size_t kMaxACFLag = 20;
 * constexpr unsigned int kMinBlockL = 2;
 * constexpr unsigned int kMaxBlockL = 12;
 *
 * kMinBootstrapSize = 30
 * - Minimum sample size for stable BCa inference
 * - Below this, bootstrap and jackknife corrections become unreliable
 *
 * kNumResamples = 10,000
 * - Produces very stable confidence interval estimates
 * - More resamples = slower runtime, less Monte Carlo noise
 *
 * kConfidenceLevel = 0.90
 * - Produces a 90% two-sided confidence interval
 * - Equivalent tail bounds to 95% one-sided inference
 * - Allows both lower and upper bounds to be obtained in one BCa run
 *
 * kMaxACFLag = 20
 * - Number of lags used for dependence estimation
 * - Large enough to capture short-horizon serial dependence and
 *   volatility clustering in daily data
 *
 * kMinBlockL = 2
 * - Enforces a minimum non-IID block size
 * - Preserves at least minimal local dependence structure even when
 *   the series appears close to white noise
 *
 * kMaxBlockL = 12
 * - Prevents the block length from becoming excessively large
 * - Acts as a practical upper bound for daily financial return series
 *
 * ---------------------------------------------------------------------------
 * BLOCK LENGTH SELECTION
 * ---------------------------------------------------------------------------
 *
 * The function uses a stationary block bootstrap, so a block length L must be
 * chosen. The purpose of L is to preserve local dependence in the resampled
 * series rather than treating the observations as IID.
 *
 * For n >= 100, block length is chosen using a smooth dependence-mass
 * estimator based on two dependence channels:
 *
 *   1. Raw log returns
 *   2. Absolute log returns
 *
 * The rationale is:
 *
 * - Raw log returns capture signed serial dependence such as short-term
 *   momentum, mean reversion, or microstructure effects.
 *
 * - Absolute log returns capture volatility clustering, which is often the
 *   dominant dependence structure in financial time series.
 *
 * Because this function is used to estimate profit-target and stop-loss widths,
 * preserving dependence in volatility is at least as important as preserving
 * dependence in return sign. Width statistics are driven strongly by the local
 * scale of returns, and volatility clustering directly affects that scale.
 *
 * ---------------------------------------------------------------------------
 * STEP 1: Convert ROC values to log returns
 * ---------------------------------------------------------------------------
 *
 * The input rocVec contains percent rate-of-change values. For dependence
 * analysis, the function converts:
 *
 *   percent ROC -> decimal returns -> log returns
 *
 * Log returns are used because:
 *
 * - they are additive over time
 * - they are a standard input for serial dependence analysis
 * - for small daily returns, they are numerically very close to percent returns
 *
 * The block length derived from log-return dependence is then applied to the
 * stationary bootstrap resampling of the original ROC series.
 *
 * ---------------------------------------------------------------------------
 * STEP 2: Compute two ACFs
 * ---------------------------------------------------------------------------
 *
 * The function computes:
 *
 *   acfRaw = ACF(logReturns, k = 0..K)
 *   acfAbs = ACF(abs(logReturns), k = 0..K)
 *
 * where K = min(kMaxACFLag, n - 1).
 *
 * Interpretation:
 *
 * - acfRaw measures signed-return dependence
 * - acfAbs measures volatility dependence
 *
 * In many financial series:
 *
 * - acfRaw is weak or short-lived
 * - acfAbs is stronger and more persistent
 *
 * This is expected and reflects the common empirical pattern that returns are
 * often nearly uncorrelated in sign while volatility is strongly autocorrelated.
 *
 * ---------------------------------------------------------------------------
 * STEP 3: Compute smooth dependence mass
 * ---------------------------------------------------------------------------
 *
 * For each ACF, the function computes a tapered dependence mass:
 *
 *   dependenceMass = sum_{k=1}^{K} w_k * |rho_k|
 *
 * where:
 *
 *   rho_k = autocorrelation at lag k
 *   w_k   = 1 - k / (K + 1)
 *
 * The taper has these effects:
 *
 * - lower lags receive more weight than higher lags
 * - short-horizon dependence is emphasized
 * - isolated or noisy higher-lag values contribute less
 *
 * This is intentionally smoother and more stable than a binary significance
 * test or a threshold-crossing rule.
 *
 * ---------------------------------------------------------------------------
 * STEP 4: Convert dependence mass to effective dependence horizon
 * ---------------------------------------------------------------------------
 *
 * For each dependence channel, the function computes:
 *
 *   tau = 1 + 2 * dependenceMass
 *
 * giving:
 *
 *   tauRaw = 1 + 2 * rawDependenceMass
 *   tauAbs = 1 + 2 * absDependenceMass
 *
 * These tau values are smooth proxies for effective dependence horizon.
 * Larger tau means stronger short-run dependence and therefore a larger
 * stationary bootstrap block length.
 *
 * ---------------------------------------------------------------------------
 * STEP 5: Convert tau to candidate block lengths
 * ---------------------------------------------------------------------------
 *
 * Each tau value is rounded to the nearest integer and clamped:
 *
 *   L_raw = clamp(round(tauRaw), kMinBlockL, kMaxBlockL)
 *   L_abs = clamp(round(tauAbs), kMinBlockL, kMaxBlockL)
 *
 * Final block length is then chosen as:
 *
 *   L = max(L_raw, L_abs)
 *
 * This choice is deliberate.
 *
 * - If signed-return dependence is stronger, L_raw can dominate.
 * - If volatility clustering is stronger, L_abs can dominate.
 *
 * Using max(L_raw, L_abs) ensures the bootstrap preserves whichever local
 * dependence structure is most relevant.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS METHOD IS BETTER THAN A THRESHOLD RULE
 * ---------------------------------------------------------------------------
 *
 * A threshold-based rule can be unstable:
 *
 * - a tiny change in sample length can move one lag above or below a threshold
 * - the selected block length can jump discontinuously
 * - late-lag threshold exceedances can dominate the result
 *
 * The smooth dependence-mass estimator avoids these pathologies:
 *
 * - all lags contribute gradually rather than in binary fashion
 * - nearby samples tend to produce nearby block lengths
 * - higher lags are downweighted rather than treated equally
 *
 * This produces more stable and interpretable block lengths, especially when
 * the function is rerun across nearby in-sample windows.
 *
 * ---------------------------------------------------------------------------
 * FALLBACK FOR SMALL SAMPLES
 * ---------------------------------------------------------------------------
 *
 * For n < 100, ACF-based dependence estimation is noisier and less reliable.
 * In that case the function uses the standard heuristic:
 *
 *   L = max(2, floor(n^(1/3)))
 *
 * This is a practical small-sample fallback commonly used in block-bootstrap
 * settings.
 *
 * ---------------------------------------------------------------------------
 * WHY VOLATILITY DEPENDENCE MATTERS HERE
 * ---------------------------------------------------------------------------
 *
 * This function is not trying to forecast return direction. It is trying to
 * estimate the distribution of width statistics:
 *
 * - q90 - q50
 * - q50 - q10
 *
 * These are scale-sensitive quantities. If volatility clustering is destroyed,
 * bootstrap resamples can mix quiet and turbulent periods too aggressively,
 * distorting the width distribution and therefore the resulting stop/target
 * estimates.
 *
 * Preserving volatility dependence helps keep the resampled series closer to
 * the local dispersion structure of the original series.
 *
 * ---------------------------------------------------------------------------
 * WIDTH STATISTIC FUNCTIONS
 * ---------------------------------------------------------------------------
 *
 * Upside Width:
 *
 *   q90 - q50
 *
 * - measures profit potential on the upside
 * - floored at zero
 *
 * Downside Width:
 *
 *   q50 - q10
 *
 * - measures downside risk exposure
 * - floored at zero
 *
 * Quantiles are computed with LinearInterpolationQuantile(), which sorts a copy
 * of the sample and interpolates between adjacent order statistics.
 *
 * ---------------------------------------------------------------------------
 * QUANTILE CHOICES: q10 / q50 / q90
 * ---------------------------------------------------------------------------
 *
 * The function uses q10, q50, and q90 rather than more extreme tail quantiles
 * because these are more stable in finite samples.
 *
 * Advantages:
 *
 * - more robust than q05 / q95
 * - less sensitive to outliers
 * - better behaved when sample sizes are modest
 * - still captures a wide central portion of the distribution
 *
 * The BCa confidence interval around these widths adds further conservatism.
 *
 * ---------------------------------------------------------------------------
 * BCa BOOTSTRAP MECHANICS
 * ---------------------------------------------------------------------------
 *
 * For each width statistic, the function constructs a BCaBootStrap object
 * using:
 *
 * - the original ROC series
 * - the chosen stationary block resampler
 * - the custom statistic lambda
 * - a two-sided 90% interval
 *
 * BCa improves over ordinary percentile bootstrap by correcting for:
 *
 * - bias in the bootstrap distribution
 * - skewness via jackknife acceleration
 *
 * This is especially useful for width statistics, which can be skewed and
 * asymmetric.
 *
 * ---------------------------------------------------------------------------
 * RETURN VALUE STRUCTURE
 * ---------------------------------------------------------------------------
 *
 * The returned BootstrappedWidthBounds contains:
 *
 * - upside_lower_bound
 * - upside_upper_bound
 * - downside_lower_bound
 * - downside_upper_bound
 *
 * The public LONG and SHORT helper functions then select the appropriate
 * conservative bounds:
 *
 * LONG:
 * - profit target = upside_lower_bound
 * - stop width    = downside_upper_bound
 *
 * SHORT:
 * - profit target = downside_lower_bound
 * - stop width    = upside_upper_bound
 *
 * ---------------------------------------------------------------------------
 * ERROR HANDLING
 * ---------------------------------------------------------------------------
 *
 * If the sample is too small or the bootstrap fails, the function returns:
 *
 *   {eps, eps, eps, eps}
 *
 * where:
 *
 *   eps = 1e-8
 *
 * This avoids throwing in normal degenerate-data situations and gives the
 * caller a sentinel-like near-zero result.
 *
 * Possible causes of failure include:
 *
 * - insufficient data
 * - zero-variance or degenerate samples
 * - numerical issues in BCa correction
 * - memory allocation failures
 *
 * ---------------------------------------------------------------------------
 * PERFORMANCE PROFILE
 * ---------------------------------------------------------------------------
 *
 * Typical cost components:
 *
 * - ACF computation: small
 * - block-length diagnostics: very small
 * - stationary block bootstrap resampling: moderate
 * - quantile sorting: substantial
 * - BCa jackknife acceleration: substantial
 *
 * Typical runtime is on the order of fractions of a second to a few seconds,
 * depending on sample size and hardware.
 *
 * ---------------------------------------------------------------------------
 * THREAD SAFETY
 * ---------------------------------------------------------------------------
 *
 * This function is thread-safe provided:
 *
 * - the input vector is not modified concurrently
 * - random-number generation is properly isolated per bootstrap engine
 * - no shared mutable global state is used
 *
 * The implementation uses a shared executor object only within the scope of
 * the function for sequential bootstrap calls.
 *
 * @tparam Decimal Numeric type for calculations
 *
 * @param rocVec Vector of rate-of-change values, typically from RocSeries().
 *               Should contain at least 30 observations for reliable results.
 *
 * @return BootstrappedWidthBounds<Decimal> containing:
 *         - conservative and liberal upside width bounds
 *         - conservative and liberal downside width bounds
 *
 * @note Returns {eps, eps, eps, eps} with eps = 1e-8 if:
 *       - rocVec.size() < 30
 *       - bootstrap construction or evaluation fails
 *       - numerical issues prevent valid inference
 *
 * @warning This function can be computationally expensive and is not intended
 *          for extremely high-frequency or latency-sensitive workflows without
 *          caching or batching.
 *
 * @see BCaBootStrap
 * @see StationaryBlockResampler
 * @see LinearInterpolationQuantile
 * @see ComputeBootStrappedLongStopAndTarget
 * @see ComputeBootStrappedShortStopAndTarget
 */
    template <class Decimal>
    BootstrappedWidthBounds<Decimal>
    ComputeBootstrappedWidths(const std::vector<Decimal>& rocVec)
    {
      // -----------------------------------------------------------------------
      // Configuration
      // -----------------------------------------------------------------------
 
      constexpr size_t        kMinBootstrapSize = 30;
      constexpr unsigned int  kNumResamples     = 10000;
      constexpr double        kConfidenceLevel  = 0.90;
      constexpr std::size_t   kMaxACFLag        = 20;
      constexpr unsigned int  kMinBlockL        = 2;
      constexpr unsigned int  kMaxBlockL        = 12;
 
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
 
      // Upside width = q90 - q50  (profit potential; used for LONG targets)
      StatFn calc_upside_width = [](const std::vector<Decimal>& v) -> Decimal
      {
        if (v.size() < 2)
          return DecimalConstants<Decimal>::DecimalZero;
 
        Decimal median = LinearInterpolationQuantile(v, 0.50);
        Decimal q90    = LinearInterpolationQuantile(v, 0.90);
        Decimal width  = q90 - median;
 
        return (width < DecimalConstants<Decimal>::DecimalZero)
               ? DecimalConstants<Decimal>::DecimalZero
               : width;
      };
 
      // Downside width = q50 - q10  (risk exposure; used for LONG stops)
      StatFn calc_downside_width = [](const std::vector<Decimal>& v) -> Decimal
      {
        if (v.size() < 2)
          return DecimalConstants<Decimal>::DecimalZero;
 
        Decimal median = LinearInterpolationQuantile(v, 0.50);
        Decimal q10    = LinearInterpolationQuantile(v, 0.10);
        Decimal width  = median - q10;
 
        return (width < DecimalConstants<Decimal>::DecimalZero)
               ? DecimalConstants<Decimal>::DecimalZero
               : width;
      };
 
      // -----------------------------------------------------------------------
      // Block length selection
      //
      // Convert percent-ROC to decimal returns (0.015 = +1.5%), then delegate
      // entirely to StatUtils::suggestStationaryBlockLength.  That function
      // handles the log transformation, both ACF computations, the tapered
      // Bartlett dependence-mass estimation, and the max(L_raw, L_abs)
      // combination rule.  The percent -> decimal conversion stays here because
      // it is specific to the data format of rocVec, not a concern of the
      // general-purpose block length estimator.
      // -----------------------------------------------------------------------
      const size_t n = rocVec.size();
      size_t L = kMinBlockL;
 
      std::cout << "[BootStrapIndicators] Block Length Calculation for n=" << n
                << " observations:\n";
 
      if (n >= 100)
      {
        std::cout << "  Method: smooth ACF dependence-mass estimator (n >= 100)\n";
 
        try
        {
          std::vector<Decimal> decimalReturns;
          decimalReturns.reserve(n);
 
          const Decimal hundred =
              DecimalConstants<Decimal>::createDecimal("100.0");
 
          for (const auto& roc_pct : rocVec)
            decimalReturns.push_back(roc_pct / hundred);
 
          L = StatUtils<Decimal>::suggestStationaryBlockLength(decimalReturns,
							       kMaxACFLag,
							       kMinBlockL,
							       kMaxBlockL,
							       &std::cout);
 
          std::cout << "  ACF-suggested block length: L=" << L << "\n";
        }
        catch (const std::exception& e)
        {
          L = std::max<size_t>(
                  2,
                  static_cast<size_t>(
                      std::pow(static_cast<double>(n), 1.0 / 3.0)));
 
          std::cout << "  ACF calculation failed (" << e.what() << ")\n";
          std::cout << "  Fallback to n^(1/3) heuristic: L=" << L << "\n";
        }
      }
      else
      {
        std::cout << "  Method: n^(1/3) heuristic (n < 100)\n";
 
        L = std::max<size_t>(
                2,
                static_cast<size_t>(
                    std::pow(static_cast<double>(n), 1.0 / 3.0)));
 
        std::cout << "  Calculated block length: L=" << L << "\n";
      }
 
      std::cout << "  Final block length used: L=" << L << "\n\n";
 
      // -----------------------------------------------------------------------
      // Bootstrap
      // -----------------------------------------------------------------------
      StationaryBlockResampler<Decimal> blockSampler(L);
 
      using ThreadPool = concurrency::ThreadPoolExecutor<0>;
      auto sharedExecutor = std::make_shared<ThreadPool>();
 
      try
      {
        using ParallelBCa = BCaBootStrap<
            Decimal,
            StationaryBlockResampler<Decimal>,
            randutils::mt19937_rng,
            void,
            Decimal,
            ThreadPool>;
 
        ParallelBCa bca_up(
            rocVec,
            kNumResamples,
            kConfidenceLevel,
            calc_upside_width,
            blockSampler,
            palvalidator::analysis::IntervalType::TWO_SIDED,
            sharedExecutor);
 
        ParallelBCa bca_down(
            rocVec,
            kNumResamples,
            kConfidenceLevel,
            calc_downside_width,
            blockSampler,
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
 * @brief Computes robust LONG-side profit target and stop widths using BCa
 *        bootstrap with adaptive dependence-preserving block resampling.
 *
 * This function calculates statistically robust stop-loss and profit-target
 * widths for LONG (buy) positions using a second-order bootstrap procedure.
 * Rather than relying on a single historical quantile estimate, it bootstraps
 * the distribution of width statistics to account for sampling uncertainty and
 * local time-series dependence.
 *
 * ---------------------------------------------------------------------------
 * METHODOLOGY OVERVIEW
 * ---------------------------------------------------------------------------
 *
 * The function proceeds in three stages:
 *
 * 1. Data Preparation
 *    - Computes a Rate-of-Change (ROC) series over the requested period
 *    - ROC(t) = ((Close(t) / Close(t - period)) - 1) * 100
 *
 * 2. Width Definition
 *    - Upside Width   = q90 - q50
 *    - Downside Width = q50 - q10
 *
 * 3. Bootstrap Inference
 *    - Uses stationary block bootstrap to preserve local dependence
 *    - Uses BCa (Bias-Corrected and Accelerated) intervals to account for
 *      bias and skewness in the bootstrap distribution
 *
 * The result is a pair of conservative LONG-side widths:
 *
 * - profit target width
 * - stop-loss width
 *
 * ---------------------------------------------------------------------------
 * WHY THIS APPROACH IS MORE ROBUST
 * ---------------------------------------------------------------------------
 *
 * A simple historical quantile width is only a point estimate from one sample.
 * This function instead asks:
 *
 *   "What width is still plausible after accounting for sampling uncertainty
 *    and dependence in the return process?"
 *
 * That produces stop/target widths that are more stable and less prone to
 * overfitting.
 *
 * ---------------------------------------------------------------------------
 * BLOCK BOOTSTRAP AND DEPENDENCE PRESERVATION
 * ---------------------------------------------------------------------------
 *
 * Financial return series are rarely IID. Even when raw returns show little
 * serial correlation, volatility often clusters over time. Because stop and
 * target widths depend strongly on the local dispersion of returns, preserving
 * this dependence is important.
 *
 * The stationary bootstrap block length is chosen adaptively inside
 * ComputeBootstrappedWidths() as follows:
 *
 * For n >= 100:
 * - Compute the ACF of raw log returns
 * - Compute the ACF of absolute log returns
 * - Convert each ACF into a smooth tapered dependence-mass estimate
 * - Convert those dependence masses into candidate block lengths
 * - Use the larger of the two candidate lengths:
 *
 *     L = max(L_raw, L_abs)
 *
 * This design preserves:
 *
 * - signed dependence in raw returns when present
 * - volatility clustering through absolute returns
 *
 * For n < 100:
 * - Use the fallback heuristic:
 *
 *     L = max(2, floor(n^(1/3)))
 *
 * This block-length rule is smoother and more stable than a threshold-based
 * significance rule and is less sensitive to small changes in in-sample length.
 *
 * ---------------------------------------------------------------------------
 * BCa CONFIDENCE INTERVALS
 * ---------------------------------------------------------------------------
 *
 * The Bias-Corrected and Accelerated (BCa) bootstrap improves on standard
 * percentile bootstrap by correcting for:
 *
 * - median bias in the bootstrap distribution
 * - skewness via jackknife-based acceleration
 *
 * This is especially useful for return-width statistics, which are often
 * asymmetric and skewed.
 *
 * ---------------------------------------------------------------------------
 * CONSERVATIVE LONG-SIDE BOUNDS
 * ---------------------------------------------------------------------------
 *
 * For LONG positions, the function uses the bootstrap bounds asymmetrically:
 *
 * - Profit Target Width = lower bound of the upside-width distribution
 *   -> conservative estimate of achievable upside
 *
 * - Stop Width = upper bound of the downside-width distribution
 *   -> conservative estimate of required downside tolerance
 *
 * This means:
 *
 * - targets are not overly optimistic
 * - stops are not unrealistically tight
 *
 * ---------------------------------------------------------------------------
 * QUANTILE CHOICES
 * ---------------------------------------------------------------------------
 *
 * The width statistics are based on:
 *
 * - q10
 * - q50
 * - q90
 *
 * rather than more extreme tail quantiles. This improves finite-sample
 * stability and reduces sensitivity to outliers while still capturing the
 * central shape of the return distribution.
 *
 * ---------------------------------------------------------------------------
 * MINIMUM DATA REQUIREMENTS
 * ---------------------------------------------------------------------------
 *
 * - Absolute minimum: 30 ROC observations after period adjustment
 * - Practical requirement: comfortably more than 30 observations for stable
 *   width estimation and block-length selection
 *
 * If there is insufficient data or the bootstrap fails, the function returns
 * near-zero epsilon values.
 *
 * ---------------------------------------------------------------------------
 * COMPUTATIONAL COST
 * ---------------------------------------------------------------------------
 *
 * This function is more expensive than a simple quantile calculation because it
 * performs:
 *
 * - adaptive block-length estimation
 * - stationary block resampling
 * - 10,000 BCa bootstrap replications
 * - jackknife-based BCa correction
 *
 * Runtime is typically acceptable for research, validation, and batch analysis,
 * but may be too expensive for latency-sensitive workflows unless cached.
 *
 * @tparam Decimal Numeric type (for example num::DefaultNumber or double)
 *
 * @param series OHLC time series to analyze
 *
 * @param period Lookback period used to compute the ROC series. Typical values:
 *               5-10 for shorter horizon analysis, 10-20 for swing-style
 *               analysis, and larger values for longer horizon studies.
 *
 * @return std::pair<Decimal, Decimal> containing:
 *         - profit target width
 *         - stop width
 *
 * Both are returned in decimal form:
 * - 0.03 means 3%
 * - 0.05 means 5%
 *
 * For LONG positions:
 * - target is applied above entry
 * - stop is applied below entry
 *
 * @throws std::domain_error If the input series is too small
 * @throws std::domain_error If the derived ROC series is too small
 *
 * @note Returns {eps, eps} where eps = 1e-8 in degenerate cases such as:
 *       - insufficient data
 *       - zero-variance samples
 *       - bootstrap failure
 *
 * @warning Results depend on the selected period and the recent dependence
 *          structure of the input series. They should be validated out of
 *          sample before being used in production trading logic.
 *
 * @see ComputeBootstrappedShortStopAndTarget
 * @see detail::ComputeBootstrappedWidths
 * @see BCaBootStrap
 * @see StationaryBlockResampler
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
 * @brief Computes robust SHORT-side profit target and stop widths using BCa
 *        bootstrap with adaptive dependence-preserving block resampling.
 *
 * This function calculates statistically robust stop-loss and profit-target
 * widths for SHORT (sell) positions using the same bootstrap engine as the
 * LONG-side function, but with the width bounds applied according to short
 * trade mechanics.
 *
 * Rather than using a single observed quantile width from historical data,
 * the function bootstraps the distribution of width statistics while preserving
 * local time-series dependence through stationary block resampling.
 *
 * ---------------------------------------------------------------------------
 * METHODOLOGY OVERVIEW
 * ---------------------------------------------------------------------------
 *
 * The function proceeds in three stages:
 *
 * 1. Data Preparation
 *    - Computes a Rate-of-Change (ROC) series over the requested period
 *
 * 2. Width Definition
 *    - Upside Width   = q90 - q50
 *    - Downside Width = q50 - q10
 *
 * 3. Bootstrap Inference
 *    - Uses adaptive stationary block bootstrap
 *    - Uses BCa confidence intervals for bias and skewness correction
 *
 * The result is a pair of conservative SHORT-side widths:
 *
 * - profit target width
 * - stop-loss width
 *
 * ---------------------------------------------------------------------------
 * BLOCK BOOTSTRAP AND DEPENDENCE PRESERVATION
 * ---------------------------------------------------------------------------
 *
 * Financial returns often exhibit:
 *
 * - weak or modest serial dependence in signed returns
 * - stronger serial dependence in volatility
 *
 * Because this function is estimating stop and target widths, preserving local
 * volatility structure is especially important. Width statistics depend more on
 * the local scale of returns than on a weak directional autocorrelation alone.
 *
 * The stationary bootstrap block length is chosen adaptively inside
 * ComputeBootstrappedWidths() as follows:
 *
 * For n >= 100:
 * - Compute ACF on raw log returns
 * - Compute ACF on absolute log returns
 * - Convert both ACFs into smooth tapered dependence-mass measures
 * - Convert those into candidate block lengths
 * - Use:
 *
 *     L = max(L_raw, L_abs)
 *
 * This preserves whichever short-horizon dependence structure is stronger:
 *
 * - signed dependence
 * - volatility clustering
 *
 * For n < 100:
 * - Fall back to:
 *
 *     L = max(2, floor(n^(1/3)))
 *
 * This adaptive rule is intentionally smoother and more stable than a binary
 * threshold-crossing block-length rule.
 *
 * ---------------------------------------------------------------------------
 * BCa CONFIDENCE INTERVALS
 * ---------------------------------------------------------------------------
 *
 * The Bias-Corrected and Accelerated (BCa) bootstrap improves on ordinary
 * percentile bootstrap by correcting for:
 *
 * - bias in the bootstrap distribution
 * - skewness through jackknife acceleration
 *
 * This is useful because width statistics for financial returns are typically
 * asymmetric and not well described by simple Gaussian assumptions.
 *
 * ---------------------------------------------------------------------------
 * CONSERVATIVE SHORT-SIDE BOUNDS
 * ---------------------------------------------------------------------------
 *
 * For SHORT positions, the interpretation of upside and downside is reversed:
 *
 * - Profit comes from downside movement
 * - Risk comes from upside movement
 *
 * Therefore the function uses:
 *
 * - Profit Target Width = lower bound of the downside-width distribution
 *   -> conservative estimate of achievable downside
 *
 * - Stop Width = upper bound of the upside-width distribution
 *   -> conservative estimate of adverse upside risk
 *
 * This produces SHORT-side widths that are intentionally conservative:
 *
 * - profit targets are not overly optimistic
 * - stops are not unrealistically tight
 *
 * ---------------------------------------------------------------------------
 * QUANTILE CHOICES
 * ---------------------------------------------------------------------------
 *
 * Widths are based on:
 *
 * - q10
 * - q50
 * - q90
 *
 * rather than more extreme tail quantiles. This provides a more stable estimate
 * of distribution width while still capturing the relevant shape of upside and
 * downside movement.
 *
 * ---------------------------------------------------------------------------
 * MINIMUM DATA REQUIREMENTS
 * ---------------------------------------------------------------------------
 *
 * - Absolute minimum: 30 ROC observations after the lookback-period offset
 * - Practical requirement: comfortably more data for stable width estimation
 *   and adaptive block-length selection
 *
 * If data are insufficient or the bootstrap fails, the function returns
 * near-zero epsilon values.
 *
 * ---------------------------------------------------------------------------
 * COMPUTATIONAL COST
 * ---------------------------------------------------------------------------
 *
 * This function is computationally heavier than a simple historical quantile
 * method because it includes:
 *
 * - adaptive dependence estimation
 * - stationary block bootstrap resampling
 * - 10,000 BCa bootstrap replications
 * - jackknife-based acceleration
 *
 * It is suitable for research and batch workflows, and can also be used in
 * production if results are cached or computed offline.
 *
 * @tparam Decimal Numeric type (for example num::DefaultNumber or double)
 *
 * @param series OHLC time series to analyze
 *
 * @param period Lookback period used to compute the ROC series
 *
 * @return std::pair<Decimal, Decimal> containing:
 *         - profit target width
 *         - stop width
 *
 * Both are returned in decimal form.
 *
 * For SHORT positions:
 * - target is applied below entry
 * - stop is applied above entry
 *
 * @throws std::domain_error If the input series is too small
 * @throws std::domain_error If the derived ROC series is too small
 *
 * @note Returns {eps, eps} where eps = 1e-8 in degenerate cases such as:
 *       - insufficient data
 *       - degenerate bootstrap samples
 *       - bootstrap failure
 *
 * @warning Short-side widths should be interpreted together with any additional
 *          short-selling constraints not modeled here, such as borrow costs,
 *          dividend effects, or execution frictions.
 *
 * @see ComputeBootstrappedLongStopAndTarget
 * @see detail::ComputeBootstrappedWidths
 * @see BCaBootStrap
 * @see StationaryBlockResampler
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
