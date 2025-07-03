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
#include "StrategyFamilyPartitioner.h" // Added include for new functionality


namespace mkc_timeseries
{
  /**
   * @class PALRomanoWolfMonteCarloValidation
   * @brief Implements the Romano & Wolf stepwise permutation test with flexible partitioning.
   *
   * This class can partition strategies in two ways:
   * 1.  (Default) By direction (all LONG vs. all SHORT).
   * 2.  (Optional) By detailed strategy family (e.g., Long-Trend, Short-Momentum)
   * via the `StrategyFamilyPartitioner`.
   *
   * @tparam Decimal The numeric type for calculations.
   * @tparam BaselineStatPolicy A policy class for computing the performance statistic.
   * @tparam Executor A policy for parallel execution.
   */
  template <class Decimal, class BaselineStatPolicy, class Executor = concurrency::ThreadPoolExecutor<>>
  class PALRomanoWolfMonteCarloValidation
  {
  public:
    // Type aliases for clarity
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
        bool verbose = false,
        bool partitionByFamily = false) // New argument to control partitioning
    {
      if (!baseSecurity || !patterns)
        throw std::invalid_argument("PALRomanoWolfMonteCarloValidation::runPermutationTests - baseSecurity and patterns must not be null.");

      if (verbose)
	{
	  std::cout << "Starting Romano-Wolf validation..." << std::endl;
	  std::cout << "OOS Date Range: " << dateRange.getFirstDateTime()
		    << " to " << dateRange.getLastDateTime() << std::endl;
	}
		  

      // 1. Prepare baseline stats for ALL strategies first
      auto templateBackTester = BackTesterFactory<Decimal>::getBackTester(baseSecurity->getTimeSeries()->getTimeFrame(), dateRange);
      StrategyDataContainerType allStrategyData = StrategyDataPreparer<Decimal, BaselineStatPolicy>::prepare(templateBackTester, baseSecurity, patterns);
      
      if (allStrategyData.empty())
      {
        if (verbose) std::cout << "No strategies to test. Exiting." << std::endl;
        return;
      }

      std::map<StrategyPtr, Decimal> finalPValues;

      // 2. Execute tests based on partitioning choice
      if (partitionByFamily)
      {
	bool partitionBySubType = (patterns->getNumPatterns() >= 1000) ? true : false;
	
	if (verbose)
	  {
	    std::string detail = partitionBySubType ? "Category, SubType, and Direction" :
	      "Category and Direction";
	    std::cout << "Partitioning strategies by detailed family (" << detail << ")..." << std::endl;
	  }

        StrategyFamilyPartitioner<Decimal> partitioner(allStrategyData, partitionBySubType);
        if (verbose)
          printFamilyStatistics(partitioner);

        for (const auto& familyPair : partitioner)
        {
          const auto& familyKey = familyPair.first;
          const auto& strategyFamily = familyPair.second;
          if (strategyFamily.empty()) continue;

          if (verbose)
            std::cout << "\n--- Testing " << FamilyKeyToString(familyKey) << " Strategy Family (" << strategyFamily.size() << " strategies) ---" << std::endl;

          auto familyPValues = runTestForFamily(strategyFamily, templateBackTester, baseSecurity, verbose);
          finalPValues.insert(familyPValues.begin(), familyPValues.end());
        }
      }
      else
      {
        // Default behavior: partition into LONG and SHORT families
        if (verbose) std::cout << "Partitioning strategies by Direction (Long vs. Short)..." << std::endl;
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
      }
      
      // 3. Populate final results policy with combined p-values from all tested families
      mStrategySelectionPolicy.clearForNewTest();
      for (const auto& strategyContext : allStrategyData)
      {
          if (finalPValues.count(strategyContext.strategy))
          {
            mStrategySelectionPolicy.addStrategy(finalPValues.at(strategyContext.strategy), strategyContext.strategy);
          }
          else
          {
            if (verbose)
              std::cerr << "Warning: P-Value for strategy " << strategyContext.strategy->getStrategyName() 
                        << " not found, defaulting to 1.0" << std::endl;
            mStrategySelectionPolicy.addStrategy(DecimalConstants<Decimal>::DecimalOne, strategyContext.strategy);
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
     */
    std::map<StrategyPtr, Decimal> runTestForFamily(
        const StrategyDataContainerType& strategyFamily,
        std::shared_ptr<BackTester<Decimal>> templateBackTester,
        std::shared_ptr<Security<Decimal>> baseSecurity,
        bool verbose)
    {
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
