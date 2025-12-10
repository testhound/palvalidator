// PercentileTBootstrap.h
// Studentized percentile-t bootstrap with composable resampler and two run paths:
//  (1) caller-supplied RNG reference
//  (2) CRN/engine-provider supplying a deterministic engine per outer replicate
//
// Shared core is factored in run_impl(... EngineMaker ...).
//
// CI construction (per-period scale):
//   1) θ̂ = sampler(x)
//   2) Outer reps b = 1..B_outer:
//        y_outer ~ resampler(x, m_outer, rng_b)
//        θ*_b    = sampler(y_outer)
//        Inner reps j = 1..B_inner:
//          y_inner ~ resampler(y_outer, m_inner, rng_b)
//          θ°_bj   = sampler(y_inner)
//        SE*_b = sd({θ°_bj})
//        t_b  = (θ*_b − θ̂)/SE*_b
//      Keep finite t_b and θ*_b with SE*_b>0
//   3) SE_hat = sd({θ*_b}) over valid outer reps
//   4) t_lo, t_hi = type-7 quantiles of {t_b} at α/2 and 1−α/2 (α=1−CL)
//   5) CI = [θ̂ − t_hi*SE_hat,  θ̂ − t_lo*SE_hat]
//
// Notes:
// - Default RNG is std::mt19937_64 (aligns with your CRN classes).
// - The provider overload expects an object with:  Rng make_engine(std::size_t b) const;
//
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <functional>

#include "number.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "RngUtils.h"

namespace palvalidator
{
  namespace analysis
  {

    template<
      class Decimal,
      class Sampler,
      class Resampler,
      class Rng = std::mt19937_64,
      class Executor = concurrency::SingleThreadExecutor
      >
    class PercentileTBootstrap {
    public:
      struct Result {
	Decimal     mean;               // θ̂ on original sample
	Decimal     lower;              // lower CI (per-period)
	Decimal     upper;              // upper CI (per-period)
	double      cl;                 // confidence level
	std::size_t B_outer;            // requested outer reps
	std::size_t B_inner;            // requested inner reps
	std::size_t effective_B;        // usable outer reps (finite pivots)
	std::size_t skipped_outer;      // outer reps skipped (degenerate θ* / SE*)
	std::size_t skipped_inner_total;// total degenerate inner reps
	std::size_t n;                  // original sample size
	std::size_t m_outer;            // outer subsample size
	std::size_t m_inner;            // inner subsample size
	std::size_t L;                  // resampler L (diagnostic)
	double      se_hat;             // sd(θ*) over effective outer reps
      };

    public:
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
	if (m_B_outer < 400)  throw std::invalid_argument("PercentileTBootstrap: B_outer must be >= 400");
	if (m_B_inner < 100)  throw std::invalid_argument("PercentileTBootstrap: B_inner must be >= 100");
	if (!(m_CL > 0.5 && m_CL < 1.0)) throw std::invalid_argument("PercentileTBootstrap: CL must be in (0.5,1)");
	if (!(m_ratio_outer > 0.0 && m_ratio_outer <= 1.0)) throw std::invalid_argument("m_ratio_outer must be in (0,1]");
	if (!(m_ratio_inner > 0.0 && m_ratio_inner <= 1.0)) throw std::invalid_argument("m_ratio_inner must be in (0,1]");
      }

      // (A) Run with caller-provided RNG (non-CRN path)
      Result run(const std::vector<Decimal>& x,
		 Sampler                      sampler,
		 Rng&                         rng,
		 std::size_t                  m_outer_override = 0,
		 std::size_t                  m_inner_override = 0) const
      {
	auto engine_maker = [&](std::size_t /*b*/) -> Rng {
	  // Derive per-outer engines deterministically from the caller RNG.
	  // Use two 64-bit draws → seed_seq (must be lvalue in ctor).
	  const uint64_t s1 = mkc_timeseries::rng_utils::get_random_value(rng);
	  const uint64_t s2 = mkc_timeseries::rng_utils::get_random_value(rng);
	  std::seed_seq seq{
	    static_cast<uint32_t>(s1),
	    static_cast<uint32_t>(s1 >> 32),
	    static_cast<uint32_t>(s2),
	    static_cast<uint32_t>(s2 >> 32)
	  };
	  return Rng(seq);
	};
	return run_impl(x, std::move(sampler), m_outer_override, m_inner_override, engine_maker);
      }

