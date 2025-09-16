#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>

// RNG (same family as used in BiasCorrectedBootstrap.h)
#include "randutils.hpp"

// Decimal infrastructure (mirrors headers used by BCaBootStrap)
#include "DecimalConstants.h"
#include "BiasCorrectedBootstrap.h" // for mkc_timeseries::BCaBootStrap and StationaryBlockResampler

namespace mkc_timeseries
{
  /**
   * @brief Utility class for estimating and bounding trading system drawdowns (magnitude-only API).
   *
   * All methods operate on **drawdown magnitudes** (non-negative). This simplifies
   * reporting and confidence intervals (larger = worse drawdown).
   *
   * Implements three pieces described in Masters, adapted to arithmetic returns:
   *  1) maxDrawdown: maximum drawdown magnitude for a sequence of percent changes per trade
   *  2) drawdownFractile: Monte‑Carlo estimate of the drawdown‑magnitude fractile of a return distribution
   *  3) bcaBoundsForDrawdownFractile: BCa bootstrap confidence interval around that fractile
   *
   * @tparam Decimal Decimal/number type (e.g., `number`, `double`, etc.)
   */
  template <class Decimal>
  class BoundedDrawdowns
  {
  public:
    struct Result
    {
      Decimal statistic;   // point estimate (fractile) of drawdown magnitude (>= 0)
      Decimal lowerBound;  // BCa lower bound (>= 0)
      Decimal upperBound;  // BCa upper bound (>= 0)
    };

    /**
     * @brief Compute maximum drawdown *magnitude* from per‑trade percent changes.
     *
     * The input should contain arithmetic returns per trade (e.g., +0.02 for +2%).
     * The equity curve is formed multiplicatively via (1 + r_i). The returned value
     * is non‑negative, e.g., 0.25 means a 25% max drawdown.
     *
     * @param percentChanges vector of per‑trade percent changes
     * @return The maximum drawdown magnitude in [0, +inf). Returns 0 if input empty.
     */
    static Decimal maxDrawdown(const std::vector<Decimal>& percentChanges)
    {
      if (percentChanges.empty())
	return DecimalConstants<Decimal>::DecimalZero;

      Decimal maxDD = DecimalConstants<Decimal>::DecimalZero; // magnitude
      Decimal peak = DecimalConstants<Decimal>::DecimalOne;
      Decimal equity = DecimalConstants<Decimal>::DecimalOne;

      for (const auto& change : percentChanges)
	{
	  equity *= (DecimalConstants<Decimal>::DecimalOne + change);
	  if (equity > peak) {
	    peak = equity;
	  }
	  else
	    {
	      const Decimal dd = (peak - equity) / peak; // >= 0
	      if (dd > maxDD) {
		maxDD = dd;
	      }
	    }
	}
      return maxDD;
    }

    /**
     * @brief Monte‑Carlo estimate of the drawdown‑magnitude fractile for a return distribution.
     *
     * Randomly samples `nTrades` trades with replacement from `returns` to form a synthetic
     * trade sequence, computes its max drawdown magnitude, repeats `nReps` times, and returns the
     * requested fractile (e.g., ddConf = 0.95).
     *
     * @param returns   Available per‑trade percent changes (history), size >= 1
     * @param nTrades   Number of trades per synthetic sample (>= 1)
     * @param nReps     Number of Monte‑Carlo repetitions (>= 1)
     * @param ddConf    Desired fractile in [0, 1]
     * @return          The ddConf fractile of the Monte‑Carlo max drawdown magnitudes (>= 0)
     */
    static Decimal drawdownFractile(const std::vector<Decimal>& returns,
				    int nTrades,
				    int nReps,
				    double ddConf)
    {
      if (returns.empty()) {
	throw std::invalid_argument("drawdownFractile: returns must be non‑empty.");
      }
      if (nTrades <= 0 || nReps <= 0) {
	throw std::invalid_argument("drawdownFractile: nTrades and nReps must be positive.");
      }
      if (!(ddConf >= 0.0 && ddConf <= 1.0)) {
	throw std::invalid_argument("drawdownFractile: ddConf must be in [0,1].");
      }

      std::vector<Decimal> tradeWork(static_cast<size_t>(nTrades));
      std::vector<Decimal> ddSamples; // magnitudes
      ddSamples.reserve(static_cast<size_t>(nReps));

      thread_local static randutils::mt19937_rng rng;

      for (int rep = 0; rep < nReps; ++rep) {
	for (int i = 0; i < nTrades; ++i) {
	  const size_t k = rng.uniform(size_t(0), returns.size() - 1);
	  tradeWork[static_cast<size_t>(i)] = returns[k];
	}
	ddSamples.push_back(maxDrawdown(tradeWork)); // magnitude >= 0
      }

      std::sort(ddSamples.begin(), ddSamples.end()); // ascending (smaller=better)

      const int idx = unbiasedIndex(ddConf, static_cast<unsigned int>(nReps));
      return ddSamples[static_cast<size_t>(idx)];
    }

    /**
     * @brief BCa bootstrap confidence bounds for the drawdown‑magnitude fractile.
     *
     * Uses BCaBootStrap with StationaryBlockResampler to respect time‑series dependence.
     * The statistic evaluated on each resample is `drawdownFractile` using the same
     * (nTrades, nReps, ddConf) parameters.
     *
     * @param returns           Input returns history
     * @param numResamples      Number of bootstrap replicates (>= 100 recommended)
     * @param confidenceLevel   e.g., 0.95
     * @param nTrades           Trades per Monte‑Carlo run inside the statistic
     * @param nReps             Monte‑Carlo repetitions for the statistic
     * @param ddConf            Target fractile for the statistic (e.g., 0.95)
     * @param meanBlockLength   Mean block length for the stationary bootstrap (>= 2)
     * @return                  Result { point estimate, lower, upper } (all magnitudes)
     */
    static Result bcaBoundsForDrawdownFractile(const std::vector<Decimal>& returns,
					       unsigned int numResamples,
					       double confidenceLevel,
					       int nTrades,
					       int nReps,
					       double ddConf,
					       size_t meanBlockLength = 3)
    {
      using Sampler = StationaryBlockResampler<Decimal>;

      // Statistic: compute the magnitude fractile for a given (possibly resampled) return vector
      typename mkc_timeseries::BCaBootStrap<Decimal, Sampler>::StatFn statFn =
	[=](const std::vector<Decimal>& sample) -> Decimal {
	  return drawdownFractile(sample, nTrades, nReps, ddConf);
	};

      mkc_timeseries::BCaBootStrap<Decimal, Sampler> bca(
							 returns,
							 numResamples,
							 confidenceLevel,
							 statFn,
							 Sampler(meanBlockLength)
							 );

      Result r;
      r.statistic  = bca.getStatistic();
      r.lowerBound = bca.getLowerBound();
      r.upperBound = bca.getUpperBound();
      return r;
    }

  private:
    // Percentile index helper matching the convention used in BCaBootStrap
    static int unbiasedIndex(double p, unsigned int B)
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
} // namespace mkc_timeseries
