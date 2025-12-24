#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include "StatUtils.h"
#include "number.h"
#include "NormalQuantile.h"

namespace palvalidator
{
  namespace analysis
  {
    namespace detail
    {
      /**
       * @brief Estimates the Pareto tail index (alpha) of the left tail (losses) using the Hill estimator.
       *
       * @details
       * This is a direct port of the Hill estimator from SmallNBootstrapHelpers.h (lines 457-493).
       * The Hill estimator focuses exclusively on the extreme observations (the "tail") and
       * treats them conditionally, following a predictable mathematical pattern (Pareto distribution).
       *
       * Algorithm:
       * 1. Isolate & Flip: Extract only negative returns (losses) and convert to positive magnitudes
       * 2. Sort: Order magnitudes descending (largest crash first)
       * 3. Threshold: Select the top k worst losses. The k-th loss becomes the threshold (xk)
       * 4. Measure Distance: Compute the "Hill Mean" (H), the average logarithmic distance of extremes beyond threshold
       * 5. Invert: The Tail Index Alpha = 1.0 / H
       *
       * Interpretation:
       * - Alpha < 2.0 (Infinite Variance / Very Heavy Tails): "Wild" regime
       * - Alpha > 4.0 (Light Tails): "Mild" regime
       *
       * @tparam Decimal Numeric type
       * @param returns The vector of raw returns
       * @param k The number of tail observations to use (default 5)
       * @return double The estimated tail index alpha. Returns -1.0 if insufficient data
       *
       * @see Hill (1975), "A Simple General Approach to Inference About the Tail of a Distribution"
       */
      template <typename Decimal>
      inline double estimate_left_tail_index_hill(const std::vector<Decimal>& returns,
						  std::size_t k = 5)
      {
        std::vector<double> losses;
        losses.reserve(returns.size());

        for (const auto& r : returns)
	  {
	    const double v = num::to_double(r);
	    if (v < 0.0)
	      losses.push_back(-v);
	  }

        constexpr std::size_t minLossesForHill = 8;

        if (losses.size() < std::max<std::size_t>(k + 1, minLossesForHill))
          return -1.0; // treat tail index as "unknown" for small samples

        std::sort(losses.begin(), losses.end(), std::greater<double>());

        k = std::min<std::size_t>(k, losses.size() - 1);
        const double xk = losses[k];

        if (xk <= 0.0)
          return -1.0;

        double sumLog = 0.0;
        for (std::size_t i = 0; i < k; ++i)
          sumLog += std::log(losses[i] / xk);

        const double hill = sumLog / static_cast<double>(k);
        if (hill <= 0.0)
          return -1.0;

        return 1.0 / hill;  // α
      }

      /**
       * @brief Encapsulates distributional characteristics for adaptive ratio decisions.
       *
       * @details
       * This class computes and caches statistical metrics needed by ratio policies.
       * It is constructed automatically from the input data and provides a clean
       * interface for policy decision-making.
       *
       * @tparam Decimal The numeric type (e.g., dec::decimal<8>)
       */
      template <class Decimal>
      class StatisticalContext {
      public:
	using Stat = mkc_timeseries::StatUtils<Decimal>;

	StatisticalContext(const std::vector<Decimal>& returns,
			   double annualizationFactor = 1.0)
	  : n_(returns.size()),
	    annualizationFactor_(annualizationFactor)
	{
	  if (n_ == 0) {
            sigmaAnn_        = std::numeric_limits<double>::quiet_NaN();
            skewness_        = std::numeric_limits<double>::quiet_NaN();
            excessKurtosis_  = std::numeric_limits<double>::quiet_NaN();
            tailIndex_       = std::numeric_limits<double>::quiet_NaN();
            heavyTails_      = false;
            strongAsymmetry_ = false;
            return;
	  }

	  // Mean / variance
	  const auto [mean, variance] =
            Stat::computeMeanAndVarianceFast(returns);

	  const double sigma = std::sqrt(num::to_double(variance));
	  sigmaAnn_ = sigma;
	  if (annualizationFactor_ > 0.0)
            sigmaAnn_ *= std::sqrt(annualizationFactor_);

	  // Moment-based shape
	  std::tie(skewness_, excessKurtosis_) =
            Stat::computeSkewAndExcessKurtosis(returns);

	  // Quantile-based shape
	  const auto qShape = Stat::computeQuantileShape(returns);
	  strongAsymmetry_ = qShape.hasStrongAsymmetry;
	  const bool heavyFromQuantiles =
            qShape.hasStrongAsymmetry || qShape.hasHeavyTails;

	  // Hill left-tail index
	  tailIndex_ = estimate_left_tail_index_hill(returns);
	  const bool validHill   = (tailIndex_ > 0.0);
	  const bool heavyViaHill = validHill && (tailIndex_ <= 2.0);

	  // **Conservative OR**: quantiles OR Hill
	  heavyTails_ = heavyFromQuantiles || heavyViaHill;
	}

	std::size_t getSampleSize()      const { return n_; }
	double      getAnnualizedVolatility() const { return sigmaAnn_; }
	double      getSkewness()        const { return skewness_; }
	double      getExcessKurtosis()  const { return excessKurtosis_; }
	double      getTailIndex()       const { return tailIndex_; }
	bool        hasHeavyTails()      const { return heavyTails_; }
	bool        hasStrongAsymmetry() const { return strongAsymmetry_; }

      private:
	std::size_t n_;
	double annualizationFactor_;
	double sigmaAnn_{std::numeric_limits<double>::quiet_NaN()};
	double skewness_{std::numeric_limits<double>::quiet_NaN()};
	double excessKurtosis_{std::numeric_limits<double>::quiet_NaN()};
	double tailIndex_{std::numeric_limits<double>::quiet_NaN()};
	bool   heavyTails_{false};
	bool   strongAsymmetry_{false};
      };
      
