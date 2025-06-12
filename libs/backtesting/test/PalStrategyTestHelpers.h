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

LongMarketEntryOnOpen *
createLongOnOpen();

ShortMarketEntryOnOpen *
createShortOnOpen();

LongSideProfitTargetInPercent *
createLongProfitTarget(const std::string& targetPct);

LongSideStopLossInPercent *
createLongStopLoss(const std::string& targetPct);

ShortSideProfitTargetInPercent *
createShortProfitTarget(const std::string& targetPct);

ShortSideStopLossInPercent *
createShortStopLoss(const std::string& targetPct);

#endif