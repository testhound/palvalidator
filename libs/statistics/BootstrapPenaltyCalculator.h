#pragma once

#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>
#include "number.h"
#include "StatUtils.h"
#include "AutoBootstrapConfiguration.h"
#include "AutoCIResult.h"
#include "NormalQuantile.h"

namespace palvalidator
{
  namespace analysis
  {
    // Forward declarations
    using mkc_timeseries::StatisticSupport;
    
    /**
     * @brief Bootstrap penalty calculation engine.
     *
     * This class encapsulates all penalty and stability computation logic for
     * bootstrap confidence interval methods. It provides static methods for
     * computing various penalty components used in the AutoBootstrapSelector
     * scoring framework.
     *
     * The penalty calculator supports:
     * - Length penalties for different bootstrap methods
     * - Stability penalties (BCa, Percentile-T)
     * - Skew penalty for distribution shape matching
     * - Domain penalty for support constraint violations
     * - Empirical under-coverage penalties
     * - BCa length overflow penalty
     */
    template <class Decimal>
    class BootstrapPenaltyCalculator
    {
    public:
      using MethodId = typename AutoCIResult<Decimal>::MethodId;
      using Candidate = typename AutoCIResult<Decimal>::Candidate;
      

      /**
       * @brief Result structure for empirical mass calculations.
       */
      struct EmpiricalMassResult
      {
        double mass_inclusive = 0.0;
        std::size_t effective_sample_count = 0;
      };

      // =========================================================================
      // CORE PENALTY COMPUTATION METHODS
      // =========================================================================

      /**
       * @brief Computes skew penalty based on bootstrap distribution skewness.
       *
       * Applies quadratic penalty when skewness exceeds threshold (1.0).
       * This penalty measures how well the bootstrap distribution matches
       * the expected shape characteristics.
       *
       * @param skew Bootstrap distribution skewness
       * @return Penalty >= 0 (0 if |skew| <= 1.0)
       */
      static double computeSkewPenalty(double skew)
      {
        const double skew_abs = std::fabs(skew);
        constexpr double kSkewThreshold = 1.0;
        const double skew_excess = std::max(0.0, skew_abs - kSkewThreshold);
        return skew_excess * skew_excess;
      }

      /**
       * @brief Computes domain penalty for support constraint violations.
       *
       * Returns a fixed penalty when the confidence interval violates
       * the statistic's natural domain (e.g., negative values for ratios).
       *
       * @param candidate Candidate with interval bounds
       * @param support Domain constraints for the statistic
       * @return Fixed penalty if violation, 0.0 otherwise
       */
      static double computeDomainPenalty(const Candidate& candidate,
                                        const StatisticSupport& support)
      {
        const double lower = num::to_double(candidate.getLower());
        if (support.violatesLowerBound(lower))
        {
          return AutoBootstrapConfiguration::kDomainViolationPenalty;
        }
        return 0.0;
      }

      /**
       * @brief Simple default weights for backward compatibility.
       */
      struct DefaultScoringWeights {
        double getBcaZ0Scale() const { return 20.0; }
        double getBcaAScale() const { return 100.0; }
      };

