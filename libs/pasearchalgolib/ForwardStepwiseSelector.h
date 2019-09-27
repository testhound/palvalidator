// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef FORWARDSTEPWISESELECTOR_H
#define FORWARDSTEPWISESELECTOR_H

#include <iostream>
#include <algorithm>
#include "UniqueSinglePAMatrix.h"
//#include "ComparisonToPalStrategy.h"
#include <chrono>
#include "BacktestProcessor.h"
#include "ShortcutSearchAlgoBacktester.h"
#include "SteppingPolicy.h"
#include "ValarrayMutualizer.h"
#include "SurvivalPolicy.h"
#include "SurvivingStrategiesContainer.h"

namespace mkc_searchalgo {

  template <class Decimal,
            typename TComparison = std::valarray<Decimal>,
            typename TSearchAlgoBacktester = ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>,
            //typename TSteppingPolicy = SimpleSteppingPolicy<Decimal, TSearchAlgoBacktester, Sorters::CombinationPPSorter<Decimal>>,
            typename TSteppingPolicy = ValarrayMutualizer<Decimal, TSearchAlgoBacktester>,
            typename TSurvivalPolicy = DefaultSurvivalPolicy<Decimal, TSearchAlgoBacktester>
            >
  class ForwardStepwiseSelector: private TSteppingPolicy, private TSurvivalPolicy
  {
  public:
    ForwardStepwiseSelector(std::shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& backtestProcessor ,
                            std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>>& singlePA,
                            const std::shared_ptr<SearchAlgoConfiguration<Decimal>>& searchConfiguration,
                            Decimal targetStopRatio,
                            std::shared_ptr<SurvivingStrategiesContainer<Decimal, std::valarray<Decimal>>>& survivingContainer):
      TSteppingPolicy(backtestProcessor, singlePA, searchConfiguration->getPassingStratNumPerRound(), searchConfiguration->getSortMultiplier()),
      TSurvivalPolicy(backtestProcessor, searchConfiguration->getProfitFactorCriterion(), targetStopRatio, searchConfiguration->getMaxConsecutiveLosers(), searchConfiguration->getPalProfitabilitySafetyFactor()),
      mBacktestProcessor(backtestProcessor),
      mSinglePa(singlePA),
      mMinTrades(searchConfiguration->getMinTrades()),
      mMaxDepth(searchConfiguration->getMaxDepth() - 1),
      mRuns(0),
      mSurvivingContainer(survivingContainer)
    {
    }
    ~ForwardStepwiseSelector()
    {}

    void runSteps()
    {
      step(mMaxDepth);
    }

  private:

    //bottom step
    std::vector<StrategyRepresentationType> step(unsigned int stepNo)
    {
      if (stepNo == 0)
        {
          for (unsigned int i = 0; i < mSinglePa->getMapSize(); ++i)
            {
              for (unsigned int c = 0; c < mSinglePa->getMapSize(); ++c)
                {
                  if (i == c)
                    continue;
                  std::vector<unsigned int> stratVect {i, c};
                  mBacktestProcessor->processResult(stratVect);
                }
              if (i % 100 == 0)
                std::cout << "Step 0 comparison, element group: " << i << std::endl;
            }
          std::cout << "finished and returning from level 0, processed results: " << mBacktestProcessor->getResults().size() << std::endl;
          std::vector<StrategyRepresentationType> newret = TSteppingPolicy::passes();
          TSurvivalPolicy::filterSurvivors();
          mSurvivingContainer->addSurvivorsPerRound(TSurvivalPolicy::getUniqueSurvivors());

          std::cout << "After step 0: Number of survivors: " << TSurvivalPolicy::getNumSurvivors() << std::endl;
          mBacktestProcessor->clearAll();
          TSurvivalPolicy::clearRound();
          return newret;
        }

      std::vector<StrategyRepresentationType> ret(step(stepNo -1));
      //all other steps
      int i = 0;
      for (auto it = ret.begin(); it != ret.end(); it++)
        {
          i++;
          StrategyRepresentationType & fetchedCompareContainer = *it;

          for (unsigned int c = 0; c < mSinglePa->getMapSize(); ++c)
          //for (unsigned int c = 0; c < 10; ++c)
            {
              if (findInVector(fetchedCompareContainer, c))
                continue;

              std::vector<unsigned int> stratVect(fetchedCompareContainer);
              stratVect.push_back(c);

              mBacktestProcessor->processResult(stratVect);
            }
          if (i % 100 == 0)
            std::cout << "Step " << stepNo << " comparison, element group: " << i << std::endl;
        }
      std::vector<StrategyRepresentationType> newret = TSteppingPolicy::passes();
      std::cout << "finished and returning from level: " << stepNo << ", processed results: " << mBacktestProcessor->getResults().size() << std::endl;
      std::cout << "Before step " << stepNo << ": Number of survivors: " << TSurvivalPolicy::getNumSurvivors() << std::endl;
      TSurvivalPolicy::filterSurvivors();
      mSurvivingContainer->addSurvivorsPerRound(TSurvivalPolicy::getUniqueSurvivors());

      mBacktestProcessor->clearAll();
      TSurvivalPolicy::clearRound();
      std::cout << "After step " << stepNo << ": Number of survivors: " << TSurvivalPolicy::getNumSurvivors() << "/" << mSurvivingContainer->getNumSurvivors() << std::endl;
      return newret;
    }

  private:

    std::shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mBacktestProcessor;
    std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>>& mSinglePa;
    unsigned mMinTrades;
    unsigned mMaxDepth;
    unsigned long mRuns;
    std::shared_ptr<SurvivingStrategiesContainer<Decimal, std::valarray<Decimal>>> mSurvivingContainer;
    //shared_ptr<TSearchAlgoBacktester> mSearchAlgoBacktester;

  };

}


#endif // FORWARDSTEPWISESELECTOR_H
