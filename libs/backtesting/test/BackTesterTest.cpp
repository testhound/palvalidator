#include <catch2/catch_test_macros.hpp>
#include <cpptrace/from_current.hpp>
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
createShortPattern1();

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

    CPPTRACE_TRY
      {
	palLongBacktester1.backtest();
      }
    CPPTRACE_CATCH(const std::exception& e) {
        std::cerr<<"Exception: "<<e.what()<<std::endl;
        cpptrace::from_current_exception().print();
    }

    BackTester<DecimalType>::StrategyIterator it = palLongBacktester1.beginStrategies();

    REQUIRE (it != palLongBacktester1.endStrategies());
    std::shared_ptr<BacktesterStrategy<DecimalType>> aStrategy1 = (*it);

    StrategyBroker<DecimalType> aBroker = aStrategy1->getStrategyBroker();
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

    StrategyBroker<DecimalType> aBroker2 = aStrategy2->getStrategyBroker();
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

        auto mkBar = [&](int Y,int M,int D, const std::string& closeStr) {
        TimeSeriesDate dt(Y, M, D);
        DT c = createDecimal(closeStr);
        return createTimeSeriesEntry(
            dt,
            c,                         // open
            c + createDecimal("0.50"), // high
            c - createDecimal("0.50"), // low
            c,                         // close
            1                          // volume
        );
    };

        // 5) Create four bars: B1→B2 (closed), B3→B4 (open)
    auto b1 = mkBar(2020, Jan, 1, "100.00");
    auto b2 = mkBar(2020, Jan, 2, "110.00");
    auto b3 = mkBar(2020, Jan, 3, "200.00");
    auto b4 = mkBar(2020, Jan, 4, "210.00");

    // 1) Build a shared portfolio with one FuturesSecurity on sym
    auto portfolio = std::make_shared<Portfolio<DT>>("port");
    auto ts = std::make_shared<OHLCTimeSeries<DT>>(
                   TimeFrame::DAILY,
                   TradingVolume::CONTRACTS
               );
    ts->addEntry(*b1);
    ts->addEntry(*b2);
    ts->addEntry(*b3);
    ts->addEntry(*b4);

    portfolio->addSecurity(
        std::make_shared<FuturesSecurity<DT>>(
            sym, sym,
            createDecimal("50.0"),   // big-point
            createDecimal("0.25"),  // tick
            ts
        )
    );

    // 2) Instantiate a PalLongStrategy with a real pattern :contentReference[oaicite:0]{index=0}&#8203;:contentReference[oaicite:1]{index=1}
    auto pattern = createLongPattern1();  // your helper for a simple firing pattern
    auto strat = std::make_shared<PalLongStrategy<DT>>(
                     "test-long", pattern, portfolio
                 );

    // 3) Wire into a concrete backtester with exact dates :contentReference[oaicite:2]{index=2}&#8203;:contentReference[oaicite:3]{index=3}
    DailyBackTester<DT> bt(
        TimeSeriesDate(2020, Jan, 1),
        TimeSeriesDate(2020, Jan, 4)
    );
    bt.addStrategy(strat);

    // 4) Helper to build a one-bar OHLC entry


    // 6) Seed the series so it’s never empty :contentReference[oaicite:4]{index=4}&#8203;:contentReference[oaicite:5]{index=5}

    // 7) Inject a CLOSED 2-bar trade: (100→110) ⇒ 0.10
    {
        auto pos = std::make_shared<TradingPositionLong<DT>>(
                       sym,
                       b1->getCloseValue(),
                       *b1,
                       oneContract
                   );
        pos->addBar(*b2);
        pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());

        // const_cast the const history returned by StrategyBroker :contentReference[oaicite:6]{index=6}&#8203;:contentReference[oaicite:7]{index=7}
        auto& closedHist = const_cast<
            ClosedPositionHistory<DT>&
        >( strat->getStrategyBroker().getClosedPositionHistory() );
        closedHist.addClosedPosition(pos);
    }

    // 8) Inject a STILL-OPEN 2-bar trade: (200→210) ⇒ 0.05
    {
        auto posO = std::make_shared<TradingPositionLong<DT>>(
                        sym,
                        b3->getCloseValue(),
                        *b3,
                        oneContract
                    );
        posO->addBar(*b4);

        auto& instrPos = const_cast<
            InstrumentPosition<DT>&
        >( strat->getStrategyBroker().getInstrumentPosition(sym) );
        instrPos.addPosition(posO);
    }

    // 9) Call getAllHighResReturns(...) and verify exactly two returns
    // we are now measuring the entry bar as well so the number of bars
    // has increased
    auto allR = bt.getAllHighResReturns(strat.get());
    REQUIRE(allR.size() == 4);

    // we only get one bar‐by‐bar return (entry→bar2)
    REQUIRE (allR[0] == ((b1->getCloseValue() - b1->getOpenValue())/b1->getOpenValue()));
    
    // closed: (110–100)/100 = 0.10
    REQUIRE(allR[1] ==
        (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue()
    );

    REQUIRE (allR[2] == ((b3->getCloseValue() - b3->getOpenValue())/b3->getOpenValue()));
    // open:   (210–200)/200 = 0.05
    REQUIRE(allR[3] ==
        (b4->getCloseValue() - b3->getCloseValue()) / b3->getCloseValue()
    );
 }

 SECTION("getAllHighResReturns with only closed positions") {
   using DT = DecimalType;
   const std::string sym = "@TEST";
   TradingVolume oneContract(1, TradingVolume::CONTRACTS);

   // 1) Build two bars for a single closed trade
   auto mkBar = [&](int Y,int M,int D, const std::string& closeStr) {
     TimeSeriesDate dt(Y, M, D);
     DT c = createDecimal(closeStr);
     return createTimeSeriesEntry(
				  dt,
				  c,
				  c + createDecimal("0.50"),
				  c - createDecimal("0.50"),
				  c,
				  1
				  );
   };
   auto b1 = mkBar(2020, Jan, 1, "100.00");
   auto b2 = mkBar(2020, Jan, 2, "120.00");

   // 2) Seed the OHLC series immediately
   auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
   ts->addEntry(*b1);
   ts->addEntry(*b2);

   // 3) Build portfolio + security
   auto portfolio = std::make_shared<Portfolio<DT>>("port");
   portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym,
								createDecimal("1.0"), createDecimal("0.01"), ts));

   // 4) PalLongStrategy with a trivial pattern
   auto pattern = createLongPattern1();
   auto strat = std::make_shared<PalLongStrategy<DT>>("only-closed", pattern, portfolio);

   // 5) Wire up backtester
   DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 1), TimeSeriesDate(2020, Jan, 2));
   bt.addStrategy(strat);

   // 6) Inject exactly one CLOSED 2-bar trade: (100→120) ⇒ return = 0.20
   {
     auto pos = std::make_shared<TradingPositionLong<DT>>(sym, b1->getCloseValue(), *b1, oneContract);
     pos->addBar(*b2);
     pos->ClosePosition(b2->getDateValue(), b2->getCloseValue());

     auto& closedHist = const_cast<ClosedPositionHistory<DT>&>(
							       strat->getStrategyBroker().getClosedPositionHistory()
							       );
     closedHist.addClosedPosition(pos);
   }

   // 7) Call getAllHighResReturns and verify one return, that covers two bars
   auto allR = bt.getAllHighResReturns(strat.get());
   REQUIRE(allR.size() == 2);
   REQUIRE (allR[0] == ((b1->getCloseValue() - b1->getOpenValue())/b1->getOpenValue()));
   REQUIRE(allR[1] == (b2->getCloseValue() - b1->getCloseValue()) / b1->getCloseValue());
 }

 SECTION("getAllHighResReturns with only open positions") {
   using DT = DecimalType;
   const std::string sym = "@TEST";
   TradingVolume oneContract(1, TradingVolume::CONTRACTS);

   // 1) Build two bars for a single open trade
   auto mkBar = [&](int Y,int M,int D, const std::string& closeStr) {
     TimeSeriesDate dt(Y, M, D);
     DT c = createDecimal(closeStr);
     return createTimeSeriesEntry(
				  dt,
				  c,
				  c + createDecimal("0.50"),
				  c - createDecimal("0.50"),
				  c,
				  1
				  );
   };
   auto b3 = mkBar(2020, Jan, 3, "200.00");
   auto b4 = mkBar(2020, Jan, 4, "240.00");

   // 2) Seed the OHLC series immediately
   auto ts = std::make_shared<OHLCTimeSeries<DT>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
   ts->addEntry(*b3);
   ts->addEntry(*b4);

   // 3) Build portfolio + security
   auto portfolio = std::make_shared<Portfolio<DT>>("port");
   portfolio->addSecurity(std::make_shared<FuturesSecurity<DT>>(sym, sym,
								createDecimal("1.0"), createDecimal("0.01"), ts));

   // 4) PalLongStrategy with a trivial pattern
   auto pattern = createLongPattern1();
   auto strat = std::make_shared<PalLongStrategy<DT>>("only-open", pattern, portfolio);

   // 5) Wire up backtester
   DailyBackTester<DT> bt(TimeSeriesDate(2020, Jan, 3), TimeSeriesDate(2020, Jan, 4));
   bt.addStrategy(strat);

   // 6) Inject exactly one STILL-OPEN 2-bar trade: (200→240) ⇒ return = 0.20
   {
     auto posO = std::make_shared<TradingPositionLong<DT>>(sym, b3->getCloseValue(), *b3, oneContract);
     posO->addBar(*b4);

     auto& instrPos = const_cast<InstrumentPosition<DT>&>(
							  strat->getStrategyBroker().getInstrumentPosition(sym)
							  );
     instrPos.addPosition(posO);
   }

   // 7) Call getAllHighResReturns and verify one return
   auto allR = bt.getAllHighResReturns(strat.get());
   REQUIRE(allR.size() == 2);
   REQUIRE (allR[0] == ((b3->getCloseValue() - b3->getOpenValue())/b3->getOpenValue()));
   REQUIRE(allR[1] == (b4->getCloseValue() - b3->getCloseValue()) / b3->getCloseValue());
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
   REQUIRE(stat == createDecimal("0.6931472"));
 }
}

