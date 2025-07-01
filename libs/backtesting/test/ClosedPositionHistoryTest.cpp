#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "ClosedPositionHistory.h"
#include "TimeSeriesIndicators.h"
#include "TestUtils.h"
#include <cmath>
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
using namespace mkc_timeseries;
using namespace boost::gregorian;

const static std::string myCornSymbol("C2");



void addBarHistoryUntilDate (std::shared_ptr<TradingPosition<DecimalType>> openPosition,
			    const TimeSeriesDate& entryDate,
			    const TimeSeriesDate& exitDate,
			    const std::shared_ptr<OHLCTimeSeries<DecimalType>>& aTimeSeries )
{
  // Use the new API with sorted iterators instead of getTimeSeriesEntry
  auto it = aTimeSeries->beginSortedAccess();
  auto itEnd = aTimeSeries->endSortedAccess();
  
  // Find the entry date
  bool foundEntry = false;
  for (; it != itEnd; ++it) {
    if (it->getDateTime().date() == entryDate) {
      foundEntry = true;
      // Don't advance iterator here, the entry bar itself is handled by the constructor.
      // This loop adds subsequent bars.
      break; 
    }
  }
  
  if (!foundEntry) {
    return; // Entry date not found
  }
  
  // Add bars until exit date
  for (; it != itEnd; ++it) {
    if (it->getDateTime().date() > entryDate)
        openPosition->addBar(*it);

    if (it->getDateTime().date() >= exitDate) {
      break; // Exit bar's date reached, we're done
    }
  }
}

std::shared_ptr<TradingPositionLong<DecimalType>>
createClosedLongPosition (const std::shared_ptr<OHLCTimeSeries<DecimalType>>& aTimeSeries,
			  const TimeSeriesDate& entryDate,
			  const DecimalType& entryPrice,
			  const TimeSeriesDate& exitDate,
			  const DecimalType& exitPrice,
			  const TradingVolume& tVolume,
			  int dummy)
{
  // Correctly find the entry bar by iterating
  auto series_it = aTimeSeries->beginSortedAccess();
  auto series_end = aTimeSeries->endSortedAccess();
  auto entry_bar_it = std::find_if(series_it, series_end, 
                                   [&](const OHLCTimeSeriesEntry<DecimalType>& entry) {
                                       return entry.getDateTime().date() == entryDate;
                                   });

  if (entry_bar_it == series_end)
      throw std::runtime_error("Entry date not found in time series for createClosedLongPosition");


  auto aPos = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol,
								 entryPrice,
								 *entry_bar_it,
								 tVolume);

  addBarHistoryUntilDate (aPos, entryDate, exitDate, aTimeSeries);
  aPos->ClosePosition (exitDate,
		       exitPrice);
  return aPos;
}


std::shared_ptr<TradingPositionShort<DecimalType>>
createClosedShortPosition (const std::shared_ptr<OHLCTimeSeries<DecimalType>>& aTimeSeries,
			   const TimeSeriesDate& entryDate,
			  const DecimalType& entryPrice,
			  const TimeSeriesDate& exitDate,
			  const DecimalType& exitPrice,
			  const TradingVolume& tVolume,
			  int dummy)
{
  // Correctly find the entry bar by iterating
  auto series_it = aTimeSeries->beginSortedAccess();
  auto series_end = aTimeSeries->endSortedAccess();
  auto entry_bar_it = std::find_if(series_it, series_end, 
                                   [&](const OHLCTimeSeriesEntry<DecimalType>& entry) {
                                       return entry.getDateTime().date() == entryDate;
                                   });

  if (entry_bar_it == series_end)
      throw std::runtime_error("Entry date not found in time series for createClosedShortPosition");

  auto aPos = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol,
						       entryPrice,
						       *entry_bar_it,
						       tVolume);
  addBarHistoryUntilDate (aPos, entryDate, exitDate, aTimeSeries);

  aPos->ClosePosition (exitDate,
		       exitPrice);
  return aPos;
}

DecimalType getLnReturn(const DecimalType& entryPrice, const DecimalType exitPrice)
{
  DecimalType lnArg (exitPrice / entryPrice);
  double lnOfReturn = std::log(lnArg.getAsDouble());
  return DecimalType(lnOfReturn);
}

