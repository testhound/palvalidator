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
#include "NormalDistribution.h"

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Encapsulates the result of the automatic confidence interval selection process.
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

      class Candidate
      {
      public:
        // Refactored Constructor: Explicit stability_penalty argument
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

        Candidate withScore(double newScore) const
        {
          return Candidate(m_method, m_mean, m_lower, m_upper, m_cl,
                      m_n, m_B_outer, m_B_inner, m_effective_B, m_skipped_total,
                      m_se_boot, m_skew_boot, m_median_boot,
                      m_center_shift_in_se, m_normalized_length,
                      m_ordering_penalty, 
                      m_length_penalty,
                      m_stability_penalty, 
                      m_z0, m_accel, newScore);
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
        double      m_score;
      };

      class SelectionDiagnostics
      {
      public:
        class ScoreBreakdown
        {
        public:
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

        SelectionDiagnostics(MethodId     chosenMethod,
                             std::string  chosenMethodName,
                             double       chosenScore,
                             double       chosenStabilityPenalty,
                             double       chosenLengthPenalty,
                             bool         hasBCaCandidate,
                             bool         bcaChosen,
                             bool         bcaRejectedForInstability,
                             bool         bcaRejectedForLength,
                             std::size_t  numCandidates)
          : m_chosen_method(chosenMethod),
            m_chosen_method_name(std::move(chosenMethodName)),
            m_chosen_score(chosenScore),
            m_chosen_stability_penalty(chosenStabilityPenalty),
            m_chosen_length_penalty(chosenLengthPenalty),
            m_has_bca_candidate(hasBCaCandidate),
            m_bca_chosen(bcaChosen),
            m_bca_rejected_for_instability(bcaRejectedForInstability),
            m_bca_rejected_for_length(bcaRejectedForLength),
            m_bca_rejected_for_domain(false),
            m_bca_rejected_for_non_finite(false),
            m_num_candidates(numCandidates),
            m_score_breakdowns()
        {}

        SelectionDiagnostics(MethodId     chosenMethod,
                             std::string  chosenMethodName,
                             double       chosenScore,
                             double       chosenStabilityPenalty,
                             double       chosenLengthPenalty,
                             bool         hasBCaCandidate,
                             bool         bcaChosen,
                             bool         bcaRejectedForInstability,
                             bool         bcaRejectedForLength,
                             std::size_t  numCandidates,
                             std::vector<ScoreBreakdown> scoreBreakdowns)
          : m_chosen_method(chosenMethod),
            m_chosen_method_name(std::move(chosenMethodName)),
            m_chosen_score(chosenScore),
            m_chosen_stability_penalty(chosenStabilityPenalty),
            m_chosen_length_penalty(chosenLengthPenalty),
            m_has_bca_candidate(hasBCaCandidate),
            m_bca_chosen(bcaChosen),
            m_bca_rejected_for_instability(bcaRejectedForInstability),
            m_bca_rejected_for_length(bcaRejectedForLength),
            m_bca_rejected_for_domain(false),
            m_bca_rejected_for_non_finite(false),
            m_num_candidates(numCandidates),
            m_score_breakdowns(std::move(scoreBreakdowns))
        {}

        SelectionDiagnostics(MethodId     chosenMethod,
                             std::string  chosenMethodName,
                             double       chosenScore,
                             double       chosenStabilityPenalty,
                             double       chosenLengthPenalty,
                             bool         hasBCaCandidate,
                             bool         bcaChosen,
                             bool         bcaRejectedForInstability,
                             bool         bcaRejectedForLength,
                             bool         bcaRejectedForDomain,
                             bool         bcaRejectedForNonFinite,
                             std::size_t  numCandidates)
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
            m_score_breakdowns()
        {}

        SelectionDiagnostics(MethodId     chosenMethod,
                             std::string  chosenMethodName,
                             double       chosenScore,
                             double       chosenStabilityPenalty,
                             double       chosenLengthPenalty,
                             bool         hasBCaCandidate,
                             bool         bcaChosen,
                             bool         bcaRejectedForInstability,
                             bool         bcaRejectedForLength,
                             bool         bcaRejectedForDomain,
                             bool         bcaRejectedForNonFinite,
                             std::size_t  numCandidates,
                             std::vector<ScoreBreakdown> scoreBreakdowns)
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

      MethodId                        getChosenMethod() const { return m_chosen_method; }
      const Candidate&                getChosenCandidate() const { return m_chosen; }
      double                          getBootstrapMedian() const { return m_chosen.getMedianBoot(); }
      const std::vector<Candidate>&   getCandidates() const { return m_candidates; }
      const SelectionDiagnostics&     getDiagnostics() const { return m_diagnostics; }

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

    template <class Decimal>
    /**
     * @brief Automatically selects the optimal bootstrap confidence interval method.
     *
     * EMPIRICAL CALIBRATION:
     * Penalty thresholds validated on 1000+ strategies across:
     * - Stocks, bonds, commodities, international ETFs, leveraged ETFs
     * - BCa z0: median ~0.002, 90th percentile ~0.2, max=0.501
     * - BCa accel: median ~-0.007, 90th percentile ~0.05, max=0.118
     * - Soft threshold (0.25) set at ~85th percentile of |z0|
     * 
     * NOTE: May require re-tuning for crypto, HFT, or options strategies.
     */
    class AutoBootstrapSelector
    {
    public:
      using Result    = AutoCIResult<Decimal>;
      using MethodId  = typename Result::MethodId;
      using Candidate = typename Result::Candidate;
      using SelectionDiagnostics = typename Result::SelectionDiagnostics;

      class ScoringWeights
      {
      public:
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

      template <class Vec>
      static double empiricalCdf(const Vec& stats, double x)
      {
        std::size_t c = 0;
        for (double v : stats)
          {
            if (v <= x) ++c;
          }
        if (stats.empty()) return 0.0;
        return static_cast<double>(c) / static_cast<double>(stats.size());
      }

      static double quantileOnSorted(const std::vector<double>& sorted, double p)
      {
        if (sorted.empty()) return std::numeric_limits<double>::quiet_NaN();
        if (p <= 0.0) return sorted.front();
        if (p >= 1.0) return sorted.back();

        const double idx = p * static_cast<double>(sorted.size() - 1);
        const std::size_t i0 = static_cast<std::size_t>(std::floor(idx));
        const std::size_t i1 = static_cast<std::size_t>(std::ceil(idx));
        const double w = idx - static_cast<double>(i0);

        const double v0 = sorted[i0];
        const double v1 = sorted[i1];
        return v0 * (1.0 - w) + v1 * w;
      }

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

        // Compute skewness using centralized StatUtils (double overload)
        const double skew_boot = mkc_timeseries::StatUtils<double>::computeSkewness(stats, mean_boot, se_boot);

        const double mu  = num::to_double(res.mean);
        const double lo  = num::to_double(res.lower);
        const double hi  = num::to_double(res.upper);
        const double len = hi - lo;

        double center_shift_in_se = 0.0;
        double normalized_length  = 1.0;

        if (se_boot > 0.0 && len > 0.0)
          {
            const double center = 0.5 * (lo + hi);
            center_shift_in_se = std::fabs(center - mu) / se_boot;
          }

        const double F_lo = empiricalCdf(stats, lo);
        const double F_hi = empiricalCdf(stats, hi);
        const double width_cdf  = F_hi - F_lo;
        const double coverage_target = res.cl;

        const double coverage_error = width_cdf - coverage_target;

        const double under_coverage = (coverage_error < 0.0) ? -coverage_error : 0.0;
        const double over_coverage  = (coverage_error > 0.0) ?  coverage_error : 0.0;

        const double cov_pen =
          kUnderCoverageMultiplier * under_coverage * under_coverage +
          kOverCoverageMultiplier  * over_coverage  * over_coverage;

        const double F_mu       = empiricalCdf(stats, mu);
        const double center_cdf = 0.5 * (F_lo + F_hi);
        const double center_pen = (center_cdf - F_mu) *
          (center_cdf - F_mu);

        const double ordering_penalty = cov_pen + center_pen;

        double length_penalty = 0.0;
        double median_val = 0.0;
        if (len > 0.0)
          {
            std::vector<double> sorted(stats.begin(), stats.end());
            std::sort(sorted.begin(), sorted.end());

            // Compute median from already-sorted vector
            median_val = mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);

            const double alpha   = 1.0 - res.cl;
            const double alphaL  = 0.5 * alpha;
            const double alphaU  = 1.0 - 0.5 * alpha;

            const double qL = quantileOnSorted(sorted, alphaL);
            const double qU = quantileOnSorted(sorted, alphaU);
            const double ideal_len_boot = qU - qL;

            if (ideal_len_boot > 0.0)
              {
                const double norm_len = len / ideal_len_boot;
                normalized_length = norm_len;

                const double L_min = kLengthMin;
                const double L_max = (method == MethodId::MOutOfN ? kLengthMaxMOutOfN : kLengthMaxStandard);

                if (norm_len < L_min)
                  {
                    const double d = L_min - norm_len;
                    length_penalty = d * d;
                  }
                else if (norm_len > L_max)
                  {
                    const double d = norm_len - L_max;
                    length_penalty = d * d;
                  }
                else
                  {
                    length_penalty = 0.0;
                  }
              }
          }

        if (len <= 0.0)
        {
          // No sorting path taken above; compute median from unsorted stats
          median_val = mkc_timeseries::StatUtils<double>::computeMedian(stats);
        }

        return Candidate(
                          method,
                          res.mean,
                          res.lower,
                          res.upper,
                          res.cl,
                         res.n,
                         res.B,            // B_outer
                         0,                // B_inner
                         res.effective_B,
                         res.skipped,      // skipped_total
                           se_boot,
                           skew_boot,
                           median_val,
                           /* center_shift_in_se follows */
                           center_shift_in_se,
                           normalized_length,
                           ordering_penalty,
                           length_penalty,
                           0.0,              // stability_penalty (Not applicable for Percentile-like)
                           0.0,              // z0
                           0.0               // accel
                           );
      }
      
      template <class PTBootstrap>
      static Candidate summarizePercentileT(
                                            const PTBootstrap&                  engine,
                                            const typename PTBootstrap::Result& res)
      {
        if (!engine.hasDiagnostics())
          {
            throw std::logic_error(
                                   "AutoBootstrapSelector: percentile-t diagnostics not available (run() not called?).");
          }

        const auto& theta_stats = engine.getThetaStarStatistics();
        const std::size_t m = theta_stats.size();
        if (m < 2)
          {
            throw std::logic_error(
                                   "AutoBootstrapSelector: need at least 2 theta* statistics for percentile-t.");
          }

        double sum = 0.0;
        for (double v : theta_stats) sum += v;
        const double mean_boot = sum / static_cast<double>(m);

        double var_boot = 0.0;
        for (double v : theta_stats)
          {
            const double d = v - mean_boot;
            var_boot += d * d;
          }
        if (m > 1)
          {
            var_boot /= static_cast<double>(m - 1);
          }
        const double se_boot_calc = std::sqrt(std::max(0.0, var_boot));

        // Compute skewness using centralized StatUtils and prepare median placeholder
        const double skew_boot = mkc_timeseries::StatUtils<double>::computeSkewness(theta_stats, mean_boot, se_boot_calc);
        double median_boot = 0.0;

        double se_ref = res.se_hat;
        if (!(se_ref > 0.0))
          se_ref = se_boot_calc;

        const double lo  = num::to_double(res.lower);
        const double hi  = num::to_double(res.upper);
        const double len = hi - lo;

        double center_shift_in_se = 0.0;
        double normalized_length  = 1.0;

        const double ordering_penalty = 0.0;

        double length_penalty = 0.0;
        if (len > 0.0)
          {
            std::vector<double> sorted(theta_stats.begin(), theta_stats.end());
            std::sort(sorted.begin(), sorted.end());

            // Compute median from already-sorted vector to avoid extra work
            median_boot = mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);

            const double alpha   = 1.0 - res.cl;
            const double alphaL  = 0.5 * alpha;
            const double alphaU  = 1.0 - 0.5 * alpha;

            const double qL = quantileOnSorted(sorted, alphaL);
            const double qU = quantileOnSorted(sorted, alphaU);
            const double ideal_len_boot = qU - qL;

            if (ideal_len_boot > 0.0)
              {
                const double norm_len = len / ideal_len_boot;
                normalized_length = norm_len;

                const double L_min = kLengthMin;
                const double L_max = kLengthMaxStandard;

                if (norm_len < L_min)
                  {
                    const double d = L_min - norm_len;
                    length_penalty = d * d;
                  }
                else if (norm_len > L_max)
                  {
                    const double d = norm_len - L_max;
                    length_penalty = d * d;
                  }
                else
                  {
                    length_penalty = 0.0;
                  }
              }
          }

        if (len <= 0.0)
        {
          // No sorting path taken above; compute median from unsorted theta_stats
          median_boot = mkc_timeseries::StatUtils<double>::computeMedian(theta_stats);
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
                         0.0,              // stability_penalty
                         0.0,
                         0.0
                         );
      }
      
      // ------------------------------------------------------------------
      // BCa engine summary (Enhanced with strict stability checks)
      // ------------------------------------------------------------------
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

        // Convert to doubles for diagnostics/selection metrics
        std::vector<double> stats;
        stats.reserve(statsD.size());
        for (const auto& d : statsD)
          {
            stats.push_back(num::to_double(d));
          }

        const std::size_t m = stats.size();

        double sum = 0.0;
        for (double v : stats) sum += v;
        const double mean_boot = sum / static_cast<double>(m);

        double var_boot = 0.0;
        for (double v : stats)
          {
            const double d = v - mean_boot;
            var_boot += d * d;
          }
        if (m > 1)
          {
            var_boot /= static_cast<double>(m - 1);
          }
        const double se_boot = std::sqrt(std::max(0.0, var_boot));

        // Compute skewness using centralized StatUtils (double overload)
        const double skew_boot = mkc_timeseries::StatUtils<double>::computeSkewness(stats, mean_boot, se_boot);
        // Compute median placeholder; may be computed from sorted vector below
        double median_boot = 0.0;

        const double lo  = num::to_double(lower);
        const double hi  = num::to_double(upper);
        const double len = hi - lo;

        // BCa explicitly shifts the interval to correct for bias. We do NOT
        // penalize this "center shift" directly via standard metrics; instead, 
        // we use the specific stability penalty below.
        double center_shift_in_se = 0.0;
        double normalized_length  = 1.0;
        
        //
        // 1. Length Penalty:
        // Ensure the BCa interval is not wildly different from the percentile interval.
        // This acts as a sanity check against exploding parameters.
        //
        double length_penalty = 0.0;
        if (len > 0.0)
          {
            std::vector<double> sorted(stats.begin(), stats.end());
            std::sort(sorted.begin(), sorted.end());

            // ============================================================
            // FIX: Compute median from already-sorted vector
            // ============================================================
            median_boot = mkc_timeseries::StatUtils<double>::computeMedianSorted(sorted);

            const double alpha    = 1.0 - cl;
            const double alphaL   = 0.5 * alpha;
            const double alphaU   = 1.0 - 0.5 * alpha;

            const double qL = quantileOnSorted(sorted, alphaL);
            const double qU = quantileOnSorted(sorted, alphaU);
            const double ideal_len_boot = qU - qL;

            if (ideal_len_boot > 0.0)
              {
                const double norm_len = len / ideal_len_boot;
                normalized_length = norm_len;

                const double L_min = 0.8;
                const double L_max = 1.8;

                if (norm_len < L_min)
                  {
                    const double d = L_min - norm_len;
                    length_penalty = d * d;
                  }
                else if (norm_len > L_max)
                  {
                    const double d = norm_len - L_max;
                    length_penalty = d * d;
                  }
                else
                  {
                    length_penalty = 0.0;
                  }
              }
          }

        //
        // 2. Stability Penalty (Updated with stricter safety checks):
        // We penalize the BCa method if its internal parameters (bias z0, acceleration a)
        // exceed safe statistical thresholds. This prevents using BCa when the 
        // approximation is breaking down.
        //
        double stability_penalty = 0.0;

        // A. Bias (z0) Check
        // Use class-level soft thresholds and configurable scales from weights.
        const double Z0_THRESHOLD = kBcaZ0SoftThreshold;

	// Adaptive acceleration threshold based on distribution skewness.
	// When |skew| > 3.0, the BCa approximation becomes less reliable
	// due to higher-order terms in the Taylor expansion. We tighten
	// the threshold from 0.10 → 0.08 to catch instability earlier.
	// Empirically validated: max observed skew=6.56 with |a|=0.118.
	const double base_accel_threshold = kBcaASoftThreshold;  // 0.10
	const double ACCEL_THRESHOLD = (std::abs(skew_boot) > 3.0) 
                               ? 0.08   // Stricter when skew > 3.0
                               : base_accel_threshold;

        // Allow penalty scales to be overridden via ScoringWeights (and adapt by skewness)
        const double base_z0_scale = weights.getBcaZ0Scale();
        const double base_a_scale  = weights.getBcaAScale();

        const double skew_multiplier = (std::abs(skew_boot) > 2.0) ? 1.5 : 1.0;
        const double Z0_SCALE    = base_z0_scale * skew_multiplier;
        const double ACCEL_SCALE = base_a_scale  * skew_multiplier;

	if (std::abs(skew_boot) > 2.0 || std::abs(skew_boot) > 3.0)
	  {
	    if (os)
	      {
	        (*os) << "[BCa DEBUG] High skew detected:\n"
	             << "  skew_boot=" << skew_boot << "\n"
	             << "  skew_multiplier=" << skew_multiplier << "\n"
	             << "  ACCEL_THRESHOLD=" << ACCEL_THRESHOLD << "\n"
	             << "  Z0_SCALE=" << Z0_SCALE << "\n"
	             << "  ACCEL_SCALE=" << ACCEL_SCALE << "\n"
	             << "  z0=" << z0 << " accel=" << accel << "\n";
	      }
	  }

        const double z0_abs       = std::abs(z0);
        if (z0_abs > Z0_THRESHOLD)
        {
            const double diff = z0_abs - Z0_THRESHOLD;
            stability_penalty += (diff * diff) * Z0_SCALE;
        }

        // B. Acceleration (a) Check
        const double accel_abs       = std::abs(accel);
        if (accel_abs > ACCEL_THRESHOLD)
        {
            const double diff = accel_abs - ACCEL_THRESHOLD;
            stability_penalty += (diff * diff) * ACCEL_SCALE;
        }

        // 3. Finite Check
        if (!std::isfinite(z0) || !std::isfinite(accel))
        {
            stability_penalty = std::numeric_limits<double>::infinity();
        }

	const double SKEW_THRESHOLD = kBcaSkewThreshold;   // Beyond this, BCa approximation strains
	const double SKEW_PENALTY_SCALE = kBcaSkewPenaltyScale;  // Aggressive scaling

	// ============================================================
	// NEW: Skewness penalty for BCa
	// ============================================================
	// BCa's Taylor expansion breaks down when the bootstrap distribution
	// is heavily skewed, even if z0 and accel remain within bounds.
	// We penalize BCa directly for high |skew_boot| to force fallback
	// to PercentileT (which is designed for skewed distributions).
	if (std::abs(skew_boot) > SKEW_THRESHOLD)
	  {
	    const double skew_excess = std::abs(skew_boot) - SKEW_THRESHOLD;
	    const double skew_penalty = skew_excess * skew_excess * SKEW_PENALTY_SCALE;
	    stability_penalty += skew_penalty;
    
	    // Optional: Log when this triggers
	    if (os && (skew_penalty > 0.1))
	      {  // Only log significant penalties
		(*os) << "[BCa] Skew penalty applied: skew_boot=" << skew_boot
			  << " penalty=" << skew_penalty 
			  << " total_stab=" << stability_penalty << "\n";
	      }
	  }

        // BCa does not use ordering penalty, pass 0.0 for that slot.
        const double ordering_penalty = 0.0;

        // If we already sorted above we may have computed median_boot; otherwise compute now
        if (len <= 0.0)
        {
          median_boot = mkc_timeseries::StatUtils<double>::computeMedian(stats);
        }

        return Candidate(
                         MethodId::BCa,
                          mean,
                          lower,
                          upper,
                          cl,
                          n,
                          B,
                          0,
                          m,
                          (B > m) ? (B - m) : 0,
                          se_boot,
                          skew_boot,
                          median_boot,
                          center_shift_in_se,
                          normalized_length,
                          ordering_penalty, 
                          length_penalty,
                          stability_penalty, // Explicitly passed
                          z0,
                          accel
                          );
      }

      static bool dominates(const Candidate& a, const Candidate& b)
      {
        const bool better_or_equal_order  = a.getOrderingPenalty() <= b.getOrderingPenalty();
        const bool better_or_equal_length = a.getLengthPenalty()   <= b.getLengthPenalty();
        const bool strictly_better =
          (a.getOrderingPenalty() < b.getOrderingPenalty()) ||
          (a.getLengthPenalty()   < b.getLengthPenalty());

        return better_or_equal_order && better_or_equal_length && strictly_better;
      }

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

      static Result select(const std::vector<Candidate>& candidates,
                           const ScoringWeights& weights = ScoringWeights())
      {
        if (candidates.empty())
          {
            throw std::invalid_argument("AutoBootstrapSelector::select: no candidates provided.");
          }

        const auto relativeEpsilon = [](double a, double b) -> double
        {
          const double scale = 1.0 + std::max(std::fabs(a), std::fabs(b));
          return kRelativeTieEpsilonScale * scale;
        };

        const auto scoresAreTied = [&](double a, double b) -> bool
        {
          return std::fabs(a - b) <= relativeEpsilon(a, b);
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

        const bool enforcePos = weights.enforcePositive();

        std::vector<RawComponents> raw;
        raw.reserve(candidates.size());

        bool hasBCaCandidate = false;

        bool bcaRejectedForInstability = false;
        bool bcaRejectedForLength      = false;
        bool bcaRejectedForDomain      = false;
        bool bcaRejectedForNonFinite   = false;

        for (const auto& c : candidates)
          {
            double centerShiftSq = c.getCenterShiftInSe();
            centerShiftSq *= centerShiftSq;

            double skewSq = c.getSkewBoot();
            skewSq *= skewSq;

            const double baseOrdering = c.getOrderingPenalty();
            const double baseLength   = c.getLengthPenalty();
            const double stabPenalty  = c.getStabilityPenalty(); 

            double domainPenalty = 0.0;
            if (enforcePos)
              {
                if (num::to_double(c.getLower()) <= kPositiveLowerEpsilon)
                  {
                    domainPenalty = kDomainViolationPenalty;
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

            const double orderingContrib  = orderingNorm; // implicit weight 1.0
            const double lengthContrib    = w_length * lengthNorm;
            const double stabilityContrib = w_stab   * stabilityNorm;
            const double centerSqContrib  = w_center * centerSqNorm;
            const double skewSqContrib    = w_skew   * skewSqNorm;
            const double domainContrib    = r.getDomainPenalty();

            const double totalScore =
              orderingContrib +
              lengthContrib +
              stabilityContrib +
              centerSqContrib +
              skewSqContrib +
              domainContrib;

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

        const auto commonCandidateOk = [&](std::size_t i) -> bool
        {
          if (!std::isfinite(enriched[i].getScore()))
            return false;
          if (enforcePos && raw[i].getDomainPenalty() > 0.0)
            return false;

          return true;
        };

        // BCa hard gate checks
        const auto bcaCandidateOk = [&](std::size_t i) -> bool
        {
          if (!commonCandidateOk(i)) return false;

          const Candidate& c = enriched[i];

          if (!std::isfinite(c.getZ0()) || !std::isfinite(c.getAccel()))
            return false;

          if (std::fabs(c.getZ0()) > kBcaZ0HardLimit)
            return false;

          if (std::fabs(c.getAccel()) > kBcaAHardLimit)
            return false;

          if (c.getLengthPenalty() > kBcaLengthPenaltyThreshold)
            return false;

          return true;
        };

        std::optional<std::size_t> chosenIdxOpt;

        // -------------------------------------------------------------------
        // UPDATED SELECTION LOGIC: Score-Based Tournament with BCa Preference
        // -------------------------------------------------------------------
        
        // 1. Identify valid candidates (BCa must pass hard gates; others pass common)
        bool foundAny = false;
        double bestScore = std::numeric_limits<double>::infinity();

        for (std::size_t i = 0; i < enriched.size(); ++i)
        {
            const MethodId m = enriched[i].getMethod();
            bool isOk = false;

            if (m == MethodId::BCa) {
                isOk = bcaCandidateOk(i);
            } else {
                isOk = commonCandidateOk(i);
            }

            if (!isOk) continue;

            const double s = enriched[i].getScore();

            if (!foundAny)
            {
                foundAny = true;
                bestScore = s;
                chosenIdxOpt = i;
                continue;
            }

            if (!scoresAreTied(s, bestScore) && s < bestScore)
            {
                bestScore = s;
                chosenIdxOpt = i;
            }
            else if (scoresAreTied(s, bestScore))
            {
                // If scores are tied, use method preference (BCa > PercentileT > ...)
                const int pBest = methodPreference(enriched[*chosenIdxOpt].getMethod());
                const int pCur  = methodPreference(enriched[i].getMethod());
                if (pCur < pBest)
                {
                    bestScore = s; // Technically same score, but we switch index
                    chosenIdxOpt = i;
                }
            }
        }

        // 2. Diagnostics: If BCa existed but wasn't chosen, record why.
        if (hasBCaCandidate && chosenIdxOpt)
        {
            const Candidate& winner = enriched[*chosenIdxOpt];
            
            // If winner is NOT BCa, check if BCa was rejected for hard limits 
            // or simply lost the tournament.
            if (winner.getMethod() != MethodId::BCa)
            {
                for (std::size_t i = 0; i < enriched.size(); ++i)
                {
                    if (enriched[i].getMethod() != MethodId::BCa) continue;

                    // Check Hard Gates
                    if (!std::isfinite(enriched[i].getScore())) { bcaRejectedForNonFinite = true; }
                    if (enforcePos && raw[i].getDomainPenalty() > 0.0) { bcaRejectedForDomain = true; }

                    if (!std::isfinite(enriched[i].getZ0()) || !std::isfinite(enriched[i].getAccel()))
                    {
                        bcaRejectedForInstability = true;
                    }
                    else
                    {
                        if (std::fabs(enriched[i].getZ0()) > kBcaZ0HardLimit) { bcaRejectedForInstability = true; }
                        if (std::fabs(enriched[i].getAccel()) > kBcaAHardLimit) { bcaRejectedForInstability = true; }
                    }

                    if (enriched[i].getLengthPenalty() > kBcaLengthPenaltyThreshold) { bcaRejectedForLength = true; }

                    // If it passed hard gates but still lost, it implies it lost on Score.
                    break;
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
                             chosen.getStabilityPenalty(), // soft stability penalty still shown
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
      // ------------------------------------------------------------------
      // Selection & penalty policy constants
      // ------------------------------------------------------------------

      // Asymmetric coverage penalty multipliers (Percentile-like only)
      static constexpr double kUnderCoverageMultiplier = 2.0; 
      static constexpr double kOverCoverageMultiplier  = 1.0;

      // Length penalty "soft band"
      static constexpr double kLengthMin           = 0.8;
      static constexpr double kLengthMaxStandard   = 1.8;
      static constexpr double kLengthMaxMOutOfN    = 6.0;

      // Domain enforcement for strictly-positive statistics
      static constexpr double kPositiveLowerEpsilon = 1e-9;
      static constexpr double kDomainViolationPenalty = 1000.0;

       // BCa “rejection reason” diagnostics thresholds used in select()

       // Hard limits -- relaxed slightly to add safety headroom (see code review)
       static constexpr double kBcaZ0HardLimit = 0.6;   // relaxed from 0.5 -> 0.6
       static constexpr double kBcaAHardLimit  = 0.25;  // relaxed from 0.2 -> 0.25

       // Soft thresholds: beyond these values soft penalties start to apply
       static constexpr double kBcaZ0SoftThreshold = 0.25;
       static constexpr double kBcaASoftThreshold  = 0.10;

       // Penalty scaling defaults (can be overridden via ScoringWeights)
       static constexpr double kBcaZ0PenaltyScale = 20.0;
       static constexpr double kBcaAPenaltyScale  = 100.0;

       // Calculate the penalty threshold dynamically based on the hard limit and
       // the soft-threshold. Threshold = (HardLimit - SoftThreshold)^2
       static constexpr double kBcaStabilityThreshold =
         (kBcaZ0HardLimit - kBcaZ0SoftThreshold) * (kBcaZ0HardLimit - kBcaZ0SoftThreshold);

       static constexpr double kBcaLengthPenaltyThreshold  = 1.0;

       // Floating-point tie tolerance scale used in select()
       static constexpr double kRelativeTieEpsilonScale = 1e-10;
      static constexpr double kBcaSkewThreshold = 2.0;    // Start penalizing beyond this
      static constexpr double kBcaSkewPenaltyScale = 5.0; // Quadratic scaling factor
    };

  } // namespace analysis
} // namespace palvalidator
