#pragma once

#include "number.h"
#include "filtering/FilteringTypes.h"

namespace palvalidator
{
  namespace filtering
  {
    namespace meta
    {

      /**
       * Legacy hurdle calculator kept specifically for Meta strategies.
       * This preserves the higher bar: max(risk-free + premium, buffered cost).
       *
       * API intentionally mirrors the pre-redesign TradingHurdleCalculator so that
       * MetaStrategyAnalyzer.cpp does not need functional rewrites.
       */
      class MetaTradingHurdleCalculator
      {
      public:
	using Num = num::DefaultNumber;

	MetaTradingHurdleCalculator(const RiskParameters& riskParams,
				    const Num& costBufferMultiplier = Num("1.5"),
				    const Num& slippagePerSide = Num("0.001"));

	// Legacy API preserved for Meta usage:
	Num calculateFinalRequiredReturnWithPerSideSlippage(const Num& annualizedTrades,
							    const Num& perSideSlippage) const;
	Num calculateRiskFreeHurdle() const;
	Num calculateCostBasedRequiredReturn(const Num& annualizedTrades) const;
	Num calculateFinalRequiredReturn(const Num& annualizedTrades) const;

	const Num& getRiskFreeRate() const { return mRiskParams.riskFreeRate; }
	const Num& getRiskPremium()  const { return mRiskParams.riskPremium; }
	const Num& getCostBufferMultiplier() const { return mCostBufferMultiplier; }
	const Num& getSlippagePerSide() const { return mSlippagePerSide; }

      private:
	Num calculateAnnualizedCostHurdle(const Num& annualizedTrades) const;

      private:
	RiskParameters mRiskParams;
	Num mCostBufferMultiplier;
	Num mSlippagePerSide;
	Num mSlippagePerRoundTrip;
      };
    } // namespace meta
  } // namespace filtering
} // namespace palvalidator