TEST_CASE ("ClosedPositionHistory operations", "[ClosedPositionHistory]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();

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

  auto longPosition1 = createClosedLongPosition (p, longEntryDate1, longEntryPrice1,
						 longExitDate1, longExitPrice1,
						 oneContract, 12);

  auto position1LogReturn = longPosition1->getLogTradeReturn();
  auto position1ReferenceLnReturn = getLnReturn(longEntryPrice1, longExitPrice1);

  std::cout << "Position 1 log return " << position1LogReturn << " Calculated ln return = " << position1ReferenceLnReturn << std::endl;
  std::cout << "position 1 percent return = " << longPosition1->getPercentReturn() << std::endl;

  auto longExitDate2 = TimeSeriesDate (1986, Jun, 12);
  auto longExitPrice2 = createDecimal("3729.28683");
  auto longEntryDate2 = TimeSeriesDate (1986, May, 16);
  auto longEntryPrice2 = createDecimal("3777.64063");

  auto longPosition2  = createClosedLongPosition (p, longEntryDate2, longEntryPrice2,
						  longExitDate2, longExitPrice2,
						  oneContract, 18);

  auto position2LogReturn = longPosition2->getLogTradeReturn();
  auto position2ReferenceLnReturn = getLnReturn(longEntryPrice2, longExitPrice2);

    std::cout << "Position 2 log return " << position2LogReturn << " Calculated ln return = " << position2ReferenceLnReturn << std::endl;
  std::cout << "position 2 percent return = " << longPosition2->getPercentReturn() << std::endl;

  auto longPosition3 = createClosedLongPosition (p, TimeSeriesDate(1986, Oct, 29),
						 createDecimal("3087.43726"),
						 TimeSeriesDate(1986, Oct, 30),
						 createDecimal("3166.47565"),
						 oneContract, 1);
  auto longPosition4 = createClosedLongPosition (p, TimeSeriesDate(1987, Apr, 22),
				      createDecimal("2808.12280"),
				      TimeSeriesDate(1987, Apr, 24),
				      createDecimal("2880.01075"),
				      oneContract, 2);
  auto longPosition5 = createClosedLongPosition (p, TimeSeriesDate(1987, Dec, 4),
				      createDecimal("2663.11865"),
				      TimeSeriesDate(1987, Dec, 16),
				      createDecimal("2624.47192"),
				      oneContract, 8);
  auto longPosition6 = createClosedLongPosition (p, TimeSeriesDate(1988, Apr, 6),
				      createDecimal("2817.15112"),
				      TimeSeriesDate(1988, Apr, 14),
				      createDecimal("2781.09159"),
				      oneContract, 6);
  auto longPosition7 = createClosedLongPosition (p, TimeSeriesDate(1989, Apr, 14),
				      createDecimal("3198.38672"),
				      TimeSeriesDate(1989, Apr, 17),
				      createDecimal("3280.26542"),
				      oneContract, 1);
  auto longPosition8 = createClosedLongPosition (p, TimeSeriesDate(1990, Jun, 5),
				      createDecimal("3207.87378"),
				      TimeSeriesDate(1990, Jun, 8),
				      createDecimal("3289.99535"),
				      oneContract, 3);
  auto longPosition9 = createClosedLongPosition (p, TimeSeriesDate(1990, Dec, 7),
				      createDecimal("2698.28857"),
				      TimeSeriesDate(1990, Dec, 20),
				      createDecimal("2663.75048"),
				      oneContract, 9);
  auto longPosition10 = createClosedLongPosition (p, TimeSeriesDate(1991, Jul, 24),
				      createDecimal("2631.70996"),
				      TimeSeriesDate(1991, Jul, 29),
				      createDecimal("2778.95728"),
				      oneContract, 3);
  auto longPosition11 = createClosedLongPosition (p, TimeSeriesDate(1991, Aug, 5),
				      createDecimal("2637.06445"),
				      TimeSeriesDate(1991, Aug, 6),
				      createDecimal("2704.57330"),
				      oneContract, 1);
  auto longPosition12 = createClosedLongPosition (p, TimeSeriesDate(1993, Jun, 30),
				      createDecimal("1917.15833"),
				      TimeSeriesDate(1993, Jul, 1),
				      createDecimal("1966.23758"),
				       oneContract, 1);
  auto longPosition13 = createClosedLongPosition (p, TimeSeriesDate(1994, Jun, 22),
				       createDecimal("1972.07410"),
				       TimeSeriesDate(1994, Jun, 27),
				       createDecimal("1946.83155"),
				       oneContract, 3);
  auto longPosition14 = createClosedLongPosition (p, TimeSeriesDate(1995, Jun, 9),
				       createDecimal("1880.15967"),
				       TimeSeriesDate(1995, Jun, 15),
				       createDecimal("1928.29176"),
				       oneContract, 4);
  auto longPosition15 = createClosedLongPosition (p, TimeSeriesDate(1995, Aug, 23),
				       createDecimal("1935.83447"),
				       TimeSeriesDate(1995, Aug, 28),
				       createDecimal("1985.39184"),
				       oneContract, 3);
  auto longPosition16 = createClosedLongPosition (p, TimeSeriesDate(1995, Oct, 6),
				       createDecimal("2116.94531"),
				       TimeSeriesDate(1995, Oct, 10),
				       createDecimal("2171.13911"),
				       oneContract, 2);
  auto longPosition17 = createClosedLongPosition (p, TimeSeriesDate(1995, Nov, 9),
				       createDecimal("2224.58643"),
				       TimeSeriesDate(1995, Nov, 14),
				       createDecimal("2196.11172"),
				       oneContract, 3);
  auto longPosition18 = createClosedLongPosition (p, TimeSeriesDate(1996, May, 22),
				       createDecimal("3384.33862"),
				       TimeSeriesDate(1996, May, 28),
				       createDecimal("3341.01909"),
				       oneContract, 3);
  auto longPosition19 = createClosedLongPosition (p, TimeSeriesDate(1997, Apr, 8),
				       createDecimal("2683.75391"),
				       TimeSeriesDate(1997, Apr, 11),
				       createDecimal("2752.45801"),
				       oneContract, 3);
  auto longPosition20 = createClosedLongPosition (p, TimeSeriesDate(1997, Oct, 17),
				       createDecimal("2617.33667"),
				       TimeSeriesDate(1997, Oct, 21),
				       createDecimal("2684.34049"),
				       oneContract, 3);
  auto longPosition21 = createClosedLongPosition (p, TimeSeriesDate(1999, Sep, 13),
				       createDecimal("1439.19373"),
				       TimeSeriesDate(1999, Sep, 15),
				       createDecimal("1420.77205"),
				       oneContract, 2);
  auto longPosition22 = createClosedLongPosition (p, TimeSeriesDate(2007, Jan, 23),
				       createDecimal("688.56763"),
				       TimeSeriesDate(2007, Jan, 24),
				       createDecimal("679.75396"),
				       oneContract, 2);
  auto longPosition23 = createClosedLongPosition (p, TimeSeriesDate(2008, Jun, 16),
				       createDecimal("983.35834"),
				       TimeSeriesDate(2008, Jun, 18),
				       createDecimal("1008.53231"),
				       oneContract, 2);
  auto longPosition24 = createClosedLongPosition (p, TimeSeriesDate(2008, Jun, 23),
						  createDecimal("980.89520"),
						  TimeSeriesDate(2008, Jun, 24),
						  createDecimal("968.33974"),
						  oneContract, 1);



  ClosedPositionHistory<DecimalType> closedLongPositions;
  DecimalType longCumReturn(longPosition1->getTradeReturnMultiplier());

  closedLongPositions.addClosedPosition(longPosition1);
  closedLongPositions.addClosedPosition(longPosition2);
  longCumReturn =longCumReturn * longPosition2->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition3);
  longCumReturn =longCumReturn * longPosition3->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition4);
  longCumReturn =longCumReturn * longPosition4->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition5);
  longCumReturn =longCumReturn * longPosition5->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition6);
  longCumReturn =longCumReturn * longPosition6->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition7);
  longCumReturn =longCumReturn * longPosition7->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition8);
  longCumReturn =longCumReturn * longPosition8->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition9);
  longCumReturn =longCumReturn * longPosition9->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition10);
  longCumReturn =longCumReturn * longPosition10->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition11);
  longCumReturn =longCumReturn * longPosition11->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition12);
  longCumReturn =longCumReturn * longPosition12->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition13);
  longCumReturn =longCumReturn * longPosition13->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition14);
  longCumReturn =longCumReturn * longPosition14->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition15);
  longCumReturn =longCumReturn * longPosition15->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition16);
  longCumReturn =longCumReturn * longPosition16->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition17);
  longCumReturn = longCumReturn * longPosition17->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition18);
  longCumReturn =longCumReturn * longPosition18->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition19);
  longCumReturn =longCumReturn * longPosition19->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition20);
  longCumReturn =longCumReturn * longPosition20->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition21);
  longCumReturn =longCumReturn * longPosition21->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition22);
  longCumReturn =longCumReturn * longPosition22->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition23);
  longCumReturn = longCumReturn * longPosition23->getTradeReturnMultiplier();

  closedLongPositions.addClosedPosition(longPosition24);
  longCumReturn = longCumReturn * longPosition24->getTradeReturnMultiplier();
  longCumReturn = longCumReturn - DecimalConstants<DecimalType>::DecimalOne;

  std::cout << "Cumulative return for longpositions = " << closedLongPositions.getCumulativeReturn() << std::endl;
  std::vector<unsigned int> barsInPositions(closedLongPositions.beginBarsPerPosition(),
					    closedLongPositions.endBarsPerPosition()) ;
  unsigned int barsMedian = Median (barsInPositions);
  std::cout << "Median bars in positions = " << barsMedian << std::endl;
  REQUIRE (barsInPositions.size() == 24);

  REQUIRE (longCumReturn == closedLongPositions.getCumulativeReturn());
  REQUIRE (closedLongPositions.getNumPositions() == 24);
  REQUIRE (closedLongPositions.getProfitFactor() >= createDecimal("2.99"));
  REQUIRE (closedLongPositions.getPercentWinners() == createDecimal("58.3333300"));
  REQUIRE (closedLongPositions.getPercentLosers() == (createDecimal ("100.00") - closedLongPositions.getPercentWinners()));
  REQUIRE (closedLongPositions.getNumWinningPositions() == 14);
  REQUIRE (closedLongPositions.getNumLosingPositions() == 10);
  REQUIRE (closedLongPositions.getPayoffRatio() == createDecimal("2.1407415"));
  std::cout << "For payoffratio = 2.14, median payoff ratio = " << closedLongPositions.getMedianPayoffRatio() << std::endl;
  std::cout << "For payoffratio = 2.14, geometric payoff ratio = " << closedLongPositions.getGeometricPayoffRatio() << std::endl;
  REQUIRE (closedLongPositions.getPALProfitability() == createDecimal("58.3333300"));


  auto shortEntryDate1 = TimeSeriesDate (1986, May, 28);
  auto shortEntryPrice1 = createDecimal("3789.64575");
  auto shortExitDate1 = TimeSeriesDate (1986, Jun, 11);
  auto shortExitPrice1 = createDecimal("3738.86450");

  auto shortPosition1 =  createClosedShortPosition (p, shortEntryDate1, shortEntryPrice1,
						    shortExitDate1, shortExitPrice1,
						    oneContract, 10);


  auto shortEntryDate2 = TimeSeriesDate (1986, Nov, 10);
  auto shortEntryPrice2 = createDecimal("3100.99854");
  auto shortExitDate2 = TimeSeriesDate (1986, Nov, 12);
  auto shortExitPrice2 = createDecimal("3140.69132");

  auto shortPosition2 = createClosedShortPosition(p, shortEntryDate2, shortEntryPrice2,
						  shortExitDate2, shortExitPrice2,
						  oneContract, 2);

  auto shortPosition3 = createClosedShortPosition (p, TimeSeriesDate(1987, Jan, 30),
					createDecimal("2690.04077"),
					TimeSeriesDate(1987, Feb, 5),
					createDecimal("2653.99423"),
					oneContract, 4);
  auto shortPosition4 = createClosedShortPosition (p, TimeSeriesDate(1987, May, 22),
					createDecimal("3014.07813"),
					TimeSeriesDate(1987, May, 26),
					createDecimal("2973.68948"),
					oneContract, 2);
  auto shortPosition5 = createClosedShortPosition (p, TimeSeriesDate(1987, Jun, 3),
					createDecimal("3006.15674"),
					TimeSeriesDate(1987, Jun, 10),
					createDecimal("2950.70728"),
					oneContract, 5);
  auto shortPosition6 = createClosedShortPosition (p, TimeSeriesDate(1989, Jul, 20),
					createDecimal("2918.04443"),
					TimeSeriesDate(1989, Jul, 24),
					createDecimal("2878.94264"),
					oneContract, 2);
  auto shortPosition7 = createClosedShortPosition (p, TimeSeriesDate(1990, Nov, 19),
					createDecimal("2703.38110"),
					TimeSeriesDate(1990, Nov, 20),
					createDecimal("2667.15580"),
					oneContract, 1);
  auto shortPosition8 = createClosedShortPosition (p, TimeSeriesDate(1991, Jul, 2),
					createDecimal("2452.33594"),
					TimeSeriesDate(1991, Jul, 5),
					createDecimal("2419.47464"),
					oneContract, 2);
  auto shortPosition9 = createClosedShortPosition (p, TimeSeriesDate(1996, May, 2),
					createDecimal("3180.06665"),
					TimeSeriesDate(1996, May, 3),
					createDecimal("3137.45376"),
					oneContract, 1);
  auto shortPosition10 = createClosedShortPosition (p, TimeSeriesDate(1997, Sep, 24),
					 createDecimal("2444.86743"),
					 TimeSeriesDate(1997, Sep, 25),
					 createDecimal("2412.10621"),
					 oneContract, 1);
  auto shortPosition11 = createClosedShortPosition (p, TimeSeriesDate(2001, Mar, 13),
					 createDecimal("1047.40698"),
					 TimeSeriesDate(2001, Mar, 14),
					 createDecimal("1033.37173"),
					 oneContract, 1);
  auto shortPosition12 = createClosedShortPosition (p, TimeSeriesDate(2001, Oct, 24),
					 createDecimal("853.33160"),
					 TimeSeriesDate(2001, Oct, 29),
					 createDecimal("841.89696"),
					 oneContract, 3);

  auto shortPosition13 = createClosedShortPosition (p, TimeSeriesDate(2003, Oct, 3),
					 createDecimal("735.21429"),
					 TimeSeriesDate(2003, Oct, 7),
					 createDecimal("744.62504"),
					 oneContract, 2);
  auto shortPosition14 = createClosedShortPosition (p, TimeSeriesDate(2006, Aug, 10),
					 createDecimal("450.62540"),
					 TimeSeriesDate(2006, Aug, 11),
					 createDecimal("444.58702"),
					 oneContract, 1);
  auto shortPosition15 = createClosedShortPosition (p, TimeSeriesDate(2007, Mar, 29),
					 createDecimal("644.04504"),
					 TimeSeriesDate(2007, Mar, 30),
					 createDecimal("635.41484"),
					 oneContract, 1);
  auto shortPosition16 = createClosedShortPosition (p, TimeSeriesDate(2007, May, 11),
					 createDecimal("583.31305"),
					 TimeSeriesDate(2007, May, 14),
					 createDecimal("575.49665"),
					 oneContract, 1);
  auto shortPosition17 = createClosedShortPosition (p, TimeSeriesDate(2007, May, 25),
					 createDecimal("592.01331"),
					 TimeSeriesDate(2007, May, 29),
					 createDecimal("584.08033"),
					 oneContract, 1);
  auto shortPosition18 = createClosedShortPosition (p, TimeSeriesDate(2008, Jul, 3),
					 createDecimal("984.19678"),
					 TimeSeriesDate(2008, Jul, 7),
					 createDecimal("971.00854"),
					 oneContract, 1);
  auto shortPosition19 = createClosedShortPosition (p, TimeSeriesDate(2008, Dec, 9),
					 createDecimal("399.64169"),
					 TimeSeriesDate(2008, Dec, 10),
					 createDecimal("404.75711"),
					 oneContract, 1);
  auto shortPosition20 = createClosedShortPosition (p, TimeSeriesDate(2010, Nov, 19),
					 createDecimal("489.98853"),
					 TimeSeriesDate(2010, Nov, 22),
					 createDecimal("496.26038"),
					 oneContract, 1);
  auto shortPosition21 = createClosedShortPosition (p, TimeSeriesDate(2011, Sep, 13),
						   createDecimal("649.45618"),
						   TimeSeriesDate(2011, Sep, 15),
						   createDecimal("640.75346"),
						   oneContract, 2);

  ClosedPositionHistory<DecimalType> closedShortPositions;
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

  //printPositionHistory (closedShortPositions);

  //std::cout << "Cumulative return for short positions = " << closedShortPositions.getCumulativeReturn() << std::endl;
  REQUIRE (closedShortPositions.getNumPositions() == 21);
  REQUIRE (closedShortPositions.getNumWinningPositions() == 17);
  REQUIRE (closedShortPositions.getProfitFactor() >= createDecimal("4.53"));
  REQUIRE (closedShortPositions.getPercentWinners() >= createDecimal("80.94"));
  REQUIRE (closedShortPositions.getPercentLosers() == (createDecimal ("100.00") - closedShortPositions.getPercentWinners()));
  REQUIRE (closedShortPositions.getNumLosingPositions() == 4);
  REQUIRE (closedShortPositions.getPayoffRatio() >= createDecimal("1.06"));
  //std::cout << "For payoffratio = 1.06, median payoff ratio = " << closedShortPositions.getMedianPayoffRatio() << std::endl;
  //std::cout << "For payoffratio = 1.06, geometric payoff ratio = " << closedShortPositions.getGeometricPayoffRatio() << std::endl;
  REQUIRE (closedShortPositions.getPALProfitability() >= createDecimal("80.9400000"));
  //std::cout << "Median PAL profitability = " << closedShortPositions.getMedianPALProfitability() << " Geometric PAL profitability = " << closedShortPositions.getGeometricPALProfitability() << std::endl;


  SECTION ("Test return iterator")
    {


      ClosedPositionHistory<DecimalType>::ConstTradeReturnIterator winnersIterator =
	closedLongPositions.beginWinnersReturns();

      ClosedPositionHistory<DecimalType>::ConstTradeReturnIterator losersIterator =
	closedLongPositions.beginLosersReturns();

      if (longPosition1->isWinningPosition())
	{
	  REQUIRE (*winnersIterator == longPosition1->getPercentReturn().getAsDouble());
	  winnersIterator++;
	}
      else
	{
	  REQUIRE (*losersIterator == longPosition1->getPercentReturn().abs().getAsDouble());
	  losersIterator++;
	}

      if (longPosition2->isWinningPosition())
	{
	  REQUIRE (*winnersIterator == longPosition2->getPercentReturn().getAsDouble());
	  winnersIterator++;
	}
      else
	{
	  REQUIRE (*losersIterator == longPosition2->getPercentReturn().abs().getAsDouble());
	  losersIterator++;
	}
    }

  SECTION ("ClosedPositionHistory Longs ConstIterator tests")
    {
      /*
	ClosedPositionHistory<DecimalType>::ConstPositionIterator  it = closedLongPositions.beginClosedPositions();

	REQUIRE (it->first == longEntryDate1);
	REQUIRE (*(it->second) == longPosition1);

	it++;
	it++;

	REQUIRE (it->first == longPosition3.getEntryDate());
	REQUIRE (*(it->second) == longPosition3);
      */
    }

  //
  // New tests for getHighResBarReturns()
  //

  SECTION("getHighResBarReturns for single-bar trade")
    {
      ClosedPositionHistory<DecimalType> history;

      // 1) Build a single-bar long position
      TimeSeriesDate entryDate(2020, Jan, 1);
      DecimalType entryPrice = createDecimal("100.00");
      auto entryBar = createTimeSeriesEntry(entryDate,
					    entryPrice, entryPrice,
					    entryPrice, entryPrice,
					    1 /* volume */);

      // ctor seeds history with that one bar
      auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
								    myCornSymbol, entryPrice, *entryBar, oneContract);

      // close on the same bar (no addBar)
      pos->ClosePosition(entryDate, entryPrice);

      history.addClosedPosition(pos);

      // We now get the return for the first bar
      auto returns = history.getHighResBarReturns();
      REQUIRE(returns.empty() == false);
    }

  SECTION("getHighResBarReturns for two-bar trade")
    {
      ClosedPositionHistory<DecimalType> history;

      // 1) Entry bar
      TimeSeriesDate entryDate(2020, Jan, 1);
      DecimalType entryPrice = createDecimal("100.00");
      auto entryBar = createTimeSeriesEntry(entryDate,
					    entryPrice, entryPrice,
					    entryPrice, entryPrice,
					    1);

      auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
								    myCornSymbol, entryPrice, *entryBar, oneContract);

      // 2) Add a second bar
      TimeSeriesDate exitDate(2020, Jan, 2);
      DecimalType exitPrice = createDecimal("110.00");
      auto secondBar = createTimeSeriesEntry(exitDate,
					     exitPrice, exitPrice,
					     exitPrice, exitPrice,
					     1);
      pos->addBar(*secondBar);

      // 3) Close on that second bar
      pos->ClosePosition(exitDate, exitPrice);

      history.addClosedPosition(pos);

      // Now exactly one high-res return: (110–100)/100 = 0.10
      auto returns = history.getHighResBarReturns();
      REQUIRE(returns.size() == 2);
      REQUIRE(returns[1] == (exitPrice - entryPrice) / entryPrice);
    }

  // In ClosedPositionHistoryTest.cpp

  SECTION("getHighResBarReturns for eight-bar trade with varying prices")
    {
      ClosedPositionHistory<DecimalType> history;

      // 1) Prepare an array of eight close prices
      std::vector<DecimalType> prices = {
        createDecimal("100.00"), // Bar 1 Close
        createDecimal("102.00"), // Bar 2 Close
        createDecimal("101.00"), // Bar 3 Close
        createDecimal("105.00"),
        createDecimal("103.00"),
        createDecimal("108.00"),
        createDecimal("110.00"),
        createDecimal("115.00")  // Bar 8 Close
      };

      // 2) Seed the position with the first bar.
      // In this specific test, entry price = open = close for the first bar.
      TimeSeriesDate baseDate(2020, Jan, 1);
      DecimalType entryPrice = prices[0];
      auto firstBar = createTimeSeriesEntry(
					    baseDate,
					    entryPrice,                          // open
					    entryPrice + createDecimal("0.50"),  // high
					    entryPrice - createDecimal("0.50"),  // low
					    entryPrice,                          // close
					    100                                  // volume
					    );
      auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
								    myCornSymbol, entryPrice, *firstBar, oneContract
								    );

      // 3) Add the next seven bars
      for (size_t i = 1; i < prices.size(); ++i) {
        TimeSeriesDate d(2020, Jan, 1 + static_cast<int>(i));
        DecimalType open  = prices[i - 1]; // Open of current bar is close of previous
        DecimalType close = prices[i];
        DecimalType high  = std::max(open, close) + createDecimal("0.50");
        DecimalType low   = std::min(open, close) - createDecimal("0.50");

        auto bar = createTimeSeriesEntry(d, open, high, low, close, 100);
        pos->addBar(*bar);
      }

      // 4) Close on the last bar and record the position
      pos->ClosePosition(TimeSeriesDate(2020, Jan, 8), prices.back());
      history.addClosedPosition(pos);

      // 5) --- CORRECTED VERIFICATION LOGIC ---
      auto returns = history.getHighResBarReturns();
      REQUIRE(returns.size() == prices.size());

      // 5a) Verify the first return (entry bar's intra-bar return)
      // In this specific test, entryPrice (100) == close of first bar (100), so return is 0.
      DecimalType expectedFirstReturn = (firstBar->getCloseValue() - entryPrice) / entryPrice;
      REQUIRE(returns[0] == expectedFirstReturn);
      REQUIRE(returns[0] == createDecimal("0.0"));

      // 5b) Verify all subsequent close-to-close returns
      for (size_t i = 1; i < prices.size(); ++i) {
        // Return for bar i should be (close_i - close_{i-1}) / close_{i-1}
        DecimalType expected = (prices[i] - prices[i - 1]) / prices[i - 1];
        
        // Check against returns[i] because returns[0] holds the first special case
        REQUIRE(returns[i] == expected);
      }
    }
  
  SECTION("ClosedPositionHistory respects intraday entry datetime as key", "[ClosedPositionHistory][ptime]") {
    // 1) build a single‐bar intraday entry at 09:15
    auto e1 = createTimeSeriesEntry(
				    "20250526", "09:15:00",
				    "100.0","101.0","99.0","100.5","100"
				    );
    ptime entryDT = time_from_string("2025-05-26 09:15:00");
    TradingVolume oneShare(1, TradingVolume::SHARES);

    // 2) construct & close the position using the ptime overload
    auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
								  myCornSymbol,                     // symbol
								  e1->getOpenValue(),               // entry price
								  *e1,                              // entry bar
								  oneShare                         // volume
								  );
    ptime exitDT = time_from_string("2025-05-26 09:20:00");
    pos->ClosePosition(exitDT, createDecimal("101.00"));  // ptime overload

    // 3) add to history and verify the map key and stored timestamps
    ClosedPositionHistory<DecimalType> hist;
    hist.addClosedPosition(pos);

    auto it = hist.beginTradingPositions();
    REQUIRE(it != hist.endTradingPositions());
    REQUIRE(it->first == entryDT);                      // key is full ptime
    REQUIRE(it->second->getEntryDateTime() == entryDT); // stored position rounds-trip
    REQUIRE(it->second->getExitDateTime()  == exitDT);
  }

  SECTION("ClosedPositionHistory.getHighResBarReturns on intraday multi‐bar trade", "[ClosedPositionHistory][ptime]") {
    // 1) build 2-bar intraday position: entry@09:00, bar2@09:05, exit@09:10
    auto eA = createTimeSeriesEntry("20250526","09:00:00","100","102","99","101","100");
    auto eB = createTimeSeriesEntry("20250526","09:05:00","101","103","100","102","100");
    DecimalType exitPrice = createDecimal("102.50");
    ptime exitDT = time_from_string("2025-05-26 09:10:00");

    TradingVolume oneShare(1, TradingVolume::SHARES);
    auto pos2 = std::make_shared<TradingPositionLong<DecimalType>>(
								   myCornSymbol,               // symbol
								   eA->getOpenValue(),         // entry price
								   *eA,                        // entry bar
								   oneShare                   // volume
								   );
    pos2->addBar(*eB);            // second intraday bar
    pos2->ClosePosition(exitDT, exitPrice);

    // 2) add to history
    ClosedPositionHistory<DecimalType> hist2;
    hist2.addClosedPosition(pos2);

    auto returns = hist2.getHighResBarReturns();
    REQUIRE(returns.size() == 2);

    // 3) Verify returns
    // Return 1: from entry price to close of first bar
    DecimalType r0 = (eA->getCloseValue() - eA->getOpenValue()) / eA->getOpenValue();
    // Return 2: from close of first bar to actual exit price
    DecimalType r1 = (exitPrice - eA->getCloseValue()) / eA->getCloseValue();
    
    REQUIRE(returns[0] == r0);
    REQUIRE(returns[1] == r1);
  }

  // *** NEW TESTS FOR SHORT POSITIONS ***
  SECTION("getHighResBarReturns for Short Positions")
  {
      TradingVolume oneShare(1, TradingVolume::SHARES);

      // Test Case 1: A winning short trade (price goes down)
      ClosedPositionHistory<DecimalType> winning_history;
      DecimalType entryPriceWin = createDecimal("100.00");
      auto entryBarWin = createTimeSeriesEntry(date(2023, 1, 1), entryPriceWin, entryPriceWin, entryPriceWin, createDecimal("100.00"), 100);
      auto midBarWin = createTimeSeriesEntry(date(2023, 1, 2), createDecimal("100.00"), createDecimal("100.00"), createDecimal("97.00"), createDecimal("98.00"), 100);
      DecimalType exitPriceWin = createDecimal("95.00");


      auto winningShortPos = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, entryPriceWin, *entryBarWin, oneShare);
      winningShortPos->addBar(*midBarWin);
      winningShortPos->ClosePosition(date(2023, 1, 2), exitPriceWin); // Exit on the second bar
      winning_history.addClosedPosition(winningShortPos);

      auto winning_returns = winning_history.getHighResBarReturns();
      REQUIRE(winning_returns.size() == 2);

      // Return 1: (close of entry bar - entry price) / entry price, negated.
      // (100 - 100) / 100 = 0. Negated is still 0.
      DecimalType expectedReturn1_win = (entryBarWin->getCloseValue() - entryPriceWin) / entryPriceWin;
      REQUIRE(winning_returns[0] == (expectedReturn1_win * -1));

      // Return 2: (exit price - close of previous bar) / close of previous bar, negated.
      // (95 - 100) / 100 = -0.05. Negated is +0.05.
      DecimalType expectedReturn2_win = (exitPriceWin - entryBarWin->getCloseValue()) / entryBarWin->getCloseValue();
      REQUIRE(winning_returns[1] == (expectedReturn2_win * -1));


      // Test Case 2: A losing short trade (price goes up)
      ClosedPositionHistory<DecimalType> losing_history;
      DecimalType entryPriceLose = createDecimal("100.00");
      auto entryBarLose = createTimeSeriesEntry(date(2023, 1, 5), entryPriceLose, entryPriceLose, entryPriceLose, createDecimal("100.00"), 100);
      auto midBarLose = createTimeSeriesEntry(date(2023, 1, 6), createDecimal("100.00"), createDecimal("103.00"), createDecimal("99.00"), createDecimal("102.00"), 100);
      DecimalType exitPriceLose = createDecimal("105.00");

      auto losingShortPos = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, entryPriceLose, *entryBarLose, oneShare);
      losingShortPos->addBar(*midBarLose);
      losingShortPos->ClosePosition(date(2023, 1, 6), exitPriceLose); // Exit on the second bar
      losing_history.addClosedPosition(losingShortPos);

      auto losing_returns = losing_history.getHighResBarReturns();
      REQUIRE(losing_returns.size() == 2);

      // Return 1: (close of entry bar - entry price) / entry price, negated.
      // (100 - 100) / 100 = 0. Negated is 0.
      DecimalType expectedReturn1_lose = (entryBarLose->getCloseValue() - entryPriceLose) / entryPriceLose;
      REQUIRE(losing_returns[0] == (expectedReturn1_lose * -1));

      // Return 2: (exit price - close of previous bar) / close of previous bar, negated.
      // (105 - 100) / 100 = +0.05. Negated is -0.05.
      DecimalType expectedReturn2_lose = (exitPriceLose - entryBarLose->getCloseValue()) / entryBarLose->getCloseValue();
      REQUIRE(losing_returns[1] == (expectedReturn2_lose * -1));
  }
}