      /**
       * @brief Computes stability penalty for BCa bootstrap intervals.
       *
       * The BCa method uses bias correction (z0) and acceleration (a) parameters.
       * This function penalizes intervals where these parameters indicate instability
       * or where the bootstrap distribution shows extreme skewness.
       *
       * Three types of instability are penalized:
       * 1. Excessive Bias (z0): When |z0| exceeds threshold (0.3)
       * 2. Excessive Acceleration (a): When |a| exceeds threshold (0.1, or 0.08 for extreme skew)
       * 3. Extreme Skewness: When bootstrap skewness exceeds threshold (2.0)
       *
       * @param z0 Bias-correction parameter from BCa
       * @param accel Acceleration parameter from BCa
       * @param skew_boot Skewness of the bootstrap distribution
       * @param weights Scoring weights containing penalty scales (defaults to standard values)
       * @param os Optional output stream for debug logging
       * @return Stability penalty >= 0 (infinity for non-finite parameters)
       */
      template <class ScoringWeightsType = DefaultScoringWeights>
      static double computeBCaStabilityPenalty(double z0,
                                               double accel,
                                               double skew_boot,
                                               const ScoringWeightsType& weights = ScoringWeightsType(),
                                               std::ostream* os = nullptr)
      {
        // Non-finite parameters indicate catastrophic failure
        if (!std::isfinite(z0) || !std::isfinite(accel) || !std::isfinite(skew_boot))
        {
          if (os)
          {
            (*os) << "[BCa] Non-finite parameters detected: "
                  << "z0=" << z0 << " accel=" << accel << " skew_boot=" << skew_boot << "\n";
          }
          return std::numeric_limits<double>::infinity();
        }

        double stability_penalty = 0.0;

        // 1. BIAS (z0) PENALTY
        const double Z0_THRESHOLD = AutoBootstrapConfiguration::kBcaZ0SoftThreshold;
        
        // Adaptive scaling: high skewness makes bias harder to correct reliably
        const double skew_multiplier = (std::abs(skew_boot) > 2.0) ? 1.5 : 1.0;
        const double Z0_SCALE = weights.getBcaZ0Scale() * skew_multiplier;

        const double z0_abs = std::abs(z0);
        if (z0_abs > Z0_THRESHOLD)
        {
          const double diff = z0_abs - Z0_THRESHOLD;
          const double z0_penalty = (diff * diff) * Z0_SCALE;
          stability_penalty += z0_penalty;

          if (os && z0_penalty > 0.01)
          {
            (*os) << "[BCa] z0 penalty: |z0|=" << z0_abs
                  << " threshold=" << Z0_THRESHOLD
                  << " penalty=" << z0_penalty << "\n";
          }
        }

        // 2. ACCELERATION (a) PENALTY
        const double base_accel_threshold = AutoBootstrapConfiguration::kBcaASoftThreshold;
        const double strict_accel_threshold_for_extreme_skew = 0.08;

        // Stricter threshold when distribution is highly skewed
        const double ACCEL_THRESHOLD = (std::abs(skew_boot) > 3.0)
          ? strict_accel_threshold_for_extreme_skew
          : base_accel_threshold;

        const double ACCEL_SCALE = weights.getBcaAScale() * skew_multiplier;

        const double accel_abs = std::abs(accel);
        if (accel_abs > ACCEL_THRESHOLD)
        {
          const double diff = accel_abs - ACCEL_THRESHOLD;
          const double accel_penalty = (diff * diff) * ACCEL_SCALE;
          stability_penalty += accel_penalty;

          if (os && accel_penalty > 0.01)
          {
            (*os) << "[BCa] acceleration penalty: |a|=" << accel_abs
                  << " threshold=" << ACCEL_THRESHOLD
                  << " penalty=" << accel_penalty << "\n";
          }
        }

        // 3. SKEWNESS PENALTY
        const double SKEW_THRESHOLD = AutoBootstrapConfiguration::kBcaSkewThreshold;
        const double SKEW_PENALTY_SCALE = AutoBootstrapConfiguration::kBcaSkewPenaltyScale;

        const double skew_abs = std::abs(skew_boot);
        if (skew_abs > SKEW_THRESHOLD)
        {
          const double skew_excess = skew_abs - SKEW_THRESHOLD;
          const double skew_penalty = skew_excess * skew_excess * SKEW_PENALTY_SCALE;
          stability_penalty += skew_penalty;

          if (os && (skew_penalty > 0.1))
          {
            (*os) << "[BCa] Skew penalty applied: skew_boot=" << skew_boot
                  << " threshold=" << SKEW_THRESHOLD
                  << " excess=" << skew_excess
                  << " penalty=" << skew_penalty
                  << " total_stability=" << stability_penalty << "\n";
          }
        }

        // Debug logging
        if (os && (std::abs(skew_boot) > 2.0))
        {
          (*os) << "[BCa DEBUG] High skew detected:\n"
                << "  skew_boot=" << skew_boot << "\n"
                << "  skew_multiplier=" << skew_multiplier << "\n"
                << "  Z0_THRESHOLD=" << Z0_THRESHOLD << "\n"
                << "  ACCEL_THRESHOLD=" << ACCEL_THRESHOLD << "\n"
                << "  Z0_SCALE=" << Z0_SCALE << "\n"
                << "  ACCEL_SCALE=" << ACCEL_SCALE << "\n"
                << "  z0=" << z0 << " (|z0|=" << z0_abs << ")\n"
                << "  accel=" << accel << " (|a|=" << accel_abs << ")\n";
        }

        if (os && (stability_penalty > 0.0))
        {
          (*os) << "[BCa] Total stability penalty: " << stability_penalty << "\n";
        }

        return stability_penalty;
      }

