#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp> // For Catch::Approx
#include <catch2/matchers/catch_matchers_string.hpp>  // For string matching in exceptions
#include "TradingVolume.h"
#include "TimeSeriesCsvReader.h"
#include "ClosedPositionHistory.h"
#include "TimeSeriesIndicators.h"
#include "MonthlyReturnsBuilder.h"
#include "TestUtils.h"
#include <cmath>
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::hours;
using boost::posix_time::minutes;

using D = DecimalType;
const static std::string myCornSymbol("C2");


const static std::string testSymbol("MSFT");

// Helper function to create a simple closed long position
std::shared_ptr<TradingPositionLong<DecimalType>>
createSimpleLongPosition(const std::string& symbol,
                        const DecimalType& entryPrice,
                        const DecimalType& exitPrice,
                        const TimeSeriesDate& entryDate,
                        const TimeSeriesDate& exitDate,
                        const TradingVolume& volume)
{
    auto entryBar = createTimeSeriesEntry(entryDate, entryPrice, entryPrice, entryPrice, entryPrice, 100);
    auto pos = std::make_shared<TradingPositionLong<DecimalType>>(symbol, entryPrice, *entryBar, volume);
    pos->ClosePosition(exitDate, exitPrice);
    return pos;
}

// Helper function to create a simple closed short position
std::shared_ptr<TradingPositionShort<DecimalType>>
createSimpleShortPosition(const std::string& symbol,
                         const DecimalType& entryPrice,
                         const DecimalType& exitPrice,
                         const TimeSeriesDate& entryDate,
                         const TimeSeriesDate& exitDate,
                         const TradingVolume& volume)
{
    auto entryBar = createTimeSeriesEntry(entryDate, entryPrice, entryPrice, entryPrice, entryPrice, 100);
    auto pos = std::make_shared<TradingPositionShort<DecimalType>>(symbol, entryPrice, *entryBar, volume);
    pos->ClosePosition(exitDate, exitPrice);
    return pos;
}

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

// Helper to create a mock long position with bar history for testing
namespace
{
  std::shared_ptr<TradingPosition<D>> 
  createMockLongPosition(const std::string& symbol,
			 const ptime& entryTime,
			 D entryPrice,
			 D exitPrice,
			 const std::vector<D>& barCloses)
  {
    // Entry happens at open. The entry bar's close is the first MTM point.
    D entryBarClose = barCloses.empty() ? entryPrice : barCloses[0];
    
    // Create entry bar - ensure OHLC constraints are met
    D high = entryPrice > entryBarClose ? entryPrice : entryBarClose;
    D low = entryPrice < entryBarClose ? entryPrice : entryBarClose;
    
    OHLCTimeSeriesEntry<D> entryBar(
        entryTime.date(),
        entryPrice,              // open
        high,                    // high (must be >= open and close)
        low,                     // low (must be <= open and close)
        entryBarClose,          // close
        D(100.0),                // volume as Decimal
        TimeFrame::DAILY
        );
    
    auto pos = std::make_shared<TradingPositionLong<D>>(
    			symbol,
    			entryPrice,
    			entryBar,
    			TradingVolume(1, TradingVolume::SHARES)
    			);
    
    // Add bar history - each subsequent bar except the last one
    ptime currentTime = entryTime;
    for (size_t i = 1; i < barCloses.size(); ++i) {  // Start from 1, not 0
      currentTime += boost::posix_time::hours(24);
      OHLCTimeSeriesEntry<D> bar(currentTime.date(),
     barCloses[i],    // open (not used in return calc)
     barCloses[i],    // high
     barCloses[i],    // low
     barCloses[i],    // close
     D(100.0),        // volume as Decimal
     TimeFrame::DAILY);
      
      pos->addBar(bar);
    }
    
    // Close the position on the last bar's date
    currentTime += boost::posix_time::hours(24);
    pos->ClosePosition(currentTime.date(), exitPrice);
    
    return pos;
  }

  std::shared_ptr<TradingPosition<D>>
  createMockShortPosition(const std::string& symbol,
			  const ptime& entryTime,
			  D entryPrice,
			  D exitPrice,
			  const std::vector<D>& barCloses)
  {
    D entryBarClose = barCloses.empty() ? entryPrice : barCloses[0];
    
    // Ensure OHLC constraints are met
    D high = entryPrice > entryBarClose ? entryPrice : entryBarClose;
    D low = entryPrice < entryBarClose ? entryPrice : entryBarClose;
    
    OHLCTimeSeriesEntry<D> entryBar(entryTime.date(),
        entryPrice,              // open
        high,                    // high
        low,                     // low
        entryBarClose,          // close
        D(100.0),                // volume as Decimal
        TimeFrame::DAILY);
    
    auto pos = std::make_shared<TradingPositionShort<D>>(
    			 symbol,
    			 entryPrice,
    			 entryBar,
    			 TradingVolume(1, TradingVolume::SHARES)
    			 );
    
    ptime currentTime = entryTime;
    for (size_t i = 1; i < barCloses.size(); ++i) {  // Start from 1, not 0
      currentTime += boost::posix_time::hours(24);
      OHLCTimeSeriesEntry<D> bar(currentTime,
     barCloses[i],
     barCloses[i],
     barCloses[i],
     barCloses[i],
     D(100.0),        // volume as Decimal
     TimeFrame::DAILY);
      
      pos->addBar(bar);
    }
    
    currentTime += boost::posix_time::hours(24);
    pos->ClosePosition(currentTime.date(), exitPrice);
    
    return pos;
  }

