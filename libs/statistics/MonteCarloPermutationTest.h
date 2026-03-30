// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

/**
 * @file MonteCarloPermutationTest.h
 * @brief Monte Carlo permutation test framework for evaluating trading strategy significance.
 *
 * Provides MonteCarloPermuteMarketChanges (permutes market returns then re-runs
 * the strategy), OriginalMCPT (Fisher–Yates shuffle on trade returns), and
 * MonteCarloPayoffRatio (synthetic backtests for payoff ratio assessment).
 */

#ifndef __MONTE_CARLO_PERMUTATION_TEST_H
#define __MONTE_CARLO_PERMUTATION_TEST_H 1

#include <exception>
#include <string>
#include <boost/date_time.hpp>
#include "number.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "SyntheticTimeSeries.h"
#include "MonteCarloTestPolicy.h"
#include "PermutationTestComputationPolicy.h"
#include "PermutationTestSubject.h"
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/count.hpp>

#include "runner.hpp"

namespace mkc_timeseries
{
  using boost::gregorian::date;

  using boost::accumulators::accumulator_set;
  using boost::accumulators::stats;
  using boost::accumulators::median;
  using boost::accumulators::count;

  typedef boost::accumulators::tag::median median_tag;
  typedef boost::accumulators::tag::count count_tag;

  /// @brief Exception thrown when a Monte Carlo permutation test encounters an error.
  class MonteCarloPermutationException : public std::runtime_error
  {
  public:
    MonteCarloPermutationException(const std::string msg)
      : std::runtime_error(msg)
    {}

    ~MonteCarloPermutationException()
    {}
  };

  /**
   * @brief Abstract base class for Monte Carlo permutation tests.
   *
   * @tparam Decimal    Numeric type for price and return calculations.
   * @tparam ReturnType The result type of the permutation test (defaults to Decimal).
   */
  template <class Decimal, typename ReturnType = Decimal> class MonteCarloPermutationTest
  {
  public:
    using ResultType = ReturnType;

    MonteCarloPermutationTest()
    {}

    virtual ~MonteCarloPermutationTest()
    {}

    virtual ReturnType runPermutationTest() = 0;

  protected:
    uint32_t
    getNumClosedTrades(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
          (*(aBackTester->beginStrategies()));

      return backTesterStrategy->getStrategyBroker().getClosedTrades();
    }

    Decimal getCumulativeReturn(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();
        }
      else
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges::getCumulativeReturn - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }

