#pragma once
#include <unordered_set>
#include "IPermutationAlgorithm.h"
#include "MastersPermutationTestComputationPolicy.h"

namespace mkc_timeseries
{
    template <class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolf : public IPermutationAlgorithm<Decimal, BaselineStatPolicy>
    {
      using Base  = IPermutationAlgorithm<Decimal, BaselineStatPolicy>;
      using Strat = typename Base::StrategyPtr;
      using Vec   = typename Base::StrategyVec;

    public:
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
                unsigned int exceedCount = MPP::computePermutationCountForStep(
                                                numPermutations,
                                                context.baselineStat,
                                                activeVec,
                                                templateBacktester,
                                                secPtr,
                                                portfolio);

                Decimal p   = Decimal(exceedCount) / Decimal(numPermutations + 1);
                Decimal adj = std::max(p, lastAdj);          // step‑down monotonicity
                pvals[S]    = adj;

                if (adj <= sigLevel)
                {
                    lastAdj = adj;               // tighter bound for next step
                    activeStrategies.erase(S);             // confirmed survivor
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
