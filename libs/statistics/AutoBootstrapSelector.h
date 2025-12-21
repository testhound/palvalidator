#pragma once

#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <string>
#include <iostream>

#include "number.h"
#include "NormalDistribution.h"

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Encapsulates the result of the automatic confidence interval selection process.
     *
     * This class holds the selected method, the winning candidate object, and the list of
     * all candidates that were evaluated. It serves as the final report for the
     * AutoBootstrapSelector.
     */
    template <class Decimal>
    class AutoCIResult
    {
    public:
      /**
       * @brief Identifiers for the supported bootstrap methods.
       */
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
       * @brief Represents a single bootstrap method's calculation result and its quality metrics.
       *
       * A Candidate stores the calculated confidence interval (mean, lower, upper) along
       * with diagnostic statistics (standard error, skewness) and "penalty" scores used
       * by the selector to judge its quality.
       */
      class Candidate
      {
      public:
        /**
         * @brief Constructs a Candidate and computes derived stability metrics.
         *
         * @param method The bootstrap method identifier.
         * @param mean The bootstrap estimate of the statistic.
         * @param lower The lower bound of the confidence interval.
         * @param upper The upper bound of the confidence interval.
         * @param cl The confidence level (e.g., 0.95).
         * @param n The original sample size.
         * @param B_outer Number of outer loop resamples.
         * @param B_inner Number of inner loop resamples (for double bootstrap).
         * @param effective_B Number of valid (non-degenerate) resamples used.
         * @param skipped_total Number of resamples skipped due to errors/NaNs.
         * @param se_boot Standard error of the bootstrap distribution.
         * @param skew_boot Skewness of the bootstrap distribution.
         * @param center_shift_in_se Shift of the interval center relative to the mean, in units of SE.
         * @param normalized_length Ratio of interval length to an ideal percentile-based length.
         * @param ordering_penalty Penalty for misalignment with the raw bootstrap CDF (0.0 for advanced methods).
         * @param length_penalty Penalty for interval length deviating from the "ideal" band.
         * @param z0 Bias-correction parameter (BCa only).
         * @param accel Acceleration parameter (BCa only).
         * @param score Optional aggregate score for logging/diagnostics.
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
                  double      center_shift_in_se,
                  double      normalized_length,
                  double      ordering_penalty,
                  double      length_penalty,
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
            m_center_shift_in_se(center_shift_in_se),
            m_normalized_length(normalized_length),
            m_ordering_penalty(ordering_penalty),
            m_length_penalty(length_penalty),
            m_z0(z0),
            m_accel(accel),
            m_score(score),
            m_stability_penalty(0.0)
        {
          //
          // Calculate BCa-specific stability penalty.
          // BCa intervals can become geometrically unstable if the bias (z0) or
          // acceleration (a) parameters are too large.
          //
          // Thresholds:
          // |z0| > 0.5: Implies heavy bias correction.
          // |a|  > 0.1: Implies extreme skewness sensitivity (approaching singularity).
          //
          if (m_method == MethodId::BCa)
            {
              const double abs_z0 = std::fabs(m_z0);
              const double abs_a  = std::fabs(m_accel);

	      const double z0_excess =
		(abs_z0 > kBcaZ0SoftThreshold) ? (abs_z0 - kBcaZ0SoftThreshold) : 0.0;
	      const double a_excess  =
		(abs_a  > kBcaASoftThreshold)  ? (abs_a  - kBcaASoftThreshold)  : 0.0;

	       m_stability_penalty = z0_excess * z0_excess +
                a_excess  * a_excess;
            }
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
        double      getCenterShiftInSe() const { return m_center_shift_in_se; }
        double      getNormalizedLength() const { return m_normalized_length; }

        /**
         * @brief Returns the penalty for misalignment with the raw bootstrap CDF.
         *
         * For simple methods (Percentile, Normal), this penalizes intervals that don't
         * cover the target % of the raw bootstrap histogram.
         * For advanced methods (BCa, PercentileT), this is explicitly 0.0 because
         * they are designed to correct/shift the interval away from the raw CDF.
         */
        double      getOrderingPenalty() const { return m_ordering_penalty; }

        /**
         * @brief Returns the penalty for interval length deviations.
         * Penalizes intervals that are significantly wider or narrower than the
         * "ideal" length derived from the percentile interval.
         */
        double      getLengthPenalty() const { return m_length_penalty; }

        double      getZ0() const { return m_z0; }
        double      getAccel() const { return m_accel; }
        double      getScore() const { return m_score; }

        /**
         * @brief Returns the BCa stability penalty.
         * Derived from the magnitude of |z0| and |a|. Zero for non-BCa methods.
         */
        double      getStabilityPenalty() const { return m_stability_penalty; }

        /**
         * @brief Returns a copy of this Candidate with a new diagnostic score.
         * Used to update the aggregate score during selection without mutating the object.
         */
        Candidate withScore(double newScore) const
        {
          Candidate c(m_method, m_mean, m_lower, m_upper, m_cl,
                      m_n, m_B_outer, m_B_inner, m_effective_B, m_skipped_total,
                      m_se_boot, m_skew_boot,
                      m_center_shift_in_se, m_normalized_length,
                      m_ordering_penalty, m_length_penalty,
                      m_z0, m_accel, newScore);
          // Constructor recomputes m_stability_penalty identically.
          return c;
        }

      private:
	// ------------------------------------------------------------------
	// BCa stability "soft thresholds"
	// ------------------------------------------------------------------
	static constexpr double kBcaZ0SoftThreshold = 0.4;  // |z0| > 0.5 implies heavy bias correction
	static constexpr double kBcaASoftThreshold  = 0.1;  // |a|  > 0.1 implies extreme skewness sensitivity

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
        double      m_center_shift_in_se;
        double      m_normalized_length;
        double      m_ordering_penalty;
        double      m_length_penalty;
        double      m_z0;
        double      m_accel;
        double      m_score;
        double      m_stability_penalty;
      };

      /**
       * @brief Immutable diagnostics describing *why* a particular method was chosen.
       *
       * This is intended purely for logging / introspection; it does not affect selection.
       */
      class SelectionDiagnostics
      {
      public:
	/**
	 * @brief Immutable per-candidate breakdown of score components.
	 *
	 * This is intended for logging / introspection only. It does not affect selection.
	 */
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

	// Existing constructor (kept exactly for backwards compatibility)
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
	    m_num_candidates(numCandidates),
	    m_score_breakdowns()
	{}

	// New constructor overload that includes breakdowns
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
	std::size_t m_num_candidates;

	std::vector<ScoreBreakdown> m_score_breakdowns;
      };
    
      /**
       * @brief Result Constructor.
       * @param chosenMethod The MethodId of the selected best interval.
       * @param chosen The Candidate object representing the best interval.
       * @param candidates The full list of evaluated candidates (for logging/audit).
       * @param diagnostics Immutable description of why the selection was made.
       */
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

      // -- Getters --
      MethodId                        getChosenMethod() const { return m_chosen_method; }
      const Candidate&                getChosenCandidate() const { return m_chosen; }
      const std::vector<Candidate>&   getCandidates() const { return m_candidates; }
      const SelectionDiagnostics&     getDiagnostics() const { return m_diagnostics; }

      /**
       * @brief Human-readable name for a MethodId.
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
     * @brief Automatically selects the optimal bootstrap confidence interval method.
     *
     * This class implements a "Hierarchy of Trust" selection logic.
     * 1. It calculates standardized metrics (penalties) for each method.
     * 2. It prefers the BCa method if its parameters indicate stability.
     * 3. If BCa is unstable, it falls back to a tournament selection (Pareto dominance)
     * among robust alternatives (M-out-of-N, Percentile-T, etc.).
     */
    template <class Decimal>
    class AutoBootstrapSelector
    {
    public:
      using Result    = AutoCIResult<Decimal>;
      using MethodId  = typename Result::MethodId;
      using Candidate = typename Result::Candidate;
      using SelectionDiagnostics = typename Result::SelectionDiagnostics;

      // ScoringWeights: controls how different penalties are combined into a score.
      class ScoringWeights
      {
      public:
        ScoringWeights(double wCenterShift = 1.0,
                       double wSkew        = 0.5,
                       double wLength      = 0.25,
                       double wStability   = 1.0,
		       bool   enforcePos   = false)
          : m_w_center_shift(wCenterShift),
            m_w_skew(wSkew),
            m_w_length(wLength),
            m_w_stability(wStability),
	    m_enforce_positive(enforcePos)
        {}

        double getCenterShiftWeight() const { return m_w_center_shift; }
        double getSkewWeight() const { return m_w_skew; }
        double getLengthWeight() const { return m_w_length; }
        double getStabilityWeight() const { return m_w_stability; }
	bool enforcePositive() const { return m_enforce_positive; }

      private:
        double m_w_center_shift;
        double m_w_skew;
        double m_w_length;
        double m_w_stability;
	bool m_enforce_positive;
      };

      /**
       * @brief Computes the empirical CDF of a value x within a collection of statistics.
       * Used to check how much bootstrap mass lies below a given threshold.
       */
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

      /**
       * @brief Estimates a quantile from a sorted vector using linear interpolation.
       */
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

      /**
       * @brief Summarizes a simple percentile-like bootstrap engine into a Candidate.
       *
       * Applies to: Normal, Basic, Percentile, M-out-of-N.
       * These methods are penalized if their coverage (CDF width) does not match
       * the target confidence level (e.g., 95%).
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

	// Calculate skewness of the bootstrap distribution
	double skew_boot = 0.0;
	if (m > 2 && se_boot > 0.0)
	  {
	    double m3 = 0.0;
	    for (double v : stats)
	      {
		const double d = v - mean_boot;
		m3 += d * d * d;
	      }
	    m3 /= static_cast<double>(m);
	    skew_boot = m3 / (se_boot * se_boot * se_boot);
	  }

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

	//
	// Ordering Penalty:
	// For simple methods, we penalize deviations from the raw bootstrap CDF.
	// If the method claims 95% confidence, it should cover 95% of the bootstrap samples.
	//
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

	//
	// Length Penalty:
	// Compare the interval length to an "ideal" length derived from the
	// Percentile method (quantiles of the raw bootstrap distribution).
	//
	double length_penalty = 0.0;
	if (len > 0.0)
	  {
	    std::vector<double> sorted(stats.begin(), stats.end());
	    std::sort(sorted.begin(), sorted.end());

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

		// Soft band: penalize if length is < 0.8x or > a method-specific
		// upper bound. M-out-of-N intervals are naturally wider, so we use
		// a more generous upper limit for that method only.

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
			 center_shift_in_se,
			 normalized_length,
			 ordering_penalty,
			 length_penalty,
			 0.0,              // z0
			 0.0               // accel
			 );
      }
      
      // ------------------------------------------------------------------
      // Percentile-t engine summary
      // ------------------------------------------------------------------
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

	double skew_boot = 0.0;
	if (m > 2 && se_boot_calc > 0.0)
	  {
	    double m3 = 0.0;
	    for (double v : theta_stats)
	      {
		const double d = v - mean_boot;
		m3 += d * d * d;
	      }
	    m3 /= static_cast<double>(m);
	    skew_boot = m3 / (se_boot_calc * se_boot_calc * se_boot_calc);
	  }

	// Prefer the studentized SE estimate (se_hat), fallback to bootstrap SE
	double se_ref = res.se_hat;
	if (!(se_ref > 0.0))
	  se_ref = se_boot_calc;

	const double lo  = num::to_double(res.lower);
	const double hi  = num::to_double(res.upper);
	const double len = hi - lo;

	// For Percentile-T, re-centering away from the plain bootstrap mean is an
	// intended correction (via t-statistics). We therefore do NOT penalize
	// center shift for this method.
	double center_shift_in_se = 0.0;
	double normalized_length  = 1.0;

	//
	// IMPORTANT: Percentile-T corrects for skew/kurtosis by using t-statistics.
	// Its interval will NOT align with the raw bootstrap CDF and may be
	// re-centered away from the naive mean. We set ordering_penalty = 0.0
	// to avoid penalizing these intended corrections.
	//
	const double ordering_penalty = 0.0;

	//
	// Length Penalty:
	// Ensure the interval length is not absurdly large/small compared to the
	// percentile interval over theta*.
	//
	double length_penalty = 0.0;
	if (len > 0.0)
	  {
	    std::vector<double> sorted(theta_stats.begin(), theta_stats.end());
	    std::sort(sorted.begin(), sorted.end());

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
			 center_shift_in_se,
			 normalized_length,
			 ordering_penalty,
			 length_penalty,
			 0.0,
			 0.0
			 );
      }
      
      // ------------------------------------------------------------------
      // BCa engine summary
      // ------------------------------------------------------------------
      template <class BCaEngine>
      static Candidate summarizeBCa(const BCaEngine& bca)
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

	double skew_boot = 0.0;
	if (m > 2 && se_boot > 0.0)
	  {
	    double m3 = 0.0;
	    for (double v : stats)
	      {
		const double d = v - mean_boot;
		m3 += d * d * d;
	      }
	    m3 /= static_cast<double>(m);
	    skew_boot = m3 / (se_boot * se_boot * se_boot);
	  }

	const double lo  = num::to_double(lower);
	const double hi  = num::to_double(upper);
	const double len = hi - lo;

	// For BCa, bias correction (z0) and acceleration (a) explicitly shift and
	// warp the interval relative to the naive bootstrap mean. We do NOT
	// penalize this center shift directly; instead, we rely on the BCa-specific
	// stability penalty derived from |z0| and |a|.
	double center_shift_in_se = 0.0;
	double normalized_length  = 1.0;

	//
	// IMPORTANT: For BCa, we do NOT penalize coverage alignment or re-centering.
	// BCa corrects bias and skewness, meaning its interval is INTENDED to
	// deviate from the raw bootstrap CDF and from the naive mean. Penalizing
	// that via ordering or center shift would be incorrect; instead, we use a
	// stability penalty based on |z0| and |a|.
	//
	const double ordering_penalty = 0.0;

	//
	// Length Penalty:
	// Ensure the BCa interval is not wildly different from the percentile interval.
	// This acts as a sanity check against exploding parameters.
	//
	double length_penalty = 0.0;
	if (len > 0.0)
	  {
	    std::vector<double> sorted(stats.begin(), stats.end());
	    std::sort(sorted.begin(), sorted.end());

	    const double alpha   = 1.0 - cl;
	    const double alphaL  = 0.5 * alpha;
	    const double alphaU  = 1.0 - 0.5 * alpha;

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
			 center_shift_in_se,
			 normalized_length,
			 ordering_penalty,
			 length_penalty,
			 z0,
			 accel
			 );
      }
      
      // ------------------------------------------------------------------
      // Pairwise dominance logic
      // ------------------------------------------------------------------
      /**
       * @brief Checks if candidate 'a' Pareto-dominates candidate 'b'.
       *
       * Dominance means 'a' is better or equal in both Ordering and Length penalties,
       * and strictly better in at least one.
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
       * @brief Static rank preference for tie-breaking.
       * Used only when candidates are otherwise indistinguishable on penalties.
       * Preference Order: BCa > PercentileT > MOutOfN > Percentile > Basic > Normal
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
       * @brief Selects the best bootstrap interval from the provided candidates.
       *
       * ALGORITHM:
       * - Compute a unified scalar score for each candidate using:
       * score = ordering_penalty
       * + w_len   * length_penalty
       * + w_stab  * stability_penalty   (BCa only; 0 for others)
       * + w_center* center_shift_in_se^2
       * + w_skew  * skew_boot^2
       * - Choose the candidate with minimum score.
       * - If scores tie within epsilon, break ties using methodPreference
       * (BCa > PercentileT > MOutOfN > Percentile > Basic > Normal).
       *
       * ScoringWeights controls w_len, w_center, w_skew, and w_stab.
       */
      static Result select(const std::vector<Candidate>& candidates,
                           const ScoringWeights& weights = ScoringWeights())
      {
        if (candidates.empty())
          {
            throw std::invalid_argument("AutoBootstrapSelector::select: no candidates provided.");
          }

        // -------------------------------------------------------------------
        // Helpers
        // -------------------------------------------------------------------
        const auto relativeEpsilon = [](double a, double b) -> double
        {
          // Practical tolerance for a score that is a sum of a handful of doubles.
          const double scale = 1.0 + std::max(std::fabs(a), std::fabs(b));
          return kRelativeTieEpsilonScale * scale;
        };

        const auto scoresAreTied = [&](double a, double b) -> bool
        {
          return std::fabs(a - b) <= relativeEpsilon(a, b);
        };

        // -------------------------------------------------------------------
        // Phase 1: Compute raw components (no weights, no normalization)
        // -------------------------------------------------------------------
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

        bool   hasBCaCandidate       = false;
        double bestBCaStabPenalty    = std::numeric_limits<double>::infinity();
        double bestBCaLengthPenalty  = std::numeric_limits<double>::infinity();

        for (const auto& c : candidates)
          {
            double centerShiftSq = c.getCenterShiftInSe();
            centerShiftSq *= centerShiftSq;

            double skewSq = c.getSkewBoot();
            skewSq *= skewSq;

            const double baseOrdering = c.getOrderingPenalty();
            const double baseLength   = c.getLengthPenalty();
            const double stabPenalty  = c.getStabilityPenalty(); // 0 for non-BCa

            double domainPenalty = 0.0;
            if (enforcePos)
              {
                // If lower bound is <= 0, candidate is invalid for strictly-positive statistics.
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
                bestBCaStabPenalty   = std::min(bestBCaStabPenalty,   stabPenalty);
                bestBCaLengthPenalty = std::min(bestBCaLengthPenalty, baseLength);
              }
          }

        // -------------------------------------------------------------------
        // Phase 2: Normalize/cap components so weights represent "importance"
        // rather than compensating for different numeric ranges.
        //

        // We do NOT clamp at 1.0 because a penalty of 10x the reference
        // should be scored as 10x worse, not capped at "Standard Bad".
        const auto enforceNonNegative = [](double x) -> double
        {
          if (x < 0.0) return 0.0;
          return x;
        };
        
        // Constants defining "Bad":
        // If a method has this much error, its normalized penalty = 1.0.
    
        // 10% coverage error (e.g. 85% instead of 95%) is "Maximum Bad"
        const double kRefOrderingErrorSq = 0.10 * 0.10; 
    
        // Length deviation of 1.0 (e.g. double the ideal width) is "Maximum Bad"
        const double kRefLengthErrorSq   = 1.0 * 1.0;   
    
        // BCa Stability (z0=0.5, a=0.1) produces roughly 0.25 penalty in your formula
        const double kRefStability       = 0.25;        
    
        // Center shift of 2.0 Standard Errors is "Maximum Bad"
        const double kRefCenterShiftSq   = 2.0 * 2.0;   
        
        // Skewness of 2.0 (highly skewed) is "Maximum Bad"
        const double kRefSkewSq          = 2.0 * 2.0;

        // -------------------------------------------------------------------
        // Phase 3: Aggregate score + enrich candidates + capture breakdowns
        // -------------------------------------------------------------------
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

            //
            const double orderingNorm  = enforceNonNegative(r.getOrderingPenalty()  / kRefOrderingErrorSq);
            const double lengthNorm    = enforceNonNegative(r.getLengthPenalty()    / kRefLengthErrorSq);
            const double stabilityNorm = enforceNonNegative(r.getStabilityPenalty() / kRefStability);
            const double centerSqNorm  = enforceNonNegative(r.getCenterShiftSq()    / kRefCenterShiftSq);
            const double skewSqNorm    = enforceNonNegative(r.getSkewSq()           / kRefSkewSq);

            const double orderingContrib  = orderingNorm;              // implicit weight 1.0
            const double lengthContrib    = w_length * lengthNorm;
            const double stabilityContrib = w_stab   * stabilityNorm;
            const double centerSqContrib  = w_center * centerSqNorm;
            const double skewSqContrib    = w_skew   * skewSqNorm;
            const double domainContrib    = r.getDomainPenalty();      // intentionally absolute

            const double totalScore =
              orderingContrib +
              lengthContrib +
              stabilityContrib +
              centerSqContrib +
              skewSqContrib +
              domainContrib;

            breakdowns.emplace_back(
                                    c.getMethod(),
                                    /* raw */  r.getOrderingPenalty(), r.getLengthPenalty(), r.getStabilityPenalty(), r.getCenterShiftSq(), r.getSkewSq(), r.getDomainPenalty(),
                                    /* norm */ orderingNorm, lengthNorm, stabilityNorm, centerSqNorm, skewSqNorm,
                                    /* contrib */ orderingContrib, lengthContrib, stabilityContrib, centerSqContrib, skewSqContrib, domainContrib,
                                    /* total */ totalScore);

            enriched.push_back(c.withScore(totalScore));
          }

        // -------------------------------------------------------------------
        // Phase 4: Choose winner: minimum score, tie-broken by methodPreference
        // -------------------------------------------------------------------
        std::size_t chosenIdx = 0;
        double      bestScore = std::numeric_limits<double>::infinity();

        for (std::size_t i = 0; i < enriched.size(); ++i)
          {
            const double s = enriched[i].getScore();

            // Skip invalid scores (NaN/Inf) entirely
	    if (!std::isfinite(s))
              {
                // Optional: Print warning to stderr if this happens
                std::cerr << "[AutoBootstrapSelector] Warning: Method " 
                          << Result::methodIdToString(enriched[i].getMethod()) 
                          << " has non-finite score. Skipping.\n";
                continue;
              }

            // Special handling for the first valid score found.
            // If bestScore is infinite, we cannot use relativeEpsilon (it would be Inf).
            // We just accept this candidate as the current best.
            if (std::isinf(bestScore))
              {
                bestScore = s;
                chosenIdx = i;
                continue; 
              }

            if (!scoresAreTied(s, bestScore) && s < bestScore)
              {
                bestScore = s;
                chosenIdx = i;
              }
            else if (scoresAreTied(s, bestScore))
              {
                const int pBest = methodPreference(enriched[chosenIdx].getMethod());
                const int pCur  = methodPreference(enriched[i].getMethod());
                if (pCur < pBest)
                  {
                    bestScore = s;
                    chosenIdx = i;
                  }
              }
          }

        const Candidate& chosen = enriched[chosenIdx];

        // -------------------------------------------------------------------
        // Phase 5: Diagnostics: did BCa exist, and was it "rejected" on stability/length?
        // -------------------------------------------------------------------
        const bool bcaChosen = (chosen.getMethod() == MethodId::BCa);

        bool bcaRejectedForInstability = false;
        bool bcaRejectedForLength      = false;

        if (hasBCaCandidate && !bcaChosen)
          {
            bcaRejectedForInstability = (bestBCaStabPenalty   > kBcaStabilityThreshold);
            bcaRejectedForLength      = (bestBCaLengthPenalty > kBcaLengthPenaltyThreshold);
          }

        SelectionDiagnostics diagnostics(
                                         chosen.getMethod(),
                                         Result::methodIdToString(chosen.getMethod()),
                                         chosen.getScore(),
                                         chosen.getStabilityPenalty(),
                                         chosen.getLengthPenalty(),
                                         hasBCaCandidate,
                                         bcaChosen,
                                         bcaRejectedForInstability,
                                         bcaRejectedForLength,
                                         enriched.size(),
                                         std::move(breakdowns));

        return Result(chosen.getMethod(), chosen, enriched, diagnostics);
      }

    private:
      // ------------------------------------------------------------------
      // Selection & penalty policy constants
      // ------------------------------------------------------------------

      // Asymmetric coverage penalty multipliers (Percentile-like only)
      static constexpr double kUnderCoverageMultiplier = 2.0; // you chose 2.0 (mildly stricter on under-coverage)
      static constexpr double kOverCoverageMultiplier  = 1.0;

      // Length penalty "soft band"
      static constexpr double kLengthMin           = 0.8;
      static constexpr double kLengthMaxStandard   = 1.8;
      static constexpr double kLengthMaxMOutOfN    = 3.0;

      // Domain enforcement for strictly-positive statistics
      static constexpr double kPositiveLowerEpsilon = 1e-9;
      static constexpr double kDomainViolationPenalty = 1000.0;

      // BCa “rejection reason” diagnostics thresholds used in select()
      static constexpr double kBcaStabilityThreshold      = 0.01;
      static constexpr double kBcaLengthPenaltyThreshold  = 1.0;

      // Floating-point tie tolerance scale used in select()
      static constexpr double kRelativeTieEpsilonScale = 1e-10;
    };

  } // namespace analysis
} // namespace palvalidator
