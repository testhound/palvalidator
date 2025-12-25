#pragma once
#include <vector>
#include <cmath>
#include <tuple>
#include <random>
#include <functional> 
#include <numeric> // Required for std::accumulate
#include <algorithm>
#include <cstddef>
#include <array> 
#include <boost/container/small_vector.hpp>
#include "DecimalConstants.h"
#include "number.h"
#include "decimal_math.h"
#include "TimeSeriesIndicators.h"
#include "randutils.hpp"

namespace mkc_timeseries
{
  // Forward declare StatUtils so ComputeFast can reference it
  template <class Decimal> struct StatUtils;

  /**
     * @brief Shared winsorization logic for geometric mean calculations.
     *
     * @details
     * This helper encapsulates the adaptive winsorization strategy used by
     * both GeoMeanStat and GeoMeanFromLogBarsStat. It computes the number of
     * values to clip per tail (k) based on sample size and mode, then applies
     * symmetric winsorization in-place to a vector of log-space values.
     *
     * The winsorization strategy eliminates the discontinuity at n=30/31 that
     * existed in the original implementation by providing smooth adaptive
     * scaling across different sample sizes.
     *
     * MODES:
     *   0 = Legacy (hard cutoff at n=30, backward compatible)
     *   1 = Smooth fade (default, eliminates discontinuity)
     *   2 = Always on (constant alpha, no fade)
     *
     * SCALING (Mode 1 - Smooth Fade):
     *   n ∈ [20, 30]:   Full protection (k ≥ 1)
     *   n ∈ [31, 50]:   Still protected (k ≥ 1, covers 94% of typical data)
     *   n ∈ [51, 100]:  Gradual fade (k scales down smoothly)
     *   n > 100:        Minimal winsorization (uses raw alpha)
     *
     * @tparam Decimal Numeric type (e.g., mkc_timeseries::number)
     */
    template <typename Decimal>
    class AdaptiveWinsorizer
    {
    public:
      /**
       * @brief Construct an adaptive winsorizer.
       *
       * @param alpha         Base winsorization proportion (e.g., 0.02 for 2% per tail)
       * @param adaptive_mode Winsorization strategy:
       *                      0 = Legacy (hard cutoff at n=30)
       *                      1 = Smooth fade (default, eliminates discontinuity)
       *                      2 = Always on (constant alpha)
       */
      explicit AdaptiveWinsorizer(double alpha = 0.02, int adaptive_mode = 1)
        : m_alpha(alpha)
        , m_adaptiveMode(adaptive_mode)
      {}

      /**
       * @brief Compute the number of values to clip per tail.
       *
       * @param n Sample size
       * @return Number of values to winsorize on each tail (k)
       *
       * @details
       * Returns k ∈ [0, (n-1)/2] based on sample size and mode.
       * The value k determines how many extreme values will be clipped
       * on each tail of the sorted distribution.
       */
      std::size_t computeK(std::size_t n) const
      {
        if (m_alpha <= 0.0 || n == 0)
          return 0;

        std::size_t k = 0;

        // ---------------------------------------------------------------------
        // MODE 0: LEGACY (Hard cutoff at n=30)
        // ---------------------------------------------------------------------
        if (m_adaptiveMode == 0) {
          if (n >= 20 && n <= 30) {
            k = static_cast<std::size_t>(std::floor(m_alpha * static_cast<double>(n)));
            if (k < 1)
              k = 1;  // Force at least one per tail
          }
          // else: k = 0 (no winsorization)
        }

        // ---------------------------------------------------------------------
        // MODE 1: SMOOTH FADE (Default - eliminates discontinuity)
        // ---------------------------------------------------------------------
        else if (m_adaptiveMode == 1) {
          if (n >= 20) {
            double base_k = m_alpha * static_cast<double>(n);

            if (n <= 30) {
              // Small samples: Full winsorization
              if (base_k < 1.0) {
                k = 1;  // Override: minimum 1 per tail
              } else {
                k = static_cast<std::size_t>(std::floor(base_k));
              }
            }
            else if (n <= 100) {
              // Medium samples: Smooth fade
              // Linear scaling: (100 - n) / (100 - 30)
              double scale_factor = (100.0 - static_cast<double>(n)) / 70.0;
              double scaled_k = base_k * scale_factor;

              // For n ∈ [31, 50]: still enforce k ≥ 1
              // This covers 94% of typical strategy data
              if (n <= 50 && scaled_k < 1.0) {
                k = 1;
              } else {
                k = static_cast<std::size_t>(std::floor(scaled_k));
              }
            }
            else {
              // Large samples (n > 100): Use raw alpha
              k = static_cast<std::size_t>(std::floor(base_k));
            }
          }
        }

        // ---------------------------------------------------------------------
        // MODE 2: ALWAYS ON (Constant alpha, no fade)
        // ---------------------------------------------------------------------
        else if (m_adaptiveMode == 2) {
          if (n >= 20) {
            k = static_cast<std::size_t>(std::floor(m_alpha * static_cast<double>(n)));
            if (k < 1)
              k = 1;
          }
        }

        // Cap k at (n-1)/2 to avoid clipping more than half the data
        const std::size_t kmax = (n > 0 ? (n - 1) / 2 : 0);
        if (k > kmax)
          k = kmax;

        return k;
      }

      /**
       * @brief Apply winsorization to a vector of log-space values.
       *
       * @param logs Vector of log-transformed values (modified in-place)
       *
       * @details
       * Sorts the values, identifies the k-th smallest and k-th largest,
       * then clips all values outside this range. The original order of
       * the vector is preserved (winsorization is applied to the unsorted
       * vector using sorted thresholds).
       *
       * If k = 0, this function does nothing (no winsorization).
       */
      void apply(std::vector<Decimal>& logs) const
      {
        const std::size_t n = logs.size();
        const std::size_t k = computeK(n);

        if (k == 0)
          return;  // No winsorization

        // For small to medium n, full sort is fine and clear
        auto sorted = logs;
        std::sort(sorted.begin(), sorted.end());

        const Decimal lo = sorted[k];
        const Decimal hi = sorted[n - 1 - k];

        // Winsorize in-place (preserves original order)
        for (auto& x : logs) {
          if (x < lo)
            x = lo;
          else if (x > hi)
            x = hi;
        }
      }

      /**
       * @brief Get the current alpha value.
       */
      double getAlpha() const { return m_alpha; }

      /**
       * @brief Get the current adaptive mode.
       */
      int getAdaptiveMode() const { return m_adaptiveMode; }

    private:
      double m_alpha;
      int    m_adaptiveMode;
    };

    // ========================================================================
    // STRUCT: GeoMeanStat (operates on raw returns)
    // ========================================================================
    /**
     * @brief Compute geometric mean from raw percent returns with adaptive winsorization.
     *
     * @details
     * This struct takes raw percent returns (e.g., 0.01 for 1%), converts them to
     * log-space, applies adaptive winsorization to reduce outlier influence, and
     * computes the geometric mean.
     *
     * WINSORIZATION:
     *   - Applied in log-space for symmetry and stability
     *   - Adaptive scaling eliminates n=30/31 discontinuity
     *   - See AdaptiveWinsorizer for detailed behavior
     *
     * TYPICAL USAGE:
     *   GeoMeanStat stat;  // Defaults to smooth fade mode
     *   Decimal gm = stat(returns);
     */
    template <typename Decimal>
    struct GeoMeanStat
    {
      using DC = DecimalConstants<Decimal>;

      /// \brief Helper to format the statistic for display (as percentage)
      static double formatForDisplay(double value)
      {
        return value * 100.0;
      }

      static constexpr bool isRatioStatistic() noexcept
      {
        return false;
      }

      /**
       * @brief Full constructor with adaptive winsorization controls.
       *
       * @param clip_ruin       If true, clip 1+r to at least ruin_eps before log.
       * @param winsor_small_n  If true, enable adaptive winsorization.
       * @param winsor_alpha    Base winsorization proportion (e.g., 0.02 for 2% per tail).
       *                        For small n, actual percentage will be higher due to k ≥ 1 enforcement.
       *                        Effective percentages with adaptive mode 1:
       *                          n=20:  10% per tail (k=1, 2 out of 20)
       *                          n=26:  7.7% per tail (k=1, 2 out of 26)
       *                          n=30:  6.7% per tail (k=1, 2 out of 30)
       *                          n=50:  4.0% per tail (k=1, 2 out of 50)
       *                          n=100: ~2.0% per tail (k=2, 4 out of 100)
       * @param ruin_eps        Floor for (1+r). Must be > 0 (e.g., 1e-8).
       * @param adaptive_mode   Winsorization strategy:
       *                        0 = Legacy (hard cutoff at n=30, backward compatible)
       *                        1 = Smooth fade (default, eliminates discontinuity)
       *                        2 = Always on (constant alpha, no fade)
       */
      explicit GeoMeanStat(bool   clip_ruin      = true,
                           bool   winsor_small_n = true,
                           double winsor_alpha   = 0.02,
                           double ruin_eps       = 1e-8,
                           int    adaptive_mode  = 1)
        : m_clipRuin(clip_ruin)
        , m_winsorSmallN(winsor_small_n)
        , m_ruinEps(ruin_eps)
        , m_winsorizer(winsor_alpha, adaptive_mode)
      {}

      /**
       * @brief Backward-compatible constructor (legacy signature).
       *        Defaults to smooth adaptive mode (mode=1).
       */
      explicit GeoMeanStat(bool clip_ruin, double ruin_eps)
        : m_clipRuin(clip_ruin)
        , m_winsorSmallN(true)
        , m_ruinEps(ruin_eps)
        , m_winsorizer(0.02, 1)  // alpha=0.02, mode=1 (smooth fade)
      {}

