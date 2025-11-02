// PercentileTBootstrap.h
// Studentized percentile-t bootstrap with composable resampler.
// - Outer loop: resample → theta*.
// - Inner loop (per outer replicate): bootstrap the outer sample to estimate SE*.
// - Collect t* = (theta* - theta_hat) / SE*; use type-7 quantiles on t*.
// - Final CI: [theta_hat - t_{1-a/2} * SE_hat, theta_hat - t_{a/2} * SE_hat],
//   where SE_hat = sd(theta*) over outer replicates.
//
// Requirements:
//   Sampler:   Decimal operator()(const std::vector<Decimal>&)
//   Resampler: void operator()(const std::vector<Decimal>& x,
//                               std::vector<Decimal>& y,
//                               std::size_t m,
//                               Rng& rng) const;
//              std::size_t getL() const;
//
// Default RNG: randutils::mt19937_rng (matches your BCa code)

#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include "number.h"
#include "randutils.hpp"
#include "RngUtils.h"
#include "MOutOfNPercentileBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

namespace palvalidator
{
  namespace analysis
  {
    /*
      PercentileTBootstrap
      --------------------
      Two-level (nested) studentized percentile-t bootstrap with a pluggable resampler.
      
      Why this class exists
      ---------------------
      Plain percentile bootstrap intervals can under-cover when:
      - the sampling distribution of your statistic is skewed or heavy-tailed, or
      - n is small (~20–40), which is common for per-strategy trade samples.
      
      The percentile-t approach helps by “studentizing” each outer bootstrap replicate:
      * Outer loop: resample the original series → compute θ* (your sampler/statistic)
      * Inner loop: resample again from the OUTER sample → estimate SE* for that θ*
      * Build the studentized pivot t = (θ* - θ_hat) / SE*
      * Take percentile quantiles of t to form a CI around θ_hat using SE_hat
      (SE_hat = SD of θ* across outer replicates, which is simple and stable)

      Composability
      -------------
      - Sampler is any callable: Decimal f(const std::vector<Decimal>&)
      (e.g., GeoMeanStat on per-period returns, log-PF, etc.)
      - Resampler is any object with:
      void operator()(const std::vector<Decimal>& x,
      std::vector<Decimal>& y,
      std::size_t m,
      Rng& rng) const;
      std::size_t getL() const;
      This lets you plug in stationary-block resampling (recommended) or IID resampling.
      - RNG defaults to randutils::mt19937_rng to match the rest of your codebase.
      
      Small-n notes
      -------------
      - You can use full-size outer samples (m_outer = n) and still get benefits from
      studentization.
      - Or, you can make the inner loop slightly smaller (m_inner < m_outer) to gain
      a touch of conservatism when n is very small. The class exposes ratios and
      per-call overrides for both outer and inner m.
      
      Replicate counts (B_outer, B_inner)
      -----------------------------------
      These affect *quantile smoothness* and stability, not your data size. As a rule of thumb:
      - B_outer >= 400
      - B_inner >= 100
      More is better if runtime allows.
      
      Returned diagnostics
      --------------------
      The Result struct includes:
      - mean, lower, upper     → θ_hat and the percentile-t CI (per-period scale)
      - cl                     → confidence level
      - B_outer, B_inner       → requested replicate counts
      - effective_B            → number of usable outer reps (finite pivots)
      - skipped_outer          → outer reps skipped due to NaN/Inf or SE* <= 0, etc.
      - skipped_inner_total    → total inner replicates skipped across all outer reps
      - n, m_outer, m_inner    → sizes used
      - L                      → resampler L (diagnostics)
      - se_hat                 → SD(θ*) across valid outer reps (used in final CI)
      
      CI construction summary
      -----------------------
      1) θ_hat = sampler(x)
      2) For b in 1..B_outer:
      y_outer ~ resampler(x, m_outer)
      θ*_b = sampler(y_outer)
      For j in 1..B_inner:
      y_inner ~ resampler(y_outer, m_inner)
      θ°_bj = sampler(y_inner)
      SE*_b = SD({θ°_bj})
      t_b = (θ*_b - θ_hat) / SE*_b
      Keep only finite t_b and finite θ*_b with positive SE*_b
      3) SE_hat = SD({θ*_b}) over valid outer replicates
      4) t_lo, t_hi = type-7 quantiles of {t_b} at α/2 and 1-α/2 (α = 1-CL)
      5) CI = [θ_hat - t_hi * SE_hat,  θ_hat - t_lo * SE_hat]
    */

