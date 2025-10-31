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
#include "MOutOfNPercentileBootstrap.h"

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
    template <class Decimal, class Sampler, class Resampler, class Rng = randutils::mt19937_rng>
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

        // Decide m_outer and m_inner
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

        // Original statistic
        const Decimal theta_hat = sampler(x);

        // Storage
        std::vector<Decimal> thetas_outer; thetas_outer.reserve(m_B_outer);
        std::vector<double>  tvals;        tvals.reserve(m_B_outer);

        std::vector<Decimal> y_outer;  y_outer.resize(m_outer);
        std::vector<Decimal> y_inner;  y_inner.resize(m_inner);

        std::size_t skipped_outer = 0;
        std::size_t skipped_inner_total = 0;

        // --- Outer loop ---
        for (std::size_t b = 0; b < m_B_outer; ++b)
	  {
            // Resample outer
            m_resampler(x, y_outer, m_outer, rng);

            // theta* on the outer resample
            const Decimal theta_star = sampler(y_outer);
            const double  theta_star_d = num::to_double(theta_star);
            if (!std::isfinite(theta_star_d))
	      {
                ++skipped_outer;
                continue;
	      }

            // --- Inner loop to estimate SE* on the OUTER sample ---
            double sum = 0.0;
            double sum2 = 0.0;
            std::size_t eff_inner = 0;

            for (std::size_t j = 0; j < m_B_inner; ++j)
	      {
                m_resampler(y_outer, y_inner, m_inner, rng);
                const Decimal theta_inner = sampler(y_inner);
                const double  v = num::to_double(theta_inner);
                if (!std::isfinite(v))
		  {
                    ++skipped_inner_total;
                    continue;
		  }
                ++eff_inner;
                sum  += v;
                sum2 += v * v;
	      }

            if (eff_inner < m_B_inner / 2)
	      {
                // Too many degenerate inner resamples; skip this outer replicate
                ++skipped_outer;
                continue;
	      }

            const double mean_inner = sum / static_cast<double>(eff_inner);
            const double var_inner =
	      std::max(0.0, (sum2 / static_cast<double>(eff_inner)) - (mean_inner * mean_inner));
            const double se_star = std::sqrt(var_inner);

            if (!(se_star > 0.0) || !std::isfinite(se_star))
	      {
                ++skipped_outer;
                continue;
	      }

            // Studentized pivot for this outer replicate
            const double t_b = (theta_star_d - num::to_double(theta_hat)) / se_star;

            if (!std::isfinite(t_b))
	      {
                ++skipped_outer;
                continue;
	      }

            thetas_outer.emplace_back(theta_star);
            tvals.emplace_back(t_b);
	  }

        // Check effective outer replicates
        if (tvals.size() < m_B_outer / 2)
	  {
            throw std::runtime_error("PercentileTBootstrap: too many degenerate outer replicates");
	  }

        // SE_hat from outer theta* spread
        double sumT = 0.0, sumT2 = 0.0;
        for (const auto& th : thetas_outer)
   {
            const double v = num::to_double(th);
            sumT  += v;
            sumT2 += v * v;
	  }
        const double effB = static_cast<double>(thetas_outer.size());
        const double meanT = sumT / effB;
        const double varT  = std::max(0.0, (sumT2 / effB) - (meanT * meanT));
        const double se_hat = std::sqrt(varT);

        // Guard against zero/NaN SE_hat; if it happens, CI collapses at mean.
        const double SE = (se_hat > 0.0 && std::isfinite(se_hat)) ? se_hat : 0.0;

        // t-quantiles (type-7) from the collected pivots
        std::sort(tvals.begin(), tvals.end());
        const double alpha = 1.0 - m_CL;
        const double t_lo = static_cast<double>(quantile_type7_sorted(tvals, alpha / 2.0));
        const double t_hi = static_cast<double>(quantile_type7_sorted(tvals, 1.0 - alpha / 2.0));

        // Build CI
        const double theta_hat_d = num::to_double(theta_hat);
        const double lb = theta_hat_d - t_hi * SE;
        const double ub = theta_hat_d - t_lo * SE;

        return Result{
	  /*mean               =*/ theta_hat,
	  /*lower              =*/ Decimal(lb),
	  /*upper              =*/ Decimal(ub),
	  /*cl                 =*/ m_CL,
	  /*B_outer            =*/ m_B_outer,
	  /*B_inner            =*/ m_B_inner,
	  /*effective_B        =*/ tvals.size(),
	  /*skipped_outer      =*/ skipped_outer,
	  /*skipped_inner_total=*/ skipped_inner_total,
	  /*n                  =*/ n,
	  /*m_outer            =*/ m_outer,
	  /*m_inner            =*/ m_inner,
	  /*L                  =*/ m_resampler.getL(),
	  /*se_hat             =*/ SE
        };
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
