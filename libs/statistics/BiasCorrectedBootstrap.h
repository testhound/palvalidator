// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
// BCa (Bias-Corrected and Accelerated) Bootstrap Implementation
// with pluggable resampling policies + policy-specific jackknife
//
// Default policy: IIDResampler (classic i.i.d. bootstrap)
// Alternative policy: StationaryBlockResampler (mean block length L)
// Statistic: pluggable (default: arithmetic mean via StatUtils)
//
// Developed with assistance from Gemini

#ifndef __BCA_BOOTSTRAP_H
#define __BCA_BOOTSTRAP_H

#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <limits>
#include <utility>
#include <cstddef>

#include "DecimalConstants.h"
#include "number.h"
#include "randutils.hpp"  // high-quality RNG
#include "TimeFrame.h"
#include "StatUtils.h"    // StatUtils<Decimal>::computeMean by default

namespace mkc_timeseries
{
  /**
   * @brief Calculates the start and end indices to divide a vector
   * into K nearly equal, contiguous slices.
   *
   * This function does not copy any data from the input vector. Instead, it computes
   * the boundaries of K contiguous, non-overlapping chunks and returns them as a
   * vector of index pairs. The slices are made as equal in size as possible. If the
   * total number of elements `n` is not perfectly divisible by `K`, the first `n % K`
   * slices will be one element larger than the rest.
   *
   * The returned indices follow the standard C++ half-open interval convention, `[start, end)`,
   * where `start` is inclusive and `end` is exclusive.
   *
   * @tparam Decimal The type of elements in the input vector. This type is part of
   * the signature but not used in the function's logic.
   * @param x A constant reference to the input vector to be sliced.
   * @param K The desired number of slices.
   * @param minLen The minimum allowable length for any single slice. If it's not
   * possible to create K slices each of at least this length, the function will fail.
   * @return A std::vector of std::pair<std::size_t, std::size_t>, where each pair
   * represents a slice `[start, end)`. Returns an empty vector if the input
   * cannot be sliced according to the given constraints.
   */
  template <class Decimal>
  std::vector<std::pair<std::size_t, std::size_t>>
  createSliceIndicesForBootstrap(const std::vector<Decimal>& x,
				 std::size_t K,
				 std::size_t minLen)
  {
    std::vector<std::pair<std::size_t, std::size_t>> out;
    const std::size_t n = x.size();

    // --- Pre-condition Checks ---
    // First, perform sanity checks to ensure the slicing is possible and makes sense.
    // If any check fails, return an empty vector immediately to signal failure.
    if (K < 2 || n < 2 || n < K * minLen)
      {
        return out; // empty => caller should skip processing
      }

    // --- Core Division Algorithm ---
    // The strategy is to distribute the `n` elements as evenly as possible among `K` slices.
    // This is achieved using integer division and the remainder.

    // `base`: The minimum number of elements every slice will receive.
    // This is the result of integer division `n / K`.
    const std::size_t base = n / K;

    // `rem`: The number of "leftover" elements after the base distribution (`n % K`).
    // These `rem` elements will be distributed one-by-one to the first `rem` slices,
    // making them one element larger than the others.
    const std::size_t rem  = n % K;

    // --- Slice Generation Loop ---
    // `start` will track the beginning index of the current slice.
    std::size_t start = 0;
    for (std::size_t k = 0; k < K; ++k)
      {
        // Calculate the length of the current slice `k`.
        // Every slice gets `base` elements. If `k` is one of the first `rem` slices,
        // it gets one extra element. The ternary operator `(k < rem ? 1 : 0)`
        // elegantly handles this distribution.
        std::size_t len = base + (k < rem ? 1 : 0);

        // A final safety check inside the loop. Although the initial check `n < K * minLen`
        // covers most cases, this ensures that the calculated non-uniform length
        // of any individual slice does not fall below the minimum. This is a robust
        // safeguard, though unlikely to be triggered if the first check passes.
        if (len < minLen)
	  {
            return {}; // abort if any slice falls below minimum
	  }

        // Store the calculated slice boundaries. The pair {start, start + len} represents
        // the half-open interval `[start, end)`.
        out.emplace_back(start, start + len);

        // Update the start position for the *next* slice to be the end of the current one.
        // This guarantees the slices are contiguous and non-overlapping.
        start += len;
      }
    return out;
  }
  // --------------------------- Resampling Policies -----------------------------

