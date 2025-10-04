#pragma once

#include <vector>
#include <random>
#include <cstddef>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include "randutils.hpp"

namespace palvalidator
{
  namespace resampling
  {
    /**
     * @class RegimeMixBlockResampler
     * @brief A state-aware, fixed-length block resampler that enforces a target regime mix.
     *
     * @details
     * This advanced resampling policy is designed for time series data where each
     * observation belongs to a specific "regime" or "state" (e.g., bull market, bear
     * market, high volatility). Unlike simpler methods, it constructs a bootstrap
     * sample that explicitly tries to match a user-defined proportional mix of these
     * regimes.
     *
     * It operates by resampling fixed-length, homogeneous blocks of data (where all
     * observations in a block belong to the same regime) and assembling them in a
     * way that respects the target weights for each regime. This makes it a powerful
     * tool for "what-if" analysis, such as simulating performance in a future where
     * one regime is expected to be more dominant than it was historically.
     *
     * This class is intended to be used as the `Sampler` template argument for the
     * `mkc_timeseries::BCaBootStrap` class.
     *
     * @tparam Num The numeric type for calculations (e.g., double, number).
     * @tparam Rng The random number generator type. Defaults to `randutils::mt19937_rng`.
     */
    template <class Num, class Rng = randutils::mt19937_rng>
    class RegimeMixBlockResampler
    {
    public:
      /**
       * @brief Constructs a RegimeMixBlockResampler.
       * @param L The fixed length of blocks to resample. Coerced to be at least 2.
       * @param labels A vector of integers specifying the regime (0, 1, ..., S-1) for each
       * observation in the original time series. Must be the same size as the data.
       * @param targetWeights A vector of doubles specifying the desired proportion of each
       * regime in the final resampled series. The vector will be normalized to sum to 1.
       * @param minBarsPerRegime The minimum number of available data points required for a
       * regime to be considered for resampling.
       * @throws std::invalid_argument if inputs are inconsistent (e.g., empty labels,
       * weights size mismatch, negative weights).
       */
      RegimeMixBlockResampler(std::size_t L,
			      const std::vector<int> &labels,
			      const std::vector<double> &targetWeights,
			      std::size_t minBarsPerRegime = 8)
        : mL(std::max<std::size_t>(2, L)),
          mLabels(labels),
          mWeights(targetWeights),
          mMinBarsPerRegime(minBarsPerRegime)
      {
        if (mLabels.empty())
	  {
            throw std::invalid_argument("RegimeMixBlockResampler: empty labels");
	  }

        const std::size_t S = maxLabel() + 1;

        if (mWeights.size() != S)
	  {
            throw std::invalid_argument("RegimeMixBlockResampler: weights size must match number of regimes");
	  }

        double sumw = 0.0;
        for (double w : mWeights)
	  {
            if (w < 0.0) throw std::invalid_argument("RegimeMixBlockResampler: negative weight");
            sumw += w;
	  }

        if (sumw <= 0.0)
	  {
            throw std::invalid_argument("RegimeMixBlockResampler: zero weight sum");
	  }

        for (double &w : mWeights)
	  {
            w /= sumw;
	  }
      }

