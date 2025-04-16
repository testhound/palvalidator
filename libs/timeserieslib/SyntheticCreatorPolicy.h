// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
//
#ifndef __SYNTHETIC_CREATOR_POLICY_H
#define __SYNTHETIC_CREATOR_POLICY_H 1

#include <exception>
#include <string>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "SyntheticTimeSeries.h"

namespace mkc_timeseries
{
  template <class Decimal, class SecurityCreatorPolicy> class PortfolioClonePolicy
  {
  public:
    static std::shared_ptr<Portfolio<Decimal>>
    createSyntheticPortfolio (std::shared_ptr<Security<Decimal>> realSecurity,
			      std::shared_ptr<Portfolio<Decimal>> realPortfolio)
    {
      std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio = realPortfolio->clone();
      syntheticPortfolio->addSecurity (SecurityCreatorPolicy<Decimal>::createSyntheticSecurity (realSecurity));

      return syntheticPortfolio;
    }
  };

  template <class Decimal> class SecurityClonePolicy
  {
  public:
    static shared_ptr<Security<Decimal>>
    createSyntheticSecurity(shared_ptr<Security<Decimal>> aSecurity)
    {
      auto aTimeSeries = aSecurity->getTimeSeries();
      SyntheticTimeSeries<Decimal> aTimeSeries2(*aTimeSeries, aSecurity->getTick(), aSecurity->getTickDiv2());
      aTimeSeries2.createSyntheticSeries();

      return aSecurity->clone (aTimeSeries2.getSyntheticTimeSeries());
    }
  };
}
#endif
