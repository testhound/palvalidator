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

#include "DecimalConstants.h"
#include "number.h"
#include "randutils.hpp"  // high-quality RNG
#include "TimeFrame.h"
#include "StatUtils.h"    // StatUtils<Decimal>::computeMean by default

namespace mkc_timeseries
{

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
  template <class Decimal>
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
    operator()(const std::vector<Decimal>& x, size_t n, randutils::mt19937_rng& rng) const
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
     * This method computes n jackknife replicates. Each replicate is the statistic
     * calculated on the original data set with one observation removed.
     * This is used to compute the acceleration factor 'a' for the BCa bootstrap.
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
   * This policy is designed for time series data with serial correlation. It
   * resamples blocks of data rather than individual observations. The blocks
   * have a variable length with a specified mean block length `L`.
   *
   * @tparam Decimal The numeric type used for the data (e.g., double, number).
   */
  template <class Decimal>
  struct StationaryBlockResampler
  {
    /**
     * @brief Constructs a StationaryBlockResampler.
     * @param L The mean block length. Must be at least 2.
     */
    explicit StationaryBlockResampler(size_t L = 3)
      : m_L(std::max<size_t>(2, L)) {}

    /**
     * @brief Resamples the input vector using the stationary block method.
     *
     * A new block is started at a random index with probability `p = 1/L`, or
     * the current block is continued by advancing to the next index (circularly).
     * This process continues until a resample of size `n` is created.
     *
     * @param x The original time series data vector.
     * @param n The size of the resampled vector.
     * @param rng A high-quality random number generator.
     * @return A new vector of size n, containing a resampled time series.
     * @throws std::invalid_argument If the input vector x is empty.
     */
    std::vector<Decimal>
    operator()(const std::vector<Decimal>& x, size_t n, randutils::mt19937_rng& rng) const
    {
      if (x.empty())
	{
	  throw std::invalid_argument("StationaryBlockResampler: empty sample.");
	}
      std::vector<Decimal> y;
      y.reserve(n);

      const double p = 1.0 / static_cast<double>(m_L);
      size_t idx = rng.uniform(size_t(0), x.size() - 1);

      while (y.size() < n)
	{
	  y.push_back(x[idx]);
	  if (rng.uniform(0.0, 1.0) < p)
	    {
	      idx = rng.uniform(size_t(0), x.size() - 1);    // start new block
	    }
	  else
	    {
	      idx = (idx + 1) % x.size();                    // continue block
	    }
	}
      return y;
    }

    /**
     * @brief Performs a block jackknife for the acceleration factor.
     *
     * This method computes n jackknife replicates by deleting a block of data
     * (with length `L_eff`) from a circular representation of the original data.
     * This is a block-based analog to the delete-one jackknife and is used to
     * compute the acceleration factor 'a' for the BCa bootstrap.
     *
     * @tparam StatFn The type of the function object that computes the statistic.
     * @param x The original time series data vector.
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
	  throw std::invalid_argument("StationaryBlockResampler::jackknife requires n>=2.");
	}
      const size_t L_eff = std::min(m_L, n - 1); // ensure remainder non-empty

      std::vector<Decimal> jk;
      jk.reserve(n);
      std::vector<Decimal> y;
      y.reserve(n - L_eff);

      for (size_t start = 0; start < n; ++start)
	{
	  const size_t end = (start + L_eff) % n; // element after the deleted block
	  y.clear();

	  if (start < end)
	    {
	      // delete [start, end): keep [0, start) and [end, n)
	      y.insert(y.end(), x.begin(), x.begin() + static_cast<std::ptrdiff_t>(start));
	      y.insert(y.end(), x.begin() + static_cast<std::ptrdiff_t>(end), x.end());
	    }
	  else
	    {
	      // wrapped deletion: delete [start, n) U [0, end); keep [end, start)
	      y.insert(y.end(),
		       x.begin() + static_cast<std::ptrdiff_t>(end),
		       x.begin() + static_cast<std::ptrdiff_t>(start));
	    }

	  // y has size n - L_eff >= 1
	  jk.push_back(stat(y));
	}

      return jk;
    }

    /**
     * @brief Gets the mean block length configured for the resampler.
     * @return The mean block length.
     */
    size_t meanBlockLen() const
    {
      return m_L;
    }

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
  template <class Decimal, class Sampler = IIDResampler<Decimal>>
  class BCaBootStrap
  {
  public:
    using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