    void validateStrategy( std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy) const
    {
      if (aStrategy->getNumSecurities() == 0)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: no securities in portfolio to test");

      if (aStrategy->getNumSecurities() != 1)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: MCPT is only designed to test one security at a time");
    }

  };

  /**
   * @brief Monte Carlo permutation test that creates synthetic time series and re-runs the strategy.
   *
   * For each permutation, the original OHLC time series is shuffled to produce a
   * synthetic series. The strategy is then backtested on each synthetic series and
   * the resulting test statistic is compared against the baseline (unpermuted) result
   * to compute a p-value.
   *
   * The back-test result metric (e.g., cumulative return) and the permutation
   * computation logic are controlled by policy template parameters, allowing the
   * caller to swap in alternative statistics or parallelization strategies.
   *
   * This class also inherits from PermutationTestSubject so that observers can be
   * attached to monitor progress during the permutation loop.
   *
   * @tparam Decimal              Numeric type for price and return calculations.
   * @tparam _BackTestResultPolicy Policy that extracts the test statistic from a completed backtest.
   * @tparam _ComputationPolicy   Policy that orchestrates the permutation loop and p-value computation.
   *
   * @see MonteCarloPermutationTest
   * @see PermutationTestSubject
   */
   template <class Decimal,
           template <class Decimal2> class _BackTestResultPolicy = CumulativeReturnPolicy,
           typename _ComputationPolicy = DefaultPermuteMarketChangesPolicy<Decimal,_BackTestResultPolicy<Decimal>>>
 class MonteCarloPermuteMarketChanges
   : public MonteCarloPermutationTest<Decimal, typename _ComputationPolicy::ReturnType>,
     public PermutationTestSubject<Decimal>
  {
  public:
    // pull the policy’s ReturnType in
    using ReturnType = typename _ComputationPolicy::ReturnType;
    
    /**
     * @brief Constructs the permutation test with a backtester, iteration count, and significance level.
     *
     * @param backtester            Backtester preloaded with exactly one strategy and one security.
     * @param numPermutations       Number of synthetic permutations to run. Must be >= 10.
     * @param pValueSignificalLevel Significance threshold for the p-value (default: SignificantPValue).
     *
     * @throws MonteCarloPermutationException If numPermutations is 0 or < 10, or if
     *         the backtester does not contain exactly one strategy.
     */
    MonteCarloPermuteMarketChanges (std::shared_ptr<BackTester<Decimal>> backtester,
                                    uint32_t numPermutations,
				    const Decimal& pValueSignificalLevel = DecimalConstants<Decimal>::SignificantPValue)
      : MonteCarloPermutationTest<Decimal, ReturnType>(),
        mBackTester (backtester),
        mNumPermutations(numPermutations),
        mBaseLineTestStat(DecimalConstants<Decimal>::DecimalZero),
	mPValueSignificalLevel(pValueSignificalLevel)
    {
      if (numPermutations == 0)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: num of permuations must be greater than zero");

      if (numPermutations < 10)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: num of permuations should be >= 100 for solution to converge");

      if (mBackTester->getNumStrategies() == 0)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: No strategy associated with backtester");

      if (mBackTester->getNumStrategies() != 1)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: Only one strategy can be associated with backtester for MCPT");
    }

    ~MonteCarloPermuteMarketChanges()
    {}

    /**
     * @brief Runs the Monte Carlo permutation test and returns the p-value.
     *
     * Backtests the original (unpermuted) time series to establish a baseline
     * statistic, then delegates to the computation policy for the permutation
     * loop. Attached observers are forwarded to the policy so they receive
     * progress notifications.
     *
     * @return The p-value (or policy-defined result) indicating the fraction
     *         of permuted outcomes that meet or exceed the baseline statistic.
     *
     * @throws MonteCarloPermutationException If the strategy has no securities
     *         or more than one security.
     */
    ReturnType runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy =
          (*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      auto theSecurity = aStrategy->beginPortfolio()->second;
      auto theTimeSeries = theSecurity->getTimeSeries();

      //std::cout << "Running MCPT backtest from " << mBackTester->getStartDate() << " to " << mBackTester->getEndDate() << std::endl << std::endl;
      // Run backtest on security with orginal unpermuted time series
      mBackTester->backtest();

      // If we have too few trades don't trust results

      mBaseLineTestStat = _BackTestResultPolicy<Decimal>::getPermutationTestStatistic(mBackTester);
      //std::cout << "Baseline test stat. for original  strategy equals: " <<  mBaseLineTestStat << ", baseline # trades:" << this->getNumClosedTrades (mBackTester) <<  std::endl << std::endl;

      // Create instance of computation policy for observer support
      _ComputationPolicy computationPolicy;
      
      // Chain attached observers to the computation policy (pass-through Subject design)
      {
	std::shared_lock<std::shared_mutex> observerLock(this->m_observersMutex);
	for (auto* observer : this->m_observers) {
	  if (observer) {
            computationPolicy.attach(observer);
	  }
	}
      }

      return computationPolicy.runPermutationTest (mBackTester, mNumPermutations, mBaseLineTestStat, mPValueSignificalLevel);
    }

  private:
    std::shared_ptr<BackTester<Decimal>> mBackTester;
    uint32_t mNumPermutations;
    Decimal mBaseLineTestStat;
    Decimal  mPValueSignificalLevel;
    
  };


  /**
   * @brief Original Monte Carlo permutation test using Fisher-Yates shuffle on trade returns.
   *
   * Implements the MCPT algorithm described in "Monte-Carlo Evaluation of Trading
   * Systems." Rather than generating synthetic time series, this variant shuffles the
   * position direction vector (long/short/flat) via the Fisher-Yates algorithm and
   * recomputes the cumulative return for each shuffled permutation. The p-value is
   * the fraction of permuted returns that equal or exceed the original strategy return.
   *
   * @tparam Decimal Numeric type for price and return calculations.
   *
   * @see MonteCarloPermutationTest
   * @see MonteCarloPermuteMarketChanges
   */
  template <class Decimal> class OriginalMCPT :
      public MonteCarloPermutationTest<Decimal>
  {
  public:
    /**
     * @brief Constructs the original MCPT with a backtester and iteration count.
     *
     * @param backtester      Backtester preloaded with exactly one strategy and one security.
     * @param numPermutations Number of random permutations to evaluate. Must be >= 100.
     *
     * @throws MonteCarloPermutationException If numPermutations is 0 or < 100, or if
     *         the backtester does not contain exactly one strategy.
     */
    OriginalMCPT (std::shared_ptr<BackTester<Decimal>> backtester,
                  uint32_t numPermutations)
      : MonteCarloPermutationTest<Decimal>(),
        mBackTester (backtester),
        mNumPermutations(numPermutations),
        mBaseLineCumulativeReturn(DecimalConstants<Decimal>::DecimalZero)
    {
      if (numPermutations == 0)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: num of permuations must be greater than zero");

      if (numPermutations < 100)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: num of permuations should be >= 100 for solution to converge");

      if (mBackTester->getNumStrategies() == 0)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: No strategy associated with backtester");

      if (mBackTester->getNumStrategies() != 1)
        throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges: Only one strategy can be associated with backtester for MCPT");
    }

    ~OriginalMCPT()
    {}

    /**
     * @brief Runs the permutation test and returns the p-value.
     *
     * Backtests the original strategy to obtain the baseline cumulative return,
     * then delegates to permuteAndGetPValue() for the Fisher-Yates shuffle loop.
     * If the strategy produces fewer than 4 closed trades the result is not
     * trustworthy, so a p-value of 1.0 is returned immediately.
     *
     * @return P-value in [0, 1] representing the fraction of shuffled outcomes
     *         that equal or exceed the baseline return. A value of 1.0 indicates
     *         too few trades for a meaningful test.
     *
     * @throws MonteCarloPermutationException If the strategy has no securities
     *         or more than one security.
     */
    Decimal runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy =
          (*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Decimal>> theSecurity = aStrategy->beginPortfolio()->second;
      std::shared_ptr<OHLCTimeSeries<Decimal>> theTimeSeries = theSecurity->getTimeSeries();

      mBackTester->backtest();

      // If we have too few trades don't trust results

      if (this->getNumClosedTrades (mBackTester) < 4)
        {
          //std::cout << " runPermutationTest: number of trades = " <<
          //getNumClosedTrades (mBackTester) << std::endl;
          return DecimalConstants<Decimal>::DecimalOne;
        }

      mBaseLineCumulativeReturn = this->getCumulativeReturn (mBackTester);
      return permuteAndGetPValue (aStrategy->numTradingOpportunities(),
                                  aStrategy->getPositionReturnsVector().data(),
                                  aStrategy->getPositionDirectionVector().data(),
                                  mNumPermutations);
    }

    /**
     * @brief Performs the Fisher-Yates shuffle loop and computes the p-value.
     *
     * For each of @p nreps repetitions the position direction vector is shuffled
     * uniformly at random using the Fisher-Yates algorithm, and the dot product
     * of shuffled positions with raw returns is computed. The p-value is
     * (count + 1) / (nreps + 1), where count is the number of shuffled returns
     * that equal or exceed the candidate (original) return.
     *
     * @param numTradingOpportunities Length of the position and returns arrays.
     * @param rawReturnsVector        Per-bar returns for every trading opportunity.
     * @param positionVector          Position direction (+1, -1, or 0) per bar.
     * @param nreps                   Number of shuffle repetitions.
     * @return P-value in (0, 1] indicating statistical significance.
     */
    Decimal permuteAndGetPValue(int numTradingOpportunities,
                                Decimal *rawReturnsVector,
                                int *positionVector,
                                int nreps)
    {
      int i, irep, k1, k2, count, temp;
      Decimal cand_return, trial_return;

      int *workSeries(new int[numTradingOpportunities]);

      try
      {
        memcpy(workSeries, positionVector, numTradingOpportunities * sizeof(int));

        /*
        Compute the return of the candidate model
        If requested, do the same for the nonparametric version
      */

        cand_return = DecimalConstants<Decimal>::DecimalZero;

        for (i = 0; i < numTradingOpportunities; i++)
          cand_return += Decimal(positionVector[i]) * rawReturnsVector[i];

        RandomMersenne randNumGen;
        //DrawRandomNumber<pcg32> randNumGen;

        count = 0; // Counts how many at least as good as candidate

        for (irep = 0; irep < nreps; irep++)
          {
            k1 = numTradingOpportunities; // Shuffle the positions, which are in 'work'

            while (k1 > 1) { // While at least 2 left to shuffle
                k2 = randNumGen.DrawNumber(0, k1 - 1); // Pick an int from 0 through k1-1

                if (k2 >= k1) // Should never happen as long as unifrand()<1
                  k2 = k1 - 1; // But this is cheap insurance against disaster

                temp = workSeries[--k1]; // Count down k1 and swap k1, k2 entries
                workSeries[k1] = workSeries[k2];
                workSeries[k2] = temp;
              } // Shuffling is complete when this loop exits

            trial_return = Decimal(0.0);
            for (i = 0; i < numTradingOpportunities; i++) // Compute return for this randomly shuffled system
              trial_return += Decimal(workSeries[i]) * rawReturnsVector[i];

            if (trial_return >= cand_return) // If this random system beat candidate
              ++count; // Count it
          }

        delete[] workSeries;
        return Decimal ((count + 1.0) / (mNumPermutations + 1.0));
      }
      catch (...)
      {
        delete[] workSeries;
        throw;   // Rethrow original exception
      }

    }

  private:
    std::shared_ptr<BackTester<Decimal>> mBackTester;
    uint32_t mNumPermutations;
    Decimal mBaseLineCumulativeReturn;
  };



  ////////////////

  /**
   * @brief Monte Carlo permutation test that estimates the payoff ratio via synthetic backtests.
   *
   * Creates multiple synthetic time series, backtests the strategy on each, and
   * accumulates winning and losing trade returns across all permutations. The
   * payoff ratio is computed as median(winners) / median(losers), converging to
   * a stable estimate as the number of permutations increases.
   *
   * Permutations are dispatched in parallel using the Boost thread-pool runner.
   * Accumulator access is serialized with a mutex to ensure thread safety.
   *
   * @tparam Decimal Numeric type for price and return calculations.
   *
   * @see MonteCarloPermutationTest
   * @see SyntheticTimeSeries
   */
  template <class Decimal> class MonteCarloPayoffRatio :
      public MonteCarloPermutationTest<Decimal>
  {
  public:
    /**
     * @brief Constructs the payoff-ratio permutation test.
     *
     * @param backtester      Backtester preloaded with exactly one strategy and one security.
     * @param numPermutations Number of synthetic backtests to run. Must be >= 10.
     *
     * @throws MonteCarloPermutationException If numPermutations is 0 or < 10, or if
     *         the backtester does not contain exactly one strategy.
     */
    MonteCarloPayoffRatio (std::shared_ptr<BackTester<Decimal>> backtester,
                           uint32_t numPermutations)
      : MonteCarloPermutationTest<Decimal>(),
        mBackTester (backtester),
        mNumPermutations(numPermutations),
        mWinnersStats(),
        mLosersStats()
    {
      if (numPermutations == 0)
        throw MonteCarloPermutationException("MonteCarloPayoffRatio: num of permuations must be greater than zero");

      if (numPermutations < 10)
        throw MonteCarloPermutationException("MonteCarloPayoffRatio: num of permuations should be >= 100 for solution to converge");

      if (mBackTester->getNumStrategies() == 0)
        throw MonteCarloPermutationException("MonteCarloPayoffRatio: No strategy associated with backtester");

      if (mBackTester->getNumStrategies() != 1)
        throw MonteCarloPermutationException("MonteCarloPayoffRatio: Only one strategy can be associated with backtester for MCPT");
    }

    ~MonteCarloPayoffRatio()
    {}

    /**
     * @brief Runs the Monte Carlo permutation test and returns the simulated payoff ratio.
     *
     * Dispatches @c mNumPermutations synthetic backtests in parallel. Each
     * permutation clones the strategy with a synthetic time series, runs the
     * backtest, and accumulates winner and loser trade returns into Boost
     * accumulators. The payoff ratio is median(winners) / median(losers).
     *
     * @return The estimated payoff ratio, or zero if there are no winning or
     *         losing trades across all permutations.
     *
     * @throws MonteCarloPermutationException If the strategy has no securities
     *         or more than one security.
     */
    /* //original code, safe to delete after parallelization
    Decimal runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy =
    (*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Decimal>> theSecurity = aStrategy->beginPortfolio()->second;

      uint32_t i;
      for (i = 0; i < mNumPermutations; i++)
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy =
        aStrategy->clone (createSyntheticPortfolio (theSecurity, aStrategy->getPortfolio()));

      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mBackTester->clone();
      clonedBackTester->addStrategy(clonedStrategy);
      clonedBackTester->backtest();

      ClosedPositionHistory<Decimal> history = clonedStrategy->getStrategyBroker().getClosedPositionHistory();
      typename ClosedPositionHistory<Decimal>::ConstTradeReturnIterator winnersIterator =
        history.beginWinnersReturns();

      for (; winnersIterator != history.endWinnersReturns(); winnersIterator++)
        {
          //std::cout << "Winners trade return " << *winnersIterator << std::endl;
          mWinnersStats (*winnersIterator);
        }

      typename ClosedPositionHistory<Decimal>::ConstTradeReturnIterator losersIterator =
        history.beginLosersReturns();

      for (; losersIterator != history.endLosersReturns(); losersIterator++)
        {
          mLosersStats (*losersIterator);
          //std::cout << "Losers trade return " << *losersIterator << std::endl;
        }

    }

      if ((count (mWinnersStats) > 0) && (count (mLosersStats) > 0))
    {
      return Decimal (median (mWinnersStats) / median (mLosersStats));
    }
      else
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
    }
*/

    Decimal runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy =
          (*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Decimal>> theSecurity = aStrategy->beginPortfolio()->second;

      runner& Runner=runner::instance();
      std::vector<boost::unique_future<void>> errorsVector;

      uint32_t i;
      for (i = 0; i < mNumPermutations; i++)
        {
          errorsVector.emplace_back(Runner.post([this,aStrategy,theSecurity](){
              std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy =
                  aStrategy->clone (createSyntheticPortfolio (theSecurity, aStrategy->getPortfolio()));

              std::shared_ptr<BackTester<Decimal>> clonedBackTester = mBackTester->clone();
              clonedBackTester->addStrategy(clonedStrategy);

              clonedBackTester->backtest();

              ClosedPositionHistory<Decimal> history = clonedStrategy->getStrategyBroker().getClosedPositionHistory();

              auto winnersIterator = history.beginWinnersReturns();
              auto losersIterator = history.beginLosersReturns();

              boost::mutex::scoped_lock Lock(accumMutex);
              for (; winnersIterator != history.endWinnersReturns(); winnersIterator++)
                {
                  //std::cout << "Winners trade return " << *winnersIterator << std::endl;
                  mWinnersStats (*winnersIterator);
                }

              for (; losersIterator != history.endLosersReturns(); losersIterator++)
                {
                  mLosersStats (*losersIterator);
                  //std::cout << "Losers trade return " << *losersIterator << std::endl;
                }
            }));

        }
      for(std::size_t slot=0;slot<errorsVector.size();++slot)
        {
          try
          {
            errorsVector[slot].get();
          }
          catch(std::exception const& e)
          {
            std::cerr<<"MonteCarloPayoffRatio::runPermutationTest error: "<<e.what()<<std::endl;
          }
        }
      if ((count (mWinnersStats) > 0) && (count (mLosersStats) > 0))
        {
          return Decimal (median (mWinnersStats) / median (mLosersStats));
        }
      else
        {
          return DecimalConstants<Decimal>::DecimalZero;
        }
    }

  private:
    std::shared_ptr<Portfolio<Decimal>> createSyntheticPortfolio (std::shared_ptr<Security<Decimal>> realSecurity,
                                                                  std::shared_ptr<Portfolio<Decimal>> realPortfolio)
    {
      std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio = realPortfolio->clone();
      syntheticPortfolio->addSecurity (createSyntheticSecurity (realSecurity));
      return syntheticPortfolio;
    }

    shared_ptr<Security<Decimal>> createSyntheticSecurity(shared_ptr<Security<Decimal>> aSecurity)
    {
      auto aTimeSeries = aSecurity->getTimeSeries();
      SyntheticTimeSeries<Decimal> aTimeSeries2(*aTimeSeries, aSecurity->getTick(), aSecurity->getTickDiv2());
      aTimeSeries2.createSyntheticSeries();

      return aSecurity->clone (aTimeSeries2.getSyntheticTimeSeries());
    }



  private:
    std::shared_ptr<BackTester<Decimal>> mBackTester;
    uint32_t mNumPermutations;
    boost::mutex                                          accumMutex;
    accumulator_set<double, stats<median_tag, count_tag>> mWinnersStats;
    accumulator_set<double, stats<median_tag, count_tag>> mLosersStats;
  };

}

#endif
