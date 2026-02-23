#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <iostream>

// RNG (same family as used in BiasCorrectedBootstrap.h)
#include "randutils.hpp"

// Decimal infrastructure (mirrors headers used by BCaBootStrap)
#include "DecimalConstants.h"
#include "BootstrapTypes.h"
#include "BiasCorrectedBootstrap.h" // for mkc_timeseries::BCaBootStrap and StationaryBlockResampler
#include "TradeResampling.h"

// Stationary (block) resampling for path simulation inside drawdownFractile
#include "StationaryMaskResamplers.h" // palvalidator::resampling::StationaryMaskValueResampler

// Executors & parallel helpers
#include "ParallelExecutors.h"   // SingleThreadExecutor, ThreadPoolExecutor, etc.
#include "ParallelFor.h"         // concurrency::parallel_for

namespace mkc_timeseries
{
  /**
   * @brief Utility class for estimating and bounding trading system drawdowns (magnitude-only API).
   *
   * All methods operate on **drawdown magnitudes** (non-negative). This simplifies
   * reporting and confidence intervals (larger = worse drawdown).
   *
   * The class is templated on an `Executor` policy (default: SingleThreadExecutor).
   * Pass a faster executor (e.g., ThreadPoolExecutor) to parallelize Monte‑Carlo work.
   *
   * Implements three pieces described in Masters, adapted to arithmetic returns:
   *  1) maxDrawdown: maximum drawdown magnitude for a sequence of percent changes per trade
   *  2) drawdownFractile: Monte‑Carlo estimate of the drawdown‑magnitude fractile of a return distribution
   *  3) bcaBoundsForDrawdownFractile: BCa bootstrap confidence interval around that fractile
   *
   * @tparam Decimal  Numeric type (e.g., mkc_timeseries::number)
   * @tparam Executor Execution policy (defaults to concurrency::SingleThreadExecutor)
   */
  template <class Decimal, typename Executor = concurrency::SingleThreadExecutor>
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
     * @brief Compute maximum drawdown *magnitude* from a sequence of Trade pointers.
     */
    static Decimal maxDrawdown(const std::vector<const mkc_timeseries::Trade<Decimal>*>& trades)
    {
      if (trades.empty())
        return DecimalConstants<Decimal>::DecimalZero;

      Decimal maxDD = DecimalConstants<Decimal>::DecimalZero; 
      Decimal peak = DecimalConstants<Decimal>::DecimalOne;
      Decimal equity = DecimalConstants<Decimal>::DecimalOne;

      for (const auto* tradePtr : trades) // Now iterating over pointers
      {
        for (const auto& change : tradePtr->getDailyReturns())
        {
          equity *= (DecimalConstants<Decimal>::DecimalOne + change);
          if (equity > peak) {
            peak = equity;
          }
          else
          {
            const Decimal dd = (peak - equity) / peak;
            if (dd > maxDD) {
              maxDD = dd;
            }
          }
        }
      }
      return maxDD;
    }

