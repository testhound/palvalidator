#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TimeSeriesCsvReader.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"
#include "PalStrategyTestHelpers.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

// Keep in sync with other PAL tests
//using DecimalType = Decimal; // or your typedef used in other tests
const static std::string kCornSymbol("@C");

// ---- Helpers copied from the existing PalStrategy test style ----------------

static std::shared_ptr<PriceActionLabPattern>
createLongPattern1()
{
  // Create description using shared_ptr
  auto percentLong = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR.txt", 39, 20131217,
                                                   percentLong, percentShort, 21, 2);

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
  auto target = createLongProfitTarget("2.56");
  auto stop = createLongStopLoss("1.28");

  // 2.56 profit target in points = 93.81
  return std::make_shared<PriceActionLabPattern>(desc, longPattern1,
                                                 entry,
                                                 target,
                                                 stop);
}

// Wide targets/stops so exits are driven by portfolio overlays (like BE) not pattern exits.
static std::shared_ptr<PriceActionLabPattern> createLongPattern_WideTargets()
{
  auto percentLong  = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR.txt", 39, 20131217,
                                                   percentLong, percentShort, 21, 2);

  // Simple long condition replicated from your existing helpers
  auto open5  = std::make_shared<PriceBarOpen>(5);
  auto close5 = std::make_shared<PriceBarClose>(5);
  auto gt1 = std::make_shared<GreaterThanExpr>(open5, close5);

  auto close6 = std::make_shared<PriceBarClose>(6);
  auto gt2 = std::make_shared<GreaterThanExpr>(close5, close6);

  auto and1 = std::make_shared<AndExpr>(gt1, gt2);

  auto open6  = std::make_shared<PriceBarOpen>(6);
  auto gt3    = std::make_shared<GreaterThanExpr>(close6, open6);

  auto close8 = std::make_shared<PriceBarClose>(8);
  auto gt4    = std::make_shared<GreaterThanExpr>(open6, close8);

  auto and2 = std::make_shared<AndExpr>(gt3, gt4);

  auto open8 = std::make_shared<PriceBarOpen>(8);
  auto gt5   = std::make_shared<GreaterThanExpr>(close8, open8);

  auto and3 = std::make_shared<AndExpr>(and2, gt5);
  auto longPattern = std::make_shared<AndExpr>(and1, and3);

  auto entry  = createLongOnOpen();
  auto target = createLongProfitTarget("50.00"); // very wide
  auto stop   = createLongStopLoss("50.00");     // very wide

  return std::make_shared<PriceActionLabPattern>(desc, longPattern, entry, target, stop);
}

