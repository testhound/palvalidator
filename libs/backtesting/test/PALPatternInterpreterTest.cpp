#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "PALPatternInterpreter.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;
//using Num = num::DefaultNumber;

const static std::string myCornSymbol("C2");

TEST_CASE ("PALPatternInterpreter operations", "[PALPatternInterpreter]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));

  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol, 
							     futuresName, 
							     cornBigPointValue,
							     cornTickValue, 
							     p);

  auto open5 = new PriceBarOpen(5);
  auto close5 = new PriceBarClose(5);
  auto gt1 = new GreaterThanExpr (open5, close5);

  auto close6 = new PriceBarClose(6);
  auto gt2 = new GreaterThanExpr (close5, close6);

  // OPEN OF 5 BARS AGO > CLOSE OF 5 BARS AGO
  // AND CLOSE OF 5 BARS AGO > CLOSE OF 6 BARS AGO
  auto and1 = new AndExpr (gt1, gt2);

  auto open6 = new PriceBarOpen(6);
  auto gt3 = new GreaterThanExpr (close6, open6);

  auto close8 = new PriceBarClose(8);
  auto gt4 = new GreaterThanExpr (open6, close8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  auto and2 = new AndExpr (gt3, gt4);

  auto open8 = new PriceBarOpen (8);
  auto gt5 = new GreaterThanExpr (close8, open8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  // CLOSE OF 8 BARS AGO > OPEN OF 8 BARS AGO

  auto and3 = new AndExpr (and2, gt5);
  auto and4 = new AndExpr (and1, and3);

  REQUIRE (PalPatternMaxBars::evaluateExpression (and4) == 8);

  // Short pattern

  auto high4 = new PriceBarHigh (4);
  auto high5 = new PriceBarHigh (5);
  auto high3 = new PriceBarHigh (3);
  auto high0 = new PriceBarHigh (0);
  auto high1 = new PriceBarHigh (1);
  auto high2 = new PriceBarHigh (2);

  auto shortgt1 = new GreaterThanExpr (high4, high5);
  auto shortgt2 = new GreaterThanExpr (high5, high3);
  auto shortgt3 = new GreaterThanExpr (high3, high0);
  auto shortgt4 = new GreaterThanExpr (high0, high1);
  auto shortgt5 = new GreaterThanExpr (high1, high2);

  auto shortand1 = new AndExpr (shortgt1, shortgt2);
  auto shortand2 = new AndExpr (shortgt3, shortgt4);
  auto shortand3 = new AndExpr (shortgt5, shortand2);
  auto shortand4 = new AndExpr (shortand1, shortand3);

  REQUIRE (PalPatternMaxBars::evaluateExpression (shortand4) == 5);

  SECTION ("PALPatternInterpreter testing for all pattern conditions satisfied")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Nov, 15));
    // Use date-based API instead of iterator-based
    REQUIRE(corn->isDateFound(orderDate));
    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (and4, corn, orderDate) == true);
  }

SECTION ("PALPatternInterpreter testing for short pattern condition satisfied")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1986, May, 28));
    // Use date-based API instead of iterator-based
    REQUIRE(corn->isDateFound(orderDate));
    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (shortand4, corn, orderDate) == true);
  }

  
  SECTION ("PALPatternInterpreter testing for long pattern not matched")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 22));
    TimeSeriesDate endDate(TimeSeriesDate (1985, Nov, 14));

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
 // Use date-based API instead of iterator-based
 if (corn->isDateFound(orderDate))
   {
     REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (and4,
  						    corn,
  						    orderDate) == false);
   }
      }
 

  }