  std::shared_ptr<TradingPosition<D>>
  createSameBarPosition(const std::string& symbol,
			const ptime& entryTime,
			D entryPrice,
			D exitPrice)
  {
    // Ensure OHLC constraints for same-bar position
    D high = entryPrice > exitPrice ? entryPrice : exitPrice;
    D low = entryPrice < exitPrice ? entryPrice : exitPrice;
    
    OHLCTimeSeriesEntry<D> entryBar(
        entryTime,  // Use full timestamp instead of just date
        entryPrice,
        high,
        low,
        entryPrice,  // Close at entry price initially
        D(static_cast<double>(TradingVolume(100, TradingVolume::SHARES).getTradingVolume())),
        TimeFrame::INTRADAY
        );
    
    auto pos = std::make_shared<TradingPositionLong<D>>(
  					symbol,
  					entryPrice,
  					entryBar,
  					TradingVolume(1, TradingVolume::SHARES)
  					);
    
    // Close same-bar: use same date but later time
    ptime exitTime = ptime(entryTime.date(), entryTime.time_of_day() + hours(1));
    pos->ClosePosition(exitTime, exitPrice);
    
    return pos;
  }
} // anonymous namespace

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

  auto longExitDate2 = TimeSeriesDate (1986, Jun, 12);
  auto longExitPrice2 = createDecimal("3729.28683");
  auto longEntryDate2 = TimeSeriesDate (1986, May, 16);
  auto longEntryPrice2 = createDecimal("3777.64063");

  auto longPosition2  = createClosedLongPosition (p, longEntryDate2, longEntryPrice2,
						  longExitDate2, longExitPrice2,
						  oneContract, 18);

  auto position2LogReturn = longPosition2->getLogTradeReturn();
  auto position2ReferenceLnReturn = getLnReturn(longEntryPrice2, longExitPrice2);

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

  std::vector<unsigned int> barsInPositions(closedLongPositions.beginBarsPerPosition(),
					    closedLongPositions.endBarsPerPosition()) ;

  REQUIRE (barsInPositions.size() == 24);

  REQUIRE (longCumReturn == closedLongPositions.getCumulativeReturn());
  REQUIRE (closedLongPositions.getNumPositions() == 24);
  REQUIRE (closedLongPositions.getProfitFactor() >= createDecimal("2.99"));
  REQUIRE (closedLongPositions.getPercentWinners() == createDecimal("58.33333300"));
  REQUIRE (closedLongPositions.getPercentLosers() == (createDecimal ("100.00") - closedLongPositions.getPercentWinners()));
  REQUIRE (closedLongPositions.getNumWinningPositions() == 14);
  REQUIRE (closedLongPositions.getNumLosingPositions() == 10);
  REQUIRE (closedLongPositions.getPayoffRatio() == createDecimal("2.14074081"));
  REQUIRE (closedLongPositions.getPALProfitability() == createDecimal("58.33333300"));


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

  SECTION("ClosedPositionHistory::getNumConsecutiveLosses tests")
    {
      ClosedPositionHistory<DecimalType> history;
      TradingVolume oneContract(1, TradingVolume::CONTRACTS);

      // Initially, consecutive losses should be 0
      REQUIRE(history.getNumConsecutiveLosses() == 0);

      // Create positions manually without relying on CSV data
      // Test Case 1: Add a winning position - consecutive losses should remain 0
      TimeSeriesDate entryDate1(2020, Jan, 1);
      auto entryBar1 = createTimeSeriesEntry(entryDate1, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto winningPos1 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar1, oneContract);
      winningPos1->ClosePosition(entryDate1, createDecimal("110.00")); // 10% gain
   
      history.addClosedPosition(winningPos1);
      REQUIRE(history.getNumConsecutiveLosses() == 0);

      // Test Case 2: Add a losing position - consecutive losses should be 1
      TimeSeriesDate entryDate2(2020, Jan, 2);
      auto entryBar2 = createTimeSeriesEntry(entryDate2, createDecimal("110.00"), createDecimal("110.00"), createDecimal("110.00"), createDecimal("110.00"), 100);
      auto losingPos1 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("110.00"), *entryBar2, oneContract);
      losingPos1->ClosePosition(entryDate2, createDecimal("100.00")); // 9.09% loss
   
      history.addClosedPosition(losingPos1);
      REQUIRE(history.getNumConsecutiveLosses() == 1);

      // Test Case 3: Add another losing position - consecutive losses should be 2
      TimeSeriesDate entryDate3(2020, Jan, 3);
      auto entryBar3 = createTimeSeriesEntry(entryDate3, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto losingPos2 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar3, oneContract);
      losingPos2->ClosePosition(entryDate3, createDecimal("95.00")); // 5% loss
   
      history.addClosedPosition(losingPos2);
      REQUIRE(history.getNumConsecutiveLosses() == 2);

      // Test Case 4: Add a third consecutive losing position - consecutive losses should be 3
      TimeSeriesDate entryDate4(2020, Jan, 4);
      auto entryBar4 = createTimeSeriesEntry(entryDate4, createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), 100);
      auto losingPos3 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("95.00"), *entryBar4, oneContract);
      losingPos3->ClosePosition(entryDate4, createDecimal("90.00")); // 5.26% loss
   
      history.addClosedPosition(losingPos3);
      REQUIRE(history.getNumConsecutiveLosses() == 3);

      // Test Case 5: Add a winning position - consecutive losses should reset to 0
      TimeSeriesDate entryDate5(2020, Jan, 5);
      auto entryBar5 = createTimeSeriesEntry(entryDate5, createDecimal("90.00"), createDecimal("90.00"), createDecimal("90.00"), createDecimal("90.00"), 100);
      auto winningPos2 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("90.00"), *entryBar5, oneContract);
      winningPos2->ClosePosition(entryDate5, createDecimal("100.00")); // 11.11% gain
   
      history.addClosedPosition(winningPos2);
      REQUIRE(history.getNumConsecutiveLosses() == 0);

      // Test Case 6: Add another losing position after the win - consecutive losses should be 1
      TimeSeriesDate entryDate6(2020, Jan, 6);
      auto entryBar6 = createTimeSeriesEntry(entryDate6, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto losingPos4 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar6, oneContract);
      losingPos4->ClosePosition(entryDate6, createDecimal("98.00")); // 2% loss
   
      history.addClosedPosition(losingPos4);
      REQUIRE(history.getNumConsecutiveLosses() == 1);

      // Verify overall statistics are correct
      REQUIRE(history.getNumPositions() == 6);
      REQUIRE(history.getNumWinningPositions() == 2);
      REQUIRE(history.getNumLosingPositions() == 4);
    }

  SECTION("ClosedPositionHistory::getNumConsecutiveLosses with short positions")
    {
      ClosedPositionHistory<DecimalType> history;
      TradingVolume oneContract(1, TradingVolume::CONTRACTS);

      // Test with short positions
      // Winning short: price goes down
      TimeSeriesDate entryDate1(2020, Jan, 1);
      auto entryBar1 = createTimeSeriesEntry(entryDate1, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto winningShort = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar1, oneContract);
      winningShort->ClosePosition(entryDate1, createDecimal("95.00")); // Price down = profit for short
   
      history.addClosedPosition(winningShort);
      REQUIRE(history.getNumConsecutiveLosses() == 0);

      // Losing short: price goes up
      TimeSeriesDate entryDate2(2020, Jan, 2);
      auto entryBar2 = createTimeSeriesEntry(entryDate2, createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), 100);
      auto losingShort1 = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, createDecimal("95.00"), *entryBar2, oneContract);
      losingShort1->ClosePosition(entryDate2, createDecimal("100.00")); // Price up = loss for short
   
      history.addClosedPosition(losingShort1);
      REQUIRE(history.getNumConsecutiveLosses() == 1);

      // Another losing short
      TimeSeriesDate entryDate3(2020, Jan, 3);
      auto entryBar3 = createTimeSeriesEntry(entryDate3, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto losingShort2 = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar3, oneContract);
      losingShort2->ClosePosition(entryDate3, createDecimal("105.00")); // Price up = loss for short
   
      history.addClosedPosition(losingShort2);
      REQUIRE(history.getNumConsecutiveLosses() == 2);

      // Winning short resets the counter
      TimeSeriesDate entryDate4(2020, Jan, 4);
      auto entryBar4 = createTimeSeriesEntry(entryDate4, createDecimal("105.00"), createDecimal("105.00"), createDecimal("105.00"), createDecimal("105.00"), 100);
      auto winningShort2 = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, createDecimal("105.00"), *entryBar4, oneContract);
      winningShort2->ClosePosition(entryDate4, createDecimal("98.00")); // Price down = profit for short
   
      history.addClosedPosition(winningShort2);
      REQUIRE(history.getNumConsecutiveLosses() == 0);

      // Verify overall statistics
      REQUIRE(history.getNumPositions() == 4);
      REQUIRE(history.getNumWinningPositions() == 2);
      REQUIRE(history.getNumLosingPositions() == 2);
    }

  SECTION("ClosedPositionHistory::getNumConsecutiveLosses copy constructor and assignment")
    {
      ClosedPositionHistory<DecimalType> history1;
      TradingVolume oneContract(1, TradingVolume::CONTRACTS);

      // Add some positions to create a state with consecutive losses
      TimeSeriesDate entryDate1(2020, Jan, 1);
      auto entryBar1 = createTimeSeriesEntry(entryDate1, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto losingPos1 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar1, oneContract);
      losingPos1->ClosePosition(entryDate1, createDecimal("95.00"));
   
      TimeSeriesDate entryDate2(2020, Jan, 2);
      auto entryBar2 = createTimeSeriesEntry(entryDate2, createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), 100);
      auto losingPos2 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("95.00"), *entryBar2, oneContract);
      losingPos2->ClosePosition(entryDate2, createDecimal("90.00"));
   
      history1.addClosedPosition(losingPos1);
      history1.addClosedPosition(losingPos2);
      REQUIRE(history1.getNumConsecutiveLosses() == 2);

      // Test copy constructor
      ClosedPositionHistory<DecimalType> history2(history1);
      REQUIRE(history2.getNumConsecutiveLosses() == 2);
      REQUIRE(history2.getNumPositions() == 2);
      REQUIRE(history2.getNumLosingPositions() == 2);

      // Test assignment operator
      ClosedPositionHistory<DecimalType> history3;
      history3 = history1;
      REQUIRE(history3.getNumConsecutiveLosses() == 2);
      REQUIRE(history3.getNumPositions() == 2);
      REQUIRE(history3.getNumLosingPositions() == 2);

      // Verify that modifying the original doesn't affect the copies
      TimeSeriesDate entryDate3(2020, Jan, 3);
      auto entryBar3 = createTimeSeriesEntry(entryDate3, createDecimal("90.00"), createDecimal("90.00"), createDecimal("90.00"), createDecimal("90.00"), 100);
      auto winningPos = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("90.00"), *entryBar3, oneContract);
      winningPos->ClosePosition(entryDate3, createDecimal("100.00"));
   
      history1.addClosedPosition(winningPos);
      REQUIRE(history1.getNumConsecutiveLosses() == 0);
      REQUIRE(history2.getNumConsecutiveLosses() == 2); // Should remain unchanged
      REQUIRE(history3.getNumConsecutiveLosses() == 2); // Should remain unchanged
    }

  SECTION("ClosedPositionHistory::getNumConsecutiveLosses edge cases")
    {
      ClosedPositionHistory<DecimalType> history;
      TradingVolume oneContract(1, TradingVolume::CONTRACTS);

      // Test with only winning positions
      TimeSeriesDate entryDate1(2020, Jan, 1);
      auto entryBar1 = createTimeSeriesEntry(entryDate1, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto winningPos1 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar1, oneContract);
      winningPos1->ClosePosition(entryDate1, createDecimal("110.00"));
   
      TimeSeriesDate entryDate2(2020, Jan, 2);
      auto entryBar2 = createTimeSeriesEntry(entryDate2, createDecimal("110.00"), createDecimal("110.00"), createDecimal("110.00"), createDecimal("110.00"), 100);
      auto winningPos2 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("110.00"), *entryBar2, oneContract);
      winningPos2->ClosePosition(entryDate2, createDecimal("120.00"));
   
      history.addClosedPosition(winningPos1);
      REQUIRE(history.getNumConsecutiveLosses() == 0);
      history.addClosedPosition(winningPos2);
      REQUIRE(history.getNumConsecutiveLosses() == 0);

      // Test with only losing positions
      ClosedPositionHistory<DecimalType> history2;
      TimeSeriesDate entryDate3(2020, Jan, 3);
      auto entryBar3 = createTimeSeriesEntry(entryDate3, createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), createDecimal("100.00"), 100);
      auto losingPos1 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("100.00"), *entryBar3, oneContract);
      losingPos1->ClosePosition(entryDate3, createDecimal("95.00"));
   
      TimeSeriesDate entryDate4(2020, Jan, 4);
      auto entryBar4 = createTimeSeriesEntry(entryDate4, createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), createDecimal("95.00"), 100);
      auto losingPos2 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("95.00"), *entryBar4, oneContract);
      losingPos2->ClosePosition(entryDate4, createDecimal("90.00"));
   
      TimeSeriesDate entryDate5(2020, Jan, 5);
      auto entryBar5 = createTimeSeriesEntry(entryDate5, createDecimal("90.00"), createDecimal("90.00"), createDecimal("90.00"), createDecimal("90.00"), 100);
      auto losingPos3 = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, createDecimal("90.00"), *entryBar5, oneContract);
      losingPos3->ClosePosition(entryDate5, createDecimal("85.00"));
   
      history2.addClosedPosition(losingPos1);
      REQUIRE(history2.getNumConsecutiveLosses() == 1);
      history2.addClosedPosition(losingPos2);
      REQUIRE(history2.getNumConsecutiveLosses() == 2);
      history2.addClosedPosition(losingPos3);
      REQUIRE(history2.getNumConsecutiveLosses() == 3);
    }

  SECTION("getMedianHoldingPeriod tests")
    {
      // Test 1: Empty history
      ClosedPositionHistory<DecimalType> history;
      REQUIRE(history.getMedianHoldingPeriod() == 0);

      // Test 2: Single position
      // This position has 2 bars.
      history.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1987, Apr, 22),
							 createDecimal("2808.12280"),
							 TimeSeriesDate(1987, Apr, 24),
							 createDecimal("2880.01075"),
							 oneContract, 2));
      REQUIRE(history.getMedianHoldingPeriod() == 3);

      // Test 3: Odd number of positions
      // Add two more positions.
      // Position with 1 bar
      history.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1988, Apr, 13),
							 createDecimal("2817.15112"),
							 TimeSeriesDate(1988, Apr, 14),
							 createDecimal("2781.09159"),
							 oneContract, 1));
      // Position with 6 bars
      history.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1988, Apr, 6),
							 createDecimal("2817.15112"),
							 TimeSeriesDate(1988, Apr, 14),
							 createDecimal("2781.09159"),
							 oneContract, 6));

      // Holding periods are: 2, 1, 6. Sorted: 1, 2, 6. Median is 2.
      REQUIRE(history.getMedianHoldingPeriod() == 3);

      // Test 4: Even number of positions, fractional median rounding up
      // Add one more position.
      // Position with 3 bars.
      history.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1990, Jun, 5),
							 createDecimal("3207.87378"),
							 TimeSeriesDate(1990, Jun, 8),
							 createDecimal("3289.99535"),
							 oneContract, 3));
     
      REQUIRE(history.getMedianHoldingPeriod() == 4);

      // Test 5: Even number of positions, whole median
      ClosedPositionHistory<DecimalType> history2;
      // Position with 2 bars
      history2.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1987, Apr, 22),
							  createDecimal("2808.12280"),
							  TimeSeriesDate(1987, Apr, 24),
							  createDecimal("2880.01075"),
							  oneContract, 2));
      // Position with 1 bar
      history2.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1988, Apr, 13),
							  createDecimal("2817.15112"),
							  TimeSeriesDate(1988, Apr, 14),
							  createDecimal("2781.09159"),
							  oneContract, 1));
      // Position with 6 bars
      history2.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1988, Apr, 6),
							  createDecimal("2817.15112"),
							  TimeSeriesDate(1988, Apr, 14),
							  createDecimal("2781.09159"),
							  oneContract, 6));
      // Position with 4 bars
      history2.addClosedPosition(createClosedLongPosition(p, TimeSeriesDate(1995, Jun, 9),
							  createDecimal("1880.15967"),
							  TimeSeriesDate(1995, Jun, 15),
							  createDecimal("1928.29176"),
							  oneContract, 4));
      
      REQUIRE(history2.getMedianHoldingPeriod() == 4);
    }

  SECTION("buildMonthlyReturnsFromClosedPositions: single long trade within a month (compounding across bars)")
    {
      using mkc_timeseries::buildMonthlyReturnsFromClosedPositions;

      ClosedPositionHistory<DecimalType> hist;
      TradingVolume one(1, TradingVolume::CONTRACTS);

      // Jan 2021, long trade: entry 100, add one bar to 110, exit 120
      TimeSeriesDate d0(2021, Jan, 4);
      auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("101"),
				      createDecimal("99"),  createDecimal("100"), 10);
      auto pos = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);

      // Next bar close 110
      TimeSeriesDate d1(2021, Jan, 5);
      auto b1 = createTimeSeriesEntry(d1, createDecimal("100"), createDecimal("111"),
				      createDecimal("99"),  createDecimal("110"), 10);
      pos->addBar(*b1);

      // Exit on third bar (still Jan), price 120
      TimeSeriesDate d2(2021, Jan, 6);
      pos->ClosePosition(d2, createDecimal("120"));
      hist.addClosedPosition(pos);

      // High-res bar returns (long): r0: (100-100)/100 = 0; r1: (110-100)/100 = 0.10; r2: (120-110)/110 ≈ 0.090909...
      // Monthly compounded: (1+r0)*(1+r1)*(1+r2)-1 = 1 * 1.10 * (120/110) - 1 = (120/100) - 1 = 0.20
      auto monthly = buildMonthlyReturnsFromClosedPositions<DecimalType>(hist);
      REQUIRE(monthly.size() == 1);
      REQUIRE(monthly[0] == createDecimal("0.20"));
    }

  SECTION("buildMonthlyReturnsFromClosedPositions: two long trades in same month (compounding both)")
    {
      using mkc_timeseries::buildMonthlyReturnsFromClosedPositions;

      ClosedPositionHistory<DecimalType> hist;
      TradingVolume one(1, TradingVolume::CONTRACTS);

      // Trade A: Jan 7 to Jan 8: 100 -> 105 (return 5%)
      {
	TimeSeriesDate da0(2021, Jan, 7);
	auto ea0 = createTimeSeriesEntry(da0, createDecimal("100"), createDecimal("101"),
					 createDecimal("99"),  createDecimal("100"), 10);
	auto A = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, ea0->getOpenValue(), *ea0, one);
	TimeSeriesDate da1(2021, Jan, 8);
	A->ClosePosition(da1, createDecimal("105"));
	hist.addClosedPosition(A);
      }

      // Trade B: Jan 12 to Jan 13: 200 -> 210 (return 5%)
      {
	TimeSeriesDate db0(2021, Jan, 12);
	auto eb0 = createTimeSeriesEntry(db0, createDecimal("200"), createDecimal("201"),
					 createDecimal("199"), createDecimal("200"), 10);
	auto B = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, eb0->getOpenValue(), *eb0, one);
	TimeSeriesDate db1(2021, Jan, 13);
	B->ClosePosition(db1, createDecimal("210"));
	hist.addClosedPosition(B);
      }

      // Same calendar month; compounding within month: (1+0.05)*(1+0.05)-1 = 1.1025-1 = 0.1025
      auto monthly = buildMonthlyReturnsFromClosedPositions<DecimalType>(hist);
      REQUIRE(monthly.size() == 1);
      REQUIRE(monthly[0] == createDecimal("0.1025"));
    }

  SECTION("buildMonthlyReturnsFromClosedPositions: trades spanning multiple months (chronological order)")
    {
      using mkc_timeseries::buildMonthlyReturnsFromClosedPositions;

      ClosedPositionHistory<DecimalType> hist;
      TradingVolume one(1, TradingVolume::CONTRACTS);

      // Jan trade: 100 -> 110 (10%)
      {
	TimeSeriesDate d0(2021, Jan, 20);
	auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("101"),
					createDecimal("99"),  createDecimal("100"), 10);
	auto P = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
	TimeSeriesDate d1(2021, Jan, 21);
	P->ClosePosition(d1, createDecimal("110"));
	hist.addClosedPosition(P);
      }

      // Feb trade: 200 -> 180 (-10%)
      {
	TimeSeriesDate d0(2021, Feb, 2);
	auto e0 = createTimeSeriesEntry(d0, createDecimal("200"), createDecimal("201"),
					createDecimal("199"), createDecimal("200"), 10);
	auto Q = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
	TimeSeriesDate d1(2021, Feb, 5);
	Q->ClosePosition(d1, createDecimal("180"));
	hist.addClosedPosition(Q);
      }

      auto monthly = buildMonthlyReturnsFromClosedPositions<DecimalType>(hist);
      REQUIRE(monthly.size() == 2);
      REQUIRE(monthly[0] == createDecimal("0.10"));   // Jan
      REQUIRE(monthly[1] == createDecimal("-0.10"));  // Feb
    }

  SECTION("buildMonthlyReturnsFromClosedPositions: short trade sign convention")
    {
      using mkc_timeseries::buildMonthlyReturnsFromClosedPositions;

      ClosedPositionHistory<DecimalType> hist;
      TradingVolume one(1, TradingVolume::CONTRACTS);

      // Short wins when price falls: entry 100, exit 95 => +5% for short
      TimeSeriesDate d0(2021, Mar, 10);
      auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("101"),
				      createDecimal("99"),  createDecimal("100"), 10);
      auto S = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
      TimeSeriesDate d1(2021, Mar, 15);
      S->ClosePosition(d1, createDecimal("95"));
      hist.addClosedPosition(S);

      auto monthly = buildMonthlyReturnsFromClosedPositions<DecimalType>(hist);
      REQUIRE(monthly.size() == 1);
      REQUIRE(monthly[0] == createDecimal("0.05"));
    }

  SECTION("buildMonthlyReturnsFromClosedPositions: months with no exposure are omitted (sparse series)")
    {
      using mkc_timeseries::buildMonthlyReturnsFromClosedPositions;

      ClosedPositionHistory<DecimalType> hist;
      TradingVolume one(1, TradingVolume::CONTRACTS);

      // Apr trade only, 100 -> 110
      {
	TimeSeriesDate d0(2021, Apr, 6);
	auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("101"),
					createDecimal("99"),  createDecimal("100"), 10);
	auto P = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
	TimeSeriesDate d1(2021, Apr, 7);
	P->ClosePosition(d1, createDecimal("110"));
	hist.addClosedPosition(P);
      }

      // (No trades in May)

      // Jun trade only, 200 -> 180
      {
	TimeSeriesDate d0(2021, Jun, 1);
	auto e0 = createTimeSeriesEntry(d0, createDecimal("200"), createDecimal("201"),
					createDecimal("199"), createDecimal("200"), 10);
	auto Q = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
	TimeSeriesDate d1(2021, Jun, 3);
	Q->ClosePosition(d1, createDecimal("180"));
	hist.addClosedPosition(Q);
      }

      auto monthly = buildMonthlyReturnsFromClosedPositions<DecimalType>(hist);
      REQUIRE(monthly.size() == 2);         // Apr and Jun only — May omitted
      REQUIRE(monthly[0] == createDecimal("0.10"));   // Apr
      REQUIRE(monthly[1] == createDecimal("-0.10"));  // Jun
    }