    /**
     * @brief Compute maximum drawdown *magnitude* from a sequence of Trades.
     * Evaluates the true intra-trade peak-to-trough drawdown by expanding
     * the underlying bar returns of each trade.
     */
    static Decimal maxDrawdown(const std::vector<mkc_timeseries::Trade<Decimal>>& trades)
    {
      if (trades.empty())
        return DecimalConstants<Decimal>::DecimalZero;

      Decimal maxDD = DecimalConstants<Decimal>::DecimalZero; 
      Decimal peak = DecimalConstants<Decimal>::DecimalOne;
      Decimal equity = DecimalConstants<Decimal>::DecimalOne;

      for (const auto& trade : trades)
      {
        // Iterate through the actual bar returns of the trade to catch intra-trade dips
        for (const auto& change : trade.getDailyReturns())
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
      Executor exec{}; // default execution policy (single-thread by default)
      return drawdownFractile(returns, nTrades, nReps, ddConf, exec);
    }

    /**
     * @brief Monte‑Carlo fractile using a provided executor (enables parallelism).
     *
     * Randomly samples `nTrades` trades with replacement from `returns` to form a synthetic
     * trade sequence, computes its max drawdown magnitude, repeats `nReps` times, and returns the
     * requested fractile (e.g., ddConf = 0.95). Uses the provided executor for parallelization.
     *
     * @param returns   Available per‑trade percent changes (history), size >= 1
     * @param nTrades   Number of trades per synthetic sample (>= 1)
     * @param nReps     Number of Monte‑Carlo repetitions (>= 1)
     * @param ddConf    Desired fractile in [0, 1]
     * @param exec      Executor for parallel processing
     * @return          The ddConf fractile of the Monte‑Carlo max drawdown magnitudes (>= 0)
     */
    static Decimal drawdownFractile(const std::vector<Decimal>& returns,
                                    int nTrades,
                                    int nReps,
                                    double ddConf,
                                    Executor& exec)
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

      std::vector<Decimal> ddSamples(static_cast<size_t>(nReps)); // one slot per rep
      const size_t m = returns.size();

      /**
       * @brief Parallel Monte Carlo simulation using a lambda function.
       *
       * @details
       * This parallel_for call distributes `nReps` Monte Carlo iterations across multiple
       * threads for high-performance simulation. The lambda function `[&](uint32_t rep)`
       * is executed concurrently by different threads, with each thread processing a
       * subset of the total iterations.
       *
       * ### Lambda Capture and Thread Safety:
       * - **Capture `[&]`**: Captures all enclosing variables by reference, including:
       *   - `returns`: Input historical data (read-only, thread-safe)
       *   - `nTrades`, `m`: Parameters (read-only, thread-safe)
       *   - `ddSamples`: Output vector (write-only, thread-safe due to unique indices)
       *
       * ### Thread-Local Storage Pattern:
       * - **`thread_local tradeWork`**: Each thread maintains its own work buffer,
       *   eliminating memory allocation overhead and avoiding contention between threads.
       * - **`thread_local rng`**: Each thread has an independent random number generator,
       *   ensuring thread safety and preventing correlation between parallel streams.
       *
       * ### Per-Thread Execution Flow:
       * 1. **Resize Work Buffer**: Prepare thread-local buffer for `nTrades` samples
       * 2. **Random Sampling**: Fill buffer with randomly selected returns from input data
       * 3. **Drawdown Calculation**: Compute maximum drawdown for the synthetic trade sequence
       * 4. **Result Storage**: Store result in `ddSamples[rep]` (thread-safe: unique index per thread)
       *
       * ### Performance Considerations:
       * - Work is automatically distributed across `hardware_concurrency()` threads
       * - Thread-local storage eliminates memory allocation bottlenecks
       * - Independent RNG streams prevent synchronization overhead
       * - Each thread writes to a unique index, avoiding race conditions
       *
       * @param rep The iteration index (0 to nReps-1), unique per thread execution
       */
      concurrency::parallel_for(static_cast<uint32_t>(nReps), exec, [&](uint32_t rep) {
        // Per‑thread reusable buffer and RNG (see documentation above)
        thread_local std::vector<Decimal> tradeWork;
        thread_local randutils::mt19937_rng rng;

        tradeWork.resize(static_cast<size_t>(nTrades));
        for (int i = 0; i < nTrades; ++i) {
          const size_t k = rng.uniform(size_t(0), m - 1);
          tradeWork[static_cast<size_t>(i)] = returns[k];
        }
        ddSamples[rep] = maxDrawdown(tradeWork);
      });

      // O(n) selection for the desired fractile
      const int idx = unbiasedIndex(ddConf, static_cast<unsigned int>(nReps));
      std::nth_element(ddSamples.begin(), ddSamples.begin() + idx, ddSamples.end());
      return ddSamples[static_cast<size_t>(idx)];
    }