  /**
   * @struct IIDResampler
   * @brief Classic i.i.d. (independent and identically distributed) bootstrap resampler.
   *
   * This policy creates a new sample of size n by drawing n items with replacement
   * from the original data set. It is suitable for data that is i.i.d., meaning
   * there are no dependencies or serial correlations.
   *
   * @tparam Decimal The numeric type used for the data (e.g., double, number).
   */
  template <class Decimal, class Rng = randutils::mt19937_rng>
  struct IIDResampler
  {
    /**
     * @brief Resamples the input vector with replacement.
     * @param x The original data vector.
     * @param n The size of the resampled vector.
     * @param rng A high-quality random number generator.
     * @return A new vector of size n, containing elements sampled with replacement from x.
     * @throws std::invalid_argument If the input vector x is empty.
     */
    std::vector<Decimal>
    operator()(const std::vector<Decimal>& x, size_t n, Rng& rng) const
    {
      if (x.empty())
	{
	  throw std::invalid_argument("IIDResampler: empty sample.");
	}
      std::vector<Decimal> y;
      y.reserve(n);
      for (size_t j = 0; j < n; ++j)
	{
	  const size_t idx = rng.uniform(size_t(0), x.size() - 1);
	  y.push_back(x[idx]);
	}
      return y;
    }

    /**
     * @brief Performs a classic jackknife (delete-one observation).
     *
     * @details
     * The jackknife procedure is essential for computing the acceleration factor 'a'
     * in the BCa bootstrap, which corrects for skewness in the bootstrap distribution.
     *
     * ### How it Works:
     * The "delete-one" jackknife systematically measures the influence of each
     * individual data point on the overall statistic. For a dataset of size `n`,
     * it works as follows:
     * 1. It creates `n` new datasets, called jackknife replicates.
     * 2. The first replicate is the original dataset with the 1st observation removed.
     * 3. The second replicate is the original dataset with the 2nd observation removed.
     * 4. This continues until `n` replicates of size `n-1` have been created.
     * 5. The statistic (e.g., mean) is calculated for each of these `n` replicates.
     *
     * The resulting collection of `n` jackknife statistics reveals how sensitive the
     * main statistic is to each observation. The skewness of this collection of
     * values is then used to calculate the acceleration factor 'a'.
     *
     * Reference: Efron, B. (1987). Better Bootstrap Confidence Intervals.
     * Journal of the American Statistical Association, 82(397), 171–185.
     *
     * @tparam StatFn The type of the function object that computes the statistic.
     * @param x The original data vector.
     * @param stat The statistic function to apply to each jackknife replicate.
     * @return A vector of n jackknife statistic values.
     * @throws std::invalid_argument If the input vector x has fewer than 2 elements.
     */
    using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

    std::vector<Decimal>
    jackknife(const std::vector<Decimal>& x, const StatFn& stat) const
    {
      const size_t n = x.size();
      if (n < 2)
	{
	  throw std::invalid_argument("IIDResampler::jackknife requires n>=2.");
	}
      std::vector<Decimal> jk;
      jk.reserve(n);
      std::vector<Decimal> tmp;
      tmp.reserve(n - 1);
      for (size_t i = 0; i < n; ++i)
	{
	  tmp.clear();
	  tmp.insert(tmp.end(), x.begin(), x.begin() + i);
	  tmp.insert(tmp.end(), x.begin() + i + 1, x.end());
	  jk.push_back(stat(tmp));
	}
      return jk;
    }
  };

  /**
   * @struct StationaryBlockResampler
   * @brief Stationary Block Bootstrap resampler (Politis & Romano, 1994).
   *
   * @details
   * This policy is designed for time series data with serial correlation. It
   * resamples blocks of data rather than individual observations. The blocks
   * have a variable length drawn from a geometric distribution, with a specified
   * mean block length `L`.
   *
   * The resampling process treats the time series as circular. If a block
   * continues past the end of the series, it simply "wraps around" to the
   * beginning. This ensures that the process never fails due to running out
   * of elements and that all data points have an equal chance of being selected.
   *
   * Reference: Politis, D. N., & Romano, J. P. (1994). The stationary bootstrap.
   * Journal of the American Statistical Association, 89(428), 1303-1313.
   *
   * @tparam Decimal The numeric type used for the data (e.g., double, number).
   */

template <class Decimal, class Rng = randutils::mt19937_rng>
struct StationaryBlockResampler
{
  explicit StationaryBlockResampler(size_t L = 3)
    : m_L(std::max<size_t>(2, L)) {}

