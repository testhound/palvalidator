// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef SEARCHALGOCONFIGURATIONFILEREADER_H
#define SEARCHALGOCONFIGURATIONFILEREADER_H

#include <string>
#include <memory>
#include <exception>
#include <vector>
#include "number.h"
#include "TimeSeries.h"
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include "PalParseDriver.h"
#include "TimeFrameUtility.h"
#include "TimeSeriesEntry.h"
#include "TimeSeriesCsvReader.h"
#include <cstdio>
#include "number.h"
#include "boost/lexical_cast.hpp"
#include "boost/lexical_cast/bad_lexical_cast.hpp"
#include "typeinfo"
#include "McptConfigurationFileReader.h"
#include "TimeSeriesCsvWriter.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "DataSourceReader.h"
#include "RunParameters.h"
#include "TimeFrameDiscovery.h"
#include "SyntheticTimeSeriesCreator.h"
#include "TimeSeriesValidator.h"

using namespace mkc_timeseries;

namespace mkc_searchalgo
{
  class SearchAlgoConfigurationFileReaderException : public std::runtime_error
  {
  public:
  SearchAlgoConfigurationFileReaderException(const std::string msg)
    : std::runtime_error(msg)
      {}

    ~SearchAlgoConfigurationFileReaderException()
      {}
  };


  template <class Decimal>
  class SearchAlgoConfiguration
  {
  public:
    SearchAlgoConfiguration (unsigned int maxDepth,
        unsigned int minTrades,
        Decimal sortMultiplier,
        unsigned int passingStratNumPerRound,
        Decimal profitFactorCriterion,
        unsigned int maxConsecutiveLosers,
        unsigned int maxInactivitySpan,
        std::vector<std::pair<Decimal, Decimal>> targetStopPairs,
        unsigned int numPermutations, unsigned int minNumStratsFullPeriod, unsigned int minNumStratsBeforeValidation,
                             Decimal palSafetyFactor,
                             Decimal stepRedundancyMultiplier,
                             Decimal survivalFilterMultiplier)
      :
      mMaxDepth(maxDepth),
      mMinTrades(minTrades),
      mActivityMultiplier(sortMultiplier),
      mPassingStratNumPerRound(passingStratNumPerRound),
      mProfitFactorCriterion(profitFactorCriterion),
      mMaxConsecutiveLosers(maxConsecutiveLosers),
      mMaxInactivitySpan(maxInactivitySpan),
      mTargetStopPairs(targetStopPairs),
      mNumPermutations(numPermutations),
      mMinNumStratsFullPeriod(minNumStratsFullPeriod),
      mMinNumStratsBeforeValidation(minNumStratsBeforeValidation),
      mPalSafetyFactor(palSafetyFactor),
      mStepRedundancyMultiplier(stepRedundancyMultiplier),
      mSurvivalFilterMultiplier(survivalFilterMultiplier)
    {}

    SearchAlgoConfiguration (const SearchAlgoConfiguration& rhs)
      : mMaxDepth(rhs.mMaxDepth),
        mMinTrades(rhs.mMinTrades),
        mActivityMultiplier(rhs.mActivityMultiplier),
        mPassingStratNumPerRound(rhs.mPassingStratNumPerRound),
        mProfitFactorCriterion(rhs.mProfitFactorCriterion),
        mMaxConsecutiveLosers(rhs.mMaxConsecutiveLosers),
        mMaxInactivitySpan(rhs.mMaxInactivitySpan),
        mTargetStopPairs(rhs.mTargetStopPairs),
        mNumPermutations(rhs.mNumPermutations),
        mMinNumStratsFullPeriod(rhs.mMinNumStratsFullPeriod),
        mMinNumStratsBeforeValidation(rhs.mMinNumStratsBeforeValidation),
        mPalSafetyFactor(rhs.mSafetyFactor),
        mStepRedundancyMultiplier(rhs.mStepRedundancyMultiplier),
        mSurvivalFilterMultiplier(rhs.mSurvivalFilterMultiplier)
    {}

    SearchAlgoConfiguration<Decimal> &
    operator=(const SearchAlgoConfiguration<Decimal> &rhs)
    {
      if (this == &rhs)
        return *this;
      mMaxDepth = rhs.mMaxDepth;
      mMinTrades = rhs.mMinTrades;
      mActivityMultiplier = rhs.mActivityMultiplier;
      mPassingStratNumPerRound = rhs.mPassingStratNumPerRound;
      mProfitFactorCriterion = rhs.mProfitFactorCriterion;
      mMaxConsecutiveLosers = rhs.mMaxConsecutiveLosers;
      mMaxInactivitySpan = rhs.mMaxInactivitySpan;
      mTargetStopPairs = rhs.mTargetStopPairs;
      mNumPermutations = rhs.mNumPermutations;
      mMinNumStratsFullPeriod = rhs.mMinNumStratsFullPeriod;
      mMinNumStratsBeforeValidation = rhs.mMinNumStratsBeforeValidation;
      mPalSafetyFactor = rhs.mPalSafetyFactor;
      mStepRedundancyMultiplier = rhs.mStepRedundancyMultiplier;
      mSurvivalFilterMultiplier = rhs.mSurvivalFilterMultiplier;

      return *this;
    }