    /**
     * @brief Monte-Carlo estimate of the drawdown-magnitude fractile using IID trade sampling.
     *
     * Pre-flattens each trade's daily returns into a cache before the parallel loop,
     * eliminating repeated pointer dereferences and heap traversal inside the hot path.
     * The cache is read-only and shared across all threads safely.
     */
    static Decimal drawdownFractile(const std::vector<mkc_timeseries::Trade<Decimal>>& trades,
				    int nTrades,
				    int nReps,
				    double ddConf,
				    Executor& exec)
    {
      if (trades.empty()) {
	throw std::invalid_argument("drawdownFractile: trades must be non-empty.");
      }
      if (nTrades <= 0 || nReps <= 0) {
	throw std::invalid_argument("drawdownFractile: nTrades and nReps must be positive.");
      }
      if (!(ddConf >= 0.0 && ddConf <= 1.0)) {
	throw std::invalid_argument("drawdownFractile: ddConf must be in [0,1].");
      }

      // --- Pre-flatten daily returns into a read-only cache ---
      // Built once here, shared across all threads as a read-only structure.
      // Eliminates getDailyReturns() pointer chasing inside the hot parallel loop.
      const size_t m = trades.size();
      std::vector<std::vector<Decimal>> returnCache(m);
      for (std::size_t i = 0; i < m; ++i)
	returnCache[i] = trades[i].getDailyReturns();

      std::vector<Decimal> ddSamples(static_cast<size_t>(nReps));

      concurrency::parallel_for(static_cast<uint32_t>(nReps), exec, [&](uint32_t rep)
      {
	// Thread-local flat buffer accumulates all daily returns for the sampled
	// trade sequence. Reused across reps to avoid per-rep heap allocation.
	thread_local std::vector<Decimal> flatReturns;
	thread_local randutils::mt19937_rng rng;

	flatReturns.clear();
	for (int i = 0; i < nTrades; ++i) {
	  const size_t k = rng.uniform(size_t(0), m - 1);
	  const auto& daily = returnCache[k]; // cache hit: contiguous read-only memory
	  flatReturns.insert(flatReturns.end(), daily.begin(), daily.end());
	}

	ddSamples[rep] = maxDrawdown(flatReturns);
      });

      const int idx = unbiasedIndex(ddConf, static_cast<unsigned int>(nReps));
      std::nth_element(ddSamples.begin(), ddSamples.begin() + idx, ddSamples.end());
      return ddSamples[static_cast<size_t>(idx)];
    }

    /**
     * @brief Monte-Carlo estimate of the drawdown-magnitude fractile using stationary (block) resampling.
     *
     * This variant generates each synthetic trade path via the Politis–Romano stationary bootstrap
     * (aka "stationary block bootstrap") to preserve short-range dependence / volatility clustering.
     *
     * - When your input is daily mark-to-market returns (or any return series with clustering),
     *   IID sampling can understate tail drawdowns.
     * - Stationary resampling stitches together random blocks whose lengths are geometric with
     *   mean `meanBlockLength`.
     *
     * @param returns           Available per-trade percent changes (history), size >= 1
     * @param nTrades           Number of trades per synthetic sample (>= 1)
     * @param nReps             Number of Monte-Carlo repetitions (>= 1)
     * @param ddConf            Desired fractile in [0, 1]
     * @param meanBlockLength   Mean block length L for the stationary bootstrap (>= 1)
     * @return                  The ddConf fractile of max drawdown magnitudes (>= 0)
     */
    static Decimal drawdownFractileStationary(const std::vector<Decimal>& returns,
                                             int nTrades,
                                             int nReps,
                                             double ddConf,
                                             std::size_t meanBlockLength)
    {
      Executor exec{};
      return drawdownFractileStationary(returns, nTrades, nReps, ddConf, meanBlockLength, exec);
    }

