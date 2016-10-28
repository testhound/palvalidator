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

  template <class Decimal> class PALPatternInterpreter
  {
  public:
// For any date there is a single Security::ConstRandomAccessIterator
    // Instead of looking up the iterator each time we have the client pass
    // the iterator. This should save alot of lookup time

    static bool evaluateExpression (PatternExpression *expression, 
				    std::shared_ptr<Security<Decimal>> security,
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

    static const Decimal& evaluatePriceBar (PriceBarReference *barReference, 
						  std::shared_ptr<Security<Decimal>> security,
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

	default:
	  throw PalPatternInterpreterException ("PALPatternInterpreter::evaluatePriceBar - unknown PriceBarReference derived class"); 
	}
    }

    
  private:
     PALPatternInterpreter()
      {

      }
  };


}


#endif
