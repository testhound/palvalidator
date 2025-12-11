#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <limits>
#include <memory>
#include "randutils.hpp"
#include "RngUtils.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
#include "AdaptiveRatioInternal.h"
#include "AdaptiveRatioPolicies.h"

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
	      class Rng      = std::mt19937_64,
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
	double      computed_ratio;    // logical ratio reported to callers
      };

    public:
      // ======================================================================
      // CONSTRUCTOR 1: Fixed Ratio
      // ======================================================================
      MOutOfNPercentileBootstrap(std::size_t B,
				 double      confidence_level,
				 double      m_ratio,
				 const Resampler& resampler)
	: m_B(B)
	, m_CL(confidence_level)
	, m_ratio(m_ratio)
	, m_resampler(resampler)
	, m_exec(std::make_shared<Executor>())
	, m_chunkHint(0)
	, m_ratioPolicy(nullptr)
      {
	validateParameters();
	if (!(m_ratio > 0.0 && m_ratio < 1.0))
	  {
	    throw std::invalid_argument("MOutOfNPercentileBootstrap: m_ratio must be in (0,1)");
	  }
      }

      // ======================================================================
      // CONSTRUCTOR 2: Adaptive Ratio (TailVolatilityAdaptivePolicy)
      // ======================================================================
      template<typename BootstrapStatistic>
      MOutOfNPercentileBootstrap(std::size_t B,
				 double      confidence_level,
				 const Resampler& resampler,
				 typename std::enable_if<
				 !std::is_floating_point<BootstrapStatistic>::value,
				 int>::type = 0)
	: m_B(B)
	, m_CL(confidence_level)
	, m_ratio(-1.0)  // sentinel: adaptive mode
	, m_resampler(resampler)
	, m_exec(std::make_shared<Executor>())
	, m_chunkHint(0)
	, m_ratioPolicy(std::make_shared<
			TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic>>())
      {
	validateParameters();
      }

      // ======================================================================
      // RUN METHODS
      // ======================================================================
      Result run(const std::vector<Decimal>& x,
		 Sampler                      sampler,
		 Rng&                         rng,
		 std::size_t                  m_sub_override = 0,
		 std::ostream*                diagnosticLog  = nullptr) const
      {
	// Derive per-replicate engine from supplied RNG
	auto make_engine = [&rng](std::size_t /*b*/) {
	  const uint64_t seed = mkc_timeseries::rng_utils::get_random_value(rng);
	  auto seq = mkc_timeseries::rng_utils::make_seed_seq(seed);
	  return mkc_timeseries::rng_utils::construct_seeded_engine<Rng>(seq);
	};

	return run_core_(x, sampler, m_sub_override, make_engine, diagnosticLog);
      }

      template <class Provider>
      Result run(const std::vector<Decimal>& x,
		 Sampler                      sampler,
		 const Provider&              provider,
		 std::size_t                  m_sub_override = 0,
		 std::ostream*                diagnosticLog  = nullptr) const
      {
	auto make_engine = [&provider](std::size_t b) {
	  // CRN: 1 engine per replicate index
	  return provider.make_engine(b);
	};

	return run_core_(x, sampler, m_sub_override, make_engine, diagnosticLog);
      }

      // ======================================================================
      // EXECUTION 3: Advanced Refinement (NEW - Two-Tier API)
      // ======================================================================
      /**
       * @brief Execute the bootstrap with adaptive ratio calculation and refinement.
       *
       * This method enables the stability-based refinement stage and requires
       * dependencies for Common Random Numbers (CRN) and probe engine creation.
       * This will call the policy's computeRatioWithRefinement method.
       *
       * @tparam BootstrapStatistic The statistic functor type
       * @tparam StrategyT Strategy object type for CRN hashing
       * @tparam BootstrapFactoryT Bootstrap factory type for creating probe engines
       *
       * @param x The input data
       * @param sampler The bootstrap statistic
       * @param strategy Strategy object for CRN hashing
       * @param factory Bootstrap factory for creating probe engines
       * @param stageTag CRN stage identifier
       * @param fold CRN fold identifier
       * @param diagnosticLog Optional output stream for diagnostics
       * @return Result with computed confidence interval and refined ratio
       */
      template <typename BootstrapStatistic, typename StrategyT, typename BootstrapFactoryT>
      Result runWithRefinement(
			       const std::vector<Decimal>& x,
			       Sampler sampler,
			       StrategyT& strategy,
			       BootstrapFactoryT& factory,
			       int stageTag,
			       int fold,
			       std::ostream* diagnosticLog = nullptr) const
      {
	const std::size_t n = x.size();
	if (n < 3)
	  {
	    throw std::invalid_argument("MOutOfNPercentileBootstrap::runWithRefinement: n must be >= 3");
	  }

	// 1. Compute Statistical Context
	detail::StatisticalContext<Decimal> ctx(x);

	// 2. Setup Probe Maker (capturing CRN state)
	// L_small: Use the resampler's block length for probe engines
	const std::size_t L_small = m_resampler.getL();

	detail::ConcreteProbeEngineMaker<Decimal, BootstrapStatistic,
					 StrategyT, BootstrapFactoryT, Resampler>
	  probeMaker(strategy, factory, stageTag, fold,
		     m_resampler, L_small, m_CL);

	// 3. Resolve Ratio using Refinement Policy
	//    Note: The policy internally clamps before and after refinement
	double actual_ratio;
	
	if (!m_ratioPolicy)
	  {
	    // No policy set, use default TailVolatilityAdaptivePolicy with refinement
	    TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic> defaultPolicy;
	    actual_ratio = defaultPolicy.computeRatioWithRefinement(
								    x, ctx, m_CL, m_B, probeMaker, diagnosticLog);
	  }
	else
	  {
	    // Use the configured policy
	    auto policy = std::static_pointer_cast<
	      IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>>(m_ratioPolicy);
	    
	    if (policy)
	      {
		actual_ratio = policy->computeRatioWithRefinement(
								  x, ctx, m_CL, m_B, probeMaker, diagnosticLog);
	      }
	    else
	      {
		// Fallback to default policy
		TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic> defaultPolicy;
		actual_ratio = defaultPolicy.computeRatioWithRefinement(
									x, ctx, m_CL, m_B, probeMaker, diagnosticLog);
	      }
	  }

	// 4. Compute m_sub and clamp to valid range (defensive final check)
	//    This should be redundant since the policy already clamped, but ensures safety
	std::size_t m_sub = static_cast<std::size_t>(std::floor(actual_ratio * n));
	if (m_sub < 2) m_sub = 2;
	if (m_sub >= n) m_sub = n - 1;
	actual_ratio = static_cast<double>(m_sub) / static_cast<double>(n);

	// 5. Create CRN provider for main bootstrap execution
	auto [mainEngine, crnProvider] = factory.template makeMOutOfN<Decimal, BootstrapStatistic, Resampler>(
													      m_B, m_CL, actual_ratio, m_resampler, strategy, stageTag, static_cast<int>(L_small), fold);

	// 6. Execute main bootstrap with refined ratio
	auto make_engine = [&crnProvider](std::size_t b) {
	  return crnProvider.make_engine(b);
	};

	// Use run_core_ with the computed m_sub
	return run_core_(x, sampler, m_sub, make_engine, diagnosticLog);
      }

      // ======================================================================
      // POLICY CONFIGURATION
      // ======================================================================
      template<typename BootstrapStatistic>
      void setAdaptiveRatioPolicy(
				  std::shared_ptr<IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>> policy)
      {
	if (!policy)
	  throw std::invalid_argument("Policy cannot be null");

	m_ratioPolicy = std::static_pointer_cast<void>(policy);
	m_ratio       = -1.0;  // switch to adaptive mode
      }

      bool isAdaptiveMode() const { return m_ratio < 0.0; }

      void setChunkSizeHint(uint32_t c) { m_chunkHint = c; }

      // Introspection
      std::size_t B()        const { return m_B; }
      double      CL()       const { return m_CL; }
      double      mratio()   const { return m_ratio; }
      const Resampler& resampler() const { return m_resampler; }

    private:
      // ======================================================================
      // INTERNAL HELPERS
      // ======================================================================
      void validateParameters() const
      {
	if (m_B < 400)
	  {
	    throw std::invalid_argument("MOutOfNPercentileBootstrap: B should be >= 400");
	  }
	if (!(m_CL > 0.5 && m_CL < 1.0))
	  {
	    throw std::invalid_argument("MOutOfNPercentileBootstrap: CL must be in (0.5,1)");
	  }
      }

      // Unsorted type-7 quantile via nth_element
      static double quantile_type7_via_nth(const std::vector<double>& s, double p)
      {
	if (s.empty())
	  throw std::invalid_argument("quantile_type7_via_nth: empty input");
	if (p <= 0.0)
	  return *std::min_element(s.begin(), s.end());
	if (p >= 1.0)
	  return *std::max_element(s.begin(), s.end());

	const double nd = static_cast<double>(s.size());
	const double h  = (nd - 1.0) * p + 1.0;
	std::size_t i1  = static_cast<std::size_t>(std::floor(h));
	if (i1 < 1)         i1 = 1;
	if (i1 >= s.size()) i1 = s.size() - 1;
	const double frac = h - static_cast<double>(i1);

	std::vector<double> w0(s.begin(), s.end());
	std::nth_element(w0.begin(),
			 w0.begin() + static_cast<std::ptrdiff_t>(i1 - 1),
			 w0.end());
	const double x0 = w0[i1 - 1];

	std::vector<double> w1(s.begin(), s.end());
	std::nth_element(w1.begin(),
			 w1.begin() + static_cast<std::ptrdiff_t>(i1),
			 w1.end());
	const double x1 = w1[i1];

	return x0 + (x1 - x0) * frac;
      }

      // ======================================================================
      // CORE BOOTSTRAP IMPLEMENTATION
      // ======================================================================
      template <class EngineMaker>
      Result run_core_(const std::vector<Decimal>& x,
		       Sampler                      sampler,
		       std::size_t                  m_sub_override,
		       EngineMaker&&                make_engine,
		       std::ostream*                diagnosticLog = nullptr) const
      {
	const std::size_t n = x.size();
	if (n < 3)
	  {
	    throw std::invalid_argument("MOutOfNPercentileBootstrap: n must be >= 3");
	  }

	// Determine m_sub and ratios
	std::size_t m_sub;
	double      actual_ratio;    // ratio used internally to derive m_sub
	double      reported_ratio;  // ratio exposed via Result::computed_ratio

	if (m_sub_override > 0)
	  {
	    // Explicit override takes precedence
	    m_sub         = m_sub_override;
	    actual_ratio  = static_cast<double>(m_sub) / static_cast<double>(n);
	    reported_ratio = actual_ratio;  // tests expect override ratio = m_sub/n
	  }
	else if (isAdaptiveMode())
	  {
	    // Adaptive calculation via policy
	    if (!m_ratioPolicy)
	      throw std::runtime_error("Adaptive mode enabled but no policy set");

	    detail::StatisticalContext<Decimal> ctx(x);
	    actual_ratio = computeAdaptiveRatio(x, ctx, diagnosticLog);
	    m_sub        = static_cast<std::size_t>(std::floor(actual_ratio * n));
	    reported_ratio = actual_ratio;  // tests check m_sub == floor(computed_ratio*n)
	  }
	else
	  {
	    // Fixed ratio mode
	    m_sub        = static_cast<std::size_t>(std::floor(m_ratio * static_cast<double>(n)));
	    actual_ratio = static_cast<double>(m_sub) / static_cast<double>(n);
	    reported_ratio = m_ratio;  // report configured fixed ratio, not m_sub/n
	  }

	// Clamp to valid range [2, n-1]
	if (m_sub < 2)
	  m_sub = 2;
	if (m_sub >= n)
	  m_sub = n - 1;
	// NOTE: we deliberately do NOT alter reported_ratio here; it represents
	//       the logical target ratio, not necessarily m_sub/n in all modes.

	const Decimal theta_hat = sampler(x);

	// Pre-allocate; NaN marks skipped/invalid replicates
	std::vector<double> thetas_d(m_B, std::numeric_limits<double>::quiet_NaN());

	// Parallel over B using the internally default-constructed Executor
	concurrency::parallel_for_chunked(
					  static_cast<uint32_t>(m_B),
					  *m_exec,
					  [&](uint32_t b) {
					    auto rng = make_engine(b);
					    std::vector<Decimal> y;
					    y.resize(m_sub);
					    auto rng_copy = rng;
					    m_resampler(x, y, m_sub, rng_copy);
					    const double v = num::to_double(sampler(y));
					    if (std::isfinite(v))
					      thetas_d[b] = v;
					  },
					  /*chunkSizeHint=*/m_chunkHint);

	// Compact NaNs and compute skipped count
	std::size_t skipped = 0;
	{
	  auto it = std::remove_if(
				   thetas_d.begin(),
				   thetas_d.end(),
				   [](double v) { return !std::isfinite(v); });
	  skipped = static_cast<std::size_t>(std::distance(it, thetas_d.end()));
	  thetas_d.erase(it, thetas_d.end());
	}

	if (thetas_d.size() < m_B / 2)
	  {
	    throw std::runtime_error(
				     "MOutOfNPercentileBootstrap: too many degenerate replicates");
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
	  /*L           =*/ m_resampler.getL(),
	  /*computed_ratio =*/ reported_ratio
	};
      }

      // ======================================================================
      // ADAPTIVE RATIO DISPATCH
      // ======================================================================
      /**
       * Helper to compute adaptive ratio using type-erased policy.
       */
      template<typename BootstrapStatistic = Sampler>
      double computeAdaptiveRatio(const std::vector<Decimal>&                x,
				  const detail::StatisticalContext<Decimal>& ctx,
				  std::ostream*                              diagnosticLog) const
      {
	// Try to cast back to the correct policy type
	auto policy = std::static_pointer_cast<
	  IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>>(m_ratioPolicy);

	if (policy)
	  {
	    return policy->computeRatio(x, ctx, m_CL, m_B, diagnosticLog);
	  }

	// Fallback: default TailVolatilityAdaptivePolicy
	TailVolatilityAdaptivePolicy<Decimal, BootstrapStatistic> defaultPolicy;
	return defaultPolicy.computeRatio(x, ctx, m_CL, m_B, diagnosticLog);
      }

    private:
      std::size_t  m_B;
      double       m_CL;
      double       m_ratio;       // -1.0 = adaptive mode, else fixed ratio
      Resampler    m_resampler;
      mutable std::shared_ptr<Executor> m_exec;
      mutable uint32_t           m_chunkHint{0};
      std::shared_ptr<void>      m_ratioPolicy;  // type-erased policy pointer
    };
  }
} // namespace palvalidator::analysis
