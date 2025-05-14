#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "PalStrategy.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

const static std::string myCornSymbol("@C");

PatternDescription *
createDescription (const std::string& fileName, unsigned int index, unsigned long indexDate, 
		   const std::string& percLong, const std::string& percShort,
		   unsigned int numTrades, unsigned int consecutiveLosses)
{
  DecimalType *percentLong = createRawDecimalPtr (percLong);
  DecimalType *percentShort = createRawDecimalPtr(percShort);

  return new PatternDescription ((char *) fileName.c_str(), index, indexDate, percentLong, percentShort,
				 numTrades, consecutiveLosses);
}

LongMarketEntryOnOpen *
createLongOnOpen()
{
  return new LongMarketEntryOnOpen();
}

ShortMarketEntryOnOpen *
createShortOnOpen()
{
  return new ShortMarketEntryOnOpen();
}

LongSideProfitTargetInPercent *
createLongProfitTarget(const std::string& targetPct);

LongSideStopLossInPercent *
createLongStopLoss(const std::string& targetPct);

ShortSideProfitTargetInPercent *
createShortProfitTarget(const std::string& targetPct);

ShortSideStopLossInPercent *
createShortStopLoss(const std::string& targetPct);

std::shared_ptr<PriceActionLabPattern>
createShortPattern1()
{
  PatternDescription *desc = createDescription(std::string("C2_122AR.txt"), 39, 
					       20111017, std::string("90.00"),
					       std::string("10.00"), 21, 2);
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
  auto shortPattern1 = new AndExpr (shortand1, shortand3);

  MarketEntryExpression *entry = createShortOnOpen();
  ProfitTargetInPercentExpression *target = createShortProfitTarget("1.34");
  StopLossInPercentExpression *stop = createShortStopLoss("1.28");

  return std::make_shared<PriceActionLabPattern>(desc, shortPattern1, entry, target, stop);
}

std::shared_ptr<PriceActionLabPattern>
createLongPattern1()
{
  PatternDescription *desc = createDescription(std::string("C2_122AR.txt"), 39, 
					       20131217, std::string("90.00"),
					       std::string("10.00"), 21, 2);

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
  auto longPattern1 = new AndExpr (and1, and3);
  MarketEntryExpression *entry = createLongOnOpen();
  ProfitTargetInPercentExpression *target = createLongProfitTarget("2.56");
  StopLossInPercentExpression *stop = createLongStopLoss("1.28");

  // 2.56 profit target in points = 93.81
  return std::make_shared<PriceActionLabPattern>(desc, longPattern1, entry, target, stop);
} 

std::shared_ptr<PriceActionLabPattern>
createLongPattern2()
{
  PatternDescription *desc = createDescription(std::string("C2_122AR.txt"), 106, 
					       20110106, std::string("53.33"),
					       std::string("46.67"), 45, 3);

    auto high4 = new PriceBarHigh(4);
    auto high5 = new PriceBarHigh(5);
    auto high6 = new PriceBarHigh(6);
    auto low4 = new PriceBarLow(4);
    auto low5 = new PriceBarLow(5);
    auto low6 = new PriceBarLow(6);
    auto close1 = new PriceBarClose(1);

    auto gt1 = new GreaterThanExpr (high4, high5);
    auto gt2 = new GreaterThanExpr (high5, high6);
    auto gt3 = new GreaterThanExpr (high6, low4);
    auto gt4 = new GreaterThanExpr (low4, low5);
    auto gt5 = new GreaterThanExpr (low5, low6);
    auto gt6 = new GreaterThanExpr (low6, close1);

    auto and1 = new AndExpr (gt1, gt2);
    auto and2 = new AndExpr (gt3, gt4);
    auto and3 = new AndExpr (gt5, gt6);
    auto and4 = new AndExpr (and1, and2);
    auto longPattern1 = new AndExpr (and4, and3);

    MarketEntryExpression *entry = createLongOnOpen();
    ProfitTargetInPercentExpression *target = createLongProfitTarget("5.12");
    StopLossInPercentExpression *stop = createLongStopLoss("2.56");
  
   return std::make_shared<PriceActionLabPattern>(desc, longPattern1, entry, target, stop);
}