    template <class Decimal,
       class Sampler,
       class Resampler,
       class Rng = randutils::mt19937_rng,
       typename Executor = concurrency::SingleThreadExecutor>
    class PercentileTBootstrap
    {
    public:
      struct Result
      {
        Decimal     mean;               // theta_hat on original sample
        Decimal     lower;              // lower CI (per-period scale of statistic)
        Decimal     upper;              // upper CI
        double      cl;                 // confidence level
        std::size_t B_outer;            // outer resamples requested
        std::size_t B_inner;            // inner resamples requested (per outer)
        std::size_t effective_B;        // # outer reps that produced finite t*
        std::size_t skipped_outer;      // outer reps skipped (NaN/Inf theta* or SE* <= 0)
        std::size_t skipped_inner_total;// sum of inner degenerates across all outer reps
        std::size_t n;                  // original sample length
        std::size_t m_outer;            // outer m used (<= n)
        std::size_t m_inner;            // inner m used (<= m_outer)
        std::size_t L;                  // resampler mean block length
        double      se_hat;             // SD(theta*) across effective outer reps
      };

    public:
      // Constructor
      //
      // B_outer, B_inner   → replicate counts (affect CI smoothness, not data size)
      // confidence_level   → between 0.5 and 1.0 (e.g. 0.95)
      // resampler          → object implementing resampler(x, y, m, rng)
      // m_ratio_outer      → outer subsample ratio (0,1], typical 1.0
      // m_ratio_inner      → inner subsample ratio (0,1], typical 1.0
      //
      // Throws std::invalid_argument for bad arguments.
      PercentileTBootstrap(std::size_t B_outer,
			   std::size_t B_inner,
			   double      confidence_level,
			   const Resampler& resampler,
			   double      m_ratio_outer = 1.0,
			   double      m_ratio_inner = 1.0)
	: m_B_outer(B_outer)
	, m_B_inner(B_inner)
	, m_CL(confidence_level)
	, m_resampler(resampler)
	, m_ratio_outer(m_ratio_outer)
	, m_ratio_inner(m_ratio_inner)
      {
        if (m_B_outer < 400)
	  {
            throw std::invalid_argument("PercentileTBootstrap: B_outer should be >= 400");
	  }
        if (m_B_inner < 100)
	  {
            throw std::invalid_argument("PercentileTBootstrap: B_inner should be >= 100");
	  }
        if (!(m_CL > 0.5 && m_CL < 1.0))
	  {
            throw std::invalid_argument("PercentileTBootstrap: CL must be in (0.5,1)");
	  }
        if (!(m_ratio_outer > 0.0 && m_ratio_outer <= 1.0))
	  {
            throw std::invalid_argument("PercentileTBootstrap: m_ratio_outer must be in (0,1]");
	  }
        if (!(m_ratio_inner > 0.0 && m_ratio_inner <= 1.0))
	  {
            throw std::invalid_argument("PercentileTBootstrap: m_ratio_inner must be in (0,1]");
	  }
      }

      // Run the nested bootstrap and return a studentized CI.
      //
      // x          → input sample (length n≥3)
      // sampler    → callable that computes θ(x)
      // rng        → random generator
      // m_outer_override, m_inner_override → optional explicit subsample sizes
      //
      // Returns: Result with θ̂, CI bounds, and diagnostics.
      // Throws:  invalid_argument or runtime_error if inputs invalid or too many degenerate reps.