      /**
       * @brief Compute geometric mean (per period) from percent returns.
       *
       * @param returns Vector of per-period percent returns where 1% is 0.01.
       * @return Decimal per-period geometric mean (same units as inputs).
       *
       * @details
       * Process:
       *   1. Convert returns to log-space: log(1 + r)
       *   2. Apply adaptive winsorization to log-values (if enabled)
       *   3. Compute mean of (winsorized) logs
       *   4. Back-transform: exp(mean_log) - 1
       *
       * @throws std::domain_error if clip_ruin=false and any 1+r ≤ 0
       */
      Decimal operator()(const std::vector<Decimal>& returns) const
      {
        const std::size_t n = returns.size();
        if (n == 0)
          return DC::DecimalZero;

        std::vector<Decimal> logs;
        logs.reserve(n);

        const Decimal eps  = Decimal(m_ruinEps);
        const Decimal zero = DC::DecimalZero;
        const Decimal one  = DC::DecimalOne;

        // Build log(1+r) with proper ruin handling
        for (const auto& r : returns) {
          Decimal growth = one + r;  // 1 + r

          if (!m_clipRuin) {
            // Strict mode: throw on invalid growth
            if (growth <= zero) {
              throw std::domain_error("GeoMeanStat: 1+r <= 0 and clip_ruin=false");
            }
            logs.push_back(std::log(growth));
          }
          else {
            // Clip mode: floor growth at ruin_eps
            if (growth <= eps)
              growth = eps;
            logs.push_back(std::log(growth));
          }
        }

        // Apply adaptive winsorization (if enabled)
        if (m_winsorSmallN) {
          m_winsorizer.apply(logs);
        }

        // Compute mean of (winsorized) logs
        Decimal sum = DC::DecimalZero;
        for (const auto& x : logs)
          sum += x;
        const Decimal meanLog = sum / Decimal(static_cast<double>(n));

        // Back-transform: exp(meanLog) - 1
        return std::exp(meanLog) - one;
      }

    private:
      bool               m_clipRuin;
      bool               m_winsorSmallN;
      double             m_ruinEps;
      AdaptiveWinsorizer<Decimal> m_winsorizer;
    };

    // ========================================================================
    // STRUCT: GeoMeanFromLogBarsStat (operates on pre-computed log-bars)
    // ========================================================================
    /**
     * @brief Compute geometric mean from pre-computed log(1+r) values with adaptive winsorization.
     *
     * @details
     * This struct is optimized for bootstrap use where log-transformations are
     * pre-computed once and resampled many times. It applies the same adaptive
     * winsorization as GeoMeanStat but operates directly on log-space values.
     *
     * PERFORMANCE:
     *   In bootstrap with 1000 iterations, this avoids 1000 × n log() computations
     *   by working with pre-computed log-bars.
     *
     * CONSISTENCY:
     *   GeoMeanStat(returns) ≡ GeoMeanFromLogBarsStat(makeLogGrowthSeries(returns))
     *   Both use identical AdaptiveWinsorizer logic.
     *
     * TYPICAL USAGE:
     *   auto logBars = StatUtils::makeLogGrowthSeries(returns, ruin_eps);
     *   GeoMeanFromLogBarsStat stat;
     *   Decimal gm = stat(logBars);
     */
    template <typename Decimal>
    struct GeoMeanFromLogBarsStat
    {
      using DC = DecimalConstants<Decimal>;

      /// \brief Helper to format the statistic for display (as percentage)
      static double formatForDisplay(double value)
      {
        return value * 100.0;
      }

      static constexpr bool isRatioStatistic() noexcept
      {
        return false;
      }

      /**
       * @brief Constructor with adaptive winsorization controls.
       *
       * @param winsor_small_n If true, enable adaptive winsorization.
       * @param winsor_alpha   Base winsorization proportion (e.g., 0.02 for 2% per tail).
       *                       See GeoMeanStat documentation for effective percentages.
       * @param adaptive_mode  Winsorization strategy:
       *                       0 = Legacy (hard cutoff at n=30)
       *                       1 = Smooth fade (default, eliminates discontinuity)
       *                       2 = Always on (constant alpha, no fade)
       */
      explicit GeoMeanFromLogBarsStat(bool   winsor_small_n = true,
                                      double winsor_alpha   = 0.02,
                                      int    adaptive_mode  = 1)
        : m_winsorSmallN(winsor_small_n)
        , m_winsorizer(winsor_alpha, adaptive_mode)
      {}

      /**
       * @brief Compute geometric mean from pre-computed log(1+r) bars.
       *
       * @param logBars Vector of log-transformed returns. Each element should be
       *                log(max(1 + r_i, ruin_eps)) as produced by makeLogGrowthSeries.
       * @return Decimal per-period geometric mean (same units as original returns).
       *
       * @details
       * Process:
       *   1. Copy logBars (to preserve caller's data)
       *   2. Apply adaptive winsorization to log-values (if enabled)
       *   3. Compute mean of (winsorized) logs
       *   4. Back-transform: exp(mean_log) - 1
       *
       * NOTE: Input is already in log-space, so no log() calls are needed.
       *       This is the key performance optimization for bootstrap use.
       */
      Decimal operator()(const std::vector<Decimal>& logBars) const
      {
        const std::size_t n = logBars.size();
        if (n == 0)
          return DC::DecimalZero;

        // Copy to preserve caller's data (winsorization modifies in-place)
        std::vector<Decimal> logs = logBars;

        // Apply adaptive winsorization (if enabled)
        if (m_winsorSmallN) {
          m_winsorizer.apply(logs);
        }

        // Compute mean of (winsorized) logs
        Decimal sum = DC::DecimalZero;
        for (const auto& x : logs)
          sum += x;
        const Decimal meanLog = sum / Decimal(static_cast<double>(n));

        // Back-transform: exp(meanLog) - 1
        const Decimal one = DC::DecimalOne;
        return std::exp(meanLog) - one;
      }

    private:
      bool               m_winsorSmallN;
      AdaptiveWinsorizer<Decimal> m_winsorizer;
    };
    
  /**
   * @brief A generic helper struct for fast mean and variance computation.
   * @tparam Decimal The data type for which statistics are computed.
   * @details This is the default implementation of the ComputeFast helper. It serves as a
   * fallback for any `Decimal` type that does not have a dedicated partial
   * specialization. It simply calls the standard `StatUtils::computeMeanAndVariance`
   * method, ensuring broad compatibility.
   */
  template <class Decimal>
  struct ComputeFast
  {
    static inline std::pair<Decimal, Decimal>
    run(const std::vector<Decimal>& data)
    {
      // Fallback: reuse your existing implementation
      return StatUtils<Decimal>::computeMeanAndVariance(data);
    }
  };

  /**
   * @brief A specialized, high-performance mean/variance calculator for `dec::decimal`.
   * @tparam Prec The precision of the `dec::decimal` type.
   * @tparam RoundPolicy The rounding policy of the `dec::decimal` type.
   * @details This partial specialization of `ComputeFast` is optimized for the
   * `dec::decimal` library. It implements a hybrid version of **Welford's algorithm** to
   * compute the mean and unbiased sample variance in a single pass.
   *
   * Welford's algorithm is highly valued for its **numerical stability**. Unlike the
   * naive two-pass approach, which can suffer from "catastrophic cancellation"
   * (a significant loss of precision) when variance is small compared to the mean,
   * Welford's updates its aggregates incrementally. This avoids the subtraction of two
   * large, nearly-equal numbers and preserves accuracy.
   *
   * To enhance performance, this implementation performs the iterative Welford updates
   * using native `long double` arithmetic, minimizing the overhead of repeated
   * conversions to and from the `dec::decimal` type.
   * @see Welford, B. P. (1962). "Note on a method for calculating corrected sums of squares and products". *Technometrics*, 4(3), 419-420.
   * @see Knuth, D. E. (1998). *The Art of Computer Programming, Vol. 2: Seminumerical Algorithms* (3rd ed.), 4.2.2.
   */
  template <int Prec, class RoundPolicy>
  struct ComputeFast<dec::decimal<Prec, RoundPolicy>>
  {
    using Decimal = dec::decimal<Prec, RoundPolicy>;

    /**
     * @brief Executes the single-pass Welford's algorithm.
     * @param data A constant reference to a vector of `Decimal` values.
     * @return A `std::pair<Decimal, Decimal>` containing the computed mean (as the first element)
     * and the unbiased sample variance (as the second element). Returns {0, 0}
     * if the input vector is empty.
     */
    static inline std::pair<Decimal, Decimal>
    run(const std::vector<Decimal>& data)
    {
      const std::size_t n = data.size();
      if (n == 0)
        {
	  return { DecimalConstants<Decimal>::DecimalZero,
		   DecimalConstants<Decimal>::DecimalZero };
        }

      // Hybrid Welford in long double; one conversion per element.
      const long double F    = static_cast<long double>(dec::DecimalFactor<Prec>::value);
      const long double invF = 1.0L / F;

      long double mean = 0.0L;
      long double m2   = 0.0L;
      std::size_t k    = 0;

      for (const auto& d : data)
        {
	  const long double x = static_cast<long double>(d.getUnbiased()) * invF;
	  ++k;
	  const long double delta  = x - mean;
	  mean += delta / static_cast<long double>(k);
	  const long double delta2 = x - mean;
	  m2 += delta * delta2;
        }

      const long double var = (k > 1) ? (m2 / static_cast<long double>(k - 1)) : 0.0L;

      // Convert aggregates back to Decimal once.
      return { Decimal(static_cast<double>(mean)),
	       Decimal(static_cast<double>(var)) };
    }
  };

  /**
   * @class StatUtils
   * @brief A template class providing static utility functions for statistical analysis of financial time series.
   * @tparam Decimal The high-precision decimal type to be used for calculations.
   */
  template<class Decimal>
  struct StatUtils
  {
  public:
    static constexpr double DefaultRuinEps       = 1e-8;
    static constexpr double DefaultDenomFloor    = 1e-6;
    static constexpr double DefaultPriorStrength = 1.0;
    static constexpr bool   DefaultCompress      = true;
    
    static inline std::vector<Decimal>
    percentBarsToLogBars(const std::vector<Decimal>& pct)
    {
      std::vector<Decimal> out;
      out.reserve(pct.size());
      const auto one = DecimalConstants<Decimal>::DecimalOne;
      for (const auto& r : pct)
	out.push_back(std::log(one + r));
      return out;
    }