      // (B) Run with a CRN/engine-provider (order/thread independent)
      // Provider concept:  Rng make_engine(std::size_t b) const;
      template<class Provider>
      Result run(const std::vector<Decimal>& x,
		 Sampler                      sampler,
		 const Provider&              provider,
		 std::size_t                  m_outer_override = 0,
		 std::size_t                  m_inner_override = 0) const
      {
	auto engine_maker = [&](std::size_t b) -> Rng {
	  return provider.make_engine(b); // by value
	};
	return run_impl(x, std::move(sampler), m_outer_override, m_inner_override, engine_maker);
      }

    private:
      template<class EngineMaker>
      Result run_impl(const std::vector<Decimal>& x,
		      Sampler                      sampler,
		      std::size_t                  m_outer_override,
		      std::size_t                  m_inner_override,
		      EngineMaker&&                make_engine) const
      {
	const std::size_t n = x.size();
	if (n < 3) throw std::invalid_argument("PercentileTBootstrap.run: n must be >= 3");

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

	// Baseline statistic
	const Decimal theta_hat   = sampler(x);
	const double  theta_hat_d = num::to_double(theta_hat);

	// Output buffers
	std::vector<double> theta_star_ds(m_B_outer, std::numeric_limits<double>::quiet_NaN());
	std::vector<double> tvals         (m_B_outer, std::numeric_limits<double>::quiet_NaN());

	// Diagnostics
	std::atomic<std::size_t> skipped_outer{0};
	std::atomic<std::size_t> skipped_inner_total{0};

	const std::size_t Ldiag = m_resampler.getL();

	// Parallelize outer loop only
	Executor exec{};
	concurrency::parallel_for_chunked(static_cast<uint32_t>(m_B_outer), exec,
					  [&](uint32_t b32)
					  {
					    const std::size_t b = static_cast<std::size_t>(b32);
					    Rng rng_b = make_engine(b); // fresh engine per outer replicate

					    std::vector<Decimal> y_outer(m_outer);
					    std::vector<Decimal> y_inner(m_inner);

					    // OUTER resample
					    m_resampler(x, y_outer, m_outer, rng_b);

					    // θ* on OUTER
					    const Decimal theta_star   = sampler(y_outer);
					    const double  theta_star_d = num::to_double(theta_star);
					    if (!std::isfinite(theta_star_d)) {
					      skipped_outer.fetch_add(1, std::memory_order_relaxed);
					      return;
					    }

					    // Inner loop: SE* with adaptive stabilization
					    double mean = 0.0, m2 = 0.0;
					    std::size_t eff_inner = 0;

					    auto push_inner = [&](double v) noexcept {
					      ++eff_inner;
					      const double delta = v - mean;
					      mean += delta / static_cast<double>(eff_inner);
					      m2   += delta * (v - mean);
					    };

					    constexpr std::size_t MIN_INNER   = 100;
					    constexpr std::size_t CHECK_EVERY = 16;
					    constexpr double      REL_EPS     = 0.015;

					    double last_se = std::numeric_limits<double>::infinity();

					    for (std::size_t j = 0; j < m_B_inner; ++j) {
					      m_resampler(y_outer, y_inner, m_inner, rng_b);
					      const double v = num::to_double(sampler(y_inner));
					      if (!std::isfinite(v)) {
						skipped_inner_total.fetch_add(1, std::memory_order_relaxed);
						continue;
					      }
					      push_inner(v);

					      if (eff_inner >= MIN_INNER && ((eff_inner % CHECK_EVERY) == 0)) {
						const double se_now = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner)));
						if (std::isfinite(se_now) &&
						    std::fabs(se_now - last_se) <= REL_EPS * std::max(se_now, 1e-300)) {
						  break; // stabilized
						}
						last_se = se_now;
					      }
					    }

					    if (eff_inner < MIN_INNER) {
					      skipped_outer.fetch_add(1, std::memory_order_relaxed);
					      return;
					    }

					    const double se_star = std::sqrt(std::max(0.0, m2 / static_cast<double>(eff_inner)));
					    if (!(se_star > 0.0) || !std::isfinite(se_star)) {
					      skipped_outer.fetch_add(1, std::memory_order_relaxed);
					      return;
					    }

					    const double t_b = (theta_star_d - theta_hat_d) / se_star;