      // PercentileTBootstrap::run — outer-only parallel, adaptive inner SE*, nth_element(type-7)
      Result run(const std::vector<Decimal>& x,
		 Sampler                      sampler,
		 Rng&                         rng,
		 std::size_t                  m_outer_override = 0,
		 std::size_t                  m_inner_override = 0) const
      {
	const std::size_t n = x.size();
	if (n < 3)
	  {
	    throw std::invalid_argument("PercentileTBootstrap.run: n must be >= 3");
	  }

	// Decide m_outer and m_inner from overrides or ratios
	std::size_t m_outer = (m_outer_override > 0)
	  ? m_outer_override
	  : static_cast<std::size_t>(std::floor(m_ratio_outer * static_cast<double>(n)));
	if (m_outer < 2)  m_outer = 2;
	if (m_outer > n)  m_outer = n;

	std::size_t m_inner = (m_inner_override > 0)
	  ? m_inner_override
	  : static_cast<std::size_t>(std::floor(m_ratio_inner * static_cast<double>(m_outer)));
	if (m_inner < 2)        m_inner = 2;
	if (m_inner > m_outer)  m_inner = m_outer;

	// Baseline statistic
	const Decimal theta_hat   = sampler(x);
	const double  theta_hat_d = num::to_double(theta_hat);

	// Output buffers (index-addressable; safe for parallel fill by index)
	std::vector<double> theta_star_ds(m_B_outer, std::numeric_limits<double>::quiet_NaN());
	std::vector<double> tvals         (m_B_outer, std::numeric_limits<double>::quiet_NaN());

	// Diagnostics
	std::atomic<std::size_t> skipped_outer{0};
	std::atomic<std::size_t> skipped_inner_total{0};

	// Per-outer RNGs: seed deterministically from caller's rng using common utilities
	std::vector<Rng> rngs(m_B_outer);
	for (std::size_t b = 0; b < m_B_outer; ++b)
	  {
	    const uint64_t s1 = mkc_timeseries::rng_utils::get_random_value(rng);
	    const uint64_t s2 = mkc_timeseries::rng_utils::get_random_value(rng);
	    std::seed_seq seq{
	      static_cast<uint32_t>(s1),
	      static_cast<uint32_t>(s1 >> 32),
	      static_cast<uint32_t>(s2),
	      static_cast<uint32_t>(s2 >> 32)
	    };
	    rngs[b] = Rng(seq);
	  }

	// Resampler diagnostics (IID resampler may return 0)
	const std::size_t Ldiag = m_resampler.getL();

	// -----------------------------
	// Parallelize the OUTER loop only
	// -----------------------------
	Executor exec{};
	concurrency::parallel_for_chunked(static_cast<uint32_t>(m_B_outer), exec,
					  [&](uint32_t b32)
					  {
					    const std::size_t b = static_cast<std::size_t>(b32);
					    Rng& local_rng = rngs[b];

					    // Thread-local reusable buffers for this task/iteration
					    std::vector<Decimal> y_outer; y_outer.resize(m_outer);
					    std::vector<Decimal> y_inner; y_inner.resize(m_inner);

					    // OUTER resample
					    m_resampler(x, y_outer, m_outer, local_rng);

					    // theta* on OUTER sample
					    const Decimal theta_star   = sampler(y_outer);
					    const double  theta_star_d = num::to_double(theta_star);
					    if (!std::isfinite(theta_star_d))
					      {
						skipped_outer.fetch_add(1, std::memory_order_relaxed);
						return;
					      }

					    // --- Inner bootstrap for SE* (adaptive early-stop, Welford accumulators) ---
					    double mean = 0.0, m2 = 0.0;
					    std::size_t eff_inner = 0;

					    auto push_inner = [&](double v) noexcept
					    {
					      ++eff_inner;
					      const double delta = v - mean;
					      mean += delta / static_cast<double>(eff_inner);
					      m2   += delta * (v - mean);
					    };

					    constexpr std::size_t MIN_INNER    = 100;   // robustness floor
					    constexpr std::size_t CHECK_EVERY  = 16;    // stabilization cadence
					    constexpr double      REL_EPS      = 0.015; // 1.5% relative tolerance
					    double last_se = std::numeric_limits<double>::infinity();

					    for (std::size_t j = 0; j < m_B_inner; ++j)
					      {
						m_resampler(y_outer, y_inner, m_inner, local_rng);
						const Decimal theta_inner = sampler(y_inner);
						const double  v = num::to_double(theta_inner);
						if (!std::isfinite(v))
						  {
						    skipped_inner_total.fetch_add(1, std::memory_order_relaxed);
						    continue;
						  }
						push_inner(v);

						if (eff_inner >= MIN_INNER && ((eff_inner % CHECK_EVERY) == 0))
						  {
						    const double se_now = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner)));
						    if (std::isfinite(se_now) &&
							std::fabs(se_now - last_se) <= REL_EPS * std::max(se_now, 1e-300))
						      {
							break; // SE* stabilized
						      }
						    last_se = se_now;
						  }
					      }

