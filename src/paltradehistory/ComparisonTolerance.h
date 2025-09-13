// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, January 2025
//

#ifndef __COMPARISON_TOLERANCE_H
#define __COMPARISON_TOLERANCE_H 1

#include <boost/date_time.hpp>
#include "number.h"

namespace mkc_timeseries
{
    /**
     * @brief Configuration class for trade comparison tolerances and matching criteria.
     * 
     * @details
     * The ComparisonTolerance class encapsulates all configurable parameters used when
     * comparing trades between PAL-generated results and external backtesting platforms.
     * This class enables flexible matching by allowing tolerances for dates, prices,
     * and returns, accommodating differences in execution timing, price precision,
     * and calculation methods between different trading systems.
     * 
     * Key objectives:
     * - Provide configurable tolerance levels for all comparison criteria
     * - Enable flexible matching strategies (exact, fuzzy, weighted)
     * - Support different precision requirements for various data types
     * - Allow customization for different external platform characteristics
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    class ComparisonTolerance
    {
    private:
        int dateTolerance_;                     ///< Maximum days difference for date matching
        Decimal priceTolerance_;                ///< Maximum price difference (absolute)
        Decimal priceTolerancePercent_;         ///< Maximum price difference (percentage)
        Decimal returnTolerance_;               ///< Maximum return difference (absolute)
        Decimal returnTolerancePercent_;        ///< Maximum return difference (percentage)
        bool useAbsolutePriceTolerance_;        ///< Whether to use absolute price tolerance
        bool usePercentagePriceTolerance_;      ///< Whether to use percentage price tolerance
        bool useAbsoluteReturnTolerance_;       ///< Whether to use absolute return tolerance
        bool usePercentageReturnTolerance_;     ///< Whether to use percentage return tolerance
        bool requireExactSymbolMatch_;          ///< Whether symbol must match exactly
        bool requireExactDirectionMatch_;       ///< Whether direction must match exactly
        Decimal minimumMatchScore_;             ///< Minimum weighted score for match acceptance

    public:
        /**
         * @brief Constructs a ComparisonTolerance with default strict matching criteria.
         * 
         * @details
         * Default configuration provides strict matching suitable for high-precision
         * validation scenarios. All tolerances are set to zero, requiring exact matches
         * for all criteria except dates (1-day tolerance for execution timing differences).
         */
        ComparisonTolerance()
            : dateTolerance_(0),
              priceTolerance_(Decimal("0.01")),
              priceTolerancePercent_(Decimal("0.1")),
              returnTolerance_(Decimal("0.01")),
              returnTolerancePercent_(Decimal("0.1")),
              useAbsolutePriceTolerance_(true),
              usePercentagePriceTolerance_(false),
              useAbsoluteReturnTolerance_(true),
              usePercentageReturnTolerance_(false),
              requireExactSymbolMatch_(true),
              requireExactDirectionMatch_(true),
              minimumMatchScore_(Decimal("0.8"))
        {
        }

        /**
         * @brief Constructs a ComparisonTolerance with custom tolerance settings.
         * 
         * @param dateTolerance Maximum days difference for date matching
         * @param priceTolerance Maximum absolute price difference
         * @param returnTolerance Maximum absolute return difference
         * @param minimumMatchScore Minimum weighted score for match acceptance
         */
        ComparisonTolerance(int dateTolerance, const Decimal& priceTolerance,
                           const Decimal& returnTolerance, const Decimal& minimumMatchScore)
            : dateTolerance_(dateTolerance),
              priceTolerance_(priceTolerance),
              priceTolerancePercent_(Decimal("0.1")),
              returnTolerance_(returnTolerance),
              returnTolerancePercent_(Decimal("0.1")),
              useAbsolutePriceTolerance_(true),
              usePercentagePriceTolerance_(false),
              useAbsoluteReturnTolerance_(true),
              usePercentageReturnTolerance_(false),
              requireExactSymbolMatch_(true),
              requireExactDirectionMatch_(true),
              minimumMatchScore_(minimumMatchScore)
        {
        }

        /**
         * @brief Gets the maximum allowed date difference in days.
         * @return Maximum days difference for considering dates as matching
         */
        int getDateTolerance() const 
        { 
            return dateTolerance_; 
        }

        /**
         * @brief Sets the maximum allowed date difference in days.
         * @param tolerance Maximum days difference for date matching
         */
        void setDateTolerance(int tolerance) 
        { 
            dateTolerance_ = tolerance; 
        }

        /**
         * @brief Gets the absolute price tolerance.
         * @return Maximum absolute price difference for matching
         */
        const Decimal& getPriceTolerance() const 
        { 
            return priceTolerance_; 
        }

        /**
         * @brief Sets the absolute price tolerance.
         * @param tolerance Maximum absolute price difference for matching
         */
        void setPriceTolerance(const Decimal& tolerance) 
        { 
            priceTolerance_ = tolerance; 
        }

        /**
         * @brief Gets the percentage price tolerance.
         * @return Maximum percentage price difference for matching
         */
        const Decimal& getPriceTolerancePercent() const 
        { 
            return priceTolerancePercent_; 
        }

        /**
         * @brief Sets the percentage price tolerance.
         * @param tolerance Maximum percentage price difference for matching
         */
        void setPriceTolerancePercent(const Decimal& tolerance) 
        { 
            priceTolerancePercent_ = tolerance; 
        }

        /**
         * @brief Gets the absolute return tolerance.
         * @return Maximum absolute return difference for matching
         */
        const Decimal& getReturnTolerance() const 
        { 
            return returnTolerance_; 
        }