    /**
     * @brief Convert a series of percent returns into log-growth values.
     *
     * @details
     * This function takes a vector of per-period returns (expressed as decimals,
     * where +5% is 0.05) and converts each return into a log-growth value:
     *
     *     log_i = log( max(1 + r_i, ruin_eps) )
     *
     * The value ruin_eps is used to clamp the growth factor in cases where
     * 1 + r_i is zero or negative. This prevents undefined logarithms and
     * represents a severe-loss, "ruin" event in a numerically stable way.
     *
     * This function does not apply stop-loss logic or any priors. It strictly
     * converts returns into valid log-growth values suitable for bootstrap
     * resampling or use by statistics that operate directly in log space,
     * such as LogProfitFactorFromLogBarsStat.
     *
     * @param returns
     *     Vector of percentage returns, where +1% = 0.01.
     *
     * @param ruin_eps
     *     A small positive constant used when 1 + r_i <= 0. Must be > 0.
     *     This value determines how "ruin" is represented in log space.
     *
     * @return
     *     A vector of log-growth values of the same length as the input.
     *     Each output element is log(max(1 + r_i, ruin_eps)).
     *
     * @note
     * The caller is responsible for using the same ruin_eps here and in the
     * statistic that consumes the resulting log-bars, ensuring consistent behavior.
     */
    static inline std::vector<Decimal>
    makeLogGrowthSeries(const std::vector<Decimal>& returns,
                                     double ruin_eps)
    {
      using DC = mkc_timeseries::DecimalConstants<Decimal>;
      const Decimal one  = DC::DecimalOne;
      const Decimal zero = DC::DecimalZero;
      const Decimal d_ruin(ruin_eps);

      std::vector<Decimal> logs;
      logs.reserve(returns.size());

      for (const auto& r : returns)
	{
	  Decimal growth = one + r;
	  if (growth <= zero)
	    growth = d_ruin;
	  logs.push_back(std::log(growth));
	}
      return logs;
    }

    /**
       * @brief Computes the mean and variance using a fast, specialized path.
       * @details This method serves as a dispatcher to the most efficient available
       * implementation for computing mean and variance. It delegates the calculation
       * to the `ComputeFast` helper struct, which leverages a partial template
       * specialization for `dec::decimal` types to use a high-performance,
       * numerically stable Welford's algorithm. For other types, it falls back
       * to a generic implementation.
       * @param data A constant reference to a vector of `Decimal` values.
       * @return A std::pair<Decimal, Decimal> containing the mean (first) and the
       * unbiased sample variance (second).
       * @see ComputeFast
       */
    static inline std::pair<Decimal, Decimal>
    computeMeanAndVarianceFast(const std::vector<Decimal>& data)
    {
      // Dispatches to the right helper (generic vs dec::decimal specialization)
      return ComputeFast<Decimal>::run(data);
    }

    /**
       * @brief Calculates the basic Sharpe Ratio from a vector of returns.
       * @details This version of the function calculates the Sharpe Ratio assuming a
       * risk-free rate of zero and no annualization. It uses the efficient
       * `computeMeanAndVarianceFast` method. The formula is simply: `mean / stddev`.
       * @param data A vector of `Decimal` returns.
       * @param eps A small epsilon value added to the variance to ensure the denominator
       * (standard deviation) is non-zero and to improve numerical stability.
       * Defaults to 1e-8.
       * @return The calculated Sharpe Ratio as a `Decimal`. Returns zero if the
       * standard deviation is effectively zero.
       * @see computeMeanAndVarianceFast
       */
    static inline Decimal sharpeFromReturns(const std::vector<Decimal>& data, double eps = 1e-8)
    {
      auto [meanDec, varDec] = StatUtils<Decimal>::computeMeanAndVarianceFast(data);
	
      const double var = num::to_double(varDec);
      const double sd  = std::sqrt(std::max(var + eps, eps));
      if (sd == 0.0)
	return DecimalConstants<Decimal>::DecimalZero;

      return Decimal(num::to_double(meanDec) / sd);
    }

    /**
       * @brief Calculates the annualized Sharpe Ratio, accounting for a risk-free rate.
       * @details This function computes the Sharpe Ratio for a series of excess returns
       * (returns minus the risk-free rate) and then annualizes the result. The
       * formula used is: `(mean(returns) - riskFreePerPeriod) / stddev(returns) * sqrt(periodsPerYear)`.
       * It leverages the `computeMeanAndVarianceFast` path for efficiency.
       * @param data A vector of `Decimal` returns for a specific period (e.g., daily, weekly).
       * @param eps A small epsilon value added to the variance for numerical stability.
       * @param periodsPerYear The number of return periods in a year used for annualization
       * (e.g., ~252 for daily returns, 52 for weekly, 12 for monthly).
       * @param riskFreePerPeriod The risk-free rate of return for a single period, which will
       * be subtracted from the mean return.
       * @return The annualized Sharpe Ratio as a `Decimal`.
       * @see computeMeanAndVarianceFast
       */
    static inline Decimal sharpeFromReturns(const std::vector<Decimal>& data,
					    double eps,
					    double periodsPerYear,
					    double riskFreePerPeriod)
    {
      // Use the fast path (hybrid Welford) for mean/var
      auto [meanDec, varDec] = StatUtils<Decimal>::computeMeanAndVarianceFast(data);

      const double mean = num::to_double(meanDec) - riskFreePerPeriod;
      const double var  = num::to_double(varDec);

      // ε-ridge to tame tiny denominators; also guards negative round-off
      const double sd = std::sqrt(std::max(var + eps, eps));

      if (sd == 0.0)
	return DecimalConstants<Decimal>::DecimalZero;

      const double ann = (periodsPerYear > 1.0) ? std::sqrt(periodsPerYear) : 1.0;
      const double sr  = (mean / sd) * ann;
      return Decimal(sr);
    }
 
    /**
     * @brief Computes the Profit Factor from a series of returns.
     * @details The Profit Factor is the ratio of gross profits to gross losses.
     * Formula: PF = sum(positive returns) / sum(abs(negative returns)).
     * @param xs A vector of returns.
     * @param compressResult If true, applies natural log compression to the final profit factor.
     * @return The calculated Profit Factor as a Decimal.
     */
    static Decimal computeProfitFactor(const std::vector<Decimal>& xs, bool compressResult = false)
    {
      Decimal win(DecimalConstants<Decimal>::DecimalZero);
      Decimal loss(DecimalConstants<Decimal>::DecimalZero);

      for (auto r : xs)
	{
	  if (r > DecimalConstants<Decimal>::DecimalZero)
	    win += r;
	  else
	    loss += r;
	}
	
      return computeFactor(win, loss, compressResult);
    }

    /**
     * @brief Compute a robust, regularized profit factor in log space.
     *
     * @details
     * This function computes a profit-factor-like statistic from a vector of
     * per-period returns using log(1 + r) internally. The design is intended
     * to be more stable and conservative than a classic profit factor, especially
     * for short series, near-ruin losses, and tiny loss denominators.
     *
     * The computation proceeds in three main stages:
     *
     *   1. Transform returns into log-growth values:
     *        • For each return r, compute growth = 1 + r.
     *        • If growth <= 0, clamp to ruin_eps (a small positive constant).
     *        • Take lr = log(growth).
     *        • If r > 0, add lr to the numerator (sum of log wins).
     *        • If r < 0, add lr to the log-loss sum and store its magnitude
     *          into a loss-magnitudes buffer.
     *
     *   2. Build a robust prior for the denominator:
     *        • If there is at least one loss, compute the median loss magnitude
     *          from the loss-magnitudes buffer and scale it by prior_strength.
     *        • If there are no losses, use default_loss_magnitude if it is
     *          positive; otherwise, derive a fallback magnitude from ruin_eps
     *          and denom_floor. This acts as a pseudo-loss so the denominator
     *          does not vanish when the sample has only wins.
     *
     *   3. Form the final ratio:
     *        • Numerator is the sum of log wins.
     *        • Denominator is the absolute sum of log losses plus the prior.
     *        • The denominator is floored at denom_floor to avoid division
     *          by extremely small values.
     *        • If compressResult is true, the final value is log(1 + PF);
     *          otherwise the raw PF ratio (numerator / denominator) is returned.
     *
     * This function is the core implementation used by LogProfitFactorStat.
     * When combined with makeLogGrowthSeries and LogProfitFactorFromLogBarsStat,
     * you can move the log(1 + r) work out of tight resampling loops and reuse
     * precomputed log-growth series.
     *
     * @param xs
     *     Vector of per-period returns, expressed as decimal fractions
     *     (for example +1% = 0.01).
     *
     * @param compressResult
     *     If true, return log(1 + PF). If false, return the raw PF ratio.
     *     The compressed version is usually preferred for inference.
     *
     * @param ruin_eps
     *     Small positive floor for (1 + r) when it would otherwise be zero
     *     or negative. This ensures log(1 + r) is always defined and heavily
     *     penalizes ruin-like events.
     *
     * @param denom_floor
     *     Minimum allowed denominator value in log space. This prevents extreme
     *     PF blow-ups when loss magnitudes are very small.
     *
     * @param prior_strength
     *     Multiplier for the prior loss magnitude. A value of 1.0 behaves like
     *     adding one extra loss of "typical" size to the denominator.
     *
     * @param default_loss_magnitude
     *     Optional fallback loss magnitude used when there are no losses in
     *     the sample. If this is greater than zero, it is interpreted as a
     *     fixed log-loss magnitude (for example derived from a strategy stop
     *     loss). If it is zero or negative, a ruin-based fallback is derived
     *     from ruin_eps and denom_floor.
     *
     * @return
     *     Robust profit-factor statistic in either compressed (log(1 + PF))
     *     or raw PF form, depending on compressResult. Returns zero if xs
     *     is empty.
     */
    static Decimal computeLogProfitFactorRobust(const std::vector<Decimal>& xs,
                                                bool   compressResult = DefaultCompress,
                                                double ruin_eps       = DefaultRuinEps,
                                                double denom_floor    = DefaultDenomFloor,
                                                double prior_strength = DefaultPriorStrength,
                                                double default_loss_magnitude = 0.0)
    {
      using DC = DecimalConstants<Decimal>;

      if (xs.empty())
	return DC::DecimalZero;

      // ... [Step 1: Build log(1+r) stream - Same as before] ...
      // (Copy existing loop logic here)
      const Decimal one     = DC::DecimalOne;
      const Decimal zero    = DC::DecimalZero;
      const Decimal d_ruin  = Decimal(ruin_eps);

      // OPTIMIZATION: Use small_vector with 64 slots of stack storage.
      // this avoids heap allocation since N is usually less than 64
      boost::container::small_vector<Decimal, 64> loss_magnitudes;

      //std::vector<Decimal> loss_magnitudes;

      Decimal sum_log_wins   = DC::DecimalZero;
      Decimal sum_log_losses = DC::DecimalZero;

      for (const auto& r : xs)
	{
	  Decimal growth = one + r;
	  if (growth <= zero) growth = d_ruin;
	  const Decimal lr = std::log(growth);

	  if (r > zero) {
	      sum_log_wins += lr;
	  } else if (r < zero) {
	      sum_log_losses += lr;
	      loss_magnitudes.push_back(-lr);
	  }
	}

      // --- Step 2: Robust prior for the denominator (|Σ losses|) ---
      Decimal prior_loss_mag = DC::DecimalZero;
      
      if (!loss_magnitudes.empty())
	{
	  // CASE A: Empirical losses exist. Use median of observed losses.
	  const std::size_t mid = loss_magnitudes.size() / 2;
	  std::nth_element(loss_magnitudes.begin(),
			   loss_magnitudes.begin() + static_cast<std::ptrdiff_t>(mid),
			   loss_magnitudes.end());
	  const Decimal med = loss_magnitudes[mid];
	  prior_loss_mag = med * Decimal(prior_strength);
	}
      else
	{
	  // CASE B: Zero losses in sample. Use the Strategy's Stop Loss (if provided).
	  // This avoids the "Ruin Proxy" spike by using a specific Bayesian prior.
	  Decimal assumed_mag;
	  if (default_loss_magnitude > 0.0) 
	    {
	      assumed_mag = Decimal(default_loss_magnitude);
	    } 
	  else 
	    {
	      // Fallback: Ruin proxy
	      assumed_mag = Decimal(std::max(-std::log(ruin_eps), denom_floor));
	    }
	  prior_loss_mag = assumed_mag * Decimal(prior_strength);
	}

      // --- Step 3 & 4: Form numerator/denominator and return ---
      const Decimal numer = sum_log_wins;
      Decimal denom       = num::abs(sum_log_losses) + prior_loss_mag;

      const Decimal d_floor = Decimal(denom_floor);
      if (denom < d_floor) denom = d_floor;

      Decimal pf = (denom > DC::DecimalZero) ? (numer / denom) : DC::DecimalZero;

      if (compressResult)
	return std::log(DC::DecimalOne + pf);

      return pf;
    }
    /**
     * @brief Functor wrapper for the robust log profit-factor on raw returns.
     *
     * @details
     * This struct adapts StatUtils::computeLogProfitFactorRobust into a
     * callable "statistic" type that can be passed into bootstrap engines
     * (for example StrategyAutoBootstrap). It expects a vector of per-period
     * returns and internally computes a robust profit-factor in log space.
     *
     * Typical usage:
     *   • Configure the constructor with compression and prior settings.
     *   • Call operator() with a vector of returns.
     *   • Optionally post-process the result using formatForDisplay, which
     *     maps the log(1 + PF) form back to a linear PF minus one.
     *
     * By default, the statistic returns log(1 + PF_robust), which is monotone
     * in PF and better behaved for confidence intervals and hypothesis tests.
     */
    struct LogProfitFactorStat
    {
      static double formatForDisplay(double value) { return std::exp(value) - 1.0; }
      static constexpr bool isRatioStatistic() noexcept { return true; }

