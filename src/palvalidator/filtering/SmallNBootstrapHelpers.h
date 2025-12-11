#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <string>
#include <ostream>
#include <optional>
#include <random>
#include <iomanip>
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "BootstrapConfig.h"
#include "Annualizer.h"
#include "StatUtils.h"

// New adaptive ratio infrastructure (Phase 5 refactoring)
#include "AdaptiveRatioInternal.h"
#include "AdaptiveRatioPolicies.h"
#include "MOutOfNPercentileBootstrap.h"

namespace palvalidator::bootstrap_helpers
{

  // -----------------------------------------------------------------------------
  // Forward declarations and helper utilities needed by template functions
  // -----------------------------------------------------------------------------

  /**
   * @brief Maps a two-sided confidence level to a Z-score (standard normal quantile).
   *
   * Used primarily to back-out an approximate standard deviation (Sigma) from
   * the width of a confidence interval for logging purposes.
   *
   * @param cl The confidence level (e.g., 0.95).
   * @return double The Z-score (e.g., 1.96). Defaults to 1.96 if CL is unrecognized.
   */
  inline double z_from_two_sided_CL(double cl)
  {
    // cl = 0.90 → z≈1.645, 0.95 → 1.960, 0.975 → 2.241 (rare), 0.99 → 2.576
    // Fallback to 1.96 if unrecognized.
    if (cl >= 0.989 && cl <= 0.991)
      return 2.576; // 99%
    
    if (cl >= 0.949 && cl <= 0.951)
      return 1.960; // 95%
    
    if (cl >= 0.899 && cl <= 0.901)
      return 1.645; // 90%
    
    if (cl >= 0.974 && cl <= 0.976)
      return 2.241; // ~97.5%
    
    return 1.960;
  }

  // tiny traits to detect availability of 'upper' member and getUpperBound()
  namespace detail
  {
    template <class T, class = void>
    struct has_member_upper : std::false_type {};
    template <class T>
    struct has_member_upper<T, std::void_t<decltype(std::declval<T>().upper)>> : std::true_type {};

    template <class T, class = void>
    struct has_getUpperBound : std::false_type {};
    template <class T>
    struct has_getUpperBound<T, std::void_t<decltype(std::declval<T>().getUpperBound())>> : std::true_type {};

    template <typename Stat, typename = void>
    struct has_isRatioStatistic : std::false_type {};

    template <typename Stat>
    struct has_isRatioStatistic<
        Stat,
        std::void_t<decltype(Stat::isRatioStatistic())>
    > : std::true_type {};

    template <typename Stat, bool Has = has_isRatioStatistic<Stat>::value>
    struct ratio_stat_flag
    {
        static constexpr bool value = false;
    };

    template <typename Stat>
    struct ratio_stat_flag<Stat, true>
    {
        static constexpr bool value = Stat::isRatioStatistic();
    };
  }

  // -----------------------------------------------------------------------------
  // Lightweight result carriers used by stages (kept simple on purpose)
  // -----------------------------------------------------------------------------

  /**
   * @brief Lightweight result container for a simple m-out-of-n bootstrap run.
   * @tparam Num The numeric type (e.g., double, decimal).
   */
  template<class Num>
  struct MNRunSimple
  {
    Num          lower{};         ///< Per-period lower bound (e.g., 5th percentile).
    std::size_t  m_sub{0};        ///< Subsample size used (m).
    std::size_t  L{0};            ///< Block length used.
    std::size_t  effective_B{0};  ///< Number of valid (non-degenerate) replicates generated.
  };

  /**
   * @brief Lightweight result container for a Percentile-t bootstrap run.
   * @tparam Num The numeric type.
   */
  template<class Num>
  struct PTRunSimple
  {
    Num          lower{};         ///< Per-period lower bound.
    std::size_t  m_outer{0};      ///< Outer loop subsample size.
    std::size_t  m_inner{0};      ///< Inner loop (variance estimation) subsample size.
    std::size_t  L{0};            ///< Block length used.
    std::size_t  effective_B{0};  ///< Number of valid replicates.
  };

