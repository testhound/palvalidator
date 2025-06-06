// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __LOG_PAL_PATTERN_H
#define __LOG_PAL_PATTERN_H 1

#include <ostream>
#include <memory>
#include "PalAst.h"

namespace mkc_timeseries
{
  using std::shared_ptr;

  class LogPalPattern
  {
  public:
    static void LogPattern (std::shared_ptr<PriceActionLabPattern> pattern,
			    std::ostream& outputFileStream);
  private:
    LogPalPattern();

  private:
    static void LogPatternDescription (std::shared_ptr<PatternDescription> desc,
				       std::ostream& outputFileStream);
    static void LogExpression (std::shared_ptr<PatternExpression> expression,
          std::ostream& outputFileStream);
    static void LogPriceBarExpr (std::shared_ptr<PriceBarReference> barReference,
     std::ostream& outputFileStream);
    static void LogMarketExpression (std::shared_ptr<MarketEntryExpression> expression,
         std::ostream& outputFileStream);
    static void LogProfitTarget (std::shared_ptr<ProfitTargetInPercentExpression> expression,
     std::ostream& outputFileStream);
    static void LogStopLoss (std::shared_ptr<StopLossInPercentExpression> expression,
        std::ostream& outputFileStream);
    static void LogPatternSeparator(std::ostream& outputFileStream);
  };


}

#endif