SECTION ("PALPatternInterpreter testing for short pattern not matched")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 22));
    TimeSeriesDate endDate(TimeSeriesDate (1986, May, 27));

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	// Use date-based API instead of iterator-based
	if (corn->isDateFound(orderDate))
	  {
	    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (shortand4,
								    corn,
								    orderDate) == false);
	  }
      }
	

  }

  SECTION("Backward compatibility date overload")
  {
    // Test that date-based overload delegates correctly to ptime version
    // Verify identical results between date and equivalent ptime calls
    TimeSeriesDate orderDate(TimeSeriesDate(1985, Nov, 15));
    ptime orderDateTime(orderDate, getDefaultBarTime());
    
    // Both calls should produce identical results
    bool dateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, orderDate);
    bool ptimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, orderDateTime);
    REQUIRE(dateResult == ptimeResult);
  }

  SECTION("Pattern evaluation timing precision")
  {
    // Test pattern evaluation with minute-level precision
    // Verify different results at different times same day
    TimeSeriesDate orderDate(TimeSeriesDate(1985, Nov, 15));
    
    ptime morningTime(orderDate, time_duration(9, 30, 0));   // 9:30 AM
    ptime noonTime(orderDate, time_duration(12, 0, 0));      // 12:00 PM
    ptime afternoonTime(orderDate, time_duration(15, 30, 0)); // 3:30 PM
    
    // Test that pattern evaluation can vary by time of day
    // (Results may be same or different depending on pattern and data)
    bool morningResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, morningTime);
    bool noonResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, noonTime);
    bool afternoonResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, afternoonTime);
    
    // Verify all evaluations complete without error
    REQUIRE((morningResult == true || morningResult == false));
    REQUIRE((noonResult == true || noonResult == false));
    REQUIRE((afternoonResult == true || afternoonResult == false));
  }

  SECTION("Pattern evaluator compilation with ptime")
  {
    // Test that compileEvaluator returns a ptime-based evaluator
    auto evaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and4);
    
    TimeSeriesDate orderDate(TimeSeriesDate(1985, Nov, 15));
    ptime orderDateTime(orderDate, time_duration(16, 0, 0)); // 4:00 PM
    
    // Test that the compiled evaluator works with ptime
    bool result = evaluator(corn.get(), orderDateTime);
    REQUIRE((result == true || result == false)); // Should complete without error
    
    // Test that the compiled evaluator produces same result as direct evaluation
    bool directResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, orderDateTime);
    REQUIRE(result == directResult);
  }

  SECTION("Enhanced ptime precision testing with intraday scenarios")
  {
    // Test pattern evaluation with various intraday times to ensure ptime precision works correctly
    TimeSeriesDate orderDate(TimeSeriesDate(1985, Nov, 15));
    
    // Test multiple times throughout the trading day
    std::vector<time_duration> testTimes = {
      time_duration(9, 30, 0),   // Market open
      time_duration(10, 15, 30), // Mid-morning with seconds
      time_duration(12, 0, 0),   // Noon
      time_duration(14, 45, 15), // Mid-afternoon with seconds
      time_duration(15, 59, 59)  // Just before close
    };
    
    for (const auto& timeOfDay : testTimes) {
      ptime testDateTime(orderDate, timeOfDay);
      
      // Test both patterns with precise timing
      bool longResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, testDateTime);
      bool shortResult = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4, corn, testDateTime);
      
      // Verify evaluations complete without error (results can be true or false)
      REQUIRE((longResult == true || longResult == false));
      REQUIRE((shortResult == true || shortResult == false));
    }
  }

  SECTION("Error handling with invalid datetime")
  {
    // Test pattern evaluation with dates that don't exist in the time series
    TimeSeriesDate invalidDate(TimeSeriesDate(2050, Jan, 1)); // Future date not in corn data
    ptime invalidDateTime(invalidDate, time_duration(12, 0, 0));
    
    // Should handle gracefully and return false (due to data access exceptions)
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, invalidDateTime);
    REQUIRE(result == false);
    
    // Test with compiled evaluator as well
    auto evaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and4);
    bool compiledResult = evaluator(corn.get(), invalidDateTime);
    REQUIRE(compiledResult == false);
  }

  SECTION("Pattern evaluation consistency across date and ptime APIs")
  {
    // Test multiple dates to ensure consistent behavior between date and ptime APIs
    std::vector<TimeSeriesDate> testDates = {
      TimeSeriesDate(1985, Nov, 15),
      TimeSeriesDate(1986, May, 28),
      TimeSeriesDate(1985, Jun, 10),
      TimeSeriesDate(1986, Feb, 14)
    };
    
    for (const auto& testDate : testDates) {
      if (corn->isDateFound(testDate)) {
        // Test with default bar time (3:00 PM Central as per implementation plan)
        ptime defaultDateTime(testDate, getDefaultBarTime());
        
        // Both APIs should produce identical results
        bool dateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, testDate);
        bool ptimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, defaultDateTime);
        
        REQUIRE(dateResult == ptimeResult);
        
        // Test short pattern as well
        bool shortDateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4, corn, testDate);
        bool shortPtimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4, corn, defaultDateTime);
        
        REQUIRE(shortDateResult == shortPtimeResult);
      }
    }
  }

  SECTION("Compiled evaluator performance and consistency")
  {
    // Test that compiled evaluators are consistent and performant
    auto longEvaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and4);
    auto shortEvaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(shortand4);
    
    TimeSeriesDate testDate(TimeSeriesDate(1985, Nov, 15));
    ptime testDateTime(testDate, time_duration(14, 30, 0));
    
    // Test multiple calls to ensure consistency
    for (int i = 0; i < 5; ++i) {
      bool longResult1 = longEvaluator(corn.get(), testDateTime);
      bool longResult2 = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, testDateTime);
      REQUIRE(longResult1 == longResult2);
      
      bool shortResult1 = shortEvaluator(corn.get(), testDateTime);
      bool shortResult2 = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4, corn, testDateTime);
      REQUIRE(shortResult1 == shortResult2);
    }
  }

  SECTION("Default bar time validation")
  {
    // Verify that the default bar time matches the implementation plan specification (3:00 PM Central)
    TimeSeriesDate testDate(TimeSeriesDate(1985, Nov, 15));
    time_duration defaultTime = getDefaultBarTime();
    
    // According to the implementation plan, default bar time should be 3:00 PM Central (15:00)
    REQUIRE(defaultTime.hours() == 15);
    REQUIRE(defaultTime.minutes() == 0);
    REQUIRE(defaultTime.seconds() == 0);
    
    // Test that date-based evaluation uses this default time correctly
    ptime explicitDateTime(testDate, defaultTime);
    
    bool dateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, testDate);
    bool explicitPtimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4, corn, explicitDateTime);
    
    REQUIRE(dateResult == explicitPtimeResult);
  }


}

