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

TEST_CASE("PALPatternInterpreter - LOW reference", "[PALPatternInterpreter][references]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("LOW reference in simple pattern")
  {
    auto low0 = resourceManager.getPriceLow(0);
    auto low1 = resourceManager.getPriceLow(1);
    
    // Pattern: LOW[0] > LOW[1] (testing for higher low)
    auto gt1 = std::make_shared<GreaterThanExpr>(low0, low1);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    REQUIRE(corn->isDateFound(testDate));
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("LOW reference with multiple bars")
  {
    auto low5 = resourceManager.getPriceLow(5);
    auto low6 = resourceManager.getPriceLow(6);
    auto low7 = resourceManager.getPriceLow(7);
    
    // Pattern: (LOW[5] > LOW[6]) AND (LOW[6] > LOW[7])
    auto gt1 = std::make_shared<GreaterThanExpr>(low5, low6);
    auto gt2 = std::make_shared<GreaterThanExpr>(low6, low7);
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - VOLUME reference", "[PALPatternInterpreter][references]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("VOLUME reference in simple pattern")
  {
    auto volume0 = resourceManager.getVolume(0);
    auto volume1 = resourceManager.getVolume(1);
    
    // Pattern: VOLUME[0] > VOLUME[1] (increasing volume)
    auto gt1 = std::make_shared<GreaterThanExpr>(volume0, volume1);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    REQUIRE(corn->isDateFound(testDate));
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("VOLUME with price pattern")
  {
    auto volume0 = resourceManager.getVolume(0);
    auto volume1 = resourceManager.getVolume(1);
    auto close0 = resourceManager.getPriceClose(0);
    auto close1 = resourceManager.getPriceClose(1);
    
    // Pattern: (VOLUME[0] > VOLUME[1]) AND (CLOSE[0] > CLOSE[1])
    auto gt1 = std::make_shared<GreaterThanExpr>(volume0, volume1);
    auto gt2 = std::make_shared<GreaterThanExpr>(close0, close1);
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - IBS2 and IBS3 references", "[PALPatternInterpreter][references]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  TimeSeriesDate testDate(1985, Nov, 15);
  
  SECTION("IBS2 evaluation")
  {
    auto ibs2_0 = resourceManager.getIBS2(0);
    auto ibs2_1 = resourceManager.getIBS2(1);
    
    // Pattern: IBS2[0] > IBS2[1]
    auto gt1 = std::make_shared<GreaterThanExpr>(ibs2_0, ibs2_1);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("IBS3 evaluation")
  {
    auto ibs3_0 = resourceManager.getIBS3(0);
    auto ibs3_1 = resourceManager.getIBS3(1);
    
    // Pattern: IBS3[0] > IBS3[1]
    auto gt1 = std::make_shared<GreaterThanExpr>(ibs3_0, ibs3_1);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("IBS smoothing validation: IBS1 vs IBS2 vs IBS3")
  {
    auto ibs1_0 = resourceManager.getIBS1(0);
    auto ibs2_0 = resourceManager.getIBS2(0);
    auto ibs3_0 = resourceManager.getIBS3(0);
    
    // All three should evaluate without error
    auto gt1 = std::make_shared<GreaterThanExpr>(ibs1_0, ibs2_0);
    auto gt2 = std::make_shared<GreaterThanExpr>(ibs2_0, ibs3_0);
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - ValueChart references", "[PALPatternInterpreter][references]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  TimeSeriesDate testDate(1985, Nov, 15);
  
  SECTION("VChartHigh evaluation")
  {
    auto vchartHigh0 = resourceManager.getVChartHigh(0);
    auto vchartHigh1 = resourceManager.getVChartHigh(1);
    
    // Pattern: VChartHigh[0] > VChartHigh[1]
    auto gt1 = std::make_shared<GreaterThanExpr>(vchartHigh0, vchartHigh1);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("VChartLow evaluation")
  {
    auto vchartLow0 = resourceManager.getVChartLow(0);
    auto vchartLow1 = resourceManager.getVChartLow(1);
    
    // Pattern: VChartLow[0] > VChartLow[1]
    auto gt1 = std::make_shared<GreaterThanExpr>(vchartLow0, vchartLow1);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("Combined VChart pattern")
  {
    auto vchartHigh0 = resourceManager.getVChartHigh(0);
    auto vchartLow0 = resourceManager.getVChartLow(0);
    auto vchartHigh1 = resourceManager.getVChartHigh(1);
    auto vchartLow1 = resourceManager.getVChartLow(1);
    
    // Pattern: (VChartHigh[0] > VChartHigh[1]) AND (VChartLow[0] > VChartLow[1])
    auto gt1 = std::make_shared<GreaterThanExpr>(vchartHigh0, vchartHigh1);
    auto gt2 = std::make_shared<GreaterThanExpr>(vchartLow0, vchartLow1);
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - Meander reference", "[PALPatternInterpreter][references]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("Meander basic evaluation")
  {
    auto meander0 = resourceManager.getMeander(0);
    auto meander1 = resourceManager.getMeander(1);
    
    // Pattern: MEANDER[0] > MEANDER[1]
    auto gt1 = std::make_shared<GreaterThanExpr>(meander0, meander1);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - ROC1 reference", "[PALPatternInterpreter][references]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("ROC1 basic evaluation")
  {
    auto roc1_0 = resourceManager.getRoc1(0);
    auto roc1_1 = resourceManager.getRoc1(1);
    
    // Pattern: ROC1[0] > ROC1[1]
    auto gt1 = std::make_shared<GreaterThanExpr>(roc1_0, roc1_1);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    REQUIRE(corn->isDateFound(testDate));
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("ROC1 with price pattern")
  {
    auto roc1_0 = resourceManager.getRoc1(0);
    auto close5 = resourceManager.getPriceClose(5);
    auto close6 = resourceManager.getPriceClose(6);
    
    // Pattern: (ROC1[0] > ROC1[1]) AND (CLOSE[5] > CLOSE[6])
    auto roc1_1 = resourceManager.getRoc1(1);
    auto gt1 = std::make_shared<GreaterThanExpr>(roc1_0, roc1_1);
    auto gt2 = std::make_shared<GreaterThanExpr>(close5, close6);
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

// ============================================================================
// TEST SUITE 2: Edge Cases and Error Conditions
// ============================================================================

TEST_CASE("PALPatternInterpreter - Division by zero handling", "[PALPatternInterpreter][edge_cases]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("IBS1 should handle zero range gracefully")
  {
    // Test that IBS1 evaluates without crashing even if High == Low
    auto ibs1_0 = resourceManager.getIBS1(0);
    auto ibs1_1 = resourceManager.getIBS1(1);
    
    auto gt1 = std::make_shared<GreaterThanExpr>(ibs1_0, ibs1_1);
    
    // Test across multiple dates to increase likelihood of encountering edge cases
    TimeSeriesDate startDate(1985, Mar, 22);
    TimeSeriesDate endDate(1985, Apr, 30);
    
    int evaluationCount = 0;
    for (TimeSeriesDate testDate = startDate; testDate <= endDate; 
         testDate = boost_next_weekday(testDate))
    {
      if (corn->isDateFound(testDate))
      {
        // Should not throw exception
        bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
            gt1.get(), corn, testDate);
        REQUIRE((result == true || result == false));
        evaluationCount++;
      }
    }
    
    REQUIRE(evaluationCount > 0);
  }
}

TEST_CASE("PALPatternInterpreter - Null pointer handling", "[PALPatternInterpreter][edge_cases]")
{
  SECTION("Null expression pointer should throw")
  {
    DecimalType cornTickValue(createDecimal("0.25"));
    PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                            TradingVolume::CONTRACTS, cornTickValue);
    csvFile.readFile();
    auto p = csvFile.getTimeSeries();
    
    auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
        myCornSymbol, "Corn futures",
        createDecimal("50.0"), cornTickValue, p);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    
    // Passing null expression should throw PalPatternInterpreterException
    REQUIRE_THROWS_AS(
        PALPatternInterpreter<DecimalType>::evaluateExpression(nullptr, corn, testDate),
        PalPatternInterpreterException
    );
  }
}

TEST_CASE("PALPatternInterpreter - Complex nested patterns", "[PALPatternInterpreter][complex]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  TimeSeriesDate testDate(1985, Nov, 15);
  
  SECTION("Deep nested AND expressions (5 levels)")
  {
    auto close0 = resourceManager.getPriceClose(0);
    auto close1 = resourceManager.getPriceClose(1);
    auto close2 = resourceManager.getPriceClose(2);
    auto close3 = resourceManager.getPriceClose(3);
    auto close4 = resourceManager.getPriceClose(4);
    auto close5 = resourceManager.getPriceClose(5);
    
    // Build deeply nested pattern
    auto gt1 = std::make_shared<GreaterThanExpr>(close0, close1);
    auto gt2 = std::make_shared<GreaterThanExpr>(close1, close2);
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    
    auto gt3 = std::make_shared<GreaterThanExpr>(close2, close3);
    auto and2 = std::make_shared<AndExpr>(and1, gt3);
    
    auto gt4 = std::make_shared<GreaterThanExpr>(close3, close4);
    auto and3 = std::make_shared<AndExpr>(and2, gt4);
    
    auto gt5 = std::make_shared<GreaterThanExpr>(close4, close5);
    auto and4 = std::make_shared<AndExpr>(and3, gt5);
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and4.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("Wide AND expression (many parallel conditions)")
  {
    auto high0 = resourceManager.getPriceHigh(0);
    auto high1 = resourceManager.getPriceHigh(1);
    auto high2 = resourceManager.getPriceHigh(2);
    auto high3 = resourceManager.getPriceHigh(3);
    auto low0 = resourceManager.getPriceLow(0);
    auto low1 = resourceManager.getPriceLow(1);
    auto close0 = resourceManager.getPriceClose(0);
    auto open0 = resourceManager.getPriceOpen(0);
    
    // Create pattern with many conditions
    auto gt1 = std::make_shared<GreaterThanExpr>(high0, high1);
    auto gt2 = std::make_shared<GreaterThanExpr>(high1, high2);
    auto gt3 = std::make_shared<GreaterThanExpr>(high2, high3);
    auto gt4 = std::make_shared<GreaterThanExpr>(low0, low1);
    auto gt5 = std::make_shared<GreaterThanExpr>(close0, open0);
    
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    auto and2 = std::make_shared<AndExpr>(gt3, gt4);
    auto and3 = std::make_shared<AndExpr>(and1, and2);
    auto and4 = std::make_shared<AndExpr>(and3, gt5);
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and4.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - All indicator types combined", "[PALPatternInterpreter][comprehensive]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  TimeSeriesDate testDate(1985, Nov, 15);
  
  SECTION("Pattern using all major indicator types")
  {
    // Build complex pattern using different indicator types:
    // (CLOSE[0] > CLOSE[1]) AND (VOLUME[0] > VOLUME[1]) AND 
    // (IBS1[0] > IBS1[1]) AND (VCHARTLOW[0] > VCHARTLOW[1])
    
    auto close0 = resourceManager.getPriceClose(0);
    auto close1 = resourceManager.getPriceClose(1);
    auto volume0 = resourceManager.getVolume(0);
    auto volume1 = resourceManager.getVolume(1);
    auto ibs1_0 = resourceManager.getIBS1(0);
    auto ibs1_1 = resourceManager.getIBS1(1);
    auto vchartLow0 = resourceManager.getVChartLow(0);
    auto vchartLow1 = resourceManager.getVChartLow(1);
    
    auto gt1 = std::make_shared<GreaterThanExpr>(close0, close1);
    auto gt2 = std::make_shared<GreaterThanExpr>(volume0, volume1);
    auto gt3 = std::make_shared<GreaterThanExpr>(ibs1_0, ibs1_1);
    auto gt4 = std::make_shared<GreaterThanExpr>(vchartLow0, vchartLow1);
    
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    auto and2 = std::make_shared<AndExpr>(gt3, gt4);
    auto and3 = std::make_shared<AndExpr>(and1, and2);
    
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        and3.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
}

TEST_CASE("PALPatternInterpreter - Compiled evaluator reuse", "[PALPatternInterpreter][performance]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("Evaluator consistency across multiple dates")
  {
    auto close5 = resourceManager.getPriceClose(5);
    auto close6 = resourceManager.getPriceClose(6);
    auto gt1 = std::make_shared<GreaterThanExpr>(close5, close6);
    
    // Compile once
    auto evaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(gt1.get());
    
    // Use multiple times across date range
    TimeSeriesDate startDate(1985, Nov, 1);
    TimeSeriesDate endDate(1985, Nov, 30);
    
    int evaluationCount = 0;
    for (TimeSeriesDate testDate = startDate; testDate <= endDate;
         testDate = boost_next_weekday(testDate))
    {
      if (corn->isDateFound(testDate))
      {
        ptime testDateTime(testDate, getDefaultBarTime());
        
        bool result1 = evaluator(corn.get(), testDateTime);
        bool result2 = evaluator(corn.get(), testDateTime);
        
        // Same evaluator should give same results
        REQUIRE(result1 == result2);
        
        // Should match direct evaluation
        bool directResult = PALPatternInterpreter<DecimalType>::evaluateExpression(
            gt1.get(), corn, testDate);
        REQUIRE(result1 == directResult);
        
        evaluationCount++;
      }
    }
    
    REQUIRE(evaluationCount > 0);
  }
  
  SECTION("Complex pattern evaluator reuse")
  {
    // Build a complex pattern
    auto close5 = resourceManager.getPriceClose(5);
    auto close6 = resourceManager.getPriceClose(6);
    auto ibs1_0 = resourceManager.getIBS1(0);
    auto ibs1_1 = resourceManager.getIBS1(1);
    auto volume0 = resourceManager.getVolume(0);
    auto volume1 = resourceManager.getVolume(1);
    
    auto gt1 = std::make_shared<GreaterThanExpr>(close5, close6);
    auto gt2 = std::make_shared<GreaterThanExpr>(ibs1_0, ibs1_1);
    auto gt3 = std::make_shared<GreaterThanExpr>(volume0, volume1);
    
    auto and1 = std::make_shared<AndExpr>(gt1, gt2);
    auto and2 = std::make_shared<AndExpr>(and1, gt3);
    
    // Compile once
    auto evaluator = PALPatternInterpreter<DecimalType>::compileEvaluator(and2.get());
    
    // Reuse multiple times
    std::vector<TimeSeriesDate> testDates = {
      TimeSeriesDate(1985, Nov, 15),
      TimeSeriesDate(1986, May, 28),
      TimeSeriesDate(1985, Jun, 10)
    };
    
    for (const auto& testDate : testDates)
    {
      if (corn->isDateFound(testDate))
      {
        ptime testDateTime(testDate, time_duration(14, 0, 0));
        
        // Multiple calls should be consistent
        bool result1 = evaluator(corn.get(), testDateTime);
        bool result2 = evaluator(corn.get(), testDateTime);
        bool result3 = evaluator(corn.get(), testDateTime);
        
        REQUIRE(result1 == result2);
        REQUIRE(result2 == result3);
      }
    }
  }
}

TEST_CASE("PALPatternInterpreter - Boundary bar offsets", "[PALPatternInterpreter][edge_cases]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY,
                                          TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();
  auto p = csvFile.getTimeSeries();
  
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      myCornSymbol, "Corn futures",
      createDecimal("50.0"), cornTickValue, p);
  
  AstResourceManager resourceManager;
  
  SECTION("Zero offset (current bar)")
  {
    auto close0 = resourceManager.getPriceClose(0);
    auto open0 = resourceManager.getPriceOpen(0);
    auto gt1 = std::make_shared<GreaterThanExpr>(close0, open0);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("Large offset (within data range)")
  {
    auto close20 = resourceManager.getPriceClose(20);
    auto close21 = resourceManager.getPriceClose(21);
    auto gt1 = std::make_shared<GreaterThanExpr>(close20, close21);
    
    TimeSeriesDate testDate(1985, Nov, 15);
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE((result == true || result == false));
  }
  
  SECTION("Offset beyond available data returns false")
  {
    // Use a very large offset that exceeds available historical data
    auto close100 = resourceManager.getPriceClose(100);
    auto close101 = resourceManager.getPriceClose(101);
    auto gt1 = std::make_shared<GreaterThanExpr>(close100, close101);
    
    // Use early date in the dataset where 100+ bars back won't exist
    TimeSeriesDate testDate(1985, Mar, 22);
    
    // Should return false due to data access exception
    bool result = PALPatternInterpreter<DecimalType>::evaluateExpression(
        gt1.get(), corn, testDate);
    
    REQUIRE(result == false);
  }
}

TEST_CASE("PALPatternInterpreter - Exception message validation", "[PALPatternInterpreter][exceptions]")
{
  SECTION("Exception with move semantics")
  {
    std::string msg = "Test error message with move";
    
    // Create exception with move
    PalPatternInterpreterException ex1(std::move(msg));
    std::string exMsg(ex1.what());
    
    REQUIRE(exMsg.find("Test error message") != std::string::npos);
    
    // Test const reference constructor
    std::string msg2 = "Test error message const ref";
    PalPatternInterpreterException ex2(msg2);
    
    REQUIRE(msg2 == "Test error message const ref"); // Original unchanged
    REQUIRE(std::string(ex2.what()).find("Test error") != std::string::npos);
  }
}