          /**
     * @brief Construct a robust log profit-factor statistic on raw returns.
     *
     * @details
     * The configuration parameters here are passed through to
     * computeLogProfitFactorRobust. They control whether the result is
     * compressed, how ruin events are handled, how strongly the loss prior
     * is applied, and what to assume when no losses appear in the sample.
     *
     * @param compressResult
     *     If true, operator() returns log(1 + PF). If false, it returns
     *     the raw PF ratio. The default is StatUtils::DefaultCompress.
     *
     * @param ruin_eps
     *     Small positive floor for (1 + r) when computing log(1 + r).
     *     Must be strictly greater than zero. The default is
     *     StatUtils::DefaultRuinEps.
     *
     * @param denom_floor
     *     Minimum denominator value in log space. This protects against
     *     extremely large PF values caused by tiny loss magnitudes.
     *     The default is StatUtils::DefaultDenomFloor.
     *
     * @param prior_strength
     *     Scale factor for the prior loss magnitude used in the denominator.
     *     A value of 1.0 behaves like adding one additional loss of typical
     *     size. The default is StatUtils::DefaultPriorStrength.
     *
     * @param stop_loss_pct
     *     Optional stop-loss percentage expressed as a decimal fraction
     *     (for example 0.025 for 2.5%). If greater than zero, this is
     *     converted to a fixed log-loss magnitude and used as the default
     *     prior when the sample contains no losses. If zero, the prior
     *     falls back to a ruin-based magnitude derived from ruin_eps.
     */
      explicit LogProfitFactorStat(bool   compressResult = StatUtils::DefaultCompress,
                                   double ruin_eps       = StatUtils::DefaultRuinEps,
                                   double denom_floor    = StatUtils::DefaultDenomFloor,
                                   double prior_strength = StatUtils::DefaultPriorStrength,
                                   double stop_loss_pct  = 0.0)
	: m_compressResult(compressResult)
	, m_ruinEps(ruin_eps)
	, m_denomFloor(denom_floor)
	, m_priorStrength(prior_strength)
      {
	if (stop_loss_pct > 0.0) {
	  m_defaultLossMag = std::abs(std::log(1.0 - stop_loss_pct));
	} else {
	  m_defaultLossMag = 0.0;
	}
      }

          /**
     * @brief Evaluate the robust log profit-factor on a return series.
     *
     * @details
     * This method forwards its arguments to computeLogProfitFactorRobust
     * using the configuration stored in this functor. It takes a vector of
     * per-period returns, expressed as decimal fractions, and returns a
     * robust profit-factor statistic in either compressed or raw form.
     *
     * The input is assumed to be raw returns r (for example +1% = 0.01),
     * not precomputed log-growth values. Ruin events, priors, and
     * denominator flooring are handled as described in
     * computeLogProfitFactorRobust.
     *
     * @param returns
     *     Vector of per-period returns. May be any length; if empty,
     *     the result is zero.
     *
     * @return
     *     Robust profit-factor statistic in either log(1 + PF) or raw PF
     *     form depending on the configuration of this functor.
     */
      Decimal operator()(const std::vector<Decimal>& returns) const
      {
	return StatUtils<Decimal>::computeLogProfitFactorRobust(returns,
								m_compressResult,
								m_ruinEps,
								m_denomFloor,
								m_priorStrength,
								m_defaultLossMag); 
      }

    private:
      bool   m_compressResult;
      double m_ruinEps;
      double m_denomFloor;
      double m_priorStrength;
      double m_defaultLossMag; 
    };

    /**
     * @brief Robust, regularized profit-factor statistic using precomputed log-growth bars.
     *
     * @details
     * This statistic computes the same robust profit-factor as LogProfitFactorStat,
     * but expects the input vector to already contain log-growth values of the form:
     *
     *     logBars[i] = log( max(1 + return_i, ruin_eps) )
     *
     * This design allows callers to precompute log(1 + r) once and reuse those
     * values across many bootstrap iterations, dramatically reducing computation
     * time by avoiding repeated logarithm evaluations.
     *
     * Interpretation of log-bars:
     *   • logBars[i] > 0  → this is a winning bar (return > 0)
     *   • logBars[i] < 0  → this is a losing bar  (return < 0)
     *   • logBars[i] = 0  → neutral bar          (return = 0)
     *
     * The statistic:
     *   1. Sums positive log-bars (wins).
     *   2. Sums negative log-bars (losses).
     *   3. Computes the median magnitude of losses if any exist.
     *   4. Applies a prior amount to the denominator to stabilize PF.
     *   5. Uses a stop-loss magnitude or a ruin-based fallback if no losses exist.
     *   6. Floors the denominator at denom_floor for numerical stability.
     *   7. Returns either:
     *        • rawPF = sum_wins / (sum_loss_magnitudes + prior), or
     *        • log(1 + rawPF) if compressResult = true.
     *
     * When logBars is created using makeLogGrowthSeries with the same ruin_eps,
     * this statistic will match LogProfitFactorStat to standard numerical tolerance.
     *
     * @param compressResult
     *     If true (default), returns log(1 + PF), which is usually more stable
     *     for bootstrap inference. If false, returns the raw PF ratio.
     *
     * @param ruin_eps
     *     The ruin clipping constant used when fallback loss magnitude is needed.
     *     Must match the value used when constructing logBars.
     *
     * @param denom_floor
     *     Minimum denominator value to prevent division by extremely small numbers.
     *
     * @param prior_strength
     *     The amount of prior added to the denominator. A value of 1.0 behaves
     *     like adding one typical loss magnitude.
     *
     * @param stop_loss_pct
     *     Optional stop-loss percentage expressed as a decimal fraction.
     *     When > 0, the fallback loss magnitude becomes abs(log(1 - stop_loss_pct)).
     *     When = 0, the fallback magnitude is derived from ruin_eps.
     */
    struct LogProfitFactorFromLogBarsStat
    {
      static double formatForDisplay(double value) { return std::exp(value) - 1.0; }
      static constexpr bool isRatioStatistic() noexcept { return true; }

      explicit LogProfitFactorFromLogBarsStat(bool   compressResult = StatUtils::DefaultCompress,
					      double ruin_eps       = StatUtils::DefaultRuinEps,
					      double denom_floor    = StatUtils::DefaultDenomFloor,
					      double prior_strength = StatUtils::DefaultPriorStrength,
					      double stop_loss_pct  = 0.0)
	: m_compressResult(compressResult)
	, m_ruinEps(ruin_eps)
	, m_denomFloor(denom_floor)
	, m_priorStrength(prior_strength)
      {
	if (stop_loss_pct > 0.0)
	  m_defaultLossMag = std::abs(std::log(1.0 - stop_loss_pct));
	else
	  m_defaultLossMag = 0.0;
      }