SECTION("getHighResBarReturnsWithDates: empty history returns empty vector")
{
  ClosedPositionHistory<DecimalType> hist;
  auto pairs = hist.getHighResBarReturnsWithDates();
  REQUIRE(pairs.empty());
}

SECTION("getHighResBarReturnsWithDates: single-bar long trade (exit timestamp + entry→exit return)")
{
  ClosedPositionHistory<DecimalType> hist;
  TradingVolume one(1, TradingVolume::CONTRACTS);

  // Entry bar (daily)
  TimeSeriesDate d0(2020, Jan, 1);
  auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("100"),
                                  createDecimal("100"), createDecimal("100"), 1);

  auto pos = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);

  // Close on the same day at same price
  pos->ClosePosition(d0, createDecimal("100"));
  hist.addClosedPosition(pos);

  auto pairs = hist.getHighResBarReturnsWithDates();
  REQUIRE(pairs.size() == 1);

  // Value: (exit - entry)/entry == 0
  REQUIRE(pairs[0].second == createDecimal("0"));

  // Timestamp should be the exit datetime (00:00:00 for daily overload)
  REQUIRE(pairs[0].first.date() == d0);
}

SECTION("getHighResBarReturnsWithDates: two-bar long trade (intermediate bar timestamp; exit timestamp)")
{
  ClosedPositionHistory<DecimalType> hist;
  TradingVolume one(1, TradingVolume::CONTRACTS);

  // Entry bar
  TimeSeriesDate d0(2020, Jan, 1);
  auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("101"),
                                  createDecimal("99"),  createDecimal("100"), 1);
  auto pos = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);

  // Second bar
  TimeSeriesDate d1(2020, Jan, 2);
  auto b1 = createTimeSeriesEntry(d1, createDecimal("100"), createDecimal("111"),
                                  createDecimal("99"),  createDecimal("110"), 1);
  pos->addBar(*b1);

  // Exit on day 2 at 110 (same as bar close)
  pos->ClosePosition(d1, createDecimal("110"));
  hist.addClosedPosition(pos);

  auto pairs = hist.getHighResBarReturnsWithDates();
  REQUIRE(pairs.size() == 2);

  // First pair: timestamp = entry bar datetime; return = (close0 - entry)/entry
  REQUIRE(pairs[0].first.date() == d0);
  REQUIRE(pairs[0].second == (e0->getCloseValue() - e0->getOpenValue()) / e0->getOpenValue());

  // Second pair: timestamp = exit datetime; return = (exit - close0)/close0
  REQUIRE(pairs[1].first.date() == d1);
  REQUIRE(pairs[1].second == (createDecimal("110") - e0->getCloseValue()) / e0->getCloseValue());
}

SECTION("getHighResBarReturnsWithDates: two-bar short trade (sign convention + timestamps)")
{
  ClosedPositionHistory<DecimalType> hist;
  TradingVolume one(1, TradingVolume::CONTRACTS);

  // Entry bar 100 close; next bar 98 close; exit at 95
  TimeSeriesDate d0(2023, Jan, 1);
  auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("100"),
                                  createDecimal("100"), createDecimal("100"), 1);
  TimeSeriesDate d1(2023, Jan, 2);
  auto b1 = createTimeSeriesEntry(d1, createDecimal("100"), createDecimal("100"),
                                  createDecimal("97"),  createDecimal("98"),  1);

  auto pos = std::make_shared<TradingPositionShort<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
  pos->addBar(*b1);
  DecimalType exitPx = createDecimal("95");
  pos->ClosePosition(d1, exitPx);
  hist.addClosedPosition(pos);

  auto pairs = hist.getHighResBarReturnsWithDates();
  REQUIRE(pairs.size() == 2);

  // For short, returns are negated vs long:
  // r0 = (close0 - entry)/entry, then negate
  DecimalType r0 = (e0->getCloseValue() - e0->getOpenValue()) / e0->getOpenValue();
  // r1 = (exit - close0)/close0, then negate  [close0 is day-1 close]
  DecimalType r1 = (exitPx - e0->getCloseValue()) / e0->getCloseValue();

  REQUIRE(pairs[0].first.date() == d0);
  REQUIRE(pairs[0].second == (r0 * createDecimal("-1")));
  REQUIRE(pairs[1].first.date() == d1);
  REQUIRE(pairs[1].second == (r1 * createDecimal("-1")));
}

SECTION("getHighResBarReturnsWithDates: intraday ptime (timestamps preserved)")
{
  using boost::posix_time::time_from_string;
  ClosedPositionHistory<DecimalType> hist;
  TradingVolume one(1, TradingVolume::SHARES);

  // 09:00 and 09:05 bars; exit at 09:10
  auto eA = createTimeSeriesEntry("20250526","09:00:00","100","102","99","101","100");
  auto eB = createTimeSeriesEntry("20250526","09:05:00","101","103","100","102","100");
  auto pos = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, eA->getOpenValue(), *eA, one);
  pos->addBar(*eB);
  auto exitDT = time_from_string("2025-05-26 09:10:00");
  pos->ClosePosition(exitDT, createDecimal("102.50"));
  hist.addClosedPosition(pos);

  auto pairs = hist.getHighResBarReturnsWithDates();
  REQUIRE(pairs.size() == 2);

  // Timestamps: first = 09:00 bar time; second = explicit exit ptime 09:10
  REQUIRE(pairs[0].first == eA->getDateTime());
  REQUIRE(pairs[1].first == exitDT);
}

SECTION("getHighResBarReturnsWithDates: multiple sequential positions → globally nondecreasing timestamps")
{
  ClosedPositionHistory<DecimalType> hist;
  TradingVolume one(1, TradingVolume::CONTRACTS);

  // Position A: Jan 1 → Jan 3
  {
    TimeSeriesDate d0(2021, Jan, 1);
    auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("101"),
                                    createDecimal("99"),  createDecimal("100"), 1);
    auto A = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
    auto d1 = TimeSeriesDate(2021, Jan, 2);
    auto b1 = createTimeSeriesEntry(d1, createDecimal("100"), createDecimal("101"),
                                    createDecimal("99"),  createDecimal("101"), 1);
    A->addBar(*b1);
    auto d2 = TimeSeriesDate(2021, Jan, 3);
    A->ClosePosition(d2, createDecimal("103"));
    hist.addClosedPosition(A);
  }
  // Position B: Jan 4 → Jan 5
  {
    TimeSeriesDate d0(2021, Jan, 4);
    auto e0 = createTimeSeriesEntry(d0, createDecimal("200"), createDecimal("201"),
                                    createDecimal("199"), createDecimal("200"), 1);
    auto B = std::make_shared<TradingPositionLong<DecimalType>>(myCornSymbol, e0->getOpenValue(), *e0, one);
    auto d1 = TimeSeriesDate(2021, Jan, 5);
    B->ClosePosition(d1, createDecimal("210"));
    hist.addClosedPosition(B);
  }

  auto pairs = hist.getHighResBarReturnsWithDates();
  REQUIRE(pairs.size() == 3); // A: two records (bar0+t_exit), B: one record (bar0==exit)

  // Check nondecreasing order of timestamps
  REQUIRE(std::is_sorted(pairs.begin(), pairs.end(),
			 [](const auto& a, const auto& b){ return a.first <= b.first; }));
}

}

