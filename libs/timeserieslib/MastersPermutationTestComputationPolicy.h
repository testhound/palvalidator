#ifndef __MASTER_PERMUTATION_TEST_COMPUTATION_POLICY_H
#define __MASTER_PERMUTATION_TEST_COMPUTATION_POLICY_H 1

#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <numeric>
#include <atomic>
#include <thread>
#include <future>

// --- Assumed necessary includes from your project ---
#include "BackTester.h"
#include "PalStrategy.h"
#include "Security.h"
#include "Portfolio.h"
#include "SyntheticTimeSeries.h"
#include "number.h"
#include "DecimalConstants.h"
#include "SyntheticSecurityHelpers.h"

namespace mkc_timeseries
{
  /**
   * @class MasterPermutationPolicy
   * @brief Implements Timothy Masters' stepwise permutation test for multiple hypothesis correction.
   *
   * This class provides a parallelized algorithm to control the Family-Wise Error Rate (FWER) in
   * multiple hypothesis testing scenarios, specifically for financial strategy backtesting.
   *
   * ## Purpose
   * This policy allows testing individual hypotheses (strategies) stepwise, comparing each against
   * a progressively shrinking active set of candidate strategies. It improves upon traditional
   * selection-bias algorithms by:
   * - Offering strong control of the Family-Wise Error Rate (FWER), meaning it accounts for partially true null hypotheses.
   * - Increasing statistical power by narrowing the null distribution at each step.
   * - Avoiding inflated p-values caused by conservative multiple comparisons.
   *
   * ## Algorithm Summary
   * 1. Start with a list of active strategies.
   * 2. For each strategy (ordered by observed performance), compare its baseline statistic to
   *    the empirical permutation distribution of the max statistic **across currently active strategies**.
   * 3. Reduce the active strategy set step by step as null hypotheses are rejected.
   * 4. Maintain strong FWER control and ensure monotonic p-value adjustments.
   *
   * ## Parallelization Details
   * This implementation is multi-threaded for efficiency:
   * - The permutation loop is split across available CPU cores using std::async.
   * - Each thread processes a range of permutation iterations.
   * - Shared state (`count_k`) is safely updated using std::atomic.
   * - Exceptions within threads are propagated back to the main thread.
   *
   * @tparam Decimal Numeric type used for calculations (e.g., double, long double).
   * @tparam BaselineStatPolicy Policy class that provides the method to compute test statistics.
   */
  template <class Decimal, class BaselineStatPolicy>
  class MasterPermutationPolicy
  {
  public:
    using StrategyDataType = std::tuple<std::shared_ptr<PalStrategy<Decimal>>, Decimal, unsigned int>;
    using StrategyDataContainer = std::vector<StrategyDataType>;

    MasterPermutationPolicy() = default;
    ~MasterPermutationPolicy() = default;

    /**
     * @brief Compute the permutation count for a specific strategy step.
     *
     * For a given strategy, this method compares its baseline performance statistic against
     * the maximum statistic from permuted datasets across the active set of strategies.
     *
     * @param numPermutations Number of permutations to perform (should be > 0).
     * @param baselineStat_k Baseline performance statistic of the strategy being tested.
     * @param active_strategies Vector of currently active strategies.
     * @param templateBackTester A template backtester object to be cloned in each test.
     * @param theSecurity Security object used to generate synthetic data.
     * @param basePortfolioPtr Portfolio object template for synthetic portfolio generation.
     * @return Number of permutations (including original data) where the max permuted statistic exceeds baselineStat_k.
     */

