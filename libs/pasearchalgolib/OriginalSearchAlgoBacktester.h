#ifndef ORIGINALSEARCHALGOBACKTESTER_H
#define ORIGINALSEARCHALGOBACKTESTER_H

#include <string>
#include <vector>
#include <memory>
#include <stdio.h>
#include "ComparisonToPalStrategy.h"
#include "BackTester.h"
#include "Portfolio.h"


using namespace mkc_timeseries;
using namespace mkc_searchalgo;
using std::shared_ptr;
using Decimal = num::DefaultNumber;


namespace mkc_searchalgo {

  template <class Decimal, typename TComparison, bool isLong>
  class OriginalSearchAlgoBackteser
  {
    public:
    using SidedComparisonToPalType = std::conditional_t<isLong, ComparisonToPalLongStrategy<Decimal>, ComparisonToPalShortStrategy<Decimal>>;

    OriginalSearchAlgoBackteser(std::shared_ptr<BackTester<Decimal>>& backtester,
                                const std::shared_ptr<Portfolio<Decimal>>& portfolio,
                                const std::shared_ptr<Decimal>& profitTarget,
                                const std::shared_ptr<Decimal>& stopLoss):
      mBacktester(backtester),
      mPortfolio(portfolio),
      mProfitTarget(profitTarget),
      mStopLoss(stopLoss),
      mRuns(0)
    {}

    bool getIsLong() const { return isLong; }

    void backtest(const std::vector<TComparison>& compareContainer)
    {
      SidedComparisonToPalType comp(compareContainer, mRuns, 0, mProfitTarget.get(), mStopLoss.get(), mPortfolio);
      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mBacktester->clone();
      clonedBackTester->addStrategy(comp.getPalStrategy());
      clonedBackTester->backtest();
      mRuns++;
      std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy = (*(clonedBackTester->beginStrategies()));
      mProfitFactor = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getProfitFactor();
      mTradeNum = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getNumPositions();
    }

    Decimal getProfitFactor() const { return mProfitFactor; }
    unsigned int getTradeNumber() const { return mTradeNum; }

  private:

    const std::shared_ptr<BackTester<Decimal>>& mBacktester;
    const std::shared_ptr<Portfolio<Decimal>>& mPortfolio;
    const std::shared_ptr<Decimal>& mProfitTarget;
    const std::shared_ptr<Decimal>& mStopLoss;
    unsigned int mRuns;
    Decimal mProfitFactor;
    unsigned int mTradeNum;
  };

//useful typedefs:
  template <class Decimal, class TComparison>
  using OriginalSearchAlgoBackteserLong = OriginalSearchAlgoBackteser<Decimal, TComparison, true>;
  template <class Decimal, class TComparison>
  using OriginalSearchAlgoBackteserShort = OriginalSearchAlgoBackteser<Decimal, TComparison, false>;

}

#endif // ORIGINALSEARCHALGOBACKTESTER_H
