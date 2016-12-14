// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __MONTE_CARLO_PERMUTATION_TEST_H
#define __MONTE_CARLO_PERMUTATION_TEST_H 1

#include <exception>
#include <string>
#include <boost/date_time.hpp>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "SyntheticTimeSeries.h"
#include "MonteCarloTestPolicy.h"
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

 class MonteCarloPermutationException : public std::runtime_error
  {
  public:
    MonteCarloPermutationException(const std::string msg) 
      : std::runtime_error(msg)
      {}

    ~MonteCarloPermutationException()
      {}
  };

  template <class Decimal> class MonteCarloPermutationTest
  {
  public:
    MonteCarloPermutationTest()
    {}

    virtual ~MonteCarloPermutationTest()
    {}

    virtual Decimal runPermutationTest() = 0;

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

  //
  // class MonteCarloPermuteMarketChanges
  //
  // This class implements the MCPT by creating synthetic time series and permutting them
  //
  template <class Decimal,
	    template <class Decimal2> class BackTestResultPolicy = CumulativeReturnPolicy> class MonteCarloPermuteMarketChanges : public MonteCarloPermutationTest<Decimal> 

  {
  public:
    MonteCarloPermuteMarketChanges (std::shared_ptr<BackTester<Decimal>> backtester,
				    uint32_t numPermutations)
      : MonteCarloPermutationTest<Decimal>(),
	mBackTester (backtester),
	mNumPermutations(numPermutations),
	mBaseLineCumulativeReturn(DecimalConstants<Decimal>::DecimalZero)
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


    // Runs the monte carlo permutation test and return the P-Value
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
	  std::cout << " runPermutationTest: number of trades = " << 
	  this->getNumClosedTrades (mBackTester) << std::endl;
	  return DecimalConstants<Decimal>::DecimalOneHundred;
	}

      mBaseLineCumulativeReturn = BackTestResultPolicy<Decimal>::getPermutationTestStatistic(mBackTester);
      //std::cout << "Baseline test stat. for original  strategy equals: " <<  mBaseLineCumulativeReturn << ", baseline # trades:" << this->getNumClosedTrades (mBackTester) <<  std::endl << std::endl;

      uint32_t count = 0;
      uint32_t i;
      for (i = 0; i < mNumPermutations; i++)
	{
	  uint32_t stratTrades = 0;

	  std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
	  std::shared_ptr<BackTester<Decimal>> clonedBackTester;
	  while (stratTrades < 2)
	    {
	      clonedStrategy = aStrategy->clone (createSyntheticPortfolio (theSecurity,
									   aStrategy->getPortfolio()));

	      clonedBackTester = mBackTester->clone();
	      clonedBackTester->addStrategy(clonedStrategy);
	      clonedBackTester->backtest();

	      stratTrades = this->getNumClosedTrades (clonedBackTester);

	    }

	  Decimal cumulativeReturn(BackTestResultPolicy<Decimal>::getPermutationTestStatistic(clonedBackTester));
	  //std::cout << "Test stat. for strategy " << (i + 1) << " equals: " << cumulativeReturn << ", num trades = " << stratTrades << std::endl;

	  if (cumulativeReturn >= mBaseLineCumulativeReturn)
	    count++;
	}

      return Decimal((count + 1.0) / (mNumPermutations + 1.0));
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
    Decimal mBaseLineCumulativeReturn;
  };


  //
  // class OriginalMonteCarloPermutationTest
  //
  // This class implements the MCPT from the paper Monte-Carlo Evaluation of Trading Systems
  //

  template <class Decimal> class OriginalMCPT : 
    public MonteCarloPermutationTest<Decimal> 
  {
  public:
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

    // Runs the monte carlo permutation test and return the P-Value
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
	  return DecimalConstants<Decimal>::DecimalOneHundred;
	}

      mBaseLineCumulativeReturn = this->getCumulativeReturn (mBackTester);
      return permuteAndGetPValue (aStrategy->numTradingOpportunities(),
				  aStrategy->getPositionReturnsVector().data(),
				  aStrategy->getPositionDirectionVector().data(),
				  mNumPermutations);
    }

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

  //
  // class MonteCarloPayoffRatio
  //
  // This class implements the MCPT by calculating the payoff ratio using a large number of trades.
  // It does this be creating multiple ynthetic time series, backtesting the pattern and does
  // does this a large number of times to converge on the payoff ratio
  //

  template <class Decimal> class MonteCarloPayoffRatio : 
    public MonteCarloPermutationTest<Decimal> 
  {
  public:
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

    // Runs the monte carlo permutation test and return the simulated payoff ratio
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
      aTimeSeries2.getSyntheticTimeSeries()->syncronizeMapAndArray();

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
