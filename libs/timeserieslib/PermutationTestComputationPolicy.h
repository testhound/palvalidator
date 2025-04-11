// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
//
#ifndef __PERMUTATION_TEST_COMPUTATION_POLICY_H
#define __PERMUTATION_TEST_COMPUTATION_POLICY_H 1

#include <exception>
#include <string>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "SyntheticTimeSeries.h"
#include "MonteCarloTestPolicy.h"

namespace mkc_timeseries
{
  template <class Decimal>
  inline uint32_t
  getNumClosedTrades(std::shared_ptr<BackTester<Decimal>> aBackTester)
  {
    std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
        (*(aBackTester->beginStrategies()));

    return backTesterStrategy->getStrategyBroker().getClosedTrades();
  }

  template <class Decimal>
  inline shared_ptr<Security<Decimal>>
  createSyntheticSecurity(shared_ptr<Security<Decimal>> aSecurity)
  {
    auto aTimeSeries = aSecurity->getTimeSeries();
    SyntheticTimeSeries<Decimal> aTimeSeries2(*aTimeSeries, aSecurity->getTick(), aSecurity->getTickDiv2());
    aTimeSeries2.createSyntheticSeries();

    return aSecurity->clone (aTimeSeries2.getSyntheticTimeSeries());
  }

  template <class Decimal>
  inline std::shared_ptr<Portfolio<Decimal>>
  createSyntheticPortfolio (std::shared_ptr<Security<Decimal>> realSecurity,
                            std::shared_ptr<Portfolio<Decimal>> realPortfolio)
  {
    std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio = realPortfolio->clone();
    syntheticPortfolio->addSecurity (createSyntheticSecurity<Decimal> (realSecurity));

    return syntheticPortfolio;
  }

