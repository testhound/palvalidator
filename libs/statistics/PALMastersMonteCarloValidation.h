#ifndef __PAL_MASTER_MONTE_CARLO_VALIDATION_H
#define __PAL_MASTER_MONTE_CARLO_VALIDATION_H 1

#include <string>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <map>
#include <memory>
#include <fstream>
#include <boost/date_time.hpp>
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PalAst.h"
#include "Portfolio.h"
#include "Security.h"
#include "TimeSeries.h"
#include "MultipleTestingCorrection.h"
#include "PALMonteCarloTypes.h"
#include "StrategyDataPreparer.h"
#include "IMastersSelectionBiasAlgorithm.h"
#include "MastersRomanoWolf.h"
#include "MastersRomanoWolfImproved.h"
#include "PermutationStatisticsCollector.h"

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::shared_ptr;
  using std::make_shared;
  using std::map;

  class PALMastersMonteCarloValidationException : public std::runtime_error
  {
  public:
    PALMastersMonteCarloValidationException(const std::string msg)
      : std::runtime_error(msg) {}

    ~PALMastersMonteCarloValidationException() noexcept = default;
  };

  // -----------------------------------
  // This class implements a stepwise permutation test for selection bias in trading system development,
  // based on Timothy Masters' algorithm described in "Permutation and Randomization Tests for Trading System Development".
  // The algorithm is designed to control the Familywise Error Rate (FWE) with strong control and improved statistical power,
  // inspired by the stepdown multiple testing procedure of Romano and Wolf (2016).
  //
  // Key features of this implementation:
  // - Computes baseline statistics for all candidate trading strategies using a customizable BaselineStatPolicy.
  // - Executes a stepwise permutation test in which strategies are tested in order of decreasing baseline performance.
  // - At each step, null distributions are constructed using only the remaining (unrejected) strategies, improving power.
  // - Adjusted p-values are calculated in a way that preserves monotonicity and provides valid inference under multiple testing.
  //
  // Template Parameters:
  // - Decimal: Numeric type used throughout (e.g., double, mpfr, etc.)
  // - BaselineStatPolicy: A policy class providing a static method for computing the statistic to test (e.g., profit factor).
  //
  // Important Method: runPermutationTests()
  // ----------------------------------------
  // Performs the full stepwise permutation testing procedure:
  // 1. Calls prepareStrategyDataAndBaselines() to compute the baseline performance metric for each strategy.
  // 2. Sorts strategies in descending order of baseline statistic.
  // 3. Initializes a pool of active strategies and performs the following loop:
  //    a. For the current best strategy:
  //       - Generate a null distribution by computing the maximum statistic across permutations,
  //         but only over the remaining (active) strategies.
  //       - Calculate a p-value by comparing the real statistic to this null distribution.
  //       - Adjust this p-value to ensure non-decreasing p-values across steps (monotonicity).
  //    b. If the adjusted p-value is <= alpha:
  //       - The strategy is accepted (null hypothesis rejected).
  //       - It is removed from the active strategy pool.
  //       - Continue testing the next-best strategy.
  //    c. If the adjusted p-value > alpha:
  //       - The test stops.
  //       - All remaining strategies are assigned this p-value.
  // 4. The final result is stored in mStrategySelectionPolicy, including adjusted p-values for all strategies.
  //
  // This stepwise procedure avoids the conservative bias of traditional max-statistic permutation tests by
  // narrowing the null hypothesis distribution as strategies are confirmed, increasing the chance of detecting
  // weaker but valid trading strategies while still controlling the overall error rate.
  //
  // -----------------------------------------------------------------------------------------

