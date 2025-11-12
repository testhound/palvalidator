#include "RobustnessAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip>
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include "DecimalConstants.h"
#include "filtering/TradingBootstrapFactory.h"
#include "BacktesterStrategy.h"
#include "SmallNBootstrapHelpers.h"
#include "Annualizer.h"

namespace palvalidator
{
  namespace analysis
  {
    using namespace mkc_timeseries;

    template <typename T>
    static inline double asDouble_(const T& x){ return x.getAsDouble(); }

    RobustnessResult RobustnessAnalyzer::runFlaggedStrategyRobustness(
								      const std::string& label,
								      const std::vector<Num>& returns,
								      size_t L_in,
								      double annualizationFactor,
								      const Num& finalRequiredReturn,
								      const RobustnessChecksConfig<Num>& cfg,
								      const mkc_timeseries::BacktesterStrategy<Num>& strategy,
								      BootstrapFactory& bootstrapFactory,
								      std::ostream& os)
    {
      using mkc_timeseries::DecimalConstants;
      using GeoStat      = mkc_timeseries::GeoMeanStat<Num>;
      using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
      namespace bh  = palvalidator::bootstrap_helpers;

      // ---------- Basic guards ----------
      const size_t n = returns.size();
      if (n == 0) {
	os << "   [ROBUST] " << label << ": empty return series. ThumbsDown.\n";
	return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::LSensitivityBound, 0.0};
      }

      // Clamp L safely (Item 3)
      const size_t L_eff = clampBlockLen_(L_in, n, cfg.minL);

      // ---------- Baseline (conservative small-N policy or BCa fallback) ----------
      const bool smallN = (n <= 40);
      Num lbPeriod_base = DecimalConstants<Num>::DecimalZero;
      Num lbAnnual_base = DecimalConstants<Num>::DecimalZero;

      if (smallN) {
	// One call: chooses IID vs Block(small L), runs m/n and BCa on SAME resampler, returns min
	auto s = bh::conservative_smallN_lower_bound<Num, GeoStat>(
								   returns,
								   L_eff,
								   annualizationFactor,
								   cfg.cl,                  // confidence
								   cfg.B,                   // resamples
								   /*rho_m auto*/ -1.0,     // use mn_ratio_from_n(n)
								   const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy), // API takes non-const
								   bootstrapFactory,
								   &os, /*stageTag*/3, /*fold*/0);

	lbPeriod_base = s.per_lower;
	lbAnnual_base = s.ann_lower;

	os << "   [ROBUST] " << label << " baseline (L=" << s.L_used << "): "
	   << "per-period Geo LB=" << (lbPeriod_base * DecimalConstants<Num>::DecimalOneHundred) << "%, "
	   << "annualized Geo LB=" << (lbAnnual_base * DecimalConstants<Num>::DecimalOneHundred) << "%  "
	   << "[SmallN: " << (s.resampler_name ? s.resampler_name : "n/a")
	   << ", m_sub=" << s.m_sub << ", L_small=" << s.L_used << "]\n";
      } else {
	// Larger-N fallback: BCa(Geo) with full stationary block resampler at L_eff
	BCaResampler sampler(L_eff);
	std::function<Num(const std::vector<Num>&)> geoFn = GeoStat();

	auto bcaGeo = bootstrapFactory.makeBCa<Num>(
						    returns, cfg.B, cfg.cl, geoFn, sampler,
						    const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy),
						    /*stageTag*/3, /*L*/L_eff, /*fold*/0);

	lbPeriod_base = bcaGeo.getLowerBound();
	lbAnnual_base = safeAnnualizeLB_(lbPeriod_base, annualizationFactor);

