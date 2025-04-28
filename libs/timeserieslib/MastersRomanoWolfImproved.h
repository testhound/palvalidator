/**************************************************************************************
 *  FastPermutation.h
 *
 *  Implements the **fast approximation** of Masters’ step‑down algorithm.
 *  Characteristics
 *  ---------------
 *    • Pre‑computes max‑statistic counts for *all* strategies in one sweep using
 *      FastMastersPermutationPolicy ⇒ O(N + permutations × back‑tests).
 *    • Step‑down loop then uses those cached counts – no per‑step re‑sampling.
 *    • Produces identical adjusted p‑values to the slow algorithm in practice
 *      for large permutation counts, but > 10× faster on typical test suites.
 *
 *  Dependencies
 *  ------------
 *    • FastMastersPermutationPolicy.h   – bulk permutation count computation
 *************************************************************************************/

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
     * Based on Masters (2016) "Efficient Computation of Adjusted p-Values for Resampling-Based
     * Stepdown Multiple Testing" (Romano & Wolf, 2016), this class uses
     * FastMastersPermutationPolicy to compute counts in a single Monte Carlo sweep.
     *
     * Template Parameters:
     *   @tparam Decimal            Numeric type for test statistics (e.g., double).
     *   @tparam BaselineStatPolicy Policy providing:
     *                              - getMinStrategyTrades()
     *                              - getPermutationTestStatistic(bt)
     */
#pragma once
#include "IPermutationAlgorithm.h"
#include "MastersPermutationComputationPolicy.h"

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
   * Based on Masters (2016) "Efficient Computation of Adjusted p-Values for Resampling-Based
   * Stepdown Multiple Testing" (Romano & Wolf, 2016), this class uses
   * FastMastersPermutationPolicy to compute counts in a single Monte Carlo sweep.
   *
   * Template Parameters:
   *   @tparam Decimal            Numeric type for test statistics (e.g., double).
   *   @tparam BaselineStatPolicy Policy providing:
   *                              - getMinStrategyTrades()
   *                              - getPermutationTestStatistic(bt)
   */
    template<class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolfImproved final
        : public IPermutationAlgorithm<Decimal, BaselineStatPolicy>
    {
        using Base       = IPermutationAlgorithm<Decimal, BaselineStatPolicy>;
        using StrategyPtr= typename Base::StrategyPtr;
        using StrategyVec= typename Base::StrategyVec;

    public:
      /**
         * @brief Run the fast stepwise FWE permutation test.
         *
         * Implements the two-phase improved algorithm:
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
         * @param strategyData     Vector of StrategyContext sorted by descending baseline statistic.
         * @param numPermutations  Number of permutations (m > 0).
         * @param tmplBT           Prototype backtester to clone per backtest.
         * @param portfolio        Portfolio containing the target security (first element).
         * @param sigLevel         Desired familywise error rate alpha.
         * @return Map from strategy ptr to its adjusted p-value.
         */
        std::map<StrategyPtr, Decimal> run(const StrategyVec&                strategyData,
                                           unsigned long                     numPermutations,
                                           const std::shared_ptr<BackTester<Decimal>>& tmplBT,
                                           const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
                                           const Decimal&                   sigLevel) override
        {
            using FMPP = FastMastersPermutationPolicy<Decimal, BaselineStatPolicy>;

	    // Phase 1: compute exceedance counts for every strategy in one Monte Carlo sweep
            //   counts[strategy] = 1 + # permutations where strategy's observed statistic
            //                      is beaten by the max-of-all in that permutation.
            // Bulk compute exceedance counts for every strategy once.
            std::map<StrategyPtr, unsigned int> counts =
                FMPP::computeAllPermutationCounts(numPermutations,
                                                  strategyData,
                                                  tmplBT,
                                                  portfolio);

            std::map<StrategyPtr, Decimal> pvals;
            Decimal lastAdj = Decimal(0);

	    // Phase 2: step-down inclusion loop (best-to-worst)
            for (auto& context : strategyData)
            {
                unsigned int c = numPermutations + 1;

		auto it = counts.find(context.strategy);
		if (it != counts.end())
		  c = it->second;

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
    };
} // namespace mkc_timeseries
