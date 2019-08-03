#ifndef COMPARISONSCOMBINER_H
#define COMPARISONSCOMBINER_H

#include <iostream>
#include <algorithm>
#include "UniqueSinglePAMatrix.h"
#include "ComparisonToPalStrategy.h"
#include <chrono>

using namespace std::chrono;

namespace mkc_searchalgo {


  static bool findInVector(const std::vector<ComparisonEntryType>& vect, const ComparisonEntryType& value)
  {
    return (std::end(vect) != std::find(std::begin(vect), std::end(vect), value));
  }

  ///
  /// valarray needs specialized handling of equality check (otherwise the operator== returns valarray of booleans)
  /// so the find algorithm needs to consider that
  ///
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
      //for (; it != mSinglePa.getMapEnd(); ++it)
      for (int i = 0; i < mSinglePa.getMap().size(); ++i)
        {
          unsigned level = 0;
          std::vector<TComparison> compareContainer;
          compareContainer.reserve(15);             //let's reserve for speed gains
          //compareContainer.push_back(it->second);
          compareContainer.push_back(mSinglePa.getMappedElement(i));
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
      //for (; it2 != mSinglePa.getMapEnd(); ++it2)
      for (int i = 0; i < mSinglePa.getMap().size(); ++i)
        {
          auto& element = mSinglePa.getMappedElement(i);
          //auto& element = it2->second;
          //already checked
          //if (findInVector(compareContainer, element))
          if (findInVector(compareContainer, element))
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

          compareContainer.push_back(element);
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
          std::cout << "Profit factor: " << mSearchAlgoBacktester->getProfitFactor() << ", trade number: " << mSearchAlgoBacktester->getTradeNumber() << std::endl;
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
