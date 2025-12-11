#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <ostream>
#include <iomanip>
#include <type_traits>
#include "AdaptiveRatioInternal.h"
#include "number.h"

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Abstract interface for m-out-of-n ratio determination policies.
     *
     * Implementations of this interface encapsulate different strategies for
     * choosing the optimal subsampling ratio based on data characteristics.
     *
     * @tparam Decimal The numeric type (e.g., dec::decimal<8>)
     * @tparam BootstrapStatistic The statistic functor type
     */
    template<typename Decimal, typename BootstrapStatistic>
    class IAdaptiveRatioPolicy
    {
    public:
      virtual ~IAdaptiveRatioPolicy() = default;

      /**
       * @brief Computes the optimal m/n ratio for the given data (prior-only).
       *
       * This method is used by the simple API and does not perform refinement.
       *
       * @param data The input return series
       * @param ctx Pre-computed statistical context
       * @param confidenceLevel The target confidence level
       * @param B Number of bootstrap replicates
       * @param os Optional output stream for diagnostics
       * @return double The ratio ρ ∈ (0,1) where m = floor(ρ * n)
       */
      virtual double computeRatio(
          const std::vector<Decimal>& data,
          const detail::StatisticalContext<Decimal>& ctx,
          double confidenceLevel,
          std::size_t B,
          std::ostream* os = nullptr) const = 0;

      /**
       * @brief Computes the optimal m/n ratio with refinement support (advanced).
       *
       * This method is used by runWithRefinement() and can perform stability-based
       * refinement using the probe engine maker.
       *
       * Default implementation: fall back to prior-only.
       *
       * @param data The input return series
       * @param ctx Pre-computed statistical context
       * @param confidenceLevel The target confidence level
       * @param B Number of bootstrap replicates
       * @param probeMaker Interface for creating and running probe engines
       * @param os Optional output stream for diagnostics
       * @return double The ratio ρ ∈ (0,1) where m = floor(ρ * n)
       */
      template<typename ProbeEngineMaker>
      double computeRatioWithRefinement(
          const std::vector<Decimal>& data,
          const detail::StatisticalContext<Decimal>& ctx,
          double confidenceLevel,
          std::size_t B,
          const ProbeEngineMaker& probeMaker,
          std::ostream* os = nullptr) const
      {
        (void)probeMaker;  // Unused in default implementation
        // Default: fall back to prior-only
        return computeRatio(data, ctx, confidenceLevel, B, os);
      }
    };

    /**
     * @brief Simple policy that returns a fixed ratio regardless of data.
     *
     * Useful for:
     * - Reproducibility studies
     * - Comparison with adaptive methods
     * - Cases where domain knowledge dictates a specific ratio
     *
     * @tparam Decimal The numeric type
     * @tparam BootstrapStatistic The statistic functor type
     */
    template<typename Decimal, typename BootstrapStatistic>
    class FixedRatioPolicy : public IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>
    {
    public:
      /**
       * @brief Constructs a fixed ratio policy.
       *
       * @param ratio The fixed ratio to return (must be in (0,1))
       * @throws std::invalid_argument if ratio is not in (0,1)
       */
      explicit FixedRatioPolicy(double ratio)
        : ratio_(ratio)
      {
        if (!(ratio > 0.0 && ratio < 1.0))
          throw std::invalid_argument("FixedRatioPolicy: ratio must be in (0,1)");
      }

      double computeRatio(
          const std::vector<Decimal>&,
          const detail::StatisticalContext<Decimal>&,
          double, std::size_t, std::ostream*) const override
      {
        return ratio_;
      }

    private:
      double ratio_;
    };

    /**
     * @brief Default adaptive ratio policy using tail/volatility heuristics.
     *
     * This policy implements a two-stage decision process:
     * 1. Prior Stage: Fast heuristic based on volatility and tail index
     * 2. Refinement Stage: Optional stability-based optimization (disabled by default)
     *
     * The policy adapts to three market regimes:
     * - High Volatility / Heavy Tails: ρ ≈ 0.80 (conservative, preserve extremes)
     * - Normal Regime: ρ ≈ 0.50 (balanced)
     * - Light Tails / Large N: ρ ≈ 0.35 (aggressive subsampling)
     *
     * Additionally enforces theoretical floors:
     * - General: m >= n^(2/3) for non-smooth statistics
     * - Ratio Statistics: m/n >= 0.60 for N >= 20
     *
     * @tparam Decimal The numeric type
     * @tparam BootstrapStatistic The statistic functor type
     */
    template<typename Decimal, typename BootstrapStatistic>
    class TailVolatilityAdaptivePolicy
      : public IAdaptiveRatioPolicy<Decimal, BootstrapStatistic>
    {
    public:
      /**
       * @brief Configuration parameters for the policy (immutable).
       */
      class Config
      {
      public:
        /**
         * @brief Construct a fully-initialized immutable Config.
         *
         * @param highVolAnnThreshold     Annualized volatility threshold (default 0.40)
         * @param heavyTailAlphaThreshold Heavy tail threshold α ≤ 2.0
         * @param lightTailAlphaThreshold Light tail threshold α ≥ 4.0
         * @param highVolRatio            Ratio used in high-vol / heavy-tail regimes
         * @param normalRatio             Ratio used in normal regimes
         * @param lightTailRatio          Ratio used in light-tail + large-N regimes
         * @param nLargeThreshold         Minimum N for light-tail logic (default 50)
         */
        explicit Config(
            double      highVolAnnThreshold     = 0.40,
            double      heavyTailAlphaThreshold = 2.0,
            double      lightTailAlphaThreshold = 4.0,
            double      highVolRatio            = 0.80,
            double      normalRatio             = 0.50,
            double      lightTailRatio          = 0.35,
            std::size_t nLargeThreshold         = 50)
          : highVolAnnThreshold_(highVolAnnThreshold)
          , heavyTailAlphaThreshold_(heavyTailAlphaThreshold)
          , lightTailAlphaThreshold_(lightTailAlphaThreshold)
          , highVolRatio_(highVolRatio)
          , normalRatio_(normalRatio)
          , lightTailRatio_(lightTailRatio)
          , nLargeThreshold_(nLargeThreshold)
        {
        }

        // Getters only — immutable configuration
        double      getHighVolAnnThreshold()     const { return highVolAnnThreshold_; }
        double      getHeavyTailAlphaThreshold() const { return heavyTailAlphaThreshold_; }
        double      getLightTailAlphaThreshold() const { return lightTailAlphaThreshold_; }
        double      getHighVolRatio()            const { return highVolRatio_; }
        double      getNormalRatio()             const { return normalRatio_; }
        double      getLightTailRatio()          const { return lightTailRatio_; }
        std::size_t getNLargeThreshold()         const { return nLargeThreshold_; }

      private:
        double      highVolAnnThreshold_;      // 40% annualized vol
        double      heavyTailAlphaThreshold_;  // α <= 2.0 → heavy tails
        double      lightTailAlphaThreshold_;  // α >= 4.0 → light tails
        double      highVolRatio_;             // Wild markets
        double      normalRatio_;              // Typical markets
        double      lightTailRatio_;           // Well-behaved, large N
        std::size_t nLargeThreshold_;          // Min N for light-tail logic
      };

      explicit TailVolatilityAdaptivePolicy(const Config& config = Config())
        : config_(config)
      {
      }

      /**
       * @brief Prior-only computation (simple API).
       *
       * This is what the unit tests call directly.
       */
      double computeRatio(
          const std::vector<Decimal>& data,
          const detail::StatisticalContext<Decimal>& ctx,
          double confidenceLevel,
          std::size_t B,
          std::ostream* os = nullptr) const override
      {
        (void)data;
        (void)confidenceLevel;
        (void)B;

        const std::size_t n = ctx.getSampleSize();

        // Degenerate: let caller handle impossibly small samples
        if (n < 3)
          return 1.0;

        // For ultra-small n, we **only** apply the ~50% rule
        // (no n^(2/3) floor, no ratio-statistic floor).
        if (n < 5)
        {
          double rho = computeSmallNSimpleRatio(n, os);
          return clampToValidBounds(rho, n);
        }

        // 1) Compute prior ratio from tail/vol regime classification
        double rho = computePriorRatio(ctx);
        rho = clampToValidBounds(rho, n);

        // 2) Apply theoretical floor: m ≈ n^(2/3)
        const double theoretical_min = mn_ratio_from_n(n);
        if (rho < theoretical_min)
        {
          if (os)
          {
            const double old_m = rho * static_cast<double>(n);
            const double new_m = theoretical_min * static_cast<double>(n);
            (*os) << "   [Bootstrap/mn-ratio-floor] "
                  << "Theoretical n^(2/3) floor applied (rho="
                  << std::fixed << std::setprecision(3) << rho
                  << " -> " << theoretical_min
                  << ", m≈" << std::setprecision(2) << new_m
                  << " from " << std::setprecision(2) << old_m << ").\n";
          }
          rho = theoretical_min;
        }

        // 3) Apply ratio-statistic floor, if the statistic advertises it
        rho = applyRatioStatisticFloor(rho, n, os);

        // 4) Final clamp in case floors pushed slightly out of bounds
        return clampToValidBounds(rho, n);
      }

      /**
       * @brief Compute ratio with refinement support (advanced API).
       *
       * This method implements the full two-stage process:
       * 1. Prior stage: Fast heuristic based on volatility and tail index
       * 2. Refinement stage: Stability-based optimization using probe engines
       *
       * @param data The input return series
       * @param ctx Pre-computed statistical context
       * @param confidenceLevel The target confidence level
       * @param B Number of bootstrap replicates
       * @param probeMaker Interface for creating and running probe engines
       * @param os Optional output stream for diagnostics
       * @return double The refined ratio ρ ∈ (0,1)
       */
      template<typename ProbeEngineMaker>
      double computeRatioWithRefinement(
          const std::vector<Decimal>& data,
          const detail::StatisticalContext<Decimal>& ctx,
          double confidenceLevel,
          std::size_t B,
          const ProbeEngineMaker& probeMaker,
          std::ostream* os = nullptr) const
      {
        const std::size_t n = ctx.getSampleSize();

        // Degenerate cases
        if (n < 3)
          return 1.0;

        // For ultra-small n, use simple 50% rule (no refinement)
        if (n < 5)
        {
          double rho = computeSmallNSimpleRatio(n, os);
          return clampToValidBounds(rho, n);
        }

        // 1. Compute prior ratio
        double baseRatio = computePriorRatio(ctx);
        baseRatio = clampToValidBounds(baseRatio, n);

        // 2. Apply refinement if N is in the refinement window [15, 60]
        constexpr std::size_t MIN_N_FOR_REFINEMENT = 15;
        constexpr std::size_t MAX_N_FOR_REFINEMENT = 60;

        double refined = baseRatio;
        if (n >= MIN_N_FOR_REFINEMENT && n <= MAX_N_FOR_REFINEMENT)
        {
          refined = refineRatio(data, ctx, baseRatio, confidenceLevel, B, probeMaker, os);
          refined = clampToValidBounds(refined, n);
        }
        else if (os)
        {
          (*os) << "   [TailVolatilityAdaptivePolicy] N=" << n
                << " outside refinement window [" << MIN_N_FOR_REFINEMENT
                << ", " << MAX_N_FOR_REFINEMENT << "], skipping refinement.\n";
        }

        // 3. Apply theoretical floor: m ≈ n^(2/3)
        const double theoretical_min = mn_ratio_from_n(n);
        if (refined < theoretical_min)
        {
          if (os)
          {
            const double old_m = refined * static_cast<double>(n);
            const double new_m = theoretical_min * static_cast<double>(n);
            (*os) << "   [Bootstrap/mn-ratio-floor] "
                  << "Theoretical n^(2/3) floor applied (rho="
                  << std::fixed << std::setprecision(3) << refined
                  << " -> " << theoretical_min
                  << ", m≈" << std::setprecision(2) << new_m
                  << " from " << std::setprecision(2) << old_m << ").\n";
          }
          refined = theoretical_min;
        }

        // 4. Apply ratio-statistic floor
        refined = applyRatioStatisticFloor(refined, n, os);

        // 5. Final clamp
        return clampToValidBounds(refined, n);
      }

    private:
      Config config_;

      /**
       * @brief Refinement stage: stability-based optimization.
       *
       * Generates candidate ratios around the base ratio and selects
       * the one with minimum instability score.
       *
       * @param data The input return series
       * @param ctx Statistical context
       * @param baseRatio The prior ratio to refine around
       * @param confidenceLevel Target confidence level
       * @param B Number of bootstrap replicates (unused, kept for API consistency)
       * @param probeMaker Interface for running probe engines
       * @param os Optional diagnostic output stream
       * @return double The refined ratio
       */
      template<typename ProbeEngineMaker>
      double refineRatio(
          const std::vector<Decimal>& data,
          const detail::StatisticalContext<Decimal>& ctx,
          double baseRatio,
          double confidenceLevel,
          std::size_t B,
          const ProbeEngineMaker& probeMaker,
          std::ostream* os) const
      {
        (void)ctx;              // Unused
        (void)confidenceLevel;  // Unused
        (void)B;                // Unused

        const std::size_t n = data.size();

        // Generate candidate ratios: 11-point grid from -0.25 to +0.25
        constexpr int NUM_DELTAS = 11;
        constexpr double DELTA_MIN = -0.25;
        constexpr double DELTA_MAX = +0.25;
        constexpr std::size_t B_PROBE = 400;  // Replicates per probe

        std::vector<double> candidates;
        candidates.reserve(NUM_DELTAS);

        for (int i = 0; i < NUM_DELTAS; ++i)
        {
          const double delta = DELTA_MIN + (DELTA_MAX - DELTA_MIN) * i / (NUM_DELTAS - 1);
          double candidate = baseRatio + delta;
          candidate = clampToValidBounds(candidate, n);
          candidates.push_back(candidate);
        }

        // Remove duplicates (can happen due to clamping)
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

        if (os)
        {
          (*os) << "   [TailVolatilityAdaptivePolicy/Refinement] "
                << "Testing " << candidates.size() << " candidate ratios around "
                << std::fixed << std::setprecision(3) << baseRatio << "\n";
        }

        // Probe each candidate
        std::vector<detail::CandidateScore> scores;
        scores.reserve(candidates.size());

        for (double rho : candidates)
        {
          auto score = probeMaker.runProbe(data, rho, B_PROBE);
          scores.push_back(score);

          if (os)
          {
            (*os) << "     rho=" << std::fixed << std::setprecision(3) << rho
                  << " -> LB=" << std::setprecision(4) << score.getLowerBound()
                  << ", sigma=" << score.getSigma()
                  << ", instability=" << score.getInstability() << "\n";
          }
        }

        // Select candidate with minimum instability
        // Tie-break by preferring smaller ratio (more conservative)
        auto best_it = std::min_element(
            scores.begin(), scores.end(),
            [](const detail::CandidateScore& a, const detail::CandidateScore& b)
            {
              const double inst_a = a.getInstability();
              const double inst_b = b.getInstability();
              if (std::abs(inst_a - inst_b) < 1e-9)
              {
                // Tie: prefer smaller ratio
                return a.getRatio() < b.getRatio();
              }
              return inst_a < inst_b;
            });

        const double refined = best_it->getRatio();

        if (os)
        {
          (*os) << "   [TailVolatilityAdaptivePolicy/Refinement] "
                << "Selected rho=" << std::fixed << std::setprecision(3) << refined
                << " (instability=" << std::setprecision(4) << best_it->getInstability()
                << ")\n";
        }

        return refined;
      }

      /**
       * @brief Small-N 50% rule used for n < 5.
       *
       * m = ceil(0.5 * n), clamped to [2, n-1].
       */
      static double computeSmallNSimpleRatio(std::size_t n, std::ostream* os)
      {
        const double m_raw = std::ceil(0.50 * static_cast<double>(n));
        const std::size_t m =
          std::max<std::size_t>(
              2,
              std::min<std::size_t>(
                  static_cast<std::size_t>(m_raw),
                  (n > 0 ? n - 1 : 0)));

        double rho = static_cast<double>(m) / static_cast<double>(n);
        rho = clampToValidBounds(rho, n);

        if (os)
        {
          (*os) << "[TailVolatilityAdaptivePolicy] small-n (n="
                << n << ") using rho=" << rho << "\n";
        }
        return rho;
      }

      /**
       * @brief Computes the prior m/n ratio based on the statistical context.
       *
       * This is the port of TailVolPriorPolicy::computePriorRatio from the
       * old SmallNBootstrapHelpers, **without** small-N special casing
       * (that is handled separately).
       */
      double computePriorRatio(const detail::StatisticalContext<Decimal>& ctx) const
      {
        const std::size_t n = ctx.getSampleSize();

        // Degenerate: let caller handle n < 3 specially if needed
        if (n < 3)
          return 1.0;

        // Clamping bounds: 2 <= m <= n-1
        const double minRho = 2.0 / static_cast<double>(n);
        const double maxRho = (n > 2)
          ? static_cast<double>(n - 1) / static_cast<double>(n)
          : 0.5;

        const double sigmaAnn = ctx.getAnnualizedVolatility();
        const double tailIdx  = ctx.getTailIndex();
        const bool   heavyFlg = ctx.hasHeavyTails();

        const bool tailIdxValid = (tailIdx > 0.0);

        // Very heavy tails (α small) – classical "infinite-variance-ish" region
        const bool extremeHeavyTail =
          tailIdxValid &&
          (tailIdx <= config_.getHeavyTailAlphaThreshold());

        // High-vol regime: heavy tails OR high σ_ann
        const bool isHighVol =
          extremeHeavyTail ||
          heavyFlg ||
          (sigmaAnn >= config_.getHighVolAnnThreshold());

        // Very light tails, only considered when n is large and not high-vol
        const bool isVeryLightTail =
          tailIdxValid &&
          (tailIdx >= config_.getLightTailAlphaThreshold()) &&
          !heavyFlg &&
          (sigmaAnn < config_.getHighVolAnnThreshold()) &&
          (n >= config_.getNLargeThreshold());

        double target;
        if (isHighVol)
        {
          // Heavy-tail / high-vol regime: keep m close to n
          target = config_.getHighVolRatio();
        }
        else if (isVeryLightTail)
        {
          // Very light tail & large n: smaller m/n is acceptable
          target = config_.getLightTailRatio();
        }
        else
        {
          // Everything else: "normal" medium subsample
          target = config_.getNormalRatio();
        }

        // Clamp to [2/n, (n-1)/n]
        double rho = std::max(minRho, std::min(target, maxRho));
        return rho;
      }

      /**
       * @brief Heuristic m/n rule: m = n^(2/3), clamped to [7, n-1].
       *
       * Port of mn_ratio_from_n() from SmallNBootstrapHelpers.h.
       */
      static double mn_ratio_from_n(std::size_t n)
      {
        if (n == 0) return 1.0;
        if (n < 3)  return 1.0; // Too small to subsample meaningfully

        // 1. Calculate Power Law Target: m = n^(2/3)
        double m_target = std::pow(static_cast<double>(n), 2.0 / 3.0);

        // 2. Define Bounds
        double m_floor = 7.0;
        double m_ceil  = static_cast<double>(n - 1);

        // 3. Clamp
        double m = std::max(m_floor, std::min(m_target, m_ceil));

        // 4. Return Ratio
        return m / static_cast<double>(n);
      }

      /**
       * @brief Helper: Clamp ratio to valid m/n bounds [2/n, (n-1)/n]
       *
       * Ensures that m is always in the range [2, n-1].
       */
      static double clampToValidBounds(double ratio, std::size_t n)
      {
        const double minRho = 2.0 / static_cast<double>(n);
        const double maxRho = (n > 2)
          ? static_cast<double>(n - 1) / static_cast<double>(n)
          : 0.5;
        return std::max(minRho, std::min(ratio, maxRho));
      }

      /**
       * @brief Apply ratio statistic floor (m/n >= 0.60 for N >= 20).
       *
       * Port of the ratio-statistic floor logic from SmallNBootstrapHelpers.
       * Uses SFINAE to detect if BootstrapStatistic has isRatioStatistic().
       */
      double applyRatioStatisticFloor(double ratio, std::size_t n, std::ostream* os) const
      {
        // SFINAE-based trait detection for isRatioStatistic()
        if constexpr (has_isRatioStatistic<BootstrapStatistic>::value)
        {
          constexpr double      RATIO_MIN_RHO   = 0.60;
          constexpr std::size_t N_MIN_FOR_FLOOR = 20;

          if (BootstrapStatistic::isRatioStatistic() &&
              n >= N_MIN_FOR_FLOOR &&
              ratio < RATIO_MIN_RHO)
          {
            if (os)
            {
              const double old_m = ratio       * static_cast<double>(n);
              const double new_m = RATIO_MIN_RHO * static_cast<double>(n);

              (*os) << "   [Bootstrap/mn-ratio-floor] "
                    << "ratio-statistic floor m/n=" << RATIO_MIN_RHO
                    << " applied (rho="
                    << std::fixed << std::setprecision(3) << ratio
                    << " → " << RATIO_MIN_RHO
                    << ", m≈" << std::setprecision(2) << old_m
                    << " → " << new_m << ")\n";
            }

            return RATIO_MIN_RHO;
          }
        }

        return ratio;
      }

      // SFINAE trait detection for isRatioStatistic()
      template <typename Stat, typename = void>
      struct has_isRatioStatistic : std::false_type {};

      template <typename Stat>
      struct has_isRatioStatistic<
          Stat,
          std::void_t<decltype(Stat::isRatioStatistic())>
      > : std::true_type {};
    };
  } // namespace analysis
} // namespace palvalidator
