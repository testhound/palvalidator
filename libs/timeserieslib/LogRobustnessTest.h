// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __LOG_ROBUSTNESS_TEST_H
#define __LOG_ROBUSTNESS_TEST_H 1

#include <fstream>
#include "SummaryStats.h"
#include "TimeSeries.h"
#include "RobustnessTest.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  
  template <class Decimal> class LogRobustnessTest
  {
  public:
    static void logRobustnessTestResults(const RobustnessCalculator<Decimal> robustnessResults,
					 std::ofstream& outputFileStream)
    {
      typename RobustnessCalculator<Decimal>::RobustnessTestResultIterator it = 
	robustnessResults.beginRobustnessTestResults();

      SummaryStats<Decimal> robustnessStats;
      SummaryStats<Decimal> profitFactorStats;
      
      for (; it != robustnessResults. endRobustnessTestResults(); it++)
	{
	  robustnessStats.addValue (it->second->getMonteCarloProfitability());
	  profitFactorStats.addValue (it->second->getProfitFactor());
	  
	  LogRobustnessTest::logRobustnessTestResult (it->first, it->second, outputFileStream);
	}

      LogRobustnessTest::logSummaryStats (robustnessStats, profitFactorStats, outputFileStream);
    }

    static void logRobustnessTestResult(const ProfitTargetStopPair<Decimal>& key,
					std::shared_ptr<RobustnessTestResult<Decimal>> testResult,
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
    static void logSummaryStats (SummaryStats<Decimal>& stats,
				 SummaryStats<Decimal>& profitFactorStats,
				 std::ofstream& outputFileStream)
    {
      //double statMedianDouble (stats.getMedian());
      //Decimal statMedian (statMedianDouble);

      Decimal statMedian (stats.getMedian());
	    
      Decimal statQn (stats.getRobustQn());
      Decimal statQn2 (statQn * DecimalConstants<Decimal>::DecimalTwo);
      Decimal lowerDev1 (statMedian - statQn);
      Decimal upperDev1 (statMedian + statQn);
      Decimal lowerDev2 (statMedian - statQn2);
      Decimal upperDev2 (statMedian + statQn2);

      Decimal profitFactorMedian (profitFactorStats.getMedian());
      Decimal profitFactorQn (profitFactorStats.getRobustQn());
      Decimal profitFactorQn2 (profitFactorQn * DecimalConstants<Decimal>::DecimalTwo);
      Decimal lowerProfitFactorDev1 (profitFactorMedian - profitFactorQn);
      Decimal lowerProfitFactorDev2 (profitFactorMedian - profitFactorQn2);
	    
      outputFileStream << "Profitability Smallest value = " << stats.getSmallestValue() << std::endl;
      outputFileStream << "Profitability Largest value = " << stats.getLargestValue() << std::endl;
      outputFileStream << "Profitability Median value = " << statMedian << std::endl;
      outputFileStream << "Profitability Robust Qn = " << statQn << std::endl;
      outputFileStream << "Profitability Lower One std Dev = " << lowerDev1 << std::endl;
      outputFileStream << "Profitability Upper One std Dev = " << upperDev1 << std::endl;
      outputFileStream << "Profitability Lower Two std Dev = " << lowerDev2 << std::endl;
      outputFileStream << "Profitability Upper Two std Dev = " << upperDev2 << std::endl;
      outputFileStream << "Profit Factor Median value = " << profitFactorMedian << std::endl;
      outputFileStream << "Profit Factor Robust Qn = " << profitFactorQn << std::endl;
      outputFileStream << "Profit Factor Lower One Std Dev = " << lowerProfitFactorDev1 << std::endl;
      outputFileStream << "Profit Factor Lower Two Std Dev = " << lowerProfitFactorDev2 << std::endl;
    }
    
    LogRobustnessTest();
  };

}

#endif
