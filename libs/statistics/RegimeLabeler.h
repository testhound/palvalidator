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

    // Labels bars into 3 volatility terciles using a rolling window of |r|.
    //  label: 0 = LowVol, 1 = MidVol, 2 = HighVol
    // No default ctor: window must be provided.
    template <class Num>
    class VolTercileLabeler
    {
    public:
      explicit VolTercileLabeler(std::size_t window)
        : mWindow(window)
      {
        if (mWindow < 2)
	  {
            throw std::invalid_argument("VolTercileLabeler: window must be >= 2");
	  }
      }

      // RegimeLabeler.h  (only the tail-fill part is new)

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

	// NEW: Fill the trailing tail (last mWindow-1 bars) with the last computed label
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