        /**
         * @brief Sets the absolute return tolerance.
         * @param tolerance Maximum absolute return difference for matching
         */
        void setReturnTolerance(const Decimal& tolerance) 
        { 
            returnTolerance_ = tolerance; 
        }

        /**
         * @brief Gets the percentage return tolerance.
         * @return Maximum percentage return difference for matching
         */
        const Decimal& getReturnTolerancePercent() const 
        { 
            return returnTolerancePercent_; 
        }

        /**
         * @brief Sets the percentage return tolerance.
         * @param tolerance Maximum percentage return difference for matching
         */
        void setReturnTolerancePercent(const Decimal& tolerance) 
        { 
            returnTolerancePercent_ = tolerance; 
        }

        /**
         * @brief Gets whether absolute price tolerance is enabled.
         * @return true if absolute price tolerance should be used
         */
        bool getUseAbsolutePriceTolerance() const 
        { 
            return useAbsolutePriceTolerance_; 
        }

        /**
         * @brief Sets whether to use absolute price tolerance.
         * @param use true to enable absolute price tolerance checking
         */
        void setUseAbsolutePriceTolerance(bool use) 
        { 
            useAbsolutePriceTolerance_ = use; 
        }

        /**
         * @brief Gets whether percentage price tolerance is enabled.
         * @return true if percentage price tolerance should be used
         */
        bool getUsePercentagePriceTolerance() const 
        { 
            return usePercentagePriceTolerance_; 
        }

        /**
         * @brief Sets whether to use percentage price tolerance.
         * @param use true to enable percentage price tolerance checking
         */
        void setUsePercentagePriceTolerance(bool use) 
        { 
            usePercentagePriceTolerance_ = use; 
        }

        /**
         * @brief Gets whether absolute return tolerance is enabled.
         * @return true if absolute return tolerance should be used
         */
        bool getUseAbsoluteReturnTolerance() const 
        { 
            return useAbsoluteReturnTolerance_; 
        }

        /**
         * @brief Sets whether to use absolute return tolerance.
         * @param use true to enable absolute return tolerance checking
         */
        void setUseAbsoluteReturnTolerance(bool use) 
        { 
            useAbsoluteReturnTolerance_ = use; 
        }

        /**
         * @brief Gets whether percentage return tolerance is enabled.
         * @return true if percentage return tolerance should be used
         */
        bool getUsePercentageReturnTolerance() const 
        { 
            return usePercentageReturnTolerance_; 
        }

        /**
         * @brief Sets whether to use percentage return tolerance.
         * @param use true to enable percentage return tolerance checking
         */
        void setUsePercentageReturnTolerance(bool use) 
        { 
            usePercentageReturnTolerance_ = use; 
        }

        /**
         * @brief Gets whether exact symbol matching is required.
         * @return true if symbols must match exactly
         */
        bool getRequireExactSymbolMatch() const 
        { 
            return requireExactSymbolMatch_; 
        }

        /**
         * @brief Sets whether exact symbol matching is required.
         * @param require true to require exact symbol matching
         */
        void setRequireExactSymbolMatch(bool require) 
        { 
            requireExactSymbolMatch_ = require; 
        }

        /**
         * @brief Gets whether exact direction matching is required.
         * @return true if trade directions must match exactly
         */
        bool getRequireExactDirectionMatch() const 
        { 
            return requireExactDirectionMatch_; 
        }

        /**
         * @brief Sets whether exact direction matching is required.
         * @param require true to require exact direction matching
         */
        void setRequireExactDirectionMatch(bool require) 
        { 
            requireExactDirectionMatch_ = require; 
        }

        /**
         * @brief Gets the minimum match score for acceptance.
         * @return Minimum weighted score (0.0 to 1.0) for considering trades as matching
         */
        const Decimal& getMinimumMatchScore() const 
        { 
            return minimumMatchScore_; 
        }

        /**
         * @brief Sets the minimum match score for acceptance.
         * @param score Minimum weighted score (0.0 to 1.0) for match acceptance
         */
        void setMinimumMatchScore(const Decimal& score) 
        { 
            minimumMatchScore_ = score; 
        }

        /**
         * @brief Creates a preset tolerance configuration for strict matching.
         * 
         * @details
         * Strict matching requires exact matches for all criteria except dates
         * (1-day tolerance) and allows minimal price/return differences.
         * 
         * @return ComparisonTolerance configured for strict matching
         */
        static ComparisonTolerance<Decimal> createStrictTolerance()
        {
            ComparisonTolerance<Decimal> tolerance;
            tolerance.setDateTolerance(1);
            tolerance.setPriceTolerance(Decimal("0.01"));
            tolerance.setReturnTolerance(Decimal("0.01"));
            tolerance.setMinimumMatchScore(Decimal("0.95"));
            return tolerance;
        }

        /**
         * @brief Creates a preset tolerance configuration for relaxed matching.
         * 
         * @details
         * Relaxed matching allows larger tolerances suitable for comparing
         * results across different platforms with varying precision and timing.
         * 
         * @return ComparisonTolerance configured for relaxed matching
         */
        static ComparisonTolerance<Decimal> createRelaxedTolerance()
        {
            ComparisonTolerance<Decimal> tolerance;
            tolerance.setDateTolerance(3);
            tolerance.setPriceTolerance(Decimal("0.05"));
            tolerance.setPriceTolerancePercent(Decimal("0.5"));
            tolerance.setReturnTolerance(Decimal("0.05"));
            tolerance.setReturnTolerancePercent(Decimal("0.5"));
            tolerance.setUsePercentagePriceTolerance(true);
            tolerance.setUsePercentageReturnTolerance(true);
            tolerance.setMinimumMatchScore(Decimal("0.7"));
            return tolerance;
        }
    };
}

#endif