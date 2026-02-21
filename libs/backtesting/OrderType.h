// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, February 2026
//

/**
 * @file OrderType.h
 *
 * @brief Enumeration and utilities for tracking trading order types in position lifecycle
 *
 * ### Responsibilities:
 * - Define OrderType enum for all order types that can create or close positions
 * - Provide utility functions for string conversion and validation
 * - Enable type-safe order type tracking throughout the backtesting system
 *
 * ### Collaboration:
 * - Used by TradingPosition classes to track entry and exit order types
 * - Integrated with StrategyBroker for order type extraction and propagation
 * - Supports enhanced analytics and debugging capabilities
 */

#ifndef __ORDER_TYPE_H
#define __ORDER_TYPE_H 1

#include <string>

namespace mkc_timeseries
{
  /**
   * @enum OrderType  
   * @brief Enumeration of all order types that can create or close trading positions
   *
   * This enum provides type-safe identification of order types for complete
   * audit trail tracking in the trading position lifecycle. Each value corresponds
   * to a specific TradingOrder subclass that can execute and affect positions.
   */
  enum class OrderType {
    // Entry order types (create positions)
    MARKET_ON_OPEN_LONG,     ///< MarketOnOpenLongOrder: Creates long position at market open
    MARKET_ON_OPEN_SHORT,    ///< MarketOnOpenShortOrder: Creates short position at market open
    
    // Exit order types (close positions)  
    MARKET_ON_OPEN_SELL,     ///< MarketOnOpenSellOrder: Closes long position at market open
    MARKET_ON_OPEN_COVER,    ///< MarketOnOpenCoverOrder: Closes short position at market open
    SELL_AT_LIMIT,           ///< SellAtLimitOrder: Closes long position at/above limit price
    COVER_AT_LIMIT,          ///< CoverAtLimitOrder: Closes short position at/below limit price
    SELL_AT_STOP,            ///< SellAtStopOrder: Closes long position at/below stop price
    COVER_AT_STOP,           ///< CoverAtStopOrder: Closes short position at/above stop price
    
    // Default/uninitialized value
    UNKNOWN                  ///< Default value for backward compatibility and uninitialized cases
  };

  /**
   * @brief Converts OrderType enum to human-readable string representation
   * @param orderType The OrderType to convert
   * @return String representation of the order type
   * @note Returns "UNKNOWN" for unrecognized values
   */
  std::string orderTypeToString(OrderType orderType);

  /**
   * @brief Checks if the order type is an entry order (creates positions)
   * @param orderType The OrderType to check
   * @return True if this order type creates new positions, false otherwise
   */
  bool isEntryOrderType(OrderType orderType);

  /**
   * @brief Checks if the order type is an exit order (closes positions)
   * @param orderType The OrderType to check
   * @return True if this order type closes existing positions, false otherwise
   */
  bool isExitOrderType(OrderType orderType);

  /**
   * @brief Validates that an order type is appropriate for position entry
   * @param orderType The OrderType to validate
   * @throws std::invalid_argument if the order type is not a valid entry type
   */
  void validateEntryOrderType(OrderType orderType);

  /**
   * @brief Validates that an order type is appropriate for position exit
   * @param orderType The OrderType to validate  
   * @throws std::invalid_argument if the order type is not a valid exit type
   */
  void validateExitOrderType(OrderType orderType);
}

#endif