#ifndef __PAL_MASTER_MONTE_CARLO_VALIDATION_H
#define __PAL_MASTER_MONTE_CARLO_VALIDATION_H 1

#include <string>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <map>
#include <memory>
#include <fstream>
#include <boost/date_time.hpp>
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PalAst.h"
#include "Portfolio.h"
#include "Security.h"
#include "TimeSeries.h"
#include "MultipleTestingCorrection.h"
#include "PALMonteCarloTypes.h"
#include "StrategyDataPreparer.h"
#include "IMastersSelectionBiasAlgorithm.h"
#include "MastersRomanoWolf.h"
#include "MastersRomanoWolfImproved.h"
#include "PermutationStatisticsCollector.h"
#include "StrategyFamilyPartitioner.h" // Added include for new functionality

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::shared_ptr;
  using std::make_shared;
  using std::map;

  class PALMastersMonteCarloValidationException : public std::runtime_error
  {
  public:
    PALMastersMonteCarloValidationException(const std::string msg)
      : std::runtime_error(msg) {}

    ~PALMastersMonteCarloValidationException() noexcept = default;
  };

  /**
   * @class PALMastersMonteCarloValidation
   * @brief Orchestrator for Timothy Masters' stepwise permutation test.
   *
   * This class can partition strategies in two ways:
   * 1.  (Default) By direction (all LONG vs. all SHORT).
   * 2.  (Optional) By detailed strategy family (e.g., Long-Trend, Short-Momentum)
   * using the `StrategyFamilyPartitioner`.
   *
   * @tparam Decimal Numeric type for calculations.
   * @tparam BaselineStatPolicy Policy for computing the performance statistic.
   */
