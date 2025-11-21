#pragma once

#include <vector>
#include <string>
#include <ostream>
#include <utility>
#include <cstddef>

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Represents a single regime-weighted scenario for stress testing.
     *
     * Objective:
     * Defines a specific "Mix" or "Texture" of market conditions to simulate.
     * For example, a "High Volatility Favored" mix might define weights like
     * {0.15, 0.35, 0.50} for Low, Mid, and High volatility regimes respectively.
     *
     * The bootstrap resampler uses these weights to determine the probability
     * of drawing the next block of returns from a specific historical regime.
     */
    class RegimeMix
    {
    public:
      /**
       * @brief Constructs a Regime Mix scenario.
       *
       * @param name A descriptive identifier for logs (e.g., "MidVolFav(0.25,0.50,0.25)").
       * @param weights A vector of relative weights for each regime (Low, Mid, High).
       *
       * Note: The weights do not need to sum to 1.0 strictly, as the
       * downstream resampler (RegimeMixStationaryResampler) will normalize
       * them into a valid probability distribution.
       */
      RegimeMix(std::string name, std::vector<double> weights)
	: mName(std::move(name)),
	  mWeights(std::move(weights))
      {
	// no normalization here; resampler normalizes
      }

      /**
       * @brief Gets the display name of the mix.
       */
      const std::string & name() const
      {
        return mName;
      }

      /**
       * @brief Gets the vector of regime weights.
       */
      const std::vector<double> & weights() const
      {
        return mWeights;
      }

    private:
      std::string mName;
      std::vector<double> mWeights;
    };

    /**
     * @brief Configuration container for the Regime Mix Stress Test stage.
     *
     * Objective:
     * Encapsulates all settings required to run a Regime Mix Validation pass.
     * It defines the set of scenarios to run (the mixes), the criteria for
     * passing the gate, and safety constraints for data sufficiency.
     */
    class RegimeMixConfig
    {
    public:
      /**
       * @brief Validates and constructs the configuration object.
       *
       * @param mixes A vector of `RegimeMix` objects defining the scenarios to test.
       * @param minPassFraction The minimum percentage of mixes that must pass
       * for the strategy to be considered robust (e.g., 0.50 for 50%).
       * @param minBarsPerRegime Safety constraint. The bootstrap will abort/skip
       * if a specific regime (e.g., High Vol) has fewer than this many
       * historical bars, ensuring statistical significance.
       *
       * @throws std::invalid_argument If `mixes` is empty or `minPassFraction`
       * is not in the range (0, 1].
       */
      RegimeMixConfig(std::vector<RegimeMix> mixes,
		      double minPassFraction,
		      std::size_t minBarsPerRegime)
	: mMixes(std::move(mixes)),
	  mMinPassFraction(minPassFraction),
	  mMinBarsPerRegime(minBarsPerRegime)
      {
	if (mMixes.empty())
	  {
	    throw std::invalid_argument("RegimeMixConfig: provide at least one mix");
	  }
	if (mMinPassFraction <= 0.0 || mMinPassFraction > 1.0)
	  {
	    throw std::invalid_argument("RegimeMixConfig: minPassFraction in (0,1]");
	  }
      }

      /**
       * @brief Returns the list of regime scenarios to simulate.
       */
      const std::vector<RegimeMix> & mixes() const
      {
        return mMixes;
      }

      /**
       * @brief Returns the required pass rate (0.0 to 1.0).
       *
       * If the fraction of mixes that maintain a positive expectancy
       * falls below this threshold, the strategy fails the robustness check.
       */
      double minPassFraction() const
      {
        return mMinPassFraction;
      }

      /**
       * @brief Returns the minimum data requirement per regime.
       *
       * Used to prevent bootstrapping from sparse data buckets (e.g.,
       * if "High Volatility" only occurred once in history).
       */
      std::size_t minBarsPerRegime() const
	{
	  return mMinBarsPerRegime;
	}

    private:
      std::vector<RegimeMix> mMixes;
      double mMinPassFraction;
      std::size_t mMinBarsPerRegime;
    };

  } // namespace analysis
} // namespace palvalidator
