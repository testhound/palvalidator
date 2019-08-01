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

        BacktestResultBaseGenerator<Decimal> resultBase(configuration, profitTarget, stopLoss);

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

        UniqueSinglePAMatrix<Decimal, ComparisonEntryType> paMatrix(compareGenerator, series->getNumEntries());

        auto origBacktester = std::make_shared<OriginalSearchAlgoBackteser<Decimal, ComparisonEntryType, true>>(backtester, aPortfolio, profitTarget, stopLoss);
        ComparisonsCombiner<Decimal, OriginalSearchAlgoBackteser<Decimal, ComparisonEntryType, true>, ComparisonEntryType> compareCombine(paMatrix, 10, depth, origBacktester);
        compareCombine.combine();

    }
    else {
        std::cout << "wrong usage, " << (argc - 1) << " arguments specified, needs to be a single argument.. " << std::endl;
    }


}
