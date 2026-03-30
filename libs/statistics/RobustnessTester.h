// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

/**
 * @file RobustnessTester.h
 * @brief Monte Carlo robustness testing framework for PriceActionLab patterns.
 *
 * Provides PalRobustnessTester (abstract base), PalStandardRobustnessTester
 * (standard PAL thresholds), and StatisticallySignificantRobustnessTester
 * (stricter thresholds) for evaluating trading strategy robustness via
 * permutation-based Monte Carlo testing.
 */

#ifndef __ROBUSTNESS_TESTER_H
#define __ROBUSTNESS_TESTER_H 1

#include <exception>
#include <string>
#include <list>
#include <unordered_map>
#include <boost/date_time.hpp>
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PalAst.h"
#include "RobustnessTest.h"


#include "runner.hpp"

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::make_shared;
  using std::unordered_map;
  typedef unsigned long long HashKey;

  /// @brief Exception thrown when duplicate strategy hash keys are detected in robustness results.
  class RobustnessTesterException : public std::runtime_error
  {
  public:
    RobustnessTesterException(const std::string msg)
      : std::runtime_error(msg)
    {}

    ~RobustnessTesterException()
    {}

  };

  /**
   * @brief Abstract base for Monte Carlo robustness testing of PriceActionLab strategies.
   *
   * Evaluates each strategy via RobustnessTestMonteCarlo and classifies it as
   * surviving or rejected based on the supplied robustness criteria.
   *
   * @tparam Decimal Numeric type for price and statistical calculations.
   */
  template <class Decimal> class PalRobustnessTester
  {
  public:
    /// Const iterator over surviving strategy pointers.
    typedef typename std::list<shared_ptr<PalStrategy<Decimal>>>::const_iterator SurvivingStrategiesIterator;
    /// Const iterator over rejected strategy pointers.
    typedef typename std::list<shared_ptr<PalStrategy<Decimal>>>::const_iterator RejectedStrategiesIterator;
    /// Const iterator over robustness result entries keyed by pattern hash code.
    typedef typename unordered_map<HashKey, shared_ptr<RobustnessCalculator<Decimal>>>::const_iterator RobustnessResultsIterator;

  public:
    /**
     * @brief Construct a robustness tester with the given backtester, permutation attributes, and criteria.
     *
     * @param aBackTester            Prototype backtester used to evaluate each strategy.
     * @param permutationAttributes  Configuration controlling permutation count and structure.
     * @param robustnessCriteria     Thresholds that a strategy must meet to be classified as robust.
     */
    PalRobustnessTester(shared_ptr<BackTester<Decimal>> aBackTester,
			shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
			const PatternRobustnessCriteria<Decimal>& robustnessCriteria)
      : mBacktesterPrototype (aBackTester),
	mPermutationAttributes(permutationAttributes),
	mRobustnessCriteria(robustnessCriteria),
	mAstFactory(make_shared<AstFactory>()),
	mStrategiesToBeTested(),
	mSurvivingStrategies(),
	mRejectedStrategies(),
	mFailedRobustnessResults(),
	mPassedRobustnessResults()
    {}

    /// @brief Copy constructor; deep-copies all strategy lists and result maps.
    PalRobustnessTester (const PalRobustnessTester& rhs)
    : mBacktesterPrototype (rhs.mBacktesterPrototype),
      mPermutationAttributes(rhs.mPermutationAttributes),
      mRobustnessCriteria(rhs.mRobustnessCriteria),
      mAstFactory(rhs.mAstFactory),
      mStrategiesToBeTested(rhs.mStrategiesToBeTested),
      mSurvivingStrategies(rhs.mSurvivingStrategies),
      mRejectedStrategies(rhs.mRejectedStrategies),
      mFailedRobustnessResults(rhs.mFailedRobustnessResults),
      mPassedRobustnessResults(rhs.mPassedRobustnessResults)
    {}

    /// @brief Copy-assignment operator with self-assignment guard.
    PalRobustnessTester&
    operator=(const PalRobustnessTester& rhs)
    {
      if (this == &rhs)
	return *this;

      mBacktesterPrototype = rhs.mBacktesterPrototype;
      mPermutationAttributes = rhs.mPermutationAttributes;
      mRobustnessCriteria = rhs.mRobustnessCriteria;
      mAstFactory = rhs.mAstFactory;
      mStrategiesToBeTested = rhs.mStrategiesToBeTested;
      mSurvivingStrategies = rhs.mSurvivingStrategies;
      mRejectedStrategies = rhs.mRejectedStrategies;
      mFailedRobustnessResults = rhs.mFailedRobustnessResults;
      mPassedRobustnessResults = rhs.mPassedRobustnessResults;

      return *this;
    }

    /// @brief Pure-virtual destructor; defined out-of-line to make the class abstract.
    virtual ~PalRobustnessTester() = 0;

    /**
     * @brief Execute Monte Carlo robustness tests on all queued strategies.
     *
     * Iterates over every strategy added via addStrategy(), runs a
     * RobustnessTestMonteCarlo for each, and classifies it as surviving
     * or rejected. Results are stored in the corresponding internal maps
     * keyed by pattern hash code.
     */
    //original code, for reference
    void runRobustnessTests()
    {
      typedef typename std::list<shared_ptr<PalStrategy<Decimal>>>::const_iterator CandiateStrategiesIterator;

      CandiateStrategiesIterator it = mStrategiesToBeTested.begin();
      shared_ptr<PalStrategy<Decimal>> aStrategy;
      shared_ptr<RobustnessCalculator<Decimal>> aRobustnessResult;
      unsigned long long aHashKey;
      bool isRobust = false;

      std::cout << "PalRobustnessTester::runRobustnessTests using dates: " << mBacktesterPrototype->getStartDate() << " - ";
      std::cout << mBacktesterPrototype->getEndDate() << std::endl << std::endl;

      for (; it != mStrategiesToBeTested.end(); it++)
	{
	  aStrategy = (*it);
	  RobustnessTestMonteCarlo<Decimal> aTest(mBacktesterPrototype, aStrategy, 
				     mPermutationAttributes, mRobustnessCriteria,
				     mAstFactory);

	  isRobust = aTest.runRobustnessTest();
	  aHashKey = aStrategy->getPalPattern()->hashCode();
	  aRobustnessResult = 
	    make_shared<RobustnessCalculator<Decimal>> (aTest.getRobustnessCalculator());

	  std::cout << "Run robustness test on ";
	  if (aStrategy->getPalPattern()->isLongPattern())
	    std::cout << "Long pattern with ";
	  else
	    std::cout << "Short pattern with ";

	  std::cout << "Index: " << aStrategy->getPalPattern()->getpatternIndex();
	  std::cout << ", Index date: " << aStrategy->getPalPattern()->getIndexDate();
	  std::cout << std::endl << std::endl;

	  if (isRobust)
	    {          
          std::cout << "runRobustnessTests: found robust pattern" << std::endl;
	      mSurvivingStrategies.push_back(aStrategy);
	      insertSurvivingRobustResult (aHashKey, aRobustnessResult);
	    }
	  else
	    {
	      mRejectedStrategies.push_back(aStrategy);
	      insertFailedRobustResult (aHashKey, aRobustnessResult);
	    }
	}
    }


    /**
     * @brief Insert a robustness result for a strategy that passed testing.
     *
     * @param aHashKey          Pattern hash code used as the map key.
     * @param aRobustnessResult Computed robustness statistics for the strategy.
     * @throws RobustnessTesterException If a result with the same hash key already exists.
     */
    void insertSurvivingRobustResult(unsigned long long aHashKey,
			    shared_ptr<RobustnessCalculator<Decimal>> aRobustnessResult)
    {
      if  (findSurvivingRobustnessResults(aHashKey) == endSurvivingRobustnessResults())
	mPassedRobustnessResults.insert(make_pair(aHashKey, aRobustnessResult));
      else
	throw RobustnessTesterException("insertSurvivingRobustResult: duplicate strategies with same hashkey found");
    }

    /**
     * @brief Insert a robustness result for a strategy that failed testing.
     *
     * @param aHashKey          Pattern hash code used as the map key.
     * @param aRobustnessResult Computed robustness statistics for the strategy.
     * @throws RobustnessTesterException If a result with the same hash key already exists.
     */
    void insertFailedRobustResult(unsigned long long aHashKey,
			    shared_ptr<RobustnessCalculator<Decimal>> aRobustnessResult)
    {
      if  (findFailedRobustnessResults(aHashKey) == endFailedRobustnessResults())
	mFailedRobustnessResults.insert(make_pair(aHashKey, aRobustnessResult));
      else
	throw RobustnessTesterException("insertFailedRobustResult: duplicate strategies with same hashkey found");
    }

    /**
     * @brief Enqueue a strategy for robustness testing.
     *
     * The strategy is cloned via cloneForBackTesting() before being stored,
     * so the caller's original instance is never mutated by the test run.
     *
     * @param aStrategy Strategy to be tested.
     */
    void addStrategy(shared_ptr<PalStrategy<Decimal>> aStrategy)
    {
      shared_ptr<PalStrategy<Decimal>> clonedStrategy = 
	std::dynamic_pointer_cast<PalStrategy<Decimal>> (aStrategy->cloneForBackTesting());

      mStrategiesToBeTested.push_back(clonedStrategy);
    }

    /// @brief Return the number of strategies that passed robustness testing.
    unsigned long getNumSurvivingStrategies() const
    {
      return mSurvivingStrategies.size();
    }

    /// @brief Return the number of strategies that failed robustness testing.
    unsigned long getNumRejectedStrategies() const
    {
      return mRejectedStrategies.size();
    }

    /// @brief Return the number of strategies queued for testing.
    unsigned long getNumStrategiesToTest() const
    {
      return mStrategiesToBeTested.size();
    }

    /// @brief Iterator to the first surviving strategy.
    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mSurvivingStrategies.begin();
    }

    /// @brief Past-the-end iterator for the surviving strategies list.
    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mSurvivingStrategies.end();
    }

    /// @brief Iterator to the first rejected strategy.
    RejectedStrategiesIterator beginRejectedStrategies() const
    {
      return mRejectedStrategies.begin();
    }

    /// @brief Past-the-end iterator for the rejected strategies list.
    RejectedStrategiesIterator endRejectedStrategies() const
    {
      return mRejectedStrategies.end();
    }

    /**
     * @brief Look up the failed-robustness result for a given strategy.
     *
     * @param aStrategy Strategy whose pattern hash code is used for the lookup.
     * @return Iterator to the result entry, or endFailedRobustnessResults() if not found.
     */
    RobustnessResultsIterator findFailedRobustnessResults(shared_ptr<PalStrategy<Decimal>> aStrategy) const
    {
      return mFailedRobustnessResults.find (aStrategy->getPalPattern()->hashCode());
    }

    /**
     * @brief Look up the failed-robustness result by pattern hash code.
     *
     * @param aHashCode Pattern hash code used as the map key.
     * @return Iterator to the result entry, or endFailedRobustnessResults() if not found.
     */
    RobustnessResultsIterator findFailedRobustnessResults(unsigned long long aHashCode) const
    {
      return mFailedRobustnessResults.find (aHashCode);
    }

    /**
     * @brief Look up the surviving-robustness result for a given strategy.
     *
     * @param aStrategy Strategy whose pattern hash code is used for the lookup.
     * @return Iterator to the result entry, or endSurvivingRobustnessResults() if not found.
     */
    RobustnessResultsIterator findSurvivingRobustnessResults(shared_ptr<PalStrategy<Decimal>> aStrategy) const
    {
      return mPassedRobustnessResults.find (aStrategy->getPalPattern()->hashCode());
    }

    /**
     * @brief Look up the surviving-robustness result by pattern hash code.
     *
     * @param aHashCode Pattern hash code used as the map key.
     * @return Iterator to the result entry, or endSurvivingRobustnessResults() if not found.
     */
    RobustnessResultsIterator findSurvivingRobustnessResults(unsigned long long aHashCode) const
    {
      return mPassedRobustnessResults.find (aHashCode);
    }

    /// @brief Past-the-end iterator for the failed robustness results map.
    RobustnessResultsIterator endFailedRobustnessResults() const
    {
      return  mFailedRobustnessResults.end();
    }

    /// @brief Past-the-end iterator for the surviving robustness results map.
    RobustnessResultsIterator endSurvivingRobustnessResults() const
    {
      return  mPassedRobustnessResults.end();
    }

  private:

    shared_ptr<BackTester<Decimal>> mBacktesterPrototype;
    shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Decimal> mRobustnessCriteria;
    shared_ptr<AstFactory> mAstFactory;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> mStrategiesToBeTested;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> mSurvivingStrategies;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> mRejectedStrategies;
    unordered_map<unsigned long long, 
		  std::shared_ptr<RobustnessCalculator<Decimal>>> mFailedRobustnessResults;
    unordered_map<unsigned long long, 
		  std::shared_ptr<RobustnessCalculator<Decimal>>> mPassedRobustnessResults;
  };



  template <class Decimal>
    inline PalRobustnessTester<Decimal>::~PalRobustnessTester()
    {}

  /**
   * @brief Robustness tester preconfigured with standard PAL permutation attributes and thresholds.
   *
   * Uses PALRobustnessPermutationAttributes and a PatternRobustnessCriteria configured
   * with a 70% win-rate floor, 2.0 profit factor, 2% minimum edge, and 0.9 robustness index.
   *
   * @tparam Decimal Numeric type for price and statistical calculations.
   */
  template <class Decimal> class PalStandardRobustnessTester : public PalRobustnessTester<Decimal>
    {
    public:
    /**
     * @brief Construct a standard robustness tester from a prototype backtester.
     *
     * @param aBackTester Prototype backtester used to evaluate each strategy.
     */
    PalStandardRobustnessTester(shared_ptr<BackTester<Decimal>> aBackTester)
      : PalRobustnessTester<Decimal>(aBackTester,
				  make_shared<PALRobustnessPermutationAttributes>(),
				  PatternRobustnessCriteria<Decimal> (createADecimal<Decimal>("70.0"), 
								   createADecimal<Decimal>("2.0"), 
								   createAPercentNumber<Decimal>("2.0"),
								   createADecimal<Decimal>("0.9")))
	{}

      /// @brief Copy constructor.
      PalStandardRobustnessTester (const PalStandardRobustnessTester& rhs)
	: PalRobustnessTester<Decimal>(rhs)
      {}

      /// @brief Copy-assignment operator with self-assignment guard.
      PalStandardRobustnessTester&
      operator=(const PalStandardRobustnessTester& rhs)
      {
	if (this == &rhs)
	  return *this;

	PalRobustnessTester<Decimal>::operator=(rhs);

	return *this;
      }

      /// @brief Destructor.
      ~PalStandardRobustnessTester()
      {}
    };

  /**
   * @brief Robustness tester using statistically significant permutation attributes.
   *
   * Uses StatSignificantAttributes for stricter permutation testing while
   * retaining the same PatternRobustnessCriteria thresholds as the standard
   * tester (70% win-rate floor, 2.0 profit factor, 2% minimum edge, 0.9
   * robustness index).
   *
   * @tparam Decimal Numeric type for price and statistical calculations.
   */
  template <class Decimal> class StatisticallySignificantRobustnessTester : public PalRobustnessTester<Decimal>
    {
    public:
    /**
     * @brief Construct a statistically significant robustness tester from a prototype backtester.
     *
     * @param aBackTester Prototype backtester used to evaluate each strategy.
     */
    StatisticallySignificantRobustnessTester(shared_ptr<BackTester<Decimal>> aBackTester)
      : PalRobustnessTester<Decimal>(aBackTester,
				  make_shared<StatSignificantAttributes>(),
				  PatternRobustnessCriteria<Decimal> (createADecimal<Decimal>("70.0"), 
								   createADecimal<Decimal>("2.0"), 
								   createAPercentNumber<Decimal>("2.0"),
								   createADecimal<Decimal>("0.9")))
	{}

      /// @brief Copy constructor.
      StatisticallySignificantRobustnessTester (const StatisticallySignificantRobustnessTester& rhs)
	: PalRobustnessTester<Decimal>(rhs)
      {}

      /// @brief Copy-assignment operator with self-assignment guard.
      StatisticallySignificantRobustnessTester&
      operator=(const StatisticallySignificantRobustnessTester& rhs)
      {
	if (this == &rhs)
	  return *this;

	PalRobustnessTester<Decimal>::operator=(rhs);

	return *this;
      }

      /// @brief Destructor.
      ~StatisticallySignificantRobustnessTester()
	{}
    };
}

#endif
