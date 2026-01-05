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
#include "number.h"
#include "StatUtils.h"
#include "NormalQuantile.h"
#include "NormalDistribution.h"
#include "AutoBootstrapConfiguration.h"

// Include the bootstrap constants

namespace palvalidator
{
  namespace analysis
  {
    // Forward declarations
    using mkc_timeseries::StatisticSupport;
    /**
     * @brief Encapsulates the complete result of the automatic confidence interval selection process.
     *
     * This class acts as the container for the "winner" of the bootstrap tournament, 
     * the full list of "contestants" (Candidates), and detailed diagnostics explaining 
     * the selection decision (SelectionDiagnostics).
     */
    template <class Decimal>
    class AutoCIResult
    {
    public:
      enum class MethodId
        {
          Normal,
          Basic,
          Percentile,
          PercentileT,
          MOutOfN,
          BCa
        };

      /**
       * @brief Represents the performance and scoring metrics for a single bootstrap method.
       *
       * A Candidate stores the calculated confidence interval bounds, the raw bootstrap 
       * statistics (SE, skewness), and the calculated penalty scores (length, stability, ordering)
       * used during the selection tournament.
       */
      class Candidate
      {
      public:
        /**
         * @brief Constructs a Candidate with all calculated metrics and penalties.
         *
         * @param method The identifier of the bootstrap method.
         * @param mean The point estimate of the statistic.
         * @param lower The lower bound of the confidence interval.
         * @param upper The upper bound of the confidence interval.
         * @param cl The confidence level (e.g., 0.95).
         * @param n The original sample size.
         * @param B_outer Number of outer bootstrap resamples.
         * @param B_inner Number of inner bootstrap resamples (for Percentile-T).
         * @param effective_B Count of valid (non-NaN/Inf) resamples.
         * @param skipped_total Total number of invalid resamples skipped.
         * @param se_boot Estimated standard error from the bootstrap distribution.
         * @param skew_boot Estimated skewness of the bootstrap distribution.
         * @param median_boot Median value of the bootstrap distribution.
         * @param center_shift_in_se Deviation of the interval center from the point estimate (normalized by SE).
         * @param normalized_length Ratio of actual interval length to the ideal length derived from quantiles.
         * @param ordering_penalty Score component penalizing coverage errors (based on empirical CDF).
         * @param length_penalty Score component penalizing intervals that are too wide or too narrow.
         * @param stability_penalty Score component penalizing instability (e.g., extreme z0/accel in BCa).
         * @param z0 Bias-correction parameter (BCa only).
         * @param accel Acceleration parameter (BCa only).
         * @param inner_failure_rate Rate of inner loop failures (Percentile-T only).
         * @param score Final computed tournament score (lower is better).
         */
        Candidate(MethodId    method,
                  Decimal     mean,
                  Decimal     lower,
                  Decimal     upper,
                  double      cl,
                  std::size_t n,
                  std::size_t B_outer,
                  std::size_t B_inner,
                  std::size_t effective_B,
                  std::size_t skipped_total,
                  double      se_boot,
                  double      skew_boot,
                  double      median_boot,
                  double      center_shift_in_se,
                  double      normalized_length,
                  double      ordering_penalty,
                  double      length_penalty,
                  double      stability_penalty, 
                  double      z0,
                  double      accel,
		  double      inner_failure_rate,
                  double      score = std::numeric_limits<double>::quiet_NaN())
        : m_method(method),
          m_mean(mean),
          m_lower(lower),
          m_upper(upper),
          m_cl(cl),
          m_n(n),
          m_B_outer(B_outer),
          m_B_inner(B_inner),
          m_effective_B(effective_B),
          m_skipped_total(skipped_total),
          m_se_boot(se_boot),
          m_skew_boot(skew_boot),
          m_median_boot(median_boot),
          m_center_shift_in_se(center_shift_in_se),
          m_normalized_length(normalized_length),
          m_ordering_penalty(ordering_penalty),
          m_length_penalty(length_penalty),
          m_stability_penalty(stability_penalty), 
          m_z0(z0),
          m_accel(accel),
	  m_inner_failure_rate(inner_failure_rate),
          m_score(score)
        {
        }

        // -- Getters --
        MethodId    getMethod() const { return m_method; }
        Decimal     getMean() const { return m_mean; }
        Decimal     getLower() const { return m_lower; }
        Decimal     getUpper() const { return m_upper; }
        double      getCl() const { return m_cl; }

        std::size_t getN() const { return m_n; }
        std::size_t getBOuter() const { return m_B_outer; }
        std::size_t getBInner() const { return m_B_inner; }
        std::size_t getEffectiveB() const { return m_effective_B; }
        std::size_t getSkippedTotal() const { return m_skipped_total; }

        double      getSeBoot() const { return m_se_boot; }
        double      getSkewBoot() const { return m_skew_boot; }
        double      getMedianBoot() const { return m_median_boot; }
        double      getCenterShiftInSe() const { return m_center_shift_in_se; }
        double      getNormalizedLength() const { return m_normalized_length; }
        double      getOrderingPenalty() const { return m_ordering_penalty; }
        double      getLengthPenalty() const { return m_length_penalty; }

        double      getZ0() const { return m_z0; }
        double      getAccel() const { return m_accel; }
        double      getScore() const { return m_score; }
        double      getStabilityPenalty() const { return m_stability_penalty; }
	double getInnerFailureRate() const { return m_inner_failure_rate; }

	/**
         * @brief Returns a copy of this candidate with an updated total score.
         * Used during the final scoring phase to attach the computed weighted score.
         */
        Candidate withScore(double newScore) const
        {
          return Candidate(m_method, m_mean, m_lower, m_upper, m_cl,
                      m_n, m_B_outer, m_B_inner, m_effective_B, m_skipped_total,
                      m_se_boot, m_skew_boot, m_median_boot,
                      m_center_shift_in_se, m_normalized_length,
                      m_ordering_penalty, 
                      m_length_penalty,
                      m_stability_penalty, 
			   m_z0, m_accel, m_inner_failure_rate, newScore);
        }
    public:

    private:
        MethodId    m_method;
        Decimal     m_mean;
        Decimal     m_lower;
        Decimal     m_upper;
        double      m_cl;
        std::size_t m_n;
        std::size_t m_B_outer;
        std::size_t m_B_inner;
        std::size_t m_effective_B;
        std::size_t m_skipped_total;
        double      m_se_boot;
        double      m_skew_boot;
        double      m_median_boot;
        double      m_center_shift_in_se;
        double      m_normalized_length;
        double      m_ordering_penalty;
        double      m_length_penalty;
        double      m_stability_penalty; 
        double      m_z0;
        double      m_accel;
	double      m_inner_failure_rate;
        double      m_score;
      };

