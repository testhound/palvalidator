// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PAL_PATTERN_INTERPRETER_H
#define __PAL_PATTERN_INTERPRETER_H 1

#include "PalAst.h"
#include "Security.h"

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

  template <int Prec> class PALPatternInterpreter
  {
  public:
// For any date there is a single Security::ConstRandomAccessIterator
    // Instead of looking up the iterator each time we have the client pass
    // the iterator. This should save alot of lookup time

    static bool evaluateExpression (PatternExpression *expression, 
				    std::shared_ptr<Security<Prec>> security,
				    typename Security<Prec>::ConstRandomAccessIterator iteratorForDate)
    {
      if (AndExpr *pAnd = dynamic_cast<AndExpr*>(expression))
	{
	  bool lhsCond = PALPatternInterpreter<Prec>::evaluateExpression (pAnd->getLHS(),
									  security,
									  iteratorForDate);
	  if (lhsCond == true)
	    return PALPatternInterpreter<Prec>::evaluateExpression (pAnd->getRHS(),
								    security,
								    iteratorForDate);
	  else
	    return false;
	}
      else if (GreaterThanExpr *pGreaterThan = dynamic_cast<GreaterThanExpr*>(expression))
	{
	  decimal<Prec> lhs = PALPatternInterpreter<Prec>::evaluatePriceBar(pGreaterThan->getLHS(), 
									    security, 
									    iteratorForDate);
	  decimal<Prec> rhs = PALPatternInterpreter<Prec>::evaluatePriceBar(pGreaterThan->getRHS(), 
									    security, 
									    iteratorForDate);
	  return (lhs > rhs);
	}
      else
	throw PalPatternInterpreterException ("PALPatternInterpreter::evaluateExpression Illegal PatternExpression");
    }

  private:
    static const decimal<Prec>& getPriceBarHigh (PriceBarHigh *barReference, 
						 std::shared_ptr<Security<Prec>> security,
						 typename Security<Prec>::ConstRandomAccessIterator iteratorForDate)
    {
      decimal<Prec> aHigh = security->getHighValue(iteratorForDate, barReference->getBarOffset());
      //std::cout << "High[" << std::to_string (barReference->getBarOffset()) << "] = " << aHigh << std::endl;
      return security->getHighValue(iteratorForDate, barReference->getBarOffset());
    }

    static const decimal<Prec>& getPriceBarLow (PriceBarLow *barReference, 
						std::shared_ptr<Security<Prec>> security,
						typename Security<Prec>::ConstRandomAccessIterator iteratorForDate)
    {
      decimal<Prec> aLow = security->getLowValue(iteratorForDate, barReference->getBarOffset());
      //std::cout << "Low[" << std::to_string (barReference->getBarOffset()) << "] = " << aLow << std::endl;

      return security->getLowValue(iteratorForDate, barReference->getBarOffset());
    }

    static const decimal<Prec>& getPriceBarClose (PriceBarClose *barReference, 
						std::shared_ptr<Security<Prec>> security,
						typename Security<Prec>::ConstRandomAccessIterator iteratorForDate)
    {
      decimal<Prec> aClose = security->getCloseValue(iteratorForDate, barReference->getBarOffset());
      //std::cout << "Close[" << std::to_string (barReference->getBarOffset()) << "] = " << aClose << std::endl;
      return security->getCloseValue(iteratorForDate, barReference->getBarOffset());
    }

    static const decimal<Prec>& getPriceBarOpen (PriceBarOpen *barReference, 
						std::shared_ptr<Security<Prec>> security,
						typename Security<Prec>::ConstRandomAccessIterator iteratorForDate)
    {
      decimal<Prec> aOpen = security->getOpenValue(iteratorForDate, barReference->getBarOffset());
      //std::cout << "Open[" << std::to_string (barReference->getBarOffset()) << "] = " << aOpen << std::endl;
      return security->getOpenValue(iteratorForDate, barReference->getBarOffset());
    }

    static const decimal<Prec>& evaluatePriceBar (PriceBarReference *barReference, 
						  std::shared_ptr<Security<Prec>> security,
						  typename Security<Prec>::ConstRandomAccessIterator iteratorForDate)
    {
      if (PriceBarHigh *pHigh = dynamic_cast<PriceBarHigh*>(barReference))
	{
	  return PALPatternInterpreter<Prec>::getPriceBarHigh (pHigh, security, iteratorForDate);
	}
      else if (PriceBarLow *pLow = dynamic_cast<PriceBarLow*>(barReference))
	{
	  return PALPatternInterpreter<Prec>::getPriceBarLow (pLow, security, iteratorForDate);
	}
      else if (PriceBarClose *pClose = dynamic_cast<PriceBarClose*>(barReference))
	{
	  return PALPatternInterpreter<Prec>::getPriceBarClose (pClose, security, iteratorForDate);
	}
      else if (PriceBarOpen *pOpen = dynamic_cast<PriceBarOpen*>(barReference))
	{
	  return PALPatternInterpreter<Prec>::getPriceBarOpen (pOpen, security, iteratorForDate);
	}
      else
	throw PalPatternInterpreterException ("PALPatternInterpreter::evaluatePriceBar - unknown PriceBarReference derived class"); 
    }

  private:
     PALPatternInterpreter()
      {

      }
  };


}


#endif
