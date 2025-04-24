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

#pragma once
#include "IPermutationAlgorithm.h"
#include "MastersPermutationComputationPolicy.h"

namespace mkc_timeseries
{
    template<class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolfImproved final
        : public IPermutationAlgorithm<Decimal, BaselineStatPolicy>
    {
        using Base       = IPermutationAlgorithm<Decimal, BaselineStatPolicy>;
        using StrategyPtr= typename Base::StrategyPtr;
        using StrategyVec= typename Base::StrategyVec;

    public:
        std::map<StrategyPtr, Decimal> run(const StrategyVec&                strategyData,
                                           unsigned long                     numPermutations,
                                           const std::shared_ptr<BackTester<Decimal>>& tmplBT,
                                           const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
                                           const Decimal&                   sigLevel) override
        {
            using FMPP = FastMastersPermutationPolicy<Decimal, BaselineStatPolicy>;

            // Bulk compute exceedance counts for every strategy once.
            std::map<StrategyPtr, unsigned int> counts =
                FMPP::computeAllPermutationCounts(numPermutations,
                                                  strategyData,
                                                  tmplBT,
                                                  portfolio);

            std::map<StrategyPtr, Decimal> pvals;
            Decimal lastAdj = Decimal(0);

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
