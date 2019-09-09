
#ifndef SEARCHALGOCONFIGURATIONFILEREADER_H
#define SEARCHALGOCONFIGURATIONFILEREADER_H

#include <string>
#include <memory>
#include <exception>
#include <vector>
//#include <boost/date_time.hpp>
//#include "DateRange.h"
//#include "BackTester.h"
//#include "PalAst.h"
#include "number.h"
#include "TimeSeries.h"

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
        std::vector<time_t> timeFrames,
        const std::shared_ptr<OHLCTimeSeries<Decimal>>& series,
        unsigned int numPermutations, unsigned int minNumStratsFullPeriod, unsigned int minNumStratsBeforeValidation)
      :
      mMaxDepth(maxDepth),
      mMinTrades(minTrades),
      mSortMultiplier(sortMultiplier),
      mPassingStratNumPerRound(passingStratNumPerRound),
      mProfitFactorCriterion(profitFactorCriterion),
      mMaxConsecutiveLosers(maxConsecutiveLosers),
      mMaxInactivitySpan(maxInactivitySpan),
      mTargetStopPairs(targetStopPairs),
      mTimeFrames(timeFrames),
      mSeries(series),
      mNumPermutations(numPermutations),
      mMinNumStratsFullPeriod(minNumStratsFullPeriod),
      mMinNumStratsBeforeValidation(minNumStratsBeforeValidation)
    {}

    SearchAlgoConfiguration (const SearchAlgoConfiguration& rhs)
      : mMaxDepth(rhs.mMaxDepth),
        mMinTrades(rhs.mMinTrades),
        mSortMultiplier(rhs.mSortMultiplier),
        mPassingStratNumPerRound(rhs.mPassingStratNumPerRound),
        mProfitFactorCriterion(rhs.mProfitFactorCriterion),
        mMaxConsecutiveLosers(rhs.mMaxConsecutiveLosers),
        mMaxInactivitySpan(rhs.mMaxInactivitySpan),
        mTargetStopPairs(rhs.mTargetStopPairs),
        mTimeFrames(rhs.mTimeFrames),
        mSeries(rhs.mSeries),
        mNumPermutations(rhs.mNumPermutations),
        mMinNumStratsFullPeriod(rhs.mMinNumStratsFullPeriod),
        mMinNumStratsBeforeValidation(rhs.mMinNumStratsBeforeValidation)
    {}

    SearchAlgoConfiguration<Decimal>&
    operator=(const SearchAlgoConfiguration<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	mMaxDepth = rhs.mMaxDepth;
	mMinTrades = rhs.mMinTrades;
	mSortMultiplier = rhs.mSortMultiplier;
	mPassingStratNumPerRound = rhs.mPassingStratNumPerRound;
	mProfitFactorCriterion = rhs.mProfitFactorCriterion;
	mMaxConsecutiveLosers = rhs.mMaxConsecutiveLosers;
	mMaxInactivitySpan = rhs.mMaxInactivitySpan;
	mTargetStopPairs = rhs.mTargetStopPairs;
	mTimeFrames = rhs.mTimeFrames;
	mSeries = rhs.mSeries;
	mNumPermutations = rhs.mNumPermutations;
	mMinNumStratsFullPeriod = rhs.mMinNumStratsFullPeriod;
	mMinNumStratsBeforeValidation = rhs.mMinNumStratsBeforeValidation;

      return *this;
    }

    ~SearchAlgoConfiguration()
    {}

    inline friend std::ostream& operator<< (std::ostream& strng, const SearchAlgoConfiguration<Decimal>& obj)
    {
      return strng << "SearchAlgo Configs:: Depth: " << obj.mMaxDepth << ", MinTrades: " << obj.mMinTrades << ", SortMultiplier: " << obj.mSortMultiplier << ", PassingStratNumPerRound: " << obj.mPassingStratNumPerRound
                   << ", ProfitFactorCriterion: " << obj.mProfitFactorCriterion << ", MaxConsecutiveLosers: " << obj.mMaxConsecutiveLosers << ", MaxInactivitySpan: " << obj.mMaxInactivitySpan << ", Targets&Stops#: "
                   << obj.mTargetStopPairs.size() << ", TimeFrames#: " << obj.mTimeFrames.size()
                   << "\nValidation settings -- # of permutations: " << obj.mNumPermutations << ", Min # of strats full period: " << obj.mMinNumStratsFullPeriod << ", Min # of strats before validation: " << obj.mMinNumStratsBeforeValidation;
    }

    unsigned int getMaxDepth() const { return mMaxDepth; }

    unsigned int getMinTrades() const { return mMinTrades; }

    const Decimal& getSortMultiplier() const { return mSortMultiplier; }

    unsigned int getPassingStratNumPerRound() const { return mPassingStratNumPerRound; }

    const Decimal& getProfitFactorCriterion() const { return mProfitFactorCriterion; }

    unsigned int getMaxConsecutiveLosers() const { return mMaxConsecutiveLosers; }

    unsigned int getMaxInactivitySpan() const { return mMaxInactivitySpan; }

    typename std::vector<std::pair<Decimal, Decimal>>::const_iterator targetStopPairsBegin() const { return mTargetStopPairs.begin(); }

    typename std::vector<std::pair<Decimal, Decimal>>::const_iterator targetStopPairsEnd() const { return mTargetStopPairs.end(); }

    const std::vector<std::pair<Decimal, Decimal>>& getTargetStopPair() const { return mTargetStopPairs; }

    typename std::vector<time_t>::const_iterator timeFramesBegin() const { return mTimeFrames.begin(); }

    typename std::vector<time_t>::const_iterator timeFramesEnd() const { return mTimeFrames.end(); }

    size_t getNumTimeFrames() const { return mTimeFrames.size(); }

    unsigned int getNumPermutations() const { return mNumPermutations; }

    unsigned int getMinNumStratsFullPeriod() const { return mMinNumStratsFullPeriod; }

    unsigned int getMinNumStratsBeforeValidation() const { return mMinNumStratsBeforeValidation; }

    const std::shared_ptr<OHLCTimeSeries<Decimal>>& getTimeSeries() const { return mSeries; }

  private:
    unsigned int mMaxDepth;
    unsigned int mMinTrades;
    Decimal mSortMultiplier;
    unsigned int mPassingStratNumPerRound;
    Decimal mProfitFactorCriterion;
    unsigned int mMaxConsecutiveLosers;
    unsigned int mMaxInactivitySpan;
    std::vector<std::pair<Decimal, Decimal>> mTargetStopPairs;
    std::vector<time_t> mTimeFrames;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSeries;
    unsigned int mNumPermutations;
    unsigned int mMinNumStratsFullPeriod;
    unsigned int mMinNumStratsBeforeValidation;
  };

  class SearchAlgoConfigurationFileReader
  {
    using Decimal = num::DefaultNumber;

  public:
    SearchAlgoConfigurationFileReader (const std::string& configurationFileName);
    ~SearchAlgoConfigurationFileReader()
      {}

    template <class SecurityT>
    std::shared_ptr<SearchAlgoConfiguration<Decimal>> readConfigurationFile(const SecurityT & security, int timeFrameIdToLoad);

  private:
    std::string mConfigurationFileName;
  };
}

#endif //SEARCHALGOCONFIGURATIONFILEREADER_H
