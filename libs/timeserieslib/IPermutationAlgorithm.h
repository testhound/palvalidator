/**************************************************************************************
 *  IPermutationAlgorithm.h
 *
 *  Purpose
 *  -------
 *  Defines the **strategy–permutation algorithm interface** used by
 *  `PALMasterMonteCarloValidation`.  A permutation algorithm receives the pre‑computed
 *  baseline statistics for every candidate trading strategy and must return a p‑value
 *  (adjusted for multiple testing) for each of them, *in a single call*.
 *
 *  Why an interface?
 *    • Open/Closed principle – new algorithms can be added without editing validator.
 *    • Unit‑testing – mock implementations can be injected to shortcut heavy back‑tests.
 *    • Dependency control – callers that only need the abstraction include this tiny
 *      header instead of the heavy concrete ones.
 *
 *  Usage pattern
 *  -------------
 *      using Algo = FastPermutation<double, ProfitFactorStat>;
 *      auto algo = std::make_unique<Algo>();
 *      std::map<StrategyPtr, double> pvals =
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
 *    • Must be **stateless** between invocations.  All per‑run state lives on the stack.
 *    • Must **not** modify `strategyData`.
 *    • Return value must contain **exactly** the same set of StrategyPtr keys as appear
 *      in `strategyData`.
 *    • Each returned p‑value must lie in the closed interval [0, 1].
 *    • The algorithm is responsible for enforcing *monotonicity* of adjusted p‑values
 *      when its statistical method requires it (Masters step‑down procedure).
 *
 *  See also
 *    • OriginalSlowPermutation.h
 *    • FastPermutation.h
 *************************************************************************************/

#pragma once
#include <map>
#include <memory>
#include <vector>

// Forward declarations only – keeps this header lightweight.
namespace mkc_timeseries
{
    template<class Decimal> class PalStrategy;
    template<class Decimal> class BackTester;
    template<class Decimal> class Portfolio;
    template<class Decimal> struct StrategyContext;

    /**
     * @brief  Stateless interface for a step‑wise permutation algorithm.
     */
    template<class Decimal, class BaselineStatPolicy>
    class IPermutationAlgorithm
    {
    public:
        using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>;
        using StrategyVec = std::vector<StrategyContext<Decimal>>;

        virtual ~IPermutationAlgorithm() = default;

        /**
         * @brief  Compute adjusted p‑values for all candidate strategies.
         *
         * @param strategyData      Baseline statistics for each strategy, sorted
         *                          in decreasing performance order (best first).
         * @param numPermutations   Number of random permutations to generate for
         *                          null‑distribution estimation.
         * @param templateBackTester  Back‑tester pre‑configured with the correct
         *                          timeframe and OOS date range.
         * @param portfolio         Portfolio object used by strategy clones.
         * @param sigLevel          Significance level α used by step‑down logic.
         *
         * @return  Map from StrategyPtr → adjusted p‑value.
         */
        virtual std::map<StrategyPtr, Decimal> run(
            const StrategyVec&                          strategyData,
            unsigned long                               numPermutations,
            const std::shared_ptr<BackTester<Decimal>>& templateBackTester,
            const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
            const Decimal&                              sigLevel) = 0;
    };
} // namespace mkc_timeseries