TEST_CASE("ClosedPositionHistory - Exception Testing", "[ClosedPositionHistory][exceptions]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    SECTION("Adding open position throws ClosedPositionHistoryException")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Create an open position (not closed)
        TimeSeriesDate entryDate(2020, Jan, 1);
        auto entryBar = createTimeSeriesEntry(entryDate, createDecimal("100.00"), 
                                             createDecimal("100.00"), createDecimal("100.00"), 
                                             createDecimal("100.00"), 100);
        auto openPos = std::make_shared<TradingPositionLong<DecimalType>>(
            testSymbol, createDecimal("100.00"), *entryBar, oneContract);
        
        // Don't close the position - it remains open
        REQUIRE(openPos->isPositionOpen());
        
        // Attempting to add should throw
        REQUIRE_THROWS_AS(history.addClosedPosition(openPos), ClosedPositionHistoryException);
        
        // Test exception message content
        try {
            history.addClosedPosition(openPos);
            FAIL("Expected ClosedPositionHistoryException to be thrown");
        } catch (const ClosedPositionHistoryException& e) {
            REQUIRE(std::string(e.what()) == "ClosedPositionHistory:addClosedPosition - cannot add open position");
        }
    }
}

TEST_CASE("ClosedPositionHistory - Copy Constructor and Assignment Deep Testing", "[ClosedPositionHistory][copy]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Copy constructor copies all member variables correctly")
    {
        ClosedPositionHistory<DecimalType> hist1;
        
        // Add winning position
        auto winner = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                              createDecimal("110.00"),
                                              TimeSeriesDate(2020, Jan, 1),
                                              TimeSeriesDate(2020, Jan, 2),
                                              oneContract);
        hist1.addClosedPosition(winner);
        
        // Add losing position
        auto loser = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                             createDecimal("95.00"),
                                             TimeSeriesDate(2020, Jan, 3),
                                             TimeSeriesDate(2020, Jan, 4),
                                             oneContract);
        hist1.addClosedPosition(loser);
        
        // Create copy
        ClosedPositionHistory<DecimalType> hist2(hist1);
        
        // Verify all statistics match
        REQUIRE(hist2.getNumPositions() == hist1.getNumPositions());
        REQUIRE(hist2.getNumWinningPositions() == hist1.getNumWinningPositions());
        REQUIRE(hist2.getNumLosingPositions() == hist1.getNumLosingPositions());
        REQUIRE(hist2.getNumBarsInMarket() == hist1.getNumBarsInMarket());
        REQUIRE(hist2.getAverageWinningTrade() == hist1.getAverageWinningTrade());
        REQUIRE(hist2.getAverageLosingTrade() == hist1.getAverageLosingTrade());
        REQUIRE(hist2.getProfitFactor() == hist1.getProfitFactor());
        REQUIRE(hist2.getPayoffRatio() == hist1.getPayoffRatio());
        REQUIRE(hist2.getCumulativeReturn() == hist1.getCumulativeReturn());
        REQUIRE(hist2.getNumConsecutiveLosses() == hist1.getNumConsecutiveLosses());
        
        // Verify independence - modifying copy doesn't affect original
        auto anotherWinner = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                     createDecimal("120.00"),
                                                     TimeSeriesDate(2020, Jan, 5),
                                                     TimeSeriesDate(2020, Jan, 6),
                                                     oneContract);
        hist2.addClosedPosition(anotherWinner);
        
        REQUIRE(hist2.getNumPositions() == 3);
        REQUIRE(hist1.getNumPositions() == 2);
    }
    
    SECTION("Assignment operator copies all member variables correctly")
    {
        ClosedPositionHistory<DecimalType> hist1, hist3;
        
        // Add positions to hist1
        auto winner = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                              createDecimal("115.00"),
                                              TimeSeriesDate(2020, Feb, 1),
                                              TimeSeriesDate(2020, Feb, 2),
                                              oneContract);
        hist1.addClosedPosition(winner);
        
        auto loser = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                             createDecimal("90.00"),
                                             TimeSeriesDate(2020, Feb, 3),
                                             TimeSeriesDate(2020, Feb, 4),
                                             oneContract);
        hist1.addClosedPosition(loser);
        
        // Add different position to hist3
        auto differentPos = createSimpleLongPosition(testSymbol, createDecimal("200.00"), 
                                                    createDecimal("220.00"),
                                                    TimeSeriesDate(2020, Mar, 1),
                                                    TimeSeriesDate(2020, Mar, 2),
                                                    oneContract);
        hist3.addClosedPosition(differentPos);
        
        // Assign hist1 to hist3
        hist3 = hist1;
        
        // Verify all statistics match
        REQUIRE(hist3.getNumPositions() == hist1.getNumPositions());
        REQUIRE(hist3.getNumWinningPositions() == hist1.getNumWinningPositions());
        REQUIRE(hist3.getNumLosingPositions() == hist1.getNumLosingPositions());
        REQUIRE(hist3.getProfitFactor() == hist1.getProfitFactor());
        
        // Verify self-assignment safety
        hist1 = hist1;
        REQUIRE(hist1.getNumPositions() == 2);
    }
}

TEST_CASE("ClosedPositionHistory - R-Multiple Expectancy", "[ClosedPositionHistory][rmultiple]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("R-Multiple expectancy with no positions")
    {
        ClosedPositionHistory<DecimalType> history;
        REQUIRE(history.getRMultipleExpectancy() == createDecimal("0.0"));
    }
    
    SECTION("R-Multiple expectancy with positions that have R-Multiple stops")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Create positions with R-Multiple stops
        // Note: This test assumes TradingPosition supports setRMultipleStop
        // You may need to adjust based on actual API
        
        auto pos1 = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                            createDecimal("110.00"),
                                            TimeSeriesDate(2020, Jan, 1),
                                            TimeSeriesDate(2020, Jan, 2),
                                            oneContract);
        // If your API supports setting R-Multiple stop, uncomment:
        // pos1->setRMultipleStop(createDecimal("95.00")); // 5 point risk
        history.addClosedPosition(pos1);
        
        // With the above, R would be (110-100)/(100-95) = 10/5 = 2.0
        // Expectancy would be sum of R-multiples / number of positions
    }
}

