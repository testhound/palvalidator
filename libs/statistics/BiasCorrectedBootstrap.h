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
#include <type_traits>

#include "BootstrapTypes.h"
#include "DecimalConstants.h"
#include "number.h"
#include "randutils.hpp"
#include "RngUtils.h"
#include "TimeFrame.h"
#include "StatUtils.h"
#include "Annualizer.h"
#include "NormalDistribution.h"

namespace mkc_timeseries
{
  using palvalidator::analysis::IntervalType;
  
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
   const size_t idx = mkc_timeseries::rng_utils::get_random_index(rng, x.size());
   y.push_back(x[idx]);
 }
      return y;
    }

    /**
     * @brief In-place resampling interface for MOutOfNPercentileBootstrap compatibility.
     *
     * This overload fills the output vector y with n resampled elements from x.
     *
     * @param x The original data vector.
     * @param y The output vector to fill (will be resized to n).
     * @param n The number of elements to resample.
     * @param rng A high-quality random number generator.
     */
    void operator()(const std::vector<Decimal>& x, std::vector<Decimal>& y, size_t n, Rng& rng) const
    {
      y = (*this)(x, n, rng);  // Delegate to the existing operator()
    }

    /**
     * @brief Gets the effective block length for IID resampling (always 1).
     * @return 1 (IID resampling has no block structure).
     */
    size_t getL() const { return 1; }

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
      size_t idx = mkc_timeseries::rng_utils::get_random_index(rng, xn);

      size_t pos = 0;  // write cursor into y
      while (pos < n)
	{
	  size_t len = 1 + m_geo(mkc_timeseries::rng_utils::get_engine(rng));                // proposed block length
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
	  idx = mkc_timeseries::rng_utils::get_random_index(rng, xn);
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

    /**
     * @brief Gets the mean block length (alias for compatibility with MOutOfNPercentileBootstrap).
     * @return The mean block length.
     */
    size_t getL() const { return m_L; }

    /**
     * @brief In-place resampling interface for MOutOfNPercentileBootstrap compatibility.
     *
     * This overload fills the output vector y with m resampled elements from x.
     *
     * @param x The original data vector.
     * @param y The output vector to fill (will be resized to m).
     * @param m The number of elements to resample.
     * @param rng A high-quality random number generator.
     */
    void operator()(const std::vector<Decimal>& x, std::vector<Decimal>& y, size_t m, Rng& rng) const
    {
      y = (*this)(x, m, rng);  // Delegate to the existing operator()
    }

  private:
    size_t m_L;
    mutable std::geometric_distribution<size_t> m_geo;
  };

  /**
   * @class BCaBootStrap
   * @brief Bias-Corrected and Accelerated (BCa) bootstrap confidence intervals.
   * 
   * Implements the BCa method from Efron & Tibshirani (1993), which provides
   * second-order accurate confidence intervals by correcting for bias (z0) and
   * skewness (acceleration parameter a).
   * 
   * VALIDITY CONSTRAINTS:
   * BCa assumes the statistic's sampling distribution can be approximated by
   * an Edgeworth expansion. This assumption breaks down when:
   * 
   *   - |z0| > 0.6:  Extreme bias in the bootstrap distribution
   *   - |a|  > 0.25: Extreme skewness (Hall 1992, Efron 1987)
   * 
   * When these thresholds are exceeded, the BCa interval may have poor coverage.
   * Users should:
   *   1. Check getZ0() and getAcceleration() after calculation
   *   2. Consider using PercentileT or MOutOfN bootstrap for extreme cases
   *   3. Or use AutoBootstrapSelector, which automatically handles these checks
   * 
   * @see AutoBootstrapSelector for automatic method selection based on diagnostics
   * 
   * References:
   *   - Efron, B. (1987). JASA 82(397), 171-185
   *   - Efron & Tibshirani (1993). An Introduction to the Bootstrap, Ch. 14
   *   - Hall, P. (1992). The Bootstrap and Edgeworth Expansion, Sec. 3.6
   */
  template <class Decimal,
            class Sampler  = IIDResampler<Decimal>,
            class Rng      = randutils::mt19937_rng,
            class Provider = void>
  class BCaBootStrap
  {
  public:
    using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

    /**
     * @brief Constructs a BCaBootStrap object with default statistic and sampler.
     * Legacy-compatible constructor.
     */
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level = 0.95,
		 IntervalType interval_type = IntervalType::TWO_SIDED)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_statistic(&mkc_timeseries::StatUtils<Decimal>::computeMean),
        m_sampler(Sampler{}),
        m_is_calculated(false),
        m_z0(0.0),
        m_accel(DecimalConstants<Decimal>::DecimalZero),
	m_interval_type(interval_type)
    {
      validateConstructorArgs();
    }

    /**
     * @brief Constructs a BCaBootStrap object with a custom statistic.
     * Legacy-compatible constructor.
     */
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level,
                 StatFn statistic,
		 IntervalType interval_type = IntervalType::TWO_SIDED)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_statistic(std::move(statistic)),
        m_sampler(Sampler{}),
        m_is_calculated(false),
        m_z0(0.0),
        m_accel(DecimalConstants<Decimal>::DecimalZero),
	m_interval_type(interval_type)
    {
      if (!m_statistic)
        throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
      validateConstructorArgs();
    }

    /**
     * @brief Constructs a BCaBootStrap object with a custom statistic and sampler.
     * Legacy-compatible constructor.
     */
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level,
                 StatFn statistic,
                 Sampler sampler,
		 IntervalType interval_type = IntervalType::TWO_SIDED)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_statistic(std::move(statistic)),
        m_sampler(std::move(sampler)),
        m_is_calculated(false),
        m_z0(0.0),
        m_accel(DecimalConstants<Decimal>::DecimalZero),
	m_interval_type(interval_type)
    {
      if (!m_statistic)
        throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
      validateConstructorArgs();
    }

    /**
     * @brief Constructs a BCaBootStrap object with a custom statistic, sampler, and RNG Provider.
     * Enabled only when Provider != void. Uses provider.make_engine(b) for per-replicate RNGs.
     */
    template <class P = Provider, std::enable_if_t<!std::is_void_v<P>, int> = 0>
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level,
                 StatFn statistic,
                 Sampler sampler,
                 const P& provider,
		 IntervalType interval_type = IntervalType::TWO_SIDED)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_statistic(std::move(statistic)),
        m_sampler(std::move(sampler)),
        m_provider(provider),
        m_is_calculated(false),
        m_z0(0.0),
        m_accel(DecimalConstants<Decimal>::DecimalZero),
	m_interval_type(interval_type)
    {
      if (!m_statistic)
        throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
      validateConstructorArgs();
    }

    virtual ~BCaBootStrap() = default;

    // --- Public accessors for BCa interval ---
    Decimal getMean() const
    {
      ensureCalculated();
      return m_theta_hat;
    }

    Decimal getStatistic() const { return getMean(); }

    Decimal getLowerBound() const
    {
      ensureCalculated();
      return m_lower_bound;
    }

    Decimal getUpperBound() const
    {
      ensureCalculated();
      return m_upper_bound;
    }

    // --- New Efron diagnostics getters ---

    /**
     * @brief Returns the BCa bias-correction parameter z0.
     */
    double getZ0() const
    {
      ensureCalculated();
      return m_z0;
    }

    /**
     * @brief Returns the BCa acceleration parameter a.
     */
    Decimal getAcceleration() const
    {
      ensureCalculated();
      return m_accel;
    }

    /**
     * @brief Returns the confidence level used for this BCa interval.
     */
    double getConfidenceLevel() const
    {
      return m_confidence_level;
    }

    /**
     * @brief Returns the number of bootstrap resamples B.
     */
    unsigned int getNumResamples() const
    {
      return m_num_resamples;
    }

    /**
     * @brief Returns the original sample size n.
     */
    std::size_t getSampleSize() const
    {
      return m_returns.size();
    }

    /**
     * @brief Returns the vector of bootstrap statistics {θ*_b}.
     *
     * The statistics are stored in unsorted form (as generated) so that callers
     * can compute arbitrary diagnostics (skewness, kurtosis, etc.) without
     * relying on any internal ordering.
     */
    const std::vector<Decimal>& getBootstrapStatistics() const
    {
      ensureCalculated();
      return m_bootstrapStats;
    }

    static double computeExtremeQuantile(double alpha, bool is_upper)
    {
      constexpr double EXTREME_TAIL_RATIO = 1000.0;
      const double extreme_tail_prob = alpha / EXTREME_TAIL_RATIO;
  
      return is_upper ? (1.0 - extreme_tail_prob) : extreme_tail_prob;
    }

  protected:
    // Data & config
    const std::vector<Decimal>& m_returns;
    unsigned int                m_num_resamples;
    double                      m_confidence_level;
    StatFn                      m_statistic;
    Sampler                     m_sampler;
    // Provider storage: materialized only if Provider != void; otherwise an empty byte
    [[no_unique_address]] std::conditional_t<std::is_void_v<Provider>, char, Provider> m_provider{};
    bool                        m_is_calculated;

    // Results
    Decimal m_theta_hat{};
    Decimal m_lower_bound{};
    Decimal m_upper_bound{};

    // Efron/BCa diagnostics
    double                    m_z0;               // bias-correction
    Decimal                   m_accel;            // acceleration
    std::vector<Decimal>      m_bootstrapStats;   // bootstrap θ*'s (unsorted copy)
    IntervalType m_interval_type;

    // Test hooks (kept for mocks)
    void setStatistic(const Decimal& theta) { m_theta_hat = theta; }
    void setMean(const Decimal& theta)      { m_theta_hat = theta; }
    void setLowerBound(const Decimal& lb)   { m_lower_bound = lb; }
    void setUpperBound(const Decimal& ub)   { m_upper_bound = ub; }

    static double computeAlpha(double confidence_level, IntervalType type)
    {
      const double tail_prob = 1.0 - confidence_level;
    
      switch (type)
	{
	case IntervalType::TWO_SIDED:
	  return tail_prob * 0.5;  // Split between both tails
        
	case IntervalType::ONE_SIDED_LOWER:
	  return tail_prob;         // All in lower tail
        
	case IntervalType::ONE_SIDED_UPPER:
	  return tail_prob;         // All in upper tail (swap bounds at end)
        
	default:
	  return tail_prob * 0.5;   // Defensive: default to two-sided
	}
    }


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
     * @brief Core BCa computation. Uses legacy thread_local RNG when Provider=void,
     *        otherwise uses provider.make_engine(b) per replicate (CRN-friendly).
     *
     * Also populates:
     *  - m_z0 (bias correction)
     *  - m_accel (acceleration)
     *  - m_bootstrapStats (unsorted copy of all θ*_b)
     */
    virtual void calculateBCaBounds()
    {
      if (m_is_calculated) return;

      const size_t n = m_returns.size();
      if (n < 2)
        throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");

      // (1) θ̂ on original sample
      m_theta_hat = m_statistic(m_returns);

      // (2) Bootstrap replicates; track count of stats less than θ̂
      std::vector<Decimal> boot_stats;
      boot_stats.reserve(m_num_resamples);

      unsigned int count_less = 0;

      if constexpr (std::is_void_v<Provider>) {
        // --- Legacy path: thread_local RNG preserved for backward-compatibility ---
        thread_local static Rng rng;
        for (unsigned int b = 0; b < m_num_resamples; ++b) {
          std::vector<Decimal> resample = m_sampler(m_returns, n, rng);
          const Decimal stat_b = m_statistic(resample);
          if (stat_b < m_theta_hat) ++count_less;
          boot_stats.push_back(stat_b);
        }
      } else {
        // --- Provider path: per-replicate deterministic engines (CRN) ---
        for (unsigned int b = 0; b < m_num_resamples; ++b) {
          Rng rng = m_provider.make_engine(b);     // fresh engine per replicate
          std::vector<Decimal> resample = m_sampler(m_returns, n, rng);
          const Decimal stat_b = m_statistic(resample);
          if (stat_b < m_theta_hat) ++count_less;
          boot_stats.push_back(stat_b);
        }
      }

      // Early collapse if all replicates equal (degenerate distribution)
      bool all_equal = true;
      for (size_t i = 1; i < boot_stats.size(); ++i) {
        if (!(boot_stats[i] == boot_stats[0])) { all_equal = false; break; }
      }
      if (all_equal) {
        // store diagnostics in a benign way for degenerate case
        m_lower_bound    = boot_stats[0];
        m_upper_bound    = boot_stats[0];
        m_theta_hat      = boot_stats[0];
        m_z0             = 0.0;
        m_accel          = DecimalConstants<Decimal>::DecimalZero;
        m_bootstrapStats = boot_stats;
        m_is_calculated  = true;
        return;
      }

      // Preserve an unsorted copy of bootstrap statistics for diagnostics.
      m_bootstrapStats = boot_stats;

      // (3) Bias-correction z0

      // Clamp prop_less away from exact 0.0 and 1.0
      const double prop_less_raw = static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
      const double prop_less = std::max(1e-10, std::min(1.0 - 1e-10, prop_less_raw));
      const double z0        = NormalDistribution::inverseNormalCdf(prop_less);
      m_z0                   = z0;

      // (4) Acceleration a via jackknife (sampler-provided)
      const std::vector<Decimal> jk_stats = m_sampler.jackknife(m_returns, m_statistic);
      const size_t n_jk = jk_stats.size();

      Decimal jk_sum = DecimalConstants<Decimal>::DecimalZero;
      for (const auto& th : jk_stats) jk_sum += th;
      const Decimal jk_avg = jk_sum / Decimal(n_jk);

      double num_d = 0.0; // Σ d^3
      double den_d = 0.0; // Σ d^2
      for (const auto& th : jk_stats) {
        const double d  = num::to_double(jk_avg - th);
        const double d2 = d * d;
        den_d += d2;
        num_d += d2 * d;
      }

      Decimal a = DecimalConstants<Decimal>::DecimalZero;
      if (den_d > 1e-100)
	{  // Threshold to prevent underflow
	  const double den15 = std::pow(den_d, 1.5);
	  if (den15 > 1e-100) // Additional safety
	    a = Decimal(num_d / (6.0 * den15));
	}

      m_accel = a;

      // (5) Adjusted percentiles → bounds
      const double alpha = computeAlpha(m_confidence_level, m_interval_type);

      // Compute z-quantiles based on interval type
      double z_alpha_lo, z_alpha_hi;

      switch (m_interval_type)
	{
	case IntervalType::TWO_SIDED:
	  // Traditional: split tail probability between both sides
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);           // α/2
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);     // 1 - α/2
	  break;
    
	case IntervalType::ONE_SIDED_LOWER:
	  // Lower bound: all tail mass in lower tail
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(computeExtremeQuantile(alpha,
										   true)); 
	  break;
    
	case IntervalType::ONE_SIDED_UPPER:
	   z_alpha_lo = NormalDistribution::inverseNormalCdf(computeExtremeQuantile(alpha,
										    false));
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;
	  
	default:
	  // Defensive fallback to two-sided
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;
	}

      const double a_d       = num::to_double(a);
      const bool   z0_finite = std::isfinite(z0);

      const double alpha1 = (!z0_finite || std::abs(a_d) < 1e-12)
        ? NormalDistribution::standardNormalCdf(z0 + z_alpha_lo)
        : NormalDistribution::standardNormalCdf(z0 + (z0 + z_alpha_lo) / (1.0 - a_d * (z0 + z_alpha_lo)));

      const double alpha2 = (!z0_finite || std::abs(a_d) < 1e-12)
        ? NormalDistribution::standardNormalCdf(z0 + z_alpha_hi)
        : NormalDistribution::standardNormalCdf(z0 + (z0 + z_alpha_hi) / (1.0 - a_d * (z0 + z_alpha_hi)));

      const auto clamp01 = [](double v) noexcept {
        return (v <= 0.0) ? std::nextafter(0.0, 1.0)
             : (v >= 1.0) ? std::nextafter(1.0, 0.0)
             : v;
      };

      const double a1 = clamp01(alpha1);
      const double a2 = clamp01(alpha2);

      const int li = unbiasedIndex(std::min(a1, a2), m_num_resamples);
      const int ui = unbiasedIndex(std::max(a1, a2), m_num_resamples);

      // Work on a local copy so m_bootstrapStats stays as originally generated
      std::vector<Decimal> work = boot_stats;

      // Select order statistics in O(B)
      std::nth_element(work.begin(), work.begin() + li, work.end());
      m_lower_bound = work[li];

      std::nth_element(work.begin(), work.begin() + ui, work.end());
      m_upper_bound = work[ui];
      
      m_is_calculated = true;
    }

  public:
    /**
     * @brief Converts a probability p to an array index for the bootstrap distribution.
     *
     * Implements the formula from Efron & Tibshirani (1993), Eq 14.15:
     *   index = ⌊p(B+1)⌋ - 1
     *
     * Clamps result to [0, B-1] to handle edge cases where p ≈ 0 or p ≈ 1.
     *
     * @param p The probability (should be in [0, 1]).
     * @param B The number of bootstrap resamples.
     * @return The corresponding array index in [0, B-1].
     */
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
   * @throws std::invalid_argument If the time frame is unsupported or intraday_minutes_per_bar
   * is zero for INTRADAY data.
   */
  inline double calculateAnnualizationFactor(TimeFrame::Duration timeFrame,
					     int intraday_minutes_per_bar = 0,
					     double trading_days_per_year = 252.0,
					     double trading_hours_per_day = 6.5)
  {
    return mkc_timeseries::computeAnnualizationFactor(timeFrame,
						      intraday_minutes_per_bar,
						      trading_days_per_year,
						      trading_hours_per_day);
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

      const Decimal r_mean  = bca_results.getMean();
      const Decimal r_lower = bca_results.getLowerBound();
      const Decimal r_upper = bca_results.getUpperBound();

      using A = Annualizer<Decimal>;
      const auto trip = A::annualize_triplet(r_lower, r_mean, r_upper, annualization_factor);

      m_annualized_mean        = trip.mean;
      m_annualized_lower_bound = trip.lower;
      m_annualized_upper_bound = trip.upper;
    }

    template <class Sampler, class Rng, class Provider,
	      std::enable_if_t<!std::is_void_v<Provider>, int> = 0>
    BCaAnnualizer(const BCaBootStrap<Decimal, Sampler, Rng, Provider>& bca_results,
		  double annualization_factor)
    {
      if (!(annualization_factor > 0.0) || !std::isfinite(annualization_factor))
	throw std::invalid_argument("Annualization factor must be positive and finite.");

      const Decimal r_mean  = bca_results.getMean();
      const Decimal r_lower = bca_results.getLowerBound();
      const Decimal r_upper = bca_results.getUpperBound();

      using A = Annualizer<Decimal>;
      const auto trip = A::annualize_triplet(r_lower, r_mean, r_upper, annualization_factor);
      m_annualized_mean        = trip.mean;
      m_annualized_lower_bound = trip.lower;
      m_annualized_upper_bound = trip.upper;
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
