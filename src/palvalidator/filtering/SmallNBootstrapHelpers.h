#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <string>
#include <ostream>
#include <optional>
#include <random>
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "TradingBootstrapFactory.h"
#include "BootstrapConfig.h"
#include "Annualizer.h"

namespace palvalidator::bootstrap_helpers
{

  // -----------------------------------------------------------------------------
  // Lightweight result carriers used by stages (kept simple on purpose)
  // -----------------------------------------------------------------------------
  template<class Num>
  struct MNRunSimple
  {
    Num          lower{};         // per-period LB
    std::size_t  m_sub{0};
    std::size_t  L{0};
    std::size_t  effective_B{0};
  };

  template<class Num>
  struct PTRunSimple
  {
    Num          lower{};         // per-period LB
    std::size_t  m_outer{0};
    std::size_t  m_inner{0};
    std::size_t  L{0};
    std::size_t  effective_B{0};
  };

  // -----------------------------------------------------------------------------
  // Simple dependence proxies & small-N heuristics
  // -----------------------------------------------------------------------------

  // Longest sign run (very cheap dependence signal)
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

  // Fraction of strictly positive returns
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

  inline bool has_heavy_tails_wide(double skew, double exkurt)
  {
    // Prior behavior might have been stricter; widen slightly:
    // - |skew| >= 0.90, or
    // - excess kurtosis >= 1.20
    return (std::fabs(skew) >= 0.90) || (exkurt >= 1.20);
  }

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
  
  // m/n rule for small N (raise m; keep m < n with guards)
  inline double mn_ratio_from_n(std::size_t n)
  {
    if (n == 0) return 1.0;

    // Target ~0.80*n with sensible floors/ceilings
    double m_target = std::ceil(0.80 * static_cast<double>(n));
    double m_floor  = 16.0;                                     // raised floor
    double m_ceil   = static_cast<double>((n > 2) ? (n - 2) : n); // ensure m < n

    double m = std::min(std::max(m_target, m_floor), m_ceil);
    if (m < 2.0) m = 2.0;

    return m / static_cast<double>(n);
  }

  // -----------------------------------------------------------------------------
  // Runtime→template dispatch for "SmallNResampler"
  // Calls fn(resampler, ratio_pos, use_block, L_small) with either IID or Block.
  // The return type is whatever fn returns (kept generic).
  // -----------------------------------------------------------------------------
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

    // Return the ceil(alpha * sims)-th order statistic of the MC longest-run distribution
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

    // Decide median-of-present vs min(all) based on proximity to hurdle (in bps, annualized).
    // parts_per are per-period LBs (e.g., daily). We compare in annualized space.
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

    template <class Num>
    inline Num min_of(const std::vector<Num>& v) {
      return *std::min_element(v.begin(), v.end(),
			       [](const Num& a, const Num& b){ return a < b; });
    }

    template <class Num>
    inline Num median_of_2_or_3(std::vector<Num> v) {
      std::sort(v.begin(), v.end(), [](const Num& a, const Num& b){ return a < b; });
      if (v.size() == 2) return v[0] + (v[1] - v[0]) / Num(2);
      return v[1]; // size()==3
    }

    // vote2==true → 2-of-3 (median of present); vote2==false → strict min(all)
    template <class Num>
    inline Num combine_LBs_2of3_or_min(const std::vector<Num>& parts, bool vote2) {
      if (!vote2 || parts.empty()) return min_of(parts);
      if (parts.size() == 1)      return parts.front();
      if (parts.size() == 2)      return median_of_2_or_3(parts);
      return median_of_2_or_3(parts);
    }

    // Consistent policy log line used across stages
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

  // -----------------------------------------------------------------------------
  // Optional: one-call "conservative small-N" runner
  // Runs M-out-of-N with the chosen SmallN resampler and also BCa with same
  // resampler; returns the MIN per-period LB and its annualized value.
  // This encapsulates common stage logic while keeping them configurable.
  // -----------------------------------------------------------------------------
  template <typename Num, typename GeoStat, typename StrategyT>
  struct SmallNConservativeResult {
    Num         per_lower{};       // combined per-period LB (min of engines)
    Num         ann_lower{};       // annualized LB
    std::size_t m_sub{0};          // chosen m for m/n
    std::size_t L_used{0};         // small-N block length actually used (2..3)
    std::size_t effB_mn{0};
    std::size_t effB_bca{0};
    const char* resampler_name{""};
  };

