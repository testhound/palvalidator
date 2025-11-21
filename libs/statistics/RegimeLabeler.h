#pragma once

#include <vector>
#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <numeric>

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Partitions a return series into three volatility regimes (Low, Mid, High).
     *
     * Objective:
     * Used by the Regime Mix filtering stage to generate market "state" labels
     * for stress testing. Instead of using arbitrary external thresholds (like VIX > 20),
     * this class calculates **Relative Volatility** based on the asset's own history.
     *
     * Algorithm:
     * 1. Calculates a rolling "Mean Absolute Deviation" (proxy for volatility)
     * over a specified window (e.g., 20 days).
     * 2. Sorts the entire history of these volatility readings.
     * 3. Identifies the 33rd and 66th percentiles (Terciles).
     * 4. Labels every bar based on which bucket it falls into:
     * - 0: Low Volatility (Bottom 33%)
     * - 1: Mid Volatility (Middle 33%)
     * - 2: High Volatility (Top 33%)
     */
    template <class Num>
    class VolTercileLabeler
    {
    public:
      /**
       * @brief Constructs the labeler with a specific lookback window.
       *
       * @param window The size of the rolling window used to calculate volatility.
       * A value of 20 (approx 1 trading month) is standard.
       * Must be >= 2.
       *
       * @throws std::invalid_argument if window < 2.
       */
      explicit VolTercileLabeler(std::size_t window)
        : mWindow(window)
      {
        if (mWindow < 2)
	  {
            throw std::invalid_argument("VolTercileLabeler: window must be >= 2");
	  }
      }

      /**
       * @brief Computes the regime label (0, 1, or 2) for every bar in the input series.
       *
       * Logic:
       * 1. **Rolling Volatility:** Computes the mean absolute return over `mWindow`.
       * 2. **Thresholds:** Sorts the volatility values to find the cut-off points
       * (q1 = 33rd percentile, q2 = 66th percentile).
       * 3. **Assignment:**
       * - If Vol <= q1 -> Label 0
       * - If Vol >= q2 -> Label 2
       * - Else         -> Label 1
       * 4. **Padding:**
       * - The "Warmup" period (indices 0 to window-1) is filled with the first calculated label.
       * - The "Tail" period is filled with the last calculated label.
       *
       * @param returns The vector of bar-to-bar returns (e.g., close-to-close).
       * @return A vector of integers (size == returns.size()) containing labels 0, 1, or 2.
       *
       * @throws std::invalid_argument If the return series is shorter than (window + 2).
       */
      std::vector<int> computeLabels(const std::vector<Num> &returns) const
      {
	const std::size_t n = returns.size();
	if (n < mWindow + 2)
	  {
	    throw std::invalid_argument("VolTercileLabeler: insufficient data for rolling window");
	  }

	std::vector<Num> rollAbs;
	rollAbs.reserve(n);

	Num acc = Num(0);
	for (std::size_t t = 0; t < mWindow; ++t)
	  {
	    acc += (returns[t] >= 0 ? returns[t] : -returns[t]);
	  }
	rollAbs.push_back(acc / Num(mWindow));

	for (std::size_t t = mWindow; t < n; ++t)
	  {
	    const Num in  = (returns[t] >= 0 ? returns[t] : -returns[t]);
	    const Num out = (returns[t - mWindow] >= 0 ? returns[t - mWindow] : -returns[t - mWindow]);
	    acc += in - out;
	    rollAbs.push_back(acc / Num(mWindow));
	  }

	// Build cut points (terciles) on the rolling series
	std::vector<Num> sorted = rollAbs;
	std::sort(sorted.begin(), sorted.end());
	const auto idx1 = static_cast<std::size_t>(sorted.size() * 1.0 / 3.0);
	const auto idx2 = static_cast<std::size_t>(sorted.size() * 2.0 / 3.0);
	const Num q1 = sorted[std::min(idx1, sorted.size() - 1)];
	const Num q2 = sorted[std::min(idx2, sorted.size() - 1)];

	std::vector<int> labels(n, 1);

	// Assign labels where we have a full rolling window
	for (std::size_t i = 0; i < rollAbs.size(); ++i)
	  {
	    labels[i] = (rollAbs[i] <= q1 ? 0 : (rollAbs[i] >= q2 ? 2 : 1));
	  }

	// Fill the warmup (first mWindow-1 bars) forward from the first valid label
	const std::size_t firstValid = mWindow - 1;
	if (firstValid < n)
	  {
	    for (std::size_t i = 0; i < firstValid; ++i)
	      {
		labels[i] = labels[firstValid];
	      }
	  }

	// Fill the trailing tail (last mWindow-1 bars) with the last computed label
	const std::size_t lastComputedIdx = (rollAbs.empty() ? 0 : rollAbs.size() - 1);
	if (lastComputedIdx < n)
	  {
	    for (std::size_t i = lastComputedIdx + 1; i < n; ++i)
	      {
		labels[i] = labels[lastComputedIdx];
	      }
	  }

	return labels;
      }

    private:
      std::size_t mWindow;
    };

  } // namespace analysis
} // namespace palvalidator
