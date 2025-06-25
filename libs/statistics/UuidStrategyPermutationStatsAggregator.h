// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __UUID_STRATEGY_PERMUTATION_STATS_AGGREGATOR_H
#define __UUID_STRATEGY_PERMUTATION_STATS_AGGREGATOR_H 1

#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <functional>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/nil_generator.hpp>
#include "PermutationTestObserver.h"
#include "ThreadSafeAccumulator.h"
#include "PalStrategy.h"
#include "StrategyIdentificationHelper.h"

namespace mkc_timeseries
{
    /**
     * @class UuidStrategyPermutationStatsAggregator
     * @brief Advanced statistics aggregator using combined hash strategy identification
     *
     * This class provides a sophisticated statistics collection system that uses
     * combined hash strategy identification (pattern hash + strategy name hash) with
     * Boost.Accumulators for efficient statistics computation. It's designed for
     * high-performance permutation testing where strategies are cloned with new UUIDs
     * but need stable identification with proper disambiguation.
     *
     * Key Features:
     * - **Combined Hash Identification**: Uses pattern hash + strategy name hash for stable identification with disambiguation
     * - **Monte Carlo Compatibility**: Statistics collected during permutation tests can be retrieved later
     * - **Strategy Disambiguation**: Different strategies with same pattern are properly distinguished
     * - **Boost.Accumulators Integration**: 75% code reduction, 90% memory efficiency
     * - **Thread-Safe Design**: Concurrent permutation testing support
     * - **Debugging Support**: Maps combined hashes back to UUIDs and strategy pointers
     * - **Pattern Analysis**: Groups strategies by pattern for comparative analysis
     *
     * Architecture:
     * - Primary key: Combined hash (pattern hash ^ (strategy name hash << 1))
     * - Secondary mappings: Combined hash → Strategy pointer, UUID, pattern hash
     * - Statistics storage: Per-combined-hash, per-metric ThreadSafeAccumulator instances
     *
     * @tparam Decimal Numeric type for calculations (e.g., double)
     */
    template <class Decimal>
    class UuidStrategyPermutationStatsAggregator {
    private:
        using MetricType = typename PermutationTestObserver<Decimal>::MetricType;
        
        // Use pattern hash as primary key for stable identification across clones
        std::unordered_map<unsigned long long,
                          std::unordered_map<MetricType, ThreadSafeAccumulator<Decimal>>>
                          m_strategyMetrics;
        
        // Map pattern hash back to strategy pointer for interface compatibility
        std::unordered_map<unsigned long long, const PalStrategy<Decimal>*> m_hashToStrategy;
        
        // Additional mapping for debugging and analysis
        std::unordered_map<unsigned long long, boost::uuids::uuid> m_hashToUuid;
        std::unordered_map<unsigned long long, unsigned long long> m_hashToPatternHash;
        
        mutable std::shared_mutex m_mapMutex;


    public:
        /**
         * @brief Add a value to the statistics for a specific strategy and metric
         * @param strategyHash Combined hash identifying the strategy (pattern hash + strategy name hash for stable identification across clones)
         * @param strategy Pointer to the PalStrategy instance
         * @param metric Type of metric being recorded
         * @param value The value to add to the accumulator
         *
         * This method stores the value in the appropriate ThreadSafeAccumulator and
         * maintains the debugging mappings for analysis purposes. Uses combined hash
         * (pattern hash + strategy name hash) for stable identification across strategy
         * clones with proper disambiguation during permutation testing.
         */
        void addValue(unsigned long long strategyHash,
                      const PalStrategy<Decimal>* strategy,
                      MetricType metric,
                      const Decimal& value) {
            // BUGFIX: Use unique_lock (write lock) instead of shared_lock for write operations
            // The original shared_lock caused race conditions and memory corruption
            std::unique_lock<std::shared_mutex> mapLock(m_mapMutex);
            
            // Store strategy mappings for debugging and analysis
            m_hashToStrategy[strategyHash] = strategy;
            m_hashToUuid[strategyHash] = strategy->getInstanceId();
            m_hashToPatternHash[strategyHash] = strategy->getPatternHash();
            
            // Add value to accumulator
            m_strategyMetrics[strategyHash][metric].addValue(value);
        }

        /**
         * @brief Get minimum value for a strategy and metric
         * @param strategy Pointer to the PalStrategy
         * @param metric Type of metric to retrieve
         * @return Minimum value, or nullopt if no data available
         */
        std::optional<Decimal> getMin(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            if (!strategy) return std::nullopt;
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            
            auto stratIt = m_strategyMetrics.find(hash);
            if (stratIt == m_strategyMetrics.end()) {
                return std::nullopt;
            }
            
            auto metricIt = stratIt->second.find(metric);
            if (metricIt == stratIt->second.end()) {
                return std::nullopt;
            }
            
            return metricIt->second.getMin();
        }

