
#ifndef SEARCHALGOCONFIGURATIONFILEREADER_H
#define SEARCHALGOCONFIGURATIONFILEREADER_H

#include <string>
#include <memory>
#include <exception>
//#include <boost/date_time.hpp>
//#include "Security.h"
//#include "DateRange.h"
//#include "BackTester.h"
//#include "PalAst.h"
#include "number.h"

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
        unsigned int maxInactivitySpan)
      :
      mMaxDepth(maxDepth),
      mMinTrades(minTrades),
      mSortMultiplier(sortMultiplier),
      mPassingStratNumPerRound(passingStratNumPerRound),
      mProfitFactorCriterion(profitFactorCriterion),
      mMaxConsecutiveLosers(maxConsecutiveLosers),
      mMaxInactivitySpan(maxInactivitySpan)
    {}

    SearchAlgoConfiguration (const SearchAlgoConfiguration& rhs)
      : mMaxDepth(rhs.mMaxDepth),
        mMinTrades(rhs.mMinTrades),
        mSortMultiplier(rhs.mSortMultiplier),
        mPassingStratNumPerRound(rhs.mPassingStratNumPerRound),
        mProfitFactorCriterion(rhs.mProfitFactorCriterion),
        mMaxConsecutiveLosers(rhs.mMaxConsecutiveLosers),
        mMaxInactivitySpan(rhs.mMaxInactivitySpan)
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

      return *this;
    }

    ~SearchAlgoConfiguration()
    {}


    unsigned int getMaxDepth() const { return mMaxDepth; }

    unsigned int getMinTrades() const { return mMinTrades; }

    const Decimal& getSortMultiplier() const { return mSortMultiplier; }

    unsigned int getPassingStratNumPerRound() const { return mPassingStratNumPerRound; }

    const Decimal& getProfitFactorCriterion() const { return mProfitFactorCriterion; }

    unsigned int getMaxConsecutiveLosers() const { return mMaxConsecutiveLosers; }

    unsigned int getMaxInactivitySpan() const { return mMaxInactivitySpan; }

  private:
    unsigned int mMaxDepth;
    unsigned int mMinTrades;
    Decimal mSortMultiplier;
    unsigned int mPassingStratNumPerRound;
    Decimal mProfitFactorCriterion;
    unsigned int mMaxConsecutiveLosers;
    unsigned int mMaxInactivitySpan;
  };

  class SearchAlgoConfigurationFileReader
  {
    using Decimal = num::DefaultNumber;

  public:
    SearchAlgoConfigurationFileReader (const std::string& configurationFileName);
    ~SearchAlgoConfigurationFileReader()
      {}

    std::shared_ptr<SearchAlgoConfiguration<Decimal>> readConfigurationFile();

  private:
    std::string mConfigurationFileName;
  };
}

#endif //SEARCHALGOCONFIGURATIONFILEREADER_H