std::shared_ptr<PriceActionLabPattern>
createLongPattern3()
{
  PatternDescription *desc = createDescription(std::string("C2_122AR.txt"), 106, 
					       20110106, std::string("53.33"),
					       std::string("46.67"), 45, 3);

    auto low0 = new PriceBarLow(0);
    auto low1 = new PriceBarLow(1);
    auto close1 = new PriceBarClose(1);
    auto close0 = new PriceBarClose(0);

    auto gt1 = new GreaterThanExpr (close0, close1);
    auto gt2 = new GreaterThanExpr (low0, low1);


    auto longPattern1 = new AndExpr (gt1, gt2);

    MarketEntryExpression *entry = createLongOnOpen();
    ProfitTargetInPercentExpression *target = createLongProfitTarget("5.12");
    StopLossInPercentExpression *stop = createLongStopLoss("2.56");
  
   return std::make_shared<PriceActionLabPattern>(desc, longPattern1, entry, target, stop);
}


void printPositionHistory(const ClosedPositionHistory<DecimalType>& history);

void backTestLoop(std::shared_ptr<Security<DecimalType>> security, BacktesterStrategy<DecimalType>& strategy, 
		  TimeSeriesDate& backTestStartDate, TimeSeriesDate& backTestEndDate)
{
  TimeSeriesDate backTesterDate(backTestStartDate);

  TimeSeriesDate orderDate;
  for (; (backTesterDate <= backTestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
    {
      orderDate = boost_previous_weekday (backTesterDate);
      if (strategy.doesSecurityHaveTradingData (*security, orderDate))
	{
	  strategy.eventUpdateSecurityBarNumber(security->getSymbol());
	  if (strategy.isShortPosition (security->getSymbol()) || strategy.isLongPosition (security->getSymbol()))
	    strategy.eventExitOrders (security.get(), 
				      strategy.getInstrumentPosition(security->getSymbol()),
				      orderDate);
	  strategy.eventEntryOrders(security.get(), 
				    strategy.getInstrumentPosition(security->getSymbol()),
				    orderDate);
	  
	}
      strategy.eventProcessPendingOrders (backTesterDate);
    }
}


TEST_CASE ("PalStrategy operations", "[PalStrategy]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();

  std::string futuresSymbol("@C");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));

  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol, 
						   futuresName, 
						   cornBigPointValue,
						   cornTickValue, 
						   p);

  std::string portName("Corn Portfolio");
  auto aPortfolio = std::make_shared<Portfolio<DecimalType>>(portName);

  aPortfolio->addSecurity (corn);

  std::string strategy1Name("PAL Long Strategy 1");

  PalLongStrategy<DecimalType> longStrategy1(strategy1Name,
				   createLongPattern1(), 
				   aPortfolio);
  REQUIRE (longStrategy1.getPatternMaxBarsBack() == 8);
  REQUIRE (longStrategy1.getSizeForOrder (*corn) == oneContract);
  REQUIRE (longStrategy1.isFlatPosition (futuresSymbol));
  REQUIRE_FALSE (longStrategy1.isLongPosition (futuresSymbol));
  REQUIRE_FALSE (longStrategy1.isShortPosition (futuresSymbol));
  REQUIRE (longStrategy1.getStrategyName() == strategy1Name);
  REQUIRE_FALSE (longStrategy1.isPyramidingEnabled());
  REQUIRE (longStrategy1.getMaxPyramidPositions() == 0);
  REQUIRE_FALSE (longStrategy1.strategyCanPyramid(futuresSymbol));

  REQUIRE (longStrategy1.doesSecurityHaveTradingData (*corn, createDate("19850301")));
  REQUIRE (longStrategy1.doesSecurityHaveTradingData (*corn, createDate("20011116")));
  REQUIRE_FALSE (longStrategy1.doesSecurityHaveTradingData (*corn, createDate("19850227")));

  PalShortStrategy<DecimalType> shortStrategy1("PAL Short Strategy 1", createShortPattern1(), aPortfolio);
  REQUIRE (shortStrategy1.getPatternMaxBarsBack() == 5);
  REQUIRE (shortStrategy1.getSizeForOrder (*corn) == oneContract);
  REQUIRE (shortStrategy1.isFlatPosition (futuresSymbol));
  REQUIRE_FALSE (shortStrategy1.isLongPosition (futuresSymbol));
  REQUIRE_FALSE (shortStrategy1.isShortPosition (futuresSymbol));
  REQUIRE_FALSE (shortStrategy1.isPyramidingEnabled());
  REQUIRE (shortStrategy1.getMaxPyramidPositions() == 0);
  REQUIRE_FALSE (shortStrategy1.strategyCanPyramid(futuresSymbol));

  REQUIRE (shortStrategy1.doesSecurityHaveTradingData (*corn, createDate("19850301")));
  REQUIRE (shortStrategy1.doesSecurityHaveTradingData (*corn, createDate("20011116")));
  REQUIRE_FALSE (shortStrategy1.doesSecurityHaveTradingData (*corn, createDate("19850227")));

  PalLongStrategy<DecimalType> longStrategy2("PAL Long Strategy 2", createLongPattern2(), aPortfolio);
  REQUIRE (longStrategy2.getPatternMaxBarsBack() == 6);
  REQUIRE (longStrategy2.getSizeForOrder (*corn) == oneContract);
  REQUIRE (longStrategy2.isFlatPosition (futuresSymbol));
  REQUIRE_FALSE (longStrategy2.isLongPosition (futuresSymbol));
  REQUIRE_FALSE (longStrategy2.isShortPosition (futuresSymbol));

  StrategyOptions enablePyramid(true, 2);

  PalLongStrategy<DecimalType> longStrategyPyramid1(strategy1Name,
						    createLongPattern3(), 
						    aPortfolio,
						    enablePyramid);
  REQUIRE (longStrategyPyramid1.getPatternMaxBarsBack() == 1);
  REQUIRE (longStrategyPyramid1.getSizeForOrder (*corn) == oneContract);
  REQUIRE (longStrategyPyramid1.isFlatPosition (futuresSymbol));
  REQUIRE_FALSE (longStrategyPyramid1.isLongPosition (futuresSymbol));
  REQUIRE_FALSE (longStrategyPyramid1.isShortPosition (futuresSymbol));
  REQUIRE (longStrategyPyramid1.getStrategyName() == strategy1Name);

  REQUIRE (longStrategyPyramid1.isPyramidingEnabled() == true);
  REQUIRE (longStrategyPyramid1.getMaxPyramidPositions() == 2);

  std::string metaStrategy1Name("PAL Meta Strategy 1");
  PalMetaStrategy<DecimalType> metaStrategy1(metaStrategy1Name, aPortfolio);
  metaStrategy1.addPricePattern(createLongPattern1());

  REQUIRE (metaStrategy1.getSizeForOrder (*corn) == oneContract);
  REQUIRE (metaStrategy1.isFlatPosition (futuresSymbol));
  REQUIRE_FALSE (metaStrategy1.isLongPosition (futuresSymbol));
  REQUIRE_FALSE (metaStrategy1.isShortPosition (futuresSymbol));
  REQUIRE (metaStrategy1.getStrategyName() == metaStrategy1Name);

  std::string metaStrategy2Name("PAL Meta Strategy 2");
  PalMetaStrategy<DecimalType> metaStrategy2(metaStrategy2Name, aPortfolio);
  metaStrategy2.addPricePattern(createShortPattern1());

  REQUIRE (metaStrategy2.getSizeForOrder (*corn) == oneContract);
  REQUIRE (metaStrategy2.isFlatPosition (futuresSymbol));
  REQUIRE_FALSE (metaStrategy2.isLongPosition (futuresSymbol));
  REQUIRE_FALSE (metaStrategy2.isShortPosition (futuresSymbol));
  REQUIRE (metaStrategy2.getStrategyName() == metaStrategy2Name);

  std::string metaStrategy3Name("PAL Meta Strategy 3");
  PalMetaStrategy<DecimalType> metaStrategy3(metaStrategy3Name, aPortfolio);
  metaStrategy3.addPricePattern(createLongPattern1());
  metaStrategy3.addPricePattern(createShortPattern1());

  SECTION ("PalStrategy testing for long pattern not matched") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 1));
    TimeSeriesDate endDate(TimeSeriesDate (1985, Nov, 14));

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	if (longStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    longStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    longStrategy1.eventEntryOrders(corn.get(), 
					   longStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    REQUIRE ( longStrategy1.isFlatPosition (futuresSymbol));
	  }
      }

    REQUIRE (orderDate == TimeSeriesDate (1985, Nov, 15));
    if (longStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
      {
	longStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	longStrategy1.eventEntryOrders(corn.get(), 
				       longStrategy1.getInstrumentPosition(futuresSymbol),
				       orderDate);
      }

    orderDate = boost_next_weekday(orderDate);
    longStrategy1.eventProcessPendingOrders(orderDate);
    REQUIRE ( longStrategy1.isLongPosition (futuresSymbol));

    StrategyBroker<DecimalType> aBroker = longStrategy1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 1);
    REQUIRE (aBroker.getClosedTrades() == 0);
  }

  SECTION ("PalStrategy testing for short pattern not matched") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 1));
    TimeSeriesDate endDate(TimeSeriesDate (1986, May, 27));

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	if (shortStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    shortStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    shortStrategy1.eventEntryOrders(corn.get(), 
					   shortStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    REQUIRE ( shortStrategy1.isFlatPosition (futuresSymbol));
	  }
      }

    REQUIRE (orderDate == TimeSeriesDate (1986, May, 28));
    if (shortStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
      {
	shortStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	shortStrategy1.eventEntryOrders(corn.get(), 
				       shortStrategy1.getInstrumentPosition(futuresSymbol),
				       orderDate);
      }

    orderDate = boost_next_weekday(orderDate);
    shortStrategy1.eventProcessPendingOrders(orderDate);
    REQUIRE ( shortStrategy1.isShortPosition (futuresSymbol));
    StrategyBroker<DecimalType> aBroker = shortStrategy1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 1);
    REQUIRE (aBroker.getClosedTrades() == 0);
  }

  SECTION ("PalStrategy testing for long with profit target exit") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 1));
    TimeSeriesDate endDate(TimeSeriesDate (1985, Nov, 15));

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	if (longStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    longStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    longStrategy1.eventEntryOrders(corn.get(), 
					   longStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);

	  }
      }

    //orderDate = boost_next_weekday(orderDate);
    REQUIRE (orderDate ==TimeSeriesDate (1985, Nov, 18)); 
    longStrategy1.eventProcessPendingOrders(orderDate);
    REQUIRE ( longStrategy1.isLongPosition (futuresSymbol));
    
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Nov, 19));
    TimeSeriesDate position1EndDate(TimeSeriesDate (1985, Dec, 4));

    for (; (backTesterDate <= position1EndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (longStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    longStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (longStrategy1.isLongPosition (futuresSymbol))
	      longStrategy1.eventExitOrders (corn.get(), 
					     longStrategy1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    longStrategy1.eventEntryOrders(corn.get(), 
					   longStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	   

	    longStrategy1.eventProcessPendingOrders (backTesterDate);
	    if (backTesterDate !=  position1EndDate)
	      REQUIRE (longStrategy1.isLongPosition(futuresSymbol)); 
	  }
      }

    StrategyBroker<DecimalType> aBroker = longStrategy1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 1);

    StrategyBroker<DecimalType>::StrategyTransactionIterator it = 
      aBroker.beginStrategyTransactions();

    REQUIRE (it !=aBroker.endStrategyTransactions()); 
    auto trans = it->second;
    REQUIRE (trans->isTransactionComplete());
    auto entryOrder = trans->getEntryTradingOrder();
    auto aPosition = trans->getTradingPosition();
    auto exitOrder = trans->getExitTradingOrder();
    REQUIRE (entryOrder->getFillDate() == TimeSeriesDate (1985, Nov, 18));
    REQUIRE (aPosition->getEntryDate() == TimeSeriesDate (1985, Nov, 18));
    REQUIRE (aPosition->getExitDate() == TimeSeriesDate (1985, Dec, 4));
    REQUIRE (exitOrder->getFillDate() == TimeSeriesDate (1985, Dec, 4));
    it++;
    REQUIRE (it == aBroker.endStrategyTransactions()); 
  }

  SECTION ("PalStrategy testing for short with profit target exit") 
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Mar, 1));
    TimeSeriesDate endDate(TimeSeriesDate (1986, May, 28));

    for (; (orderDate <= endDate); orderDate = boost_next_weekday(orderDate))
      {
	if (shortStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    shortStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    shortStrategy1.eventEntryOrders(corn.get(), 
					   shortStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);

	  }
      }

    //orderDate = boost_next_weekday(orderDate);
    REQUIRE (orderDate == TimeSeriesDate (1986, May, 29)); 
    shortStrategy1.eventProcessPendingOrders(orderDate);
    REQUIRE ( shortStrategy1.isShortPosition (futuresSymbol));
    
    TimeSeriesDate backTesterDate(TimeSeriesDate (1986, May, 30));
    TimeSeriesDate position1EndDate(TimeSeriesDate (1986, Jun, 11));

    for (; (backTesterDate <= position1EndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (shortStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    shortStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (shortStrategy1.isShortPosition (futuresSymbol))
	      shortStrategy1.eventExitOrders (corn.get(), 
					     shortStrategy1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    shortStrategy1.eventEntryOrders(corn.get(), 
					   shortStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	   

	    shortStrategy1.eventProcessPendingOrders (backTesterDate);
	    if (backTesterDate !=  position1EndDate)
	      REQUIRE (shortStrategy1.isShortPosition(futuresSymbol)); 
	  }
      }

    StrategyBroker<DecimalType> aBroker = shortStrategy1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 1);

    StrategyBroker<DecimalType>::StrategyTransactionIterator it = 
      aBroker.beginStrategyTransactions();

    REQUIRE (it !=aBroker.endStrategyTransactions()); 
    auto trans = it->second;
    REQUIRE (trans->isTransactionComplete());
    auto entryOrder = trans->getEntryTradingOrder();
    auto aPosition = trans->getTradingPosition();
    auto exitOrder = trans->getExitTradingOrder();
    REQUIRE (entryOrder->getFillDate() == TimeSeriesDate (1986, May, 29));
    REQUIRE (aPosition->getEntryDate() == TimeSeriesDate (1986, May, 29));
    REQUIRE (aPosition->getExitDate() == TimeSeriesDate (1986, Jun, 11));
    REQUIRE (exitOrder->getFillDate() == TimeSeriesDate (1986, Jun, 11));
    it++;
    REQUIRE (it == aBroker.endStrategyTransactions()); 
  }

SECTION ("PalStrategy testing for all long trades - pattern 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2008, Dec, 31));

    TimeSeriesDate orderDate;
    for (; (backTesterDate <= backtestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (longStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    longStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (longStrategy1.isLongPosition (futuresSymbol))
	      longStrategy1.eventExitOrders (corn.get(), 
					     longStrategy1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    longStrategy1.eventEntryOrders(corn.get(), 
					   longStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    
	  }
	longStrategy1.eventProcessPendingOrders (backTesterDate);
      }

    StrategyBroker<DecimalType> aBroker = longStrategy1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 24);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 24); 

    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    //printPositionHistory (history);

    REQUIRE (history.getNumWinningPositions() == 13);
    REQUIRE (history.getNumLosingPositions() == 11);
 
  }

SECTION ("PalStrategy testing for all long trades - MetaStrategy1 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2008, Dec, 31));

    TimeSeriesDate orderDate;
    for (; (backTesterDate <= backtestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (metaStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    metaStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (metaStrategy1.isLongPosition (futuresSymbol))
	      metaStrategy1.eventExitOrders (corn.get(), 
					     metaStrategy1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    metaStrategy1.eventEntryOrders(corn.get(), 
					   metaStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    
	  }
	metaStrategy1.eventProcessPendingOrders (backTesterDate);
      }

    StrategyBroker<DecimalType> aBroker = metaStrategy1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 24);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 24); 

    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    //printPositionHistory (history);

    REQUIRE (history.getNumWinningPositions() == 13);
    REQUIRE (history.getNumLosingPositions() == 11);
 
  }

SECTION ("PalStrategy testing for all long trades with pyramiding - pattern 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2008, Dec, 31));

    TimeSeriesDate orderDate;
    for (; (backTesterDate <= backtestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (longStrategyPyramid1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    longStrategyPyramid1.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (longStrategyPyramid1.isLongPosition (futuresSymbol))
	      longStrategyPyramid1.eventExitOrders (corn.get(), 
					     longStrategyPyramid1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    longStrategyPyramid1.eventEntryOrders(corn.get(), 
					   longStrategyPyramid1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    
	  }
	longStrategyPyramid1.eventProcessPendingOrders (backTesterDate);
      }

    StrategyBroker<DecimalType> aBroker = longStrategyPyramid1.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() > 546);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() > 546); 

    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    printPositionHistory (history);

    //REQUIRE (history.getNumWinningPositions() == 13);
    //REQUIRE (history.getNumLosingPositions() == 11);
 
  }

  //
  
SECTION ("PalStrategy testing for all long trades - pattern 2") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2011, Oct, 27));

    TimeSeriesDate orderDate;
    for (; (backTesterDate <= backtestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (longStrategy2.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    longStrategy2.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (longStrategy2.isLongPosition (futuresSymbol))
	      longStrategy2.eventExitOrders (corn.get(), 
					     longStrategy2.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    longStrategy2.eventEntryOrders(corn.get(), 
					   longStrategy2.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    
	  }
	longStrategy2.eventProcessPendingOrders (backTesterDate);
      }

    StrategyBroker<DecimalType> aBroker = longStrategy2.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 45);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 45); 

    //ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    //printPositionHistory (history);

    //REQUIRE (history.getNumWinningPositions() == 13);
    //REQUIRE (history.getNumLosingPositions() == 11);
 
  }

  // 
SECTION ("PalStrategy testing for all short trades") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2011, Sep, 15));

    TimeSeriesDate orderDate;
    for (; (backTesterDate <= backtestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (shortStrategy1.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    shortStrategy1.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (shortStrategy1.isShortPosition (futuresSymbol))
	      shortStrategy1.eventExitOrders (corn.get(), 
					     shortStrategy1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    shortStrategy1.eventEntryOrders(corn.get(), 
					   shortStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	   

	    
	  }
	shortStrategy1.eventProcessPendingOrders (backTesterDate);
      }

    //std::cout << "Backtester end date = " << backTesterDate << std::endl; 
    StrategyBroker<DecimalType> aBroker2 = shortStrategy1.getStrategyBroker();
    ClosedPositionHistory<DecimalType> history2 = aBroker2.getClosedPositionHistory();
    //std::cout << "Calling printPositionHistory for short strategy" << std::endl << std::endl;
    //printPositionHistory (history2);

    REQUIRE (aBroker2.getTotalTrades() == 21);
    REQUIRE (aBroker2.getOpenTrades() == 0);
    REQUIRE (aBroker2.getClosedTrades() == 21); 

   

    REQUIRE (history2.getNumWinningPositions() == 15);
    REQUIRE (history2.getNumLosingPositions() == 6);
 
  }    

SECTION ("PalStrategy testing for all short trades - MetaStrategy2") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2011, Sep, 15));

    TimeSeriesDate orderDate;
    for (; (backTesterDate <= backtestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
      {
	orderDate = boost_previous_weekday (backTesterDate);
	if (metaStrategy2.doesSecurityHaveTradingData (*corn, orderDate))
	  {
	    metaStrategy2.eventUpdateSecurityBarNumber(futuresSymbol);
	    if (metaStrategy2.isShortPosition (futuresSymbol))
	      metaStrategy2.eventExitOrders (corn.get(), 
					     metaStrategy2.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    metaStrategy2.eventEntryOrders(corn.get(), 
					   metaStrategy2.getInstrumentPosition(futuresSymbol),
					   orderDate);
	    
	  }
	metaStrategy2.eventProcessPendingOrders (backTesterDate);
      }

    StrategyBroker<DecimalType> aBroker = metaStrategy2.getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 21);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 21); 

    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    //printPositionHistory (history);

    REQUIRE (history.getNumWinningPositions() == 15);
    REQUIRE (history.getNumLosingPositions() == 6);
 
  }

