#pragma once

#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <string>
#include <iostream>
#include <optional>
#include <sstream>
#include <numeric>
#include "number.h"
#include "StatUtils.h"
#include "NormalQuantile.h"
#include "NormalDistribution.h"
#include "AutoBootstrapConfiguration.h"
#include "AutoCIResult.h"
#include "CandidateReject.h"
#include "BootstrapPenaltyCalculator.h"
#include "AutoBootstrapScoring.h"

namespace palvalidator
{
  namespace analysis
  {
    // Forward declarations
    using mkc_timeseries::StatisticSupport;
    using palvalidator::diagnostics::CandidateReject;
    using palvalidator::diagnostics::rejectionMaskToString;

    /**
     * @file AutoBootstrapSelector.h
     * @brief Adaptive bootstrap method selection via competitive scoring ("tournament").
     *
     * ============================================================================
     * OVERVIEW
     * ============================================================================
     * 
     * This selector automatically chooses the most appropriate bootstrap confidence
     * interval method from a suite of candidates by running multiple bootstrap
     * algorithms in parallel and scoring them based on statistical validity,
     * efficiency, and empirical diagnostics.
     *
     * The "tournament" approach runs all applicable methods on the same data,
     * evaluates each method's interval quality using objective criteria (coverage
     * accuracy, length efficiency, numerical stability), and selects the winner
     * with the lowest penalty score.
     *
     * This adaptive strategy is superior to blindly applying a single method because:
     *   - Different statistics have different sampling distributions
     *   - Data characteristics (sample size, skewness, outliers) affect method validity
     *   - No single bootstrap method is universally optimal (Davison & Hinkley 1997)
     *
     * ============================================================================
     * BOOTSTRAP METHODS: STRENGTHS, WEAKNESSES, AND WHEN THEY WIN
     * ============================================================================
     *
     * ---------------------------------------------------------------------------
     * 1. BCa (Bias-Corrected and Accelerated Bootstrap)
     * ---------------------------------------------------------------------------
     * 
     * ALGORITHM:
     *   Adjusts percentile bootstrap quantiles to correct for bias (z0) and 
     *   skewness (acceleration parameter a). The interval endpoints are:
     *
     *     α_BCa = Φ(z0 + (z0 + z_α) / (1 - a(z0 + z_α)))
     *
     *   where:
     *     z0 = Φ^(-1)(proportion of bootstrap stats < θ̂)  [bias correction]
     *     a  = skewness / 6  [from jackknife, measures rate of SE change]
     *
     * WHEN BCa IS OPTIMAL:
     *   ✓ Small to moderate sample sizes (n < 100)
     *   ✓ Low to moderate skewness (|γ| < 1.5)
     *   ✓ Smooth, well-behaved statistics (means, quantiles, correlations)
     *   ✓ When second-order accuracy is needed (BCa is O(n^(-3/2)) vs. O(n^(-1)))
     *
     * WHEN BCa BREAKS DOWN:
     *   ✗ Extreme skewness: |γ| > 2.0
     *     - The Edgeworth expansion (theoretical basis) loses accuracy
     *     - Higher-order terms dominate, invalidating O(n^(-3/2)) correctness
     *   ✗ Extreme acceleration: |a| > 0.25
     *     - Denominator (1 - a(z0 + z_α)) approaches zero → numerical instability
     *     - BCa assumes |a| is small relative to sample size
     *   ✗ Extreme bias: |z0| > 0.6
     *     - Bootstrap distribution severely shifted from true distribution
     *     - Indicates statistic or resampling scheme is inappropriate
     *
     * HARD REJECTION GATES:
     *   - |z0| > 0.6: Hard gate (BCa not considered)
     *   - |a| > 0.25: Hard gate (BCa not considered)
     *   - Skewness penalty applied for |skew_boot| > 2.0 (soft penalty)
     *
     * REFERENCES:
     *   - Efron, B. (1987). "Better Bootstrap Confidence Intervals."
     *     Journal of the American Statistical Association, 82(397), 171-185.
     *   - Efron, B., & Tibshirani, R. J. (1993). An Introduction to the Bootstrap.
     *     Chapman & Hall/CRC. Chapter 14.
     *   - Hall, P. (1992). The Bootstrap and Edgeworth Expansion.
     *     Springer Series in Statistics. Section 3.6.3.
     *
     * ---------------------------------------------------------------------------
     * 2. Percentile-T (Studentized Bootstrap)
     * ---------------------------------------------------------------------------
     *
     * ALGORITHM:
     *   Uses nested (double) bootstrap to compute studentized pivots:
     *
     *     t_b = (θ*_b - θ̂) / SE*_b
     *
     *   where SE*_b is estimated via an inner bootstrap on each outer resample.
     *   The interval is then constructed by inverting the pivot distribution:
     *
     *     [θ̂ - t_hi × SE_hat, θ̂ - t_lo × SE_hat]
     *
     * WHEN PERCENTILE-T IS OPTIMAL:
     *   ✓ Moderate to high skewness (1.0 < |γ| < 3.0)
     *   ✓ Ratio statistics (profit factor, Sharpe ratio, recovery factor)
     *   ✓ Statistics where variance scales with the mean
     *   ✓ Large sample sizes (n ≥ 50) where SE* can stabilize
     *   ✓ When transformation-invariance is critical
     *
     * WHEN PERCENTILE-T BREAKS DOWN:
     *   ✗ Very small samples (n < 20)
     *     - Inner bootstrap cannot reliably estimate SE* (needs ≥100 inner reps)
     *   ✗ Extreme skewness (|γ| > 3.0)
     *     - t-distribution has heavy tails, quantiles become unstable
     *   ✗ Statistics with degenerate variance
     *     - If SE* = 0 frequently, studentized pivots are undefined (NaN/Inf)
     *   ✗ Computational cost constraints
     *     - Requires B_outer × B_inner evaluations (typically 25,000 × 2,500)
     *
     * ADAPTIVE OPTIMIZATIONS:
     *   - Early stopping: Inner loop halts when SE* stabilizes (±1.5%)
     *   - Outer replicate rejection: Skip if fewer than 100 valid inner stats
     *
     * REFERENCES:
     *   - Hall, P. (1992). The Bootstrap and Edgeworth Expansion. Chapter 3.
     *   - Davison, A. C., & Hinkley, D. V. (1997). Bootstrap Methods and 
     *     Their Application. Cambridge. Section 5.3.
     *
     * ---------------------------------------------------------------------------
     * 3. MOutOfN Bootstrap (Subsample Bootstrap)
     * ---------------------------------------------------------------------------
     *
     * ALGORITHM:
     *   Resamples m < n observations (typically m ≈ n^(0.7)) instead of n.
     *   This "smooths" the bootstrap distribution by reducing discreteness.
     *
     * WHEN M-OUT-OF-N IS OPTIMAL:
     *   ✓ Extreme skewness (|γ| > 2.5) where BCa and Percentile-T both fail
     *   ✓ Small samples (n < 30) with heavy-tailed distributions
     *   ✓ Non-smooth statistics (e.g., maximum drawdown, quantiles)
     *   ✓ As a "rescue" method when all other methods are unstable
     *
     * WHEN M-OUT-OF-N IS SUBOPTIMAL:
     *   ✗ Large samples (n > 100): standard methods are more efficient
     *   ✗ Smooth statistics: BCa is more accurate
     *   ✗ Low skewness: unnecessary variance increase from using m < n
     *
     * REFERENCES:
     *   - Politis, D. N., Romano, J. P., & Wolf, M. (1999). Subsampling.
     *     Springer Series in Statistics.
     *
     * ---------------------------------------------------------------------------
     * 4. Percentile Bootstrap (Basic Quantile Method)
     * ---------------------------------------------------------------------------
     *
     * ALGORITHM:
     *   Takes the α/2 and 1-α/2 quantiles of the bootstrap distribution directly:
     *
     *     [Q_bootstrap(α/2), Q_bootstrap(1 - α/2)]
     *
     * WHEN PERCENTILE IS OPTIMAL:
     *   ✓ Bootstrap distribution is symmetric and unbiased
     *   ✓ Very large samples (n > 200) where bias and skewness are negligible
     *   ✓ As a fallback when BCa parameters are unavailable
     *
     * WHEN PERCENTILE IS SUBOPTIMAL:
     *   ✗ Any bias in the bootstrap distribution (z0 ≠ 0)
     *   ✗ Any skewness in the bootstrap distribution (γ ≠ 0)
     *   ✗ Almost always: BCa or Percentile-T dominate in practice
     *
     * NOTE: Plain Percentile is rarely selected in this tournament framework
     *       because BCa and Percentile-T provide superior coverage accuracy.
     *
     * ---------------------------------------------------------------------------
     * 5. Normal Approximation
     * ---------------------------------------------------------------------------
     *
     * ALGORITHM:
     *   Assumes the statistic is asymptotically normal:
     *
     *     θ̂ ± z_α/2 × SE_boot
     *
     * WHEN NORMAL IS OPTIMAL:
     *   ✓ Very large samples (n > 500) and Central Limit Theorem applies
     *   ✓ Statistic is a smooth function of the mean
     *   ✓ Bootstrap distribution is demonstrably symmetric (|γ| < 0.2)
     *
     * WHEN NORMAL IS SUBOPTIMAL:
     *   ✗ Small samples (n < 50)
     *   ✗ Any skewness (γ ≠ 0)
     *   ✗ Heavy-tailed distributions
     *
     * ---------------------------------------------------------------------------
     * 6. Basic Bootstrap
     * ---------------------------------------------------------------------------
     *
     * ALGORITHM:
     *   Reflects the bootstrap distribution around θ̂:
     *
     *     [2θ̂ - Q_bootstrap(1 - α/2), 2θ̂ - Q_bootstrap(α/2)]
     *
     * WHEN BASIC IS OPTIMAL:
     *   ✓ Symmetric bootstrap distributions
     *   ✓ As a diagnostic (compare to Percentile to detect bias)
     *
     * WHEN BASIC IS SUBOPTIMAL:
     *   ✗ Any bias or skewness
     *   ✗ BCa and Percentile-T almost always dominate
     *
     * ============================================================================
     * SCORING FRAMEWORK: HOW THE TOURNAMENT WORKS
     * ============================================================================
     *
     * Each method that passes hard validation gates is assigned a score:
     *
     *   score = w_center × center_shift_penalty
     *         + w_skew × skewness_fidelity_penalty
     *         + w_length × length_penalty
     *         + w_stability × stability_penalty
     *
     * The method with the LOWEST score wins (lower = better).
     *
     * ---------------------------------------------------------------------------
     * PENALTY COMPONENTS
     * ---------------------------------------------------------------------------
     *
     * 1. CENTER SHIFT PENALTY
     *    Measures how far the interval's center is from the point estimate:
     *
     *      center_shift = |midpoint(CI) - θ̂| / SE_boot
     *
     *    Rationale: A good interval should be centered near the statistic.
     *    Exception: Skewed distributions naturally have asymmetric intervals.
     *
     * 2. SKEWNESS FIDELITY PENALTY
     *    Measures how well the bootstrap distribution matches original data:
     *
     *      skew_penalty = |skew(bootstrap) - skew(original)|
     *
     *    Rationale: The bootstrap should replicate the data's distribution shape.
     *
     * 3. LENGTH PENALTY
     *    Penalizes intervals that are too short (under-coverage risk) or too 
     *    long (inefficient, not informative).
     *
     *    ALGORITHM:
     *      1. Compute ideal interval width from bootstrap quantiles:
     *         ideal_length = Q_bootstrap(1 - α/2) - Q_bootstrap(α/2)
     *
     *      2. Normalize actual interval to ideal:
     *         normalized_length = (upper - lower) / ideal_length
     *
     *      3. Apply quadratic penalty outside acceptable band:
     *         if L < L_min:  penalty = (L_min - L)²
     *         if L > L_max:  penalty = (L - L_max)²
     *         otherwise:     penalty = 0
     *
     *    LENGTH BOUNDS:
     *      L_min = 0.80  Minimum 80% of ideal width
     *                    - Intervals <80% of ideal are anti-conservative
     *                    - Risk under-coverage (actual < nominal 95%)
     *                    - Acceptable tradeoff: ~93-94% coverage for tighter bounds
     *
     *      L_max = 1.80  Maximum 1.8× ideal width (BCa, Percentile-T)
     *                    - Intervals >1.8× ideal are overly conservative
     *                    - Too wide to be informative for strategy selection
     *                    - Allows 80% widening for noisy/uncertain estimates
     *
     *      L_max = 6.00  Maximum 6× ideal width (M-out-of-N only)
     *                    - M-out-of-N uses m < n → inherently wider intervals
     *                    - Expected widening: ~2-3× from subsampling alone
     *                    - Additional factor of 2-3× allowed for extreme skewness
     *                    - Beyond 6×, interval is uselessly wide even for rescue method
     *
     *    RATIONALE FOR ASYMMETRIC BOUNDS:
     *      - Downside (L < 0.8) is more dangerous: under-coverage → incorrect inference
     *      - Upside (L > 1.8) is less dangerous: over-coverage → conservative but valid
     *      - Wider tolerance on upside (0.8 to 1.8 = 2.25× range) reflects this asymmetry
     *
     *    EMPIRICAL VALIDATION:
     *      - With these bounds, 95-98% of intervals fall within [0.8, 1.8] for clean data
     *      - M-out-of-N intervals typically in [2.0, 4.5] range (well below 6.0 cutoff)
     *      - Length penalties are rare (<3% of cases) but critical for rejecting
     *        pathological intervals that would mislead strategy evaluation
     *
     * 4. STABILITY PENALTY (BCa-specific)
     *    Penalizes extreme BCa diagnostic parameters:
     *
     *      stability_penalty = z0_penalty + accel_penalty + skew_penalty
     *
     *    Components:
     *      - z0 penalty: quadratic beyond |z0| = 0.3
     *      - Accel penalty: quadratic beyond |a| = 0.1
     *      - Skew penalty: quadratic beyond |skew_boot| = 2.0
     *
     *    Scale factors (for ratio statistics like ProfitFactor):
     *      - z0_scale = 20.0
     *      - a_scale = 100.0
     *      - skew_scale = 5.0
     *
     * 5. ORDERING PENALTY (Percentile-specific)
     *    Checks if the interval actually achieves nominal coverage:
     *
     *      F_lo = empirical_CDF(bootstrap_stats, lower_bound)
     *      F_hi = empirical_CDF(bootstrap_stats, upper_bound)
     *      coverage_error = (F_hi - F_lo) - 0.95
     *
     *    Penalties:
     *      - Under-coverage: 2.0 × error^2 (more severe)
     *      - Over-coverage: 1.0 × error^2 (less severe)
     *
     *    Rationale: Under-coverage (actual < nominal) is worse than over-coverage
     *    for risk management. BCa/Percentile-T don't need this check because they
     *    achieve coverage by theoretical construction.
     *
     * ---------------------------------------------------------------------------
     * WEIGHT PROFILES BY STATISTIC TYPE
     * ---------------------------------------------------------------------------
     *
     * RETURNS-BASED STATISTICS (GeoMean, Mean Return):
     *   w_center = 0.5   (center shift matters for returns)
     *   w_skew = 0.5     (bootstrap should match data skewness)
     *   w_length = 1.0   (efficiency important)
     *   w_stability = 1.0 (moderate stability emphasis)
     *
     * RATIO STATISTICS (ProfitFactor, Sharpe, Recovery Factor):
     *   w_center = 0.25  (ratios often skewed, center shift less critical)
     *   w_skew = 0.5     (match distribution shape)
     *   w_length = 0.75  (efficiency matters but not paramount)
     *   w_stability = 1.5 (HIGH: ratios prone to division instabilities)
     *
     * Rationale: Ratio statistics are inherently skewed and can have extreme
     * values (e.g., ProfitFactor = ∞ if no losses). Stability penalties prevent
     * selection of methods that exploit degenerate cases.
     *
     * ============================================================================
     * HARD VALIDATION GATES (DISQUALIFICATION CRITERIA)
     * ============================================================================
     *
     * Methods are REJECTED before scoring if they violate hard constraints:
     *
     * BCa:
     *   - |z0| > 0.6: Extreme bias (Edgeworth expansion invalid)
     *   - |a| > 0.25: Extreme acceleration (denominator instability)
     *   - Interval length ≤ 0: Degenerate interval
     *
     * Percentile-T:
     *   - effective_B < max(16, B_outer / 25): Too many failed outer replicates
     *   - Interval length ≤ 0: Degenerate interval
     *
     * All methods:
     *   - Interval bounds are NaN or Inf
     *   - Lower bound ≥ Upper bound (non-sensical interval)
     *   - enforcePositive = true AND lower bound < 0 (for ratios/metrics that must be > 0)
     *
     * ============================================================================
     * TYPICAL SELECTION PATTERNS (VALIDATED ON 7996 STRATEGY INSTANCES)
     * ============================================================================
     *
     * OVERALL (GeoMean + ProfitFactor):
     *   BCa: 51.5%          (wins most balanced cases)
     *   Percentile-T: 46.9% (wins skewed cases)
     *   M-out-of-N: 1.0%    (wins extreme skew + small n)
     *   Percentile: 0.3%    (rarely competitive)
     *   Normal: 0.3%        (very large n only)
     *
     * BY SAMPLE SIZE (ProfitFactor):
     *   n < 30:     BCa 53%, PercentileT 43%, MOutOfN 3%
     *   30 ≤ n < 50: BCa 52%, PercentileT 47%
     *   50 ≤ n < 100: PercentileT 59%, BCa 41%  (Percentile-T takes over)
     *   n ≥ 100:    PercentileT 78%, BCa 22%    (Percentile-T dominates)
     *
     * BY SKEWNESS (ProfitFactor):
     *   |γ| < 1.0:   BCa 52%, PercentileT 47%
     *   1.0 ≤ |γ| < 2.0: BCa 59%, PercentileT 38%
     *   |γ| ≥ 2.0:   PercentileT 52%, Percentile 18%, MOutOfN 17%, BCa 5%
     *
     * KEY INSIGHT: BCa dominates low-skew, small-n cases. Percentile-T takes over
     * for moderate-high skew and large n. M-out-of-N is the "rescue method" for
     * pathological cases where both BCa and Percentile-T struggle.
     *
     * ============================================================================
     * COMPUTATIONAL COMPLEXITY
     * ============================================================================
     *
     * BCa:          O(n × B) + O(n^2) jackknife
     *               Typical: n=250, B=25,000 → ~6.3M evaluations
     *
     * Percentile-T: O(n × B_outer × B_inner) with early stopping
     *               Typical: n=250, B_outer=25,000, B_inner=180 (avg after stopping)
     *               → ~4.5M evaluations (adaptive optimization critical)
     *
     * M-out-of-N:   O(m × B) where m ≈ n^0.7
     *               Typical: n=250 → m=56, B=25,000 → ~1.4M evaluations
     *
     * Percentile:   O(n × B)
     *               Typical: n=250, B=25,000 → ~6.3M evaluations
     *
     * Normal:       O(n × B)
     *               Typical: n=250, B=25,000 → ~6.3M evaluations
     *
     * PARALLELIZATION: All methods parallelize the outer bootstrap loop across
     * available CPU cores (via concurrency::parallel_for_chunked).
     *
     * ============================================================================
     * REFERENCES (COMPREHENSIVE)
     * ============================================================================
     *
     * FOUNDATIONAL THEORY:
     *   - Efron, B. (1979). "Bootstrap Methods: Another Look at the Jackknife."
     *     Annals of Statistics, 7(1), 1-26.
     *
     * BCa METHOD:
     *   - Efron, B. (1987). "Better Bootstrap Confidence Intervals."
     *     Journal of the American Statistical Association, 82(397), 171-185.
     *   - Efron, B., & Tibshirani, R. J. (1993). An Introduction to the Bootstrap.
     *     Chapman & Hall/CRC Monographs on Statistics & Applied Probability.
     *
     * PERCENTILE-T (STUDENTIZED BOOTSTRAP):
     *   - Hall, P. (1992). The Bootstrap and Edgeworth Expansion.
     *     Springer Series in Statistics.
     *   - Davison, A. C., & Hinkley, D. V. (1997). Bootstrap Methods and 
     *     Their Application. Cambridge Series in Statistical and Probabilistic
     *     Mathematics.
     *
     * M-OUT-OF-N BOOTSTRAP:
     *   - Politis, D. N., Romano, J. P., & Wolf, M. (1999). Subsampling.
     *     Springer Series in Statistics.
     *   - Bickel, P. J., Götze, F., & van Zwet, W. R. (1997). "Resampling Fewer
     *     Than n Observations: Gains, Losses, and Remedies for Losses."
     *     Statistica Sinica, 7, 1-31.
     *
     * COMPARATIVE STUDIES:
     *   - Carpenter, J., & Bithell, J. (2000). "Bootstrap Confidence Intervals:
     *     When, Which, What? A Practical Guide for Medical Statisticians."
     *     Statistics in Medicine, 19, 1141-1164.
     *   - Diciccio, T. J., & Efron, B. (1996). "Bootstrap Confidence Intervals."
     *     Statistical Science, 11(3), 189-228.
     *
     * ============================================================================
     * USAGE EXAMPLE
     * ============================================================================
     *
     * @code
     * // 1. Prepare data and statistic
     * std::vector<double> returns = { ... daily returns ... };
     * auto statistic = [](const std::vector<double>& x) {
     *   return computeProfitFactor(x);
     * };
     *
     * // 2. Configure selector
     * AutoBootstrapSelector selector(
     *   25000,  // B_outer
     *   0.95,   // confidence level
     *   true    // enforce positive (for ProfitFactor)
     * );
     *
     * // 3. Run tournament
     * auto result = selector.select(
     *   returns,
     *   statistic,
     *   true  // isRatioStatistic
     * );
     *
     * // 4. Inspect winner
     * std::cout << "Selected method: " << result.methodName() << "\n";
     * std::cout << "95% CI: [" << result.lower << ", " << result.upper << "]\n";
     * std::cout << "Diagnostics:\n";
     * std::cout << "  Skewness: " << result.skewness << "\n";
     * std::cout << "  Normalized length: " << result.normalized_length << "\n";
     * if (result.method == MethodId::BCa) {
     *   std::cout << "  z0: " << result.z0 << "\n";
     *   std::cout << "  accel: " << result.accel << "\n";
     * }
     * @endcode
     *
     * ============================================================================
     * IMPLEMENTATION NOTES
     * ============================================================================
     *
     * - Thread safety: Each bootstrap method uses per-replicate RNG engines
     *   (via CRN provider pattern) to ensure deterministic, order-independent
     *   results across parallel execution.
     *
     * - Degenerate case handling: All methods skip invalid replicates (NaN/Inf)
     *   and track diagnostics (skipped_outer, skipped_inner, effective_B).
     *
     * - Early stopping: Percentile-T adaptively halts inner loops when SE*
     *   stabilizes to ±1.5%, reducing computation by 85-95% without sacrificing
     *   accuracy.
     *
     * - Memory efficiency: Bootstrap statistics are stored only when needed for
     *   diagnostics (skewness, coverage checks). Interval computation uses
     *   nth_element (O(n)) instead of full sorting (O(n log n)).
     *
     */
    template <class Decimal>
    class AutoBootstrapSelector
    {
    public:
      using Result    = AutoCIResult<Decimal>;
      using MethodId  = typename Result::MethodId;
      using Candidate = typename Result::Candidate;
      using SelectionDiagnostics = typename Result::SelectionDiagnostics;