      /**
       * @brief Computes stability penalty for Percentile-T based on resample quality.
       *
       * Penalizes high failure rates and low effective sample sizes that indicate
       * the double-bootstrap procedure is struggling (e.g., small n, heavy tails).
       *
       * @param res Result structure from Percentile-T engine
       * @return Stability penalty >= 0 (infinity for invalid inputs)
       */
      template <class Result>
      static double computePercentileTStability(const Result& res)
      {
        const double B_outer = static_cast<double>(res.B_outer);
        const double B_inner = static_cast<double>(res.B_inner);
        const double skipped_outer = static_cast<double>(res.skipped_outer);
        const double skipped_inner = static_cast<double>(res.skipped_inner_total);
        const double effective_B = static_cast<double>(res.effective_B);
        const double inner_attempted_total = static_cast<double>(res.inner_attempted_total);

        // Guard against non-finite / division by zero
        if (!std::isfinite(B_outer) || !std::isfinite(B_inner) ||
            !std::isfinite(skipped_outer) || !std::isfinite(skipped_inner) ||
            !std::isfinite(effective_B) || !std::isfinite(inner_attempted_total))
        {
          return std::numeric_limits<double>::infinity();
        }

        if (B_outer < 1.0 || B_inner < 1.0 || inner_attempted_total <= 0.0)
        {
          return std::numeric_limits<double>::infinity();
        }

        double penalty = 0.0;

        // 1. OUTER RESAMPLE FAILURE RATE
        double outer_failure_rate = std::clamp(skipped_outer / B_outer, 0.0, 1.0);
        const double kOuterThreshold = AutoBootstrapConfiguration::kPercentileTOuterFailThreshold;

        if (outer_failure_rate > kOuterThreshold)
        {
          const double excess = outer_failure_rate - kOuterThreshold;
          penalty += excess * excess * AutoBootstrapConfiguration::kPercentileTOuterPenaltyScale;
        }

        // 2. INNER SE FAILURE RATE  
        double inner_failure_rate = std::clamp(skipped_inner / inner_attempted_total, 0.0, 1.0);
        const double kInnerThreshold = AutoBootstrapConfiguration::kPercentileTInnerFailThreshold;

        if (inner_failure_rate > kInnerThreshold)
        {
          const double excess = inner_failure_rate - kInnerThreshold;
          penalty += excess * excess * AutoBootstrapConfiguration::kPercentileTInnerPenaltyScale;
        }

        // 3. EFFECTIVE SAMPLE SIZE
        const double kMinEffectiveFraction = AutoBootstrapConfiguration::kPercentileTMinEffectiveFraction;
        const double min_effective = kMinEffectiveFraction * B_outer;

        if (effective_B < min_effective)
        {
          const double deficit_fraction = (min_effective - effective_B) / B_outer;
          penalty += deficit_fraction * deficit_fraction *
            AutoBootstrapConfiguration::kPercentileTEffectiveBPenaltyScale;
        }

        return penalty;
      }

      // =========================================================================
      // LENGTH PENALTY METHODS
      // =========================================================================

