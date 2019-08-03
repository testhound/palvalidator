#include <string>
#include <vector>
#include <memory>
#include <stdio.h>
#include "McptConfigurationFileReader.h"
#include "PALMonteCarloValidation.h"
#include "RobustnessTester.h"
#include "LogPalPattern.h"
#include "LogRobustnessTest.h"
#include "number.h"
#include <cstdlib>
#include "ComparisonsGenerator.h"
#include "UniqueSinglePAMatrix.h"
#include "ComparisonsCombiner.h"
#include <map>
#include "BacktestResultBaseGenerator.h"
#include "OriginalSearchAlgoBacktester.h"
#include "ShortcutSearchAlgoBacktester.h"
#include <chrono>

using namespace mkc_timeseries;
using namespace mkc_searchalgo;
using std::shared_ptr;
using Decimal = num::DefaultNumber;


template <class Decimal>
static std::shared_ptr<BackTester<Decimal>> buildBacktester(std::shared_ptr<McptConfiguration<Decimal>>& configuration)
{
  DateRange iisDates = configuration->getInsampleDateRange();
  std::shared_ptr<BackTester<Decimal>> theBackTester;

  return getBackTester(configuration->getSecurity()->getTimeSeries()->getTimeFrame(),
                       iisDates.getFirstDate(),
                       iisDates.getLastDate());

}

