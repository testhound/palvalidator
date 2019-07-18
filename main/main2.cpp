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

using namespace mkc_timeseries;
using namespace mkc_searchalgo;
using std::shared_ptr;
using Decimal = num::DefaultNumber;

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
        std::shared_ptr<OHLCTimeSeries<Decimal>> series = configuration->getSecurity()->getTimeSeries();

        typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = series->beginRandomAccess();

        ComparisonsGenerator<Decimal> compareGenerator(15);

        for (; it != series->endRandomAccess(); it++)
        {

            const Decimal& cOpen = series->getOpenValue (it, 0);
            const Decimal& cHigh = series->getHighValue (it, 0);
            const Decimal& cLow = series->getLowValue (it, 0);
            const Decimal& cClose = series->getCloseValue (it, 0);

            auto dt = series->getDateValue(it, 0);
            std::cout << dt << " OHLC: " << cOpen << "," << cHigh << "," << cLow << "," << cClose << std::endl;

            compareGenerator.addNewLastBar(cOpen, cHigh, cLow, cClose);

//            OHLCBar<Decimal, 4> newBar(cOpen, cHigh, cLow, cClose, 0);

//            std::cout << (cOpen > cHigh) << "," <<  (cOpen > cLow) << "," << (cOpen > cHigh) << "," << (cOpen > cClose) <<  std::endl;

//         mRelativeOpen.push_back(currentOpen /
//                     mTimeSeries.getCloseValue (it, 1));
//        mRelativeHigh.push_back(mTimeSeries.getHighValue (it, 0) /
//                     currentOpen) ;
//        mRelativeLow.push_back(mTimeSeries.getLowValue (it, 0) /
//                    currentOpen) ;
//        mRelativeClose.push_back(mTimeSeries.getCloseValue (it, 0) /
//        currentOpen) ;

//        mDateSeries.addElement (mTimeSeries.getDateValue(it,0));
        }

        std::cout << " Full comparisons universe #:" << compareGenerator.getComparisonsCount() << std::endl;
        std::cout << " Unique comparisons #:" << compareGenerator.getUniqueComparisons().size() << std::endl;
        //for (series->beginRandomAccess()
        UniqueSinglePAMatrix<Decimal> paVectorMatrix(compareGenerator.getUniqueComparisons(), series->getNumEntries());
        std::cout << "vectorizing..." << std::endl;
        paVectorMatrix.vectorizeComparisons(compareGenerator.getComparisons());
        const std::map<ComparisonEntryType, std::valarray<int>>& matrix = paVectorMatrix.getMatrix();

        std::map<ComparisonEntryType, std::valarray<int>>::const_iterator m_it = matrix.begin();
        int c = 0;
        for (; m_it != matrix.end(); ++m_it)
          {

            std::map<ComparisonEntryType, std::valarray<int>>::const_iterator m_it2 = matrix.begin();
            for (; m_it2 != matrix.end(); ++m_it2)
              {
                if (m_it2 == m_it)
                    continue;

                std::valarray<int> newvec = m_it->second * m_it2->second;
                std::cout << c << " vec size: " << newvec.sum() << "from vec1: " << m_it->second.sum() << " and vec2: " << m_it2->second.sum() << std::endl;
                c++;
              }

          }

    }
    else {
        std::cout << "wrong usage, " << (argc - 1) << " arguments specified, needs to be a single argument.. " << std::endl;
    }


}