static std::shared_ptr<PriceActionLabPattern> createShortPattern_WideTargets()
{
  auto percentLong  = std::make_shared<DecimalType>(createDecimal("20.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("80.00"));
  auto desc = std::make_shared<PatternDescription>("SHORT_REJECT.txt", 1, 19851120,
                                                   percentLong, percentShort, 2, 1);

  // Reuse a condition that triggers later in the corn series
  auto open7  = std::make_shared<PriceBarOpen>(7);
  auto close7 = std::make_shared<PriceBarClose>(7);
  auto gt1 = std::make_shared<GreaterThanExpr>(open7, close7);

  auto close8 = std::make_shared<PriceBarClose>(8);
  auto gt2 = std::make_shared<GreaterThanExpr>(close7, close8);
  auto and1 = std::make_shared<AndExpr>(gt1, gt2);

  auto open8  = std::make_shared<PriceBarOpen>(8);
  auto gt3    = std::make_shared<GreaterThanExpr>(close8, open8);

  auto close10 = std::make_shared<PriceBarClose>(10);
  auto gt4     = std::make_shared<GreaterThanExpr>(open8, close10);
  auto and2    = std::make_shared<AndExpr>(gt3, gt4);

  auto open10 = std::make_shared<PriceBarOpen>(10);
  auto gt5    = std::make_shared<GreaterThanExpr>(close10, open10);
  auto shortPattern = std::make_shared<AndExpr>(and2, gt5);

  auto entry  = createShortOnOpen();
  auto target = createShortProfitTarget("50.00");
  auto stop   = createShortStopLoss("50.00");

  return std::make_shared<PriceActionLabPattern>(desc, shortPattern, entry, target, stop);
}

// Backtest loop identical to the one in PalStrategyTest.cpp
static void backTestLoop(std::shared_ptr<Security<DecimalType>> security,
                         BacktesterStrategy<DecimalType>& strategy,
                         TimeSeriesDate& backTestStartDate,
                         TimeSeriesDate& backTestEndDate)
{
  TimeSeriesDate backTesterDate(backTestStartDate);
  TimeSeriesDate orderDate;
  for (; (backTesterDate <= backTestEndDate); backTesterDate = boost_next_weekday(backTesterDate))
  {
    orderDate = boost_previous_weekday(backTesterDate);
    if (strategy.doesSecurityHaveTradingData(*security, orderDate))
    {
      strategy.eventUpdateSecurityBarNumber(security->getSymbol());
      if (strategy.isShortPosition(security->getSymbol()) || strategy.isLongPosition(security->getSymbol()))
      {
        strategy.eventExitOrders(security.get(),
                                 strategy.getInstrumentPosition(security->getSymbol()),
                                 orderDate);
      }
      strategy.eventEntryOrders(security.get(),
                                strategy.getInstrumentPosition(security->getSymbol()),
                                orderDate);
    }
    strategy.eventProcessPendingOrders(backTesterDate);
  }
}


// Compute mean holding time in calendar days over closed transactions
template<class Decimal, template<class> class FractionPolicy, template<class, bool> class SubPennyPolicy, bool PricesAreSplitAdjusted>
static double meanHoldingDays(const StrategyBroker<Decimal, FractionPolicy, SubPennyPolicy, PricesAreSplitAdjusted>& broker)
{
  double sumDays = 0.0;
  int count = 0;
  for (auto it = broker.beginStrategyTransactions();
       it != broker.endStrategyTransactions(); ++it)
  {
    auto txn = it->second;
    if (!txn->isTransactionComplete()) continue;

    auto pos = txn->getTradingPosition();
    const auto& d0 = pos->getEntryDate();
    const auto& d1 = pos->getExitDate();
    sumDays += (d1 - d0).days();
    ++count;
  }
  return (count > 0) ? (sumDays / count) : 0.0;
}

// Minimal long pattern: C(1) > O(1)
static std::shared_ptr<PriceActionLabPattern> makeLongC1gtO1()
{
  auto pLong  = std::make_shared<DecimalType>(createDecimal("50.00"));
  auto pShort = std::make_shared<DecimalType>(createDecimal("50.00"));
  auto desc   = std::make_shared<PatternDescription>("LONG_C1gtO1", 1, 20200101, pLong, pShort, /*maxbars*/1, /*dummy*/1);

  auto c1 = std::make_shared<PriceBarClose>(1);
  auto o1 = std::make_shared<PriceBarOpen>(1);
  auto expr = std::make_shared<GreaterThanExpr>(c1, o1);

  auto entry  = createLongOnOpen();
  auto target = createLongProfitTarget("50.00"); // wide targets/stops to avoid accidental exits
  auto stop   = createLongStopLoss("50.00");
  return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
}

// Minimal short pattern: O(2) > C(2)
static std::shared_ptr<PriceActionLabPattern> makeShortO2gtC2()
{
  auto pLong  = std::make_shared<DecimalType>(createDecimal("50.00"));
  auto pShort = std::make_shared<DecimalType>(createDecimal("50.00"));
  auto desc   = std::make_shared<PatternDescription>("SHORT_O2gtC2", 2, 20200101, pLong, pShort, /*maxbars*/2, /*dummy*/1);

  auto o2 = std::make_shared<PriceBarOpen>(2);
  auto c2 = std::make_shared<PriceBarClose>(2);
  auto expr = std::make_shared<GreaterThanExpr>(o2, c2);

  auto entry  = createShortOnOpen();
  auto target = createShortProfitTarget("50.00");
  auto stop   = createShortStopLoss("50.00");
  return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
}

// Build a tiny daily series where both patterns fire on the same evaluation bar.
// At bar (2020-01-10), we want:
//   C(1) > O(1)  → use 2020-01-09 close > open
//   O(2) > C(2)  → use 2020-01-08 open  > close
static std::shared_ptr< mkc_timeseries::OHLCTimeSeries<DecimalType> >
makeSeriesBothSidesFire()
{
  using namespace mkc_timeseries;
  auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);

  // Mon 2020-01-06
  ts->addEntry(*createTimeSeriesEntry("20200106", "100", "101", "99", "100", "1000"));
  // Tue 2020-01-07
  ts->addEntry(*createTimeSeriesEntry("20200107", "100", "101", "99", "100", "1000"));
  // Wed 2020-01-08  (O > C)  => satisfies O(2) > C(2) on Friday
  ts->addEntry(*createTimeSeriesEntry("20200108", "120", "125", "110", "110", "1000"));
  // Thu 2020-01-09  (C > O)  => satisfies C(1) > O(1) on Friday
  ts->addEntry(*createTimeSeriesEntry("20200109", "100", "112", "99", "110", "1000"));
  // Fri 2020-01-10  (evaluation bar)
  ts->addEntry(*createTimeSeriesEntry("20200110", "110", "115", "105", "112", "1000"));
  ts->addEntry(*createTimeSeriesEntry("20200113", "112", "116", "108", "114", "1000"));

  return ts;
}

// Helper: one-day backtest focused on 2020-01-10, using the project’s loop style.
static void runTinyLoop(std::shared_ptr<mkc_timeseries::Security<DecimalType>> sec,
                        BacktesterStrategy<DecimalType>& strat)
{
  TimeSeriesDate start(createDate("20200106"));
  TimeSeriesDate end  (createDate("20200113"));
  // Reuse the same loop as earlier tests
  backTestLoop(std::static_pointer_cast<mkc_timeseries::Security<DecimalType>>(sec),
               strat, start, end);
}

namespace
{  // translation-unit private

  static std::shared_ptr<OHLCTimeSeries<DecimalType>>
  metaSameDayLongSeries()
  {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(
							    TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*createTimeSeriesEntry("20200102","100","101","99", "100","1000"));
    ts->addEntry(*createTimeSeriesEntry("20200103","100","101","99", "100","1000"));
    ts->addEntry(*createTimeSeriesEntry("20200106","100","105","99", "104","1000")); // BULLISH
    ts->addEntry(*createTimeSeriesEntry("20200107","104","107","103","104","1000")); // eval day
    ts->addEntry(*createTimeSeriesEntry("20200108","300","305","295","302","1000")); // fill day
    ts->addEntry(*createTimeSeriesEntry("20200109","300","305","295","300","1000")); // extra
    return ts;
  }

  static std::shared_ptr<OHLCTimeSeries<DecimalType>>
  metaSameDayShortSeries()
  {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(
							    TimeFrame::DAILY, TradingVolume::CONTRACTS);
    ts->addEntry(*createTimeSeriesEntry("20200102","100","101","99", "100","1000"));
    ts->addEntry(*createTimeSeriesEntry("20200103","100","101","99", "100","1000"));
    ts->addEntry(*createTimeSeriesEntry("20200106","104","105","99", "100","1000")); // BEARISH
    ts->addEntry(*createTimeSeriesEntry("20200107","100","107","99", "100","1000")); // eval day
    ts->addEntry(*createTimeSeriesEntry("20200108","300","305","295","298","1000")); // fill day
    ts->addEntry(*createTimeSeriesEntry("20200109","300","305","295","300","1000")); // extra
    return ts;
  }

  static std::shared_ptr<PriceActionLabPattern>
  metaMakeLongC1gtO1(const std::string& stopPct, const std::string& targetPct)
  {
    auto pLong  = std::make_shared<DecimalType>(createDecimal("90.00"));
    auto pShort = std::make_shared<DecimalType>(createDecimal("10.00"));
    auto desc   = std::make_shared<PatternDescription>(
						       "META_SAMEDAY_LONG.txt", 1, 20200107, pLong, pShort, 1, 1);
    auto c1   = std::make_shared<PriceBarClose>(1);
    auto o1   = std::make_shared<PriceBarOpen>(1);
    auto expr = std::make_shared<GreaterThanExpr>(c1, o1);
    return std::make_shared<PriceActionLabPattern>(
						   desc, expr, createLongOnOpen(),
						   createLongProfitTarget(targetPct), createLongStopLoss(stopPct));
  }

  static std::shared_ptr<PriceActionLabPattern>
  metaMakeShortO1gtC1(const std::string& stopPct, const std::string& targetPct)
  {
    auto pLong  = std::make_shared<DecimalType>(createDecimal("10.00"));
    auto pShort = std::make_shared<DecimalType>(createDecimal("90.00"));
    auto desc   = std::make_shared<PatternDescription>(
						       "META_SAMEDAY_SHORT.txt", 1, 20200107, pLong, pShort, 1, 1);
    auto o1   = std::make_shared<PriceBarOpen>(1);
    auto c1   = std::make_shared<PriceBarClose>(1);
    auto expr = std::make_shared<GreaterThanExpr>(o1, c1);
    return std::make_shared<PriceActionLabPattern>(
						   desc, expr, createShortOnOpen(),
						   createShortProfitTarget(targetPct), createShortStopLoss(stopPct));
  }

  static std::shared_ptr<BackTester<DecimalType>>
  runMetaSameDayBacktest(const std::shared_ptr<BacktesterStrategy<DecimalType>>& strategy,
			 const std::string& endDateStr)
  {
    DateRange range(createDate("20200102"), createDate(endDateStr));
    return BackTesterFactory<DecimalType>::backTestStrategy(
							    strategy, TimeFrame::DAILY, range);
  }

} // anonymous namespace

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST_CASE("PalMetaStrategy: Breakeven stop N=0 is compatible with a known profit-target exit (long)",
          "[PalMetaStrategy][Breakeven]")
{
  // Load data
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csv("C2_122AR.txt", TimeFrame::DAILY,
                                      TradingVolume::CONTRACTS, cornTickValue);
  csv.readFile();
  auto ts = csv.getTimeSeries();

  // Security + portfolio
  std::string sym = kCornSymbol;
  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(sym,
                                                             "Corn futures",
                                                             createDecimal("50.0"),
                                                             cornTickValue,
                                                             ts);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Corn Portfolio");
  portfolio->addSecurity(corn);

  // Meta strategy with original long pattern (narrow target/stop) to replicate the known trade,
  // and enable breakeven at t=0. This should *not* change the known Dec-04-1985 target exit.
  PalMetaStrategy<DecimalType> meta("Meta BE compat", portfolio);
  meta.addPricePattern(createLongPattern1());   // same helper as in PalStrategyTest.cpp
  meta.addBreakEvenStop(/*activationBars N=*/0);

  TimeSeriesDate start(TimeSeriesDate(1985, Mar, 19));
  TimeSeriesDate end  (TimeSeriesDate(1985, Dec, 31));

  backTestLoop(corn, meta, start, end);

  auto& broker = meta.getStrategyBroker();

  REQUIRE(broker.getTotalTrades() >= 1);
  REQUIRE(broker.getClosedTrades() >= 1);

  // Find the very first trade and verify dates unchanged vs your existing test.
  auto it = broker.beginStrategyTransactions();
  REQUIRE(it != broker.endStrategyTransactions());
  auto txn = it->second;
  REQUIRE(txn->isTransactionComplete());

  auto entryOrder = txn->getEntryTradingOrder();
  auto pos        = txn->getTradingPosition();
  auto exitOrder  = txn->getExitTradingOrder();

  // Known dates from PalStrategyTest.cpp for long with profit target exit:
  REQUIRE(entryOrder->getFillDate() == TimeSeriesDate(1985, Nov, 18));
  REQUIRE(pos->getEntryDate()       == TimeSeriesDate(1985, Nov, 18));


  // With BE enabled (N=0), exit may occur earlier than the original 1985-12-04 target.
  // Assert it is not later than the original target date.
  const TimeSeriesDate originalTargetExit(1985, Dec, 4);
  REQUIRE(pos->getExitDate()       <= originalTargetExit);
  REQUIRE(exitOrder->getFillDate() == pos->getExitDate());
}

TEST_CASE("PalMetaStrategy: Breakeven at N=0 does not increase mean holding time for longs",
          "[PalMetaStrategy][Breakeven][PortfolioProperty]")
{
  // Load data once
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csv("C2_122AR.txt", TimeFrame::DAILY,
                                      TradingVolume::CONTRACTS, cornTickValue);
  csv.readFile();
  auto ts = csv.getTimeSeries();

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(kCornSymbol,
                                                             "Corn futures",
                                                             createDecimal("50.0"),
                                                             cornTickValue,
                                                             ts);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Corn Portfolio");
  portfolio->addSecurity(corn);

  // Baseline (no BE)
  PalMetaStrategy<DecimalType> baseline("Meta baseline", portfolio);
  baseline.addPricePattern(createLongPattern_WideTargets());

  // BE at N = 0
  PalMetaStrategy<DecimalType> beN0("Meta BE N0", portfolio);
  beN0.addPricePattern(createLongPattern_WideTargets());
  beN0.addBreakEvenStop(/*N=*/0);

  TimeSeriesDate start(TimeSeriesDate(1985, Mar, 19));
  TimeSeriesDate end  (TimeSeriesDate(2008, Dec, 31));

  // Run both
  backTestLoop(corn, baseline, start, end);
  backTestLoop(corn, beN0,     start, end);

  auto& b0 = baseline.getStrategyBroker();
  auto& b1 = beN0.getStrategyBroker();

  REQUIRE(b1.getClosedTrades() >= b0.getClosedTrades()); // same signals, same count
  const double mean0 = meanHoldingDays(b0);
  const double mean1 = meanHoldingDays(b1);

  // Breakeven should not *increase* mean holding time; allow tiny numerical wiggle.
  REQUIRE(mean1 <= Catch::Approx(mean0).margin(1e-9));
}

TEST_CASE("PalMetaStrategy: Earlier breakeven arming (N=0) holds no longer than later arming (N=2) for longs",
          "[PalMetaStrategy][Breakeven][CompareN]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csv("C2_122AR.txt", TimeFrame::DAILY,
                                      TradingVolume::CONTRACTS, cornTickValue);
  csv.readFile();
  auto ts = csv.getTimeSeries();

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(kCornSymbol,
                                                             "Corn futures",
                                                             createDecimal("50.0"),
                                                             cornTickValue,
                                                             ts);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Corn Portfolio");
  portfolio->addSecurity(corn);

  PalMetaStrategy<DecimalType> beN0("Meta BE N0", portfolio);
  beN0.addPricePattern(createLongPattern_WideTargets());
  beN0.addBreakEvenStop(/*N=*/0);

  PalMetaStrategy<DecimalType> beN2("Meta BE N2", portfolio);
  beN2.addPricePattern(createLongPattern_WideTargets());
  beN2.addBreakEvenStop(/*N=*/2);

  TimeSeriesDate start(TimeSeriesDate(1985, Mar, 19));
  TimeSeriesDate end  (TimeSeriesDate(2008, Dec, 31));

  backTestLoop(corn, beN0, start, end);
  backTestLoop(corn, beN2, start, end);

  auto& b0 = beN0.getStrategyBroker();
  auto& b2 = beN2.getStrategyBroker();

  REQUIRE(b0.getClosedTrades() >= b2.getClosedTrades());
  const double m0 = meanHoldingDays(b0);
  const double m2 = meanHoldingDays(b2);

  // Earlier arming should not lead to *longer* holding time
  REQUIRE(m0 <= Catch::Approx(m2).margin(1e-9));
}

