#pragma once
#include <map>
#include <memory>
#include <vector>

namespace mkc_timeseries
{
    template<class Decimal> class PalStrategy;
    template<class Decimal> class BackTester;
    template<class Decimal> class Portfolio;
    template<class Decimal> struct StrategyContext;

    /**************************************************************************************
     *  IMastersSelectionBiasAlgorithm
     *
     *  Purpose
     *  -------
     *  Defines the **strategy–permutation algorithm interface** used by
     *  `PALMasterMonteCarloValidation`.  A permutation algorithm receives the pre‑computed
     *  baseline statistics for every candidate trading strategy and must return a p‑value
     *  (adjusted for multiple testing) for each of them, *in a single call*.
     *

     *  Usage pattern
     *  -------------
     *      using Algo = FastPermutation<double, ProfitFactorStat>;
     *      auto algo = std::make_unique<Algo>();
     *      std::map<unsigned long long, double> pvals =
     *          algo->run(strategies, 1000, tmplBackTester, portfolio, 0.05);
     *
     *  Template parameters
     *  -------------------
     *    @tparam Decimal            Numeric type (`double`, `long double`, mpfr…).
     *    @tparam BaselineStatPolicy Compile‑time policy that provides
     *                               `static Decimal getPermutationTestStatistic(BackTesterPtr)`.
     *
     *  Contract for implementers (`run()`)
     *  -----------------------------------
     *    • Must be **stateless** between invocations.  All per‑run state lives on the stack.
     *    • Must **not** modify `strategyData`.
     *    • Return value must contain **exactly** the same set of strategy hashes as appear
     *      in `strategyData` (obtained via strategy->getPatternHash()).
     *    • Each returned p‑value must lie in the closed interval [0, 1].
     *    • The algorithm is responsible for enforcing *monotonicity* of adjusted p‑values
     *      when its statistical method requires it (Masters step‑down procedure).
     *
     *  See also
     *    • OriginalSlowPermutation.h
     *    • FastPermutation.h
     *************************************************************************************/
    template <class Decimal, class BaselineStatPolicy>
    class IMastersSelectionBiasAlgorithm
    {
    public:
        using StrategyPtr  = std::shared_ptr<PalStrategy<Decimal>>;
        using StrategyVec  = std::vector<StrategyContext<Decimal>>;

        virtual ~IMastersSelectionBiasAlgorithm() = default;

        virtual std::map<unsigned long long, Decimal> run(
            const StrategyVec&                 strategyData,
            unsigned long                      numPermutations,
            const std::shared_ptr<BackTester<Decimal>>& templateBackTester,
            const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
            const Decimal&                     sigLevel) = 0;
    };
} // namespace mkc_timeseries