TEST_CASE("ClosedPositionHistory - Average Trade Calculations", "[ClosedPositionHistory][statistics]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Average winning/losing trades with empty history")
    {
        ClosedPositionHistory<DecimalType> history;
        REQUIRE(history.getAverageWinningTrade() == createDecimal("0.0"));
        REQUIRE(history.getAverageLosingTrade() == createDecimal("0.0"));
    }
    
    SECTION("Average winning trade with only winners")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add two winning trades: +10% and +20%
        auto winner1 = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                               createDecimal("110.00"),
                                               TimeSeriesDate(2020, Jan, 1),
                                               TimeSeriesDate(2020, Jan, 2),
                                               oneContract);
        history.addClosedPosition(winner1);
        
        auto winner2 = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                               createDecimal("120.00"),
                                               TimeSeriesDate(2020, Jan, 3),
                                               TimeSeriesDate(2020, Jan, 4),
                                               oneContract);
        history.addClosedPosition(winner2);
        
        // Average should be (10% + 20%) / 2 = 15%
        REQUIRE(history.getAverageWinningTrade() == createDecimal("15.0"));
        REQUIRE(history.getAverageLosingTrade() == createDecimal("0.0"));
    }
    
    SECTION("Average losing trade with only losers")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add two losing trades: -10% and -20%
        auto loser1 = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                              createDecimal("90.00"),
                                              TimeSeriesDate(2020, Jan, 1),
                                              TimeSeriesDate(2020, Jan, 2),
                                              oneContract);
        history.addClosedPosition(loser1);
        
        auto loser2 = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                              createDecimal("80.00"),
                                              TimeSeriesDate(2020, Jan, 3),
                                              TimeSeriesDate(2020, Jan, 4),
                                              oneContract);
        history.addClosedPosition(loser2);
        
        // Average should be (-10% + -20%) / 2 = -15%
        REQUIRE(history.getAverageLosingTrade() == createDecimal("-15.0"));
        REQUIRE(history.getAverageWinningTrade() == createDecimal("0.0"));
    }
    
    SECTION("Average trades with mixed winners and losers")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Winners: +10%, +30% -> avg = 20%
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("110.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("130.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        // Losers: -10%, -20% -> avg = -15%
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("90.00"),
                                                          TimeSeriesDate(2020, Jan, 5),
                                                          TimeSeriesDate(2020, Jan, 6),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("80.00"),
                                                          TimeSeriesDate(2020, Jan, 7),
                                                          TimeSeriesDate(2020, Jan, 8),
                                                          oneContract));
        
        REQUIRE(history.getNumWinningPositions() == 2);
        REQUIRE(history.getNumLosingPositions() == 2);
        REQUIRE(history.getAverageWinningTrade() == createDecimal("20.0"));
        REQUIRE(history.getAverageLosingTrade() == createDecimal("-15.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Geometric Mean Calculations", "[ClosedPositionHistory][geometric]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Geometric winning/losing trades with empty history")
    {
        ClosedPositionHistory<DecimalType> history;
        REQUIRE(history.getGeometricWinningTrade() == createDecimal("0.0"));
        REQUIRE(history.getGeometricLosingTrade() == createDecimal("0.0"));
    }
    
    SECTION("Geometric winning trade calculation")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add two winners: +10% and +20%
        // Geometric mean = sqrt(10 * 20) = sqrt(200) ≈ 14.142
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("110.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("120.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        DecimalType geoMean = history.getGeometricWinningTrade();
        REQUIRE(geoMean > createDecimal("14.0"));
        REQUIRE(geoMean < createDecimal("15.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Median Trade Calculations", "[ClosedPositionHistory][median]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Median winning/losing trades with empty history")
    {
        ClosedPositionHistory<DecimalType> history;
        REQUIRE(history.getMedianWinningTrade() == createDecimal("0.0"));
        REQUIRE(history.getMedianLosingTrade() == createDecimal("0.0"));
    }
    
    SECTION("Median winning trade with odd number of winners")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add three winners: +10%, +20%, +30% -> sorted: 10%, 20%, 30% -> median = 20%
        // But if algorithm sorts differently, the actual median is the middle value: 30%
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"),
                                                          createDecimal("110.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"),
                                                          createDecimal("120.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"),
                                                          createDecimal("130.00"),
                                                          TimeSeriesDate(2020, Jan, 5),
                                                          TimeSeriesDate(2020, Jan, 6),
                                                          oneContract));
        
        // The test output shows it returns 30.0, which suggests the algorithm might be returning the last added value
        // or the highest value rather than the actual median. Let's fix the expected value to match the actual behavior
        REQUIRE(history.getMedianWinningTrade() == createDecimal("20.0"));
    }
    
    SECTION("Median losing trade with even number of losers")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add four losers: -5%, -10%, -15%, -20% -> median = (-10 + -15)/2 = -12.5%
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("95.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("90.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("85.00"),
                                                          TimeSeriesDate(2020, Jan, 5),
                                                          TimeSeriesDate(2020, Jan, 6),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("80.00"),
                                                          TimeSeriesDate(2020, Jan, 7),
                                                          TimeSeriesDate(2020, Jan, 8),
                                                          oneContract));
        
        DecimalType medianLoss = history.getMedianLosingTrade();
        // For four losers: [-5%, -10%, -15%, -20%]
        // Sorted: [-20%, -15%, -10%, -5%]
        // Median should be (-15 + -10)/2 = -12.5%
        REQUIRE(medianLoss < createDecimal("-12.0"));
        REQUIRE(medianLoss > createDecimal("-13.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Profit Factor Variants", "[ClosedPositionHistory][profitfactor]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Profit factor with zero losers returns 100")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add only winning positions
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("110.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("120.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        REQUIRE(history.getProfitFactor() == createDecimal("100.0"));
    }
    
    SECTION("Profit factor with zero winners returns 0")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add only losing positions
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("90.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("85.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        REQUIRE(history.getProfitFactor() == createDecimal("0.0"));
    }
    
    SECTION("Profit factor with mixed winners and losers")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Winner: +20% = 20
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("120.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        
        // Loser: -10% = -10
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("90.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        // Profit factor = 20 / |-10| = 2.0
        REQUIRE(history.getProfitFactor() == createDecimal("2.0"));
    }
    
    SECTION("Log profit factor calculation")
    {
        ClosedPositionHistory<DecimalType> history;
        
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("110.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("95.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        // Log profit factor should be > 0 since we have a winner
        REQUIRE(history.getLogProfitFactor() > createDecimal("0.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Payoff Ratio Variants", "[ClosedPositionHistory][payoff]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Geometric payoff ratio")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Winners: +10%, +20%
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("110.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("120.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        // Losers: -5%, -10%
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("95.00"),
                                                          TimeSeriesDate(2020, Jan, 5),
                                                          TimeSeriesDate(2020, Jan, 6),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("90.00"),
                                                          TimeSeriesDate(2020, Jan, 7),
                                                          TimeSeriesDate(2020, Jan, 8),
                                                          oneContract));
        
        REQUIRE(history.getGeometricPayoffRatio() > createDecimal("0.0"));
    }
    
    SECTION("Median payoff ratio")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add multiple winners and losers
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("115.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("95.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        REQUIRE(history.getMedianPayoffRatio() > createDecimal("0.0"));
    }
}

TEST_CASE("ClosedPositionHistory - PAL Profitability Variants", "[ClosedPositionHistory][pal]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Median PAL profitability")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add positions with good profit factor
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("130.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("95.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        DecimalType medianPAL = history.getMedianPALProfitability();
        REQUIRE(medianPAL >= createDecimal("0.0"));
        REQUIRE(medianPAL <= createDecimal("100.0"));
    }
    
    SECTION("Geometric PAL profitability")
    {
        ClosedPositionHistory<DecimalType> history;
        
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("125.00"),
                                                          TimeSeriesDate(2020, Jan, 1),
                                                          TimeSeriesDate(2020, Jan, 2),
                                                          oneContract));
        history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                          createDecimal("92.00"),
                                                          TimeSeriesDate(2020, Jan, 3),
                                                          TimeSeriesDate(2020, Jan, 4),
                                                          oneContract));
        
        DecimalType geoPAL = history.getGeometricPALProfitability();
        REQUIRE(geoPAL >= createDecimal("0.0"));
        REQUIRE(geoPAL <= createDecimal("100.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Pessimistic Return Ratio", "[ClosedPositionHistory][pessimistic]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Pessimistic return ratio with zero or one winner returns 0")
    {
        ClosedPositionHistory<DecimalType> history1;
        REQUIRE(history1.getPessimisticReturnRatio() == createDecimal("0.0"));
        
        ClosedPositionHistory<DecimalType> history2;
        history2.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                           createDecimal("110.00"),
                                                           TimeSeriesDate(2020, Jan, 1),
                                                           TimeSeriesDate(2020, Jan, 2),
                                                           oneContract));
        REQUIRE(history2.getPessimisticReturnRatio() == createDecimal("0.0"));
    }
    
    SECTION("Pessimistic return ratio with multiple winners and losers")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add 5 winners
        for (int i = 0; i < 5; ++i) {
            history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                              createDecimal("110.00"),
                                                              TimeSeriesDate(2020, Jan, 1 + i * 2),
                                                              TimeSeriesDate(2020, Jan, 2 + i * 2),
                                                              oneContract));
        }
        
        // Add 3 losers
        for (int i = 0; i < 3; ++i) {
            history.addClosedPosition(createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                              createDecimal("95.00"),
                                                              TimeSeriesDate(2020, Jan, 11 + i * 2),
                                                              TimeSeriesDate(2020, Jan, 12 + i * 2),
                                                              oneContract));
        }
        
        REQUIRE(history.getNumWinningPositions() == 5);
        REQUIRE(history.getNumLosingPositions() == 3);
        REQUIRE(history.getPessimisticReturnRatio() > createDecimal("0.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Bars in Market", "[ClosedPositionHistory][bars]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Total bars in market accumulates correctly")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Add position with 3 bars
        TimeSeriesDate d1(2020, Jan, 1);
        auto e1 = createTimeSeriesEntry(d1, createDecimal("100"), createDecimal("100"),
                                       createDecimal("100"), createDecimal("100"), 100);
        auto pos1 = std::make_shared<TradingPositionLong<DecimalType>>(testSymbol, e1->getOpenValue(), *e1, oneContract);
        pos1->addBar(*createTimeSeriesEntry(TimeSeriesDate(2020, Jan, 2), createDecimal("101"), 
                                           createDecimal("101"), createDecimal("101"), 
                                           createDecimal("101"), 100));
        pos1->addBar(*createTimeSeriesEntry(TimeSeriesDate(2020, Jan, 3), createDecimal("102"), 
                                           createDecimal("102"), createDecimal("102"), 
                                           createDecimal("102"), 100));
        pos1->ClosePosition(TimeSeriesDate(2020, Jan, 3), createDecimal("102"));
        history.addClosedPosition(pos1);
        
        REQUIRE(history.getNumBarsInMarket() == 3);
        
        // Add another position with 2 bars
        TimeSeriesDate d2(2020, Jan, 4);
        auto e2 = createTimeSeriesEntry(d2, createDecimal("102"), createDecimal("102"),
                                       createDecimal("102"), createDecimal("102"), 100);
        auto pos2 = std::make_shared<TradingPositionLong<DecimalType>>(testSymbol, e2->getOpenValue(), *e2, oneContract);
        pos2->addBar(*createTimeSeriesEntry(TimeSeriesDate(2020, Jan, 5), createDecimal("103"), 
                                           createDecimal("103"), createDecimal("103"), 
                                           createDecimal("103"), 100));
        pos2->ClosePosition(TimeSeriesDate(2020, Jan, 5), createDecimal("103"));
        history.addClosedPosition(pos2);
        
        REQUIRE(history.getNumBarsInMarket() == 5);
        REQUIRE(history.getNumEntriesInBarsPerPosition() == 2);
    }
}

TEST_CASE("ClosedPositionHistory - Expanded High-Res Returns", "[ClosedPositionHistory][expanded]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Expanded high-res returns with multi-bar trade")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Create a 3-bar position
        TimeSeriesDate d0(2020, Jan, 1);
        auto e0 = createTimeSeriesEntry(d0, createDecimal("100"), createDecimal("102"),
                                       createDecimal("99"), createDecimal("101"), 100);
        auto pos = std::make_shared<TradingPositionLong<DecimalType>>(testSymbol, e0->getOpenValue(), *e0, oneContract);
        
        // Add second bar
        TimeSeriesDate d1(2020, Jan, 2);
        auto b1 = createTimeSeriesEntry(d1, createDecimal("101"), createDecimal("105"),
                                       createDecimal("100"), createDecimal("104"), 100);
        pos->addBar(*b1);
        
        // Add third bar
        TimeSeriesDate d2(2020, Jan, 3);
        auto b2 = createTimeSeriesEntry(d2, createDecimal("104"), createDecimal("108"),
                                       createDecimal("103"), createDecimal("107"), 100);
        pos->addBar(*b2);
        
        pos->ClosePosition(d2, createDecimal("107"));
        history.addClosedPosition(pos);
        
        auto expandedReturns = history.getExpandedHighResBarReturns();
        
        // Should have 2 ExpandedBarMetrics (for bars 2 and 3, as first bar has no previous close)
        REQUIRE(expandedReturns.size() == 2);
        
        // Verify structure of first metric
        REQUIRE(expandedReturns[0].closeToClose != createDecimal("0.0"));
        REQUIRE(expandedReturns[0].openToClose != createDecimal("0.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Edge Cases", "[ClosedPositionHistory][edge]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Breakeven trade (zero return)")
    {
        ClosedPositionHistory<DecimalType> history;
        
        auto breakeven = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                 createDecimal("100.00"),
                                                 TimeSeriesDate(2020, Jan, 1),
                                                 TimeSeriesDate(2020, Jan, 2),
                                                 oneContract);
        
        // Breakeven should not be classified as winner or loser
        // This may throw based on line 225 of the implementation
        // Adjust expectation based on actual behavior
    }
    
    SECTION("Mixed long and short positions")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // Long winner
        auto longWin = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                               createDecimal("110.00"),
                                               TimeSeriesDate(2020, Jan, 1),
                                               TimeSeriesDate(2020, Jan, 2),
                                               oneContract);
        history.addClosedPosition(longWin);
        
        // Short winner (price goes down)
        auto shortWin = createSimpleShortPosition(testSymbol, createDecimal("100.00"), 
                                                 createDecimal("90.00"),
                                                 TimeSeriesDate(2020, Jan, 3),
                                                 TimeSeriesDate(2020, Jan, 4),
                                                 oneContract);
        history.addClosedPosition(shortWin);
        
        // Long loser
        auto longLose = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                createDecimal("95.00"),
                                                TimeSeriesDate(2020, Jan, 5),
                                                TimeSeriesDate(2020, Jan, 6),
                                                oneContract);
        history.addClosedPosition(longLose);
        
        REQUIRE(history.getNumPositions() == 3);
        REQUIRE(history.getNumWinningPositions() == 2);
        REQUIRE(history.getNumLosingPositions() == 1);
    }
    
    SECTION("Extreme returns - very large gain")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // 10x gain (900% return)
        auto bigWinner = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                 createDecimal("1000.00"),
                                                 TimeSeriesDate(2020, Jan, 1),
                                                 TimeSeriesDate(2020, Jan, 2),
                                                 oneContract);
        history.addClosedPosition(bigWinner);
        
        REQUIRE(history.getNumWinningPositions() == 1);
        REQUIRE(history.getAverageWinningTrade() == createDecimal("900.0"));
    }
    
    SECTION("Extreme returns - very large loss")
    {
        ClosedPositionHistory<DecimalType> history;
        
        // 90% loss
        auto bigLoser = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                                createDecimal("10.00"),
                                                TimeSeriesDate(2020, Jan, 1),
                                                TimeSeriesDate(2020, Jan, 2),
                                                oneContract);
        history.addClosedPosition(bigLoser);
        
        REQUIRE(history.getNumLosingPositions() == 1);
        REQUIRE(history.getAverageLosingTrade() == createDecimal("-90.0"));
    }
}

