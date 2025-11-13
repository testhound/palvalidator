#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TimeSeriesCsvReader.h"
#include "PalStrategy.h"
#include "BoostDateHelper.h"
#include "BackTester.h"
#include "DecimalConstants.h"
#include "TestUtils.h"
#include "MonteCarloTestPolicy.h"
#include "PalStrategyTestHelpers.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

template <class Decimal>
class MockMonteCarloBackTester : public DailyBackTester<Decimal> {
private:
    uint32_t mExpectedTrades = 0;
    uint32_t mExpectedBars = 0;

public:
    MockMonteCarloBackTester() : DailyBackTester<Decimal>() {}

    // Override the clone method to return an instance of the mock
    std::shared_ptr<BackTester<Decimal>> clone() const override {
        auto mock = std::make_shared<MockMonteCarloBackTester<Decimal>>();
        mock->setExpectedTrades(this->mExpectedTrades);
        mock->setExpectedBars(this->mExpectedBars);
        return mock;
    }

    // --- Methods to control the mock's behavior ---
    void setExpectedTrades(uint32_t trades) {
        mExpectedTrades = trades;
    }
    void setExpectedBars(uint32_t bars) {
        mExpectedBars = bars;
    }

    // --- Override the methods our policy depends on ---
    uint32_t getNumTrades() const {
        return mExpectedTrades;
    }

    uint32_t getNumBarsInTrades() const {
        return mExpectedBars;
    }
};

const static std::string myCornSymbol("@C");

PatternDescription *
createDescription (const std::string& fileName, unsigned int index, unsigned long indexDate,
		   const std::string& percLong, const std::string& percShort,
		   unsigned int numTrades, unsigned int consecutiveLosses);



std::shared_ptr<PriceActionLabPattern>
static createLongPattern1()
{
  // Create description using shared_ptr
  auto percentLong = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR.txt", 39, 20131217,
                                                   percentLong, percentShort, 21, 2);

  // Create price bar references using shared_ptr
  auto open5 = std::make_shared<PriceBarOpen>(5);
  auto close5 = std::make_shared<PriceBarClose>(5);
  auto gt1 = std::make_shared<GreaterThanExpr>(open5, close5);

  auto close6 = std::make_shared<PriceBarClose>(6);
  auto gt2 = std::make_shared<GreaterThanExpr>(close5, close6);

  // OPEN OF 5 BARS AGO > CLOSE OF 5 BARS AGO
  // AND CLOSE OF 5 BARS AGO > CLOSE OF 6 BARS AGO
  auto and1 = std::make_shared<AndExpr>(gt1, gt2);

  auto open6 = std::make_shared<PriceBarOpen>(6);
  auto gt3 = std::make_shared<GreaterThanExpr>(close6, open6);

  auto close8 = std::make_shared<PriceBarClose>(8);
  auto gt4 = std::make_shared<GreaterThanExpr>(open6, close8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  auto and2 = std::make_shared<AndExpr>(gt3, gt4);

  auto open8 = std::make_shared<PriceBarOpen>(8);
  auto gt5 = std::make_shared<GreaterThanExpr>(close8, open8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  // CLOSE OF 8 BARS AGO > OPEN OF 8 BARS AGO

  auto and3 = std::make_shared<AndExpr>(and2, gt5);
  auto longPattern1 = std::make_shared<AndExpr>(and1, and3);
  auto entry = createLongOnOpen();
  auto target = createLongProfitTarget("0.32");
  auto stop = createLongStopLoss("0.16");

  // 2.56 profit target in points = 93.81
  return std::make_shared<PriceActionLabPattern>(desc, longPattern1,
                                                 entry,
                                                 target,
                                                 stop);
}

// Definition for the short pattern used in testing
static std::shared_ptr<PriceActionLabPattern>
createShortPattern1()
{
  // Create description using shared_ptr
  auto percentLong = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR_Short.txt", 1, 20131217,
                                                   percentLong, percentShort, 15, 3);

  // A simple expression that can be evaluated. For testing purposes,
  // we don't need a complex real-world pattern.
  // C > O
  auto open1 = std::make_shared<PriceBarOpen>(1);
  auto close1 = std::make_shared<PriceBarClose>(1);
  auto shortPattern = std::make_shared<GreaterThanExpr>(close1, open1);

  auto entry = createShortOnOpen();
  auto target = createShortProfitTarget("0.50");
  auto stop = createShortStopLoss("0.25");

  return std::make_shared<PriceActionLabPattern>(desc, shortPattern,
                                                 entry,
                                                 target,
                                                 stop);
}


std::shared_ptr<PriceActionLabPattern>
createLongPattern2();


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

TEST_CASE ("BackTester operations", "[BackTester]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  //
  auto ts = csvFile.getTimeSeries();
  auto beginIt = ts->beginRandomAccess();
  auto endIt   = ts->endRandomAccess();
  auto lastIt  = endIt; --lastIt;
  std::cerr << "Series covers: "
	    << to_simple_string(beginIt->getDateValue())
	    << " through "
	    << to_simple_string(lastIt->getDateValue())
	    << "\n";

  //

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

    auto aBroker = aStrategy1->getStrategyBroker();
    REQUIRE (aBroker.getTotalTrades() == 24);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 24);

    ClosedPositionHistory<DecimalType> history = aBroker.getClosedPositionHistory();

    REQUIRE (history.getNumLosingPositions() == 8);
    REQUIRE (history.getNumWinningPositions() == 16);

    DecimalType rMultiple = history.getRMultipleExpectancy();
    REQUIRE (rMultiple > DecimalConstants<DecimalType>::DecimalZero);
  }



SECTION ("PalStrategy testing for all long trades - pattern 2")
  {
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

    auto aBroker2 = aStrategy2->getStrategyBroker();
    REQUIRE (aBroker2.getTotalTrades() == 46);
    REQUIRE (aBroker2.getOpenTrades() == 0);
    REQUIRE (aBroker2.getClosedTrades() == 46);

    ClosedPositionHistory<DecimalType> history = aBroker2.getClosedPositionHistory();

    DecimalType rMultiple = history.getRMultipleExpectancy();
    REQUIRE (rMultiple > DecimalConstants<DecimalType>::DecimalZero);
  }


