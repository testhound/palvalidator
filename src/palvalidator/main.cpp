#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <variant>
#include <algorithm>
#include <set>
#include <numeric>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include "ValidatorConfiguration.h"
#include "SecurityAttributesFactory.h"
#include "PALMastersMonteCarloValidation.h"
#include "PALRomanoWolfMonteCarloValidation.h"
#include "PALMonteCarloValidation.h"
#include "MonteCarloPermutationTest.h"
#include "MultipleTestingCorrection.h"
#include "PermutationTestComputationPolicy.h"
#include "PermutationTestResultPolicy.h"
#include "MonteCarloTestPolicy.h"
#include "PermutationStatisticsCollector.h"
#include "LogPalPattern.h"
#include "number.h"
#include <cstdlib>

// New policy architecture includes
#include "PolicyRegistry.h"
#include "PolicyConfiguration.h"
#include "PolicyFactory.h"
#include "PolicySelector.h"
#include "PolicyRegistration.h"
#include "ValidationInterface.h"
#include "BiasCorrectedBootstrap.h"

using namespace mkc_timeseries;

using Num = num::DefaultNumber;

// ---- Enums, Structs, and Helper Functions ----

enum class ValidationMethod
{
    Masters,
    RomanoWolf,
    BenjaminiHochberg,
    Unadjusted
};

// ComputationPolicy enum removed - now using dynamic policy selection

struct ValidationParameters
{
    unsigned long permutations;
    Num pValueThreshold;
    Num falseDiscoveryRate; // For Benjamini-Hochberg
};

std::string getValidationMethodString(ValidationMethod method)
{
    switch (method)
    {
        case ValidationMethod::Masters:
            return "Masters";
        case ValidationMethod::RomanoWolf:
            return "RomanoWolf";
        case ValidationMethod::BenjaminiHochberg:
            return "BenjaminiHochberg";
        case ValidationMethod::Unadjusted:
            return "Unadjusted";
        default:
            throw std::invalid_argument("Unknown validation method");
    }
}

// getComputationPolicyString function removed - now using dynamic policy names

static std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%b_%d_%Y_%H%M");
    return ss.str();
}

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol, ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

static std::string createDetailedSurvivingPatternsFileName (const std::string& securitySymbol,
                                                            ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

static std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                          ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_RejectedPatterns_" + getCurrentTimestamp() + ".txt";
}

void writeBacktestPerformanceReport(std::ofstream& file, std::shared_ptr<BackTester<Num>> backtester)
{
    auto positionHistory = backtester->getClosedPositionHistory();
    
    // Write performance metrics to file
    file << "=== Backtest Performance Report ===" << std::endl;
    file << "Total Closed Positions: " << positionHistory.getNumPositions() << std::endl;
    file << "Number of Winning Trades: " << positionHistory.getNumWinningPositions() << std::endl;
    file << "Number of Losing Trades: " << positionHistory.getNumLosingPositions() << std::endl;
    file << "Total Bars in Market: " << positionHistory.getNumBarsInMarket() << std::endl;
    file << "Percent Winners: " << positionHistory.getPercentWinners() << "%" << std::endl;
    file << "Percent Losers: " << positionHistory.getPercentLosers() << "%" << std::endl;
    file << "Profit Factor: " << positionHistory.getProfitFactor() << std::endl;
    file << "High Resolution Profit Factor: " << positionHistory.getHighResProfitFactor() << std::endl;
    file << "PAL Profitability: " << positionHistory.getPALProfitability() << "%" << std::endl;
    file << "High Resolution Profitability: " << positionHistory.getHighResProfitability() << std::endl;
    file << "===================================" << std::endl << std::endl;
}

// Calculate theoretical PAL profitability based on strategy's risk/reward parameters
template<typename Num>
Num calculateTheoreticalPALProfitability(std::shared_ptr<PalStrategy<Num>> strategy,
                                         Num targetProfitFactor = DecimalConstants<Num>::DecimalTwo)
{
    auto pattern = strategy->getPalPattern();
    Num target = pattern->getProfitTargetAsDecimal();
    Num stop = pattern->getStopLossAsDecimal();
    
    if (stop == DecimalConstants<Num>::DecimalZero) {
        return DecimalConstants<Num>::DecimalZero;
    }
    
    Num payoffRatio = target / stop;
    Num oneHundred = DecimalConstants<Num>::DecimalOneHundred;
    
    // Formula from BootStrappedProfitabilityPFPolicy::getPermutationTestStatistic
    Num expectedPALProfitability = (targetProfitFactor / (targetProfitFactor + payoffRatio)) * oneHundred;
    
    return expectedPALProfitability;
}

template<typename Num>
struct RobustnessChecksConfig {
  unsigned int B = 1200;
  double cl = 0.95;

  // L-sensitivity
  double relVarTol = 0.25;        // keep 0.25
  size_t minL = 2;

  // Split-sample eligibility
  size_t minTotalForSplit = 40;
  size_t minHalfForSplit  = 20;

  // Tail risk
  double tailAlpha = 0.05;
  Num    tailMultiple = Num("3.0");
  Num    borderlineAnnualMargin = Num("0.02");

  // NEW: only fail on variability if we're close to the hurdle
  Num    varOnlyMarginAbs = Num("0.02");  // within +2% annual of hurdle
  double varOnlyMarginRel = 0.25;         // or within +25% relative to hurdle
};

enum class RobustnessVerdict { ThumbsUp, ThumbsDown };

template<typename Num>
static inline Num annualizeLB_(const Num& perPeriodLB, double k) {
  const Num one = DecimalConstants<Num>::DecimalOne;
  return Num(std::pow((one + perPeriodLB).getAsDouble(), k)) - one;
}

template<typename Num>
static inline Num absNum_(const Num& x) {
  return (x < DecimalConstants<Num>::DecimalZero) ? -x : x;
}

enum class RobustnessFailReason {
  None = 0,
  LSensitivityBound,        // a bound at {L-1,L,L+1} ≤ 0 or ≤ hurdle
  LSensitivityVarNearHurdle,// variability too high AND base near hurdle
  SplitSample,              // a half ≤ 0 or ≤ hurdle
  TailRisk                  // severe tails + borderline base
};

struct RobustnessResult {
  RobustnessVerdict verdict;
  RobustnessFailReason reason;
  double relVar; // for logging/diagnostics
};

/// Prints a human-readable explanation of tail risk metrics (q05, ES05)
/// relative to the conservative per-period edge (GM BCa lower bound).
///
/// q05: 5% worst-case one-period loss threshold ("bad-day cutoff").
/// ES05: average loss within the worst 5% of periods ("how bad are bad days, on average").
///
/// We scale both by |perPeriodGMLB| to communicate how many times larger a bad day is
/// than the conservative edge you’re compounding at.
///
/// Example line emitted:
///   "• Tail-risk context: a 5% bad day (q05) is about 9.12× your conservative per-period edge;
///    average of bad days (ES05) ≈ 12.80×. (Flag 'severe' when q05 > 3× edge.)"
// ---- Tail-risk explanation helper ----
template <class Num>
void logTailRiskExplanation(std::ostream& os,
                            const Num& perPeriodGMLB,
                            const Num& q05,
                            const Num& es05,
                            double severeMultiple /* e.g., cfg.tailMultiple */)
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

