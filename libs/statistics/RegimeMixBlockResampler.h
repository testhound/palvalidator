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

    // State-aware fixed-length block resampler enforcing a target regime mix.
    //  - Mean block length ≈ L (fixed L here; simple and stable for short holds)
    //  - Labels z_t in {0..S-1}
    //  - Target weights w (sum to 1) for each regime
    // No default ctor: must provide L, labels, and target weights.
    template <class Num, class Rng = randutils::mt19937_rng>
    class RegimeMixBlockResampler
    {
    public:
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

      // Bootstrap operator used by BCaBootStrap: returns a resampled series of length n.
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
	// Build start pools per regime (require L-homogeneous block)
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

        // Compute target quotas (bars) per regime
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

        // Simple round-robin over regimes with remaining quota
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
                // Fallback: no starts for regime s; steal quota to closest available regime
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

        // If quotas exhausted but not enough bars due to pool constraints, pad with unconditional blocks
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

      // Delete-block jackknife compatible with BCa acceleration (stat over remaining data)
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
