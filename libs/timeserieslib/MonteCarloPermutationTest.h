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
#include "decimal.h"
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
  using dec::decimal;
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

  template <int Prec> class MonteCarloPermutationTest
  {
  public:
    MonteCarloPermutationTest()
    {}

    virtual ~MonteCarloPermutationTest()
    {}

    virtual dec::decimal<Prec> runPermutationTest() = 0;

  protected:
    uint32_t
    getNumClosedTrades(std::shared_ptr<BackTester<Prec>> aBackTester)
    {
      std::shared_ptr<BacktesterStrategy<Prec>> backTesterStrategy = 
	(*(aBackTester->beginStrategies()));

      return backTesterStrategy->getStrategyBroker().getClosedTrades();
    }

    dec::decimal<Prec> getCumulativeReturn(std::shared_ptr<BackTester<Prec>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
	{
	  std::shared_ptr<BacktesterStrategy<Prec>> backTesterStrategy = 
	    (*(aBackTester->beginStrategies()));

	  return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();
	}
      else
	throw MonteCarloPermutationException("MonteCarloPermuteMarketChanges::getCumulativeReturn - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }

    void validateStrategy( std::shared_ptr<BacktesterStrategy<Prec>> aStrategy) const
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
  template <int Prec,
	    template <int Prec2> class BackTestResultPolicy = CumulativeReturnPolicy> class MonteCarloPermuteMarketChanges : public MonteCarloPermutationTest<Prec> 

  {
  public:
    MonteCarloPermuteMarketChanges (std::shared_ptr<BackTester<Prec>> backtester,
				    uint32_t numPermutations)
      : MonteCarloPermutationTest<Prec>(),
	mBackTester (backtester),
	mNumPermutations(numPermutations),
	mBaseLineCumulativeReturn(DecimalConstants<Prec>::DecimalZero)
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
    dec::decimal<Prec> runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Prec>> aStrategy = 
	(*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Prec>> theSecurity = aStrategy->beginPortfolio()->second;
      std::shared_ptr<OHLCTimeSeries<Prec>> theTimeSeries = theSecurity->getTimeSeries();

      mBackTester->backtest();

      // If we have too few trades don't trust results

      if (this->getNumClosedTrades (mBackTester) < 4)
	{
	  //std::cout << " runPermutationTest: number of trades = " << 
	  //getNumClosedTrades (mBackTester) << std::endl;
	  return DecimalConstants<Prec>::DecimalOneHundred;
	}

      mBaseLineCumulativeReturn = BackTestResultPolicy<Prec>::getPermutationTestStatistic(mBackTester);
      //std::cout << "Baaeline test stat. for original  strategy equals: " <<  mBaseLineCumulativeReturn << std::endl;

      uint32_t count = 0;
      uint32_t i;
      for (i = 0; i < mNumPermutations; i++)
	{
	  uint32_t stratTrades = 0;

	  std::shared_ptr<BacktesterStrategy<Prec>> clonedStrategy;
	  std::shared_ptr<BackTester<Prec>> clonedBackTester;
	  while (stratTrades < 2)
	    {
	      clonedStrategy = aStrategy->clone (createSyntheticPortfolio (theSecurity,
									   aStrategy->getPortfolio()));

	      clonedBackTester = mBackTester->clone();
	      clonedBackTester->addStrategy(clonedStrategy);
	      clonedBackTester->backtest();

	      stratTrades = this->getNumClosedTrades (clonedBackTester);

	    }

	  dec::decimal<Prec> cumulativeReturn(BackTestResultPolicy<Prec>::getPermutationTestStatistic(clonedBackTester));
	  //std::cout << "Test stat. for strategy " << (i + 1) << " equals: " << cumulativeReturn << ", num trades = " << stratTrades << std::endl;

	  if (cumulativeReturn >= mBaseLineCumulativeReturn)
	    count++;
	}

      return decimal<Prec> ((count + 1.0) / (mNumPermutations + 1.0));
    }

  private:
    std::shared_ptr<Portfolio<Prec>> createSyntheticPortfolio (std::shared_ptr<Security<Prec>> realSecurity,
							       std::shared_ptr<Portfolio<Prec>> realPortfolio)
    {
      std::shared_ptr<Portfolio<Prec>> syntheticPortfolio = realPortfolio->clone();
      syntheticPortfolio->addSecurity (createSyntheticSecurity (realSecurity));
      return syntheticPortfolio;
    }

    shared_ptr<Security<Prec>> createSyntheticSecurity(shared_ptr<Security<Prec>> aSecurity)
    {
      auto aTimeSeries = aSecurity->getTimeSeries();
      SyntheticTimeSeries<Prec> aTimeSeries2(*aTimeSeries);
      aTimeSeries2.createSyntheticSeries();

      return aSecurity->clone (aTimeSeries2.getSyntheticTimeSeries());
    }

   

  private:
    std::shared_ptr<BackTester<Prec>> mBackTester;
    uint32_t mNumPermutations;
    dec::decimal<Prec> mBaseLineCumulativeReturn;
  };


  //
  // class OriginalMonteCarloPermutationTest
  //
  // This class implements the MCPT from the paper Monte-Carlo Evaluation of Trading Systems
  //

  template <int Prec> class OriginalMCPT : 
    public MonteCarloPermutationTest<Prec> 
  {
  public:
    OriginalMCPT (std::shared_ptr<BackTester<Prec>> backtester,
		  uint32_t numPermutations)
      : MonteCarloPermutationTest<Prec>(),
	mBackTester (backtester),
	mNumPermutations(numPermutations),
	mBaseLineCumulativeReturn(DecimalConstants<Prec>::DecimalZero)
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
    dec::decimal<Prec> runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Prec>> aStrategy = 
	(*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Prec>> theSecurity = aStrategy->beginPortfolio()->second;
      std::shared_ptr<OHLCTimeSeries<Prec>> theTimeSeries = theSecurity->getTimeSeries();

      mBackTester->backtest();

      // If we have too few trades don't trust results

      if (this->getNumClosedTrades (mBackTester) < 4)
	{
	  //std::cout << " runPermutationTest: number of trades = " << 
	  //getNumClosedTrades (mBackTester) << std::endl;
	  return DecimalConstants<Prec>::DecimalOneHundred;
	}

      mBaseLineCumulativeReturn = this->getCumulativeReturn (mBackTester);
      return permuteAndGetPValue (aStrategy->numTradingOpportunities(),
				  aStrategy->getPositionReturnsVector().data(),
				  aStrategy->getPositionDirectionVector().data(),
				  mNumPermutations);
    }

    decimal<Prec> permuteAndGetPValue(int numTradingOpportunities, 
				      decimal<Prec> *rawReturnsVector, 
				      int *positionVector,
				      int nreps)
    {
      int i, irep, k1, k2, count, temp;
      decimal<Prec> cand_return, trial_return;

      int *workSeries(new int[numTradingOpportunities]);

      try
	{
	  memcpy(workSeries, positionVector, numTradingOpportunities * sizeof(int));

	  /*
	    Compute the return of the candidate model
	    If requested, do the same for the nonparametric version
	  */

	  cand_return = DecimalConstants<Prec>::DecimalZero;

	  for (i = 0; i < numTradingOpportunities; i++)
	    cand_return += decimal_cast<Prec> (positionVector[i]) * rawReturnsVector[i];

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

	      trial_return = 0.0;
	      for (i = 0; i < numTradingOpportunities; i++) // Compute return for this randomly shuffled system
		trial_return += decimal_cast<Prec> (workSeries[i]) * rawReturnsVector[i];

	      if (trial_return >= cand_return) // If this random system beat candidate
		++count; // Count it
	    }

	  delete[] workSeries;
	  return decimal<Prec> ((count + 1.0) / (mNumPermutations + 1.0));
	}
      catch (...)
	{
	  delete[] workSeries;
	  throw;   // Rethrow original exception
	}

      }

  private:
    std::shared_ptr<BackTester<Prec>> mBackTester;
    uint32_t mNumPermutations;
    dec::decimal<Prec> mBaseLineCumulativeReturn;
  };



  ////////////////

  //
  // class MonteCarloPayoffRatio
  //
  // This class implements the MCPT by calculating the payoff ratio using a large number of trades.
  // It does this be creating multiple ynthetic time series, backtesting the pattern and does
  // does this a large number of times to converge on the payoff ratio
  //

  template <int Prec> class MonteCarloPayoffRatio : 
    public MonteCarloPermutationTest<Prec> 
  {
  public:
    MonteCarloPayoffRatio (std::shared_ptr<BackTester<Prec>> backtester,
			   uint32_t numPermutations)
      : MonteCarloPermutationTest<Prec>(),
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
    dec::decimal<Prec> runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Prec>> aStrategy = 
	(*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Prec>> theSecurity = aStrategy->beginPortfolio()->second;

      uint32_t i;
      for (i = 0; i < mNumPermutations; i++)
	{
	  std::shared_ptr<BacktesterStrategy<Prec>> clonedStrategy = 
	    aStrategy->clone (createSyntheticPortfolio (theSecurity, aStrategy->getPortfolio()));

	  std::shared_ptr<BackTester<Prec>> clonedBackTester = mBackTester->clone();
	  clonedBackTester->addStrategy(clonedStrategy);
	  clonedBackTester->backtest();

	  ClosedPositionHistory<Prec> history = clonedStrategy->getStrategyBroker().getClosedPositionHistory();
	  typename ClosedPositionHistory<Prec>::ConstTradeReturnIterator winnersIterator = 
	    history.beginWinnersReturns();

	  for (; winnersIterator != history.endWinnersReturns(); winnersIterator++)
	    {
	      //std::cout << "Winners trade return " << *winnersIterator << std::endl;
	      mWinnersStats (*winnersIterator);
	    }

	  typename ClosedPositionHistory<Prec>::ConstTradeReturnIterator losersIterator = 
	    history.beginLosersReturns();

	  for (; losersIterator != history.endLosersReturns(); losersIterator++)
	    {
	      mLosersStats (*losersIterator);
	      //std::cout << "Losers trade return " << *losersIterator << std::endl;
	    }

	}

      if ((count (mWinnersStats) > 0) && (count (mLosersStats) > 0))
	{
	  return decimal<Prec> (median (mWinnersStats) / median (mLosersStats));
	}
      else
	{
	  return DecimalConstants<Prec>::DecimalZero;
	}
    }
*/

    dec::decimal<Prec> runPermutationTest()
    {
      std::shared_ptr<BacktesterStrategy<Prec>> aStrategy =
    (*(mBackTester->beginStrategies()));

      this->validateStrategy (aStrategy);

      shared_ptr<Security<Prec>> theSecurity = aStrategy->beginPortfolio()->second;

      runner& Runner=getRunner();
      std::vector<std::future<void>> errorsVector;

      uint32_t i;
      for (i = 0; i < mNumPermutations; i++)
      {
          errorsVector.emplace_back(Runner.post([this,aStrategy,theSecurity](){
            std::shared_ptr<BacktesterStrategy<Prec>> clonedStrategy =
                  aStrategy->clone (createSyntheticPortfolio (theSecurity, aStrategy->getPortfolio()));

            std::shared_ptr<BackTester<Prec>> clonedBackTester = mBackTester->clone();
            clonedBackTester->addStrategy(clonedStrategy);

            clonedBackTester->backtest();

            ClosedPositionHistory<Prec> history = clonedStrategy->getStrategyBroker().getClosedPositionHistory();

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
      return decimal<Prec> (median (mWinnersStats) / median (mLosersStats));
    }
      else
    {
      return DecimalConstants<Prec>::DecimalZero;
    }
    }

  private:
    std::shared_ptr<Portfolio<Prec>> createSyntheticPortfolio (std::shared_ptr<Security<Prec>> realSecurity,
							       std::shared_ptr<Portfolio<Prec>> realPortfolio)
    {
      std::shared_ptr<Portfolio<Prec>> syntheticPortfolio = realPortfolio->clone();
      syntheticPortfolio->addSecurity (createSyntheticSecurity (realSecurity));
      return syntheticPortfolio;
    }

    shared_ptr<Security<Prec>> createSyntheticSecurity(shared_ptr<Security<Prec>> aSecurity)
    {
      auto aTimeSeries = aSecurity->getTimeSeries();
      SyntheticTimeSeries<Prec> aTimeSeries2(*aTimeSeries);
      aTimeSeries2.createSyntheticSeries();
      aTimeSeries2.getSyntheticTimeSeries()->syncronizeMapAndArray();

      return aSecurity->clone (aTimeSeries2.getSyntheticTimeSeries());
    }

   

  private:
    std::shared_ptr<BackTester<Prec>> mBackTester;
    uint32_t mNumPermutations;
    boost::mutex                                          accumMutex;
    accumulator_set<double, stats<median_tag, count_tag>> mWinnersStats;
    accumulator_set<double, stats<median_tag, count_tag>> mLosersStats;
  };

}

#endif