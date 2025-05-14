// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef SURVIVINGSTRATEGIESCONTAINER_H
#define SURVIVINGSTRATEGIESCONTAINER_H

#include "SurvivalPolicy.h"

namespace mkc_searchalgo
{

  template <class Decimal, class TComparison>
  class SurvivingStrategiesContainer
  {

  public:
    SurvivingStrategiesContainer(
        std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>>& singlePA):
      mSinglePA(singlePA),
      mSurvivors()
    {}

    void addSurvivorsPerRound(std::vector<StrategyRepresentationType> roundSurvivors)
    {
      std::cout << "Adding survivors in new round, items already in: " << mSurvivors.size() << std::endl;
      mSurvivors.insert(mSurvivors.end(), roundSurvivors.begin(), roundSurvivors.end());
      std::cout << "After adding survivors in new round, items in now: " << mSurvivors.size() << std::endl;
    }

    void addStatisticsPerRound(const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& roundStatistics)
    {
      mStatistics.insert(mStatistics.end(), roundStatistics.begin(), roundStatistics.end());
    }

    template<class TSearchAlgoBacktester>
    void removeRedundant(ValarrayMutualizer<Decimal, TSearchAlgoBacktester>& mutualizer)
    {
        std::cout << "REMOVING REDUNDANT in compiled survival strategies container." << std::endl;
        mutualizer.getMaxRelMinRed(mStatistics, mStatistics.size(), 0.0, 1.0, 1.0);
        mSurvivors = mutualizer.getSelectedStrategies();
        mStatistics = mutualizer.getSelectedStatistics();
    }

    std::vector<std::vector<ComparisonEntryType>> getSurvivorsAsComparisons()
    {
      std::vector<std::vector<ComparisonEntryType>> ret;
      for (const StrategyRepresentationType& strat: mSurvivors)
        {
          std::vector<ComparisonEntryType> comparisonStrat;
          for (unsigned int el: strat)
            {
              const ComparisonEntryType& entry = mSinglePA->getUnderlying(el);
              comparisonStrat.push_back(entry);
            }
          ret.push_back(comparisonStrat);
        }
      return ret;
    }

    const std::vector<StrategyRepresentationType>& getSurvivors() const { return mSurvivors; }

    const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& getStatistics() const { return mStatistics; }

    size_t getNumSurvivors() const { return mSurvivors.size(); }

  private:
    std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>> mSinglePA;
    std::vector<StrategyRepresentationType> mSurvivors;
    std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>> mStatistics;

  };

}


#endif // SURVIVINGSTRATEGIESCONTAINER_H
