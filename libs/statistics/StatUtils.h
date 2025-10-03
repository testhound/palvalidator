#pragma once
#include <vector>
#include <cmath>
#include <tuple>
#include <random>
#include <functional> 
#include <numeric> // Required for std::accumulate
#include <algorithm>
#include <cstddef>
#include "DecimalConstants.h"
#include "number.h"
#include "TimeSeriesIndicators.h"
#include "randutils.hpp"

namespace mkc_timeseries
{
  template <class Decimal>
  struct GeoMeanStat
  {
    // Optional behavior for r <= -1: "throw" or "clip".
    explicit GeoMeanStat(bool clip_ruin = false, double eps = 1e-12)
      : clip(clip_ruin),
	epsilon(eps)
    {}

    Decimal operator()(const std::vector<Decimal>& v) const
    {
      if (v.empty())
	return Decimal(0);

      long double sum = 0.0L;
      size_t n = 0;

      for (const auto& d : v)
	{
	  double r = d.getAsDouble();
	  if (r <= -1.0)
	    {
	      if (!clip)
		throw std::domain_error("GeoMeanStat: r <= -1 (log(1+r) undefined)");

	      r = -1.0 + epsilon; // winsorize to avoid -inf; also log it in your pipeline
            }

	  sum += std::log1p(r);
	  ++n;
        }

      return Decimal(std::expm1(sum / static_cast<long double>(n)));
    }

  private:
    bool   clip;
    double epsilon;
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
       * @brief Computes the sample standard deviation of a vector of Decimal values.
       * @param data The vector of data points.
       * @param mean The pre-computed mean of the data.
       * @return The sample standard deviation.
       */
      static Decimal computeStdDev(const std::vector<Decimal>& data, const Decimal& mean)
      {
          if (data.size() < 2) {
              return DecimalConstants<Decimal>::DecimalZero;
          }
          Decimal sq_sum = std::accumulate(data.begin(), data.end(), DecimalConstants<Decimal>::DecimalZero,
              [&mean](const Decimal& acc, const Decimal& val) {
                  return acc + (val - mean) * (val - mean);
              });
          return Decimal(std::sqrt(num::to_double(sq_sum / Decimal(data.size() - 1))));
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
