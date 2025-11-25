#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
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

namespace palvalidator::bootstrap_helpers
{

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

  // -----------------------------------------------------------------------------
  // Simple dependence proxies & small-N heuristics
  // -----------------------------------------------------------------------------

  /**
   * @brief Calculates the length of the longest contiguous sequence of returns with the same sign.
   *
   * A cheap proxy for serial dependence (clustering). It ignores zero-returns
   * when checking for sign continuity but counts the streak length.
   *
   * @param x The vector of returns.
   * @return std::size_t The length of the longest positive or negative run.
   */
  template <class Num>
  inline std::size_t longest_sign_run(const std::vector<Num>& x)
  {
    const std::size_t n = x.size();
    if (n == 0) return 0;

    auto sgn = [](double v){ return (v > 0.0) ? 1 : (v < 0.0 ? -1 : 0); };

    int last = sgn(num::to_double(x[0]));
    std::size_t cur = 1, best = 1;
    for (std::size_t i = 1; i < n; ++i) {
      const int s = sgn(num::to_double(x[i]));
      if (s == last && s != 0) { ++cur; }
      else { if (cur > best) best = cur; cur = 1; last = s; }
    }
    if (cur > best) best = cur;
    return best;
  }

  /**
   * @brief Calculates the ratio of strictly positive returns to total returns.
   *
   * Used as a proxy for trend or sign imbalance.
   *
   * @param x The vector of returns.
   * @return double The ratio (0.0 to 1.0).
   */
  template <class Num>
  inline double sign_positive_ratio(const std::vector<Num>& x)
  {
    const std::size_t n = x.size();
    if (n == 0) return 0.0;

    std::size_t num_pos = 0;
    for (const auto& r : x)
      if (num::to_double(r) > 0.0)
	++num_pos;

    return static_cast<double>(num_pos) / static_cast<double>(n);
  }

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

  /**
   * @brief Heuristic to choose between Block and IID resampling for small samples.
   *
   * When N is too small for ACF to be reliable, this uses "cheap proxies" (sign ratio
   * and longest run) to detect structure.
   *
   * @param ratio_pos The ratio of positive returns.
   * @param n Sample size.
   * @param longest_run The longest run of same-sign returns.
   * @param hi_thresh Upper bound for balanced ratio (default 0.65).
   * @param lo_thresh Lower bound for balanced ratio (default 0.35).
   * @param n_thresh The sample size threshold below which this logic applies (default 40).
   * @return true if Block resampling is recommended (imbalanced or streaky).
   * @return false if IID resampling is acceptable (balanced and random-looking).
   */
  inline bool choose_block_smallN(double ratio_pos,
                                std::size_t n,
                                std::size_t longest_run,
                                double hi_thresh = 0.65,
                                double lo_thresh = 0.35,
                                std::size_t n_thresh = 40)
  {
    const bool sign_imbalance = (ratio_pos > hi_thresh) || (ratio_pos < lo_thresh);

    // Adaptive run trigger: floor at 6, grow slowly with n (≈ 0.18n capped)
    const std::size_t base = 6;
    const std::size_t scaled = static_cast<std::size_t>(std::ceil(0.18 * std::min(n, n_thresh)));
    const std::size_t run_thresh = std::max(base, scaled); // 6..7 for n in [20,40]

    const bool streaky_smallN = (n <= n_thresh) && (longest_run >= run_thresh);
    return sign_imbalance || streaky_smallN;
  }
  

  // Tiny L for individual M2M: clamp to [2,3] and ≤ requested L
  inline std::size_t clamp_smallL(std::size_t L)
  {
    return std::max<std::size_t>(2, std::min<std::size_t>(3, L));
  }
  
