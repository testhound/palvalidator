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
#include "PALMonteCarloTypes.h"

namespace mkc_timeseries
{
  /**
 * @class MastersPermutationPolicy
 * @brief Computes permutation test statistics for stepwise multiple hypothesis testing in strategy backtesting.
 *
 * This class is an integral component of the stepwise permutation testing procedure used by PALMasterMonteCarloValidation.
 * It computes an empirical distribution of permutation test statistics in order to derive adjusted p-values that control
 * the Family-Wise Error Rate (FWER) while mitigating selection bias in financial trading strategy evaluations.
 *
 * ## Objectives
 * - Generate synthetic market scenarios by creating synthetic portfolios.
 * - Ensure that each backtest simulation produces a minimum number of trades for statistic validity.
 * - Compute the permutation test statistic for each active strategy by repeatedly cloning the strategy and its backtester,
 *   running the backtest until a predefined minimum trade threshold is reached.
 * - Aggregate the maximum test statistic over all currently active strategies in each permutation.
 * - Count the number of permutations (including the original unpermuted case) where the maximum test statistic meets or
 *   exceeds the observed baseline statistic for a given strategy.
 *
 * ## Process Overview
 * 1. For each permutation iteration:
 *    - A synthetic portfolio is generated using the given security and base portfolio.
 * 2. For each active strategy:
 *    - The strategy is cloned and paired with a cloned backtester.
 *    - The backtester is executed repeatedly until it produces the minimum required number of trades.
 *    - A permutation test statistic is computed via the supplied BaselineStatPolicy.
 * 3. The maximum statistic over all active strategies is computed and compared against the baseline statistic.
 * 4. A count is maintained of how many permutations yield a maximum statistic greater than or equal to the baseline.
 *
 * ## Parallelization Details
 * - The total number of permutations is divided among available CPU cores using std::async.
 * - Each thread processes a subset of permutation iterations.
 * - A shared atomic counter is used to track the number of permutations exceeding the baseline statistic.
 * - Exceptions in any thread are propagated back to ensure that errors are not silently ignored.
 *
 * @tparam Decimal Numeric type used for calculations (e.g., double, long double).
 * @tparam BaselineStatPolicy Policy class that defines methods to:
 *         - Determine the minimum number of trades required for a valid test.
 *         - Compute the permutation test statistic for a backtest result.
 */
  template <class Decimal, class BaselineStatPolicy>
  class MastersPermutationPolicy
  {
  public:
    MastersPermutationPolicy() = default;
    ~MastersPermutationPolicy() = default;

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
	  std::cerr << "Warning: MastersPermutationPolicy::computePermutationCountForStep called with empty active_strategies set." << std::endl;
	  return 1;
	}

      if (numPermutations == 0)
	{
	  throw std::runtime_error("MastersPermutationPolicy::computePermutationCountForStep - Number of permutations cannot be zero.");
	}

      if (!templateBackTester || !theSecurity || !basePortfolioPtr)
	{
	  throw std::runtime_error("MastersPermutationPolicy::computePermutationCountForStep - Null pointer provided for backtester, security, or portfolio.");
	}

      std::atomic<unsigned int> count_k(1);
      const unsigned int hardware_threads = std::thread::hardware_concurrency();
      const unsigned int num_threads = (hardware_threads == 0) ? 2 : hardware_threads;
      // Ensure integer division handles numPermutations < num_threads correctly
      const unsigned int tasks_per_thread = (numPermutations > 0) ? (numPermutations / num_threads) : 0;
      const unsigned int remaining_tasks = (numPermutations > 0) ? (numPermutations % num_threads) : 0;