  // --- Bootstrap sample: geometric-length blocks ---
  std::vector<Decimal>
  operator()(const std::vector<Decimal>& x, size_t n, Rng& rng) const
  {
    if (x.empty())
      throw std::invalid_argument("StationaryBlockResampler: empty sample.");

    std::vector<Decimal> y;
    y.reserve(n);

    // Build doubled buffer once so we can copy contiguous runs without mod math.
    const size_t xn = x.size();
    std::vector<Decimal> x2;
    x2.reserve(2 * xn);
    x2.insert(x2.end(), x.begin(), x.end());
    x2.insert(x2.end(), x.begin(), x.end());

    const double p = 1.0 / static_cast<double>(m_L);
    const double log1m_p = std::log1p(-p);  // ln(1 - p) < 0

    auto draw_geometric_len = [&](Rng& r) -> size_t {
      // L = 1 + floor( ln(1-U) / ln(1-p) ), U ~ U(0,1)
      double U = r.uniform(0.0, 1.0);
      // ensure U in (0,1) to avoid log(0)
      if (U <= 0.0) U = std::numeric_limits<double>::min();
      if (U >= 1.0) U = std::nextafter(1.0, 0.0);
      const double t = std::log1p(-U) / log1m_p;
      const auto len = static_cast<size_t>(std::floor(t)) + 1u;
      return (len == 0 ? 1u : len);
    };

    // Start index uniform in [0, xn-1]
    size_t idx = rng.uniform(size_t(0), xn - 1);

    while (y.size() < n)
    {
        const size_t len = draw_geometric_len(rng);
        const size_t remaining = n - y.size();
        const size_t k = std::min({len, remaining, xn});  // ensures base + k ≤ base + xn ≤ 2*xn

      // Copy k items contiguously from doubled buffer
      const size_t base = idx;
      y.insert(y.end(), x2.begin() + static_cast<std::ptrdiff_t>(base),
                        x2.begin() + static_cast<std::ptrdiff_t>(base + k));

      // Start next block at a fresh random position (as in stationary bootstrap)
      idx = rng.uniform(size_t(0), xn - 1);
    }
    return y;
  }

  // --- Block jackknife with doubled buffer & single copy ---
  using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

  std::vector<Decimal>
  jackknife(const std::vector<Decimal>& x, const StatFn& stat) const
  {
    const size_t n = x.size();
    if (n < 2)
      throw std::invalid_argument("StationaryBlockResampler::jackknife requires n>=2.");

    const size_t L_eff = std::min(m_L, n - 1); // ensure remainder non-empty
    const size_t keep = n - L_eff;

    // Double buffer for one-shot contiguous copies irrespective of wrap.
    std::vector<Decimal> x2;
    x2.reserve(2 * n);
    x2.insert(x2.end(), x.begin(), x.end());
    x2.insert(x2.end(), x.begin(), x.end());

    std::vector<Decimal> jk(n);
    std::vector<Decimal> y(keep);

    for (size_t start = 0; start < n; ++start)
    {
      const size_t end = start + L_eff; // first kept index after deleted block
      // Keep the run [end, end+keep) in circular sense -> contiguous in x2
      std::copy_n(x2.begin() + static_cast<std::ptrdiff_t>(end),
                  static_cast<std::ptrdiff_t>(keep),
                  y.begin());
      jk[start] = stat(y);
    }
    return jk;
  }

  size_t meanBlockLen() const { return m_L; }

private:
  size_t m_L;
};


  // ------------------------------ BCa Bootstrap --------------------------------

  /**
   * @class BCaBootStrap
   * @brief Computes a statistic and BCa (Bias-Corrected and Accelerated) confidence interval.
   *
   * This class provides a robust and flexible implementation of the BCa bootstrap method.
   * It is designed with a pluggable architecture, allowing users to:
   * - Choose a **resampling policy** (`Sampler`) to handle different data types
   * (e.g., i.i.d. vs. time series).
   * - Specify a custom **statistic** (`StatFn`) to compute a measure of interest
   * (e.g., mean, median, standard deviation).
   *
   * The BCa method is a second-order accurate bootstrap interval that accounts for
   * both bias and skewness in the distribution of the statistic. It is generally
   * considered superior to the standard percentile bootstrap method.
   *
   * @tparam Decimal The numeric type for calculations (e.g., `double`, `number`).
   * @tparam Sampler The resampling policy class (e.g., `IIDResampler`, `StationaryBlockResampler`).
   * Defaults to `IIDResampler<Decimal>`.
   */
  template <class Decimal, class Sampler = IIDResampler<Decimal>, class Rng = randutils::mt19937_rng>
class BCaBootStrap
{
public:
  using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