  /**
   * @brief Heuristic m/n rule for the m-out-of-n bootstrap in very small samples.
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
  
  /**
   * @brief Helper to dispatch a functor with the appropriate Resampler type.
   *
   * Decides between `IIDResampler` and `StationaryMaskValueResamplerAdapter`
   * based on data characteristics, then calls the provided lambda `fn`.
   *
   * @tparam Num Numeric type.
   * @tparam Fn Functor type accepting (Resampler, ratio, use_block, L_small).
   * @param data Returns vector.
   * @param L Requested block length.
   * @param fn The functor/lambda to execute.
   * @param chosen_name [out] Optional pointer to C-string for the chosen resampler name.
   * @param L_small_out [out] Optional pointer to receive the clamped block length used.
   * @return The return value of `fn`.
   */
  template <class Num, class Fn>
  auto dispatch_smallN_resampler(const std::vector<Num>& data,
  		 std::size_t L,
  		 Fn&& fn,
  		 const char** chosen_name = nullptr,
  		 std::size_t* L_small_out = nullptr)
  {
    using IIDResampler  = mkc_timeseries::IIDResampler<Num>;
    using BlockValueRes = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    const std::size_t n       = data.size();
    const double      ratio   = sign_positive_ratio(data);
    const std::size_t runlen  = longest_sign_run(data);
    const bool        use_blk = choose_block_smallN(ratio, n, runlen);

    const std::size_t L_small = clamp_smallL(L);
    if (L_small_out) *L_small_out = L_small;
    if (chosen_name) *chosen_name = use_blk
         ? "StationaryMaskValueResamplerAdapter(small L)"
         : "IIDResampler";

    if (use_blk) {
      BlockValueRes blockResampler(L_small);
      return fn(blockResampler, ratio, /*use_block=*/true, L_small);
    } else {
      IIDResampler iidResampler;
      return fn(iidResampler, ratio, /*use_block=*/false, L_small);
    }
  }

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
     * @brief Generates a single IID Bernoulli sequence and finds its longest run.
     * @param n Sequence length.
     * @param p Probability of success (positive return).
     * @param rng Random number engine.
     * @return std::size_t Longest run length in the simulated sequence.
     */
    inline std::size_t longest_run_iid_once(std::size_t n, double p, std::mt19937_64& rng)
    {
      std::bernoulli_distribution bern(p);
      if (n == 0) return 0;

      // Draw first
      bool prev = bern(rng);
      std::size_t longest = 1, cur = 1;

      for (std::size_t i = 1; i < n; ++i) {
	const bool x = bern(rng);
	if (x == prev) { ++cur; }
	else { longest = std::max(longest, cur); cur = 1; prev = x; }
      }
      longest = std::max(longest, cur);
      return longest;
    }

    /**
     * @brief Estimates the quantile of the "longest run" distribution via Monte Carlo.
     *
     * Used to determine if an observed run length is statistically significant
     * (i.e., unlikely to happen in IID data).
     *
     * @param n Sequence length.
     * @param p Probability of success.
     * @param cfg Configuration (alpha level, number of simulations).
     * @param seed Random seed.
     * @return std::size_t The estimated quantile (e.g., 95th percentile) of run length.
     */
    inline std::size_t runs_longest_quantile_MC(std::size_t n, double p,
						RunsTestConfig cfg = {},
						std::uint64_t seed = 0xC0FFEEULL)
    {
      if (n == 0) return 0;
      std::mt19937_64 rng(seed);

      std::vector<std::size_t> samples;
      samples.reserve(std::max(1, cfg.num_sims));
      for (int s = 0; s < cfg.num_sims; ++s) {
	samples.push_back(longest_run_iid_once(n, p, rng));
      }
      std::sort(samples.begin(), samples.end());
      const std::size_t k = static_cast<std::size_t>(
						     std::min<std::size_t>(samples.size()-1,
									   std::max<std::size_t>(0, static_cast<std::size_t>(std::ceil(cfg.alpha_quantile * samples.size()) - 1))));
      return samples[k];
    }

    /**
     * @brief Checks if the observed longest run exceeds the MC-simulated 95th percentile.
     * @return true if the observed run is suspiciously long (suggesting dependence).
     */
    inline bool borderline_run_exceeds_MC95(std::size_t n,
					    double ratio_pos,
					    std::size_t observed_longest_run,
					    RunsTestConfig cfg = {},
					    std::uint64_t seed = 0xC0FFEEULL)
    {
      // Compute the 95th percentile under IID(p=ratio_pos)
      const std::size_t q95 = runs_longest_quantile_MC(n, ratio_pos, cfg, seed);
      return observed_longest_run >= q95;
    }