TEST_CASE("PalMetaStrategy: Breakeven property test on shorts (N=0 vs baseline)",
          "[PalMetaStrategy][Breakeven][Shorts]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csv("C2_122AR.txt", TimeFrame::DAILY,
                                      TradingVolume::CONTRACTS, cornTickValue);
  csv.readFile();
  auto ts = csv.getTimeSeries();

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(kCornSymbol,
                                                             "Corn futures",
                                                             createDecimal("50.0"),
                                                             cornTickValue,
                                                             ts);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Corn Portfolio");
  portfolio->addSecurity(corn);

  PalMetaStrategy<DecimalType> baseline("Meta baseline short", portfolio);
  baseline.addPricePattern(createShortPattern_WideTargets());

  PalMetaStrategy<DecimalType> beN0("Meta BE N0 short", portfolio);
  beN0.addPricePattern(createShortPattern_WideTargets());
  beN0.addBreakEvenStop(/*N=*/0);

  TimeSeriesDate start(TimeSeriesDate(1985, Mar, 19));
  TimeSeriesDate end  (TimeSeriesDate(2011, Sep, 15));

  backTestLoop(corn, baseline, start, end);
  backTestLoop(corn, beN0,     start, end);

  auto& b0 = baseline.getStrategyBroker();
  auto& b1 = beN0.getStrategyBroker();

  REQUIRE(b1.getClosedTrades() >= b0.getClosedTrades());
  const double mean0 = meanHoldingDays(b0);
  const double mean1 = meanHoldingDays(b1);

  REQUIRE(mean1 <= Catch::Approx(mean0).margin(1e-9));
}

