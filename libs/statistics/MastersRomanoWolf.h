#pragma once
#include <stdexcept> // Required for std::invalid_argument
#include <unordered_set>
#include "IMastersSelectionBiasAlgorithm.h"
#include "MastersPermutationTestComputationPolicy.h"
#include "PermutationTestSubject.h"

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
     *   How This Fixes the "Two Annoying Weaknesses" of Romano and Wolf
     * 1. **Strong Control of FWE (Weakness #1)**
     *    - Traditional selection-bias tests require the joint null that *all* competitors are unrelated,
     *      yielding only weak control of family-wise error (valid only if no competitor has any real relationship).
     *    - By testing and removing each strategy one at a time, this stepwise approach provides *strong*
     *      control of FWE: it remains valid under any configuration of true and false null hypotheses.
     *
     * 2. **Improved Power & Exact p‑Values (Weakness #2)**
     *    - The classical max-statistic test builds its null by taking the maximum over *all* competitors,
     *      producing exact p-values only for the top scorer and conservative upper bounds for the rest.
     *    - Here, as each strategy is removed, the null distribution is *shrunk* (max over fewer competitors),
     *      yielding p-values that more closely match each competitor’s true null distribution and restoring power
     *      for “second‑best,” “third‑best,” etc., while still controlling the overall error rate.
     *
     * @tparam Decimal Numeric type for statistics (e.g., double, long double).
     * @tparam BaselineStatPolicy Policy providing:
     *         - static unsigned int getMinStrategyTrades(): minimum trades threshold.
     *         - static Decimal getPermutationTestStatistic(bt): statistic extraction.
     */
    template <class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolf : public IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>,
                              public PermutationTestSubject<Decimal>
    {
      using Base  = IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>;
      using Strat = typename Base::StrategyPtr;
      using Vec   = typename Base::StrategyVec;

    public:
      /**
       * @brief Execute the stepwise permutation test with strong FWE control.
       *
       * Precondition: `strategyData` **must** be sorted in **descending** order by
       *   `baselineStat` (highest first) before calling.
       * Implements Masters’ algorithm:
       *  - Sort strategies descending by their observed baseline statistic.
       *  - Iterate from best to worst (stepwise loop), computing a Monte Carlo null distribution
       *    of max-statistic over *remaining* competitors only, thus shrinking the null distribution
       *    at each step.
       *  - Estimate right-tail p-value = (# permuted max >= observed) / (m + 1).
       *  - Enforce monotonicity (step-down): p_adj[i] = max(p_i, p_adj[i-1]).
       *  - Stop early when adjusted p-value > alpha, assigning that p-value to all remaining.
       *
       * @param strategyData     Pre-sorted vector of StrategyContext (strategy + observed statistic).
       *                          **Precondition:** sorted descending by `baselineStat`
       * @param numPermutations   Number of Monte Carlo permutations (m > 0).
       * @param templateBacktester Prototype BackTester to clone for each permutation.
       * @param portfolio         Portfolio owning the target security (used to extract security ptr).
       * @param sigLevel          Desired familywise alpha level (e.g., 0.05).
       * @return Map from each strategy ptr to its adjusted p-value.
       * @throws std::runtime_error if portfolio empty.
       */
      std::map<unsigned long long, Decimal> run(const Vec& strategyData,
       unsigned long numPermutations,
       const std::shared_ptr<BackTester<Decimal>>& templateBacktester,
       const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
       const Decimal& sigLevel) override
        {
            using MPP = MastersPermutationPolicy<Decimal, BaselineStatPolicy>;

	    // Check the precondition and throw if violated
	    if (!std::is_sorted(strategyData.begin(), strategyData.end(),
				[](auto const& a, auto const& b) {
				  return a.baselineStat > b.baselineStat;
				}))
	      {
		const std::string error_message =
                    "MastersRomanoWolf::run requires strategyData to be "
                    "pre-sorted in descending order by baselineStat.";
		
		throw std::invalid_argument(error_message);
	      }

	    std::map<unsigned long long, Decimal> pvals;
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
	               auto strategyHash = S->getPatternHash();

	               // If previously resolved (happens when we fail early) just propagate.
	               if (!activeStrategies.count(S))
	               {
	                   pvals[strategyHash] = lastAdj;
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
                // Create instance of MastersPermutationPolicy for observer support
                MPP permutationPolicy;
                
                // Chain attached observers to the policy instance (pass-through Subject design)
                std::shared_lock<std::shared_mutex> observerLock(this->m_observersMutex);
                for (auto* observer : this->m_observers) {
                    if (observer) {
                        permutationPolicy.attach(observer);
                    }
                }
                observerLock.unlock();
                
                unsigned int exceedCount = permutationPolicy.computePermutationCountForStep(
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
		              pvals[strategyHash] = adj;

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
		    {
		      auto rHash = R->getPatternHash();
		      pvals[rHash] = adj;
		    }
		                  break;
		              }
            }
            return pvals;
	}
    };
}
