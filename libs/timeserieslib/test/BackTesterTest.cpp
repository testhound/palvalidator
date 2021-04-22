#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../PalStrategy.h"
#include "../BoostDateHelper.h"
#include "../BackTester.h"
#include "../DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

std::string myCornSymbol("@C");


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
createLongProfitTarget(const std::string& targetPct)
{
  return new LongSideProfitTargetInPercent (createRawDecimalPtr (targetPct));
}

LongSideStopLossInPercent *
createLongStopLoss(const std::string& targetPct)
{
  return new LongSideStopLossInPercent (createRawDecimalPtr (targetPct));
}

ShortSideProfitTargetInPercent *
createShortProfitTarget(const std::string& targetPct)
{
  return new ShortSideProfitTargetInPercent (createRawDecimalPtr (targetPct));
}

ShortSideStopLossInPercent *
createShortStopLoss(const std::string& targetPct)
{
  return new ShortSideStopLossInPercent (createRawDecimalPtr (targetPct));
}

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
  //ProfitTargetInPercentExpression *target = createLongProfitTarget("2.56");
  //StopLossInPercentExpression *stop = createLongStopLoss("1.28");

  ProfitTargetInPercentExpression *target = createLongProfitTarget("0.32");
  StopLossInPercentExpression *stop = createLongStopLoss("0.16");

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


void printPositionHistorySummary(const ClosedPositionHistory<DecimalType>& history)
{
  std::cout << "In printPositionHistorySummary" << std::endl;
  std::cout << "Number of positions = " << history.getNumPositions() << std::endl << std::endl;
  std::cout << "PAL Profitability = " << history.getPALProfitability() << std::endl;
  std::cout << "Profit factor = " << history.getProfitFactor() << std::endl;
  std::cout << "Payoff ratio = " << history.getPayoffRatio() << std::endl;
}

void printPositionHistory(const ClosedPositionHistory<DecimalType>& history)
{
  ClosedPositionHistory<DecimalType>::ConstPositionIterator it = history.beginTradingPositions();
  std::shared_ptr<TradingPosition<DecimalType>> p;
  std::string posStateString;
  std::string openStr("Position open");
  std::string closedStr("Position closed");
  std::string dirStrLong("Long");
  std::string dirStrShort("Short");
  std::string dirStr;

  int positionNum = 1;
  int numWinners = 0;
  int numLosers = 0;

  std::cout << "In printPositionHistory" << std::endl;
  std::cout << "Number of positions = " << history.getNumPositions() << std::endl << std::endl;

  for (; it != history.endTradingPositions(); it++)
    {
      p = it->second;
      if (p->isPositionOpen())
	posStateString = openStr;
      else
	posStateString = closedStr;

      if (p->isLongPosition())
	dirStr = dirStrLong;
      else
	dirStr = dirStrShort;

      std::cout << "Position # " << positionNum << ", " << dirStr << " position state: " << posStateString << std::endl;
      std::cout << "Position entry date: " << p->getEntryDate() << " entry price: " << 
	p->getEntryPrice() << std::endl;

      if (p->isPositionClosed())
	{
	  std::cout << "Position exit date: " << p->getExitDate() << " exit price: " << 
	    p->getExitPrice() << std::endl;
	  if (p->RMultipleStopSet())
	    {
	      std::cout << "Position R stop: " << p->getRMultipleStop() << std::endl;
	      std::cout << "Position R multiple: " << p->getRMultiple() << std::endl;
	    }
	}

      if (p->isWinningPosition())
	{
	  std::cout << "Winning position!" << std::endl << std::endl;;
	  numWinners++;
	}
      else
	{
	  std::cout << "Losing position @#$%" << std::endl << std::endl;
	  numLosers++;
	}

      positionNum++;
      
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

 
  std::shared_ptr<PalLongStrategy<DecimalType>> longStrategy1 = 
    std::make_shared<PalLongStrategy<DecimalType>>(strategy1Name, createLongPattern1(), 
					 aPortfolio);

  PalShortStrategy<DecimalType> shortStrategy1("PAL Short Strategy 1", createShortPattern1(), aPortfolio);
 

  std::shared_ptr<PalLongStrategy<DecimalType>> longStrategy2 = 
    std::make_shared<PalLongStrategy<DecimalType>>("PAL Long Strategy 2", createLongPattern2(), 
					 aPortfolio);

SECTION ("PalStrategy testing for all long trades - pattern 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2011, Oct, 27));

    DailyBackTester<DecimalType> palLongBacktester1(backTesterDate,
				       backtestEndDate);

    palLongBacktester1.addStrategy(longStrategy1);
    REQUIRE (palLongBacktester1.getStartDate() == backTesterDate);
    REQUIRE (palLongBacktester1.getEndDate() == backtestEndDate);

    palLongBacktester1.backtest();

    BackTester<DecimalType>::StrategyIterator it = palLongBacktester1.beginStrategies();

    REQUIRE (it != palLongBacktester1.endStrategies());
    std::shared_ptr<BacktesterStrategy<DecimalType>> aStrategy1 = (*it);

    StrategyBroker<DecimalType> aBroker = aStrategy1->getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 24);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 24); 

    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();
    printPositionHistorySummary (history);
    printPositionHistory (history);

    REQUIRE (history.getNumWinningPositions() == 16);
    REQUIRE (history.getNumLosingPositions() == 8);

    DecimalType rMultiple = history.getRMultipleExpectancy();
    REQUIRE (rMultiple > DecimalConstants<DecimalType>::DecimalZero);
    std::cout << "RMultiple for longStrategy1 = " << rMultiple << std::endl << std::endl;;
  }


  