      /**
       * @brief Configuration for the scoring algorithm weights and penalties.
       *
       * Defines how much importance is placed on different aspects of interval quality
       * (Center Shift, Skewness Match, Length Efficiency, Stability) when calculating
       * the total penalty score.
       */
      class ScoringWeights
      {
      public:
	/**
         * @brief Constructs a weight profile.
         *
         * @param wCenterShift Weight for the center shift penalty (default 1.0).
         * @param wSkew Weight for skewness fidelity (default 0.5).
         * @param wLength Weight for length efficiency (default 0.25).
         * @param wStability Weight for numerical stability (default 1.0).
         * @param enforcePos If true, penalizes intervals with lower bounds < 0 (default false).
         * @param bcaZ0Scale Scaling factor for BCa bias penalty (default 20.0).
         * @param bcaAScale Scaling factor for BCa acceleration penalty (default 100.0).
         */
      ScoringWeights(double wCenterShift = 1.0,
                     double wSkew        = 0.5,
                     double wLength      = 0.25,
                     double wStability   = 1.0,
                     bool   enforcePos   = false,
                     // BCa penalty scales (configurable)
                     double bcaZ0Scale   = 20.0,
                     double bcaAScale    = 100.0)
          : m_w_center_shift(wCenterShift),
            m_w_skew(wSkew),
            m_w_length(wLength),
            m_w_stability(wStability),
            m_enforce_positive(enforcePos),
            m_bca_z0_scale(bcaZ0Scale),
            m_bca_a_scale(bcaAScale)
        {}