  /**
   * @brief Encapsulates distributional characteristics of the return series for adaptive m/n decision-making.
   * @deprecated This class is superseded by palvalidator::analysis::detail::StatisticalContext.
   *             Keep for logging compatibility only.
   *
   * @details
   * This class acts as a Data Transfer Object (DTO) that carries all relevant statistical
   * properties of the input data (sample size, volatility, shape, and tail behavior) required
   * by the bootstrapping policies (e.g., `TailVolPriorPolicy`).
   *
   * By bundling these metrics, it allows policy classes to make informed decisions about:
   * - Whether the market is "Wild" (High Volatility / Heavy Tails) or "Stable".
   * - What the baseline subsampling ratio ($\rho = m/n$) should be.
   */
  class MNRatioContext
  {
  public:
    /**
     * @brief Constructs the context with calculated statistical metrics.
     *
     * @param n The sample size (number of observations).
     * @param sigmaAnn The annualized volatility (standard deviation).
     * @param skew The sample skewness.
     * @param exkurt The sample excess kurtosis.
     * @param tailIndex The estimated Pareto tail index ($\alpha$) via the Hill estimator.
     * Values $\le 0$ indicate an invalid or failed estimate.
     * @param heavyTails Boolean flag indicating if basic shape heuristics (skew/kurtosis)
     * detected heavy tails.
     */
    MNRatioContext(std::size_t n,
                   double      sigmaAnn,
                   double      skew,
                   double      exkurt,
                   double      tailIndex,
                   bool        heavyTails)
      : n_(n)
      , sigmaAnn_(sigmaAnn)
      , skew_(skew)
      , exkurt_(exkurt)
      , tailIndex_(tailIndex)
      , heavyTails_(heavyTails)
    {
    }

    /**
     * @brief Gets the sample size.
     * @return std::size_t Number of returns ($n$).
     */
    std::size_t getN() const
    {
      return n_;
    }

    /**
     * @brief Gets the annualized volatility.
     * @return double Annualized Standard Deviation ($\sigma_{ann}$).
     */
    double      getSigmaAnn() const
    {
      return sigmaAnn_;
    }

    /**
     * @brief Gets the sample skewness.
     * @return double Skewness value.
     */
    double      getSkew() const
    {
      return skew_;
    }

    /**
     * @brief Gets the sample excess kurtosis.
     * @return double Excess Kurtosis (Normal distribution $\approx 0$).
     */
    double      getExKurt() const
    {
      return exkurt_;
    }

    /**
     * @brief Gets the estimated Pareto tail index ($\alpha$).
     * @details Smaller values indicate heavier tails (e.g., $\alpha < 2$ implies infinite variance).
     * @return double The tail index, or $\le 0.0$ if estimation failed/insufficient data.
     */
    double      getTailIndex() const
    {
      return tailIndex_;
    }

    /**
     * @brief Checks if the basic heavy-tail heuristic triggered.
     * @return true If |Skew| > 0.9 or ExKurt > 1.2 (typical defaults).
     */
    bool        hasHeavyTails() const
    {
      return heavyTails_;
    }

  private:
    std::size_t n_;
    double      sigmaAnn_;
    double      skew_;
    double      exkurt_;
    double      tailIndex_;   // Pareto α estimate; <= 0 if invalid
    bool        heavyTails_;
  };

  // NOTE: IMNRatioPolicy and TailVolPriorPolicy have been superseded by the new infrastructure:
  // - Use palvalidator::analysis::IAdaptiveRatioPolicy instead of IMNRatioPolicy
  // - Use palvalidator::analysis::TailVolatilityAdaptivePolicy instead of TailVolPriorPolicy

  // NOTE: estimate_left_tail_index_hill() has been moved to AdaptiveRatioInternal.h
  // Use palvalidator::analysis::detail::estimate_left_tail_index_hill() instead.

  // NOTE: The following policy classes have been superseded by the new infrastructure:
  // - IRatioRefinementPolicy → palvalidator::analysis::IAdaptiveRatioPolicy
  // - LBStabilityRefinementPolicy → palvalidator::analysis::TailVolatilityAdaptivePolicy::refineRatio()
  // - NoRefinementPolicy → Default behavior in IAdaptiveRatioPolicy::computeRatioWithRefinement()
  // - TailVolStabilityPolicy → palvalidator::analysis::TailVolatilityAdaptivePolicy

  // -----------------------------------------------------------------------------
  // Simple dependence proxies & small-N heuristics
  // -----------------------------------------------------------------------------

  // NOTE: The following functions have been removed as dead code (only used by each other or dispatch_smallN_resampler):
  // - longest_sign_run() - only used by choose_block_smallN()
  // - sign_positive_ratio() - only used by choose_block_smallN()
  // - choose_block_smallN() - only used by dispatch_smallN_resampler()

  /**
   * @brief Detects if the distribution exhibits heavy tails based on skew and kurtosis.
   *
   * Thresholds: |Skew| >= 0.90 OR Excess Kurtosis >= 1.20.
   *
   * @param skew Sample skewness.
   * @param exkurt Sample excess kurtosis.
   * @return true if heavy tails are detected.
   */
  inline bool has_heavy_tails_wide(double skew, double exkurt)
  {
    // Prior behavior might have been stricter; widen slightly:
    // - |skew| >= 0.90, or
    // - excess kurtosis >= 1.20
    return (std::fabs(skew) >= 0.90) || (exkurt >= 1.20);
  }

