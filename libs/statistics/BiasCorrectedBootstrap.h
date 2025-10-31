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
#include "randutils.hpp"
#include "TimeFrame.h"
#include "StatUtils.h"

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
      : m_L(std::max<size_t>(2, L)),
	m_geo(1.0 / static_cast<double>(m_L))
    {}

    // --- Bootstrap sample: geometric-length blocks (no x2 allocation) ---
    std::vector<Decimal>
    operator()(const std::vector<Decimal>& x, size_t n, Rng& rng) const
    {
      if (x.empty())
	throw std::invalid_argument("StationaryBlockResampler: empty sample.");

      const size_t xn = x.size();

      // Pre-size output once; we’ll fill by index (no push_back, no insert)
      std::vector<Decimal> y(n);

      // Stationary bootstrap: mean block length L -> p = 1/L; length = 1 + Geom(p) on {0,1,...}
      // First start idx uniform in [0, xn-1]
      size_t idx = rng.uniform(size_t(0), xn - 1);

      size_t pos = 0;  // write cursor into y
      while (pos < n)
	{
	  size_t len = 1 + m_geo(rng.engine());                // proposed block length
	  size_t remaining = n - pos;
	  size_t k = std::min({len, remaining, xn});         // never copy more than xn in one shot

	  // Fast contiguous copy with wrap handling (0 or 1 wrap):
	  size_t room_to_end = xn - idx;
	  if (k <= room_to_end) {
	    // Single span: [idx, idx+k)
	    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(idx),
			static_cast<std::ptrdiff_t>(k),
			y.begin() + static_cast<std::ptrdiff_t>(pos));
	  } else {
	    // Wrap: copy tail [idx, xn), then head [0, k - room_to_end)
	    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(idx),
			static_cast<std::ptrdiff_t>(room_to_end),
			y.begin() + static_cast<std::ptrdiff_t>(pos));
	    const size_t rem = k - room_to_end;
	    std::copy_n(x.begin(),
			static_cast<std::ptrdiff_t>(rem),
			y.begin() + static_cast<std::ptrdiff_t>(pos + room_to_end));
	  }

	  pos += k;
	  // Next block starts at fresh random index (stationary bootstrap)
	  idx = rng.uniform(size_t(0), xn - 1);
	}

      return y;
    }

    /**
     * @brief Performs a block jackknife for the acceleration factor.
     *
     * @details
     * The jackknife procedure is essential for computing the acceleration factor 'a'
     * in the BCa bootstrap. For time series data, the standard "delete-one"
     * jackknife is invalid as it breaks the dependence structure. This "delete-block"
     * jackknife is the correct analog.
     *
     * ### How it Works:
     * The "delete-block" jackknife measures the influence of contiguous segments
     * of the time series. For a dataset of size `n` and a block length `L`:
     * 1. It creates `n` new datasets, called jackknife replicates.
     * 2. The first replicate is the original dataset with the block from index 0 to `L-1` removed.
     * 3. The second replicate is the original dataset with the block from index 1 to `L` removed.
     * 4. This continues for all `n` possible starting positions, treating the data
     * as circular (a block removed from the end wraps around to the beginning).
     * 5. The statistic (e.g., mean) is calculated for each of these `n` replicates.
     *
     * This method correctly assesses the sensitivity of the statistic to different
     * segments of the time series while preserving the data's autocorrelation,
     * which is crucial for dependent data. The skewness of the resulting `n`
     * jackknife statistics is then used to calculate the acceleration factor 'a'.
     *
     * Reference: Künsch, H. R. (1989). The Jackknife and the Bootstrap for
     * General Stationary Observations. The Annals of Statistics, 17(3), 1217–1241.
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
	throw std::invalid_argument("StationaryBlockResampler::jackknife requires n>=2.");

      const size_t L_eff = std::min(m_L, n - 1); // ensure at least 1 kept
      const size_t keep  = n - L_eff;

      std::vector<Decimal> jk(n);
      std::vector<Decimal> y(keep);

      for (size_t start = 0; start < n; ++start)
	{
	  // Circular index where the kept region begins (immediately after deleted block)
	  const size_t start_keep = (start + L_eff) % n;

	  // Copy keep entries from x[start_keep … start_keep+keep) with wrap if needed
	  const size_t tail = std::min(keep, n - start_keep);   // bytes available to end
	  // First span: [start_keep, start_keep + tail)
	  std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
		      static_cast<std::ptrdiff_t>(tail),
		      y.begin());

	  // Second span (wrap): [0, keep - tail)
	  const size_t head = keep - tail;
	  if (head != 0) {
	    std::copy_n(x.begin(),
			static_cast<std::ptrdiff_t>(head),
			y.begin() + static_cast<std::ptrdiff_t>(tail));
	  }

	  jk[start] = stat(y);
	}
      return jk;
    }

    /**
     * @brief Gets the mean block length configured for the resampler.
     * @return The mean block length.
     */
    size_t meanBlockLen() const { return m_L; }

  private:
    size_t m_L;
    mutable std::geometric_distribution<size_t> m_geo;
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
	throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
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
	throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
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
    }// alias

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
     * @brief The core algorithm for computing the BCa confidence bounds.
     *
     * @details
     * This method calculates the Bias-Corrected and Accelerated (BCa) confidence
     * interval. It's more accurate than a simple percentile interval because it
     * adjusts for both bias and skewness in the bootstrap distribution. The
     * algorithm can be understood as a five-step journey where each step
     * builds upon the last.
     *
     * ### Step 1: Calculate the Original Statistic (Our Anchor Point)
     * We start by calculating the statistic (e.g., the mean) on the original,
     * untouched data. This gives us our single best estimate, `m_theta_hat`,
     * which serves as the central point of our analysis.
     *
     * ### Step 2: Generate the Bootstrap Distribution (Create a "Bootstrap World")
     * Next, we create thousands of new datasets by resampling from the original
     * data (using the chosen `Sampler` policy). For each of these resamples, we
     * calculate the same statistic. This collection of statistics forms the
     * "bootstrap distribution." It shows us how our statistic might vary if we
     * could sample from the true underlying population. At the end of this step,
     * we sort this distribution from smallest to largest.
     *
     * ### Step 3: Compute the Bias-Correction Factor, `z₀` (Is Our Anchor Centered?)
     * Now, we check if our original statistic (`m_theta_hat`) is in the center
     * of the bootstrap world we just created. We do this by finding the proportion
     * of our bootstrap statistics that are less than `m_theta_hat`. If this
     * proportion is not 50%, our bootstrap distribution is "biased." The `z₀`
     * value is simply the standard normal score (Z-score) of this proportion,
     * quantifying this bias. A `z₀` of 0 means no bias.
     *
     * ### Step 4: Compute the Acceleration Factor, `a` (Is Our World Skewed?)
     * This step measures the skewness of our statistic's distribution. It answers
     * the question: "Does the standard error of our statistic change as the
     * data changes?" We use the jackknife procedure for this. By systematically
     * removing parts of the original data and recalculating the statistic each
     * time, we see how much our statistic's value is influenced by different
     * parts of the data. The skewness of these "influence" values gives us the
     * acceleration factor `a`. A non-zero `a` indicates that the bootstrap
     * distribution is lopsided (skewed).
     *
     * ### Step 5: Calculate the Adjusted Percentiles (Find the New Endpoints)
     * This is the final step where everything comes together. We start with the
     * standard percentiles for our confidence level (e.g., for 95% confidence),
     * We then plug our bias factor (`z₀`) and acceleration factor (`a`) into
     * the BCa formula. This formula "warps" the standard percentiles to account
     * for the bias and skew we found. The output is a new pair of adjusted percentiles.
     * We use these new percentiles to find the corresponding values in our sorted bootstrap
     * distribution from Step 2. These values are our final, more accurate, BCa confidence bounds.
     *
     * @throws std::invalid_argument If the original data vector has fewer than 2 data points.
     */
    virtual void calculateBCaBounds()
    {
      if (m_is_calculated)
	return;

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

      bool all_equal = true;
      for (size_t i = 1; i < boot_stats.size(); ++i)
	{
	  if (!(boot_stats[i] == boot_stats[0]))
	    {
	      all_equal = false;
	      break; 
	    }
	}

      if (all_equal)
	{
	  // CI collapses to a point mass; no need to compute z0/a or select order stats

	  m_lower_bound = m_upper_bound = boot_stats[0];
	  m_is_calculated = true;
	  return;
      }

      // (3) Bias-correction z0
      const double prop_less = static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
      const double z0 = inverseNormalCdf(prop_less);

      // (4) Acceleration a via jackknife (use doubles for cubic/quadratic sums)
      /**
       * @details
       * ### Why is the Skew Correction Called "Accelerated"?
       *
       * The term "accelerated" refers to the acceleration constant `a`, which
       * measures how rapidly the standard error of our statistic changes on a
       * normalized scale. It's called an "acceleration" because, in statistical
       * theory, it relates to the rate of change (or "acceleration") of the
       * statistic's variance as the underlying data distribution changes.
       *
       * A simple way to think about it is:
       * - If our statistic's standard error is very stable and doesn't change
       * much when we subtly alter the data, the acceleration is low.
       * - If the standard error is very sensitive and changes quickly, the
       * acceleration is high. This indicates a more skewed or unstable
       * distribution that requires a larger correction.
       *
       * The jackknife procedure is the mechanism we use to estimate this value.
       * By removing parts of the data and observing how much the statistic
       * "wobbles," we get a practical measure of this theoretical acceleration.
       */
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
      /**
       * @details
       * ### Step 5: Combining and Adjusting to Find the Final Bounds
       *
       * This is the final step where all our calculated components come together.
       * The goal is to "warp" the standard confidence percentiles (e.g., 2.5%
       * and 97.5%) using the bias and skewness we discovered in the data.
       *
       * #### The Process:
       * 1.  **Get Standard Endpoints:** We first find the standard Z-scores that
       * correspond to our desired confidence level (e.g., -1.96 and +1.96 for
       * 95% confidence). These are our uncorrected, "textbook" endpoints.
       *
       * 2.  **Apply the BCa Formula:** We plug these Z-scores, along with our
       * bias-correction factor (`z₀`) and our acceleration factor (`a`), into
       * the BCa adjustment formula. This is where the result of the jackknife
       * (`a`) is finally used. It modifies the interval to account for skew:
       * - The **bias factor `z₀`** shifts the center of the confidence interval.
       * - The **acceleration factor `a`** expands or contracts the interval
       * asymmetrically to better fit the lopsided shape of a skewed distribution.
       *
       * 3.  **Find the Final Bounds:** The BCa formula outputs two new, adjusted
       * percentiles (`alpha1` and `alpha2`). The very last step is to take
       * these final percentiles and find the values at those positions in our
       * sorted bootstrap distribution (from Step 2). These values are the
       * final, robust, BCa confidence interval bounds.
       */
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

    /**
     * @brief Computes the Standard Normal Cumulative Distribution Function (CDF).
     *
     * @details
     * ### What it Does (In Simple Terms)
     * Imagine a perfect bell curve (a "standard normal distribution") where the
     * total area underneath it is 1.0 (representing 100% probability). This
     * function takes a point on the horizontal axis (a Z-score `x`) and returns
     * the total area under the curve to the left of that point.
     *
     * In essence, it **converts a Z-score into a percentile**. For example:
     * - `standardNormalCdf(0.0)` would return `0.5`, because the point 0.0 is exactly
     * at the center of the bell curve, with 50% of the area to its left.
     * - `standardNormalCdf(1.96)` would return approximately `0.975`, meaning that
     * 97.5% of all values are less than or equal to 1.96.
     *
     * ### Its Role in the BCa Algorithm
     * This function is used in **Step 5** of the `calculateBCaBounds` method.
     * After we have calculated the final, adjusted Z-scores for our confidence
     * bounds (which account for bias `z₀` and skewness `a`), we need to turn
     * them back into percentiles. This function performs that final conversion,
     * giving us the exact percentile points (e.g., 3.1% and 98.2%) we need to
     * look up in our sorted list of bootstrap results.
     *
     * @param x The value (Z-score) at which to evaluate the CDF.
     * @return The probability `P(Z <= x)`, a value between 0.0 and 1.0.
     */
    static inline double standardNormalCdf(double x) noexcept
    {
      // Hoist constants to constexpr for compile-time folding
      constexpr double INV_SQRT2 = 1.0 / 1.4142135623730950488; // 1/sqrt(2)
      return 0.5 * (1.0 + std::erf(x * INV_SQRT2));
    }

    /**
     * @brief Computes the inverse of the Standard Normal CDF (quantile function).
     *
     * @details
     * ### What it Does (In Simple Terms)
     * This function does the **exact opposite** of `standardNormalCdf`. You give
     * it a percentile (a probability `p` between 0.0 and 1.0), and it tells you
     * the corresponding Z-score on the horizontal axis of the bell curve.
     *
     * It's like using a Z-table in reverse. For example:
     * - `inverseNormalCdf(0.5)` would return `0.0`, the center of the distribution.
     * - `inverseNormalCdf(0.975)` would return approximately `1.96`.
     *
     * ### Its Role in the BCa Algorithm
     * This function is critical in two steps of `calculateBCaBounds`:
     * 1.  **Step 3 (Bias `z₀`):** We calculate the proportion of bootstrap results
     * less than our original statistic. This proportion is a percentile. We
     * feed it into this function to get our bias-correction factor, `z₀`.
     * 2.  **Step 5 (Adjusted Percentiles):** We need to know the starting Z-scores
     * for our desired confidence level (e.g., 95%). We call this function
     * with `0.025` and `0.975` to get the initial Z-scores (`z_alpha_lo`
     * and `z_alpha_hi`) before we adjust them for bias and skew.
     *
     * @param p The probability or percentile (a value between 0 and 1).
     * @return The Z-score `x` such that `P(Z <= x) = p`.
     */
    static inline double inverseNormalCdf(double p) noexcept
    {
      if (p <= 0.0)
	return -std::numeric_limits<double>::infinity();

      if (p >= 1.0) return  std::numeric_limits<double>::infinity();

      return (p < 0.5) ? -inverseNormalCdfHelper(p) : inverseNormalCdfHelper(1.0 - p);
    }

    /**
     * @brief A helper function for `inverseNormalCdf` using a mathematical approximation.
     *
     * @details
     * ### What it Does (In Simple Terms)
     * Calculating the inverse normal CDF exactly is extremely complex. There's no
     * simple formula for it. This function uses a well-known and highly accurate
     * mathematical "shortcut" (a rational approximation) to compute the result.
     * It's the computational engine that does the heavy lifting for the main
     * `inverseNormalCdf` function.
     *
     * For a maintainer, this function can be treated as a "black box." Its internal
     * constants and formula are a standard, proven method for getting the job done
     * with sufficient precision for confidence interval calculations.
     *
     * ### Its Role in the BCa Algorithm
     * This function does not have a direct role in the BCa algorithm itself; it
     * is purely an implementation detail. It is called by `inverseNormalCdf` to
     * perform the actual calculation.
     *
     * Implements the rational approximation from Approximation from Abramowitz & Stegun,
     * "Handbook of Mathematical Functions", Eq. 26.2.23.
     *
     * Citation details:
     *
     * Chapter 26: "Probability Functions"
     *
     * Section 2: "Normal or Gaussian Probability Function"
     *
     * Formula 23: This is the specific equation number within that section.
     * It provides a highly accurate rational approximation for the inverse normal
     * CDF, which is what the inverseNormalCdfHelper function implements.
     *
     * @param p The probability, assumed to be in the range (0, 0.5].
     * @return The inverse normal CDF value.
     */
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
    static inline int unbiasedIndex(double p, unsigned int B) noexcept
    {
      int idx = static_cast<int>(std::floor(p * (static_cast<double>(B) + 1.0))) - 1;

      if (idx < 0)
	idx = 0;

      const int maxIdx = static_cast<int>(B) - 1;

      if (idx > maxIdx)
	idx = maxIdx;
 
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
   * @throws std::invalid_argument If the time frame is unsupported or intraday_minutes_per_bar
   * is zero for INTRADAY data.
   */
  inline double calculateAnnualizationFactor(TimeFrame::Duration timeFrame,
					     int intraday_minutes_per_bar = 0,
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
	  double bars_per_hour = 60.0 / static_cast<double>(intraday_minutes_per_bar);
	  if (!(bars_per_hour > 0.0) || !(trading_days_per_year > 0.0) || !(trading_hours_per_day > 0.0))
            {
                throw std::invalid_argument("Annualization inputs must be positive finite values.");
            }
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
    BCaAnnualizer(const BCaBootStrap<Decimal, Sampler, Rng>& bca_results,
		  double annualization_factor)
    {
      if (!(annualization_factor > 0.0) || !std::isfinite(annualization_factor))
	{
	  throw std::invalid_argument("Annualization factor must be positive and finite.");
	}

      const Decimal one  = DecimalConstants<Decimal>::DecimalOne;
      const Decimal epsD = Decimal(1e-12);        // small ε to stay inside domain of log1p
      const Decimal neg1 = DecimalConstants<Decimal>::DecimalMinusOne;

      const Decimal r_mean  = bca_results.getMean();
      const Decimal r_lower = bca_results.getLowerBound();
      const Decimal r_upper = bca_results.getUpperBound();

      // Clip returns to (-1, +∞) to ensure log1p is well-defined.
      const Decimal r_mean_c  = std::max(r_mean,  neg1 + epsD);
      const Decimal r_lower_c = std::max(r_lower, neg1 + epsD);
      const Decimal r_upper_c = std::max(r_upper, neg1 + epsD);

      // Compute with long double intermediates for stability:
      auto annualize = [annualization_factor](const Decimal& r) -> Decimal
      {
	const long double lr = std::log1p(static_cast<long double>(r.getAsDouble()));
	const long double K  = static_cast<long double>(annualization_factor);
	long double y  = std::exp(K * lr) - 1.0L;

	// Guard: near-ruin + large K can underflow numerically to exactly -1.0.
	// Bump to a value that will remain > -1.0 after quantizing to decimal<8>.
	const long double bump = 1e-7L;                // 10 ULPs at 8 decimal places

	if (y <= -1.0L)
	  y = -1.0L + bump;

	return Decimal(static_cast<double>(y));
      };

      m_annualized_mean        = annualize(r_mean_c);
      m_annualized_lower_bound = annualize(r_lower_c);
      m_annualized_upper_bound = annualize(r_upper_c);
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
    Decimal m_annualized_mean{};
    Decimal m_annualized_lower_bound{};
    Decimal m_annualized_upper_bound{};
  };
} // namespace mkc_timeseries

#endif // __BCA_BOOTSTRAP_H