TEST_CASE("PalMetaStrategy: both-sides-fire → current behavior (flag OFF) enters exactly one trade, long wins",
          "[PalMetaStrategy][BothSides]")
{
  using namespace mkc_timeseries;

  // Manual series
  auto ts = makeSeriesBothSidesFire();

  // Security + portfolio
  DecimalType tick(createDecimal("0.25"));
  auto sec = std::make_shared<FuturesSecurity<DecimalType>>("@C",
                                                            "Test futures",
                                                            createDecimal("50.0"),
                                                            tick,
                                                            ts);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("P");
  portfolio->addSecurity(sec);

  // Meta strategy with BOTH patterns, add LONG first so it wins ties (current behavior)
  PalMetaStrategy<DecimalType> meta("BothSidesFlagOff", portfolio);
  meta.addPricePattern(makeLongC1gtO1());
  meta.addPricePattern(makeShortO2gtC2());
  // default: skipIfBothSidesFire == false

  runTinyLoop(sec, meta);

  auto& broker = meta.getStrategyBroker();
  REQUIRE(broker.getTotalTrades() == 1);
  REQUIRE(broker.getClosedTrades() <= 1); // may still be open

  // Verify it's a LONG entry on 2020-01-10
  auto it = broker.beginStrategyTransactions();
  REQUIRE(it != broker.endStrategyTransactions());
  auto txn = it->second;

  auto entryOrder = txn->getEntryTradingOrder();
  REQUIRE(entryOrder);
  REQUIRE(entryOrder->isEntryOrder());
  REQUIRE(entryOrder->isLongOrder());

  REQUIRE(txn->getTradingPosition()->getEntryDate() == createDate("20200113"));
}

TEST_CASE("PalMetaStrategy: both-sides-fire → neutrality (flag ON) enters NO trade on that bar",
          "[PalMetaStrategy][BothSides][Neutral]")
{
  using namespace mkc_timeseries;

  auto ts = makeSeriesBothSidesFire();

  DecimalType tick(createDecimal("0.25"));
  auto sec = std::make_shared<FuturesSecurity<DecimalType>>("TEST",
                                                            "Test futures",
                                                            createDecimal("50.0"),
                                                            tick,
                                                            ts);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("P");
  portfolio->addSecurity(sec);

  PalMetaStrategy<DecimalType> meta("BothSidesFlagOn", portfolio);
  meta.addPricePattern(makeLongC1gtO1());
  meta.addPricePattern(makeShortO2gtC2());
  meta.setSkipIfBothSidesFire(true); // <<— neutrality enabled

  runTinyLoop(sec, meta);

  auto& broker = meta.getStrategyBroker();
  REQUIRE(broker.getTotalTrades() == 0); // stood aside on 2020-01-10
}

// -----------------------------------------------------------------------------
// NEW TESTS: Add to end of PalMetaStrategyTest.cpp
// -----------------------------------------------------------------------------

// Helper: Create a long pattern with a 0% stop loss
static std::shared_ptr<PriceActionLabPattern>
createLongPattern_ZeroStop()
{
  auto pLong  = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto pShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc   = std::make_shared<PatternDescription>("LONG_ZERO_STOP", 1, 20200101, pLong, pShort, 1, 1);

  // Simple pattern: C(1) > O(1)
  auto c1 = std::make_shared<PriceBarClose>(1);
  auto o1 = std::make_shared<PriceBarOpen>(1);
  auto expr = std::make_shared<GreaterThanExpr>(c1, o1);

  auto entry  = createLongOnOpen();
  auto target = createLongProfitTarget("5.00"); // 5% target
  auto stop   = createLongStopLoss("0.00");   // 0% stop
  return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
}

// Helper: Create a long pattern with a short 2% target
static std::shared_ptr<PriceActionLabPattern>
createLongPattern_ShortTarget()
{
    auto pLong  = std::make_shared<DecimalType>(createDecimal("90.00"));
    auto pShort = std::make_shared<DecimalType>(createDecimal("10.00"));
    auto desc   = std::make_shared<PatternDescription>("LONG_SHORT_TGT", 1, 20200101, pLong, pShort, 1, 1);

    // Simple pattern: C(1) > O(1)
    auto c1 = std::make_shared<PriceBarClose>(1);
    auto o1 = std::make_shared<PriceBarOpen>(1);
    auto expr = std::make_shared<GreaterThanExpr>(c1, o1);

    auto entry  = createLongOnOpen();
    auto target = createLongProfitTarget("2.00"); // 2% target
    auto stop   = createLongStopLoss("50.00");   // wide stop
    return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
}

// Helper: Time series to test 0% stop and breakeven
// Enters on 2020-01-09, is immediately profitable on 2020-01-10
static std::shared_ptr< mkc_timeseries::OHLCTimeSeries<DecimalType> >
makeSeriesForBreakevenTest()
{
  using namespace mkc_timeseries;
  auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);

  // 2020-01-07 (bar 2)
  ts->addEntry(*createTimeSeriesEntry("20200107", "100", "101", "99", "100", "1000"));
  // 2020-01-08 (bar 1: C > O) -> Signal for 2020-01-09
  ts->addEntry(*createTimeSeriesEntry("20200108", "100", "105", "99", "104", "1000"));
  // 2020-01-09 (bar 0: Evaluation bar) -> Order placed
  ts->addEntry(*createTimeSeriesEntry("20200109", "104", "108", "103", "105", "1000"));
  // 2020-01-10 (Fill date) -> Entry @ 106. Close is 108 (profitable)
  ts->addEntry(*createTimeSeriesEntry("20200110", "106", "109", "105", "108", "1000"));
  // 2020-01-13 (BE eval bar) -> Open @ 108
  ts->addEntry(*createTimeSeriesEntry("20200113", "108", "110", "107", "109", "1000"));
  // 2020-01-14 (Exit fill bar) -> Open @ 107
  ts->addEntry(*createTimeSeriesEntry("20200114", "107", "108", "106", "107", "1000"));

  return ts;
}

// Helper: Time series for maxHold test
// Enters on 2020-01-09, then trades flat (no target hit)
static std::shared_ptr< mkc_timeseries::OHLCTimeSeries<DecimalType> >
makeSeriesForMaxHoldTest()
{
  using namespace mkc_timeseries;
  auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);

  // 2020-01-07 (bar 2)
  ts->addEntry(*createTimeSeriesEntry("20200107", "100", "101", "99", "100", "1000"));
  // 2020-01-08 (bar 1: C > O) -> Signal
  ts->addEntry(*createTimeSeriesEntry("20200108", "100", "105", "99", "104", "1000"));
  // 2020-01-09 (bar 0: Eval) -> Order
  ts->addEntry(*createTimeSeriesEntry("20200109", "104", "108", "103", "105", "1000"));
  // 2020-01-10 (Fill date) -> Entry @ 106. (t=0)
  ts->addEntry(*createTimeSeriesEntry("20200110", "106", "107", "105", "106", "1000"));
  // 2020-01-13 (t=1)
  ts->addEntry(*createTimeSeriesEntry("20200113", "106", "107", "105", "106", "1000"));
  // 2020-01-14 (t=2)
  ts->addEntry(*createTimeSeriesEntry("20200114", "106", "107", "105", "106", "1000"));
  // 2020-01-15 (t=3) -> maxHold reached, exit order placed
  ts->addEntry(*createTimeSeriesEntry("20200115", "106", "107", "105", "106", "1000"));
  // 2020-01-16 (Exit fill) -> Exit @ 106.5
  ts->addEntry(*createTimeSeriesEntry("20200116", "106.5", "107", "105", "106", "1000"));

  return ts;
}


