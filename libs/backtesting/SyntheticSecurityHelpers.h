#ifndef __SYNTHETIC_SECURITY_HELPERS_H
#define __SYNTHETIC_SECURITY_HELPERS_H 1

#include <memory>
#include "number.h"
#include "Security.h"
#include "Portfolio.h"
#include "TimeSeries.h"
#include "SyntheticTimeSeries.h"

namespace mkc_timeseries
{
  template <class Decimal>
  inline shared_ptr<Security<Decimal>>
  createSyntheticSecurity(const shared_ptr<Security<Decimal>>& aSecurity)
  {
    auto aTimeSeries = aSecurity->getTimeSeries();
    SyntheticTimeSeries<Decimal> aTimeSeries2(*aTimeSeries, aSecurity->getTick(), aSecurity->getTickDiv2());
    aTimeSeries2.createSyntheticSeries();

    return aSecurity->clone (aTimeSeries2.getSyntheticTimeSeries());
  }

  template <class Decimal>
  inline std::shared_ptr<Portfolio<Decimal>>
  createSyntheticPortfolio (const std::shared_ptr<Security<Decimal>>& realSecurity,
                            const std::shared_ptr<Portfolio<Decimal>>& realPortfolio)
  {
    std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio = realPortfolio->clone();
    syntheticPortfolio->addSecurity (createSyntheticSecurity<Decimal> (realSecurity));

    return syntheticPortfolio;
  }
}

#endif
