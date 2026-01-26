#pragma once

#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <optional>
#include <algorithm>
#include "AutoBootstrapConfiguration.h"
#include "AutoCIResult.h"
#include "BootstrapPenaltyCalculator.h"

/**
 * @file AutoBootstrapScoring.h
 * @brief Auxiliary classes for bootstrap method selection and scoring.
 *
 * This file contains supporting infrastructure for AutoBootstrapSelector:
 * - NormalizedScores: Encapsulates normalized scoring components
 * - BcaRejectionAnalysis: BCa rejection analysis results
 * - ScoreNormalizer: Handles score normalization and computation
 * - CandidateGateKeeper: Validates candidates against gating criteria
 * - ImprovedTournamentSelector: Tournament selection with tie-breaking
 *
 * These classes were refactored out of AutoBootstrapSelector.h to improve
 * code organization and maintainability.
 */

namespace palvalidator
{
  namespace analysis
  {
    // Forward declaration for CandidateGateKeeper
    template <class Decimal>
    class AutoBootstrapSelector;

    namespace detail
    {
      // ===========================================================================
      // HELPER CLASSES FOR REFACTORED SELECT METHOD
      // ===========================================================================

      /**
       * @brief Raw penalty components for a bootstrap candidate
       *
       * This class encapsulates the raw (unweighted, unnormalized) penalty components
       * computed for a bootstrap candidate. These serve as input to the scoring and
       * normalization process.
       */
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

	RawComponents(RawComponents&&) noexcept = default;
	RawComponents& operator=(RawComponents&&) noexcept = default;
	
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

      /**
       * @brief Builder for computing raw penalty components for bootstrap candidates
       *
       * This class implements the builder design pattern for computing raw penalty
       * components. It provides methods to build RawComponents for individual candidates
       * or process batches of candidates.
       */
      template <class Decimal>
      class RawComponentsBuilder
      {
      public:
        using Candidate = typename AutoCIResult<Decimal>::Candidate;
        using StatisticSupport = mkc_timeseries::StatisticSupport;

        /**
         * @brief Compute raw penalty components for a single candidate
         */
        static RawComponents computeRawComponentsForCandidate(
          const Candidate& c,
          const StatisticSupport& support)
        {
          // Robustify cosmetic metrics
          double center_shift = c.getCenterShiftInSe();
          if (!std::isfinite(center_shift))
            center_shift = 0.0;
          const double center_shift_sq = center_shift * center_shift;
          
          const double skew = std::isfinite(c.getSkewBoot()) ? c.getSkewBoot() : 0.0;
          const double skew_sq = BootstrapPenaltyCalculator<Decimal>::computeSkewPenalty(skew);
          
          const double domain_penalty = BootstrapPenaltyCalculator<Decimal>::computeDomainPenalty(c, support);
          
          return RawComponents(
            c.getOrderingPenalty(),
            c.getLengthPenalty(),
            c.getStabilityPenalty(),
            center_shift_sq,
            skew_sq,
            domain_penalty);
        }
        
        /**
         * @brief Compute raw penalties for all candidates
         */
        static std::vector<RawComponents> computeRawPenalties(
          const std::vector<Candidate>& candidates,
          const StatisticSupport& support)
        {
          std::vector<RawComponents> raw;
          raw.reserve(candidates.size());
          
          for (const auto& c : candidates)
          {
            raw.push_back(computeRawComponentsForCandidate(c, support));
          }
          
          return raw;
        }
      };

      /**
       * @brief Encapsulates normalized scoring components
       *
       * This class provides normalization results and weighted contributions for
       * scoring bootstrap candidates. It ensures all components are properly initialized
       * and provides read-only access to the normalized values and contributions.
       */
      class NormalizedScores
      {
      public:
        /**
         * @brief Constructs normalized scores with all 10 components
         *
         * @param orderingNorm Normalized ordering penalty
         * @param lengthNorm Normalized length penalty
         * @param stabilityNorm Normalized stability penalty
         * @param centerSqNorm Normalized center shift squared
         * @param skewSqNorm Normalized skewness squared
         * @param orderingContrib Weighted ordering contribution
         * @param lengthContrib Weighted length contribution
         * @param stabilityContrib Weighted stability contribution
         * @param centerSqContrib Weighted center shift contribution
         * @param skewSqContrib Weighted skewness contribution
         */
        NormalizedScores(double orderingNorm,
                        double lengthNorm,
                        double stabilityNorm,
                        double centerSqNorm,
                        double skewSqNorm,
                        double orderingContrib,
                        double lengthContrib,
                        double stabilityContrib,
                        double centerSqContrib,
                        double skewSqContrib)
          : m_ordering_norm(orderingNorm),
            m_length_norm(lengthNorm),
            m_stability_norm(stabilityNorm),
            m_center_sq_norm(centerSqNorm),
            m_skew_sq_norm(skewSqNorm),
            m_ordering_contrib(orderingContrib),
            m_length_contrib(lengthContrib),
            m_stability_contrib(stabilityContrib),
            m_center_sq_contrib(centerSqContrib),
            m_skew_sq_contrib(skewSqContrib)
        {}