SECTION("BackTester::getAllHighResReturns with PalLongStrategy") {
    using DT = DecimalType;
    const std::string sym = "@C";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    // Updated helper to allow distinct open and close values
    auto mkBar = [&](int Y,int M,int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        return createTimeSeriesEntry(dt, o, c, o, c, 1); // Simplified high/low
    };

    // Create bars with distinct open/close to test both return types
    auto b1 = mkBar(2020, Jan, 1, "100.00", "105.00"); // 1st trade, entry bar
    auto b2 = mkBar(2020, Jan, 2, "105.00", "110.00"); // 1st trade, exit bar
    auto b3 = mkBar(2020, Jan, 3, "200.00", "202.00"); // 2nd trade, entry bar
    auto b4 = mkBar(2020, Jan, 4, "202.00", "210.00"); // 2nd trade, 2nd bar (open)

    auto portfolio = std::make_shared<Portfolio<DT>>("port");
    auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*b1);
    ts->addEntry(*b2);
    ts->addEntry(*b3);
    ts->addEntry(*b4);

    portfolio->addSecurity(
        std::make_shared<FuturesSecurity<DT>>(
            sym, sym, createDecimal("50.0"), createDecimal("0.25"), ts
        )
    );

    auto pattern = createLongPattern1();
    auto strat = std::make_shared<PalLongStrategy<DT>>("test-long", pattern, portfolio);

    DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 4));
    bt.addStrategy(strat);

    // Inject a CLOSED 2-bar trade
    {
        // NOTE: The constructor's entry price is the OPEN of the entry bar
        auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
        pos->addBar(*b2);
        pos->ClosePosition(b2->getDateValue(), b2->getCloseValue()); // Exit at b2's close
        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
        closedHist.addClosedPosition(pos);
    }

    // Inject a STILL-OPEN 2-bar trade
    {
        auto posO = std::make_shared<TradingPositionLong<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
        posO->addBar(*b4);
        auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
        instrPos.addPosition(posO);
    }

    // A 2-bar closed trade and a 2-bar open trade should yield 4 returns total (2 each)
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 4);

    // Verify returns for the CLOSED trade
    // Return 1: (105-100)/100 = 0.05
    REQUIRE(allR[0] == (b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue());
    // Return 2: (110-105)/105 = 0.0476...
    REQUIRE(allR[1] == (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue());

    // Verify returns for the OPEN trade
    // Return 3: (202-200)/200 = 0.01
    REQUIRE(allR[2] == (b3->getCloseValue() - b3->getOpenValue()) / b3->getOpenValue());
    // Return 4: (210-202)/202 = 0.0396...
    REQUIRE(allR[3] == (b4->getCloseValue() - b3->getCloseValue()) / b3->getCloseValue());
 }

 SECTION("getAllHighResReturns with only closed positions") {
    using DT = DecimalType;
    const std::string sym = "@TEST";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y,int M,int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        return createTimeSeriesEntry(dt, o, c, o, c, 1);
    };
    auto b1 = mkBar(2020, Jan, 1, "100.00", "104.00");
    auto b2 = mkBar(2020, Jan, 2, "104.00", "120.00");

    auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*b1);
    ts->addEntry(*b2);

    auto portfolio = std::make_shared<Portfolio<DT>>("port");
    portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

    auto pattern = createLongPattern1();
    auto strat = std::make_shared<PalLongStrategy<DT>>("only-closed", pattern, portfolio);

    DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
    bt.addStrategy(strat);

    {
        auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
        pos->addBar(*b2);
        pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
        closedHist.addClosedPosition(pos);
    }

    // A two-bar trade should produce two returns
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 2);

    // Return 1: (104 - 100) / 100 = 0.04
    REQUIRE(allR[0] == (b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue());
    // Return 2: (120 - 104) / 104 = 0.1538...
    REQUIRE(allR[1] == (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue());
 }

 SECTION("getAllHighResReturns with only open positions") {
    using DT = DecimalType;
    const std::string sym = "@TEST";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y,int M,int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        return createTimeSeriesEntry(dt, o, c, o, c, 1);
    };
    auto b1 = mkBar(2020, Jan, 3, "200.00", "202.00");
    auto b2 = mkBar(2020, Jan, 4, "202.00", "240.00");

    auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*b1);
    ts->addEntry(*b2);

    auto portfolio = std::make_shared<Portfolio<DT>>("port");
    portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

    auto pattern = createLongPattern1();
    auto strat = std::make_shared<PalLongStrategy<DT>>("only-open", pattern, portfolio);

    DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 3), TimeSeriesDate(2020, Jan, 4));
    bt.addStrategy(strat);

    {
        auto posO = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
        posO->addBar(*b2);
        auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
        instrPos.addPosition(posO);
    }

    // A two-bar open trade should have two returns so far
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 2);

    // Return 1: (202 - 200) / 200 = 0.01
    REQUIRE(allR[0] == (b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue());
    // Return 2: (240 - 202) / 202 = 0.1881...
    REQUIRE(allR[1] == (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue());
 }

 //
 // NEW UNIT TESTS FOR SHORT TRADES
 //

 SECTION("BackTester::getAllHighResReturns with PalShortStrategy") {
    using DT = DecimalType;
    const std::string sym = "@C_SHORT";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y, int M, int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        // Correctly set high and low to ensure valid bars
        DT h = std::max(o, c);
        DT l = std::min(o, c);
        return createTimeSeriesEntry(dt, o, h, l, c, 1);
    };
    
    // Create bars for two short trades
    auto b1 = mkBar(2020, Jan, 1, "100.00", "95.00"); // 1st trade, entry (profit)
    auto b2 = mkBar(2020, Jan, 2, "95.00", "90.00");  // 1st trade, exit (profit)
    auto b3 = mkBar(2020, Jan, 3, "200.00", "205.00"); // 2nd trade, entry (loss)
    auto b4 = mkBar(2020, Jan, 4, "205.00", "210.00"); // 2nd trade, 2nd bar (loss)

    auto portfolio = std::make_shared<Portfolio<DT>>("port_short");
    auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*b1);
    ts->addEntry(*b2);
    ts->addEntry(*b3);
    ts->addEntry(*b4);

    portfolio->addSecurity(
        std::make_shared<FuturesSecurity<DT>>(
            sym, sym, createDecimal("50.0"), createDecimal("0.25"), ts
        )
    );

    auto pattern = createShortPattern1();
    auto strat = std::make_shared<PalShortStrategy<DT>>("test-short", pattern, portfolio);

    DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 4));
    bt.addStrategy(strat);

    // Inject a CLOSED short trade
    {
        auto pos = std::make_shared<TradingPositionShort<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
        pos->addBar(*b2);
        pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
        closedHist.addClosedPosition(pos);
    }

    // Inject an OPEN short trade
    {
        auto posO = std::make_shared<TradingPositionShort<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
        posO->addBar(*b4);
        auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
        instrPos.addPosition(posO);
    }

    // Two 2-bar trades should yield 4 returns (2 per trade)
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 4);

    // --- Verify CLOSED Short Trade ---
    // Return 1 (entry bar): -1 * (95 - 100) / 100 = 0.05
    REQUIRE(allR[0] == -((b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue()));
    // Return 2 (exit bar): -1 * (90 - 95) / 95 = 0.0526...
    REQUIRE(allR[1] == -((b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue()));

    // --- Verify OPEN Short Trade ---
    // Return 3 (entry bar): -1 * (205 - 200) / 200 = -0.025
    REQUIRE(allR[2] == -((b3->getCloseValue() - b3->getOpenValue()) / b3->getOpenValue()));
    // Return 4 (2nd bar): -1 * (210 - 205) / 205 = -0.0243...
    REQUIRE(allR[3] == -((b4->getCloseValue() - b3->getCloseValue()) / b3->getCloseValue()));
}

//
// Replace the failing "only closed short positions" test section with this corrected version
//
SECTION("getAllHighResReturns with only closed short positions") {
    using DT = DecimalType;
    const std::string sym = "@TEST_SHORT_CLOSED";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y, int M, int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        // Correctly set high and low to ensure valid bars
        DT h = std::max(o, c);
        DT l = std::min(o, c);
        return createTimeSeriesEntry(dt, o, h, l, c, 1);
    };

    auto b1 = mkBar(2020, Jan, 1, "100.00", "90.00"); // Profit
    auto b2 = mkBar(2020, Jan, 2, "90.00", "80.00"); // Profit

    auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*b1);
    ts->addEntry(*b2);

    auto portfolio = std::make_shared<Portfolio<DT>>("port_short_closed");
    portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

    auto pattern = createShortPattern1();
    auto strat = std::make_shared<PalShortStrategy<DT>>("only-closed-short", pattern, portfolio);

    DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
    bt.addStrategy(strat);

    {
        auto pos = std::make_shared<TradingPositionShort<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
        pos->addBar(*b2);
        pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
        closedHist.addClosedPosition(pos);
    }

    // A two-bar trade should have two returns
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 2);

    // Return 1: -1 * (90 - 100) / 100 = 0.10
    REQUIRE(allR[0] == -((b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue()));
    // Return 2: -1 * (80 - 90) / 90 = 0.111...
    REQUIRE(allR[1] == -((b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue()));
}

SECTION("getAllHighResReturns with only open short positions") {
    using DT = DecimalType;
    const std::string sym = "@TEST_SHORT_OPEN";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    // Corrected mkBar to handle valid high/low
    auto mkBar = [&](int Y, int M, int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        DT h = std::max(o, c);
        DT l = std::min(o, c);
        return createTimeSeriesEntry(dt, o, h, l, c, 1);
    };

    auto b1 = mkBar(2020, Jan, 3, "200.00", "205.00"); // Loss
    auto b2 = mkBar(2020, Jan, 4, "205.00", "210.00"); // Loss

    auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*b1);
    ts->addEntry(*b2);

    auto portfolio = std::make_shared<Portfolio<DT>>("port_short_open");
    portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

    auto pattern = createShortPattern1();
    auto strat = std::make_shared<PalShortStrategy<DT>>("only-open-short", pattern, portfolio);

    DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 3), TimeSeriesDate(2020, Jan, 4));
    bt.addStrategy(strat);

    {
        // Entry is at the open of the first bar
        auto posO = std::make_shared<TradingPositionShort<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
        posO->addBar(*b2);
        auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
        instrPos.addPosition(posO);
    }

    // A two-bar open trade should have two returns
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 2);

    // Return 1: -1 * (205 - 200) / 200 = -0.025
    REQUIRE(allR[0] == -((b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue()));
    // Return 2: -1 * (210 - 205) / 205 = -0.0243...
    REQUIRE(allR[1] == -((b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue()));
}

 // --- New tests for BackTester::getAllHighResReturnsWithDates ---
// Place these near your existing "getAllHighResReturns" sections.

SECTION("getAllHighResReturnsWithDates — mixed: one closed + one open long trade") {
  using DT = DecimalType;
  const std::string sym = "@C_MIXED_LONG";
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto mkBar = [&](int Y,int M,int D, const std::string& openStr, const std::string& closeStr) {
    TimeSeriesDate dt(Y, M, D);
    DT o = createDecimal(openStr);
    DT c = createDecimal(closeStr);
    // simple high/low = open/close envelope keeps entries valid
    DT h = std::max(o, c);
    DT l = std::min(o, c);
    return createTimeSeriesEntry(dt, o, h, l, c, 1);
  };

  // Two 2-bar sequences
  auto b1 = mkBar(2020, Jan, 1, "100.00", "105.00"); // closed trade: entry
  auto b2 = mkBar(2020, Jan, 2, "105.00", "110.00"); // closed trade: exit
  auto b3 = mkBar(2020, Jan, 3, "200.00", "202.00"); // open trade: entry
  auto b4 = mkBar(2020, Jan, 4, "202.00", "210.00"); // open trade: second bar

  auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  ts->addEntry(*b1); ts->addEntry(*b2); ts->addEntry(*b3); ts->addEntry(*b4);

  auto portfolio = std::make_shared<Portfolio<DT>>("port");
  portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("50.0"), createDecimal("0.25"), ts));

  auto pattern = createLongPattern1();
  auto strat = std::make_shared<PalLongStrategy<DT>>("long-mixed", pattern, portfolio);

  DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 4));
  bt.addStrategy(strat);

  // Inject CLOSED 2-bar trade
  {
    auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
    pos->addBar(*b2);
    pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
    auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
    closedHist.addClosedPosition(pos);
  }
  // Inject OPEN 2-bar trade
  {
    auto posO = std::make_shared<TradingPositionLong<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
    posO->addBar(*b4);
    auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
    instrPos.addPosition(posO);
  }

  // Baseline returns for cross-check
  auto onlyR = bt.getAllHighResReturns(strat.get());
  REQUIRE(onlyR.size() == 4);

  // With dates
  auto allWithDates = bt.getAllHighResReturnsWithDates(strat.get());
  REQUIRE(allWithDates.size() == onlyR.size());

  // Timestamp order should correspond to the bar sequence we fed (Jan 1 → Jan 4)
  REQUIRE(allWithDates.size() == 4);
  REQUIRE(allWithDates[0].first.date() == b1->getDateValue());
  REQUIRE(allWithDates[1].first.date() == b2->getDateValue());
  REQUIRE(allWithDates[2].first.date() == b3->getDateValue());
  REQUIRE(allWithDates[3].first.date() == b4->getDateValue());

  // Values identical to getAllHighResReturns
  for (size_t i = 0; i < allWithDates.size(); ++i) {
    REQUIRE(allWithDates[i].second == onlyR[i]);
  }

  // Spot-check math
  REQUIRE(allWithDates[0].second == (b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue());
  REQUIRE(allWithDates[1].second == (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue());
  REQUIRE(allWithDates[2].second == (b3->getCloseValue() - b3->getOpenValue()) / b3->getOpenValue());
  REQUIRE(allWithDates[3].second == (b4->getCloseValue() - b3->getCloseValue()) / b3->getCloseValue());
}

SECTION("getAllHighResReturnsWithDates — only closed long positions") {
  using DT = DecimalType;
  const std::string sym = "@ONLY_CLOSED_LONG";
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto mkBar = [&](int Y,int M,int D, const std::string& openStr, const std::string& closeStr) {
    TimeSeriesDate dt(Y, M, D);
    DT o = createDecimal(openStr), c = createDecimal(closeStr);
    DT h = std::max(o, c), l = std::min(o, c);
    return createTimeSeriesEntry(dt, o, h, l, c, 1);
  };

  auto b1 = mkBar(2020, Jan, 1, "100.00", "104.00");
  auto b2 = mkBar(2020, Jan, 2, "104.00", "120.00");

  auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  ts->addEntry(*b1); ts->addEntry(*b2);

  auto portfolio = std::make_shared<Portfolio<DT>>("port");
  portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

  auto pattern = createLongPattern1();
  auto strat = std::make_shared<PalLongStrategy<DT>>("long-closed", pattern, portfolio);

  DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
  bt.addStrategy(strat);

  {
    auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
    pos->addBar(*b2);
    pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
    auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
    closedHist.addClosedPosition(pos);
  }

  auto allWithDates = bt.getAllHighResReturnsWithDates(strat.get());
  REQUIRE(allWithDates.size() == 2);
  REQUIRE(allWithDates[0].first.date() == b1->getDateValue());
  REQUIRE(allWithDates[1].first.date() == b2->getDateValue());

  REQUIRE(allWithDates[0].second == (b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue());
  REQUIRE(allWithDates[1].second == (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue());
}

SECTION("getAllHighResReturnsWithDates — mixed: one closed + one open SHORT trade") {
  using DT = DecimalType;
  const std::string sym = "@MIXED_SHORT";
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto mkBar = [&](int Y,int M,int D, const std::string& openStr, const std::string& closeStr) {
    TimeSeriesDate dt(Y, M, D);
    DT o = createDecimal(openStr), c = createDecimal(closeStr);
    DT h = std::max(o, c), l = std::min(o, c);
    return createTimeSeriesEntry(dt, o, h, l, c, 1);
  };

  auto b1 = mkBar(2020, Jan, 1, "100.00", "95.00");   // closed short: entry
  auto b2 = mkBar(2020, Jan, 2, "95.00",  "90.00");   // closed short: exit
  auto b3 = mkBar(2020, Jan, 3, "200.00", "205.00");  // open short: entry (loss)
  auto b4 = mkBar(2020, Jan, 4, "205.00", "210.00");  // open short: 2nd bar (loss)

  auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  ts->addEntry(*b1); ts->addEntry(*b2); ts->addEntry(*b3); ts->addEntry(*b4);

  auto portfolio = std::make_shared<Portfolio<DT>>("port");
  portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("50.0"), createDecimal("0.25"), ts));

  auto pattern = createShortPattern1();
  auto strat = std::make_shared<PalShortStrategy<DT>>("short-mixed", pattern, portfolio);

  DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 4));
  bt.addStrategy(strat);

  // Closed short
  {
    auto pos = std::make_shared<TradingPositionShort<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
    pos->addBar(*b2);
    pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
    auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
    closedHist.addClosedPosition(pos);
  }
  // Open short
  {
    auto posO = std::make_shared<TradingPositionShort<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
    posO->addBar(*b4);
    auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
    instrPos.addPosition(posO);
  }

  auto onlyR = bt.getAllHighResReturns(strat.get());
  auto allWithDates = bt.getAllHighResReturnsWithDates(strat.get());

  REQUIRE(allWithDates.size() == 4);
  REQUIRE(allWithDates[0].first.date() == b1->getDateValue());
  REQUIRE(allWithDates[1].first.date() == b2->getDateValue());
  REQUIRE(allWithDates[2].first.date() == b3->getDateValue());
  REQUIRE(allWithDates[3].first.date() == b4->getDateValue());

  // Cross-check equals non-dated version
  for (size_t i = 0; i < allWithDates.size(); ++i) REQUIRE(allWithDates[i].second == onlyR[i]);

  // Short sign convention spot checks
  REQUIRE(allWithDates[0].second == -((b1->getCloseValue() - b1->getOpenValue()) / b1->getOpenValue()));
  REQUIRE(allWithDates[1].second == -((b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue()));
  REQUIRE(allWithDates[2].second == -((b3->getCloseValue() - b3->getOpenValue()) / b3->getOpenValue()));
  REQUIRE(allWithDates[3].second == -((b4->getCloseValue() - b3->getCloseValue()) / b3->getCloseValue()));
}
 
 //