      /**
       * @brief Evaluate the robust log profit-factor from a vector of log-bars.
       *
       * @details
       * This function processes each log-growth value to categorize wins and losses,
       * compute prior-adjusted denominator terms, enforce minimum denominator size,
       * and produce a profit-factor estimate. The output is either:
       *
       *     log(1 + PF)     if compressResult = true
       *     PF              if compressResult = false
       *
       * The function expects each element of logBars to be a valid log-growth value
       * produced by makeLogGrowthSeries or an equivalent transformation.
       *
       * Behavior:
       *   • Empty input returns zero.
       *   • Positive log-bars add to the numerator (wins).
       *   • Negative log-bars contribute to loss magnitudes.
       *   • Median loss magnitude is used as a Bayesian-style prior if losses exist.
       *   • A stop-loss or ruin-based fallback magnitude is used if no losses exist.
       *   • Denominator is floored at denom_floor.
       *
       * @param logBars
       *     Vector of precomputed log-growth values. Must not contain unprocessed
       *     percent returns. Each element should represent log(max(1 + r_i, ruin_eps)).
       *
       * @return
       *     The profit-factor statistic, either compressed or raw depending on
       *     the constructor configuration.
       */
      Decimal operator()(const std::vector<Decimal>& logBars) const
      {
	using DC = DecimalConstants<Decimal>;

	if (logBars.empty())
	  return DC::DecimalZero;

	boost::container::small_vector<Decimal, 32> loss_mags;
	Decimal sum_log_wins   = DC::DecimalZero;
	Decimal sum_log_losses = DC::DecimalZero;

	for (const auto& lr : logBars)
	  {
	    if (lr > DC::DecimalZero)
	      sum_log_wins += lr;
	    else if (lr < DC::DecimalZero)
	      {
		sum_log_losses += lr;
		loss_mags.push_back(-lr);
	      }
	  }

	// identical prior / denom logic as computeLogProfitFactorRobust,
	// but now we’re already in log-space so no std::log(1+r) calls here.
	Decimal prior_loss_mag = DC::DecimalZero;

	if (!loss_mags.empty())
	  {
	    const std::size_t mid = loss_mags.size() / 2;
	    std::nth_element(loss_mags.begin(),
			     loss_mags.begin() + static_cast<std::ptrdiff_t>(mid),
			     loss_mags.end());
	    const Decimal med = loss_mags[mid];
	    prior_loss_mag = med * Decimal(m_priorStrength);
	  }
	else
	  {
	    Decimal assumed_mag;
	    if (m_defaultLossMag > 0.0)
	      assumed_mag = Decimal(m_defaultLossMag);
	    else
	      assumed_mag = Decimal(std::max(-std::log(m_ruinEps), m_denomFloor));

	    prior_loss_mag = assumed_mag * Decimal(m_priorStrength);
	  }

	const Decimal numer = sum_log_wins;
	Decimal denom       = num::abs(sum_log_losses) + prior_loss_mag;

	const Decimal d_floor(m_denomFloor);
	if (denom < d_floor) denom = d_floor;

	Decimal pf = (denom > DC::DecimalZero) ? (numer / denom) : DC::DecimalZero;
	if (m_compressResult)
	  return std::log(DC::DecimalOne + pf);
	return pf;
      }

    private:
      bool   m_compressResult;
      double m_ruinEps;
      double m_denomFloor;
      double m_priorStrength;
      double m_defaultLossMag;
    };
    
    /**
     * @brief Computes the Log Profit Factor from a series of returns.
     * @details This variant takes the logarithm of each return (plus one) before summing,
     * which gives less weight to extreme outliers. Returns where (1+r) is not
     * positive are ignored.
     * Formula: LPF = sum(log(1+r>0)) / abs(sum(log(1+r<0))).
     * @param xs A vector of returns.
     * @param compressResult If true, applies an additional natural log compression to the final result.
     * @return The calculated Log Profit Factor as a Decimal.
     */
    static Decimal computeLogProfitFactor(const std::vector<Decimal>& xs, bool compressResult = false)
    {
      Decimal lw(DecimalConstants<Decimal>::DecimalZero);
      Decimal ll(DecimalConstants<Decimal>::DecimalZero);

      for (auto r : xs)
	{
	  double m = 1 + num::to_double(r);
	  if (m <= 0)
	    continue;
	    
	  Decimal lr(std::log(m));
	  if (r > DecimalConstants<Decimal>::DecimalZero)
	    lw += lr;
	  else
	    ll += lr;
	}
	
      return computeFactor(lw, ll, compressResult);
    }

    /**
     * @brief Computes the Profit Factor and the required Win Rate (Profitability).
     * @details This function efficiently calculates both the Profit Factor and the minimum
     * win rate required for a strategy to be profitable, based on the relationship
     * between Profit Factor (PF) and Payoff Ratio (Rwl).
     * Formula: P = 100 * PF / (PF + Rwl).
     * @param xs A vector of returns.
     * @return A std::tuple<Decimal, Decimal> containing:
     * - get<0>: The Profit Factor.
     * - get<1>: The required Win Rate (Profitability) as a percentage.
     */
    static std::tuple<Decimal, Decimal> computeProfitability(const std::vector<Decimal>& xs)
    {
      if (xs.empty())
        {
	  return std::make_tuple(DecimalConstants<Decimal>::DecimalZero, DecimalConstants<Decimal>::DecimalZero);
        }

      Decimal grossWins(DecimalConstants<Decimal>::DecimalZero);
      Decimal grossLosses(DecimalConstants<Decimal>::DecimalZero);
      std::size_t numWinningTrades = 0;
      std::size_t numLosingTrades = 0;

      // Efficiently calculate all required components in one loop
      for (const auto& r : xs)
        {
	  if (r > DecimalConstants<Decimal>::DecimalZero)
            {
	      grossWins += r;
	      numWinningTrades++;
            }
	  else if (r < DecimalConstants<Decimal>::DecimalZero)
            {
	      grossLosses += r;
	      numLosingTrades++;
            }
        }

      // 1. Calculate Profit Factor (PF) by reusing the existing helper
      Decimal pf = computeFactor(grossWins, grossLosses, false);

      // 2. Calculate Payoff Ratio (Rwl = AWT / ALT)
      Decimal rwl(DecimalConstants<Decimal>::DecimalZero);
      if (numWinningTrades > 0 && numLosingTrades > 0)
        {
	  Decimal awt = grossWins / Decimal(numWinningTrades);
	  Decimal alt = num::abs(grossLosses) / Decimal(numLosingTrades);

	  // Avoid division by zero if average loss is zero
	  if (alt > DecimalConstants<Decimal>::DecimalZero)
            {
	      rwl = awt / alt;
            }
        }

      // 3. Calculate Profitability (P) using the provided formula: P = 100 * PF / (PF + Rwl)
      Decimal p(DecimalConstants<Decimal>::DecimalZero);
      Decimal denominator = pf + rwl;
      if (denominator > DecimalConstants<Decimal>::DecimalZero)
        {
	  p = (DecimalConstants<Decimal>::DecimalOneHundred * pf) / denominator;
        }

      return std::make_tuple(pf, p);
    }

    /**
     * @brief Computes the Log Profit Factor and Log Profitability.
     * @details This is the log-space equivalent of `computeProfitability`. It calculates
     * the ratio of summed log-returns (Log Profit Factor) and a measure of
     * efficiency (Log Profitability) based on the Log Payoff Ratio.
     * @param xs A vector of returns.
     * @return A std::tuple<Decimal, Decimal> containing:
     * - get<0>: The Log Profit Factor.
     * - get<1>: The Log Profitability as a percentage.
     */
    static std::tuple<Decimal, Decimal> computeLogProfitability(const std::vector<Decimal>& xs)
    {
      if (xs.empty()) {
	return std::make_tuple(DecimalConstants<Decimal>::DecimalZero, DecimalConstants<Decimal>::DecimalZero);
      }

      Decimal log_wins(DecimalConstants<Decimal>::DecimalZero);
      Decimal log_losses(DecimalConstants<Decimal>::DecimalZero);
      size_t num_wins = 0;
      size_t num_losses = 0;

      for (const auto& r : xs) {
	double m = 1 + num::to_double(r);
	if (m <= 0) continue;

	Decimal lr(std::log(m));
	if (r > DecimalConstants<Decimal>::DecimalZero) {
	  log_wins += lr;
	  num_wins++;
	} else if (r < DecimalConstants<Decimal>::DecimalZero) {
	  log_losses += lr;
	  num_losses++;
	}
      }

      // 1. Calculate Log Profit Factor (LPF)
      Decimal lpf = computeFactor(log_wins, log_losses, false);

      // 2. Calculate Log Payoff Ratio (LRWL)
      Decimal lrwl(DecimalConstants<Decimal>::DecimalZero);
      if (num_wins > 0 && num_losses > 0) {
	Decimal avg_log_win = log_wins / Decimal(num_wins);
	Decimal avg_log_loss = num::abs(log_losses) / Decimal(num_losses);
	if (avg_log_loss > DecimalConstants<Decimal>::DecimalZero) {
	  lrwl = avg_log_win / avg_log_loss;
	}
      }

      // 3. Calculate Log Profitability (P_log)
      Decimal p_log(DecimalConstants<Decimal>::DecimalZero);
      Decimal denominator = lpf + lrwl;
      if (denominator > DecimalConstants<Decimal>::DecimalZero) {
	p_log = (DecimalConstants<Decimal>::DecimalOneHundred * lpf) / denominator;
      }

      return std::make_tuple(lpf, p_log);
    }

    /**
     * @brief Compute the autocorrelation function (ACF) ρ[k] for k=0..maxLag, in Decimal.
     *
     * @tparam Decimal  dec::decimal<...> (or compatible numeric) type.
     * @param monthly   Vector of monthly returns (decimal fractions, e.g. 0.012 == +1.2%).
     * @param maxLag    Maximum lag to compute (capped at n-1).
     * @return          std::vector<Decimal> with length (min(maxLag,n-1)+1), ρ[0] == 1.
     *
     * Definition:
     *   ρ(k) = sum_{t=k}^{n-1} (x_t - μ)(x_{t-k} - μ) / sum_{t=0}^{n-1} (x_t - μ)^2
     */
    static std::vector<Decimal>
    computeACF(const std::vector<Decimal>& monthly, std::size_t maxLag)
    {
      const std::size_t n = monthly.size();
      if (n < 2) {
        throw std::invalid_argument("computeACF: need at least 2 months to compute ACF.");
      }

      const std::size_t L = (maxLag >= (n - 1)) ? (n - 1) : maxLag;

      // Mean in Decimal
      Decimal mu = Decimal(0);
      for (const auto& v : monthly)
	mu += v;
      
      mu /= Decimal(static_cast<double>(n)); // safe: Decimal / Decimal

      // Centered values and denominator (sum of squares)
      std::vector<Decimal> xd(n);
      Decimal denom = Decimal(0);
      for (std::size_t t = 0; t < n; ++t)
	{
	  xd[t] = monthly[t] - mu;
	  denom += xd[t] * xd[t];
	}

      std::vector<Decimal> acf(L + 1, Decimal(0));
      if (denom == Decimal(0)) {
        // Constant series: define ρ[0]=1 and others = 0
        acf[0] = Decimal(1);
        return acf;
      }

      acf[0] = Decimal(1);

      for (std::size_t k = 1; k <= L; ++k)
	{
	  Decimal num = Decimal(0);
	  for (std::size_t t = k; t < n; ++t)
	    num += xd[t] * xd[t - k];

	  acf[k] = num / denom;   // stays in Decimal
	}

      return acf;
    }

