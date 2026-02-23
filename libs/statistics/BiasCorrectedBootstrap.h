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
// GENERALIZATION NOTE (trade-level bootstrap):
//   BCaBootStrap now accepts a 5th template parameter SampleType (default: Decimal).
//   When SampleType = Trade<Decimal>, the bootstrap operates on a vector of Trade
//   objects rather than a flat vector of returns. All existing instantiations with
//   fewer than 5 template parameters are 100% backward compatible.
//
//   IIDResampler now uses T in place of Decimal so it can be instantiated as
//   IIDResampler<Trade<Decimal>> for trade-level i.i.d. resampling.

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

    if (K < 2 || n < 2 || n < K * minLen)
      {
        return out;
      }

    const std::size_t base = n / K;
    const std::size_t rem  = n % K;

    std::size_t start = 0;
    for (std::size_t k = 0; k < K; ++k)
      {
        std::size_t len = base + (k < rem ? 1 : 0);

        if (len < minLen)
	  {
            return {};
	  }

        out.emplace_back(start, start + len);
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
   * there are no dependencies or serial correlations between elements.
   *
   * GENERALIZATION: The template parameter is now T rather than Decimal, allowing
   * this resampler to be used with any copyable type. In particular:
   *
   *   - IIDResampler<Decimal>          : bar-level bootstrap (existing usage, unchanged)
   *   - IIDResampler<Trade<Decimal>>   : trade-level bootstrap (new capability)
   *
   * The jackknife method is now a function template so it can accept statistics
   * that return a different type than T (e.g., a stat that takes
   * std::vector<Trade<Decimal>> and returns Decimal).
   *
   * @tparam T   The element type of the sample vector (e.g., Decimal or Trade<Decimal>).
   * @tparam Rng The random number generator type.
   */
  template <class T, class Rng = randutils::mt19937_rng>
  struct IIDResampler
  {
    /**
     * @brief Resamples the input vector with replacement.
     *
     * @param x The original data vector.
     * @param n The size of the resampled vector.
     * @param rng A high-quality random number generator.
     * @return A new vector of size n, containing elements sampled with replacement from x.
     * @throws std::invalid_argument If the input vector x is empty.
     */
    std::vector<T>
    operator()(const std::vector<T>& x, size_t n, Rng& rng) const
    {
      if (x.empty())
	{
	  throw std::invalid_argument("IIDResampler: empty sample.");
	}
      std::vector<T> y;
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
     * @param x The original data vector.
     * @param y The output vector to fill (will be resized to n).
     * @param n The number of elements to resample.
     * @param rng A high-quality random number generator.
     */
    void operator()(const std::vector<T>& x, std::vector<T>& y, size_t n, Rng& rng) const
    {
      y = (*this)(x, n, rng);
    }

    /**
     * @brief Gets the effective block length for IID resampling (always 1).
     * @return 1 (IID resampling has no block structure).
     */
    size_t getL() const { return 1; }

    /**
     * @brief Performs a classic delete-one jackknife.
     *
     * @details
     * The jackknife procedure computes the acceleration factor 'a' for the BCa
     * bootstrap by measuring the influence of each individual element on the
     * statistic. For a dataset of size n it produces n pseudo-values, each
     * computed on a leave-one-out subset of size n-1.
     *
     * GENERALIZATION: StatFunc is now a deduced template parameter rather than a
     * fixed std::function typedef. This allows the statistic to map
     * std::vector<T> -> R where R need not equal T. In particular, when
     * T = Trade<Decimal> and the statistic is GeoMeanStat, R = Decimal and the
     * returned pseudo-value vector is std::vector<Decimal> as required by
     * BCaBootStrap::calculateBCaBounds().
     *
     * The backward-compatible alias StatFn is preserved for any code that
     * references IIDResampler<Decimal>::StatFn explicitly.
     *
     * Reference: Efron, B. (1987). Better Bootstrap Confidence Intervals.
     * Journal of the American Statistical Association, 82(397), 171–185.
     *
     * @tparam StatFunc  Any callable with signature R(const std::vector<T>&).
     * @param  x         The original data vector.
     * @param  stat      The statistic function applied to each leave-one-out subset.
     * @return           A vector of n jackknife pseudo-values of type R.
     * @throws std::invalid_argument If x has fewer than 2 elements.
     */
    template <class StatFunc>
    auto jackknife(const std::vector<T>& x, const StatFunc& stat) const
      -> std::vector<decltype(stat(x))>
    {
      using ResultType = decltype(stat(x));

      const size_t n = x.size();
      if (n < 2)
	{
	  throw std::invalid_argument("IIDResampler::jackknife requires n >= 2.");
	}

      std::vector<ResultType> jk;
      jk.reserve(n);

      std::vector<T> tmp;
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

    // ---------------------------------------------------------------------------
    // Backward-compatible StatFn typedef.
    //
    // Existing code that references IIDResampler<Decimal>::StatFn continues to
    // compile unchanged. When T = Trade<Decimal> this typedef produces
    // std::function<Trade<Decimal>(const std::vector<Trade<Decimal>>&)>, which is
    // not the correct statistic signature for the trade path — but that is fine
    // because trade-level callers never reference this alias; they pass a
    // GeoMeanStat (or equivalent) callable directly to BCaBootStrap.
    // ---------------------------------------------------------------------------
    using StatFn = std::function<T(const std::vector<T>&)>;
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
   * beginning.
   *
   * NOTE: This resampler is appropriate for bar-level bootstrapping where
   * consecutive bars exhibit serial correlation. For trade-level bootstrapping,
   * use IIDResampler<Trade<Decimal>> since trades are the independent atomic unit
   * and no block structure is required.
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

    std::vector<Decimal>
    operator()(const std::vector<Decimal>& x, size_t n, Rng& rng) const
    {
      if (x.empty())
	throw std::invalid_argument("StationaryBlockResampler: empty sample.");

      const size_t xn = x.size();
      std::vector<Decimal> y(n);

      size_t idx = mkc_timeseries::rng_utils::get_random_index(rng, xn);

      size_t pos = 0;
      while (pos < n)
	{
	  size_t len = 1 + m_geo(mkc_timeseries::rng_utils::get_engine(rng));
	  size_t remaining = n - pos;
	  size_t k = std::min({len, remaining, xn});

	  size_t room_to_end = xn - idx;
	  if (k <= room_to_end) {
	    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(idx),
		       static_cast<std::ptrdiff_t>(k),
		       y.begin() + static_cast<std::ptrdiff_t>(pos));
	  } else {
	    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(idx),
		       static_cast<std::ptrdiff_t>(room_to_end),
		       y.begin() + static_cast<std::ptrdiff_t>(pos));
	    const size_t rem = k - room_to_end;
	    std::copy_n(x.begin(),
		       static_cast<std::ptrdiff_t>(rem),
		       y.begin() + static_cast<std::ptrdiff_t>(pos + room_to_end));
	  }

	  pos += k;
	  idx = mkc_timeseries::rng_utils::get_random_index(rng, xn);
	}

      return y;
    }

    /**
     * @brief Performs a delete-block jackknife (Künsch 1989) for the BCa
     * acceleration factor.
     *
     * Uses non-overlapping blocks stepping by L_eff to produce floor(n/L_eff)
     * genuinely distinct pseudo-values, avoiding the systematic underestimation
     * of |a| caused by the sliding-window delete approach.
     *
     * Reference: Künsch, H. R. (1989). The Jackknife and the Bootstrap for
     * General Stationary Observations. The Annals of Statistics, 17(3), 1217–1241.
     *
     * @tparam StatFunc Any callable with signature R(const std::vector<Decimal>&).
     * @param x The original time series data vector.
     * @param stat The statistic function to apply to each jackknife replicate.
     * @return A vector of floor(n/L_eff) jackknife pseudo-values.
     * @throws std::invalid_argument If x has fewer than 3 elements or the sample
     *         is too small for the configured block length.
     */
    template <class StatFunc>
    auto jackknife(const std::vector<Decimal>& x, const StatFunc& stat) const
      -> std::vector<decltype(stat(x))>
    {
      using ResultType = decltype(stat(x));

      const std::size_t n = x.size();

      const std::size_t minKeep = 2;
      if (n < minKeep + 1)
	{
	  throw std::invalid_argument(
				      "StationaryBlockResampler::jackknife requires n >= 3.");
	}

      const std::size_t L_eff = std::min<std::size_t>(m_L, n - minKeep);

      if (n < L_eff + minKeep)
	{
	  throw std::invalid_argument(
				      "StationaryBlockResampler::jackknife: sample too small for "
				      "delete-block jackknife with this block length. "
				      "Reduce block length or increase sample size.");
	}

      const std::size_t keep = n - L_eff;
      const std::size_t numBlocks = n / L_eff;

      std::vector<ResultType> jk(numBlocks);
      std::vector<Decimal> y(keep);

      for (std::size_t b = 0; b < numBlocks; ++b)
	{
	  const std::size_t start      = b * L_eff;
	  const std::size_t start_keep = (start + L_eff) % n;
	  const std::size_t tail       = std::min<std::size_t>(keep, n - start_keep);

	  std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
		      static_cast<std::ptrdiff_t>(tail),
		      y.begin());

	  const std::size_t head = keep - tail;
	  if (head != 0)
	    {
	      std::copy_n(x.begin(),
			  static_cast<std::ptrdiff_t>(head),
			  y.begin() + static_cast<std::ptrdiff_t>(tail));
	    }

	  jk[b] = stat(y);
	}

      return jk;
    }

    size_t meanBlockLen() const { return m_L; }
    size_t getL()         const { return m_L; }

    void operator()(const std::vector<Decimal>& x,
		    std::vector<Decimal>& y,
		    size_t m,
		    Rng& rng) const
    {
      y = (*this)(x, m, rng);
    }

    // Backward-compatible typedef
    using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

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
   * GENERALIZATION (trade-level bootstrap):
   *   The 5th template parameter SampleType (default: Decimal) controls the
   *   element type of the input data vector and the resampler's output type.
   *
   *   Bar-level (existing, default):
   *     BCaBootStrap<Decimal>
   *     BCaBootStrap<Decimal, StationaryBlockResampler<Decimal>>
   *     -- SampleType defaults to Decimal; behaviour is identical to before.
   *
   *   Trade-level (new):
   *     BCaBootStrap<Decimal,
   *                  IIDResampler<Trade<Decimal>>,
   *                  randutils::mt19937_rng,
   *                  void,
   *                  Trade<Decimal>>
   *     -- m_returns holds std::vector<Trade<Decimal>>
   *     -- StatFn maps std::vector<Trade<Decimal>> -> Decimal
   *     -- Sampler produces std::vector<Trade<Decimal>>
   *     -- All BCa math (z0, a, bounds) still operates on Decimal throughout.
   *
   * All existing code with 1–4 explicit template parameters compiles unchanged.
   *
   * @tparam Decimal     Numeric type for statistics and bounds (e.g., dec::decimal<8>).
   * @tparam Sampler     Resampling policy (default: IIDResampler<Decimal>).
   * @tparam Rng         Random number generator type (default: randutils::mt19937_rng).
   * @tparam Provider    Optional per-replicate RNG provider for CRN (default: void).
   * @tparam SampleType  Element type of the input data vector (default: Decimal).
   *                     Set to Trade<Decimal> for trade-level bootstrapping.
   *
   * @see AutoBootstrapSelector for automatic method selection based on diagnostics
   *
   * References:
   *   - Efron, B. (1987). JASA 82(397), 171-185
   *   - Efron & Tibshirani (1993). An Introduction to the Bootstrap, Ch. 14
   *   - Hall, P. (1992). The Bootstrap and Edgeworth Expansion, Sec. 3.6
   */
  template <class Decimal,
            class Sampler    = IIDResampler<Decimal>,
            class Rng        = randutils::mt19937_rng,
            class Provider   = void,
            class SampleType = Decimal>
  class BCaBootStrap
  {
  public:
    /**
     * @brief Statistic function type: maps a sample vector to a Decimal result.
     *
     * When SampleType = Decimal (default/bar-level):
     *   StatFn = std::function<Decimal(const std::vector<Decimal>&)>
     *   -- identical to the previous definition; all existing code compiles unchanged.
     *
     * When SampleType = Trade<Decimal> (trade-level):
     *   StatFn = std::function<Decimal(const std::vector<Trade<Decimal>>&)>
     *   -- statistics such as GeoMeanStat provide operator()(const std::vector<Trade<Decimal>>&)
     *      and satisfy this typedef directly, with no adapter needed.
     */
    using StatFn = std::function<Decimal(const std::vector<SampleType>&)>;

    // -------------------------------------------------------------------------
    // Constructors
    //
    // All constructors are identical to the original except that `returns` is
    // now std::vector<SampleType> rather than std::vector<Decimal>. When
    // SampleType = Decimal (the default) the signatures are byte-for-byte
    // identical to the originals and all existing call sites compile unchanged.
    // -------------------------------------------------------------------------

    /**
     * @brief Default-statistic constructor (legacy-compatible).
     *
     * Uses StatUtils<Decimal>::computeMean as the statistic. This constructor
     * is only appropriate when SampleType = Decimal, since computeMean expects
     * a flat vector of Decimal. For trade-level use, supply a statistic explicitly.
     */
    BCaBootStrap(const std::vector<SampleType>& returns,
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
     * @brief Custom-statistic constructor (legacy-compatible).
     */
    BCaBootStrap(const std::vector<SampleType>& returns,
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
     * @brief Custom-statistic + custom-sampler constructor (legacy-compatible).
     */
    BCaBootStrap(const std::vector<SampleType>& returns,
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
     * @brief Full constructor with RNG Provider (CRN-friendly). Enabled only when
     * Provider != void.
     */
    template <class P = Provider, std::enable_if_t<!std::is_void_v<P>, int> = 0>
    BCaBootStrap(const std::vector<SampleType>& returns,
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

    // -------------------------------------------------------------------------
    // Public accessors (unchanged from original)
    // -------------------------------------------------------------------------

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

    double getZ0() const
    {
      ensureCalculated();
      return m_z0;
    }

    Decimal getAcceleration() const
    {
      ensureCalculated();
      return m_accel;
    }

    double getConfidenceLevel() const
    {
      return m_confidence_level;
    }

    unsigned int getNumResamples() const
    {
      return m_num_resamples;
    }

    /**
     * @brief Returns the original sample size n.
     *
     * For bar-level bootstrapping (SampleType = Decimal) this is the number of
     * return bars. For trade-level bootstrapping (SampleType = Trade<Decimal>)
     * this is the number of trades.
     */
    std::size_t getSampleSize() const
    {
      return m_returns.size();
    }

    /**
     * @brief Returns the vector of bootstrap statistics {θ*_b} in generation order.
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
    // -------------------------------------------------------------------------
    // Data & config
    //
    // m_returns is now std::vector<SampleType>. When SampleType = Decimal this
    // is identical to the original. When SampleType = Trade<Decimal> it holds
    // the trade population from which bootstrap resamples are drawn.
    // -------------------------------------------------------------------------
    const std::vector<SampleType>& m_returns;
    unsigned int                   m_num_resamples;
    double                         m_confidence_level;
    StatFn                         m_statistic;
    Sampler                        m_sampler;

    // Provider storage: zero-size when Provider = void (no overhead).
    [[no_unique_address]]
    std::conditional_t<std::is_void_v<Provider>, char, Provider> m_provider{};

    bool m_is_calculated;

    // Results
    Decimal m_theta_hat{};
    Decimal m_lower_bound{};
    Decimal m_upper_bound{};

    // BCa diagnostics
    double               m_z0;
    Decimal              m_accel;
    std::vector<Decimal> m_bootstrapStats;   // bootstrap θ*'s (unsorted)
    IntervalType         m_interval_type;

    // Test hooks
    void setStatistic(const Decimal& theta) { m_theta_hat = theta; }
    void setMean(const Decimal& theta)      { m_theta_hat = theta; }
    void setLowerBound(const Decimal& lb)   { m_lower_bound = lb; }
    void setUpperBound(const Decimal& ub)   { m_upper_bound = ub; }

    static double computeAlpha(double confidence_level, IntervalType type)
    {
      const double tail_prob = 1.0 - confidence_level;
      switch (type)
	{
	case IntervalType::TWO_SIDED:     return tail_prob * 0.5;
	case IntervalType::ONE_SIDED_LOWER: return tail_prob;
	case IntervalType::ONE_SIDED_UPPER: return tail_prob;
	default:                           return tail_prob * 0.5;
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
     * @brief Core BCa computation.
     *
     * This implementation is unchanged from the original except for two lines:
     *
     *   1. The resampler call produces std::vector<SampleType> rather than
     *      std::vector<Decimal>. When SampleType = Decimal this is identical.
     *      When SampleType = Trade<Decimal> the resampler returns a bootstrap
     *      sample of Trade objects, which the statistic then maps to Decimal.
     *
     *   2. The jackknife call is now resolved against the templated jackknife
     *      on IIDResampler (or StationaryBlockResampler), which deduces the
     *      return type as Decimal regardless of SampleType.
     *
     * All z0, acceleration, and bound calculations operate on std::vector<Decimal>
     * and are unaffected by the SampleType generalization.
     */
    virtual void calculateBCaBounds()
    {
      if (m_is_calculated) return;

      const size_t n = m_returns.size();
      if (n < 2)
        throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");

      // (1) θ̂ on original sample
      m_theta_hat = m_statistic(m_returns);

      // (2) Bootstrap replicates
      std::vector<Decimal> boot_stats;
      boot_stats.reserve(m_num_resamples);
      unsigned int count_less = 0;

      if constexpr (std::is_void_v<Provider>)
	{
	  // Legacy path: thread_local RNG preserved for backward-compatibility.
	  thread_local static Rng rng;
	  for (unsigned int b = 0; b < m_num_resamples; ++b)
	    {
	      // resample is std::vector<SampleType> (Decimal or Trade<Decimal>)
	      std::vector<SampleType> resample = m_sampler(m_returns, n, rng);
	      const Decimal stat_b = m_statistic(resample);
	      if (stat_b < m_theta_hat) ++count_less;
	      boot_stats.push_back(stat_b);
	    }
	}
      else
	{
	  // Provider path: per-replicate deterministic engines (CRN).
	  for (unsigned int b = 0; b < m_num_resamples; ++b)
	    {
	      Rng rng = m_provider.make_engine(b);
	      std::vector<SampleType> resample = m_sampler(m_returns, n, rng);
	      const Decimal stat_b = m_statistic(resample);
	      if (stat_b < m_theta_hat) ++count_less;
	      boot_stats.push_back(stat_b);
	    }
	}

      // Early collapse: degenerate distribution (all replicates equal)
      bool all_equal = true;
      for (size_t i = 1; i < boot_stats.size(); ++i)
	{
	  if (!(boot_stats[i] == boot_stats[0])) { all_equal = false; break; }
	}
      if (all_equal)
	{
	  m_lower_bound    = boot_stats[0];
	  m_upper_bound    = boot_stats[0];
	  m_theta_hat      = boot_stats[0];
	  m_z0             = 0.0;
	  m_accel          = DecimalConstants<Decimal>::DecimalZero;
	  m_bootstrapStats = boot_stats;
	  m_is_calculated  = true;
	  return;
	}

      m_bootstrapStats = boot_stats;

      // (3) Bias-correction z0
      const double prop_less_raw =
	static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
      const double prop_less =
	std::max(1e-10, std::min(1.0 - 1e-10, prop_less_raw));
      const double z0 = NormalDistribution::inverseNormalCdf(prop_less);
      m_z0            = z0;

      // (4) Acceleration a via jackknife.
      //
      // m_sampler.jackknife(m_returns, m_statistic) works for both paths:
      //
      //   Bar-level:   T = Decimal, StatFn maps vector<Decimal> -> Decimal
      //                jackknife returns std::vector<Decimal>  (n pseudo-values)
      //
      //   Trade-level: T = Trade<Decimal>, StatFn maps vector<Trade<Decimal>> -> Decimal
      //                jackknife returns std::vector<Decimal>  (n pseudo-values)
      //
      // In both cases jk_stats is std::vector<Decimal>, and all arithmetic below
      // is unchanged.
      const std::vector<Decimal> jk_stats =
	m_sampler.jackknife(m_returns, m_statistic);
      const size_t n_jk = jk_stats.size();

      Decimal jk_sum = DecimalConstants<Decimal>::DecimalZero;
      for (const auto& th : jk_stats) jk_sum += th;
      const Decimal jk_avg = jk_sum / Decimal(n_jk);

      double num_d = 0.0;  // Σ d³
      double den_d = 0.0;  // Σ d²
      for (const auto& th : jk_stats)
	{
	  const double d  = num::to_double(jk_avg - th);
	  const double d2 = d * d;
	  den_d += d2;
	  num_d += d2 * d;
	}

      Decimal a = DecimalConstants<Decimal>::DecimalZero;
      if (den_d > 1e-100)
	{
	  const double den15 = std::pow(den_d, 1.5);
	  if (den15 > 1e-100)
	    a = Decimal(num_d / (6.0 * den15));
	}
      m_accel = a;

      // (5) Adjusted percentiles → bounds
      const double alpha = computeAlpha(m_confidence_level, m_interval_type);

      double z_alpha_lo, z_alpha_hi;
      switch (m_interval_type)
	{
	case IntervalType::TWO_SIDED:
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;

	case IntervalType::ONE_SIDED_LOWER:
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(
	    computeExtremeQuantile(alpha, true));
	  break;

	case IntervalType::ONE_SIDED_UPPER:
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(
	    computeExtremeQuantile(alpha, false));
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;

	default:
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;
	}

      const double a_d       = num::to_double(a);
      const bool   z0_finite = std::isfinite(z0);

      const double alpha1 =
	(!z0_finite || std::abs(a_d) < 1e-12)
	? NormalDistribution::standardNormalCdf(z0 + z_alpha_lo)
	: NormalDistribution::standardNormalCdf(
	    z0 + (z0 + z_alpha_lo) / (1.0 - a_d * (z0 + z_alpha_lo)));

      const double alpha2 =
	(!z0_finite || std::abs(a_d) < 1e-12)
	? NormalDistribution::standardNormalCdf(z0 + z_alpha_hi)
	: NormalDistribution::standardNormalCdf(
	    z0 + (z0 + z_alpha_hi) / (1.0 - a_d * (z0 + z_alpha_hi)));

      const auto clamp01 = [](double v) noexcept {
        return (v <= 0.0) ? std::nextafter(0.0, 1.0)
	     : (v >= 1.0) ? std::nextafter(1.0, 0.0)
	     : v;
      };

      const double a1 = clamp01(alpha1);
      const double a2 = clamp01(alpha2);

      const int li = unbiasedIndex(std::min(a1, a2), m_num_resamples);
      const int ui = unbiasedIndex(std::max(a1, a2), m_num_resamples);

      std::vector<Decimal> work = boot_stats;
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
     * Implements Efron & Tibshirani (1993), Eq 14.15: index = ⌊p(B+1)⌋ - 1
     * Clamped to [0, B-1] to handle edge cases where p ≈ 0 or p ≈ 1.
     */
    static inline int unbiasedIndex(double p, unsigned int B) noexcept
    {
      int idx = static_cast<int>(
        std::floor(p * (static_cast<double>(B) + 1.0))) - 1;
      if (idx < 0) idx = 0;
      const int maxIdx = static_cast<int>(B) - 1;
      if (idx > maxIdx) idx = maxIdx;
      return idx;
    }
  };

  // ------------------------------ Annualizer -----------------------------------

  /**
   * @brief Calculates an annualization factor based on a given time frame.
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
   * Accepts any BCaBootStrap instantiation regardless of Sampler, Rng, Provider,
   * or SampleType because it only reads getMean(), getLowerBound(), getUpperBound()
   * which always return Decimal.
   */
  template <class Decimal>
  class BCaAnnualizer
  {
  public:
    template <class Sampler, class Rng = randutils::mt19937_rng>
    BCaAnnualizer(const BCaBootStrap<Decimal, Sampler, Rng>& bca_results,
		  double annualization_factor)
    {
      init(bca_results, annualization_factor);
    }

    template <class Sampler, class Rng, class Provider,
	      std::enable_if_t<!std::is_void_v<Provider>, int> = 0>
    BCaAnnualizer(const BCaBootStrap<Decimal, Sampler, Rng, Provider>& bca_results,
		  double annualization_factor)
    {
      init(bca_results, annualization_factor);
    }

    /**
     * @brief Constructor that accepts any BCaBootStrap instantiation, including
     * those with a non-default SampleType (trade-level bootstrap).
     *
     * The BCaAnnualizer only reads Decimal accessors from BCaBootStrap, so it
     * is agnostic to SampleType.
     */
    template <class Sampler, class Rng, class Provider, class SampleType>
    BCaAnnualizer(
      const BCaBootStrap<Decimal, Sampler, Rng, Provider, SampleType>& bca_results,
      double annualization_factor)
    {
      init(bca_results, annualization_factor);
    }

    Decimal getAnnualizedMean()        const { return m_annualized_mean; }
    Decimal getAnnualizedLowerBound()  const { return m_annualized_lower_bound; }
    Decimal getAnnualizedUpperBound()  const { return m_annualized_upper_bound; }

  private:
    template <class BcaT>
    void init(const BcaT& bca_results, double annualization_factor)
    {
      if (!(annualization_factor > 0.0) || !std::isfinite(annualization_factor))
	throw std::invalid_argument("Annualization factor must be positive and finite.");

      const Decimal r_mean  = bca_results.getMean();
      const Decimal r_lower = bca_results.getLowerBound();
      const Decimal r_upper = bca_results.getUpperBound();

      using A = Annualizer<Decimal>;
      const auto trip = A::annualize_triplet(r_lower, r_mean, r_upper,
					     annualization_factor);
      m_annualized_mean        = trip.mean;
      m_annualized_lower_bound = trip.lower;
      m_annualized_upper_bound = trip.upper;
    }

    Decimal m_annualized_mean{};
    Decimal m_annualized_lower_bound{};
    Decimal m_annualized_upper_bound{};
  };

} // namespace mkc_timeseries

#endif // __BCA_BOOTSTRAP_H