template <class Decimal, class BaselineStatPolicy>
    class PALMastersMonteCarloValidation
    {
    public:
      // Use types from PALMonteCarloTypes.h
      using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>; // Convenience alias
      using StrategyContextType = StrategyContext<Decimal>;
      using StrategyDataContainerType = StrategyDataContainer<Decimal>;
      using AlgoType = IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>;

      // Alias for result iterator
      using SurvivingStrategiesIterator = typename UnadjustedPValueStrategySelection<Decimal>::ConstSurvivingStrategiesIterator;

      /*!
       * @brief Constructor for PALMastersMonteCarloValidation.
       * @tparam Decimal The numeric type used for calculations.
       * @tparam BaselineStatPolicy Policy class for computing the baseline statistic.
       * @param numPermutations The number of permutations to run. Must be greater than zero.
       * @param algo A unique pointer to the selection bias algorithm implementation. Defaults to MastersRomanoWolfImproved.
       * @throw PALMastersMonteCarloValidationException if numPermutations is zero.
       */
      PALMastersMonteCarloValidation(unsigned long numPermutations,
        std::unique_ptr<AlgoType> algo =
        std::make_unique<MastersRomanoWolf<Decimal,BaselineStatPolicy>>())
 : mNumPermutations(numPermutations),
   mStrategySelectionPolicy(),
   mAlgorithm(std::move(algo)),
   mStatisticsCollector(std::make_unique<PermutationStatisticsCollector<Decimal>>())
      {
 if (numPermutations == 0)
   throw PALMastersMonteCarloValidationException("Number of permutations cannot be zero.");
      }

      // Default copy/move constructors/assignment (as provided)
      PALMastersMonteCarloValidation(const PALMastersMonteCarloValidation&) = default;
      PALMastersMonteCarloValidation& operator=(const PALMastersMonteCarloValidation&) = default;
      PALMastersMonteCarloValidation(PALMastersMonteCarloValidation&&) noexcept = default;
      PALMastersMonteCarloValidation& operator=(PALMastersMonteCarloValidation&&) noexcept = default;

      /*!
       * @brief Default virtual destructor.
       */
      virtual ~PALMastersMonteCarloValidation() = default;

      /*!
       * @brief Gets an iterator to the beginning of the surviving strategies.
       * @tparam Decimal The numeric type used for calculations.
       * @tparam BaselineStatPolicy Policy class for computing the baseline statistic.
       * @return A const iterator to the first surviving strategy.
       */
      SurvivingStrategiesIterator beginSurvivingStrategies() const
      {
	return mStrategySelectionPolicy.beginSurvivingStrategies();
      }

      /*!
       * @brief Gets an iterator to the end of the surviving strategies.
       * @tparam Decimal The numeric type used for calculations.
       * @tparam BaselineStatPolicy Policy class for computing the baseline statistic.
       * @return A const iterator to the position past the last surviving strategy.
       */
      SurvivingStrategiesIterator endSurvivingStrategies() const
      {
	return mStrategySelectionPolicy.endSurvivingStrategies();
      }

      /*!
       * @brief Gets the number of strategies that survived the permutation testing.
       * @tparam Decimal The numeric type used for calculations.
       * @tparam BaselineStatPolicy Policy class for computing the baseline statistic.
       * @return The count of surviving strategies.
       */
      unsigned long getNumSurvivingStrategies() const
      {
	return static_cast<unsigned long>(mStrategySelectionPolicy.getNumSurvivingStrategies());
      }

      /*!
       * @brief Get access to the permutation statistics collector
       * @return Reference to the statistics collector for accessing detailed permutation metrics
       */
      const PermutationStatisticsCollector<Decimal>& getStatisticsCollector() const {
        return *mStatisticsCollector;
      }

      /*!
       * @brief Get access to the permutation statistics collector (non-const)
       * @return Reference to the statistics collector for accessing detailed permutation metrics
       */
      PermutationStatisticsCollector<Decimal>& getStatisticsCollector() {
        return *mStatisticsCollector;
      }

      /*!
       * @brief Get all tested strategies with their p-values
       * @return Vector of pairs containing strategy and its p-value
       */
      std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> getAllTestedStrategies() const {
        return mStrategySelectionPolicy.getAllTestedStrategies();
      }

      /*!
       * @brief Get the p-value for a specific strategy
       * @param strategy The strategy to get the p-value for
       * @return The p-value for the strategy
       */
      Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const {
        return mStrategySelectionPolicy.getStrategyPValue(strategy);
      }

      /*!
       * @brief Runs the stepwise permutation tests to validate trading strategies.
       *
       * This method performs the full stepwise permutation testing procedure:
       * 1. Prepares strategy data and computes baseline performance metrics for each strategy.
       * 2. Sorts strategies in descending order based on their baseline statistic.
       * 3. Utilizes the configured algorithm (e.g., MastersRomanoWolfImproved) to run the permutation tests.
       * This involves generating null distributions and calculating p-values for strategies iteratively.
       * 4. Stores the results, including adjusted p-values for all strategies, in the internal
       * strategy selection policy object.
       *
       * @tparam Decimal The numeric type used for calculations.
       * @tparam BaselineStatPolicy Policy class for computing the baseline statistic.
       * @param baseSecurity A shared pointer to the base security used for backtesting.
       * @param patterns A pointer to the PriceActionLabSystem containing the trading patterns/strategies.
       * @param dateRange The date range over which to run the tests.
       * @param pValueSignificanceLevel The significance level (alpha) for p-values. Defaults to a predefined significant p-value.
       * @throw PALMastersMonteCarloValidationException if the base security or patterns are missing,
       *or if a template backtester cannot be created.
       */
      void runPermutationTests(std::shared_ptr<Security<Decimal>> baseSecurity,
			       std::shared_ptr<PriceActionLabSystem> patterns,
			       const DateRange& dateRange,
			       const Decimal& pValueSignificanceLevel =
			       DecimalConstants<Decimal>::SignificantPValue,
			       bool verbose = false)
      {
	if (!baseSecurity)
	  throw PALMastersMonteCarloValidationException("Base security missing in runPermutationTests setup.");

	if (!patterns)
	  throw PALMastersMonteCarloValidationException("Price patterns missing in runPermutationTests setup.");

	mStrategySelectionPolicy.clearForNewTest();
	auto timeFrame = baseSecurity->getTimeSeries()->getTimeFrame();
	auto templateBackTester = BackTesterFactory<Decimal>::getBackTester(timeFrame,
									    dateRange);
	if (!templateBackTester)
	  throw PALMastersMonteCarloValidationException("Failed to create template backtester.");
	
	mStrategyData = StrategyDataPreparer<Decimal, BaselineStatPolicy>::prepare(templateBackTester,
										   baseSecurity,
										   patterns);
	    
	if (mStrategyData.empty()) {
	  std::cout << "No strategies found for permutation testing." << std::endl;
	  return;
	}

	if (verbose)
	  {
	    std::cout << "PALMastersMonteCarloValidation starting validation for " << std::endl;
	    std::cout << "Out-of-Sample Date Range: " << dateRange.getFirstDateTime()
		      << " to " << dateRange.getLastDateTime() << std::endl;
	  }
	
	// Sort DESCENDING by baselineStat (best first)
	std::sort(mStrategyData.begin(), mStrategyData.end(),
		  [](const StrategyContextType& a, const StrategyContextType& b) {
		    return a.baselineStat > b.baselineStat;
		  });
	
	// Write sorted strategy data to file if verbose is enabled
	if (verbose)
	  {
	    std::string filename = baseSecurity->getSymbol() + "_StrategyData.txt";
	    std::ofstream outFile(filename);
	    
	    if (outFile.is_open())
	      {
		outFile << "Strategy Name\tBaseline Statistic\tNumber of Trades\n";
		for (const auto& strategyContext : mStrategyData)
		  {
		    outFile << strategyContext.strategy->getStrategyName() << "\t"
			    << strategyContext.baselineStat << "\t"
			    << strategyContext.count << "\n";
		  }
		outFile.close();
		std::cout << "Strategy data written to file: " << filename << std::endl;
	      }
	    else
	      {
		std::cerr << "Warning: Could not open file " << filename << " for writing strategy data." << std::endl;
	      }
	  }
	
	auto portfolio = std::make_shared<Portfolio<Decimal>>("PermutationPortfolio");
	portfolio->addSecurity(baseSecurity->clone(baseSecurity->getTimeSeries()));

	// Determine Significance Level (Alpha)
	Decimal sigLevel = pValueSignificanceLevel;
	std::map<unsigned long long, Decimal> pvalMap;

	// Attach the statistics collector to the algorithm for observer pattern
	// The algorithm will chain this observer to its internal policy instances
	auto* subject = dynamic_cast<PermutationTestSubject<Decimal>*>(mAlgorithm.get());
	if (subject) {
	    subject->attach(mStatisticsCollector.get());
	}

	pvalMap = mAlgorithm->run(mStrategyData,
				  mNumPermutations,
				  templateBackTester,
				  portfolio,
				  sigLevel);

	if (verbose)
	  std::cout << "PALMastersMonteCarloValidation: finishing validation, populating strategy selection policy" << std::endl;
	
	for (const auto& entry : mStrategyData)
	  {
	    Decimal finalPval = DecimalConstants<Decimal>::DecimalOne; // Default p-value
	    auto strategyHash = entry.strategy->getPatternHash();
	    auto it = pvalMap.find(strategyHash);

	    if (it != pvalMap.end())
	      {
		finalPval = it->second;
	      }
	    else
	      {
		std::cerr << "Warning: Final p-value not found for strategy "
			  << entry.strategy->getStrategyName() << ", assigning 1.0" << std::endl;
	      }

	    //if (verbose)
	    //std::cout << "PALMastersMonteCarloValidation adding strategy" << std::endl;
	    
	    mStrategySelectionPolicy.addStrategy(finalPval, entry.strategy);
	  }
	
	mStrategySelectionPolicy.correctForMultipleTests(pValueSignificanceLevel);

	if (verbose)
	  std::cout << "PALMastersMonteCarloValidation finished validation" << std::endl;
      }

    private:
      unsigned long mNumPermutations;
      StrategyDataContainerType mStrategyData;
      UnadjustedPValueStrategySelection<Decimal> mStrategySelectionPolicy;
      std::unique_ptr<IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>>  mAlgorithm;
      std::unique_ptr<PermutationStatisticsCollector<Decimal>> mStatisticsCollector;
    };
} // namespace mkc_timeseries

#endif // __PAL_MASTER_MONTE_CARLO_VALIDATION_H
