#pragma once

#include "CandidateReject.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <cmath>

namespace palvalidator::diagnostics
{
  enum class MetricType { GeoMean, ProfitFactor };

  /**
   * @brief Diagnostic record for bootstrap tournament results with complete candidate details.
   *
   * One record corresponds to one (tournament, candidate method) row.
   * Uses nested classes for logical grouping and strict initialization.
   */
  class BootstrapDiagnosticRecord final
  {
  public:
    // =========================================================================
    // Tournament-level context shared by all candidates (true invariants only)
    // =========================================================================
    class TournamentContext
    {
    public:
      TournamentContext(std::uint64_t runID,
                        std::uint64_t strategyUniqueId,
                        std::string   strategyName,
                        std::string   symbol,
                        MetricType    metricType,
                        double        confidenceLevel,
                        std::size_t   sampleSize,
                        std::size_t   numCandidates,
                        double        tieEpsilon)
        : m_runID(runID),
          m_strategyUniqueId(strategyUniqueId),
          m_strategyName(std::move(strategyName)),
          m_symbol(std::move(symbol)),
          m_metricType(metricType),
          m_confidenceLevel(confidenceLevel),
          m_sampleSize(sampleSize),
          m_numCandidates(numCandidates),
          m_tieEpsilon(tieEpsilon)
      {
        if (m_confidenceLevel <= 0.0 || m_confidenceLevel >= 1.0) {
          throw std::invalid_argument("TournamentContext: confidenceLevel must be in (0, 1)");
        }
        if (m_numCandidates == 0) {
          throw std::invalid_argument("TournamentContext: numCandidates must be > 0");
        }
      }

      TournamentContext() = delete;

      std::uint64_t getRunID() const { return m_runID; }
      std::uint64_t getStrategyUniqueId() const { return m_strategyUniqueId; }
      const std::string& getStrategyName() const { return m_strategyName; }
      const std::string& getSymbol() const { return m_symbol; }
      MetricType getMetricType() const { return m_metricType; }
      double getConfidenceLevel() const { return m_confidenceLevel; }
      std::size_t getSampleSize() const { return m_sampleSize; }
      std::size_t getNumCandidates() const { return m_numCandidates; }
      double getTieEpsilon() const { return m_tieEpsilon; }

    private:
      const std::uint64_t m_runID;
      const std::uint64_t m_strategyUniqueId;
      const std::string   m_strategyName;
      const std::string   m_symbol;
      const MetricType    m_metricType;
      const double        m_confidenceLevel;
      const std::size_t   m_sampleSize;
      const std::size_t   m_numCandidates;
      const double        m_tieEpsilon;
    };

    // =========================================================================
    // Candidate identification and ranking
    // =========================================================================
    class CandidateIdentity
    {
    public:
      /**
       * @param rank 1-based for ranked candidates; 0 allowed to mean "unranked / not eligible".
       *             If isChosen==true, rank must be >= 1.
       */
      CandidateIdentity(std::uint64_t candidateID,
                        std::string   methodName,
                        bool          isChosen,
                        std::size_t   rank)
        : m_candidateID(candidateID),
          m_methodName(std::move(methodName)),
          m_isChosen(isChosen),
          m_rank(rank)
      {
        if (m_methodName.empty()) {
          throw std::invalid_argument("CandidateIdentity: methodName cannot be empty");
        }
        if (m_isChosen && m_rank == 0) {
          throw std::invalid_argument("CandidateIdentity: chosen candidate must have rank >= 1");
        }
      }

      CandidateIdentity() = delete;

      std::uint64_t getCandidateID() const { return m_candidateID; }
      const std::string& getMethodName() const { return m_methodName; }
      bool isChosen() const { return m_isChosen; }
      std::size_t getRank() const { return m_rank; }

    private:
      const std::uint64_t m_candidateID;
      const std::string   m_methodName;
      const bool          m_isChosen;
      const std::size_t   m_rank;
    };

    // =========================================================================
    // Candidate-level distribution / engine stats (method-specific in practice)
    // =========================================================================
    class CandidateDistributionStats
    {
    public:
      CandidateDistributionStats(std::size_t B_outer,
                                 std::size_t B_inner,
                                 std::size_t effective_B,
                                 std::size_t skipped_total,
                                 double      se_boot,
                                 double      skew_boot,
                                 double      median_boot,
                                 double      center_shift_in_se,
                                 double      normalized_length)
        : m_B_outer(B_outer),
          m_B_inner(B_inner),
          m_effective_B(effective_B),
          m_skipped_total(skipped_total),
          m_se_boot(se_boot),
          m_skew_boot(skew_boot),
          m_median_boot(median_boot),
          m_center_shift_in_se(center_shift_in_se),
          m_normalized_length(normalized_length)
      {}

