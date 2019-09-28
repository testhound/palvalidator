// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, September-October 2019

#ifndef VALARRAYMUTUALIZER_H
#define VALARRAYMUTUALIZER_H

#include "BacktestProcessor.h"
#include <chrono>

using namespace std::chrono;
namespace mkc_searchalgo
{

//  // A hash function used to hash a pair of any kind
//  struct hash_pair {
//      template <class T1, class T2>
//      size_t operator()(const pair<T1, T2>& p) const
//      {
//          auto hash1 = hash<T1>{}(p.first);
//          auto hash2 = hash<T2>{}(p.second);
//          return hash1 ^ hash2;
//      }
//  };

  template <class Decimal, class TSearchAlgoBacktester>
  class ValarrayMutualizer
  {
  public:
    ValarrayMutualizer(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy,
                       const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& singlePA,
                       const std::string& runType):
      mStratMap(processingPolicy->getStrategyMap()),
      mSinglePA(singlePA),
      mRunType(runType)
    {
      std::cout << mRunType << " - Building mutual info matrix." << std::endl;
      for (size_t i = 0; i < mSinglePA->getMapSize(); i++)
        {
          for (size_t c = 0; c < mSinglePA->getMapSize(); c++)
            {
              const std::valarray<Decimal>& v1 = mSinglePA->getMappedElement(i);
              const std::valarray<Decimal>& v2 = mSinglePA->getMappedElement(c);
              Decimal red = getRedundancy(v1, v2);
              mIndividuals.insert(std::make_pair((i < c)?(i*10000 + c):(c*10000 + i), red.getAsDouble()));
            }
        }
        std::cout << mRunType << " - Built mutual info matrix of size: " << mIndividuals.size() << std::endl;
    }

  public:

    void getMaxRelMinRed2(const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& sortedResults,
                          unsigned int selectCount, double activityMult, double redundancyMult, double redundancyFilter)
    {
      //clearing
      mSelectedStrategies.clear();
      mSelectedStrategies.shrink_to_fit();
      mIndexedSums.clear();
      double bestRelevance;
      double bestRedundancy;
      double bestActivity;
      int maxIndexToSearch = sortedResults.size();
      while (mSelectedStrategies.size() < selectCount)
        {
          int index = -1;
          double maxScore = -1.0;
          bool first = true;
          StrategyRepresentationType bestStrat;
          for (const std::tuple<ResultStat<Decimal>, unsigned int, int>& tup: sortedResults)
            {
              int ind = std::get<2>(tup);
              unsigned int trades = std::get<1>(tup);
              const ResultStat<Decimal>& stat = std::get<0>(tup);
              //increment index immediately
              index++;

              if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero)
                continue;

              const StrategyRepresentationType& strat = mStratMap[ind];

              if (findInVector(mSelectedStrategies, strat))
                continue;

              double relevance = stat.PALProfitability.getAsDouble();
              double activity = (trades * activityMult) / mSinglePA->getMapSize();

              if (maxScore > relevance + activityMult*0.5 || index >= maxIndexToSearch)       //there is no need to search more, as it is (almost) impossible to improve the score
                {
                  if (mSelectedStrategies.size() == 1)
                    maxIndexToSearch = index;
                  break;
                }

              //just pick the top strategy as seed
              if (mSelectedStrategies.size() == 0)
                {
                  bestStrat = strat;
                  break;
                }
              double redundancy;
              if (mSelectedStrategies.size() == 1)
                redundancy = initRedundancy(index, mSelectedStrategies.back(), strat) * redundancyMult;
              else
                redundancy = getRedundancy(index, strat) * redundancyMult;

              if (redundancy > redundancyFilter * redundancyMult)
                  continue;


              double score = relevance + activity - redundancy;
              if (score > maxScore)
                {
                  first = false;
                  bestStrat = strat;
                  maxScore = score;
                  bestActivity = activity;
                  bestRelevance = relevance;
                  bestRedundancy = redundancy;
//                  std::cout << "Round : " << mSelectedStrategies.size() << " adding strategy with score: " << maxScore
//                            << ", relevance: " << bestRelevance << ", activity: :" << bestActivity << ", redundancy: " << bestRedundancy << std::endl;
                }

            }
          if (mSelectedStrategies.size() > 0 && first)   //nothing to add
            { ; }
          else {
              //add selected strategy
              std::cout << mRunType << " - Round : " << mSelectedStrategies.size() << " adding strategy with score: " << maxScore
                        << ", relevance: " << bestRelevance << ", activity: :" << bestActivity << ", redundancy: " << bestRedundancy << std::endl;
              //mSelectedGroup.insert(mSelectedGroup.begin(), bestStrat.begin(), bestStrat.end());
              mSelectedStrategies.push_back(bestStrat);
              //mSelectedRelActRed.push_back(std::make_tuple(bestRelevance, bestActivity, bestRedundancy));
            }
        }
    }

//    void getMaxRelMinRed1()
//    {
//      while (mSelectedStrategies.size() < mSelectCount)
//        {
//          Decimal maxScore = DecimalConstants<Decimal>::DecimalMinusOne;
//          StrategyRepresentationType bestStrat;
//          std::valarray<Decimal> bestTrading;
//          for (const std::tuple<ResultStat<Decimal>, unsigned int, int>& tup: mSortedResults)
//            {
//              int ind = std::get<2>(tup);
//              // do not pass "perfect" or "useless" strategies
//              const ResultStat<Decimal>& stat = std::get<0>(tup);

//              if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero)
//                continue;

//              const StrategyRepresentationType& strat = mStratMap[ind];
//              //already in
//              if (findInVector(mSelectedStrategies, strat))
//                continue;

//              Decimal relevance = stat.PALProfitability;
//              if (maxScore > relevance)       //there is no need to search more, as it is impossible to improve the score
//                break;

//              std::valarray<Decimal> trading = getTrading(strat);
//              //just pick the top strategy as seed
//              if (mSelectedStrategies.size() == 0)
//                {
//                  bestStrat = strat;
//                  bestTrading = trading;
//                  break;
//                }
//              std::valarray<Decimal> avgSelected = mSumSelected / Decimal(static_cast<double>(mSelectedStrategies.size()));
//              Decimal redundancy = getRedundancy(avgSelected, trading);
//              Decimal score = relevance - redundancy;
//              if (score > maxScore)
//                {
//                  bestStrat = strat;
//                  bestTrading = trading;
//                  maxScore = score;
//                  std::cout << "Round : " << mSelectedStrategies.size() << " -- relevance: " << relevance << ", redundancy: " << redundancy << ", new score: " << maxScore << std::endl;
//                }
//            }
//          if (mSelectedStrategies.size() > 0 && maxScore == DecimalConstants<Decimal>::DecimalMinusOne)   //nothing more to search
//            break;
//          //add selected strategy
//          std::cout << "Round : " << mSelectedStrategies.size() << " adding strategy with score: " << maxScore << std::endl;
//          mSumSelected += bestTrading;
//          mSelectedStrategies.push_back(bestStrat);
//        }
//    }

