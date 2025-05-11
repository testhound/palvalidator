/**************************************************************************************
 *  StrategyDataPreparer.h
 *
 *  Purpose
 *  -------
 *  Produce the `StrategyDataContainer<Decimal>` used by the permutation algorithms:
 *    • Builds concrete `PalStrategy` objects (long/short) for each pattern.
 *    • Runs one baseline back‑test per strategy and records the statistic defined
 *      by `BaselineStatPolicy`.
 *
 *  Design notes
 *  ------------
 *  • Stateless utility – everything is done in a single static `prepare()` call.
  *************************************************************************************/

#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <future>
#include <stdexcept>
#include <iostream>
#include <string>
#include "PALMonteCarloTypes.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "Portfolio.h"
#include "TimeSeries.h"
#include "BackTester.h"
#include "ParallelExecutors.h"

namespace mkc_timeseries
{
  /**
   * @class StrategyDataPreparer
   * @brief Builds strategies and computes their baseline statistics in parallel.
   *
   * @tparam Decimal Numeric type used in calculations (e.g., double).
   * @tparam BaselineStatPolicy Policy to compute permutation test statistic and min trades.
   * @tparam Executor Concurrency executor (defaults to BoostRunnerExecutor).
   */
  template
  <
    class Decimal,
    class BaselineStatPolicy,
    class Executor = concurrency::BoostRunnerExecutor
    >
  class StrategyDataPreparer
  {
  public:
    using StrategyPtr              = std::shared_ptr<PalStrategy<Decimal>>;
    using StrategyContextType      = StrategyContext<Decimal>;
    using StrategyDataContainerType= StrategyDataContainer<Decimal>;

    /**
     * @brief Builds strategies for each pattern and computes baseline statistics in parallel.
     *
     * @param templateBacktester BackTester pre-configured with date ranges.
     * @param baseSecurity       Security to trade (full series).
     * @param patterns           PriceActionLabSystem containing patterns.
     * @return Container of StrategyContext, in insertion order.
     * @throws std::runtime_error on null inputs.
     */
    static StrategyDataContainerType
    prepare
    (
     const std::shared_ptr<BackTester<Decimal>>& templateBacktester,
     const std::shared_ptr<Security<Decimal>>&    baseSecurity,
     const PriceActionLabSystem*                  patterns
     )
    {
      // Validate inputs
      if (!templateBacktester)
        {
	  throw std::runtime_error("StrategyDataPreparer::prepare - null backtester");
        }
      if (!baseSecurity)
        {
	  throw std::runtime_error("StrategyDataPreparer::prepare - null security");
        }
      if (!patterns)
        {
	  throw std::runtime_error("StrategyDataPreparer::prepare - null patterns");
        }

      StrategyDataContainerType result;

      // Create a portfolio for all strategies
      auto portfolio = std::make_shared<Portfolio<Decimal>>(baseSecurity->getName() + " Portfolio");
      portfolio->addSecurity(baseSecurity);

      // Executor for parallel tasks and synchronization primitives
      Executor executor{};
      std::mutex dataMutex;
      std::vector<std::future<void>> futures;

      // Launch one task per pattern
      unsigned long idx = 1;
      for (auto it = patterns->allPatternsBegin();
	   it != patterns->allPatternsEnd();
	   ++it, ++idx)
        {
	  // Build strategy instance
	  const auto pattern  = *it;
	  const std::string name =
	    (pattern->isLongPattern() ? "PAL Long " : "PAL Short ")
	    + std::to_string(idx);
	  StrategyPtr strategy = createStrategyFromPattern(pattern, name, portfolio);

	  // Task: run baseline backtest and record statistic
	  auto task = [strategy,
		       templateBacktester,
		       &result,
		       &dataMutex]()
	  {
	    // Execute backtest; allow exceptions to propagate and be handled by executor
	    Decimal stat = runSingleBacktest(strategy, templateBacktester);

	    // Append result under lock
	    std::lock_guard<std::mutex> lock(dataMutex);
	    result.push_back(StrategyContextType{ strategy, stat, 1 });
	  };

	  // Submit task to executor
	  futures.emplace_back(executor.submit(std::move(task)));
        }

      // Wait for all tasks to complete
      executor.waitAll(futures);

      return result;
    }

  private:
    /**
     * @brief Create a concrete strategy from a pattern.
     */
    static StrategyPtr
    createStrategyFromPattern
    (
     const PALPatternPtr& pattern,
     const std::string&   strategyName,
     std::shared_ptr<Portfolio<Decimal>> portfolio
     )
    {
      if (pattern->isLongPattern())
        {
	  return std::static_pointer_cast<PalStrategy<Decimal>>(
								std::make_shared<PalLongStrategy<Decimal>>(strategyName, pattern, portfolio));
        }
      else
        {
	  return std::static_pointer_cast<PalStrategy<Decimal>>(
								std::make_shared<PalShortStrategy<Decimal>>(strategyName, pattern, portfolio));
        }
    }

    /**
     * @brief Clone the backtester, add the strategy, and execute backtest.
     */
    static Decimal
    runSingleBacktest
    (
     StrategyPtr                                  strategy,
     const std::shared_ptr<BackTester<Decimal>>&  templateBacktester
     )
    {
      auto btClone = templateBacktester->clone();
      btClone->addStrategy(strategy);
      btClone->backtest();
      return BaselineStatPolicy::getPermutationTestStatistic(btClone);
    }
  };

} // namespace mkc_timeseries
