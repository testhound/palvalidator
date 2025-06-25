// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __STRATEGY_IDENTIFICATION_HELPER_H
#define __STRATEGY_IDENTIFICATION_HELPER_H 1

#include <boost/uuid/uuid.hpp>
#include <functional>
#include "BackTester.h"
#include "BacktesterStrategy.h"
#include "PalStrategy.h"

namespace mkc_timeseries
{
    /**
     * @class StrategyIdentificationHelper
     * @brief Helper class for extracting strategy identification and statistics from BackTester
     * 
     * This class provides static methods for extracting strategy-related information
     * from BackTester instances during permutation testing. It handles the enhanced
     * UUID-based strategy identification and uses the new BackTester methods for
     * accurate trade and bar counting.
     * 
     * Key Features:
     * - **UUID-Based Identification**: Combines instance UUID with pattern hash
     * - **Enhanced Statistics**: Uses new BackTester methods for accurate counting
     * - **Type Safety**: Provides safe casting to PalStrategy when needed
     * - **Debugging Support**: Extracts individual components for analysis
     * 
     * The helper abstracts the complexity of strategy identification from observers,
     * allowing them to focus on statistics collection rather than extraction logic.
     * 
     * @tparam Decimal Numeric type for calculations (e.g., double)
     */
    template <class Decimal>
    class StrategyIdentificationHelper {
    public:
        /**
         * @brief Compute combined hash from pattern hash and strategy name for stable identification
         * @param patternHash The pattern hash component
         * @param strategyName The strategy name for disambiguation
         * @return Combined hash for stable identification with disambiguation
         *
         * CENTRALIZED HASH COMPUTATION: This is the single source of truth for strategy
         * identification hashing. All storage and retrieval operations must use this method
         * to ensure consistency and prevent hash mismatches.
         */
        static unsigned long long computeCombinedHash(unsigned long long patternHash, const std::string& strategyName) {
            std::hash<std::string> stringHasher;
            unsigned long long nameHash = stringHasher(strategyName);
            return patternHash ^ (nameHash << 1); // Simple hash combination
        }

        /**
         * @brief Extract strategy hash from BackTester using combined hash for stable identification
         * @param backTester The BackTester containing the strategy
         * @return Combined hash (pattern + name) for stable identification across strategy clones
         *
         * This method extracts the combined hash which remains stable across strategy clones
         * and provides disambiguation between strategies with the same pattern but different names.
         * This enables statistics collected during permutation tests (where strategies are cloned)
         * to be retrieved later using the original strategy objects.
         */
        static unsigned long long extractStrategyHash(const BackTester<Decimal>& backTester) {
            auto strategy = *(backTester.beginStrategies());
            auto palStrategy = dynamic_cast<const PalStrategy<Decimal>*>(strategy.get());
            if (!palStrategy) return 0;
            
            // Use centralized combined hash computation for consistency
            return computeCombinedHash(palStrategy->getPatternHash(), palStrategy->getStrategyName());
        }

        /**
         * @brief Extract combined hash directly from PalStrategy
         * @param strategy The PalStrategy instance
         * @return Combined hash (pattern + name) for stable identification
         *
         * Convenience method for computing combined hash directly from a strategy pointer.
         * Uses the same centralized hash computation as extractStrategyHash.
         */
        static unsigned long long extractCombinedHash(const PalStrategy<Decimal>* strategy) {
            if (!strategy) return 0;
            return computeCombinedHash(strategy->getPatternHash(), strategy->getStrategyName());
        }

        /**
         * @brief Extract strategy pointer for direct keying
         * @param backTester The BackTester containing the strategy
         * @return Pointer to the BacktesterStrategy (can be cast to PalStrategy if needed)
         */
        static const BacktesterStrategy<Decimal>* extractStrategy(const BackTester<Decimal>& backTester) {
            return (*(backTester.beginStrategies())).get();
        }

        /**
         * @brief Extract PalStrategy pointer with type safety
         * @param backTester The BackTester containing the strategy
         * @return Pointer to the PalStrategy, or nullptr if not a PalStrategy
         * 
         * This method safely casts to PalStrategy and returns nullptr if the
         * strategy is not a PalStrategy instance. Essential for PAL-specific
         * observer implementations.
         */
        static const PalStrategy<Decimal>* extractPalStrategy(const BackTester<Decimal>& backTester) {
            auto strategy = *(backTester.beginStrategies());
            return dynamic_cast<const PalStrategy<Decimal>*>(strategy.get());
        }

        /**
         * @brief Extract strategy UUID for debugging/logging
         * @param backTester The BackTester containing the strategy
         * @return UUID of the strategy instance
         * 
         * Useful for debugging and logging to track individual strategy instances
         * across permutation runs.
         */
        static boost::uuids::uuid extractStrategyUuid(const BackTester<Decimal>& backTester) {
            auto strategy = *(backTester.beginStrategies());
            return strategy->getInstanceId();
        }

        /**
         * @brief Extract pattern hash component for analysis
         * @param backTester The BackTester containing the strategy
         * @return Pattern hash, or 0 if not a PalStrategy
         * 
         * Extracts just the pattern hash component, useful for grouping strategies
         * by pattern type during analysis.
         */
        static unsigned long long extractPatternHash(const BackTester<Decimal>& backTester) {
            auto palStrategy = extractPalStrategy(backTester);
            return palStrategy ? palStrategy->getPatternHash() : 0;
        }

        /**
         * @brief Extract total number of trades (closed + open) from BackTester
         * @param backTester The BackTester after running backtest
         * @return Total number of trades including open positions
         * 
         * Uses the new getNumTrades() method for comprehensive trade count that
         * includes both closed trades and currently open position units. This
         * provides more accurate statistics than counting only closed trades.
         */
        static uint32_t extractNumTrades(const BackTester<Decimal>& backTester) {
            // Use the new getNumTrades() method for comprehensive trade count
            return backTester.getNumTrades();
        }

        /**
         * @brief Extract total number of bars in all trades from BackTester
         * @param backTester The BackTester after running backtest
         * @return Total number of bars across all trades (closed + open)
         * 
         * Uses the new getNumBarsInTrades() method for comprehensive bar count
         * across all trades. This includes bars from both closed trades and
         * currently open positions, providing accurate market exposure metrics.
         */
        static uint32_t extractNumBarsInTrades(const BackTester<Decimal>& backTester) {
            // Use the new getNumBarsInTrades() method for comprehensive bar count
            return backTester.getNumBarsInTrades();
        }
    };
}

#endif