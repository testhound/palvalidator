// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include "LogPalPattern.h"
#include "ThrowAssert.hpp"

namespace mkc_timeseries
{
  void LogPalPattern::LogPattern (shared_ptr<PriceActionLabPattern> pattern,
				  std::ostream& outputFileStream)
  {
    LogPalPattern::LogPatternDescription (pattern->getPatternDescription(), outputFileStream);
    outputFileStream << std::endl;
    
    outputFileStream << "IF ";
    LogPalPattern::LogExpression (pattern->getPatternExpression(), outputFileStream);

    LogPalPattern::LogMarketExpression (pattern->getMarketEntry(), outputFileStream);
    LogPalPattern::LogProfitTarget (pattern->getProfitTarget(), outputFileStream);
    LogPalPattern::LogStopLoss (pattern->getStopLoss(), outputFileStream);
    LogPalPattern::LogPatternSeparator(outputFileStream);
    outputFileStream << std::endl;
  }

  void LogPalPattern::LogPatternDescription (std::shared_ptr<PatternDescription> desc,
					     std::ostream& outputFileStream)
  {
    outputFileStream << "{File:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
    << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLongShared())
                         << "%  PS: " << *(desc->getPercentShortShared()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
  }

  void LogPalPattern::LogExpression (std::shared_ptr<PatternExpression> expression,
  		     std::ostream& outputFileStream)
  {
    if (!expression) {
      outputFileStream << "[NULL EXPRESSION]";
      return;
    }
    
    if (auto pAnd = std::dynamic_pointer_cast<AndExpr>(expression))
 {
   LogPalPattern::LogExpression (pAnd->getLHSShared(), outputFileStream);
   outputFileStream << "AND ";
   LogPalPattern::LogExpression (pAnd->getRHSShared(), outputFileStream);
 }
    else if (auto pGreaterThan = std::dynamic_pointer_cast<GreaterThanExpr>(expression))
 {
   LogPalPattern::LogPriceBarExpr (pGreaterThan->getLHSShared(), outputFileStream);
   outputFileStream << " > ";
   LogPalPattern::LogPriceBarExpr (pGreaterThan->getRHSShared(), outputFileStream);
   outputFileStream << std::endl;
 }
  }

  void LogPalPattern::LogPriceBarExpr (std::shared_ptr<PriceBarReference> barReference,
  		       std::ostream& outputFileStream)
  {
    if (!barReference) {
      outputFileStream << "[NULL PRICE BAR REFERENCE]";
      return;
    }
    
    switch (barReference->getReferenceType())
 {
 case PriceBarReference::OPEN:
   outputFileStream << "OPEN OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::HIGH:
   outputFileStream << "HIGH OF " << barReference->getBarOffset() << " BARS AGO";
   break;
   
 case PriceBarReference::LOW:
   outputFileStream << "LOW OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::CLOSE:
   outputFileStream << "CLOSE OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::VOLUME:
   outputFileStream << "VOLUME OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::MEANDER:
   outputFileStream << "MEANDER OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::IBS1:
   outputFileStream << "IBS1 OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::IBS2:
   outputFileStream << "IBS2 OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 case PriceBarReference::IBS3:
   outputFileStream << "IBS3 OF " << barReference->getBarOffset() << " BARS AGO";
   break;

 default:
   throw_assert (false, "LogPriceBarExpr: PriceBarRefererence is not OHLC");
 }
  }

  void LogPalPattern::LogMarketExpression (std::shared_ptr<MarketEntryExpression> expression,
  			   std::ostream& outputFileStream)
  {
    if (!expression) {
      outputFileStream << "THEN [NULL MARKET ENTRY] WITH" << std::endl;
      return;
    }
    
    if (expression->isLongPattern())
      outputFileStream << "THEN BUY NEXT BAR ON THE OPEN WITH" << std::endl;
    else
      outputFileStream << "THEN SELL NEXT BAR ON THE OPEN WITH" << std::endl;
  }

  void LogPalPattern::LogProfitTarget (std::shared_ptr<ProfitTargetInPercentExpression> expression,
  		       std::ostream& outputFileStream)
  {
    if (!expression) {
      outputFileStream << "PROFIT TARGET [NULL]" << std::endl;
      return;
    }
    
    auto target = expression->getProfitTargetShared();
    if (!target) {
      outputFileStream << "PROFIT TARGET [NULL VALUE]" << std::endl;
      return;
    }

    if (expression->isLongSideProfitTarget())
      outputFileStream << "PROFIT TARGET AT ENTRY PRICE + " << *target << " %" << std::endl;
    else
      outputFileStream << "PROFIT TARGET AT ENTRY PRICE - " << *target << " %" << std::endl;
  }

  void LogPalPattern::LogStopLoss (std::shared_ptr<StopLossInPercentExpression> expression,
  		   std::ostream& outputFileStream)
  {
    if (!expression) {
      outputFileStream << "AND STOP LOSS [NULL]" << std::endl;
      return;
    }
    
    auto stop = expression->getStopLossShared();
    if (!stop) {
      outputFileStream << "AND STOP LOSS [NULL VALUE]" << std::endl;
      return;
    }

    if (expression->isLongSideStopLoss())
      outputFileStream << "AND STOP LOSS AT ENTRY PRICE - " << *stop << " %" <<std::endl;
    else
      outputFileStream << "AND STOP LOSS AT ENTRY PRICE + " << *stop << " %" <<std::endl;
  }

  void LogPalPattern::LogPatternSeparator(std::ostream& outputFileStream)
  {
    outputFileStream << "--------------------";
    outputFileStream << "--------------------";
    outputFileStream << "--------------------";
    outputFileStream << "--------------------";
    outputFileStream << "--------------------";
    outputFileStream << "--------------------";
    outputFileStream << "----------" << std::endl;
  }
}