	NormalizedScores(NormalizedScores &&) noexcept = default;
	NormalizedScores& operator=(NormalizedScores &&) noexcept = default;

        // Normalized penalty getters
        double getOrderingNorm() const { return m_ordering_norm; }
        double getLengthNorm() const { return m_length_norm; }
        double getStabilityNorm() const { return m_stability_norm; }
        double getCenterSqNorm() const { return m_center_sq_norm; }
        double getSkewSqNorm() const { return m_skew_sq_norm; }
        
        // Weighted contribution getters
        double getOrderingContrib() const { return m_ordering_contrib; }
        double getLengthContrib() const { return m_length_contrib; }
        double getStabilityContrib() const { return m_stability_contrib; }
        double getCenterSqContrib() const { return m_center_sq_contrib; }
        double getSkewSqContrib() const { return m_skew_sq_contrib; }

      private:
        // Normalized penalties (raw penalties divided by reference values)
        double m_ordering_norm;
        double m_length_norm;
        double m_stability_norm;
        double m_center_sq_norm;
        double m_skew_sq_norm;
        
        // Weighted contributions (normalized values multiplied by weights)
        double m_ordering_contrib;
        double m_length_contrib;
        double m_stability_contrib;
        double m_center_sq_contrib;
        double m_skew_sq_contrib;
      };

      /**
       * @brief Encapsulates BCa rejection analysis results
       *
       * This class provides a complete analysis of why BCa was or wasn't selected
       * during the bootstrap method tournament. It ensures consistent initialization
       * and provides read-only access to the analysis results.
       */
      class BcaRejectionAnalysis
      {
      public:
        /**
         * @brief Constructs a BCa rejection analysis with all parameters
         *
         * @param hasBcaCandidate True if a BCa candidate was present in the tournament
         * @param bcaChosen True if BCa was ultimately selected as the winner
         * @param rejectedForInstability True if BCa was rejected due to extreme z0/accel parameters
         * @param rejectedForLength True if BCa was rejected due to excessive length penalty
         * @param rejectedForDomain True if BCa was rejected due to domain/support violations
         * @param rejectedForNonFinite True if BCa was rejected due to non-finite scores
         */
        BcaRejectionAnalysis(bool hasBcaCandidate,
                           bool bcaChosen,
                           bool rejectedForInstability,
                           bool rejectedForLength,
                           bool rejectedForDomain,
                           bool rejectedForNonFinite)
          : m_has_bca_candidate(hasBcaCandidate),
            m_bca_chosen(bcaChosen),
            m_rejected_for_instability(rejectedForInstability),
            m_rejected_for_length(rejectedForLength),
            m_rejected_for_domain(rejectedForDomain),
            m_rejected_for_non_finite(rejectedForNonFinite)
        {}
        
        /**
         * @brief Gets whether a BCa candidate was present in the tournament
         */
        bool hasBcaCandidate() const { return m_has_bca_candidate; }
        
        /**
         * @brief Gets whether BCa was selected as the winner
         */
        bool bcaChosen() const { return m_bca_chosen; }
        
        /**
         * @brief Gets whether BCa was rejected due to parameter instability
         */
        bool rejectedForInstability() const { return m_rejected_for_instability; }
        
        /**
         * @brief Gets whether BCa was rejected due to excessive length penalty
         */
        bool rejectedForLength() const { return m_rejected_for_length; }
        
        /**
         * @brief Gets whether BCa was rejected due to domain violations
         */
        bool rejectedForDomain() const { return m_rejected_for_domain; }
        
        /**
         * @brief Gets whether BCa was rejected due to non-finite scores
         */
        bool rejectedForNonFinite() const { return m_rejected_for_non_finite; }

      private:
        bool m_has_bca_candidate;
        bool m_bca_chosen;
        bool m_rejected_for_instability;
        bool m_rejected_for_length;
        bool m_rejected_for_domain;
        bool m_rejected_for_non_finite;
      };

