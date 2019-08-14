#ifndef SEARCHCONTROLLER_H
#define SEARCHCONTROLLER_H

#include "McptConfigurationFileReader.h"
#include "BacktestResultBaseGenerator.h"
#include "UniqueSinglePAMatrix.h"
#include "ShortcutSearchAlgoBacktester.h"
#include "ForwardStepwiseSelector.h"

using namespace mkc_timeseries;

namespace mkc_searchalgo
{
//  template <class Decimal>
//  static std::shared_ptr<BackTester<Decimal>> buildBacktester(std::shared_ptr<McptConfiguration<Decimal>>& configuration)
//  {
//    DateRange iisDates = configuration->getInsampleDateRange();
//    std::shared_ptr<BackTester<Decimal>> theBackTester;

//    return getBackTester(configuration->getSecurity()->getTimeSeries()->getTimeFrame(),
//                         iisDates.getFirstDate(),
//                         iisDates.getLastDate());

//  }

  template <class Decimal>
  class SearchController
  {
  public:
    SearchController(const std::shared_ptr<McptConfiguration<Decimal>>& configuration, unsigned int depth):
      mDepth(depth),
      mConfiguration(configuration)
    {}
    void prepare()
    {
      std::string portfolioName(mConfiguration->getSecurity()->getName() + std::string(" Portfolio"));
      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(mConfiguration->getSecurity());
      mSeries = mConfiguration->getSecurity()->getTimeSeries();

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = mSeries->beginRandomAccess();

      mComparisonGenerator = std::make_shared<ComparisonsGenerator<Decimal>>(mDepth);

      for (; it != mSeries->endRandomAccess(); it++)
      {
          const Decimal& cOpen = mSeries->getOpenValue (it, 0);
          const Decimal& cHigh = mSeries->getHighValue (it, 0);
          const Decimal& cLow = mSeries->getLowValue (it, 0);
          const Decimal& cClose = mSeries->getCloseValue (it, 0);

          auto dt = mSeries->getDateValue(it, 0);
          //std::cout << dt << " OHLC: " << cOpen << "," << cHigh << "," << cLow << "," << cClose << std::endl;

          mComparisonGenerator->addNewLastBar(cOpen, cHigh, cLow, cClose);

      }
      std::cout << " Comparisons have been generated. " << std::endl;
      std::cout << " Full comparisons universe #: " << mComparisonGenerator->getComparisonsCount() << std::endl;
      std::cout << " Unique comparisons #: " << mComparisonGenerator->getUniqueComparisons().size() << std::endl;

      mPaMatrix = std::make_shared<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>(mComparisonGenerator, mSeries->getNumEntries());

    }

    template <bool isLong>
    void run()
    {
      std::shared_ptr<Decimal> profitTarget = std::make_shared<Decimal>(2.04);
      std::shared_ptr<Decimal> stopLoss = std::make_shared<Decimal>(2.04);
      BacktestResultBaseGenerator<Decimal, isLong> resultBase(mConfiguration, profitTarget, stopLoss);

      resultBase.buildBacktestMatrix();

      using TBacktester = ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>;

      unsigned int minTrades = 20;
      Decimal sortMultiplier(0.05);
      unsigned int passingStratNumPerRound = 1500;
      Decimal profitFactorCriterion(2.0);
      std::shared_ptr<TBacktester> shortcut = std::make_shared<TBacktester>(resultBase.getBacktestResultBase(), resultBase.getBacktestNumBarsInPosition(), minTrades, isLong);
      std::shared_ptr<BacktestProcessor<Decimal, TBacktester>> backtestProcessor = std::make_shared<BacktestProcessor<Decimal, TBacktester>>(minTrades, shortcut, mPaMatrix);
      ForwardStepwiseSelector<Decimal>
          forwardStepwise(backtestProcessor, mPaMatrix, minTrades, mDepth, passingStratNumPerRound, profitFactorCriterion, sortMultiplier, ( (*profitTarget)/(*stopLoss) ) );
      forwardStepwise.runSteps();
    }

  private:
    unsigned int mDepth;
    std::shared_ptr<McptConfiguration<Decimal>> mConfiguration;
    std::shared_ptr<ComparisonsGenerator<Decimal>> mComparisonGenerator;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSeries;
    std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>> mPaMatrix;
  };

}



#endif // SEARCHCONTROLLER_H
