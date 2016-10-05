#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../PalStrategy.h"
#include "../BoostDateHelper.h"
#include "../RobustnessTest.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef dec::decimal<7> DecimalType;
typedef OHLCTimeSeriesEntry<7> EntryType;



std::string myCornSymbol("C2");

void printPositionHistory(const ClosedPositionHistory<7>& history);

DecimalType
createDecimal(const std::string& valueString)
{
  return fromString<DecimalType>(valueString);
}

const StrategyBroker<7>& 
getStrategyBroker(std::shared_ptr<BackTester<7>> backTester)
{
  std::shared_ptr<BacktesterStrategy<7>> backTesterStrategy = 
    (*(backTester->beginStrategies()));

  return backTesterStrategy->getStrategyBroker();
}

const ClosedPositionHistory<7>& 
getClosedPositionHistory (std::shared_ptr<BackTester<7>> backtester) 
{
  return getStrategyBroker (backtester).getClosedPositionHistory();
}

std::shared_ptr<RobustnessTestResult<7>> 
createRobustnessTestResult(std::shared_ptr<BackTester<7>> backtester)
{
  ClosedPositionHistory<7> closedPositions = 
    getClosedPositionHistory (backtester);

  return 
    make_shared<RobustnessTestResult<7>> (closedPositions.getPALProfitability(),
					  closedPositions.getProfitFactor(),
					  closedPositions.getNumPositions(),
					  closedPositions.getPayoffRatio(),
					  closedPositions.getMedianPayoffRatio(),
					  closedPositions.getRMultipleExpectancy());
}

void
performOneLongSideTest(RobustnessCalculator<7>& calculator,
		       std::shared_ptr<BackTester<7>> aBackTester,
		       std::shared_ptr<PalLongStrategy<7>> aLongStrategy,
		       std::shared_ptr<AstFactory> aFactory, 
		       const decimal<7>& aStop,
		       const decimal<7>& aTarget)
{
  decimal7 *newStopPtr ;
  decimal7 *newTargetPtr;
  std::shared_ptr<BackTester<7>> clonedTester;
  std::shared_ptr<PalLongStrategy<7>> localLongStrategy;
  ProfitTargetInPercentExpression *localProfitTarget;
  StopLossInPercentExpression *localStopLoss;
  std::shared_ptr<PriceActionLabPattern> localClonedPattern;
  std::shared_ptr<PriceActionLabPattern> aLocalPattern = 
	aLongStrategy->getPalPattern();
  std::shared_ptr<RobustnessTestResult<7>> testResult;

  newStopPtr = 
    aFactory->getDecimalNumber ((char *) toString(aStop).c_str());
  newTargetPtr = 
    aFactory->getDecimalNumber ((char *) toString(aTarget).c_str());

  localProfitTarget = aFactory->getLongProfitTarget (newTargetPtr);
  localStopLoss = aFactory->getLongStopLoss (newStopPtr);

  localClonedPattern = aLocalPattern->clone (localProfitTarget, localStopLoss);

  localLongStrategy = 
    make_shared<PalLongStrategy<7>>(aLongStrategy->getStrategyName(),
				    localClonedPattern,
				    aLongStrategy->getPortfolio());

  clonedTester = aBackTester->clone();
  clonedTester->addStrategy(localLongStrategy);
  clonedTester->backtest();

  //if (aStop == createDecimal("1.28"))
  //printPositionHistory (getClosedPositionHistory (clonedTester));

  testResult = createRobustnessTestResult(clonedTester);
  calculator.addTestResult (testResult, localClonedPattern);
}