      /**
       * @brief Provides detailed diagnostic information about the selection process.
       *
       * Stores reasoning for why the winning method was chosen and why others (specifically BCa)
       * might have been rejected (e.g., due to instability or invalid parameters).
       */
      class SelectionDiagnostics
      {
      public:
	/**
         * @brief Breakdown of the specific penalty components that contributed to a method's total score.
         * Useful for debugging why a specific method lost the tournament.
         */
        class ScoreBreakdown
        {
        public:
	  /**
	   * @brief Constructs a breakdown of raw and normalized penalty components.
	   * * @param method The method being scored.
	   * @param orderingRaw Raw ordering/coverage penalty.
	   * @param lengthRaw Raw length efficiency penalty.
	   * @param stabilityRaw Raw stability penalty.
	   * @param centerSqRaw Squared center shift raw value.
	   * @param skewSqRaw Squared skewness fidelity raw value.
	   * @param domainRaw Penalty for violating domain constraints (e.g., negative values).
	   * @param orderingNorm Normalized ordering penalty (0-1 scale relative to reference).
	   * @param lengthNorm Normalized length penalty.
	   * @param stabilityNorm Normalized stability penalty.
	   * @param centerSqNorm Normalized center shift penalty.
	   * @param skewSqNorm Normalized skewness penalty.
	   * @param orderingContrib Weighted contribution of ordering to total score.
	   * @param lengthContrib Weighted contribution of length to total score.
	   * @param stabilityContrib Weighted contribution of stability to total score.
	   * @param centerSqContrib Weighted contribution of center shift to total score.
	   * @param skewSqContrib Weighted contribution of skewness to total score.
	   * @param domainContrib Weighted contribution of domain violations.
	   * @param totalScore The final summed score.
	   */
          ScoreBreakdown(MethodId method,
                         double orderingRaw,
                         double lengthRaw,
                         double stabilityRaw,
                         double centerSqRaw,
                         double skewSqRaw,
                         double domainRaw,
                         double orderingNorm,
                         double lengthNorm,
                         double stabilityNorm,
                         double centerSqNorm,
                         double skewSqNorm,
                         double orderingContrib,
                         double lengthContrib,
                         double stabilityContrib,
                         double centerSqContrib,
                         double skewSqContrib,
                         double domainContrib,
                         double totalScore)
            : m_method(method),
              m_ordering_raw(orderingRaw),
              m_length_raw(lengthRaw),
              m_stability_raw(stabilityRaw),
              m_center_sq_raw(centerSqRaw),
              m_skew_sq_raw(skewSqRaw),
              m_domain_raw(domainRaw),
              m_ordering_norm(orderingNorm),
              m_length_norm(lengthNorm),
              m_stability_norm(stabilityNorm),
              m_center_sq_norm(centerSqNorm),
              m_skew_sq_norm(skewSqNorm),
              m_ordering_contrib(orderingContrib),
              m_length_contrib(lengthContrib),
              m_stability_contrib(stabilityContrib),
              m_center_sq_contrib(centerSqContrib),
              m_skew_sq_contrib(skewSqContrib),
              m_domain_contrib(domainContrib),
              m_total_score(totalScore)
          {}

          MethodId getMethod() const { return m_method; }
          double getOrderingRaw() const { return m_ordering_raw; }
          double getLengthRaw() const { return m_length_raw; }
          double getStabilityRaw() const { return m_stability_raw; }
          double getCenterSqRaw() const { return m_center_sq_raw; }
          double getSkewSqRaw() const { return m_skew_sq_raw; }
          double getDomainRaw() const { return m_domain_raw; }
          double getOrderingNorm() const { return m_ordering_norm; }
          double getLengthNorm() const { return m_length_norm; }
          double getStabilityNorm() const { return m_stability_norm; }
          double getCenterSqNorm() const { return m_center_sq_norm; }
          double getSkewSqNorm() const { return m_skew_sq_norm; }
          double getOrderingContribution() const { return m_ordering_contrib; }
          double getLengthContribution() const { return m_length_contrib; }
          double getStabilityContribution() const { return m_stability_contrib; }
          double getCenterSqContribution() const { return m_center_sq_contrib; }
          double getSkewSqContribution() const { return m_skew_sq_contrib; }
          double getDomainContribution() const { return m_domain_contrib; }
          double getTotalScore() const { return m_total_score; }

        private:
          MethodId m_method;
          double m_ordering_raw;
          double m_length_raw;
          double m_stability_raw;
          double m_center_sq_raw;
          double m_skew_sq_raw;
          double m_domain_raw;
          double m_ordering_norm;
          double m_length_norm;
          double m_stability_norm;
          double m_center_sq_norm;
          double m_skew_sq_norm;
          double m_ordering_contrib;
          double m_length_contrib;
          double m_stability_contrib;
          double m_center_sq_contrib;
          double m_skew_sq_contrib;
          double m_domain_contrib;
          double m_total_score;
        };

        /**
         * @brief Constructs comprehensive selection diagnostics.
         *
         * @param chosenMethod The selected bootstrap method
         * @param chosenMethodName Human-readable name of the chosen method
         * @param chosenScore The final tournament score of the winner
         * @param chosenStabilityPenalty Stability penalty component of winner's score
         * @param chosenLengthPenalty Length penalty component of winner's score
         * @param hasBCaCandidate True if BCa was a candidate in the tournament
         * @param bcaChosen True if BCa was ultimately selected
         * @param bcaRejectedForInstability True if BCa was rejected due to |z0| or |accel| limits
         * @param bcaRejectedForLength True if BCa was rejected due to excessive interval length
         * @param bcaRejectedForDomain [Optional] True if BCa violated domain constraints
         *                             (default: false)
         * @param bcaRejectedForNonFinite [Optional] True if BCa had non-finite parameters
         *                                (default: false)
         * @param numCandidates [Optional] Total number of candidates evaluated (default: 0)
         * @param scoreBreakdowns [Optional] Detailed score decomposition for all candidates
         *                        (default: empty vector)
         *
         * @note The last 4 parameters have defaults and may be omitted for simpler use cases.
         */
        SelectionDiagnostics(MethodId     chosenMethod,
                             std::string  chosenMethodName,
                             double       chosenScore,
                             double       chosenStabilityPenalty,
                             double       chosenLengthPenalty,
                             bool         hasBCaCandidate,
                             bool         bcaChosen,
                             bool         bcaRejectedForInstability,
                             bool         bcaRejectedForLength,
                             bool         bcaRejectedForDomain      = false,
                             bool         bcaRejectedForNonFinite   = false,
                             std::size_t  numCandidates             = 0,
                             std::vector<ScoreBreakdown> scoreBreakdowns = std::vector<ScoreBreakdown>())
          : m_chosen_method(chosenMethod),
            m_chosen_method_name(std::move(chosenMethodName)),
            m_chosen_score(chosenScore),
            m_chosen_stability_penalty(chosenStabilityPenalty),
            m_chosen_length_penalty(chosenLengthPenalty),
            m_has_bca_candidate(hasBCaCandidate),
            m_bca_chosen(bcaChosen),
            m_bca_rejected_for_instability(bcaRejectedForInstability),
            m_bca_rejected_for_length(bcaRejectedForLength),
            m_bca_rejected_for_domain(bcaRejectedForDomain),
            m_bca_rejected_for_non_finite(bcaRejectedForNonFinite),
            m_num_candidates(numCandidates),
            m_score_breakdowns(std::move(scoreBreakdowns))
        {}

