// Refactored and formatted in Allman C++ style
#ifndef __PAL_MASTER_MONTE_CARLO_VALIDATION_H
#define __PAL_MASTER_MONTE_CARLO_VALIDATION_H 1

#include <string>
#include <vector>
#include <list>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

#include <boost/date_time.hpp>
#include <boost/thread/mutex.hpp>

#include "number.h"
#include "DecimalConstants.h"
#include "McptConfiguration.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PriceActionLabSystem.h"
#include "Portfolio.h"
#include "Security.h"
#include "TimeSeries.h"
#include "runner.hpp"
#include "MastersPermutationTestComputationPolicy.h"
#include "MultipleTestingCorrection.h"
#include "PALMonteCarloTypes.h"

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::shared_ptr;
  using std::make_shared;
  using std::vector;
  using std::tuple;
  using std::map;
  using std::unordered_set;

  class PALMasterMonteCarloValidationException : public std::runtime_error
  {
  public:
    PALMasterMonteCarloValidationException(const std::string msg)
      : std::runtime_error(msg) {}

    ~PALMasterMonteCarloValidationException() noexcept = default;
  };

  // Algorithm Type Enum (as provided)
    enum class MasterAlgorithmType 
      {
        OriginalSlow,
        Fast
      };

  // -----------------------------------
  // This class implements a stepwise permutation test for selection bias in trading system development,
  // based on Timothy Masters' algorithm described in "Permutation and Randomization Tests for Trading System Development".
  // The algorithm is designed to control the Familywise Error Rate (FWE) with strong control and improved statistical power,
  // inspired by the stepdown multiple testing procedure of Romano and Wolf (2016).
  //
  // Key features of this implementation:
  // - Computes baseline statistics for all candidate trading strategies using a customizable BaselineStatPolicy.
  // - Executes a stepwise permutation test in which strategies are tested in order of decreasing baseline performance.
  // - At each step, null distributions are constructed using only the remaining (unrejected) strategies, improving power.
  // - Adjusted p-values are calculated in a way that preserves monotonicity and provides valid inference under multiple testing.
  //
  // Template Parameters:
  // - Decimal: Numeric type used throughout (e.g., double, mpfr, etc.)
  // - BaselineStatPolicy: A policy class providing a static method for computing the statistic to test (e.g., profit factor).
  //
  // Important Method: runPermutationTests()
  // ----------------------------------------
  // Performs the full stepwise permutation testing procedure:
  // 1. Calls prepareStrategyDataAndBaselines() to compute the baseline performance metric for each strategy.
  // 2. Sorts strategies in descending order of baseline statistic.
  // 3. Initializes a pool of active strategies and performs the following loop:
  //    a. For the current best strategy:
  //       - Generate a null distribution by computing the maximum statistic across permutations,
  //         but only over the remaining (active) strategies.
  //       - Calculate a p-value by comparing the real statistic to this null distribution.
  //       - Adjust this p-value to ensure non-decreasing p-values across steps (monotonicity).
  //    b. If the adjusted p-value is <= alpha:
  //       - The strategy is accepted (null hypothesis rejected).
  //       - It is removed from the active strategy pool.
  //       - Continue testing the next-best strategy.
  //    c. If the adjusted p-value > alpha:
  //       - The test stops.
  //       - All remaining strategies are assigned this p-value.
  // 4. The final result is stored in mStrategySelectionPolicy, including adjusted p-values for all strategies.
  //
  // This stepwise procedure avoids the conservative bias of traditional max-statistic permutation tests by
  // narrowing the null hypothesis distribution as strategies are confirmed, increasing the chance of detecting
  // weaker but valid trading strategies while still controlling the overall error rate.
  //
  // -----------------------------------------------------------------------------------------