/**
 * @brief Run GM-only robustness checks for strategies flagged by AM–GM divergence.
 *
 * This routine is **diagnostic**. We ultimately accept/reject by the
 * GEOMETRIC-mean (GM) BCa lower bound vs the hurdle. However, when the caller
 * detects a large AM–GM divergence, this function probes whether the GM result
 * itself is stable under small, realistic perturbations of the resampling
 * scheme and the sample window.
 *
 * What it does (all GM-based):
 *  1) Baseline LB (per-period and annualized): compute BCa LB using the
 *     current block policy (e.g., StationaryBlockResampler) with block length
 *     L and the caller’s annualization factor. This mirrors the production
 *     filter settings.
 *
 *  2) L-sensitivity (block-length wiggle): recompute the GM LB at L-1, L,
 *     and L+1 (clamped to ≥2). This catches dependence on the precise block
 *     size.
 *        - Immediate FAIL if any tested LB ≤ 0 or ≤ hurdleAnn.
 *        - Otherwise compute relative variability:
 *              relVar = (max_annLB - min_annLB) / max_annLB
 *          If relVar > cfg.relVarTol (default ~0.25), we _only_ FAIL when the
 *          baseline annualized LB is “near the hurdle”:
 *              near = (abs(LB_ann - hurdleAnn) ≤ cfg.nearHurdleAbs)
 *                     OR (abs(LB_ann - hurdleAnn)/hurdleAnn ≤ cfg.nearHurdleRel)
 *          If baseline is comfortably above the hurdle, we PASS despite high
 *          relVar (we still log that variability).
 *
 *  3) Split-sample stability (optional, size-gated):
 *        - Skip entirely if returns.size() < cfg.minTotalForSplit (e.g., 40),
 *          or if either half would be < cfg.minHalfForSplit (e.g., 20).
 *        - Otherwise split the series into first/second halves and compute
 *          GM BCa LBs with the same policy and L.
 *        - FAIL if either half’s annualized LB ≤ 0 or ≤ hurdleAnn.
 *
 *  4) Tail-risk sanity (advisory):
 *        - Compute empirical lower α-quantile (qα, α=cfg.tailAlpha, e.g., 5%)
 *          and expected shortfall ESα on **per-period** raw returns.
 *        - Mark “severe” if |qα| > cfg.tailMultiple × (baseline per-period GM LB).
 *          This is for logging; it does not, by itself, cause FAIL unless you
 *          choose to treat “severe && near-hurdle” as a policy failure.
 *
 * Inputs
 *  @param name                 Strategy name (used for logging).
 *  @param returns              Per-period net returns (after costs/slippage).
 *                              These are the same units used in your BCa filter.
 *  @param L                    Block length for the block sampler (e.g., median
 *                              holding period, clamped ≥ 2).
 *  @param annualizationFactor  Multiplier to annualize per-period LBs (e.g.,
 *                              252 for daily, ~52 for weekly, etc.).
 *  @param hurdleAnn            Annualized hurdle used in the main filter (max of
 *                              cost and risk-free, or any policy you apply).
 *  @param cfg                  Thresholds and toggles for the checks:
 *                                - relVarTol        : tolerance for L-sensitivity
 *                                                     variability (default ~0.25)
 *                                - nearHurdleAbs    : absolute “near hurdle” band
 *                                - nearHurdleRel    : relative “near hurdle” band
 *                                - minTotalForSplit : minimum n to attempt split
 *                                - minHalfForSplit  : minimum n per half
 *                                - tailAlpha        : tail quantile (e.g., 0.05)
 *                                - tailMultiple     : multiple for “severe” flag
 *  @param os                   Stream for human-readable diagnostics (defaults
 *                              to std::cout). Nothing is thrown on logging.
 *
 * Output
 *  @return RobustnessVerdict   A small enum/struct indicating:
 *                                - ThumbsUp (all checks passed or skipped)
 *                                - Fail_Lbound      (LB ≤ 0 or ≤ hurdle at some L)
 *                                - Fail_LvariabilityNearHurdle (relVar too high
 *                                  AND baseline LB near hurdle)
 *                                - Fail_SplitSample (a half fails LB > hurdle)
 *                                - Fail_TailRisk    (only if you choose to enforce)
 *                                - Skip_SmallSample (split skipped; not a failure)
 *
 * Notes & intent
 *  - This function **does not replace** your primary selection rule (GM LB vs
 *    hurdle). It is invoked only for “flagged” cases (e.g., large AM–GM LB gap)
 *    and aims to catch fragile strategies whose GM LB is sensitive to block
 *    length or sub-sample choice.
 *  - All computations are GM-centric; AM is not used here for any decision.
 *  - Tail-risk is advisory by default; it helps you decide between exclusion
 *    vs down-weighting.
 *  - Determinism: it uses the same bootstrap/jackknife policies as production.
 *    If you seed your RNG upstream, the results are reproducible.
 *
 * Complexity
 *  - Roughly O(B · n · (#L-tests + #splits)), where B is the bootstrap size,
 *    n is sample length. Size-gating avoids expensive splits on small n.
 *
 * Example
 *  RobustnessChecksConfig<Num> cfg{};
 *  cfg.relVarTol = Num("0.25");
 *  cfg.minTotalForSplit = 40;
 *  auto verdict = runFlaggedStrategyRobustness<Num>(
 *      strategyName, returns, L, 252.0, finalRequiredReturn, cfg, std::cout
 *  );
 *  if (verdict.isFail()
 *   {// Exclude or down-weith the strategy}
 */