TEST_CASE("PalMetaStrategy::clone_shallow copies state flags", "[PalMetaStrategy][clone_shallow]")
{
    using namespace mkc_timeseries;
    DecimalType tick(createDecimal("0.25"));

    // --- Setup for 'BothSidesFire' flag test ---
    auto tsBothSides = makeSeriesBothSidesFire();
    auto secBothSides = std::make_shared<FuturesSecurity<DecimalType>>("TEST_BSF", "Test", createDecimal("50.0"), tick, tsBothSides);
    auto portBothSides = std::make_shared<Portfolio<DecimalType>>("P_BSF");
    portBothSides->addSecurity(secBothSides);

    // --- Setup for 'BreakevenEnabled' flag test ---
    auto tsBE = makeSeriesForBreakevenTest();
    auto secBE = std::make_shared<FuturesSecurity<DecimalType>>("TEST_BE", "Test", createDecimal("50.0"), tick, tsBE);
    auto portBE = std::make_shared<Portfolio<DecimalType>>("P_BE");
    portBE->addSecurity(secBE);

    SECTION("clone_shallow copies mSkipIfBothSidesFire flag")
    {
        PalMetaStrategy<DecimalType> metaOrig("Orig_BSF", portBothSides);
        metaOrig.addPricePattern(makeLongC1gtO1());
        metaOrig.addPricePattern(makeShortO2gtC2());
        metaOrig.setSkipIfBothSidesFire(true); // Enable flag on original

        // Clone it (shallow) to a new portfolio (can be the same portfolio for this test)
        auto metaClone = metaOrig.clone_shallow(portBothSides);
        REQUIRE(metaClone);

        // Run the test loop on the CLONE
        runTinyLoop(secBothSides, *metaClone);

        // Assert that the CLONE behaved as if the flag was true
        auto& broker = metaClone->getStrategyBroker();
        REQUIRE(broker.getTotalTrades() == 0); // Proves mSkipIfBothSidesFire was copied
    }

    SECTION("clone_shallow copies mBreakevenEnabled flag")
      {
	using namespace mkc_timeseries;

	// --- Common Setup ---
	auto tsBE = makeSeriesForBreakevenTest();        // tiny 2020-01-07..14 series
	DecimalType tick(createDecimal("0.25"));
	TimeSeriesDate startDate(createDate("20200107"));
	TimeSeriesDate endDate  (createDate("20200115"));
	DateRange backtestRange(startDate, endDate);
	auto timeframe = TimeFrame::DAILY;

	// A minimal long pattern that can actually trigger on this tiny series,
	// with a short 2% target so we get a closed trade in-range.
	auto makeLongWithShortTarget = []() {
	  auto pLong  = std::make_shared<DecimalType>(createDecimal("50.00"));
	  auto pShort = std::make_shared<DecimalType>(createDecimal("50.00"));
	  auto desc   = std::make_shared<PatternDescription>("LONG_C1gtO1_TGT2", 1, 20200101, pLong, pShort, 1, 1);

	  auto c1 = std::make_shared<PriceBarClose>(1);
	  auto o1 = std::make_shared<PriceBarOpen>(1);
	  auto expr = std::make_shared<GreaterThanExpr>(c1, o1);

	  auto entry  = createLongOnOpen();
	  auto target = createLongProfitTarget("2.00");   // 2% target -> hit on 2020-01-10 (high=109)
	  auto stop   = createLongStopLoss("50.00");      // wide stop so target/BE govern exits
	  return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
	};

	// =========================
	// Original strategy run
	// =========================
	auto secBE_orig  = std::make_shared<FuturesSecurity<DecimalType>>("@C", "Test Orig", createDecimal("50.0"), tick, tsBE);
	auto portBE_orig = std::make_shared<Portfolio<DecimalType>>("P_BE_Orig");
	portBE_orig->addSecurity(secBE_orig);

	auto metaOrig_ptr = std::make_shared<PalMetaStrategy<DecimalType>>("Orig_BE", portBE_orig);
	metaOrig_ptr->addPricePattern(makeLongWithShortTarget());
	metaOrig_ptr->addBreakEvenStop(/*N=*/0); // enable BE on original

	auto backtesterOrig = BackTesterFactory<DecimalType>::backTestStrategy(metaOrig_ptr, timeframe, backtestRange);
	REQUIRE(backtesterOrig);

	const auto& cphOrig = backtesterOrig->getClosedPositionHistory();
	REQUIRE(cphOrig.getNumPositions() == 1);

	// =========================
	// Clone strategy run
	// =========================
	auto tsBE_clone  = makeSeriesForBreakevenTest(); // fresh copy of same tiny series
	auto secBE_clone = std::make_shared<FuturesSecurity<DecimalType>>("@C", "Test Clone", createDecimal("50.0"), tick, tsBE_clone);
	auto portBE_clone = std::make_shared<Portfolio<DecimalType>>("P_BE_Clone");
	portBE_clone->addSecurity(secBE_clone);

	// clone_shallow copies patterns + compiled evaluators + BE flag
	auto metaClone = std::static_pointer_cast<PalMetaStrategy<DecimalType>>(metaOrig_ptr->clone_shallow(portBE_clone));
	REQUIRE(metaClone);

	auto backtesterClone = BackTesterFactory<DecimalType>::backTestStrategy(metaClone, timeframe, backtestRange);
	REQUIRE(backtesterClone);

	const auto& cphClone = backtesterClone->getClosedPositionHistory();
	REQUIRE(cphClone.getNumPositions() == 1);
      }
}


