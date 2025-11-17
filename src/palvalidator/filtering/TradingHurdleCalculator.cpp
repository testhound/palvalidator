#include "TradingHurdleCalculator.h"
#include "DecimalConstants.h"
#include <algorithm>

namespace palvalidator
{
    namespace filtering
    {

        TradingHurdleCalculator::TradingHurdleCalculator(const Num& slippagePerSide)
            : mSlippagePerSide(slippagePerSide),
              mSlippagePerRoundTrip(slippagePerSide * mkc_timeseries::DecimalConstants<Num>::DecimalTwo)
        {
        }

      Num
      TradingHurdleCalculator::calculateTradingSpreadCost(const Num& annualizedTrades,
							  const std::optional<OOSSpreadStats>& oosSpreadStats) const
      {
	if (!oosSpreadStats)
	    // Use precomputed default slippage per round trip when no OOS stats are provided
	    return annualizedTrades * mSlippagePerRoundTrip;

	const Num roundTrip = oosSpreadStats->mean;
	return annualizedTrades * roundTrip;
      }
    } // namespace filtering
} // namespace palvalidator