SECTION ("PalStrategy testing for all trades - MetaStrategy3") 
  {
    TimeSeriesDate backTestStartDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backTestEndDate(TimeSeriesDate (2008, Dec, 31));

    backTestLoop (corn, metaStrategy3, backTestStartDate, backTestEndDate);

    StrategyBroker<DecimalType> aBroker = metaStrategy3.getStrategyBroker();
    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    printPositionHistory (history);

    int twoPatternTotalTrades = aBroker.getTotalTrades();

    REQUIRE (aBroker.getTotalTrades() > 24);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() > 24); 

    //ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    //printPositionHistory (history);

    //REQUIRE (history.getNumWinningPositions() == 13);
    //REQUIRE (history.getNumLosingPositions() == 11);

    std::string metaStrategy4Name("PAL Meta Strategy 4");
    PalMetaStrategy<DecimalType> metaStrategy4(metaStrategy4Name, aPortfolio);
    metaStrategy4.addPricePattern(createLongPattern1());
    metaStrategy4.addPricePattern(createLongPattern2());
    metaStrategy4.addPricePattern(createShortPattern1());
 
    backTestLoop (corn, metaStrategy4, backTestStartDate, backTestEndDate);
 
    StrategyBroker<DecimalType> aBroker2 = metaStrategy4.getStrategyBroker();
    ClosedPositionHistory<DecimalType> history2 = aBroker2.getClosedPositionHistory();
    printPositionHistory (history2);

    int threePatternTotalTrades = aBroker2.getTotalTrades();
    REQUIRE (threePatternTotalTrades > twoPatternTotalTrades);

    StrategyOptions stratOptions(true, 2);  // Enable pyramiding
    std::string metaStrategy5Name("PAL Meta Strategy 5");
    PalMetaStrategy<DecimalType> metaStrategy5(metaStrategy5Name, aPortfolio, stratOptions);
    metaStrategy5.addPricePattern(createLongPattern1());
    metaStrategy5.addPricePattern(createLongPattern2());
    metaStrategy5.addPricePattern(createShortPattern1());

    backTestLoop (corn, metaStrategy5, backTestStartDate, backTestEndDate);
    StrategyBroker<DecimalType> aBroker3 = metaStrategy5.getStrategyBroker();
    ClosedPositionHistory<DecimalType> history3 = aBroker3.getClosedPositionHistory();
    printPositionHistory (history3);

    REQUIRE (aBroker3.getTotalTrades() > threePatternTotalTrades);
  }

}

