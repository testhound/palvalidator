#ifndef COMPARISONSCOMBINER_H
#define COMPARISONSCOMBINER_H

#include <iostream>
#include <algorithm>
#include "UniqueSinglePAMatrix.h"
#include "ComparisonToPalStrategy.h"
#include <chrono>

using namespace std::chrono;

namespace mkc_searchalgo {

  template <class Decimal, typename TSearchAlgoBacktester, typename TComparison> class ComparisonsCombiner
  {
  public:
    ComparisonsCombiner(const UniqueSinglePAMatrix<Decimal, TComparison>& singlePA,
                        unsigned minTrades, unsigned maxDepth, shared_ptr<TSearchAlgoBacktester>& searchAlgoBacktester):
      mSinglePa(singlePA),
      mMinTrades(minTrades),
      mMaxDepth(maxDepth - 1),
      mRuns(0),
      mSearchAlgoBacktester(searchAlgoBacktester)
    {
    }

    void combine()
    {
      typename std::unordered_map<unsigned int, TComparison>::const_iterator it = mSinglePa.getMapBegin();

      mMaxDepth = 1;
      for (; it != mSinglePa.getMapEnd(); ++it)
        {
          unsigned level = 0;
          std::vector<TComparison> compareContainer;
          compareContainer.reserve(15);             //let's reserve for speed gains
          compareContainer.push_back(it->second);
          //std::cout << "new top level" << std::endl;
          recurse(it, level, compareContainer);
        }
    }

    ~ComparisonsCombiner()
    {}

  private:

    //template <class ArrayType>
    void recurse(typename std::unordered_map<unsigned int, TComparison>::const_iterator& it, unsigned level, std::vector<TComparison>& compareContainer)
    {
      if (++level > mMaxDepth)
        {
          compareContainer.pop_back();
          return;
        }
      typename std::unordered_map<unsigned int, TComparison>::const_iterator it2 = mSinglePa.getMapBegin();
      for (; it2 != mSinglePa.getMapEnd(); ++it2)
        {
          //already checked
          if (std::end(compareContainer) != std::find(std::begin(compareContainer), std::end(compareContainer), it2->second))
            {
              //std::cout << "already checked: " << it2->first << std::endl;
              continue;
            }

          //std::valarray<int> newvec = it->second * it2->second;
          //int trades = newvec.sum();


//          if (trades < mMinTrades)
//            {
//              continue;
//            }

          compareContainer.push_back(it2->second);
//          ComparisonToPal(const std::vector<ComparisonEntryType>& compareBatch,
//                          bool isLongPattern, const unsigned patternIndex, const unsigned long indexDate,
//                          decimal7* const profitTarget, decimal7* const stopLoss, std::shared_ptr<Portfolio<Decimal>>& portfolio):

          //std::cout << "backtester built." << std::endl;
          //bool isLong = true;
          //std::cout << compareContainer[0][0] << compareContainer[0][1] << compareContainer[0][2] << compareContainer[0][3] <<  " and " << compareContainer[1][0] << compareContainer[1][1] << compareContainer[1][2] << compareContainer[1][3] << std::endl;

          //auto start = high_resolution_clock::now();

          //ComparisonToPal<Decimal> comp(compareContainer, isLong, mRuns, 0, mProfitTarget.get(), mStopLoss.get(), mPortfolio);
          //auto clonedBackTester = mBacktester->clone();
          //clonedBackTester->addStrategy(comp.getPalStrategy());

          //auto stop = high_resolution_clock::now();
          //auto duration = duration_cast<microseconds>(stop - start);

          //std::cout << "building compare/clone and add strategy: " << duration.count() << " microseconds" << std::endl;

          //start = high_resolution_clock::now();

          //std::cout << "added backtest strategy." << std::endl;
          //clonedBackTester->backtest();

          //stop = high_resolution_clock::now();
          //duration = duration_cast<microseconds>(stop - start);
          //std::cout << "backtesting: " << duration.count() << " microseconds" << std::endl;

          //std::cout << "backtested." << std::endl;
          //std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy = (*(clonedBackTester->beginStrategies()));

          //std::cout << "Profit factor: " << backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getProfitFactor() << std::endl;
          mSearchAlgoBacktester->backtest(compareContainer);
          std::cout << "Profit factor: " << mSearchAlgoBacktester->getProfitFactor() << std::endl;
          mRuns++;
          if (mRuns % 1000 == 0)
            std::cout << "number of runs: " << mRuns << std::endl;



          //call new level
          recurse(it2, level, compareContainer);
        }
        compareContainer.pop_back();

    }

    //const std::unordered_map<unsigned int, std::valarray<int>>& mMatrix;
    const UniqueSinglePAMatrix<Decimal, TComparison>& mSinglePa;
    unsigned mMinTrades;
    unsigned mMaxDepth;
    unsigned long mRuns;
    shared_ptr<TSearchAlgoBacktester> mSearchAlgoBacktester;

  };

}

#endif // COMPARISONSCOMBINER_H