					    theta_star_ds[b] = theta_star_d;
					    tvals[b]         = t_b;
					  });

	// Collect effective outer replicates
	std::vector<double> t_eff;     t_eff.reserve(m_B_outer);
	std::vector<double> theta_eff; theta_eff.reserve(m_B_outer);

	for (std::size_t b = 0; b < m_B_outer; ++b) {
	  const double tb = tvals[b];
	  const double th = theta_star_ds[b];
	  if (std::isfinite(tb) && std::isfinite(th)) {
	    t_eff.push_back(tb);
	    theta_eff.push_back(th);
	  }
	}

	const std::size_t effective_B = t_eff.size();
	if (effective_B < 16) {
	  throw std::runtime_error("PercentileTBootstrap.run: too few finite studentized pivots");
	}

	// SE_hat = sd(θ*) across valid outer reps
	double se_hat;
	{
	  double sum = 0.0, sum2 = 0.0;
	  for (double v : theta_eff) { sum += v; sum2 += v*v; }
	  const double m = static_cast<double>(theta_eff.size());
	  const double var = std::max(0.0, (sum2 / m) - (sum / m)*(sum / m));
	  se_hat = std::sqrt(var);
	}

	// Type-7 quantiles via nth_element
	auto type7_quantile = [](std::vector<double> v, double p) -> double {
	  const std::size_t m = v.size();
	  if (m == 0) return std::numeric_limits<double>::quiet_NaN();
	  if (m == 1) return v[0];

	  const double h = (m - 1) * p;
	  const std::size_t k = static_cast<std::size_t>(std::floor(h));
	  const double frac = h - static_cast<double>(k);

	  std::nth_element(v.begin(), v.begin() + k, v.end());
	  const double vk = v[k];
	  if (frac == 0.0 || k + 1 == m) return vk;

	  std::nth_element(v.begin() + k + 1, v.begin() + k + 1, v.end());
	  const double vkp1 = v[k + 1];
	  return vk + frac * (vkp1 - vk);
	};

	const double alpha = 1.0 - m_CL;
	const double t_lo  = type7_quantile(t_eff, alpha / 2.0);
	const double t_hi  = type7_quantile(t_eff, 1.0 - alpha / 2.0);

	const double lower_d = theta_hat_d - t_hi * se_hat;
	const double upper_d = theta_hat_d - t_lo * se_hat;

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

    template <class Decimal,
	      class Sampler, // Sampler here corresponds to Resampler in BCa
	      class Rng      = std::mt19937_64,
	      class Provider = void>
    class BCaCompatibleTBootstrap
    {
    public:
      using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

      // Must match the BCaBootStrap constructor signature!
      template <class P = Provider, std::enable_if_t<!std::is_void_v<P>, int> = 0>
      BCaCompatibleTBootstrap(const std::vector<Decimal>& returns,
			      unsigned int num_resamples, // B_outer
			      double confidence_level,
			      StatFn statistic,           // Sampler for PTB
			      Sampler sampler,            // Resampler for PTB
			      const P& provider)
	// Use the statistic (which is a StatFn) as the Sampler type in the inner PTB
	: m_internal_pt(num_resamples, m_B_inner_default, confidence_level, std::move(sampler), 1.0, 1.0)
	, m_returns(returns)
	, m_statistic(std::move(statistic))
	, m_provider(provider)
      {
	if (m_returns.empty() || num_resamples < 100u || confidence_level <= 0.0 || confidence_level >= 1.0)
	  {
	    throw std::invalid_argument("BCaCompatibleTBootstrap: Invalid construction arguments.");
	  }
      }

      // Public BCa-compatible interface
      Decimal getLowerBound()
      {
 ensureCalculated();
 return m_cached_result.value().lower;
      }

      Decimal getUpperBound()
      {
 ensureCalculated();
 return m_cached_result.value().upper;
      }
    
      Decimal getStatistic()
      {
 ensureCalculated();
 return m_cached_result.value().mean;
      }

    private:
      // Constants and members
      static constexpr std::size_t m_B_inner_default = 200; // Recommend >100 for stability
      PercentileTBootstrap<Decimal, StatFn, Sampler, Rng> m_internal_pt;
      const std::vector<Decimal>& m_returns;
      StatFn                      m_statistic;
      Provider                    m_provider;  // Store by value, not reference
    
      std::optional<typename PercentileTBootstrap<Decimal, StatFn, Sampler, Rng>::Result> m_cached_result;

      // Lazy calculation
      void ensureCalculated()
      {
 if (!m_cached_result.has_value()) {
   m_cached_result = m_internal_pt.run(m_returns, m_statistic, m_provider);
 }
      }
    };
  } // namespace analysis
} // namespace palvalidator
