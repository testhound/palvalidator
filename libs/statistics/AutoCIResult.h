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
#include "number.h"
#include "StatUtils.h"
#include "NormalQuantile.h"
#include "NormalDistribution.h"
#include "AutoBootstrapConfiguration.h"
#include "CandidateReject.h"

namespace palvalidator
{
  namespace analysis
  {
    // Forward declarations
    using mkc_timeseries::StatisticSupport;
    using palvalidator::diagnostics::CandidateReject;
    using palvalidator::diagnostics::CandidateReject;
    using palvalidator::diagnostics::hasRejection;
    using palvalidator::diagnostics::rejectionMaskToString;

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

      // ===========================================================================
      // CANDIDATE CLASS - MODIFIED
      // ===========================================================================
      /**
       * @brief Represents the performance and scoring metrics for a single bootstrap method.
       *
       * A Candidate stores the calculated confidence interval bounds, the raw bootstrap 
       * statistics (SE, skewness), and the calculated penalty scores (length, stability, ordering)
       * used during the selection tournament.
       * 
       * V2 ENHANCEMENTS:
       * - candidate_id: Unique identifier within tournament
       * - rank: Ranking by score (1=best)
       * - is_chosen: Flag indicating if this candidate won
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
	 * @param candidate_id [V2] Unique identifier within the tournament (default: 0).
	 * @param rank [V2] Ranking by score, 1-based (default: 0 = unranked).
	 * @param is_chosen [V2] True if this candidate won the tournament (default: false).
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
		  double      score = std::numeric_limits<double>::quiet_NaN(),
		  // NEW V2 PARAMETERS (defaults for backward compatibility)
		  std::uint64_t candidate_id = 0,
		  std::size_t   rank = 0,
		  bool          is_chosen = false)
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
	  m_score(score),
	  m_candidate_id(candidate_id),
	  m_rank(rank),
	  m_is_chosen(is_chosen)
	{
	}

	// -- Existing Getters (unchanged) --
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
	double      getStabilityPenalty() const { return m_stability_penalty; }

	double      getZ0() const { return m_z0; }
	double      getAccel() const { return m_accel; }
	double      getScore() const { return m_score; }
	double      getInnerFailureRate() const { return m_inner_failure_rate; }

	// -- NEW V2 Getters --
	/**
	 * @brief Returns the unique identifier for this candidate within the tournament.
	 */
	std::uint64_t getCandidateId() const { return m_candidate_id; }
    
	/**
	 * @brief Returns the rank of this candidate (1=best, 2=second best, etc.).
	 * Returns 0 if unranked.
	 */
	std::size_t getRank() const { return m_rank; }
    
	/**
	 * @brief Returns true if this candidate was chosen as the winner.
	 */
	bool isChosen() const { return m_is_chosen; }

	/**
	 * @brief Returns a copy of this candidate with an updated total score.
	 * Used during the final scoring phase to attach the computed weighted score.
	 * 
	 * UPDATED FOR V2: Now preserves candidate_id, rank, and is_chosen fields.
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
			   m_z0, m_accel, m_inner_failure_rate, 
			   newScore,
			   m_candidate_id, m_rank, m_is_chosen);  // V2: Preserve metadata
	}

	Candidate markAsChosen() const
        {
          Candidate out = *this;
          out.m_is_chosen = true;
          return out;
        }

	/**
	 * @brief [V2] Returns a copy with candidate_id and rank set.
	 * Used during candidate preparation phase.
	 */
	Candidate withMetadata(std::uint64_t id, std::size_t final_rank, bool chosen) const
	{
	  return Candidate(m_method, m_mean, m_lower, m_upper, m_cl,
			   m_n, m_B_outer, m_B_inner, m_effective_B, m_skipped_total,
			   m_se_boot, m_skew_boot, m_median_boot,
			   m_center_shift_in_se, m_normalized_length,
			   m_ordering_penalty, 
			   m_length_penalty,
			   m_stability_penalty, 
			   m_z0, m_accel, m_inner_failure_rate, 
			   m_score,
			   id, final_rank, chosen);
	}

      private:
	// Existing fields
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
    
	// NEW V2 fields
	std::uint64_t m_candidate_id;
	std::size_t   m_rank;
	bool          m_is_chosen;
      };

      // ===========================================================================
      // SELECTION DIAGNOSTICS CLASS - MODIFIED
      // ===========================================================================
      /**
       * @brief Provides detailed diagnostic information about the selection process.
       *
       * Stores reasoning for why the winning method was chosen and why others (specifically BCa)
       * might have been rejected (e.g., due to instability or invalid parameters).
       * 
       * V2 ENHANCEMENTS:
       * - ScoreBreakdown: Now includes rejection tracking and support validation
       * - tie_epsilon: Tracks the tolerance used for tie detection
       */
      class SelectionDiagnostics
      {
      public:
	// =========================================================================
	// SCORE BREAKDOWN CLASS - MODIFIED
	// =========================================================================
	/**
	 * @brief Breakdown of the specific penalty components that contributed to a method's total score.
	 * Useful for debugging why a specific method lost the tournament.
	 * 
	 * V2 ENHANCEMENTS:
	 * - rejection_mask: Bitmask of rejection reasons
	 * - rejection_text: Human-readable rejection reasons
	 * - passed_gates: Whether candidate passed all hard gates
	 * - violates_support: Whether interval violates domain constraints
	 * - support_lower/upper: Domain constraint bounds
	 */
	class ScoreBreakdown
	{
	public:
	  /**
	   * @brief Constructs a breakdown of raw and normalized penalty components.
	   * 
	   * @param method The method being scored.
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
	   * @param rejection_mask [V2] Bitmask of rejection reasons (default: None).
	   * @param rejection_text [V2] Human-readable rejection reasons (default: empty).
	   * @param passed_gates [V2] True if passed all gates (default: true).
	   * @param violates_support [V2] True if violates domain (default: false).
	   * @param support_lower [V2] Minimum allowed value (default: NaN).
	   * @param support_upper [V2] Maximum allowed value (default: NaN).
	   */
	  ScoreBreakdown(
			 MethodId method,
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
			 double totalScore,
			 // NEW V2 PARAMETERS (defaults for backward compatibility)
			 palvalidator::diagnostics::CandidateReject rejection_mask = 
			 palvalidator::diagnostics::CandidateReject::None,
			 std::string rejection_text = "",
			 bool passed_gates = true,
			 bool violates_support = false,
			 double support_lower = std::numeric_limits<double>::quiet_NaN(),
			 double support_upper = std::numeric_limits<double>::quiet_NaN())
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
	    m_total_score(totalScore),
	    m_rejection_mask(rejection_mask),
	    m_rejection_text(std::move(rejection_text)),
	    m_passed_gates(passed_gates),
	    m_violates_support(violates_support),
	    m_support_lower(support_lower),
	    m_support_upper(support_upper)
	  {}