      /**
       * @brief Score for a candidate ratio during refinement.
       *
       * This class holds the results of probing a specific m/n ratio
       * during the stability-based refinement stage.
       */
      class CandidateScore
      {
      public:
        /**
         * @brief Constructs a candidate score with all metrics.
         *
         * @param lowerBound Lower bound from probe
         * @param sigma Standard deviation of replicates
         * @param instability Instability metric (lower is better)
         * @param ratio The ratio that was tested
         */
        CandidateScore(double lowerBound, double sigma, double instability, double ratio)
          : lowerBound_(lowerBound)
          , sigma_(sigma)
          , instability_(instability)
          , ratio_(ratio)
        {
        }

        // Getters
        double getLowerBound() const { return lowerBound_; }
        double getSigma() const { return sigma_; }
        double getInstability() const { return instability_; }
        double getRatio() const { return ratio_; }

      private:
        double lowerBound_;      // Lower bound from probe
        double sigma_;           // Standard deviation of replicates
        double instability_;     // Instability metric (lower is better)
        double ratio_;           // The ratio that was tested
      };

      /**
       * @brief Internal interface for creating probe engines during refinement.
       *
       * This interface decouples the refinement policy from the complex dependencies
       * (StrategyT, BootstrapFactoryT, CRN state) needed to create and run probe engines.
       * The concrete implementation captures these dependencies at execution time.
       *
       * @tparam Decimal The numeric type
       * @tparam BootstrapStatistic The statistic functor type
       */
      template<typename Decimal, typename BootstrapStatistic>
      class IProbeEngineMaker
      {
      public:
        virtual ~IProbeEngineMaker() = default;

        /**
         * @brief Run a probe and return the candidate score.
         *
         * @param returns The return series to analyze
         * @param rho The m/n ratio to test
         * @param B_probe Number of bootstrap replicates for the probe
         * @return CandidateScore containing LB, sigma, and instability metric
         */
        virtual CandidateScore runProbe(
            const std::vector<Decimal>& returns,
            double rho,
            std::size_t B_probe) const = 0;
      };

      /**
       * @brief Concrete implementation of IProbeEngineMaker that captures CRN state.
       *
       * This class is instantiated inside runWithRefinement() and captures all the
       * complex dependencies needed for probe execution. It is never exposed to
       * client code or policy implementations.
       *
       * @tparam Decimal The numeric type
       * @tparam BootstrapStatistic The statistic functor type
       * @tparam StrategyT Strategy type for CRN hashing
       * @tparam BootstrapFactoryT Factory type for creating bootstrap engines
       * @tparam Resampler Resampler type
       */
      template<typename Decimal, typename BootstrapStatistic,
               typename StrategyT, typename BootstrapFactoryT, typename Resampler>
      class ConcreteProbeEngineMaker final
        : public IProbeEngineMaker<Decimal, BootstrapStatistic>
      {
      public:
        /**
         * @brief Constructor captures the full execution context (CRN state).
         *
         * @param strategy Strategy object for CRN hashing
         * @param factory Bootstrap factory for creating probe engines
         * @param stageTag CRN stage identifier
         * @param fold CRN fold identifier
         * @param resampler Resampler instance
         * @param L_small Block length for resampler
         * @param confLevel Confidence level for bootstrap
         */
        ConcreteProbeEngineMaker(
            StrategyT& strategy,
            BootstrapFactoryT& factory,
            int stageTag,
            int fold,
            const Resampler& resampler,
            std::size_t L_small,
            double confLevel)
          : strategy_(strategy)
          , factory_(factory)
          , stageTag_(stageTag)
          , fold_(fold)
          , resampler_(resampler)
          , L_small_(L_small)
          , confLevel_(confLevel)
        {
        }

        /**
         * @brief Execute a probe and compute the instability score.
         *
         * @param returns The return series to analyze
         * @param rho The m/n ratio to test
         * @param B_probe Number of bootstrap replicates for the probe
         * @return CandidateScore containing LB, sigma, and instability metric
         */
        CandidateScore runProbe(
            const std::vector<Decimal>& returns,
            double rho,
            std::size_t B_probe) const override
        {
          // Create probe engine using the captured factory and CRN state
          auto [probeEngine, crnProvider] = factory_.template makeMOutOfN<Decimal, BootstrapStatistic, Resampler>(
              B_probe, confLevel_, rho, resampler_, strategy_,
              stageTag_, static_cast<int>(L_small_), fold_);

          // Run the probe bootstrap
          auto probeResult = probeEngine.run(
              returns, BootstrapStatistic(), crnProvider);

          // Compute instability metric (same logic as LBStabilityRefinementPolicy)
          const double lb = num::to_double(probeResult.lower);

          // Calculate sigma from CI width as proxy
          // sigma ≈ (upper - lower) / (2 * z_alpha/2)
          const double width = num::to_double(probeResult.upper - probeResult.lower);

	  // Compute proper quantile based on actual CL
	  const double z = compute_normal_critical_value(confLevel_);

          const double sigma = width / (2.0 * z);

          // Instability score: coefficient of variation of the lower bound
          const double instability = (lb != 0.0) ? std::abs(sigma / lb) : sigma;

          return CandidateScore(lb, sigma, instability, probeResult.computed_ratio);
        }

      private:
        StrategyT&           strategy_;
        BootstrapFactoryT&   factory_;
        const int            stageTag_;
        const int            fold_;
        const Resampler&     resampler_;
        const std::size_t    L_small_;
        const double         confLevel_;
      };

    } // namespace detail
  } // namespace analysis
} // namespace palvalidator