      /**
       * @brief Handles score normalization and computation
       */
      template <class Decimal, class ScoringWeights, class RawComponents>
      class ScoreNormalizer
      {
      public:
        using MethodId = typename AutoCIResult<Decimal>::MethodId;
        
        explicit ScoreNormalizer(const ScoringWeights& weights)
          : m_weights(weights)
        {}
        
        /**
         * @brief Normalize raw penalty components
         */
        NormalizedScores normalize(const RawComponents& raw) const
        {
          // Compute normalized penalties first
          const double ordering_norm = enforceNonNegative(
							  raw.getOrderingPenalty() / AutoBootstrapConfiguration::kRefOrderingErrorSq);
          const double length_norm = enforceNonNegative(
            raw.getLengthPenalty() / AutoBootstrapConfiguration::kRefLengthErrorSq);
          const double stability_norm = enforceNonNegative(
            raw.getStabilityPenalty() / AutoBootstrapConfiguration::kRefStability);
          const double center_sq_norm = enforceNonNegative(
            raw.getCenterShiftSq() / AutoBootstrapConfiguration::kRefCenterShiftSq);
          const double skew_sq_norm = enforceNonNegative(
            raw.getSkewSq() / AutoBootstrapConfiguration::kRefSkewSq);
          
          // Get weights
          const double w_order = 1.0;
          const double w_center = m_weights.getCenterShiftWeight();
          const double w_skew = m_weights.getSkewWeight();
          const double w_length = m_weights.getLengthWeight();
          const double w_stab = m_weights.getStabilityWeight();
          
          // Compute weighted contributions
          const double ordering_contrib = w_order * ordering_norm;
          const double length_contrib = w_length * length_norm;
          const double stability_contrib = w_stab * stability_norm;
          const double center_sq_contrib = w_center * center_sq_norm;
          const double skew_sq_contrib = w_skew * skew_sq_norm;
          
          // Construct and return NormalizedScores with all computed values
          return NormalizedScores(
            ordering_norm, length_norm, stability_norm, center_sq_norm, skew_sq_norm,
            ordering_contrib, length_contrib, stability_contrib, center_sq_contrib, skew_sq_contrib
          );
        }
        
        /**
         * @brief Compute total score including BCa-specific overflow penalty
         */
        double computeTotalScore(const NormalizedScores& norm,
                                 const RawComponents& raw,
                                 MethodId method,
                                 double length_penalty) const
        {
          double total = norm.getOrderingContrib() +
                        norm.getLengthContrib() +
                        norm.getStabilityContrib() +
                        norm.getCenterSqContrib() +
                        norm.getSkewSqContrib() +
                        raw.getDomainPenalty();
          
          // BCa-specific length overflow penalty
          if (method == MethodId::BCa)
          {
            total += computeBcaLengthOverflow(length_penalty);
          }
          
          return total;
        }

	static double computeBcaLengthOverflow(double length_penalty)
        {
          const double threshold = AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold;
          
          if (std::isfinite(length_penalty) && length_penalty > threshold)
          {
            const double overflow = length_penalty - threshold;
            return AutoBootstrapConfiguration::kBcaLengthOverflowScale *
                   (overflow * overflow);
          }
          
          return 0.0;
        }

      private:
        static double enforceNonNegative(double x)
        {
          return (x < 0.0) ? 0.0 : x;
        }
        
        
        ScoringWeights m_weights;
      };

      /**
       * @brief Validates candidates against gating criteria
       */
      template <class Decimal, class RawComponents>
      class CandidateGateKeeper
      {
      public:
	using Result    = AutoCIResult<Decimal>;
	using MethodId  = typename Result::MethodId;
        using Candidate = typename Result::Candidate;

	/**
	 * @brief Checks if a candidate passes the effective bootstrap sample size gate.
	 *
	 * This gate ensures that the bootstrap method has sufficient valid samples
	 * to generate reliable confidence intervals. Different methods have different
	 * requirements based on their statistical properties.
	 *
	 * @param candidate The candidate to check
	 * @return True if the candidate passes the effective B gate, false otherwise
	 */
	static bool passesEffectiveBGate(const Candidate& candidate)
	{
	  const std::size_t requested = candidate.getBOuter();
	  const std::size_t effective = candidate.getEffectiveB();
        
	  if (requested < 2)
	    return false;
        
	  constexpr std::size_t kMinEffectiveAbsolute = 200;
        
	  // Method-specific minimum fraction requirements
	  double min_frac = 0.90;
	
	  switch (candidate.getMethod())
	    {
	    case MethodId::PercentileT:
	      min_frac = AutoBootstrapConfiguration::kPercentileTMinEffectiveFraction;
	      break;

	    case MethodId::BCa:
	      min_frac = 0.90;
	      break;

	    default:
	      min_frac = 0.90;
	      break;
	    }
        
	  const std::size_t required_by_frac =
	    static_cast<std::size_t>(std::ceil(min_frac * static_cast<double>(requested)));

	  const std::size_t required = std::max(kMinEffectiveAbsolute, required_by_frac);
        
	  return effective >= required;
	}

