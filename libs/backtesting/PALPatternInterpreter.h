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
#include <iostream> // For potential debug output in catch blocks
#include "PalAst.h"
#include "Security.h" // Includes new Security API and indirectly TimeSeries.h (for exceptions)
#include "DecimalConstants.h"
#include <boost/date_time/gregorian/gregorian_types.hpp> // For boost::gregorian::date

namespace mkc_timeseries
{
  class PalPatternInterpreterException : public std::runtime_error
  {
  public:
    PalPatternInterpreterException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    // Added virtual destructor for base class
    virtual ~PalPatternInterpreterException() = default;
  };

  /**
   * @brief Compiles and evaluates PAL pattern expressions efficiently.
   *
   * This class provides a way to compile a PatternExpression AST into
   * a fast, reusable lambda (PatternEvaluator). It handles data access
   * exceptions by typically evaluating the affected sub-expression to false.
   */
  template <class Decimal> class PALPatternInterpreter
  {
  public:
    /**
     * @brief Defines the signature for a compiled pattern evaluator.
     *
     * The evaluator takes a pointer to a Security object and an evaluation date.
     * It returns true if the pattern matches for that security on that date, false otherwise.
     * It will also return false if a data access error occurs during evaluation.
     */
    using PatternEvaluator = std::function<bool(Security<Decimal>*,
						const boost::gregorian::date& evalDate)>;

    /**
     * @brief Back-compat wrapper: compile & run in one call.
     *
     * Allows existing code/tests to call evaluateExpression(...) with an evaluation date.
     * The iterator-based version is removed due to API changes.
     *
     * @param expr      The pattern AST.
     * @param security  Shared_ptr to the security under test.
     * @param evalDate  The date on which to evaluate the pattern.
     * @return          The result of the compiled predicate.
     */
    static bool evaluateExpression(PatternExpression* expr,
				   const std::shared_ptr<Security<Decimal>>& security,
				   const boost::gregorian::date& evalDate)
    {
      auto pred = compileEvaluator(expr);
      return pred(security.get(), evalDate);
    }

    /**
     * @brief Compile a PatternExpression into a fast lambda.
     *
     * Recursively traverses the AST and builds a boolean predicate.
     * The generated lambda includes error handling for data access exceptions,
     * causing the sub-expression to evaluate to false in case of such errors.
     * @param expr The PatternExpression abstract syntax tree.
     * @return A PatternEvaluator lambda.
     */
    static PatternEvaluator compileEvaluator(PatternExpression* expr)
    {
      if (auto pAnd = dynamic_cast<AndExpr*>(expr))
	{
	  auto L = compileEvaluator(pAnd->getLHS());
	  auto R = compileEvaluator(pAnd->getRHS());
	  
	  return [L,R](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> bool 
          {
            // Lambdas L and R already handle their own exceptions and return false if an error occurs.
	    return L(s, evalDate) && R(s, evalDate);
          };
      }
      else if (auto pGt = dynamic_cast<GreaterThanExpr*>(expr))
	{
	  auto Lf = compilePriceBar(pGt->getLHS());
	  auto Rf = compilePriceBar(pGt->getRHS());
	  
	  return [Lf,Rf](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> bool 
          {
            try
            {
              Decimal lhs_val = Lf(s, evalDate);
              Decimal rhs_val = Rf(s, evalDate);
              return lhs_val > rhs_val;
            }
            catch (const mkc_timeseries::TimeSeriesDataAccessException& e)
            {
              // Optional: Log the exception for debugging if needed
              // std::cerr << "PALPatternInterpreter: Data access error in GreaterThanExpr for date "
              //           << boost::gregorian::to_iso_extended_string(evalDate) << ": " << e.what() << std::endl;
              return false; // Expression evaluates to false if data is inaccessible
            }
          };
      }
      // Add other expression types (OrExpr, NotExpr, etc.) here as needed.
      // For example:
      /*
      else if (auto pOr = dynamic_cast<OrExpr*>(expr))
      {
        auto L = compileEvaluator(pOr->getLHS());
        auto R = compileEvaluator(pOr->getRHS());

        return [L,R](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> bool
        {
          return L(s, evalDate) || R(s, evalDate);
        };
      }
      */
      else 
      {
        throw PalPatternInterpreterException(
          "compileEvaluator: unsupported PatternExpression type");
      }
    }

  private:
    /**
     * @brief Compile a PriceBarReference into a fast evaluator lambda that returns a Decimal.
     * The returned lambda will throw TimeSeriesDataAccessException if data is not found.
     * The calling lambda (from compileEvaluator) is responsible for catching these exceptions.
     * @param barRef Pointer to the PriceBarReference AST node.
     * @return A lambda `std::function<Decimal(Security<Decimal>*, const boost::gregorian::date&)>`.
     */
    static std::function<Decimal(Security<Decimal>*, const boost::gregorian::date&)>
    compilePriceBar(PriceBarReference* barRef)
    {
      auto offset = barRef->getBarOffset(); // This is typically unsigned long
      switch (barRef->getReferenceType()) 
      {
        case PriceBarReference::OPEN:
          return [offset](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> Decimal 
          {
            return s->getOpenValue(evalDate, offset);
          };
        case PriceBarReference::HIGH:
          return [offset](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> Decimal 
          {
            return s->getHighValue(evalDate, offset);
          };
        case PriceBarReference::LOW:
          return [offset](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> Decimal 
          {
            return s->getLowValue(evalDate, offset);
          };
        case PriceBarReference::CLOSE:
          return [offset](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> Decimal 
          {
            return s->getCloseValue(evalDate, offset);
          };
        case PriceBarReference::VOLUME:
          return [offset](Security<Decimal>* s, const boost::gregorian::date& evalDate) -> Decimal 
          {
            return s->getVolumeValue(evalDate, offset);
          };
        // MEANDER, IBS, etc. are not directly handled by compilePriceBar in this version based on PR.
        // If they were to be used in compiled expressions, compilePriceBar would need cases for them,
        // potentially calling the refactored legacy static methods.
        default:
          throw PalPatternInterpreterException(
            "compilePriceBar: unknown or unsupported PriceBarReference type for compilation");
      }
    }
    
  private:
    // LEGACY code. Refactored to use the new Security API (date-based access).
    // Keep for now as we may come back and wire in IBS or some of the
    // other indicators into the compiled expressions, or for specific direct use.
    // These methods will throw TimeSeriesDataAccessException if data is not found.
    static Decimal evaluatePriceBar (PriceBarReference *barReference, 
					   Security<Decimal>* security, // Changed from shared_ptr
					   const boost::gregorian::date& evalDate) // Changed from iterator
    {
      unsigned long offset = barReference->getBarOffset();
      switch (barReference->getReferenceType())
	{
	case PriceBarReference::OPEN:
	  return security->getOpenValue(evalDate, offset);
	  
	case PriceBarReference::HIGH:
	  return security->getHighValue(evalDate, offset);
	  
	case PriceBarReference::LOW:
	  return security->getLowValue(evalDate, offset);
	  
	case PriceBarReference::CLOSE:
	  return security->getCloseValue(evalDate, offset);

	case PriceBarReference::VOLUME:
	  return security->getVolumeValue(evalDate, offset);

	case PriceBarReference::MEANDER:
	  // Hack Meander to VWAP to see if it works
	  //return PALPatternInterpreter<Decimal>::Meander (security, evalDate, offset);
	  return PALPatternInterpreter<Decimal>::Vwap (security, evalDate, offset);

	case PriceBarReference::VCHARTLOW:
	  return PALPatternInterpreter<Decimal>::ValueChartLow (security, evalDate, offset);

	case PriceBarReference::VCHARTHIGH:
	  return PALPatternInterpreter<Decimal>::ValueChartHigh (security, evalDate, offset);

	case PriceBarReference::IBS1:
	  return PALPatternInterpreter<Decimal>::IBS1 (security, evalDate, offset);

	case PriceBarReference::IBS2:
	  return PALPatternInterpreter<Decimal>::IBS2 (security, evalDate, offset);

	case PriceBarReference::IBS3:
	  return PALPatternInterpreter<Decimal>::IBS3 (security, evalDate, offset);

	default:
	  throw PalPatternInterpreterException ("PALPatternInterpreter::evaluatePriceBar - unknown PriceBarReference derived class"); 
	}
    }

    static Decimal Meander(Security<Decimal>* security, // Changed from shared_ptr
				  const boost::gregorian::date& evalDate, // Changed from iterator
				  unsigned long offset) // 'offset' is the lookback for the reference bar for Meander calculation
    {
      // The 'offset' parameter defines the base bar for the Meander calculation.
      // Subsequent lookups (0 to 4) are relative to this 'evalDate' considering the 'offset'.
      int i;

      Decimal sum(DecimalConstants<Decimal>::DecimalZero);
      Decimal prevClose, currentClose, currentOpen, currentHigh, currentLow;
      Decimal denom(num::fromString<Decimal>("20.0")); // 5 bars * 4 components per bar
      
      for (i = 0; i <= 4; i++) // Iterate 5 days for Meander (current + 4 previous days relative to the target bar)
	{
          // offset + i: current bar of the 5-day window
          // offset + i + 1: previous bar to the current bar of the 5-day window
	  prevClose = security->getCloseValue (evalDate, offset + i + 1);
	  currentOpen = security->getOpenValue (evalDate, offset + i);
	  currentHigh = security->getHighValue (evalDate, offset + i);
	  currentClose = security->getCloseValue (evalDate, offset + i);
	  currentLow = security->getLowValue (evalDate, offset + i);

          if (prevClose == DecimalConstants<Decimal>::DecimalZero)
          {
            throw PalPatternInterpreterException("Meander calculation: Division by zero (prevClose is zero).");
          }
	  sum = sum + ((currentOpen - prevClose)/prevClose) + ((currentHigh - prevClose)/prevClose) +
	    ((currentLow - prevClose)/prevClose) + ((currentClose - prevClose)/prevClose);
	}
      Decimal avg( sum / denom);
      // Result is projected from the most recent close of the Meander period (evalDate offset by 'offset')
      return security->getCloseValue (evalDate, offset) * (DecimalConstants<Decimal>::DecimalOne + avg);
    }

    static Decimal IBS1(Security<Decimal>* security, // Changed
			    const boost::gregorian::date& evalDate, // Changed
			    unsigned long offset) // 'offset' refers to the bar for which IBS1 is calculated (0 for evalDate, 1 for prior bar, etc.)
    {
      Decimal currentClose, currentHigh, currentLow;
      
      currentHigh = security->getHighValue (evalDate, offset);
      currentLow = security->getLowValue (evalDate, offset);
      // currentOpen = security->getOpenValue (evalDate, offset); // Open not used in IBS
      currentClose = security->getCloseValue (evalDate, offset);

      Decimal num(currentClose - currentLow);
      Decimal denom(currentHigh - currentLow);

      if (denom != DecimalConstants<Decimal>::DecimalZero)
      {
	return (num/denom) * DecimalConstants<Decimal>::DecimalOneHundred;
      }
      else
      {
	return DecimalConstants<Decimal>::DecimalZero; // Or some other defined behavior for zero range
      }
    }

  static Decimal IBS2(Security<Decimal>* security, // Changed
			    const boost::gregorian::date& evalDate, // Changed
			    unsigned long offset) // 'offset' is for the most recent bar of the IBS2 calculation
  {
    // IBS2 is the average of IBS1 for the bar at 'offset' (from evalDate)
    // and IBS1 for the bar 'offset + 1' (one bar prior to that).
    Decimal ibsThisBar(IBS1 (security, evalDate, offset));
    Decimal ibsPrevBar(IBS1 (security, evalDate, offset + 1));

    return (ibsThisBar + ibsPrevBar)/DecimalConstants<Decimal>::DecimalTwo;
  }

  static Decimal IBS3(Security<Decimal>* security, // Changed
			    const boost::gregorian::date& evalDate, // Changed
			    unsigned long offset) // 'offset' is for the most recent bar of the IBS3 calculation
  {
    static Decimal decThree (num::fromString<Decimal>("3.0"));
    
    // IBS3 averages IBS1 for the bar at 'offset', 'offset + 1', and 'offset + 2'.
    Decimal ibsBar0(IBS1 (security, evalDate, offset));
    Decimal ibsBar1(IBS1 (security, evalDate, offset + 1));
    Decimal ibsBar2(IBS1 (security, evalDate, offset + 2));

    return (ibsBar0 + ibsBar1 + ibsBar2)/decThree;
  }

  static Decimal Vwap(Security<Decimal>* security, // Changed
			  const boost::gregorian::date& evalDate, // Changed
			  unsigned long offset) // 'offset' is for the bar whose VWAP (simplified) is calculated
    {
      static Decimal decThree (num::fromString<Decimal>("3.0"));

      Decimal currentOpen, currentHigh, currentLow, currentClose;
      Decimal priceAvg;
      
      currentHigh = security->getHighValue (evalDate, offset);
      currentLow = security->getLowValue (evalDate, offset);
      priceAvg = (currentHigh + currentLow)/DecimalConstants<Decimal>::DecimalTwo; // Typical price

      currentOpen = security->getOpenValue (evalDate, offset);
      currentClose = security->getCloseValue (evalDate, offset);
      Decimal num (currentOpen + currentClose + priceAvg); // Sum of O, C, Typical

      return (num / decThree); // Average of O, C, Typical
    }
    
    static const Decimal volatilityUnitConstant() // Remains unchanged
    {
      static Decimal volatilityConstant (num::fromString<Decimal>("0.20"));
      return volatilityConstant;
    }
    
    static Decimal ValueChartHigh(Security<Decimal>* security, // Changed
					const boost::gregorian::date& evalDate, // Changed
					unsigned long offset) // 'offset' is for the bar whose ValueChartHigh is calculated
    {
      static Decimal decFive (num::fromString<Decimal>("5.0"));
	    
      int i;
      
      Decimal prevClose, currentHigh, currentLow, priceAvg, priceAvgSum(DecimalConstants<Decimal>::DecimalZero);
      Decimal relativeHigh, averagePrice, currentCloseForRange; // Renamed currentClose to avoid conflict
      Decimal trueHigh, trueLow, trueRange, trueRangeSum(DecimalConstants<Decimal>::DecimalZero), avgTrueRange, volatilityUnit;
      Decimal closeToCloseRange, highLowRange, range;
      
      // Loop for 5 days relative to the target bar (evalDate + offset)
      for (i = 0; i <= 4; i++)
	{
          unsigned long current_bar_lookback = offset + i;
          unsigned long prev_bar_lookback = offset + i + 1;

	  currentCloseForRange = security->getCloseValue (evalDate, current_bar_lookback);
	  prevClose = security->getCloseValue (evalDate, prev_bar_lookback);
	  closeToCloseRange = num::abs (currentCloseForRange - prevClose);
	  
	  currentHigh = security->getHighValue (evalDate, current_bar_lookback);
	  currentLow = security->getLowValue (evalDate, current_bar_lookback);
	  highLowRange = currentHigh - currentLow;

	  range = std::max (closeToCloseRange, highLowRange);
	  
	  priceAvg = (currentHigh + currentLow)/DecimalConstants<Decimal>::DecimalTwo;
	  priceAvgSum = priceAvgSum + priceAvg;
	  
	  trueRange = range; // Simplified True Range based on problem description context
	  trueRangeSum = trueRangeSum + trueRange;
	}

      averagePrice = priceAvgSum / decFive;
      // Relative high of the target bar (evalDate + offset)
      relativeHigh = security->getHighValue (evalDate, offset) - averagePrice;
      avgTrueRange = trueRangeSum / decFive;
      volatilityUnit = avgTrueRange * volatilityUnitConstant();

      if (volatilityUnit != DecimalConstants<Decimal>::DecimalZero)
	{
	  Decimal retVal = (relativeHigh / volatilityUnit);
	  return (retVal);
	}
      else
	{
	  return DecimalConstants<Decimal>::DecimalZero;
	}
    }

    static Decimal ValueChartLow(Security<Decimal>* security, // Changed
					const boost::gregorian::date& evalDate, // Changed
					unsigned long offset) // 'offset' is for the bar whose ValueChartLow is calculated
    {
      static Decimal decFive (num::fromString<Decimal>("5.0"));
	    
      int i;
      
      Decimal prevClose, currentHigh, currentLow, priceAvg, priceAvgSum(DecimalConstants<Decimal>::DecimalZero);
      Decimal relativeLow, averagePrice; // currentClose removed as it's not used for relativeLow calc here
      Decimal trueHigh, trueLow, trueRange, trueRangeSum(DecimalConstants<Decimal>::DecimalZero), avgTrueRange, volatilityUnit;
      // Added currentCloseForRange for clarity inside loop, similar to ValueChartHigh
      Decimal currentCloseForRange; 
      Decimal closeToCloseRange, highLowRange, range;


      // Loop for 5 days relative to the target bar (evalDate + offset)
      for (i = 0; i <= 4; i++)
	{
          unsigned long current_bar_lookback = offset + i;
          unsigned long prev_bar_lookback = offset + i + 1;

	  prevClose = security->getCloseValue (evalDate, prev_bar_lookback);
	  currentHigh = security->getHighValue (evalDate, current_bar_lookback);
	  currentLow = security->getLowValue (evalDate, current_bar_lookback);

	  priceAvg = (currentHigh + currentLow)/DecimalConstants<Decimal>::DecimalTwo;
	  priceAvgSum = priceAvgSum + priceAvg;
	  
          // True range calculation as per ValueChartHigh interpretation
          currentCloseForRange = security->getCloseValue(evalDate, current_bar_lookback);
          closeToCloseRange = num::abs(currentCloseForRange - prevClose);
          highLowRange = currentHigh - currentLow;
          range = std::max(closeToCloseRange, highLowRange);
	  trueRange = range; // Simplified True Range

	  trueRangeSum = trueRangeSum + trueRange;
	}

      averagePrice = priceAvgSum / decFive;
      // Relative low of the target bar (evalDate + offset)
      relativeLow = security->getLowValue (evalDate, offset) - averagePrice;
      avgTrueRange = trueRangeSum / decFive;
      volatilityUnit = avgTrueRange * volatilityUnitConstant();

      if (volatilityUnit != DecimalConstants<Decimal>::DecimalZero)
      {
	return (relativeLow / volatilityUnit);
      }
      else
      {
	return DecimalConstants<Decimal>::DecimalZero;
      }
    }

  private:
     // Private constructor to prevent instantiation of this utility class
     PALPatternInterpreter()
      {
      }
  }; // End class PALPatternInterpreter

} // End namespace mkc_timeseries


#endif // __PAL_PATTERN_INTERPRETER_H
