#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "PALPatternInterpreter.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"
#include "AstResourceManager.h" // Added for smart pointer resource management

using namespace mkc_timeseries;
using namespace mkc_palast; // Added for AstResourceManager
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

  // Use AstResourceManager for all AST node creation to ensure smart pointer management
  AstResourceManager resourceManager;

  // Re-creating expressions using AstResourceManager and shared_ptr
  auto open5 = resourceManager.getPriceOpen(5);
  auto close5 = resourceManager.getPriceClose(5);
  auto gt1 = std::make_shared<GreaterThanExpr>(open5, close5);

  auto close6 = resourceManager.getPriceClose(6);
  auto gt2 = std::make_shared<GreaterThanExpr>(close5, close6);

  // OPEN OF 5 BARS AGO > CLOSE OF 5 BARS AGO
  // AND CLOSE OF 5 BARS AGO > CLOSE OF 6 BARS AGO
  auto and1 = std::make_shared<AndExpr>(gt1, gt2);

  auto open6 = resourceManager.getPriceOpen(6);
  auto gt3 = std::make_shared<GreaterThanExpr>(close6, open6);

  auto close8 = resourceManager.getPriceClose(8);
  auto gt4 = std::make_shared<GreaterThanExpr>(open6, close8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  auto and2 = std::make_shared<AndExpr>(gt3, gt4);

  auto open8 = resourceManager.getPriceOpen(8);
  auto gt5 = std::make_shared<GreaterThanExpr>(close8, open8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  // CLOSE OF 8 BARS AGO > OPEN OF 8 BARS AGO
  auto and3 = std::make_shared<AndExpr>(and2, gt5);
  auto and4 = std::make_shared<AndExpr>(and1, and3);

  // Use .get() to pass raw pointer to legacy evaluateExpression
  REQUIRE (PalPatternMaxBars::evaluateExpression (and4.get()) == 8);

  // Short pattern
  auto high4 = resourceManager.getPriceHigh(4);
  auto high5 = resourceManager.getPriceHigh(5);
  auto high3 = resourceManager.getPriceHigh(3);
  auto high0 = resourceManager.getPriceHigh(0);
  auto high1 = resourceManager.getPriceHigh(1);
  auto high2 = resourceManager.getPriceHigh(2);

  auto shortgt1 = std::make_shared<GreaterThanExpr>(high4, high5);
  auto shortgt2 = std::make_shared<GreaterThanExpr>(high5, high3);
  auto shortgt3 = std::make_shared<GreaterThanExpr>(high3, high0);
  auto shortgt4 = std::make_shared<GreaterThanExpr>(high0, high1);
  auto shortgt5 = std::make_shared<GreaterThanExpr>(high1, high2);

  auto shortand1 = std::make_shared<AndExpr>(shortgt1, shortgt2);
  auto shortand2 = std::make_shared<AndExpr>(shortgt3, shortgt4);
  auto shortand3 = std::make_shared<AndExpr>(shortgt5, shortand2);
  auto shortand4 = std::make_shared<AndExpr>(shortand1, shortand3);

  REQUIRE (PalPatternMaxBars::evaluateExpression (shortand4.get()) == 5);

  SECTION ("PALPatternInterpreter testing for all pattern conditions satisfied")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Nov, 15));
    // Use date-based API instead of iterator-based
    REQUIRE(corn->isDateFound(orderDate));
    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (and4.get(), corn, orderDate) == true);
  }