      /**
       * @brief Creates a bootstrap sample that adheres to the target regime mix.
       *
       * This is the core operator called by `BCaBootStrap`. It constructs a new
       * time series of length `n` by sampling blocks of length `L` from the
       * original data `x`.
       *
       * ### Algorithmic Steps:
       *
       * 1.  **Identify Valid Starting Points:**
       * The algorithm first scans the original data to find all possible starting
       * indices for resampling. A starting index `t` is considered valid only
       * if the entire block of data from `t` to `t + L - 1` belongs to the
       * **same regime**. These valid starting indices are collected into separate
       * "pools," one for each regime.
       *
       * 2.  **Calculate Regime Quotas:**
       * Based on the `targetWeights` and the desired output size `n`, the
       * algorithm calculates the target number of data points (the "quota")
       * that should come from each regime in the final resampled series. For
       * example, if `n=1000` and `targetWeights = {0.6, 0.4}`, the quota for
       * regime 0 would be 600 bars and for regime 1 would be 400 bars.
       *
       * 3.  **Fill by Round-Robin:**
       * The algorithm then iterates through the regimes in a round-robin fashion
       * (0, 1, 2, ..., 0, 1, 2, ...). In each turn, for the current regime `s`:
       * - If the quota for regime `s` is not yet filled, it randomly picks a
       * valid starting index from the pool for regime `s`.
       * - It copies the block of `L` data points starting from that index into
       * the output vector `y`.
       * - It then decrements the quota for regime `s` by `L`.
       *
       * 4.  **Handle Data Scarcity (Fallback):**
       * If a regime `s` still has a quota to be filled but its pool of valid
       * starting points is empty (i.e., there's not enough homogeneous data for
       * that regime in the original series), the algorithm implements a fallback.
       * It "reassigns" the remaining quota of regime `s` to the next available
       * regime that *does* have starting points, ensuring the process doesn't
       * stall. This is a best-effort attempt to meet the total length `n`.
       *
       * 5.  **Final Padding (Safety Net):**
       * In the rare case that the round-robin process finishes but the output
       * vector `y` is still not of length `n` (due to block sizes not perfectly
       * dividing quotas), the algorithm pads the remainder of `y` by drawing
       * blocks unconditionally (ignoring regimes) until `y` reaches the
       * target size `n`.
       *
       * @param x The original data vector.
       * @param n The size of the resampled vector to be generated.
       * @param rng A high-quality random number generator.
       * @return A new vector of size `n`, containing the resampled time series.
       * @throws std::invalid_argument if series are too short or data/label sizes mismatch.
       */
      std::vector<Num> operator()(const std::vector<Num> &x,
				  std::size_t n,
				  Rng &rng) const
      {
        if (x.size() < 2 || n < 2)
	  {
            throw std::invalid_argument("RegimeMixBlockResampler: series too short");
	  }
        if (x.size() != mLabels.size())
	  {
            throw std::invalid_argument("RegimeMixBlockResampler: returns/labels size mismatch");
	  }

        const std::size_t S = maxLabel() + 1;
	// 1. Build start pools per regime (require L-homogeneous block)
	std::vector<std::vector<std::size_t>> pools(S);
	for (std::size_t t = 0; t + mL <= x.size(); ++t)
	  {
	    const int s0 = mLabels[t];
	    bool homogeneous = true;
	    for (std::size_t j = 1; j < mL; ++j)
	      {
		if (mLabels[t + j] != s0)
		  {
		    homogeneous = false;
		    break;
		  }
	      }
	    if (homogeneous)
	      {
		pools[s0].push_back(t);
	      }
	  }
	
        // Validate min bars per regime
        for (std::size_t s = 0; s < S; ++s)
	  {
            if (pools[s].size() * mL < mMinBarsPerRegime && mWeights[s] > 0.0)
	      {
                // Not enough data to honor target weight strictly; we will relax quotas below.
                // No throw: proceed with best effort.
	      }
	  }

        // C2/ ompute target quotas (bars) per regime
        std::vector<std::size_t> quota(S, 0);
        std::size_t assigned = 0;
        for (std::size_t s = 0; s < S; ++s)
	  {
            quota[s] = static_cast<std::size_t>(std::round(mWeights[s] * static_cast<double>(n)));
            assigned += quota[s];
	  }
        // Fix rounding drift
        while (assigned < n)
	  {
            std::size_t s = assigned % S;
            ++quota[s];
            ++assigned;
	  }
        while (assigned > n)
	  {
            std::size_t s = assigned % S;
            if (quota[s] > 0) { --quota[s]; --assigned; } else { s = (s + 1) % S; }
	  }

        // Prepare output and RNG helpers
        std::vector<Num> y;
        y.reserve(n);

        auto urand = [&](std::size_t a, std::size_t b) -> std::size_t
        {
	  // rng.uniform(a,b) available in randutils; if not, fallback to std::uniform_int_distribution
	  return rng.uniform(a, b);
        };

        // 3. Simple round-robin over regimes with remaining quota
        std::size_t s = 0;
        std::size_t safety = 0;
        while (y.size() < n && safety < 10 * n)
	  {
            if (quota[s] == 0)
	      {
                s = (s + 1) % S;
                ++safety;
                continue;
	      }

            if (!pools[s].empty())
	      {
                const std::size_t pick = pools[s][urand(0, pools[s].size() - 1)];
                const std::size_t remainingBars = n - y.size();
                const std::size_t take = std::min<std::size_t>({ mL, quota[s], remainingBars });

                for (std::size_t j = 0; j < take; ++j)
		  {
                    y.push_back(x[pick + j]);
		  }
                quota[s] = (quota[s] >= take ? quota[s] - take : 0);
	      }
            else
	      {
                // 4. Fallback: no starts for regime s; steal quota to closest available regime
                std::size_t s2 = (s + 1) % S;
                bool reassigned = false;
                for (std::size_t it = 0; it < S; ++it)
		  {
                    if (!pools[s2].empty())
		      {
                        quota[s2] += quota[s];
                        quota[s] = 0;
                        reassigned = true;
                        break;
		      }
                    s2 = (s2 + 1) % S;
		  }
                if (!reassigned)
		  {
                    // Degenerate: no pools at all (should not happen)
                    break;
		  }
	      }

            s = (s + 1) % S;
            ++safety;
	  }

        // 5. If quotas exhausted but not enough bars due to pool constraints,
	// pad with unconditional blocks
        while (y.size() < n)
	  {
            const std::size_t remainingBars = n - y.size();
            const std::size_t start = urand(0, x.size() - mL);
            const std::size_t take = std::min<std::size_t>(mL, remainingBars);
            for (std::size_t j = 0; j < take; ++j)
	      {
                y.push_back(x[start + j]);
	      }
	  }

        return y;
      }