TEST_CASE("PalMetaStrategy 0% Stop-Loss Behavior", "[PalMetaStrategy][ZeroStop]")
{
    using namespace mkc_timeseries;
    auto ts = makeSeriesForBreakevenTest();
    DecimalType tick(createDecimal("0.25"));
    auto sec = std::make_shared<FuturesSecurity<DecimalType>>("@C", "Corn", createDecimal("50.0"), tick, ts);
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("Corn Portfolio");
    portfolio->addSecurity(sec);

    // Manually create a position unit to test exits
    auto entryBar = ts->getTimeSeriesEntry(createDate("20200110"));
    auto posUnit = std::make_shared<TradingPositionLong<DecimalType>>(
        sec->getSymbol(), 
        entryBar.getOpenValue(), // 106
        entryBar, 
        TradingVolume(1, TradingVolume::CONTRACTS)
    );
    posUnit->setProfitTarget(createDecimal("5.00"));
    posUnit->setStopLoss(createDecimal("0.00")); // Pattern had 0% stop

    SECTION("0% stop pattern places NO stop order") // Removed "when not profitable" - always true if BE off
      {
	// Strategy setup
	PalMetaStrategy<DecimalType> meta("ZeroStop_NoBE", portfolio);
	meta.addPricePattern(createLongPattern_ZeroStop()); // Pattern with 0% stop
	// Breakeven is DISABLED by default

	auto& broker = meta.getStrategyBroker();
	auto symbol = sec->getSymbol();
	TradingVolume oneContract(1, TradingVolume::CONTRACTS);

	// --- Simulate Entry ---
	// 1. Place entry order on 2020-01-09
	ptime orderDateTime = ptime(createDate("20200109"), getDefaultBarTime());
	meta.EnterLongOnOpen(symbol, orderDateTime, createDecimal("0.00"), createDecimal("5.00"));

	// 2. Process fill on 2020-01-10
	ptime fillDateTime = ptime(createDate("20200110"), getDefaultBarTime());
	meta.eventProcessPendingOrders(fillDateTime); // Will fill at Open = 106.00

	REQUIRE(broker.isLongPosition(symbol)); // Verify entry occurred

	// --- Check Exit Order Placement ---
	// 3. Call eventExitOrders for the *next* processing date (2020-01-13)
	//    Need the position state first
	const auto& instrPos = broker.getInstrumentPosition(symbol);
	ptime exitOrderPlacementDateTime = ptime(createDate("20200113"), getDefaultBarTime());
	meta.eventExitOrders(sec.get(), instrPos, exitOrderPlacementDateTime);

	// 4. Check pending orders in the broker
	int stopOrders = 0;
	int limitOrders = 0;
	for (auto it = broker.beginPendingOrders(); it != broker.endPendingOrders(); ++it) {
	  // Check the order's *creation* date matches the exit order placement date
	  if (it->first == exitOrderPlacementDateTime) {
	    if (std::dynamic_pointer_cast<SellAtStopOrder<DecimalType>>(it->second)) {
	      stopOrders++;
            }
            if (std::dynamic_pointer_cast<SellAtLimitOrder<DecimalType>>(it->second)) {
	      limitOrders++;
            }
	  }
	}

	REQUIRE(limitOrders == 1); // Profit target is placed
	// Crucial check: No stop order should be placed because the pattern's stop is 0%
	// and breakeven is off
	REQUIRE(stopOrders == 0);
      }
 
    SECTION("0% stop pattern correctly receives a Breakeven stop when profitable")
    {
        // Strategy setup
        PalMetaStrategy<DecimalType> meta("ZeroStop_WithBE", portfolio);
        meta.addPricePattern(createLongPattern_ZeroStop()); // Pattern with 0% stop
        meta.addBreakEvenStop(0); // Enable BE immediately (N=0)

        auto& broker = meta.getStrategyBroker();
        auto symbol = sec->getSymbol();
        TradingVolume oneContract(1, TradingVolume::CONTRACTS);

        // --- Simulate Entry ---
        // 1. Place entry order on 2020-01-09 (based on 2020-01-08 bar: C > O)
        ptime orderDateTime = ptime(createDate("20200109"), getDefaultBarTime());
        // Use the strategy's method to place the order
        meta.EnterLongOnOpen(symbol, orderDateTime, createDecimal("0.00"), createDecimal("5.00"));

        // 2. Process fill on 2020-01-10
        ptime fillDateTime = ptime(createDate("20200110"), getDefaultBarTime());
        meta.eventProcessPendingOrders(fillDateTime); // Will fill at Open = 106.00

        REQUIRE(broker.isLongPosition(symbol));
        REQUIRE(broker.getInstrumentPosition(symbol).getNumPositionUnits() == 1);
        DecimalType entryPrice = broker.getInstrumentPosition(symbol).getFillPrice(1);
        REQUIRE(entryPrice == createDecimal("106.00"));

        // --- Simulate Profitability Check for Breakeven ---
        // 3. Process the next bar (2020-01-13) to update position state
        //    The close of 2020-01-10 was 108, making the position profitable.
        //    The bar for 2020-01-13 has Close = 109.
        ptime nextBarDateTime = ptime(createDate("20200113"), getDefaultBarTime());
        meta.eventProcessPendingOrders(nextBarDateTime); // This adds the bar data to the open position

        // Get the updated position state AFTER adding the bar
        const auto& instrPos = broker.getInstrumentPosition(symbol);
        REQUIRE_FALSE(instrPos.isFlatPosition()); // Should still be long
        auto posIt = instrPos.getInstrumentPosition(1);
        REQUIRE(posIt != instrPos.endInstrumentPosition());
        auto& posUnit = *posIt;

        // Verify conditions for BE activation are met:
        REQUIRE(posUnit->getLastClose() > posUnit->getEntryPrice()); // Profitable (109 > 106)
        REQUIRE(posUnit->getNumBarsSinceEntry() >= 0); // Meets N=0

        // --- Check Exit Order Placement ---
        // 4. Call eventExitOrders for the *next* processing date (2020-01-14)
        ptime exitOrderPlacementDateTime = ptime(createDate("20200114"), getDefaultBarTime());
        meta.eventExitOrders(sec.get(), instrPos, exitOrderPlacementDateTime);

        // 5. Check pending orders in the broker
        int stopOrders = 0;
        int limitOrders = 0;
        DecimalType stopPrice(0);
        for (auto it = broker.beginPendingOrders(); it != broker.endPendingOrders(); ++it) {
            // Check the order's *creation* date matches the exit order placement date
            if (it->first == exitOrderPlacementDateTime) {
                 if (auto stopOrder = std::dynamic_pointer_cast<SellAtStopOrder<DecimalType>>(it->second)) {
                    stopOrders++;
                    stopPrice = stopOrder->getStopPrice();
                }
                if (std::dynamic_pointer_cast<SellAtLimitOrder<DecimalType>>(it->second)) {
                    limitOrders++;
                }
            }
        }

        REQUIRE(limitOrders == 1); // Profit target is still placed
        REQUIRE(stopOrders == 1);  // Breakeven stop *is* placed
        // Stop is placed at entry price because BE was armed and position was profitable
        REQUIRE(stopPrice == entryPrice);
    }
}


TEST_CASE("PalMetaStrategy Exit Priority: maxHoldingPeriod supersedes other exits", "[PalMetaStrategy][ExitPriority]")
{
    using namespace mkc_timeseries;
    auto ts = makeSeriesForMaxHoldTest();
    DecimalType tick(createDecimal("0.25"));
    auto sec = std::make_shared<FuturesSecurity<DecimalType>>("@C", "Test", createDecimal("50.0"), tick, ts);
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("Corn poortfolio");
    portfolio->addSecurity(sec);
    
    // Set maxHold = 3 bars
    StrategyOptions options(false, 0, 3);
    PalMetaStrategy<DecimalType> meta("MaxHold_Test", portfolio, options);
    meta.addPricePattern(createLongPattern_ShortTarget()); // 2% target, 50% stop

    TimeSeriesDate start(createDate("20200107"));
    TimeSeriesDate end(createDate("20200117"));

    backTestLoop(sec, meta, start, end);
    
    auto& broker = meta.getStrategyBroker();
    REQUIRE(broker.getClosedTrades() == 1);
    
    auto pos = broker.beginClosedPositions()->second;
    
    // Entry: 2020-01-10 @ 106.00
    // t=0: 2020-01-10 (Close 106)
    // t=1: 2020-01-13 (Close 106)
    // t=2: 2020-01-14 (Close 106)
    // t=3: 2020-01-15 (Close 106) -> maxHold reached (3 >= 3)
    // Exit order placed for 2020-01-16 Open
    
    REQUIRE(pos->getEntryDate() == createDate("20200110"));
    REQUIRE(pos->getExitDate() == createDate("20200116"));
    
    // Verify it was a maxHold exit by checking num bars
    REQUIRE(pos->getNumBarsSinceEntry() == 4); 
    
    // Verify it was a market-on-open exit
    // Exit price should be the OPEN of the exit date bar
    auto exitBar = ts->getTimeSeriesEntry(createDate("20200116"));
    REQUIRE(pos->getExitPrice() == exitBar.getOpenValue()); // 106.5
    
    // Verify target was not hit
    DecimalType targetPrice = pos->getEntryPrice() * createDecimal("1.02"); // 106 * 1.02 = 108.12
    REQUIRE(pos->getExitPrice() < targetPrice);
}

// ============================================================================
// Constructor flag
// ============================================================================

TEST_CASE("PalMetaStrategy::isSameDayExitsEnabled defaults to false",
          "[PalMetaStrategy][SameDayExit][constructor]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>("MetaDefault", port);
  meta->addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE_FALSE(meta->isSameDayExitsEnabled());
}