void
performOneShortSideTest(RobustnessCalculator<7>& calculator,
			std::shared_ptr<BackTester<7>> aBackTester,
			std::shared_ptr<PalShortStrategy<7>> aShortStrategy,
			std::shared_ptr<AstFactory> aFactory, 
			const decimal<7>& aStop,
			const decimal<7>& aTarget)
{
  decimal7 *newStopPtr ;
  decimal7 *newTargetPtr;
  std::shared_ptr<BackTester<7>> clonedTester;
  std::shared_ptr<PalShortStrategy<7>> localShortStrategy;
  ProfitTargetInPercentExpression *localProfitTarget;
  StopLossInPercentExpression *localStopLoss;
  std::shared_ptr<PriceActionLabPattern> localClonedPattern;
  std::shared_ptr<PriceActionLabPattern> aLocalPattern = 
	aShortStrategy->getPalPattern();
  std::shared_ptr<RobustnessTestResult<7>> testResult;

  newStopPtr = 
    aFactory->getDecimalNumber ((char *) toString(aStop).c_str());
  newTargetPtr = 
    aFactory->getDecimalNumber ((char *) toString(aTarget).c_str());

  localProfitTarget = aFactory->getLongProfitTarget (newTargetPtr);
  localStopLoss = aFactory->getLongStopLoss (newStopPtr);

  localClonedPattern = aLocalPattern->clone (localProfitTarget, localStopLoss);

  localShortStrategy = 
    make_shared<PalShortStrategy<7>>(aShortStrategy->getStrategyName(),
				    localClonedPattern,
				    aShortStrategy->getPortfolio());

  clonedTester = aBackTester->clone();
  clonedTester->addStrategy(localShortStrategy);
  clonedTester->backtest();

  //if (aStop == createDecimal("1.28"))
  //printPositionHistory (getClosedPositionHistory (clonedTester));

  testResult = createRobustnessTestResult(clonedTester);
  calculator.addTestResult (testResult, localClonedPattern);
}

std::shared_ptr<PALRobustnessPermutationAttributes> 
getPalPermutationAttributes()
{
  return std::make_shared<PALRobustnessPermutationAttributes>();
}


std::shared_ptr<StatSignificantAttributes>
getStatSignificantAttributes()
{
  return std::make_shared<StatSignificantAttributes>();
}


std::shared_ptr<BackTester<7>> 
getBackTester(boost::gregorian::date startDate, 
	      boost::gregorian::date endDate) 
{
  return std::make_shared<DailyBackTester<7>>(startDate, endDate);
}

PercentNumber<7>
createPercentNumber(const decimal<7>& num)
{
  return PercentNumber<7>::createPercentNumber(num);
}

PatternRobustnessCriteria<7>
getPatternRobustness()
{
  return PatternRobustnessCriteria<7> (createDecimal("70.0"), 
				       createDecimal("2.0"),
				       createPercentNumber(createDecimal("2.0")), 
				       createDecimal("0.90"));
}

PatternRobustnessCriteria<7>
getPatternRobustness2()
{
  return PatternRobustnessCriteria<7> (createDecimal("68.0"), 
				       createDecimal("2.25"),
				       createPercentNumber(createDecimal("2.0")), 
				       createDecimal("0.80"));
}
std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString)
{
  return std::make_shared<DecimalType> (fromString<DecimalType>(valueString));
}

DecimalType *
createRawDecimalPtr(const std::string& valueString)
{
  return new dec::decimal<7> (fromString<DecimalType>(valueString));
}



date createDate (const std::string& dateString)
{
  return from_undelimited_string(dateString);
}

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

void printRobustnessTestResult(ProfitTargetStopPair<7> key,
			       std::shared_ptr<RobustnessTestResult<7>> testResult)
{
  std::cout << key.getProfitTarget() << "," << key.getProtectiveStop() << "," << testResult->getPALProfitability() << "," << testResult->getProfitFactor() << "," << testResult->getNumTrades() << "," << testResult->getPayOffRatio() << std::endl;
}