// In BackTesterTest.cpp, replace the old section with this.

 SECTION("AllHighResLogPFPolicy with five closed 5-bar positions") {
   using DT = DecimalType;
   const std::string sym = "@POLICY";
   TradingVolume oneContract(1, TradingVolume::CONTRACTS);

   // 1) Helper to build bars remains the same
   auto mkBar = [&](int dayOffset, DT closeVal) {
     TimeSeriesDate dt(2020, Jan, 1 + dayOffset);
     return createTimeSeriesEntry(dt, closeVal, closeVal, closeVal, closeVal, 1);
   };

   // 2) Create bar data remains the same
   std::vector<std::shared_ptr<OHLCTimeSeriesEntry<DT>>> bars;
   for (int pos = 0; pos < 5; ++pos) {
     for (int j = 0; j < 5; ++j) {
       DT price = (j % 2 == 0) ? createDecimal("100.0") : createDecimal("200.0");
       bars.push_back(mkBar(pos * 5 + j, price));
     }
   }
    
   // 3) Setup portfolio and strategy remains the same
   auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
   for (auto& b : bars) ts->addEntry(*b);

   auto portfolio = std::make_shared<Portfolio<DT>>("policy-port");
   portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));
    
   auto pattern = createLongPattern1();
   auto strat = std::make_shared<PalLongStrategy<DT>>("policy-test", pattern, portfolio);

   // 4) --- USE THE MOCK BACKTESTER ---
   auto bt = std::make_shared<MockMonteCarloBackTester<DT>>();
   bt->addStrategy(strat);
   // Manually tell the mock what to report for our checks
   bt->setExpectedTrades(5); // We are manually creating 5 trades
   bt->setExpectedBars(25); // 5 trades * 5 bars each

   // 5) Inject the five closed positions into the history (same as before)
   for (int pos = 0; pos < 5; ++pos) {
     auto entryBar = bars[pos * 5 + 0];
     auto exitBar  = bars[pos * 5 + 4];
     auto p = std::make_shared<TradingPositionLong<DT>>(sym, entryBar->getCloseValue(), *entryBar, oneContract);
     for (int j = 1; j <= 4; ++j)
       p->addBar(*bars[pos * 5 + j]);
     p->ClosePosition(exitBar->getDateValue(), exitBar->getCloseValue());
        
     auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
     closedHist.addClosedPosition(p);
   }

   // 6) Compute the statistic. The test should now pass.
   // The policy will call our mock's getNumTrades(), which will return 5.
   // The condition '5 < 3' will be false, and the real statistic will be calculated.
   DT stat = AllHighResLogPFPolicy<DT>::getPermutationTestStatistic(bt);

   // Expected value is log(1 + 1.0) = log(2) = 0.69314718...
   // The alternating 100->200 and 200->100 returns create a log profit factor of 1.
   REQUIRE(stat == createDecimal("1.0"));
 }
}