  /**
   * @brief Determines if the Small-N (m-out-of-n) logic path should be activated.
   *
   * Policy:
   * - Always run if N <= 40.
   * - Run if N <= 60 AND data has heavy tails.
   *
   * @param n Sample size.
   * @param heavy_tails Result of has_heavy_tails_wide().
   * @return true if Small-N bootstrap is recommended.
   */
  inline bool should_run_smallN(std::size_t n, bool heavy_tails)
  {
    return (n <= 40) || ( (n <= 60) && heavy_tails );
  }
  

  // Tiny L for individual M2M: clamp to [2,3] and ≤ requested L
  inline std::size_t clamp_smallL(std::size_t L)
  {
    return std::max<std::size_t>(2, std::min<std::size_t>(3, L));
  }
  
  /**
   * @brief Heuristic m/n rule for the m-out-of-n bootstrap in very small samples.
   * @deprecated This function is already ported to AdaptiveRatioPolicies.h. Keep for backward compatibility.
   *
   * This helper returns an m/n ratio used by the m-out-of-n bootstrap when n is tiny
   * (e.g., n ≈ 20–40). Conceptually, we are saying:
   *
   *   "With such a small sample, we are skeptical of the ordinary n-out-of-n bootstrap.
   *    To stress-test the statistic / strategy, we only give it a substantially smaller
   *    subsample of size m << n on each bootstrap replicate and see if it still passes."
   *
   * Rationale:
   * ----------
   * The ordinary bootstrap (resampling n points with replacement) can fail badly for
   * small n, heavy-tailed data, or non-smooth statistics (e.g., quantiles, extrema,
   * ratio-type or geometric-mean statistics). In these regimes it often produces
   * confidence intervals that are too narrow and anti-conservative. The m-out-of-n
   * bootstrap fixes this by using a subsample size m that satisfies:
   *
   *   - m → ∞ as n → ∞, but
   *   - m/n → 0 as n → ∞.
   *
   * Under these conditions, one can often recover consistency and better coverage
   * properties when the ordinary bootstrap fails; see, e.g.:
   *
   *   - Bickel & Sakov (2008), "On the choice of m in the m out of n bootstrap and
   *     confidence bounds for extrema", Statistica Sinica 18(3), 967–985.
   *       PDF: https://www.stat.berkeley.edu/~bickel/BS2008SS.pdf
   *       Journal page: https://www3.stat.sinica.edu.tw/statistica/j18n3/j18n38/j18n38.html
   *
   *   - Shao & Tu (1995), "The Jackknife and Bootstrap", Springer Series in Statistics.
   *       Springer: https://doi.org/10.1007/978-1-4612-0795-5
   *
   *   - Politis, Romano & Wolf (1999), "Subsampling", Springer Series in Statistics.
   *       Springer: https://link.springer.com/book/10.1007/978-1-4612-1554-7
   *
   *   - Hall (1992), "The Bootstrap and Edgeworth Expansion", Springer.
   *       Springer: https://link.springer.com/book/10.1007/978-1-4612-4384-7
   *
   * Choice of m:
   * ------------
   * We adopt a simple power-law rule
   *
   *      m_target = n^(2/3),
   *
   * which is a commonly recommended compromise in the small-n literature:
   *  - It grows with n (so m is not pathologically small),
   *  - but m/n shrinks as n increases (so we still get the m-out-of-n benefits).
   *
   * For the ultra-small n regime this code targets (typical n in [20, 40]):
   *
   *   - m ≈ n^(2/3) gives m/n around 0.30–0.40,
   *   - this is intentionally much smaller than the n-out-of-n bootstrap (m = n),
   *   - so each bootstrap replicate is a "harder test" of the strategy’s stability.
   *
   * Practically, a robust strategy should still show a favorable statistic when given
   * only 30–40% of the original sample on each resample; fragile, overfit, or
   * outlier-driven strategies tend to fail under this harsher subsampling regime.
   *
   * Implementation details:
   * -----------------------
   *  - For very small n, we enforce a hard floor m >= 7 so that the statistic is
   *    computed on at least a minimally meaningful subsample.
   *  - We also ensure m <= n - 1 so that we perform a genuine m-out-of-n bootstrap
   *    (never collapsing back to the ordinary n-bootstrap).
   *  - The function returns the ratio m/n as a double; the caller converts this ratio
   *    to an integer m_sub = floor( (m/n) * n ) and enforces [2, n-1] as a final guard.
   *
   * Summary:
   * --------
   * This function embodies a conservative policy: for tiny samples where we do not
   * trust the ordinary bootstrap, it deliberately shrinks m to roughly n^(2/3).
   * That makes the bootstrap a stress test of the statistic’s robustness, rather than
   * a mechanism that amplifies small-sample luck.
   */
  inline double mn_ratio_from_n(std::size_t n)
  {
    if (n == 0) return 1.0;
    if (n < 3) return 1.0; // Too small to subsample meaningfully

    // 1. Calculate Power Law Target: m = n^(2/3)
    //    For N=30 -> m=9.65 (approx 10)
    double m_target = std::pow(static_cast<double>(n), 2.0/3.0);
    
    // 2. Define Bounds
    //    Floor: We want at least ~7-8 items to calculate a meaningful statistic
    //    Ceil:  Must be strictly less than n (n-1 or n-2) to be a true subsample
    double m_floor  = 7.0; 
    double m_ceil   = static_cast<double>(n - 1); 

    // 3. Clamp
    double m = std::max(m_floor, std::min(m_target, m_ceil));
    
    // 4. Return Ratio
    return m / static_cast<double>(n);
  }
  