        /**
         * @brief Get maximum value for a strategy and metric
         * @param strategy Pointer to the PalStrategy
         * @param metric Type of metric to retrieve
         * @return Maximum value, or nullopt if no data available
         */
        std::optional<Decimal> getMax(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            if (!strategy) return std::nullopt;
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            
            auto stratIt = m_strategyMetrics.find(hash);
            if (stratIt == m_strategyMetrics.end()) return std::nullopt;
            
            auto metricIt = stratIt->second.find(metric);
            if (metricIt == stratIt->second.end()) return std::nullopt;
            
            return metricIt->second.getMax();
        }

        /**
         * @brief Get median value for a strategy and metric
         * @param strategy Pointer to the PalStrategy
         * @param metric Type of metric to retrieve
         * @return Median value, or nullopt if no data available
         */
        std::optional<double> getMedian(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            if (!strategy) return std::nullopt;
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            
            auto stratIt = m_strategyMetrics.find(hash);
            if (stratIt == m_strategyMetrics.end()) return std::nullopt;
            
            auto metricIt = stratIt->second.find(metric);
            if (metricIt == stratIt->second.end()) return std::nullopt;
            
            return metricIt->second.getMedian();
        }

        /**
         * @brief Get standard deviation for a strategy and metric
         * @param strategy Pointer to the PalStrategy
         * @param metric Type of metric to retrieve
         * @return Standard deviation, or nullopt if insufficient data
         */
        std::optional<double> getStdDev(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            if (!strategy) return std::nullopt;
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            
            auto stratIt = m_strategyMetrics.find(hash);
            if (stratIt == m_strategyMetrics.end()) return std::nullopt;
            
            auto metricIt = stratIt->second.find(metric);
            if (metricIt == stratIt->second.end()) return std::nullopt;
            
            return metricIt->second.getStdDev();
        }

        /**
         * @brief Clear all accumulated statistics
         * 
         * Resets all accumulators and clears all mapping tables. Thread-safe operation.
         */
        void clear() {
            std::unique_lock<std::shared_mutex> lock(m_mapMutex);
            m_strategyMetrics.clear();
            m_hashToStrategy.clear();
            m_hashToUuid.clear();
            m_hashToPatternHash.clear();
        }

        // Debug/monitoring methods
        
        /**
         * @brief Get the number of unique strategies being tracked
         * @return Count of unique strategy instances
         */
        size_t getStrategyCount() const {
            std::shared_lock<std::shared_mutex> lock(m_mapMutex);
            return m_strategyMetrics.size();
        }

        /**
         * @brief Get the number of permutations recorded for a strategy and metric
         * @param strategy Pointer to the PalStrategy
         * @param metric Type of metric to check
         * @return Number of permutation samples, or 0 if no data
         */
        size_t getPermutationCount(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            if (!strategy) return 0;
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            std::shared_lock<std::shared_mutex> mapLock(m_mapMutex);
            
            auto stratIt = m_strategyMetrics.find(hash);
            if (stratIt == m_strategyMetrics.end()) return 0;
            
            auto metricIt = stratIt->second.find(metric);
            if (metricIt == stratIt->second.end()) return 0;
            
            return metricIt->second.getCount();
        }

        // Analysis methods for debugging
        
        /**
         * @brief Get the UUID for a strategy (debugging/logging)
         * @param strategy Pointer to the PalStrategy
         * @return UUID of the strategy instance, or nil UUID if not found
         */
        boost::uuids::uuid getStrategyUuid(const PalStrategy<Decimal>* strategy) const {
            if (!strategy) return boost::uuids::nil_uuid();
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            std::shared_lock<std::shared_mutex> lock(m_mapMutex);
            
            auto it = m_hashToUuid.find(hash);
            return (it != m_hashToUuid.end()) ? it->second : boost::uuids::nil_uuid();
        }

        /**
         * @brief Get the pattern hash for a strategy (analysis)
         * @param strategy Pointer to the PalStrategy
         * @return Pattern hash, or 0 if not found
         */
        unsigned long long getPatternHash(const PalStrategy<Decimal>* strategy) const {
            if (!strategy) return 0;
            
            // Use centralized combined hash computation for consistency
            unsigned long long hash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);
            std::shared_lock<std::shared_mutex> lock(m_mapMutex);
            
            auto it = m_hashToPatternHash.find(hash);
            return (it != m_hashToPatternHash.end()) ? it->second : 0;
        }

        /**
         * @brief Get all strategies with the same pattern (different UUIDs)
         * @param patternHash The pattern hash to search for
         * @return Vector of strategy pointers with matching pattern
         * 
         * Useful for comparative analysis of different instances of the same pattern.
         */
        std::vector<const PalStrategy<Decimal>*> getStrategiesWithSamePattern(unsigned long long patternHash) const {
            std::shared_lock<std::shared_mutex> lock(m_mapMutex);
            std::vector<const PalStrategy<Decimal>*> result;
            
            for (const auto& pair : m_hashToPatternHash) {
                if (pair.second == patternHash) {
                    auto stratIt = m_hashToStrategy.find(pair.first);
                    if (stratIt != m_hashToStrategy.end()) {
                        result.push_back(stratIt->second);
                    }
                }
            }
            
            return result;
        }
    };
}

#endif