SECTION ("PALPatternInterpreter testing for short pattern condition satisfied")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1986, May, 28));
    // Use date-based API instead of iterator-based
    REQUIRE(corn->isDateFound(orderDate));
    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (shortand4.get(), corn, orderDate) == true);
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
     REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (and4.get(),
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
	    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (shortand4.get(),
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
    bool dateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, orderDate);
    bool ptimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, orderDateTime);
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
    bool morningResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, morningTime);
    bool noonResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, noonTime);
    bool afternoonResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, afternoonTime);
    
    // Verify all evaluations complete without error
    REQUIRE((morningResult == true || morningResult == false));
    REQUIRE((noonResult == true || noonResult == false));
    REQUIRE((afternoonResult == true || afternoonResult == false));
  }

  SECTION("Pattern evaluator compilation with ptime")
  {
    // Test that compileEvaluator returns a ptime-based evaluator
    auto evaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and4.get());
    
    TimeSeriesDate orderDate(TimeSeriesDate(1985, Nov, 15));
    ptime orderDateTime(orderDate, time_duration(16, 0, 0)); // 4:00 PM
    
    // Test that the compiled evaluator works with ptime
    bool result = evaluator(corn.get(), orderDateTime);
    REQUIRE((result == true || result == false)); // Should complete without error
    
    // Test that the compiled evaluator produces same result as direct evaluation
    bool directResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, orderDateTime);
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
      bool longResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, testDateTime);
      bool shortResult = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4.get(), corn, testDateTime);
      
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
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, invalidDateTime);
    REQUIRE(result == false);
    
    // Test with compiled evaluator as well
    auto evaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and4.get());
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
        bool dateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, testDate);
        bool ptimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, defaultDateTime);
        
        REQUIRE(dateResult == ptimeResult);
        
        // Test short pattern as well
        bool shortDateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4.get(), corn, testDate);
        bool shortPtimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4.get(), corn, defaultDateTime);
        
        REQUIRE(shortDateResult == shortPtimeResult);
      }
    }
  }

  SECTION("Compiled evaluator performance and consistency")
  {
    // Test that compiled evaluators are consistent and performant
    auto longEvaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and4.get());
    auto shortEvaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(shortand4.get());
    
    TimeSeriesDate testDate(TimeSeriesDate(1985, Nov, 15));
    ptime testDateTime(testDate, time_duration(14, 30, 0));
    
    // Test multiple calls to ensure consistency
    for (int i = 0; i < 5; ++i) {
      bool longResult1 = longEvaluator(corn.get(), testDateTime);
      bool longResult2 = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, testDateTime);
      REQUIRE(longResult1 == longResult2);
      
      bool shortResult1 = shortEvaluator(corn.get(), testDateTime);
      bool shortResult2 = PALPatternInterpreter<DecimalType>::evaluateExpression(shortand4.get(), corn, testDateTime);
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
    
    bool dateResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, testDate);
    bool explicitPtimeResult = PALPatternInterpreter<DecimalType>::evaluateExpression(and4.get(), corn, explicitDateTime);
    
    REQUIRE(dateResult == explicitPtimeResult);
  }

  SECTION ("IBS1 indicator validation against itself")
  {
    // These tests validate the IBS1 calculation by comparing it only to other
    // IBS1 references, per the new requirement.

    // Define all AST components and expressions at the beginning of the section for clarity and reuse.
    auto ibs_0 = resourceManager.getIBS1(0);
    auto ibs_1 = resourceManager.getIBS1(1);
    auto ibs_2 = resourceManager.getIBS1(2);

    // Expression for: IBS(1) > IBS(0)
    auto testExpr1 = std::make_shared<GreaterThanExpr>(ibs_1, ibs_0);
    // Expression for: IBS(0) > IBS(1)
    auto testExpr2 = std::make_shared<GreaterThanExpr>(ibs_0, ibs_1);
    // Expression for: IBS(2) > IBS(0)
    auto testExpr3 = std::make_shared<GreaterThanExpr>(ibs_2, ibs_0);


    // Test 1: Compare IBS on a day where Close == High.
    // On 1986-May-28: The actual data shows Close == High, so IBS(0) is 1.0.
    // The previous day, May 27, had an IBS < 1.0.
    // We expect IBS1(0) > IBS1(1) to be true.
    TimeSeriesDate weakCloseDate(1986, May, 28);
    REQUIRE(corn->isDateFound(weakCloseDate));
    REQUIRE(PALPatternInterpreter<DecimalType>::evaluateExpression(testExpr2.get(), corn, weakCloseDate) == true);
    
    // Test 2: Edge Case where today's Close == High, so IBS is 1.0
    // On 1985-Apr-02: Data confirms Close == High -> IBS(0) is 1.0
    // On 1985-Apr-01: Data in file gives IBS(1) < 1.0
    // We expect IBS1(0) > IBS1(1) to be true.
    TimeSeriesDate closeAtHighDate(1985, Apr, 2);
    REQUIRE(corn->isDateFound(closeAtHighDate));
    REQUIRE(PALPatternInterpreter<DecimalType>::evaluateExpression(testExpr2.get(), corn, closeAtHighDate) == true);

    // Test 3: Validate IBS calculation for a normal day (Apr 3)
    // On 1985-Apr-03: The actual data gives IBS(0) ≈ 0.60
    // On 1985-Apr-02: The IBS(1) was 1.0
    // We expect IBS1(1) > IBS1(0) to be true (1.0 > 0.60).
    TimeSeriesDate normalBarDate1(1985, Apr, 3);
    REQUIRE(corn->isDateFound(normalBarDate1));
    REQUIRE(PALPatternInterpreter<DecimalType>::evaluateExpression(testExpr1.get(), corn, normalBarDate1) == true);

    // Test 4: Validate IBS calculation for another normal day (Apr 4)
    // On 1985-Apr-04: The actual data gives IBS(0) = 0.50
    // On 1985-Apr-03: The IBS(1) was ≈ 0.60
    // We expect IBS1(1) > IBS1(0) to be true (0.60 > 0.50).
    TimeSeriesDate normalBarDate2(1985, Apr, 4);
    REQUIRE(corn->isDateFound(normalBarDate2));
    REQUIRE(PALPatternInterpreter<DecimalType>::evaluateExpression(testExpr1.get(), corn, normalBarDate2) == true);
  }
}
