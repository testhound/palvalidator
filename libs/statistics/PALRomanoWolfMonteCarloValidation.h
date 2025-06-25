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
   * This implementation corrects the flawed strategy-by-strategy approach and follows the
   * efficient algorithm described in the literature (e.g., Masters' "fast" algorithm).
   * The process is as follows:
   *
   * 1.  **Prepare Baselines**: Compute the baseline performance statistic for all strategies on the
   * original, unpermuted data. Sort the strategies from best to worst.
   *
   * 2.  **Efficient Permutation Stage**: Run a single block of N permutations. In each permutation:
   * - A single synthetic (shuffled) dataset is created.
   * - ALL strategies are backtested against this *same* synthetic dataset.
   * - The results are stored in a matrix of permuted statistics (permutations x strategies).
   *
   * 3.  **Exceedance Count Stage ("Worst-to-Best" Pass)**: Using the matrix of permuted stats,
   * calculate the exceedance counts for each strategy. This pass iterates from the worst-performing
   * strategy to the best, updating a running maximum statistic for each permutation. This correctly
   * constructs the shrinking null distributions required for a powerful stepwise test.
   *
   * 4.  **P-Value Adjustment Stage ("Best-to-Worst" Pass)**: Convert the exceedance counts into
   * final, adjusted p-values. This pass iterates from best to worst, enforcing monotonicity
   * to ensure that p-values are non-decreasing.
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

    /**
     * @brief Constructor for the batch computation policy.
     * @param numPermutations The number of Monte Carlo permutations to execute.
     */
    explicit PALRomanoWolfMonteCarloValidation(unsigned long numPermutations)
      : mNumPermutations(numPermutations),
        mStrategySelectionPolicy()
    {
      if (mNumPermutations == 0)
        throw std::invalid_argument("Number of permutations must be greater than zero.");
    }

    ~PALRomanoWolfMonteCarloValidation() = default;

    /**
     * @brief The main orchestration method to run the entire efficient permutation test.
     *
     * This method follows the interface of PALMastersMonteCarloValidation but implements the
     * correct, efficient, batch-oriented algorithm.
     *
     * @param baseSecurity A shared pointer to the security for backtesting.
     * @param patterns A shared pointer to the PriceActionLabSystem containing all candidate patterns.
     * @param dateRange The date range for the backtests.
     * @param pValueSignificanceLevel The alpha level for determining final survivors.
     * @param verbose If true, prints progress and diagnostic information.
     */
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
	{
	  std::cout << "In PALRomanoWolfMonteCarloValidation::runPermutationTests" << std::endl;
	}

      // 1. DATA PREPARATION: Compute baseline stats and sort strategies from best to worst.
      // This part is similar to the original orchestrator.
      auto templateBackTester = BackTesterFactory<Decimal>::getBackTester(baseSecurity->getTimeSeries()->getTimeFrame(), dateRange);
      StrategyDataContainerType sortedStrategyData = StrategyDataPreparer<Decimal, BaselineStatPolicy>::prepare(templateBackTester, baseSecurity, patterns);
      
      if (sortedStrategyData.empty())
      {
        if (verbose) std::cout << "No strategies to test. Exiting." << std::endl;
        return;
      }

      std::sort(sortedStrategyData.begin(), sortedStrategyData.end(),
                [](const StrategyContextType& a, const StrategyContextType& b) {
                  return a.baselineStat > b.baselineStat; // Sort descending (best first)
                });
      
      size_t numStrategies = sortedStrategyData.size();
      if (verbose)
        std::cout << "Prepared and sorted " << numStrategies << " strategies based on baseline performance." << std::endl;
      
      // 2. STAGE 1: EFFICIENT PERMUTATION - Run all permutations in one batch
      if (verbose)
        std::cout << "Stage 1: Running " << mNumPermutations << " permutations for all strategies..." << std::endl;

      std::vector<std::vector<Decimal>> permutedStats(mNumPermutations, std::vector<Decimal>(numStrategies));
      Executor executor;

      // Create base portfolio once before the parallel loop
      auto basePortfolio = std::make_shared<Portfolio<Decimal>>(baseSecurity->getName() + " Base Portfolio");
      basePortfolio->addSecurity(baseSecurity);

      concurrency::parallel_for(mNumPermutations, executor,
        [&](uint32_t p) {
          // Inside each permutation, create ONE synthetic dataset
          auto syntheticPortfolio = createSyntheticPortfolio<Decimal>(baseSecurity, basePortfolio);

          // Test ALL strategies on this single synthetic dataset
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
      
      if (verbose)
        std::cout << "Stage 1 Complete: Generated matrix of permuted statistics." << std::endl;

      // 3. STAGE 2: EXCEEDANCE COUNT ("Worst-to-Best" Pass)
      if (verbose)
        std::cout << "Stage 2: Calculating exceedance counts (Worst-to-Best pass)..." << std::endl;

      std::vector<unsigned int> exceedanceCounts(numStrategies, 1); // Start counts at 1 for the original data

      for (uint32_t p = 0; p < mNumPermutations; ++p)
      {
        Decimal max_f_so_far = std::numeric_limits<Decimal>::lowest();
        // Loop from WORST to BEST strategy (reverse order of sorted data)
        for (int s = numStrategies - 1; s >= 0; --s)
        {
          max_f_so_far = std::max(max_f_so_far, permutedStats[p][s]);
          
          if (max_f_so_far >= sortedStrategyData[s].baselineStat)
          {
            exceedanceCounts[s]++;
          }
        }
      }

      if (verbose)
        std::cout << "Stage 2 Complete: Calculated exceedance counts." << std::endl;

      // 4. STAGE 3: P-VALUE ADJUSTMENT ("Best-to-Worst" Pass)
      if (verbose)
        std::cout << "Stage 3: Adjusting p-values for monotonicity (Best-to-Worst pass)..." << std::endl;
      
      std::map<StrategyPtr, Decimal> finalPValues;
      Decimal last_p_adj = DecimalConstants<Decimal>::DecimalZero;

      for (size_t s = 0; s < numStrategies; ++s)
      {
        const auto& strategyContext = sortedStrategyData[s];
        Decimal p_raw = static_cast<Decimal>(exceedanceCounts[s]) / static_cast<Decimal>(mNumPermutations + 1);

        // Enforce monotonicity
        Decimal p_final = std::max(p_raw, last_p_adj);

	if (verbose)
	  std::cout << "Raw pvalue = " << p_raw << ", adjusted p-value = " <<  p_final << std::endl;

        finalPValues[strategyContext.strategy] = p_final;
        last_p_adj = p_final;
      }
      
      if (verbose)
        std::cout << "Stage 3 Complete: Final adjusted p-values computed." << std::endl;

      // 5. POPULATE FINAL RESULTS
      mStrategySelectionPolicy.clearForNewTest();
      for (const auto& strategyContext : sortedStrategyData)
      {
          mStrategySelectionPolicy.addStrategy(finalPValues.at(strategyContext.strategy), strategyContext.strategy);
      }
      mStrategySelectionPolicy.correctForMultipleTests(pValueSignificanceLevel);

      if (verbose)
        std::cout << "Efficient batch permutation test complete. Found " << getNumSurvivingStrategies() << " surviving strategies." << std::endl;
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
    unsigned long mNumPermutations;
    // We use a simple strategy selection policy to hold and filter the final results.
    UnadjustedPValueStrategySelection<Decimal> mStrategySelectionPolicy;
  };
} // namespace mkc_timeseries