  // NOTE: dispatch_smallN_resampler() has been removed as dead code (not used anymore)

  // -----------------------------------------------------------------------------
  // LB combine helpers (shared by stages)
  // -----------------------------------------------------------------------------
  namespace internal
  {

    struct RunsTestConfig
    {
      double alpha_quantile = 0.95;  // one-sided quantile (e.g., 95th)
      int    num_sims       = 256;   // tiny, fast MC
    };

    /**
     * @brief Returns the minimum value in the vector.
     */
    template <class Num>
    inline Num min_of(const std::vector<Num>& v) {
      return *std::min_element(v.begin(), v.end(),
			       [](const Num& a, const Num& b){ return a < b; });
    }

    /**
     * @brief Returns the median of a vector of size 2 or 3.
     * For size 2, returns the arithmetic mean.
     */
    template <class Num>
    inline Num median_of_2_or_3(std::vector<Num> v) {
      std::sort(v.begin(), v.end(), [](const Num& a, const Num& b){ return a < b; });
      if (v.size() == 2) return v[0] + (v[1] - v[0]) / Num(2);
      return v[1]; // size()==3
    }


    /**
     * @brief Logs the decision policy used for LB construction to the stream.
     */
    inline void log_policy_line(std::ostream& os,
				const char* policyLabel,
				std::size_t n, std::size_t L,
				double skew, double exkurt,
				bool heavy_tails,
				const char* resamplerName,
				std::size_t L_small)
    {
      os << "   [Bootstrap] Conservative LB construction policy = policy: "
	 << policyLabel
	 << "  | n=" << n << " L=" << L
	 << "  skew=" << skew << " exkurt=" << exkurt
	 << "  heavy_tails=" << (heavy_tails ? "yes" : "no")
	 << "  SmallNResampler=" << resamplerName
	 << "  L_small=" << L_small << "\n";
    }
  } // namespace internal

  /**
   * @brief Aggregated result from the "Conservative Small-N" logic.
   * * Contains the combined lower bound (min of m-out-of-n and BCa) and
   * diagnostic information about which resampler and parameters were used.
   */
  template <typename Num, typename BootstrapStatistic, typename StrategyT>
  struct SmallNConservativeResult {
    Num         per_lower{};       ///< Combined per-period LB (min of engines).
    Num         ann_lower{};       ///< Annualized LB.
    std::size_t m_sub{0};          ///< The subsample size used for m-out-of-n.
    std::size_t L_used{0};         ///< The block length actually used (clamped).
    std::size_t effB_mn{0};        ///< Effective B (non-degenerate) for m-out-of-n.
    std::size_t effB_bca{0};       ///< Effective B for BCa.
    const char* resampler_name{""};///< Name of the chosen resampler (IID or Block).
    double      duel_ratio{std::numeric_limits<double>::quiet_NaN()};  ///< >= 1.0 if both engines valid; NaN otherwise
    bool        duel_ratio_valid{false};  ///< true if duel_ratio is meaningful
  };