  template <class Decimal,
	    class BackTestResultPolicy,
	    typename _PermutationTestResultPolicy = PValueReturnPolicy<Decimal>,
	    typename _PermutationTestStatisticsCollectionPolicy = PermutationTestingNullTestStatisticPolicy<Decimal>> class DefaultPermuteMarketChangesPolicy
  {
  public:
    using ComputationPolicyReturnType = typename _PermutationTestResultPolicy::ReturnType;

    DefaultPermuteMarketChangesPolicy()
    {}

    ~DefaultPermuteMarketChangesPolicy()
    {}

    static ComputationPolicyReturnType
    runPermutationTest (std::shared_ptr<BackTester<Decimal>> theBackTester,
                        uint32_t numPermutations,
                        const Decimal& baseLineTestStat)
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy =
          (*(theBackTester->beginStrategies()));

      shared_ptr<Security<Decimal>> theSecurity = aStrategy->beginPortfolio()->second;

      uint32_t count = 0;
      uint32_t i;

      // Construct test statistics class
      _PermutationTestStatisticsCollectionPolicy testStatisticCollection;

      for (i = 0; i < numPermutations; i++)
        {
          uint32_t stratTrades = 0;

          std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
          std::shared_ptr<BackTester<Decimal>> clonedBackTester;
          while (stratTrades < BackTestResultPolicy::getMinStrategyTrades())
            {
              clonedStrategy = aStrategy->clone (createSyntheticPortfolio<Decimal> (theSecurity,
                                                                                    aStrategy->getPortfolio()));

              clonedBackTester = theBackTester->clone();
              clonedBackTester->addStrategy(clonedStrategy);
              clonedBackTester->backtest();

              stratTrades = getNumClosedTrades<Decimal> (clonedBackTester);

            }

          Decimal testStatistic(BackTestResultPolicy::getPermutationTestStatistic(clonedBackTester));

          if (testStatistic >= baseLineTestStat)
            count++;

	  testStatisticCollection.updateTestStatistic(testStatistic);
        }

      Decimal pValue(Decimal(count) / Decimal (numPermutations));
      Decimal summaryTestStat(testStatisticCollection.getTestStat());

      return _PermutationTestResultPolicy::createReturnValue(pValue, summaryTestStat);
    }
  };

  template <class Decimal, class BackTestResultPolicy> class MultiStrategyPermuteMarketChangesPolicy
  {
  public:

    MultiStrategyPermuteMarketChangesPolicy()
    {}

    ~MultiStrategyPermuteMarketChangesPolicy()
    {}

    template <typename StrategyResultMapType>
    static Decimal
    runPermutationTest (std::shared_ptr<BackTester<Decimal>> theBackTester,
                        uint32_t numPermutations,
                        StrategyResultMapType &strategyBaselineReturns)
    {
      auto const& firstStrategyContainer = strategyBaselineReturns.begin()->second;
      auto const& aStrategy = std::get<0>(firstStrategyContainer);
      shared_ptr<Security<Decimal>> theSecurity = aStrategy->beginPortfolio()->second;

      uint32_t i;
      uint32_t numberOfValidTests = 0;

      for (i = 0; i < numPermutations; i++)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
          std::shared_ptr<BackTester<Decimal>> clonedBackTester;

          std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio = createSyntheticPortfolio<Decimal> (theSecurity,
                                                                                                      aStrategy->getPortfolio());
          uint32_t stratTrades;
          std::string synthMarketId;
          synthMarketId = " running on Synthetic market #" + std::to_string(i);
          std::cout << synthMarketId << std::endl;

          // Iterate map, and strategies, and run backtest with given strategy+sytheticPortfolio/timeseries
          for (auto const& [outerBaselineStat, outerStratContainer] : strategyBaselineReturns)
            {
	      // Avoid unused warning
	      (void) outerBaselineStat;

              auto const& strategy = std::get<0>(outerStratContainer);
              std::stringstream ss;
              ss << "Synthetic " << strategy->getStrategyName() << synthMarketId;
              std::string sytheticStrategyName = ss.str();

              clonedStrategy = strategy->clone(syntheticPortfolio);
              clonedBackTester = theBackTester->clone();
              clonedBackTester->addStrategy(clonedStrategy);
              clonedBackTester->backtest();
              stratTrades = getNumClosedTrades<Decimal> (clonedBackTester);
              if (stratTrades > 0)
                {
                  Decimal syntheticStat(BackTestResultPolicy::getPermutationTestStatistic(clonedBackTester));

                  // Compare synthetic strategy result with all real strategies
                  for (auto & [innerBaselineStat, innerStratConainer] : strategyBaselineReturns)
                    {
                      // Increment number of times beaten recorded for real strategies
                      if (syntheticStat > innerBaselineStat)
                        std::get<1>(innerStratConainer) ++;
                      else
                        break;  // Can break out of the loop because of ordering

                    }
                }
              // A strategy with no trading is not a strategy with a decision not to trade, rather it is a market that produced no patterns.
              // Whether to consider as beaten by all, or to disregard, remains an open question. (chosen approach is to consider it beaten)
              numberOfValidTests++;

            }
        }
      return Decimal(numberOfValidTests);         // Conforming to return type
    }
  };

  template <class Decimal, class BackTestResultPolicy> class ShortCutPermuteMarketChangesPolicy
  {
  public:
    ShortCutPermuteMarketChangesPolicy()
    {}

    ~ShortCutPermuteMarketChangesPolicy()
    {}

    static Decimal
    runPermutationTest (std::shared_ptr<BackTester<Decimal>> theBackTester,
                        uint32_t numPermutations,
                        const Decimal& baseLineTestStat)
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy =
          (*(theBackTester->beginStrategies()));

      shared_ptr<Security<Decimal>> theSecurity = aStrategy->beginPortfolio()->second;

      uint32_t count = 0;
      uint32_t i;

      Decimal shortCutThreshold (Decimal (numPermutations) * DecimalConstants<Decimal>::SignificantPValue);

      for (i = 0; i < numPermutations; i++)
        {
          uint32_t stratTrades = 0;

          std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
          std::shared_ptr<BackTester<Decimal>> clonedBackTester;
          while (stratTrades < BackTestResultPolicy::getMinStrategyTrades())
            {
              clonedStrategy = aStrategy->clone (createSyntheticPortfolio (theSecurity,
                                                                           aStrategy->getPortfolio()));

              clonedBackTester = theBackTester->clone();
              clonedBackTester->addStrategy(clonedStrategy);
              clonedBackTester->backtest();

              stratTrades = getNumClosedTrades<Decimal> (clonedBackTester);
            }

          Decimal testStatistic(BackTestResultPolicy::getPermutationTestStatistic(clonedBackTester));

          if (testStatistic >= baseLineTestStat)
            {
              count++;

              // If the number of strategies with testStatistic > baseline test stat is
              // greater than the threshold there is no point continuing with the tests
              // if we don't need an accurate p-Value.

              if ((Decimal (count) + DecimalConstants<Decimal>::DecimalOne) > shortCutThreshold)
                return DecimalConstants<Decimal>::SignificantPValue;
            }

        }

      return Decimal(count) / Decimal (numPermutations);
    }
  };

}
#endif