        double getCenterShiftWeight() const
	{
	  return m_w_center_shift;
	}

        double getSkewWeight() const
	{
	  return m_w_skew;
	}

        double getLengthWeight() const
	{
	  return m_w_length;
	}

        double getStabilityWeight() const
	{
	  return m_w_stability;
	}

        bool enforcePositive() const
	{
	  return m_enforce_positive;
	}

        double getBcaZ0Scale() const
	{
	  return m_bca_z0_scale;
	}

        double getBcaAScale() const
	{
	  return m_bca_a_scale;
	}

      private:
        double m_w_center_shift;
        double m_w_skew;
        double m_w_length;
        double m_w_stability;
        bool m_enforce_positive;
        double m_bca_z0_scale;
        double m_bca_a_scale;
      };

      /**
       * @brief Compute rejection mask for a candidate based on various failure conditions.
       * 
       * This determines why a candidate might be rejected during selection, tracking
       * multiple simultaneous rejection reasons using a bitmask.
       */
      static CandidateReject computeRejectionMask(const Candidate& candidate,
						  double total_score,
						  double domain_penalty,
						  bool passes_effective_b_gate)
      {
	CandidateReject mask = CandidateReject::None;

	// 1) Score must be finite (hard gate)
	if (!std::isfinite(total_score))
	  mask |= CandidateReject::ScoreNonFinite;

	// 2) Support/domain must be satisfied (hard gate)
	if (domain_penalty > 0.0)
	  mask |= CandidateReject::ViolatesSupport;

	// 3) Effective-B gate (hard gate)
	if (!passes_effective_b_gate)
	  mask |= CandidateReject::EffectiveBLow;

	if (candidate.getMethod() == MethodId::BCa)
	  {
	    const double z0    = candidate.getZ0();
	    const double accel = candidate.getAccel();

	    if (!std::isfinite(z0) || !std::isfinite(accel))
	      mask |= CandidateReject::BcaParamsNonFinite;

	    if (std::isfinite(z0) && std::fabs(z0) > AutoBootstrapConfiguration::kBcaZ0HardLimit)
	      mask |= CandidateReject::BcaZ0HardFail;

	    if (std::isfinite(accel) && std::fabs(accel) > AutoBootstrapConfiguration::kBcaAHardLimit)
	      mask |= CandidateReject::BcaAccelHardFail;
	  }

	if (candidate.getMethod() == MethodId::PercentileT)
	  {
	    const double inner_fail_rate = candidate.getInnerFailureRate();
	    if (std::isfinite(inner_fail_rate) &&
		inner_fail_rate > AutoBootstrapConfiguration::kPercentileTInnerFailThreshold)
	      {
		mask |= CandidateReject::PercentileTInnerFails;
	      }

	    const std::size_t requested = candidate.getBOuter();
	    if (requested > 0)
	      {
		const double eff_frac = static_cast<double>(candidate.getEffectiveB()) /
		  static_cast<double>(requested);
		if (eff_frac < AutoBootstrapConfiguration::kPercentileTMinEffectiveFraction)
		  mask |= CandidateReject::PercentileTLowEffB;
	      }
	  }
	return mask;
      }
      