    /**
     * @brief Constructs a BCaBootStrap object with default settings.
     * @param returns The input data vector (e.g., a time series of returns).
     * @param num_resamples The number of bootstrap replicates to generate.
     * @param confidence_level The desired confidence level for the interval (e.g., 0.95).
     * @throws std::invalid_argument If inputs are invalid.
     */
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

    /**
     * @brief Constructs a BCaBootStrap object with a custom statistic.
     * @param returns The input data vector.
     * @param num_resamples The number of bootstrap replicates.
     * @param confidence_level The desired confidence level.
     * @param statistic The function to compute the statistic of interest.
     * @throws std::invalid_argument If inputs are invalid.
     */
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
	{
	  throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
	}
      validateConstructorArgs();
    }

    /**
     * @brief Constructs a BCaBootStrap object with a custom statistic and sampler.
     * @param returns The input data vector.
     * @param num_resamples The number of bootstrap replicates.
     * @param confidence_level The desired confidence level.
     * @param statistic The function to compute the statistic of interest.
     * @param sampler The resampling policy instance to use.
     * @throws std::invalid_argument If inputs are invalid.
     */
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
	{
	  throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
	}
      validateConstructorArgs();
    }

    virtual ~BCaBootStrap() = default;

    /**
     * @brief Gets the value of the statistic on the original data.
     * @return The statistic value.
     */
    Decimal getMean() const
    {
      ensureCalculated();
      return m_theta_hat;
    }

    /**
     * @brief Alias for `getMean()`.
     * @return The statistic value.
     */
    Decimal getStatistic() const
    {
      return getMean();
    } // alias for backward compat

    /**
     * @brief Gets the lower bound of the BCa confidence interval.
     * @return The lower bound.
     */
    Decimal getLowerBound() const
    {
      ensureCalculated();
      return m_lower_bound;
    }

    /**
     * @brief Gets the upper bound of the BCa confidence interval.
     * @return The upper bound.
     */
    Decimal getUpperBound() const
    {
      ensureCalculated();
      return m_upper_bound;
    }

  protected:
    // Data & config
    const std::vector<Decimal>& m_returns;
    unsigned int m_num_resamples;
    double m_confidence_level;
    StatFn m_statistic;
    Sampler m_sampler;
    bool m_is_calculated;

    // Results
    Decimal m_theta_hat;
    Decimal m_lower_bound;
    Decimal m_upper_bound;

    // Test hooks (kept for mocks)
    void setStatistic(const Decimal& theta)
    {
      m_theta_hat = theta;
    }
    void setMean(const Decimal& theta)
    {
      m_theta_hat = theta;
    }
    void setLowerBound(const Decimal& lower)
    {
      m_lower_bound = lower;
    }
    void setUpperBound(const Decimal& upper)
    {
      m_upper_bound = upper;
    }

    /**
     * @brief Validates the constructor arguments.
     * @throws std::invalid_argument If any argument is invalid.
     */
    void validateConstructorArgs() const
    {
      if (m_returns.empty())
	{
	  throw std::invalid_argument("BCaBootStrap: input returns vector cannot be empty.");
	}
      if (m_num_resamples < 100)
	{
	  throw std::invalid_argument("BCaBootStrap: number of resamples should be at least 100.");
	}
      if (m_confidence_level <= 0.0 || m_confidence_level >= 1.0)
	{
	  throw std::invalid_argument("BCaBootStrap: confidence level must be between 0 and 1.");
	}
    }

    /**
     * @brief Ensures that the confidence bounds have been calculated.
     *
     * This is a lazy calculation mechanism. The first time a getter for the
     * results is called, `calculateBCaBounds()` is executed. Subsequent calls
     * return the cached results.
     */
    void ensureCalculated() const
    {
      if (!m_is_calculated)
	{
	  const_cast<BCaBootStrap*>(this)->calculateBCaBounds();
	}
    }

