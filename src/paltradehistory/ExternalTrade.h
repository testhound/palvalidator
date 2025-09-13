// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, January 2025
//

#ifndef __EXTERNAL_TRADE_H
#define __EXTERNAL_TRADE_H 1

#include <string>
#include <boost/date_time.hpp>
#include "number.h"

namespace mkc_timeseries
{
    template <class Decimal> class ComparisonTolerance;
    template <class Decimal> class GeneratedTrade;

    /**
     * @brief Represents a trade from external backtesting platforms for comparison purposes.
     * 
     * @details
     * The ExternalTrade class encapsulates trade data imported from external backtesting
     * platforms such as WealthLab, TradeStation, or other trading systems. This class
     * provides a standardized interface for accessing trade information while preserving
     * the high precision required for accurate financial comparisons.
     * 
     * Key objectives:
     * - Provide standardized access to external trade data
     * - Maintain high precision using Decimal arithmetic for financial calculations
     * - Enable efficient comparison against PAL-generated trades
     * - Support multiple external platform formats through consistent interface
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    class ExternalTrade
    {
    private:
        int position_;                          ///< Position number from external system
        std::string symbol_;                    ///< Security symbol (e.g., "ASML")
        boost::gregorian::date entryDate_;      ///< Entry date from external system
        boost::gregorian::date exitDate_;       ///< Exit date from external system
        Decimal entryPrice_;                    ///< Entry price with full decimal precision
        Decimal exitPrice_;                     ///< Exit price with full decimal precision
        std::string direction_;                 ///< Trade direction ("Long" or "Short")
        Decimal profitPercent_;                 ///< Profit percentage with full decimal precision
        int barsHeld_;                          ///< Number of bars the position was held

    public:
        /**
         * @brief Constructs an ExternalTrade from external backtesting data.
         * 
         * @param position Position number from external system
         * @param symbol Security symbol (e.g., "ASML")
         * @param entryDate Entry date from external system
         * @param exitDate Exit date from external system
         * @param entryPrice Entry price with full decimal precision
         * @param exitPrice Exit price with full decimal precision
         * @param direction Trade direction ("Long" or "Short")
         * @param profitPercent Profit percentage with full decimal precision
         * @param barsHeld Number of bars the position was held
         */
        ExternalTrade(int position, const std::string& symbol, 
                      const boost::gregorian::date& entryDate,
                      const boost::gregorian::date& exitDate,
                      const Decimal& entryPrice, const Decimal& exitPrice,
                      const std::string& direction, const Decimal& profitPercent, int barsHeld)
            : position_(position), symbol_(symbol), entryDate_(entryDate), exitDate_(exitDate),
              entryPrice_(entryPrice), exitPrice_(exitPrice), direction_(direction),
              profitPercent_(profitPercent), barsHeld_(barsHeld)
        {
        }
        
        /**
         * @brief Gets the position number from the external system.
         * @return Position number as assigned by external backtesting platform
         */
        int getPosition() const 
        { 
            return position_; 
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
         * @brief Gets the entry date.
         * @return Reference to the entry date from external system
         */
        const boost::gregorian::date& getEntryDate() const 
        { 
            return entryDate_; 
        }
        
        /**
         * @brief Gets the exit date.
         * @return Reference to the exit date from external system
         */
        const boost::gregorian::date& getExitDate() const 
        { 
            return exitDate_; 
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
         * @brief Gets the trade direction.
         * @return Reference to the direction string ("Long" or "Short")
         */
        const std::string& getDirection() const 
        { 
            return direction_; 
        }
        
        /**
         * @brief Gets the profit percentage.
         * @return Reference to the profit percentage with full decimal precision
         */
        const Decimal& getProfitPercent() const 
        { 
            return profitPercent_; 
        }
        
        /**
         * @brief Gets the number of bars held.
         * @return Number of bars the position was held
         */
        int getBarsHeld() const 
        { 
            return barsHeld_; 
        }
        
        /**
         * @brief Checks if the trade direction matches another direction.
         * 
         * @param otherDirection Direction string to compare against
         * @return true if directions match exactly, false otherwise
         */
        bool matchesDirection(const std::string& otherDirection) const
        {
            return direction_ == otherDirection;
        }
        
        /**
         * @brief Checks if the entry date matches another date within tolerance.
         * 
         * @param otherDate Date to compare against
         * @param toleranceDays Maximum days difference allowed (default: 0 for exact match)
         * @return true if dates match within tolerance, false otherwise
         */
        bool matchesEntryDate(const boost::gregorian::date& otherDate, int toleranceDays = 0) const
        {
            if (toleranceDays == 0)
                return entryDate_ == otherDate;
            
            boost::gregorian::date_duration tolerance(toleranceDays);
            return (otherDate >= (entryDate_ - tolerance)) && (otherDate <= (entryDate_ + tolerance));
        }
        
        /**
         * @brief Checks if the exit date matches another date within tolerance.
         * 
         * @param otherDate Date to compare against
         * @param toleranceDays Maximum days difference allowed (default: 0 for exact match)
         * @return true if dates match within tolerance, false otherwise
         */
        bool matchesExitDate(const boost::gregorian::date& otherDate, int toleranceDays = 0) const
        {
            if (toleranceDays == 0)
                return exitDate_ == otherDate;
            
            boost::gregorian::date_duration tolerance(toleranceDays);
            return (otherDate >= (exitDate_ - tolerance)) && (otherDate <= (exitDate_ + tolerance));
        }

        /**
         * @brief Determines if this external trade matches a generated trade within specified tolerances.
         * 
         * @details
         * Performs comprehensive comparison against a PAL-generated trade using configurable
         * tolerances for dates, prices, and returns. The matching algorithm considers
         * multiple criteria to determine trade equivalence.
         * 
         * @param generated Generated trade to compare against
         * @param tolerance Comparison tolerance settings
         * @return true if trades match within tolerances, false otherwise
         */
        bool matches(const GeneratedTrade<Decimal>& generated, const ComparisonTolerance<Decimal>& tolerance) const;
    };
}

#endif