	/**
         * @brief Returns the ID of the winning method.
         */
        MethodId getChosenMethod() const
	{
	  return m_chosen_method;
	}

	/**
         * @brief Returns a human-readable string name of the winning method.
         */
        const std::string& getChosenMethodName() const
	{
	  return m_chosen_method_name;
	}
	
        double getChosenScore() const { return m_chosen_score; }
        double getChosenStabilityPenalty() const { return m_chosen_stability_penalty; }
        double getChosenLengthPenalty() const { return m_chosen_length_penalty; }

	/**
         * @brief Returns true if a BCa candidate was generated during the process.
         */
        bool hasBCaCandidate() const { return m_has_bca_candidate; }
        bool isBCaChosen() const { return m_bca_chosen; }

	/**
         * @brief Returns true if BCa was rejected specifically due to numerical instability checks.
         * Checks include |z0| > 0.6, |accel| > 0.25, or non-finite parameters.
         */
        bool wasBCaRejectedForInstability() const
	{
	  return m_bca_rejected_for_instability;
	}

        bool wasBCaRejectedForLength() const { return m_bca_rejected_for_length; }
        bool wasBCaRejectedForDomain() const { return m_bca_rejected_for_domain; }
        bool wasBCaRejectedForNonFiniteParameters() const { return m_bca_rejected_for_non_finite; }
        std::size_t getNumCandidates() const { return m_num_candidates; }

        bool hasScoreBreakdowns() const { return !m_score_breakdowns.empty(); }
        const std::vector<ScoreBreakdown>& getScoreBreakdowns() const { return m_score_breakdowns; }

      private:
        MethodId    m_chosen_method;
        std::string m_chosen_method_name;
        double      m_chosen_score;
        double      m_chosen_stability_penalty;
        double      m_chosen_length_penalty;
        bool        m_has_bca_candidate;
        bool        m_bca_chosen;
        bool        m_bca_rejected_for_instability;
        bool        m_bca_rejected_for_length;
        bool        m_bca_rejected_for_domain;
        bool        m_bca_rejected_for_non_finite;
        std::size_t m_num_candidates;

        std::vector<ScoreBreakdown> m_score_breakdowns;
      };
    
      AutoCIResult(MethodId            chosenMethod,
                   Candidate           chosen,
                   std::vector<Candidate> candidates,
                   SelectionDiagnostics   diagnostics)
        : m_chosen_method(chosenMethod),
          m_chosen(std::move(chosen)),
          m_candidates(std::move(candidates)),
          m_diagnostics(std::move(diagnostics))
      {
      }

      /**
       * @brief Returns the method ID of the winner.
       */
      MethodId                        getChosenMethod() const
      {
	return m_chosen_method;
      }

      /**
       * @brief Returns the full Candidate object of the winner.
       */
      const Candidate&                getChosenCandidate() const
      {
	return m_chosen;
      }

      /**
       * @brief Returns the median of the bootstrap distribution from the winning method.
       */
      double                          getBootstrapMedian() const
      {
	return m_chosen.getMedianBoot();
      }

      /**
       * @brief Returns the vector of all candidates considered in the tournament.
       */
      const std::vector<Candidate>&   getCandidates() const
      {
	return m_candidates;
      }

      const SelectionDiagnostics&     getDiagnostics() const
      {
	return m_diagnostics;
      }

      /**
       * @brief Converts a MethodId enum to a string representation.
       */
      static const char* methodIdToString(MethodId m)
      {
        switch (m)
          {
          case MethodId::Normal:      return "Normal";
          case MethodId::Basic:       return "Basic";
          case MethodId::Percentile:  return "Percentile";
          case MethodId::PercentileT: return "PercentileT";
          case MethodId::MOutOfN:     return "MOutOfN";
          case MethodId::BCa:         return "BCa";
          }
        return "Unknown";
      }

    private:
      MethodId               m_chosen_method;
      Candidate              m_chosen;
      std::vector<Candidate> m_candidates;
      SelectionDiagnostics   m_diagnostics;
    };

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

        double getCenterShiftWeight() const { return m_w_center_shift; }
        double getSkewWeight() const { return m_w_skew; }
        double getLengthWeight() const { return m_w_length; }
        double getStabilityWeight() const { return m_w_stability; }
        bool enforcePositive() const { return m_enforce_positive; }

        double getBcaZ0Scale() const { return m_bca_z0_scale; }
        double getBcaAScale() const { return m_bca_a_scale; }

      private:
        double m_w_center_shift;
        double m_w_skew;
        double m_w_length;
        double m_w_stability;
        bool m_enforce_positive;
        double m_bca_z0_scale;
        double m_bca_a_scale;
      };

      struct EmpiricalMassResult
      {
	double mass_inclusive = 0.0;
	std::size_t effective_sample_count = 0;
      };

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

      static double compute_under_coverage_with_half_step_tolerance(double width_cdf,
								    double cl,
								    std::size_t B_eff)
      {
	const double step = (B_eff > 0) ? (1.0 / static_cast<double>(B_eff)) : 1.0;
	const double tol  = 0.5 * step;
	return std::max(0.0, (cl - width_cdf) - tol);
      }

      // -----------------------------------------------------------------------------
      // Empirical under-coverage penalty for Percentile-T Bootstrap (soft gate)
      //
      // CRITICAL DESIGN NOTE:
      //   Percentile-T intervals are constructed using quantiles of the STUDENTIZED
      //   t-statistics: t_b = (θ*_b - θ̂) / SE*_b
      //   
      //   Therefore, we must check coverage in T-SPACE (the distribution of t-statistics),
      //   not in the raw θ* space. Otherwise we're measuring something the method doesn't
      //   actually use.
      //
      // Idea:
      //   1. Transform the interval [lo, hi] from θ-space to t-space
      //   2. Compute the empirical CDF mass of bootstrap t-statistics inside this range
      //   3. If that "bootstrap-world coverage" is BELOW the nominal cl, penalize
      //   4. If it is ABOVE cl, do NOT penalize (we only punish under-coverage)
      //
      // This is intentionally lightweight: it removes Percentile-T's "free pass" on 
      // ordering without trying to fully re-test frequentist coverage (which would 
      // require double-bootstrap or bootstrap calibration).
      //
      // Math refresher for the interval construction:
      //   Given: θ̂ (point estimate), se_hat (SD of θ* over outer reps)
      //   Quantiles: t_lo = quantile(t_stats, α/2), t_hi = quantile(t_stats, 1-α/2)
      //   Interval:  [θ̂ - t_hi * se_hat,  θ̂ - t_lo * se_hat]
      //              └─────── lo ────────┘  └─────── hi ────────┘
      //
      // To invert this for coverage checking:
      //   lo = θ̂ - t_hi * se_hat  =>  t_hi = (θ̂ - lo) / se_hat
      //   hi = θ̂ - t_lo * se_hat  =>  t_lo = (θ̂ - hi) / se_hat
      //
      // Note the sign flip: the UPPER bound in θ-space (hi) corresponds to the 
      // LOWER quantile in t-space (t_lo), and vice versa.
      // -----------------------------------------------------------------------------