      /**
       * @brief Computes length penalty for percentile-based bootstrap methods.
       *
       * Compares actual interval length to ideal length derived from bootstrap quantiles.
       * Applies quadratic penalty when length falls outside acceptable bounds.
       *
       * @param actual_length Width of the actual interval (upper - lower)
       * @param boot_stats Vector of bootstrap statistics (unsorted)
       * @param confidence_level Nominal confidence level (e.g., 0.95)
       * @param method Bootstrap method (affects L_max selection)
       * @param normalized_length [out] Ratio of actual to ideal length
       * @param median_val [out] Median of bootstrap distribution
       * @return Length penalty (0.0 if within bounds, >0.0 if outside)
       */
      static double computeLengthPenalty_Percentile(double actual_length,
                                                    const std::vector<double>& boot_stats,
                                                    double confidence_level,
                                                    MethodId method,
                                                    double& normalized_length,
                                                    double& median_val)
      {
        // Initialize outputs
        normalized_length = 1.0;
        median_val = 0.0;
    
        if (actual_length <= 0.0 || boot_stats.size() < 2) {
          return 0.0;
        }
    
        // Sort bootstrap statistics
        std::vector<double> sorted(boot_stats.begin(), boot_stats.end());
        std::sort(sorted.begin(), sorted.end());
    
        // Compute median
        median_val = mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);
    
        // Compute ideal interval length from bootstrap quantiles
        const double alpha = 1.0 - confidence_level;
        const double alphaL = 0.5 * alpha;
        const double alphaU = 1.0 - 0.5 * alpha;
    
        const double qL = mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, alphaL);
        const double qU = mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, alphaU);
        const double ideal_len_boot = qU - qL;
    
        if (ideal_len_boot <= 0.0) {
          return 0.0; // Bootstrap distribution is degenerate
        }
    
        // Normalize actual length to ideal length
        normalized_length = actual_length / ideal_len_boot;
    
        // Select appropriate L_max based on method
        const double L_min = AutoBootstrapConfiguration::kLengthMin;
        const double L_max = (method == MethodId::MOutOfN)
          ? AutoBootstrapConfiguration::kLengthMaxMOutOfN
          : AutoBootstrapConfiguration::kLengthMaxStandard;
    
        // Quadratic penalty outside acceptable bounds
        if (normalized_length < L_min) {
          const double deficit = L_min - normalized_length;
          return deficit * deficit;
        }
        else if (normalized_length > L_max) {
          const double excess = normalized_length - L_max;
          return excess * excess;
        }
        
