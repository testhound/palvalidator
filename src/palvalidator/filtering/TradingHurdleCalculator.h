#pragma once

#include "number.h"
#include "filtering/FilteringTypes.h"
#include <optional>

namespace palvalidator
{
  namespace filtering
  {
    using Num = num::DefaultNumber;

    /**
     * @brief Helper class for calculating trading-related cost hurdles.
     *
     * This class encapsulates the logic for calculating the total annualized
     * trading spread cost, which serves as a hurdle for strategy validation.
     */
    class TradingHurdleCalculator
    {
    public:
      /**
       * @brief Constructor with cost assumptions.
       * @param slippagePerSide Slippage assumption per side (default: 0.001 = 0.10%).
       */
      explicit TradingHurdleCalculator(const Num& slippagePerSide = Num("0.001"));

      /**
       * @brief Calculates the total annualized trading spread cost.
       *
       * This method computes the cost hurdle based on the number of trades,
       * the base slippage, and optional out-of-sample (OOS) spread statistics,
       * which can provide a more realistic, data-driven cost estimate.
       *
       * @param annualizedTrades The number of annualized trades for the strategy.
       * @param oosSpreadStats Optional OOS statistics (mean and Qn of spreads)
       *                       to calibrate the cost. If not provided, the base
       *                       slippage assumption is used.
       * @return The total annualized trading spread cost.
       */
      Num calculateTradingSpreadCost(
          const Num& annualizedTrades,
          const std::optional<OOSSpreadStats>& oosSpreadStats) const;

    private:
      Num mSlippagePerSide;      ///< Default slippage assumption per side.
      Num mSlippagePerRoundTrip; ///< Calculated default slippage per round trip.
    };
  } // namespace filtering
} // namespace palvalidator
