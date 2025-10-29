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
   * @brief Computes the geometric mean of a series of percentage returns with automatic
   *        ruin clipping and adaptive log-space winsorization for small sample sizes.
   *
   * The geometric mean represents the average compound return per period:
   * \f[
   *    \text{GeoMean} = \exp\left(\frac{1}{N}\sum_{i=1}^{N}\log(1 + r_i)\right) - 1
   * \f]
   * where \(r_i\) are individual period returns expressed as fractions (e.g., 0.01 = 1%).
   *
   * ### Key Features
   * - **Decimal-native math:** All operations (log, exp) use the Decimal overloads from
   *   `decimal_math.h` for high precision and consistency with other financial statistics.
   * - **Ruin-aware clamping:** If \(1 + r_i \le 0\), the growth factor is replaced by
   *   `ruin_eps` to prevent domain errors in the logarithm while strongly penalizing
   *   catastrophic losses.
   * - **Automatic winsorization (small-N safeguard):**
   *   - For sample sizes 20 ≤ N < 50, the most extreme low and high log(1+r) values are
   *     winsorized (clamped to the 2nd and (N-1)th order statistics).
   *   - For N < 20 or N ≥ 50, no winsorization is applied.
   *   - This stabilization reduces volatility in BCa bootstrap acceleration values (`a`)
   *     when bootstrapping small datasets without introducing noticeable bias.
   *
   * ### Typical Use
   * This statistic is intended for Monte-Carlo or bootstrap analysis where small-N series
   * of percent returns (e.g., daily or monthly) must be aggregated into a robust mean
   * performance measure.
   *
   * ### Example
   * @code
   * std::vector<dec::decimal<8>> returns = { 0.01, -0.005, 0.02, 0.00, -0.03 };
   * GeoMeanStat<dec::decimal<8>> stat;
   * auto gm = stat(returns);
   * std::cout << "Geometric mean = " << gm << std::endl;
   * @endcode
   *
   * ### Notes
   * - The returned value is a fractional return (e.g., 0.015 = +1.5%).
   * - Default ruin epsilon (`1e-8`) matches other robust statistics such as
   *   `computeLogProfitFactorRobust` for consistency.
   * - Default clipping is enabled (`clip_ruin = true`) so resampling pipelines never throw
   *   exceptions on bad data; strict validation can be enforced by setting
   *   `clip_ruin = false`.
   *
   * @tparam Decimal Decimal number type supporting `std::log` and `std::exp`.
   */
  template <class Decimal>
  struct GeoMeanStat
  {
    // Production defaults: clip ruin; epsilon coherent with other robust stats
    explicit GeoMeanStat(bool clip_ruin = true, double ruin_eps = 1e-8)
      : clip(clip_ruin), epsilon(ruin_eps) {}

    Decimal operator()(const std::vector<Decimal>& v) const
    {
      using DC = DecimalConstants<Decimal>;
      if (v.empty()) return DC::DecimalZero;

      const Decimal one  = DC::DecimalOne;
      const Decimal zero = DC::DecimalZero;
      const Decimal epsD = Decimal(epsilon);

      // 1) Build log(1+r) vector with ruin-aware clamp
      std::vector<Decimal> logs;
      logs.reserve(v.size());
      for (const auto& r : v)
	{
	  Decimal growth = one + r;
	  if (growth <= zero) {
	    if (!clip)
	      throw std::domain_error("GeoMeanStat: r <= -1 (log(1+r) undefined)");
	    growth = epsD;              // punitive clamp
	  }
	  logs.push_back(std::log(growth));  // Decimal-native log
	}

      // 2) Auto winsorization policy (only for small N)
      const std::size_t n = logs.size();
      const std::size_t k = chooseWinsorK(n);  // 0 or 1 under current policy

      if (k > 0)
	{
	  auto sorted = logs;                   // small N: full sort is fine
	  std::sort(sorted.begin(), sorted.end());
	  const Decimal lo = sorted[k];
	  const Decimal hi = sorted[n - 1 - k];
	  for (auto& x : logs)
	    {
	      if (x < lo)
		x = lo;
	      else if (x > hi)
		x = hi;
	    }
	}

      // 3) Mean of (possibly winsorized) logs → exp(mean) - 1
      Decimal sumLogs = DC::DecimalZero;
      for (const auto& x : logs)
	sumLogs += x;

      const Decimal avgLog = sumLogs / Decimal(static_cast<double>(n));
      return std::exp(avgLog) - one;
    }

  private:
    // One per side for 20..49; otherwise none
    static inline std::size_t chooseWinsorK(std::size_t n)
    {
      if (n >= 20 && n < 50)
	return 1;  // clamps just the most extreme low & high

      return 0;
    }

    bool   clip;
    double epsilon;
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
     * @brief Computes a robust, regularized Profit Factor in logarithmic space.
     * @details This function calculates a highly stabilized version of the Profit Factor
     * by transforming returns into log space (`log(1+r)`), which inherently tames
     * the impact of extreme outliers. It introduces several "tricks" to enhance
     * its statistical stability and honesty, especially for series with few trades
     * or catastrophic losses:
     *
     * 1.  **Log Transformation**: Mitigates the effect of massive outlier wins/losses.
     * 2.  **Ruin-Aware Clamping**: Instead of ignoring ruinous trades (r <= -100%),
     * it clamps them to a small value (`ruin_eps`), ensuring they are heavily
     * penalized rather than discarded.
     * 3.  **Bayesian Regularization**: It adds a "pseudo-loss" (a prior) to the
     * denominator. This prior is based on the median log-loss magnitude, which
     * prevents the Profit Factor from becoming infinite or unstable when there
     * are few or no observed losses. This makes the metric more reliable for
     * short return series.
     * 4.  **Denominator Floor**: An absolute minimum value for the denominator is
     * enforced to prevent division by pathologically small numbers.
     *
     * The resulting metric is less susceptible to distortion and provides a more
     * conservative and reliable measure of a strategy's profitability.
     *
     * @param xs A vector of returns.
     * @param compressResult If true (default), applies a final `log(1+x)`
     * compression to the result to make it more symmetric and easier to compare.
     * @param ruin_eps A small positive value to clamp the growth factor (1+r) to when
     * it is <= 0. This prevents undefined logs and penalizes ruinous outcomes.
     * @param denom_floor An absolute minimum value for the denominator (in log-space
     * units) to ensure numerical stability.
     * @param prior_strength A multiplier for the median log-loss used to create the
     * Bayesian prior. A value of 1.0 represents one "pseudo-loss" of typical magnitude.
     * @return The robust Log Profit Factor as a Decimal.
     */
    static Decimal computeLogProfitFactorRobust(const std::vector<Decimal>& xs,
						bool   compressResult = true,   // keep compressed scale by default
						double ruin_eps       = 1e-8,    // clamp for (1+r) <= 0
						double denom_floor    = 1e-6,    // absolute floor in log-space units
						double prior_strength = 1.0)     // ~1 “pseudo-loss” of median magnitude
    {
      using DC = DecimalConstants<Decimal>;

      if (xs.empty())
	return DC::DecimalZero;

      // --- Step 1: Build log(1+r) stream with ruin-aware clamping ---
      // Keep everything in Decimal where practical.
      const Decimal one     = DC::DecimalOne;
      const Decimal zero    = DC::DecimalZero;
      const Decimal d_ruin  = Decimal(ruin_eps);

      std::vector<Decimal> loss_magnitudes;      // store |-log(1+r)| for losses
      loss_magnitudes.reserve(xs.size());

      Decimal sum_log_wins   = DC::DecimalZero;  // Σ log(1+r) for r>0
      Decimal sum_log_losses = DC::DecimalZero;  // Σ log(1+r) for r<0 (will be <= 0)

      for (const auto& r : xs)
	{
	  // growth = 1 + r, clamped for ruin
	  Decimal growth = one + r;
	  if (growth <= zero)
	    growth = d_ruin;

	  const Decimal lr = std::log(growth);     // Decimal-native log

	  // Classify purely by raw return sign (clean & unambiguous):
	  if (r > zero)               // win: lr > 0 (except r tiny)
	    {
	      sum_log_wins += lr;
	    }
	  else if (r < zero)          // loss: lr < 0 (except clamped ruin -> large negative)
	    {
	      sum_log_losses += lr;
	      loss_magnitudes.push_back(-lr); // store positive magnitude
	    }
	  // r == 0 → lr == 0 → ignore (neither win nor loss)
	}

      // --- Step 2: Robust prior for the denominator (|Σ losses|) ---
      // If we observed losses, use median(|log-loss|).
      // Otherwise, tie prior to ruin model: magnitude ~ -log(ruin_eps).
      Decimal prior_loss_mag = DC::DecimalZero;
      if (!loss_magnitudes.empty())
	{
	  const std::size_t mid = loss_magnitudes.size() / 2;
	  std::nth_element(loss_magnitudes.begin(),
			   loss_magnitudes.begin() + static_cast<std::ptrdiff_t>(mid),
			   loss_magnitudes.end());
	  const Decimal med = loss_magnitudes[mid];
	  prior_loss_mag = med * Decimal(prior_strength);
	}
      else
	{
	  const Decimal mag = Decimal(std::max(-std::log(ruin_eps), denom_floor));
	  prior_loss_mag = mag * Decimal(prior_strength);
	}

      // --- Step 3: Form stabilized numerator/denominator ---
      const Decimal numer = sum_log_wins;
      Decimal denom       = num::abs(sum_log_losses) + prior_loss_mag;

      // Absolute floor in log-space units
      const Decimal d_floor = Decimal(denom_floor);
      if (denom < d_floor)
	denom = d_floor;

      Decimal pf = (denom > DC::DecimalZero) ? (numer / denom) : DC::DecimalZero;

      // --- Step 4: Optional final compression (log(1+pf)) ---
      if (compressResult)
	return std::log(DC::DecimalOne + pf);

      return pf;
    }
    
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
	  const long double x = static_cast<long double>(d.getAsDouble());
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