    /**
     * @brief Combines Lower Bounds (LBs) from multiple engines, adjusting strategy based on hurdle proximity.
     *
     * Logic:
     * 1. If the median of the annualized LBs is "close" (within `proximity_bps`) to the hurdle,
     * it acts conservatively and returns the MINIMUM of the LBs.
     * 2. Otherwise, it returns the Median-of-Present (or closer neighbor to the mean for N=2).
     *
     * @param parts_per Vector of per-period LBs.
     * @param annualizationFactor Factor to convert per-period to annualized.
     * @param hurdle_annual The target annualized return.
     * @param proximity_bps The window around the hurdle (in basis points) to trigger conservative mode.
     * @return Num The selected per-period Lower Bound.
     */
    template <class Num>
    inline Num combine_LBs_with_near_hurdle(const std::vector<Num>& parts_per,
					    double annualizationFactor,
					    Num hurdle_annual,
					    double proximity_bps = 75.0) // 0.75% default window
    {
      if (parts_per.empty())
	return Num(0);

      // If only one, nothing to combine.
      if (parts_per.size() == 1)
	return parts_per.front();

      // Build annualized candidates
      std::vector<Num> annualized; annualized.reserve(parts_per.size());
      for (const auto& p : parts_per)
	annualized.push_back(mkc_timeseries::Annualizer<Num>::annualize_one(p, annualizationFactor));

      // Find the "central" candidate via median-of-present (2 or 3)
      auto median_of_2_or_3 = [](std::vector<Num> v)
      {
	std::sort(v.begin(), v.end(), [](const Num& a, const Num& b){ return a < b; });
	if (v.size() == 2)
	  return v[0] + (v[1] - v[0]) / Num(2);
      
	return v[1]; // size()==3
      };
    
      const Num med_ann = median_of_2_or_3(annualized);

      // Proximity check in basis points (annualized)
      const Num delta = (med_ann - hurdle_annual);
      const double delta_bps = 10000.0 * delta.getAsDouble(); // 10000 bps = 100%

      // If near hurdle → be conservative: use MIN across engines
      if (std::fabs(delta_bps) <= proximity_bps) {
	// pick the per-period LB corresponding to the MIN annualized
	auto it = std::min_element(annualized.begin(), annualized.end(),
				   [](const Num& a, const Num& b){ return a < b; });
	const std::size_t idx = static_cast<std::size_t>(std::distance(annualized.begin(), it));
	return parts_per[idx];
      }

      // Else use median-of-present (return the matching per-period value)
      std::vector<std::pair<Num, std::size_t>> pairs;
      pairs.reserve(annualized.size());
      for (std::size_t i = 0; i < annualized.size(); ++i)
	pairs.emplace_back(annualized[i], i);

      std::sort(pairs.begin(), pairs.end(), [](auto& A, auto& B)
      {
	return A.first < B.first;
      });

      if (pairs.size() == 2)
	{
	  const Num mid = pairs[0].first + (pairs[1].first - pairs[0].first) / Num(2);
	  // choose the closer of the two to the arithmetic mid (stable tie-break)
	  const double d0 = std::fabs((pairs[0].first - mid).getAsDouble());
	  const double d1 = std::fabs((pairs[1].first - mid).getAsDouble());
	  return parts_per[ d0 <= d1 ? pairs[0].second : pairs[1].second ];
	}
      // size()==3 → exact median index is 1
      return parts_per[pairs[1].second];
    }

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
     * @brief Combines bounds using either a "2-of-3" (median) vote or a strict minimum.
     * @param parts Vector of bounds.
     * @param vote2 If true, uses median logic. If false, uses min_of().
     * @return Num The combined bound.
     */
    template <class Num>
    inline Num combine_LBs_2of3_or_min(const std::vector<Num>& parts, bool vote2) {
      if (!vote2 || parts.empty()) return min_of(parts);
      if (parts.size() == 1)      return parts.front();
      if (parts.size() == 2)      return median_of_2_or_3(parts);
      return median_of_2_or_3(parts);
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
  template <typename Num, typename GeoStat, typename StrategyT>
  struct SmallNConservativeResult {
    Num         per_lower{};       ///< Combined per-period LB (min of engines).
    Num         ann_lower{};       ///< Annualized LB.
    std::size_t m_sub{0};          ///< The subsample size used for m-out-of-n.
    std::size_t L_used{0};         ///< The block length actually used (clamped).
    std::size_t effB_mn{0};        ///< Effective B (non-degenerate) for m-out-of-n.
    std::size_t effB_bca{0};       ///< Effective B for BCa.
    const char* resampler_name{""};///< Name of the chosen resampler (IID or Block).
  };

  // Forward declaration so the 11-arg legacy overload can delegate to it
  template <typename Num, typename GeoStat, typename StrategyT,
            typename BootstrapFactoryT = palvalidator::bootstrap_cfg::BootstrapFactory>
  SmallNConservativeResult<Num, GeoStat, StrategyT>
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
   * @tparam GeoStat Statistic functor type (e.g., GeoMeanStat).
   * @tparam StrategyT The strategy type (used for CRN hashing).
   * @tparam BootstrapFactoryT The factory type for creating bootstrap engines.
   *
   * @param returns The vector of high-resolution returns (e.g., per-trade or daily).
   * @param L The block length suggestion (will be clamped to [2,3] for small N internally).
   * @param annualizationFactor The factor to convert per-period metrics to annualized (e.g., 252.0).
   * @param confLevel The confidence level for the lower bound (e.g., 0.95).
   * @param B The number of bootstrap replicates (e.g., 1000+).
   * @param rho_m The m-out-of-n subsampling ratio. Pass <= 0.0 to trigger the n^(2/3) heuristic.
   * @param strategy Reference to the strategy object (for identification/hashing).
   * @param bootstrapFactory Reference to the factory generating RNG engines.
   * @param os Optional output stream for logging diagnostics (can be nullptr).
   * @param stageTag CRN tag for the pipeline stage (default 3).
   * @param fold CRN fold index (default 0).
   *
   * @return SmallNConservativeResult containing the annualized/per-period LB and diagnostics.
   */
  template <typename Num, typename GeoStat, typename StrategyT,
            typename BootstrapFactoryT = palvalidator::bootstrap_cfg::BootstrapFactory>
  inline SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
  		  std::size_t L,
  		  double annualizationFactor,
  		  double confLevel,
  		  std::size_t B,
  		  double rho_m,                        // if <=0 → compute via mn_ratio_from_n
  		  StrategyT& strategy,
  		  BootstrapFactoryT& bootstrapFactory,
  		  std::ostream* os = nullptr,
  		  int stageTag = 3, int fold = 0)
  {
    const auto [skew, exkurt] =
        mkc_timeseries::StatUtils<Num>::computeSkewAndExcessKurtosis(returns);
    const bool heavy = has_heavy_tails_wide(skew, exkurt);
    const std::optional<bool> heavy_override = heavy ? std::optional<bool>(true)
                                                     : std::nullopt;

    return conservative_smallN_lower_bound<Num, GeoStat, StrategyT, BootstrapFactoryT>(returns,
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
  }

  /**
   * @brief Core implementation of the conservative small-N lower bound logic.
   *
   * Executes a "duel" between the m-out-of-n bootstrap and the BCa bootstrap, returning
   * the more conservative (minimum) Lower Bound of the two.
   *
   * @details
   * **Purpose:**
   * Standard bootstrapping (n-out-of-n) often under-estimates risk in small samples (N < 40)
   * because it overfits to the specific realized history. This function mitigates that by:
   *
   * 1. Subsampling: Using m-out-of-n resampling (where m << n) to inflate variance and
   * account for the uncertainty of the small sample size.
   *
   * 2. Resampler Selection: Automatically choosing between an IID Resampler (for balanced data)
   * and a Stationary Block Resampler (for streaky/heavy-tailed data).
   *
   * 3. Conservative Policy:*It runs *both* m-out-of-n and BCa, then returns `min(LB_mn, LB_bca)`.
   *
   * Heuristics:
   *
   * - **m-ratio:** If `rho_m <= 0`, the subsample size `m` is calculated using the Power Law
   * heuristic \f$ m \approx n^{2/3} \f$. This ensures `m/n` shrinks as `n` grows, preserving
   * consistency.
   *
   * - **Resampler:** If `heavy_tails_override` is set, it dictates the resampler. Otherwise,
   * heuristics based on run-length and sign-imbalance (`choose_block_smallN`) decide.
   *
   * @param returns The vector of high-resolution returns.
   * @param L The requested block length. Note: For small N, this is internally clamped to [2, 3].
   * @param annualizationFactor Factor to annualize the resulting per-period statistic.
   * @param confLevel Confidence level (0.5 < CL < 1.0).
   * @param B Number of bootstrap replicates.
   * @param rho_m The subsampling ratio. **Important:** Pass 0.0 or -1.0 to enable the
   * automatic \f$ n^{2/3} \f$ scaling rule (Recommended).
   * @param strategy The strategy object (used for CRN key generation).
   * @param bootstrapFactory Factory for creating the specific bootstrap engines.
   * @param os Optional stream for detailed logging (shrinkage rates, effective sigma, variance).
   * @param stageTag CRN tag for reproducibility.
   * @param fold CRN fold for reproducibility.
   * @param heavy_tails_override Optional boolean. If set, forces Block resampling (true) or
   * IID resampling (false), bypassing internal detection heuristics.
   *
   * @return SmallNConservativeResult Struct containing the minimum LB, the chosen resampler name,
   * and the effective subsample size used.
   */
  template <typename Num, typename GeoStat, typename StrategyT,
	    typename BootstrapFactoryT>
  inline SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
				  std::size_t              L,
				  double                   annualizationFactor,
				  double                   confLevel,
				  std::size_t              B,
				  double                   rho_m,   // if <=0 → compute via mn_ratio_from_n
				  StrategyT&               strategy,
				  BootstrapFactoryT&       bootstrapFactory,
				  std::ostream*            os,       // optional stage logger
				  int                      stageTag,
				  int                      fold,
				  std::optional<bool>      heavy_tails_override)
  {
    using IIDResampler  = mkc_timeseries::IIDResampler<Num>;
    using BlockValueRes = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    const std::size_t n   = returns.size();
    const double      rho = (rho_m > 0.0 ? rho_m : mn_ratio_from_n(n));

    // Small-N dependence proxies (cheap and deterministic)
    const double      ratio_pos = sign_positive_ratio(returns);
    const std::size_t runlen    = longest_sign_run(returns);
    const std::size_t L_small   = clamp_smallL(L);

    // Decide resampler with optional heavy-tail override + tiny MC guard
    bool use_block = false;

    constexpr std::size_t N_BLOCK_ALWAYS = 60; // tweakable

    if (heavy_tails_override.has_value())
      {
	use_block = *heavy_tails_override;           // explicit caller choice
      }
    else if (n <= N_BLOCK_ALWAYS)
      {
	use_block = true;                             // never IID at tiny n
      }
    else
      {
	// Original heuristic (kept for larger samples)
	const bool choose_block_fast = choose_block_smallN(ratio_pos, n, runlen);
	use_block = choose_block_fast;
      }

    const char* chosenName = use_block
      ? "StationaryMaskValueResamplerAdapter"
      : "IIDResampler";

    SmallNConservativeResult<Num, GeoStat, StrategyT> r{};
    r.L_used = L_small;

    // z for two-sided CL (for CI→σ back-out)
    const double z = z_from_two_sided_CL(confLevel);

    // ---------- Run engines on the chosen resampler ----------
    if (use_block)
      {
	BlockValueRes resampler(L_small);

	// m-out-of-n on SAME resampler
	GeoStat statGeo;
	auto [mnBoot, mnCrn] =
	  bootstrapFactory.template makeMOutOfN<Num, GeoStat, BlockValueRes>(
									     B, confLevel, rho, resampler, strategy, stageTag, /*L*/L_small, fold);

	auto mnR      = mnBoot.run(returns, GeoStat(), mnCrn);
	const Num lbP_mn = mnR.lower;

	// Log m_sub/n shrink
	const double mn_ratio   = (n > 0) ? (static_cast<double>(mnR.m_sub) / static_cast<double>(n)) : 0.0;
	const double shrinkRate = 1.0 - mn_ratio;
	if (os) {
	  (*os) << "   [Bootstrap] m_sub=" << mnR.m_sub
		<< "  n=" << n
		<< "  m/n=" << std::fixed << std::setprecision(3) << mn_ratio
		<< "  shrink=" << std::fixed << std::setprecision(3) << shrinkRate
		<< "\n";
	}

	// Try to log an effective σ from CI width if 'upper' is available on mnR
	if (os) {
	  if constexpr (detail::has_member_upper<decltype(mnR)>::value) {
	    const double width = std::max(0.0, num::to_double(mnR.upper - mnR.lower));
	    const double sigma = (z > 0.0) ? (width / (2.0 * z)) : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma * sigma) * 100.0;
	    (*os) << "   [Diag] m/n σ(per-period)≈ " << sigma
		  << "  var≈ " << var
		  << "  effB=" << mnR.effective_B
		  << "  L=" << mnR.L
		  << "\n";
	  } else {
	    (*os) << "   [Diag] m/n σ: skipped (no two-sided CI available)\n";
	  }
	}

	// BCa on SAME resampler
	auto bca      = bootstrapFactory.template makeBCa<Num>(
							       returns, B, confLevel, statGeo, resampler,
							       strategy, stageTag, /*L*/L_small, fold);
	const Num lbP_bca = bca.getLowerBound();

	// Try to log an effective σ from CI width if getUpperBound() exists
	if (os) {
	  if constexpr (detail::has_getUpperBound<decltype(bca)>::value) {
	    const Num ubP_bca = bca.getUpperBound();
	    const double width = std::max(0.0, num::to_double(ubP_bca - lbP_bca));
	    const double sigma = (z > 0.0) ? (width / (2.0 * z)) : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma * sigma) * 100.0;
	    (*os) << "   [Diag] BCa σ(per-period)≈ " << sigma
		  << "  var≈ " << var
		  << "  effB=" << B
		  << "  L=" << L_small
		  << "\n";
	  } else {
	    (*os) << "   [Diag] BCa σ: skipped (upper bound API not available)\n";
	  }
	}

