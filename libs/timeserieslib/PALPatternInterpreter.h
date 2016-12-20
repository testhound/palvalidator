// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PAL_PATTERN_INTERPRETER_H
#define __PAL_PATTERN_INTERPRETER_H 1

#include "PalAst.h"
#include "Security.h"
#include "DecimalConstants.h"
#include <algorithm>

namespace mkc_timeseries
{
  class PalPatternInterpreterException : public std::runtime_error
  {
  public:
    PalPatternInterpreterException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~PalPatternInterpreterException()
      {}
  };

  template <class Decimal> class PALPatternInterpreter
  {
  public:
// For any date there is a single Security::ConstRandomAccessIterator
    // Instead of looking up the iterator each time we have the client pass
    // the iterator. This should save alot of lookup time

    static bool evaluateExpression (PatternExpression *expression, 
				    const std::shared_ptr<Security<Decimal>>& security,
				    typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate)
    {
      if (AndExpr *pAnd = dynamic_cast<AndExpr*>(expression))
	{
	  bool lhsCond = PALPatternInterpreter<Decimal>::evaluateExpression (pAnd->getLHS(),
									  security,
									  iteratorForDate);
	  if (lhsCond == true)
	    return PALPatternInterpreter<Decimal>::evaluateExpression (pAnd->getRHS(),
								    security,
								    iteratorForDate);
	  else
	    return false;
	}
      else if (GreaterThanExpr *pGreaterThan = dynamic_cast<GreaterThanExpr*>(expression))
	{
	  Decimal lhs = PALPatternInterpreter<Decimal>::evaluatePriceBar(pGreaterThan->getLHS(), 
									    security, 
									    iteratorForDate);
	  Decimal rhs = PALPatternInterpreter<Decimal>::evaluatePriceBar(pGreaterThan->getRHS(), 
									    security, 
									    iteratorForDate);
	  return (lhs > rhs);
	}
      else
	throw PalPatternInterpreterException ("PALPatternInterpreter::evaluateExpression Illegal PatternExpression");
    }

  private:

    static const Decimal evaluatePriceBar (PriceBarReference *barReference, 
					   const std::shared_ptr<Security<Decimal>>& security,
						  typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate)
    {
      switch (barReference->getReferenceType())
	{
	case PriceBarReference::OPEN:
	  return security->getOpenValue(iteratorForDate, barReference->getBarOffset());
	  
	case PriceBarReference::HIGH:
	  return security->getHighValue(iteratorForDate, barReference->getBarOffset());
	  
	case PriceBarReference::LOW:
	  return security->getLowValue(iteratorForDate, barReference->getBarOffset());
	  
	case PriceBarReference::CLOSE:
	  return security->getCloseValue(iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::VOLUME:
	  return security->getVolumeValue(iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::MEANDER:
	  return PALPatternInterpreter<Decimal>::Meander (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::VCHARTLOW:
	  return PALPatternInterpreter<Decimal>::ValueChartLow (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::VCHARTHIGH:
	  return PALPatternInterpreter<Decimal>::ValueChartHigh (security, iteratorForDate, barReference->getBarOffset());

	default:
	  throw PalPatternInterpreterException ("PALPatternInterpreter::evaluatePriceBar - unknown PriceBarReference derived class"); 
	}
    }

    static const Decimal Meander(const std::shared_ptr<Security<Decimal>>& security,
				  typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
				  unsigned long offset)
    {
      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
      int i;

      Decimal sum(0);
      Decimal prevClose, currentClose, currentOpen, currentHigh, currentLow;
      Decimal denom(20);
      
      for (i = 0; i <= 4; i++)
	{
	  prevClose = security->getCloseValue (baseIt, i + 1);
	  currentOpen = security->getOpenValue (baseIt, i);
	  currentHigh = security->getHighValue (baseIt, i);
	  currentClose = security->getCloseValue (baseIt, i);
	  currentLow = security->getLowValue (baseIt, i);

	  sum = sum + ((currentOpen - prevClose)/prevClose) + ((currentHigh - prevClose)/prevClose) +
	    ((currentLow - prevClose)/prevClose) + ((currentClose - prevClose)/prevClose);
	}
      Decimal avg( sum / denom);
      return security->getCloseValue (baseIt, 0) * (DecimalConstants<Decimal>::DecimalOne + avg);
    }

    static const Decimal volatilityUnitConstant()
    {
      static Decimal volatilityConstant (num::fromString<Decimal>(std::string("0.20")));

      return volatilityConstant;
    }
    
    static const Decimal ValueChartHigh(const std::shared_ptr<Security<Decimal>>& security,
					typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
					unsigned long offset)
    {
      static Decimal decFive (num::fromString<Decimal>(std::string("5.0")));
	    
      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
      int i;
      
      Decimal prevClose, currentHigh, currentLow, priceAvg, priceAvgSum(DecimalConstants<Decimal>::DecimalZero),
	relativeHigh, averagePrice, currentClose;
      Decimal trueHigh, trueLow, trueRange, trueRangeSum(DecimalConstants<Decimal>::DecimalZero), avgTrueRange, volatilityUnit;
      Decimal closeToCloseRange, highLowRange, range;
      
      for (i = 0; i <= 4; i++)
	{
	  currentClose = security->getCloseValue (baseIt, i);
	  prevClose = security->getCloseValue (baseIt, i + 1);
	  closeToCloseRange = num::abs (currentClose - prevClose);
	  
	  currentHigh = security->getHighValue (baseIt, i);
	  currentLow = security->getLowValue (baseIt, i);
	  highLowRange = currentHigh - currentLow;

	  range = std::max (closeToCloseRange, highLowRange);
	  
	  priceAvg = (currentHigh + currentLow)/DecimalConstants<Decimal>::DecimalTwo;
	  priceAvgSum = priceAvgSum + priceAvg;
	  
	  //trueHigh = std::max (currentHigh, prevClose);
	  //trueLow = std::min (currentLow, prevClose);
	  //trueRange = trueHigh - trueLow;
	  trueRange = range;
	  trueRangeSum = trueRangeSum + trueRange;
	}


      averagePrice = priceAvgSum / decFive;
      relativeHigh = security->getHighValue (baseIt, 0) - averagePrice;
      avgTrueRange = trueRangeSum / decFive;
      volatilityUnit = avgTrueRange * volatilityUnitConstant();

      if (volatilityUnit != DecimalConstants<Decimal>::DecimalZero)
	{
	  Decimal retVal = (relativeHigh / volatilityUnit);
	  //std::cout << "ValueChartHigh = " << retVal << std::endl;
	  return (retVal);
	}
      else
	{
	  //std::cout << "ValueChartHigh = 0" << std::endl;
	  return DecimalConstants<Decimal>::DecimalZero;
	}
    }

    static const Decimal ValueChartLow(const std::shared_ptr<Security<Decimal>>& security,
					typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
					unsigned long offset)
    {
      static Decimal decFive (num::fromString<Decimal>(std::string("5.0")));
	    
      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
      int i;
      
      Decimal prevClose, currentHigh, currentLow, priceAvg, priceAvgSum(DecimalConstants<Decimal>::DecimalZero),
	relativeLow, averagePrice;
      Decimal trueHigh, trueLow, trueRange, trueRangeSum(DecimalConstants<Decimal>::DecimalZero), avgTrueRange, volatilityUnit;

      for (i = 0; i <= 4; i++)
	{
	  prevClose = security->getCloseValue (baseIt, i + 1);
	  currentHigh = security->getHighValue (baseIt, i);
	  currentLow = security->getLowValue (baseIt, i);

	  priceAvg = (currentHigh + currentLow)/DecimalConstants<Decimal>::DecimalTwo;
	  priceAvgSum = priceAvgSum + priceAvg;
	  
	  trueHigh = std::max (currentHigh, prevClose);
	  trueLow = std::min (currentLow, prevClose);
	  trueRange = trueHigh - trueLow;
	  trueRangeSum = trueRangeSum + trueRange;
	}


      averagePrice = priceAvgSum / decFive;
      relativeLow = security->getLowValue (baseIt, 0) - averagePrice;
      avgTrueRange = trueRangeSum / decFive;
      volatilityUnit = avgTrueRange * volatilityUnitConstant();

      if (volatilityUnit != DecimalConstants<Decimal>::DecimalZero)
	return (relativeLow / volatilityUnit);
      else
	return DecimalConstants<Decimal>::DecimalZero;
    }

  private:
     PALPatternInterpreter()
      {

      }
  };


}


#endif
