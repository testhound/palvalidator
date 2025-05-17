#pragma once
#include <cassert>
#include <stdexcept> // Required for std::invalid_argument
#include <algorithm>
#include "IMastersSelectionBiasAlgorithm.h"
#include "MastersPermutationTestComputationPolicy.h"

namespace mkc_timeseries
{
  /**
   * @class MastersRomanoWolfImproved
   * @brief Fast stepwise permutation testing algorithm with strong FWE control.
   *
   * This "improved" version performs all m permutations in one bulk pass,
   * computing exceedance counts for each strategy simultaneously (from worst to best),
   * then applies a step-down inclusion loop (from best to worst) over the precomputed counts.
   * This is mathematically equivalent to the naive stepwise algorithm but avoids repeating
   * the expensive shuffle/backtest m times per strategy, reducing the complexity to
   * O(N + m × total_backtests).
   *
   * Based on the algorithm in Timothy Masters book"
   * "Permutation and Randomization Tests for Trading System Development: Algorithms in C++"
   *
   * Which itself is base don "Efficient Computation of Adjusted p-Values for Resampling-Based
   * Stepdown Multiple Testing" (Romano & Wolf, 2016)
   *
   * this class uses
   * class FastMastersPermutationPolicy to compute counts in a single Monte Carlo sweep.
   *
   * Template Parameters:
   *   @tparam Decimal            Numeric type for test statistics (e.g., double).
   *   @tparam BaselineStatPolicy Policy providing:
   *                              - getMinStrategyTrades()
   *                              - getPermutationTestStatistic(bt)
   */

