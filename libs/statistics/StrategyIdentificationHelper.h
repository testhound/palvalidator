// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __STRATEGY_IDENTIFICATION_HELPER_H
#define __STRATEGY_IDENTIFICATION_HELPER_H 1

#include <boost/uuid/uuid.hpp>
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
         * @brief Extract strategy hash from BackTester using UUID + pattern hash
         * @param backTester The BackTester containing the strategy
         * @return Combined hash from strategy's UUID and pattern
         * 
         * This method extracts the unique hash that combines the strategy's instance
         * UUID with its pattern hash, providing guaranteed uniqueness across all
         * strategy instances, even those with identical patterns.
         */
        static unsigned long long extractStrategyHash(const BackTester<Decimal>& backTester) {
            auto strategy = *(backTester.beginStrategies());
            return strategy->hashCode();  // Now includes UUID + pattern hash
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
            return const_cast<BackTester<Decimal>&>(backTester).getNumTrades();
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
            return const_cast<BackTester<Decimal>&>(backTester).getNumBarsInTrades();
        }
    };
}

#endif