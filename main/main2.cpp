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

using namespace mkc_timeseries;
using namespace mkc_searchalgo;
using std::shared_ptr;
using Decimal = num::DefaultNumber;

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

        ComparisonsCombiner<Decimal, BackTester<Decimal>, Portfolio<Decimal>, ComparisonEntryType> compareCombine(paMatrix, 10, depth, backtester, aPortfolio);
        compareCombine.combine();

    }
    else {
        std::cout << "wrong usage, " << (argc - 1) << " arguments specified, needs to be a single argument.. " << std::endl;
    }


}