    /**
     * @brief The core algorithm for computing the BCa confidence bounds.
     *
     * @details
     * The method follows a five-step process to compute the Bias-Corrected and
     * Accelerated (BCa) confidence interval.
     *
     * ### Algorithm:
     * 1.  **Calculate the original statistic (θ̂):**
     * - Compute the statistic of interest (e.g., mean) on the original `m_returns` data set.
     * - This value, `m_theta_hat`, serves as the point estimate.
     *
     * 2.  **Generate Bootstrap Replicates:**
     * - Create `m_num_resamples` new data sets by calling the `m_sampler` policy's `operator()`.
     * - For `IIDResampler`, this involves drawing `n` elements with replacement.
     * - For `StationaryBlockResampler`, this involves creating a new time series of length `n`
     * by resampling blocks.
     * - For each resampled data set, calculate the statistic using `m_statistic`.
     * - Store these `m_num_resamples` statistic values in `boot_stats`.
     * - Sort `boot_stats` in ascending order.
     *
     * 3.  **Compute the Bias-Correction Factor (z0):**
     * - Find the proportion of bootstrap replicates (`boot_stats`) that are less than the
     * original statistic (`m_theta_hat`).
     * - `prop_less = (number of bootstrap stats < θ̂) / m_num_resamples`.
     * - `z0` is the `prop_less`-th percentile of the standard normal distribution,
     * calculated using `inverseNormalCdf(prop_less)`.
     * - `z0` measures the bias in the bootstrap distribution of the statistic. A non-zero
     * `z0` indicates that the bootstrap distribution is not centered on the original statistic.
     *
     * 4.  **Compute the Acceleration Factor (a):**
     * - The acceleration factor `a` corrects for skewness in the bootstrap distribution.
     * - This is computed using a jackknife procedure, which is delegated to the `m_sampler` policy.
     * - For `IIDResampler`, this is a classic delete-one jackknife.
     * - For `StationaryBlockResampler`, this is a delete-one-block jackknife.
     * - Let `jk_stats` be the vector of jackknife statistic values.
     * - Compute `jk_avg`, the mean of the jackknife statistics.
     * - `a` is calculated based on the third- and second-order centered moments of the jackknife
     * replicates:
     * `a = (sum((jk_avg - jk_stats_i)^3)) / (6 * (sum((jk_avg - jk_stats_i)^2))^1.5)`.
     * - This formula relates the acceleration to the skewness of the jackknife distribution.
     *
     * 5.  **Calculate the Adjusted Percentiles and Bounds:**
     * - Determine the percentile indices for a standard percentile confidence interval
     * (`alpha1`, `alpha2`).
     * - `alpha = (1.0 - m_confidence_level) / 2.0`.
     * - The standard normal percentiles are `z_alpha_lo = inverseNormalCdf(alpha)` and
     * `z_alpha_hi = inverseNormalCdf(1.0 - alpha)`.
     * - Apply the bias-correction (`z0`) and acceleration (`a`) factors to these percentiles
     * to get the adjusted percentiles:
     * - `alpha_lower_adj = standardNormalCdf(z0 + (z0 + z_alpha_lo) / (1 - a * (z0 + z_alpha_lo)))`
     * - `alpha_upper_adj = standardNormalCdf(z0 + (z0 + z_alpha_hi) / (1 - a * (z0 + z_alpha_hi)))`
     * - Find the corresponding indices in the sorted `boot_stats` vector using these adjusted
     * percentiles (`unbiasedIndex`).
     * - The `m_lower_bound` and `m_upper_bound` are the values at these indices.
     *
     * @throws std::invalid_argument If the original data vector has fewer than 2 data points.
     */
    virtual void calculateBCaBounds()
    {
      if (m_is_calculated)
	{
	  return;
	}

      const size_t n = m_returns.size();
      if (n < 2)
	{
	  throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");
	}

      // 1) θ̂ (original statistic)
      m_theta_hat = m_statistic(m_returns);

      // 2) Bootstrap replicates using the selected resampling policy
      std::vector<Decimal> boot_stats;
      boot_stats.reserve(m_num_resamples);

      thread_local static randutils::mt19937_rng rng;

      for (unsigned int b = 0; b < m_num_resamples; ++b)
	{
	  // *** The only place resampling occurs: policy call ***
	  std::vector<Decimal> resample = m_sampler(m_returns, n, rng);
	  boot_stats.push_back(m_statistic(resample));
	}
      std::sort(boot_stats.begin(), boot_stats.end());

      // 3) Bias-correction z0
      const auto count_less = std::count_if(
					    boot_stats.begin(), boot_stats.end(),
					    [this](const Decimal& v){ return v < this->m_theta_hat; });
      const double prop_less = static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
      const double z0 = inverseNormalCdf(prop_less);

      // 4) Acceleration a via jackknife, delegated to the sampler
      const std::vector<Decimal> jk_stats = m_sampler.jackknife(m_returns, m_statistic);
      const size_t n_jk = jk_stats.size();

      Decimal jk_sum(DecimalConstants<Decimal>::DecimalZero);
      for (const auto& th : jk_stats)
	{
	  jk_sum += th;
	}
      const Decimal jk_avg = jk_sum / Decimal(n_jk);

      Decimal num(DecimalConstants<Decimal>::DecimalZero);
      Decimal den(DecimalConstants<Decimal>::DecimalZero);
      for (const auto& th : jk_stats)
	{
	  const Decimal d = jk_avg - th;
	  num += d * d * d;
	  den += d * d;
	}

      Decimal a(DecimalConstants<Decimal>::DecimalZero);
      if (den > DecimalConstants<Decimal>::DecimalZero)
	{
	  const double den15 = std::pow(num::to_double(den), 1.5);
	  if (den15 > 0.0)
	    {
	      a = num / (Decimal(6) * Decimal(den15));
	    }
	}

      // 5) Adjusted percentiles → bounds
      const double alpha = (1.0 - m_confidence_level) / 2.0;
      const double z_alpha_lo = inverseNormalCdf(alpha);
      const double z_alpha_hi = inverseNormalCdf(1.0 - alpha);

      const double a_d = a.getAsDouble();

      const double t1 = z0 + z_alpha_lo;
      const double alpha1 = standardNormalCdf(z0 + t1 / (1.0 - a_d * t1));

      const double t2 = z0 + z_alpha_hi;
      const double alpha2 = standardNormalCdf(z0 + t2 / (1.0 - a_d * t2));

      const int lower_idx = unbiasedIndex(alpha1, m_num_resamples);
      const int upper_idx = unbiasedIndex(alpha2, m_num_resamples);

      m_lower_bound = boot_stats[lower_idx];
      m_upper_bound = boot_stats[upper_idx];

      m_is_calculated = true;
    }

