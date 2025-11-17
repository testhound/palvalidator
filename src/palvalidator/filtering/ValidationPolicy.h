#pragma once

#include "number.h"
#include "filtering/FilteringTypes.h"

namespace palvalidator
{
    namespace filtering
    {

        class ValidationPolicy
        {
        public:
            using Num = num::DefaultNumber;

            /**
             * @brief Construct a new Validation Policy object
             *
             * @param tradingSpreadCost The annualized cost of trading spreads.
             */
            explicit ValidationPolicy(const Num& tradingSpreadCost);

            /**
             * @brief Evaluates if a strategy's performance meets the passing criteria.
             *
             * @param lowerBound The bootstrapped lower bound of the strategy's returns.
             * @return true if the strategy passes, false otherwise.
             */
            bool hasPassed(const Num& lowerBound) const;

            /**
             * @brief Gets the minimum required return for a strategy to pass.
             *
             * @return The trading spread cost.
             */
            const Num& getRequiredReturn() const;

        private:
            Num mTradingSpreadCost;
        };

    } // namespace filtering
} // namespace palvalidator