    /**
     * @brief Stationary-resampled Monte-Carlo fractile using a provided executor (enables parallelism).
     *
     * See drawdownFractileStationary(…, meanBlockLength) for details.
     */
    static Decimal drawdownFractileStationary(const std::vector<Decimal>& returns,
                                             int nTrades,
                                             int nReps,
                                             double ddConf,
                                             std::size_t meanBlockLength,
                                             Executor& exec)
    {
      if (returns.empty()) {
        throw std::invalid_argument("drawdownFractileStationary: returns must be non-empty.");
      }
      if (nTrades <= 0 || nReps <= 0) {
        throw std::invalid_argument("drawdownFractileStationary: nTrades and nReps must be positive.");
      }
      if (!(ddConf >= 0.0 && ddConf <= 1.0)) {
        throw std::invalid_argument("drawdownFractileStationary: ddConf must be in [0,1].");
      }
      if (meanBlockLength < 1) {
        throw std::invalid_argument("drawdownFractileStationary: meanBlockLength must be >= 1.");
      }

      // If we cannot meaningfully do stationary resampling (too-short series or path),
      // fall back to IID sampling (exactly matches drawdownFractile behaviour).
      if (returns.size() < 2 || nTrades < 2) {
        return drawdownFractile(returns, nTrades, nReps, ddConf, exec);
      }

      std::vector<Decimal> ddSamples(static_cast<size_t>(nReps));
      const palvalidator::resampling::StationaryMaskValueResampler<Decimal> resampler(meanBlockLength);

      concurrency::parallel_for(static_cast<uint32_t>(nReps), exec, [&](uint32_t rep) {
        thread_local std::vector<Decimal> tradeWork;
        thread_local randutils::mt19937_rng rng;

        // Stationary bootstrap generates a length-nTrades path with dependence preserved.
        resampler(returns, tradeWork, static_cast<std::size_t>(nTrades), rng);
        ddSamples[rep] = maxDrawdown(tradeWork);
      });

      const int idx = unbiasedIndex(ddConf, static_cast<unsigned int>(nReps));
      std::nth_element(ddSamples.begin(), ddSamples.begin() + idx, ddSamples.end());
      return ddSamples[static_cast<size_t>(idx)];
    }