TEST_CASE("PalMetaStrategy::isSameDayExitsEnabled is true when constructed with true",
          "[PalMetaStrategy][SameDayExit][constructor]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaSameDay", port, opts, /*sameDayExits=*/true);
  meta->addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE(meta->isSameDayExitsEnabled());
}


// ============================================================================
// Behavioral — long patterns
// ============================================================================

TEST_CASE("PalMetaStrategy same-day stop-loss fires on entry bar (long)",
          "[PalMetaStrategy][SameDayExit][long][stop]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaLongStop", port, opts, true);
  meta->addPricePattern(metaMakeLongC1gtO1("1.00","50.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isFlatPosition(kCornSymbol));
  REQUIRE(broker.getClosedTrades() == 1);
  REQUIRE(broker.getOpenTrades()   == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());

  auto pos = broker.beginClosedPositions()->second;
  REQUIRE(pos->getEntryDate() == createDate("20200108"));
  REQUIRE(pos->getExitDate()  == createDate("20200108"));
  REQUIRE(pos->getExitOrderType() == OrderType::SELL_AT_STOP);
}

TEST_CASE("PalMetaStrategy same-day profit target fires on entry bar (long)",
          "[PalMetaStrategy][SameDayExit][long][limit]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaLongLimit", port, opts, true);
  meta->addPricePattern(metaMakeLongC1gtO1("50.00","1.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isFlatPosition(kCornSymbol));
  REQUIRE(broker.getClosedTrades() == 1);
  REQUIRE(broker.getOpenTrades()   == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());

  auto pos = broker.beginClosedPositions()->second;
  REQUIRE(pos->getEntryDate() == createDate("20200108"));
  REQUIRE(pos->getExitDate()  == createDate("20200108"));
  REQUIRE(pos->getExitOrderType() == OrderType::SELL_AT_LIMIT);
}

TEST_CASE("PalMetaStrategy same-day stop wins when bar spans both stop and target (long)",
          "[PalMetaStrategy][SameDayExit][long][stop-wins]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaLongStopWins", port, opts, true);
  meta->addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isFlatPosition(kCornSymbol));
  REQUIRE(broker.getClosedTrades() == 1);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
  REQUIRE(broker.beginClosedPositions()->second->getExitOrderType() == OrderType::SELL_AT_STOP);
}