    /**
     * @brief Suggest a stationary-bootstrap mean block length from an ACF curve.
     *
     * Heuristic:
     *  - Noise band ≈ 2/sqrt(nSamples).
     *  - Let k* be the largest lag with |ρ(k)| > band, clamp to [minL, maxL].
     *
     * Works with ACF stored as Decimal or double.
     */
    static unsigned
    suggestStationaryBlockLengthFromACF(const std::vector<Decimal>& acf,
    	std::size_t nSamples,
    	unsigned minL = 2,
    	unsigned maxL = 6)
    {
      if (acf.empty() || nSamples == 0)
        throw std::invalid_argument("suggestStationaryBlockLengthFromACF: empty ACF or nSamples=0.");

      const double thresh = 2.0 / std::sqrt(static_cast<double>(nSamples));
      unsigned k_star = 1;

      // skip ρ[0]
      for (std::size_t k = 1; k < acf.size(); ++k)
	{
	  const double rk = std::fabs(acf[k].getAsDouble());
	  if (rk > thresh)
	    k_star = static_cast<unsigned>(k);
	}

      if (k_star < minL)
	k_star = minL;

      if (k_star > maxL)
	k_star = maxL;

      return k_star;
    }
    
    static std::tuple<Decimal, Decimal> getBootStrappedProfitability(const std::vector<Decimal>& barReturns,
								     std::function<std::tuple<Decimal, Decimal>(const std::vector<Decimal>&)> statisticFunc,
								     size_t numBootstraps = 100)
    {
      thread_local static randutils::mt19937_rng rng;
      return getBootstrappedTupleStatistic(barReturns, statisticFunc, numBootstraps, rng);
    }

    static std::tuple<Decimal, Decimal> getBootStrappedProfitability(const std::vector<Decimal>& barReturns,
								     std::function<std::tuple<Decimal, Decimal>(const std::vector<Decimal>&)> statisticFunc,
								     size_t numBootstraps,
								     uint64_t seed)
    {
      randutils::mt19937_rng rng(seed);
      return getBootstrappedTupleStatistic(barReturns, statisticFunc, numBootstraps, rng);
    }

    static std::tuple<Decimal, Decimal> getBootStrappedLogProfitability(const std::vector<Decimal>& barReturns,
									size_t numBootstraps = 100)
    {
      thread_local static randutils::mt19937_rng rng;
      return getBootstrappedTupleStatistic(barReturns, &StatUtils<Decimal>::computeLogProfitability, numBootstraps, rng);
    }

    static std::tuple<Decimal, Decimal> getBootStrappedLogProfitability(const std::vector<Decimal>& barReturns,
									size_t numBootstraps,
									uint64_t seed)
    {
      randutils::mt19937_rng rng(seed);
      return getBootstrappedTupleStatistic(barReturns, &StatUtils<Decimal>::computeLogProfitability, numBootstraps, rng);
    }

    // 🌐 Overload 1: Random bootstrap using thread-local rng
    static std::vector<Decimal> bootstrapWithReplacement(const std::vector<Decimal>& input, size_t sampleSize = 0)
    {
      thread_local static randutils::mt19937_rng rng;
      return bootstrapWithRNG(input, sampleSize, rng);
    }

    // 🔐 Overload 2: Deterministic bootstrap using seed
    static std::vector<Decimal> bootstrapWithReplacement(const std::vector<Decimal>& input, size_t sampleSize, uint64_t seed)
    {
      std::array<uint32_t, 2> seed_data = {static_cast<uint32_t>(seed), static_cast<uint32_t>(seed >> 32)};
      randutils::seed_seq_fe128 seed_seq(seed_data.begin(), seed_data.end());
      randutils::mt19937_rng rng(seed_seq);
      return bootstrapWithRNG(input, sampleSize, rng);
    }

    static Decimal getBootStrappedStatistic(const std::vector<Decimal>& barReturns,
					    std::function<Decimal(const std::vector<Decimal>&)> statisticFunc,
					    size_t numBootstraps = 100)
    {
      if (barReturns.size() < 5)
	return DecimalConstants<Decimal>::DecimalZero;

      std::vector<Decimal> statistics;
      statistics.reserve(numBootstraps);
	
      for (size_t i = 0; i < numBootstraps; ++i)
	{
	  auto sample = StatUtils<Decimal>::bootstrapWithReplacement(barReturns);
	  statistics.push_back(statisticFunc(sample));
	}

      return mkc_timeseries::MedianOfVec(statistics);
    }

    /**
     * @brief Computes the arithmetic mean of a vector of Decimal values.
     * @param data The vector of data points.
     * @return The mean of the data.
     */
    static Decimal computeMean(const std::vector<Decimal>& data)
    {
      if (data.empty()) {
	return DecimalConstants<Decimal>::DecimalZero;
      }
      Decimal sum = std::accumulate(data.begin(), data.end(), DecimalConstants<Decimal>::DecimalZero);
      return sum / Decimal(data.size());
    }

    /**
     * @brief Computes the (unbiased) sample variance given a precomputed mean.
     *        Returns 0 when data.size() < 2.
     */
    static Decimal computeVariance(const std::vector<Decimal>& data, const Decimal& mean)
    {
      const size_t n = data.size();
      if (n < 2)
        return DecimalConstants<Decimal>::DecimalZero;

      Decimal sq_sum = std::accumulate(data.begin(), data.end(), DecimalConstants<Decimal>::DecimalZero,
				       [&mean](const Decimal& acc, const Decimal& val) {
					 const Decimal diff = (val - mean);
					 return acc + diff * diff;
				       });

      // Unbiased sample variance (N-1)
      return sq_sum / Decimal(n - 1);
    }

    /**
     * @brief Single-pass, numerically stable mean and (unbiased) variance via Welford.
     *        Returns {0,0} for empty; variance=0 for n<2.
     */
    static std::pair<Decimal, Decimal> computeMeanAndVariance(const std::vector<Decimal>& data)
    {
      const size_t n = data.size();
      if (n == 0)
        return {DecimalConstants<Decimal>::DecimalZero, DecimalConstants<Decimal>::DecimalZero};

      // Accumulate in long double for stability; convert back to Decimal at the end.
      long double mean = 0.0L;
      long double m2   = 0.0L;
      size_t k = 0;

      for (const auto& d : data)
 {
   const long double x = static_cast<long double>(num::to_double(d));
   ++k;
   const long double delta  = x - mean;
   mean += delta / static_cast<long double>(k);
   const long double delta2 = x - mean;
   m2 += delta * delta2;
 }

      if (k < 2)
        return {Decimal(mean), DecimalConstants<Decimal>::DecimalZero};

      const long double var = m2 / static_cast<long double>(k - 1);  // unbiased
      return {Decimal(mean), Decimal(var)};
    }

    /**
     * @brief Computes Moors' Kurtosis (K_Moors) and returns the excess.
     *
     * @details Moors' Kurtosis is a robust, quantile-based measure of a distribution's peakedness
     * and tail heaviness, providing an alternative to the traditional (Fisher's) moment-based kurtosis
     * that is less sensitive to extreme outliers.
     *
     * The formula uses the spread between octiles (O1, O3, O5, O7) normalized by the
     * interquartile range (IQR = Q3 - Q1):
     * $$K_{Moors} = (O7 - O5) + (O3 - O1) / (Q3 - Q1)$$
     *
     * This function returns the **excess Moors' Kurtosis**, which is defined as:
     * $$K_{Excess} = K_{Moors} - 1.233$$
     * where $1.233$ is the theoretical value of Moors' Kurtosis for the **Normal distribution**.
     *
     * A positive excess kurtosis ($K_{Moors} > 1.233$) indicates **leptokurtosis**
     * (heavier tails and a more pronounced peak than a Normal distribution), which is common
     * in financial return series.
     *
     * @param v A constant reference to a vector of Decimal values.
     * @return The excess Moors' Kurtosis ($K_{Moors} - 1.233$) as a Decimal.
     *         Returns DecimalZero if the sample size is < 7 or the denominator ($Q3-Q1$) is zero.
     *
     * @see Moors, J.J.A. (1988). "A quantile alternative for kurtosis". *The Statistician*, 37(1), 25-32.
     *      (Defines the $K_{Moors}$ statistic and its normal-case value.)
     */
    static Decimal getMoorsKurtosis(const std::vector<Decimal>& v)
    {
      using Consts = DecimalConstants<Decimal>;
      
      // Need at least 7 points for stable octile estimation (N=8 guarantees integer indices for O1/O7).
      if (v.size() < 7) 
          return Consts::DecimalZero;
      
      // K_Moors for Normal is approx 1.233
      const Decimal NormalKurtosis = Decimal(1.233);
      
      // Quantile function takes a copy and sorts internally. Pass the copy v.
      
      // Quartiles (for the denominator)
      const Decimal q1 = StatUtils<Decimal>::quantile(v, 0.25);
      const Decimal q3 = StatUtils<Decimal>::quantile(v, 0.75);
      
      // Octiles (for the numerator)
      const Decimal o1 = StatUtils<Decimal>::quantile(v, 0.125);
      const Decimal o3 = StatUtils<Decimal>::quantile(v, 0.375);
      const Decimal o5 = StatUtils<Decimal>::quantile(v, 0.625);
      const Decimal o7 = StatUtils<Decimal>::quantile(v, 0.875);
      
      const Decimal denominator = q3 - q1;
      if (denominator == Consts::DecimalZero)
          return Consts::DecimalZero;
          
      const Decimal numerator   = (o7 - o5) + (o3 - o1);
      const Decimal moors_kurt = numerator / denominator;
      
      // Return EXCESS Moors' Kurtosis: K_Moors - K_Normal (1.233)
      return moors_kurt - NormalKurtosis;
    }