    static unsigned int computePermutationCountForStep(
						       uint32_t numPermutations,
						       const Decimal baselineStat_k,
						       const std::vector<std::shared_ptr<PalStrategy<Decimal>>>& active_strategies,
						       std::shared_ptr<BackTester<Decimal>> templateBackTester,
						       std::shared_ptr<Security<Decimal>> theSecurity,
						       std::shared_ptr<Portfolio<Decimal>> basePortfolioPtr)
    {
      if (active_strategies.empty())
	{
	  std::cerr << "Warning: computePermutationCountForStep called with empty active_strategies set." << std::endl;
	  return 1;
	}

      if (numPermutations == 0)
	{
	  throw std::runtime_error("computePermutationCountForStep - Number of permutations cannot be zero.");
	}

      if (!templateBackTester || !theSecurity || !basePortfolioPtr)
	{
	  throw std::runtime_error("computePermutationCountForStep - Null pointer provided for backtester, security, or portfolio.");
	}

      std::atomic<unsigned int> count_k(1); // Initialize count to include the original (non-permuted) data.
      const unsigned int hardware_threads = std::thread::hardware_concurrency();
      const unsigned int num_threads = (hardware_threads == 0) ? 2 : hardware_threads;
      const unsigned int tasks_per_thread = numPermutations / num_threads;
      const unsigned int remaining_tasks = numPermutations % num_threads;

      std::vector<std::future<void>> futures;

      for (unsigned int thread_idx = 0; thread_idx < num_threads; ++thread_idx)
	{
	  const unsigned int start_idx = thread_idx * tasks_per_thread;
	  const unsigned int end_idx = (thread_idx == num_threads - 1)
            ? (start_idx + tasks_per_thread + remaining_tasks)
            : (start_idx + tasks_per_thread);

	  futures.emplace_back(std::async(std::launch::async, [&, start_idx, end_idx]()
	  {
            for (unsigned int p = start_idx; p < end_idx; ++p)
	      {
                std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio;
                try
		  {
                    syntheticPortfolio = createSyntheticPortfolio<Decimal>(theSecurity, basePortfolioPtr);
		  }
                catch (const std::exception& e)
		  {
                    std::cerr << "Error creating synthetic portfolio for permutation " 
                              << p << ": " << e.what() << std::endl;
                    throw;
		  }

                Decimal max_stat_perm_this_step = std::numeric_limits<Decimal>::lowest();

                for (const auto& active_strategy_ptr : active_strategies)
		  {
                    if (!active_strategy_ptr)
		      {
                        std::cerr << "Warning: Null strategy pointer encountered in active set during permutation " 
                                  << p << std::endl;
                        continue;
		      }

                    uint32_t stratTrades = 0;
                    Decimal stat_perm_active = std::numeric_limits<Decimal>::lowest();

                    // Repeat the cloned backtest until the minimum number of trades is reached.
                    while (stratTrades < BaselineStatPolicy::getMinStrategyTrades())
		      {
                        std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
                        try
			  {
                            clonedStrategy = active_strategy_ptr->clone(syntheticPortfolio);
			  }
                        catch (const std::exception& e)
			  {
                            std::cerr << "Warning: Failed to clone strategy " 
                                      << active_strategy_ptr->getStrategyName() 
                                      << " permutation " << p << ": " << e.what() << std::endl;
                            break;
			  }

                        std::shared_ptr<BackTester<Decimal>> clonedBackTester;
                        try
			  {
                            clonedBackTester = templateBackTester->clone();
			  }
                        catch (const std::exception& e)
			  {
                            std::cerr << "Warning: Failed to clone backtester for strategy " 
                                      << active_strategy_ptr->getStrategyName() 
                                      << " permutation " << p << ": " << e.what() << std::endl;
                            break;
			  }

                        clonedBackTester->addStrategy(clonedStrategy);

                        try
			  {
                            clonedBackTester->backtest();
                            stratTrades = BackTesterFactory<Decimal>::getNumClosedTrades<Decimal>(clonedBackTester);

                            if (stratTrades >= BaselineStatPolicy::getMinStrategyTrades())
			      {
                                stat_perm_active = BaselineStatPolicy::getPermutationTestStatistic(clonedBackTester);
			      }
			  }
                        catch (const std::exception& e)
			  {
                            std::cerr << "Warning: Backtest failed for strategy " 
                                      << active_strategy_ptr->getStrategyName() 
                                      << " permutation " << p << ": " << e.what() << std::endl;
			  }
		      } // End while (stratTrades < BaselineStatPolicy::getMinStrategyTrades())

                    max_stat_perm_this_step = std::max(max_stat_perm_this_step, stat_perm_active);
		  } // End for each active strategy

                if (max_stat_perm_this_step >= baselineStat_k)
		  {
                    count_k.fetch_add(1, std::memory_order_relaxed);
		  }
	      } // End for each permutation iteration (p)
	  }));
	}

      for (auto& future : futures)
	{
	  future.get();
	}

      return count_k.load();
    }
  }; // End class MasterPermutationPolicy

} // namespace mkc_timeseries

#endif // __MASTER_PERMUTATION_TEST_COMPUTATION_POLICY_H
