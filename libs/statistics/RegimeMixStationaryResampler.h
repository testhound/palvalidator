#pragma once

#include <vector>
#include <random>
#include <cstddef>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <functional>
#include "randutils.hpp"

namespace palvalidator
{
  namespace resampling
  {

    /**
     * @class RegimeMixStationaryResampler
     * @brief Regime-aware **stationary** (geometric-length) block bootstrap.
     *
     * @details
     * This sampler generates bootstrap samples honoring a desired **regime mix** while
     * using **variable-length blocks** whose lengths follow a geometric distribution
     * with mean `L` (i.e., Politis & Romano stationary bootstrap within each regime).
     *
     * Key behaviors:
     * - **Within-regime stationary blocks:** Each copied block stays inside a single
     *   regime. Block length is `1 + Geom(p)`, where `p = 1/L`, but it is truncated
     *   to the remaining same-regime run length and to the remaining quota/space.
     * - **Circular wrap:** The source series is treated as circular for both data and labels.
     * - **Target mix quotas:** Desired regime weights (normalized) are turned into bar
     *   quotas. We fill quotas in a simple round-robin over regimes. Quotas are
     *   **approximate** with stationary lengths but converge over n.
     * - **Graceful scarcity:** If a regime lacks starts, we skip it and continue; if all
     *   are scarce, we pad from any regime to reach length `n`.
     *
     * This class is intended to be used as the `Sampler` template parameter for
     * `mkc_timeseries::BCaBootStrap`.
     *
     * @tparam Num The numeric type for calculations (e.g., `double`, `number`).
     * @tparam Rng The RNG type (defaults to `randutils::mt19937_rng`).
     */
    template <class Num, class Rng = randutils::mt19937_rng>
    class RegimeMixStationaryResampler
    {
    public:
      /**
       * @brief Construct a regime-aware stationary resampler.
       * @param L            Mean block length (coerced to >= 2).
       * @param labels       Regime label for each observation (size must equal data size).
       * @param targetWeights Desired proportion for each regime; normalized internally.
       * @param minBarsPerRegime Minimum bars required for a regime (best-effort if unmet).
       */
      RegimeMixStationaryResampler(std::size_t L,
                                   const std::vector<int>& labels,
                                   const std::vector<double>& targetWeights,
                                   std::size_t minBarsPerRegime = 8)
        : mL(std::max<std::size_t>(2, L)),
          mLabels(labels),
          mWeights(targetWeights),
          mMinBarsPerRegime(minBarsPerRegime)
      {
        if (mLabels.empty())
          throw std::invalid_argument("RegimeMixStationaryResampler: empty labels");

        const std::size_t S = maxLabel() + 1;

        if (mWeights.size() != S)
          throw std::invalid_argument("RegimeMixStationaryResampler: weights size must match number of regimes");

        double sumw = 0.0;
        for (double w : mWeights) {
          if (w < 0.0) throw std::invalid_argument("RegimeMixStationaryResampler: negative weight");
          sumw += w;
        }
        if (sumw <= 0.0)
          throw std::invalid_argument("RegimeMixStationaryResampler: zero weight sum");

        for (double& w : mWeights) w /= sumw;
      }

      /**
       * @brief Create a bootstrap sample that adheres to the target regime mix.
       *
       * @param x   Original data vector (percent returns).
       * @param n   Desired output length.
       * @param rng RNG instance.
       * @return New vector of size `n`, resampled with regime-aware stationary blocks.
       *
       * @throws std::invalid_argument if series are too short or sizes mismatch.
       */
      std::vector<Num> operator()(const std::vector<Num>& x,
                                  std::size_t n,
                                  Rng& rng) const
      {
        if (x.size() < 2 || n < 2)
          throw std::invalid_argument("RegimeMixStationaryResampler: series too short");
        if (x.size() != mLabels.size())
          throw std::invalid_argument("RegimeMixStationaryResampler: returns/labels size mismatch");

        const std::size_t xn = x.size();
        const std::size_t S  = maxLabel() + 1;

        // --- Build start pools per regime (any index with that regime) ---
        // Unlike fixed-L sampler, we do *not* require L-homogeneity here;
        // we enforce same-regime constraint during copy by truncation to run length.
        std::vector<std::vector<std::size_t>> pools(S);
        for (std::size_t t = 0; t < xn; ++t) {
          const int s = mLabels[t];
          if (s >= 0 && static_cast<std::size_t>(s) < S)
            pools[static_cast<std::size_t>(s)].push_back(t);
        }

        // --- Validate min bars per regime (best-effort; do not throw) ---
        for (std::size_t s = 0; s < S; ++s) {
          if (pools[s].size() < mMinBarsPerRegime && mWeights[s] > 0.0) {
            // Log outside if desired; continue with relaxed quotas.
          }
        }

        // --- Compute quotas (bars) per regime from target weights (rounded) ---
        std::vector<std::size_t> quota(S, 0);
        std::size_t assigned = 0;
        for (std::size_t s = 0; s < S; ++s) {
          quota[s] = static_cast<std::size_t>(std::round(mWeights[s] * static_cast<double>(n)));
          assigned += quota[s];
        }
        // Fix rounding drift
        while (assigned < n) { std::size_t s = assigned % S; ++quota[s]; ++assigned; }
        while (assigned > n) {
          std::size_t s = assigned % S;
          if (quota[s] > 0) { --quota[s]; --assigned; } else { s = (s + 1) % S; }
        }

        // --- Prepare output, RNG helpers, and geometric length draw ---
        std::vector<Num> y;
        y.reserve(n);

        auto urand = [&](std::size_t a, std::size_t b) -> std::size_t {
          return rng.uniform(a, b);
        };

        // Parameter p = 1 / L for Geometric(p); length = 1 + Geom(p) on {0,1,...}
        const double p = 1.0 / static_cast<double>(mL);
        // NOTE: randutils exposes rng.engine() compatible with <random> distributions.
        std::geometric_distribution<std::size_t> geo(p);

        // --- Round-robin fill honoring quotas approximately ---
        std::size_t s = 0;
        std::size_t safety = 0;

        while (y.size() < n && safety < 10 * n)
	  {
	    if (quota[s] == 0 || pools[s].empty()) {
	      s = (s + 1) % S;
	      ++safety;
	      continue;
	    }

	    // Random start index from regime s
	    const auto& pool = pools[s];
	    const std::size_t startIdxInPool = urand(0, pool.size() - 1);
	    const std::size_t start = pool[startIdxInPool];

	    // Proposed stationary block length (at least 1)
	    std::size_t len = 1 + geo(rng.engine());

	    // Compute maximum contiguous run length in this regime from 'start' with wrap.
	    const std::size_t runLen = sameRegimeRunLenFrom(start, static_cast<int>(s), xn);

	    // Truncate: cannot cross regime boundary; cannot exceed remaining output slots or quota.
	    const std::size_t remaining = n - y.size();
	    std::size_t k = std::min({len, runLen, remaining, quota[s]});
	    if (k == 0) { // If truncation yields zero, skip regime this round.
	      s = (s + 1) % S;
	      ++safety;
	      continue;
	    }

	    // Copy k elements from x with wrap handling
	    copyWithWrap(x, start, k, y);

	    // Reduce quota by actual k copied
	    quota[s] -= k;

	    // Next regime in round-robin
	    s = (s + 1) % S;
	    ++safety;
	  }

        // --- Final padding if still short (fallback: ignore regimes) ---
        while (y.size() < n) {
          const std::size_t remaining = n - y.size();
          const std::size_t start = urand(0, xn - 1);

          // Draw a length and clip to remaining and to same-regime run to avoid label conflicts
          const int s0 = mLabels[start];
          const std::size_t runLen = sameRegimeRunLenFrom(start, s0, xn);
          std::size_t len = 1 + geo(rng.engine());
          std::size_t k = std::min({len, runLen, remaining});

          if (k == 0) k = 1; // as a last-resort guarantee progress

          copyWithWrap(x, start, k, y);
        }

        return y;
      }

