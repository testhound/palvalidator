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
				  std::ofstream& outputFileStream)
  {
    LogPalPattern::LogPatternDescription (pattern->getPatternDescription(), outputFileStream);
    outputFileStream << std::endl;
    
    outputFileStream << "IF ";
    LogPalPattern::LogExpression (pattern->getPatternExpression().get(), outputFileStream);

    LogPalPattern::LogMarketExpression (pattern->getMarketEntry(), outputFileStream);
    LogPalPattern::LogProfitTarget (pattern->getProfitTarget(), outputFileStream);
    LogPalPattern::LogStopLoss (pattern->getStopLoss(), outputFileStream);
    LogPalPattern::LogPatternSeparator(outputFileStream);
    outputFileStream << std::endl;
  }

  void LogPalPattern::LogPatternDescription (std::shared_ptr<PatternDescription> desc,
					     std::ofstream& outputFileStream)
  {
    outputFileStream << "{File:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
  }

  void LogPalPattern::LogExpression (PatternExpression *expression,
				     std::ofstream& outputFileStream)
  {
    if (AndExpr *pAnd = dynamic_cast<AndExpr*>(expression))
	{
	  LogPalPattern::LogExpression (pAnd->getLHS(), outputFileStream);
	  outputFileStream << "AND ";
	  LogPalPattern::LogExpression (pAnd->getRHS(), outputFileStream);
	}
    else if (GreaterThanExpr *pGreaterThan = dynamic_cast<GreaterThanExpr*>(expression))
	{
	  LogPalPattern::LogPriceBarExpr (pGreaterThan->getLHS(), outputFileStream);
	  outputFileStream << " > ";
	  LogPalPattern::LogPriceBarExpr (pGreaterThan->getRHS(), outputFileStream);
	  outputFileStream << std::endl;
	}
  }

  void LogPalPattern::LogPriceBarExpr (PriceBarReference *barReference,
				       std::ofstream& outputFileStream)
  {
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

	default:
	  throw_assert (false, "LogPriceBarExpr: PriceBarRefererence is not OHLC");
	}
  }

  void LogPalPattern::LogMarketExpression (MarketEntryExpression *expression,
					   std::ofstream& outputFileStream)
  {
    if (expression->isLongPattern())
      outputFileStream << "THEN BUY NEXT BAR ON THE OPEN WITH" << std::endl;
    else
      outputFileStream << "THEN SELL NEXT BAR ON THE OPEN WITH" << std::endl;
  }

  void LogPalPattern::LogProfitTarget (ProfitTargetInPercentExpression *expression,
				       std::ofstream& outputFileStream)
  {
    decimal7 *target = expression->getProfitTarget();

    if (expression->isLongSideProfitTarget())
      outputFileStream << "PROFIT TARGET AT ENTRY PRICE + " << *target << " %" << std::endl;
    else
      outputFileStream << "PROFIT TARGET AT ENTRY PRICE - " << *target << " %" << std::endl;
  }

  void LogPalPattern::LogStopLoss (StopLossInPercentExpression *expression,
				   std::ofstream& outputFileStream)
  {
    decimal7 *stop = expression->getStopLoss();

    if (expression->isLongSideStopLoss())
      outputFileStream << "AND STOP LOSS AT ENTRY PRICE - " << *stop << " %" <<std::endl;
    else
      outputFileStream << "AND STOP LOSS AT ENTRY PRICE + " << *stop << " %" <<std::endl;
  }

  void LogPalPattern::LogPatternSeparator(std::ofstream& outputFileStream)
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