template<typename Num>
RobustnessResult runFlaggedStrategyRobustness(
    const std::string& label,
    const std::vector<Num>& returns,    // per-period returns after slippage, etc.
    size_t L_in,                        // median holding bars (inclusive) -> block length to use
    double annualizationFactor,         // k
    const Num& finalRequiredReturn,     // hurdle (annual)
    const RobustnessChecksConfig<Num>& cfg /* has varOnlyMarginAbs/varOnlyMarginRel */,
    std::ostream& os)
{
  using Sampler  = StationaryBlockResampler<Num>;
  using BlockBCA = BCaBootStrap<Num, Sampler>;
  GeoMeanStat<Num> statGeo;

  const size_t n = returns.size();
  const size_t L = std::max(cfg.minL, L_in);

  if (n == 0)
    {
      os << "   [ROBUST] " << label << ": empty return series. ThumbsDown.\n";
      return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::LSensitivityBound, 0.0};
    }

  if (n < cfg.minTotalForSplit)
    {
      os << "   [ROBUST] " << label << ": small sample (" << n
	 << "). Will SKIP split-sample; running L-sensitivity and tail-risk only.\n";
    }

  // Baseline
  Sampler pol_base(L);
  BlockBCA bca_base(returns, cfg.B, cfg.cl, statGeo, pol_base);
  const Num lbPeriod_base = bca_base.getLowerBound();
  const Num lbAnnual_base = annualizeLB_<Num>(lbPeriod_base, annualizationFactor);

  os << "   [ROBUST] " << label << " baseline (L=" << L << "): "
     << "per-period Geo LB=" << (lbPeriod_base * DecimalConstants<Num>::DecimalOneHundred) << "%, "
     << "annualized Geo LB=" << (lbAnnual_base * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

  /*The L-sensitivity test directly addresses a known challenge with block bootstrap methods:
   * the optimal choice of the block length, L. A block length that is too small fails to
   * capture the serial correlation in the data, while a block length that is too large over-smooths
   * the data and can lead to inaccurate variance estimates. Since there is no universally "correct"
   * way to determine the optimal L, using a rule-of-thumb like the median holding period is a practical
   * heuristic. However, relying on a single value for L can be dangerous.
   *
   * By wiggling the block size (L−1,L,L+1), runFlaggedStrategyRobustness effectively checks
   * if the strategy's success is dependent on a specific, perhaps fortuitous, choice of L.
   *
   *If the strategy is genuinely robust, its performance metrics should be relatively stable
   * regardless of minor variations in the block size.
   *
   * If the strategy is fragile or overfitted, a small change in L could cause the performance
   * metric to collapse below the required hurdle, indicating that the original successful
   * result was an artifact of that specific block size choice.
   *
   * This check provides crucial information about the sensitivity of the bootstrap results
   * to the underlying assumptions of the resampling method.
   */
  
  // 1) L-sensitivity
  std::vector<size_t> Ls;
  if (L > cfg.minL) Ls.push_back(L - 1);
  Ls.push_back(L);
  Ls.push_back(L + 1);

  Num ann_min = lbAnnual_base;
  Num ann_max = lbAnnual_base;
  bool ls_fail = false;

  os << "   [ROBUST] L-sensitivity:";
  for (size_t Ltry : Ls)
    {
      Sampler pol(Ltry);
      BlockBCA b(returns, cfg.B, cfg.cl, statGeo, pol);
      const Num lbP = b.getLowerBound();
      const Num lbA = annualizeLB_<Num>(lbP, annualizationFactor);

      ann_min = (lbA < ann_min) ? lbA : ann_min;
      ann_max = (lbA > ann_max) ? lbA : ann_max;

      // Hard fail if any LB ≤ 0 or ≤ hurdle
      if (lbA <= DecimalConstants<Num>::DecimalZero || lbA <= finalRequiredReturn)
	ls_fail = true;

      os << "  L=" << Ltry
	 << " → per=" << (lbP * DecimalConstants<Num>::DecimalOneHundred) << "%,"
	 << " ann=" << (lbA * DecimalConstants<Num>::DecimalOneHundred) << "%;";
    }
  os << "\n";

  // Relative variability = (max - min) / max
  double relVar = 0.0;
  if (ann_max > DecimalConstants<Num>::DecimalZero)
    relVar = (ann_max.getAsDouble() - ann_min.getAsDouble()) / ann_max.getAsDouble();

  const bool ls_var_fail_raw = (relVar > cfg.relVarTol);

  // Near-hurdle test: absolute or relative band
  const double baseA = lbAnnual_base.getAsDouble();
  const double hurA  = finalRequiredReturn.getAsDouble();
  const bool nearHurdle =
      (lbAnnual_base <= (finalRequiredReturn + cfg.varOnlyMarginAbs)) ||
      (baseA <= hurA * (1.0 + cfg.varOnlyMarginRel));

  // Verdict for L-sensitivity
  if (ls_fail)
    {
      os << "   [ROBUST] L-sensitivity FAIL: LB below hurdle/zero at some L.\n";
      return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::LSensitivityBound, relVar};
    }

  if (ls_var_fail_raw)
    {
      if (nearHurdle) {
	os << "   [ROBUST] L-sensitivity FAIL: relVar=" << relVar
	   << " > " << cfg.relVarTol << " and base LB near hurdle.\n";
	return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::LSensitivityVarNearHurdle, relVar};
      }
      else
	{
	  os << "   [ROBUST] L-sensitivity PASS (high variability relVar=" << relVar
	     << " > " << cfg.relVarTol << " but base LB comfortably above hurdle).\n";
	}
    }
  else
    {
      os << "   [ROBUST] L-sensitivity PASS (relVar=" << relVar << ")\n";
    }

  /*
   * Splitting the Returns (Split-sample stability)
   * The split-sample stability test is a powerful method for detecting non-stationarity
   * or lucky sub-periods in the historical data.
   *
   * Non-stationarity: If the statistical properties of the returns (e.g., mean, variance, or correlation)
   * change over time, a strategy that performed well in one regime might fail in another. By splitting
   * the sample into two halves (e.g., the first 5 years and the last 5 years), this test checks
   * for a significant change in performance.
   *
   * Overfitting to a specific period: A strategy might be overfitted to a bull market or a particularly
   * favorable market regime in the first half of the data. The performance in the second half of the
   * data might then be significantly worse. A failure on this test indicates that the strategy's
   * historical success  might not be generalizable to future market conditions.
   * This check is a form of walk-forward validation in miniature and helps to prevent the selection
   * of strategies that are only profitable during a single, fortunate period of the backtest.
   */

  // 2) Split-sample (only if sample is large enough)
  if (n >= cfg.minTotalForSplit)
    {
      const size_t mid = n / 2;
      const size_t n1 = mid;
      const size_t n2 = n - mid;

      bool canSplit = (n1 >= cfg.minHalfForSplit) && (n2 >= cfg.minHalfForSplit);
      if (!canSplit)
	{
	  os << "   [ROBUST] Split-sample SKIP (insufficient per-half data: "
	     << n1 << " & " << n2 << ")\n";
	}
      else
	{
	  std::vector<Num> r1(returns.begin(), returns.begin() + mid);
	  std::vector<Num> r2(returns.begin() + mid, returns.end());

	  BlockBCA b1(r1, cfg.B, cfg.cl, statGeo, pol_base);
	  BlockBCA b2(r2, cfg.B, cfg.cl, statGeo, pol_base);

	  const Num lb1P = b1.getLowerBound();
	  const Num lb2P = b2.getLowerBound();
	  const Num lb1A = annualizeLB_<Num>(lb1P, annualizationFactor);
	  const Num lb2A = annualizeLB_<Num>(lb2P, annualizationFactor);
	  
	  os << "   [ROBUST] Split-sample: "
	     << "H1 per=" << (lb1P * DecimalConstants<Num>::DecimalOneHundred) << "% (ann="
	     << (lb1A * DecimalConstants<Num>::DecimalOneHundred) << "%), "
	     << "H2 per=" << (lb2P * DecimalConstants<Num>::DecimalOneHundred) << "% (ann="
	     << (lb2A * DecimalConstants<Num>::DecimalOneHundred) << "%)\n";

	  if (lb1A <= DecimalConstants<Num>::DecimalZero || lb2A <= DecimalConstants<Num>::DecimalZero
	      || lb1A <= finalRequiredReturn || lb2A <= finalRequiredReturn)
	    {
	      os << "   [ROBUST] Split-sample FAIL: a half falls to ≤ 0 or ≤ hurdle.\n";
	      return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::SplitSample, relVar};
	    }
	  else
	    {
	      os << "   [ROBUST] Split-sample PASS\n";
	    }
	}
    }
  else
    {
      os << "   [ROBUST] Split-sample SKIP (n=" << n << " < " << cfg.minTotalForSplit << ")\n";
    }

  /* The "tail risk sanity" check assesses the potential for a strategy's
   * returns to be negatively impacted by severe losses in the tails of its
   * return distribution. This check is advisory and does not, by itself,
   * cause a strategy to be rejected unless its BCa lower bound is already
   *borderline (close to the hurdle).
   *
   *Here’s a step-by-step breakdown of the process:
   *
   * Sort the Returns: The function takes a vector of per-period returns and sorts
   * them in ascending order. This makes it easy to find the values at specific percentiles.
   *
   * Calculate the 5% Quantile (q05): It computes the empirical lower 5% quantile, denoted as q05
   *
   * This value represents the point below which 5% of the worst returns fall.
   *
   * Calculate the Expected Shortfall (ES): The expected shortfall (ES) is calculated as the
   * average of all returns that fall at or below the 5% quantile (q05). This provides a
   * measure of the average magnitude of losses in the worst-case scenarios, giving a more
   * complete picture of tail risk than the quantile alone.
   *
   * Check for "Severe" Tails: The function determines if the tail is "severe".
   * A tail is considered severe if the absolute value of the 5% quantile (∣q05∣) is greater
   * than a configured multiple (e.g., 3.0) of the baseline per-period geometric mean (GM)
   * BCa lower bound. This comparison highlights cases where the strategy's worst losses
   * are disproportionately large relative to its expected long-term return.
   *
   * Determine the Verdict:
   *
   * PASS: If the tail is not severe, the check passes.
   *
   *CONDITIONAL FAIL: If the tail is severe AND the strategy's annualized GM BCa lower bound
   * is "borderline" (i.e., within a small margin of the required return hurdle),
   * the check results in a ThumbsDown verdict and the strategy is rejected.
   * This prevents the selection of strategies that barely meet the performance criteria
   * but have a high risk of catastrophic losses.
   */
  
  // 3) Tail risk sanity
  std::vector<Num> sorted = returns;
  std::sort(sorted.begin(), sorted.end(),
            [](const Num& a, const Num& b){ return a < b; });

 /**
 * Tail risk metrics (per-period, on raw returns):
 *  - q05  = empirical 5% quantile of returns (a “bad-day cutoff”).
 *  - ES05 = average return conditional on being in the worst 5% (how bad
 *           those bad days are, on average).
 *
 * We don’t use these to accept/reject by themselves. Instead, we compare them
 * to the conservative per-period GM LB to convey scale:
 *   |q05| / |LB_per(GM)|  and  |ES05| / |LB_per(GM)|.
 *
 * If |q05| is more than tailMultiple × LB_per(GM), we mark “severe”.
 * Policy: “severe” is advisory unless the strategy is also near the hurdle.
 */

  size_t k = static_cast<size_t>(std::floor(cfg.tailAlpha * static_cast<double>(n)));
  if (k >= n) k = n - 1;
  const Num q05 = sorted[k];

  Num sumTail = DecimalConstants<Num>::DecimalZero;
  size_t cntTail = 0;
  for (size_t i = 0; i <= k; ++i) { sumTail += sorted[i]; ++cntTail; }
  const Num es05 = (cntTail > 0) ? (sumTail / Num(static_cast<int>(cntTail)))
                                 : q05;

  const bool severe_tails =
      (q05 < DecimalConstants<Num>::DecimalZero) &&
      (absNum_(q05) > cfg.tailMultiple * lbPeriod_base);

  const bool borderline =
      (lbAnnual_base <= (finalRequiredReturn + cfg.borderlineAnnualMargin));

  os << "   [ROBUST] Tail risk: q05=" << (q05 * DecimalConstants<Num>::DecimalOneHundred) << "%, "
     << "ES05=" << (es05 * DecimalConstants<Num>::DecimalOneHundred) << "%, "
     << "severe=" << (severe_tails ? "yes" : "no") << ", "
     << "borderline=" << (borderline ? "yes" : "no") << "\n";

     logTailRiskExplanation(os, lbPeriod_base, q05, es05, cfg.tailMultiple.getAsDouble());

  if (severe_tails && borderline) {
    os << "   [ROBUST] Tail risk FAIL (severe tails and borderline LB) → ThumbsDown.\n";
    return {RobustnessVerdict::ThumbsDown, RobustnessFailReason::TailRisk, relVar};
  }

  os << "   [ROBUST] All checks PASS → ThumbsUp.\n";
  return {RobustnessVerdict::ThumbsUp, RobustnessFailReason::None, relVar};
}