  BCaBootStrap(const std::vector<Decimal>& returns,
               unsigned int num_resamples,
               double confidence_level = 0.95)
    : m_returns(returns),
      m_num_resamples(num_resamples),
      m_confidence_level(confidence_level),
      m_statistic(&mkc_timeseries::StatUtils<Decimal>::computeMean),
      m_sampler(Sampler{}),
      m_is_calculated(false)
  {
    validateConstructorArgs();
  }

  BCaBootStrap(const std::vector<Decimal>& returns,
               unsigned int num_resamples,
               double confidence_level,
               StatFn statistic)
    : m_returns(returns),
      m_num_resamples(num_resamples),
      m_confidence_level(confidence_level),
      m_statistic(std::move(statistic)),
      m_sampler(Sampler{}),
      m_is_calculated(false)
  {
    if (!m_statistic)
      throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
    validateConstructorArgs();
  }

  BCaBootStrap(const std::vector<Decimal>& returns,
               unsigned int num_resamples,
               double confidence_level,
               StatFn statistic,
               Sampler sampler)
    : m_returns(returns),
      m_num_resamples(num_resamples),
      m_confidence_level(confidence_level),
      m_statistic(std::move(statistic)),
      m_sampler(std::move(sampler)),
      m_is_calculated(false)
  {
    if (!m_statistic)
      throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
    validateConstructorArgs();
  }

  virtual ~BCaBootStrap() = default;

  Decimal getMean() const        { ensureCalculated(); return m_theta_hat; }
  Decimal getStatistic() const   { return getMean(); } // alias
  Decimal getLowerBound() const  { ensureCalculated(); return m_lower_bound; }
  Decimal getUpperBound() const  { ensureCalculated(); return m_upper_bound; }

protected:
  // Data & config
  const std::vector<Decimal>& m_returns;
  unsigned int                m_num_resamples;
  double                      m_confidence_level;
  StatFn                      m_statistic;
  Sampler                     m_sampler;
  bool                        m_is_calculated;

  // Results
  Decimal m_theta_hat{};
  Decimal m_lower_bound{};
  Decimal m_upper_bound{};

  // Test hooks (kept for mocks)
  void setStatistic(const Decimal& theta)  { m_theta_hat = theta; }
  void setMean(const Decimal& theta)       { m_theta_hat = theta; }
  void setLowerBound(const Decimal& lower) { m_lower_bound = lower; }
  void setUpperBound(const Decimal& upper) { m_upper_bound = upper; }

  void validateConstructorArgs() const
  {
    if (m_returns.empty())
      throw std::invalid_argument("BCaBootStrap: input returns vector cannot be empty.");
    if (m_num_resamples < 100u)
      throw std::invalid_argument("BCaBootStrap: number of resamples should be at least 100.");
    if (m_confidence_level <= 0.0 || m_confidence_level >= 1.0)
      throw std::invalid_argument("BCaBootStrap: confidence level must be between 0 and 1.");
  }

  void ensureCalculated() const
  {
    if (!m_is_calculated)
      const_cast<BCaBootStrap*>(this)->calculateBCaBounds();
  }