    ~SearchAlgoConfiguration()
    {}

    inline friend std::ostream& operator<< (std::ostream& strng, const SearchAlgoConfiguration<Decimal>& obj)
    {
      return strng << "SearchAlgo Configs:: Depth: " << obj.mMaxDepth << ", MinTrades: " << obj.mMinTrades << ", SortMultiplier: " << obj.mActivityMultiplier
                   << ", PassingStratNumPerRound: " << obj.mPassingStratNumPerRound<< ", ProfitFactorCriterion: " << obj.mProfitFactorCriterion
                   << ", MaxConsecutiveLosers: " << obj.mMaxConsecutiveLosers << ", MaxInactivitySpan: " << obj.mMaxInactivitySpan
                   << ", Targets&Stops#: "<< obj.mTargetStopPairs.size() << ", "
                   << ", SafetyFactor: " << obj.mPalSafetyFactor << ", StepMultiplier: " << obj.mStepRedundancyMultiplier << ", survivalFilt: " << obj.mSurvivalFilterMultiplier
                   << "\nValidation settings -- # of permutations: " << obj.mNumPermutations
                   << ", Min # of strats full period: "<< obj.mMinNumStratsFullPeriod
                   << ", Min # of strats before validation: " << obj.mMinNumStratsBeforeValidation;
    }

    unsigned int getMaxDepth() const { return mMaxDepth; }

    unsigned int getMinTrades() const { return mMinTrades; }

    const Decimal& getActivityMultiplier() const { return mActivityMultiplier; }

    unsigned int getPassingStratNumPerRound() const { return mPassingStratNumPerRound; }

    const Decimal& getProfitFactorCriterion() const { return mProfitFactorCriterion; }

    unsigned int getMaxConsecutiveLosers() const { return mMaxConsecutiveLosers; }

    unsigned int getMaxInactivitySpan() const { return mMaxInactivitySpan; }

    typename std::vector<std::pair<Decimal, Decimal>>::const_iterator targetStopPairsBegin() const { return mTargetStopPairs.begin(); }

    typename std::vector<std::pair<Decimal, Decimal>>::const_iterator targetStopPairsEnd() const { return mTargetStopPairs.end(); }

    const std::vector<std::pair<Decimal, Decimal>>& getTargetStopPair() const { return mTargetStopPairs; }

    unsigned int getNumPermutations() const { return mNumPermutations; }

    unsigned int getMinNumStratsFullPeriod() const { return mMinNumStratsFullPeriod; }

    unsigned int getMinNumStratsBeforeValidation() const { return mMinNumStratsBeforeValidation; }

    const Decimal& getPalProfitabilitySafetyFactor() const { return mPalSafetyFactor; }

    const Decimal& getStepRedundancyMultiplier() const { return mStepRedundancyMultiplier; }

    const Decimal& getSurvivalFilterMultiplier() const {return mSurvivalFilterMultiplier; }

  private:
    unsigned int mMaxDepth;
    unsigned int mMinTrades;
    Decimal mActivityMultiplier;
    unsigned int mPassingStratNumPerRound;
    Decimal mProfitFactorCriterion;
    unsigned int mMaxConsecutiveLosers;
    unsigned int mMaxInactivitySpan;
    std::vector<std::pair<Decimal, Decimal>> mTargetStopPairs;
    unsigned int mNumPermutations;
    unsigned int mMinNumStratsFullPeriod;
    unsigned int mMinNumStratsBeforeValidation;
    Decimal mPalSafetyFactor;
    Decimal mStepRedundancyMultiplier;
    Decimal mSurvivalFilterMultiplier;
  };

  class SearchAlgoConfigurationFileReader
  {
    using Decimal = num::DefaultNumber;

  public:
    SearchAlgoConfigurationFileReader (const std::shared_ptr<RunParameters>& runParameters);
    ~SearchAlgoConfigurationFileReader()
      {}

    //template <class SecurityT>
    std::shared_ptr<SearchAlgoConfiguration<Decimal>> readConfigurationFile(
                const std::shared_ptr<McptConfiguration<Decimal>>& mcptConfiguration);

  private:
    std::shared_ptr<RunParameters> mRunParameters;
  };
}

#endif //SEARCHALGOCONFIGURATIONFILEREADER_H
