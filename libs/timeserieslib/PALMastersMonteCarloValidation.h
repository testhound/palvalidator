// Refactored and formatted in Allman C++ style
#ifndef __PAL_MASTER_MONTE_CARLO_VALIDATION_H
#define __PAL_MASTER_MONTE_CARLO_VALIDATION_H 1

#include <string>
#include <vector>
#include <list>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

#include <boost/date_time.hpp>
#include <boost/thread/mutex.hpp>

#include "number.h"
#include "DecimalConstants.h"
#include "McptConfiguration.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PriceActionLabSystem.h"
#include "Portfolio.h"
#include "Security.h"
#include "TimeSeries.h"
#include "runner.hpp"
#include "MastersPermutationTestComputationPolicy.h"
#include "MultipleTestingCorrection.h"

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::shared_ptr;
  using std::make_shared;
  using std::vector;
  using std::tuple;
  using std::map;
  using std::unordered_set;

  class PALMasterMonteCarloValidationException : public std::runtime_error
  {
  public:
    PALMasterMonteCarloValidationException(const std::string msg)
      : std::runtime_error(msg) {}

    ~PALMasterMonteCarloValidationException() noexcept = default;
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
  class PALMasterMonteCarloValidation
  {
  public:
    /**
     * @brief A container for storing the baseline evaluation results of a trading strategy.
     *
     * This struct represents a single entry in the mStrategyData container.
     * It holds the data necessary for stepwise permutation testing of a strategy.
     *
     * Fields:
     * - strategy: A shared pointer to the PalStrategy instance being evaluated.
     * - baselineStat: The performance statistic of the strategy on real (non-permuted) OOS data.
     *                 This value is used for sorting and comparison during permutation testing.
     * - count: Currently unused placeholder (always 1 in this implementation), but may support
     *          future extensions like vote-counting or ensemble aggregation.
     *
     * This struct replaces the use of std::tuple for better clarity and maintainability,
     * enabling named access to strategy data elements and improving code readability.
     */
    
    struct StrategyContext
    {
      shared_ptr<PalStrategy<Decimal>> strategy;
      Decimal baselineStat;
      unsigned int count;
    };

    using StrategyDataType = StrategyContext;
    using StrategyDataContainer = vector<StrategyDataType>;
    using SurvivingStrategiesIterator = typename UnadjustedPValueStrategySelection<Decimal>::surviving_const_iterator;

    PALMasterMonteCarloValidation(shared_ptr<McptConfiguration<Decimal>> configuration,
				  unsigned long numPermutations)
      : mMonteCarloConfiguration(configuration),
	mNumPermutations(numPermutations),
	mStrategySelectionPolicy()
    {
      if (!configuration)
	{
	  throw PALMasterMonteCarloValidationException("McptConfiguration cannot be null.");
	}
      if (numPermutations == 0)
	{
	  throw PALMasterMonteCarloValidationException("Number of permutations cannot be zero.");
	}
    }

    PALMasterMonteCarloValidation(const PALMasterMonteCarloValidation&) = default;
    PALMasterMonteCarloValidation& operator=(const PALMasterMonteCarloValidation&) = default;

    // Move constructor and assignment
    PALMasterMonteCarloValidation(PALMasterMonteCarloValidation&&) noexcept = default;
    PALMasterMonteCarloValidation& operator=(PALMasterMonteCarloValidation&&) noexcept = default;
      
    virtual ~PALMasterMonteCarloValidation() = default;

    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.beginSurvivingStrategies();
    }

    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.endSurvivingStrategies();
    }

    unsigned long getNumSurvivingStrategies() const
    {
      return (unsigned long)mStrategySelectionPolicy.getNumSurvivingStrategies();
    }

  protected:
    /**
     * @brief Constructs a PalStrategy instance (long or short) from a pattern.
     *
     * Given a PALPatternPtr and strategy name, this method determines whether the pattern
     * is long or short and returns a shared_ptr to the appropriate PalStrategy subtype.
     *
     * @param pattern     A smart pointer to the pattern to be converted into a strategy.
     * @param strategyName The name to assign to the resulting strategy.
     * @param portfolio    The portfolio to associate with the strategy for backtesting.
     *
     * @return A shared_ptr to either a PalLongStrategy or PalShortStrategy.
     *
     * Notes:
     * - Used during strategy initialization before backtesting.
     * - This method encapsulates the logic of mapping a pattern's direction into
     *   a concrete strategy object, improving readability and reusability.
     */
    shared_ptr<PalStrategy<Decimal>> createStrategyFromPattern(
							       const PALPatternPtr& pattern,
							       const std::string& strategyName,
							       shared_ptr<Portfolio<Decimal>> portfolio)
    {
      return pattern->isLongPattern()
	? make_shared<PalLongStrategy<Decimal>>(strategyName, pattern, portfolio)
	: make_shared<PalShortStrategy<Decimal>>(strategyName, pattern, portfolio);
    }

    /**
     * @brief Executes a single backtest on a strategy and returns the baseline statistic.
     *
     * This method:
     * 1. Creates a backtester instance using the specified timeframe and date range.
     * 2. Adds the provided strategy to the backtester.
     * 3. Executes the backtest.
     * 4. Computes and returns a baseline statistic using the configured BaselineStatPolicy.
     *
     * @param strategy   The strategy to be tested.
     * @param timeframe  The time frame of the market data (e.g., daily, weekly).
     * @param range      The out-of-sample date range to test against.
     *
     * @return The baseline performance statistic of the strategy, as defined by the BaselineStatPolicy.
     *
     * Notes:
     * - This method is designed to be used by prepareStrategyDataAndBaselines().
     * - The result is used for ranking strategies before stepwise permutation testing.
     */
    Decimal runSingleBacktest(
			      shared_ptr<PalStrategy<Decimal>> strategy,
			      TimeFrame::Duration timeframe,
			      const DateRange& range)
    {
      auto backTester = BackTesterFactory::getBackTester(timeframe, range.getFirstDate(), range.getLastDate());
      backTester->addStrategy(strategy);
      backTester->backtest();
      return BaselineStatPolicy::getPermutationTestStatistic(backTester);
    }

    /**
     * @brief Prepares strategy objects and computes their baseline performance statistics.
     *
     * This method performs the first major step of the stepwise permutation testing algorithm:
     * establishing the actual (non-permuted) performance of each strategy under real market data.
     *
     * Process Summary:
     * 1. Clones the base security and applies the user-defined out-of-sample (OOS) date range.
     * 2. Constructs a portfolio containing this OOS-trimmed version of the security.
     * 3. Iterates over all patterns in the PriceActionLabSystem:
     *    - For each pattern, creates either a PalLongStrategy or PalShortStrategy based on pattern direction.
     *    - Each strategy is wrapped in a task that:
     *      - Backtests the strategy on the OOS data using the appropriate backtester.
     *      - Computes a baseline performance statistic via the BaselineStatPolicy.
     *      - Stores the result as a StrategyContext entry in mStrategyData.
     * 4. Submits all tasks to a thread pool using `runner::instance().post()`.
     * 5. Waits for all tasks to complete and handles any exceptions.
     *
     * Threading:
     * - Each strategy baseline is computed in parallel using the thread pool runner.
     * - Access to the shared mStrategyData container is protected with a boost::mutex.
     *
     * Output:
     * - The mStrategyData vector will be populated with StrategyContext entries:
     *   {strategy, baseline_statistic, 1}, where the third element is a dummy count.
     * - This data serves as the input to the stepwise permutation procedure in runPermutationTests().
     *
     * Notes:
     * - If no patterns are present or the base security is null, an exception is thrown.
     * - This method should be called once before running permutation tests.
     */
    void prepareStrategyDataAndBaselines()
    {
      mStrategyData.clear();

      auto baseSecurity = mMonteCarloConfiguration->getSecurity();
      auto patternsToTest = mMonteCarloConfiguration->getPricePatterns();
      auto oosDates = mMonteCarloConfiguration->getOosDateRange();

      if (!baseSecurity)
	{
	  throw PALMasterMonteCarloValidationException("Base security not loaded in configuration.");
	}

      if (!patternsToTest)
	{
	  throw PALMasterMonteCarloValidationException("Price patterns not loaded in configuration.");
	}

      auto timeFrame = baseSecurity->getTimeSeries()->getTimeFrame();
      auto oosTimeSeries = make_shared<OHLCTimeSeries<Decimal>>(FilterTimeSeries<Decimal>(*baseSecurity->getTimeSeries(), oosDates));
      auto securityToTest = baseSecurity->clone(oosTimeSeries);
      securityToTest->getTimeSeries()->syncronizeMapAndArray();

      auto portfolio = make_shared<Portfolio<Decimal>>(securityToTest->getName() + " Portfolio");
      portfolio->addSecurity(securityToTest);

      vector<std::function<void()>> tasks;
      boost::mutex dataMutex;

      unsigned long strategyNumber = 1;
      for (auto it = patternsToTest->allPatternsBegin(); it != patternsToTest->allPatternsEnd(); ++it, ++strategyNumber)
	{
	  auto pattern = *it;
	  std::string name = (pattern->isLongPattern() ? "PAL Long Strategy " : "PAL Short Strategy ") + std::to_string(strategyNumber);
	  auto strategy = createStrategyFromPattern(pattern, name, portfolio);

	  tasks.emplace_back([this, strategy, timeFrame, oosDates, &dataMutex]() {
	    try
	      {
		Decimal stat = runSingleBacktest(strategy, timeFrame, oosDates);
		boost::mutex::scoped_lock lock(dataMutex);
		mStrategyData.push_back({ strategy, stat, 1 });
	      }
	    catch (const std::exception& e)
	      {
		std::cerr << "Baseline error for strategy " << strategy->getStrategyName() << ": " << e.what() << std::endl;
	      }
	  });
	}

      runner& Runner = runner::instance();
      vector<boost::unique_future<void>> futures;
      for (auto& task : tasks)
	{
	  futures.emplace_back(Runner.post(std::move(task)));
	}
      for (size_t i = 0; i < futures.size(); ++i)
	{
	  try
	    {
	      futures[i].wait();
	      futures[i].get();
	    }
	  catch (const std::exception& e)
	    {
	      std::cerr << "Task " << i << " failed: " << e.what() << std::endl;
	    }
	  catch (...)
	    {
	      std::cerr << "Unknown error in task " << i << std::endl;
	    }
	}
    }

    /**
     * @brief Performs a stepwise permutation test to adjust for selection bias in strategy evaluation.
     *
     * This method implements the algorithm described by Timothy Masters in
     * "Permutation and Randomization Tests for Trading System Development", which itself is inspired by
     * Romano & Wolf’s (2016) stepdown multiple testing procedure.
     *
     * The algorithm is designed to control the familywise error rate (FWE) with strong control
     * and improve the statistical power for detecting valid trading strategies.
     *
     * Step-by-step breakdown:
     *
     * 1. prepareStrategyDataAndBaselines():
     *    - Computes the actual (non-permuted) performance statistics for each strategy
     *      using the provided BaselineStatPolicy (e.g., profit factor).
     *    - Stores the results in mStrategyData.
     *
     * 2. Sort strategies:
     *    - Sorts all candidate strategies in descending order by their actual (baseline) performance.
     *    - This defines the order in which hypotheses will be tested (from strongest to weakest).
     *
     * 3. Initialize:
     *    - A template backtester is created to ensure consistent testing across permutations.
     *    - All strategies are initially considered "active", meaning they have not been rejected or accepted.
     *    - A map (pvalMap) is prepared to store adjusted p-values.
     *
     * 4. Stepwise testing loop:
     *    - For each strategy k (starting from the best):
     *
     *      a. Skip if it’s already been excluded from the active pool.
     *
     *      b. Generate the null hypothesis distribution for this step:
     *         - Perform N permutations.
     *         - For each permutation, compute the max statistic over only the current active strategies.
     *         - Count how many times this max exceeds the observed baseline statistic for strategy k.
     *
     *      c. Compute the raw p-value: count / (numPermutations + 1)
     *         - This is the empirical probability that the baseline stat could have occurred under the null.
     *
     *      d. Adjust the p-value:
     *         - Take the maximum of this p-value and the last adjusted p-value to ensure monotonicity.
     *         - This implements the stepdown nature of the Romano-Wolf method.
     *
     *      e. Decision:
     *         - If adjusted p-value ≤ α: strategy passes, remove it from the active pool.
     *         - If adjusted p-value > α: stop the loop.
     *             - Assign this p-value to all remaining untested strategies to preserve FWE control.
     *
     *      f. If the active pool becomes empty early, fill in the remaining strategies’ p-values with the last value.
     *
     * 5. Store results:
     *    - Each strategy is added to the strategy selection policy with its final adjusted p-value.
     *    - The survivors (those with adjusted p-value ≤ α) are retained for further analysis.
     *
     * This algorithm avoids the overconservatism of traditional max-statistic methods
     * by shrinking the null distribution at each step, thus increasing sensitivity while maintaining rigorous error control.
     */
    void runPermutationTests()
    {
      prepareStrategyDataAndBaselines();
      if (mStrategyData.empty())
	{
	  std::cout << "No strategies found. Aborting tests." << std::endl;
	  mStrategySelectionPolicy.clear();
	  return;
	}

      std::sort(mStrategyData.begin(), mStrategyData.end(), [](const StrategyDataType& a, const StrategyDataType& b)
      {
	return a.baselineStat > b.baselineStat;
      });

      auto baseSecurity = mMonteCarloConfiguration->getSecurity();
      auto dateRange = mMonteCarloConfiguration->getOosDateRange();
      auto templateBackTester = BackTesterFactory::getBackTester(baseSecurity->getTimeSeries()->getTimeFrame(), dateRange.getFirstDate(), dateRange.getLastDate());
      auto portfolio = make_shared<Portfolio<Decimal>>("BasePortfolioForPermutation");
      portfolio->addSecurity(baseSecurity->clone(baseSecurity->getTimeSeries()));

      unordered_set<shared_ptr<PalStrategy<Decimal>>> active;
      for (const auto& entry : mStrategyData)
	{
	  active.insert(entry.strategy);
	}

      map<shared_ptr<PalStrategy<Decimal>>, Decimal> pvalMap;
      Decimal lastAdjPval = DecimalConstants<Decimal>::Zero;
      Decimal sigLevel = DecimalConstants<Decimal>::SignificantPValue;

      for (size_t k = 0; k < mStrategyData.size(); ++k)
	{
	  auto& entry = mStrategyData[k];
	  auto strategy = entry.strategy;

	  if (active.find(strategy) == active.end())
	    {
	      pvalMap[strategy] = DecimalConstants<Decimal>::One;
	      continue;
	    }

	  // Computes how often a permuted set of strategies outperforms the current strategy’s
	  // baseline statistic.
	  //
	  //For each permutation:

	  // 1. Creates synthetic portfolios (shuffled time series).
	  //
	  // 2. Runs all currently active strategies on the permuted data.
	  //
	  // 3. Collects the maximum statistic among them.
	  //
	  // Compares each permutation’s max to the strategy’s baseline, and counts how many
	  // permutations beat it.

	  //This count is turned into a p-value.
	  unsigned int count = MasterPermutationPolicy<Decimal, BaselineStatPolicy>::computePermutationCountForStep(
														    mNumPermutations,
														    entry.baselineStat,
														    vector<shared_ptr<PalStrategy<Decimal>>>(active.begin(), active.end()),
														    templateBackTester,
														    baseSecurity,
														    portfolio);

	  Decimal pval = static_cast<Decimal>(count) / static_cast<Decimal>(mNumPermutations + 1);

	  // Ensure monotonicity of adjusted p-values: they should never decrease.
	  // This is a core requirement in stepdown procedures to maintain statistical validity.
	  // Take the maximum of the current strategy's p-value and the last adjusted p-value.
	  Decimal adjPval = std::max(pval, lastAdjPval);
	  pvalMap[strategy] = adjPval;

	  // Stepwise decision rule:
	  // If the adjusted p-value is less than or equal to the significance level (e.g., alpha = 0.05),
	  // then we reject the null hypothesis for this strategy — it is statistically significant.
		
	  if (adjPval <= sigLevel)
	    {
	      // Save this as the new reference p-value for the next step.
	      // It enforces the non-decreasing (step-up) behavior of adjusted p-values.
		  
	      lastAdjPval = adjPval;

	      // Remove this strategy from the active set,
	      // so it won't be included in the null distribution for the next step.
	      active.erase(strategy);
	    }
	  else
	    {
	      // If the strategy does not pass the alpha test, we stop testing.
	      // All remaining strategies are not evaluated individually.

	      // Assign the current (non-passing) p-value to all remaining strategies
	      // to maintain the monotonic property and avoid anti-conservative errors.
	      for (size_t j = k + 1; j < mStrategyData.size(); ++j)
		{
		  auto& rem = mStrategyData[j].strategy;

		  // Only assign to those without a p-value yet.
		  if (pvalMap.find(rem) == pvalMap.end())
		    {
		      pvalMap[rem] = adjPval;
		    }
		}
	      break;
	    }

	  // Special case: if all strategies have now been rejected and removed from the active pool,
	  // and there are still untested strategies left in the sorted list,
	  // we must still assign adjusted p-values to those for reporting consistency.
	  if (active.empty() && k < mStrategyData.size() - 1)
	    {
	      for (size_t j = k + 1; j < mStrategyData.size(); ++j)
		{
		  auto& rem = mStrategyData[j].strategy;

		  // Ensure we only assign once.
		  if (pvalMap.find(rem) == pvalMap.end())
		    {
		      pvalMap[rem] = lastAdjPval;
		    }
		}
	      break;
	    }
	}

      for (const auto& entry : mStrategyData)
	{
	  Decimal pval = DecimalConstants<Decimal>::One;
	  auto it = pvalMap.find(entry.strategy);
	  if (it != pvalMap.end())
	    {
	      pval = it->second;
	    }
	  mStrategySelectionPolicy.addStrategy(pval, entry.strategy);
	}

      mStrategySelectionPolicy.selectSurvivors(sigLevel);
    }

  protected:
    shared_ptr<McptConfiguration<Decimal>> mMonteCarloConfiguration;
    unsigned long mNumPermutations;
    StrategyDataContainer mStrategyData;
    UnadjustedPValueStrategySelection<Decimal> mStrategySelectionPolicy;
  };
} // namespace mkc_timeseries

#endif // __PAL_MASTER_MONTE_CARLO_VALIDATION_H