    // ---- Math helpers ----

    /**
     * @brief Computes the Standard Normal Cumulative Distribution Function (CDF).
     * @param x The value at which to evaluate the CDF.
     * @return The probability `P(Z <= x)` where Z is a standard normal random variable.
     */
    double standardNormalCdf(double x) const
    {
      return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
    }

    /**
     * @brief Computes the inverse of the Standard Normal CDF (quantile function).
     * @param p The probability (a value between 0 and 1).
     * @return The value `x` such that `P(Z <= x) = p`.
     */
    double inverseNormalCdf(double p) const
    {
      if (p <= 0.0)
	{
	  return -std::numeric_limits<double>::infinity();
	}
      if (p >= 1.0)
	{
	  return  std::numeric_limits<double>::infinity();
	}
      return (p < 0.5) ? -inverseNormalCdfHelper(p) : inverseNormalCdfHelper(1.0 - p);
    }

    /**
     * @brief A helper function for `inverseNormalCdf` using an approximation.
     *
     * Implements the rational approximation from Abramowitz & Stegun, 26.2.23.
     * It's accurate enough for typical confidence interval calculations.
     *
     * @param p The probability, assumed to be in the range (0, 0.5].
     * @return The inverse normal CDF value.
     */
    double inverseNormalCdfHelper(double p) const
    {
      // Abramowitz & Stegun 26.2.23 (sufficient for CI work)
      static const double c[] = {2.515517, 0.802853, 0.010328};
      static const double d[] = {1.432788, 0.189269, 0.001308};
      const double t = std::sqrt(-2.0 * std::log(p));
      const double num = (c[0] + c[1] * t + c[2] * t * t);
      const double den = (1.0 + d[0] * t + d[1] * t * t + d[2] * t * t * t);
      return t - num / den;
    }

    /**
     * @brief Calculates an "unbiased" index for a sorted vector based on a percentile.
     *
     * This method uses `p * (B + 1)` and flooring to determine the index, which is
     * a common approach to find an index that corresponds to a percentile of a
     * finite sample. The result is clamped to be within the valid index range `[0, B-1]`.
     *
     * @param p The percentile (a value from 0.0 to 1.0).
     * @param B The total number of elements in the sorted vector.
     * @return The integer index corresponding to the given percentile.
     */
    static int unbiasedIndex(double p, unsigned int B)
    {
      int idx = static_cast<int>(std::floor(p * (static_cast<double>(B) + 1.0))) - 1;
      if (idx < 0)
	{
	  idx = 0;
	}
      const int maxIdx = static_cast<int>(B) - 1;
      if (idx > maxIdx)
	{
	  idx = maxIdx;
	}
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
    template <class Sampler>
    BCaAnnualizer(const BCaBootStrap<Decimal, Sampler>& bca_results, double annualization_factor)
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
