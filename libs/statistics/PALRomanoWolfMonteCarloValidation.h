#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <limits>
#include <atomic>

// Core MKC Timeseries Headers
#include "number.h"
#include "DecimalConstants.h"
#include "Security.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "Portfolio.h"
#include "TimeSeries.h"

// Helper and Policy Headers
#include "PALMonteCarloTypes.h"
#include "StrategyDataPreparer.h"
#include "MultipleTestingCorrection.h"
#include "SyntheticSecurityHelpers.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"


namespace mkc_timeseries
{
  /**
   * @class PALRomanoWolfMonteCarloValidation
   * @brief Implements an efficient, batch-oriented stepwise permutation test based on Romano & Wolf (2016).
   *
   * This class serves as a high-level orchestrator for running a multiple hypothesis test that
   * correctly implements the "efficient computation" methodology. It processes all candidate
   * strategies in a single, unified permutation block, avoiding the inefficiency of re-running
   * permutations for each individual strategy.
   *
   * ## Core Architectural Changes
   * This implementation now tests LONG and SHORT strategies as two separate families to avoid
   * contaminating the null distributions and to increase statistical power.
   * The process is as follows:
   *
   * 1.  **Partition Strategies**: All strategies are prepared and then separated into a "longs"
   * family and a "shorts" family.
   * 2.  **Run Test Per-Family**: A dedicated helper function runs the full 3-stage test on each family:
   * a. **Efficient Permutation Stage**: Run a single block of N permutations for the family.
   * b. **Exceedance Count Stage ("Worst-to-Best" Pass)**: Calculate exceedance counts.
   * c. **P-Value Adjustment Stage ("Best-to-Worst" Pass)**: Adjust p-values for monotonicity.
   * 3.  **Combine Results**: The p-values from both tests are merged, and the final list of
   * survivors is determined from the combined results.
   *
   * @tparam Decimal The numeric type for calculations (e.g., double).
   * @tparam BaselineStatPolicy A policy class defining how to compute the performance statistic.
   * @tparam Executor A policy for parallel execution of the permutation loop.
   */
  template <class Decimal, class BaselineStatPolicy, class Executor = concurrency::ThreadPoolExecutor<>>
  class PALRomanoWolfMonteCarloValidation
  {
  public:
    // Type aliases for clarity, consistent with PALMastersMonteCarloValidation
    using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>;
    using StrategyContextType = StrategyContext<Decimal>;
    using StrategyDataContainerType = StrategyDataContainer<Decimal>;
    using SurvivingStrategiesIterator = typename UnadjustedPValueStrategySelection<Decimal>::ConstSurvivingStrategiesIterator;

    explicit PALRomanoWolfMonteCarloValidation(unsigned long numPermutations)
      : mNumPermutations(numPermutations),
        mStrategySelectionPolicy()
    {
      if (mNumPermutations == 0)
        throw std::invalid_argument("Number of permutations must be greater than zero.");
    }

    ~PALRomanoWolfMonteCarloValidation() = default;

    void runPermutationTests(
        std::shared_ptr<Security<Decimal>> baseSecurity,
        std::shared_ptr<PriceActionLabSystem> patterns,
        const DateRange& dateRange,
        const Decimal& pValueSignificanceLevel = DecimalConstants<Decimal>::SignificantPValue,
        bool verbose = false)
    {
      if (!baseSecurity || !patterns)
        throw std::invalid_argument("PALRomanoWolfMonteCarloValidation::runPermutationTests - baseSecurity and patterns must not be null.");

      if (verbose)
        std::cout << "Starting separate Long/Short Romano-Wolf validation..." << std::endl;

      // Prepare baseline stats for ALL strategies first
      auto templateBackTester = BackTesterFactory<Decimal>::getBackTester(baseSecurity->getTimeSeries()->getTimeFrame(), dateRange);
      StrategyDataContainerType allStrategyData = StrategyDataPreparer<Decimal, BaselineStatPolicy>::prepare(templateBackTester, baseSecurity, patterns);
      
      if (allStrategyData.empty())
      {
        if (verbose) std::cout << "No strategies to test. Exiting." << std::endl;
        return;
      }

      // Partition strategies into LONG and SHORT families
      StrategyDataContainerType longStrategies, shortStrategies;
      for (const auto& context : allStrategyData)
      {
        if (context.strategy->isLongStrategy())
          longStrategies.push_back(context);
        else if (context.strategy->isShortStrategy())
          shortStrategies.push_back(context);
      }

      if (verbose)
      {
        std::cout << "Partitioned strategies: " << longStrategies.size() << " Long, " 
                  << shortStrategies.size() << " Short." << std::endl;
      }

      // Run separate tests and collect p-values
      std::map<StrategyPtr, Decimal> finalPValues;

      if (!longStrategies.empty())
      {
        if (verbose) std::cout << "\n--- Testing LONG Strategy Family ---" << std::endl;
        auto longPValues = runTestForFamily(longStrategies, templateBackTester, baseSecurity, verbose);
        finalPValues.insert(longPValues.begin(), longPValues.end());
      }

      if (!shortStrategies.empty())
      {
        if (verbose) std::cout << "\n--- Testing SHORT Strategy Family ---" << std::endl;
        auto shortPValues = runTestForFamily(shortStrategies, templateBackTester, baseSecurity, verbose);
        finalPValues.insert(shortPValues.begin(), shortPValues.end());
      }
      
      // Populate final results policy with combined p-values from both tests
      mStrategySelectionPolicy.clearForNewTest();
      for (const auto& strategyContext : allStrategyData)
      {
          // It's possible a strategy has no p-value if its family was empty
          if (finalPValues.count(strategyContext.strategy))
          {
            mStrategySelectionPolicy.addStrategy(finalPValues.at(strategyContext.strategy), strategyContext.strategy);
          }
      }
      mStrategySelectionPolicy.correctForMultipleTests(pValueSignificanceLevel);

      if (verbose)
        std::cout << "\nCombined validation complete. Found " << getNumSurvivingStrategies() << " total surviving strategies." << std::endl;
    }