      CandidateDistributionStats() = delete;

      std::size_t getBOuter() const { return m_B_outer; }
      std::size_t getBInner() const { return m_B_inner; }
      std::size_t getEffectiveB() const { return m_effective_B; }
      std::size_t getSkippedTotal() const { return m_skipped_total; }

      double getSeBoot() const { return m_se_boot; }
      double getSkewBoot() const { return m_skew_boot; }
      double getMedianBoot() const { return m_median_boot; }
      double getCenterShiftInSe() const { return m_center_shift_in_se; }
      double getNormalizedLength() const { return m_normalized_length; }

    private:
      const std::size_t m_B_outer;
      const std::size_t m_B_inner;
      const std::size_t m_effective_B;
      const std::size_t m_skipped_total;

      const double m_se_boot;
      const double m_skew_boot;
      const double m_median_boot;
      const double m_center_shift_in_se;
      const double m_normalized_length;
    };

    // =========================================================================
    // Confidence interval bounds and final score (candidate-level)
    // =========================================================================
    class IntervalAndScore
    {
    public:
      IntervalAndScore(double lowerBound,
                       double upperBound,
                       double finalScore)
        : m_lowerBound(lowerBound),
          m_upperBound(upperBound),
          m_intervalLength(upperBound - lowerBound),
          m_finalScore(finalScore)
      {
        if (std::isfinite(m_lowerBound) && std::isfinite(m_upperBound) &&
            m_lowerBound > m_upperBound) {
          throw std::invalid_argument("IntervalAndScore: lowerBound > upperBound");
        }
      }

      IntervalAndScore() = delete;

      double getLowerBound() const { return m_lowerBound; }
      double getUpperBound() const { return m_upperBound; }
      double getIntervalLength() const { return m_intervalLength; }
      double getFinalScore() const { return m_finalScore; }

    private:
      const double m_lowerBound;
      const double m_upperBound;
      const double m_intervalLength;
      const double m_finalScore;
    };

    // =========================================================================
    // Rejection and gating information
    // =========================================================================
    class RejectionInfo
    {
    public:
      RejectionInfo(CandidateReject rejectionMask,
                    std::string     rejectionText,
                    bool            passedGates)
        : m_rejectionMask(rejectionMask),
          m_rejectionText(std::move(rejectionText)),
          m_passedGates(passedGates)
      {
        // Consistency check
        if (m_passedGates && m_rejectionMask != CandidateReject::None) {
          throw std::invalid_argument("RejectionInfo: passedGates=true but rejectionMask != None");
        }
        if (!m_passedGates && m_rejectionMask == CandidateReject::None) {
          throw std::invalid_argument("RejectionInfo: passedGates=false but rejectionMask == None");
        }
      }

      RejectionInfo() = delete;

      CandidateReject getRejectionMask() const { return m_rejectionMask; }
      const std::string& getRejectionText() const { return m_rejectionText; }
      bool passedGates() const { return m_passedGates; }

    private:
      const CandidateReject m_rejectionMask;
      const std::string     m_rejectionText;
      const bool            m_passedGates;
    };

    // =========================================================================
    // Support constraint validation
    // =========================================================================
    class SupportValidation
    {
    public:
      SupportValidation(bool   violatesSupport,
                        double supportLowerBound,
                        double supportUpperBound)
        : m_violatesSupport(violatesSupport),
          m_supportLowerBound(supportLowerBound),
          m_supportUpperBound(supportUpperBound)
      {}

      SupportValidation() = delete;

      bool violatesSupport() const { return m_violatesSupport; }
      double getSupportLowerBound() const { return m_supportLowerBound; }
      double getSupportUpperBound() const { return m_supportUpperBound; }

    private:
      const bool   m_violatesSupport;
      const double m_supportLowerBound;
      const double m_supportUpperBound;
    };