TEST_CASE("BackTester::getEstimatedAnnualizedTrades", "[BackTester]") {

    // A simple mock class to control the inputs for the method under test.
    // This isolates the test from the complexities of a full backtest run.
    struct MockAnnualizationBackTester : public DailyBackTester<DecimalType> {
        uint32_t m_num_trades = 0;

        // Constructor that calls the base class constructor to set the date range
        MockAnnualizationBackTester(date start_date, date end_date)
            : DailyBackTester<DecimalType>(start_date, end_date) {}

        // Override only the method that we need to control for the test
        uint32_t getNumTrades() const override {
            return m_num_trades;
        }
    };

    SECTION("Two-year period with 100 trades") {
        // Use non-weekend dates to avoid constructor adjustments
        date start = date(2020, Jan, 1); // Wednesday
        date end = date(2022, Jan, 1);   // Saturday -> will be adjusted to 2021-Dec-31
        MockAnnualizationBackTester bt(start, end);
        bt.m_num_trades = 100;

        // Calculate duration based on the backtester's actual dates
        double duration_in_days = (bt.getEndDate() - bt.getStartDate()).days();
        double duration_in_years = duration_in_days / 365.25;
        double expected_annual_trades = 100.0 / duration_in_years;

        REQUIRE(bt.getEstimatedAnnualizedTrades() == Catch::Approx(expected_annual_trades));
    }

    SECTION("Six-month period with 25 trades") {
        // Use non-weekend dates
        date start = date(2021, Jan, 1); // Friday
        date end = date(2021, Jul, 1);   // Thursday
        MockAnnualizationBackTester bt(start, end);
        bt.m_num_trades = 25;

        double duration_in_days = (bt.getEndDate() - bt.getStartDate()).days();
        double duration_in_years = duration_in_days / 365.25;
        double expected_annual_trades = 25.0 / duration_in_years;
        REQUIRE(bt.getEstimatedAnnualizedTrades() == Catch::Approx(expected_annual_trades));
    }

    SECTION("One-year period with 1 trade") {
        // Use non-weekend dates
        date start = date(2021, Jan, 1); // Friday
        date end = date(2022, Jan, 1);   // Saturday -> becomes 2021-Dec-31
        MockAnnualizationBackTester bt(start, end);
        bt.m_num_trades = 1;

        double duration_in_days = (bt.getEndDate() - bt.getStartDate()).days();
        double duration_in_years = duration_in_days / 365.25;
        double expected_annual_trades = 1.0 / duration_in_years;
        REQUIRE(bt.getEstimatedAnnualizedTrades() == Catch::Approx(expected_annual_trades));
    }
    
    SECTION("Zero or negative duration throws exception") {
        // Test case 1: Constructor should throw if end < start
        // 2022-Jan-01 (Sat) -> becomes 2021-Dec-31 (Fri)
        // 2022-Jan-02 (Sun) -> becomes 2022-Jan-03 (Mon)
        // So start > end, which is invalid for DateRange constructor
        REQUIRE_THROWS_AS(MockAnnualizationBackTester(date(2022, Jan, 2), date(2022, Jan, 1)), std::runtime_error);

        // Test case 2: Method should throw if duration is zero
        // Use a valid, non-weekend date for start and end
        MockAnnualizationBackTester bt(date(2022, Jan, 3), date(2022, Jan, 3));
        bt.m_num_trades = 10;
        REQUIRE_THROWS_AS(bt.getEstimatedAnnualizedTrades(), BackTesterException);
    }
}

