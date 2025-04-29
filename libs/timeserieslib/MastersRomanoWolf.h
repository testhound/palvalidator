#pragma once
#include <unordered_set>
#include "IMastersSelectionBiasAlgorithm.h"
#include "MastersPermutationTestComputationPolicy.h"

namespace mkc_timeseries
{
   /**
     * @class MastersRomanoWolf
     * @brief Implements the stepwise, strong-familywise-error-rate (FWE) permutation testing algorithm
     *        described by Masters (2016), based on Romano and Wolf (2016).
     *
     * This class runs a step-down multiple-hypothesis test where each strategy's null hypothesis
     * (no relationship to the target) is evaluated one at a time, starting from the highest-performing
     * strategy down to the lowest.  At each step:
     *  1. The active set of competitors is permuted by shuffling the target (or generating a synthetic data set).
     *  2. A null distribution of the maximum test statistic over the current active strategies is built via
     *     Monte Carlo (m permutations).
     *  3. The right-tail p-value for the current strategy is estimated by counting how often the max permuted
     *     statistic exceeds its observed statistic, then dividing by (m + 1).
     *  4. A step-down adjustment enforces monotonicity: each adjusted p-value is at least as large as the last.
     *  5. If the adjusted p-value <= alpha, the strategy is declared significant and removed from the active set;
     *     otherwise, the procedure stops and all remaining strategies inherit the same p-value.
     *
     * @tparam Decimal Numeric type for statistics (e.g., double, long double).
     * @tparam BaselineStatPolicy Policy providing:
     *         - static unsigned int getMinStrategyTrades(): minimum trades threshold.
     *         - static Decimal getPermutationTestStatistic(bt): statistic extraction.
     */
    template <class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolf : public IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>
    {
      using Base  = IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>;
      using Strat = typename Base::StrategyPtr;
      using Vec   = typename Base::StrategyVec;

    public:
      /**
       * @brief Execute the stepwise permutation test with strong FWE control.
       *
       * Implements Masters’ algorithm:
       *  - Sort strategies descending by their observed baseline statistic.
       *  - Iterate from best to worst (stepwise loop), computing a Monte Carlo null distribution
       *    of max-statistic over *remaining* competitors only, thus shrinking the null distribution
       *    at each step.
       *  - Estimate right-tail p-value = (# permuted max >= observed) / (m + 1).
       *  - Enforce monotonicity (step-down): p_adj[i] = max(p_i, p_adj[i-1]).
       *  - Stop early when adjusted p-value > alpha, assigning that p-value to all remaining.
       *
       * @param strategyData      Pre-sorted vector of StrategyContext (strategy + observed statistic).
       * @param numPermutations   Number of Monte Carlo permutations (m > 0).
       * @param templateBacktester Prototype BackTester to clone for each permutation.
       * @param portfolio         Portfolio owning the target security (used to extract security ptr).
       * @param sigLevel          Desired familywise alpha level (e.g., 0.05).
       * @return Map from each strategy ptr to its adjusted p-value.
       * @throws std::runtime_error if portfolio empty.
       */
      std::map<Strat, Decimal> run(const Vec& strategyData,
				   unsigned long numPermutations,
				   const std::shared_ptr<BackTester<Decimal>>& templateBacktester,
				   const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
				   const Decimal& sigLevel) override
        {
            using MPP = MastersPermutationPolicy<Decimal, BaselineStatPolicy>;

	    std::map<Strat, Decimal> pvals;
            Decimal lastAdj = Decimal(0);


	    // Extract the first security from the portfolio
	    auto it = portfolio->beginPortfolio();
	    if (it == portfolio->endPortfolio()) {
	      throw std::runtime_error(
				       "MastersRomanoWolf::run - portfolio contains no securities");
	    }
	    auto secPtr = it->second;  // SecurityPtr

            // Active set holds strategies still under consideration.
            std::unordered_set<Strat> activeStrategies;
	    
	    for (auto& context : strategyData)
	      activeStrategies.insert(context.strategy);

	    // Stepwise accumulation loop (from best to worst competitor)
            for (auto& context : strategyData)
            {
                const Strat& S = context.strategy;

                // If previously resolved (happens when we fail early) just propagate.
                if (!activeStrategies.count(S))
                {
                    pvals[S] = lastAdj;
                    continue;
                }

                // Compute permutation count using ONLY currently active strategies.
                std::vector<Strat> activeVec(activeStrategies.begin(), activeStrategies.end());

		// Step 1: Monte Carlo null‐distribution generation over the active strategies
		// ---------------------------------------------------------------------------------
		// For each of the m permutations we want the distribution of the *maximum* strategy
		// statistic (e.g. profit factor) under the null hypothesis.  Concretely:
		//   1) createSyntheticPortfolio(...) shuffles the original OHLC series into a “null”
		//      synthetic time series (breaks any real predictive signal, preserves vol/structure).
		//
		//   2) For each PalStrategy in active_strategies:
		//        a) clone it against the syntheticPortfolio
		//        b) clone the templateBacktester, add the cloned strategy
		//        c) backtest to produce its performance statistic (profit factor, etc.)
		//        d) extract that statistic via BaselineStatPolicy
		//
		//   3) Track max_stat = max(max_stat, stat) across all strategies in this permutation.
		//
		// Repeating steps 1–3( )m times yields {max_stat₁, …, max_statₘ}, the empirical null
		// distribution of the *best* strategy’s performance by chance.  We’ll later compare
		// the observed baseline statistic of the current strategy to this null to get its p-value.
                unsigned int exceedCount = MPP::computePermutationCountForStep(
                                                numPermutations,
                                                context.baselineStat,
                                                activeVec,
                                                templateBacktester,
                                                secPtr,
                                                portfolio);

		// Step 2: estimate p-value = (# exceedances) / (m+1)
                Decimal p   = Decimal(exceedCount) / Decimal(numPermutations + 1);

		 // Step 3: step-down monotonicity adjustment
                Decimal adj = std::max(p, lastAdj);          // step‑down monotonicity
                pvals[S]    = adj;

                if (adj <= sigLevel)
                {
		  // As we remove one surviving strategy at each step, the set over which we take the
		  // maximum shrinks—and so the null distribution tightens—giving us more power on
		  // subsequent (weaker) strategies while preserving strong control of the family-wise error rate.
		  
                    lastAdj = adj;
                    activeStrategies.erase(S);
                }
                else
                {
                    // Failure ⇒ all remaining strategies inherit same p‑value.
                    for (auto& R : activeStrategies)
		      pvals[R] = adj;
                    break;
                }
            }
            return pvals;
	}
    };
}