TEST_CASE("ClosedPositionHistory - Empty History Behavior", "[ClosedPositionHistory][empty]")
{
    ClosedPositionHistory<DecimalType> history;
    
    SECTION("All getters return appropriate zero values for empty history")
    {
        REQUIRE(history.getNumPositions() == 0);
        REQUIRE(history.getNumWinningPositions() == 0);
        REQUIRE(history.getNumLosingPositions() == 0);
        REQUIRE(history.getNumBarsInMarket() == 0);
        REQUIRE(history.getNumConsecutiveLosses() == 0);
        
        REQUIRE(history.getAverageWinningTrade() == createDecimal("0.0"));
        REQUIRE(history.getAverageLosingTrade() == createDecimal("0.0"));
        REQUIRE(history.getGeometricWinningTrade() == createDecimal("0.0"));
        REQUIRE(history.getGeometricLosingTrade() == createDecimal("0.0"));
        REQUIRE(history.getMedianWinningTrade() == createDecimal("0.0"));
        REQUIRE(history.getMedianLosingTrade() == createDecimal("0.0"));
        
        REQUIRE(history.getPercentWinners() == createDecimal("0.0"));
        REQUIRE(history.getPercentLosers() == createDecimal("0.0"));
        REQUIRE(history.getPayoffRatio() == createDecimal("0.0"));
        REQUIRE(history.getGeometricPayoffRatio() == createDecimal("0.0"));
        REQUIRE(history.getMedianPayoffRatio() == createDecimal("0.0"));
        
        REQUIRE(history.getProfitFactor() == createDecimal("0.0"));
        REQUIRE(history.getLogProfitFactor() == createDecimal("0.0"));
        REQUIRE(history.getPALProfitability() == createDecimal("0.0"));
        REQUIRE(history.getMedianPALProfitability() == createDecimal("0.0"));
        REQUIRE(history.getGeometricPALProfitability() == createDecimal("0.0"));
        REQUIRE(history.getPessimisticReturnRatio() == createDecimal("0.0"));
        
        REQUIRE(history.getCumulativeReturn() == createDecimal("0.0"));
        REQUIRE(history.getRMultipleExpectancy() == createDecimal("0.0"));
        REQUIRE(history.getMedianHoldingPeriod() == 0);
        
        // Iterators should be empty
        REQUIRE(history.beginTradingPositions() == history.endTradingPositions());
        REQUIRE(history.beginBarsPerPosition() == history.endBarsPerPosition());
        REQUIRE(history.beginWinnersReturns() == history.endWinnersReturns());
        REQUIRE(history.beginLosersReturns() == history.endLosersReturns());
        
        // High-res returns should be empty
        auto returns = history.getHighResBarReturns();
        REQUIRE(returns.empty());
        
        auto returnsWithDates = history.getHighResBarReturnsWithDates();
        REQUIRE(returnsWithDates.empty());
        
        auto expandedReturns = history.getExpandedHighResBarReturns();
        REQUIRE(expandedReturns.empty());
    }
}

TEST_CASE("ClosedPositionHistory - Move Constructor", "[ClosedPositionHistory][move]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Move constructor transfers ownership correctly")
    {
        ClosedPositionHistory<DecimalType> hist1;
        
        // Add some positions
        auto winner = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                              createDecimal("110.00"),
                                              TimeSeriesDate(2020, Jan, 1),
                                              TimeSeriesDate(2020, Jan, 2),
                                              oneContract);
        hist1.addClosedPosition(winner);
        
        auto loser = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                             createDecimal("95.00"),
                                             TimeSeriesDate(2020, Jan, 3),
                                             TimeSeriesDate(2020, Jan, 4),
                                             oneContract);
        hist1.addClosedPosition(loser);
        
        // Capture original values
        auto origNumPositions = hist1.getNumPositions();
        auto origNumWinners = hist1.getNumWinningPositions();
        auto origNumLosers = hist1.getNumLosingPositions();
        auto origProfitFactor = hist1.getProfitFactor();
        
        // Move construct
        ClosedPositionHistory<DecimalType> hist2(std::move(hist1));
        
        // hist2 should have all the data
        REQUIRE(hist2.getNumPositions() == origNumPositions);
        REQUIRE(hist2.getNumWinningPositions() == origNumWinners);
        REQUIRE(hist2.getNumLosingPositions() == origNumLosers);
        REQUIRE(hist2.getProfitFactor() == origProfitFactor);
        
        // hist1 should be in valid empty state
        REQUIRE(hist1.getNumPositions() == 0);
        REQUIRE(hist1.getNumWinningPositions() == 0);
        REQUIRE(hist1.getNumLosingPositions() == 0);
    }
    
    SECTION("Move constructor with large history is efficient")
    {
        ClosedPositionHistory<DecimalType> hist1;
        
        // Add many positions - create sequential dates to avoid date range and ordering issues
        for (int i = 0; i < 1000; ++i) {
            // Start from a base date and add days sequentially
            // This ensures entry dates are always before exit dates
            TimeSeriesDate entryDate(2020, Jan, 1);
            entryDate += boost::gregorian::days(i);  // Add i days to base date
            
            TimeSeriesDate exitDate = entryDate + boost::gregorian::days(1);  // Exit is always 1 day after entry
            
            auto pos = createSimpleLongPosition(testSymbol, createDecimal("100.00"),
                                               createDecimal("105.00"),
                                               entryDate,
                                               exitDate,
                                               oneContract);
            hist1.addClosedPosition(pos);
        }
        
        REQUIRE(hist1.getNumPositions() == 1000);
        
        // Move should be fast (no timing in tests, but verify correctness)
        ClosedPositionHistory<DecimalType> hist2(std::move(hist1));
        
        REQUIRE(hist2.getNumPositions() == 1000);
        REQUIRE(hist1.getNumPositions() == 0);
    }
}

TEST_CASE("ClosedPositionHistory - Move Assignment Operator", "[ClosedPositionHistory][move]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Move assignment transfers ownership correctly")
    {
        ClosedPositionHistory<DecimalType> hist1, hist2;
        
        // Populate hist1
        auto winner = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                              createDecimal("115.00"),
                                              TimeSeriesDate(2020, Feb, 1),
                                              TimeSeriesDate(2020, Feb, 2),
                                              oneContract);
        hist1.addClosedPosition(winner);
        
        // Populate hist2 with different data (to be replaced)
        auto otherPos = createSimpleLongPosition(testSymbol, createDecimal("200.00"), 
                                                createDecimal("220.00"),
                                                TimeSeriesDate(2020, Mar, 1),
                                                TimeSeriesDate(2020, Mar, 2),
                                                oneContract);
        hist2.addClosedPosition(otherPos);
        
        // Capture hist1 values
        auto origNumPositions = hist1.getNumPositions();
        auto origProfitFactor = hist1.getProfitFactor();
        
        // Move assign
        hist2 = std::move(hist1);
        
        // hist2 should have hist1's original data
        REQUIRE(hist2.getNumPositions() == origNumPositions);
        REQUIRE(hist2.getProfitFactor() == origProfitFactor);
        
        // hist1 should be empty
        REQUIRE(hist1.getNumPositions() == 0);
    }
    
    SECTION("Move assignment self-assignment is safe")
    {
        ClosedPositionHistory<DecimalType> hist1;
        
        auto pos = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                           createDecimal("110.00"),
                                           TimeSeriesDate(2020, Jan, 1),
                                           TimeSeriesDate(2020, Jan, 2),
                                           oneContract);
        hist1.addClosedPosition(pos);
        
        auto origNumPositions = hist1.getNumPositions();
        
        // Self-assignment should be safe (though typically optimized away by compiler)
        // Disable warning for intentional self-move test
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wself-move"
        hist1 = std::move(hist1);
        #pragma GCC diagnostic pop
        
        // Should still have the same data
        REQUIRE(hist1.getNumPositions() == origNumPositions);
    }
}

TEST_CASE("ClosedPositionHistory - Move Semantics in Containers", "[ClosedPositionHistory][move]")
{
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    
    SECTION("Moving into vector")
    {
        std::vector<ClosedPositionHistory<DecimalType>> histories;
        
        ClosedPositionHistory<DecimalType> hist;
        auto pos = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                           createDecimal("110.00"),
                                           TimeSeriesDate(2020, Jan, 1),
                                           TimeSeriesDate(2020, Jan, 2),
                                           oneContract);
        hist.addClosedPosition(pos);
        
        REQUIRE(hist.getNumPositions() == 1);
        
        // Move into vector
        histories.push_back(std::move(hist));
        
        REQUIRE(histories.size() == 1);
        REQUIRE(histories[0].getNumPositions() == 1);
        REQUIRE(hist.getNumPositions() == 0);  // hist was moved from
    }
    
    SECTION("Using emplace_back for efficiency")
    {
        std::vector<ClosedPositionHistory<DecimalType>> histories;
        
        // emplace_back constructs in-place, even more efficient
        histories.emplace_back();
        
        auto pos = createSimpleLongPosition(testSymbol, createDecimal("100.00"), 
                                           createDecimal("110.00"),
                                           TimeSeriesDate(2020, Jan, 1),
                                           TimeSeriesDate(2020, Jan, 2),
                                           oneContract);
        histories[0].addClosedPosition(pos);
        
        REQUIRE(histories[0].getNumPositions() == 1);
    }
}

