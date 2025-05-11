// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __MONTE_CARLO_POLICY_H
#define __MONTE_CARLO_POLICY_H 1

#include <exception>
#include <string>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "StatUtils.h"

namespace mkc_timeseries
{
  template <class Decimal>
  class AllHighResLogPFPolicy
  {
  public:
    /**
     * @brief Compute the permutation‐test statistic as the log‐profit‐factor
     *        over every bar‐by‐bar return (closed + open) for the sole strategy.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      // We expect exactly one strategy per cloned backtester:
      if (bt->getNumStrategies() != 1) {
	throw BackTesterException(
				  "AllHighResLogPFPolicy::getPermutationTestStatistic - "
				  "expected one strategy, got " 
				  + std::to_string(bt->getNumStrategies()));
      }

      // Grab the only strategy pointer:
      auto stratPtr = *(bt->beginStrategies());

      // Pull every bar‐by‐bar return (entry→exit and any still‐open):
      std::vector<Decimal> barSeries = bt->getAllHighResReturns(stratPtr.get());

      // Compute the bias‐corrected log‐profit‐factor on that fine‐res data:
      return StatUtils<Decimal>::computeLogProfitFactor(barSeries);
    }

    /// Minimum number of closed trades required to even attempt this test
    static unsigned int getMinStrategyTrades() { return 3; }
  };

  template <class Decimal> class NonGranularProfitFactorPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getLogProfitFactor();
        }
      else
        throw BackTesterException("NonGranularProfitFactorPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

  };

  
  template <class Decimal> class CumulativeReturnPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();
        }
      else
        throw BackTesterException("CumulativeReturnPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

  };


  template <class Decimal> class NormalizedReturnPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {

      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          double factor;
          Decimal cumulativeReturn;
          uint32_t timeInMarket;
          Decimal normalizationRatio;

          factor = sqrt(backTesterStrategy->numTradingOpportunities());

          timeInMarket = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getNumBarsInMarket();

          if (timeInMarket == 0)
            throw BackTesterException("NormalizedReturnPolicy::getPermutationTestStatistic - time in market cannot be 0!");

          cumulativeReturn = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();

          normalizationRatio = Decimal( (factor / sqrt(timeInMarket)) );

          return cumulativeReturn * normalizationRatio;
        }
      else
        throw BackTesterException("NormalizedReturnPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

  };

  //
  template <class Decimal> class PalProfitabilityPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getMedianPALProfitability();
        }
      else
        throw BackTesterException("PalProfitabilityPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

  };

  //
  template <class Decimal> class PessimisticReturnRatioPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getPessimisticReturnRatio();
        }
      else
        throw BackTesterException("PessimisticReturnRatioPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }

    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }
  };

}

#endif

