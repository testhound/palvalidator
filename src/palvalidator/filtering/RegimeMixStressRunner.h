// RegimeMixStressRunner.h

#pragma once

#include "RegimeMixStress.h"
#include "RegimeMixBlockResampler.h"
#include "filtering/ValidationPolicy.h"

#include <vector>
#include <string>
#include <ostream>
#include <limits>

#include "randutils.hpp"
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include "DecimalConstants.h"

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Orchestrates the Regime Mix Stress Test execution.
     *
     * Objective:
     * This class acts as the runner for the regime-conditional bootstrap analysis.
     * It takes the aligned market data (returns and regime labels) and iterates
     * through a list of hypothetical market scenarios ("Mixes").
     *
     * For each mix (e.g., "High Volatility Favored"), it:
     * 1. Configures a regime-aware resampler (`SamplerT`) with the mix's target weights.
     * 2. Runs a Bias-Corrected (BCa) Bootstrap to estimate the Lower Bound of returns
     * under those specific simulated conditions.
     * 3. Compares the result against the Validation Policy (Hurdle).
     * 4. Aggregates the results to determine an overall Pass/Fail decision.
     *
     * @tparam Num The numeric type (e.g., double).
     * @tparam Rng The random number generator type (default: randutils::mt19937_rng).
     * @tparam SamplerT The resampling policy to use. This allows the runner to switch
     * between "Stationary" (random block length) and "Block" (fixed block length)
     * resampling strategies while reusing the same execution logic.
     */
    template <class Num,
	      class Rng = randutils::mt19937_rng,
	      template<class, class> class SamplerT = palvalidator::resampling::RegimeMixBlockResampler>
    class RegimeMixStressRunner
    {
    public:
      public:
      /**
       * @brief Constructs the runner with all necessary configuration.
       *
       * @param config Contains the list of Mixes to test and the pass/fail threshold.
       * @param L The block length for bootstrapping (dependent structure preservation).
       * @param numResamples Number of bootstrap iterations (e.g., 2000).
       * @param confidenceLevel The BCa confidence level (e.g., 0.95 for 95% LB).
       * @param annualizationFactor Scaling factor to convert per-period returns to annualized.
       * @param validationPolicy The policy containing the Cost Hurdle the strategy must clear.
       */
      RegimeMixStressRunner(const RegimeMixConfig &config,
                            std::size_t L,
                            unsigned int numResamples,
                            double confidenceLevel,
                            double annualizationFactor,
                            const palvalidator::filtering::ValidationPolicy& validationPolicy)
          : mConfig(config),
            mL(std::max<std::size_t>(2, L)),
            mNumResamples(numResamples),
            mConfidenceLevel(confidenceLevel),
            mAnnualizationFactor(annualizationFactor),
            mValidationPolicy(validationPolicy)
      {
      }

      /**
       * @brief Result container for a single specific Mix scenario.
       */
      class MixResult
      {
      public:
        MixResult(std::string mixName, Num annLb, bool pass)
	  : mMixName(std::move(mixName)),
	    mAnnualizedLowerBound(annLb),
	    mPass(pass)
        {
        }

	/// @brief The name of the scenario (e.g., "LowVolFav").
        const std::string & mixName() const
        {
	  return mMixName;
        }

	/// @brief The annualized BCa Lower Bound achieved under this scenario.
	
        Num annualizedLowerBound() const
        {
	  return mAnnualizedLowerBound;
        }

	/// @brief True if LB > Hurdle.
        bool pass() const
        {
	  return mPass;
        }

      private:
        std::string mMixName;
        Num mAnnualizedLowerBound;
        bool mPass;
      };

      /**
       * @brief Aggregate result for the entire suite of regime tests.
       */
      class Result
      {
      public:
        Result(std::vector<MixResult> perMix, double passFrac, bool overallPass)
	  : mPerMix(std::move(perMix)), mPassFraction(passFrac), mOverallPass(overallPass)
        {
        }

	/// @brief Detailed results for every mix tested.
        const std::vector<MixResult> & perMix() const
        {
	  return mPerMix;
        }

	/// @brief The percentage of mixes that passed (0.0 to 1.0).
        double passFraction() const
        {
	  return mPassFraction;
        }

	/// @brief True if passFraction >= config.minPassFraction().
        bool overallPass() const
        {
	  return mOverallPass;
        }

      private:
        std::vector<MixResult> mPerMix;
        double mPassFraction;
        bool mOverallPass;
      };

      /**
       * @brief Executes the stress test suite.
       *
       * Algorithm:
       * 1. **Validation:** Ensures `returns` and `labels` have equal length.
       *
       * 2. **Iteration:** Loops through every `RegimeMix` defined in `mConfig`.
       *
       * 3. **Sampler Configuration:** For each mix, instantiates a `SamplerT`.
       * - This sampler is fed the `labels` (history) and the `mix.weights()` (target).
       * - The sampler creates a probability distribution that biases the bootstrap
       * selection toward the specific regimes defined in the mix.
       *
       * 4. **Bootstrap:** Runs `BCaBootStrap` using this biased sampler.
       * - This generates a synthetic distribution of returns representing the
       * hypothetical market condition.
       *
       * 5. **Assessment:** Calculates the Annualized Geometric Mean Lower Bound.
       * - Checks `mValidationPolicy.hasPassed(lb)`.
       *
       * 6. **Aggregation:** Counts how many mixes passed and calculates the pass fraction.
       *
       *        *
       * @param returns The vector of per-period returns.
       * @param labels The vector of regime labels (0, 1, 2) aligned to `returns`.
       * @param os Output stream for real-time logging of results.
       * @return A `Result` object containing the aggregate decision and details.
       *
       * @throws std::invalid_argument If `returns.size() != labels.size()`.
       */
      Result run(const std::vector<Num> &returns,
		 const std::vector<int> &labels,
		 std::ostream &os) const
      {
        using palvalidator::resampling::RegimeMixBlockResampler;
        using mkc_timeseries::BCaBootStrap;
        using mkc_timeseries::GeoMeanStat;
        using mkc_timeseries::BCaAnnualizer;
        using mkc_timeseries::DecimalConstants;

	// Select the sampler type chosen by template argument
	using Sampler = SamplerT<Num, Rng>;
	using MixBCA  = BCaBootStrap<Num, Sampler, Rng>;
        //using BlockBCA = BCaBootStrap<Num, RegimeMixBlockResampler<Num, Rng>, Rng>;

        if (returns.size() != labels.size())
	  {
            throw std::invalid_argument("RegimeMixStressRunner: returns/labels size mismatch");
	  }

        std::size_t passCount = 0;
        std::vector<MixResult> details;
        details.reserve(mConfig.mixes().size());

        for (const auto &mix : mConfig.mixes())
	  {
            // Build state-aware resampler for this mix (with RNG type Rng)
            Sampler sampler
	      (
	       mL,
	       labels,
	       mix.weights(),
	       mConfig.minBarsPerRegime()
	       );

            GeoMeanStat<Num> statGeo;
            MixBCA bcaGeo(returns, mNumResamples, mConfidenceLevel, statGeo, sampler);

            const Num lbGeoPeriod = bcaGeo.getLowerBound();
            (void)lbGeoPeriod; // keep for symmetry; annualizer uses the same bca object

            BCaAnnualizer<Num> annualizer(bcaGeo, mAnnualizationFactor);
            const Num lbGeoAnn = annualizer.getAnnualizedLowerBound();

            const bool pass = mValidationPolicy.hasPassed(lbGeoAnn);
            details.emplace_back(mix.name(), lbGeoAnn, pass);
            if (pass) ++passCount;

            os << "      [RegimeMix] " << mix.name()
               << " → Ann GM LB = " << (lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "% "
               << (pass ? "(PASS)" : "(FAIL)")
               << "\n";
	  }

        const double passFrac = (details.empty() ? 0.0
				 : static_cast<double>(passCount) / static_cast<double>(details.size()));
        const bool overall = (passFrac >= mConfig.minPassFraction());

        os << "        → regime-mix pass fraction = " << (100.0 * passFrac)
           << "%, decision: " << (overall ? "PASS" : "FAIL") << "\n";

        return Result(std::move(details), passFrac, overall);
      }

    private:
      const RegimeMixConfig &mConfig;
      std::size_t mL;
      unsigned int mNumResamples;
      double mConfidenceLevel;
      double mAnnualizationFactor;
      const palvalidator::filtering::ValidationPolicy& mValidationPolicy;
    };

  } // namespace analysis
} // namespace palvalidator