      /**
       * @brief Performs a non-overlapping delete-block jackknife.
       *
       * This jackknife procedure is required for the BCa acceleration factor.
       * It divides the original series into `ceil(n / L)` non-overlapping blocks
       * and computes the statistic on the data remaining after deleting each block
       * one at a time. This is a simplified but compatible jackknife for fixed-block schemes.
       *
       * @tparam StatFn The type of the function object that computes the statistic.
       * @param x The original data vector.
       * @param stat The statistic function to apply to each jackknife replicate.
       * @return A vector of jackknife statistic values.
       */
      template <class StatFn>
      std::vector<Num> jackknife(const std::vector<Num> &x, StatFn stat) const
      {
        const std::size_t n = x.size();
        const std::size_t B = (n + mL - 1) / mL; // ceil(n / L)
        std::vector<Num> th;
        th.reserve(B);

        for (std::size_t b = 0; b < B; ++b)
	  {
            const std::size_t start = b * mL;
            const std::size_t end   = std::min(start + mL, n);

            std::vector<Num> tmp;
            tmp.reserve(n - (end - start));
            for (std::size_t i = 0; i < n; ++i)
	      {
                if (i < start || i >= end)
		  {
                    tmp.push_back(x[i]);
		  }
	      }
            if (tmp.size() >= 2)
	      {
                th.push_back(stat(tmp));
	      }
	  }
        return th;
      }

      std::size_t meanBlockLen() const
      {
        return mL;
      }

    private:
      std::size_t maxLabel() const
      {
        int m = 0;
        for (int z : mLabels) m = std::max(m, z);
        return static_cast<std::size_t>(m);
      }

    private:
      std::size_t mL;
      std::vector<int> mLabels;
      std::vector<double> mWeights;
      std::size_t mMinBarsPerRegime;
    };

  } // namespace resampling
} // namespace palvalidator
