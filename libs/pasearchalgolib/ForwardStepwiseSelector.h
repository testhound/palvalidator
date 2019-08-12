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
#include "SurvivalPolicy.h"

namespace mkc_searchalgo {

template <class Decimal,
          typename TComparison = std::valarray<Decimal>,
          typename TSearchAlgoBacktester = ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>,
          typename TSteppingPolicy = SimpleSteppingPolicy<Decimal, TSearchAlgoBacktester, Sorters::CombinationPfSorter<Decimal>>,
          typename TSurvivalPolicy = DefaultSurvivalPolicy<Decimal, TSearchAlgoBacktester>
          >
class ForwardStepwiseSelector: private TSteppingPolicy, private TSurvivalPolicy
{
public:
  ForwardStepwiseSelector(std::shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& backtestProcessor ,
                          std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>>& singlePA,
                          unsigned minTrades, unsigned maxDepth, size_t passingStratNumPerRound,
                          Decimal survivalCriterion):
    mBacktestProcessor(backtestProcessor),
    TSteppingPolicy(backtestProcessor, passingStratNumPerRound),
    TSurvivalPolicy(backtestProcessor, survivalCriterion),
    mSinglePa(singlePA),
    mMinTrades(minTrades),
    mMaxDepth(maxDepth - 1),
    mRuns(0)
  {
  }
  ~ForwardStepwiseSelector()
  {}

  void runSteps()
  {
    unsigned int stepNo = 0;
    std::vector<StrategyRepresentationType> ret;
    while (stepNo < mMaxDepth)
      {
        step(stepNo, ret);
        stepNo++;
      }
  }

private:

  //bottom step
  void step0(std::vector<StrategyRepresentationType>& ret)
  {
      for (unsigned int i = 0; i < mSinglePa->getMapSize(); ++i)
        {
          //for (unsigned int c = 0; c < mSinglePa->getMapSize(); ++c)
          for (unsigned int c = 0; c < 10; ++c)
            {
              if (i == c)
                continue;
              std::vector<unsigned int> stratVect {i, c};
              //stratVect.reserve(mMaxDepth);
              //stratVect = {i, c};
              mBacktestProcessor->processResult(stratVect);
            }
            if (i % 100 == 0)
              std::cout << "Step 0 comparison, element group: " << i << std::endl;
        }
        std::cout << "finished and returning from level 0, processed results: " << mBacktestProcessor->getResults().size() << std::endl;
        std::vector<StrategyRepresentationType> newret = TSteppingPolicy::passes();
        ret.insert(ret.end(), newret.begin(), newret.end());
        TSurvivalPolicy::saveSurvivors();
        std::cout << "After step 0: Number of survivors: " << TSurvivalPolicy::getNumSurvivors() << std::endl;
        mBacktestProcessor->clearAll();
    }

    void step(unsigned int stepNo, std::vector<StrategyRepresentationType>& ret)
    {
        if (stepNo == 0)
          {
            return step0(ret);
          }
        else if (stepNo == 1)
          {
            mLastBegin = ret.begin();
          }

    //all other steps
     int i = 0;
     for (auto it = mLastBegin; it != ret.end(); it++)
       {
          i++;
          StrategyRepresentationType & fetchedCompareContainer = *it;

         //for (unsigned int c = 0; c < mSinglePa->getMapSize(); ++c)
         for (unsigned int c = 0; c < 10; ++c)
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
      mLastBegin = std::prev(ret.end()); //save iterator to start from
      ret.insert(ret.end(), newret.begin(), newret.end());
      std::cout << "finished and returning from level: " << stepNo << ", processed results: " << mBacktestProcessor->getResults().size() << std::endl;
      std::cout << "Before step " << stepNo << ": Number of survivors: " << TSurvivalPolicy::getNumSurvivors() << std::endl;
      TSurvivalPolicy::saveSurvivors();
      std::cout << "After step " << stepNo << ": Number of survivors: " << TSurvivalPolicy::getNumSurvivors() << std::endl;
      mBacktestProcessor->clearAll();
  }

private:

  std::shared_ptr<UniqueSinglePAMatrix<Decimal, TComparison>>& mSinglePa;
  unsigned mMinTrades;
  unsigned mMaxDepth;
  unsigned long mRuns;
  std::shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mBacktestProcessor;
  std::vector<StrategyRepresentationType>::iterator mLastBegin;
  //shared_ptr<TSearchAlgoBacktester> mSearchAlgoBacktester;

};

}


#endif // FORWARDSTEPWISESELECTOR_H
