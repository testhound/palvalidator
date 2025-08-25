#include "RobustnessAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip>
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include "DecimalConstants.h"

namespace palvalidator
{
namespace analysis
{

using namespace mkc_timeseries;

RobustnessResult RobustnessAnalyzer::runFlaggedStrategyRobustness(
    const std::string& label,
    const std::vector<Num>& returns,
    size_t L_in,
    double annualizationFactor,
    const Num& finalRequiredReturn,
    const RobustnessChecksConfig<Num>& cfg,
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
    const Num lbAnnual_base = annualizeLB_(lbPeriod_base, annualizationFactor);

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
    if (L > cfg.minL)
        Ls.push_back(L - 1);
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
        const Num lbA = annualizeLB_(lbP, annualizationFactor);

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
        if (nearHurdle)
        {
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
            const Num lb1A = annualizeLB_(lb1P, annualizationFactor);
            const Num lb2A = annualizeLB_(lb2P, annualizationFactor);
            
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
     *Here's a step-by-step breakdown of the process:
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
   *  - q05  = empirical 5% quantile of returns (a "bad-day cutoff").
   *  - ES05 = average return conditional on being in the worst 5% (how bad
   *           those bad days are, on average).
   *
   * We don't use these to accept/reject by themselves. Instead, we compare them
   * to the conservative per-period GM LB to convey scale:
   *   |q05| / |LB_per(GM)|  and  |ES05| / |LB_per(GM)|.
   *
   * If |q05| is more than tailMultiple × LB_per(GM), we mark "severe".
   * Policy: "severe" is advisory unless the strategy is also near the hurdle.
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

Num RobustnessAnalyzer::annualizeLB_(const Num& perPeriodLB, double k)
{
    const Num one = DecimalConstants<Num>::DecimalOne;
    return Num(std::pow((one + perPeriodLB).getAsDouble(), k)) - one;
}

Num RobustnessAnalyzer::absNum_(const Num& x)
{
    return (x < DecimalConstants<Num>::DecimalZero) ? -x : x;
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