SECTION ("PalStrategy testing for all long trades - pattern 2") 
  {
    std::cout << "In second long pattern backtest" << std::endl;

    TimeSeriesDate backTesterDate(TimeSeriesDate (1985, Mar, 19));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2011, Oct, 27));

    DailyBackTester<DecimalType> palLongBacktester2(backTesterDate,
					  backtestEndDate);

    palLongBacktester2.addStrategy(longStrategy2);
    REQUIRE (palLongBacktester2.getStartDate() == backTesterDate);
    REQUIRE (palLongBacktester2.getEndDate() == backtestEndDate);

    palLongBacktester2.backtest();

    BackTester<DecimalType>::StrategyIterator it = palLongBacktester2.beginStrategies();

    REQUIRE (it != palLongBacktester2.endStrategies());
    std::shared_ptr<BacktesterStrategy<DecimalType>> aStrategy2 = (*it);

    StrategyBroker<DecimalType> aBroker2 = aStrategy2->getStrategyBroker();
    REQUIRE (aBroker2.getTotalTrades() == 45);
    REQUIRE (aBroker2.getOpenTrades() == 0);
    REQUIRE (aBroker2.getClosedTrades() == 45); 

    ClosedPositionHistory<DecimalType> history = aBroker2.getClosedPositionHistory();
    //    printPositionHistory (history);
    DecimalType rMultiple = history.getRMultipleExpectancy();
    REQUIRE (rMultiple > DecimalConstants<DecimalType>::DecimalZero);
    std::cout << "RMultiple for longStrategy1 = " << rMultiple << std::endl << std::endl;;
  }

/* 
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
	      shortStrategy1.eventExitOrders (corn, 
					     shortStrategy1.getInstrumentPosition(futuresSymbol),
					     orderDate);
	    shortStrategy1.eventEntryOrders(corn, 
					   shortStrategy1.getInstrumentPosition(futuresSymbol),
					   orderDate);
	   

	    
	  }
	shortStrategy1.eventProcessPendingOrders (backTesterDate);
      }

    std::cout << "Backtester end date = " << backTesterDate << std::endl; 
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
*/
}