    // =========================================================================
    // Penalty decomposition
    // =========================================================================
    class PenaltyComponents
    {
    public:
      PenaltyComponents(double raw, double normalized, double contribution)
        : m_raw(raw),
          m_normalized(normalized),
          m_contribution(contribution)
      {}

      PenaltyComponents() = delete;

      double getRaw() const { return m_raw; }
      double getNormalized() const { return m_normalized; }
      double getContribution() const { return m_contribution; }

    private:
      double m_raw;
      double m_normalized;
      double m_contribution;
    };

    /**
     * @brief Complete penalty breakdown (now includes the score terms actually used)
     *
     * Note: Some components may be 0.0 for methods where they don't apply.
     */
    class PenaltyBreakdown
    {
    public:
      PenaltyBreakdown(PenaltyComponents ordering,
                       PenaltyComponents length,
                       PenaltyComponents stability,
                       PenaltyComponents domain,
                       PenaltyComponents centerShift,
                       PenaltyComponents skew,
                       PenaltyComponents bcaOverflow)
        : m_ordering(std::move(ordering)),
          m_length(std::move(length)),
          m_stability(std::move(stability)),
          m_domain(std::move(domain)),
          m_centerShift(std::move(centerShift)),
          m_skew(std::move(skew)),
          m_bcaOverflow(std::move(bcaOverflow))
      {}

      PenaltyBreakdown() = delete;

      const PenaltyComponents& getOrdering() const { return m_ordering; }
      const PenaltyComponents& getLength() const { return m_length; }
      const PenaltyComponents& getStability() const { return m_stability; }
      const PenaltyComponents& getDomain() const { return m_domain; }
      const PenaltyComponents& getCenterShift() const { return m_centerShift; }
      const PenaltyComponents& getSkew() const { return m_skew; }
      const PenaltyComponents& getBcaOverflow() const { return m_bcaOverflow; }

    private:
      const PenaltyComponents m_ordering;
      const PenaltyComponents m_length;
      const PenaltyComponents m_stability;
      const PenaltyComponents m_domain;
      const PenaltyComponents m_centerShift;
      const PenaltyComponents m_skew;
      const PenaltyComponents m_bcaOverflow;
    };

    // =========================================================================
    // BCa-specific diagnostics
    // =========================================================================
    class BcaDiagnostics
    {
    public:
      BcaDiagnostics(double z0,
                     double accel,
                     bool   z0ExceedsHardLimit,
                     bool   accelExceedsHardLimit,
                     double rawLength)
        : m_available(true),
          m_z0(z0),
          m_accel(accel),
          m_z0ExceedsHardLimit(z0ExceedsHardLimit),
          m_accelExceedsHardLimit(accelExceedsHardLimit),
          m_rawLength(rawLength)
      {}

      static BcaDiagnostics notAvailable()
      {
        return BcaDiagnostics();
      }

      bool isAvailable() const { return m_available; }
      double getZ0() const { return m_z0; }
      double getAccel() const { return m_accel; }
      bool z0ExceedsHardLimit() const { return m_z0ExceedsHardLimit; }
      bool accelExceedsHardLimit() const { return m_accelExceedsHardLimit; }
      double getRawLength() const { return m_rawLength; }

    private:
      BcaDiagnostics()
        : m_available(false),
          m_z0(std::numeric_limits<double>::quiet_NaN()),
          m_accel(std::numeric_limits<double>::quiet_NaN()),
          m_z0ExceedsHardLimit(false),
          m_accelExceedsHardLimit(false),
          m_rawLength(std::numeric_limits<double>::quiet_NaN())
      {}

      bool   m_available;
      double m_z0;
      double m_accel;
      bool   m_z0ExceedsHardLimit;
      bool   m_accelExceedsHardLimit;
      double m_rawLength;
    };

    // =========================================================================
    // Percentile-T specific diagnostics
    // =========================================================================
    class PercentileTDiagnostics
    {
    public:
      /**
       * NOTE: innerFailCount may be an estimate depending on how you compute it.
       * Prefer logging innerFailRate as the authoritative metric.
       */
      PercentileTDiagnostics(std::size_t B_outer,
                             std::size_t B_inner,
                             std::size_t outerFailCount,
                             std::size_t innerFailCount,
                             double      innerFailRate,
                             double      effectiveB)
        : m_available(true),
          m_B_outer(B_outer),
          m_B_inner(B_inner),
          m_outerFailCount(outerFailCount),
          m_innerFailCount(innerFailCount),
          m_innerFailRate(innerFailRate),
          m_effectiveB(effectiveB)
      {
        if (m_outerFailCount > m_B_outer) {
          throw std::invalid_argument("PercentileTDiagnostics: outerFailCount > B_outer");
        }
      }