      template <class StatFn>
      std::vector<Num> jackknife(const std::vector<Num>& x, StatFn stat) const
      {
	const std::size_t n = x.size();
	if (n < 2) {
	  throw std::invalid_argument("RegimeMixStationaryResampler::jackknife requires n>=2.");
	}

	// Effective delete-block length: use mean block length, but ensure keep >= 1
	const std::size_t L_eff = std::min(mL, n - 1);
	const std::size_t keep  = n - L_eff;

	std::vector<Num> jk(n);     // one replicate per circular start
	std::vector<Num> y(keep);   // buffer to hold the kept portion

	for (std::size_t start = 0; start < n; ++start)
	  {
	    // Kept region begins immediately after the deleted block (with wrap)
	    const std::size_t start_keep = (start + L_eff) % n;

	    // Copy keep entries from x[start_keep … start_keep+keep) with wrap
	    const std::size_t tail = std::min(keep, n - start_keep); // contiguous tail to end
	    // First span: [start_keep, start_keep + tail)
	    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
			static_cast<std::ptrdiff_t>(tail),
			y.begin());

	    // Second span (wrap): [0, keep - tail)
	    const std::size_t head = keep - tail;
	    if (head != 0) {
	      std::copy_n(x.begin(),
			  static_cast<std::ptrdiff_t>(head),
			  y.begin() + static_cast<std::ptrdiff_t>(tail));
	    }

	    jk[start] = stat(y);
	  }

	return jk;
      }
      
      /** @return Mean block length parameter L. */
      std::size_t meanBlockLen() const { return mL; }

    private:
      // --- Helpers ---

      std::size_t maxLabel() const
      {
        int m = -1;
        for (int v : mLabels) if (v > m) m = v;
        return static_cast<std::size_t>(m < 0 ? 0 : m);
      }

      /**
       * @brief Compute the longest run (with wrap) starting at idx that stays in regime s.
       * @param idx Start index in [0, xn).
       * @param s   Regime label to match.
       * @param xn  Total length of the series.
       * @return Run length in [1, xn].
       */
      std::size_t sameRegimeRunLenFrom(std::size_t idx, int s, std::size_t xn) const
      {
        std::size_t len = 0;
        while (len < xn) {
          const std::size_t j = (idx + len) % xn;
          if (mLabels[j] != s) break;
          ++len;
        }
        return std::max<std::size_t>(1, len); // at least 1 if labels[idx]==s
      }

      /**
       * @brief Copy k elements from x starting at idx (with wrap) into y (append).
       */
      void copyWithWrap(const std::vector<Num>& x,
                        std::size_t idx,
                        std::size_t k,
                        std::vector<Num>& y) const
      {
        const std::size_t xn = x.size();
        const std::size_t room_to_end = xn - idx;
        if (k <= room_to_end) {
          y.insert(y.end(), x.begin() + static_cast<std::ptrdiff_t>(idx),
                             x.begin() + static_cast<std::ptrdiff_t>(idx + k));
        } else {
          y.insert(y.end(), x.begin() + static_cast<std::ptrdiff_t>(idx), x.end());
          const std::size_t rem = k - room_to_end;
          y.insert(y.end(), x.begin(), x.begin() + static_cast<std::ptrdiff_t>(rem));
        }
      }

    private:
      std::size_t       mL;
      std::vector<int>  mLabels;
      std::vector<double> mWeights;
      std::size_t       mMinBarsPerRegime;
    };

  } // namespace resampling
} // namespace palvalidator
