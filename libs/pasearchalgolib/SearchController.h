#ifndef SEARCHCONTROLLER_H
#define SEARCHCONTROLLER_H

#include "McptConfigurationFileReader.h"
#include "BacktestResultBaseGenerator.h"
#include "UniqueSinglePAMatrix.h"
#include "ShortcutSearchAlgoBacktester.h"
#include "ForwardStepwiseSelector.h"
#include "SearchAlgoConfigurationFileReader.h"
#include "SurvivingStrategiesContainer.h"
#include "LogPalPattern.h"

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
  class   SearchController
  {
  public:
    SearchController(const std::shared_ptr<McptConfiguration<Decimal>>& configuration, const std::shared_ptr<OHLCTimeSeries<Decimal>>& series, const std::shared_ptr<SearchAlgoConfiguration<Decimal>>& searchConfiguration):
      mSearchConfiguration(searchConfiguration),
      mConfiguration(configuration),
      mSeries(series),
      mPatternIndex(0)
    {}
    void prepare()
    {
      std::string portfolioName(mConfiguration->getSecurity()->getName() + std::string(" Portfolio"));
      mPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      mPortfolio->addSecurity(mConfiguration->getSecurity());

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = mSeries->beginRandomAccess();

      mComparisonGenerator = std::make_shared<ComparisonsGenerator<Decimal>>(mSearchConfiguration->getMaxDepth());

      std::cout << "Preparing controller with timeseries of size: " << mSeries->getNumEntries() << std::endl;

      for (; it != mSeries->endRandomAccess(); it++)
      {
          const Decimal& cOpen = mSeries->getOpenValue (it, 0);
          const Decimal& cHigh = mSeries->getHighValue (it, 0);
          const Decimal& cLow = mSeries->getLowValue (it, 0);
          const Decimal& cClose = mSeries->getCloseValue (it, 0);

          auto dt = mSeries->getDateValue(it, 0);
          std::cout << dt << " OHLC: " << cOpen << "," << cHigh << "," << cLow << "," << cClose << std::endl;

          mComparisonGenerator->addNewLastBar(cOpen, cHigh, cLow, cClose);

      }
      std::cout << " Comparisons have been generated. " << std::endl;
      std::cout << " Full comparisons universe #: " << mComparisonGenerator->getComparisonsCount() << std::endl;
      std::cout << " Unique comparisons #: " << mComparisonGenerator->getUniqueComparisons().size() << std::endl;

      mPaMatrix = std::make_shared<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>(mComparisonGenerator, mSeries->getNumEntries());
      mLongSurvivors = std::make_shared<SurvivingStrategiesContainer<Decimal, std::valarray<Decimal>>>(mPaMatrix);
      mShortSurvivors = std::make_shared<SurvivingStrategiesContainer<Decimal, std::valarray<Decimal>>>(mPaMatrix);;

    }

    template <bool isLong>
    void run(const shared_ptr<Decimal>& profitTarget, const shared_ptr<Decimal>& stopLoss, bool inSampleOnly)
    {
      //std::shared_ptr<Decimal> profitTarget = std::make_shared<Decimal>(2.04);
      //std::shared_ptr<Decimal> stopLoss = std::make_shared<Decimal>(2.04);
      BacktestResultBaseGenerator<Decimal, isLong> resultBase(mConfiguration, mSeries, profitTarget, stopLoss, inSampleOnly);

      resultBase.buildBacktestMatrix();

      using TBacktester = ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>;

//      unsigned int minTrades = 20;
//      Decimal sortMultiplier(5.0);
//      unsigned int passingStratNumPerRound = 1000;
//      Decimal profitFactorCriterion(2.0);
//      unsigned int maxLosers = 4;
//      unsigned int maxInactivity = 500;
      //1: 4, 500
      //2: 4, 10000
      //3: 1, 500
      //4: 4, 500 (sorter: 5.0)

      std::shared_ptr<TBacktester> shortcut = std::make_shared<TBacktester>(resultBase.getBacktestResultBase(), resultBase.getBacktestNumBarsInPosition(), mSearchConfiguration->getMinTrades(), isLong);
      std::shared_ptr<BacktestProcessor<Decimal, TBacktester>> backtestProcessor = std::make_shared<BacktestProcessor<Decimal, TBacktester>>(
            mSearchConfiguration->getMinTrades(),
            mSearchConfiguration->getMaxConsecutiveLosers(),
            mSearchConfiguration->getMaxInactivitySpan(),
            shortcut,
            mPaMatrix);
      ForwardStepwiseSelector<Decimal>
          forwardStepwise(backtestProcessor,
                          mPaMatrix,
                          mSearchConfiguration->getMinTrades(),
                          mSearchConfiguration->getMaxDepth(),
                          mSearchConfiguration->getPassingStratNumPerRound(),
                          mSearchConfiguration->getProfitFactorCriterion(),
                          mSearchConfiguration->getSortMultiplier(),
                          ( (*profitTarget)/(*stopLoss) ),
                          (isLong)? mLongSurvivors: mShortSurvivors);
      forwardStepwise.runSteps();
    }

    void exportSurvivingLongPatterns(const shared_ptr<Decimal>& profitTarget, const shared_ptr<Decimal>& stopLoss, const std::string& exportFileName)
    {

        std::cout << "Exporting long strategies into file: " << exportFileName << std::endl;
       std::ofstream exportFile(exportFileName);
       std::vector<std::vector<ComparisonEntryType>> survivingLong = mLongSurvivors->getSurvivorsAsComparisons();
       for (const std::vector<ComparisonEntryType>& strat: survivingLong)
         {
            ComparisonToPalLongStrategy<Decimal> comp(strat, mPatternIndex++, 0, profitTarget.get(), stopLoss.get(), mPortfolio);
            LogPalPattern::LogPattern(comp.getPalPattern(), exportFile);
         }
    }

    void exportSurvivingShortPatterns(const shared_ptr<Decimal>& profitTarget, const shared_ptr<Decimal>& stopLoss, const std::string& exportFileName)
    {
       std::cout << "Exporting short strategies into file: " << exportFileName << std::endl;
       std::ofstream exportFile(exportFileName);
       std::vector<std::vector<ComparisonEntryType>> survivingLong = mShortSurvivors->getSurvivorsAsComparisons();
       for (const std::vector<ComparisonEntryType>& strat: survivingLong)
         {
            ComparisonToPalShortStrategy<Decimal> comp(strat, mPatternIndex++, 0, profitTarget.get(), stopLoss.get(), mPortfolio);
            LogPalPattern::LogPattern(comp.getPalPattern(), exportFile);
         }
    }

  private:
    std::shared_ptr<Portfolio<Decimal>> mPortfolio;
    std::shared_ptr<SearchAlgoConfiguration<Decimal>> mSearchConfiguration;
    std::shared_ptr<McptConfiguration<Decimal>> mConfiguration;
    std::shared_ptr<ComparisonsGenerator<Decimal>> mComparisonGenerator;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mSeries;
    std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>> mPaMatrix;
    std::shared_ptr<SurvivingStrategiesContainer<Decimal, std::valarray<Decimal>>> mLongSurvivors;
    std::shared_ptr<SurvivingStrategiesContainer<Decimal, std::valarray<Decimal>>> mShortSurvivors;
    unsigned int mPatternIndex;
  };

}



#endif // SEARCHCONTROLLER_H
