#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "PalStrategy.h"
#include "BoostDateHelper.h"
#include "MonteCarloPermutationTest.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

namespace {

std::string myCornSymbol("C2");

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

}


TEST_CASE ("MonteCarloPermutationTest2-PalStrategy operations", "[PalStrategy]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR_OOS.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
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

  std::string portName("Corn Portfolio");
  auto aPortfolio = std::make_shared<Portfolio<DecimalType>>(portName);

  aPortfolio->addSecurity (corn);

  std::string strategy1Name("PAL Long Strategy 1");

 
  std::shared_ptr<PalLongStrategy<DecimalType>> longStrategy1 = 
    std::make_shared<PalLongStrategy<DecimalType>>(strategy1Name, createLongPattern1(), 
					 aPortfolio);

  std::shared_ptr<PalShortStrategy<DecimalType>> shortStrategy1 =
    std::make_shared<PalShortStrategy<DecimalType>>("PAL Short Strategy 1", createShortPattern1(), aPortfolio);

  std::shared_ptr<PalLongStrategy<DecimalType>> longStrategy2 = 
    std::make_shared<PalLongStrategy<DecimalType>>("PAL Long Strategy 2", createLongPattern2(), 
					 aPortfolio);

SECTION ("PalStrategy testing for all long trades - pattern 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (2011, Oct, 28));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2015, Oct, 26));

    auto palLongBacktester1 = std::make_shared< DailyBackTester<DecimalType>>(backTesterDate,
								    backtestEndDate);

    palLongBacktester1->addStrategy(longStrategy1);
    MonteCarloPermuteMarketChanges<DecimalType> mcpt(palLongBacktester1,
					    200);
    std::cout << "P-Value for strategy 1 is " << mcpt.runPermutationTest() << std::endl;
  }

SECTION ("PalStrategy testing long trades using original MCPT - pattern 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (2011, Oct, 28));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2015, Oct, 26));

    auto palLongBacktester1 = std::make_shared< DailyBackTester<DecimalType>>(backTesterDate,
								    backtestEndDate);

    palLongBacktester1->addStrategy(longStrategy1);
    OriginalMCPT<DecimalType> mcpt(palLongBacktester1,
			 5000);
    std::cout << "P-Value for strategy 1 using original MCPT is " << mcpt.runPermutationTest() << std::endl;
  }
  
SECTION ("PalStrategy testing for all long trades - pattern 2") 
  {
    std::cout << "In second long pattern backtest" << std::endl;

    TimeSeriesDate backTesterDate(TimeSeriesDate (2011, Oct, 28));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2015, Oct, 26));

    auto palLongBacktester2 = std::make_shared<DailyBackTester<DecimalType>>(backTesterDate,
								    backtestEndDate);
    palLongBacktester2->addStrategy(longStrategy2);
    MonteCarloPermuteMarketChanges<DecimalType> mcpt2(palLongBacktester2,
					    200);
    std::cout << "P-Value for strategy 2 is " << mcpt2.runPermutationTest() << std::endl;
  }

SECTION ("MonteCarlo simulation of payoff ratio - pattern 1") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (2011, Oct, 28));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2015, Oct, 26));

    auto palLongBacktester1 = std::make_shared< DailyBackTester<DecimalType>>(backTesterDate,
								    backtestEndDate);

    palLongBacktester1->addStrategy(longStrategy1);
    MonteCarloPayoffRatio<DecimalType> mcpt(palLongBacktester1,
				  1000);
    std::cout << "Monte Carlo Payoff Ratio for strategy 1 is " << mcpt.runPermutationTest() << std::endl;

    auto palLongBacktester2 = std::make_shared< DailyBackTester<DecimalType>>(backTesterDate,
								    backtestEndDate);

    std::shared_ptr<PalLongStrategy<DecimalType>> longStrategy1Clone = 
      std::make_shared<PalLongStrategy<DecimalType>>(strategy1Name, createLongPattern1(), 
					 aPortfolio);
    palLongBacktester2->addStrategy(longStrategy1Clone);
    palLongBacktester2->backtest();

    ClosedPositionHistory<DecimalType> hist = longStrategy1Clone->getStrategyBroker().getClosedPositionHistory();

    std::cout << "*** Number of positions = " << hist.getNumPositions() << std::endl;
    std::cout << "*** Number of winning positions = " << hist.getNumWinningPositions() << std::endl;
    std::cout << "*** Number of losing positions = " << hist.getNumLosingPositions() << std::endl;

    DecimalType payoff = hist.getMedianPayoffRatio();
    std::cout << "*** Payoff ratio from backtesting = " << payoff << std::endl;
    
  }
 
SECTION ("PalStrategy testing for all short trades") 
  {
    TimeSeriesDate backTesterDate(TimeSeriesDate (2011, Oct, 28));
    TimeSeriesDate backtestEndDate(TimeSeriesDate (2015, Oct, 26));

    auto palShortBacktester1= std::make_shared<DailyBackTester<DecimalType>>(backTesterDate,
								    backtestEndDate);
    palShortBacktester1->addStrategy(shortStrategy1);
    MonteCarloPermuteMarketChanges<DecimalType> mcpt3(palShortBacktester1,
					    200);

    std::cout << "P-Value for short strategy 1 is " << mcpt3.runPermutationTest() << std::endl;
 
  }    
}