  // NOTE: resolve_adaptive_subsample_ratio() has been superseded by MOutOfNPercentileBootstrap::runWithRefinement()

/**
   * @brief Executes the "Duel" between m-out-of-n and BCa bootstraps.
   *
   * @details
   * This kernel function performs the heavy lifting:
   * 1. Runs the m-out-of-n bootstrap using the provided resampler and ratio.
   * 2. Runs the BCa bootstrap using the same resampler.
   * 3. Logs detailed diagnostics (shrinkage rates, implied sigma).
   * 4. Returns the result containing the MINIMUM of the two Lower Bounds.
   *
   * @tparam ResamplerT The specific resampler type (IID or Block).
   * @tparam Num Numeric type.
   * @tparam BootstrapStatistic Statistic functor type.
   * @tparam StrategyT Strategy type.
   * @tparam BootstrapFactoryT Factory type.
   *
   * @param returns The vector of returns.
   * @param resampler The instantiated resampler object.
   * @param rho The subsampling ratio to use for m-out-of-n.
   * @param L_small The block length.
   * @param annualizationFactor Factor to annualize results.
   * @param confLevel Confidence level.
   * @param B Number of bootstrap replicates.
   * @param z Z-score corresponding to confidence level (for sigma calc).
   * @param strategy The strategy object.
   * @param factory The bootstrap factory.
   * @param stageTag CRN tag.
   * @param fold CRN fold.
   * @param os Optional output stream.
   * @param resamplerName String name of the resampler for reporting.
   * @return SmallNConservativeResult Combined result struct.
   */
  template <typename ResamplerT, typename Num, typename BootstrapStatistic, typename StrategyT, typename BootstrapFactoryT>
  SmallNConservativeResult<Num, BootstrapStatistic, StrategyT>
  execute_bootstrap_duel(
			 const std::vector<Num>& returns,
			 ResamplerT& resampler,
			 double rho,
			 std::size_t L_small,
			 double annualizationFactor,
			 double confLevel,
			 std::size_t B,
			 double z,
			 StrategyT& strategy,
			 BootstrapFactoryT& factory,
			 int stageTag,
			 int fold,
			 std::ostream* os,
			 const char* resamplerName)
  {
    SmallNConservativeResult<Num, BootstrapStatistic, StrategyT> r{};
    r.L_used = L_small;
    r.resampler_name = resamplerName;

    const std::size_t n = returns.size();

    // ---------------------------------------------------------
    // 1. Run m-out-of-n Bootstrap
    // ---------------------------------------------------------
    auto [mnBoot, mnCrn] = factory.template makeMOutOfN<Num, BootstrapStatistic, ResamplerT>(B,
  							  confLevel,
  							  rho,
  							  resampler,
  							  strategy,
  							  stageTag,
  							  static_cast<int>(L_small),
  							  fold);

    auto mnR = mnBoot.run(returns, BootstrapStatistic(), mnCrn);
    const Num lbP_mn = mnR.lower;
    const Num lbA_mn = mkc_timeseries::Annualizer<Num>::annualize_one(lbP_mn, annualizationFactor); // Store annualized LB

    r.m_sub = mnR.m_sub;
    r.effB_mn = mnR.effective_B;

    // --- Diagnostics for m-out-of-n ---
    if (os)
      {
	const double mn_ratio = (n > 0)
	  ? (static_cast<double>(mnR.m_sub) / static_cast<double>(n))
	  : 0.0;
	const double shrinkRate = 1.0 - mn_ratio;

	(*os) << "   [Bootstrap] m_sub=" << mnR.m_sub
	      << "  n=" << n
	      << "  m/n=" << std::fixed << std::setprecision(3) << mn_ratio
	      << "  shrink=" << std::fixed << std::setprecision(3) << shrinkRate
	      << "\n";

	// Attempt to calculate implied sigma if upper bound is available
	if constexpr (detail::has_member_upper<decltype(mnR)>::value)
	  {
	    const double width = std::max(0.0, num::to_double(mnR.upper - mnR.lower));
	    const double sigma_mn = (z > 0.0)
	      ? (width / (2.0 * z))
	      : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma_mn * sigma_mn) * 100.0;

	    (*os) << "   [Diag] m/n σ(per-period)≈ " << sigma_mn
		  << "  var≈ " << var
		  << "  effB=" << mnR.effective_B
		  << "  L=" << mnR.L
		  << "\n";
	  }
	else
	  {
	    (*os) << "   [Diag] m/n σ: skipped (no two-sided CI available)\n";
	  }
      }

    // ---------------------------------------------------------
    // 2. Run BCa Bootstrap
    // ---------------------------------------------------------
    auto bca = factory.template makeBCa<Num>(
    	     returns, B, confLevel, BootstrapStatistic(), resampler,
    	     strategy, stageTag, static_cast<int>(L_small), fold);

    const Num lbP_bca = bca.getLowerBound();
    const Num lbA_bca = mkc_timeseries::Annualizer<Num>::annualize_one(lbP_bca, annualizationFactor); // Store annualized LB
    r.effB_bca = B;