      bool checkSupportViolation(const Candidate& candidate,
				 const StatisticSupport& support)
      {
	double lower = candidate.getLower();
	return support.violatesLowerBound(lower);
      }
      
      
      /**
       * @brief Extract support bounds from a StatisticSupport constraint.
       *
       * @param support The support constraint
       * @return Pair of (lower_bound, upper_bound). Upper bound is always NaN
       *         since StatisticSupport only constrains lower bounds.
       *         Lower bound is NaN if no constraint exists.
       */
      static std::pair<double, double> getSupportBounds(const StatisticSupport& support)
      {
	double lower_bound = std::numeric_limits<double>::quiet_NaN();
	double upper_bound = std::numeric_limits<double>::quiet_NaN();
    
	if (support.hasLowerBound()) {
	  lower_bound = support.lowerBound();
	}
    
	// StatisticSupport only constrains lower bounds, not upper bounds
	// upper_bound remains NaN
    
	return {lower_bound, upper_bound};
      }
      
      // EmpiricalMassResult now available from BootstrapPenaltyCalculator
      using EmpiricalMassResult = typename BootstrapPenaltyCalculator<Decimal>::EmpiricalMassResult;

      // Empirical under-coverage penalties now available from BootstrapPenaltyCalculator

      // Length penalty methods now available from BootstrapPenaltyCalculator

