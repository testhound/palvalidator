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
//
// PARALLELISM NOTE:
//   BCaBootStrap now accepts a 6th template parameter Executor
//   (default: concurrency::SingleThreadExecutor). The bootstrap resampling loop
//   (step 2 of calculateBCaBounds) is parallelized via parallel_for_chunked using
//   the supplied executor. All existing instantiations with 1-5 explicit template
//   parameters are 100% backward compatible.
//
//   For the Provider=void path, per-replicate seeds are precomputed in the calling
//   thread so the thread_local RNG is never touched inside the parallel region,
//   matching the seed-precomputation strategy used in PercentileTBootstrap.
//
//   Recommended executors (from ParallelExecutors.h):
//     - SingleThreadExecutor  : default; deterministic, no concurrency overhead
//     - ThreadPoolExecutor<N> : fixed N worker threads; best for high throughput
//     - StdAsyncExecutor      : portable; good for small numbers of long tasks
//     - BoostRunnerExecutor   : integrates with an existing Boost runner thread pool

#ifndef __BCA_BOOTSTRAP_H
#define __BCA_BOOTSTRAP_H

#include <vector>
#include <numeric>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <limits>
#include <optional>
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
#include "ParallelExecutors.h"
#include "ParallelFor.h"

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
   * @class AccelerationReliability
   * @brief Encapsulates the result of a jackknife influence analysis on the BCa
   * acceleration parameter â.
   *
   * The BCa acceleration parameter is estimated from n jackknife leave-one-out
   * values via:
   *
   *   â = Σd³ / (6 × (Σd²)^1.5)
   *
   * where d_i = jk_avg − jk_i. If a single observation contributes more than
   * kDominanceThreshold of |Σd³|, the estimate is dominated by one data point
   * and is unreliable regardless of â's magnitude. This class reports that
   * condition so the tournament can gate BCa accordingly.
   *
   * There is no default constructor. Every instance is produced by
   * JackknifeInfluence::compute() via the parameterised constructor, which
   * guarantees that all members reflect a real computation.
   */
  class AccelerationReliability
  {
  public:
    /// Fraction of |Σd³| above which a single observation is considered dominant.
    static constexpr double kDominanceThreshold = 0.5;

    /**
     * @brief Constructs a fully-computed reliability result.
     *
     * @param isReliable           True if no single observation exceeds kDominanceThreshold.
     * @param maxInfluenceFraction Largest single |d³| / |Σd³| across all LOO observations.
     * @param maxInfluenceIndex    Index of the observation with maxInfluenceFraction.
     * @param nDominant            Number of observations whose fraction exceeds kDominanceThreshold.
     */
    AccelerationReliability(bool        isReliable,
                            double      maxInfluenceFraction,
                            std::size_t maxInfluenceIndex,
                            std::size_t nDominant)
      : m_isReliable(isReliable),
        m_maxInfluenceFraction(maxInfluenceFraction),
        m_maxInfluenceIndex(maxInfluenceIndex),
        m_nDominant(nDominant)
    {}

    /// True if no single jackknife observation dominates the acceleration estimate.
    bool        isReliable()              const { return m_isReliable; }

    /// Largest fraction of |Σd³| contributed by any single LOO observation.
    double      getMaxInfluenceFraction() const { return m_maxInfluenceFraction; }

    /// Index (into the original sample) of the most influential LOO observation.
    std::size_t getMaxInfluenceIndex()    const { return m_maxInfluenceIndex; }

    /// Number of observations whose influence fraction exceeds kDominanceThreshold.
    std::size_t getNDominant()            const { return m_nDominant; }

  private:
    bool        m_isReliable;
    double      m_maxInfluenceFraction;
    std::size_t m_maxInfluenceIndex;
    std::size_t m_nDominant;
  };

  /**
   * @class JackknifeInfluence
   * @brief Single-responsibility utility for computing jackknife influence
   * diagnostics on BCa acceleration estimates.
   *
   * This class knows nothing about BCa, resampling, or statistics. Its sole
   * responsibility is: given the signed cubic contributions from the jackknife
   * loop (d_i² × d_i for each LOO observation), determine whether any single
   * observation dominates the acceleration numerator Σd³.
   *
   * Called from BCaBootStrap::calculateBCaBounds() after the jackknife loop.
   * The result is stored as std::optional<AccelerationReliability> and exposed
   * via BCaBootStrap::getAccelerationReliability().
   */
  class JackknifeInfluence
  {
  public:
    /**
     * @brief Computes acceleration reliability from signed cubic LOO contributions.
     *
     * @param dCubed  Per-observation signed cubic values (d_i² × d_i) from the
     *                jackknife loop. Their sum is the BCa numerator Σd³.
     * @return        AccelerationReliability describing whether any single
     *                observation dominates the acceleration estimate.
     */
    static AccelerationReliability compute(const std::vector<double>& dCubed)
    {
      double sumD = 0.0;
      for (double v : dCubed) sumD += v;
      const double absNumD = std::fabs(sumD);

      // Numerator negligible: â ≈ 0, acceleration has no effect on the interval.
      // BCa degenerates cleanly to plain BC — reliable by definition.
      if (absNumD <= 1e-100)
        return AccelerationReliability(true, 0.0, 0, 0);

      double      maxFrac   = 0.0;
      std::size_t maxIdx    = 0;
      std::size_t nDominant = 0;

      for (std::size_t i = 0; i < dCubed.size(); ++i)
      {
        const double frac = std::fabs(dCubed[i]) / absNumD;
        if (frac > maxFrac)
        {
          maxFrac = frac;
          maxIdx  = i;
        }
        if (frac > AccelerationReliability::kDominanceThreshold)
          ++nDominant;
      }

      const bool reliable = (maxFrac <= AccelerationReliability::kDominanceThreshold);
      return AccelerationReliability(reliable, maxFrac, maxIdx, nDominant);
    }
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
   * @tparam Executor    Parallel execution policy for the bootstrap resampling loop
   *                     (default: concurrency::SingleThreadExecutor).
   *                     Any executor from ParallelExecutors.h may be supplied:
   *                       - SingleThreadExecutor  : serial, deterministic (default)
   *                       - ThreadPoolExecutor<N> : fixed N-thread pool
   *                       - StdAsyncExecutor      : std::async per chunk
   *                       - BoostRunnerExecutor   : Boost runner thread pool
   *                     All existing code with 1-5 explicit template parameters
   *                     compiles unchanged.
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
            class SampleType = Decimal,
            class Executor   = concurrency::SingleThreadExecutor>
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

    /**
     * @brief Returns the jackknife influence analysis for the acceleration estimate.
     *
     * Indicates whether â was dominated by a single leave-one-out observation.
     * If AccelerationReliability::isReliable() returns false, the BCa interval
     * should be treated with caution regardless of â's magnitude — the estimate
     * is being driven by one data point rather than reflecting a distributional
     * property.
     *
     * The tournament (AutoBootstrapSelector) uses this to hard-gate BCa when
     * acceleration is unreliable, rather than relying solely on the magnitude
     * of â or n.
     */
    AccelerationReliability getAccelerationReliability() const
    {
      ensureCalculated();
      return m_accelReliability.value(); // safe: ensureCalculated() guarantees population
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
     * @brief Configures the tail ratio used to place the finite display bound
     * on the irrelevant side of a one-sided BCa interval.
     *
     * The irrelevant-side quantile is computed as alpha / ratio.  A larger
     * value pushes that bound deeper into the tail (more extreme).  This
     * setting has no effect on two-sided intervals, and no effect on the
     * meaningful bound of a one-sided interval.
     *
     * @param ratio  Must be >= 2.0.  Default is 1000.0.
     * @throws std::invalid_argument if ratio < 2.0.
     */
    void setOneSidedTailRatio(double ratio)
    {
      if (ratio < 2.0)
        throw std::invalid_argument(
          "BCaBootStrap: one-sided tail ratio must be >= 2.0.");
      m_one_sided_tail_ratio = ratio;
    }

    double getOneSidedTailRatio() const { return m_one_sided_tail_ratio; }

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

    /**
     * @brief Returns a pragmatic finite display bound for one-sided BCa intervals.
     *
     * CONVENTION — NOT textbook BCa:
     * A textbook one-sided BCa interval is half-open: one endpoint is the
     * BCa-adjusted quantile for the relevant tail, and the other side is
     * conceptually ±∞ (or the sample extremum).  This class instead returns a
     * *finite* bound on the "irrelevant" side for display and reporting
     * purposes.
     *
     * For ONE_SIDED_LOWER: only getLowerBound() is statistically meaningful.
     * getUpperBound() is a finite placeholder for +∞, computed by pushing the
     * irrelevant tail to alpha / tail_ratio (default: alpha / 1000).
     * Callers should ignore getUpperBound() in this mode, and vice-versa for
     * ONE_SIDED_UPPER.
     *
     * This method is static so it can be called without an instance (e.g. in
     * unit tests).  Internal BCa computation passes m_one_sided_tail_ratio
     * explicitly so the configurable value is honoured at runtime.
     *
     * @param alpha       The tail probability for the meaningful side.
     * @param is_upper    true  → compute the extreme upper display bound
     *                    false → compute the extreme lower display bound
     * @param tail_ratio  Divisor applied to alpha (default 1000).  A larger
     *                    value pushes the display bound deeper into the tail.
     * @return A quantile probability for the extreme/irrelevant side.
     */
    static double computeExtremeQuantile(double alpha,
                                         bool   is_upper,
                                         double tail_ratio = 1000.0) noexcept
    {
      const double extreme_tail_prob = alpha / tail_ratio;
      return is_upper ? (1.0 - extreme_tail_prob) : extreme_tail_prob;
    }

  protected:
    // -------------------------------------------------------------------------
    // Data & config
    //
    // m_returns is now std::vector<SampleType>. When SampleType = Decimal this
    // is identical to the original. When SampleType = Trade<Decimal> it holds
    // the trade population from which bootstrap resamples are drawn.
    //
    // NOTE: stored by value (not reference) to prevent dangling-reference UB
    // under lazy evaluation. ensureCalculated() may defer calculateBCaBounds()
    // until the first accessor call, by which time a caller-owned vector could
    // have gone out of scope. Owning the copy eliminates that hazard entirely.
    // -------------------------------------------------------------------------
    const std::vector<SampleType> m_returns;
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
    std::optional<AccelerationReliability> m_accelReliability; // populated by calculateBCaBounds()
    std::vector<Decimal> m_bootstrapStats;   // bootstrap θ*'s (unsorted)
    IntervalType         m_interval_type;

    // Controls how far into the extreme tail the finite "display bound" is
    // placed on the irrelevant side of a one-sided BCa interval.
    // See computeExtremeQuantile() for full convention documentation.
    // Default: 1000 (alpha / 1000).  Exposed via setOneSidedTailRatio().
    double               m_one_sided_tail_ratio{1000.0};

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
     * Changes from the original implementation:
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
     *   3. count_equal is tracked alongside count_less and the mid-rank tie
     *      correction is applied when computing z0, making BCa correct for
     *      discrete statistics (e.g. consecutive losers, win-rate).
     *
     *   4. The |a| < 1e-12 short-circuit in the BCa percentile formula has
     *      been removed. The full formula is always used (when z0 is finite),
     *      which naturally reduces to the correct BC result when a ≈ 0.
     *
     *   5. m_theta_hat is no longer overwritten in the degenerate all-equal
     *      path; it retains the value computed from the original sample.
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
      //
      // We track both count_less (θ*_b < θ̂) and count_equal (θ*_b == θ̂) so
      // that step (3) can apply the mid-rank tie correction for z0.  For
      // continuous statistics (geometric mean, profit factor) count_equal will
      // essentially always be zero and the correction has no effect.  For
      // discrete statistics (consecutive losers, win-rate, integer P&L) many
      // replicates may equal θ̂ exactly, and the strict-< definition
      // systematically underestimates z0; the mid-rank correction
      //   prop_less_adj = (count_less + 0.5 * count_equal) / B
      // is the standard remedy (Efron & Tibshirani 1993, §14.3 footnote).
      // (2) Bootstrap replicates — parallelized via Executor policy.
      //
      // Design mirrors PercentileTBootstrap::run_impl():
      //
      //   boot_stats is pre-sized rather than push_back'd, so each parallel task
      //   writes to its own disjoint index with no synchronization on the vector.
      //
      //   count_less / count_equal are std::atomic so concurrent increments are
      //   safe regardless of chunk granularity or executor choice.
      //
      //   Provider=void path: each parallel task uses a thread_local Rng instance,
      //   default-constructed once per worker thread. This avoids constructing Rng
      //   from std::seed_seq (as PercentileTBootstrap does), which is not universally
      //   supported — neither randutils::mt19937_rng nor user-defined generators like
      //   FixedRng have a std::seed_seq constructor. thread_local gives each worker
      //   thread its own independent RNG stream with no sharing or data races.
      //
      //   For SingleThreadExecutor: chunkSizeHint = m_num_resamples so the entire
      //   loop runs as one inline task on the calling thread. The single thread_local
      //   Rng is created once and advances through the full sequence, exactly
      //   equivalent to the original serial thread_local Rng path.
      //
      //   For randutils::mt19937_rng: default construction is auto-seeded with
      //   entropy, giving each worker thread a statistically independent stream.
      //
      //   Provider!=void path: m_provider.make_engine(b) is called inside each
      //   task; Provider thread-safety is the provider's responsibility (const
      //   method, matching the PercentileTBootstrap convention).
      //
      //   Sampler is copied per-task.  IIDResampler is stateless so the copy is
      //   trivial.  StationaryBlockResampler carries mutable m_geo, but each
      //   task copy owns its own distribution object driven by its own task_rng,
      //   so there is no shared mutable state after the copy.
      //
      //   m_statistic (std::function) is called concurrently; operator() is
      //   const, so this is safe as long as the wrapped callable is re-entrant —
      //   a requirement any pure bootstrap statistic satisfies by definition.
      //
      //   chunkSizeHint: parallel_for_chunked's auto-calculated chunk size is
      //   clamped to a minimum of 512 to guard against low-cost workloads. For
      //   bootstrap iterations the per-iteration cost is substantial (n RNG draws
      //   + statistic computation), so the 512 floor frequently over-chunks the
      //   work relative to the available hardware threads. For example, with
      //   B=25,000 and 24 hw threads the auto-chunk of 512 yields only 49 chunks
      //   (~2 rounds), giving ~68% parallel efficiency. Overriding with a hint
      //   targeting ~6 chunks per hw thread (the same ratio used by the
      //   auto-calculation before clamping) recovers near-linear scaling.
      //
      //   The hint adapts automatically to the executing machine:
      //     24-thread machine: hint = ceil(25000 / 144) = 174 → 144 chunks, ~97% eff.
      //     32-thread machine: hint = ceil(25000 / 192) = 131 → 191 chunks, ~99% eff.
      //
      //   A hint of 0 would re-engage the auto-calculation (including its 512
      //   clamp), so we floor at 1 to ensure the hint path is always taken.
      std::vector<Decimal>      boot_stats(m_num_resamples);
      std::atomic<unsigned int> count_less{0};
      std::atomic<unsigned int> count_equal{0};

      Executor exec{};

      // Compute chunk size hint for parallel_for_chunked.
      //
      // The hint is policy-aware: the correct value differs fundamentally
      // between single-threaded and parallel executors.
      //
      // SingleThreadExecutor:
      //   All tasks run inline on the calling thread regardless of chunk count.
      //   Submitting many small chunks therefore adds pure overhead: each chunk
      //   allocates a std::promise, invokes the lambda, and returns a future —
      //   all serially.  One chunk of size m_num_resamples eliminates that cost
      //   entirely while preserving identical numeric output.
      //
      // Parallel executors (ThreadPoolExecutor, StdAsyncExecutor, etc.):
      //   parallel_for_chunked's auto-calculated chunk size is clamped to a
      //   minimum of 512 to guard against low-cost workloads. Bootstrap
      //   iterations are expensive (n RNG draws + statistic computation), so
      //   the 512 floor over-chunks the work on high core-count machines.
      //   Example: B=25,000 on a 24-thread machine → 49 chunks (~2 rounds,
      //   ~68% efficiency). Overriding with a hint targeting ~6 chunks per
      //   hardware thread recovers near-linear scaling:
      //     24-thread: hint = ceil(25000/144) = 174 → 144 chunks, ~97% eff.
      //     32-thread: hint = ceil(25000/192) = 131 → 191 chunks, ~99% eff.
      //   Falls back to 1 on single-core or if hardware_concurrency() = 0.
      uint32_t chunkSizeHint;
      if constexpr (std::is_same_v<Executor, concurrency::SingleThreadExecutor>)
	{
	  // One chunk: the entire loop runs as a single inline task.
	  chunkSizeHint = static_cast<uint32_t>(m_num_resamples);
	}
      else
	{
	  // Parallel executor: target ~6 chunks per hardware thread.
	  constexpr unsigned int chunksPerThread = 6u;
	  const unsigned int hwThreads =
	    std::max(1u, static_cast<unsigned int>(std::thread::hardware_concurrency()));
	  chunkSizeHint = std::max(
	    1u,
	    static_cast<uint32_t>(
	      (static_cast<uint32_t>(m_num_resamples) + (hwThreads * chunksPerThread) - 1u)
	      / (hwThreads * chunksPerThread)));
	}

      if constexpr (std::is_void_v<Provider>)
	{
	  concurrency::parallel_for_chunked(
	    static_cast<uint32_t>(m_num_resamples), exec,
	    [&](uint32_t b)
	    {
	      // One Rng instance per worker thread, default-constructed on first use.
	      // For randutils::mt19937_rng: auto-seeded with entropy — each thread
	      // gets a statistically independent stream at no cost to the caller.
	      // For SingleThreadExecutor: this is the calling thread's own instance,
	      // created once and advanced across all B iterations — identical
	      // behaviour to the original serial thread_local Rng path.
	      thread_local Rng task_rng;

	      Sampler sampler_local = m_sampler;
	      const std::vector<SampleType> resample = sampler_local(m_returns, n, task_rng);
	      const Decimal stat_b = m_statistic(resample);

	      boot_stats[b] = stat_b;
	      if      (stat_b <  m_theta_hat) count_less.fetch_add(1, std::memory_order_relaxed);
	      else if (stat_b == m_theta_hat) count_equal.fetch_add(1, std::memory_order_relaxed);
	    },
	    chunkSizeHint);
	}
      else
	{
	  // Provider path: deterministic per-replicate engines — naturally parallel.
	  concurrency::parallel_for_chunked(
	    static_cast<uint32_t>(m_num_resamples), exec,
	    [&](uint32_t b)
	    {
	      Rng rng_b = m_provider.make_engine(static_cast<unsigned int>(b));

	      Sampler sampler_local = m_sampler;
	      const std::vector<SampleType> resample = sampler_local(m_returns, n, rng_b);
	      const Decimal stat_b = m_statistic(resample);

	      boot_stats[b] = stat_b;
	      if      (stat_b <  m_theta_hat) count_less.fetch_add(1, std::memory_order_relaxed);
	      else if (stat_b == m_theta_hat) count_equal.fetch_add(1, std::memory_order_relaxed);
	    },
	    chunkSizeHint);
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
	  // m_theta_hat intentionally NOT overwritten: it was set correctly in
	  // step (1) as m_statistic(m_returns) on the original sample. Replacing
	  // it with boot_stats[0] (a replicate statistic) violates the invariant
	  // that getMean() returns the observed statistic, not a bootstrap value.
	  // In a truly degenerate case the two are numerically equal, but the
	  // semantic distinction matters for diagnostics and subclass overrides.
	  m_z0             = 0.0;
	  m_accel          = DecimalConstants<Decimal>::DecimalZero;
	  // Degenerate bootstrap: â = 0 by construction. Acceleration has no
	  // effect on the interval, so reliability is trivially true.
	  m_accelReliability = AccelerationReliability(true, 0.0, 0, 0);
	  m_bootstrapStats = boot_stats;
	  m_is_calculated  = true;
	  return;
	}

      m_bootstrapStats = boot_stats;

      // (3) Bias-correction z0
      // Apply mid-rank tie correction: credit each tied replicate as half a
      // "less-than".  For continuous statistics count_equal ≈ 0 and this
      // reduces to the classic formula.  For discrete statistics (e.g.
      // consecutive losers) this prevents systematic downward bias in z0.
      const double prop_less_raw =
	(static_cast<double>(count_less.load()) + 0.5 * static_cast<double>(count_equal.load()))
	/ static_cast<double>(m_num_resamples);
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

      // Per-observation signed cubic contributions for influence analysis.
      // Collected alongside the existing d² / d³ accumulation at zero extra
      // passes through jk_stats. Passed to JackknifeInfluence::compute() below.
      std::vector<double> dCubed(n_jk);

      for (std::size_t i = 0; i < n_jk; ++i)
	{
	  const double d  = num::to_double(jk_avg - jk_stats[i]);
	  const double d2 = d * d;
	  den_d      += d2;
	  num_d      += d2 * d;
	  dCubed[i]   = d2 * d;  // signed cubic contribution of observation i
	}

      Decimal a = DecimalConstants<Decimal>::DecimalZero;
      if (den_d > 1e-100)
	{
	  const double den15 = std::pow(den_d, 1.5);
	  if (den15 > 1e-100)
	    a = Decimal(num_d / (6.0 * den15));
	}
      m_accel = a;

      // Influence analysis: did any single LOO observation dominate Σd³?
      // JackknifeInfluence has sole responsibility for this computation.
      // The result is stored as optional so the accessor can assert it is
      // populated before returning, catching any future code path that
      // bypasses this line.
      m_accelReliability = JackknifeInfluence::compute(dCubed);

      // (5) Adjusted percentiles → bounds
      //
      // BCa formula (Efron 1987, eq. 6.8):
      //   α₁ = Φ( z₀ + (z₀ + zα) / (1 − a·(z₀ + zα)) )
      //
      // When a ≈ 0 the denominator ≈ 1 and the formula naturally reduces to
      // the correct BC (bias-corrected) result Φ(2·z₀ + zα). We therefore do
      // NOT short-circuit for |a| < ε: doing so would skip the second z₀ term
      // and compute Φ(z₀ + zα) instead, which is wrong.
      //
      // The only fallback retained is for non-finite z₀ (±inf / NaN), which
      // indicates a completely degenerate bootstrap distribution. In that case
      // the BCa method has broken down regardless, and the unadjusted quantile
      // is used as a safe fallback.
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
	    computeExtremeQuantile(alpha, true, m_one_sided_tail_ratio));
	  break;

	case IntervalType::ONE_SIDED_UPPER:
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(
	    computeExtremeQuantile(alpha, false, m_one_sided_tail_ratio));
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;

	default:
	  z_alpha_lo = NormalDistribution::inverseNormalCdf(alpha);
	  z_alpha_hi = NormalDistribution::inverseNormalCdf(1.0 - alpha);
	  break;
	}

      const double a_d       = num::to_double(a);
      const bool   z0_finite = std::isfinite(z0);

      // BCa adjusted percentiles (Efron 1987, eq. 6.8).
      // Fallback when z0 is non-finite: BCa has completely broken down, so
      // fall back to the plain percentile bootstrap, which is simply Φ(zα).
      // Using Φ(z0 + zα) in this branch would propagate ±∞ into standardNormalCdf,
      // returning 0 or 1 and slamming the bound to the array extreme — the
      // opposite of a safe fallback.
      // In practice this branch is dead code: the [1e-10, 1-1e-10] clamping
      // of prop_less in step (3) ensures inverseNormalCdf always returns a
      // finite z0 (|z0| <= ~6.36). It is fixed here for mathematical correctness.
      const double alpha1 =
	!z0_finite
	? NormalDistribution::standardNormalCdf(z_alpha_lo)
	: NormalDistribution::standardNormalCdf(
	    z0 + (z0 + z_alpha_lo) / (1.0 - a_d * (z0 + z_alpha_lo)));

      const double alpha2 =
	!z0_finite
	? NormalDistribution::standardNormalCdf(z_alpha_hi)
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
     */
    template <class Sampler, class Rng, class Provider, class SampleType>
    BCaAnnualizer(
      const BCaBootStrap<Decimal, Sampler, Rng, Provider, SampleType>& bca_results,
      double annualization_factor)
    {
      init(bca_results, annualization_factor);
    }

    /**
     * @brief Constructor that accepts any BCaBootStrap instantiation with a
     * non-default Executor (6th template parameter).
     *
     * This overload handles the case where BCaBootStrap is instantiated with an
     * explicit Executor policy (e.g., ThreadPoolExecutor<N>). The three existing
     * overloads above only match instantiations where Executor = SingleThreadExecutor
     * (the default), so this 4th overload extends coverage without disturbing any
     * existing call site.
     */
    template <class Sampler, class Rng, class Provider, class SampleType, class Executor>
    BCaAnnualizer(
      const BCaBootStrap<Decimal, Sampler, Rng, Provider, SampleType, Executor>& bca_results,
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