void printPositionHistory(const ClosedPositionHistory<7>& history)
{
  ClosedPositionHistory<7>::ConstPositionIterator it = history.beginTradingPositions();
  std::shared_ptr<TradingPosition<7>> p;
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

TEST_CASE ("RobustnessTestUnitTest operations", "[RobustnessTest]")
{
  PALFormatCsvReader<7> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<7>> p = csvFile.getTimeSeries();

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  decimal<7> cornBigPointValue(createDecimal("50.0"));
  decimal<7> cornTickValue(createDecimal("0.25"));
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto corn = std::make_shared<FuturesSecurity<7>>(futuresSymbol, 
						   futuresName, 
						   cornBigPointValue,
						   cornTickValue, 
						   p);

  std::string portName("Corn Portfolio");
  auto aPortfolio = std::make_shared<Portfolio<7>>(portName);

  aPortfolio->addSecurity (corn);

  std::string strategy1Name("PAL Long Strategy 1");

 
  std::shared_ptr<PalLongStrategy<7>> longStrategy1 = 
    std::make_shared<PalLongStrategy<7>>(strategy1Name, createLongPattern1(), 
					 aPortfolio);

  std::shared_ptr<PalShortStrategy<7>> shortStrategy1 =
    std::make_shared<PalShortStrategy<7>>("PAL Short Strategy 1", createShortPattern1(), aPortfolio);

  std::shared_ptr<PalLongStrategy<7>> longStrategy2 = 
    std::make_shared<PalLongStrategy<7>>("PAL Long Strategy 2", createLongPattern2(), 
					 aPortfolio);

  TimeSeriesDate backtestStartDate(TimeSeriesDate (1985, Mar, 19));
  TimeSeriesDate backtestEndDate(TimeSeriesDate (2011, Oct, 27));

  auto palPermutationAttributes = getPalPermutationAttributes();
  auto statPermutationAttributes = getStatSignificantAttributes();   

  PatternRobustnessCriteria<7> standardCriteria(getPatternRobustness());
  auto theBacktester = getBackTester(backtestStartDate, backtestEndDate);

  //AstFactory factory;
  
  auto factory = std::make_shared<AstFactory>();

  SECTION ("PatternRobustness Criteria", "[Pattern Robustness]")
    {
      REQUIRE (standardCriteria.getMinimumRobustnessIndex() ==
	       createDecimal("70.0"));
      REQUIRE (standardCriteria.getDesiredProfitFactor() ==
	       createDecimal("2.0"));
      REQUIRE (standardCriteria.getProfitabilitySafetyFactor() ==
	       createDecimal("0.90"));
      REQUIRE (standardCriteria.getRobustnessTolerance() ==
	       createPercentNumber(createDecimal("2.0")));
      REQUIRE (standardCriteria.getToleranceForNumTrades(100) ==
	       createPercentNumber(createDecimal("5.0")));

      REQUIRE (standardCriteria.getToleranceForNumTrades(30) ==
	       createPercentNumber(createDecimal("2.738613")));
      // Test copy constructor
      PatternRobustnessCriteria<7> criteria2(standardCriteria);
      REQUIRE (criteria2.getMinimumRobustnessIndex() ==
	       createDecimal("70.0"));
      REQUIRE (criteria2.getDesiredProfitFactor() ==
	       createDecimal("2.0"));
      REQUIRE (criteria2.getProfitabilitySafetyFactor() ==
	       createDecimal("0.90"));
      REQUIRE (criteria2.getRobustnessTolerance() ==
	       createPercentNumber(createDecimal("2.0")));

      PatternRobustnessCriteria<7> criteria3(getPatternRobustness2());

      // Test assignment operator
      criteria2 = criteria3;
      REQUIRE (criteria2.getMinimumRobustnessIndex() ==
	       createDecimal("68.0"));
      REQUIRE (criteria2.getDesiredProfitFactor() ==
	       createDecimal("2.25"));
      REQUIRE (criteria2.getProfitabilitySafetyFactor() ==
	       createDecimal("0.80"));
      REQUIRE (criteria2.getRobustnessTolerance() ==
	       createPercentNumber(createDecimal("2.0")));
    }

  SECTION ("PALRobustnessPermutationAttributes Criteria", "[PAL Permutation Attributes]")
    {
      REQUIRE (palPermutationAttributes->getNumberOfPermutations() == 19);
      REQUIRE (palPermutationAttributes->getNumPermutationsBelowRef() == 14);
      REQUIRE (palPermutationAttributes->getNumPermutationsAboveRef() == 4);
      REQUIRE (palPermutationAttributes->getPermutationsDivisor() == 16);
      REQUIRE (palPermutationAttributes->numEntriesToTestAtBeginning() == 2);
      REQUIRE (palPermutationAttributes->numEntriesToTestAtEnd() == 2);
    }

  SECTION ("PALRobustnessPermutationAttributes Criteria", "[PAL Permutation Attributes]")
    {
      REQUIRE (statPermutationAttributes->getNumberOfPermutations() == 30);
      REQUIRE (statPermutationAttributes->getNumPermutationsBelowRef() == 15);
      REQUIRE (statPermutationAttributes->getNumPermutationsAboveRef() == 14);
      REQUIRE (statPermutationAttributes->getPermutationsDivisor() == 30);
      REQUIRE (statPermutationAttributes->numEntriesToTestAtBeginning() == 3);
      REQUIRE (statPermutationAttributes->numEntriesToTestAtEnd() == 3);
    }

  SECTION ("ProfitTargetStopPair", "[ProfitTargetStopPair operations]")
    {
      ProfitTargetStopPair<7> pair1(createDecimal("2.56"), createDecimal("1.28"));
      REQUIRE (pair1.getProfitTarget() == createDecimal("2.56"));
      REQUIRE (pair1.getProtectiveStop() == createDecimal("1.28"));

      ProfitTargetStopPair<7> pair2(createDecimal("1.34"), createDecimal("1.28"));
      REQUIRE (pair2.getProfitTarget() == createDecimal("1.34"));
      REQUIRE (pair2.getProtectiveStop() == createDecimal("1.28"));

      ProfitTargetStopPair<7> pair3(pair1);
      REQUIRE (pair3.getProfitTarget() == createDecimal("2.56"));
      REQUIRE (pair3.getProtectiveStop() == createDecimal("1.28"));

      pair3 = pair2;
      REQUIRE (pair3.getProfitTarget() == createDecimal("1.34"));
      REQUIRE (pair3.getProtectiveStop() == createDecimal("1.28"));
    }

  SECTION ("ProfitTargetStopComparator", "[ProfitTargetStopComparator operations]")
    {
      ProfitTargetStopPair<7> pair1(createDecimal("2.56"),
				    createDecimal("1.28"));

      ProfitTargetStopPair<7> pair2(createDecimal("2.70"),
				    createDecimal("1.35"));

      ProfitTargetStopPair<7> pair3(createDecimal("2.42"),
				    createDecimal("1.21"));

      ProfitTargetStopComparator<7> comp1;
      REQUIRE (comp1(pair1, pair2));
      REQUIRE_FALSE (comp1(pair1, pair3));
    }

  SECTION ("RobustnessTestResult", "[RobustnessTestResult operations]")
    {
      decimal<7> profitability1(createDecimal("68.00"));
      decimal<7> profitFactor1(createDecimal("2.30"));
      decimal<7> payoffRatio1(createDecimal("1.05"));
      decimal<7> rMultipleExpection1(createDecimal("1.07"));
      decimal<7> rMultipleExpection2(createDecimal("1.04"));

      RobustnessTestResult<7> result1(profitability1,
				      profitFactor1,
				      21,
				      payoffRatio1,
				      payoffRatio1,
				      rMultipleExpection1);

      REQUIRE (result1.getPALProfitability() == profitability1);
      REQUIRE (result1.getProfitFactor() == profitFactor1);
      REQUIRE (result1.getNumTrades() == 21);
      REQUIRE (result1.getPayOffRatio() == payoffRatio1);
      REQUIRE (result1.getRMultipleExpectancy() == rMultipleExpection1);

      RobustnessTestResult<7> result2(result1);

      REQUIRE (result2.getPALProfitability() == profitability1);
      REQUIRE (result2.getProfitFactor() == profitFactor1);
      REQUIRE (result2.getNumTrades() == 21);
      REQUIRE (result2.getPayOffRatio() == payoffRatio1);
      REQUIRE (result2.getRMultipleExpectancy() == rMultipleExpection1);

      RobustnessTestResult<7> result3(profitability1,
				      profitFactor1,
				      33,
				      payoffRatio1,
				      payoffRatio1,
				      rMultipleExpection2);

      result2 = result3;
      REQUIRE (result2.getPALProfitability() == profitability1);
      REQUIRE (result2.getProfitFactor() == profitFactor1);
      REQUIRE (result2.getNumTrades() == 33);
      REQUIRE (result2.getPayOffRatio() == payoffRatio1);
      REQUIRE (result2.getRMultipleExpectancy() == rMultipleExpection2);
    }

  /*
  SECTION ("RobustnessCalculator long pattern 1", "[RobustnessCalculator operations]")
    {
      //std::cout << "In RobustnessCalculator long pattern 1" << std::endl;

      RobustnessCalculator<7> robustCalc (longStrategy1->getPalPattern(), 
					  palPermutationAttributes,
					  standardCriteria);
      std::shared_ptr<PriceActionLabPattern> pattern = 
	longStrategy1->getPalPattern();
      decimal<7> originalStop(pattern->getStopLossAsDecimal());
      decimal<7> payOffRatio (pattern->getProfitTargetAsDecimal()/
			      originalStop);
      decimal<7> divisor(palPermutationAttributes->getPermutationsDivisor());
      decimal<7> permutationStopInc(originalStop / divisor);
      uint32_t permsBelowRef(palPermutationAttributes->getNumPermutationsBelowRef());
      uint32_t permsAboveRef(palPermutationAttributes->getNumPermutationsAboveRef());
      decimal<7> firstStopPermutation(originalStop -
				      (decimal<7>(permsBelowRef) * permutationStopInc));
      decimal<7> currentStop (firstStopPermutation);
      decimal<7> currentTarget (currentStop * payOffRatio);

      //std::cout << "About to create test results below ref" << std::endl;

      uint32_t i;
      for (i = 1; i <= permsBelowRef; i++)
	{
	  //std::cout << "Test results below ref, permutation # " << i << std::endl;
	  performOneLongSideTest(robustCalc,theBacktester,longStrategy1,
				 factory, currentStop, currentTarget);

	  currentStop = currentStop + permutationStopInc;
	  currentTarget = currentStop * payOffRatio;
	}

      currentStop  = originalStop;
      currentTarget = (currentStop * payOffRatio);

      performOneLongSideTest(robustCalc, theBacktester, longStrategy1,
			     factory, currentStop, currentTarget);

      currentStop  = originalStop + permutationStopInc;
      currentTarget = (currentStop * payOffRatio);

      //std::cout << "About to create test results above ref" << std::endl;
      //std::cout << "Permutations above reference = " << (int) permsAboveRef.getAsInteger() << std::endl << std::endl;

      for (i = 1; i <= permsAboveRef; i++)
	{
	  performOneLongSideTest(robustCalc,
				 theBacktester,
				 longStrategy1,
				 factory, 
				 currentStop,
				 currentTarget);

	  currentStop = currentStop + permutationStopInc;
	  currentTarget = currentStop * payOffRatio;
	}

      RobustnessCalculator<7>::RobustnessTestResultIterator it = 
	robustCalc.beginRobustnessTestResults();

      std::cout << "Long pattern 1 Robustness index = " << robustCalc.getRobustnessIndex() << std::endl;

      REQUIRE_FALSE (robustCalc.isRobust());
      for (; it != robustCalc.endRobustnessTestResults(); it++)
	{
	  printRobustnessTestResult(it->first, it->second);
	}
    }

  SECTION ("RobustnessCalculator long pattern 2", "[RobustnessCalculator operations]")
    {
      //std::cout << "In RobustnessCalculator long pattern 2" << std::endl;

      RobustnessCalculator<7> robustCalc (longStrategy2->getPalPattern(), 
					  statPermutationAttributes,
					  standardCriteria);
      std::shared_ptr<PriceActionLabPattern> pattern = 
	longStrategy2->getPalPattern();
      decimal<7> originalStop(pattern->getStopLossAsDecimal());
      decimal<7> payOffRatio (pattern->getProfitTargetAsDecimal()/
			      originalStop);
      decimal<7> divisor(statPermutationAttributes->getPermutationsDivisor());
      decimal<7> permutationStopInc(originalStop / divisor);
      uint32_t permsBelowRef(statPermutationAttributes->getNumPermutationsBelowRef());
      uint32_t permsAboveRef(statPermutationAttributes->getNumPermutationsAboveRef());
      decimal<7> firstStopPermutation(originalStop -
				      (decimal<7>(permsBelowRef) * permutationStopInc));
      decimal<7> currentStop (firstStopPermutation);
      decimal<7> currentTarget (currentStop * payOffRatio);

      //std::cout << "About to create test results below ref" << std::endl;

      uint32_t i;
      for (i = 1; i <= permsBelowRef; i++)
	{
	  //std::cout << "Test results below ref, permutation # " << i << std::endl;
	  performOneLongSideTest(robustCalc,theBacktester,longStrategy2,
				 factory, currentStop, currentTarget);

	  currentStop = currentStop + permutationStopInc;
	  currentTarget = currentStop * payOffRatio;
	}

      currentStop  = originalStop;
      currentTarget = (currentStop * payOffRatio);

      performOneLongSideTest(robustCalc, theBacktester, longStrategy2,
			     factory, currentStop, currentTarget);

      currentStop  = originalStop + permutationStopInc;
      currentTarget = (currentStop * payOffRatio);

      //std::cout << "About to create test results above ref" << std::endl;
      //std::cout << "Permutations above reference = " << (int) permsAboveRef.getAsInteger() << std::endl << std::endl;

      for (i = 1; i <= permsAboveRef; i++)
	{
	  performOneLongSideTest(robustCalc,
				 theBacktester,
				 longStrategy2,
				 factory, 
				 currentStop,
				 currentTarget);

	  currentStop = currentStop + permutationStopInc;
	  currentTarget = currentStop * payOffRatio;
	}

      RobustnessCalculator<7>::RobustnessTestResultIterator it = 
	robustCalc.beginRobustnessTestResults();

      std::cout << "Long pattern 2 Robustness index = " << robustCalc.getRobustnessIndex() << std::endl;

      REQUIRE_FALSE (robustCalc.isRobust());
      for (; it != robustCalc.endRobustnessTestResults(); it++)
	{
	  printRobustnessTestResult(it->first, it->second);
	}
    }

  SECTION ("RobustnessCalculator short pattern 1", "[RobustnessCalculator operations]")
    {
      std::cout << "In RobustnessCalculator short pattern 1" << std::endl;

      RobustnessCalculator<7> robustCalc (shortStrategy1->getPalPattern(), 
					  palPermutationAttributes,
					  standardCriteria);
      std::shared_ptr<PriceActionLabPattern> pattern = 
	shortStrategy1->getPalPattern();
      decimal<7> originalStop(pattern->getStopLossAsDecimal());
      decimal<7> payOffRatio (pattern->getProfitTargetAsDecimal()/
			      originalStop);
      decimal<7> divisor(palPermutationAttributes->getPermutationsDivisor());
      decimal<7> permutationStopInc(originalStop / divisor);
      uint32_t permsBelowRef(palPermutationAttributes->getNumPermutationsBelowRef());
      uint32_t permsAboveRef(palPermutationAttributes->getNumPermutationsAboveRef());
      decimal<7> firstStopPermutation(originalStop -
				      (decimal<7>(permsBelowRef) * permutationStopInc));
      decimal<7> currentStop (firstStopPermutation);
      decimal<7> currentTarget (currentStop * payOffRatio);

      //std::cout << "About to create test results below ref" << std::endl;

      uint32_t i;
      for (i = 1; i <= permsBelowRef; i++)
	{
	  //std::cout << "Test results below ref, permutation # " << i << std::endl;
	  performOneShortSideTest(robustCalc,theBacktester,shortStrategy1,
				 factory, currentStop, currentTarget);

	  currentStop = currentStop + permutationStopInc;
	  currentTarget = currentStop * payOffRatio;
	}

      currentStop  = originalStop;
      currentTarget = (currentStop * payOffRatio);

      performOneShortSideTest(robustCalc, theBacktester, shortStrategy1,
			     factory, currentStop, currentTarget);

      currentStop  = originalStop + permutationStopInc;
      currentTarget = (currentStop * payOffRatio);

      //std::cout << "About to create test results above ref" << std::endl;
      //std::cout << "Permutations above reference = " << (int) permsAboveRef.getAsInteger() << std::endl << std::endl;

      for (i = 1; i <= permsAboveRef; i++)
	{
	  performOneShortSideTest(robustCalc,
				 theBacktester,
				 shortStrategy1,
				 factory, 
				 currentStop,
				 currentTarget);

	  currentStop = currentStop + permutationStopInc;
	  currentTarget = currentStop * payOffRatio;
	}

      RobustnessCalculator<7>::RobustnessTestResultIterator it = 
	robustCalc.beginRobustnessTestResults();

      std::cout << "Short Pattern 1 Robustness index = " << robustCalc.getRobustnessIndex() << std::endl;

      robustCalc.isRobust();
      std::cout << "Print short pattern 1 robustness results" << std::endl;

      for (; it != robustCalc.endRobustnessTestResults(); it++)
	{
	  printRobustnessTestResult(it->first, it->second);
	}
    }
  */

  SECTION ("RobustnessTest long pattern 1", "[RobustnessTest operations]")
    {
      //std::cout << "In RobustnessCalculator long pattern 1" << std::endl;


      RobustnessTest<7> testRobustness(theBacktester, longStrategy1, 
				       palPermutationAttributes,
				       standardCriteria, factory);

      testRobustness.runRobustnessTest();

      RobustnessTestMonteCarlo<7> testRobustness2(theBacktester, longStrategy2, 
						 palPermutationAttributes,
						 standardCriteria, factory);

      //testRobustness2.runRobustnessTest();

    }
}