      std::vector<std::future<void>> futures;
      for (unsigned int thread_idx = 0; thread_idx < num_threads; ++thread_idx)
	{
	  // Handle case where numPermutations < num_threads
	  if (tasks_per_thread == 0 && thread_idx >= remaining_tasks)
	    break;

	  const unsigned int start_idx = thread_idx * tasks_per_thread +
	    std::min(thread_idx, remaining_tasks);
	  const unsigned int num_tasks_this_thread = tasks_per_thread +
	    (thread_idx < remaining_tasks ? 1 : 0);
	  const unsigned int end_idx = start_idx + num_tasks_this_thread;

	  if (num_tasks_this_thread == 0)
	    continue; // Skip threads with no work


	  futures.emplace_back(std::async(std::launch::async, [&, start_idx, end_idx]() { // Pass needed captures
	    for (unsigned int p = start_idx; p < end_idx; ++p)
	      {
		std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio;
		try
		  {
		    // Ensure createSyntheticPortfolio is defined and accessible
		    syntheticPortfolio = createSyntheticPortfolio<Decimal>(theSecurity, basePortfolioPtr);
		  }
		catch (const std::exception& e)
		  {
		    std::cerr << "Error creating synthetic portfolio for permutation " << p << ": " << e.what() << std::endl;
		    // Decide if throwing is appropriate or if this permutation should be skipped
		    throw; // Re-throwing for now
		  }

		Decimal max_stat_perm_this_step = std::numeric_limits<Decimal>::lowest();

		for (const auto& active_strategy_ptr : active_strategies)
		  {
		    if (!active_strategy_ptr)
		      {
			throw std::runtime_error("Critical Error: Null strategy pointer encountered in active set!");
		      }

		    uint32_t stratTrades = 0;
		    Decimal stat_perm_active = std::numeric_limits<Decimal>::lowest();

		    // Min trades loop
		    // Ensure getMinStrategyTrades() is static or accessible on BaselineStatPolicy
		    while (stratTrades < BaselineStatPolicy::getMinStrategyTrades())
		      {
			std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
			try
			  {
			    clonedStrategy = active_strategy_ptr->clone(syntheticPortfolio);
			  }
			catch (const std::exception& e)
			  {
			    std::cerr << "Warning: Failed to clone strategy " <<
			      active_strategy_ptr->getStrategyName() << " perm "
				      << p << ": " << e.what() << std::endl;
			    break;
			  }

			std::shared_ptr<BackTester<Decimal>> clonedBackTester;

			try
			  {
			    clonedBackTester = templateBackTester->clone();
			  }
			catch (const std::exception& e)
			  {
			    std::cerr << "Warning: Failed to clone backtester for strategy " << active_strategy_ptr->getStrategyName() << " perm " << p << ": " << e.what() << std::endl; break;
			  }

			clonedBackTester->addStrategy(clonedStrategy);
			try
			  {
			    clonedBackTester->backtest();
			    stratTrades = BackTesterFactory<Decimal>::getNumClosedTrades(clonedBackTester);
			    if (stratTrades >= BaselineStatPolicy::getMinStrategyTrades()) {
                                         // Ensure getPermutationTestStatistic is static or accessible
                                         stat_perm_active = BaselineStatPolicy::getPermutationTestStatistic(clonedBackTester);
                                     }
                                 } catch (const std::exception& e) { std::cerr << "Warning: Backtest failed for strategy " << active_strategy_ptr->getStrategyName() << " perm " << p << ": " << e.what() << std::endl; /* Allow continuing? */ }
                                  // Add a break condition if backtest fails irrecoverably or min trades cannot be met
                                  if (/* condition to break loop, e.g., too many attempts */ false) break;
                             } // End while min trades

                             max_stat_perm_this_step = std::max(max_stat_perm_this_step, stat_perm_active);
                         } // End for active_strategies

                         if (max_stat_perm_this_step >= baselineStat_k) {
                             count_k.fetch_add(1, std::memory_order_relaxed);
                         }
                     } // End for p (permutations)
                 })); // End async lambda
             } // End for threads

