#include "TradingHurdleCalculator.h"
#include "DecimalConstants.h"
#include <algorithm>
#include <iostream>

namespace palvalidator
{
  namespace filtering
  {
    TradingHurdleCalculator::TradingHurdleCalculator(
						     const RiskParameters& riskParams,
						     const Num& costBufferMultiplier,
						     const Num& slippagePerSide)
      : mRiskParams(riskParams),
	mCostBufferMultiplier(costBufferMultiplier),
	mSlippagePerSide(slippagePerSide),
	mSlippagePerRoundTrip(slippagePerSide * mkc_timeseries::DecimalConstants<Num>::DecimalTwo)
    {
    }

    Num TradingHurdleCalculator::calculateRiskFreeHurdle() const
    {
      return mRiskParams.riskFreeRate + mRiskParams.riskPremium;
    }

    Num TradingHurdleCalculator::calculateAnnualizedCostHurdle(const Num& annualizedTrades) const
    {
      return annualizedTrades * mSlippagePerRoundTrip;
    }

    Num TradingHurdleCalculator::calculateCostBasedRequiredReturn(const Num& annualizedTrades) const
    {
      const Num annualizedCostHurdle = calculateAnnualizedCostHurdle(annualizedTrades);
      return annualizedCostHurdle * mCostBufferMultiplier;
    }

    Num TradingHurdleCalculator::calculateFinalRequiredReturn(const Num& annualizedTrades) const
    {
      const Num riskFreeHurdle = calculateRiskFreeHurdle();
      const Num costBasedRequiredReturn = calculateCostBasedRequiredReturn(annualizedTrades);
      return std::max(costBasedRequiredReturn, riskFreeHurdle);
    }

    Num TradingHurdleCalculator::calculateFinalRequiredReturnWithPerSideSlippage(
										 const Num& annualizedTrades,
										 const Num& perSideSlippage) const
    {
      // risk-free part unchanged
      const Num rf = calculateRiskFreeHurdle();

      // cost part with override:
      // per round-trip = 2 * per-side
      const Num two = mkc_timeseries::DecimalConstants<Num>::DecimalTwo;
      const Num perRoundTrip = two * perSideSlippage;

      // annualized cost before buffer
      const Num annualizedCost = annualizedTrades * perRoundTrip;

      // apply buffer (same multiplier as in the calculator)
      const Num costReq = annualizedCost * mCostBufferMultiplier;

      return std::max(rf, costReq);
    }
  } // namespace filtering
} // namespace palvalidator
