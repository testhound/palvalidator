// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

#ifndef __PATTERN_POSITION_REGISTRY_H
#define __PATTERN_POSITION_REGISTRY_H 1

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>
#include "PalAst.h"

namespace mkc_timeseries
{
    /**
     * @class PatternPositionRegistry
     * @brief Thread-safe registry for tracking relationships between PriceActionLabPattern objects and trading orders/positions.
     * 
     * This class provides a centralized mapping system that maintains the relationship between patterns and the
     * orders/positions they generate without modifying the core trading classes. It uses order and position IDs
     * as keys to maintain these relationships throughout the trading lifecycle.
     * 
     * Key Features:
     * - Thread-safe operations using mutex protection
     * - Singleton pattern for global access
     * - Automatic pattern propagation from orders to positions
     * - Reverse lookup capabilities (pattern -> positions)
     * - Memory management and cleanup utilities
     * - Zero impact on core trading performance when disabled
     */
    class PatternPositionRegistry 
    {
    private:
        // Thread-safe access
        mutable std::mutex mMutex;
        
        // Order ID -> Pattern mapping
        std::unordered_map<uint32_t, std::shared_ptr<PriceActionLabPattern>> mOrderPatterns;
        
        // Position ID -> Pattern mapping  
        std::unordered_map<uint32_t, std::shared_ptr<PriceActionLabPattern>> mPositionPatterns;
        
        // Pattern -> Position IDs mapping (for reverse lookups)
        std::unordered_map<std::shared_ptr<PriceActionLabPattern>, 
                          std::vector<uint32_t>> mPatternPositions;
        
        // Statistics
        mutable size_t mTotalOrdersRegistered = 0;
        mutable size_t mTotalPositionsRegistered = 0;
        
        // Private constructor for singleton
        PatternPositionRegistry() = default;
        
    public:
        // Singleton access
        static PatternPositionRegistry& getInstance() {
            static PatternPositionRegistry instance;
            return instance;
        }
        
        // Disable copy/move
        PatternPositionRegistry(const PatternPositionRegistry&) = delete;
        PatternPositionRegistry& operator=(const PatternPositionRegistry&) = delete;
        PatternPositionRegistry(PatternPositionRegistry&&) = delete;
        PatternPositionRegistry& operator=(PatternPositionRegistry&&) = delete;
        
        /**
         * @brief Register a pattern for a trading order
         * @param orderID The unique ID of the trading order
         * @param pattern Shared pointer to the PriceActionLabPattern
         */
        void registerOrderPattern(uint32_t orderID, 
                                 std::shared_ptr<PriceActionLabPattern> pattern);
        
        /**
         * @brief Transfer pattern mapping from order to position
         * @param orderID The ID of the executed order
         * @param positionID The ID of the resulting position
         * @details This method is called when an order is filled and a position is created
         */
        void transferOrderToPosition(uint32_t orderID, uint32_t positionID);
        
        /**
         * @brief Get the pattern associated with a position (PRIMARY REQUIREMENT)
         * @param positionID The unique ID of the trading position
         * @return Shared pointer to the associated pattern, or nullptr if none exists
         */
        std::shared_ptr<PriceActionLabPattern> getPatternForPosition(uint32_t positionID) const;
        
        /**
         * @brief Get the pattern associated with an order
         * @param orderID The unique ID of the trading order
         * @return Shared pointer to the associated pattern, or nullptr if none exists
         */
        std::shared_ptr<PriceActionLabPattern> getPatternForOrder(uint32_t orderID) const;
        
        /**
         * @brief Get all position IDs associated with a pattern
         * @param pattern The pattern to look up
         * @return Vector of position IDs associated with the pattern
         */
        std::vector<uint32_t> getPositionsForPattern(
            std::shared_ptr<PriceActionLabPattern> pattern) const;
        
        /**
         * @brief Get all patterns currently tracked by the registry
         * @return Vector of all unique patterns in the registry
         */
        std::vector<std::shared_ptr<PriceActionLabPattern>> getAllPatterns() const;
        
        /**
         * @brief Remove an order from the registry
         * @param orderID The ID of the order to remove
         * @details This can be called to clean up completed orders
         */
        void removeOrder(uint32_t orderID);
        
        /**
         * @brief Remove a position from the registry
         * @param positionID The ID of the position to remove
         * @details This can be called to clean up closed positions
         */
        void removePosition(uint32_t positionID);
        
        /**
         * @brief Check if an order has an associated pattern
         * @param orderID The order ID to check
         * @return True if the order has an associated pattern
         */
        bool hasPatternForOrder(uint32_t orderID) const;
        
        /**
         * @brief Check if a position has an associated pattern
         * @param positionID The position ID to check
         * @return True if the position has an associated pattern
         */
        bool hasPatternForPosition(uint32_t positionID) const;
        
        // Statistics and debugging methods
        size_t getOrderCount() const;
        size_t getPositionCount() const;
        size_t getPatternCount() const;
        size_t getTotalOrdersRegistered() const;
        size_t getTotalPositionsRegistered() const;
        
        /**
         * @brief Clear all mappings (primarily for testing)
         */
        void clear();
        
        /**
         * @brief Generate a debug report of current registry state
         * @param output Stream to write the report to
         */
        void generateDebugReport(std::ostream& output) const;
    };

} // namespace mkc_timeseries

#endif