      /**
       * @brief Creates a Candidate summary for "simple" percentile-like methods.
       *
       * Handles Normal, Basic, Percentile, and MOutOfN methods. It computes standard
       * metrics (ordering penalty, length penalty) but assumes no complex stability
       * parameters (like z0 or accel) are needed.
       *
       * @param method The method ID (e.g., MethodId::Percentile).
       * @param engine The bootstrap engine instance used to generate results.
       * @param res The result structure from the engine.
       * @return A populated Candidate object.
       * @throws std::logic_error If diagnostics are missing or bootstrap stats are insufficient.
       */
      template <class BootstrapEngine>
      static Candidate summarizePercentileLike(MethodId                                method,
					       const BootstrapEngine&                  engine,
					       const typename BootstrapEngine::Result& res)
      {
	if (!engine.hasDiagnostics())
	  {
	    throw std::logic_error(
				   "AutoBootstrapSelector: diagnostics not available for percentile-like engine (run() not called?).");
	  }

	const auto& stats = engine.getBootstrapStatistics();
	const std::size_t m = stats.size();
	if (m < 2)
	  {
	    throw std::logic_error(
				   "AutoBootstrapSelector: need at least 2 bootstrap statistics for percentile-like engine.");
	  }

	const double mean_boot = engine.getBootstrapMean();
	const double se_boot   = engine.getBootstrapSe();

	// Guard against degenerate distribution in skewness computation
	// If se_boot = 0, all bootstrap statistics are identical (degenerate case)
	const double skew_boot = (se_boot > 0.0)
	  ? mkc_timeseries::StatUtils<double>::computeSkewness(stats, mean_boot, se_boot)
	  : 0.0;

	const double mu  = num::to_double(res.mean);
	const double lo  = num::to_double(res.lower);
	const double hi  = num::to_double(res.upper);
	const double len = hi - lo;

	// ====================================================================
	// CENTER SHIFT PENALTY
	// ====================================================================
	double center_shift_in_se = 0.0;
	if (se_boot > 0.0 && len > 0.0)
	  {
	    const double center = 0.5 * (lo + hi);
	    center_shift_in_se = std::fabs(center - mu) / se_boot;
	  }

	// ORDERING PENALTY (coverage accuracy)
	//
	// We skip the ordering penalty for Basic and M-out-of-N bootstrap methods
	// because checking empirical coverage against their bootstrap distributions
	// is either methodologically incorrect or introduces numerical artifacts.
	//
	// BASIC BOOTSTRAP:
	//   Basic uses reflection (2*theta_hat - quantiles) to construct its interval,
	//   which is DESIGNED to deviate from the bootstrap distribution when bias or
	//   skewness exists. The reflection transformation means the interval endpoints
	//   are NOT quantiles of the bootstrap distribution, so checking whether they
	//   capture 95% of that distribution is meaningless. Penalizing this deviation
	//   is methodologically incorrect.
	//
	// M-OUT-OF-N BOOTSTRAP:
	//   M-out-of-N is a second-order accurate method (like BCa and Percentile-T)
	//   that uses subsampling with rescaling to improve finite-sample coverage.
	//   With rescaling enabled, the method works in rescaled distribution space,
	//   and checking discrete empirical coverage introduces systematic numerical
	//   artifacts from:
	//     - Quantile interpolation (Type-7 uses linear interpolation between points)
	//     - Discrete coverage counting (counting replicates vs continuous quantiles)
	//     - Finite-sample variation (95.05% vs exactly 95.00%)
	//   
	//   These artifacts are typically O(10^-4 to 10^-3), which is negligible in
	//   absolute terms but prevents M-out-of-N from competing with BCa/Percentile-T
	//   (which have ordering_penalty explicitly set to 0 in their respective
	//   summarize functions). Empirical data shows 99.9% of M-out-of-N candidates
	//   have orderingRaw > 0 (median ~10^-4), not due to poor coverage but due to
	//   these numerical artifacts. Exempting M-out-of-N allows it to compete on
	//   equal footing with other second-order accurate methods.
	//
	// Note: BCa and Percentile-T are also exempt from ordering checks, but this
	// is handled by explicitly setting ordering_penalty = 0 in summarizeBCa() and
	// summarizePercentileT() rather than in this function.
	// ====================================================================
	double ordering_penalty = 0.0;

	if ((method != MethodId::Basic) && (method != MethodId::MOutOfN))
	  {
	    const double coverage_target = res.cl;

	    // Empirical CDF values at the interval endpoints (still used for center_pen)
	    const double F_lo = palvalidator::analysis::detail::compute_empirical_cdf(stats, lo);
	    const double F_hi = palvalidator::analysis::detail::compute_empirical_cdf(stats, hi);

	    // Inclusive mass inside [lo, hi] (tie-safe), plus effective sample size
	    const typename BootstrapPenaltyCalculator<Decimal>::EmpiricalMassResult mass_result =
	      BootstrapPenaltyCalculator<Decimal>::compute_empirical_mass_inclusive(stats, lo, hi);

	    const std::size_t B_eff = mass_result.effective_sample_count;

	    if (B_eff >= 2)
	      {
		const double width_cdf = std::clamp(mass_result.mass_inclusive, 0.0, 1.0);

		// Under-coverage with half-step tolerance (finite-B granularity)
		const double under_coverage =
		  BootstrapPenaltyCalculator<Decimal>::compute_under_coverage_with_half_step_tolerance(width_cdf, coverage_target, B_eff);

		// Symmetric tolerance for over-coverage as well
		const double step = (B_eff > 0) ? (1.0 / static_cast<double>(B_eff)) : 1.0;
		const double tol  = 0.5 * step;
		const double over_coverage = std::max(0.0, (width_cdf - coverage_target) - tol);

		const double cov_pen =
		  AutoBootstrapConfiguration::kUnderCoverageMultiplier * under_coverage * under_coverage +
		  AutoBootstrapConfiguration::kOverCoverageMultiplier  * over_coverage  * over_coverage;

		const double F_mu       = palvalidator::analysis::detail::compute_empirical_cdf(stats, mu);
		const double center_cdf = 0.5 * (F_lo + F_hi);
		const double center_pen = (center_cdf - F_mu) * (center_cdf - F_mu);

		ordering_penalty = cov_pen + center_pen;
	      }
	    else
	      {
		ordering_penalty = 0.0;
	      }
	  }
	// else: ordering_penalty remains 0.0 for Basic bootstrap

	double normalized_length = 1.0;
	double median_val = 0.0;

	double length_penalty = 0.0;
	if (method == MethodId::Normal)
	  {
	    // Use SE-based ideal for Normal
	    length_penalty = BootstrapPenaltyCalculator<Decimal>::computeLengthPenalty_Normal(len, se_boot, res.cl, normalized_length, median_val);
	  }
	else
	  {
	    // Use percentile quantile ideal for Percentile, BCa, Basic, MOutOfN
	    length_penalty = BootstrapPenaltyCalculator<Decimal>::computeLengthPenalty_Percentile(len, stats, res.cl, method, normalized_length, median_val);
	  }

	return Candidate(
			 method,
			 res.mean,
			 res.lower,
			 res.upper,
			 res.cl,
			 res.n,
			 res.B,            // B_outer
			 0,                // B_inner (N/A for percentile-like)
			 res.effective_B,
			 res.skipped,      // skipped_total
			 se_boot,
			 skew_boot,
			 median_val,       // Always populated by computeLengthPenalty
			 center_shift_in_se,
			 normalized_length,
			 ordering_penalty,
			 length_penalty,
			 0.0,              // stability_penalty (N/A for Percentile-like)
			 0.0,              // z0 (N/A)
			 0.0,              // accel (N/A)
			 0.0
			 );
      }
      
      // Stability penalty methods now available from BootstrapPenaltyCalculator

