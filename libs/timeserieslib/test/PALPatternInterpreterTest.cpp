#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../PALPatternInterpreter.h"
#include "../BoostDateHelper.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using Num = num::DefaultNumber;

typedef num::DefaultNumber DecimalType;
typedef OHLCTimeSeriesEntry<DecimalType> EntryType;

std::string myCornSymbol("C2");

std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString)
{
  return std::make_shared<DecimalType> (num::fromString<DecimalType>(valueString));
}

DecimalType
createDecimal(const std::string& valueString)
{
  return num::fromString<DecimalType>(valueString);
}

std::shared_ptr<OHLCTimeSeriesEntry<DecimalType>>
    createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    auto date1 = from_undelimited_string(dateString);
    auto open1 = num::fromString<DecimalType>(openPrice);
    auto high1 = num::fromString<DecimalType>(highPrice);
    auto low1 = num::fromString<DecimalType>(lowPrice);
    auto close1 = num::fromString<DecimalType>(closePrice);
    return std::make_shared<OHLCTimeSeriesEntry<DecimalType>>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }


std::shared_ptr<OHLCTimeSeriesEntry<DecimalType>>
    createTimeSeriesEntry2 (const TimeSeriesDate& aDate,
		       const DecimalType& openPrice,
		       const DecimalType& highPrice,
		       const DecimalType& lowPrice,
		       const DecimalType& closePrice,
		       volume_t vol)
  {

    return std::make_shared<OHLCTimeSeriesEntry<DecimalType>>(aDate, openPrice, highPrice, lowPrice, 
						closePrice, vol, TimeFrame::DAILY);
  }






TEST_CASE ("PALPatternInterpreter operations", "[PALPatternInterpreter]")
{
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));
  DecimalType cornTickValue(createDecimal("0.25"));
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
    typename Security<DecimalType>::ConstRandomAccessIterator it = 
      corn->getRandomAccessIterator (orderDate);

    REQUIRE_FALSE (it == corn->getRandomAccessIteratorEnd());
    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (and4, corn, it) == true);
  }

SECTION ("PALPatternInterpreter testing for short pattern condition satisfied") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1986, May, 28));
    typename Security<DecimalType>::ConstRandomAccessIterator it = 
      corn->getRandomAccessIterator (orderDate);

    REQUIRE_FALSE (it == corn->getRandomAccessIteratorEnd());
    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (shortand4, corn, it) == true);
  }

  
  SECTION ("PALPatternInterpreter testing for long pattern not matched") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 22));
    TimeSeriesDate endDate(TimeSeriesDate (1985, Nov, 14));

    typename Security<DecimalType>::ConstRandomAccessIterator it;

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	it = corn->findTimeSeriesEntry (orderDate);
	if (it != corn->getRandomAccessIteratorEnd())
	  {
	    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (and4, 
								    corn, 
								    it) == false);
	  }
      }
	

  }

SECTION ("PALPatternInterpreter testing for short pattern not matched") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 22));
    TimeSeriesDate endDate(TimeSeriesDate (1986, May, 27));

    typename Security<DecimalType>::ConstRandomAccessIterator it;

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	it = corn->findTimeSeriesEntry (orderDate);
	if (it != corn->getRandomAccessIteratorEnd())
	  {
	    REQUIRE ( PALPatternInterpreter<DecimalType>::evaluateExpression (shortand4, 
								    corn, 
								    it) == false);
	  }
      }
	

  }


}