  private:

    double initRedundancy(int index, const StrategyRepresentationType& strat1, const StrategyRepresentationType& strat2)
    {
      double sumRed = 0.0;
      int cnt = 0;
      for (unsigned int i: strat1)
        {
          for (unsigned int c: strat2)
            {
              double red = mIndividuals[(i < c)?(i*10000 + c):(c*10000 + i)];
              sumRed += red;
              cnt++;
            }
        }
        mIndexedSums[index] = sumRed;
        return (sumRed / cnt);
    }

    double getRedundancy(int index, const StrategyRepresentationType& strat2)
    {
      //the last added strategy is the only one missing from the recorded sum
      const StrategyRepresentationType& strat1 = mSelectedStrategies.back();
      double sumRed = 0.0;
      int cnt = 0;
      for (unsigned int i: strat1)
        {
          for (unsigned int c: strat2)
            {
              double red = mIndividuals[(i < c)?(i*10000 + c):(c*10000 + i)];
              sumRed += red;
              cnt++;
            }
        }
      double& origsum = mIndexedSums[index];
      origsum += sumRed;
      return (origsum / (mSelectedStrategies.size()*strat1.size()*strat1.size() + cnt));
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

  public:
    const std::vector<StrategyRepresentationType>& getSelectedStrategies() const { return mSelectedStrategies; }
    //const std::vector<std::tuple<double,double,double>>& getRelActRed() const {return mSelectedRelActRed; }

  private:
    std::unordered_map<int, StrategyRepresentationType>& mStratMap;
    const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& mSinglePA;
    std::vector<StrategyRepresentationType> mSelectedStrategies;
    std::unordered_map<unsigned int, double> mIndividuals;
    std::unordered_map<int, double> mIndexedSums;
    std::string mRunType;
    //std::vector<std::tuple<double, double, double>> mSelectedRelActRed;

  };

}

#endif // VALARRAYMUTUALIZER_H