enum class DivergencePrintRel { Defined, NotDefined };

template<typename Num>
struct DivergenceResult {
  bool flagged;
  double absDiff;       // absolute annualized difference (as a fraction, not %)
  double relDiff;       // relative annualized difference (abs/max), undefined if max<=0
  DivergencePrintRel relState;
};

/**
 * Diagnostic sentinel: AM vs GM lower-bound divergence
 *
 * We make decisions using the GEOMETRIC mean (GM) because it matches compounding.
 * However, we also compare the BCa *annualized* lower bounds of the ARITHMETIC mean (AM)
 * and GM as a cheap, informative warning signal. This function computes:
 *
 *   abs_gap = | LB_ann(GM) - LB_ann(AM) |
 *   rel_gap = abs_gap / max(LB_ann(GM), LB_ann(AM))   // guarded against 0
 *
 * and returns them so the caller can decide whether to flag the strategy and run
 * deeper robustness checks (L-sensitivity, split-sample when n>=40, tail-risk).
 *
 * Why keep AM if we filter on GM?
 *  - Volatility drag proxy: for small returns, GM ≈ AM − ½·Var(r). A large AM–GM gap
 *    is a red flag that variance and/or skew/fat tails are hurting true compounding.
 *  - Different influence functions: AM (linear) and GM (log-domain) react differently
 *    to outliers, zeros, and serial dependence; disagreement is a useful “smoke detector”
 *    for shape/resampling sensitivity or data/plumbing mistakes.
 *  - Sanity & transparency: printing AM alongside GM helps diagnose unexpected shifts
 *    (e.g., slippage handling, transform errors) without changing the pass/fail rule.
 *
 * Important:
 *  - This divergence is **diagnostic only**—it does NOT accept or reject a strategy.
 *    It merely gates the robustness suite. Final acceptance remains GM-LB vs hurdle.
 *  - Thresholds for abs/rel gaps are heuristics; tune per risk tolerance and sample size.
 *    Near-hurdle strategies deserve extra scrutiny even for modest gaps.
 *  - Divergences can occur legitimately (finite samples, BCa asymmetry, block resampling),
 *    so never drop solely on AM–GM gap—always confirm with the robustness checks.
 */
template<typename Num>
DivergenceResult<Num>
assessAMDivergence(const Num& gmAnn, const Num& amAnn,
                   double absThresh, double relThresh)
{
  const double g = gmAnn.getAsDouble();
  const double a = amAnn.getAsDouble();
  const double absd = std::fabs(g - a);

  const double denom = std::max(g, a);
  DivergenceResult<Num> out{};
  out.absDiff = absd;

  if (denom > 0.0)
    {
      out.relDiff = absd / denom;
      out.relState = DivergencePrintRel::Defined;
      out.flagged = (absd > absThresh) || (out.relDiff > relThresh);
    }
  else
    {
      out.relDiff = 0.0; // meaningless; we'll print "n/a"
      out.relState = DivergencePrintRel::NotDefined;
      // Still allow flagging by absolute gap even if relative is undefined.
      out.flagged = (absd > absThresh);
    }
  return out;
}