    // --- Diagnostics for BCa ---
    if (os)
      {
	if constexpr (detail::has_getUpperBound<decltype(bca)>::value)
	  {
	    const Num ubP_bca  = bca.getUpperBound();
	    const double width = std::max(0.0, num::to_double(ubP_bca - lbP_bca));
	    const double sigma_bca = (z > 0.0)
	      ? (width / (2.0 * z))
	      : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma_bca * sigma_bca) * 100.0;

	    (*os) << "   [Diag] BCa σ(per-period)≈ " << sigma_bca
		  << "  var≈ " << var
		  << "  effB=" << B
		  << "  L=" << L_small
		  << "\n";
	  }
	else
	  {
	    (*os) << "   [Diag] BCa σ: skipped (upper bound API not available)\n";
	  }
      }

    // ---------------------------------------------------------
    // 3. Combine (Conservative Minimum) & Log Duel Results
    // ---------------------------------------------------------
    r.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
    r.ann_lower = (lbP_mn < lbP_bca) ? lbA_mn : lbA_bca; // Final LB is min of the two

    // ---------------------------------------------------------
    // 4. Compute Duel Ratio (for ratio statistics like Profit Factor)
    // ---------------------------------------------------------
    double disp_mn  = BootstrapStatistic::formatForDisplay(num::to_double(lbA_mn));
    double disp_bca = BootstrapStatistic::formatForDisplay(num::to_double(lbA_bca));
    
    double duel_ratio     = std::numeric_limits<double>::quiet_NaN();
    bool   duel_ratio_ok  = false;
    
    if (disp_mn > 0.0 && disp_bca > 0.0)
    {
      double lo = std::min(disp_mn, disp_bca);
      double hi = std::max(disp_mn, disp_bca);
      if (lo > 0.0)
      {
        duel_ratio    = hi / lo;  // >= 1
        duel_ratio_ok = true;
      }
    }
    
    r.duel_ratio       = duel_ratio;
    r.duel_ratio_valid = duel_ratio_ok;

    if (os)
      {
 // Log the duel results for comparison on one line (formatted appropriately for the statistic type)
 (*os) << "   [Bootstrap/Duel] LB(ann) Duel: "
       << "  m/n = " << std::fixed << std::setprecision(4) << BootstrapStatistic::formatForDisplay(num::to_double(lbA_mn)) << "%"
       << "  BCa =" << std::setprecision(4) << BootstrapStatistic::formatForDisplay(num::to_double(lbA_bca)) << "%"
       << "  Winner =" << BootstrapStatistic::formatForDisplay(num::to_double(r.ann_lower)) << "%"
       << (duel_ratio_ok ? ("  ratio=" + std::to_string(duel_ratio)) : "  ratio=n/a")
       << "\n";

 (*os) << "   [Bootstrap] SmallNResampler = " << r.resampler_name
       << "  (L_small = " << r.L_used << ")\n";
      }

    return r;
  }

  // Forward declaration so the 11-arg legacy overload can delegate to it
  template <typename Num, typename BootstrapStatistic, typename StrategyT,
            typename BootstrapFactoryT = palvalidator::bootstrap_cfg::BootstrapFactory>
  SmallNConservativeResult<Num, BootstrapStatistic, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
                                  std::size_t              L,
                                  double                   annualizationFactor,
                                  double                   confLevel,
                                  std::size_t              B,
                                  double                   rho_m,
                                  StrategyT&               strategy,
                                  BootstrapFactoryT&       bootstrapFactory,
                                  std::ostream*            os,
                                  int                      stageTag,
                                  int                      fold,
                                  std::optional<bool>      heavy_tails_override);


  /**
   * @brief runs the conservative small-N lower bound analysis with automatic heavy-tail detection.
   *
   * This function acts as a convenience wrapper. It automatically computes the skewness
   * and excess kurtosis of the input returns to determine if a "heavy tail" override
   * is necessary, then delegates to the core implementation.
   *
   * @details
   * **Objective:** To provide a robust Lower Bound (LB) estimate for strategies with small sample
   * sizes (typically N=20 to 60) where the standard bootstrap might be too optimistic.
   *
   * **Logic:**
   * 1. Computes sample skewness and excess kurtosis.
   * 2. Determines if the distribution has "heavy tails" (via `has_heavy_tails_wide`).
   * 3. Calls the main implementation, passing the detected heavy-tail status as an override.
   *
   * @tparam Num Numeric type (e.g., double, decimal).
   * @tparam BootstrapStatistic Statistic functor type (e.g., GeoMeanStat, LogProfitFactorStat).
   * @tparam StrategyT The strategy type (used for CRN hashing).
   * @tparam BootstrapFactoryT The factory type for creating bootstrap engines.
   *
   * @param returns The vector of high-resolution returns (e.g., per-trade or daily).
   * @param L The block length suggestion (will be clamped to [2,3] for small N internally).
   * @param annualizationFactor The factor to convert per-period metrics to annualized (e.g., 252.0).
   * @param confLevel The confidence level for the lower bound (e.g., 0.95).
   * @param B The number of bootstrap replicates (e.g., 1000+).
   * @param rho_m The m-out-of-n subsampling ratio. Pass <= 0.0 to trigger the adaptive
   *              TailVolStabilityPolicy (tail/vol prior + LB-stability refinement).
   * @param strategy Reference to the strategy object (for identification/hashing).
   * @param bootstrapFactory Reference to the factory generating RNG engines.
   * @param os Optional output stream for logging diagnostics (can be nullptr).
   * @param stageTag CRN tag for the pipeline stage (default 3).
   * @param fold CRN fold index (default 0).
   *
   * @return SmallNConservativeResult containing the annualized/per-period LB and diagnostics.
   */
  template <typename Num, typename BootstrapStatistic, typename StrategyT, typename BootstrapFactoryT>
  inline SmallNConservativeResult<Num, BootstrapStatistic, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
                                  std::size_t              L,
                                  double                   annualizationFactor,
                                  double                   confLevel,
                                  std::size_t              B,
                                  double                   rho_m,  // if <=0 → use TailVolStabilityPolicy
                                  StrategyT&               strategy,
                                  BootstrapFactoryT&       bootstrapFactory,
                                  std::ostream*            os = nullptr,
                                  int                      stageTag = 3,
                                  int                      fold = 0)
  {
    using Stat = mkc_timeseries::StatUtils<Num>;

    // Robust, quantile-based shape summary (Bowley skew + tail span ratio).
    const auto qShape = Stat::computeQuantileShape(returns);

    // For small N, be conservative: if either side screams "non-Gaussian",
    // we treat this as heavy-tailed for the bootstrap configuration.
    const bool heavy =
      (qShape.hasStrongAsymmetry || qShape.hasHeavyTails);

    const std::optional<bool> heavy_override =
      heavy ? std::optional<bool>(true) : std::nullopt;

    return conservative_smallN_lower_bound<Num, BootstrapStatistic, StrategyT, BootstrapFactoryT>(
        returns,
        L,
        annualizationFactor,
        confLevel,
        B,
        rho_m,
        strategy,
        bootstrapFactory,
        os,
        stageTag,
        fold,
        heavy_override);
  }