  /**
   * Core BCa algorithm with micro-optimizations:
   *  1) Compute θ̂
   *  2) Generate bootstrap replicates; count < θ̂ on the fly
   *  3) Compute z0 from the running count (no extra pass)
   *  4) Compute a via jackknife with double accumulators
   *  5) Get α1, α2 and select order statistics via nth_element
   */
  virtual void calculateBCaBounds()
  {
    if (m_is_calculated) return;

    const size_t n = m_returns.size();
    if (n < 2)
      throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");

    // (1) θ̂
    m_theta_hat = m_statistic(m_returns);

    // (2) Bootstrap replicates; count < θ̂ on the fly
    std::vector<Decimal> boot_stats;
    boot_stats.reserve(m_num_resamples);

    thread_local static Rng rng;
    unsigned int count_less = 0;

    for (unsigned int b = 0; b < m_num_resamples; ++b)
    {
      std::vector<Decimal> resample = m_sampler(m_returns, n, rng);
      const Decimal stat_b = m_statistic(resample);
      if (stat_b < m_theta_hat) ++count_less;
      boot_stats.push_back(stat_b);
    }

    // (3) Bias-correction z0
    const double prop_less = static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
    const double z0 = inverseNormalCdf(prop_less);

    // (4) Acceleration a via jackknife (use doubles for cubic/quadratic sums)
    const std::vector<Decimal> jk_stats = m_sampler.jackknife(m_returns, m_statistic);
    const size_t n_jk = jk_stats.size();

    // mean of jackknife stats in Decimal to preserve your numeric type
    Decimal jk_sum = DecimalConstants<Decimal>::DecimalZero;
    for (const auto& th : jk_stats) jk_sum += th;
    const Decimal jk_avg = jk_sum / Decimal(n_jk);

    // accumulate in double to cheapen cubic/quad math
    double num_d = 0.0; // sum (d^3)
    double den_d = 0.0; // sum (d^2)
    for (const auto& th : jk_stats)
    {
      const double d = (jk_avg - th).getAsDouble();
      const double d2 = d * d;
      den_d += d2;
      num_d += d2 * d;
    }

    Decimal a = DecimalConstants<Decimal>::DecimalZero;
    if (den_d > 0.0)
    {
      const double den15 = std::pow(den_d, 1.5);
      if (den15 > 0.0)
        a = Decimal(num_d / (6.0 * den15));
    }

    // (5) Adjusted percentiles → bounds
    const double alpha       = (1.0 - m_confidence_level) * 0.5;
    const double z_alpha_lo  = inverseNormalCdf(alpha);
    const double z_alpha_hi  = inverseNormalCdf(1.0 - alpha);

    const double a_d = a.getAsDouble();

    // Early-exit when a ~ 0 to avoid divisions & improve stability

    const bool z0_finite = std::isfinite(z0);
    const double alpha1 = (!z0_finite || std::abs(a_d) < 1e-12)
      ? standardNormalCdf(z0 + z_alpha_lo)
      : standardNormalCdf(z0 + (z0 + z_alpha_lo) / (1.0 - a_d * (z0 + z_alpha_lo)));

    const double alpha2 = (!z0_finite || std::abs(a_d) < 1e-12)
      ? standardNormalCdf(z0 + z_alpha_hi)
      : standardNormalCdf(z0 + (z0 + z_alpha_hi) / (1.0 - a_d * (z0 + z_alpha_hi)));

    const auto clamp01 = [](double v) noexcept {
      return (v <= 0.0) ? std::nextafter(0.0, 1.0)
       : (v >= 1.0) ? std::nextafter(1.0, 0.0)
                    : v;
    };

    const double a1 = clamp01(alpha1);
    const double a2 = clamp01(alpha2);

    int li = unbiasedIndex(std::min(a1, a2), m_num_resamples);
    int ui = unbiasedIndex(std::max(a1, a2), m_num_resamples);  

    // Select order statistics in O(B)
    std::nth_element(boot_stats.begin(), boot_stats.begin() + li, boot_stats.end());
    m_lower_bound = boot_stats[li];

    std::nth_element(boot_stats.begin(), boot_stats.begin() + ui, boot_stats.end());
    m_upper_bound = boot_stats[ui];

    m_is_calculated = true;
  }

  // ---- Math helpers ----
  static inline double standardNormalCdf(double x) noexcept
  {
    // Hoist constants to constexpr for compile-time folding
    constexpr double INV_SQRT2 = 1.0 / 1.4142135623730950488; // 1/sqrt(2)
    return 0.5 * (1.0 + std::erf(x * INV_SQRT2));
  }

  static inline double inverseNormalCdf(double p) noexcept
  {
    if (p <= 0.0) return -std::numeric_limits<double>::infinity();
    if (p >= 1.0) return  std::numeric_limits<double>::infinity();
    return (p < 0.5) ? -inverseNormalCdfHelper(p) : inverseNormalCdfHelper(1.0 - p);
  }

  static inline double inverseNormalCdfHelper(double p) noexcept
  {
    // Abramowitz & Stegun 26.2.23
    constexpr double c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
    constexpr double d0 = 1.432788, d1 = 0.189269, d2 = 0.001308;
    const double t  = std::sqrt(-2.0 * std::log(p));
    const double num = (c0 + c1 * t + c2 * t * t);
    const double den = (1.0 + d0 * t + d1 * t * t + d2 * t * t * t);
    return t - num / den;
  }

  static inline int unbiasedIndex(double p, unsigned int B) noexcept
  {
    int idx = static_cast<int>(std::floor(p * (static_cast<double>(B) + 1.0))) - 1;
    if (idx < 0) idx = 0;
    const int maxIdx = static_cast<int>(B) - 1;
    if (idx > maxIdx) idx = maxIdx;
    return idx;
  }
};