template<typename Num>
std::vector<std::shared_ptr<PalStrategy<Num>>>
filterSurvivingStrategiesByPerformance(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame)
{
  std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;

  // Filtering parameters
  const Num costBufferMultiplier = Num("1.5");
  const Num riskFreeRate         = Num("0.03");
  const Num riskFreeMultiplier   = Num("2.0");
  const Num riskFreeHurdle       = riskFreeRate * riskFreeMultiplier;

  const RobustnessChecksConfig<Num> cfg{};

  // Summary counters
  size_t cnt_insufficient = 0;
  size_t cnt_flagged = 0, cnt_flag_pass = 0;
  size_t cnt_fail_Lbound = 0, cnt_fail_Lvar = 0, cnt_fail_split = 0, cnt_fail_tail = 0;

  std::cout << "\nFiltering " << survivingStrategies.size() << " surviving strategies by BCa performance...\n";
  std::cout << "Filter 1 (Statistical Viability): Annualized Lower Bound > 0\n";
  std::cout << "Filter 2 (Economic Significance): Annualized Lower Bound > (Annualized Cost Hurdle * " << costBufferMultiplier << ")\n";
  std::cout << "Filter 3 (Risk-Adjusted Return): Annualized Lower Bound > (Risk-Free Rate * " << riskFreeMultiplier << ")\n";
  std::cout << "  - Cost assumptions: $0 commission, 0.10% slippage/spread per side.\n";
  std::cout << "  - Risk-Free Rate assumption: " << (riskFreeRate * DecimalConstants<Num>::DecimalOneHundred) << "%.\n";

  for (const auto& strategy : survivingStrategies)
  {
    try
    {
      auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
      freshPortfolio->addSecurity(baseSecurity);
      auto clonedStrat = strategy->clone2(freshPortfolio);

      auto backtester     = BackTesterFactory<Num>::backTestStrategy(clonedStrat, theTimeFrame, backtestingDates);
      auto highResReturns = backtester->getAllHighResReturns(clonedStrat.get());

      if (highResReturns.size() < 20) {
        std::cout << "✗ Strategy filtered out: " << strategy->getStrategyName()
                  << " - Insufficient returns for bootstrap (" << highResReturns.size() << " < 20).\n";
        ++cnt_insufficient;
        continue;
      }

      const unsigned int medianHoldBars = backtester->getClosedPositionHistory().getMedianHoldingPeriod();
      std::cout << "Strategy Median holding period = " << medianHoldBars << "\n";
      const std::size_t  L = std::max<std::size_t>(2, static_cast<std::size_t>(medianHoldBars));
      StationaryBlockResampler<Num> sampler(L);

      const unsigned int num_resamples    = 2000;
      const double       confidence_level = 0.95;

      GeoMeanStat<Num> statGeo;
      using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;
      BlockBCA bcaGeo (highResReturns, num_resamples, confidence_level, statGeo, sampler);
      BlockBCA bcaMean(highResReturns, num_resamples, confidence_level,
                       &mkc_timeseries::StatUtils<Num>::computeMean, sampler);

      const Num lbGeoPeriod  = bcaGeo.getLowerBound();
      const Num lbMeanPeriod = bcaMean.getLowerBound();

      double annualizationFactor;
      if (theTimeFrame == TimeFrame::INTRADAY) {
        annualizationFactor = calculateAnnualizationFactor(
            theTimeFrame,
            baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes());
      } else {
        annualizationFactor = calculateAnnualizationFactor(theTimeFrame);
      }

      BCaAnnualizer<Num> annualizerGeo (bcaGeo,  annualizationFactor);
      BCaAnnualizer<Num> annualizerMean(bcaMean, annualizationFactor);

      const Num annualizedLowerBoundGeo  = annualizerGeo.getAnnualizedLowerBound();
      const Num annualizedLowerBoundMean = annualizerMean.getAnnualizedLowerBound();

      // Hurdles: cost & risk-free
      const Num slippagePerSide      = Num("0.001"); // 0.10% per side
      const Num slippagePerRoundTrip = slippagePerSide * DecimalConstants<Num>::DecimalTwo; // 0.20% per trade
      const Num annualizedTrades(backtester->getEstimatedAnnualizedTrades());
      const Num annualizedCostHurdle = annualizedTrades * slippagePerRoundTrip;
      const Num costBasedRequiredReturn = annualizedCostHurdle * costBufferMultiplier;
      const Num finalRequiredReturn     = std::max(costBasedRequiredReturn, riskFreeHurdle);

      // ---- Early decision on the only thing we care about: GM LB vs hurdle ----
      if (annualizedLowerBoundGeo <= finalRequiredReturn) {
        std::cout << "✗ Strategy filtered out: " << strategy->getStrategyName()
                  << " (Lower Bound = "
                  << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
                  << "% <= Required Return = "
                  << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
                  << "  [Block L=" << L << "]\n\n";
        continue; // Skip divergence/robustness for obvious fails
      }

      // If we reach here, GM LB passes the hurdle → optionally run diagnostic divergence & robustness
      const auto divergence = assessAMDivergence<Num>(annualizedLowerBoundGeo, annualizedLowerBoundMean,
                                                      /*absThresh=*/0.05, /*relThresh=*/0.30);
      if (divergence.flagged) {
        ++cnt_flagged;
        std::cout << "   [FLAG] Large AM vs GM divergence (abs="
                  << (Num(divergence.absDiff) * DecimalConstants<Num>::DecimalOneHundred) << "%, rel=";
        if (divergence.relState == DivergencePrintRel::Defined) {
          std::cout << divergence.relDiff;
        } else {
          std::cout << "n/a";
        }
        std::cout << "); running robustness checks...\n";

        const auto rob = runFlaggedStrategyRobustness<Num>(
            strategy->getStrategyName(),
            highResReturns,
            L,
            annualizationFactor,
            finalRequiredReturn,
            cfg,
            std::cout
        );

        if (rob.verdict == RobustnessVerdict::ThumbsDown) {
          switch (rob.reason) {
            case RobustnessFailReason::LSensitivityBound:        ++cnt_fail_Lbound; break;
            case RobustnessFailReason::LSensitivityVarNearHurdle:++cnt_fail_Lvar;   break;
            case RobustnessFailReason::SplitSample:              ++cnt_fail_split;  break;
            case RobustnessFailReason::TailRisk:                 ++cnt_fail_tail;   break;
            default: break;
          }
          std::cout << "   [FLAG] Robustness checks FAILED → excluding strategy.\n\n";
          continue;
        } else {
          ++cnt_flag_pass;
          std::cout << "   [FLAG] Robustness checks PASSED.\n";
        }
      }

      // Passed GM hurdle (and any flagged robustness) → keep
      filteredStrategies.push_back(strategy);

      std::cout << "✓ Strategy passed: " << strategy->getStrategyName()
                << " (Lower Bound = "
                << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
                << "% > Required Return = "
                << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
                << "  [Block L=" << L << "]\n";

      std::cout << "   ↳ Lower bounds (annualized): "
                << "GeoMean = " << (annualizedLowerBoundGeo  * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                << "Mean = "    << (annualizedLowerBoundMean * DecimalConstants<Num>::DecimalOneHundred) << "%\n\n";
    }
    catch (const std::exception& e)
    {
      std::cout << "Warning: Failed to evaluate strategy '" << strategy->getStrategyName()
                << "' performance: " << e.what() << "\n";
      std::cout << "Excluding strategy from filtered results.\n";
    }
  }

  // Directional survivor counts (based on strategy name containing "Long"/"Short")
  size_t survivorsLong = 0, survivorsShort = 0;
  for (const auto& s : filteredStrategies) {
    const auto& nm = s->getStrategyName();
    if (nm.find("Long")  != std::string::npos) ++survivorsLong;
    if (nm.find("Short") != std::string::npos) ++survivorsShort;
  }

  // Summary
  std::cout << "BCa Performance Filtering complete: " << filteredStrategies.size()
            << "/" << survivingStrategies.size() << " strategies passed criteria.\n\n";
  std::cout << "[Summary] Flagged for divergence: " << cnt_flagged
            << " (passed robustness: " << cnt_flag_pass << ", failed: "
            << (cnt_flagged >= cnt_flag_pass ? (cnt_flagged - cnt_flag_pass) : 0) << ")\n";
  std::cout << "          Fail reasons → "
            << "L-bound/hurdle: " << cnt_fail_Lbound
            << ", L-variability near hurdle: " << cnt_fail_Lvar
            << ", split-sample: " << cnt_fail_split
            << ", tail-risk: " << cnt_fail_tail << "\n";
  std::cout << "          Insufficient sample (pre-filter): " << cnt_insufficient << "\n";
  std::cout << "          Survivors by direction → Long: " << survivorsLong
            << ", Short: " << survivorsShort << "\n";

  return filteredStrategies;
}


template<typename Num>
void filterMetaStrategy(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame)
{
  if (survivingStrategies.empty()) {
    std::cout << "\n[Meta] No surviving strategies to aggregate.\n";
    return;
  }

  std::cout << "\n[Meta] Building equal-weight portfolio from "
            << survivingStrategies.size() << " survivors...\n";

  // Gather per-strategy high-res returns, annualized trade counts, and median holds (inclusive bars)
  std::vector<std::vector<Num>> survivorReturns;
  survivorReturns.reserve(survivingStrategies.size());
  std::vector<Num> survivorAnnualizedTrades;
  survivorAnnualizedTrades.reserve(survivingStrategies.size());
  std::vector<unsigned int> survivorMedianHolds;
  survivorMedianHolds.reserve(survivingStrategies.size());

  size_t T = std::numeric_limits<size_t>::max();

  for (const auto& strat : survivingStrategies) {
    try {
      auto freshPortfolio = std::make_shared<Portfolio<Num>>(strat->getStrategyName() + " Portfolio");
      freshPortfolio->addSecurity(baseSecurity);
      auto cloned = strat->clone2(freshPortfolio);

      auto bt = BackTesterFactory<Num>::backTestStrategy(cloned, theTimeFrame, backtestingDates);
      auto r  = bt->getAllHighResReturns(cloned.get());

      if (r.size() < 2) {
        std::cout << "  [Meta] Skipping " << strat->getStrategyName()
                  << " (insufficient returns: " << r.size() << ")\n";
        continue;
      }

      const unsigned int medHold = bt->getClosedPositionHistory().getMedianHoldingPeriod(); // inclusive bars (min 2)
      survivorMedianHolds.push_back(medHold);

      T = std::min(T, r.size());
      survivorReturns.push_back(std::move(r));
      survivorAnnualizedTrades.push_back(Num(bt->getEstimatedAnnualizedTrades()));
    }
    catch (const std::exception& e) {
      std::cout << "  [Meta] Skipping " << strat->getStrategyName()
                << " due to error: " << e.what() << "\n";
    }
  }

  if (survivorReturns.empty() || T < 2) {
    std::cout << "[Meta] Not enough aligned data to form portfolio.\n";
    return;
  }

  // Equal-weight portfolio series (truncate to shortest length T)
  const size_t n = survivorReturns.size();
  const Num w = Num(1) / Num(static_cast<int>(n));

  std::vector<Num> metaReturns(T, DecimalConstants<Num>::DecimalZero);
  for (size_t i = 0; i < n; ++i) {
    for (size_t t = 0; t < T; ++t) {
      metaReturns[t] += w * survivorReturns[i][t];
    }
  }

  // Per-period point estimates (pre-annualization)
  {
    const Num am = StatUtils<Num>::computeMean(metaReturns);
    const Num gm = GeoMeanStat<Num>{}(metaReturns);
    std::cout << "      Per-period point estimates (pre-annualization): "
              << "Arithmetic mean =" << (am * DecimalConstants<Num>::DecimalOneHundred) << "%, "
              << "Geometric mean =" << (gm * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
  }

  // Annualization factor (same logic as strategy-level)
  double annualizationFactor;
  if (theTimeFrame == TimeFrame::INTRADAY) {
    auto minutes = baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes();
    annualizationFactor = calculateAnnualizationFactor(theTimeFrame, minutes);
  } else {
    annualizationFactor = calculateAnnualizationFactor(theTimeFrame);
  }

  // -------- Block length for meta bootstrap: median of survivors' median holds (round-half-up), clamp to >=2
  auto computeMedianUH = [](std::vector<unsigned int> v) -> size_t {
    if (v.empty()) return 2;
    const size_t m = v.size();
    const size_t mid = m / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (m & 1U) {
      return std::max<size_t>(2, v[mid]);
    } else {
      // round-half-up average of the two middles
      auto hi = v[mid];
      std::nth_element(v.begin(), v.begin() + (mid - 1), v.begin() + mid);
      auto lo = v[mid - 1];
      return std::max<size_t>(2, (static_cast<size_t>(lo) + static_cast<size_t>(hi) + 1ULL) / 2ULL);
    }
  };

  const size_t Lmeta = computeMedianUH(survivorMedianHolds);
  StationaryBlockResampler<Num> metaSampler(Lmeta);
  using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;

  // Bootstrap portfolio series — GeoMean (decision) and Arithmetic mean (comparison) with blocks
  const unsigned int num_resamples   = 2000;
  const double       confidence_level = 0.95;

  GeoMeanStat<Num> statGeo;
  BlockBCA metaGeo(metaReturns, num_resamples, confidence_level, statGeo, metaSampler);
  BlockBCA metaMean(metaReturns, num_resamples, confidence_level,
                    &mkc_timeseries::StatUtils<Num>::computeMean, metaSampler);

  const Num lbGeoPeriod  = metaGeo.getLowerBound();
  const Num lbMeanPeriod = metaMean.getLowerBound();

  std::cout << "      Per-period BCa lower bounds (pre-annualization): "
            << "Geo="  << (lbGeoPeriod  * DecimalConstants<Num>::DecimalOneHundred) << "%, "
            << "Mean=" << (lbMeanPeriod * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
  std::cout << "      (Meta uses block resampling with L=" << Lmeta << ")\n";

  // Annualize portfolio BCa results
  BCaAnnualizer<Num> metaGeoAnn(metaGeo, annualizationFactor);
  BCaAnnualizer<Num> metaMeanAnn(metaMean, annualizationFactor);

  const Num lbGeoAnn  = metaGeoAnn.getAnnualizedLowerBound();
  const Num lbMeanAnn = metaMeanAnn.getAnnualizedLowerBound();

  // Portfolio-level cost hurdle (0.10% per side => 0.20% round trip), equal-weight scaling
  const Num costBufferMultiplier = Num("1.5");
  const Num riskFreeRate         = Num("0.03");
  const Num riskFreeMultiplier   = Num("2.0");
  const Num riskFreeHurdle       = riskFreeRate * riskFreeMultiplier;

  const Num slippagePerSide      = Num("0.001"); // 0.10%
  const Num slippagePerRoundTrip = slippagePerSide * DecimalConstants<Num>::DecimalTwo; // 0.20%

  Num sumTrades = DecimalConstants<Num>::DecimalZero;
  for (const auto& tr : survivorAnnualizedTrades) sumTrades += tr;
  Num portfolioAnnualizedTrades = w * sumTrades; // equal-weight portfolio: scale trades by weight

  Num annualizedCostHurdle      = portfolioAnnualizedTrades * slippagePerRoundTrip;
  Num costBasedRequiredReturn   = annualizedCostHurdle * costBufferMultiplier;
  Num finalRequiredReturn       = std::max(costBasedRequiredReturn, riskFreeHurdle);

  std::cout << "\n[Meta] Portfolio of " << n << " survivors (equal-weight):\n"
            << "      Annualized Lower Bound (GeoMean): " << (lbGeoAnn  * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
            << "      Annualized Lower Bound (Mean):    " << (lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
            << "      Required Return (max(cost,riskfree)): "
            << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

  if (lbGeoAnn > finalRequiredReturn) {
    std::cout << "      RESULT: ✓ Metastrategy PASSES\n";
  } else {
    std::cout << "      RESULT: ✗ Metastrategy FAILS\n";
  }

  std::cout << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
}

void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                        ValidationMethod method,
                                        ValidationInterface* validation,
                                        const DateRange& backtestingDates,
                                        TimeFrame::Duration theTimeFrame)
{
    std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(baseSecurity->getSymbol(),
                                                                                 method));
    std::ofstream survivingPatternsFile(detailedPatternsFileName);
    
    auto survivingStrategies = validation->getSurvivingStrategies();
    for (const auto& strategy : survivingStrategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                       theTimeFrame,
                                                                       backtestingDates);
            // Note: monteCarloStats not used for surviving patterns in this implementation
            // auto& monteCarloStats = validation->getStatisticsCollector();
            survivingPatternsFile << "Surviving Pattern:" << std::endl << std::endl;
            LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
            survivingPatternsFile << std::endl;
            writeBacktestPerformanceReport(survivingPatternsFile, backtester);
            survivingPatternsFile << std::endl << std::endl;
            //BacktestingStatPolicy<Num>::printDetailedScoreBreakdown(backtester, survivingPatternsFile);
            //writeMonteCarloPermutationStats(monteCarloStats, survivingPatternsFile, clonedStrat);
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception " << e.what() << std::endl;
            break;
        }
    }
}

// Overloaded version that takes a filtered strategies list directly with validation summary
void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                         ValidationMethod method,
                                         const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                                         const DateRange& backtestingDates,
                                         TimeFrame::Duration theTimeFrame,
                                         const std::string& policyName,
                                         const ValidationParameters& params)
{
    std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(baseSecurity->getSymbol(),
                                                                                 method));
    std::ofstream survivingPatternsFile(detailedPatternsFileName);
    
    // Write validation summary header
    survivingPatternsFile << "=== VALIDATION SUMMARY ===" << std::endl;
    survivingPatternsFile << "Security Ticker: " << baseSecurity->getSymbol() << std::endl;
    survivingPatternsFile << "Validation Method: " << getValidationMethodString(method) << std::endl;
    survivingPatternsFile << "Computation Policy: " << policyName << std::endl;
    survivingPatternsFile << "Out-of-Sample Range: " << backtestingDates.getFirstDateTime()
                          << " to " << backtestingDates.getLastDateTime() << std::endl;
    survivingPatternsFile << "Number of Permutations: " << params.permutations << std::endl;
    survivingPatternsFile << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (method == ValidationMethod::BenjaminiHochberg) {
        survivingPatternsFile << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    survivingPatternsFile << "Total Surviving Strategies (Performance Filtered): " << strategies.size() << std::endl;
    survivingPatternsFile << "===========================" << std::endl << std::endl;
    
    for (const auto& strategy : strategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                       theTimeFrame,
                                                                       backtestingDates);
            // Note: monteCarloStats not used for surviving patterns in this implementation
            // auto& monteCarloStats = validation->getStatisticsCollector();
            survivingPatternsFile << "Surviving Pattern:" << std::endl << std::endl;
            LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
            survivingPatternsFile << std::endl;
            writeBacktestPerformanceReport(survivingPatternsFile, backtester);
            survivingPatternsFile << std::endl << std::endl;
            //BacktestingStatPolicy<Num>::printDetailedScoreBreakdown(backtester, survivingPatternsFile);
            //writeMonteCarloPermutationStats(monteCarloStats, survivingPatternsFile, clonedStrat);
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception " << e.what() << std::endl;
            break;
        }
    }
}