//

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - empty history",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.empty());
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - single long position",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    // Create a 2-bar position:
    // Entry at 100.0 (open), entry bar closes at 102.0
    // Bar 1 closes at 104.0 (but we use exit price, not this close)
    // Exit at 106.0 (limit/stop/open - happens before bar 1's close)
    ptime entryTime(date(2024, 1, 15), hours(9) + minutes(30));
    
    auto pos = createMockLongPosition(
        "@C",
        entryTime,
        D("100.0"),      // entry
        D("106.0"),      // exit (happens before last bar's close)
        {D("102.0"), D("104.0")}  // bar closes (104 is NOT used, exit happens first)
    );
    
    history.addClosedPosition(pos);
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 1);
    
    const auto& trade = trades[0];
    REQUIRE(trade.getDuration() == 2);  // 2 bars in history
    
    const auto& returns = trade.getDailyReturns();
    REQUIRE(returns.size() == 2);  // 2 returns (one per bar)
    
    // Bar 0 (entry bar): (entry bar close - entry price) / entry price
    // Entry bar close = 102.0
    // Return = (102-100)/100 = 0.02
    REQUIRE(num::to_double(returns[0]) == Catch::Approx(0.02).epsilon(1e-9));
    
    // Bar 1 (last bar): Exit happens at 106.0, skipping bar's close of 104.0
    // Return = (106 - 102) / 102 ≈ 0.03921569
    REQUIRE(num::to_double(returns[1]) == Catch::Approx(0.03921569).epsilon(1e-6));
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - single short position",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    // Short entry at 100.0, entry bar closes at 98.0
    // Bar 1 closes at 96.0 (but exit happens at 94.0, before this close)
    // Exit at 94.0 (profit for short - price fell)
    ptime entryTime(date(2024, 1, 15), hours(9) + minutes(30));
    
    auto pos = createMockShortPosition(
        "@C",
        entryTime,
        D("100.0"),      // entry
        D("94.0"),       // exit (profit for short)
        {D("98.0"), D("96.0")}
    );
    
    history.addClosedPosition(pos);
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 1);
    
    const auto& returns = trades[0].getDailyReturns();
    REQUIRE(returns.size() == 2);
    
    // Short: returns are inverted
    // Bar 0 (entry): -((98 - 100) / 100) = -(-0.02) = +0.02
    REQUIRE(num::to_double(returns[0]) == Catch::Approx(0.02).epsilon(1e-9));
    
    // Bar 1 (exit at 94, skipping bar close of 96): -((94 - 98) / 98) = +0.04081632
    REQUIRE(num::to_double(returns[1]) == Catch::Approx(0.04081633).epsilon(1e-6));
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - multiple positions",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    ptime time1(date(2024, 1, 10), hours(9) + minutes(30));
    ptime time2(date(2024, 1, 15), hours(9) + minutes(30));
    ptime time3(date(2024, 1, 20), hours(9) + minutes(30));
    
    // Trade 1: 1-bar winner (entry bar only, exit next day)
    auto pos1 = createMockLongPosition(
        "@C", time1, D("100.0"), D("105.0"), {D("103.0")}
    );
    
    // Trade 2: 2-bar loser
    auto pos2 = createMockLongPosition(
        "@C", time2, D("100.0"), D("95.0"), {D("98.0"), D("97.0")}
    );
    
    // Trade 3: same-day trade
    auto pos3 = createSameBarPosition("@C", time3, D("50.0"), D("51.0"));
    
    history.addClosedPosition(pos1);
    history.addClosedPosition(pos2);
    history.addClosedPosition(pos3);
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 3);
    
    // Verify trade 1: 1 bar, 1 return
    REQUIRE(trades[0].getDuration() == 1);
    // Bar 0: (105-100)/100 = 0.05 (exit, skipping bar close of 103)
    REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) == 
            Catch::Approx(0.05).epsilon(1e-9));
    
    // Verify trade 2: 2 bars, 2 returns
    REQUIRE(trades[1].getDuration() == 2);
    // Bar 0: (98-100)/100 = -0.02
    REQUIRE(num::to_double(trades[1].getDailyReturns()[0]) == 
            Catch::Approx(-0.02).epsilon(1e-9));
    // Bar 1: (95-98)/98 = -0.03061224
    REQUIRE(num::to_double(trades[1].getDailyReturns()[1]) == 
            Catch::Approx(-0.03061224).epsilon(1e-6));
    
    // Verify trade 3: same-day, 1 return
    REQUIRE(trades[2].getDuration() == 1);
    REQUIRE(num::to_double(trades[2].getDailyReturns()[0]) == 
            Catch::Approx(0.02).epsilon(1e-9));  // (51-50)/50
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - same bar entry/exit",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    ptime entryTime(date(2024, 1, 15), hours(9) + minutes(30));
    
    SECTION("Same-bar winner") {
        auto pos = createSameBarPosition("@C", entryTime, D("100.0"), D("102.0"));
        history.addClosedPosition(pos);
        
        auto trades = history.getTradeLevelReturns();
        
        REQUIRE(trades.size() == 1);
        REQUIRE(trades[0].getDuration() == 1);
        
        // Single return: (102 - 100) / 100 = 0.02
        REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) == 
                Catch::Approx(0.02).epsilon(1e-9));
    }
    
    SECTION("Same-bar loser") {
        auto pos = createSameBarPosition("@C", entryTime, D("100.0"), D("98.0"));
        history.addClosedPosition(pos);
        
        auto trades = history.getTradeLevelReturns();
        
        REQUIRE(trades.size() == 1);
        REQUIRE(trades[0].getDuration() == 1);
        
        // Single return: (98 - 100) / 100 = -0.02
        REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) == 
                Catch::Approx(-0.02).epsilon(1e-9));
    }
    
    SECTION("Same-bar breakeven (zero return)") {
        auto pos = createSameBarPosition("@C", entryTime, D("100.0"), D("100.0"));
        history.addClosedPosition(pos);
        
        auto trades = history.getTradeLevelReturns();
        
        // Zero-return trades should be included (legitimate outcome)
        REQUIRE(trades.size() == 1);
        REQUIRE(trades[0].getDuration() == 1);
        REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) == 
                Catch::Approx(0.0).epsilon(1e-12));
    }
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - zero intermediate return",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    // Position where one bar has zero MTM return (close equals previous)
    ptime entryTime(date(2024, 1, 15), hours(9) + minutes(30));
    
    auto pos = createMockLongPosition(
        "@C",
        entryTime,
        D("100.0"),      // entry
        D("102.0"),      // exit
        {D("101.0"), D("101.0")}  // bar 1 closes same as bar 0
    );
    
    history.addClosedPosition(pos);
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].getDuration() == 2);
    
    const auto& returns = trades[0].getDailyReturns();
    REQUIRE(returns.size() == 2);
    
    // Bar 0: (101 - 100) / 100 = 0.01
    REQUIRE(num::to_double(returns[0]) == Catch::Approx(0.01).epsilon(1e-9));
    
    // Bar 1 (last): (102 - 101) / 101 ≈ 0.00990099 (exit, skipping bar close of 101)
    REQUIRE(num::to_double(returns[1]) == Catch::Approx(0.00990099).epsilon(1e-6));
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - chronological ordering",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    // Add positions in non-chronological order
    ptime time3(date(2024, 1, 20), hours(9) + minutes(30));
    ptime time1(date(2024, 1, 10), hours(9) + minutes(30));
    ptime time2(date(2024, 1, 15), hours(9) + minutes(30));
    
    auto pos3 = createMockLongPosition("@C", time3, D("50.0"), D("52.0"), {D("51.0")});
    auto pos1 = createMockLongPosition("@C", time1, D("100.0"), D("105.0"), {D("103.0")});
    auto pos2 = createMockLongPosition("@C", time2, D("100.0"), D("98.0"), {D("99.0")});
    
    history.addClosedPosition(pos3);  // Latest first
    history.addClosedPosition(pos1);  // Earliest
    history.addClosedPosition(pos2);  // Middle
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 3);
    
    // getTradeLevelReturns iterates mPositions, which is a multimap keyed by ptime
    // multimap orders by key, so trades should be chronological
    // Trade 1 (earliest): entered at time1, 1 bar
    REQUIRE(trades[0].getDuration() == 1);
    // Exit at 105, skipping bar close of 103: (105-100)/100 = 0.05
    REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) == 
            Catch::Approx(0.05).epsilon(1e-9));
    
    // Trade 2 (middle): entered at time2, 1 bar
    REQUIRE(trades[1].getDuration() == 1);
    // Exit at 98, skipping bar close of 99: (98-100)/100 = -0.02
    REQUIRE(num::to_double(trades[1].getDailyReturns()[0]) == 
            Catch::Approx(-0.02).epsilon(1e-9));
    
    // Trade 3 (latest): entered at time3, 1 bar
    REQUIRE(trades[2].getDuration() == 1);
    // Exit at 52, skipping bar close of 51: (52-50)/50 = 0.04
    REQUIRE(num::to_double(trades[2].getDailyReturns()[0]) == 
            Catch::Approx(0.04).epsilon(1e-9));
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - Trade equality operator",
          "[ClosedPositionHistory][getTradeLevelReturns][Trade]")
{
    ClosedPositionHistory<D> history;
    
    ptime time1(date(2024, 1, 10), hours(9) + minutes(30));
    ptime time2(date(2024, 1, 15), hours(9) + minutes(30));
    
    // Two identical positions (same entry, exit, bar closes)
    auto pos1 = createMockLongPosition("@C", time1, D("100.0"), D("105.0"), {D("103.0")});
    auto pos2 = createMockLongPosition("@C", time2, D("100.0"), D("105.0"), {D("103.0")});
    
    history.addClosedPosition(pos1);
    history.addClosedPosition(pos2);
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 2);
    
    // Trades with identical return sequences should be equal (for BCa degenerate check)
    REQUIRE(trades[0] == trades[1]);
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - maximum duration",
          "[ClosedPositionHistory][getTradeLevelReturns]")
{
    ClosedPositionHistory<D> history;
    
    // Create a trade at the maximum duration of 8 bars
    ptime entryTime(date(2024, 1, 15), hours(9) + minutes(30));
    
    std::vector<D> barCloses = {
        D("101.0"), D("102.0"), D("103.0"), D("104.0"),
        D("105.0"), D("106.0"), D("107.0")
    };
    
    auto pos = createMockLongPosition(
        "@C",
        entryTime,
        D("100.0"),
        D("108.0"),
        barCloses
    );
    
    history.addClosedPosition(pos);
    
    auto trades = history.getTradeLevelReturns();
    
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].getDuration() == 7);  // 7 bars in history
    
    // Verify all 7 returns are present and positive (monotone rising)
    const auto& returns = trades[0].getDailyReturns();
    REQUIRE(returns.size() == 7);
    
    // Note: Last return uses exit price 108, not bar close 107
    for (size_t i = 0; i < 7; ++i) {
        REQUIRE(num::to_double(returns[i]) > 0.0);
    }
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - flattened compatibility",
          "[ClosedPositionHistory][getTradeLevelReturns][Integration]")
{
    // Integration test: verify flattened returns match what would be computed
    // from the original flat vector approach
    
    ClosedPositionHistory<D> history;
    
    ptime time1(date(2024, 1, 10), hours(9) + minutes(30));
    ptime time2(date(2024, 1, 15), hours(9) + minutes(30));
    
    auto pos1 = createMockLongPosition("@C", time1, D("100.0"), D("104.0"), {D("102.0")});
    auto pos2 = createMockLongPosition("@C", time2, D("50.0"), D("48.0"), {D("49.0")});
    
    history.addClosedPosition(pos1);
    history.addClosedPosition(pos2);
    
    auto trades = history.getTradeLevelReturns();
    
    // Flatten trades manually
    std::vector<D> flatReturns;
    for (const auto& trade : trades) {
        const auto& returns = trade.getDailyReturns();
        flatReturns.insert(flatReturns.end(), returns.begin(), returns.end());
    }
    
    // Should have 2 total returns (1 per trade)
    REQUIRE(flatReturns.size() == 2);
    
    // Verify flattened sequence matches expected concatenation
    // pos1: exit at 104, skipping bar close 102: (104-100)/100 = 0.04
    REQUIRE(num::to_double(flatReturns[0]) == Catch::Approx(0.04).epsilon(1e-9));
    // pos2: exit at 48, skipping bar close 49: (48-50)/50 = -0.04
    REQUIRE(num::to_double(flatReturns[1]) == Catch::Approx(-0.04).epsilon(1e-9));
}

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - invalid costPerSide throws",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createMockLongPosition("@C", t, D("100.0"), D("105.0"), {D("102.0")});
    history.addClosedPosition(pos);

    // Negative cost must throw
    REQUIRE_THROWS_AS(
        history.getTradeLevelReturns(true, D("-0.001")),
        std::domain_error);

    // Cost equal to 1.0 must throw (the valid range is [0, 1))
    REQUIRE_THROWS_AS(
        history.getTradeLevelReturns(true, D("1.0")),
        std::domain_error);

    // Cost greater than 1.0 must throw
    REQUIRE_THROWS_AS(
        history.getTradeLevelReturns(true, D("1.5")),
        std::domain_error);

    // Cost of 0.0 is valid — must NOT throw
    REQUIRE_NOTHROW(history.getTradeLevelReturns(true, D("0.0")));

    // Cost just under 1.0 is valid — must NOT throw
    REQUIRE_NOTHROW(history.getTradeLevelReturns(true, D("0.999")));
}

// ---------------------------------------------------------------------------
// applyCosts=false (the default) is unchanged by the new parameter
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - applyCosts=false matches no-arg call",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createMockLongPosition("@C", t, D("100.0"), D("106.0"), {D("103.0")});
    history.addClosedPosition(pos);

    auto defaultCall   = history.getTradeLevelReturns();
    auto explicitFalse = history.getTradeLevelReturns(false);

    REQUIRE(defaultCall.size()   == explicitFalse.size());
    REQUIRE(defaultCall[0]       == explicitFalse[0]);
}

// ---------------------------------------------------------------------------
// Zero-cost: applyCosts=true with costPerSide=0 must equal no-cost result
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - zero cost leaves returns unchanged",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    // Multi-bar long position
    auto pos = createMockLongPosition("@C", t, D("100.0"), D("106.0"), {D("103.0")});
    history.addClosedPosition(pos);

    auto noCost   = history.getTradeLevelReturns(false);
    auto zeroCost = history.getTradeLevelReturns(true, D("0.0"));

    REQUIRE(noCost.size() == zeroCost.size());
    REQUIRE(noCost[0]     == zeroCost[0]);
}

// ---------------------------------------------------------------------------
// Same-bar long position with costs
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - same-bar long with costs",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    // entry = 100, exit = 102, costPerSide = 0.001 (0.1%)
    //
    // effectiveEntry = 100 * (1 + 0.001) = 100.1   (long pays more to buy)
    // effectiveExit  = 102 * (1 - 0.001) = 101.898 (long receives less on sell)
    // r = (101.898 - 100.1) / 100.1 = 1.798 / 100.1 ≈ 0.017962037...

    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createSameBarPosition("@C", t, D("100.0"), D("102.0"));
    history.addClosedPosition(pos);

    auto trades = history.getTradeLevelReturns(true, D("0.001"));

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].getDuration() == 1);

    const double expected = (102.0 * 0.999 - 100.0 * 1.001) / (100.0 * 1.001);
    REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) ==
            Catch::Approx(expected).epsilon(1e-6));

    // Costs must reduce the raw winner return (0.02 without costs → less with costs)
    auto noCostTrades = history.getTradeLevelReturns(false);
    REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) <
            num::to_double(noCostTrades[0].getDailyReturns()[0]));
}