  // ------------------------------ Annualizer -----------------------------------

  /**
   * @brief Calculates an annualization factor based on a given time frame.
   *
   * @param timeFrame The time frame of the data (e.g., DAILY, WEEKLY).
   * @param intraday_minutes_per_bar The number of minutes per bar for INTRADAY data.
   * @param trading_days_per_year The number of trading days in a year.
   * @param trading_hours_per_day The number of trading hours in a day.
   * @return The annualization factor.
   * @throws std::invalid_argument If the time frame is unsupported or intraday_minutes_per_bar is zero for INTRADAY data.
   */
  inline double calculateAnnualizationFactor(TimeFrame::Duration timeFrame,
					     unsigned int intraday_minutes_per_bar = 0,
					     double trading_days_per_year = 252.0,
					     double trading_hours_per_day = 6.5)
  {
    switch (timeFrame)
      {
      case TimeFrame::DAILY:
	return trading_days_per_year;
      case TimeFrame::WEEKLY:
	return 52.0;
      case TimeFrame::MONTHLY:
	return 12.0;
      case TimeFrame::INTRADAY:
	{
	  if (intraday_minutes_per_bar == 0)
	    {
	      throw std::invalid_argument("For INTRADAY timeframe, intraday_minutes_per_bar must be specified.");
	    }
	  double bars_per_hour = 60.0 / intraday_minutes_per_bar;
	  return trading_hours_per_day * bars_per_hour * trading_days_per_year;
	}
      case TimeFrame::QUARTERLY:
	return 4.0;
      case TimeFrame::YEARLY:
	return 1.0;
      default:
	throw std::invalid_argument("Unsupported time frame for annualization.");
      }
  }

  /**
   * @class BCaAnnualizer
   * @brief Annualizes the mean and confidence interval bounds from a BCaBootStrap result.
   *
   * This class takes the results of a `BCaBootStrap` calculation and annualizes
   * the statistic and its confidence bounds. This is typically used for financial
   * data, such as converting a daily return and its confidence interval to an
   * annualized return and interval.
   *
   * The annualization formula used is `(1 + rate)^factor - 1`.
   *
   * @tparam Decimal The numeric type used for the calculations.
   */
  template <class Decimal>
  class BCaAnnualizer
  {
  public:
    /**
     * @brief Constructs a BCaAnnualizer and computes the annualized values.
     *
     * This constructor performs the annualization immediately upon creation.
     *
     * @tparam Sampler The sampler type of the `BCaBootStrap` object.
     * @param bca_results The calculated `BCaBootStrap` object.
     * @param annualization_factor The factor to use for annualization.
     * @throws std::invalid_argument If the annualization factor is not positive.
     */
    template <class Sampler, class Rng = randutils::mt19937_rng>
    BCaAnnualizer(const BCaBootStrap<Decimal, Sampler, Rng>& bca_results, double annualization_factor)
    {
      if (annualization_factor <= 0.0)
	{
	  throw std::invalid_argument("Annualization factor must be positive.");
	}
      const Decimal one = DecimalConstants<Decimal>::DecimalOne;
      const double mean_d  = (one + bca_results.getMean()).getAsDouble();
      const double lower_d = (one + bca_results.getLowerBound()).getAsDouble();
      const double upper_d = (one + bca_results.getUpperBound()).getAsDouble();

      m_annualized_mean        = Decimal(std::pow(mean_d,  annualization_factor)) - one;
      m_annualized_lower_bound = Decimal(std::pow(lower_d, annualization_factor)) - one;
      m_annualized_upper_bound = Decimal(std::pow(upper_d, annualization_factor)) - one;
    }

    /**
     * @brief Gets the annualized mean (statistic).
     * @return The annualized mean.
     */
    Decimal getAnnualizedMean() const
    {
      return m_annualized_mean;
    }

    /**
     * @brief Gets the annualized lower bound of the confidence interval.
     * @return The annualized lower bound.
     */
    Decimal getAnnualizedLowerBound() const
    {
      return m_annualized_lower_bound;
    }

    /**
     * @brief Gets the annualized upper bound of the confidence interval.
     * @return The annualized upper bound.
     */
    Decimal getAnnualizedUpperBound() const
    {
      return m_annualized_upper_bound;
    }

  private:
    Decimal m_annualized_mean;
    Decimal m_annualized_lower_bound;
    Decimal m_annualized_upper_bound;
  };
} // namespace mkc_timeseries

#endif // __BCA_BOOTSTRAP_H