  template <typename Num, typename GeoStat, typename StrategyT>
  inline SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
  		  std::size_t L,
  		  double annualizationFactor,
  		  double confLevel,
  		  std::size_t B,
  		  double rho_m,                        // if <=0 → compute via mn_ratio_from_n
  		  StrategyT& strategy,
  		  palvalidator::bootstrap_cfg::BootstrapFactory& bootstrapFactory,
  		  std::ostream* os = nullptr,
  		  int stageTag = 3, int fold = 0)
  {
    using mkc_timeseries::DecimalConstants;

    const std::size_t n = returns.size();
    const double rho = (rho_m > 0.0 ? rho_m : mn_ratio_from_n(n));

    const char* chosenName = nullptr;
    std::size_t L_small = 0;

    auto out = dispatch_smallN_resampler<Num>(
					      returns, L,
					      [&](auto& resampler, double /*ratio_pos*/, bool /*use_block*/, std::size_t Ls){
						using ResamplerT = std::decay_t<decltype(resampler)>;

						// M-out-of-N
						GeoStat statGeo;
						auto [mnBoot, mnCrn] =
						  bootstrapFactory.template makeMOutOfN<Num, GeoStat, ResamplerT>(
														  B, confLevel, rho, resampler, strategy, stageTag, /*L*/Ls, fold);

						auto mnR = mnBoot.run(returns, GeoStat(), mnCrn);
						const Num lbP_mn = mnR.lower;

						// BCa with same resampler (comparability)
						auto bca = bootstrapFactory.makeBCa<Num>(
											 returns, B, confLevel, statGeo, resampler, strategy, stageTag, /*L*/Ls, fold);
						const Num lbP_bca = bca.getLowerBound();

						SmallNConservativeResult<Num, GeoStat, StrategyT> r;
						r.per_lower     = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
						r.ann_lower     = mkc_timeseries::Annualizer<Num>::annualize_one(r.per_lower, annualizationFactor);
						r.m_sub         = mnR.m_sub;
						r.L_used        = mnR.L;
						r.effB_mn       = mnR.effective_B;
						r.effB_bca      = B; // BCa effective_B equals B here
						return r;
					      },
					      &chosenName, &L_small
					      );

    out.resampler_name = chosenName;
    out.L_used         = (out.L_used == 0 ? L_small : out.L_used);

    if (os) {
      (*os) << "   [Bootstrap] SmallNResampler=" << chosenName
	    << "  (L_small=" << L_small << ")\n";
    }

    return out;
  }

  template <typename Num, typename GeoStat, typename StrategyT>
  inline SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
				  std::size_t L,
				  double annualizationFactor,
				  double confLevel,
				  std::size_t B,
				  double rho_m,                        // if <=0 → compute via mn_ratio_from_n
				  StrategyT& strategy,
				  palvalidator::bootstrap_cfg::BootstrapFactory& bootstrapFactory,
				  std::ostream* os,
				  int stageTag, int fold,
				  std::optional<bool> heavy_tails_override)
  {
    using mkc_timeseries::DecimalConstants;
    using IIDResampler  = mkc_timeseries::IIDResampler<Num>;
    using BlockValueRes = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    const std::size_t n   = returns.size();
    const double      rho = (rho_m > 0.0 ? rho_m : mn_ratio_from_n(n));

    // Small-N dependence proxies
    const double      ratio_pos = sign_positive_ratio(returns);
    const std::size_t runlen    = longest_sign_run(returns);
    const std::size_t L_small   = clamp_smallL(L);

    // --- Decide resampler with optional heavy-tail override and MC guard ------
    bool use_block = false;

    if (heavy_tails_override.has_value()) {
      // Caller (e.g., LSensitivity) forces the choice based on widened tails
      use_block = *heavy_tails_override;  // true = block, false = IID
    }
    else
      {
	// First: cheap heuristic (sign-imbalance OR obvious streakiness)
	const bool choose_block_fast = choose_block_smallN(ratio_pos, n, runlen);

	if (choose_block_fast)
	  {
	    use_block = true;
	  }
	else if (n <= 40)
	  {
	    // Borderline zone: consult a tiny MC 1-sided runs test at 95% quantile
	    using palvalidator::bootstrap_helpers::internal::borderline_run_exceeds_MC95;
	    use_block = borderline_run_exceeds_MC95(n, ratio_pos, runlen);
	  }
	else
	  {
	    use_block = false; // large-n → IID unless overridden elsewhere
	  }
      }

    const char* chosenName = use_block
      ? "StationaryMaskValueResamplerAdapter(small L)"
      : "IIDResampler";

    // ------------------------- Run the engines on the chosen resampler --------
    SmallNConservativeResult<Num, GeoStat, StrategyT> r{};
    r.L_used = L_small;

    if (use_block)
      {
	BlockValueRes resampler(L_small);

	// m-out-of-n on SAME resampler
	GeoStat statGeo;
	auto [mnBoot, mnCrn] =
	  bootstrapFactory.template makeMOutOfN<Num, GeoStat, BlockValueRes>(B,
									     confLevel,
									     rho,
									     resampler,
									     strategy,
									     stageTag,
									     /*L*/L_small,
									     fold);

	auto mnR      = mnBoot.run(returns, GeoStat(), mnCrn);
	const double mn_ratio   = (n > 0) ? (static_cast<double>(mnR.m_sub) / static_cast<double>(n)) : 0.0;
	const double shrinkRate = 1.0 - mn_ratio;

	if (os) {
	  (*os) << "   [Bootstrap] m_sub=" << mnR.m_sub
		<< "  n=" << n
		<< "  m/n=" << std::fixed << std::setprecision(3) << mn_ratio
		<< "  shrink=" << std::fixed << std::setprecision(3) << shrinkRate
		<< "\n";
	}
	
	const Num lbP_mn  = mnR.lower;

	// BCa on SAME resampler
	auto bca     = bootstrapFactory.makeBCa<Num>(
						     returns, B, confLevel, statGeo, resampler,
						     strategy, stageTag, /*L*/L_small, fold);
	const Num lbP_bca = bca.getLowerBound();

	r.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
	r.ann_lower = mkc_timeseries::Annualizer<Num>::annualize_one(r.per_lower, annualizationFactor);
	r.m_sub     = mnR.m_sub;
	r.effB_mn   = mnR.effective_B;
	r.effB_bca  = B;
      }
    else
      {
	IIDResampler resampler;

	// m-out-of-n on SAME resampler
	GeoStat statGeo;
	auto [mnBoot, mnCrn] =
	  bootstrapFactory.template makeMOutOfN<Num, GeoStat, IIDResampler>(B,
									    confLevel,
									    rho,
									    resampler,
									    strategy,
									    stageTag,
									    /*L*/L_small,
									    fold);

	auto mnR      = mnBoot.run(returns, GeoStat(), mnCrn);

	const double mn_ratio   = (n > 0) ? (static_cast<double>(mnR.m_sub) / static_cast<double>(n)) : 0.0;
	const double shrinkRate = 1.0 - mn_ratio;

	if (os) {
	  (*os) << "   [Bootstrap] m_sub=" << mnR.m_sub
		<< "  n=" << n
		<< "  m/n=" << std::fixed << std::setprecision(3) << mn_ratio
		<< "  shrink=" << std::fixed << std::setprecision(3) << shrinkRate
		<< "\n";
	}
	
	const Num lbP_mn  = mnR.lower;

	// BCa on SAME resampler
	auto bca     = bootstrapFactory.makeBCa<Num>(
						     returns, B, confLevel, statGeo, resampler,
						     strategy, stageTag, /*L*/L_small, fold);
	const Num lbP_bca = bca.getLowerBound();

	r.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
	r.ann_lower = mkc_timeseries::Annualizer<Num>::annualize_one(r.per_lower, annualizationFactor);
	r.m_sub     = mnR.m_sub;
	r.effB_mn   = mnR.effective_B;
	r.effB_bca  = B;
      }

    r.resampler_name = chosenName;

    if (os) {
      (*os) << "   [Bootstrap] SmallNResampler=" << chosenName
	    << "  (L_small=" << L_small << ")\n";
    }

    return r;
  }
} // namespace palvalidator::bootstrap_helpers
