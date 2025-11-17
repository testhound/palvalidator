#include "filtering/MetaTradingHurdleCalculator.h"
#include "DecimalConstants.h"
#include <algorithm>

namespace palvalidator
{
  namespace filtering
  {
    namespace meta
    {
      MetaTradingHurdleCalculator::MetaTradingHurdleCalculator(
							       const RiskParameters& riskParams,
							       const Num& costBufferMultiplier,
							       const Num& slippagePerSide)
	: mRiskParams(riskParams),
	  mCostBufferMultiplier(costBufferMultiplier),
	  mSlippagePerSide(slippagePerSide),
	  mSlippagePerRoundTrip(slippagePerSide * mkc_timeseries::DecimalConstants<Num>::DecimalTwo)
      {}

      Num MetaTradingHurdleCalculator::calculateRiskFreeHurdle() const
      {
	return mRiskParams.riskFreeRate + mRiskParams.riskPremium;
      }

      Num MetaTradingHurdleCalculator::calculateAnnualizedCostHurdle(const Num& annualizedTrades) const
      {
	return annualizedTrades * mSlippagePerRoundTrip;
      }

      Num MetaTradingHurdleCalculator::calculateCostBasedRequiredReturn(const Num& annualizedTrades) const
      {
	const Num annualizedCostHurdle = calculateAnnualizedCostHurdle(annualizedTrades);
	return annualizedCostHurdle * mCostBufferMultiplier;
      }

      Num MetaTradingHurdleCalculator::calculateFinalRequiredReturn(const Num& annualizedTrades) const
      {
	const Num rf  = calculateRiskFreeHurdle();
	const Num cbr = calculateCostBasedRequiredReturn(annualizedTrades);
	return std::max(cbr, rf);
      }

      Num MetaTradingHurdleCalculator::calculateFinalRequiredReturnWithPerSideSlippage(
										       const Num& annualizedTrades,
										       const Num& perSideSlippage) const
      {
	const Num rf = calculateRiskFreeHurdle();

	const Num two = mkc_timeseries::DecimalConstants<Num>::DecimalTwo;
	const Num perRoundTrip = two * perSideSlippage;

	const Num annualizedCost = annualizedTrades * perRoundTrip;
	const Num costReq = annualizedCost * mCostBufferMultiplier;

	return std::max(rf, costReq);
      }

    } // namespace meta
  } // namespace filtering
} // namespace palvalidator