  template<class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolfImproved final
        : public IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>
    {
        using Base       = IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>;
        using StrategyPtr= typename Base::StrategyPtr;
        using StrategyVec= typename Base::StrategyVec;

    public:
      /**
         * @brief Run the fast stepwise FWE permutation test.
         *
         * Implements the two-phase improved algorithm:
         *
	 * 
	 * Precondition: `strategyData` **must** be sorted in **descending** order by
	 *   `baselineStat` (highest first) before calling.
	 *
         * Phase 1: Bulk permutation counts (worst-to-best)
         *   - Call FastMastersPermutationPolicy::computeAllPermutationCounts to
         *     generate a map of each strategy to its exceedance count:
         *     count_i = 1 + # of permutations where max_{all active strategies}
         *               (statistic) >= original_statistic_i
         *   - This single Monte Carlo loop shuffles once per permutation,
         *     runs backtests for all strategies, and accumulates counts.
         *
         * Phase 2: Step‑down inclusion (best-to-worst)
         *   - Iterate through strategies in descending order of observed performance.
         *   - Compute p_i = count_i / (m + 1), then adjust: p_adj_i = max(p_i, lastAdj).
         *   - If p_adj_i <= alpha, accept (remove tightening bound), else assign
         *     p_adj_i to all remaining and exit.
         *
         * @param strategyData     Pre-sorted vector of StrategyContext (strategy + observed statistic).
	 *                          **Precondition:** sorted descending by `baselineStat`
         * @param numPermutations  Number of permutations (m > 0).
         * @param tmplBT           Prototype backtester to clone per backtest.
         * @param portfolio        Portfolio containing the target security (first element).
         * @param sigLevel         Desired familywise error rate alpha.
         * @return Map from strategy ptr to its adjusted p-value.
         */
      std::map<StrategyPtr, Decimal> run(const StrategyVec&                strategyData,
					 unsigned long                     numPermutations,
					 const std::shared_ptr<BackTester<Decimal>>& templateBacktester,
					 const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
					 const Decimal&                   sigLevel) override
      {
	if ( !templateBacktester )
	  {
            throw std::runtime_error("MastersRomanoWolfImproved::run - backtester is null");
	  }

	using FMPP = FastMastersPermutationPolicy<Decimal, BaselineStatPolicy>;

	// Check the precondition and throw if violated
	if (!std::is_sorted(strategyData.begin(), strategyData.end(),
			    [](auto const& a, auto const& b) {
			      return a.baselineStat > b.baselineStat;
			    }))
	  {
	    const std::string error_message =
	      "MastersRomanoWolfImproved::run requires strategyData to be "
	      "pre-sorted in descending order by baselineStat.";
		
	    throw std::invalid_argument(error_message);
	  }

	// Extract the target security from the portfolio:
	auto secIt = portfolio->beginPortfolio();
	if (secIt == portfolio->endPortfolio()) {
	  throw std::runtime_error(
				   "MastersRomanoWolfImproved::run - portfolio contains no securities");
	}
	auto secPtr = secIt->second;
	    
	// Phase 1: compute exceedance counts for every strategy in one Monte Carlo sweep
	//   counts[strategy] = 1 + # permutations where strategy's observed statistic
	//                      is beaten by the max-of-all in that permutation.
	// Bulk compute exceedance counts for every strategy once.

	std::map<StrategyPtr, unsigned int> counts =
	  FMPP::computeAllPermutationCounts(numPermutations,
					    strategyData,
					    templateBacktester,
					    secPtr,
					    portfolio);

	// SANITY CHECK
        sanityCheckCounts(counts, strategyData);

	std::map<StrategyPtr, Decimal> pvals;
	Decimal lastAdj = Decimal(0);

	// Phase 2: step-down inclusion loop (best-to-worst)
	for (auto& context : strategyData)
	  {
	    unsigned int c = numPermutations + 1;

	    auto it = counts.find(context.strategy);
	    if (it != counts.end())
	      c = it->second;

	    /**
	     * Enforce monotonicity on the adjusted p-values in the step-down permutation test.
	     *
	     * In a step-down procedure, we rank strategies by their observed statistic (e.g. Profit-Factor)
	     * from highest (best) to lowest (worst), then compute a “raw” p-value for each:
	     *
	     *     // count of permutations whose test statistic ≥ observed, divided by total draws
	     *     Decimal p = Decimal(c) / Decimal(numPermutations + 1);
	     *
	     * To prevent a weaker (lower-ranked) strategy from ever appearing more significant
	     * than a stronger (higher-ranked) one, we enforce that the sequence of adjusted
	     * p-values never decreases as we move down the list:
	     *
	     *     // take the larger of this strategy’s raw p and the previous (best) adjusted p
	     *     Decimal adj = std::max(p, lastAdj);
	     *
	     * This ensures:
	     *  1. **Non-decreasing p-values**: Once you hit, say, 0.04 at the top, every following
	     *     strategy’s adjusted p will be ≥ 0.04.
	     *  2. **Logical consistency**: You cannot claim a weaker system is more significant
	     *     than a stronger one.
	     *  3. **Step-down stopping rule**: As soon as an adjusted p exceeds your α threshold,
	     *     you can stop: no weaker strategy further down can sneak in below the threshold.
	     *
	     * In plain English:
	     *
	     * “We first decide how likely it is that pure chance could give us each strategy’s
	     * observed result.  Then, to keep our decisions consistent from best to worst, we
	     * never let a later strategy’s p-value drop below the one before it.”
	     */
	    Decimal p   = Decimal(c) / Decimal(numPermutations + 1);
	    Decimal adj = std::max(p, lastAdj);
	    pvals[context.strategy] = adj;

	    if (adj <= sigLevel)
	      lastAdj = adj;      // tighten bound
	    else
	      {
		// failure ⇒ all remaining inherit same p‑value
		for (auto& later : strategyData)
		  if (!pvals.count(later.strategy))
		    pvals[later.strategy] = adj;
		break;
	      }
	  }

	return pvals;
      }
    private:
      // Helper to verify that 'counts' has exactly one entry per strategy
    void sanityCheckCounts(const std::map<StrategyPtr, unsigned int>& counts,
			   const StrategyVec& strategyData) const
      {
	if (counts.size() != strategyData.size())
	  throw std::logic_error("Permutation count map has wrong number of entries");

	for (auto const& ctx : strategyData) {
	  if (counts.find(ctx.strategy) == counts.end())
            throw std::logic_error("Missing permutation count for a strategy");
	}

	for (auto const& kv : counts)
	  {
	    bool found = false;
	    for (auto const& ctx : strategyData)
	      if (kv.first == ctx.strategy)
		{
		  found = true;
		  break;
		}

	    if (!found)
	      throw std::logic_error("counts map contains an unexpected strategy key");
	  }
      }
    };
} // namespace mkc_timeseries
