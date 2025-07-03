// In a new header, e.g., StrategyFamilyPartitioner.h
#pragma once

#include <map>
#include <vector>
#include <iostream>
#include <iomanip>
#include "PALMonteCarloTypes.h"     // For StrategyDataContainerType
#include "PALPatternClassifier.h" // For StrategyCategory, etc.

namespace mkc_timeseries
{
  // A key to uniquely identify each family
  struct StrategyFamilyKey {
    mkc_timeseries::StrategyCategory category;
    mkc_timeseries::StrategySubType subType; // New field for granularity
    bool isLong; // true for Long, false for Short

    // Add comparison operators for use as a map key
    bool operator<(const StrategyFamilyKey& other) const {
      if (category != other.category) {
        return category < other.category;
      }
      // Add subType to the comparison logic
      if (subType != other.subType) {
        return subType < other.subType;
      }
      return isLong < other.isLong;
    }
  };

  // A struct to hold the analytical data for a single family
  struct FamilyStatistics {
    StrategyFamilyKey key;
    size_t count = 0;
    double percentageOfTotal = 0.0;
  };

  // In StrategyFamilyPartitioner.h

  template <class Decimal>
    class StrategyFamilyPartitioner
    {
    public:
      using StrategyDataContainerType = mkc_timeseries::StrategyDataContainer<Decimal>;
      using FamilyMap = std::map<StrategyFamilyKey, StrategyDataContainerType>;

      /**
       * @brief Constructs the partitioner.
       * @param allStrategies The container of all strategies to partition.
       * @param partitionBySubType If true, families will be partitioned by both category and sub-type.
       * If false (default), partitions only by category, preserving old behavior.
       */
      explicit StrategyFamilyPartitioner(const StrategyDataContainerType& allStrategies, bool partitionBySubType = false)
        : m_totalStrategyCount(allStrategies.size()),
          m_partitionBySubType(partitionBySubType)
      {
        partition(allStrategies);
      }

      typename FamilyMap::const_iterator begin() const { return m_families.begin(); }
      typename FamilyMap::const_iterator end() const { return m_families.end(); }
      size_t getNumberOfFamilies() const { return m_families.size(); }

      // --- NEW Methods for Analytics ---

      /**
       * @brief Gets the total number of strategies that were partitioned.
       */
      size_t getTotalStrategyCount() const
      {
        return m_totalStrategyCount;
      }

      /**
       * @brief Gets the number of strategies in a specific family.
       * @param key The key identifying the family.
       * @return The number of strategies in that family, or 0 if the family doesn't exist.
       */
      size_t getFamilyCount(const StrategyFamilyKey& key) const
      {
        auto it = m_families.find(key);
        if (it != m_families.end()) {
          return it->second.size();
        }
        return 0;
      }

      /**
       * @brief Returns a vector containing the statistics for every identified family.
       */
      std::vector<FamilyStatistics> getStatistics() const
        {
          std::vector<FamilyStatistics> stats;
          if (m_totalStrategyCount == 0) {
            return stats; // Return empty vector if there are no strategies
          }

          for (const auto& pair : m_families) {
            FamilyStatistics familyStat;
            familyStat.key = pair.first;
            familyStat.count = pair.second.size();
            familyStat.percentageOfTotal = (static_cast<double>(familyStat.count) / m_totalStrategyCount) * 100.0;
            stats.push_back(familyStat);
          }
          return stats;
        }


    private:
      void partition(const StrategyDataContainerType& allStrategies)
      {
        for (const auto& context : allStrategies)
          {
            // 1. Get the pattern object from the strategy instance.
            std::shared_ptr<PriceActionLabPattern> pattern = context.strategy->getPalPattern();

            // 2. Pass the pattern to the static classifier to get the result.
            ClassificationResult classification = PALPatternClassifier::classify(pattern);

            StrategyFamilyKey key;
            key.category = classification.primary_classification;
            key.isLong = context.strategy->isLongStrategy();

            // Conditionally partition by sub-type based on the constructor flag
            if (m_partitionBySubType) {
                key.subType = classification.sub_type;
            } else {
                key.subType = StrategySubType::NONE; // Default value for broader categories
            }

            m_families[key].push_back(context);
          }
      }

      FamilyMap m_families;
      size_t m_totalStrategyCount; // Total number of strategies
      bool m_partitionBySubType;   // Flag to control partitioning depth
    };

  /**
   * @brief Converts a StrategyFamilyKey struct into a human-readable string.
   * @param key The StrategyFamilyKey to convert.
   * @return A formatted string representation (e.g., "Long - Trend-Following" or "Long - Momentum - Pullback").
   */
  inline std::string FamilyKeyToString(const StrategyFamilyKey& key)
  {
    // Determine the direction string
    std::string direction_str = key.isLong ? "Long" : "Short";

    // Use the existing helper from PALPatternClassifier.h to convert the enum to a string
    std::string category_str = mkc_timeseries::strategyCategoryToString(key.category);

    // If a meaningful sub-type exists, append it to the name.
    if (key.subType != mkc_timeseries::StrategySubType::NONE && key.subType != mkc_timeseries::StrategySubType::AMBIGUOUS) {
        std::string subType_str = mkc_timeseries::strategySubTypeToString(key.subType);
        return direction_str + " - " + category_str + " - " + subType_str;
    }

    // Otherwise, return the original format
    return direction_str + " - " + category_str;
  }

  template<class Decimal>
  void printFamilyStatistics(const StrategyFamilyPartitioner<Decimal>& partitioner)
  {
    std::cout << "--- Strategy Family Composition ---" << std::endl;
    std::cout << "Total Strategies Processed: " << partitioner.getTotalStrategyCount() << std::endl;
    std::cout << "-------------------------------------" << std::endl;

    auto stats = partitioner.getStatistics();

    // Set output formatting for percentages
    std::cout << std::fixed << std::setprecision(2);

    for (const auto& familyStat : stats)
      {
        // You would have a helper to convert the key to a readable string
        std::string familyName = FamilyKeyToString(familyStat.key); 
        
        std::cout << "Family: " << std::setw(35) << std::left << familyName
                  << " Count: " << std::setw(5) << std::right << familyStat.count
                  << " (" << familyStat.percentageOfTotal << "%)" << std::endl;
      }
    std::cout << "-------------------------------------" << std::endl;
  }
}