    /**
     * @brief BCa bootstrap confidence bounds for the drawdown‑magnitude fractile.
     *
     * Uses BCaBootStrap with StationaryBlockResampler to respect time‑series dependence.
     * The statistic evaluated on each resample is `drawdownFractile` using the same
     * (nTrades, nReps, ddConf) parameters.
     *
     **Interval Type Selection:**
     * - TWO_SIDED (default): Full range [lower, upper], most conservative upper bound
     * - ONE_SIDED_UPPER (recommended): "95% confident max DD won't exceed X" - natural for risk
     * - ONE_SIDED_LOWER (rare): Bounds best-case scenario
     *
     * **Example Usage:**
     * ```cpp
     * // For risk management (recommended):
     * auto result = BoundedDrawdowns<Decimal>::bcaBoundsForDrawdownFractile(
     *     returns, 1000, 0.95, 252, 5000, 0.95, 3, exec, IntervalType::ONE_SIDED_UPPER);
     * // result.upperBound = "95% confident the 95th percentile max DD won't exceed this"
     *
     * // For maximum conservatism:
     * auto result = BoundedDrawdowns<Decimal>::bcaBoundsForDrawdownFractile(
     *     returns, 1000, 0.95, 252, 5000, 0.95, 3, exec, IntervalType::TWO_SIDED);
     * // result.upperBound = more conservative (97.5th percentile instead of 95th)
     * ```
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
                                               size_t meanBlockLength = 3,
					       IntervalType intervalType = IntervalType::TWO_SIDED)
    {
      Executor exec{};
      return bcaBoundsForDrawdownFractile(returns, numResamples, confidenceLevel,
                                          nTrades, nReps, ddConf, meanBlockLength, exec, intervalType);
    }

    /**
     * @brief BCa CI for the drawdown‑magnitude fractile using a provided executor.
     *
     * Uses BCaBootStrap with StationaryBlockResampler to respect time‑series dependence.
     * The statistic evaluated on each resample is `drawdownFractile` using the same
     * (nTrades, nReps, ddConf) parameters with the provided executor for parallelization.
     *
     * @param returns           Input returns history
     * @param numResamples      Number of bootstrap replicates (>= 100 recommended)
     * @param confidenceLevel   e.g., 0.95
     * @param nTrades           Trades per Monte‑Carlo run inside the statistic
     * @param nReps             Monte‑Carlo repetitions for the statistic
     * @param ddConf            Target fractile for the statistic (e.g., 0.95)
     * @param meanBlockLength   Mean block length for the stationary bootstrap (>= 2)
     * @param exec              Executor for parallel processing
     * @param intervalType      Type of confidence interval (default: TWO_SIDED)
     * @return                  Result { point estimate, lower, upper } (all magnitudes)
     */
    static Result bcaBoundsForDrawdownFractile(const std::vector<Decimal>& returns,
                                               unsigned int numResamples,
                                               double confidenceLevel,
                                               int nTrades,
                                               int nReps,
                                               double ddConf,
                                               size_t meanBlockLength,
                                               Executor& exec,
					       IntervalType intervalType = IntervalType::TWO_SIDED)
    {
      using Sampler = StationaryBlockResampler<Decimal>;

      // Statistic computed with (possibly parallel) Monte‑Carlo
      typename mkc_timeseries::BCaBootStrap<Decimal, Sampler>::StatFn statFn =
        [=, &exec](const std::vector<Decimal>& sample) -> Decimal {
          // IMPORTANT: Use stationary (block) path generation *inside* the statistic,
          // otherwise the dependence preserved by the outer bootstrap can be destroyed
          // when simulating max drawdowns.
          return drawdownFractileStationary(sample, nTrades, nReps, ddConf, meanBlockLength, exec);
        };

      mkc_timeseries::BCaBootStrap<Decimal, Sampler> bca(returns,
							 numResamples,
							 confidenceLevel,
							 statFn,
							 Sampler(meanBlockLength),
							 intervalType);

      Result r;
      r.statistic  = bca.getStatistic();
      r.lowerBound = bca.getLowerBound();
      r.upperBound = bca.getUpperBound();
      return r;
    }

    /**
     * @brief BCa bootstrap confidence bounds for the drawdown-magnitude fractile using Trade objects.
     * Uses IID sampling, as individual trades are assumed to be independent events.
     */
    static Result bcaBoundsForDrawdownFractile(const std::vector<mkc_timeseries::Trade<Decimal>>& trades,
                                               unsigned int numResamples,
                                               double confidenceLevel,
                                               int nTrades,
                                               int nReps,
                                               double ddConf,
                                               Executor& exec,
                                               IntervalType intervalType = IntervalType::TWO_SIDED)
    {
      using TradeType = mkc_timeseries::Trade<Decimal>;
      using Sampler = IIDResampler<TradeType>;

      // Statistic computed with parallel Monte-Carlo on synthetic trade sequences
      typename mkc_timeseries::BCaBootStrap<Decimal, Sampler, randutils::mt19937_rng, void, TradeType>::StatFn statFn =
        [=, &exec](const std::vector<TradeType>& sample) -> Decimal {
          return drawdownFractile(sample, nTrades, nReps, ddConf, exec);
        };

      // Instantiate BCa with TradeType overrides
      mkc_timeseries::BCaBootStrap<Decimal, Sampler, randutils::mt19937_rng, void, TradeType> 
          bca(trades, numResamples, confidenceLevel, statFn, Sampler(), intervalType);

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
