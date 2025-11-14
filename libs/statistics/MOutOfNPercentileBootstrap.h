#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include "randutils.hpp"
#include "RngUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Hyndman–Fan type-7 empirical quantile on a pre-sorted vector.
     *
     * Implements the default quantile definition used by many statistical packages
     * (R's type-7): for a sorted sample \f$x_{(1)} \le \dots \le x_{(n)}\f$ and
     * probability \f$p \in [0,1]\f$,
     * \f[
     *   h = (n-1)p + 1,\quad i = \lfloor h \rfloor,\quad \gamma = h - i,
     * \f]
     * and the quantile is
     * \f[
     *   Q_7(p) =
     *   \begin{cases}
     *     x_{(1)} & p \le 0,\\
     *     x_{(n)} & p \ge 1,\\
     *     (1-\gamma)\,x_{(i)} + \gamma\,x_{(i+1)} & \text{otherwise.}
     *   \end{cases}
     * \f]
     *
     * @tparam Decimal
     *   Numeric value type (e.g., dec::decimal<8>, double). Must support
     *   +, -, * by a double/Decimal and copying.
     *
     * @param sorted
     *   Input data sorted in ascending order. Must not be empty when @p p is in (0,1).
     * @param p
     *   Quantile in \f$[0,1]\f$. Values \f$\le 0\f$ clamp to the first element;
     *   values \f$\ge 1\f$ clamp to the last.
     *
     * @return Decimal
     *   The type-7 quantile at probability @p p.
     *
     * @throws std::invalid_argument
     *   If @p sorted is empty.
     *
     * @complexity
     *   \f$O(1)\f$ time and space (assuming random access).
     */
    template <class Decimal>
    inline Decimal quantile_type7_sorted(const std::vector<Decimal>& sorted, double p)
    {
      if (sorted.empty())
	{
	  throw std::invalid_argument("quantile_type7_sorted: empty input");
	}
      if (p <= 0.0) return sorted.front();
      if (p >= 1.0) return sorted.back();

      const double n = static_cast<double>(sorted.size());
      const double h = (n - 1.0) * p + 1.0;               // 1-based
      const std::size_t i = static_cast<std::size_t>(std::floor(h));  // 1..n-1 for p in (0,1)
      const double frac = h - static_cast<double>(i);     // in [0,1)

      // With p in (0,1), h is in (1, n), so i is in {1, ..., n-1}; no early return needed.
      // Interpolate between x[i-1] and x[i] (0-based indices).
      const Decimal x0 = sorted[i - 1];
      const Decimal x1 = sorted[i];
      return x0 + (x1 - x0) * Decimal(frac);
    }

    /**
     * @brief m-out-of-n percentile bootstrap (stationary-block resampling aware).
     *
     * This class performs a conservative percentile bootstrap by drawing
     * \f$m=\lfloor \rho n \rfloor\f$ observations (with replacement via a provided
     * resampler) from the original sample of length \f$n\f$ for each of \f$B\f$ replicates,
     * computing a user-supplied statistic \f$\theta(\cdot)\f$ on each subsample, and
     * returning a \f$100 \cdot \mathrm{CL}\%\f$ confidence interval using type-7
     * quantiles of the bootstrap distribution \f$\{\theta^\*_b\}\f$.
     *
     * Design highlights:
     * - **Composable resampler**: Inject any resampler implementing
     *   `resampler(x, y, m, rng)` and `getL()`, e.g., a stationary block value resampler,
     *   enabling dependence-aware draws and synchronized resampling across strategies.
     * - **Small-n friendly**: Picking \f$m < n\f$ often improves coverage for small samples
     *   by reducing the influence of single outliers.
     * - **Numerically robust**: Degenerate/NaN replicates are skipped; too many degenerates
     *   raise an error to avoid misleading intervals.
     *
     * @tparam Decimal
     *   Numeric value type (e.g., dec::decimal<8>).
     * @tparam Sampler
     *   Callable with signature `Decimal(const std::vector<Decimal>&)` that computes the
     *   statistic of interest on a series (e.g., GeoMeanStat).
     * @tparam Resampler
     *   Type that provides `void operator()(const std::vector<Decimal>& x,
     *   std::vector<Decimal>& y, std::size_t m, Rng& rng) const;` and `std::size_t getL() const;`.
     * @tparam Rng
     *   Random-number generator type. Defaults to `randutils::mt19937_rng`.
     */
    template <class Decimal,
	      class Sampler,
	      class Resampler,
	      class Rng = std::mt19937_64,
	      class Executor = concurrency::SingleThreadExecutor>
    class MOutOfNPercentileBootstrap
    {
    public:
      struct Result
      {
        Decimal     mean;              // stat on original sample
        Decimal     lower;             // percentile lower bound
        Decimal     upper;             // percentile upper bound
        double      cl;
        std::size_t B;
        std::size_t effective_B;
        std::size_t skipped;
        std::size_t n;
        std::size_t m_sub;
        std::size_t L;
      };

    public:
      /**
       * @brief Construct an m-out-of-n percentile bootstrap engine.
       *
       * @param B
       *   Number of bootstrap replicates (\f$B \ge 400\f$ recommended for stable quantiles).
       * @param confidence_level
       *   Confidence level \f$\mathrm{CL}\in(0.5,1)\f$ (e.g., 0.95).
       * @param m_ratio
       *   Subsampling ratio \f$\rho\in(0,1)\f$; the subsample size is \f$m=\lfloor \rho n \rfloor\f$.
       *   (Clamped to \f$[2, n-1]\f$ inside @ref run.)
       * @param resampler
       *   Resampler instance used to generate each length-\f$m\f$ bootstrap sample.
       *
       * @throws std::invalid_argument
       *   If @p B < 400, or @p confidence_level not in (0.5,1), or @p m_ratio not in (0,1).
       */
      MOutOfNPercentileBootstrap(std::size_t B,
				 double      confidence_level,
				 double      m_ratio,
				 const Resampler& resampler)
	: m_B(B),
	  m_CL(confidence_level),
	  m_ratio(m_ratio),
	  m_resampler(resampler),
	  m_exec(std::make_shared<Executor>()),
	  m_chunkHint(0)
      {
        if (m_B < 400)
	  {
            throw std::invalid_argument("MOutOfNPercentileBootstrap: B should be >= 400");
	  }
        if (!(m_CL > 0.5 && m_CL < 1.0))
	  {
            throw std::invalid_argument("MOutOfNPercentileBootstrap: CL must be in (0.5,1)");
	  }
        if (!(m_ratio > 0.0 && m_ratio < 1.0))
	  {
            throw std::invalid_argument("MOutOfNPercentileBootstrap: m_ratio must be in (0,1)");
	  }
      }

      /**
       * @brief Execute the bootstrap and form a percentile confidence interval.
       *
       * For each replicate \f$b=1,\dots,B\f$:
       * 1. Draw a resample of length \f$m = \lfloor \rho n \rfloor\f$ (or @p m_sub_override if set)
       *    using the injected @p m_resampler.
       * 2. Compute \f$\theta^\*_b = \mathrm{sampler}(y_b)\f$.
       * 3. Skip if \f$\theta^\*_b\f$ is non-finite; otherwise retain it.
       *
       * After the loop, sort the retained \f$\{\theta^\*_b\}\f$ and take type-7 quantiles at
       * \f$\alpha/2\f$ and \f$1-\alpha/2\f$ where \f$\alpha = 1-\mathrm{CL}\f$.
       *
       * @param x
       *   Original sample (length \f$n \ge 3\f$).
       * @param sampler
       *   Callable that computes the statistic of interest on a series.
       * @param rng
       *   Random number generator (forwarded to the resampler).
       * @param m_sub_override
       *   Optional explicit subsample size \f$m\in[2,n-1]\f$. If 0 (default), compute from @p m_ratio.
       *
       * @return Result
       *   Struct containing \f$\hat\theta\f$, lower/upper bounds, diagnostics, and bookkeeping.
       *
       * @throws std::invalid_argument
       *   If \f$n<3\f$, or \f$m<2\f$ after clamping, or @p m_sub_override \f\ge n\f.
       * @throws std::runtime_error
       *   If fewer than \f$B/2\f$ valid replicates remain (too many degenerates).
       *
       * @complexity
       *   Time: \f$O(B \cdot T_\mathrm{resample} + B \log B)\f$ (sort); Space: \f$O(B)\f$.
       *
       * @note
       *   - This method returns percentile intervals (no bias correction or acceleration).
       *   - For highly skewed statistics or very small \f$n\f$, consider Studentized (percentile-t)
       *     intervals or BCa; this class is meant as a conservative, simple alternative,
       *     especially when used with an “m-out-of-n” (\f$m<n\f$) regime.
       */
      Result run(const std::vector<Decimal>& x,
		 Sampler sampler,
		 Rng& rng,
		 std::size_t m_sub_override = 0) const
      {
	// Derive a per-replicate engine from the supplied RNG (works for std engines AND randutils)
	auto make_engine = [&rng](std::size_t /*b*/) {
	  const uint64_t seed = mkc_timeseries::rng_utils::get_random_value(rng);
	  auto seq = mkc_timeseries::rng_utils::make_seed_seq(seed);
	  return mkc_timeseries::rng_utils::construct_seeded_engine<Rng>(seq);
	};

	return run_core_(x, sampler, m_sub_override, make_engine);
      }

      template <class Provider>
      Result run(const std::vector<Decimal>& x,
		 Sampler                      sampler,
		 const Provider&              provider,
		 std::size_t                  m_sub_override = 0) const
      {
	auto make_engine = [&provider](std::size_t b) {
	  return provider.make_engine(b);  // CRN: 1 engine per replicate index
	};
	
	return run_core_(x, sampler, m_sub_override, make_engine);
      }

      void setChunkSizeHint(uint32_t c)
      {
	m_chunkHint = c;
      }

      // Introspection helpers
      std::size_t B()        const { return m_B; }
      double      CL()       const { return m_CL; }
      double      mratio()   const { return m_ratio; }
      const Resampler& resampler() const { return m_resampler; }

    private:
      // Extracted: unsorted type-7 quantile via two nth_element passes
      static double quantile_type7_via_nth(const std::vector<double>& s, double p)
      {
	if (s.empty()) throw std::invalid_argument("quantile_type7_via_nth: empty input");
	if (p <= 0.0)  return *std::min_element(s.begin(), s.end());
	if (p >= 1.0)  return *std::max_element(s.begin(), s.end());

	const double nd = static_cast<double>(s.size());
	const double h  = (nd - 1.0) * p + 1.0;
	std::size_t i1  = static_cast<std::size_t>(std::floor(h));
	if (i1 < 1)         i1 = 1;
	if (i1 >= s.size()) i1 = s.size() - 1;
	const double frac = h - static_cast<double>(i1);

	std::vector<double> w0(s.begin(), s.end());
	std::nth_element(w0.begin(), w0.begin() + static_cast<std::ptrdiff_t>(i1 - 1), w0.end());
	const double x0 = w0[i1 - 1];

	std::vector<double> w1(s.begin(), s.end());
	std::nth_element(w1.begin(), w1.begin() + static_cast<std::ptrdiff_t>(i1), w1.end());
	const double x1 = w1[i1];

	return x0 + (x1 - x0) * frac;
      }

      template <class EngineMaker>
      Result run_core_(const std::vector<Decimal>& x,
		       Sampler                      sampler,
		       std::size_t                  m_sub_override,
		       EngineMaker&&                make_engine) const
      {
	const std::size_t n = x.size();
	if (n < 3) {
	  throw std::invalid_argument("MOutOfNPercentileBootstrap: n must be >= 3");
	}

	std::size_t m_sub = (m_sub_override > 0)
	  ? m_sub_override
	  : static_cast<std::size_t>(std::floor(m_ratio * static_cast<double>(n)));
	if (m_sub < 2)  m_sub = 2;
	if (m_sub >= n) m_sub = n - 1;

	const Decimal theta_hat = sampler(x);

	// Pre-allocate; NaN marks skipped/invalid replicates
	std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

	// Parallel over B using the internally default-constructed Executor
	concurrency::parallel_for_chunked(static_cast<uint32_t>(m_B), *m_exec,
						    [&](uint32_t b) {
						      auto rng = make_engine(b);
						      std::vector<Decimal> y; y.resize(m_sub);
						      m_resampler(x, y, m_sub, rng);
						      const double v = num::to_double(sampler(y));
						      if (std::isfinite(v)) thetas_d[b] = v;
						    },
						    /*chunkSizeHint=*/m_chunkHint
						    );

	// Compact NaNs and validate effective_B
	std::size_t skipped = 0;
	{
	  auto it = std::remove_if(thetas_d.begin(), thetas_d.end(),
				   [](double v){ return !std::isfinite(v); });
	  skipped = static_cast<std::size_t>(std::distance(it, thetas_d.end()));
	  thetas_d.erase(it, thetas_d.end());
	}
	if (thetas_d.size() < m_B / 2) {
	  throw std::runtime_error("MOutOfNPercentileBootstrap: too many degenerate replicates");
	}

		// Percentile CI (type-7) at CL
	const double alpha = 1.0 - m_CL;
	const double pl    = alpha / 2.0;
	const double pu    = 1.0 - alpha / 2.0;

	const double lb_d = quantile_type7_via_nth(thetas_d, pl);
	const double ub_d = quantile_type7_via_nth(thetas_d, pu);

	return Result{
	  /*mean        =*/ theta_hat,
	  /*lower       =*/ Decimal(lb_d),
	  /*upper       =*/ Decimal(ub_d),
	  /*cl          =*/ m_CL,
	  /*B           =*/ m_B,
	  /*effective_B =*/ thetas_d.size(),
	  /*skipped     =*/ skipped,
	  /*n           =*/ n,
	  /*m_sub       =*/ m_sub,
	  /*L           =*/ m_resampler.getL()
	};
      }
      
    private:
      std::size_t  m_B;
      double       m_CL;
      double       m_ratio;
      Resampler    m_resampler;
      mutable std::shared_ptr<Executor> m_exec; 
      mutable uint32_t m_chunkHint{0};
    };
  }
} // namespace palvalidator::analysis