    /**
     * @brief Computes the Bowley (Quartile) Skewness (B).
     *
     * @details Bowley Skewness (also known as the Quartile Coefficient of Skewness) is a robust
     * measure of asymmetry based purely on the quartiles of the data. Since it does not rely
     * on the mean or higher moments, it is highly **resistant to outliers** and is preferred
     * for heavy-tailed or highly skewed financial data.
     *
     * The formula is:
     * $$B = (Q_1 + Q_3 - 2 \cdot Q_2) / (Q_3 - Q_1)$$
     * where $Q_1$, $Q_2$ (median), and $Q_3$ are the 25th, 50th, and 75th percentiles.
     *
     * The coefficient ranges from $-1$ to $+1$.
     * *   **$B > 0$**: Positively skewed (right-skewed); longer/heavier upper tail.
     * *   **$B < 0$**: Negatively skewed (left-skewed); longer/heavier lower tail (e.g., larger downside moves).
     * *   **$B \approx 0$**: Symmetric distribution.
     *
     * @param v A constant reference to a vector of Decimal values.
     * @return The Bowley Skewness (B) as a Decimal. Returns DecimalZero if the sample size
     *         is < 4 or the interquartile range ($Q3-Q1$) is numerically tiny.
     *
     * @see Bowley, A. L. (1920). *Elements of Statistics*. P. S. King & Son.
     *      (One of the early proponents of this measure for descriptive statistics.)
     */
    static Decimal getBowleySkewness(const std::vector<Decimal>& v)
    {
      using Consts = DecimalConstants<Decimal>;

      const std::size_t n = v.size();
      if (n < 4)
        return Consts::DecimalZero;

      const Decimal q1 = StatUtils<Decimal>::quantile(v, 0.25);
      const Decimal q2 = StatUtils<Decimal>::quantile(v, 0.50);
      const Decimal q3 = StatUtils<Decimal>::quantile(v, 0.75);

      const Decimal denominator = q3 - q1;
      if (denominator == Consts::DecimalZero)
        return Consts::DecimalZero;

      // Optional: small “tiny” guard on the denominator
      const double dDen = std::fabs(num::to_double(denominator));
      const double tiny = 1e-12;
      if (dDen < tiny)
        return Consts::DecimalZero;

      const Decimal numerator = q1 + q3 - q2 * Decimal(2.0);
      return numerator / denominator;
    }

    /**
     * @brief Measures asymmetry in tail spread between lower and upper sides.
     *
     * @details
     *   Uses 10–50–90% quantiles by default:
     *
     *     lowerSpan = Q50 - Q10
     *     upperSpan = Q90 - Q50
     *
     *   Returns:
     *     tailRatio = max(lowerSpan, upperSpan) / min(lowerSpan, upperSpan)
     *
     *   If either span is non-positive or too small: returns 1.0.
     *
     *   Interpretation:
     *     ~1.0  → roughly symmetric tails around the median
     *     >>1.0 → one side is much more stretched (heavier or more volatile tail)
     */
    static double getTailSpanRatio(const std::vector<Decimal>& v,
				   double pLow  = 0.10,
				   double pHigh = 0.90)
    {
      const std::size_t n = v.size();
      if (n < 8)
        return 1.0;  // too small to say much

      const Decimal qLow  = StatUtils<Decimal>::quantile(v, pLow);
      const Decimal qMed  = StatUtils<Decimal>::quantile(v, 0.50);
      const Decimal qHigh = StatUtils<Decimal>::quantile(v, pHigh);

      const double dLow  = num::to_double(qLow);
      const double dMed  = num::to_double(qMed);
      const double dHigh = num::to_double(qHigh);

      const double lowerSpan = dMed - dLow;
      const double upperSpan = dHigh - dMed;

      // If either span is non-positive or extremely small, treat as symmetric.
      const double tiny = 1e-12 * std::max(1.0, std::fabs(dMed));
      if (lowerSpan <= tiny || upperSpan <= tiny)
        return 1.0;

      const double lo = std::min(lowerSpan, upperSpan);
      const double hi = std::max(lowerSpan, upperSpan);
      return hi / lo;
    }

    struct QuantileShape
    {
      double bowleySkew        {0.0};
      double tailRatio         {1.0};
      bool   hasStrongAsymmetry{false};
      bool   hasHeavyTails     {false};
    };

    /**
     * @brief Robust, quantile-based shape summary (Bowley skew + tail span ratio).
     *
     * @details This function combines two highly robust, non-moment-based statistics—
     * **Bowley Skewness** and the **Tail Span Ratio**—to provide a comprehensive summary
     * of a distribution's shape, specifically its asymmetry and tail heaviness.
     *
     * 1.  **Bowley Skew (bowleySkew):** Measures overall asymmetry in the body of the distribution.
     * 2.  **Tail Span Ratio (tailRatio):** Quantifies tail asymmetry by comparing the
     *     spread of the lower tail ($Q_{50}-Q_{10}$) to the upper tail ($Q_{90}-Q_{50}$).
     *     A high ratio (>> 1) indicates that one tail is much more extended/volatile than the other.
     *
     * The resulting struct classifies the distribution's shape based on two critical flags:
     * *   `hasStrongAsymmetry`: True if $| \text{bowleySkew} | >= \text{bowleyThreshold}$.
     * *   `hasHeavyTails`: True if $\text{tailRatio} >= \text{tailRatioThreshold}$.
     *
     * This summary is particularly valuable in finance for assessing the non-Normal characteristics
     * of returns (e.g., negative skew and high tail risk) without being corrupted by rare, massive outliers.
     *
     * @param v A constant reference to a vector of Decimal values.
     * @param bowleyThreshold Absolute threshold for Bowley Skewness to be considered "strong asymmetry" (default: 0.30).
     * @param tailRatioThreshold Threshold for the Tail Span Ratio to be considered "heavy tails" (default: 2.50).
     * @return A `QuantileShape` struct containing the computed statistics and boolean flags.
     */
    static inline QuantileShape
    computeQuantileShape(const std::vector<Decimal>& v,
			 double bowleyThreshold    = 0.30,
			 double tailRatioThreshold = 2.50)
    {
      QuantileShape out;

      const std::size_t n = v.size();
      if (n < 8)
        return out;

      // Bowley skew via standalone helper
      const Decimal bowleyDec = StatUtils<Decimal>::getBowleySkewness(v);
      const double  bowley    = num::to_double(bowleyDec);
      out.bowleySkew          = bowley;

      // Tail span ratio via standalone helper
      const double tailRatio  = StatUtils<Decimal>::getTailSpanRatio(v, 0.10, 0.90);
      out.tailRatio           = tailRatio;

      out.hasStrongAsymmetry  = (std::fabs(bowley)   >= bowleyThreshold);
      out.hasHeavyTails       = (tailRatio           >= tailRatioThreshold);

      return out;
    }

    /**
     * @brief Computes robust, quantile-based skewness and excess kurtosis.
     *
     * @details This function calculates two non-moment-based measures of distribution shape:
     * **Bowley Skewness** and **Moors' Excess Kurtosis**.
     *
     * Unlike traditional Fisher's moment-based statistics, these quantile-based measures are
     * highly **robust to extreme outliers** in the data, making them particularly suitable
     * for analyzing real-world financial returns where catastrophic events can heavily
     * distort moment-based results.
     *
     * 1.  **Skewness (First element):** Calculated using `getBowleySkewness`. This measures the
     *     asymmetry of the distribution based on quartiles. A negative value indicates a
     *     heavier left (negative) tail, common in financial losses.
     * 2.  **Excess Kurtosis (Second element):** Calculated using `getMoorsKurtosis`. This measures
     *     the heaviness of the tails relative to the Normal distribution (where $K_{Excess} \approx 0$).
     *     A positive value indicates fatter tails (leptokurtosis).
     *
     * **Robustness Note:** The result is always based on the more stable quantile methods
     * (Bowley/Moors), not the standard Fisher's moments, which are also available in
     * `computeSkewAndExcessKurtosisFisher`.
     *
     * @param v A constant reference to a vector of Decimal values.
     * @return A `std::pair<double, double>` containing:
     *         - **First:** Bowley Skewness (B).
     *         - **Second:** Moors' Excess Kurtosis ($K_{Excess}$).
     *         Returns $\{0.0, 0.0\}$ if the sample size is less than 7 (the minimum required
     *         for a stable Moors' Kurtosis calculation).
     * @see getBowleySkewness
     * @see getMoorsKurtosis
     */
    static inline std::pair<double,double>
    computeSkewAndExcessKurtosis(const std::vector<Decimal>& v)
    {
      const std::size_t n = v.size();
      
      // Minimum sample size is n=7 for the octile-based Moors' Kurtosis.
      if (n < 7) {
        // Moment-based calculations for N < 7 are highly unstable; return zero.
	    return {0.0, 0.0};
      }
      
      // 1. Compute Bowley Skewness (B) - Robust Skewness
      const Decimal skewDec = StatUtils<Decimal>::getBowleySkewness(v);
      const double skew = num::to_double(skewDec);
      
      // 2. Compute Moors' Excess Kurtosis (K_Moors - K_Normal) - Robust Excess Kurtosis
      const Decimal exkurtDec = StatUtils<Decimal>::getMoorsKurtosis(v);
      const double exkurt = num::to_double(exkurtDec);

      return { skew, exkurt };
    }