      static PercentileTDiagnostics notAvailable()
      {
        return PercentileTDiagnostics();
      }

      bool isAvailable() const { return m_available; }
      std::size_t getBOuter() const { return m_B_outer; }
      std::size_t getBInner() const { return m_B_inner; }
      std::size_t getOuterFailCount() const { return m_outerFailCount; }
      std::size_t getInnerFailCount() const { return m_innerFailCount; }
      double getInnerFailRate() const { return m_innerFailRate; }
      double getEffectiveB() const { return m_effectiveB; }

    private:
      PercentileTDiagnostics()
        : m_available(false),
          m_B_outer(0),
          m_B_inner(0),
          m_outerFailCount(0),
          m_innerFailCount(0),
          m_innerFailRate(std::numeric_limits<double>::quiet_NaN()),
          m_effectiveB(0.0)
      {}

      bool        m_available;
      std::size_t m_B_outer;
      std::size_t m_B_inner;
      std::size_t m_outerFailCount;
      std::size_t m_innerFailCount;
      double      m_innerFailRate;
      double      m_effectiveB;
    };

    // =========================================================================
    // Main Constructor
    // =========================================================================
    BootstrapDiagnosticRecord(TournamentContext         tournament,
                              CandidateIdentity         identity,
                              CandidateDistributionStats stats,
                              IntervalAndScore          interval,
                              RejectionInfo             rejection,
                              SupportValidation         support,
                              PenaltyBreakdown          penalties,
                              BcaDiagnostics            bca,
                              PercentileTDiagnostics    percentileT)
      : m_tournament(std::move(tournament)),
        m_identity(std::move(identity)),
        m_stats(std::move(stats)),
        m_interval(std::move(interval)),
        m_rejection(std::move(rejection)),
        m_support(std::move(support)),
        m_penalties(std::move(penalties)),
        m_bca(std::move(bca)),
        m_percentileT(std::move(percentileT))
    {}

    BootstrapDiagnosticRecord() = delete;

    // =========================================================================
    // Accessors - Return const references to nested classes
    // =========================================================================
    const TournamentContext& getTournament() const { return m_tournament; }
    const CandidateIdentity& getIdentity() const { return m_identity; }
    const CandidateDistributionStats& getStats() const { return m_stats; }
    const IntervalAndScore& getInterval() const { return m_interval; }
    const RejectionInfo& getRejection() const { return m_rejection; }
    const SupportValidation& getSupport() const { return m_support; }
    const PenaltyBreakdown& getPenalties() const { return m_penalties; }
    const BcaDiagnostics& getBca() const { return m_bca; }
    const PercentileTDiagnostics& getPercentileT() const { return m_percentileT; }

    // =========================================================================
    // Convenience accessors
    // =========================================================================
    std::uint64_t getRunID() const { return m_tournament.getRunID(); }
    std::uint64_t getStrategyUniqueId() const { return m_tournament.getStrategyUniqueId(); }
    std::uint64_t getCandidateID() const { return m_identity.getCandidateID(); }

    const std::string& getStrategyName() const { return m_tournament.getStrategyName(); }
    const std::string& getSymbol() const { return m_tournament.getSymbol(); }
    MetricType getMetricType() const { return m_tournament.getMetricType(); }

    const std::string& getMethodName() const { return m_identity.getMethodName(); }
    bool isChosen() const { return m_identity.isChosen(); }
    std::size_t getRank() const { return m_identity.getRank(); }

    double getScore() const { return m_interval.getFinalScore(); }
    double getLowerBound() const { return m_interval.getLowerBound(); }
    double getUpperBound() const { return m_interval.getUpperBound(); }

  private:
    const TournamentContext          m_tournament;
    const CandidateIdentity          m_identity;
    const CandidateDistributionStats m_stats;
    const IntervalAndScore           m_interval;
    const RejectionInfo              m_rejection;
    const SupportValidation          m_support;
    const PenaltyBreakdown           m_penalties;
    const BcaDiagnostics             m_bca;
    const PercentileTDiagnostics     m_percentileT;
  };

} // namespace palvalidator::diagnostics