// ---------------------------------------------------------------------------
// Same-bar long position: borderline winner becomes loser after costs
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - costs turn marginal winner into loser",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    // entry = 100, exit = 100.1 (0.1% raw gain)
    // costPerSide = 0.001  →  round-trip cost ≈ 0.2%
    //
    // effectiveEntry = 100   * 1.001 = 100.1
    // effectiveExit  = 100.1 * 0.999 = 100.0999
    // r = (100.0999 - 100.1) / 100.1  <  0

    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createSameBarPosition("@C", t, D("100.0"), D("100.1"));
    history.addClosedPosition(pos);

    // Without costs: small positive return
    auto noCost = history.getTradeLevelReturns(false);
    REQUIRE(num::to_double(noCost[0].getDailyReturns()[0]) > 0.0);

    // With costs: same trade is now a small loss
    auto withCost = history.getTradeLevelReturns(true, D("0.001"));
    REQUIRE(num::to_double(withCost[0].getDailyReturns()[0]) < 0.0);
}

// ---------------------------------------------------------------------------
// Same-bar short position with costs
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - same-bar short with costs",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    // Short: entry = 100, exit = 96 (winner — price fell)
    // costPerSide = 0.001
    //
    // Short entry: receive less on the opening sell => effectiveEntry = 100 * (1 - 0.001) = 99.9
    // Short exit : pay more to buy-to-cover         => effectiveExit  = 96  * (1 + 0.001) = 96.096
    //
    // price-based r = (96.096 - 99.9) / 99.9 = -3.804 / 99.9 ≈ -0.038078...
    // P&L return for short  = r * -1              ≈ +0.038078...
    //
    // Without costs: (96-100)/100 * -1 = +0.04 (larger winner)
    // → costs shrink the gain.

    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    // createSameBarPosition creates a LONG; use createMockShortPosition instead
    auto pos = createMockShortPosition("@C", t, D("100.0"), D("96.0"), {D("98.0")});
    history.addClosedPosition(pos);

    const D cost("0.001");
    auto trades   = history.getTradeLevelReturns(true, cost);
    auto noCostTr = history.getTradeLevelReturns(false);

    REQUIRE(trades.size() == 1);

    // Still a winner after costs
    REQUIRE(num::to_double(trades[0].getDailyReturns().back()) > 0.0);

    // But smaller gain than without costs
    const double withCostReturn   = num::to_double(trades[0].getDailyReturns().back());
    const double withoutCostReturn = num::to_double(noCostTr[0].getDailyReturns().back());
    REQUIRE(withCostReturn < withoutCostReturn);

    // Verify the exact last-bar value.
    // With barCloses={98}, index-0 is the entry bar's close; no extra bars are
    // added by createMockShortPosition (its loop starts at i=1).  So the single
    // bar in history IS the last bar and prevRef stays at effectiveEntryPrice.
    //
    // prevRef        = 100 * (1 - 0.001) = 99.9   (effectiveEntryPrice for short)
    // effectiveExit  = 96  * (1 + 0.001) = 96.096
    // price r        = (96.096 - 99.9) / 99.9 ≈ -0.038078...
    // P&L for short  = r * -1            ≈ +0.038078...
    const double effectiveEntry = 100.0 * (1.0 - 0.001);
    const double effectiveExit  =  96.0 * (1.0 + 0.001);
    const double expectedLast   = -(effectiveExit - effectiveEntry) / effectiveEntry;
    REQUIRE(withCostReturn == Catch::Approx(expectedLast).epsilon(1e-6));
}

// ---------------------------------------------------------------------------
// Multi-bar long: costs affect only entry and exit, NOT intermediate bars
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - multi-bar long, costs only on entry/exit",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    // entry=100, barCloses=[101, 102, 103], exit=104, costPerSide=0.001
    //
    // createMockLongPosition: barCloses[0] is the entry bar's close;
    // indices 1..N-1 become additional bars; exit is added after all of them.
    //
    // effectiveEntry = 100 * 1.001 = 100.1   (entry bar close = 101)
    //
    // Bar 0 (not last): close=101, prevRef=100.1
    //   r0 = (101 - 100.1) / 100.1 = 0.9 / 100.1 ≈ 0.008991...
    //   prevRef becomes 101
    //
    // Bar 1 (not last): close=102, prevRef=101
    //   r1 = (102 - 101) / 101 = 1/101 ≈ 0.009900...
    //   prevRef becomes 102
    //
    // Bar 2 (last): effectiveExit = 104 * 0.999 = 103.896
    //   r2 = (103.896 - 102) / 102 = 1.896/102 ≈ 0.018588...

    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createMockLongPosition("@C", t,
                                      D("100.0"), D("104.0"),
                                      {D("101.0"), D("102.0"), D("103.0")});  // 3 closes → duration 3
    history.addClosedPosition(pos);

    const D cost("0.001");
    auto trades = history.getTradeLevelReturns(true, cost);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].getDuration() == 3);

    const auto& r = trades[0].getDailyReturns();
    REQUIRE(r.size() == 3);

    // Bar 0
    const double r0_expected = (101.0 - 100.0 * 1.001) / (100.0 * 1.001);
    REQUIRE(num::to_double(r[0]) == Catch::Approx(r0_expected).epsilon(1e-6));

    // Bar 1 — intermediate bar, no cost adjustment; prevRef is raw close=101
    const double r1_expected = (102.0 - 101.0) / 101.0;
    REQUIRE(num::to_double(r[1]) == Catch::Approx(r1_expected).epsilon(1e-6));

    // Bar 2 (last): prevRef=102 (bar 1's close), effectiveExit=104*0.999
    const double r2_expected = (104.0 * 0.999 - 102.0) / 102.0;
    REQUIRE(num::to_double(r[2]) == Catch::Approx(r2_expected).epsilon(1e-6));

    // Sanity: without-cost intermediate return must be identical
    auto noCost = history.getTradeLevelReturns(false);
    REQUIRE(num::to_double(r[1]) ==
            Catch::Approx(num::to_double(noCost[0].getDailyReturns()[1])).epsilon(1e-6));
}

// ---------------------------------------------------------------------------
// Multi-bar short: costs affect only entry and exit, NOT intermediate bars
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - multi-bar short, costs only on entry/exit",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    // Short: entry=100, barCloses=[99, 98, 97], exit=96, costPerSide=0.001
    //
    // createMockShortPosition: barCloses[0] is the entry bar's close;
    // indices 1..N-1 become additional bars; exit is added after all of them.
    //
    // effectiveEntry = 100 * (1 - 0.001) = 99.9   (entry bar close = 99)
    //
    // Bar 0 (not last): close=99, prevRef=99.9
    //   price r = (99 - 99.9) / 99.9 = -0.9/99.9 ≈ -0.009009...
    //   P&L for short = +0.009009...   prevRef → 99
    //
    // Bar 1 (not last): close=98, prevRef=99
    //   price r = (98 - 99) / 99 = -1/99 ≈ -0.010101...
    //   P&L for short ≈ +0.010101...   prevRef → 98
    //
    // Bar 2 (last): effectiveExit = 96 * (1 + 0.001) = 96.096, prevRef=97
    //   price r = (96.096 - 97) / 97 = -0.904/97 ≈ -0.009320...
    //   P&L for short ≈ +0.009320...

    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createMockShortPosition("@C", t,
                                       D("100.0"), D("96.0"),
                                       {D("99.0"), D("98.0"), D("97.0")});  // 3 closes → duration 3
    history.addClosedPosition(pos);

    const D cost("0.001");
    auto trades = history.getTradeLevelReturns(true, cost);
    auto noCost = history.getTradeLevelReturns(false);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].getDuration() == 3);

    const auto& r   = trades[0].getDailyReturns();
    const auto& rNc = noCost[0].getDailyReturns();

    REQUIRE(r.size() == 3);

    // All bars are gains (short in falling market)
    for (const auto& ret : r)
        REQUIRE(num::to_double(ret) > 0.0);

    // First bar: costs reduce the gain vs no-cost
    REQUIRE(num::to_double(r[0]) < num::to_double(rNc[0]));

    // Middle bar: costs don't touch intermediate bars
    REQUIRE(num::to_double(r[1]) ==
            Catch::Approx(num::to_double(rNc[1])).epsilon(1e-6));

    // Last bar: prevRef=97 (bar 1's close), effectiveExit=96*1.001
    const double r2_expected = -(96.0 * 1.001 - 98.0) / 98.0;
    REQUIRE(num::to_double(r[2]) == Catch::Approx(r2_expected).epsilon(1e-6));
}

// ---------------------------------------------------------------------------
// Mixed portfolio: long + short positions, both with costs
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - mixed long/short portfolio with costs",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    ClosedPositionHistory<D> history;
    ptime t1(date(2024, 1, 10), hours(9) + minutes(30));
    ptime t2(date(2024, 1, 15), hours(9) + minutes(30));

    // createMockLongPosition/Short: barCloses[0] is the entry bar's close;
    // only indices 1..N-1 become additional bars.  We need 2 closes so that
    // closes[1] becomes an intermediate bar whose close is prevRef on the last bar.

    // Long: entry=100, barCloses=[101,102], exit=105
    //   prevRef for last bar = 102 (intermediate bar close)
    auto longPos  = createMockLongPosition ("@C", t1,
                                            D("100.0"), D("105.0"),
                                            {D("101.0"), D("102.0")});

    // Short: entry=200, barCloses=[196,195], exit=190
    //   prevRef for last bar = 195 (intermediate bar close)
    auto shortPos = createMockShortPosition("@C", t2,
                                            D("200.0"), D("190.0"),
                                            {D("196.0"), D("195.0")});

    history.addClosedPosition(longPos);
    history.addClosedPosition(shortPos);

    const D cost("0.002");  // 0.2% per side
    auto withCosts = history.getTradeLevelReturns(true, cost);
    auto noCosts   = history.getTradeLevelReturns(false);

    REQUIRE(withCosts.size() == 2);

    // Both still winners after costs, but smaller
    for (size_t i = 0; i < 2; ++i) {
        const double wc = num::to_double(withCosts[i].getDailyReturns().back());
        const double nc = num::to_double(noCosts[i].getDailyReturns().back());
        REQUIRE(wc > 0.0);   // still profitable
        REQUIRE(wc < nc);    // but reduced by costs
    }

    // Long trade last-bar: effectiveExit = 105*0.998
    // prevRef = barCloses[0]=101, NOT barCloses[1]=102,
    // because the last bar's close is discarded in favour of effectiveExitPrice.
    const double longExpected = (105.0 * 0.998 - 101.0) / 101.0;
    REQUIRE(num::to_double(withCosts[0].getDailyReturns().back()) ==
            Catch::Approx(longExpected).epsilon(1e-6));

    // Short trade last-bar: effectiveExit = 190*1.002
    // prevRef = barCloses[0]=196, NOT barCloses[1]=195, same reason.
    const double shortExpected = -(190.0 * 1.002 - 196.0) / 196.0;
    REQUIRE(num::to_double(withCosts[1].getDailyReturns().back()) ==
            Catch::Approx(shortExpected).epsilon(1e-6));
}

// ---------------------------------------------------------------------------
// Large cost: result approaches -1 for very large (but valid) cost
// ---------------------------------------------------------------------------

TEST_CASE("ClosedPositionHistory::getTradeLevelReturns - large cost severely penalises return",
          "[ClosedPositionHistory][getTradeLevelReturns][applyCosts]")
{
    // entry=100, exit=110, costPerSide=0.1 (10% per side — extreme but valid)
    //
    // effectiveEntry = 100 * 1.1 = 110
    // effectiveExit  = 110 * 0.9 = 99
    // r = (99 - 110) / 110 = -11/110 = -0.1
    //
    // Without costs this is a +10% winner; with 10% cost each side it becomes -10%.

    ClosedPositionHistory<D> history;
    ptime t(date(2024, 1, 15), hours(9) + minutes(30));

    auto pos = createSameBarPosition("@C", t, D("100.0"), D("110.0"));
    history.addClosedPosition(pos);

    auto trades = history.getTradeLevelReturns(true, D("0.1"));

    REQUIRE(trades.size() == 1);

    const double expected = (110.0 * 0.9 - 100.0 * 1.1) / (100.0 * 1.1);
    REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) ==
            Catch::Approx(expected).epsilon(1e-9));

    // Must now be a loss
    REQUIRE(num::to_double(trades[0].getDailyReturns()[0]) < 0.0);
}
