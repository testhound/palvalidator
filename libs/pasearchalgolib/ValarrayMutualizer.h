// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, September-October 2019

#ifndef VALARRAYMUTUALIZER_H
#define VALARRAYMUTUALIZER_H

#include "SteppingPolicy.h"

namespace mkc_searchalgo
{


  template <class Decimal, class TSearchAlgoBacktester>
  class ValarrayMutualizer
  {
  public:
    ValarrayMutualizer(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy,
                       const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& singlePA,
                       size_t passingStratNumPerRound,
                       Decimal sortMultiplier):
      mProcessingPolicy(processingPolicy),
      mSortedResults(processingPolicy->getResults()),
      mStratMap(processingPolicy->getStrategyMap()),
      mSinglePA(singlePA),
      mSumSelected(DecimalConstants<Decimal>::DecimalZero, singlePA->getDateCount()),
      mSelectCount(passingStratNumPerRound)
    {}

  protected:
    std::vector<StrategyRepresentationType> passes()
    {
      //sort by PALProfitability before any operation
      mProcessingPolicy->template sortResults<Sorters::PALProfitabilitySorter<Decimal>>();
      getMaxRelMinRed();
      return mSelectedStrategies;
    }

  private:

    void getMaxRelMinRed()
    {
      while (mSelectedStrategies.size() < mSelectCount)
        {
          Decimal maxScore = DecimalConstants<Decimal>::DecimalMinusOne;
          StrategyRepresentationType bestStrat;
          std::valarray<Decimal> bestTrading;
          for (const std::tuple<ResultStat<Decimal>, unsigned int, int>& tup: mSortedResults)
            {
              int ind = std::get<2>(tup);
              // do not pass "perfect" or "useless" strategies
              const ResultStat<Decimal>& stat = std::get<0>(tup);

              if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero)
                continue;

              const StrategyRepresentationType& strat = mStratMap[ind];
              //already in
              if (findInVector(mSelectedStrategies, strat))
                continue;

              Decimal relevance = stat.PALProfitability;
              if (maxScore > relevance)       //there is no need to search more, as it is impossible to improve the score
                break;

              std::valarray<Decimal> trading = getTrading(strat);
              //just pick the top strategy as seed
              if (mSelectedStrategies.size() == 0)
                {
                  bestStrat = strat;
                  bestTrading = trading;
                  break;
                }
              std::valarray<Decimal> avgSelected = mSumSelected / Decimal(static_cast<double>(mSelectedStrategies.size()));
              Decimal redundancy = getRedundancy(avgSelected, trading);
              Decimal score = relevance - redundancy;
              if (score > maxScore)
                {
                  bestStrat = strat;
                  bestTrading = trading;
                  maxScore = score;
                  std::cout << "Round : " << mSelectedStrategies.size() << " -- relevance: " << relevance << ", redundancy: " << redundancy << ", new score: " << maxScore << std::endl;
                }
            }
          if (mSelectedStrategies.size() > 0 && maxScore == DecimalConstants<Decimal>::DecimalMinusOne)   //nothing more to search
            break;
          //add selected strategy
          std::cout << "Round : " << mSelectedStrategies.size() << " adding strategy with score: " << maxScore << std::endl;
          mSumSelected += bestTrading;
          mSelectedStrategies.push_back(bestStrat);
        }
    }

    ///
    /// \brief getRedundancy - calculates a simplified mutual info-based redundancy between two valarrays of trading
    ///         0 means no trading, 1 means trading. However, the baseArray is an average,
    ///         so values between 0 and 1 can appear as well
    /// \param baseArray - the average trading-non-trading score for each day for all strategies selected so far
    /// \param newArray - the trading-array to compare with
    /// \return a quasi-mutual info score for redundancy (1 is fully redundant 0 is no redundancy)
    ///
    Decimal getRedundancy(const std::valarray<Decimal>& baseArray, const std::valarray<Decimal>& newArray)
    {
      //std::cout << "base_array sum: " << baseArray.sum() << ", newArray sum: " << newArray.sum() << std::endl;
      std::valarray<Decimal> diff = (baseArray - newArray);
      //std::cout << "diff before abs: " << diff.sum() << std::endl;
      diff = diff.apply([](const Decimal& val)->Decimal {
          return val.abs();
        });
      //std::cout << "diff after abs: " << diff.sum() << std::endl;
      Decimal avgDiff(diff.sum() / Decimal(static_cast<double>(diff.size())));                      //avg (of abs diff) rather than squared/root squared in order to gain a more continuous 0-1 range
      //std::cout << "avg diff: " << avgDiff << std::endl;
      return (DecimalConstants<Decimal>::DecimalOne - avgDiff);
    }

    ///
    /// \brief getTrading - retrieves trading rep for a given strategy
    /// \param strat - strategy as represented by vector of int indices
    /// \return valarray of trading either 0 for no trade, or 1 for trade
    ///
    std::valarray<Decimal> getTrading(const StrategyRepresentationType & strat)
    {
      std::valarray<Decimal> occurences(DecimalConstants<Decimal>::DecimalOne, mSinglePA->getDateCount());
      //the multiplication
      for (unsigned int el: strat)
        occurences *= mSinglePA->getMappedElement(el);
      return occurences;
    }

  private:
    const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& mProcessingPolicy;
    const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& mSortedResults;
    std::unordered_map<int, StrategyRepresentationType>& mStratMap;
    const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& mSinglePA;
    std::valarray<Decimal> mSumSelected;
    size_t mSelectCount;
    std::vector<StrategyRepresentationType> mSelectedStrategies;

  };

}

#endif // VALARRAYMUTUALIZER_H
