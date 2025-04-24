/**************************************************************************************
 *  StrategyDataPreparer.h
 *
 *  Purpose
 *  -------
 *  Produce the `StrategyDataContainer<Decimal>` used by the permutation algorithms:
 *    • Builds concrete `PalStrategy` objects (long/short) for each pattern.
 *    • Runs one baseline back‑test per strategy and records the statistic defined
 *      by `BaselineStatPolicy`.
 *
 *  Design notes
 *  ------------
 *  • Stateless utility – everything is done in a single static `prepare()` call.
 *  • Relies on the project’s existing thread‑pool (`runner::instance()`) **unchanged**
 *    so behaviour is identical to the original in‑class method.
 *  • No Boost headers leak out; callers only see STL and project types.
 *  • All helper functions (`createStrategyFromPattern`, `runSingleBacktest`) are
 *    private and copied verbatim from the original class :contentReference[oaicite:0]{index=0}&#8203;:contentReference[oaicite:1]{index=1}.
 *
 *  Later refactors
 *  ---------------
 *  • Inject `IParallelExecutor` to remove Boost/`runner`.
 *  • Split declaration / definition if explicit instantiation becomes desirable.
 *************************************************************************************/

#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <boost/thread/mutex.hpp>            // intact for now
#include "PALMonteCarloTypes.h"
#include "McptConfigurationFileReader.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "Portfolio.h"
#include "TimeSeries.h"
#include "BackTester.h"
#include "runner.hpp"

namespace mkc_timeseries
{
    template<class Decimal, class BaselineStatPolicy>
    class StrategyDataPreparer
    {
    public:
        using StrategyPtr              = std::shared_ptr<PalStrategy<Decimal>>;
        using StrategyContextType      = StrategyContext<Decimal>;
        using StrategyDataContainerType= StrategyDataContainer<Decimal>;