             // Wait for futures and handle potential exceptions
             try {
                 for (auto& future : futures) { future.get(); }
             } catch (const std::exception& e) {
                 std::cerr << "Exception caught during permutation execution (slow policy): " << e.what() << std::endl;
                 throw; // Re-throw or handle as appropriate
             }
             return count_k.load();
        } // End computePermutationCountForStep
    }; // End class MastersPermutationPolicy


    // --- FastMastersPermutationPolicy (New Fast) ---
    template <class Decimal, class BaselineStatPolicy>
    class FastMastersPermutationPolicy
    {
    public:
        using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>;
        using LocalStrategyDataContainer = StrategyDataContainer<Decimal>;
        using AtomicCountsMap = std::map<StrategyPtr, std::atomic<unsigned int>>;
        using FinalCountsMap = std::map<StrategyPtr, unsigned int>;

        FastMastersPermutationPolicy() = delete; // Static class

        static FinalCountsMap computeAllPermutationCounts(
            uint32_t numPermutations,
            const LocalStrategyDataContainer& sorted_strategy_data, // Uses type from PALMonteCarloTypes.h
            std::shared_ptr<BackTester<Decimal>> templateBackTester,
            std::shared_ptr<Security<Decimal>> theSecurity,
            std::shared_ptr<Portfolio<Decimal>> basePortfolioPtr)
        {
            if (sorted_strategy_data.empty()) { return {}; }

            if (numPermutations == 0) {
                 throw std::runtime_error("FastMastersPermutationPolicy::computeAllPermutationCounts - Number of permutations cannot be zero.");
            }
             if (!templateBackTester || !theSecurity || !basePortfolioPtr) {
                 throw std::runtime_error("FastMastersPermutationPolicy::computeAllPermutationCounts - Null pointer provided for backtester, security, or portfolio.");
            }


            AtomicCountsMap atomic_counts;
            for (const auto& entry : sorted_strategy_data) { // entry is StrategyContext<Decimal>
                atomic_counts[entry.strategy].store(1);
            }

            const unsigned int hardware_threads = std::thread::hardware_concurrency();
            const unsigned int num_threads = (hardware_threads == 0) ? 2 : hardware_threads;
            const unsigned int tasks_per_thread = (numPermutations > 0) ? (numPermutations / num_threads) : 0;
            const unsigned int remaining_tasks = (numPermutations > 0) ? (numPermutations % num_threads) : 0;

            std::vector<std::future<void>> futures;
            for (unsigned int thread_idx = 0; thread_idx < num_threads; ++thread_idx)
            {
                 if (tasks_per_thread == 0 && thread_idx >= remaining_tasks) break;
                 const unsigned int start_idx = thread_idx * tasks_per_thread + std::min(thread_idx, remaining_tasks);
                 const unsigned int num_tasks_this_thread = tasks_per_thread + (thread_idx < remaining_tasks ? 1 : 0);
                 const unsigned int end_idx = start_idx + num_tasks_this_thread;
                 if (num_tasks_this_thread == 0) continue;

                futures.emplace_back(std::async(std::launch::async, [&, start_idx, end_idx]() {
                    for (unsigned int p = start_idx; p < end_idx; ++p)
                    {
                        std::shared_ptr<Portfolio<Decimal>> syntheticPortfolio;
                         try {
                             syntheticPortfolio = createSyntheticPortfolio<Decimal>(theSecurity, basePortfolioPtr);
                         } catch (const std::exception& e) {
                              std::cerr << "Error creating synthetic portfolio for permutation " << p << ": " << e.what() << std::endl;
                              throw; // Or skip permutation
                         }

                        std::map<StrategyPtr, Decimal> permuted_stats_this_rep;
                        // Backtest ALL strategies for this permutation
                        for (const auto& entry : sorted_strategy_data)
                        {
                            StrategyPtr strategy = entry.strategy;
                            Decimal stat = std::numeric_limits<Decimal>::lowest();
                            uint32_t trades = 0;

                             // Min trades loop
                            while (trades < BaselineStatPolicy::getMinStrategyTrades()) {
                                 std::shared_ptr<BacktesterStrategy<Decimal>> clonedStrategy;
                                 try { clonedStrategy = strategy->clone(syntheticPortfolio); }
                                 catch (const std::exception& e) { std::cerr << "Warning: Failed to clone strategy " << strategy->getStrategyName() << " perm " << p << ": " << e.what() << std::endl; break;}

                                 std::shared_ptr<BackTester<Decimal>> clonedBackTester;
                                 try { clonedBackTester = templateBackTester->clone(); }
                                 catch (const std::exception& e) { std::cerr << "Warning: Failed to clone backtester for strategy " << strategy->getStrategyName() << " perm " << p << ": " << e.what() << std::endl; break;}

                                 clonedBackTester->addStrategy(clonedStrategy);
                                 try {
                                     clonedBackTester->backtest();
                                     trades = BackTesterFactory<Decimal>::getNumClosedTrades(clonedBackTester);
                                     if (trades >= BaselineStatPolicy::getMinStrategyTrades()) {
                                         stat = BaselineStatPolicy::getPermutationTestStatistic(clonedBackTester);
                                     }
                                 } catch (const std::exception& e) { std::cerr << "Warning: Backtest failed for strategy " << strategy->getStrategyName() << " perm " << p << ": " << e.what() << std::endl;}
                                  if (/* condition to break loop */ false) break;
                             } // End while min trades
                            permuted_stats_this_rep[strategy] = stat;
                        } // End for entry


                        Decimal max_f = std::numeric_limits<Decimal>::lowest();
                        // Iterate WORST to BEST (reverse order of descending sort)
                        for (int k = sorted_strategy_data.size() - 1; k >= 0; --k)
                        {
                            const auto& entry = sorted_strategy_data[k];
                            StrategyPtr strategy_k = entry.strategy;
                            const Decimal original_baseline_k = entry.baselineStat;
                            Decimal permuted_stat_k = std::numeric_limits<Decimal>::lowest();

                            auto it_perm_stat = permuted_stats_this_rep.find(strategy_k);
                            if (it_perm_stat != permuted_stats_this_rep.end()) {
                                permuted_stat_k = it_perm_stat->second;
                            } else {
                                 std::cerr << "Warning: Permuted stat not found for " << strategy_k->getStrategyName() << " in permutation " << p << std::endl;
                            }

                            max_f = std::max(max_f, permuted_stat_k);

                            if (max_f >= original_baseline_k)
                            {
                                auto it_atomic = atomic_counts.find(strategy_k);
                                if (it_atomic != atomic_counts.end()) {
                                     it_atomic->second.fetch_add(1, std::memory_order_relaxed);
                                } else {
                                     std::cerr << "Warning: Atomic counter not found for " << strategy_k->getStrategyName() << std::endl;
                                }
                            }
                        } // End for k (worst to best)
                    } // End for p (permutations)
                })); // End async lambda
            } // End for threads

            // Wait for futures and handle potential exceptions
             try {
                 for (auto& future : futures) { future.get(); }
             } catch (const std::exception& e) {
                 std::cerr << "Exception caught during permutation execution (fast policy): " << e.what() << std::endl;
                 throw; // Re-throw or handle as appropriate
             }


            FinalCountsMap final_counts;
            for (const auto& pair : atomic_counts) {
                final_counts[pair.first] = pair.second.load();
            }
            return final_counts;
        } // End computeAllPermutationCounts
    }; // End class FastMastersPermutationPolicy

} // namespace mkc_timeseries

#endif // __MASTER_PERMUTATION_TEST_COMPUTATION_POLICY_H
