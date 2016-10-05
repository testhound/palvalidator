// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __LOG_ROBUSTNESS_TEST_H
#define __LOG_ROBUSTNESS_TEST_H 1

#include <fstream>
#include "RobustnessTest.h"

namespace mkc_timeseries
{
  template <int Prec> class LogRobustnessTest
  {
  public:
    static void logRobustnessTestResults(const RobustnessCalculator<Prec> robustnessResults,
					 std::ofstream& outputFileStream)
    {
      typename RobustnessCalculator<Prec>::RobustnessTestResultIterator it = 
	robustnessResults.beginRobustnessTestResults();

      for (; it != robustnessResults. endRobustnessTestResults(); it++)
	LogRobustnessTest::logRobustnessTestResult (it->first, it->second, outputFileStream);
    }

    static void logRobustnessTestResult(const ProfitTargetStopPair<Prec>& key,
					std::shared_ptr<RobustnessTestResult<Prec>> testResult,
					std::ofstream& outputFileStream)
    {
      outputFileStream << key.getProfitTarget() << "," << key.getProtectiveStop();
      outputFileStream << "," << testResult->getMonteCarloProfitability() << ",";
      outputFileStream << testResult->getProfitFactor() << ",";
      outputFileStream << testResult->getNumTrades() << ",";
      outputFileStream << testResult->getPayOffRatio() << ",";
      outputFileStream << testResult->getMedianPayOffRatio() << ",";
      outputFileStream << testResult->getMonteCarloPayOffRatio() << std::endl;
    }

  private:
    LogRobustnessTest();
  };

}

#endif
