#ifndef FORWARDSTEPWISESELECTOR_H
#define FORWARDSTEPWISESELECTOR_H

#include <iostream>
#include <algorithm>
#include "UniqueSinglePAMatrix.h"
//#include "ComparisonToPalStrategy.h"
#include <chrono>
#include "BacktestProcessor.h"

namespace mkc_searchalgo {


static bool findInVector(const std::vector<ComparisonEntryType>& vect, const ComparisonEntryType& value)
{
  return (std::end(vect) != std::find(std::begin(vect), std::end(vect), value));
}

/// valarray needs specialized handling of equality check (otherwise the operator== returns valarray of booleans)
template <class Decimal>
static bool findInVector(const std::vector<std::valarray<Decimal>>& vect, const std::valarray<Decimal>& value)
{
  for (const auto& el: vect)
    {
      if ((el == value).min())
        return true;
    }
  return false;
}



template <class Decimal, typename TSearchAlgoBacktester, typename TComparison,
          typename TSeedSteppingPolicy, typename TSteppingPolicy, typename TSurvivalPolicy>
class ForwardStepwiseSelector: private TSeedSteppingPolicy, private TSteppingPolicy, private TSurvivalPolicy
{
public:
  ForwardStepwiseSelector(const UniqueSinglePAMatrix<Decimal, TComparison>& singlePA,
                      unsigned minTrades, unsigned maxDepth, size_t passingStratNumPerRound, shared_ptr<TSearchAlgoBacktester>& searchAlgoBacktester,
                          Decimal survivalCriterion):
    mBacktestProcessor(minTrades, searchAlgoBacktester),
    TSeedSteppingPolicy(mBacktestProcessor, passingStratNumPerRound),
    TSteppingPolicy(mBacktestProcessor, passingStratNumPerRound),
    TSurvivalPolicy(survivalCriterion),
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
    step(mMaxDepth);
  }

private:

  std::vector<std::vector<TComparison>> step(unsigned int stepNo)
  {
    //typename std::unordered_map<unsigned int, TComparison>::const_iterator it = mSinglePa.getMapBegin();
    using indexType = typename UniqueSinglePAMatrix<Decimal, TComparison>::size_type;

    //bottom step
    if (stepNo == 0)
      {
        std::vector<std::unordered_map<unsigned int, TComparison>> ret0;
        ret0.resize(mSinglePa.getMap().size() * mSinglePa.getMap().size());

        for (indexType i = 0; i < mSinglePa.getMap().size(); ++i)
          {
            std::unordered_map<unsigned int, TComparison> compareContainer0;
            compareContainer0[i] = mSinglePa.getMappedElement(i);

            for (indexType i = 0; i < mSinglePa.getMap().size(); ++i)
              {
                std::unordered_map<unsigned int, TComparison> compareContainer(compareContainer0);

                if (compareContainer.find(i) != compareContainer.end())
                  continue;
                compareContainer[i] = mSinglePa.getMappedElement(i);
                ret0.push_back(compareContainer);
                mBacktestProcessor(compareContainer);
              }
            std::cout << "finished and returning from level: " << stepNo << std::endl;
            return TSeedSteppingPolicy::passes();
          }
      }

    //all other steps
     std::vector<std::vector<TComparison>> ret(step(stepNo - 1));

     for (size_t i = 0; i < ret.size(); ++i)
       {
         auto & compareContainer = ret[i];
         auto& element = mSinglePa.getMappedElement(i);
         if (findInVector(compareContainer, element))
           continue;

         compareContainer.push_back(element);
         mBacktestProcessor(compareContainer);
         //backtest result storage
       }
      return TSteppingPolicy::passes();

  }

private:

  const UniqueSinglePAMatrix<Decimal, TComparison>& mSinglePa;
  unsigned mMinTrades;
  unsigned mMaxDepth;
  unsigned long mRuns;
  BacktestProcessor<Decimal, TSearchAlgoBacktester> mBacktestProcessor;
  //shared_ptr<TSearchAlgoBacktester> mSearchAlgoBacktester;

};

}


#endif // FORWARDSTEPWISESELECTOR_H