TEST_CASE("BackTester::getProfitFactor", "[BackTester]") {
    using DT = DecimalType;
    const std::string sym = "@TEST_PF";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y, int M, int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        DT h = std::max(o, c);
        DT l = std::min(o, c);
        return createTimeSeriesEntry(dt, o, h, l, c, 1);
    };

    SECTION("getProfitFactor with no strategies throws exception") {
        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 5));
        
        REQUIRE_THROWS_AS(bt.getProfitFactor(), BackTesterException);
    }

    SECTION("getProfitFactor with mixed winning and losing trades") {
        // Create bars with mixed returns: +10%, -5%, +15%, -8%
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // +10% return
        auto b2 = mkBar(2020, Jan, 2, "110.00", "104.50"); // -5% return
        auto b3 = mkBar(2020, Jan, 3, "104.50", "120.18"); // +15% return
        auto b4 = mkBar(2020, Jan, 4, "120.18", "110.57"); // -8% return

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);
        ts->addEntry(*b3);
        ts->addEntry(*b4);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 4));
        bt.addStrategy(strat);

        // Inject a closed position that spans all bars
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->addBar(*b3);
            pos->addBar(*b4);
            pos->ClosePosition(b4->getDateValue(), b4->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        DT profitFactor = bt.getProfitFactor();
        
        // Expected calculation:
        // Returns: +0.10, -0.05, +0.15, -0.08
        // Gross wins: 0.10 + 0.15 = 0.25
        // Gross losses: |-0.05| + |-0.08| = 0.13
        // Profit Factor: 0.25 / 0.13 ≈ 1.923
        DT expectedPF = createDecimal("0.25") / createDecimal("0.13");
        REQUIRE(num::to_double(profitFactor) == Catch::Approx(num::to_double(expectedPF)).epsilon(0.001));
    }

    SECTION("getProfitFactor with all winning trades") {
        // Create bars with only positive returns
        auto b1 = mkBar(2020, Jan, 1, "100.00", "105.00"); // +5% return
        auto b2 = mkBar(2020, Jan, 2, "105.00", "110.25"); // +5% return

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Inject a closed position with only winning bars
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        DT profitFactor = bt.getProfitFactor();
        
        // When there are no losses, StatUtils returns 100.0 as the profit factor
        REQUIRE(profitFactor == createDecimal("100.0"));
    }

    SECTION("getProfitFactor with all losing trades") {
        // Create bars with only negative returns
        auto b1 = mkBar(2020, Jan, 1, "100.00", "95.00"); // -5% return
        auto b2 = mkBar(2020, Jan, 2, "95.00", "90.25");  // -5% return

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Inject a closed position with only losing bars
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        DT profitFactor = bt.getProfitFactor();
        
        // When there are no wins, profit factor should be 0
        REQUIRE(profitFactor == DecimalConstants<DT>::DecimalZero);
    }

    SECTION("getProfitFactor with short positions") {
        // Test with short positions where returns are inverted
        auto b1 = mkBar(2020, Jan, 1, "100.00", "95.00");  // Price down = profit for short
        auto b2 = mkBar(2020, Jan, 2, "95.00", "105.00");  // Price up = loss for short

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createShortPattern1();
        auto strat = std::make_shared<PalShortStrategy<DT>>("test-short-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Inject a closed short position
        {
            auto pos = std::make_shared<TradingPositionShort<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        DT profitFactor = bt.getProfitFactor();
        
        // For short positions:
        // Bar 1: -1 * (95-100)/100 = +0.05 (profit)
        // Bar 2: -1 * (105-95)/95 = -0.1053 (loss)
        // Profit Factor = 0.05 / 0.1053 ≈ 0.475
        DT expectedWin = createDecimal("0.05");
        DT expectedLoss = createDecimal("105.00") - createDecimal("95.00");
        expectedLoss = expectedLoss / createDecimal("95.00"); // 0.1053
        DT expectedPF = expectedWin / expectedLoss;
        
        REQUIRE(num::to_double(profitFactor) == Catch::Approx(num::to_double(expectedPF)).epsilon(0.01));
    }
}

TEST_CASE("BackTester::getProfitability", "[BackTester]") {
    using DT = DecimalType;
    const std::string sym = "@TEST_PROF";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y, int M, int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        DT h = std::max(o, c);
        DT l = std::min(o, c);
        return createTimeSeriesEntry(dt, o, h, l, c, 1);
    };

    SECTION("getProfitability with no strategies throws exception") {
        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 5));
        
        REQUIRE_THROWS_AS(bt.getProfitability(), BackTesterException);
    }

    SECTION("getProfitability with mixed trades") {
        // Create a scenario with known profit factor and payoff ratio
        // 2 winning trades: +20%, +10% (avg win = 15%)
        // 2 losing trades: -10%, -5% (avg loss = 7.5%)
        auto b1 = mkBar(2020, Jan, 1, "100.00", "120.00"); // +20% return
        auto b2 = mkBar(2020, Jan, 2, "120.00", "108.00");  // -10% return
        auto b3 = mkBar(2020, Jan, 3, "108.00", "118.80");  // +10% return
        auto b4 = mkBar(2020, Jan, 4, "118.80", "112.86");  // -5% return

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);
        ts->addEntry(*b3);
        ts->addEntry(*b4);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 4));
        bt.addStrategy(strat);

        // Inject a closed position spanning all bars
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->addBar(*b3);
            pos->addBar(*b4);
            pos->ClosePosition(b4->getDateValue(), b4->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        auto [profitFactor, profitability] = bt.getProfitability();
        
        // Expected calculation:
        // Returns: +0.20, -0.10, +0.10, -0.05
        // Gross wins: 0.20 + 0.10 = 0.30 (2 trades)
        // Gross losses: 0.10 + 0.05 = 0.15 (2 trades)
        // Profit Factor: 0.30 / 0.15 = 2.0
        // Average win: 0.30 / 2 = 0.15
        // Average loss: 0.15 / 2 = 0.075
        // Payoff Ratio (Rwl): 0.15 / 0.075 = 2.0
        // Profitability: 100 * PF / (PF + Rwl) = 100 * 2.0 / (2.0 + 2.0) = 50%
        
        DT expectedPF = createDecimal("2.0");
        DT expectedProf = createDecimal("50.0");
        
        REQUIRE(num::to_double(profitFactor) == Catch::Approx(num::to_double(expectedPF)).epsilon(0.001));
        REQUIRE(num::to_double(profitability) == Catch::Approx(num::to_double(expectedProf)).epsilon(0.001));
    }

    SECTION("getProfitability with all winning trades") {
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // +10% return
        auto b2 = mkBar(2020, Jan, 2, "110.00", "121.00"); // +10% return

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        auto [profitFactor, profitability] = bt.getProfitability();
        
        // With no losses: PF = 100, Rwl = 0, Profitability = 100 * 100 / (100 + 0) = 100%
        REQUIRE(profitFactor == createDecimal("100.0"));
        REQUIRE(profitability == createDecimal("100.0"));
    }

    SECTION("getProfitability with all losing trades") {
        auto b1 = mkBar(2020, Jan, 1, "100.00", "90.00");  // -10% return
        auto b2 = mkBar(2020, Jan, 2, "90.00", "81.00");   // -10% return

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        auto [profitFactor, profitability] = bt.getProfitability();
        
        // With no wins: PF = 0, profitability = 0
        REQUIRE(profitFactor == DecimalConstants<DT>::DecimalZero);
        REQUIRE(profitability == DecimalConstants<DT>::DecimalZero);
    }

    SECTION("getProfitability with empty returns") {
        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        
        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // No positions added - should return zero values
        auto [profitFactor, profitability] = bt.getProfitability();
        
        REQUIRE(profitFactor == DecimalConstants<DT>::DecimalZero);
        REQUIRE(profitability == DecimalConstants<DT>::DecimalZero);
    }

    SECTION("getProfitability integration with open positions") {
        // Test that both closed and open positions are included in the calculation
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // +10% return (closed)
        auto b2 = mkBar(2020, Jan, 2, "110.00", "99.00");  // -10% return (closed)
        auto b3 = mkBar(2020, Jan, 3, "200.00", "220.00"); // +10% return (open)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);
        ts->addEntry(*b3);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 3));
        bt.addStrategy(strat);

        // Add closed position
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        // Add open position
        {
            auto posO = std::make_shared<TradingPositionLong<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
            auto& instrPos = const_cast<InstrumentPosition<DT>&>(strat->getStrategyBroker().getInstrumentPosition(sym));
            instrPos.addPosition(posO);
        }

        auto [profitFactor, profitability] = bt.getProfitability();
        
        // Returns: +0.10, -0.10, +0.10
        // Gross wins: 0.20 (2 trades), Gross losses: 0.10 (1 trade)
        // PF = 0.20 / 0.10 = 2.0
        // Avg win = 0.10, Avg loss = 0.10, Rwl = 1.0
        // Profitability = 100 * 2.0 / (2.0 + 1.0) = 66.67%
        
        DT expectedPF = createDecimal("2.0");
        DT expectedProf = createDecimal("100.0") * createDecimal("2.0") / createDecimal("3.0"); // 66.67
        
        REQUIRE(num::to_double(profitFactor) == Catch::Approx(num::to_double(expectedPF)).epsilon(0.001));
        REQUIRE(num::to_double(profitability) == Catch::Approx(num::to_double(expectedProf)).epsilon(0.01));
    }
}