    // Fisher bias-corrected *sample* skewness and *excess* kurtosis.
    // Returns {skew, exkurt}. For n<4 or zero variance, returns {0,0}.
    static inline std::pair<double,double>
    computeSkewAndExcessKurtosisFisher(const std::vector<Decimal>& v)
    {
      const std::size_t n = v.size();
      if (n < 4)
	return {0.0, 0.0};

      // Use the fast, numerically-stable dispatcher you already have.
      auto [meanDec, varDec] = StatUtils<Decimal>::computeMeanAndVarianceFast(v);
      const double mu  = num::to_double(meanDec);
      double var       = num::to_double(varDec);

      if (var <= 0.0)
	return {0.0, 0.0};

      const double s   = std::sqrt(var);
      
      long double m3 = 0.0L, m4 = 0.0L;
      for (const auto& xi : v) {
	const long double z  = static_cast<long double>(num::to_double(xi) - mu);
	const long double z2 = z * z;
	m3 += z * z2;
	m4 += z2 * z2;
      }

      const long double nl = static_cast<long double>(n);
      // Fisher (bias-corrected) sample skewness g1
      const long double g1 = (nl / ((nl - 1.0L) * (nl - 2.0L))) * (m3 / std::pow(static_cast<long double>(s), 3));
      // Fisher (bias-corrected) *excess* kurtosis g2
      const long double g2 =
	( (nl * (nl + 1.0L)) / ((nl - 1.0L) * (nl - 2.0L) * (nl - 3.0L)) ) * (m4 / std::pow(static_cast<long double>(s), 4))
	- ( 3.0L * std::pow(nl - 1.0L, 2) ) / ( (nl - 2.0L) * (nl - 3.0L) );
      
      return { static_cast<double>(g1), static_cast<double>(g2) };
    }

    /**
     * @brief Generic moment-based skewness for a vector of numeric-like values.
     *
     * Template T may be `double` or a Decimal-like type that supports basic
     * arithmetic and comparison. Returns the sample skewness as type T.
     */
    template <typename T>
    static inline T computeSkewness(const std::vector<T>& data, T mean, T se)
    {
      const std::size_t n = data.size();
      if (n < 3) return T(0);
      if (!(se > T(0))) return T(0);

      T m3 = T(0);
      for (const auto& v : data)
      {
        const T d = v - mean;
        m3 += d * d * d;
      }

      m3 /= T(static_cast<double>(n));

      const T denom = se * se * se;
      if (!(denom > T(0))) return T(0);

      return m3 / denom;
    }

    /**
     * @brief Generic median for a vector of numeric-like values.
     *
     * Makes a local copy (so caller data is not mutated), sorts, and returns
     * the median as type T.
     */
    template <typename T>
    static inline T computeMedian(std::vector<T> data)
    {
      if (data.empty()) return T(0);
      std::sort(data.begin(), data.end());
      const std::size_t n = data.size();
      if (n % 2 == 0)
      {
        const std::size_t mid1 = n / 2 - 1;
        const std::size_t mid2 = n / 2;
        return (data[mid1] + data[mid2]) / T(2.0);
      }
      else
      {
        return data[n / 2];
      }
    }

    /**
     * @brief Median for a vector of numeric-like values that is already sorted.
     */
    template <typename T>
    static inline T computeMedianSorted(const std::vector<T>& sorted_data)
    {
      if (sorted_data.empty()) return T(0);
      const std::size_t n = sorted_data.size();
      if (n % 2 == 0)
      {
        const std::size_t mid1 = n / 2 - 1;
        const std::size_t mid2 = n / 2;
        return (sorted_data[mid1] + sorted_data[mid2]) / T(2.0);
      }
      else
      {
        return sorted_data[n / 2];
      }
    }

    // Pragmatic heavy-tail detector: |skew| > SKEW_T or exkurt > EXKURT_T.
    static inline bool
    hasHeavyTails(const std::vector<Decimal>& v,
		  double SKEW_T = 0.8,
		  double EXKURT_T = 2.0)
    {
      const auto [sk, ek] = computeSkewAndExcessKurtosis(v);
      return (std::fabs(sk) > SKEW_T) || (ek > EXKURT_T);
    }

    /**
     * @brief Computes the sample standard deviation of a vector of Decimal values.
     * @param data The vector of data points.
     * @param mean The pre-computed mean of the data.
     * @return The sample standard deviation.
     */
    static Decimal computeStdDev(const std::vector<Decimal>& data, const Decimal& mean)
    {
      const Decimal var = computeVariance(data, mean);
      const double  v   = num::to_double(var);
      return (v > 0.0) ? Decimal(std::sqrt(v)) : DecimalConstants<Decimal>::DecimalZero; 
    }

    /**
     * @brief Computes a quantile from a vector of values using linear interpolation.
     *
     * This method treats the dataset as a sample from a continuous distribution. It
     * calculates a fractional index based on the quantile `q`. If the index is an
     * integer, the value at that index is returned. If the index is fractional,
     * the function linearly interpolates between the values at the two surrounding
     * integer indices. This function operates on a copy of the input vector.
     *
     * @tparam Decimal The floating-point or custom decimal type of the data.
     * @param v A std::vector<Decimal> containing the data. The vector is passed
     * by value (copied) so the original vector is not modified.
     * @param q The desired quantile, which must be between 0.0 and 1.0 (inclusive).
     * Values outside this range will be clamped.
     * @return The computed quantile value as type Decimal. Returns Decimal(0) if
     * the input vector is empty.
     */

    static Decimal
    quantile(std::vector<Decimal> v, double q)
    {
      // --- Pre-conditions and Setup ---

      // If the vector is empty, there is no quantile to compute. Return a default value.
      if (v.empty())
	{
	  return Decimal(0);
	}

      // Clamp the quantile q to the valid range [0.0, 1.0] to prevent out-of-bounds access.
      q = std::min(std::max(q, 0.0), 1.0);

      // --- Index Calculation ---

      // Calculate the continuous, zero-based index for the quantile.
      // The formula q * (N-1) is a common definition for sample quantiles.
      // For a vector of size N, indices range from 0 to N-1.
      const double idx = q * (static_cast<double>(v.size()) - 1.0);

      // Find the integer indices that surround the continuous index `idx`.
      // `lo` is the index of the data point at or just below `idx`.
      // `hi` is the index of the data point at or just above `idx`.
      const auto lo = static_cast<std::size_t>(std::floor(idx));
      const auto hi = static_cast<std::size_t>(std::ceil(idx));

      // --- Value Extraction ---

      // Find the value at the lower-bound index `lo`.
      // std::nth_element is an efficient O(N) algorithm that rearranges the vector
      // such that the element at the n-th position is the one that *would be*
      // in that position in a fully sorted vector. All elements before it are
      // less than or equal to it. We don't need a full sort.
      std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(lo), v.end());
      const Decimal vlo = v[lo];

      // If `lo` and `hi` are the same, `idx` was an exact integer.
      // The quantile is simply the value at that index, so no interpolation is needed.
      if (hi == lo)
	{
	  return vlo;
	}

      // If interpolation is needed, find the value at the upper-bound index `hi`.
      // We call nth_element again. While this might seem inefficient, it's often
      // faster than a full sort, especially if `lo` and `hi` are close. The elements
      // are already partially sorted from the first call.
      std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(hi), v.end());
      const Decimal vhi = v[hi];

      // --- Linear Interpolation ---

      // Calculate the interpolation weight `w`. This is the fractional part of `idx`,
      // representing how far `idx` is from `lo` on the way to `hi`.
      const Decimal w = Decimal(idx - std::floor(idx));

      // The final result is the lower value plus a fraction of the difference
      // between the high and low values.
      return vlo + (vhi - vlo) * w;
    }

  private:
    /**
     * @brief A private helper function to compute a factor from gains and losses.
     * @details This function encapsulates the core ratio calculation and handles the
     * division-by-zero case where losses are zero. It can also apply
     * logarithmic compression to the result.
     * @param gains The total sum of positive values (wins).
     * @param losses The total sum of negative values (losses).
     * @param compressResult A boolean flag; if true, applies log(1 + pf) to the result.
     * @return The calculated factor as a Decimal.
     */
    static Decimal computeFactor(const Decimal& gains, const Decimal& losses, bool compressResult)
    {
      Decimal pf;
	
      // If there are no losses, return a fixed large number to signify a highly profitable state.
      if (losses == DecimalConstants<Decimal>::DecimalZero)
	{
	  pf = DecimalConstants<Decimal>::DecimalOneHundred;
	}
      else
	{
	  pf = gains / num::abs(losses);
	}
	
      // Apply log compression to the final profit factor if requested
      if (compressResult)
	return Decimal(std::log(num::to_double(DecimalConstants<Decimal>::DecimalOne + pf)));
      else
	return pf;
    }

    // 🔒 Internal bootstrap helper that takes a user-supplied RNG
    template<typename RNG>
    static std::vector<Decimal> bootstrapWithRNG(const std::vector<Decimal>& input, size_t sampleSize, RNG& rng)
    {
      if (input.empty())
	throw std::invalid_argument("bootstrapWithRNG: input vector must not be empty");

      if (sampleSize == 0)
	sampleSize = input.size();

      std::vector<Decimal> result;
      result.reserve(sampleSize);

      for (size_t i = 0; i < sampleSize; ++i) {
	size_t index = rng.uniform(size_t(0), input.size() - 1);
	result.push_back(input[index]);
      }

      return result;
    }

    // 🔒 Internal core logic for bootstrapping tuple-based statistics
    template<typename RNG>
    static std::tuple<Decimal, Decimal> getBootstrappedTupleStatistic(
								      const std::vector<Decimal>& barReturns,
								      std::function<std::tuple<Decimal, Decimal>(const std::vector<Decimal>&)> statisticFunc,
								      size_t numBootstraps,
								      RNG& rng)
    {
      if (barReturns.size() < 5)
	return std::make_tuple(DecimalConstants<Decimal>::DecimalZero, DecimalConstants<Decimal>::DecimalZero);

      std::vector<Decimal> stat1_values;
      std::vector<Decimal> stat2_values;
      stat1_values.reserve(numBootstraps);
      stat2_values.reserve(numBootstraps);

      for (size_t i = 0; i < numBootstraps; ++i)
	{
	  auto sample = bootstrapWithRNG(barReturns, 0, rng);
	  auto [stat1, stat2] = statisticFunc(sample);
	  stat1_values.push_back(stat1);
	  stat2_values.push_back(stat2);
	}

      Decimal median_stat1 = mkc_timeseries::MedianOfVec(stat1_values);
      Decimal median_stat2 = mkc_timeseries::MedianOfVec(stat2_values);

      return std::make_tuple(median_stat1, median_stat2);
    }
  };
}