/**
   * @brief Core implementation of the conservative small-N lower bound logic.
   *
   * @details
   * This function orchestrates the "Small-N" bootstrap process:
   * 1. Analyzes distribution (Vol, Shape, Tail Index).
   * 2. Selects the appropriate Resampler (IID or StationaryBlock) based on
   *    data characteristics or overrides.
   * 3. Calculates the optimal m-out-of-n ratio (using `resolve_adaptive_subsample_ratio`).
   * 4. Executes the "Duel" (m/n vs BCa) via `execute_bootstrap_duel`.
   *
   * Heavy-tail logic:
   *   - By default, heavy tails are detected via a **conservative OR** of quantile shape
   *     and the Hill tail index (alpha <= 2.0). The legacy moment-based check 
   *     (has_heavy_tails_wide) is explicitly excluded from this decision.
   *   - If `heavy_tails_override` is provided, it overrides this combined flag.
   *
   * @tparam Num Numeric type.
   * @tparam BootstrapStatistic Statistic functor.
   * @tparam StrategyT Strategy type.
   * @tparam BootstrapFactoryT Factory type.
   */
template <typename Num, typename BootstrapStatistic, typename StrategyT,
            typename BootstrapFactoryT>
  inline SmallNConservativeResult<Num, BootstrapStatistic, StrategyT>
  conservative_smallN_lower_bound(
      const std::vector<Num>& returns,
      std::size_t              L,
      double                   annualizationFactor,
      double                   confLevel,
      std::size_t              B,
      double                   rho_m,   // if <=0 → use TailVolStabilityPolicy
      StrategyT&               strategy,
      BootstrapFactoryT&       bootstrapFactory,
      std::ostream* os,      // optional stage logger
      int                      stageTag,
      int                      fold,
      std::optional<bool>      heavy_tails_override)
  {
    // =========================================================================
    // PHASE 5 REFACTORING: This function now delegates adaptive ratio calculation
    // to the new MOutOfNPercentileBootstrap infrastructure while preserving
    // the unique "duel" logic (min of m-out-of-n and BCa).
    // =========================================================================
    
    using palvalidator::analysis::detail::StatisticalContext;
    using palvalidator::analysis::MOutOfNPercentileBootstrap;
    using palvalidator::analysis::FixedRatioPolicy;
    using palvalidator::resampling::StationaryMaskValueResamplerAdapter;
    using palvalidator::analysis::IAdaptiveRatioPolicy;
    using palvalidator::analysis::TailVolatilityAdaptivePolicy;
    
    // ---------------------------------------------------------
    // 1. Setup & Statistical Analysis (DELEGATED to new infrastructure)
    // ---------------------------------------------------------
    const std::size_t n = returns.size();
    const std::size_t L_small = clamp_smallL(L);
    
    // Create statistical context (replaces ~40 lines of manual calculation)
    StatisticalContext<Num> statCtx(returns, annualizationFactor);
    
    // Compute skew/exkurt separately for logging (still needed by MNRatioContext)
    const auto [skew, exkurt] =
        mkc_timeseries::StatUtils<Num>::computeSkewAndExcessKurtosis(returns);
    
    // Apply heavy-tail override if provided (for backward compatibility)
    const bool heavy_flag = heavy_tails_override.has_value()
                              ? *heavy_tails_override
                              : statCtx.hasHeavyTails();
    
    // Create MNRatioContext for logging compatibility
    MNRatioContext ctx(n, statCtx.getAnnualizedVolatility(), skew, exkurt,
                       statCtx.getTailIndex(), heavy_flag);
    
    // ---------------------------------------------------------
    // 2. M-out-of-N Bootstrap with Adaptive Ratio (DELEGATED)
    // ---------------------------------------------------------
    SmallNConservativeResult<Num, BootstrapStatistic, StrategyT> result{};
    result.L_used = L_small;
    result.resampler_name = "StationaryMaskValueResamplerAdapter";
    
    // Create resampler (always use Block resampler for small-N)
    StationaryMaskValueResamplerAdapter<Num> resampler(L_small);
    
    // Create m-out-of-n bootstrap
    using MNBootstrap = MOutOfNPercentileBootstrap<
        Num, BootstrapStatistic, StationaryMaskValueResamplerAdapter<Num>>;
    
    // Choose policy: TailVol adaptive by default, FixedRatio if rho_m > 0
    std::shared_ptr<IAdaptiveRatioPolicy<Num, BootstrapStatistic>> policy;

    if (rho_m > 0.0)
      {
	policy = std::make_shared<FixedRatioPolicy<Num, BootstrapStatistic>>(rho_m);
      }
    else
      {
	policy = std::make_shared<TailVolatilityAdaptivePolicy<Num, BootstrapStatistic>>();
      }

    // Build the bootstrap engine in adaptive mode with the chosen policy
    auto mnBootstrap =
      MNBootstrap::template createAdaptiveWithPolicy<BootstrapStatistic>(B, confLevel, resampler, policy);
    
    auto mnResult = mnBootstrap.template runWithRefinement<BootstrapStatistic>(
        returns, BootstrapStatistic(), strategy, bootstrapFactory,
        stageTag, fold, os);
    
    // Extract m-out-of-n results
    const Num lbP_mn = mnResult.lower;
    const Num lbA_mn = mkc_timeseries::Annualizer<Num>::annualize_one(lbP_mn, annualizationFactor);
    result.m_sub = mnResult.m_sub;
    result.effB_mn = mnResult.effective_B;
    
    // ---------------------------------------------------------
    // 3. BCa Bootstrap (UNCHANGED)
    // ---------------------------------------------------------
    auto bca = bootstrapFactory.template makeBCa<Num>(
        returns, B, confLevel, BootstrapStatistic(), resampler,
        strategy, stageTag, static_cast<int>(L_small), fold);
    
    const Num lbP_bca = bca.getLowerBound();
    const Num lbA_bca = mkc_timeseries::Annualizer<Num>::annualize_one(lbP_bca, annualizationFactor);
    result.effB_bca = B;
    
    // ---------------------------------------------------------
    // 4. Duel Logic - Select Minimum (PRESERVED - UNIQUE FUNCTIONALITY)
    // ---------------------------------------------------------
    result.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
    result.ann_lower = (lbP_mn < lbP_bca) ? lbA_mn : lbA_bca;
    
    // Compute duel ratio for diagnostics
    double disp_mn  = BootstrapStatistic::formatForDisplay(num::to_double(lbA_mn));
    double disp_bca = BootstrapStatistic::formatForDisplay(num::to_double(lbA_bca));
    
    double duel_ratio = std::numeric_limits<double>::quiet_NaN();
    bool duel_ratio_ok = false;
    
    if (disp_mn > 0.0 && disp_bca > 0.0)
    {
        double lo = std::min(disp_mn, disp_bca);
        double hi = std::max(disp_mn, disp_bca);
        if (lo > 0.0)
        {
            duel_ratio = hi / lo;
            duel_ratio_ok = true;
        }
    }
    
    result.duel_ratio = duel_ratio;
    result.duel_ratio_valid = duel_ratio_ok;
    
    // ---------------------------------------------------------
    // 5. Diagnostic Logging (PRESERVED)
    // ---------------------------------------------------------
    if (os)
    {
        (*os) << "   [Bootstrap/Duel] LB(ann) Duel: "
              << "  m/n = " << std::fixed << std::setprecision(4)
              << BootstrapStatistic::formatForDisplay(num::to_double(lbA_mn)) << "%"
              << "  BCa = " << std::setprecision(4)
              << BootstrapStatistic::formatForDisplay(num::to_double(lbA_bca)) << "%"
              << "  Winner = " << BootstrapStatistic::formatForDisplay(num::to_double(result.ann_lower)) << "%"
              << (duel_ratio_ok ? ("  ratio=" + std::to_string(duel_ratio)) : "  ratio=n/a")
              << "\n";
        
        (*os) << "   [Bootstrap] SmallNResampler = " << result.resampler_name
              << "  (L_small = " << result.L_used << ")\n";
    }
    
    return result;
  }
} // namespace palvalidator::bootstrap_helpers
