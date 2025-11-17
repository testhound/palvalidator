#include "ValidationPolicy.h"
#include "DecimalConstants.h"

namespace palvalidator
{
    namespace filtering
    {

        ValidationPolicy::ValidationPolicy(const Num& tradingSpreadCost)
            : mTradingSpreadCost(tradingSpreadCost)
        {
        }

        bool ValidationPolicy::hasPassed(const Num& lowerBound) const
        {
            // New simplified validation criteria:
            // 1. Lower bound must be greater than zero.
            // 2. Lower bound must be greater than the trading spread costs.
            return (lowerBound > mkc_timeseries::DecimalConstants<Num>::DecimalZero) &&
                   (lowerBound > mTradingSpreadCost);
        }

        const Num& ValidationPolicy::getRequiredReturn() const
        {
            return mTradingSpreadCost;
        }

    } // namespace filtering
} // namespace palvalidator