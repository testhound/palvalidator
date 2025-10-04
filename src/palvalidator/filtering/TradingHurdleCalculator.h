#pragma once

#include "number.h"
#include "filtering/FilteringTypes.h"

namespace palvalidator
{
  namespace filtering
  {
    using Num = num::DefaultNumber;

    /**
     * @brief Helper class for calculating trading-related required return hurdles
     *
     * This class encapsulates the logic for calculating various types of hurdles
     * used in performance filtering, including risk-free hurdles and cost-based hurdles.
     */
    class TradingHurdleCalculator
    {
    public:
      /**
       * @brief Constructor with risk parameters and cost assumptions
       * @param riskParams Risk parameters including risk-free rate and premium
       * @param costBufferMultiplier Multiplier for cost-based hurdle (default: 1.5)
       * @param slippagePerSide Slippage assumption per side (default: 0.001 = 0.10%)
       */
      TradingHurdleCalculator(
			      const RiskParameters& riskParams,
			      const Num& costBufferMultiplier = Num("1.5"),
			      const Num& slippagePerSide = Num("0.001")
			      );

      /**
       * @brief Calculate the final required return using a provided per-side slippage (proportional)
       * @param annualizedTrades Number of trades per year
       * @param perSideSlippage  Per-side slippage/spread as a proportion (e.g., 0.001 = 0.10%)
       * @return max(risk-free hurdle, cost-based hurdle with buffer), using the supplied per-side slippage
       */
      Num calculateFinalRequiredReturnWithPerSideSlippage(const Num& annualizedTrades,
							  const Num& perSideSlippage) const;

      /**
       * @brief Calculate the risk-free hurdle rate
       * @return Risk-free rate plus risk premium
       */
      Num calculateRiskFreeHurdle() const;

      /**
       * @brief Calculate cost-based required return for a given number of trades
       * @param annualizedTrades Number of trades per year
       * @return Cost-based required return including buffer
       */
      Num calculateCostBasedRequiredReturn(const Num& annualizedTrades) const;

      /**
       * @brief Calculate the final required return hurdle
       * @param annualizedTrades Number of trades per year
       * @return The higher of cost-based or risk-free hurdle
       */
      Num calculateFinalRequiredReturn(const Num& annualizedTrades) const;

      /**
       * @brief Get the risk-free rate
       * @return Risk-free rate from parameters
       */
      const Num& getRiskFreeRate() const
      {
        return mRiskParams.riskFreeRate;
      }

      /**
       * @brief Get the risk premium
       * @return Risk premium from parameters
       */
      const Num& getRiskPremium() const
      {
        return mRiskParams.riskPremium;
      }

      /**
       * @brief Get the cost buffer multiplier
       * @return Cost buffer multiplier
       */
      const Num& getCostBufferMultiplier() const
      {
        return mCostBufferMultiplier;
      }

      /**
       * @brief Get the slippage per side
       * @return Slippage assumption per side
       */
      const Num& getSlippagePerSide() const
      {
        return mSlippagePerSide;
      }

    private:
      /**
       * @brief Calculate annualized cost hurdle from trades
       * @param annualizedTrades Number of trades per year
       * @return Annualized cost hurdle before buffer
       */
      Num calculateAnnualizedCostHurdle(const Num& annualizedTrades) const;

    private:
      RiskParameters mRiskParams;           ///< Risk parameters
      Num mCostBufferMultiplier;            ///< Multiplier for cost-based hurdle
      Num mSlippagePerSide;                 ///< Slippage assumption per side
      Num mSlippagePerRoundTrip;            ///< Calculated slippage per round trip
    };
  } // namespace filtering
} // namespace palvalidator
