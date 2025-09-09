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

} // namespace filtering
} // namespace palvalidator