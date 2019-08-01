#ifndef SHORTCUTBACKTESTER_H
#define SHORTCUTBACKTESTER_H

#include "McptConfigurationFileReader.h"
#include <string>
#include <vector>
#include <memory>
#include <stdio.h>

#include "PALMonteCarloValidation.h"
#include "ComparisonToPalStrategy.h"

using namespace mkc_timeseries;
using namespace mkc_searchalgo;
using std::shared_ptr;
using Decimal = num::DefaultNumber;

namespace mkc_searchalgo {

  static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
                                                     boost::gregorian::date startDate,
                                                     boost::gregorian::date endDate)
  {
    if (theTimeFrame == TimeFrame::DAILY)
      return std::make_shared<DailyBackTester<Decimal>>(startDate, endDate);
    else if (theTimeFrame == TimeFrame::WEEKLY)
      return std::make_shared<WeeklyBackTester<Decimal>>(startDate, endDate);
    else if (theTimeFrame == TimeFrame::MONTHLY)
      return std::make_shared<MonthlyBackTester<Decimal>>(startDate, endDate);
    else
      throw PALMonteCarloValidationException("PALMonteCarloValidation::getBackTester - Only daily and monthly time frame supported at present.");
  }


  ///
  ///
  ///
  template <class Decimal> class BacktestResultBaseGenerator
  {
  public:
    BacktestResultBaseGenerator(const std::shared_ptr<McptConfiguration<Decimal>>& configuration, const std::shared_ptr<Decimal>& profitTarget, const std::shared_ptr<Decimal>& stopLoss):
      mConfiguration(configuration),
      mProfitTarget(profitTarget),
      mStopLoss(stopLoss),
      mDayBatches(10),
      mLongSideReady(false),
      mShortSideReady(false)
    {}

    void buildBacktestMatrix(bool isLong)
    {
      if ((isLong && mLongSideReady) || (!isLong && mShortSideReady))
        return;

      std::string portfolioName(mConfiguration->getSecurity()->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);

      aPortfolio->addSecurity(mConfiguration->getSecurity());

      ComparisonEntryType alwaysTrue {0, 1, 0, 2};  //aka on current bar: high greater than low = always true (as long as the bars are valid)

      std::vector<ComparisonEntryType> compareContainer { alwaysTrue };
      //map to hold unique entries (keyed on signal dates)
      std::map<TimeSeriesDate, std::tuple<Decimal, Decimal, unsigned int>> tradesMap;

      //get time series, iterate
      std::shared_ptr<OHLCTimeSeries<Decimal>> series = mConfiguration->getSecurity()->getTimeSeries();

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = series->beginRandomAccess();

      unsigned long i = 0;

      for (; it != series->endRandomAccess(); it++)
      {
          i++;
          if (i > 1)
            {
              ComparisonToPalStrategy<Decimal> comparison(compareContainer, isLong, 1, i, mProfitTarget.get(), mStopLoss.get(), aPortfolio);
              auto offset = std::min((series->getNumEntries() - 1), (i + mDayBatches));
              std::cout << "offset: " << offset << ", size: " << series->getNumEntries() << ", i: " << i << std::endl;
              auto interimBacktester = getBackTester(mConfiguration->getSecurity()->getTimeSeries()->getTimeFrame(), it->getDateValue(), (series->beginRandomAccess() + offset)->getDateValue());
              interimBacktester->addStrategy(comparison.getPalStrategy());
              interimBacktester->backtest();
              std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy = (*(interimBacktester->beginStrategies()));
              ClosedPositionHistory<Decimal> closedPositions = backTesterStrategy->getStrategyBroker().getClosedPositionHistory();

              if (closedPositions.getNumPositions() > 0)
                {
                  std::pair<TimeSeriesDate,std::shared_ptr<TradingPosition<Decimal>>> firstPos = *(closedPositions.beginTradingPositions());
                  tradesMap[firstPos.first] = std::make_tuple<Decimal, Decimal, unsigned int>(firstPos.second->getTradeReturn(), firstPos.second->getPercentReturn(), firstPos.second->getNumBarsInPosition());

                  std::cout << "(position for same day) first position on " << firstPos.first << ", Entry Date: " << firstPos.second->getEntryDate() << ": "
                            << firstPos.second->getTradeReturn() << " in percent: " << firstPos.second->getPercentReturn() << ", bars in pos: "
                            << firstPos.second->getNumBarsInPosition() << ", exit date: " << firstPos.second->getExitDate() << std::endl;
                }
            }

      }
      //the crux: create (not so sparse) vector of trading result per signal-date.
      std::valarray<Decimal> arr(Decimal(0.0), series->getNumEntries());    //initialize to all 0-es
      //then the number of bars that it should occupy
      std::valarray<unsigned int> arrNumBars(static_cast<unsigned int>(0), series->getNumEntries());

      size_t  iCounter = 0;
      for (auto it = series->beginRandomAccess(); it != series->endRandomAccess(); it++)
        {
          typename std::map<TimeSeriesDate, std::tuple<Decimal, Decimal, unsigned int>>::const_iterator mapIt = tradesMap.find(series->getDateValue(it, 0));

          if ( mapIt != tradesMap.end())
            {
              arr[iCounter] = std::get<1>(mapIt->second);
              arrNumBars[iCounter] = std::get<2>(mapIt->second);
            }
          iCounter++;
        }

      //store vectors
      if (isLong)
        {
          mArrLong = arr;
          mNumBarsLong = arrNumBars;
          mLongSideReady = true;
        }
      else
        {
          mArrShort = arr;
          mNumBarsLong = arrNumBars;
          mShortSideReady = true;
        }

    }

    template<bool isLong> void prepare();


    std::valarray<Decimal>& getBacktestResultBase(bool isLong) const
    {
      prepare<isLong>();
      return isLong? mArrLong: mArrShort;
    }

    std::valarray<unsigned int>& getBacktestNumBarsInPosition(bool isLong) const
    {
      prepare<isLong>();
      return isLong? mNumBarsLong: mNumBarsShort;
    }

  private:
    std::shared_ptr<McptConfiguration<Decimal>> mConfiguration;
    std::shared_ptr<Decimal> mProfitTarget;
    std::shared_ptr<Decimal> mStopLoss;
    unsigned int mDayBatches;
    bool mLongSideReady;
    bool mShortSideReady;
    std::valarray<Decimal> mArrLong;
    std::valarray<unsigned int> mNumBarsLong;
    std::valarray<Decimal> mArrShort;
    std::valarray<unsigned int> mNumBarsShort;

  };

  template<>
  template<> inline void BacktestResultBaseGenerator<Decimal>::prepare<true>()
  {
    if (!mLongSideReady)
      buildBacktestMatrix(true);
  }

  template<>
  template<> inline void BacktestResultBaseGenerator<Decimal>::prepare<false>()
  {
    if (!mShortSideReady)
      buildBacktestMatrix(false);
  }


}

#endif // SHORTCUTBACKTESTER_H