    // --- Accessor methods to retrieve results ---

    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.beginSurvivingStrategies();
    }

    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.endSurvivingStrategies();
    }

    unsigned long getNumSurvivingStrategies() const
    {
      return static_cast<unsigned long>(mStrategySelectionPolicy.getNumSurvivingStrategies());
    }
    
    std::vector<std::pair<StrategyPtr, Decimal>> getAllTestedStrategies() const
    {
      return mStrategySelectionPolicy.getAllTestedStrategies();
    }

    Decimal getStrategyPValue(StrategyPtr strategy) const
    {
      return mStrategySelectionPolicy.getStrategyPValue(strategy);
    }

  private:
    /**
     * @brief Private helper to run the 3-stage Romano-Wolf test on a single family of strategies.
     * @param strategyFamily A container with strategies of the same type (all long or all short).
     * @param templateBackTester A template backtester instance to clone.
     * @param baseSecurity The security to test against.
     * @param verbose Flag to enable detailed logging.
     * @return A map of strategy pointers to their final adjusted p-values for this family.
     */
    std::map<StrategyPtr, Decimal> runTestForFamily(
        const StrategyDataContainerType& strategyFamily,
        std::shared_ptr<BackTester<Decimal>> templateBackTester,
        std::shared_ptr<Security<Decimal>> baseSecurity,
        bool verbose)
    {
      // The incoming strategyFamily is a subset of the already-prepared data.
      // We just need to sort this specific family.
      StrategyDataContainerType sortedStrategyData = strategyFamily;
      std::sort(sortedStrategyData.begin(), sortedStrategyData.end(),
                [](const StrategyContextType& a, const StrategyContextType& b) {
                  return a.baselineStat > b.baselineStat; // Sort descending (best first)
                });
      
      size_t numStrategies = sortedStrategyData.size();
      if (verbose)
        std::cout << "Starting test for " << numStrategies << " strategies in this family." << std::endl;
      
      // STAGE 1: EFFICIENT PERMUTATION
      if (verbose) std::cout << "  Stage 1: Running " << mNumPermutations << " permutations..." << std::endl;

      std::vector<std::vector<Decimal>> permutedStats(mNumPermutations, std::vector<Decimal>(numStrategies));
      Executor executor;
      
      auto basePortfolio = std::make_shared<Portfolio<Decimal>>(baseSecurity->getName() + " Base Portfolio");
      basePortfolio->addSecurity(baseSecurity);

      concurrency::parallel_for(mNumPermutations, executor,
        [&](uint32_t p) {
          auto syntheticPortfolio = createSyntheticPortfolio<Decimal>(baseSecurity, basePortfolio);
          for (size_t s = 0; s < numStrategies; ++s)
          {
            const auto& strategyContext = sortedStrategyData[s];
            auto clonedStrat = strategyContext.strategy->clone(syntheticPortfolio);
            auto btClone = templateBackTester->clone();
            btClone->addStrategy(clonedStrat);
            btClone->backtest();
            permutedStats[p][s] = BaselineStatPolicy::getPermutationTestStatistic(btClone);
          }
        });
      
      // STAGE 2: EXCEEDANCE COUNT ("Worst-to-Best" Pass)
      if (verbose) std::cout << "  Stage 2: Calculating exceedance counts..." << std::endl;

      std::vector<unsigned int> exceedanceCounts(numStrategies, 1);
      for (uint32_t p = 0; p < mNumPermutations; ++p)
      {
        Decimal max_f_so_far = std::numeric_limits<Decimal>::lowest();
        for (int s = numStrategies - 1; s >= 0; --s)
        {
          max_f_so_far = std::max(max_f_so_far, permutedStats[p][s]);
          if (max_f_so_far >= sortedStrategyData[s].baselineStat)
          {
            exceedanceCounts[s]++;
          }
        }
      }

      // STAGE 3: P-VALUE ADJUSTMENT ("Best-to-Worst" Pass)
      if (verbose) std::cout << "  Stage 3: Adjusting p-values..." << std::endl;
      
      std::map<StrategyPtr, Decimal> familyPValues;
      Decimal last_p_adj = DecimalConstants<Decimal>::DecimalZero;

      for (size_t s = 0; s < numStrategies; ++s)
      {
        const auto& strategyContext = sortedStrategyData[s];
        Decimal p_raw = static_cast<Decimal>(exceedanceCounts[s]) / static_cast<Decimal>(mNumPermutations + 1);
        Decimal p_final = std::max(p_raw, last_p_adj);
        familyPValues[strategyContext.strategy] = p_final;
        last_p_adj = p_final;
      }
      
      if (verbose) std::cout << "Test for this family complete." << std::endl;
      return familyPValues;
    }

  private:
    unsigned long mNumPermutations;
    UnadjustedPValueStrategySelection<Decimal> mStrategySelectionPolicy;
  };
} // namespace mkc_timeseries