      static double computeEmpiricalUnderCoveragePenalty_PercentileT(
								     const std::vector<double>& t_stats,   // Bootstrap t-statistics (NOT raw θ*)
								     double theta_hat,                      // Point estimate θ̂
								     double se_hat,                         // Standard error used for t: SD(θ*) (or your chosen SE)
								     double lo,                             // Lower CI bound (in θ-space)
								     double hi,                             // Upper CI bound (in θ-space)
								     double cl)                             // Nominal confidence level
      {
	// -------------------------------------------------------------------------
	// Guard clauses
	// -------------------------------------------------------------------------
	if (t_stats.size() < 2) return 0.0;
	if (!std::isfinite(theta_hat)) return 0.0;
	if (!std::isfinite(se_hat) || !(se_hat > 0.0)) return 0.0;
	if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo)) return 0.0;
	if (!(cl > 0.0 && cl < 1.0)) return 0.0;

	// -------------------------------------------------------------------------
	// Transform θ-space interval [lo, hi] to t-space interval [t_lo, t_hi]
	//
	// Percentile-t typically uses: CI = [ θ̂ - t_hi * SE , θ̂ - t_lo * SE ]
	// which implies:
	//   t at upper θ bound (hi) corresponds to LOWER t-quantile
	//   t at lower θ bound (lo) corresponds to UPPER t-quantile
	//
	// Solve:
	//   hi = θ̂ - t_lo * SE  => t_lo = (θ̂ - hi) / SE
	//   lo = θ̂ - t_hi * SE  => t_hi = (θ̂ - lo) / SE
	// -------------------------------------------------------------------------
	const double t_lo = (theta_hat - hi) / se_hat;
	const double t_hi = (theta_hat - lo) / se_hat;

	if (!std::isfinite(t_lo) || !std::isfinite(t_hi)) return 0.0;
	if (!(t_lo < t_hi)) return 0.0;

	// -------------------------------------------------------------------------
	// Empirical inclusive mass of t_stats inside [t_lo, t_hi]
	// -------------------------------------------------------------------------
	const EmpiricalMassResult mass_result = compute_empirical_mass_inclusive(t_stats, t_lo, t_hi);

	const std::size_t B_eff = mass_result.effective_sample_count;
	if (B_eff < 2) return 0.0;

	const double width_cdf = std::clamp(mass_result.mass_inclusive, 0.0, 1.0);

	// -------------------------------------------------------------------------
	// Under-coverage only, with half-step tolerance for finite B
	// -------------------------------------------------------------------------
	const double under_coverage =
	  compute_under_coverage_with_half_step_tolerance(width_cdf, cl, B_eff);

	return AutoBootstrapConfiguration::kUnderCoverageMultiplier *
	  under_coverage * under_coverage;
      }

      static double computeEmpiricalUnderCoveragePenalty(const std::vector<double>& t_stats,
							 double theta_hat,
							 double se_hat,
							 double lo,
							 double hi,
							 double cl)
      {
	return computeEmpiricalUnderCoveragePenalty_PercentileT(t_stats, theta_hat, se_hat, lo, hi, cl);
      }

      // -----------------------------------------------------------------------------
      // Empirical under-coverage penalty (soft gate)
      //
      // Idea:
      //   Compute the empirical CDF mass of bootstrap theta* that lies inside [lo, hi].
      //   If that "bootstrap-world coverage" is BELOW the nominal cl, penalize.
      //   If it is ABOVE cl, do NOT penalize (we only punish under-coverage).
      //
      // This is intentionally lightweight: it removes BCa/PT's "free pass" on ordering
      // without trying to fully re-test frequentist coverage (which would require
      // double-bootstrap or a bootstrap calibration).
      // -----------------------------------------------------------------------------
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
       * @brief Computes length penalty for a bootstrap interval.
       * 
       * @param actual_length    Width of the actual interval (upper - lower)
       * @param boot_stats       Vector of bootstrap statistics (unsorted)
       * @param confidence_level Nominal confidence level (e.g., 0.95)
       * @param method           Bootstrap method (affects L_max selection)
       * @param[out] normalized_length Ratio of actual to ideal length
       * @param[out] median_val  Median of bootstrap distribution
       * 
       * @return Length penalty (0.0 if within bounds, >0.0 if outside)
       */
      static double computeLengthPenalty_Percentile(double actual_length,
						    const std::vector<double>& boot_stats,
						    double confidence_level,
						    MethodId method,
						    double& normalized_length,  // output
						    double& median_val)          // output
      {
	// Initialize outputs
	normalized_length = 1.0;
	median_val = 0.0;
  
	if (actual_length <= 0.0 || boot_stats.size() < 2) {
	  return 0.0;  // Cannot compute penalty for degenerate interval
	}
  
	// Sort bootstrap statistics (local copy to avoid mutating input)
	std::vector<double> sorted(boot_stats.begin(), boot_stats.end());
	std::sort(sorted.begin(), sorted.end());
  
	// Compute median from sorted vector
	median_val = mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);
  
	// Compute ideal interval length from bootstrap quantiles
	const double alpha  = 1.0 - confidence_level;
	const double alphaL = 0.5 * alpha;
	const double alphaU = 1.0 - 0.5 * alpha;
  
	const double qL = mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, alphaL);
	const double qU = mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, alphaU);
	const double ideal_len_boot = qU - qL;
  
	if (ideal_len_boot <= 0.0) {
	  // Bootstrap distribution is degenerate (all values identical)
	  return 0.0;
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
	else {
	  return 0.0;  // Within acceptable range
	}
      }

      static double computeLengthPenalty_Normal(
						double actual_length,
						double se_boot,
						double confidence_level,
						double& normalized_length,
						double& median_val_placeholder)  // Not meaningful; set to 0
      {
	median_val_placeholder = 0.0;  // Normal doesn't use bootstrap median
    
	if (actual_length <= 0.0 || se_boot <= 0.0) {
	  normalized_length = 1.0;
	  return 0.0;
	}
    
	// Normal's theoretical ideal: θ̂ ± z_{α/2} * SE
	// Width = 2 * z_{α/2} * SE
	const double alpha = 1.0 - confidence_level;
	const double z_alpha_2 = detail::compute_normal_quantile(1.0 - 0.5 * alpha);;
	const double ideal_len = 2.0 * z_alpha_2 * se_boot;
    
	if (ideal_len <= 0.0)
	  {
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

      static double computeLengthPenalty_PercentileT(
						     double actual_length,
						     const std::vector<double>& t_star_stats,  // T* distribution
						     double se_hat,                             // SE used in interval construction
						     double confidence_level,
						     double& normalized_length,
						     double& median_val)
      {
	if (actual_length <= 0.0 || t_star_stats.size() < 2 || se_hat <= 0.0) {
	  normalized_length = 1.0;
	  median_val = 0.0;
	  return 0.0;
	}
    
	// Compute median of T* distribution (for diagnostics)
	std::vector<double> sorted(t_star_stats.begin(), t_star_stats.end());
	std::sort(sorted.begin(), sorted.end());
	median_val =  mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);
    
	// Percentile-T's theoretical ideal:
	// CI = θ̂ - t_hi*SE_hat to θ̂ - t_lo*SE_hat
	// Width = (t_hi - t_lo) * SE_hat
	const double alpha = 1.0 - confidence_level;
	const double t_lo =  mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, 0.5 * alpha);
	const double t_hi =  mkc_timeseries::StatUtils<double>::quantileType7Sorted(sorted, 1.0 - 0.5 * alpha);
    
	const double ideal_len = (t_hi - t_lo) * se_hat;  // ← PT's actual construction formula
    
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
      static Candidate summarizePercentileLike(
					       MethodId                                method,
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

	// ====================================================================
	// ORDERING PENALTY (coverage accuracy)
	//
	// NOTE: Basic bootstrap uses reflection (2*theta_hat - quantiles) to
	// construct its interval, which is DESIGNED to deviate from the bootstrap
	// distribution when bias/skewness exists. Penalizing this deviation is
	// methodologically incorrect, similar to BCa and Percentile-T.
	// Therefore, we skip the ordering penalty for Basic bootstrap.
	// ====================================================================
	double ordering_penalty = 0.0;

	if (method != MethodId::Basic)
	  {
	    using palvalidator::analysis::detail::compute_empirical_cdf;

	    const double coverage_target = res.cl;

	    // Empirical CDF values at the interval endpoints (still used for center_pen)
	    const double F_lo = compute_empirical_cdf(stats, lo);
	    const double F_hi = compute_empirical_cdf(stats, hi);

	    // Inclusive mass inside [lo, hi] (tie-safe), plus effective sample size
	    const EmpiricalMassResult mass_result = compute_empirical_mass_inclusive(stats, lo, hi);
	    const std::size_t B_eff = mass_result.effective_sample_count;

	    if (B_eff >= 2)
	      {
		const double width_cdf = std::clamp(mass_result.mass_inclusive, 0.0, 1.0);

		// Under-coverage with half-step tolerance (finite-B granularity)
		const double under_coverage =
		  compute_under_coverage_with_half_step_tolerance(width_cdf, coverage_target, B_eff);

		// Symmetric tolerance for over-coverage as well
		const double step = (B_eff > 0) ? (1.0 / static_cast<double>(B_eff)) : 1.0;
		const double tol  = 0.5 * step;
		const double over_coverage = std::max(0.0, (width_cdf - coverage_target) - tol);

		const double cov_pen =
		  AutoBootstrapConfiguration::kUnderCoverageMultiplier * under_coverage * under_coverage +
		  AutoBootstrapConfiguration::kOverCoverageMultiplier  * over_coverage  * over_coverage;

		const double F_mu       = compute_empirical_cdf(stats, mu);
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
	    length_penalty = computeLengthPenalty_Normal(len, se_boot, res.cl, normalized_length, median_val);
	  }
	else
	  {
	    // Use percentile quantile ideal for Percentile, BCa, Basic, MOutOfN
	    length_penalty = computeLengthPenalty_Percentile(len, stats, res.cl, method, normalized_length, median_val);
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
      
      /**
       * @brief Compute stability penalty for Percentile-T based on resample quality.
       *
       * Penalizes:
       * - High outer resample failure rate (>10%)
       * - High inner SE estimation failure rate (>5%) [uses true attempted inner count]
       * - Low effective bootstrap sample count (<70% of B_outer)
       *
       * This prevents Percentile-T from winning when the double-bootstrap
       * procedure is struggling (e.g., small n, heavy tails, degeneracies).
       */
      template <class Result>
      static double computePercentileTStability(const Result& res)
      {
	const double B_outer = static_cast<double>(res.B_outer);
	const double B_inner = static_cast<double>(res.B_inner);

	const double skipped_outer = static_cast<double>(res.skipped_outer);
	const double skipped_inner = static_cast<double>(res.skipped_inner_total);
	const double effective_B   = static_cast<double>(res.effective_B);

	// Exact inner attempts (accounts for early stopping)
	const double inner_attempted_total = static_cast<double>(res.inner_attempted_total);

	// -------------------------------------------------------------------------
	// Guard against non-finite / division by zero / nonsense
	// -------------------------------------------------------------------------
	if (!std::isfinite(B_outer) || !std::isfinite(B_inner) ||
	    !std::isfinite(skipped_outer) || !std::isfinite(skipped_inner) ||
	    !std::isfinite(effective_B) || !std::isfinite(inner_attempted_total))
	  {
	    return std::numeric_limits<double>::infinity();
	  }

	if (B_outer < 1.0 || B_inner < 1.0)
	  {
	    return std::numeric_limits<double>::infinity();
	  }

	// If no inner attempts at all -> PT is unusable in this run.
	if (inner_attempted_total <= 0.0)
	  {
	    return std::numeric_limits<double>::infinity();
	  }

	double penalty = 0.0;

	// -------------------------------------------------------------------------
	// 1) OUTER RESAMPLE FAILURE RATE
	// -------------------------------------------------------------------------
	// Threshold: >10% outer failures indicates the statistic is unstable
	double outer_failure_rate = skipped_outer / B_outer;
	outer_failure_rate = std::clamp(outer_failure_rate, 0.0, 1.0);

	const double kOuterThreshold = AutoBootstrapConfiguration::kPercentileTOuterFailThreshold;

	if (outer_failure_rate > kOuterThreshold)
	  {
	    const double excess = outer_failure_rate - kOuterThreshold;
	    penalty += excess * excess * AutoBootstrapConfiguration::kPercentileTOuterPenaltyScale;
	  }

	// -------------------------------------------------------------------------
	// 2) INNER SE FAILURE RATE
	// -------------------------------------------------------------------------
	// Threshold: >5% inner failures indicates SE* estimation is unreliable.
	// Uses the actual number of attempted inner draws across all outers.
	double inner_failure_rate = skipped_inner / inner_attempted_total;
	inner_failure_rate = std::clamp(inner_failure_rate, 0.0, 1.0);

	const double kInnerThreshold = AutoBootstrapConfiguration::kPercentileTInnerFailThreshold;

	if (inner_failure_rate > kInnerThreshold)
	  {
	    const double excess = inner_failure_rate - kInnerThreshold;
	    penalty += excess * excess * AutoBootstrapConfiguration::kPercentileTInnerPenaltyScale;
	  }

	// -------------------------------------------------------------------------
	// 3) EFFECTIVE SAMPLE SIZE
	// -------------------------------------------------------------------------
	// We want effective_B ≥ 70% of B_outer for reliable quantile estimation.
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

      /**
       * @brief Computes stability penalty for BCa bootstrap intervals.
       *
       * The BCa (bias-corrected and accelerated) bootstrap method uses two correction
       * parameters: z0 (bias correction) and a (acceleration). This function penalizes
       * intervals where these parameters indicate instability or where the bootstrap
       * distribution shows extreme skewness that challenges BCa assumptions.
       *
       * Three types of instability are penalized:
       *
       * 1. **Excessive Bias (z0)**: When |z0| exceeds a threshold, it indicates
       *    significant bias in the bootstrap distribution. The penalty is quadratic
       *    in the excess: penalty ∝ (|z0| - threshold)²
       *
       * 2. **Excessive Acceleration (a)**: When |a| exceeds a threshold (which adapts
       *    based on skewness), it indicates the standard error is changing too rapidly.
       *    The threshold becomes stricter (0.08 vs 0.10) when skewness exceeds 3.0.
       *
       * 3. **Extreme Skewness**: When the bootstrap distribution's skewness exceeds
       *    a threshold, BCa's normal approximation becomes questionable. This is
       *    particularly important because BCa assumes mild asymmetry.
       *
       * Penalty scaling adapts to skewness:
       *   - For |skew| > 2.0: All penalty scales are multiplied by 1.5
       *   - For |skew| > 3.0: Acceleration threshold tightens from 0.10 to 0.08
       *
       * @param z0 Bias-correction parameter from BCa (typically in range [-1, 1])
       * @param accel Acceleration parameter from BCa (typically in range [-0.5, 0.5])
       * @param skew_boot Skewness of the bootstrap distribution
       * @param weights ScoringWeights containing base penalty scales (getBcaZ0Scale, getBcaAScale)
       * @param os Optional output stream for debug logging (logs when skew > 2.0 or penalty > 0)
       *
       * @return Stability penalty >= 0. Returns infinity if z0 or accel are non-finite.
       *
       * @note The penalty is always non-negative and increases quadratically with
       *       parameter excess, making it sensitive to outliers.
       *
       * @see AutoBootstrapConfiguration for threshold constants:
       *      - kBcaZ0SoftThreshold
       *      - kBcaASoftThreshold
       *      - kBcaSkewThreshold
       *      - kBcaSkewPenaltyScale
       */
      static double computeBCaStabilityPenalty(double z0,
					       double accel,
					       double skew_boot,
					       const ScoringWeights& weights = ScoringWeights(),
					       std::ostream* os = nullptr)
      {
	// ====================================================================
	// 0. NON-FINITE CHECK
	// ====================================================================
	// Non-finite parameters indicate catastrophic failure in BCa computation.
	// This should disqualify the candidate immediately.
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

	// ====================================================================
	// 1. BIAS (z0) PENALTY
	// ====================================================================
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

	// ====================================================================
	// 2. ACCELERATION (a) PENALTY
	// ====================================================================
	const double base_accel_threshold = AutoBootstrapConfiguration::kBcaASoftThreshold;

	// Avoid magic constant: if you can, prefer a config constant.
	// If you don't have one yet, this local named constant at least documents intent.
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

	// ====================================================================
	// 3. SKEWNESS PENALTY
	// ====================================================================
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

	// ====================================================================
	// 4. DEBUG LOGGING
	// ====================================================================
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

	const double theta_hat_d = num::to_double(res.mean);

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
	    ordering_penalty =
	      computeEmpiricalUnderCoveragePenalty(
						   t_stats,
						   theta_hat_d,
						   se_ref,
						   lo,
						   hi,
						   res.cl
						   );

	    double median_t_dummy = 0.0;
	    length_penalty =
	      computeLengthPenalty_PercentileT(len,
					       t_stats,
					       se_ref,
					       res.cl,
					       normalized_length,
					       median_t_dummy
					       );
	  }

	auto stability_penalty = computePercentileTStability(res);

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

	const double length_penalty = computeLengthPenalty_Percentile(
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
	const double stability_penalty = computeBCaStabilityPenalty(
								    z0,
								    accel,
								    skew_boot,
								    weights,
								    os
								    );

	// ====================================================================
	// ORDERING PENALTY (soft under-coverage only)
	// ====================================================================
	const double ordering_penalty = computeEmpiricalUnderCoveragePenalty(
									     stats,  // θ* statistics
									     lo,
									     hi,
									     cl
									     );

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
      
      /**
       * @brief Determines if Candidate 'a' strictly dominates Candidate 'b'.
       * * A candidate dominates another if it has equal or better scores in all 
       * primary categories (ordering, length) and is strictly better in at least one.
       * * @note Used for internal pairwise comparisons.
       */
      static bool dominates(const Candidate& a, const Candidate& b)
      {
        const bool better_or_equal_order  = a.getOrderingPenalty() <= b.getOrderingPenalty();
        const bool better_or_equal_length = a.getLengthPenalty()   <= b.getLengthPenalty();
        const bool strictly_better =
          (a.getOrderingPenalty() < b.getOrderingPenalty()) ||
          (a.getLengthPenalty()   < b.getLengthPenalty());

        return better_or_equal_order && better_or_equal_length && strictly_better;
      }

      /**
       * @brief Returns the tie-breaking priority for a method (lower is better).
       *
       * Preference order: BCa (1) > PercentileT (2) > MOutOfN (3) > Percentile (4) > Basic (5) > Normal (6).
       */
      static int methodPreference(MethodId m)
      {
        switch (m)
          {
          case MethodId::BCa:         return 1; // Highest preference
          case MethodId::PercentileT: return 2;
          case MethodId::MOutOfN:     return 3;
          case MethodId::Percentile:  return 4;
          case MethodId::Basic:       return 5;
          case MethodId::Normal:      return 6; // Lowest preference
          }
        return 100; // should not happen
             }
       
             /**
       * @brief Executes the selection tournament to find the best bootstrap method.
       *
       * This is the core logic that:
       * 1. Normalizes raw penalties for all candidates.
       * 2. Computes the weighted total score for each candidate.
       * 3. Applies hard rejection gates (e.g., BCa |z0| > 0.6).
       * 4. Selects the winner with the lowest score.
       * 5. Resolves ties using `methodPreference`.
       *
       * @param candidates A list of Candidate objects generated by the summarize methods.
       * @param weights Configuration for scoring weights.
       * @return An AutoCIResult containing the winner and full diagnostics.
       * @throws std::runtime_error If no valid candidates remain after hard gating.
       */
      static Result select(const std::vector<Candidate>& candidates,
			   const ScoringWeights& weights = ScoringWeights(),
			   const StatisticSupport& support = StatisticSupport::unbounded())
      {
	if (candidates.empty())
	  {
	    throw std::invalid_argument("AutoBootstrapSelector::select: no candidates provided.");
	  }

	const auto relativeEpsilon = [](double a, double b) -> double
	{
	  const double scale = 1.0 + std::max(std::fabs(a), std::fabs(b));
	  return AutoBootstrapConfiguration::kRelativeTieEpsilonScale * scale;
	};

	const auto enforceNonNegative = [](double x) -> double
	{
	  return (x < 0.0) ? 0.0 : x;
	};

	class RawComponents
	{
	public:
	  RawComponents(double orderingPenalty,
			double lengthPenalty,
			double stabilityPenalty,
			double centerShiftSq,
			double skewSq,
			double domainPenalty)
	    : m_ordering_penalty(orderingPenalty),
	      m_length_penalty(lengthPenalty),
	      m_stability_penalty(stabilityPenalty),
	      m_center_shift_sq(centerShiftSq),
	      m_skew_sq(skewSq),
	      m_domain_penalty(domainPenalty)
	  {}

	  double getOrderingPenalty() const { return m_ordering_penalty; }
	  double getLengthPenalty() const { return m_length_penalty; }
	  double getStabilityPenalty() const { return m_stability_penalty; }
	  double getCenterShiftSq() const { return m_center_shift_sq; }
	  double getSkewSq() const { return m_skew_sq; }
	  double getDomainPenalty() const { return m_domain_penalty; }

	private:
	  double m_ordering_penalty;
	  double m_length_penalty;
	  double m_stability_penalty;
	  double m_center_shift_sq;
	  double m_skew_sq;
	  double m_domain_penalty;
	};

	std::vector<RawComponents> raw;
	raw.reserve(candidates.size());

	bool hasBCaCandidate = false;

	bool bcaRejectedForInstability = false;
	bool bcaRejectedForLength      = false;
	bool bcaRejectedForDomain      = false;
	bool bcaRejectedForNonFinite   = false;

	for (const auto& c : candidates)
	  {
	    // ------------------------------------------------------------
	    // Robustify cosmetic metrics: if they are non-finite, treat as neutral.
	    // This prevents rejecting otherwise-usable candidates due to NaN skewness
	    // in degenerate bootstrap distributions (sd == 0, etc.).
	    // ------------------------------------------------------------
	    double centerShift = c.getCenterShiftInSe();
	    if (!std::isfinite(centerShift))
	      centerShift = 0.0;
	    double centerShiftSq = centerShift * centerShift;

	    const double skew = std::isfinite(c.getSkewBoot()) ? c.getSkewBoot() : 0.0;
	    const double skewAbs = std::fabs(skew);
	    constexpr double kSkewThreshold = 1.0;  // tune: 0.75–1.5
	    const double skewExcess = std::max(0.0, skewAbs - kSkewThreshold);
	    const double skewSq = skewExcess * skewExcess;

	    const double baseOrdering = c.getOrderingPenalty();
	    const double baseLength   = c.getLengthPenalty();
	    const double stabPenalty  = c.getStabilityPenalty();

	    // Domain penalty is driven by StatisticSupport, not by "ratio-ness".
	    double domainPenalty = 0.0;
	    {
	      const double lower = num::to_double(c.getLower());
	      if (support.violatesLowerBound(lower))
		{
		  domainPenalty = AutoBootstrapConfiguration::kDomainViolationPenalty;
		}
	    }

	    raw.emplace_back(baseOrdering,
			     baseLength,
			     stabPenalty,
			     centerShiftSq,
			     skewSq,
			     domainPenalty);

	    if (c.getMethod() == MethodId::BCa)
	      {
		hasBCaCandidate = true;
	      }
	  }

	const double kRefOrderingErrorSq = 0.10 * 0.10;
	const double kRefLengthErrorSq   = 1.0 * 1.0;
	const double kRefStability       = 0.25;
	const double kRefCenterShiftSq   = 2.0 * 2.0;
	const double kRefSkewSq          = 2.0 * 2.0;

	// Weights: ordering is intentionally left at 1.0 (dominant) for now.
	// If you later want it configurable, add it to ScoringWeights.
	const double w_order  = 1.0;
	const double w_center = weights.getCenterShiftWeight();
	const double w_skew   = weights.getSkewWeight();
	const double w_length = weights.getLengthWeight();
	const double w_stab   = weights.getStabilityWeight();

	std::vector<Candidate> enriched;
	enriched.reserve(candidates.size());

	std::vector<typename SelectionDiagnostics::ScoreBreakdown> breakdowns;
	breakdowns.reserve(candidates.size());

	for (std::size_t i = 0; i < candidates.size(); ++i)
	  {
	    const Candidate& c = candidates[i];
	    const RawComponents& r = raw[i];

	    const double orderingNorm  = enforceNonNegative(r.getOrderingPenalty()  / kRefOrderingErrorSq);
	    const double lengthNorm    = enforceNonNegative(r.getLengthPenalty()    / kRefLengthErrorSq);
	    const double stabilityNorm = enforceNonNegative(r.getStabilityPenalty() / kRefStability);
	    const double centerSqNorm  = enforceNonNegative(r.getCenterShiftSq()    / kRefCenterShiftSq);
	    const double skewSqNorm    = enforceNonNegative(r.getSkewSq()           / kRefSkewSq);

#ifdef DEBUG_BOOTSTRAP_SELECTION
	    if (orderingNorm > 5.0 || lengthNorm > 5.0 || stabilityNorm > 5.0) {
	      std::cerr << "[AutoCI WARNING] Method "
			<< Result::methodIdToString(c.getMethod())
			<< " extreme penalty: ord=" << orderingNorm
			<< " len=" << lengthNorm
			<< " stab=" << stabilityNorm << "\n";
	    }
#endif

	    const double orderingContrib  = w_order  * orderingNorm;
	    const double lengthContrib    = w_length * lengthNorm;
	    const double stabilityContrib = w_stab   * stabilityNorm;
	    const double centerSqContrib  = w_center * centerSqNorm;
	    const double skewSqContrib    = w_skew   * skewSqNorm;

	    // Note: domainPenalty is already used as a hard gate (below). Including it
	    // in the score is therefore redundant, but we keep it for diagnostic clarity.
	    const double domainContrib    = r.getDomainPenalty();

	    double bcaLengthOverflowPenalty = 0.0;

	    if (c.getMethod() == MethodId::BCa)
	      {
		const double threshold = AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold;
		const double lengthPenalty = c.getLengthPenalty();
		
		if (std::isfinite(lengthPenalty) && lengthPenalty > threshold)
		  {
		    const double overflow = lengthPenalty - threshold;
		    
		    bcaLengthOverflowPenalty = AutoBootstrapConfiguration::kBcaLengthOverflowScale * (overflow * overflow);
		  }
	      }
	    
	    const double totalScore =
	      orderingContrib +
	      lengthContrib +
	      stabilityContrib +
	      centerSqContrib +
	      skewSqContrib +
	      domainContrib +
	      bcaLengthOverflowPenalty;

	    breakdowns.emplace_back(
				    c.getMethod(),
				    /* raw */     r.getOrderingPenalty(), r.getLengthPenalty(), r.getStabilityPenalty(),
				    r.getCenterShiftSq(), r.getSkewSq(), r.getDomainPenalty(),
				    /* norm */    orderingNorm, lengthNorm, stabilityNorm, centerSqNorm, skewSqNorm,
				    /* contrib */ orderingContrib, lengthContrib, stabilityContrib,
				    centerSqContrib, skewSqContrib, domainContrib,
				    /* total */   totalScore);

	    enriched.push_back(c.withScore(totalScore));
	  }

	// -------------------------------------------------------------------
	// Minimum effective_B gate (hard reliability filter)
	//
	// Rationale:
	//   In small samples and/or with failure-prone methods (Percentile-T inner SE,
	//   degenerate stats, etc.), a candidate may request B reps but produce far fewer
	//   usable draws. Quantiles/diagnostics become unreliable when effective_B is too low.
	//   This gate disqualifies such candidates before scoring/tie-breaking.
	// -------------------------------------------------------------------
	const auto requestedOuterReplications = [](const Candidate& c) -> std::size_t
	{
	  // For all methods, Candidate::m_B_outer is the requested outer count.
	  // Percentile-like methods store their B in B_outer as well.
	  return c.getBOuter();
	};

	const auto minEffectiveFractionForMethod = [](MethodId m) -> double
	{
	  switch (m)
	    {
	    case MethodId::PercentileT:
	      // Keep consistent with PT stability logic already in config
	      return AutoBootstrapConfiguration::kPercentileTMinEffectiveFraction;

	      // BCa: can occasionally have more invalid draws depending on stat/resampler,
	      // but generally should still be high when healthy.
	    case MethodId::BCa:
	      return 0.90;

	      // Percentile / Basic / Normal / MOutOfN should typically succeed at very high rates.
	    default:
	      return 0.90;
	    }
	};

	const auto passesEffectiveBGate = [&](const Candidate& c) -> bool
	{
	  const std::size_t requested = requestedOuterReplications(c);
	  const std::size_t effective = c.getEffectiveB();

	  if (requested < 2)
	    return false;

	  // Absolute floor: even if fraction passes, enforce a minimum sample size for
	  // quantiles and penalty estimates to be meaningful.
	  constexpr std::size_t kMinEffectiveAbsolute = 200;

	  const double min_frac = minEffectiveFractionForMethod(c.getMethod());
	  const std::size_t required_by_frac =
	    static_cast<std::size_t>(std::ceil(min_frac * static_cast<double>(requested)));

	  const std::size_t required = std::max(kMinEffectiveAbsolute, required_by_frac);

	  return effective >= required;
	};
 
	const auto commonCandidateOk = [&](std::size_t i) -> bool
	{
	  if (!std::isfinite(enriched[i].getScore()))
	    return false;

	  // Domain violations are always disqualifying when support is provided.
	  // (If support == unbounded(), domain penalty is always zero.)
	  if (raw[i].getDomainPenalty() > 0.0)
	    return false;

	  // Hard gate: candidate must have enough usable bootstrap draws.
	  if (!passesEffectiveBGate(enriched[i]))
	    return false;

	  return true;
	};

	const auto bcaCandidateOk = [&](std::size_t i) -> bool
	{
	  if (!commonCandidateOk(i)) return false;

	  const Candidate& c = enriched[i];

	  if (!std::isfinite(c.getZ0()) || !std::isfinite(c.getAccel()))
	    return false;

	  if (std::fabs(c.getZ0()) > AutoBootstrapConfiguration::kBcaZ0HardLimit)
	    return false;

	  if (std::fabs(c.getAccel()) > AutoBootstrapConfiguration::kBcaAHardLimit)
	    return false;

	  return true;
	};

	std::optional<std::size_t> chosenIdxOpt;

	// -------------------------------------------------------------------
	// Score-Based Tournament with BCa Preference (tie-break only)
	// -------------------------------------------------------------------
	bool foundAny = false;
	double bestScore = std::numeric_limits<double>::infinity();

	for (std::size_t i = 0; i < enriched.size(); ++i)
	  {
	    const MethodId m = enriched[i].getMethod();
	    const bool isOk = (m == MethodId::BCa) ? bcaCandidateOk(i) : commonCandidateOk(i);
	    if (!isOk) continue;

	    const double s = enriched[i].getScore();

	    if (!foundAny)
	      {
		foundAny = true;
		bestScore = s;
		chosenIdxOpt = i;
		continue;
	      }

	    const double eps = relativeEpsilon(s, bestScore);

	    if (s < bestScore - eps)
	      {
		bestScore = s;
		chosenIdxOpt = i;
	      }
	    else if (std::fabs(s - bestScore) <= eps)
	      {
		// Tie: use method preference (BCa > PercentileT > ...)
		const int pBest = methodPreference(enriched[*chosenIdxOpt].getMethod());
		const int pCur  = methodPreference(enriched[i].getMethod());
		if (pCur < pBest)
		  {
		    bestScore = s;
		    chosenIdxOpt = i;
		  }
	      }
	  }

	// Diagnostics: If BCa existed but wasn't chosen, record why.
	if (hasBCaCandidate && chosenIdxOpt)
	  {
	    const Candidate& winner = enriched[*chosenIdxOpt];

	    if (winner.getMethod() != MethodId::BCa)
	      {
		for (std::size_t i = 0; i < enriched.size(); ++i)
		  {
		    if (enriched[i].getMethod() != MethodId::BCa) continue;

		    if (!std::isfinite(enriched[i].getScore())) { bcaRejectedForNonFinite = true; }
		    if (raw[i].getDomainPenalty() > 0.0)        { bcaRejectedForDomain  = true; }

		    if (!std::isfinite(enriched[i].getZ0()) || !std::isfinite(enriched[i].getAccel()))
		      {
			bcaRejectedForInstability = true;
		      }
		    else
		      {
			if (std::fabs(enriched[i].getZ0()) > AutoBootstrapConfiguration::kBcaZ0HardLimit)
			  {
			    bcaRejectedForInstability = true;
			  }
			if (std::fabs(enriched[i].getAccel()) > AutoBootstrapConfiguration::kBcaAHardLimit)
			  {
			    bcaRejectedForInstability = true;
			  }
		      }

		    if (enriched[i].getLengthPenalty() > AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold)
		      {
			bcaRejectedForLength = true;
		      }

		    break; // BCa is unique in this design
		  }
	      }
	  }

	if (!chosenIdxOpt)
	  {
	    throw std::runtime_error(
				     "AutoBootstrapSelector::select: no valid candidate (all scores non-finite or domain-violating).");
	  }

	const std::size_t chosenIdx = *chosenIdxOpt;
	const Candidate& chosen = enriched[chosenIdx];

	const bool bcaChosen = (chosen.getMethod() == MethodId::BCa);

	bool bcaRejectedForInstabilityPublic = false;
	bool bcaRejectedForLengthPublic      = false;
	bool bcaRejectedForDomainPublic      = false;
	bool bcaRejectedForNonFinitePublic   = false;

	if (hasBCaCandidate && !bcaChosen)
	  {
	    bcaRejectedForInstabilityPublic = bcaRejectedForInstability;
	    bcaRejectedForLengthPublic      = bcaRejectedForLength;
	    bcaRejectedForDomainPublic      = bcaRejectedForDomain;
	    bcaRejectedForNonFinitePublic   = bcaRejectedForNonFinite;
	  }

	SelectionDiagnostics diagnostics(
					 chosen.getMethod(),
					 Result::methodIdToString(chosen.getMethod()),
					 chosen.getScore(),
					 chosen.getStabilityPenalty(),
					 chosen.getLengthPenalty(),
					 hasBCaCandidate,
					 bcaChosen,
					 bcaRejectedForInstabilityPublic,
					 bcaRejectedForLengthPublic,
					 bcaRejectedForDomainPublic,
					 bcaRejectedForNonFinitePublic,
					 enriched.size(),
					 std::move(breakdowns));

	return Result(chosen.getMethod(), chosen, enriched, diagnostics);
      }      
    private:
    };

  } // namespace analysis
} // namespace palvalidator