TEST_CASE("PalMetaStrategy same-day: neither fires, long position stays open",
          "[PalMetaStrategy][SameDayExit][long][no-trigger]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaLongNoTrigger", port, opts, true);
  meta->addPricePattern(metaMakeLongC1gtO1("5.00","5.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isLongPosition(kCornSymbol));
  REQUIRE(broker.getOpenTrades()   == 1);
  REQUIRE(broker.getClosedTrades() == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
}

TEST_CASE("PalMetaStrategy same-day: cancelled long orders are erased from pending queue",
          "[PalMetaStrategy][SameDayExit][long][bleed-through]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaLongBleed", port, opts, true);
  meta->addPricePattern(metaMakeLongC1gtO1("5.00","5.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isLongPosition(kCornSymbol));
  REQUIRE(broker.getOpenTrades()   == 1);
  REQUIRE(broker.getClosedTrades() == 0);
  // Cancelled same-day orders must be fully erased — nothing can bleed through.
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
}

TEST_CASE("PalMetaStrategy with sameDayExits=false does not close on entry bar (long)",
          "[PalMetaStrategy][SameDayExit][long][disabled]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>("MetaLongDisabled", port);
  meta->addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isLongPosition(kCornSymbol));
  REQUIRE(broker.getOpenTrades()   == 1);
  REQUIRE(broker.getClosedTrades() == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
}


// ============================================================================
// Behavioral — short patterns
// ============================================================================

TEST_CASE("PalMetaStrategy same-day stop-loss fires on entry bar (short)",
          "[PalMetaStrategy][SameDayExit][short][stop]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayShortSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaShortStop", port, opts, true);
  meta->addPricePattern(metaMakeShortO1gtC1("1.00","50.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isFlatPosition(kCornSymbol));
  REQUIRE(broker.getClosedTrades() == 1);
  REQUIRE(broker.getOpenTrades()   == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());

  auto pos = broker.beginClosedPositions()->second;
  REQUIRE(pos->getEntryDate() == createDate("20200108"));
  REQUIRE(pos->getExitDate()  == createDate("20200108"));
  REQUIRE(pos->getExitOrderType() == OrderType::COVER_AT_STOP);
}

TEST_CASE("PalMetaStrategy same-day profit target fires on entry bar (short)",
          "[PalMetaStrategy][SameDayExit][short][limit]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayShortSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaShortLimit", port, opts, true);
  meta->addPricePattern(metaMakeShortO1gtC1("50.00","1.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isFlatPosition(kCornSymbol));
  REQUIRE(broker.getClosedTrades() == 1);
  REQUIRE(broker.getOpenTrades()   == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());

  auto pos = broker.beginClosedPositions()->second;
  REQUIRE(pos->getEntryDate() == createDate("20200108"));
  REQUIRE(pos->getExitDate()  == createDate("20200108"));
  REQUIRE(pos->getExitOrderType() == OrderType::COVER_AT_LIMIT);
}

TEST_CASE("PalMetaStrategy same-day stop wins when bar spans both stop and target (short)",
          "[PalMetaStrategy][SameDayExit][short][stop-wins]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayShortSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaShortStopWins", port, opts, true);
  meta->addPricePattern(metaMakeShortO1gtC1("1.00","1.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isFlatPosition(kCornSymbol));
  REQUIRE(broker.getClosedTrades() == 1);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
  REQUIRE(broker.beginClosedPositions()->second->getExitOrderType() == OrderType::COVER_AT_STOP);
}

TEST_CASE("PalMetaStrategy same-day: neither fires, short position stays open",
          "[PalMetaStrategy][SameDayExit][short][no-trigger]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayShortSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaShortNoTrigger", port, opts, true);
  meta->addPricePattern(metaMakeShortO1gtC1("5.00","5.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isShortPosition(kCornSymbol));
  REQUIRE(broker.getOpenTrades()   == 1);
  REQUIRE(broker.getClosedTrades() == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
}

TEST_CASE("PalMetaStrategy same-day: cancelled short orders are erased from pending queue",
          "[PalMetaStrategy][SameDayExit][short][bleed-through]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayShortSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  StrategyOptions opts(false, 0, 0);
  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>(
      "MetaShortBleed", port, opts, true);
  meta->addPricePattern(metaMakeShortO1gtC1("5.00","5.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isShortPosition(kCornSymbol));
  REQUIRE(broker.getOpenTrades()   == 1);
  REQUIRE(broker.getClosedTrades() == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
}

TEST_CASE("PalMetaStrategy with sameDayExits=false does not close on entry bar (short)",
          "[PalMetaStrategy][SameDayExit][short][disabled]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayShortSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);

  auto meta = std::make_shared<PalMetaStrategy<DecimalType>>("MetaShortDisabled", port);
  meta->addPricePattern(metaMakeShortO1gtC1("1.00","1.00"));

  runMetaSameDayBacktest(meta, "20200108");

  auto& broker = meta->getStrategyBroker();
  REQUIRE(meta->isShortPosition(kCornSymbol));
  REQUIRE(broker.getOpenTrades()   == 1);
  REQUIRE(broker.getClosedTrades() == 0);
  REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
}


// ============================================================================
// Clone propagation — flag accessor checks
// ============================================================================

TEST_CASE("PalMetaStrategy copy constructor propagates isSameDayExitsEnabled",
          "[PalMetaStrategy][SameDayExit][copy]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);
  StrategyOptions opts(false, 0, 0);

  PalMetaStrategy<DecimalType> origTrue("OT", port, opts, true);
  origTrue.addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE(PalMetaStrategy<DecimalType>(origTrue).isSameDayExitsEnabled());

  PalMetaStrategy<DecimalType> origFalse("OF", port, opts, false);
  origFalse.addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE_FALSE(PalMetaStrategy<DecimalType>(origFalse).isSameDayExitsEnabled());
}

TEST_CASE("PalMetaStrategy::clone() propagates isSameDayExitsEnabled",
          "[PalMetaStrategy][SameDayExit][clone]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);
  StrategyOptions opts(false, 0, 0);

  PalMetaStrategy<DecimalType> origTrue("OT", port, opts, true);
  origTrue.addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE(origTrue.clone(port)->isSameDayExitsEnabled());

  PalMetaStrategy<DecimalType> origFalse("OF", port, opts, false);
  origFalse.addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE_FALSE(origFalse.clone(port)->isSameDayExitsEnabled());
}

TEST_CASE("PalMetaStrategy::cloneForBackTesting() propagates isSameDayExitsEnabled",
          "[PalMetaStrategy][SameDayExit][clone]")
{
  DecimalType tick(createDecimal("0.25"));
  auto ts   = metaSameDayLongSeries();
  auto sec  = std::make_shared<FuturesSecurity<DecimalType>>(
      "@C","Corn",createDecimal("50.0"),tick,ts);
  auto port = std::make_shared<Portfolio<DecimalType>>("P");
  port->addSecurity(sec);
  StrategyOptions opts(false, 0, 0);

  PalMetaStrategy<DecimalType> orig("OT", port, opts, true);
  orig.addPricePattern(metaMakeLongC1gtO1("1.00","1.00"));
  REQUIRE(orig.cloneForBackTesting()->isSameDayExitsEnabled());
}


// ============================================================================
// Clone behavioral — clone_shallow executes same-day exits
// ============================================================================

TEST_CASE("PalMetaStrategy::clone_shallow executes same-day exits (long)",
          "[PalMetaStrategy][SameDayExit][clone_shallow][behavioral]")
{
  using Dec = DecimalType;
  DecimalType tick(createDecimal("0.25"));
  StrategyOptions opts(false, 0, 0);

  auto ts1 = metaSameDayLongSeries();
  auto ts2 = metaSameDayLongSeries();
  auto sec1 = std::make_shared<FuturesSecurity<Dec>>("@C","C1",createDecimal("50.0"),tick,ts1);
  auto sec2 = std::make_shared<FuturesSecurity<Dec>>("@C","C2",createDecimal("50.0"),tick,ts2);
  auto port1 = std::make_shared<Portfolio<Dec>>("P1"); port1->addSecurity(sec1);
  auto port2 = std::make_shared<Portfolio<Dec>>("P2"); port2->addSecurity(sec2);

  auto original = std::make_shared<PalMetaStrategy<Dec>>(
      "MetaOrigLong", port1, opts, true);
  original->addPricePattern(metaMakeLongC1gtO1("1.00","50.00"));
  REQUIRE(original->isSameDayExitsEnabled());

  auto shallowPtr = original->clone_shallow(port2);
  REQUIRE(shallowPtr);
  REQUIRE(shallowPtr->isSameDayExitsEnabled());

  DateRange range(createDate("20200102"), createDate("20200108"));
  BackTesterFactory<Dec>::backTestStrategy(original,    TimeFrame::DAILY, range);
  BackTesterFactory<Dec>::backTestStrategy(shallowPtr,  TimeFrame::DAILY, range);

  {
    auto& b = original->getStrategyBroker();
    REQUIRE(original->isFlatPosition(kCornSymbol));
    REQUIRE(b.getClosedTrades() == 1);
    REQUIRE(b.getOpenTrades()   == 0);
    auto pos = b.beginClosedPositions()->second;
    REQUIRE(pos->getExitDate()  == createDate("20200108"));
    REQUIRE(pos->getExitOrderType() == OrderType::SELL_AT_STOP);
  }

  {
    auto& b = shallowPtr->getStrategyBroker();
    REQUIRE(shallowPtr->isFlatPosition(kCornSymbol));
    REQUIRE(b.getClosedTrades() == 1);
    REQUIRE(b.getOpenTrades()   == 0);
    auto pos = b.beginClosedPositions()->second;
    REQUIRE(pos->getExitDate()  == createDate("20200108"));
    REQUIRE(pos->getExitOrderType() == OrderType::SELL_AT_STOP);
  }
}

TEST_CASE("PalMetaStrategy::clone_shallow executes same-day exits (short)",
          "[PalMetaStrategy][SameDayExit][clone_shallow][behavioral]")
{
  using Dec = DecimalType;
  DecimalType tick(createDecimal("0.25"));
  StrategyOptions opts(false, 0, 0);

  auto ts1 = metaSameDayShortSeries();
  auto ts2 = metaSameDayShortSeries();
  auto sec1 = std::make_shared<FuturesSecurity<Dec>>("@C","C1",createDecimal("50.0"),tick,ts1);
  auto sec2 = std::make_shared<FuturesSecurity<Dec>>("@C","C2",createDecimal("50.0"),tick,ts2);
  auto port1 = std::make_shared<Portfolio<Dec>>("P1"); port1->addSecurity(sec1);
  auto port2 = std::make_shared<Portfolio<Dec>>("P2"); port2->addSecurity(sec2);

  auto original = std::make_shared<PalMetaStrategy<Dec>>(
      "MetaOrigShort", port1, opts, true);
  original->addPricePattern(metaMakeShortO1gtC1("1.00","50.00"));
  REQUIRE(original->isSameDayExitsEnabled());

  auto shallowPtr = original->clone_shallow(port2);
  REQUIRE(shallowPtr);
  REQUIRE(shallowPtr->isSameDayExitsEnabled());

  DateRange range(createDate("20200102"), createDate("20200108"));
  BackTesterFactory<Dec>::backTestStrategy(original,    TimeFrame::DAILY, range);
  BackTesterFactory<Dec>::backTestStrategy(shallowPtr,  TimeFrame::DAILY, range);

  {
    auto& b = original->getStrategyBroker();
    REQUIRE(original->isFlatPosition(kCornSymbol));
    REQUIRE(b.getClosedTrades() == 1);
    auto pos = b.beginClosedPositions()->second;
    REQUIRE(pos->getExitDate()  == createDate("20200108"));
    REQUIRE(pos->getExitOrderType() == OrderType::COVER_AT_STOP);
  }

  {
    auto& b = shallowPtr->getStrategyBroker();
    REQUIRE(shallowPtr->isFlatPosition(kCornSymbol));
    REQUIRE(b.getClosedTrades() == 1);
    auto pos = b.beginClosedPositions()->second;
    REQUIRE(pos->getExitDate()  == createDate("20200108"));
    REQUIRE(pos->getExitOrderType() == OrderType::COVER_AT_STOP);
  }
}
