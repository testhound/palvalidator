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

    const std::vector<StrategyRepresentationType>& getSurvivors() const {return mSurvivors;}

    size_t getNumSurvivors() const { return mSurvivors.size(); }

  private:
    std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>> mSinglePA;
    std::vector<StrategyRepresentationType> mSurvivors;

  };

}


#endif // SURVIVINGSTRATEGIESCONTAINER_H
