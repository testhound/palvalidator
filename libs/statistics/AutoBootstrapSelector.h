#pragma once

#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <algorithm>

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

                // Soft thresholds; empirically tuned for financial time series.
                const double z0_ok = 0.5;
                const double a_ok  = 0.1;

                const double z0_excess =
                  (abs_z0 > z0_ok) ? (abs_z0 - z0_ok) : 0.0;
                const double a_excess  =
                  (abs_a  > a_ok)  ? (abs_a  - a_ok)  : 0.0;

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
         * @brief Result Constructor.
         * @param chosenMethod The MethodId of the selected best interval.
         * @param chosen The Candidate object representing the best interval.
         * @param candidates The full list of evaluated candidates (for logging/audit).
         */
        AutoCIResult(MethodId chosenMethod,
                     Candidate chosen,
                     std::vector<Candidate> candidates)
          : m_chosen_method(chosenMethod),
            m_chosen(std::move(chosen)),
            m_candidates(std::move(candidates))
        {
        }

        // -- Getters --
        MethodId                        getChosenMethod() const { return m_chosen_method; }
        const Candidate&                getChosenCandidate() const { return m_chosen; }
        const std::vector<Candidate>&   getCandidates() const { return m_candidates; }

      private:
        MethodId               m_chosen_method;
        Candidate              m_chosen;
        std::vector<Candidate> m_candidates;
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

        // Kept for backward compatibility; not used in selection anymore.
        class ScoringWeights
        {
        public:
          ScoringWeights(double wCenterShift = 1.0,
                         double wSkew        = 0.5,
                         double wLength      = 0.25)
            : m_w_center_shift(wCenterShift),
              m_w_skew(wSkew),
              m_w_length(wLength)
          {}

          double getCenterShiftWeight() const { return m_w_center_shift; }
          double getSkewWeight() const { return m_w_skew; }
          double getLengthWeight() const { return m_w_length; }

        private:
          double m_w_center_shift;
          double m_w_skew;
          double m_w_length;
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

            const double cov_pen = (width_cdf - coverage_target) *
                                  (width_cdf - coverage_target);

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

                    // Soft band: penalize if length is < 0.8x or > 1.8x the ideal length.
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

            const double mu  = num::to_double(res.mean);
            const double lo  = num::to_double(res.lower);
            const double hi  = num::to_double(res.upper);
            const double len = hi - lo;

            double center_shift_in_se = 0.0;
            double normalized_length  = 1.0;

            if (se_ref > 0.0 && len > 0.0)
              {
                const double center = 0.5 * (lo + hi);
                center_shift_in_se = std::fabs(center - mu) / se_ref;
              }

            //
            // IMPORTANT: Percentile-T corrects for skew/kurtosis by using t-statistics.
            // Its interval will NOT align with the raw bootstrap CDF.
            // Therefore, we set ordering_penalty = 0.0 to avoid penalizing its valid corrections.
            //
            const double ordering_penalty = 0.0;

            //
            // Length Penalty:
            // Ensure the interval length is not absurdly large/small compared to the percentile interval.
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

            const double z0       = bca.getZ0();
            const Decimal accelD  = bca.getAcceleration();
            const double accel    = accelD.getAsDouble();

            const auto& statsD = bca.getBootstrapStatistics();
            if (statsD.size() < 2)
              {
                throw std::logic_error(
                  "AutoBootstrapSelector: need at least 2 bootstrap stats for BCa engine.");
              }

            // Convert to doubles for diagnostics
            std::vector<double> stats;
            stats.reserve(statsD.size());
            for (const auto& d : statsD)
              stats.push_back(d.getAsDouble());

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

            const double mu  = num::to_double(mean);
            const double lo  = num::to_double(lower);
            const double hi  = num::to_double(upper);
            const double len = hi - lo;

            double center_shift_in_se = 0.0;
            double normalized_length  = 1.0;

            if (se_boot > 0.0 && len > 0.0)
              {
                const double center = 0.5 * (lo + hi);
                center_shift_in_se = std::fabs(center - mu) / se_boot;
              }

            //
            // IMPORTANT: For BCa, we do NOT penalize coverage alignment.
            // BCa corrects bias and skewness, meaning its interval is INTENDED to
            // deviate from the raw bootstrap CDF. Penalizing it would be incorrect.
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

        // ------------------------------------------------------------------
        // Selection Logic
        // ------------------------------------------------------------------
        /**
         * @brief Selects the best bootstrap interval from the provided candidates.
         *
         * ALGORITHM:
         * 1. Hierarchy of Trust:
         * - If BCa is present, check its stability (z0, a) and length.
         * - If BCa is stable, return it immediately (it is theoretically superior).
         * 2. Fallback Tournament:
         * - If BCa is unstable (or absent), filter it out.
         * - Compare remaining candidates using Pareto dominance on (OrderingPenalty, LengthPenalty).
         * - Select the non-dominated candidate with the best scores, using method preference as a tie-breaker.
         */
        static Result select(const std::vector<Candidate>& candidates,
                             const ScoringWeights& = ScoringWeights())
        {
          if (candidates.empty())
            {
              throw std::invalid_argument("AutoBootstrapSelector::select: no candidates provided.");
            }

          // 1. Enrich candidates with a simple aggregate score for logging/diagnostics.
          std::vector<Candidate> enriched;
          enriched.reserve(candidates.size());
          for (const auto& c : candidates)
            {
              const double s = c.getOrderingPenalty() + c.getLengthPenalty();
              enriched.push_back(c.withScore(s));
            }

          //
          // 2. BCa-first hierarchy check.
          //    We prefer BCa if it is structurally stable (low stability penalty)
          //    and produces a plausible interval length.
          //
          const double BCa_STABILITY_THRESHOLD       = 0.1;
          const double BCa_LENGTH_PENALTY_THRESHOLD  = 1.0;

          int bestBcaIdx = -1;
          for (std::size_t i = 0; i < enriched.size(); ++i)
            {
              if (enriched[i].getMethod() != MethodId::BCa)
                continue;

              if (bestBcaIdx < 0)
                {
                  bestBcaIdx = static_cast<int>(i);
                }
              else
                {
                  // Find the BCa candidate with the lowest stability penalty.
                  const auto& cBest = enriched[bestBcaIdx];
                  const auto& cCur  = enriched[i];

                  const double stabBest = cBest.getStabilityPenalty();
                  const double stabCur  = cCur.getStabilityPenalty();

                  if (stabCur < stabBest - 1e-15)
                    {
                      bestBcaIdx = static_cast<int>(i);
                      continue;
                    }
                  // Tie-break on length penalty
                  if (std::fabs(stabCur - stabBest) <= 1e-15)
                    {
                      const double lBest = cBest.getLengthPenalty();
                      const double lCur  = cCur.getLengthPenalty();
                      if (lCur < lBest - 1e-15)
                        {
                          bestBcaIdx = static_cast<int>(i);
                          continue;
                        }
                    }
                }
            }

          if (bestBcaIdx >= 0)
            {
              const auto& bestBCa = enriched[bestBcaIdx];
              const double stab   = bestBCa.getStabilityPenalty();
              const double lpen   = bestBCa.getLengthPenalty();

              if (stab <= BCa_STABILITY_THRESHOLD &&
                  lpen <= BCa_LENGTH_PENALTY_THRESHOLD)
                {
                  // TRUST: BCa is stable and valid. Return it.
                  return Result(bestBCa.getMethod(), bestBCa, enriched);
                }
              // Else: BCa exists but is unstable. Fall through to robust methods.
            }

          //
          // 3. Fallback Tournament:
          //    Filter out BCa (since it failed the trust check) and select the best
          //    remaining method using Pareto dominance.
          //
          std::vector<Candidate> filtered;
          filtered.reserve(enriched.size());
          for (const auto& c : enriched)
            {
              if (c.getMethod() != MethodId::BCa)
                filtered.push_back(c);
            }

          if (filtered.empty())
            {
              // Edge case: Only BCa candidates existed, and all failed checks.
              // Fall back to the "least bad" BCa.
              const auto& bestBCa = enriched[bestBcaIdx];
              return Result(bestBCa.getMethod(), bestBCa, enriched);
            }

          // Compute Pareto dominance
          const std::size_t K = filtered.size();
          std::vector<bool> isDominated(K, false);

          for (std::size_t i = 0; i < K; ++i)
            {
              if (isDominated[i]) continue;
              for (std::size_t j = 0; j < K; ++j)
                {
                  if (i == j || isDominated[j]) continue;
                  if (dominates(filtered[j], filtered[i]))
                    {
                      isDominated[i] = true;
                      break;
                    }
                }
            }

          // Identify the Pareto Frontier (non-dominated candidates)
          std::vector<std::size_t> frontier;
          for (std::size_t i = 0; i < K; ++i)
            {
              if (!isDominated[i])
                frontier.push_back(i);
            }

          // Select winner from frontier:
          // Priority: Ordering Penalty -> Length Penalty -> Method Preference
          std::size_t bestIdx = frontier[0];
          for (std::size_t idx : frontier)
            {
              const auto& cBest = filtered[bestIdx];
              const auto& cCur  = filtered[idx];

              const double oBest = cBest.getOrderingPenalty();
              const double oCur  = cCur.getOrderingPenalty();

              if (oCur < oBest - 1e-15)
                {
                  bestIdx = idx;
                  continue;
                }
              if (std::fabs(oCur - oBest) <= 1e-15)
                {
                  const double lBest = cBest.getLengthPenalty();
                  const double lCur  = cCur.getLengthPenalty();
                  if (lCur < lBest - 1e-15)
                    {
                      bestIdx = idx;
                      continue;
                    }
                  if (std::fabs(lCur - lBest) <= 1e-15)
                    {
                      const int pBest = methodPreference(cBest.getMethod());
                      const int pCur  = methodPreference(cCur.getMethod());
                      if (pCur < pBest) // Lower rank number = Higher preference
                        {
                          bestIdx = idx;
                          continue;
                        }
                    }
                }
            }

          const Candidate& chosen = filtered[bestIdx];
          return Result(chosen.getMethod(), chosen, enriched);
        }
      };

  } // namespace analysis
} // namespace palvalidator