	  // -- Existing Getters (unchanged) --
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

	  // -- NEW V2 Getters --
	  /**
	   * @brief Returns bitmask of rejection reasons (CandidateReject::None if passed).
	   */
	  palvalidator::diagnostics::CandidateReject getRejectionMask() const 
	  { 
	    return m_rejection_mask; 
	  }
      
	  /**
	   * @brief Returns human-readable rejection reasons (empty if passed).
	   */
	  const std::string& getRejectionText() const { return m_rejection_text; }
      
	  /**
	   * @brief Returns true if this candidate passed all hard gates.
	   */
	  bool passedGates() const { return m_passed_gates; }
      
	  /**
	   * @brief Returns true if this candidate's interval violates domain constraints.
	   */
	  bool violatesSupport() const { return m_violates_support; }
      
	  /**
	   * @brief Returns the lower bound of the support (NaN if unbounded below).
	   */
	  double getSupportLowerBound() const { return m_support_lower; }
      
	  /**
	   * @brief Returns the upper bound of the support (NaN if unbounded above).
	   */
	  double getSupportUpperBound() const { return m_support_upper; }

	private:
	  // Existing fields
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
      
	  // NEW V2 fields
	  palvalidator::diagnostics::CandidateReject m_rejection_mask;
	  std::string m_rejection_text;
	  bool m_passed_gates;
	  bool m_violates_support;
	  double m_support_lower;
	  double m_support_upper;
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
	 * @param bcaRejectedForDomain True if BCa violated domain constraints (default: false)
	 * @param bcaRejectedForNonFinite True if BCa had non-finite parameters (default: false)
	 * @param numCandidates Total number of candidates evaluated (default: 0)
	 * @param scoreBreakdowns Detailed score decomposition for all candidates (default: empty)
	 * @param tie_epsilon [V2] Relative tolerance used for tie detection (default: 1e-10)
	 */
	SelectionDiagnostics(
			     MethodId     chosenMethod,
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
			     std::vector<ScoreBreakdown> scoreBreakdowns = std::vector<ScoreBreakdown>(),
			     // NEW V2 PARAMETER (default for backward compatibility)
			     double       tie_epsilon = 1e-10)
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
	  m_score_breakdowns(std::move(scoreBreakdowns)),
	  m_tie_epsilon(tie_epsilon)
	{}

	// -- Existing Getters (unchanged) --
	MethodId getChosenMethod() const { return m_chosen_method; }
	const std::string& getChosenMethodName() const { return m_chosen_method_name; }
	double getChosenScore() const { return m_chosen_score; }
	double getChosenStabilityPenalty() const { return m_chosen_stability_penalty; }
	double getChosenLengthPenalty() const { return m_chosen_length_penalty; }
	bool hasBCaCandidate() const { return m_has_bca_candidate; }
	bool isBCaChosen() const { return m_bca_chosen; }
	bool wasBCaRejectedForInstability() const { return m_bca_rejected_for_instability; }
	bool wasBCaRejectedForLength() const { return m_bca_rejected_for_length; }
	bool wasBCaRejectedForDomain() const { return m_bca_rejected_for_domain; }
	bool wasBCaRejectedForNonFiniteParameters() const { return m_bca_rejected_for_non_finite; }
	std::size_t getNumCandidates() const { return m_num_candidates; }
	bool hasScoreBreakdowns() const { return !m_score_breakdowns.empty(); }
	const std::vector<ScoreBreakdown>& getScoreBreakdowns() const { return m_score_breakdowns; }

	// -- NEW V2 Getter --
	/**
	 * @brief Returns the relative tolerance used for tie detection in scoring.
	 */
	double getTieEpsilon() const { return m_tie_epsilon; }

      private:
	// Existing fields
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
    
	// NEW V2 field
	double m_tie_epsilon;
      };

      // ===========================================================================
      // AutoCIResult Constructor and Methods (unchanged)
      // ===========================================================================
  
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

      MethodId getChosenMethod() const { return m_chosen_method; }
      const Candidate& getChosenCandidate() const { return m_chosen; }
      double getBootstrapMedian() const { return m_chosen.getMedianBoot(); }
      const std::vector<Candidate>& getCandidates() const { return m_candidates; }
      const SelectionDiagnostics& getDiagnostics() const { return m_diagnostics; }

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

  } // namespace analysis
} // namespace palvalidator