void writeDetailedRejectedPatternsFile(const std::string& securitySymbol,
                                       ValidationMethod method,
                                       ValidationInterface* validation,
                                       const DateRange& backtestingDates,
                                       TimeFrame::Duration theTimeFrame,
                                       const Num& pValueThreshold,
                                       std::shared_ptr<Security<Num>> baseSecurity,
                                       const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies = {})
{
    std::string detailedPatternsFileName = createDetailedRejectedPatternsFileName(securitySymbol, method);
    std::ofstream rejectedPatternsFile(detailedPatternsFileName);
    
    // Get all strategies and identify rejected ones with their p-values
    auto allStrategies = validation->getAllTestedStrategies();
    std::set<std::shared_ptr<PalStrategy<Num>>> survivingSet;
    auto survivingStrategies = validation->getSurvivingStrategies();
    for (const auto& strategy : survivingStrategies)
    {
        survivingSet.insert(strategy);
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> rejectedStrategiesWithPValues;
    for (const auto& [strategy, pValue] : allStrategies)
    {
        if (survivingSet.find(strategy) == survivingSet.end())
        {
            rejectedStrategiesWithPValues.emplace_back(strategy, pValue);
        }
    }
    
    // Write header
    rejectedPatternsFile << "=== REJECTED PATTERNS REPORT ===" << std::endl;
    rejectedPatternsFile << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    rejectedPatternsFile << "P-Value Threshold: " << pValueThreshold << std::endl;
    rejectedPatternsFile << "Validation Method: " <<
        (method == ValidationMethod::Masters ? "Masters" : "Romano-Wolf") << std::endl;
    rejectedPatternsFile << "=================================" << std::endl << std::endl;
    
    if (rejectedStrategiesWithPValues.empty())
    {
        rejectedPatternsFile << "No rejected patterns found." << std::endl;
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "All " << validation->getNumSurvivingStrategies()
                            << " tested patterns survived the validation process." << std::endl;
        rejectedPatternsFile << "This indicates very strong patterns or a lenient p-value threshold." << std::endl;
        
        // Write basic summary statistics even when no rejected patterns are found
        struct RejectionReasonStats {
            int totalPatterns = 0;
            int survivingPatterns = 0;
            int rejectedPatterns = 0;
            double rejectionRate = 0.0;
        };
        
        RejectionReasonStats basicStats = {};
        basicStats.totalPatterns = static_cast<int>(allStrategies.size());
        basicStats.survivingPatterns = validation->getNumSurvivingStrategies();
        basicStats.rejectedPatterns = basicStats.totalPatterns - basicStats.survivingPatterns;
        basicStats.rejectionRate = basicStats.totalPatterns > 0 ?
            (double)basicStats.rejectedPatterns / basicStats.totalPatterns * 100.0 : 0.0;
        
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "=== Summary Statistics ===" << std::endl;
        rejectedPatternsFile << "Total Patterns Tested: " << basicStats.totalPatterns << std::endl;
        rejectedPatternsFile << "Surviving Patterns: " << basicStats.survivingPatterns << std::endl;
        rejectedPatternsFile << "Rejected Patterns: " << basicStats.rejectedPatterns << std::endl;
        rejectedPatternsFile << "Rejection Rate: " << std::fixed << std::setprecision(2)
                            << basicStats.rejectionRate << "%" << std::endl;
        
        return;
    }
    
    // Sort rejected strategies by p-value (ascending)
    std::sort(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Write detailed information for each rejected strategy
    for (const auto& [strategy, pValue] : rejectedStrategiesWithPValues)
    {
        // Write rejected pattern details inline since function is not defined
        rejectedPatternsFile << "Rejected Pattern (p-value: " << pValue << "):" << std::endl;
        LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
        rejectedPatternsFile << "P-Value: " << pValue << std::endl;
        rejectedPatternsFile << "Threshold: " << pValueThreshold << std::endl;
        rejectedPatternsFile << "Reason: P-value exceeds threshold" << std::endl;
        rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
    }
    
    // Calculate and write summary statistics
    // Write summary statistics inline since functions are not defined
    rejectedPatternsFile << std::endl << "=== Summary Statistics ===" << std::endl;
    rejectedPatternsFile << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    rejectedPatternsFile << "Validation Method: " << getValidationMethodString(method) << std::endl;
    rejectedPatternsFile << "P-Value Threshold: " << pValueThreshold << std::endl;
    
    if (!rejectedStrategiesWithPValues.empty()) {
        auto minPValue = std::min_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        auto maxPValue = std::max_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        rejectedPatternsFile << "Min P-Value: " << minPValue << std::endl;
        rejectedPatternsFile << "Max P-Value: " << maxPValue << std::endl;
    }
    
    // Add performance-filtered strategies section
    if (!performanceFilteredStrategies.empty()) {
        rejectedPatternsFile << std::endl << std::endl;
        rejectedPatternsFile << "=== PERFORMANCE-FILTERED PATTERNS ===" << std::endl;
        rejectedPatternsFile << "These patterns survived Monte Carlo validation but were filtered out due to insufficient backtesting performance." << std::endl;
        rejectedPatternsFile << "Total Performance-Filtered Patterns: " << performanceFilteredStrategies.size() << std::endl;
        rejectedPatternsFile << "Filtering Criteria: Profit Factor >= 1.75 AND PAL Profitability >= 85% of theoretical" << std::endl;
        rejectedPatternsFile << "=======================================" << std::endl << std::endl;
        
        for (const auto& strategy : performanceFilteredStrategies) {
            try {
                // Create fresh portfolio and clone strategy for backtesting
                auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
                freshPortfolio->addSecurity(baseSecurity);
                auto clonedStrat = strategy->clone2(freshPortfolio);
                
                // Run backtest to get performance metrics for reporting
                auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                           theTimeFrame,
                                                                           backtestingDates);
                
                // Extract performance metrics
                auto positionHistory = backtester->getClosedPositionHistory();
                Num profitFactor = positionHistory.getProfitFactor();
                Num actualPALProfitability = positionHistory.getPALProfitability();
                
                // Calculate theoretical PAL profitability
                Num theoreticalPALProfitability = calculateTheoreticalPALProfitability(strategy);
                
                // Write pattern details
                rejectedPatternsFile << "Performance-Filtered Pattern:" << std::endl;
                LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
                rejectedPatternsFile << std::endl;
                
                // Write performance metrics that caused rejection
                rejectedPatternsFile << "=== Performance Metrics ===" << std::endl;
                rejectedPatternsFile << "Profit Factor: " << profitFactor << " (Required: >= 1.75)" << std::endl;
                rejectedPatternsFile << "PAL Profitability: " << actualPALProfitability << "%" << std::endl;
                rejectedPatternsFile << "Theoretical PAL Profitability: " << theoreticalPALProfitability << "%" << std::endl;
                
                if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                    Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                    rejectedPatternsFile << "PAL Ratio: " << (palRatio * DecimalConstants<Num>::DecimalOneHundred) << "% (Required: >= 85%)" << std::endl;
                }
                
                rejectedPatternsFile << "Reason: ";
                bool profitFactorFailed = profitFactor < DecimalConstants<Num>::DecimalOnePointSevenFive;
                bool palProfitabilityFailed = false;
                
                if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                    Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                    Num eightyFivePercent = DecimalConstants<Num>::createDecimal("0.85");
                    palProfitabilityFailed = palRatio < eightyFivePercent;
                }
                
                if (profitFactorFailed && palProfitabilityFailed) {
                    rejectedPatternsFile << "Both Profit Factor and PAL Profitability criteria failed";
                } else if (profitFactorFailed) {
                    rejectedPatternsFile << "Profit Factor below threshold";
                } else if (palProfitabilityFailed) {
                    rejectedPatternsFile << "PAL Profitability below 85% of theoretical";
                }
                
                rejectedPatternsFile << std::endl << std::endl << "---" << std::endl << std::endl;
                
            } catch (const std::exception& e) {
                rejectedPatternsFile << "Performance-Filtered Pattern (Error in analysis):" << std::endl;
                LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
                rejectedPatternsFile << "Error: " << e.what() << std::endl;
                rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
            }
        }
    }
}



