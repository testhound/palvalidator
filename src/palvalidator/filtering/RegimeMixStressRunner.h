// RegimeMixStressRunner.h

#pragma once

#include "RegimeMixStress.h"
#include "RegimeMixBlockResampler.h"

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

    // Templated on RNG to enable deterministic tests when needed.
    // Default remains randutils::mt19937_rng for production.
    template <class Num,
	      class Rng = randutils::mt19937_rng,
	      template<class, class> class SamplerT = palvalidator::resampling::RegimeMixBlockResampler>
    class RegimeMixStressRunner
    {
    public:
      RegimeMixStressRunner(const RegimeMixConfig &config,
			    std::size_t L,
			    unsigned int numResamples,
			    double confidenceLevel,
			    double annualizationFactor,
			    Num finalRequiredReturn)
        : mConfig(config),
          mL(std::max<std::size_t>(2, L)),
          mNumResamples(numResamples),
          mConfidenceLevel(confidenceLevel),
          mAnnualizationFactor(annualizationFactor),
          mFinalRequiredReturn(finalRequiredReturn)
      {
      }

      class MixResult
      {
      public:
        MixResult(std::string mixName, Num annLb, bool pass)
	  : mMixName(std::move(mixName)), mAnnualizedLowerBound(annLb), mPass(pass)
        {
        }

        const std::string & mixName() const
        {
	  return mMixName;
        }

        Num annualizedLowerBound() const
        {
	  return mAnnualizedLowerBound;
        }

        bool pass() const
        {
	  return mPass;
        }

      private:
        std::string mMixName;
        Num mAnnualizedLowerBound;
        bool mPass;
      };

      class Result
      {
      public:
        Result(std::vector<MixResult> perMix, double passFrac, bool overallPass)
	  : mPerMix(std::move(perMix)), mPassFraction(passFrac), mOverallPass(overallPass)
        {
        }

        const std::vector<MixResult> & perMix() const
        {
	  return mPerMix;
        }

        double passFraction() const
        {
	  return mPassFraction;
        }

        bool overallPass() const
        {
	  return mOverallPass;
        }

      private:
        std::vector<MixResult> mPerMix;
        double mPassFraction;
        bool mOverallPass;
      };

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

            const bool pass = (lbGeoAnn > mFinalRequiredReturn);
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
      Num mFinalRequiredReturn;
    };

  } // namespace analysis
} // namespace palvalidator
