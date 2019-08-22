#ifndef STDESTIMATOR_H
#define STDESTIMATOR_H

#include "TimeSeriesIndicators.h"
#include "McptConfigurationFileReader.h"

using namespace mkc_timeseries;

namespace mkc_searchalgo
{
  template <class Decimal>
  class StdEstimator
  {
  public:
    StdEstimator(const std::shared_ptr<McptConfiguration<Decimal>>& configuration):
      mConfiguration(configuration)
    {}
    Decimal estimate()
    {
      std::string portfolioName(mConfiguration->getSecurity()->getName() + std::string(" Portfolio"));
      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(mConfiguration->getSecurity());
      shared_ptr<OHLCTimeSeries<Decimal>> timeSeries = mConfiguration->getSecurity()->getTimeSeries();

      OHLCTimeSeries<Decimal> aTimeSeries = FilterTimeSeries(*timeSeries, mConfiguration->getInsampleDateRange());

      int rocPeriod = 1;

      NumericTimeSeries<Decimal> closingPrices (aTimeSeries.CloseTimeSeries());
      NumericTimeSeries<Decimal> rocOfClosingPrices (RocSeries (closingPrices, rocPeriod));
      
      std::vector<Decimal> aSortedVec (rocOfClosingPrices.getTimeSeriesAsVector());

      Decimal medianOfRoc (Median (rocOfClosingPrices));
      RobustQn<Decimal> robustDev (rocOfClosingPrices);

      Decimal robustQn (robustDev.getRobustQn());

      std::cout << "Median of roc of close = " << medianOfRoc << std::endl;
      std::cout << "Qn of roc of close  = " << robustQn << std::endl;
      std::cout << "Combined  = " << (medianOfRoc + robustQn) << std::endl;

      return (medianOfRoc + robustQn);
    }

  private:
    std::shared_ptr<McptConfiguration<Decimal>> mConfiguration;
  };
}

#endif //STDESTIMATOR_H