// ---- Core Logic ----

// This is the common worker function that runs the validation and prints results.
// It is called by the higher-level functions AFTER the validation object has been created.
void runValidationWorker(std::unique_ptr<ValidationInterface> validation,
                         std::shared_ptr<ValidatorConfiguration<Num>> config,
                         const ValidationParameters& params,
                         ValidationMethod validationMethod,
                         const std::string& policyName,
                         bool partitionByFamily = false)
{
    std::cout << "Starting Monte Carlo validation...\n" << std::endl;

    validation->runPermutationTests(config->getSecurity(),
        config->getPricePatterns(),
        config->getOosDateRange(),
        params.pValueThreshold,
        true, // Enable verbose logging by default
        partitionByFamily); // Pass the partitioning preference

    std::cout << "\nMonte Carlo validation completed." << std::endl;
    std::cout << "Number of surviving strategies = " << validation->getNumSurvivingStrategies() << std::endl;

    // -- Output --
    std::vector<std::shared_ptr<PalStrategy<Num>>> performanceFilteredStrategies;
    
    if (validation->getNumSurvivingStrategies() > 0)
    {
        auto survivingStrategies = validation->getSurvivingStrategies();
        
        // Apply performance-based filtering to surviving strategies
        std::cout << "\nApplying performance-based filtering to surviving strategies..." << std::endl;
        auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
        auto filteredStrategies = filterSurvivingStrategiesByPerformance<Num>(
            survivingStrategies,
            config->getSecurity(),
            config->getOosDateRange(),
            timeFrame
        );
        
        // Identify strategies that were filtered out due to performance criteria
        std::set<std::shared_ptr<PalStrategy<Num>>> filteredSet(filteredStrategies.begin(), filteredStrategies.end());
        for (const auto& strategy : survivingStrategies) {
            if (filteredSet.find(strategy) == filteredSet.end()) {
                performanceFilteredStrategies.push_back(strategy);
            }
        }

	if (!filteredStrategies.empty())
	  {
	    filterMetaStrategy<Num>(filteredStrategies,
				    config->getSecurity(),
				    config->getOosDateRange(),
				    timeFrame);
	  }
	
        std::cout << "Performance filtering results: " << filteredStrategies.size() << " passed, "
                  << performanceFilteredStrategies.size() << " filtered out" << std::endl;
        
        // Write the performance-filtered surviving patterns to the basic file
        if (!filteredStrategies.empty()) {
            std::string fn = createSurvivingPatternsFileName(config->getSecurity()->getSymbol(), validationMethod);
            std::ofstream survivingPatternsFile(fn);
            std::cout << "Writing surviving patterns to file: " << fn << std::endl;
            
            for (const auto& strategy : filteredStrategies)
            {
                LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
            }
        }

        // Write detailed report using filtered strategies
        if (!filteredStrategies.empty()) {
            std::cout << "Writing detailed surviving patterns report for " << filteredStrategies.size()
                      << " performance-filtered strategies..." << std::endl;
            writeDetailedSurvivingPatternsFile(config->getSecurity(), validationMethod, filteredStrategies,
                                               config->getOosDateRange(), timeFrame, policyName, params);
        } else {
            std::cout << "No strategies passed performance filtering criteria. Skipping detailed report." << std::endl;
        }
    }

    std::cout << "Writing detailed rejected patterns report..." << std::endl;
    auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
    writeDetailedRejectedPatternsFile(config->getSecurity()->getSymbol(), validationMethod, validation.get(),
                                      config->getOosDateRange(), timeFrame, params.pValueThreshold,
                                      config->getSecurity(), performanceFilteredStrategies);
    
    std::cout << "Validation run finished." << std::endl;
}

// ---- Validation Method Specific Orchestrators ----