	// Combine (conservative: min of engines)
	r.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
	r.ann_lower = mkc_timeseries::Annualizer<Num>::annualize_one(r.per_lower, annualizationFactor);
	r.m_sub     = mnR.m_sub;                // from MOutOfN result
	r.effB_mn   = mnR.effective_B;          // number of usable replicates
	r.effB_bca  = B;                        // BCa effective_B equals B here
      }
    else
      {
	IIDResampler resampler;

	// m-out-of-n on SAME resampler
	GeoStat statGeo;
	auto [mnBoot, mnCrn] =
	  bootstrapFactory.template makeMOutOfN<Num, GeoStat, IIDResampler>(
									    B, confLevel, rho, resampler, strategy, stageTag, /*L*/L_small, fold);

	auto mnR      = mnBoot.run(returns, GeoStat(), mnCrn);
	const Num lbP_mn = mnR.lower;

	// Log m_sub/n shrink
	const double mn_ratio   = (n > 0) ? (static_cast<double>(mnR.m_sub) / static_cast<double>(n)) : 0.0;
	const double shrinkRate = 1.0 - mn_ratio;
	if (os) {
	  (*os) << "   [Bootstrap] m_sub=" << mnR.m_sub
		<< "  n=" << n
		<< "  m/n=" << std::fixed << std::setprecision(3) << mn_ratio
		<< "  shrink=" << std::fixed << std::setprecision(3) << shrinkRate
		<< "\n";
	}

	// Try to log an effective σ from CI width if 'upper' is available on mnR
	if (os) {
	  if constexpr (detail::has_member_upper<decltype(mnR)>::value) {
	    const double width = std::max(0.0, num::to_double(mnR.upper - mnR.lower));
	    const double sigma = (z > 0.0) ? (width / (2.0 * z)) : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma * sigma) * 100.0;
	    (*os) << "   [Diag] m/n σ(per-period)≈ " << sigma
		  << "  var≈ " << var
		  << "  effB=" << mnR.effective_B
		  << "  L=" << mnR.L
		  << "\n";
	  } else {
	    (*os) << "   [Diag] m/n σ: skipped (no two-sided CI available)\n";
	  }
	}

	// BCa on SAME resampler
	auto bca      = bootstrapFactory.template makeBCa<Num>(
							       returns, B, confLevel, statGeo, resampler,
							       strategy, stageTag, /*L*/L_small, fold);
	const Num lbP_bca = bca.getLowerBound();

	// Try to log an effective σ from CI width if getUpperBound() exists
	if (os) {
	  if constexpr (detail::has_getUpperBound<decltype(bca)>::value) {
	    const Num ubP_bca = bca.getUpperBound();
	    const double width = std::max(0.0, num::to_double(ubP_bca - lbP_bca));
	    const double sigma = (z > 0.0) ? (width / (2.0 * z)) : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma * sigma) * 100.0;
	    (*os) << "   [Diag] BCa σ(per-period)≈ " << sigma
		  << "  var≈ " << var
		  << "  effB=" << B
		  << "  L=" << L_small
		  << "\n";
	  } else {
	    (*os) << "   [Diag] BCa σ: skipped (upper bound API not available)\n";
	  }
	}

	// Combine (conservative: min of engines)
	r.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
	r.ann_lower = mkc_timeseries::Annualizer<Num>::annualize_one(r.per_lower, annualizationFactor);
	r.m_sub     = mnR.m_sub;                // from MOutOfN result
	r.effB_mn   = mnR.effective_B;          // number of usable replicates
	r.effB_bca  = B;                        // BCa effective_B equals B here
      }

    r.resampler_name = chosenName;

    if (os) {
      (*os) << "   [Bootstrap] SmallNResampler=" << r.resampler_name
	    << "  (L_small=" << L_small << ")\n";
    }

    return r;
  }
} // namespace palvalidator::bootstrap_helpers