int main(int argc, char **argv)
{
    std::cout << "started..." << std::endl;
    std::vector<std::string> v(argv, argv + argc);

    if (argc == 2)
    {
        std::string configurationFileName (v[1]);

        std::cout << configurationFileName << std::endl;
        McptConfigurationFileReader reader(configurationFileName);
        std::shared_ptr<McptConfiguration<Decimal>> configuration = reader.readConfigurationFile();
        std::shared_ptr<Decimal> profitTarget = std::make_shared<Decimal>(1.0);
        std::shared_ptr<Decimal> stopLoss = std::make_shared<Decimal>(1.0);

        BacktestResultBaseGenerator<Decimal, ComparisonToPalLongStrategyAlwaysOn<Decimal>> resultBase(configuration, profitTarget, stopLoss);

        resultBase.buildBacktestMatrix(true);

        std::shared_ptr<BackTester<Decimal>> backtester = buildBacktester(configuration);

        std::string portfolioName(configuration->getSecurity()->getName() + std::string(" Portfolio"));

        auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
        aPortfolio->addSecurity(configuration->getSecurity());

        std::shared_ptr<OHLCTimeSeries<Decimal>> series = configuration->getSecurity()->getTimeSeries();

        typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = series->beginRandomAccess();

        unsigned depth = 10;
        ComparisonsGenerator<Decimal> compareGenerator(depth);

        for (; it != series->endRandomAccess(); it++)
        {
            const Decimal& cOpen = series->getOpenValue (it, 0);
            const Decimal& cHigh = series->getHighValue (it, 0);
            const Decimal& cLow = series->getLowValue (it, 0);
            const Decimal& cClose = series->getCloseValue (it, 0);

            auto dt = series->getDateValue(it, 0);
            std::cout << dt << " OHLC: " << cOpen << "," << cHigh << "," << cLow << "," << cClose << std::endl;

            compareGenerator.addNewLastBar(cOpen, cHigh, cLow, cClose);

        }

        std::cout << " Full comparisons universe #:" << compareGenerator.getComparisonsCount() << std::endl;
        std::cout << " Unique comparisons #:" << compareGenerator.getUniqueComparisons().size() << std::endl;

        using TBacktester = OriginalSearchAlgoBackteser<Decimal, ComparisonEntryType, ComparisonToPalLongStrategy<Decimal>>;
        UniqueSinglePAMatrix<Decimal, ComparisonEntryType> paMatrix1(compareGenerator, series->getNumEntries());
        std::shared_ptr<TBacktester> origBacktester = std::make_shared<OriginalSearchAlgoBackteser<Decimal, ComparisonEntryType, ComparisonToPalLongStrategy<Decimal>>>(backtester, aPortfolio, profitTarget, stopLoss);

        using TBacktester2 = ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>;
        using TComparison = std::valarray<Decimal>;
        UniqueSinglePAMatrix<Decimal, TComparison> paMatrix2(compareGenerator, series->getNumEntries());
        bool isLong = true;
        unsigned int minTrades = 5;
        std::shared_ptr<TBacktester2> shortcut = std::make_shared<TBacktester2>(resultBase.getBacktestResultBase(isLong), resultBase.getBacktestNumBarsInPosition(isLong), minTrades, isLong);

        //a 100 random tests
        for (unsigned int c = 0; c < 100; c++)
          {
              size_t maxn = paMatrix1.getMap().size();
              int rand1 = rand()%(maxn-0 + 1) + 0;
              int rand2 = rand()%(maxn-0 + 1) + 0;
              auto& el11 = paMatrix1.getMappedElement(rand1);
              auto& el12 = paMatrix1.getMappedElement(rand2);
              auto& el21 = paMatrix2.getMappedElement(rand1);
              auto& el22 = paMatrix2.getMappedElement(rand2);

              std::vector<ComparisonEntryType> vect1 = {el11, el12};
              std::vector<TComparison> vect2 = {el21, el22};

              auto el211 = paMatrix2.getUnderlying(rand1);
              std::cout << "orig: " << el11[0] << el11[1] << el11[2] << el11[3] << std::endl;
              std::cout << "shortcut underlying: " << el211[0] << el211[1] << el211[2] << el211[3] << std::endl;

              auto start = high_resolution_clock::now();
              origBacktester->backtest(vect1);
              auto stop = high_resolution_clock::now();
              auto duration = duration_cast<microseconds>(stop - start);
              std::cout << "backtest1 took: " << duration.count() << " microseconds" << std::endl;
              std::cout << "orig tradenum: " << origBacktester->getTradeNumber() << ", profit factor: " << origBacktester->getProfitFactor() << std::endl;
              start = high_resolution_clock::now();
              shortcut->backtest(vect2);
              stop = high_resolution_clock::now();
              duration = duration_cast<microseconds>(stop - start);
              std::cout << "backtest2 took: " << duration.count() << " microseconds" << std::endl;
              std::cout << "shortcut tradenum: " << shortcut->getTradeNumber() << ", profit factor: " << shortcut->getProfitFactor() << std::endl;

          }

//        UniqueSinglePAMatrix<Decimal, ComparisonEntryType> paMatrix(compareGenerator, series->getNumEntries());
//        using TBacktester = OriginalSearchAlgoBackteser<Decimal, ComparisonEntryType, true>;
//        std::shared_ptr<TBacktester> origBacktester = std::make_shared<OriginalSearchAlgoBackteser<Decimal, ComparisonEntryType, true>>(backtester, aPortfolio, profitTarget, stopLoss);
//        ComparisonsCombiner<Decimal, TBacktester, ComparisonEntryType> compareCombine(paMatrix, 10, depth, origBacktester);
//        compareCombine.combine();

        //vectorized version

//        using TBacktester = ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>;
//        using TComparison = std::valarray<Decimal>;
//        UniqueSinglePAMatrix<Decimal, TComparison> paMatrix(compareGenerator, series->getNumEntries());
        //        //(const std::valarray<Decimal>& backtestResults, const std::valarray<unsigned int>& numBarsInPosition, unsigned int minTrades, bool isLong)
//        bool isLong = true;
//        unsigned int minTrades = 5;
//        std::shared_ptr<TBacktester> shortcut = std::make_shared<TBacktester>(resultBase.getBacktestResultBase(isLong), resultBase.getBacktestNumBarsInPosition(isLong), minTrades, isLong);
//        ComparisonsCombiner<Decimal, TBacktester, TComparison> compareCombine(paMatrix, 10, depth, shortcut);
//        compareCombine.combine();

    }
    else {
        std::cout << "wrong usage, " << (argc - 1) << " arguments specified, needs to be a single argument.. " << std::endl;
    }


}
