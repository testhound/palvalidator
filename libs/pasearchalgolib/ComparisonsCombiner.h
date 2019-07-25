#ifndef COMPARISONSCOMBINER_H
#define COMPARISONSCOMBINER_H

#include <iostream>
#include <algorithm>
#include "UniqueSinglePAMatrix.h"
#include "ComparisonToPal.h"

namespace mkc_searchalgo {

  template <class Decimal, typename BacktesterType, typename PortfolioType> class ComparisonsCombiner
  {
  public:
    ComparisonsCombiner(const UniqueSinglePAMatrix<Decimal>& singlePA,
                        unsigned minTrades, unsigned maxDepth, std::shared_ptr<BacktesterType>& backtester,
                        std::shared_ptr<PortfolioType>& portfolio):
      mSinglePa(singlePA),
      mMinTrades(minTrades),
      mMaxDepth(maxDepth - 1),
      mRuns(0),
      mBacktester(backtester),
      mPortfolio(portfolio)
    {
    }

    void combine()
    {
      std::unordered_map<unsigned int, std::valarray<int>>::const_iterator it = mSinglePa.getMatrixBegin();

      for (; it != mSinglePa.getMatrixEnd(); ++it)
        {
          unsigned level = 0;
          std::array<unsigned int, mMaxDepth> levelId{};
          levelId[level] = it->first;
          std::cout << "new top level" << std::endl;
          recurse(it, level, levelId);
        }
      //std::cout << "number of patterns: " << c << " of which valid: " << valid << " = " << (valid * 100.0/c) << "%" << std::endl;
    }

    ~ComparisonsCombiner()
    {}

  private:
    template <class ArrayType>
    void recurse(std::unordered_map<unsigned int, std::valarray<int>>::const_iterator& it, unsigned level, ArrayType levelId)
    {
      if (++level > mMaxDepth)
          return;

      std::unordered_map<unsigned int, std::valarray<int>>::const_iterator it2 = mSinglePa.getMatrixBegin();
      for (; it2 != mSinglePa.getMatrixEnd(); ++it2)
        {
          //already checked
          if (std::end(levelId) != std::find(std::begin(levelId), std::end(levelId), it2->first))
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

          auto clonedBackTester = mBacktester->clone();
//          ComparisonToPal(const std::unordered_set<ComparisonEntryType>& compareBatch,
//                          bool isLongPattern, const unsigned patternIndex, const unsigned long indexDate,
//                          decimal7* const profitTarget, decimal7* const stopLoss, std::shared_ptr<Portfolio<Decimal>>& portfolio):

          ComparisonToPal<Decimal> longStrategy();
          clonedBackTester->addStrategy(longStrategy);
          clonedBackTester->backtest();

          mRuns++;
          if (mRuns % 10000000 == 0)
            std::cout << "number of runs: " << mRuns << std::endl;

          levelId[level] = it2->first;
          //call new level
          recurse(it2, level);
        }

    }

    //const std::unordered_map<unsigned int, std::valarray<int>>& mMatrix;
    const UniqueSinglePAMatrix<Decimal>& mSinglePa;
    unsigned mMinTrades;
    unsigned mMaxDepth;
    unsigned long mRuns;
    std::shared_ptr<BacktesterType>& mBacktester;
    std::shared_ptr<PortfolioType>& mPortfolio;

  };

}

#endif // COMPARISONSCOMBINER_H
