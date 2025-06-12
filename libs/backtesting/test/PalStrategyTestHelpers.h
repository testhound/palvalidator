#ifndef __PAL_STRATEGY_TEST_HELPERS_H
#define __PAL_STRATEGY_TEST_HELPERS_H 1

#include "PalAst.h"
#include "PalStrategy.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

// Helper function declarations for creating test patterns and components
PatternDescription *
createDescription (const std::string& fileName, unsigned int index, unsigned long indexDate, 
		   const std::string& percLong, const std::string& percShort,
		   unsigned int numTrades, unsigned int consecutiveLosses);

std::shared_ptr<LongMarketEntryOnOpen>
createLongOnOpen();

std::shared_ptr<ShortMarketEntryOnOpen>
createShortOnOpen();

std::shared_ptr<LongSideProfitTargetInPercent>
createLongProfitTarget(const std::string& targetPct);

std::shared_ptr<LongSideStopLossInPercent>
createLongStopLoss(const std::string& targetPct);

std::shared_ptr<ShortSideProfitTargetInPercent>
createShortProfitTarget(const std::string& targetPct);

std::shared_ptr<ShortSideStopLossInPercent>
createShortStopLoss(const std::string& targetPct);

#endif