					    if (eff_inner < MIN_INNER)
					      {
						skipped_outer.fetch_add(1, std::memory_order_relaxed);
						return;
					      }

					    const double se_star = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner)));
					    if (!(se_star > 0.0) || !std::isfinite(se_star))
					      {
						skipped_outer.fetch_add(1, std::memory_order_relaxed);
						return;
					      }

					    const double t_b = (theta_star_d - theta_hat_d) / se_star;

					    // Write results
					    theta_star_ds[b] = theta_star_d;
					    tvals[b]         = t_b;
					  });

	// Collect effective outer replicates
	std::vector<double> t_eff;     t_eff.reserve(m_B_outer);
	std::vector<double> theta_eff; theta_eff.reserve(m_B_outer);

	for (std::size_t b = 0; b < m_B_outer; ++b)
	  {
	    const double tb = tvals[b];
	    const double th = theta_star_ds[b];
	    if (std::isfinite(tb) && std::isfinite(th))
	      {
		t_eff.push_back(tb);
		theta_eff.push_back(th);
	      }
	  }

	const std::size_t effective_B = t_eff.size();
	if (effective_B < 16)
	  {
	    throw std::runtime_error("PercentileTBootstrap.run: too few finite studentized pivots");
	  }

	// SE_hat = SD(theta*) across valid outer replicates
	double se_hat;
	{
	  double sum = 0.0, sum2 = 0.0;
	  for (double v : theta_eff)
	    {
	      sum  += v;
	      sum2 += v * v;
	    }
	  const double m = static_cast<double>(theta_eff.size());
	  const double var = std::max(0.0, (sum2 / m) - (sum / m) * (sum / m));
	  se_hat = std::sqrt(var);
	}

	// Type-7 quantile via two nth_element passes (O(B))
	auto type7_quantile = [](std::vector<double> v, double p) -> double
	{
	  const std::size_t m = v.size();
	  if (m == 0) return std::numeric_limits<double>::quiet_NaN();
	  if (m == 1) return v[0];

	  const double h = (m - 1) * p;
	  const std::size_t k = static_cast<std::size_t>(std::floor(h));
	  const double frac = h - static_cast<double>(k);

	  std::nth_element(v.begin(), v.begin() + k, v.end());
	  const double vk = v[k];

	  if (frac == 0.0 || k + 1 == m)
            return vk;

	  std::nth_element(v.begin() + k + 1, v.begin() + k + 1, v.end());
	  const double vkp1 = v[k + 1];

	  return vk + frac * (vkp1 - vk);
	};

	const double alpha = 1.0 - m_CL;
	const double t_lo  = type7_quantile(t_eff, alpha / 2.0);
	const double t_hi  = type7_quantile(t_eff, 1.0 - alpha / 2.0);

	// Final CI on per-period scale
	const double lower_d = theta_hat_d - t_hi * se_hat;
	const double upper_d = theta_hat_d - t_lo * se_hat;

	// Fill Result (unchanged struct)
	Result R;
	R.mean                = theta_hat;
	R.lower               = Decimal(lower_d);
	R.upper               = Decimal(upper_d);
	R.cl                  = m_CL;
	R.B_outer             = m_B_outer;
	R.B_inner             = m_B_inner;
	R.effective_B         = effective_B;
	R.skipped_outer       = skipped_outer.load(std::memory_order_relaxed);
	R.skipped_inner_total = skipped_inner_total.load(std::memory_order_relaxed);
	R.n                   = n;
	R.m_outer             = m_outer;
	R.m_inner             = m_inner;
	R.L                   = Ldiag;
	R.se_hat              = se_hat;
	return R;
      }

    private:
      std::size_t  m_B_outer;
      std::size_t  m_B_inner;
      double       m_CL;
      Resampler    m_resampler;
      double       m_ratio_outer;
      double       m_ratio_inner;
    };
  }
} // namespace palvalidator::analysis
