// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, January 2025
//

#ifndef __GENERATED_TRADE_H
#define __GENERATED_TRADE_H 1

#include <string>
#include <boost/date_time.hpp>
#include "number.h"

namespace mkc_timeseries
{
    template <class Decimal> class ComparisonTolerance;
    template <class Decimal> class ExternalTrade;

    /**
     * @brief Represents a trade generated from PAL pattern backtesting for comparison purposes.
     * 
     * @details
     * The GeneratedTrade class serves as an adapter/wrapper that converts complex TradingPosition
     * objects from the ClosedPositionHistory into a simplified, standardized format suitable for
     * comparison with external backtesting results. This class maintains high precision using
     * the same Decimal type as the underlying trading system to prevent precision loss during
     * comparison operations.
     * 
     * Key objectives:
     * - Provide a clean interface for trade comparison operations
     * - Maintain full precision of financial data using Decimal arithmetic
     * - Enable efficient matching against external backtesting platforms
     * - Support configurable tolerance-based comparison logic
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    class GeneratedTrade
    {
    private:
        std::string symbol_;                        ///< Security symbol (e.g., "ASML")
        std::string direction_;                     ///< Trade direction ("Long" or "Short")
        boost::posix_time::ptime entryDateTime_;    ///< Entry date and time with full precision
        boost::posix_time::ptime exitDateTime_;     ///< Exit date and time with full precision
        Decimal entryPrice_;                        ///< Entry price with full decimal precision
        Decimal exitPrice_;                         ///< Exit price with full decimal precision
        Decimal percentReturn_;                     ///< Percentage return with full decimal precision
        int barsInPosition_;                        ///< Number of bars the position was held

    public:
        /**
         * @brief Constructs a GeneratedTrade from trading position data.
         * 
         * @param symbol Security symbol
         * @param direction Trade direction ("Long" or "Short")
         * @param entryDateTime Entry date and time
         * @param exitDateTime Exit date and time
         * @param entryPrice Entry price
         * @param exitPrice Exit price
         * @param percentReturn Percentage return
         * @param barsInPosition Number of bars held
         */
        GeneratedTrade(const std::string& symbol, const std::string& direction,
                       const boost::posix_time::ptime& entryDateTime,
                       const boost::posix_time::ptime& exitDateTime,
                       const Decimal& entryPrice, const Decimal& exitPrice,
                       const Decimal& percentReturn, int barsInPosition)
            : symbol_(symbol), direction_(direction), entryDateTime_(entryDateTime), exitDateTime_(exitDateTime),
              entryPrice_(entryPrice), exitPrice_(exitPrice), percentReturn_(percentReturn),
              barsInPosition_(barsInPosition)
        {
        }
        
        /**
         * @brief Gets the security symbol.
         * @return Reference to the security symbol string
         */
        const std::string& getSymbol() const 
        { 
            return symbol_; 
        }
        
        /**
         * @brief Gets the trade direction.
         * @return Reference to the direction string ("Long" or "Short")
         */
        const std::string& getDirection() const 
        { 
            return direction_; 
        }
        
        /**
         * @brief Gets the entry date and time.
         * @return Reference to the entry ptime with full precision
         */
        const boost::posix_time::ptime& getEntryDateTime() const 
        { 
            return entryDateTime_; 
        }
        
        /**
         * @brief Gets the exit date and time.
         * @return Reference to the exit ptime with full precision
         */
        const boost::posix_time::ptime& getExitDateTime() const 
        { 
            return exitDateTime_; 
        }
        
        /**
         * @brief Gets the entry price.
         * @return Reference to the entry price with full decimal precision
         */
        const Decimal& getEntryPrice() const 
        { 
            return entryPrice_; 
        }
        
        /**
         * @brief Gets the exit price.
         * @return Reference to the exit price with full decimal precision
         */
        const Decimal& getExitPrice() const 
        { 
            return exitPrice_; 
        }
        
        /**
         * @brief Gets the percentage return.
         * @return Reference to the percentage return with full decimal precision
         */
        const Decimal& getPercentReturn() const 
        { 
            return percentReturn_; 
        }
        
        /**
         * @brief Gets the number of bars the position was held.
         * @return Number of bars in position
         */
        int getBarsInPosition() const 
        { 
            return barsInPosition_; 
        }
        
        /**
         * @brief Extracts the entry date from the entry date-time.
         * 
         * @details
         * Converts the full precision entry date-time to a date for comparison
         * with external systems that may only provide date-level precision.
         * 
         * @return Entry date extracted from the entry date-time
         */
        boost::gregorian::date getEntryDate() const
        {
            return entryDateTime_.date();
        }
        
        /**
         * @brief Extracts the exit date from the exit date-time.
         * 
         * @details
         * Converts the full precision exit date-time to a date for comparison
         * with external systems that may only provide date-level precision.
         * 
         * @return Exit date extracted from the exit date-time
         */
        boost::gregorian::date getExitDate() const
        {
            return exitDateTime_.date();
        }
        
        /**
         * @brief Determines if this trade matches an external trade within specified tolerances.
         * 
         * @details
         * Performs comprehensive comparison against an external trade using configurable
         * tolerances for dates, prices, and returns. The matching algorithm considers
         * multiple criteria to determine trade equivalence.
         * 
         * @param external External trade to compare against
         * @param tolerance Comparison tolerance settings
         * @return true if trades match within tolerances, false otherwise
         */
        bool matches(const ExternalTrade<Decimal>& external, const ComparisonTolerance<Decimal>& tolerance) const;
    };
}

#endif