	os << "   [ROBUST] " << label << " baseline (L=" << L_eff << "): "
	   << "per-period Geo LB=" << (lbPeriod_base * DecimalConstants<Num>::DecimalOneHundred) << "%, "
	   << "annualized Geo LB=" << (lbAnnual_base * DecimalConstants<Num>::DecimalOneHundred) << "%  [BCa]\n";
      }

      // ---------- L-sensitivity with cached baseline (Item 7) ----------
      const auto ls = runLSensitivityWithCache_(
						returns, L_eff, annualizationFactor, lbAnnual_base, cfg, strategy, bootstrapFactory, os);

      // Fail if any L produced <= 0, or if min across L falls below the hurdle
      if (ls.anyFail || (ls.ann_min <= finalRequiredReturn)) {
	os << "   [ROBUST] L-sensitivity FAIL: LB below zero/hurdle at some L.\n";
	return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::LSensitivityBound, ls.relVar};
      }

      // Variability verdict uses symmetric near-hurdle helper (Item 6)
      const auto nh_l = nearHurdle_(lbAnnual_base, finalRequiredReturn, cfg);
      if (ls.relVar > cfg.relVarTol) {
	if (nh_l.near) {
	  os << "   [ROBUST] L-sensitivity FAIL: relVar=" << ls.relVar
	     << " > " << cfg.relVarTol << " and base LB near hurdle "
	     << "(Δabs=" << nh_l.distAbs << ", Δrel=" << nh_l.distRel << ").\n";
	  return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::LSensitivityVarNearHurdle, ls.relVar};
	} else {
	  os << "   [ROBUST] L-sensitivity PASS (high variability relVar=" << ls.relVar
	     << " but base LB comfortably above hurdle).\n";
	}
      } else {
	os << "   [ROBUST] L-sensitivity PASS (relVar=" << ls.relVar << ")\n";
      }

      // ---------- Split-sample with ACF-derived L & B bump (Item 4) ----------
      if (n >= cfg.minTotalForSplit) {
	const size_t mid = n / 2;
	const size_t n1  = mid;
	const size_t n2  = n - mid;

	if (n1 < cfg.minHalfForSplit || n2 < cfg.minHalfForSplit) {
	  os << "   [ROBUST] Split-sample SKIP (insufficient per-half data: "
	     << n1 << " & " << n2 << ")\n";
	} else {
	  std::vector<Num> r1(returns.begin(), returns.begin() + mid);
	  std::vector<Num> r2(returns.begin() + mid, returns.end());

	  const size_t hardMaxL1 = (n1 >= 2) ? (n1 - 1) : 1;
	  const size_t hardMaxL2 = (n2 >= 2) ? (n2 - 1) : 1;

	  const size_t L1 = suggestHalfLfromACF_(r1, cfg.minL, hardMaxL1);
	  const size_t L2 = suggestHalfLfromACF_(r2, cfg.minL, hardMaxL2);

	  const size_t B1 = adjustBforHalf_(cfg.B, n1);
	  const size_t B2 = adjustBforHalf_(cfg.B, n2);

	  // Half 1
	  Num lb1A_cons;
	  if (n1 <= 40) {
	    auto s1 = bh::conservative_smallN_lower_bound<Num, GeoStat>(
									r1, L1, annualizationFactor, cfg.cl, B1,
									/*rho_m auto*/ -1.0,
									const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy),
									bootstrapFactory, &os, /*stage*/3, /*fold*/1);
	    lb1A_cons = s1.ann_lower;
	    os << "   [ROBUST] Split-sample (ACF L) H1 L=" << s1.L_used << ", B=" << B1
	       << " → per=" << (s1.per_lower * DecimalConstants<Num>::DecimalOneHundred)
	       << "% (ann=" << (lb1A_cons * DecimalConstants<Num>::DecimalOneHundred)
	       << "%) [SmallN]\n";
	  } else {
	    BCaResampler pol1(L1);
	    std::function<Num(const std::vector<Num>&)> geoFn = GeoStat();
	    auto b1 = bootstrapFactory.makeBCa<Num>(r1, B1, cfg.cl, geoFn, pol1,
						    const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy), 3, L1, 1);
	    lb1A_cons = safeAnnualizeLB_(b1.getLowerBound(), annualizationFactor);
	    os << "   [ROBUST] Split-sample (ACF L) H1 L=" << L1 << ", B=" << B1
	       << " → ann=" << (lb1A_cons * DecimalConstants<Num>::DecimalOneHundred) << "% [BCa]\n";
	  }

	  // Half 2
	  Num lb2A_cons;
	  if (n2 <= 40) {
	    auto s2 = bh::conservative_smallN_lower_bound<Num, GeoStat>(
									r2, L2, annualizationFactor, cfg.cl, B2,
									/*rho_m auto*/ -1.0,
									const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy),
									bootstrapFactory, &os, /*stage*/3, /*fold*/2);
	    lb2A_cons = s2.ann_lower;
	    os << "   [ROBUST] Split-sample (ACF L) H2 L=" << s2.L_used << ", B=" << B2
	       << " → per=" << (s2.per_lower * DecimalConstants<Num>::DecimalOneHundred)
	       << "% (ann=" << (lb2A_cons * DecimalConstants<Num>::DecimalOneHundred)
	       << "%) [SmallN]\n";
	  } else {
	    BCaResampler pol2(L2);
	    std::function<Num(const std::vector<Num>&)> geoFn = GeoStat();
	    auto b2 = bootstrapFactory.makeBCa<Num>(r2, B2, cfg.cl, geoFn, pol2,
						    const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy), 3, L2, 2);
	    lb2A_cons = safeAnnualizeLB_(b2.getLowerBound(), annualizationFactor);
	    os << "   [ROBUST] Split-sample (ACF L) H2 L=" << L2 << ", B=" << B2
	       << " → ann=" << (lb2A_cons * DecimalConstants<Num>::DecimalOneHundred) << "% [BCa]\n";
	  }

	  if (lb1A_cons <= DecimalConstants<Num>::DecimalZero || lb2A_cons <= DecimalConstants<Num>::DecimalZero
	      || lb1A_cons <= finalRequiredReturn || lb2A_cons <= finalRequiredReturn) {
	    os << "   [ROBUST] Split-sample FAIL: a half falls to ≤ 0 or ≤ hurdle.\n";
	    return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::SplitSample, ls.relVar};
	  } else {
	    os << "   [ROBUST] Split-sample PASS\n";
	  }
	}
      } else {
	os << "   [ROBUST] Split-sample SKIP (n=" << n << " < " << cfg.minTotalForSplit << ")\n";
      }

      // ---------- Tail-risk sanity (Items 1,2,8) ----------
      std::vector<Num> returns_log;
      toLog1pVector_(returns, returns_log);
      std::sort(returns_log.begin(), returns_log.end(), [](const Num& a, const Num& b){ return a < b; });

      const double alphaEff = effectiveTailAlpha_(n, cfg.tailAlpha);
      const TailStats tlog = computeTailStatsType7_(returns_log, alphaEff);

      const Num q05_log      = tlog.q_alpha;
      const Num es05_log     = tlog.es_alpha;
      const Num lbLog_base   = toLog1p_(lbPeriod_base);

      const bool severe_tails =
	(q05_log < DecimalConstants<Num>::DecimalZero) &&
	(absNum_(q05_log) > cfg.tailMultiple * absNum_(lbLog_base));

      const auto nh_t = nearHurdle_(lbAnnual_base, finalRequiredReturn, cfg);

      // Human-friendly display in raw space
      std::vector<Num> sorted_raw = returns;
      std::sort(sorted_raw.begin(), sorted_raw.end(), [](const Num& a, const Num& b){ return a < b; });
      const TailStats tDisp = computeTailStatsType7_(sorted_raw, alphaEff);
      const Num q05_disp  = tDisp.q_alpha;
      const Num es05_disp = tDisp.es_alpha;

      os << "   [ROBUST] Tail risk (alpha=" << alphaEff << "): q05="
	 << (q05_disp * DecimalConstants<Num>::DecimalOneHundred) << "%, ES05="
	 << (es05_disp * DecimalConstants<Num>::DecimalOneHundred) << "%, "
	 << "severe=" << (severe_tails ? "yes" : "no") << ", "
	 << "borderline=" << (nh_t.near ? "yes" : "no") << "\n";

      logTailRiskExplanation(os, lbPeriod_base, q05_disp, es05_disp, cfg.tailMultiple.getAsDouble());

      if (severe_tails && nh_t.near) {
	os << "   [ROBUST] Tail risk FAIL (severe tails and borderline LB) → ThumbsDown.\n";
	return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::TailRisk, ls.relVar};
      }

      os << "   [ROBUST] All checks PASS → ThumbsUp.\n";
      return {RobustnessVerdict::ThumbsUp, RobustnessFailReason::None, ls.relVar};
    }
    
    Num RobustnessAnalyzer::annualizeLB_(const Num& perPeriodLB, double k)
    {
      return mkc_timeseries::Annualizer<Num>::annualize_one(perPeriodLB, k);
    }

    Num RobustnessAnalyzer::absNum_(const Num& x)
    {
      return (x < DecimalConstants<Num>::DecimalZero) ? -x : x;
    }

    // Empirical type-7 quantile (R default) on a sorted ascending array, plus fractional ES
    typename RobustnessAnalyzer::TailStats
    RobustnessAnalyzer::computeTailStatsType7_(const std::vector<Num>& xSortedAsc, double alpha)
    {
      TailStats out;
      const size_t n = xSortedAsc.size();
      if (n == 0 || alpha <= 0.0) {
        out.q_alpha = (n ? xSortedAsc.front() : DecimalConstants<Num>::DecimalZero);
        out.es_alpha = out.q_alpha;
        return out;
      }
      if (alpha >= 1.0) {
        out.q_alpha = xSortedAsc.back();
        // ES over full support == mean
        Num s = DecimalConstants<Num>::DecimalZero;
        for (const auto& v: xSortedAsc) s += v;
        out.es_alpha = s / Num(static_cast<int>(n));
        return out;
      }

      // --- type-7 quantile
      const double p = alpha;
      const double h = (n - 1) * p + 1.0;   // 1-indexed position
      const size_t a = static_cast<size_t>(std::floor(h));          // floor
      const double g = h - static_cast<double>(a);                   // fractional part in [0,1)

      const size_t ia = (a == 0 ? 0 : a - 1);                        // convert to 0-index
      const size_t ib = std::min(ia + 1, n - 1);

      const double qa = asDouble_(xSortedAsc[ia]);
      const double qb = asDouble_(xSortedAsc[ib]);
      const double q  = (1.0 - g) * qa + g * qb;
      out.q_alpha = Num(q);

      // --- fractional ES at alpha: average up to alpha mass with partial weight on cutoff
      // Mass per order statistic in type-7 picture ~ 1/(n-1); use m = (n-1)*alpha
      const double m = (n - 1) * alpha;
      const size_t j = static_cast<size_t>(std::floor(m));           // fully included count
      const double f = m - static_cast<double>(j);                    // partial mass for the (j)-th (0-index)

      Num sum = DecimalConstants<Num>::DecimalZero;
      if (j > 0) {
        for (size_t i = 0; i < j; ++i) sum += xSortedAsc[i];
      }
      if (j < n) {
        const size_t idx = std::min(j, n - 1);
        sum += xSortedAsc[idx] * Num(f);
      }
      const Num denom = Num(static_cast<int>(j)) + Num(f);
      out.es_alpha = (denom > DecimalConstants<Num>::DecimalZero) ? (sum / denom)
	: out.q_alpha;
      return out;
    }

    Num RobustnessAnalyzer::toLog1p_(const Num& r)
    {
      const double v = std::max(-0.999999, (DecimalConstants<Num>::DecimalOne + r).getAsDouble());
      return Num(std::log(v));
    }

    void RobustnessAnalyzer::toLog1pVector_(const std::vector<Num>& in, std::vector<Num>& out)
    {
      out.resize(in.size());
      for (size_t i = 0; i < in.size(); ++i) {
        out[i] = toLog1p_(in[i]);
      }
    }

    size_t RobustnessAnalyzer::clampBlockLen_(size_t Ltry, size_t n, size_t minL)
    {
      if (n <= 1)
	return std::max<size_t>(1, minL);
 
      const size_t maxL = (n >= 2) ? (n - 1) : 1;
      const size_t L1   = std::max(Ltry, minL);

      return std::min(L1, maxL);
    }

    //
    size_t RobustnessAnalyzer::suggestHalfLfromACF_(const std::vector<Num>& rHalf,
						    size_t minL,
						    size_t hardMaxL)
    {
      const size_t n = rHalf.size();
      if (n == 0)
	return std::max<size_t>(1, minL);

      // ---- Small-sample guard: skip ACF if half is too short
      // Rationale: with n≈10–17 the ACF estimate is high-variance and can be misleading.
      // Threshold 30 is conservative; you can raise to 40 if you like.

      constexpr size_t kMinNForACF = 30;
      if (n < kMinNForACF)
	{
	  // fallback heuristic: n^(1/3), then clamp
	  const size_t h = std::max<size_t>(minL, static_cast<size_t>(std::llround(std::cbrt(static_cast<double>(n)))));
	  return std::min(h, (hardMaxL == 0 ? h : hardMaxL));
	}

      // ---- Try ACF-based suggestion; fall back to cube-root if it fails/returns 0
      size_t L_suggest = 0;
      try
	{
	  const size_t n = rHalf.size();
	  const size_t maxLag = std::min<size_t>(hardMaxL, (n > 1) ? (n - 1) : 1);
	  std::vector<Num> acf = StatUtils<Num>::computeACF(rHalf, maxLag);
	  L_suggest = StatUtils<Num>::suggestStationaryBlockLengthFromACF(acf, n, minL, hardMaxL);
	}
      catch (...)
	{
	  L_suggest = 0;
	}

      if (L_suggest == 0)
        L_suggest = std::max<size_t>(minL, static_cast<size_t>(std::llround(std::cbrt(static_cast<double>(n)))));

      // Final clamps
      L_suggest = std::max(L_suggest, minL);
      
      if (hardMaxL > 0)
	L_suggest = std::min(L_suggest, hardMaxL);

      return L_suggest;
    }


    size_t RobustnessAnalyzer::adjustBforHalf_(size_t B, size_t nHalf)
    {
      // Light stabilization bump for halves (configurable later if desired)
      // Keep it modest to avoid perf regressions:
      if (nHalf < 128)
	return std::max<size_t>(B, 1500);

      return B;
    }

    Num RobustnessAnalyzer::safeAnnualizeLB_(const Num& perPeriodLB, double k, double eps)
    {
      return mkc_timeseries::Annualizer<Num>::annualize_one(perPeriodLB, k, eps);
    }

    typename RobustnessAnalyzer::HurdleCloseness
    RobustnessAnalyzer::nearHurdle_(const Num& lbAnnual_base,
        const Num& finalRequiredReturn,
        const RobustnessChecksConfig<Num>& cfg)
    {
      const double baseA = lbAnnual_base.getAsDouble();
      const double hurA  = finalRequiredReturn.getAsDouble();
      const double distA = baseA - hurA;
      const double denom = std::max(std::abs(hurA), 1e-12);
      const double distR = distA / denom;
      
    const bool near =
      (lbAnnual_base <= (finalRequiredReturn + cfg.varOnlyMarginAbs)) ||
      (distR <= cfg.varOnlyMarginRel);

    return {near, distA, distR};
    }

    typename RobustnessAnalyzer::LSweepResult
    RobustnessAnalyzer::runLSensitivityWithCache_(
						  const std::vector<Num>& returns,
						  size_t L_baseline,
						  double annualizationFactor,
						  const Num& lbAnnual_base,
						  const RobustnessChecksConfig<Num>& cfg,
						  const mkc_timeseries::BacktesterStrategy<Num>& strategy,
						  BootstrapFactory& bootstrapFactory,
						  std::ostream& os)
    {
      using mkc_timeseries::DecimalConstants;
      using GeoStat      = mkc_timeseries::GeoMeanStat<Num>;
      using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
      namespace bh = palvalidator::bootstrap_helpers;

      const size_t n      = returns.size();
      const bool   smallN = (n <= 40);

      // Clamp Ls (baseline and neighbors)
      const size_t L0 = clampBlockLen_(L_baseline, n, cfg.minL);
      const size_t Lm = (L0 > cfg.minL) ? clampBlockLen_(L0 - 1, n, cfg.minL) : L0;
      const size_t Lp = clampBlockLen_(L0 + 1, n, cfg.minL);

      // Initialize sweep using the cached baseline annualized LB
      Num ann_min = lbAnnual_base;
      Num ann_max = lbAnnual_base;
      bool anyFail = false;

      os << "   [ROBUST] L-sensitivity:";

      auto evalL = [&](size_t Ltry, int foldTag)
      {
	if (Ltry == L0) {
	  os << "  L=" << L0 << " (base);";
	  return; // baseline already computed by caller
	}

	Num lbA_cons;

	if (smallN) {
	  // One-call conservative engine: picks IID vs Block(small L), runs m/n & BCa, returns min
	  auto s = bh::conservative_smallN_lower_bound<Num, GeoStat>(
								     returns,
								     Ltry,
								     annualizationFactor,
								     cfg.cl,
								     cfg.B,
								     /*rho_m auto*/ -1.0,
								     const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy),
								     bootstrapFactory,
								     &os, /*stageTag=*/3, /*fold=*/foldTag);

	  lbA_cons = s.ann_lower;

	  os << "  L=" << Ltry
	     << " → per=" << (s.per_lower * DecimalConstants<Num>::DecimalOneHundred) << "%,"
	     << " ann="   << (lbA_cons     * DecimalConstants<Num>::DecimalOneHundred) << "%;"
	     << "";
	} else {
	  // Larger-N: BCa(Geo) with full stationary block resampler at Ltry
	  BCaResampler sampler(Ltry);
	  std::function<Num(const std::vector<Num>&)> geoFn = GeoStat();
	  auto b = bootstrapFactory.makeBCa<Num>(
						 returns, cfg.B, cfg.cl, geoFn, sampler,
						 const_cast<mkc_timeseries::BacktesterStrategy<Num>&>(strategy),
						 /*stageTag=*/3, /*L=*/Ltry, /*fold=*/foldTag);

	  const Num lbP_bca = b.getLowerBound();
	  lbA_cons = safeAnnualizeLB_(lbP_bca, annualizationFactor);

	  os << "  L=" << Ltry
	     << " [BCa] → per=" << (lbP_bca * DecimalConstants<Num>::DecimalOneHundred) << "%,"
	     << " ann="         << (lbA_cons * DecimalConstants<Num>::DecimalOneHundred) << "%;";
	}

	// Track extrema & failures
	ann_min = (lbA_cons < ann_min) ? lbA_cons : ann_min;
	ann_max = (lbA_cons > ann_max) ? lbA_cons : ann_max;
	if (lbA_cons <= DecimalConstants<Num>::DecimalZero) anyFail = true;
      };

      // Evaluate neighbors around baseline (use distinct fold tags for traceability)
      evalL(Lm, /*foldTag=*/1);
      os << "  L=" << L0 << " (base);";
      evalL(Lp, /*foldTag=*/2);
      os << "\n";

      // Simple relative variability summary (range / max)
      double relVar = 0.0;
      if (ann_max > DecimalConstants<Num>::DecimalZero) {
	relVar = (ann_max.getAsDouble() - ann_min.getAsDouble()) /
	  ann_max.getAsDouble();
      }

      return {ann_min, ann_max, relVar, anyFail};
    }
    
    double RobustnessAnalyzer::effectiveTailAlpha_(size_t n, double alpha)
    {
      if (n == 0)
	return 0.0;

      // Ensure at least ~1 observation in tail when possible; cap to avoid alpha > 0.5
      const double minAlpha = (n >= 20) ? alpha : std::min(0.5, std::max(alpha, 1.0 / static_cast<double>(n)));
      return minAlpha;
    }

    void RobustnessAnalyzer::logTailRiskExplanation(
						    std::ostream& os,
						    const Num& perPeriodGMLB,
						    const Num& q05,
						    const Num& es05,
						    double severeMultiple)
    {
      const double edge = std::abs(perPeriodGMLB.getAsDouble());
      const double q    = std::abs(q05.getAsDouble());
      const double es   = std::abs(es05.getAsDouble());

      double multQ  = std::numeric_limits<double>::infinity();
      double multES = std::numeric_limits<double>::infinity();
      if (edge > 0.0) {
        multQ  = q  / edge;
        multES = es / edge;
      }

      std::ostream::fmtflags f(os.flags());
      os << "      \u2022 Tail-risk context: a 5% bad day (q05) is about "
	 << std::fixed << std::setprecision(2) << multQ
	 << "\u00D7 your conservative per-period edge; average of bad days (ES05) \u2248 "
	 << multES << "\u00D7.\n"
	 << "        (Heuristic: flag 'severe' when q05 exceeds "
	 << std::setprecision(2) << severeMultiple
	 << "\u00D7 the per-period GM lower bound.)\n";
      os.flags(f);
    }
  } // namespace analysis
} // namespace palvalidator
