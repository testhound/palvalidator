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
#include "ParallelExecutors.h"
#include "ParallelFor.h"

namespace mkc_timeseries
{
   // pick a sensible default pool size for these “5000 permutations” loops
  static constexpr std::size_t kMasterThreads =
    std::thread::hardware_concurrency() > 0
    ? std::thread::hardware_concurrency()
    : 2;

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
 * @tparam Executor Concurrency executor (defaults to StdAsyncExecutor).
 */
  template <class Decimal, class BaselineStatPolicy, class Executor = concurrency::ThreadPoolExecutor<kMasterThreads>>
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
     *
     * @return Number of permutations (including original data) where the max permuted statistic exceeds baselineStat_k.
     */
    static unsigned int computePermutationCountForStep(
        uint32_t                                                        numPermutations,
        const Decimal                                                   baselineStat_k,
        const std::vector<std::shared_ptr<PalStrategy<Decimal>>>&       active_strategies,
        std::shared_ptr<BackTester<Decimal>>                            templateBackTester,
        std::shared_ptr<Security<Decimal>>                              theSecurity,
        std::shared_ptr<Portfolio<Decimal>>                             basePortfolioPtr
    )
    {
      if (active_strategies.empty())
        {
	  std::cerr << "Warning: no active strategies supplied." << std::endl;
	  return 1;
        }

      if (numPermutations == 0)
        {
	  throw std::runtime_error(
				   "MastersPermutationPolicy::computePermutationCountForStep - numPermutations cannot be zero"
				   );
        }

      if (!templateBackTester || !theSecurity || !basePortfolioPtr)
        {
	  throw std::runtime_error(
				   "MastersPermutationPolicy::computePermutationCountForStep - null pointer provided"
				   );
        }

      Executor executor{};
      std::atomic<unsigned> count_k{1};

      // Launch a parallel loop over the range [0 … numPermutations), using our executor.
      // For each index p, invoke the lambda body below.
      //
      // [=, &count_k] means:
      // Capture-by-value for all automatic (stack) variables that the lambda uses
      // (e.g. active_strategies, templateBackTester, theSecurity, basePortfolioPtr, baselineStat_k, etc.).
      //
      // &count_k:
      // Capture-by-reference only for count_k, our std::atomic<unsigned> counter.
      // Allows each lambda invocation (across all threads) to increment the same shared counter.
      //
      // Putting them together:
      // You get value semantics (thread-safe reads) for everything you only need to read,
      // and reference semantics for that single shared counter you need to update.
      // Define work lambda for a single permutation
        auto work = [ =, &count_k ]
        (uint32_t p)
        {
	  auto syntheticPortfolio = createSyntheticPortfolio<Decimal>
            (
	     theSecurity,
	     basePortfolioPtr
	     );

	  // Compute maximum statistic across strategies
	  Decimal max_stat = std::numeric_limits<Decimal>::lowest();
	  unsigned int minTrades = BaselineStatPolicy::getMinStrategyTrades();

	  for ( auto const& strat : active_strategies )
            {
	      if ( !strat )
                {
		  throw std::runtime_error
                    (
		     "Null strategy pointer in active_strategies"
		     );
                }

	      Decimal stat = std::numeric_limits<Decimal>::lowest();
	      
	      if (minTrades == 0)
		{
		  // single draw when no minimum trades required
		  auto btClone = templateBackTester->clone();
		  auto clonedStrat = strat->clone(syntheticPortfolio);
		  btClone->addStrategy(clonedStrat);
		  btClone->backtest();
		  stat = BaselineStatPolicy::getPermutationTestStatistic(btClone);
		}
	      else
		{
		  auto btClone = templateBackTester->clone();
		  auto clonedStrat = strat->clone(syntheticPortfolio);
		  btClone->addStrategy(clonedStrat);
		  btClone->backtest();

		  uint32_t trades = BackTesterFactory<Decimal>::getNumClosedTrades(btClone);
		  trades = BackTesterFactory<Decimal>::getNumClosedTrades(btClone);
		  if (trades >= minTrades)
		    stat = BaselineStatPolicy::getPermutationTestStatistic(btClone);
		  else
		    // below minimum, count as “no relationship” under the null hypothesis
		    stat = std::numeric_limits<Decimal>::lowest();
		}

	      max_stat = std::max( max_stat, stat );
            }

	  // Increment count if statistic exceeds baseline
	  if ( max_stat >= baselineStat_k )
	      count_k.fetch_add( 1, std::memory_order_relaxed );
        };

        // Execute the work in parallel for each permutation
        concurrency::parallel_for
        (
	 numPermutations,
	 executor,
	 work
	 );

        return count_k.load();
    }
  }; // End class MastersPermutationPolicy

  /**
   * @class FastMastersPermutationPolicy
   * @brief Computes exceedance counts for all strategies in one parallel sweep.
   *
   * This "fast" policy runs every strategy on each permutation exactly once,
   * accumulating how often each strategy's statistic is beaten by the maximum
   * permuted statistic across all strategies.  This yields a map of counts
   * that can be converted to adjusted p-values in a step-down procedure.
   *
   * @tparam Decimal Numeric type for calculations (e.g., double).
   * @tparam BaselineStatPolicy Policy to extract stats and minimum trades.
   * @tparam Executor Concurrency executor (defaults to StdAsyncExecutor).
   */
  template<
    class Decimal,
    class BaselineStatPolicy,
    class Executor = concurrency::ThreadPoolExecutor<kMasterThreads>>
  class FastMastersPermutationPolicy
  {
  public:
    using StrategyPtr       = std::shared_ptr<PalStrategy<Decimal>>;
    using LocalStrategyData = StrategyDataContainer<Decimal>;
    using LocalStrategyDataContainer = StrategyDataContainer<Decimal>;
    using AtomicCountsMap   = std::map<StrategyPtr, std::atomic<unsigned>>;
    using FinalCountsMap    = std::map<StrategyPtr, unsigned>;

    FastMastersPermutationPolicy() = delete;  // static-only

    /**
     * @brief Bulk computes exceedance counts for each strategy.
     *
     * Divides [0..numPermutations) into chunks, runs in parallel,
     * and for each permutation:
     *   1. Generate a synthetic portfolio.
     *   2. Run backtests for every strategy and record each statistic.
     *   3. Compute the maximum statistic over all strategies.
     *   4. For each strategy whose baseline <= max, increment its count.
     *
     * @param numPermutations Number of permutation iterations (>0).
     * @param sorted_strategy_data Pre-sorted container of StrategyContext.
     * @param templateBackTester BackTester to clone each iteration.
     * @param theSecurity Security to create synthetic data.
     * @param basePortfolioPtr Base portfolio for synthetic generation.
     * @return Map from each strategy to its exceedance count.
     */
    static FinalCountsMap computeAllPermutationCounts
    (
     uint32_t                                numPermutations,
     const LocalStrategyData&                sorted_strategy_data,
     std::shared_ptr<BackTester<Decimal>>    templateBackTester,
     std::shared_ptr<Security<Decimal>>      theSecurity,
     std::shared_ptr<Portfolio<Decimal>>     basePortfolioPtr
     )
    {
      // Validate inputs
      if (sorted_strategy_data.empty())
        {
	  return {};
        }

      if (numPermutations == 0)
        {
	  throw std::runtime_error(
				   "FastMastersPermutationPolicy::computeAllPermutationCounts - numPermutations cannot be zero"
				   );
        }

      if (!templateBackTester || !theSecurity || !basePortfolioPtr)
        {
	  throw std::runtime_error(
				   "FastMastersPermutationPolicy::computeAllPermutationCounts - null pointer provided"
				   );
        }

      // Initialize atomic counters for each strategy (start at 1 for the unpermuted case)
      AtomicCountsMap atomic_counts;
      for (auto const& ctx : sorted_strategy_data)
        {
	  atomic_counts[ctx.strategy].store(1);
        }

      Executor executor{};  // default or platform-specific executor

      // Define work lambda: processes one permutation index 'p'
      auto work = [=, &atomic_counts]
        (
	 uint32_t p
	 )
      {
	// 1) Create synthetic portfolio for this permutation
	auto syntheticPortfolio = createSyntheticPortfolio<Decimal>
	  (
	   theSecurity,
	   basePortfolioPtr
	   );

	// 2) Compute statistic for each strategy
	std::map<StrategyPtr, Decimal> stats_this_perm;
	for (auto const& ctx : sorted_strategy_data)
	  {
	    auto strategy = ctx.strategy;

	    // Clone backtester until minimum trades satisfied
	    uint32_t trades = 0;
	    Decimal stat = std::numeric_limits<Decimal>::lowest();

	    auto clonedStrat = strategy->clone(syntheticPortfolio);
	    auto btClone     = templateBackTester->clone();
	    btClone->addStrategy(clonedStrat);
	    btClone->backtest();

	    trades = BackTesterFactory<Decimal>::getNumClosedTrades(btClone);
	    if (trades >= BaselineStatPolicy::getMinStrategyTrades())
	      {
		stat = BaselineStatPolicy::getPermutationTestStatistic(btClone);
	      }
	    else
	      {
		// below minimum, count as “no relationship” under the null hypothesis
		stat = std::numeric_limits<Decimal>::lowest();
	      }
	    
	    stats_this_perm[strategy] = stat;
	  }

	// 3) Determine max statistic
	Decimal max_f = std::numeric_limits<Decimal>::lowest();
	for (auto const& entry : stats_this_perm)
	  {
	    max_f = std::max(max_f, entry.second);
	  }

	// 4) Increment counters for any strategy beaten by max_f
	for (auto const& ctx : sorted_strategy_data)
	  {
	    if (max_f >= ctx.baselineStat)
	      {
		atomic_counts[ctx.strategy].fetch_add(1, std::memory_order_relaxed);
	      }
	  }
      };

      // Run work in parallel across all permutation indices
      concurrency::parallel_for
        (
	 numPermutations,
	 executor,
	 work
	 );

      // Collect final counts into a non-atomic map
      FinalCountsMap final_counts;
      for (auto const& pair : atomic_counts)
        {
	  final_counts[pair.first] = pair.second.load();
        }

      return final_counts;
    }
  };
} // namespace mkc_timeseries

#endif // __MASTER_PERMUTATION_TEST_COMPUTATION_POLICY_H