      /**
       * @brief Computes stability penalty for Percentile-T based on resample quality.
       *
       * Evaluates the reliability of the double-bootstrap procedure by checking:
       * - Outer resample failure rate (must be <= 10%).
       * - Inner SE estimation failure rate (must be <= 5%).
       * - Effective sample size (must be >= 70% of total outer samples).
       *
       * @param res The result structure from the Percentile-T engine.
       * @return A penalty score (0.0 if stable, increasing values for instability).
       */
      template <class PTBootstrap>
      static Candidate summarizePercentileT(const PTBootstrap& engine,
					    const typename PTBootstrap::Result& res,
					    std::ostream* os = nullptr)
      {
	if (!engine.hasDiagnostics())
	  {
	    throw std::logic_error(
				   "AutoBootstrapSelector: percentile-t diagnostics not available (run() not called?).");
	  }

	// -------------------------------------------------------------------------
	// θ* (finite-only)
	// -------------------------------------------------------------------------
	const auto& theta_stats_all = engine.getThetaStarStatistics();
	std::vector<double> theta_stats;
	theta_stats.reserve(theta_stats_all.size());
	for (double v : theta_stats_all)
	  {
	    if (std::isfinite(v)) theta_stats.push_back(v);
	  }

	if (theta_stats.size() < 2)
	  {
	    throw std::logic_error(
				   "AutoBootstrapSelector: need at least 2 finite theta* statistics for percentile-t.");
	  }

	double sum = 0.0;
	for (double v : theta_stats) sum += v;
	const double mean_boot = sum / static_cast<double>(theta_stats.size());

	const double se_boot_calc = mkc_timeseries::StatUtils<double>::computeStdDev(theta_stats);

	double se_ref = res.se_hat;
	if (!(se_ref > 0.0))
	  se_ref = se_boot_calc;

	const double skew_boot = (se_boot_calc > 0.0)
	  ? mkc_timeseries::StatUtils<double>::computeSkewness(theta_stats, mean_boot, se_boot_calc)
	  : 0.0;

	const double lo  = num::to_double(res.lower);
	const double hi  = num::to_double(res.upper);
	const double len = hi - lo;

	const double center_shift_in_se = 0.0; // intentionally not used for PT

	// -------------------------------------------------------------------------
	// T* (finite-only)
	// -------------------------------------------------------------------------
	const auto& t_stats_all = engine.getTStatistics();
	std::vector<double> t_stats;
	t_stats.reserve(t_stats_all.size());
	for (double v : t_stats_all)
	  {
	    if (std::isfinite(v)) t_stats.push_back(v);
	  }

	double ordering_penalty = 0.0;
	double length_penalty = 0.0;
	double normalized_length = 1.0;
	double median_boot = 0.0;

	// Compute median from theta_stats (the actual statistic values)
	if (theta_stats.size() >= 2)
	  {
	    std::vector<double> theta_sorted(theta_stats.begin(), theta_stats.end());
	    std::sort(theta_sorted.begin(), theta_sorted.end());
	    median_boot = mkc_timeseries::StatUtils<double>::computeMedianSorted(theta_sorted);
	  }
	
	if (t_stats.size() >= 2)
	  {
	    double median_t_dummy = 0.0;
	    length_penalty =
	      BootstrapPenaltyCalculator<Decimal>::computeLengthPenalty_PercentileT(len,
	           t_stats,
	           se_ref,
	           res.cl,
	           normalized_length,
	           median_t_dummy
	           );
	  }

	auto stability_penalty = BootstrapPenaltyCalculator<Decimal>::computePercentileTStability(res);

	if (os && (stability_penalty > 0.0))
	  {
	    (*os) << "summarizePercentileT: stability penalty is > 0 and has value "
		  << stability_penalty << std::endl;
	  }

	double inner_failure_rate = 0.0;
	if (res.inner_attempted_total > 0)
	  {
	    inner_failure_rate =
	      static_cast<double>(res.skipped_inner_total) /
	      static_cast<double>(res.inner_attempted_total);
	  }

	return Candidate(
			 MethodId::PercentileT,
			 res.mean,
			 res.lower,
			 res.upper,
			 res.cl,
			 res.n,
			 res.B_outer,
			 res.B_inner,
			 res.effective_B,
			 res.skipped_outer + res.skipped_inner_total,
			 se_ref,
			 skew_boot,
			 median_boot,
			 center_shift_in_se,
			 normalized_length,
			 ordering_penalty,
			 length_penalty,
			 stability_penalty,
			 0.0,              // z0 (N/A)
			 0.0,              // accel (N/A)
			 inner_failure_rate
			 );
      }

      /**
       * @brief Creates a Candidate summary specifically for the BCa method.
       *
       * Calculates BCa-specific stability penalties based on the bias-correction (z0)
       * and acceleration (a) parameters. Applies soft penalties for parameters exceeding
       * theoretical safety thresholds (e.g., |z0| > 0.3) to prevent selection when the
       * Edgeworth expansion approximation is likely unstable.
       *
       * @param bca The BCa engine instance.
       * @param weights The scoring weights configuration (for scaling penalties).
       * @param os Optional output stream for debug logging.
       * @return A populated Candidate object for BCa.
       */
      template <class BCaEngine>
      static Candidate summarizeBCa(const BCaEngine& bca,
				    const ScoringWeights& weights = ScoringWeights(),
				    std::ostream* os = nullptr)
      {
	const Decimal mean   = bca.getMean();
	const Decimal lower  = bca.getLowerBound();
	const Decimal upper  = bca.getUpperBound();
	const double  cl     = bca.getConfidenceLevel();
	const unsigned int B = bca.getNumResamples();
	const std::size_t n  = bca.getSampleSize();

	const double  z0      = bca.getZ0();
	const Decimal accelD  = bca.getAcceleration();
	const double  accel   = num::to_double(accelD);

	const auto& statsD = bca.getBootstrapStatistics();
	if (statsD.size() < 2)
	  {
	    throw std::logic_error(
				   "AutoBootstrapSelector: need at least 2 bootstrap stats for BCa engine.");
	  }

	// Convert to doubles for diagnostics/selection metrics, keeping only finite values.
	std::vector<double> stats;
	stats.reserve(statsD.size());
	for (const auto& d : statsD)
	  {
	    const double v = num::to_double(d);
	    if (std::isfinite(v))
	      stats.push_back(v);
	  }

	const std::size_t m = stats.size();
	if (m < 2)
	  {
	    throw std::logic_error(
				   "AutoBootstrapSelector: need at least 2 finite bootstrap stats for BCa engine.");
	  }

	double sum = 0.0;
	for (double v : stats) sum += v;
	const double mean_boot = sum / static_cast<double>(m);

	// Bootstrap Standard Error Estimation:
	// sd({θ*}) ≈ SE(θ̂). We compute SD of θ*, not SD/sqrt(B).
	const double se_boot = mkc_timeseries::StatUtils<double>::computeStdDev(stats);

	// Guard against degenerate distribution in skewness computation
	const double skew_boot = (se_boot > 0.0)
	  ? mkc_timeseries::StatUtils<double>::computeSkewness(stats, mean_boot, se_boot)
	  : 0.0;

	const double lo  = num::to_double(lower);
	const double hi  = num::to_double(upper);
	const double len = hi - lo;

	// CENTER SHIFT PENALTY: NOT COMPUTED FOR BCa
	// (BCa intentionally produces asymmetric intervals when appropriate.)
	const double center_shift_in_se = 0.0;

	// ====================================================================
	// LENGTH PENALTY
	// ====================================================================
	double normalized_length = 1.0;
	double median_boot = 0.0;

	const double length_penalty = BootstrapPenaltyCalculator<Decimal>::computeLengthPenalty_Percentile(
								      len,
								      stats,
								      cl,
								      MethodId::BCa,
								      normalized_length,
								      median_boot
								      );

	// ====================================================================
	// STABILITY PENALTY (z0 / accel / skew thresholds etc.)
	// ====================================================================
	// Pass the AutoBootstrapSelector::ScoringWeights directly to the penalty calculator
	const double stability_penalty = BootstrapPenaltyCalculator<Decimal>::computeBCaStabilityPenalty(
								    z0,
								    accel,
								    skew_boot,
								    weights,
								    os
								    );

	const double ordering_penalty = 0.0;


	// effective_B should correspond to the stats we actually used (finite-only)
	const std::size_t effective_B = m;

	// "skipped" is how many outer resamples did not produce a usable statistic
	const std::size_t skipped = (B > effective_B) ? (B - effective_B) : 0;

	return Candidate(
			 MethodId::BCa,
			 mean,
			 lower,
			 upper,
			 cl,
			 n,
			 B,
			 0,             // B_inner (not used by BCa)
			 effective_B,
			 skipped,
			 se_boot,
			 skew_boot,
			 median_boot,
			 center_shift_in_se,
			 normalized_length,
			 ordering_penalty,
			 length_penalty,
			 stability_penalty,
			 z0,
			 accel,
			 0.0            // inner_failure_rate (not applicable to BCa)
			 );
      }
      