// Orchestrator for Masters Validation
void runValidationForMasters(std::shared_ptr<ValidatorConfiguration<Num>> config,
                             const ValidationParameters& params,
                             const std::string& policyName,
                             bool partitionByFamily = false)
{
    std::cout << "\nUsing Masters validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }
    
    try {
        auto validation = statistics::PolicyFactory::createMastersValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Masters, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Masters validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Romano-Wolf Validation
void runValidationForRomanoWolf(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
                                bool partitionByFamily = false)
{
    std::cout << "\nUsing Romano-Wolf validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;

    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }

    try {
        auto validation = statistics::PolicyFactory::createRomanoWolfValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::RomanoWolf, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Romano-Wolf validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Benjamini-Hochberg Validation
void runValidationForBenjaminiHochberg(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                       const ValidationParameters& params,
                                       const std::string& policyName,
                                       bool partitionByFamily = false)
{
    std::cout << "\nUsing Benjamini-Hochberg validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    std::cout << "[INFO] False Discovery Rate (FDR) set to: " << params.falseDiscoveryRate << std::endl;

    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: None (all patterns tested together)" << std::endl;
    }

    try {
        auto validation = statistics::PolicyFactory::createBenjaminiHochbergValidation(
            policyName, params.permutations, params.falseDiscoveryRate.getAsDouble());
        runValidationWorker(std::move(validation), config, params, ValidationMethod::BenjaminiHochberg, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Benjamini-Hochberg validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Unadjusted Validation
void runValidationForUnadjusted(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
                                bool partitionByFamily = false)
{
    std::cout << "\nUsing Unadjusted validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }
    
    try {
        auto validation = statistics::PolicyFactory::createUnadjustedValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Unadjusted, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Unadjusted validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}


// ---- Main Application Entry Point ----

void usage()
{
    printf("Usage: PalValidator <config file>\n");
    printf("  All other parameters will be requested via interactive prompts.\n");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage();
        return 1;
    }
    
    // Initialize the policy registry with all available policies
    std::cout << "Initializing policy registry..." << std::endl;
    statistics::initializePolicyRegistry();
    
    // Load policy configuration (optional)
    palvalidator::PolicyConfiguration policyConfig;
    std::string configPath = "policies.json";
    if (!policyConfig.loadFromFile(configPath)) {
        std::cout << "No policy configuration file found, using defaults." << std::endl;
        policyConfig = palvalidator::PolicyConfiguration::createDefault();
    }
    
    // -- Configuration File Reading with existence check --
    std::string configurationFileName = std::string(argv[1]);
    std::shared_ptr<ValidatorConfiguration<Num>> config;
    
    // Check if configuration file exists before asking for other inputs
    if (!std::filesystem::exists(configurationFileName)) {
        std::cout << "Error: Configuration file '" << configurationFileName << "' does not exist." << std::endl;
        std::cout << "Please enter the correct configuration file path: ";
        std::getline(std::cin, configurationFileName);
    }
    
    // Try to read the configuration file
    ValidatorConfigurationFileReader reader(configurationFileName);
    try {
        config = reader.readConfigurationFile();
    }
    catch (const SecurityAttributesFactoryException& e) {
        std::cout << "SecurityAttributesFactoryException: Error reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    catch (const ValidatorConfigurationException& e) {
        std::cout << "ValidatorConfigurationException thrown when reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    
    // -- Get parameters interactively --
    ValidationParameters params;
    std::string input;

    std::cout << "\nEnter number of permutations (default: 5000): ";
    std::getline(std::cin, input);
    params.permutations = input.empty() ? 5000 : std::stoul(input);

    std::cout << "Enter p-value threshold (default: 0.05): ";
    std::getline(std::cin, input);
    params.pValueThreshold = input.empty() ? Num(0.05) : Num(std::stod(input));
    
    // Ask for Validation Method
    std::cout << "\nChoose validation method:" << std::endl;
    std::cout << "  1. Masters (default)" << std::endl;
    std::cout << "  2. Romano-Wolf" << std::endl;
    std::cout << "  3. Benjamini-Hochberg" << std::endl;
    std::cout << "  4. Unadjusted" << std::endl;
    std::cout << "Enter choice (1, 2, 3, or 4): ";
    std::getline(std::cin, input);
    
    ValidationMethod validationMethod = ValidationMethod::Masters;
    if (input == "2") {
        validationMethod = ValidationMethod::RomanoWolf;
    } else if (input == "3") {
        validationMethod = ValidationMethod::BenjaminiHochberg;
    } else if (input == "4") {
        validationMethod = ValidationMethod::Unadjusted;
    }
    
    // Conditionally ask for FDR
    params.falseDiscoveryRate = Num(0.10); // Set default
    if (validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "Enter False Discovery Rate (FDR) for Benjamini-Hochberg (default: 0.10): ";
        std::getline(std::cin, input);
        if (!input.empty()) {
            params.falseDiscoveryRate = Num(std::stod(input));
        }
    }
    
    // Ask about pattern partitioning for Masters, Romano-Wolf, and Benjamini-Hochberg methods
    bool partitionByFamily = false;
    if (validationMethod == ValidationMethod::Masters ||
        validationMethod == ValidationMethod::RomanoWolf ||
        validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "\nPattern Partitioning Options:" << std::endl;
        
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "  1. No Partitioning (all patterns tested together) - Default" << std::endl;
            std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
        } else {
            std::cout << "  1. By Direction Only (Long vs Short) - Default" << std::endl;
            std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
        }
        
        std::cout << "Choose partitioning method (1 or 2): ";
        std::getline(std::cin, input);
        
        if (input == "2") {
            partitionByFamily = true;
            std::cout << "Selected: Detailed family partitioning" << std::endl;
        } else {
            if (validationMethod == ValidationMethod::BenjaminiHochberg) {
                std::cout << "Selected: No partitioning (default)" << std::endl;
            } else {
                std::cout << "Selected: Direction-only partitioning (default)" << std::endl;
            }
        }
    }
    
    // Interactive policy selection using the new system
    std::cout << "\n=== Policy Selection ===" << std::endl;
    auto availablePolicies = palvalidator::PolicyRegistry::getAvailablePolicies();
    std::cout << "Available policies: " << availablePolicies.size() << std::endl;
    
    std::string selectedPolicy;
    if (policyConfig.getPolicySettings().interactiveMode) {
        selectedPolicy = statistics::PolicySelector::selectPolicy(availablePolicies, &policyConfig);
    } else {
        // Use default policy from configuration
        selectedPolicy = policyConfig.getDefaultPolicy();
        if (selectedPolicy.empty() || !palvalidator::PolicyRegistry::isPolicyAvailable(selectedPolicy)) {
            selectedPolicy = "GatedPerformanceScaledPalPolicy"; // Fallback default
        }
        std::cout << "Using configured default policy: " << selectedPolicy << std::endl;
    }

    // Display selected policy information
    try {
        auto metadata = palvalidator::PolicyRegistry::getPolicyMetadata(selectedPolicy);
        std::cout << "\nSelected Policy: " << metadata.displayName << std::endl;
        std::cout << "Description: " << metadata.description << std::endl;
        std::cout << "Category: " << metadata.category << std::endl;
        if (metadata.isExperimental) {
            std::cout << "⚠️  WARNING: This is an experimental policy!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not retrieve policy metadata: " << e.what() << std::endl;
    }
    
    // -- Summary --
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    std::cout << "Security Ticker: " << config->getSecurity()->getSymbol() << std::endl;
    std::cout << "In-Sample Range: " << config->getInsampleDateRange().getFirstDateTime()
              << " to " << config->getInsampleDateRange().getLastDateTime() << std::endl;
    std::cout << "Validation Method: " << getValidationMethodString(validationMethod) << std::endl;
    std::cout << "Computation Policy: " << selectedPolicy << std::endl;
    if (validationMethod == ValidationMethod::Masters ||
        validationMethod == ValidationMethod::RomanoWolf ||
        validationMethod == ValidationMethod::BenjaminiHochberg) {
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "None") << std::endl;
        } else {
            std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "By Direction Only") << std::endl;
        }
    } else if (validationMethod == ValidationMethod::Unadjusted) {
        std::cout << "Pattern Partitioning: None (not applicable for Unadjusted)" << std::endl;
    }
    std::cout << "Permutations: " << params.permutations << std::endl;
    std::cout << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    std::cout << "=============================" << std::endl;

    // -- Top-level dispatch based on the VALIDATION METHOD --
    try {
        switch (validationMethod)
        {
            case ValidationMethod::Masters:
                runValidationForMasters(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::RomanoWolf:
                runValidationForRomanoWolf(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::BenjaminiHochberg:
                runValidationForBenjaminiHochberg(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::Unadjusted:
                runValidationForUnadjusted(config, params, selectedPolicy, partitionByFamily);
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Validation failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