        return 0.0;
      }

      /**
       * @brief Computes length penalty for Normal approximation method.
       *
       * Uses theoretical normal distribution width (2 * z_{α/2} * SE) as ideal reference.
       *
       * @param actual_length Width of the actual interval
       * @param se_boot Bootstrap standard error
       * @param confidence_level Nominal confidence level
       * @param normalized_length [out] Ratio of actual to ideal length
       * @param median_val_placeholder [out] Not meaningful for Normal; set to 0
       * @return Length penalty (0.0 if within bounds, >0.0 if outside)
       */
      static double computeLengthPenalty_Normal(double actual_length,
                                               double se_boot,
                                               double confidence_level,
                                               double& normalized_length,
                                               double& median_val_placeholder)
      {
        median_val_placeholder = 0.0; // Normal doesn't use bootstrap median
        
        if (actual_length <= 0.0 || se_boot <= 0.0) {
          normalized_length = 1.0;
          return 0.0;
        }
        
        // Normal's theoretical ideal: θ̂ ± z_{α/2} * SE
        const double alpha = 1.0 - confidence_level;
        const double z_alpha_2 = palvalidator::analysis::detail::compute_normal_quantile(1.0 - 0.5 * alpha);
        const double ideal_len = 2.0 * z_alpha_2 * se_boot;
        
        if (ideal_len <= 0.0) {
          normalized_length = 1.0;
          return 0.0;
        }
        
        normalized_length = actual_length / ideal_len;
        
        // Standard bounds for Normal
        const double L_min = AutoBootstrapConfiguration::kLengthMin;
        const double L_max = AutoBootstrapConfiguration::kLengthMaxStandard;
        
        if (normalized_length < L_min) {
          const double deficit = L_min - normalized_length;
          return deficit * deficit;
        }
        else if (normalized_length > L_max) {
          const double excess = normalized_length - L_max;
          return excess * excess;
        }
        
        return 0.0;
      }

      /**
       * @brief Computes length penalty for Percentile-T (studentized) bootstrap.
       *
       * Uses the T* distribution quantiles and SE for constructing ideal interval width.
       * Percentile-T constructs intervals as: θ̂ - t_hi*SE to θ̂ - t_lo*SE
       *
       * @param actual_length Width of the actual interval
       * @param t_star_stats Vector of T* statistics from double bootstrap
       * @param se_hat Standard error used in interval construction
       * @param confidence_level Nominal confidence level
       * @param normalized_length [out] Ratio of actual to ideal length
       * @param median_val [out] Median of T* distribution
       * @return Length penalty (0.0 if within bounds, >0.0 if outside)
       */
      static double computeLengthPenalty_PercentileT(double actual_length,
                                                     const std::vector<double>& t_star_stats,
                                                     double se_hat,
                                                     double confidence_level,
                                                     double& normalized_length,
                                                     double& median_val)
      {
        if (actual_length <= 0.0 || t_star_stats.size() < 2 || se_hat <= 0.0) {
          normalized_length = 1.0;
          median_val = 0.0;
          return 0.0;
        }
        
        // Compute median of T* distribution
        std::vector<double> sorted(t_star_stats.begin(), t_star_stats.end());
        std::sort(sorted.begin(), sorted.end());
        median_val = mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);
        
        // Percentile-T's theoretical ideal width: (t_hi - t_lo) * SE_hat
        const double alpha = 1.0 - confidence_level;
        const double t_lo = mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, 0.5 * alpha);
        const double t_hi = mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, 1.0 - 0.5 * alpha);
        
        const double ideal_len = (t_hi - t_lo) * se_hat;
        
        if (ideal_len <= 0.0) return 0.0;
        
        normalized_length = actual_length / ideal_len;
        
        // Standard bounds for Percentile-T
        const double L_min = AutoBootstrapConfiguration::kLengthMin;
        const double L_max = AutoBootstrapConfiguration::kLengthMaxStandard;
        
        if (normalized_length < L_min) {
          const double deficit = L_min - normalized_length;
          return deficit * deficit;
        }
        else if (normalized_length > L_max) {
          const double excess = normalized_length - L_max;
          return excess * excess;
        }
        
        return 0.0;
      }

      // =========================================================================
      // EMPIRICAL COVERAGE PENALTY METHODS
      // =========================================================================

      /**
       * @brief Computes empirical mass of bootstrap statistics within interval bounds.
       *
       * @param xs Vector of bootstrap statistics
       * @param lo Lower interval bound
       * @param hi Upper interval bound
       * @return Structure containing mass and effective sample count
       */
      static EmpiricalMassResult compute_empirical_mass_inclusive(const std::vector<double>& xs,
                                                                  double lo, double hi)
      {
        std::size_t n = 0, inside = 0;
        
        for (double v : xs)
        {
          if (!std::isfinite(v))
            continue;
          
          ++n;
          if (v >= lo && v <= hi)
            ++inside;
        }
        
        EmpiricalMassResult result;
        result.effective_sample_count = n;
        result.mass_inclusive = (n == 0) ? 0.0 : static_cast<double>(inside) / static_cast<double>(n);
        return result;
      }

      /**
       * @brief Computes under-coverage penalty with finite-sample tolerance.
       *
       * @param width_cdf Empirical coverage fraction
       * @param cl Nominal confidence level
       * @param B_eff Effective bootstrap sample size
       * @return Under-coverage amount (0 if adequate coverage)
       */
      static double compute_under_coverage_with_half_step_tolerance(double width_cdf,
                                                                   double cl,
                                                                   std::size_t B_eff)
      {
        const double step = (B_eff > 0) ? (1.0 / static_cast<double>(B_eff)) : 1.0;
        const double tol = 0.5 * step;
        return std::max(0.0, (cl - width_cdf) - tol);
      }

      /**
       * @brief Computes empirical under-coverage penalty for general bootstrap methods.
       *
       * Measures how well the interval captures the bootstrap distribution. Only
       * penalizes under-coverage (actual < nominal), not over-coverage.
       *
       * @param boot_stats Bootstrap statistics distribution
       * @param lo Lower interval bound
       * @param hi Upper interval bound  
       * @param cl Nominal confidence level
       * @return Under-coverage penalty >= 0
       */
      static double computeEmpiricalUnderCoveragePenalty(const std::vector<double>& boot_stats,
                                                         double lo,
                                                         double hi,
                                                         double cl)
      {
        if (boot_stats.size() < 2) return 0.0;
        if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo)) return 0.0;
        if (!(cl > 0.0 && cl < 1.0)) return 0.0;

        const EmpiricalMassResult mass_result = compute_empirical_mass_inclusive(boot_stats, lo, hi);

        const std::size_t B_eff = mass_result.effective_sample_count;
        if (B_eff < 2) return 0.0;

        const double width_cdf = std::clamp(mass_result.mass_inclusive, 0.0, 1.0);

        const double under_coverage =
          compute_under_coverage_with_half_step_tolerance(width_cdf, cl, B_eff);

        return AutoBootstrapConfiguration::kUnderCoverageMultiplier *
          under_coverage * under_coverage;
      }

      /**
       * @brief Computes empirical under-coverage penalty for Percentile-T method.
       *
       * Special version for Percentile-T that checks coverage in T-space (studentized
       * statistics) rather than θ-space, since that's what the method actually uses.
       *
       * @param t_stats Bootstrap T-statistics (NOT raw θ*)
       * @param theta_hat Point estimate θ̂
       * @param se_hat Standard error used for studentization
       * @param lo Lower CI bound (in θ-space)
       * @param hi Upper CI bound (in θ-space)
       * @param cl Nominal confidence level
       * @return Under-coverage penalty >= 0
       */
      static double computeEmpiricalUnderCoveragePenalty_PercentileT(
        const std::vector<double>& t_stats,
        double theta_hat,
        double se_hat,
        double lo,
        double hi,
        double cl)
      {
        // Guard clauses
        if (t_stats.size() < 2) return 0.0;
        if (!std::isfinite(theta_hat)) return 0.0;
        if (!std::isfinite(se_hat) || !(se_hat > 0.0)) return 0.0;
        if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo)) return 0.0;
        if (!(cl > 0.0 && cl < 1.0)) return 0.0;

        // Transform θ-space interval [lo, hi] to t-space interval [t_lo, t_hi]
        // Percentile-T uses: CI = [θ̂ - t_hi * SE, θ̂ - t_lo * SE]
        // So: t_lo = (θ̂ - hi) / SE,  t_hi = (θ̂ - lo) / SE
        const double t_lo = (theta_hat - hi) / se_hat;
        const double t_hi = (theta_hat - lo) / se_hat;

        if (!std::isfinite(t_lo) || !std::isfinite(t_hi) || !(t_lo < t_hi)) return 0.0;

        // Empirical inclusive mass of t_stats inside [t_lo, t_hi]
        const EmpiricalMassResult mass_result = compute_empirical_mass_inclusive(t_stats, t_lo, t_hi);

        const std::size_t B_eff = mass_result.effective_sample_count;
        if (B_eff < 2) return 0.0;

        const double width_cdf = std::clamp(mass_result.mass_inclusive, 0.0, 1.0);

        // Under-coverage only, with half-step tolerance for finite B
        const double under_coverage =
          compute_under_coverage_with_half_step_tolerance(width_cdf, cl, B_eff);

        return AutoBootstrapConfiguration::kUnderCoverageMultiplier *
          under_coverage * under_coverage;
      }

      /**
       * @brief Wrapper for Percentile-T empirical under-coverage penalty (backward compatibility).
       *
       * This is a convenience wrapper that calls computeEmpiricalUnderCoveragePenalty_PercentileT
       * to maintain compatibility with existing test code that uses the 6-parameter signature.
       */
      static double computeEmpiricalUnderCoveragePenalty(const std::vector<double>& t_stats,
                                                         double theta_hat,
                                                         double se_hat,
                                                         double lo,
                                                         double hi,
                                                         double cl)
      {
        return computeEmpiricalUnderCoveragePenalty_PercentileT(t_stats, theta_hat, se_hat, lo, hi, cl);
      }
    };

  } // namespace analysis
} // namespace palvalidator