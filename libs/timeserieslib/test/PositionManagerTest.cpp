#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../ClosedPositionHistory.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef dec::decimal<7> DecimalType;
typedef OHLCTimeSeriesEntry<7> EntryType;

std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString)
{
  return std::make_shared<DecimalType> (fromString<DecimalType>(valueString));
}

DecimalType
createDecimal(const std::string& valueString)
{
  return fromString<DecimalType>(valueString);
}

std::shared_ptr<EntryType>
    createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    auto date1 = std::make_shared<date> (from_undelimited_string(dateString));
    auto open1 = std::make_shared<DecimalType> (fromString<DecimalType>(openPrice));
    auto high1 = std::make_shared<DecimalType> (fromString<DecimalType>(highPrice));
    auto low1 = std::make_shared<DecimalType> (fromString<DecimalType>(lowPrice));
    auto close1 = std::make_shared<DecimalType> (fromString<DecimalType>(closePrice));
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }


TEST_CASE ("ClosedPositionHistory operations", "[ClosedPositionHistory]")
{
  auto entry0 = createTimeSeriesEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry1 = createTimeSeriesEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry2 = createTimeSeriesEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry3 = createTimeSeriesEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry4 = createTimeSeriesEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createTimeSeriesEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry6 = createTimeSeriesEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);

  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  auto longExitDate1 = TimeSeriesDate (1985, Dec, 4);
  auto longExitPrice1 = createDecimal("3758.32172");
  auto longEntryDate1 = TimeSeriesDate (1985, Nov, 15);
  auto longEntryPrice1 = createDecimal("3664.51025");

  ClosedLongPosition<7> longPosition1(longEntryDate1, longEntryPrice1, 
				      longExitDate1, longExitPrice1,
				      oneContract, 12);
  
  auto longExitDate2 = TimeSeriesDate (1986, Jun, 12);
  auto longExitPrice2 = createDecimal("3729.28683");
  auto longEntryDate2 = TimeSeriesDate (1986, May, 16);
  auto longEntryPrice2 = createDecimal("3777.64063");

  ClosedLongPosition<7> longPosition2(longEntryDate2, longEntryPrice2, 
				      longExitDate2, longExitPrice2,
				      oneContract, 18);
  
  ClosedLongPosition<7> longPosition3(TimeSeriesDate(1986, Oct, 29),
				      createDecimal("3087.43726"),
				      TimeSeriesDate(1986, Oct, 30),
				      createDecimal("3166.47565"),
				      oneContract, 1);
  ClosedLongPosition<7> longPosition4(TimeSeriesDate(1987, Apr, 22),
				      createDecimal("2808.12280"),
				      TimeSeriesDate(1987, Apr, 24),
				      createDecimal("2880.01075"),
				      oneContract, 2);
  ClosedLongPosition<7> longPosition5(TimeSeriesDate(1987, Dec, 4),
				      createDecimal("2663.11865"),
				      TimeSeriesDate(1987, Dec, 16),
				      createDecimal("2624.47192"),
				      oneContract, 8);
  ClosedLongPosition<7> longPosition6(TimeSeriesDate(1988, Apr, 6),
				      createDecimal("2817.15112"),
				      TimeSeriesDate(1988, Apr, 14),
				      createDecimal("2781.09159"),
				      oneContract, 6);
  ClosedLongPosition<7> longPosition7(TimeSeriesDate(1989, Apr, 14),
				      createDecimal("3198.38672"),
				      TimeSeriesDate(1989, Apr, 17),
				      createDecimal("3280.26542"),
				      oneContract, 1);
  ClosedLongPosition<7> longPosition8(TimeSeriesDate(1990, Jun, 5),
				      createDecimal("3207.87378"),
				      TimeSeriesDate(1990, Jun, 8),
				      createDecimal("3289.99535"),
				      oneContract, 3);
  ClosedLongPosition<7> longPosition9(TimeSeriesDate(1990, Dec, 7),
				      createDecimal("2698.28857"),
				      TimeSeriesDate(1990, Dec, 20),
				      createDecimal("2663.75048"),
				      oneContract, 9);
  ClosedLongPosition<7> longPosition10(TimeSeriesDate(1991, Jul, 24),
				      createDecimal("2631.70996"),
				      TimeSeriesDate(1991, Jul, 29),
				      createDecimal("2778.95728"),
				      oneContract, 3);
  ClosedLongPosition<7> longPosition11(TimeSeriesDate(1991, Aug, 5),
				      createDecimal("2637.06445"),
				      TimeSeriesDate(1991, Aug, 6),
				      createDecimal("2704.57330"),
				      oneContract, 1);
  ClosedLongPosition<7> longPosition12(TimeSeriesDate(1993, Jun, 30),
				      createDecimal("1917.15833"),
				      TimeSeriesDate(1993, Jul, 1),
				      createDecimal("1966.23758"),
				       oneContract, 1);
  ClosedLongPosition<7> longPosition13(TimeSeriesDate(1994, Jun, 22),
				       createDecimal("1972.07410"),
				       TimeSeriesDate(1994, Jun, 27),
				       createDecimal("1946.83155"),
				       oneContract, 3);
  ClosedLongPosition<7> longPosition14(TimeSeriesDate(1995, Jun, 9),
				       createDecimal("1880.15967"),
				       TimeSeriesDate(1995, Jun, 15),
				       createDecimal("1928.29176"),
				       oneContract, 4);
  ClosedLongPosition<7> longPosition15(TimeSeriesDate(1995, Aug, 23),
				       createDecimal("1935.83447"),
				       TimeSeriesDate(1995, Aug, 28),
				       createDecimal("1985.39184"),
				       oneContract, 3);
  ClosedLongPosition<7> longPosition16(TimeSeriesDate(1995, Oct, 6),
				       createDecimal("2116.94531"),
				       TimeSeriesDate(1995, Oct, 10),
				       createDecimal("2171.13911"),
				       oneContract, 2);
  ClosedLongPosition<7> longPosition17(TimeSeriesDate(1995, Nov, 9),
				       createDecimal("2224.58643"),
				       TimeSeriesDate(1995, Nov, 14),
				       createDecimal("2196.11172"),
				       oneContract, 3);
  ClosedLongPosition<7> longPosition18(TimeSeriesDate(1996, May, 22),
				       createDecimal("3384.33862"),
				       TimeSeriesDate(1996, May, 28),
				       createDecimal("3341.01909"),
				       oneContract, 3);
  ClosedLongPosition<7> longPosition19(TimeSeriesDate(1997, Apr, 8),
				       createDecimal("2683.75391"),
				       TimeSeriesDate(1997, Apr, 11),
				       createDecimal("2752.45801"),
				       oneContract, 3);
  ClosedLongPosition<7> longPosition20(TimeSeriesDate(1997, Oct, 17),
				       createDecimal("2617.33667"),
				       TimeSeriesDate(1997, Oct, 21),
				       createDecimal("2684.34049"),
				       oneContract, 3);
  ClosedLongPosition<7> longPosition21(TimeSeriesDate(1999, Sep, 13),
				       createDecimal("1439.19373"),
				       TimeSeriesDate(1999, Sep, 15),
				       createDecimal("1420.77205"),
				       oneContract, 2);
  ClosedLongPosition<7> longPosition22(TimeSeriesDate(2007, Jan, 23),
				       createDecimal("688.56763"),
				       TimeSeriesDate(2007, Jan, 24),
				       createDecimal("679.75396"),
				       oneContract, 2);
  ClosedLongPosition<7> longPosition23(TimeSeriesDate(2008, Jun, 16),
				       createDecimal("983.35834"),
				       TimeSeriesDate(2008, Jun, 18),
				       createDecimal("1008.53231"),
				       oneContract, 2);
  ClosedLongPosition<7> longPosition24(TimeSeriesDate(2008, Jun, 23),
				       createDecimal("980.89520"),
				       TimeSeriesDate(2008, Jun, 24),
				       createDecimal("968.33974"),
				       oneContract, 1);



  ClosedPositionHistory<7> closedLongPositions;
  closedLongPositions.addClosedPosition(longPosition1);
  closedLongPositions.addClosedPosition(longPosition2);
  closedLongPositions.addClosedPosition(longPosition3);
  closedLongPositions.addClosedPosition(longPosition4);
  closedLongPositions.addClosedPosition(longPosition5);
  closedLongPositions.addClosedPosition(longPosition6);
  closedLongPositions.addClosedPosition(longPosition7);
  closedLongPositions.addClosedPosition(longPosition8);
  closedLongPositions.addClosedPosition(longPosition9);
  closedLongPositions.addClosedPosition(longPosition10);
  closedLongPositions.addClosedPosition(longPosition11);
  closedLongPositions.addClosedPosition(longPosition12);
  closedLongPositions.addClosedPosition(longPosition13);
  closedLongPositions.addClosedPosition(longPosition14);
  closedLongPositions.addClosedPosition(longPosition15);
  closedLongPositions.addClosedPosition(longPosition16);
  closedLongPositions.addClosedPosition(longPosition17);
  closedLongPositions.addClosedPosition(longPosition18);
  closedLongPositions.addClosedPosition(longPosition19);
  closedLongPositions.addClosedPosition(longPosition20);
  closedLongPositions.addClosedPosition(longPosition21);
  closedLongPositions.addClosedPosition(longPosition22);
  closedLongPositions.addClosedPosition(longPosition23);
  closedLongPositions.addClosedPosition(longPosition24);


  REQUIRE (closedLongPositions.getNumPositions() == 24);
  REQUIRE (closedLongPositions.getProfitFactor() >= createDecimal("2.99"));
  REQUIRE (closedLongPositions.getPercentWinners() == createDecimal("58.3333300"));
  REQUIRE (closedLongPositions.getPercentLosers() == (createDecimal ("100.00") - closedLongPositions.getPercentWinners()));
  REQUIRE (closedLongPositions.getNumWinningPositions() == 14);
  REQUIRE (closedLongPositions.getNumLosingPositions() == 10);
  REQUIRE (closedLongPositions.getPayoffRatio() == createDecimal("2.1407415"));
  REQUIRE (closedLongPositions.getPALProfitability() == createDecimal("58.3333300"));

  
  auto shortEntryDate1 = TimeSeriesDate (1986, May, 28);
  auto shortEntryPrice1 = createDecimal("3789.64575");
  auto shortExitDate1 = TimeSeriesDate (1986, Jun, 11);
  auto shortExitPrice1 = createDecimal("3738.86450");

  ClosedShortPosition<7> shortPosition1(shortEntryDate1, shortEntryPrice1, 
				      shortExitDate1, shortExitPrice1,
				      oneContract, 10);

 
  auto shortEntryDate2 = TimeSeriesDate (1986, Nov, 10);
  auto shortEntryPrice2 = createDecimal("3100.99854");
  auto shortExitDate2 = TimeSeriesDate (1986, Nov, 12);
  auto shortExitPrice2 = createDecimal("3140.69132");

  ClosedShortPosition<7> shortPosition2(shortEntryDate2, shortEntryPrice2, 
				      shortExitDate2, shortExitPrice2,
				      oneContract, 2);

  ClosedShortPosition<7> shortPosition3(TimeSeriesDate(1987, Jan, 30),
					createDecimal("2690.04077"),
					TimeSeriesDate(1987, Feb, 5),
					createDecimal("2653.99423"),
					oneContract, 4); 
  ClosedShortPosition<7> shortPosition4(TimeSeriesDate(1987, May, 21),
					createDecimal("3014.07813"),
					TimeSeriesDate(1987, May, 26),
					createDecimal("2973.68948"),
					oneContract, 2); 
  ClosedShortPosition<7> shortPosition5(TimeSeriesDate(1987, Jun, 3),
					createDecimal("3006.15674"),
					TimeSeriesDate(1987, Jun, 10),
					createDecimal("2950.70728"),
					oneContract, 5);
  ClosedShortPosition<7> shortPosition6(TimeSeriesDate(1989, Jul, 20),
					createDecimal("2918.04443"),
					TimeSeriesDate(1989, Jul, 24),
					createDecimal("2878.94264"),
					oneContract, 2); 
  ClosedShortPosition<7> shortPosition7(TimeSeriesDate(1990, Nov, 19),
					createDecimal("2703.38110"),
					TimeSeriesDate(1990, Nov, 20),
					createDecimal("2667.15580"),
					oneContract, 1);
  ClosedShortPosition<7> shortPosition8(TimeSeriesDate(1991, Jul, 2),
					createDecimal("2452.33594"),
					TimeSeriesDate(1991, Jul, 5),
					createDecimal("2419.47464"),
					oneContract, 2);
  ClosedShortPosition<7> shortPosition9(TimeSeriesDate(1996, May, 2),
					createDecimal("3180.06665"),
					TimeSeriesDate(1996, May, 3),
					createDecimal("3137.45376"),
					oneContract, 1);
  ClosedShortPosition<7> shortPosition10(TimeSeriesDate(1997, Sep, 24),
					 createDecimal("2444.86743"),
					 TimeSeriesDate(1997, Sep, 25),
					 createDecimal("2412.10621"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition11(TimeSeriesDate(2001, Mar, 13),
					 createDecimal("1047.40698"),
					 TimeSeriesDate(2001, Mar, 14),
					 createDecimal("1033.37173"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition12(TimeSeriesDate(2001, Oct, 24),
					 createDecimal("853.33160"),
					 TimeSeriesDate(2001, Oct, 29),
					 createDecimal("841.89696"),
					 oneContract, 3);

  ClosedShortPosition<7> shortPosition13(TimeSeriesDate(2003, Oct, 3),
					 createDecimal("735.21429"),
					 TimeSeriesDate(2003, Oct, 7),
					 createDecimal("744.62504"),
					 oneContract, 2);
  ClosedShortPosition<7> shortPosition14(TimeSeriesDate(2006, Aug, 10),
					 createDecimal("450.62540"),
					 TimeSeriesDate(2006, Aug, 11),
					 createDecimal("444.58702"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition15(TimeSeriesDate(2007, Mar, 29),
					 createDecimal("644.04504"),
					 TimeSeriesDate(2007, Mar, 30),
					 createDecimal("635.41484"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition16(TimeSeriesDate(2007, May, 11),
					 createDecimal("583.31305"),
					 TimeSeriesDate(2007, May, 14),
					 createDecimal("575.49665"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition17(TimeSeriesDate(2007, May, 25),
					 createDecimal("592.01331"),
					 TimeSeriesDate(2007, May, 29),
					 createDecimal("584.08033"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition18(TimeSeriesDate(2008, Jul, 3),
					 createDecimal("984.19678"),
					 TimeSeriesDate(2008, Jul, 7),
					 createDecimal("971.00854"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition19(TimeSeriesDate(2008, Dec, 9),
					 createDecimal("399.64169"),
					 TimeSeriesDate(2008, Dec, 10),
					 createDecimal("404.75711"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition20(TimeSeriesDate(2010, Nov, 19),
					 createDecimal("489.98853"),
					 TimeSeriesDate(2010, Nov, 22),
					 createDecimal("496.26038"),
					 oneContract, 1);
  ClosedShortPosition<7> shortPosition21(TimeSeriesDate(2011, Sep, 13),
					 createDecimal("649.45618"),
					 TimeSeriesDate(2011, Sep, 15),
					 createDecimal("640.75346"),
					 oneContract, 2);

  ClosedPositionHistory<7> closedShortPositions;
  closedShortPositions.addClosedPosition(shortPosition1);
  closedShortPositions.addClosedPosition(shortPosition2);
  closedShortPositions.addClosedPosition(shortPosition3);
  closedShortPositions.addClosedPosition(shortPosition4);
  closedShortPositions.addClosedPosition(shortPosition5);
  closedShortPositions.addClosedPosition(shortPosition6);
  closedShortPositions.addClosedPosition(shortPosition7);
  closedShortPositions.addClosedPosition(shortPosition8);
  closedShortPositions.addClosedPosition(shortPosition9);
  closedShortPositions.addClosedPosition(shortPosition10);
  closedShortPositions.addClosedPosition(shortPosition11);
  closedShortPositions.addClosedPosition(shortPosition12);
  closedShortPositions.addClosedPosition(shortPosition13);
  closedShortPositions.addClosedPosition(shortPosition14);
  closedShortPositions.addClosedPosition(shortPosition15);
  closedShortPositions.addClosedPosition(shortPosition16);
  closedShortPositions.addClosedPosition(shortPosition17);
  closedShortPositions.addClosedPosition(shortPosition18);
  closedShortPositions.addClosedPosition(shortPosition19);
  closedShortPositions.addClosedPosition(shortPosition20);
  closedShortPositions.addClosedPosition(shortPosition21);

  REQUIRE (closedShortPositions.getNumPositions() == 21);
  REQUIRE (closedShortPositions.getNumWinningPositions() == 17);
  REQUIRE (closedShortPositions.getProfitFactor() >= createDecimal("4.53"));
  REQUIRE (closedShortPositions.getPercentWinners() >= createDecimal("80.94"));
  REQUIRE (closedShortPositions.getPercentLosers() == (createDecimal ("100.00") - closedShortPositions.getPercentWinners()));
  REQUIRE (closedShortPositions.getNumLosingPositions() == 4);
  REQUIRE (closedShortPositions.getPayoffRatio() >= createDecimal("1.06"));
  REQUIRE (closedShortPositions.getPALProfitability() >= createDecimal("80.9400000"));

  SECTION ("ClosedPositionHistory Longs Iterator tests");
  {
    ClosedPositionHistory<7>::ClosedPositionIterator it = closedLongPositions.beginClosedPositions();

    REQUIRE (it->first == longEntryDate1);
    REQUIRE (*(it->second) == longPosition1);

    it++;
    it++;

    REQUIRE (it->first == longPosition3.getEntryDate());
    REQUIRE (*(it->second) == longPosition3);
    
  }

SECTION ("ClosedPositionHistory Longs ConstIterator tests");
  {
    ClosedPositionHistory<7>::ConstClosedPositionIterator it = closedLongPositions.beginClosedPositions();

    REQUIRE (it->first == longEntryDate1);
    REQUIRE (*(it->second) == longPosition1);

    it++;
    it++;

    REQUIRE (it->first == longPosition3.getEntryDate());
    REQUIRE (*(it->second) == longPosition3);
    
  }

  

  
  
  
}