        /**
         * @brief Check if candidate passes common gates (non-BCa methods)
         */
        bool isCommonCandidateValid(const Candidate& candidate,
                                   const RawComponents& raw) const
        {
          if (!std::isfinite(candidate.getScore()))
            return false;
          
          if (raw.getDomainPenalty() > 0.0)
            return false;

          if (!passesEffectiveBGate(candidate))
            return false;
          
          return true;
        }
        
        /**
         * @brief Check if BCa candidate passes additional BCa-specific gates
         */
        bool isBcaCandidateValid(const Candidate& candidate,
                                const RawComponents& raw) const
        {
          if (!isCommonCandidateValid(candidate, raw))
            return false;
          
          if (!std::isfinite(candidate.getZ0()) ||
              !std::isfinite(candidate.getAccel()))
            return false;
          
          if (std::fabs(candidate.getZ0()) >
              AutoBootstrapConfiguration::kBcaZ0HardLimit)
            return false;
          
          if (std::fabs(candidate.getAccel()) >
              AutoBootstrapConfiguration::kBcaAHardLimit)
            return false;
          
          return true;
        }
      };

      /**
       * @brief Improved tournament selector that properly handles ties
       */
      template <class Decimal>
      class ImprovedTournamentSelector
      {
      public:
        using Candidate = typename AutoCIResult<Decimal>::Candidate;
        using MethodId = typename AutoCIResult<Decimal>::MethodId;
        
        ImprovedTournamentSelector(const std::vector<Candidate>& candidates)
          : m_candidates(candidates),
            m_found_any(false),
            m_best_score(std::numeric_limits<double>::infinity()),
            m_tie_epsilon_used(0.0)
        {}
        
        void consider(std::size_t index)
        {
          const Candidate& candidate = m_candidates[index];
          const double score = candidate.getScore();
          
          if (!m_found_any)
          {
            m_found_any = true;
            m_best_score = score;
            m_winner_idx = index;
            m_tie_epsilon_used = relativeEpsilon(score, score);
            return;
          }
          
          const double eps = relativeEpsilon(score, m_best_score);
          m_tie_epsilon_used = eps;
          
          if (score < m_best_score - eps)
          {
            m_best_score = score;
            m_winner_idx = index;
          }
          else if (std::fabs(score - m_best_score) <= eps)
          {
            // Tie: use method preference
            const Candidate& current_winner = m_candidates[m_winner_idx.value()];
            const int pBest = methodPreference(current_winner.getMethod());
            const int pCur = methodPreference(candidate.getMethod());
            
            if (pCur < pBest)
            {
              m_best_score = score;
              m_winner_idx = index;
            }
          }
        }
        
        bool hasWinner() const
        {
          return m_found_any;
        }

        std::size_t getWinnerIndex() const
        {
          if (!m_found_any)
            throw std::logic_error("TournamentSelector: no winner selected");
          return m_winner_idx.value();
        }
        
        double getTieEpsilon() const
	{
	  return m_tie_epsilon_used;
	}

	static int methodPreference(MethodId m)
        {
          switch (m)
          {
            case MethodId::BCa: return 1;
            case MethodId::PercentileT: return 2;
            case MethodId::MOutOfN: return 3;
            case MethodId::Percentile: return 4;
            case MethodId::Basic: return 5;
            case MethodId::Normal: return 6;
          }
          return 100;
        }

      private:
        
        static double relativeEpsilon(double a, double b)
        {
          const double scale = 1.0 + std::max(std::fabs(a), std::fabs(b));
          return AutoBootstrapConfiguration::kRelativeTieEpsilonScale * scale;
        }
        
        const std::vector<Candidate>& m_candidates;
        bool m_found_any;
        double m_best_score;
        std::optional<std::size_t> m_winner_idx;
        double m_tie_epsilon_used;
      };

    } // namespace detail
  } // namespace analysis
} // namespace palvalidator
