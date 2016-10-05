// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __MONTE_CARLO_POLICY_H
#define __MONTE_CARLO_POLICY_H 1

#include <exception>
#include <string>
#include "decimal.h"
#include "DecimalConstants.h"
#include "BackTester.h"

namespace mkc_timeseries
{
  using dec::decimal;
 
  template <int Prec> class CumulativeReturnPolicy
    {
    public:
      static decimal<Prec> getPermutationTestStatistic(std::shared_ptr<BackTester<Prec>> aBackTester)
      {
	if (aBackTester->getNumStrategies() == 1)
	{
	std::shared_ptr<BacktesterStrategy<Prec>> backTesterStrategy = 
	    (*(aBackTester->beginStrategies()));

	  return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();
	}
      else
	throw BackTesterException("CumulativeReturnPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
      }
    };

    template <int Prec> class PessimisticReturnRatioPolicy
    {
    public:
      static decimal<Prec> getPermutationTestStatistic(std::shared_ptr<BackTester<Prec>> aBackTester)
      {
	if (aBackTester->getNumStrategies() == 1)
	{
	std::shared_ptr<BacktesterStrategy<Prec>> backTesterStrategy = 
	    (*(aBackTester->beginStrategies()));

	  return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getPessimisticReturnRatio();
	}
      else
	throw BackTesterException("PessimisticReturnRatioPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
      }
    };

}

#endif