template <class Decimal, class BaselineStatPolicy>
    class PALMasterMonteCarloValidation
    {
    public:
        // Use types from PALMonteCarloTypes.h
        using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>; // Convenience alias
        using StrategyContextType = StrategyContext<Decimal>;
        using StrategyDataContainerType = StrategyDataContainer<Decimal>;

        // Alias for result iterator
        using SurvivingStrategiesIterator = typename UnadjustedPValueStrategySelection<Decimal>::surviving_const_iterator;

        // Constructor with algorithm selection
        PALMasterMonteCarloValidation(shared_ptr<McptConfiguration<Decimal>> configuration,
                                      unsigned long numPermutations,
                                      MasterAlgorithmType algoType = MasterAlgorithmType::Fast)
            : mMonteCarloConfiguration(configuration),
              mNumPermutations(numPermutations),
              mStrategySelectionPolicy(), // Default initialize selection policy
              mAlgorithmType(algoType)    // Store algorithm choice
        {
            if (!configuration) {
                throw PALMasterMonteCarloValidationException("McptConfiguration cannot be null.");
            }
            if (numPermutations == 0) {
                throw PALMasterMonteCarloValidationException("Number of permutations cannot be zero.");
            }
            // mStrategyData is default initialized (empty vector)
        }

        // Default copy/move constructors/assignment (as provided)
        PALMasterMonteCarloValidation(const PALMasterMonteCarloValidation&) = default;
        PALMasterMonteCarloValidation& operator=(const PALMasterMonteCarloValidation&) = default;
        PALMasterMonteCarloValidation(PALMasterMonteCarloValidation&&) noexcept = default;
        PALMasterMonteCarloValidation& operator=(PALMasterMonteCarloValidation&&) noexcept = default;

        // Virtual destructor (as provided)
        virtual ~PALMasterMonteCarloValidation() = default;

        // --- Public methods for results ---
        SurvivingStrategiesIterator beginSurvivingStrategies() const {
            return mStrategySelectionPolicy.beginSurvivingStrategies();
        }
        SurvivingStrategiesIterator endSurvivingStrategies() const {
            return mStrategySelectionPolicy.endSurvivingStrategies();
        }
        unsigned long getNumSurvivingStrategies() const {
            // Ensure getNumSurvivingStrategies returns a type convertible to unsigned long
            return static_cast<unsigned long>(mStrategySelectionPolicy.getNumSurvivingStrategies());
        }

        // --- Main Public Method: runPermutationTests (Dispatcher) ---
        void runPermutationTests()
        {
            prepareStrategyDataAndBaselines(); // Populates mStrategyData
            if (mStrategyData.empty()) {
                std::cout << "No strategies found for permutation testing." << std::endl;
                mStrategySelectionPolicy.clear();
                return;
            }

            // Sort DESCENDING by baselineStat (best first)
            std::sort(mStrategyData.begin(), mStrategyData.end(),
                      [](const StrategyContextType& a, const StrategyContextType& b) {
                          return a.baselineStat > b.baselineStat;
                      });

            // --- Common Setup ---
            auto baseSecurity = mMonteCarloConfiguration->getSecurity();
             if (!baseSecurity) { throw PALMasterMonteCarloValidationException("Base security missing in runPermutationTests setup."); }
            auto dateRange = mMonteCarloConfiguration->getOosDateRange(); // Assuming this returns a valid DateRange

            // Ensure BackTesterFactory and its dependencies (like TimeFrame) are available
            auto templateBackTester = BackTesterFactory::getBackTester(
                                         baseSecurity->getTimeSeries()->getTimeFrame(), // Ensure getTimeSeries() and getTimeFrame() exist
                                         dateRange.getFirstDate(), dateRange.getLastDate()); // Ensure DateRange methods exist
             if (!templateBackTester) { throw PALMasterMonteCarloValidationException("Failed to create template backtester."); }

            auto portfolio = std::make_shared<Portfolio<Decimal>>("PermutationPortfolio");
            portfolio->addSecurity(baseSecurity->clone(baseSecurity->getTimeSeries())); // Ensure clone and getTimeSeries exist

            // Determine Significance Level (Alpha)
            Decimal sigLevel = DecimalConstants<Decimal>::SignificantPValue; // Or get from config

            mStrategySelectionPolicy.clear(); // Reset results

            map<StrategyPtr, Decimal> pvalMap;

            // --- Dispatch to the appropriate private algorithm method ---
            if (mAlgorithmType == MasterAlgorithmType::OriginalSlow) {
                pvalMap = runOriginalSlowAlgorithm(templateBackTester, baseSecurity, portfolio, sigLevel);
            } else { // Fast algorithm
                pvalMap = runFastAlgorithm(templateBackTester, baseSecurity, portfolio, sigLevel);
            }

            // --- Final Policy Population (Common) ---
            for (const auto& entry : mStrategyData) {
                Decimal finalPval = DecimalConstants<Decimal>::One; // Default p-value
                auto it = pvalMap.find(entry.strategy);
                if (it != pvalMap.end()) {
                    finalPval = it->second;
                } else {
                     std::cerr << "Warning: Final p-value not found for strategy "
                               << entry.strategy->getStrategyName() << ", assigning 1.0" << std::endl;
                }
                // Ensure UnadjustedPValueStrategySelection methods exist
                mStrategySelectionPolicy.addStrategy(finalPval, entry.strategy);
            }
            mStrategySelectionPolicy.selectSurvivors(sigLevel);
        }


    protected:
        // --- Protected Helper Methods (Keep implementations as provided) ---

        // Ensure PALPatternPtr is defined (likely via PriceActionLabSystem.h or similar)
        shared_ptr<PalStrategy<Decimal>> createStrategyFromPattern(
            const PALPatternPtr& pattern, const std::string& strategyName, shared_ptr<Portfolio<Decimal>> portfolio)
        {
            // Implementation assumes pattern->isLongPattern() exists
            // And PalLongStrategy/PalShortStrategy constructors are correct
             return pattern->isLongPattern()
                 ? std::make_shared<PalLongStrategy<Decimal>>(strategyName, pattern, portfolio)
                 : std::make_shared<PalShortStrategy<Decimal>>(strategyName, pattern, portfolio);
        }

        // Ensure DateRange, BackTesterFactory, BaselineStatPolicy requirements are met
        Decimal runSingleBacktest(
            shared_ptr<PalStrategy<Decimal>> strategy, TimeFrame::Duration timeframe, const DateRange& range)
        {
            auto backTester = BackTesterFactory::getBackTester(timeframe, range.getFirstDate(), range.getLastDate());
            backTester->addStrategy(strategy);
            backTester->backtest();
            return BaselineStatPolicy::getPermutationTestStatistic(backTester); // Assumes static method exists
        }

        // Ensure McptConfiguration, PriceActionLabSystem, runner, mutex types/methods exist
        void prepareStrategyDataAndBaselines()
        {
             mStrategyData.clear();
             auto baseSecurity = mMonteCarloConfiguration->getSecurity();
             auto patternsToTest = mMonteCarloConfiguration->getPricePatterns(); // Needs PriceActionLabSystem?
             auto oosDates = mMonteCarloConfiguration->getOosDateRange();

             if (!baseSecurity) { throw PALMasterMonteCarloValidationException("Base security not loaded."); }
             if (!patternsToTest) { throw PALMasterMonteCarloValidationException("Price patterns not loaded."); }

             auto timeFrame = baseSecurity->getTimeSeries()->getTimeFrame();
             // Ensure FilterTimeSeries, OHLCTimeSeries exist and work as expected
             auto oosTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(FilterTimeSeries<Decimal>(*baseSecurity->getTimeSeries(), oosDates));
             auto securityToTest = baseSecurity->clone(oosTimeSeries);
             securityToTest->getTimeSeries()->syncronizeMapAndArray(); // Ensure method exists

             auto portfolio = std::make_shared<Portfolio<Decimal>>(securityToTest->getName() + " OOS Portfolio");
             portfolio->addSecurity(securityToTest);

             vector<std::function<void()>> tasks;
             boost::mutex dataMutex; // Consider std::mutex if Boost is not strictly needed elsewhere
             unsigned long strategyNumber = 1;

             // Ensure patternsToTest has allPatternsBegin/End iterators yielding PALPatternPtr
             for (auto it = patternsToTest->allPatternsBegin(); it != patternsToTest->allPatternsEnd(); ++it, ++strategyNumber) {
                 auto pattern = *it;
                 std::string name = (pattern->isLongPattern() ? "PAL Long " : "PAL Short ") + std::to_string(strategyNumber);
                 auto strategy = createStrategyFromPattern(pattern, name, portfolio);

                 tasks.emplace_back([this, strategy, timeFrame, oosDates, &dataMutex]() { // Check captures
                     try {
                         Decimal stat = runSingleBacktest(strategy, timeFrame, oosDates);
                         boost::mutex::scoped_lock lock(dataMutex); // Use std::lock_guard<std::mutex> if using std::mutex
                         // Use the correct type StrategyContextType from the new header
                         mStrategyData.push_back(StrategyContextType{ strategy, stat, 1 });
                     } catch (const std::exception& e) {
                         std::cerr << "Baseline error for strategy " << strategy->getStrategyName() << ": " << e.what() << std::endl;
                     }
                 });
             }

             // Ensure runner::instance() and post() exist and return futures
             runner& Runner = runner::instance();
             vector<boost::unique_future<void>> futures; // Check future type if runner changes
             for (auto& task : tasks) {
                 futures.emplace_back(Runner.post(std::move(task)));
             }
             for (size_t i = 0; i < futures.size(); ++i) { // Handle exceptions from futures
                 try {
                     futures[i].wait(); // Or futures[i].get(); which waits and throws
                     futures[i].get();
                 } catch (const std::exception& e) {
                     std::cerr << "Task " << i << " failed: " << e.what() << std::endl;
                 } catch (...) {
                     std::cerr << "Unknown error in task " << i << std::endl;
                 }
             }
        }

        // --- Protected Member variables ---
        shared_ptr<McptConfiguration<Decimal>> mMonteCarloConfiguration;
        unsigned long mNumPermutations;
        StrategyDataContainerType mStrategyData; // Uses type from PALMonteCarloTypes.h
        UnadjustedPValueStrategySelection<Decimal> mStrategySelectionPolicy;
        MasterAlgorithmType mAlgorithmType;


    // --- Private Helper Methods for Algorithms ---
    private:
        // Helper method for the original (slow) algorithm
        std::map<StrategyPtr, Decimal> runOriginalSlowAlgorithm(
            const std::shared_ptr<BackTester<Decimal>>& templateBackTester,
            const std::shared_ptr<Security<Decimal>>& baseSecurity,
            const std::shared_ptr<Portfolio<Decimal>>& portfolio,
            const Decimal& sigLevel)
        {
             std::cout << "Running Original Slow Masters Algorithm..." << std::endl;
             map<StrategyPtr, Decimal> pvalMap;
             Decimal lastAdjPval = DecimalConstants<Decimal>::Zero;
             std::unordered_set<StrategyPtr> active_strategies_set;
             for (const auto& entry : mStrategyData) { active_strategies_set.insert(entry.strategy); }

             for (size_t k = 0; k < mStrategyData.size(); ++k) {
                 const auto& entry = mStrategyData[k]; // entry is StrategyContextType
                 auto strategy = entry.strategy;

                 if (active_strategies_set.find(strategy) == active_strategies_set.end()) {
                     if (pvalMap.find(strategy) == pvalMap.end()) { pvalMap[strategy] = lastAdjPval; } // Assign last known pval
                     continue;
                 }

                 std::vector<StrategyPtr> active_strategies_vec(active_strategies_set.begin(), active_strategies_set.end());

                 using MPP = MasterPermutationPolicy<Decimal, BaselineStatPolicy>;
                 unsigned int count = MPP::computePermutationCountForStep(
                                            mNumPermutations, entry.baselineStat, active_strategies_vec,
                                            templateBackTester, baseSecurity, portfolio);

                 Decimal pval = static_cast<Decimal>(count) / static_cast<Decimal>(mNumPermutations + 1);
                 Decimal adjPval = std::max(pval, lastAdjPval);
                 pvalMap[strategy] = adjPval;

                 if (adjPval <= sigLevel) {
                     lastAdjPval = adjPval;
                     active_strategies_set.erase(strategy);
                 } else {
                     Decimal failingPval = adjPval;
                     for(const auto& active_strat_ptr : active_strategies_set) { pvalMap[active_strat_ptr] = failingPval; }
                     for (size_t j = k + 1; j < mStrategyData.size(); ++j) {
                         if (pvalMap.find(mStrategyData[j].strategy) == pvalMap.end()) { pvalMap[mStrategyData[j].strategy] = failingPval;}
                     }
                     break;
                 }

                 if (active_strategies_set.empty() && k < mStrategyData.size() - 1) {
                      for (size_t j = k + 1; j < mStrategyData.size(); ++j) {
                         if (pvalMap.find(mStrategyData[j].strategy) == pvalMap.end()) { pvalMap[mStrategyData[j].strategy] = lastAdjPval; }
                      }
                      break;
                 }
             } // End loop k

             // Final check
             for(const auto& entry : mStrategyData) {
                 if (pvalMap.find(entry.strategy) == pvalMap.end()) {
                     std::cerr << "Warning: Strategy " << entry.strategy->getStrategyName() << " missed p-value (slow), setting to 1.0" << std::endl;
                     pvalMap[entry.strategy] = DecimalConstants<Decimal>::One;
                 }
             }
             return pvalMap;
        } // End runOriginalSlowAlgorithm


        // Helper method for the fast algorithm
        std::map<StrategyPtr, Decimal> runFastAlgorithm(
            const std::shared_ptr<BackTester<Decimal>>& templateBackTester,
            const std::shared_ptr<Security<Decimal>>& baseSecurity,
            const std::shared_ptr<Portfolio<Decimal>>& portfolio,
            const Decimal& sigLevel)
        {
             std::cout << "Running Fast Masters Algorithm..." << std::endl;
             map<StrategyPtr, Decimal> pvalMap;
             Decimal lastAdjPval = DecimalConstants<Decimal>::Zero;

             // Step 1: Call Fast Policy
             using FMPP = FastMastersPermutationPolicy<Decimal, BaselineStatPolicy>;
             // Pass mStrategyData (which is StrategyDataContainerType)
             map<StrategyPtr, unsigned int> counts = FMPP::computeAllPermutationCounts(
                                                        mNumPermutations, mStrategyData, templateBackTester,
                                                        baseSecurity, portfolio);

             // Step 2: Stepwise Inclusion
             for (size_t k = 0; k < mStrategyData.size(); ++k) {
                 const auto& entry = mStrategyData[k]; // entry is StrategyContextType
                 auto strategy = entry.strategy;
                 unsigned int count_k = mNumPermutations + 1;

                 auto count_it = counts.find(strategy);
                 if (count_it != counts.end()) { count_k = count_it->second; }
                 else { std::cerr << "Warning: Count not found (fast) for " << strategy->getStrategyName() << std::endl; }

                 Decimal pval = static_cast<Decimal>(count_k) / static_cast<Decimal>(mNumPermutations + 1);
                 Decimal adjPval = std::max(pval, lastAdjPval);
                 pvalMap[strategy] = adjPval;

                 if (adjPval <= sigLevel) {
                     lastAdjPval = adjPval;
                 } else {
                     Decimal failingPval = adjPval;
                     for (size_t j = k + 1; j < mStrategyData.size(); ++j) {
                         if (pvalMap.find(mStrategyData[j].strategy) == pvalMap.end()) { pvalMap[mStrategyData[j].strategy] = failingPval; }
                     }
                     break;
                 }
             } // End loop k

            // Final check
             for(const auto& entry : mStrategyData) {
                 if (pvalMap.find(entry.strategy) == pvalMap.end()) {
                      std::cerr << "Warning: Strategy " << entry.strategy->getStrategyName() << " missed p-value (fast), setting to 1.0" << std::endl;
                      pvalMap[entry.strategy] = DecimalConstants<Decimal>::One;
                 }
             }
             return pvalMap;
        } // End runFastAlgorithm

    }; // End class
} // namespace mkc_timeseries

#endif // __PAL_MASTER_MONTE_CARLO_VALIDATION_H
