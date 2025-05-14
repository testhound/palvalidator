// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PAL_PATTERN_INTERPRETER_H
#define __PAL_PATTERN_INTERPRETER_H 1

#include <functional>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include "PalAst.h"
#include "Security.h"
#include "DecimalConstants.h"


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

  /**
   * @brief Compiles and evaluates PAL pattern expressions efficiently.
   *
   * This class provides a way to compile a PatternExpression AST into
   * a fast, reusable lambda (PatternEvaluator) and also retains
   * backward-compatible evaluateExpression for existing tests.
   */
  template <class Decimal> class PALPatternInterpreter
  {
  public:
    using PatternEvaluator = std::function<bool(Security<Decimal>*,
						typename Security<Decimal>::ConstRandomAccessIterator)>;

    /**
     * @brief Back-compat wrapper: compile & run in one call.
     *
     * Allows existing code/tests to keep calling evaluateExpression(...) without
     * changing their call sites.
     *
     * @param expr      The pattern AST.
     * @param security  Shared_ptr to the security under test.
     * @param it        Iterator into its time series.
     * @return          The result of the compiled predicate.
     */
    static bool evaluateExpression(PatternExpression* expr,
				   const std::shared_ptr<Security<Decimal>>& security,
				   typename Security<Decimal>::ConstRandomAccessIterator it)
    {
      auto pred = compileEvaluator(expr);
      return pred(security.get(), it);
    }

    /**
     * @brief Compile a PatternExpression into a fast lambda.
     *
     * Recursively traverses the AST and builds a boolean predicate.
     */
    static PatternEvaluator compileEvaluator(PatternExpression* expr)
    {
      if (auto pAnd = dynamic_cast<AndExpr*>(expr))
	{
	  auto L = compileEvaluator(pAnd->getLHS());
	  auto R = compileEvaluator(pAnd->getRHS());
	  
	  return [L,R](Security<Decimal>* s, auto it) {
	    return L(s,it) && R(s,it);
        };
      }
      else if (auto pGt = dynamic_cast<GreaterThanExpr*>(expr))
	{
	  auto Lf = compilePriceBar(pGt->getLHS());
	  auto Rf = compilePriceBar(pGt->getRHS());
	  
	  return [Lf,Rf](Security<Decimal>* s, auto it) {
	    return Lf(s,it) > Rf(s,it);
        };
      }
      else {
        throw PalPatternInterpreterException(
          "compileEvaluator: unsupported PatternExpression type");
      }
    }

  private:
    /**
     * @brief Compile a PriceBarReference into a fast evaluator lambda.
     */
    static std::function<Decimal(Security<Decimal>*, typename Security<Decimal>::ConstRandomAccessIterator)>
    compilePriceBar(PriceBarReference* barRef)
    {
      auto offset = barRef->getBarOffset();
      switch (barRef->getReferenceType()) {
        case PriceBarReference::OPEN:
          return [offset](Security<Decimal>* s, auto it) {
            return s->getOpenValue(it, offset);
          };
        case PriceBarReference::HIGH:
          return [offset](Security<Decimal>* s, auto it) {
            return s->getHighValue(it, offset);
          };
        case PriceBarReference::LOW:
          return [offset](Security<Decimal>* s, auto it) {
            return s->getLowValue(it, offset);
          };
        case PriceBarReference::CLOSE:
          return [offset](Security<Decimal>* s, auto it) {
            return s->getCloseValue(it, offset);
          };
        case PriceBarReference::VOLUME:
          return [offset](Security<Decimal>* s, auto it) {
            return s->getVolumeValue(it, offset);
          };
        default:
          throw PalPatternInterpreterException(
            "compilePriceBar: unknown PriceBarReference type");
      }
    }
    
  private:
    // LEGACY code. Keep for now as we may come back and wire in IBS or some of the
    // other indicators
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
	  // Hack Meander to VWAP to see if it works
	  //return PALPatternInterpreter<Decimal>::Meander (security, iteratorForDate, barReference->getBarOffset());
	  return PALPatternInterpreter<Decimal>::Vwap (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::VCHARTLOW:
	  return PALPatternInterpreter<Decimal>::ValueChartLow (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::VCHARTHIGH:
	  return PALPatternInterpreter<Decimal>::ValueChartHigh (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::IBS1:
	  return PALPatternInterpreter<Decimal>::IBS1 (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::IBS2:
	  return PALPatternInterpreter<Decimal>::IBS2 (security, iteratorForDate, barReference->getBarOffset());

	case PriceBarReference::IBS3:
	  return PALPatternInterpreter<Decimal>::IBS3 (security, iteratorForDate, barReference->getBarOffset());

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

    static const Decimal IBS1(const std::shared_ptr<Security<Decimal>>& security,
					typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
					unsigned long offset)
    {
      Decimal currentClose, currentOpen, currentHigh, currentLow;

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
      
      currentHigh = security->getHighValue (baseIt, 0);
      currentLow = security->getLowValue (baseIt, 0);
      currentOpen = security->getOpenValue (baseIt, 0);
      currentClose = security->getCloseValue (baseIt, 0);

      Decimal num(currentClose - currentLow);
      Decimal denom(currentHigh - currentLow);

      if (denom != DecimalConstants<Decimal>::DecimalZero)
	return (num/denom) * DecimalConstants<Decimal>::DecimalOneHundred;
      else
	return DecimalConstants<Decimal>::DecimalZero;
    }

  static const Decimal IBS2(const std::shared_ptr<Security<Decimal>>& security,
			    typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
			    unsigned long offset)
  {
    typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
    
    Decimal ibsThisBar(IBS1 (security, baseIt, 0));
    Decimal ibsPrevBar(IBS1 (security, baseIt, 1));

    return (ibsThisBar + ibsPrevBar)/DecimalConstants<Decimal>::DecimalTwo;
  }

  static const Decimal IBS3(const std::shared_ptr<Security<Decimal>>& security,
			    typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
			    unsigned long offset)
  {
    typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
    static Decimal decThree (num::fromString<Decimal>(std::string("3.0")));
    
    Decimal ibsThisBar(IBS1 (security, baseIt, 0));
    Decimal ibsBarMinus1(IBS1 (security, baseIt, 1));
    Decimal ibsBarMinus2(IBS1 (security, baseIt, 2));

    return (ibsThisBar + ibsBarMinus1 + ibsBarMinus2)/decThree;
  }

  static const Decimal Vwap(const std::shared_ptr<Security<Decimal>>& security,
				  typename Security<Decimal>::ConstRandomAccessIterator iteratorForDate,
				  unsigned long offset)
    {
      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator baseIt = iteratorForDate - offset;
      static Decimal decThree (num::fromString<Decimal>(std::string("3.0")));

      Decimal prevClose, currentClose, currentOpen, currentHigh, currentLow;
      Decimal priceAvg;
      
      currentHigh = security->getHighValue (baseIt, 0);
      currentLow = security->getLowValue (baseIt, 0);
      priceAvg = (currentHigh + currentLow)/DecimalConstants<Decimal>::DecimalTwo;

      currentOpen = security->getOpenValue (baseIt, 0);
      currentClose = security->getCloseValue (baseIt, 0);
      Decimal num (currentOpen + currentClose + priceAvg);

      return (num / decThree);
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
