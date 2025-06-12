#include "PalStrategyTestHelpers.h"
#include "DecimalConstants.h"

using namespace mkc_timeseries;

PatternDescription *
createDescription (const std::string& fileName, unsigned int index, unsigned long indexDate,
		   const std::string& percLong, const std::string& percShort,
		   unsigned int numTrades, unsigned int consecutiveLosses)
{
  auto percentLong = std::make_shared<DecimalType>(createDecimal(percLong));
  auto percentShort = std::make_shared<DecimalType>(createDecimal(percShort));

  return new PatternDescription ((char *) fileName.c_str(), index, indexDate, percentLong, percentShort,
  		 numTrades, consecutiveLosses);
}

std::shared_ptr<LongMarketEntryOnOpen>
createLongOnOpen()
{
  return std::make_shared<LongMarketEntryOnOpen>();
}

std::shared_ptr<ShortMarketEntryOnOpen>
createShortOnOpen()
{
  return std::make_shared<ShortMarketEntryOnOpen>();
}

std::shared_ptr<LongSideProfitTargetInPercent>
createLongProfitTarget(const std::string& targetPct)
{
  return std::make_shared<LongSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

std::shared_ptr<LongSideStopLossInPercent>
createLongStopLoss(const std::string& targetPct)
{
  return std::make_shared<LongSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

std::shared_ptr<ShortSideProfitTargetInPercent>
createShortProfitTarget(const std::string& targetPct)
{
  return std::make_shared<ShortSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

std::shared_ptr<ShortSideStopLossInPercent>
createShortStopLoss(const std::string& targetPct)
{
  return std::make_shared<ShortSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}