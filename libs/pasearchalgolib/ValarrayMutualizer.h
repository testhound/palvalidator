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
              mIndividualRedundancyPairValues.insert(std::make_pair((i < c)?(i*10000 + c):(c*10000 + i), red.getAsDouble()));
            }
        }
        std::cout << mRunType << " - Built mutual info matrix of size: " << mIndividualRedundancyPairValues.size() << std::endl;
    }

  public:

    void getMaxRelMinRed(const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& sortedResults,
                          unsigned int selectCount, double activityMult, double redundancySeedMultiplier, double redundancyFilter, Decimal inverseSurvivalFilter = DecimalConstants<Decimal>::DecimalZero)
    {
      //Asserts floating point compatibility at compile time
      static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
      std::cout << "getMaxRelMinRed was called with results#: " << sortedResults.size() << ", selectCount: " << selectCount << ", activityMult: " << activityMult
                << ", redundancySeedMult: " << redundancySeedMultiplier << ", redundancyFilter: " << redundancyFilter << ", inverseSurvivalFilter: " << inverseSurvivalFilter << std::endl;

      if (Decimal(redundancyFilter) > DecimalConstants<Decimal>::DecimalOne || Decimal(redundancyFilter) <= DecimalConstants<Decimal>::DecimalZero){
          throw std::logic_error("Redundancy Filter needs to be between 0 and 1, provided: " + std::to_string(redundancyFilter));
        }
      //clearing
      mSelectedStrategies.clear();
      mSelectedStrategies.shrink_to_fit();
      mSelectedStatistics.clear();
      mSelectedStatistics.shrink_to_fit();
      mIndexedSums.clear();
      double bestRelevance;
      double bestRedundancy = 0.0;
      double bestActivity;
      int maxIndexToSearch = sortedResults.size();
      double breakOut = false;
      double redundancyMult = redundancySeedMultiplier;
      while (mSelectedStrategies.size() < selectCount && !breakOut)
        {
          int index = -1;
          double maxScore = -std::numeric_limits<double>::infinity();
          bool first = true;
          //redundancyMult += redundancySeedMultiplier * 0.5;
          redundancyMult = redundancySeedMultiplier;
          //std::cout << "New redundancy mult: " << redundancyMult << std::endl;
          StrategyRepresentationType bestStrat;
          std::tuple<ResultStat<Decimal>, unsigned int, int> selectedStatistics;
          for (const std::tuple<ResultStat<Decimal>, unsigned int, int>& tup: sortedResults)
            {
              int ind = std::get<2>(tup);
              unsigned int trades = std::get<1>(tup);
              const ResultStat<Decimal>& stat = std::get<0>(tup);
              //increment index immediately
              index++;

              if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero)
                continue;

              if (inverseSurvivalFilter > DecimalConstants<Decimal>::DecimalZero && stat.ProfitFactor > inverseSurvivalFilter)
                continue;

              const StrategyRepresentationType& strat = mStratMap[ind];

              if (findInVector(mSelectedStrategies, strat))
                continue;

              double relevance = stat.PALProfitability.getAsDouble();
              double activity = (trades * activityMult) / mSinglePA->getDateCount();
//              if (activity > 1.0)
//                {
//                  std::cout << "Incorrect acvitity (" << activity << ")-- trades: " << trades << ", activytMult: " << activityMult << ", mapsize: " << mSinglePA->getDateCount() << std::endl;
//                  throw;
//                }

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
                  selectedStatistics = tup;
                  break;
                }
              double redundancy;
              if (mSelectedStrategies.size() == 1)
                redundancy = initRedundancyMax(index, mSelectedStrategies.back(), strat) * redundancyMult;
              else
                redundancy = getRedundancyMax(index, strat) * redundancyMult;

              if (redundancy >= redundancyFilter * redundancyMult)
                {
                  //std::cout << "redundancy: " << redundancy << " exceeds filter: " << (redundancyFilter * redundancyMult)<< ", breaking out. " << std::endl;
                  continue;
                }

              double score = relevance + activity - redundancy;
              if (score > maxScore)
                {
                  first = false;
                  bestStrat = strat;
                  selectedStatistics = tup;
                  maxScore = score;
                  bestActivity = activity;
                  bestRelevance = relevance;
                  bestRedundancy = redundancy;
                  std::cout << "Round : " << mSelectedStrategies.size() << " considering strategy with score: " << maxScore
                            << ", relevance: " << bestRelevance << ", activity: :" << bestActivity << ", redundancy: " << bestRedundancy << std::endl;
                }

            }
          if (mSelectedStrategies.size() > 0 && first)   //nothing to add
            {
              breakOut = true;
            }
          else
            {
              //add selected strategy
              if (mSelectedStrategies.size() % 100 == 0)
                {
                  std::cout << mRunType << " - Round : " << mSelectedStrategies.size() << " adding strategy with score: " << maxScore
                            << ", relevance: " << bestRelevance << ", activity: " << bestActivity << ", redundancy: " << bestRedundancy << ", redundancy Mult: "
                            << redundancyMult << ", adjusted redundancy: " << (bestRedundancy / redundancyMult) << std::endl;
                }
              //mSelectedGroup.insert(mSelectedGroup.begin(), bestStrat.begin(), bestStrat.end());
              mSelectedStrategies.push_back(bestStrat);
              mSelectedStatistics.push_back(selectedStatistics);
              //mSelectedRelActRed.push_back(std::make_tuple(bestRelevance, bestActivity, bestRedundancy));
            }
        }
    }

  private:

    double initRedundancyMax(int index, const StrategyRepresentationType& strat1, const StrategyRepresentationType& strat2)
    {
      double maxRed = 0.0;
      for (unsigned int i: strat1)
        {
          for (unsigned int c: strat2)
            {
              double red = mIndividualRedundancyPairValues[(i < c)?(i*10000 + c):(c*10000 + i)];
              if (red > maxRed){
                  maxRed = red;
                }
            }
        }
        mIndexedSums[index] = maxRed;
        return maxRed;
    }

    double getRedundancyMax(int index, const StrategyRepresentationType& strat2)
    {
      //the last added strategy is the only one missing from the recorded sum
      const StrategyRepresentationType& strat1 = mSelectedStrategies.back();
      double maxRed = 0.0;
      for (unsigned int i: strat1)
        {
          for (unsigned int c: strat2)
            {
              double red = mIndividualRedundancyPairValues[(i < c)?(i*10000 + c):(c*10000 + i)];
              if (red > maxRed){
                  maxRed = red;
                }
            }
        }
      double& originalMax = mIndexedSums[index];
      originalMax = std::max(originalMax, maxRed);
      return originalMax;
    }

    double initRedundancy(int index, const StrategyRepresentationType& strat1, const StrategyRepresentationType& strat2)
    {
      double sumRed = 0.0;
      int cnt = 0;
      for (unsigned int i: strat1)
        {
          for (unsigned int c: strat2)
            {
              double red = mIndividualRedundancyPairValues[(i < c)?(i*10000 + c):(c*10000 + i)];
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
              double red = mIndividualRedundancyPairValues[(i < c)?(i*10000 + c):(c*10000 + i)];
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
    const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& getSelectedStatistics() const { return mSelectedStatistics; }

  private:
    std::unordered_map<int, StrategyRepresentationType>& mStratMap;
    const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& mSinglePA;
    std::vector<StrategyRepresentationType> mSelectedStrategies;
    std::unordered_map<unsigned int, double> mIndividualRedundancyPairValues;
    std::unordered_map<int, double> mIndexedSums;
    std::string mRunType;
    std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>> mSelectedStatistics;
    //std::vector<std::tuple<double, double, double>> mSelectedRelActRed;

  };

}

#endif // VALARRAYMUTUALIZER_H