template <class Decimal, class BaselineStatPolicy>
    class PALMastersMonteCarloValidation
    {
    public:
      // Use types from PALMonteCarloTypes.h
      using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>; // Convenience alias
      using StrategyContextType = StrategyContext<Decimal>;
      using StrategyDataContainerType = StrategyDataContainer<Decimal>;
      using AlgoType = IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>;

      // Alias for result iterator
      using SurvivingStrategiesIterator = typename UnadjustedPValueStrategySelection<Decimal>::ConstSurvivingStrategiesIterator;
      
      PALMastersMonteCarloValidation(unsigned long numPermutations,
        std::unique_ptr<AlgoType> algo =
        std::make_unique<MastersRomanoWolfImproved<Decimal,BaselineStatPolicy>>())
        : mNumPermutations(numPermutations),
          mStrategySelectionPolicy(),
          mAlgorithm(std::move(algo)),
          mStatisticsCollector(std::make_unique<PermutationStatisticsCollector<Decimal>>())
      {
         if (numPermutations == 0)
           throw PALMastersMonteCarloValidationException("Number of permutations cannot be zero.");
      }

      // Default copy/move constructors/assignment
      PALMastersMonteCarloValidation(const PALMastersMonteCarloValidation&) = default;
      PALMastersMonteCarloValidation& operator=(const PALMastersMonteCarloValidation&) = default;
      PALMastersMonteCarloValidation(PALMastersMonteCarloValidation&&) noexcept = default;
      PALMastersMonteCarloValidation& operator=(PALMastersMonteCarloValidation&&) noexcept = default;

      virtual ~PALMastersMonteCarloValidation() = default;

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

      const PermutationStatisticsCollector<Decimal>& getStatisticsCollector() const {
        return *mStatisticsCollector;
      }

      PermutationStatisticsCollector<Decimal>& getStatisticsCollector() {
        return *mStatisticsCollector;
      }

      std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> getAllTestedStrategies() const {
        return mStrategySelectionPolicy.getAllTestedStrategies();
      }
      
      Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const {
        return mStrategySelectionPolicy.getStrategyPValue(strategy);
      }

      void runPermutationTests(std::shared_ptr<Security<Decimal>> baseSecurity,
			       std::shared_ptr<PriceActionLabSystem> patterns,
			       const DateRange& dateRange,
			       const Decimal& pValueSignificanceLevel = DecimalConstants<Decimal>::SignificantPValue,
			       bool verbose = false,
                               bool partitionByFamily = false) // New argument to control partitioning
      {
        if (!baseSecurity)
          throw PALMastersMonteCarloValidationException("Base security missing in runPermutationTests setup.");

        if (!patterns)
          throw PALMastersMonteCarloValidationException("Price patterns missing in runPermutationTests setup.");

        mStrategySelectionPolicy.clearForNewTest();
        auto timeFrame = baseSecurity->getTimeSeries()->getTimeFrame();
        auto templateBackTester = BackTesterFactory<Decimal>::getBackTester(timeFrame, dateRange);
        if (!templateBackTester)
          throw PALMastersMonteCarloValidationException("Failed to create template backtester.");
        
        // 1. Prepare data for ALL strategies
        mStrategyData = StrategyDataPreparer<Decimal, BaselineStatPolicy>::prepare(templateBackTester,
                                                                                 baseSecurity,
                                                                                 patterns);
            
        if (mStrategyData.empty()) {
          if (verbose) std::cout << "No strategies found for permutation testing." << std::endl;
          return;
        }

        if (verbose)
          std::cout << "PALMastersMonteCarloValidation starting validation..." << std::endl;
        
        auto portfolio = std::make_shared<Portfolio<Decimal>>("PermutationPortfolio");
        portfolio->addSecurity(baseSecurity->clone(baseSecurity->getTimeSeries()));
        
        std::map<unsigned long long, Decimal> pvalMap;
        Decimal sigLevel = pValueSignificanceLevel;

        // Attach observer to the main algorithm instance
        auto* subject = dynamic_cast<PermutationTestSubject<Decimal>*>(mAlgorithm.get());
        if (subject) {
            subject->attach(mStatisticsCollector.get());
        }

        // --- Execute tests based on partitioning choice ---
        if (partitionByFamily)
        {
            // 2a. Use the new StrategyFamilyPartitioner
            if (verbose) std::cout << "Partitioning strategies by detailed family (Category + Direction)..." << std::endl;

            StrategyFamilyPartitioner<Decimal> partitioner(mStrategyData);
            if (verbose) {
                printFamilyStatistics(partitioner);
            }

            for (const auto& familyPair : partitioner)
            {
                const StrategyFamilyKey& familyKey = familyPair.first;
                StrategyDataContainerType strategyFamily = familyPair.second; // Make a mutable copy

                if (strategyFamily.empty()) continue;

                if (verbose)
                    std::cout << "\n--- Testing " << FamilyKeyToString(familyKey) << " Strategy Family (" << strategyFamily.size() << " strategies) ---" << std::endl;

                std::sort(strategyFamily.begin(), strategyFamily.end(),
                          [](const StrategyContextType& a, const StrategyContextType& b) {
                              return a.baselineStat > b.baselineStat; // Sort descending
                          });
                auto familyPvals = mAlgorithm->run(strategyFamily, mNumPermutations, templateBackTester, portfolio, sigLevel);
                pvalMap.insert(familyPvals.begin(), familyPvals.end());
            }

        }
        else
        {
            // 2b. Partition into default LONG and SHORT families (original behavior)
            if (verbose) std::cout << "Partitioning strategies by Direction (Long vs. Short)..." << std::endl;

            StrategyDataContainerType longStrategies, shortStrategies;
            for (const auto& context : mStrategyData) {
                if (context.strategy->isLongStrategy()) {
                    longStrategies.push_back(context);
                } else if (context.strategy->isShortStrategy()) {
                    shortStrategies.push_back(context);
                }
            }
            
            if (verbose) {
                std::cout << "Partitioned strategies: " << longStrategies.size() << " Long, " 
                          << shortStrategies.size() << " Short." << std::endl;
            }

            // 3. Run test for LONG family
            if (!longStrategies.empty()) {
                if (verbose) std::cout << "\n--- Testing LONG Strategy Family ---" << std::endl;
                std::sort(longStrategies.begin(), longStrategies.end(),
                          [](const StrategyContextType& a, const StrategyContextType& b) {
                              return a.baselineStat > b.baselineStat; // Sort descending
                          });
                auto longPvals = mAlgorithm->run(longStrategies, mNumPermutations, templateBackTester, portfolio, sigLevel);
                pvalMap.insert(longPvals.begin(), longPvals.end());
            }

            // 4. Run test for SHORT family
            if (!shortStrategies.empty()) {
                if (verbose) std::cout << "\n--- Testing SHORT Strategy Family ---" << std::endl;
                std::sort(shortStrategies.begin(), shortStrategies.end(),
                          [](const StrategyContextType& a, const StrategyContextType& b) {
                              return a.baselineStat > b.baselineStat; // Sort descending
                          });
                auto shortPvals = mAlgorithm->run(shortStrategies, mNumPermutations, templateBackTester, portfolio, sigLevel);
                pvalMap.insert(shortPvals.begin(), shortPvals.end());
            }
        }

        if (verbose)
          std::cout << "\nPALMastersMonteCarloValidation: finishing validation, populating strategy selection policy" << std::endl;
        
        // 5. Populate final results from the combined p-value map
        for (const auto& entry : mStrategyData)
          {
            Decimal finalPval = DecimalConstants<Decimal>::DecimalOne; // Default p-value
            auto strategyHash = entry.strategy->getPatternHash();
            auto it = pvalMap.find(strategyHash);

            if (it != pvalMap.end())
              {
                finalPval = it->second;
              }
            else
              {
                if (verbose)
                    std::cerr << "Warning: Final p-value not found for strategy "
                              << entry.strategy->getStrategyName() << " (Hash: " << strategyHash 
                              << "), assigning 1.0" << std::endl;
              }
            
            mStrategySelectionPolicy.addStrategy(finalPval, entry.strategy);
          }
        
        mStrategySelectionPolicy.correctForMultipleTests(pValueSignificanceLevel);

        if (verbose)
          std::cout << "PALMastersMonteCarloValidation finished validation. Found " 
                    << getNumSurvivingStrategies() << " total surviving strategies." << std::endl;
      }

    private:
      unsigned long mNumPermutations;
      StrategyDataContainerType mStrategyData;
      UnadjustedPValueStrategySelection<Decimal> mStrategySelectionPolicy;
      std::unique_ptr<IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>>  mAlgorithm;
      std::unique_ptr<PermutationStatisticsCollector<Decimal>> mStatisticsCollector;
    };
} // namespace mkc_timeseries

#endif // __PAL_MASTER_MONTE_CARLO_VALIDATION_H