TEST_CASE("BackTester::getNumConsecutiveLosses", "[BackTester]") {
    using DT = DecimalType;
    const std::string sym = "@TEST_CONSEC";
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);

    auto mkBar = [&](int Y, int M, int D, const std::string& openStr, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT o = createDecimal(openStr);
        DT c = createDecimal(closeStr);
        DT h = std::max(o, c);
        DT l = std::min(o, c);
        return createTimeSeriesEntry(dt, o, h, l, c, 1);
    };

    SECTION("getNumConsecutiveLosses with no strategies throws exception") {
        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 5));
        
        REQUIRE_THROWS_AS(bt.getNumConsecutiveLosses(), BackTesterException);
    }

    SECTION("getNumConsecutiveLosses with no trades returns 0") {
        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        
        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // No positions added - should return 0
        REQUIRE(bt.getNumConsecutiveLosses() == 0);
    }

    SECTION("getNumConsecutiveLosses with single winning trade") {
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // Entry bar
        auto b2 = mkBar(2020, Jan, 2, "110.00", "120.00"); // Exit bar (winning)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Create a winning long position
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        REQUIRE(bt.getNumConsecutiveLosses() == 0);
    }

    SECTION("getNumConsecutiveLosses with single losing trade") {
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // Entry bar
        auto b2 = mkBar(2020, Jan, 2, "110.00", "90.00");  // Exit bar (losing)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Create a losing long position
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        REQUIRE(bt.getNumConsecutiveLosses() == 1);
    }

    SECTION("getNumConsecutiveLosses with multiple consecutive losses") {
        // Create bars for 3 consecutive losing trades
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // Trade 1 entry
        auto b2 = mkBar(2020, Jan, 2, "110.00", "90.00");  // Trade 1 exit (loss)
        auto b3 = mkBar(2020, Jan, 3, "200.00", "210.00"); // Trade 2 entry
        auto b4 = mkBar(2020, Jan, 4, "210.00", "180.00"); // Trade 2 exit (loss)
        auto b5 = mkBar(2020, Jan, 5, "300.00", "310.00"); // Trade 3 entry
        auto b6 = mkBar(2020, Jan, 6, "310.00", "280.00"); // Trade 3 exit (loss)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);
        ts->addEntry(*b3);
        ts->addEntry(*b4);
        ts->addEntry(*b5);
        ts->addEntry(*b6);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 6));
        bt.addStrategy(strat);

        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());

        // Add first losing trade
        {
            auto pos1 = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos1->addBar(*b2);
            pos1->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            closedHist.addClosedPosition(pos1);
        }

        // Add second losing trade
        {
            auto pos2 = std::make_shared<TradingPositionLong<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
            pos2->addBar(*b4);
            pos2->ClosePosition(b4->getDateValue(), b4->getCloseValue());
            closedHist.addClosedPosition(pos2);
        }

        // Add third losing trade
        {
            auto pos3 = std::make_shared<TradingPositionLong<DT>>(sym, b5->getOpenValue(), *b5, oneContract);
            pos3->addBar(*b6);
            pos3->ClosePosition(b6->getDateValue(), b6->getCloseValue());
            closedHist.addClosedPosition(pos3);
        }

        REQUIRE(bt.getNumConsecutiveLosses() == 3);
    }

    SECTION("getNumConsecutiveLosses resets after winning trade") {
        // Create bars for: Loss, Loss, Win sequence
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // Trade 1 entry
        auto b2 = mkBar(2020, Jan, 2, "110.00", "90.00");  // Trade 1 exit (loss)
        auto b3 = mkBar(2020, Jan, 3, "200.00", "210.00"); // Trade 2 entry
        auto b4 = mkBar(2020, Jan, 4, "210.00", "180.00"); // Trade 2 exit (loss)
        auto b5 = mkBar(2020, Jan, 5, "300.00", "310.00"); // Trade 3 entry
        auto b6 = mkBar(2020, Jan, 6, "310.00", "350.00"); // Trade 3 exit (win)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);
        ts->addEntry(*b3);
        ts->addEntry(*b4);
        ts->addEntry(*b5);
        ts->addEntry(*b6);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 6));
        bt.addStrategy(strat);

        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());

        // Add first losing trade
        {
            auto pos1 = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos1->addBar(*b2);
            pos1->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            closedHist.addClosedPosition(pos1);
        }

        // Add second losing trade
        {
            auto pos2 = std::make_shared<TradingPositionLong<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
            pos2->addBar(*b4);
            pos2->ClosePosition(b4->getDateValue(), b4->getCloseValue());
            closedHist.addClosedPosition(pos2);
        }

        // Add winning trade (should reset counter)
        {
            auto pos3 = std::make_shared<TradingPositionLong<DT>>(sym, b5->getOpenValue(), *b5, oneContract);
            pos3->addBar(*b6);
            pos3->ClosePosition(b6->getDateValue(), b6->getCloseValue());
            closedHist.addClosedPosition(pos3);
        }

        REQUIRE(bt.getNumConsecutiveLosses() == 0);
    }

    SECTION("getNumConsecutiveLosses with short positions") {
        // Test with short positions where price movements are inverted
        auto b1 = mkBar(2020, Jan, 1, "100.00", "95.00");  // Short entry (price down = profit)
        auto b2 = mkBar(2020, Jan, 2, "95.00", "105.00");  // Short exit (price up = loss)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createShortPattern1();
        auto strat = std::make_shared<PalShortStrategy<DT>>("test-short-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Create a losing short position (price went up)
        {
            auto pos = std::make_shared<TradingPositionShort<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        REQUIRE(bt.getNumConsecutiveLosses() == 1);
    }

    SECTION("getNumConsecutiveLosses with mixed win/loss pattern") {
        // Test pattern: Win, Loss, Loss, Win, Loss
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // Trade 1 entry
        auto b2 = mkBar(2020, Jan, 2, "110.00", "120.00"); // Trade 1 exit (win)
        auto b3 = mkBar(2020, Jan, 3, "200.00", "210.00"); // Trade 2 entry
        auto b4 = mkBar(2020, Jan, 4, "210.00", "180.00"); // Trade 2 exit (loss)
        auto b5 = mkBar(2020, Jan, 5, "300.00", "310.00"); // Trade 3 entry
        auto b6 = mkBar(2020, Jan, 6, "310.00", "280.00"); // Trade 3 exit (loss)
        auto b7 = mkBar(2020, Jan, 7, "400.00", "410.00"); // Trade 4 entry
        auto b8 = mkBar(2020, Jan, 8, "410.00", "450.00"); // Trade 4 exit (win)
        auto b9 = mkBar(2020, Jan, 9, "500.00", "510.00"); // Trade 5 entry
        auto b10 = mkBar(2020, Jan, 10, "510.00", "480.00"); // Trade 5 exit (loss)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);
        ts->addEntry(*b3);
        ts->addEntry(*b4);
        ts->addEntry(*b5);
        ts->addEntry(*b6);
        ts->addEntry(*b7);
        ts->addEntry(*b8);
        ts->addEntry(*b9);
        ts->addEntry(*b10);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 10));
        bt.addStrategy(strat);

        auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());

        // Add trades in sequence: Win, Loss, Loss, Win, Loss
        {
            auto pos1 = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos1->addBar(*b2);
            pos1->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            closedHist.addClosedPosition(pos1);
        }

        {
            auto pos2 = std::make_shared<TradingPositionLong<DT>>(sym, b3->getOpenValue(), *b3, oneContract);
            pos2->addBar(*b4);
            pos2->ClosePosition(b4->getDateValue(), b4->getCloseValue());
            closedHist.addClosedPosition(pos2);
        }

        {
            auto pos3 = std::make_shared<TradingPositionLong<DT>>(sym, b5->getOpenValue(), *b5, oneContract);
            pos3->addBar(*b6);
            pos3->ClosePosition(b6->getDateValue(), b6->getCloseValue());
            closedHist.addClosedPosition(pos3);
        }

        {
            auto pos4 = std::make_shared<TradingPositionLong<DT>>(sym, b7->getOpenValue(), *b7, oneContract);
            pos4->addBar(*b8);
            pos4->ClosePosition(b8->getDateValue(), b8->getCloseValue());
            closedHist.addClosedPosition(pos4);
        }

        {
            auto pos5 = std::make_shared<TradingPositionLong<DT>>(sym, b9->getOpenValue(), *b9, oneContract);
            pos5->addBar(*b10);
            pos5->ClosePosition(b10->getDateValue(), b10->getCloseValue());
            closedHist.addClosedPosition(pos5);
        }

        // After Win, Loss, Loss, Win, Loss sequence, consecutive losses should be 1
        REQUIRE(bt.getNumConsecutiveLosses() == 1);
    }

    SECTION("getNumConsecutiveLosses integration with existing methods") {
        // Test that the method works correctly alongside other BackTester methods
        auto b1 = mkBar(2020, Jan, 1, "100.00", "110.00"); // Entry
        auto b2 = mkBar(2020, Jan, 2, "110.00", "90.00");  // Exit (loss)

        auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
        ts->addEntry(*b1);
        ts->addEntry(*b2);

        auto portfolio = std::make_shared<Portfolio<DT>>("test-portfolio");
        portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym, createDecimal("1.0"), createDecimal("0.01"), ts));

        auto pattern = createLongPattern1();
        auto strat = std::make_shared<PalLongStrategy<DT>>("test-strategy", pattern, portfolio);

        DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
        bt.addStrategy(strat);

        // Create a losing position
        {
            auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getOpenValue(), *b1, oneContract);
            pos->addBar(*b2);
            pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());
            auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(strat->getStrategyBroker().getClosedPositionHistory());
            closedHist.addClosedPosition(pos);
        }

        // Verify that all methods work correctly together
        REQUIRE(bt.getNumConsecutiveLosses() == 1);
        REQUIRE(bt.getClosedPositionHistory().getNumPositions() == 1);
        REQUIRE(bt.getClosedPositionHistory().getNumLosingPositions() == 1);
        REQUIRE(bt.getClosedPositionHistory().getNumWinningPositions() == 0);
        
        // Verify that the method delegates correctly to ClosedPositionHistory
        REQUIRE(bt.getNumConsecutiveLosses() == bt.getClosedPositionHistory().getNumConsecutiveLosses());
    }
}