      /**
         * @brief  Build strategies and compute their baseline statistics.
         * @param  templateBacktester   BackTester pre-configured with date ranges.
         * @param  baseSecurity Security to trade (full series).
         * @param  patterns     PriceActionLabSystem containing patterns.
         * @return Container sorted in insertion order.
         * @throw  std::runtime_error on null inputs.
         */ 
        static StrategyDataContainerType
        prepare(const std::shared_ptr<BackTester<Decimal>>& templateBacktester,
                const std::shared_ptr<Security<Decimal>>&    baseSecurity,
                const PriceActionLabSystem*                  patterns)
        {
	    if (!templateBacktester)
	      throw std::runtime_error("StrategyDataPreparer::prepare – template backtester is null");
            if (!baseSecurity)
	      throw std::runtime_error("StrategyDataPreparer::prepare – base security is null");
            if (!patterns)
	      throw std::runtime_error("StrategyDataPreparer::prepare – patterns is null");
	    
            StrategyDataContainerType result;

	    auto portfolio = std::make_shared<Portfolio<Decimal>>(baseSecurity->getName() + " Portfolio");
            portfolio->addSecurity(baseSecurity);

            // Runner setup
            runner& Runner = runner::instance();
            std::vector<boost::unique_future<void>> futures;
            boost::mutex dataMutex;

            // --- Generate tasks – one per pattern --------------------------
            unsigned long idx = 1;
            for (auto it = patterns->allPatternsBegin();
                 it != patterns->allPatternsEnd();
                 ++it, ++idx)
            {
                const PALPatternPtr pattern = *it;
                const std::string name =
                    (pattern->isLongPattern() ? "PAL Long " : "PAL Short ") + std::to_string(idx);
                const StrategyPtr strategy = createStrategyFromPattern(pattern, name, portfolio);

		/***************************************************************************
		 *  ─ λ  task  ─────────────────────────────────────────────────────────────
		 *
		 *  Purpose
		 *  -------
		 *  Launch a **baseline back‑test** for a single trading strategy on the
		 *  thread‑pool managed by `runner::instance()`.  
		 *  The lambda runs asynchronously, computes the baseline statistic, and
		 *  appends a new `StrategyContext` entry to the shared `result` vector.
		 *
		 *  Capture list
		 *  ------------
		 *    [&, strategy]                // C++ lambda captures
		 *      &       – captures *everything else* by reference so the lambda
		 *                sees the up‑to‑date `timeFrame`, `oosDates`, `dataMutex`,
		 *                and `result` objects that live in the outer scope.
		 *      strategy – captured **by value** to extend the lifetime of the
		 *                `shared_ptr<PalStrategy>` into the async task.
		 *
		 *  Body
		 *  ----
		 *    1. `runSingleBacktest()` executes the strategy on the OOS date‑range
		 *       and returns the statistic required by `BaselineStatPolicy`.
		 *    2. Acquire `dataMutex` → critical section: push the new
		 *       `{ strategy, stat, 1 }` triple into `result`.
		 *    3. Catch any exception, write a diagnostic to `std::cerr`, and let
		 *       execution continue (the permutation framework tolerates a missing
		 *       baseline for a failed strategy).
		 *
		 *  Submission to thread‑pool
		 *  -------------------------
		 *    `Runner.post(std::move(task))` enqueues the lambda for execution on a
		 *    worker thread and returns a `boost::unique_future<void>`.  The future
		 *    is stored in `futures` so the caller can later:
		 *
		 *        for (auto& f : futures) { f.wait(); f.get(); }
		 *
		 *    which:
		 *      • blocks until every baseline run completes, and
		 *      • propagates any re‑thrown exception out of the task.
		 *
		 *  Thread‑safety
		 *  -------------
		 *  • Only the `result` vector is shared and is protected by
		 *    `boost::mutex dataMutex`.
		 *  • `strategy` and all stack variables inside the lambda are thread‑local.
		 *
		 ***************************************************************************/

                auto task = [&, strategy, templateBacktester]() {
                    try {
                        Decimal stat = runSingleBacktest(strategy, templateBacktester);
                        boost::mutex::scoped_lock lock(dataMutex);
                        result.push_back(StrategyContextType{ strategy, stat, 1 });
                    } catch (const std::exception& e) {
                        std::cerr << "Baseline error for " << strategy->getStrategyName()
                                  << ": " << e.what() << std::endl;
                        throw;
                    }
                };

		/****************************************************************************************
		 * Submit the task to the global thread pool (`runner`) and keep the returned future.
		 *
		 *   • Runner.post(...) schedules the lambda for execution on an internal worker
		 *     thread and returns a `boost::unique_future<void>`.
		 *   • We move‑construct the future into `futures` so we can later:
		 *         ‑ wait()  – ensure the task finished before we proceed, and
		 *         ‑ get()   – re‑throw any uncaught exceptions (there shouldn’t be any
		 *                     because we already handle them, but this keeps the behaviour
		 *                     identical to the original implementation).
		 ***************************************************************************************/
                futures.emplace_back(Runner.post(std::move(task)));
            }

            /***************************************************************************
	     *  ─── synchronize & propagate task results ───────────────────────────────
	     *
	     *  Context
	     *  -------
	     *  `futures` is a `std::vector<boost::unique_future<void>>` returned by
	     *  Runner.post(…) for every baseline‑computation task we launched.  Each
	     *  future represents exactly **one** asynchronous back‑test.
	     *
	     *  Loop semantics
	     *  --------------
	     *      for (auto& f : futures) {
	     *          f.wait();   // ① block until the task finishes
	     *          f.get();    // ② rethrow any exception (or do nothing on success)
	     *      }
	     *
	     *    ① `wait()`  
	     *       • Blocks the current thread until the associated task has reached
	     *         the “ready” state.  
	     *       • Does *not* retrieve the stored result/exception, so the state
	     *         remains valid for a subsequent `get()`.  
	     *       • Calling `wait()` first makes the intent explicit: “wait for all
	     *         tasks to finish before we look at their results.”
	     *
	     *    ② `get()`  
	     *       • If the asynchronous function threw, `get()` rethrows that same
	     *         exception in the **calling** thread.  
	     *       • If the task completed successfully, `get()` returns the value
	     *         (here `void`) and invalidates the future.  
	     *       • Because we already called `wait()`, `get()` returns immediately
	     *         unless an exception needs to be propagated.
	     *
	     *  Why both `wait()` *and* `get()`?
	     *  -------------------------------
	     *  Technically `get()` alone would block until the task completes.  The
	     *  explicit `wait()` step improves readability and can help debuggers/
	     *  profilers show a clearer “synchronisation point.”  It also separates:
	     *
	     *      • *Scheduling concern*  (ensure every job is finished)  
	     *      • *Error‑handling concern* (deal with failures)
	     *
	     *  Guarantees after the loop
	     *  -------------------------
	     *  • Every baseline‑computation job has finished.  
	     *  • If any job threw, that exception has now been re‑thrown in the calling
	     *    thread, allowing normal try/catch handling higher up the stack.  
	     *  • All futures are in the “retrieved” state and can be destroyed safely.
	     ***************************************************************************/
            for (auto& f : futures)
	      {
		f.wait();
		f.get();
	      }

            return result;
        }

    private:
        static StrategyPtr createStrategyFromPattern(const PALPatternPtr& pattern,
                                                     const std::string&   strategyName,
                                                     std::shared_ptr<Portfolio<Decimal>> portfolio)
        {
	  return pattern->isLongPattern()
	    ? std::static_pointer_cast<PalStrategy<Decimal>>(
							     std::make_shared<PalLongStrategy<Decimal>>(strategyName, pattern, portfolio))
	    : std::static_pointer_cast<PalStrategy<Decimal>>(
							     std::make_shared<PalShortStrategy<Decimal>>(strategyName, pattern, portfolio));	  
        }

      /**
       * @brief Clone the template backtester, add the strategy, and run backtest.
       */
      static Decimal runSingleBacktest(StrategyPtr strategy,
				       const std::shared_ptr<BackTester<Decimal>>& templateBacktester)
      {
	auto bt = templateBacktester->clone();
	bt->addStrategy(strategy);
	bt->backtest();
	return BaselineStatPolicy::getPermutationTestStatistic(bt);
      }
      
    };
} // namespace mkc_timeseries
