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

    class RegimeMix
    {
    public:
    RegimeMix(std::string name, std::vector<double> weights)
      : mName(std::move(name)),
	mWeights(std::move(weights))
	{
	  // no normalization here; resampler normalizes
	}

      const std::string & name() const
      {
        return mName;
      }

      const std::vector<double> & weights() const
      {
        return mWeights;
      }

    private:
      std::string mName;
      std::vector<double> mWeights;
    };

    class RegimeMixConfig
    {
    public:
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

      const std::vector<RegimeMix> & mixes() const
      {
        return mMixes;
      }

      double minPassFraction() const
      {
        return mMinPassFraction;
      }

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
