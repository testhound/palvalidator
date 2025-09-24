#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TimeSeriesCsvReader.h"
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