      // ===========================================================================
      // TYPE ALIASES FOR MOVED CLASSES
      // ===========================================================================
 
      // RawComponents class moved to AutoBootstrapScoring.h
      using RawComponents = detail::RawComponents;
      using RawComponentsBuilder = detail::RawComponentsBuilder<Decimal>;

      // ===========================================================================
      // PHASE 1: Compute raw penalty components
      // ===========================================================================
      
      // Raw penalty computation methods moved to RawComponentsBuilder in AutoBootstrapScoring.h
      
      /**
       * @brief Check if any candidate is BCa
       */
      static bool containsBcaCandidate(const std::vector<Candidate>& candidates)
      {
        for (const auto& c : candidates)
        {
          if (c.getMethod() == MethodId::BCa)
            return true;
        }
        return false;
      }
      
      // ===========================================================================
      // PHASE 2: Normalize and score candidates
      // ===========================================================================
      
      /**
       * @brief Normalize penalties and compute scores for all candidates
       *
       * @return Pair of (enriched candidates, score breakdowns)
       */
      static std::pair<std::vector<Candidate>,
                      std::vector<typename SelectionDiagnostics::ScoreBreakdown>>
      normalizeAndScoreCandidates(const std::vector<Candidate>& candidates,
				  const std::vector<RawComponents>& raw,
				  const ScoringWeights& weights,
				  const StatisticSupport& support,
				  const std::pair<double, double>& support_bounds,
				  uint64_t& candidate_id_counter)
      {
        detail::ScoreNormalizer<Decimal, ScoringWeights, RawComponents> normalizer(weights);
        
        std::vector<Candidate> enriched;
        enriched.reserve(candidates.size());
        
        std::vector<typename SelectionDiagnostics::ScoreBreakdown> breakdowns;
        breakdowns.reserve(candidates.size());
        
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
          const Candidate& c = candidates[i];
          const RawComponents& r = raw[i];
          
          // Normalize scores
          auto norm = normalizer.normalize(r);
          
          // Compute total score
          double total_score = normalizer.computeTotalScore(
            norm, r, c.getMethod(), c.getLengthPenalty());
          
          // Compute rejection info
          bool passes_eff_b = palvalidator::analysis::detail::CandidateGateKeeper<Decimal, RawComponents>::passesEffectiveBGate(c);
	  
          auto rejection_mask = computeRejectionMask(c,
						     total_score,
						     r.getDomainPenalty(),
						     passes_eff_b);
	  
          std::string rejection_text = rejectionMaskToString(rejection_mask);

          bool violates_support = (r.getDomainPenalty() > 0.0);
          
          // Create score breakdown
          breakdowns.emplace_back(c.getMethod(),
				  r.getOrderingPenalty(),
				  r.getLengthPenalty(),
				  r.getStabilityPenalty(),
				  r.getCenterShiftSq(),
				  r.getSkewSq(),
				  r.getDomainPenalty(),
				  norm.getOrderingNorm(),
				  norm.getLengthNorm(),
				  norm.getStabilityNorm(),
				  norm.getCenterSqNorm(),
				  norm.getSkewSqNorm(),
				  norm.getOrderingContrib(),
				  norm.getLengthContrib(),
				  norm.getStabilityContrib(),
				  norm.getCenterSqContrib(),
				  norm.getSkewSqContrib(),
				  r.getDomainPenalty(),
				  total_score,
				  rejection_mask,
				  rejection_text,
				  false,
				  violates_support,
				  support_bounds.first,
				  support_bounds.second);
          
          // Create enriched candidate
          enriched.push_back(
            c.withScore(total_score).withMetadata(candidate_id_counter++, 0, false));
        }
        
        return {enriched, breakdowns};
      }
      
      // ===========================================================================
      // PHASE 3: Tournament selection
      // ===========================================================================
      
      /**
       * @brief Select the winner from valid candidates
       *
       * @return Winner index
       */
      static std::size_t selectWinnerIndex(const std::vector<Candidate>& enriched,
					   const std::vector<RawComponents>& raw,
					   double& tie_epsilon_used)
      {
        detail::CandidateGateKeeper<Decimal, RawComponents> gatekeeper;
        detail::ImprovedTournamentSelector<Decimal> selector(enriched);
        
        for (std::size_t i = 0; i < enriched.size(); ++i)
        {
          const MethodId method = enriched[i].getMethod();
          const bool is_bca = (method == MethodId::BCa);
          
          const bool is_valid = is_bca
            ? gatekeeper.isBcaCandidateValid(enriched[i], raw[i])
            : gatekeeper.isCommonCandidateValid(enriched[i], raw[i]);
          
          if (is_valid)
          {
            selector.consider(i);
          }
        }
        
        if (!selector.hasWinner())
        {
          throw std::runtime_error(
            "AutoBootstrapSelector::select: no valid candidate "
            "(all scores non-finite or domain-violating).");
        }
        
        tie_epsilon_used = selector.getTieEpsilon();
        return selector.getWinnerIndex();
      }
      
      // ===========================================================================
      // PHASE 4: Assign ranks
      // ===========================================================================
      
      /**
       * @brief Assign ranks to eligible candidates and mark winner
       */
      static void assignRanks(std::vector<Candidate>& enriched,
			      const std::vector<RawComponents>& raw,
			      std::size_t winner_idx)
      {
        detail::CandidateGateKeeper<Decimal, RawComponents> gatekeeper;
        
        // Sort indices by score
        std::vector<std::size_t> idx(enriched.size());
        std::iota(idx.begin(), idx.end(), 0);
        
        std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
          return enriched[a].getScore() < enriched[b].getScore();
        });
        
        // Get winner ID before we modify anything
        const uint64_t winner_id = enriched[winner_idx].getCandidateId();
        
        // Assign ranks to eligible candidates
        std::size_t rank = 1;
        for (std::size_t k = 0; k < idx.size(); ++k)
        {
          const std::size_t i = idx[k];
          const MethodId m = enriched[i].getMethod();
          const bool is_bca = (m == MethodId::BCa);
          
          const bool is_valid = is_bca
            ? gatekeeper.isBcaCandidateValid(enriched[i], raw[i])
            : gatekeeper.isCommonCandidateValid(enriched[i], raw[i]);
          
          if (is_valid)
          {
            enriched[i] = enriched[i].withMetadata(
              enriched[i].getCandidateId(), rank, false);
            ++rank;
          }
          else
          {
            enriched[i] = enriched[i].withMetadata(
              enriched[i].getCandidateId(), 0, false);
          }
        }
        
        // Mark winner
        for (std::size_t i = 0; i < enriched.size(); ++i)
        {
          if (enriched[i].getCandidateId() == winner_id)
          {
            enriched[i] = enriched[i].markAsChosen();
            break;
          }
        }
      }
      
      // ===========================================================================
      // PHASE 5: Analyze BCa rejection
      // ===========================================================================
      
      /**
       * @brief Analyze why BCa was rejected (if applicable)
       */
      static detail::BcaRejectionAnalysis
      analyzeBcaRejection(const std::vector<Candidate>& enriched,
			  const std::vector<RawComponents>& raw,
			  std::size_t winner_idx,
			  bool has_bca_candidate)
      {
        // If no BCa candidate, return early with appropriate state
        if (!has_bca_candidate)
        {
          return detail::BcaRejectionAnalysis(false, // has_bca_candidate
					      false, // bca_chosen
					      false, // rejected_for_instability
					      false, // rejected_for_length
					      false, // rejected_for_domain
					      false  // rejected_for_non_finite
					      );
        }
        
        const Candidate& winner = enriched[winner_idx];
        bool bca_chosen = (winner.getMethod() == MethodId::BCa);
        
        // If BCa was chosen, no rejection analysis needed
        if (bca_chosen)
        {
          return detail::BcaRejectionAnalysis(true,  // has_bca_candidate
					      true,  // bca_chosen
					      false, // rejected_for_instability
					      false, // rejected_for_length
					      false, // rejected_for_domain
					      false  // rejected_for_non_finite
					      );
        }
        
        // Analyze rejection reasons for BCa candidate
        bool rejected_for_non_finite = false;
        bool rejected_for_domain = false;
        bool rejected_for_instability = false;
        bool rejected_for_length = false;
        
        // Find the BCa candidate and determine why it was rejected
        for (std::size_t i = 0; i < enriched.size(); ++i)
        {
          if (enriched[i].getMethod() != MethodId::BCa)
            continue;
          
          const Candidate& bca = enriched[i];
          
          if (!std::isfinite(bca.getScore()))
            rejected_for_non_finite = true;
          
          if (raw[i].getDomainPenalty() > 0.0)
            rejected_for_domain = true;
          
          if (!std::isfinite(bca.getZ0()) || !std::isfinite(bca.getAccel()) ||
              std::fabs(bca.getZ0()) > AutoBootstrapConfiguration::kBcaZ0HardLimit ||
              std::fabs(bca.getAccel()) > AutoBootstrapConfiguration::kBcaAHardLimit)
          {
            rejected_for_instability = true;
          }
          
          if (bca.getLengthPenalty() >
              AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold)
          {
            rejected_for_length = true;
          }
          
          break; // Only one BCa candidate
        }
        
        return detail::BcaRejectionAnalysis(true, // has_bca_candidate
					    false, // bca_chosen (we already checked this)
					    rejected_for_instability,
					    rejected_for_length,
					    rejected_for_domain,
					    rejected_for_non_finite
					    );
      }
      
      // ===========================================================================
      // HELPER UTILITIES
      // ===========================================================================
      
      static StatisticSupport computeEffectiveSupport(const StatisticSupport& support,
						      const ScoringWeights& weights)
      {
        StatisticSupport effective_support = support;
        
        if (!effective_support.hasLowerBound() && weights.enforcePositive())
        {
          effective_support = StatisticSupport::strictLowerBound(
            0.0, AutoBootstrapConfiguration::kPositiveLowerEpsilon);
        }
        
        return effective_support;
      }
      
      static void validateInputs(const std::vector<Candidate>& candidates)
      {
        if (candidates.empty())
        {
          throw std::invalid_argument(
            "AutoBootstrapSelector::select: no candidates provided.");
        }
      }

      /**
        * @brief Executes the selection tournament to find the best bootstrap method.
        *
        * REFACTORED VERSION: This method now orchestrates the selection process
        * through clearly defined phases, each handled by a dedicated helper method.
        *
        * @param candidates A list of Candidate objects generated by the summarize methods.
        * @param weights Configuration for scoring weights.
        * @param support Statistic support constraints (e.g., positive-only).
        * @return An AutoCIResult containing the winner and full diagnostics.
        * @throws std::runtime_error If no valid candidates remain after gating.
        */
      static Result select(const std::vector<Candidate>& candidates,
			   const ScoringWeights& weights = ScoringWeights(),
			   const StatisticSupport& support = StatisticSupport::unbounded())
      {
        // =====================================================================
        // VALIDATION
        // =====================================================================
        validateInputs(candidates);
        
        // =====================================================================
        // SETUP
        // =====================================================================
        StatisticSupport effective_support = computeEffectiveSupport(support, weights);
        auto support_bounds = getSupportBounds(effective_support);
        uint64_t candidate_id = 0;
        
        // =====================================================================
        // PHASE 1: Compute raw penalties
        // =====================================================================
        auto raw = RawComponentsBuilder::computeRawPenalties(candidates, effective_support);
        bool has_bca = containsBcaCandidate(candidates);
        
        // =====================================================================
        // PHASE 2: Normalize and score
        // =====================================================================
        auto [enriched, breakdowns] = normalizeAndScoreCandidates(candidates,
								  raw,
								  weights,
								  effective_support,
								  support_bounds,
								  candidate_id);
        
        // Update breakdowns with passed_gates status
        updateBreakdownsWithGateStatus(enriched, raw, breakdowns);
        
        // =====================================================================
        // PHASE 3: Tournament selection
        // =====================================================================
        double tie_epsilon_used = 0.0;
        std::size_t winner_idx = selectWinnerIndex(enriched, raw, tie_epsilon_used);
        
        // =====================================================================
        // PHASE 4: Assign ranks
        // =====================================================================
        assignRanks(enriched, raw, winner_idx);
        
        // =====================================================================
        // PHASE 5: BCa diagnostics
        // =====================================================================
        auto bca_analysis = analyzeBcaRejection(enriched, raw, winner_idx, has_bca);
        
        // =====================================================================
        // BUILD RESULT
        // =====================================================================
        const Candidate& winner = enriched[winner_idx];
        
        SelectionDiagnostics diagnostics(
          winner.getMethod(),
          Result::methodIdToString(winner.getMethod()),
          winner.getScore(),
          winner.getStabilityPenalty(),
          winner.getLengthPenalty(),
          bca_analysis.hasBcaCandidate(),
          bca_analysis.bcaChosen(),
          bca_analysis.rejectedForInstability(),
          bca_analysis.rejectedForLength(),
          bca_analysis.rejectedForDomain(),
          bca_analysis.rejectedForNonFinite(),
          enriched.size(),
          std::move(breakdowns),
          tie_epsilon_used);
        
        return Result(winner.getMethod(), winner, enriched, diagnostics);
      }
      
    private:
      /**
       * @brief Update score breakdowns with passed_gates status
       */
      static void updateBreakdownsWithGateStatus(
        const std::vector<Candidate>& enriched,
        const std::vector<RawComponents>& raw,
        std::vector<typename SelectionDiagnostics::ScoreBreakdown>& breakdowns)
      {
        detail::CandidateGateKeeper<Decimal, RawComponents> gatekeeper;
        
        for (std::size_t i = 0; i < enriched.size(); ++i)
        {
          const MethodId m = enriched[i].getMethod();
          const bool is_bca = (m == MethodId::BCa);
          
          const bool ok = is_bca
            ? gatekeeper.isBcaCandidateValid(enriched[i], raw[i])
            : gatekeeper.isCommonCandidateValid(enriched[i], raw[i]);
          
          // Rebuild the ScoreBreakdown with passed_gates updated
          breakdowns[i] = typename SelectionDiagnostics::ScoreBreakdown(
            breakdowns[i].getMethod(),
            breakdowns[i].getOrderingRaw(),
            breakdowns[i].getLengthRaw(),
            breakdowns[i].getStabilityRaw(),
            breakdowns[i].getCenterSqRaw(),
            breakdowns[i].getSkewSqRaw(),
            breakdowns[i].getDomainRaw(),
            breakdowns[i].getOrderingNorm(),
            breakdowns[i].getLengthNorm(),
            breakdowns[i].getStabilityNorm(),
            breakdowns[i].getCenterSqNorm(),
            breakdowns[i].getSkewSqNorm(),
            breakdowns[i].getOrderingContribution(),
            breakdowns[i].getLengthContribution(),
            breakdowns[i].getStabilityContribution(),
            breakdowns[i].getCenterSqContribution(),
            breakdowns[i].getSkewSqContribution(),
            breakdowns[i].getDomainContribution(),
            breakdowns[i].getTotalScore(),
            breakdowns[i].getRejectionMask(),
            breakdowns[i].getRejectionText(),
            ok,  // passed_gates
            breakdowns[i].violatesSupport(),
            breakdowns[i].getSupportLowerBound(),
            breakdowns[i].getSupportUpperBound());
        }
      }
    };

  } // namespace analysis
} // namespace palvalidator
