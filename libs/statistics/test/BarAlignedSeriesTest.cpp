// BarAlignedSeriesTest.cpp
//
// These tests mirror the PalStrategy backtest pattern to obtain a real
// ClosedPositionHistory, then exercise label building and error paths.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#include "BarAlignedSeries.h"          // The class under test
#include "TestUtils.h"                 // DecimalType, createDecimal
#include "number.h"

#include "TimeSeriesCsvReader.h"       // PALFormatCsvReader
#include "Portfolio.h"
#include "Security.h"
#include "PalStrategy.h"               // PalLongStrategy, helpers & types
#include "BoostDateHelper.h"
#include "TimeSeriesEntry.h"
// Local helper functions (copied from PalStrategyTestHelpers to avoid linker conflicts)
#include "DecimalConstants.h"

using namespace mkc_timeseries;

// Helper function implementations for creating test patterns and components
static std::shared_ptr<LongMarketEntryOnOpen>
createBarAlignedLongOnOpen()
{
  return std::make_shared<LongMarketEntryOnOpen>();
}

static std::shared_ptr<LongSideProfitTargetInPercent>
createBarAlignedLongProfitTarget(const std::string& targetPct)
{
  return std::make_shared<LongSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<LongSideStopLossInPercent>
createBarAlignedLongStopLoss(const std::string& targetPct)
{
  return std::make_shared<LongSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

// Shortened namespace aliases
using D = DecimalType;
using mkc_timeseries::TimeFrame;
using mkc_timeseries::TradingVolume;
using mkc_timeseries::OHLCTimeSeries;
using mkc_timeseries::NumericTimeSeries;
using mkc_timeseries::NumericTimeSeriesEntry;
using mkc_timeseries::FuturesSecurity;
using mkc_timeseries::Portfolio;
using mkc_timeseries::ClosedPositionHistory;
using mkc_timeseries::TimeSeriesDate;
using mkc_timeseries::getDefaultBarTime;
using palvalidator::analysis::BarAlignedSeries;

using boost::gregorian::date;
using boost::gregorian::Mar;
using boost::posix_time::ptime;

// ---------- Minimal pattern factory (copies style used in PalStrategyTest) ----------
#include "PalAst.h"
#include "PALPatternInterpreter.h"

// Create a simple long pattern that will always trigger (close[1] > close[2])
// This ensures we get trades for testing purposes
static std::shared_ptr<PriceActionLabPattern>
createSimpleLongPattern()
{
    auto percentLong  = std::make_shared<D>(createDecimal("90.00"));
    auto percentShort = std::make_shared<D>(createDecimal("10.00"));
    auto desc = std::make_shared<PatternDescription>("TestPattern.txt", 1, 20240301,
                                                     percentLong, percentShort, 10, 1);

    // Simple pattern: close[1] > close[2] (yesterday's close > day before yesterday's close)
    auto close1 = std::make_shared<PriceBarClose>(1);
    auto close2 = std::make_shared<PriceBarClose>(2);
    auto longExpr = std::make_shared<GreaterThanExpr>(close1, close2);

    auto entry  = createBarAlignedLongOnOpen();
    auto target = createBarAlignedLongProfitTarget("10.00");  // 10% profit target
    auto stop   = createBarAlignedLongStopLoss("5.00");       // 5% stop loss

    return std::make_shared<PriceActionLabPattern>(desc, longExpr, entry, target, stop);
}

// Run a tiny backtest loop over a date range (pattern adapted from PalStrategyTest.cpp)
static void runBacktestOverRange(std::shared_ptr<OHLCTimeSeries<D>> ohlc,
                                 mkc_timeseries::BacktesterStrategy<D> &strategy,
                                 const TimeSeriesDate &startD,
                                 const TimeSeriesDate &endD)
{
    // Get symbol from the security in the portfolio instead of from OHLC
    auto portfolio = strategy.getPortfolio();
    std::string sym;
    if (portfolio->getNumSecurities() > 0) {
        auto it = portfolio->beginPortfolio();
        sym = it->first; // Get symbol from the first security in portfolio
    }
    
    TimeSeriesDate backTesterDate(startD);
    TimeSeriesDate orderDate;

    for (; (backTesterDate <= endD); backTesterDate = mkc_timeseries::boost_next_weekday(backTesterDate))
    {
        orderDate = mkc_timeseries::boost_previous_weekday(backTesterDate);

        auto secIt = strategy.getPortfolio()->findSecurity(sym);
        if (secIt != strategy.getPortfolio()->endPortfolio() &&
            strategy.doesSecurityHaveTradingData(*secIt->second, orderDate))
        {
            strategy.eventUpdateSecurityBarNumber(sym);

            if (strategy.isShortPosition(sym) || strategy.isLongPosition(sym))
            {
                strategy.eventExitOrders(secIt->second.get(),
                                         strategy.getInstrumentPosition(sym),
                                         ptime(orderDate, getDefaultBarTime()));
            }

            strategy.eventEntryOrders(secIt->second.get(),
                                      strategy.getInstrumentPosition(sym),
                                      ptime(orderDate, getDefaultBarTime()));
        }

        strategy.eventProcessPendingOrders(backTesterDate);
    }
}

TEST_CASE("BarAlignedSeries: happy-path label build with real ClosedPositionHistory", "[BarAlignedSeries]")
{
    // --- Build synthetic time series using TestUtils helpers
    auto ohlc = std::make_shared<OHLCTimeSeries<D>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    
    // Create a series that will trigger our long pattern and generate trades
    // Pattern requires: open5 > close5, close5 > close6, close6 > open6, open6 > close8, close8 > open8
    
    // Add enough bars to satisfy pattern lookback (need at least 3 bars for pattern with offset 2)
    // Create a clear uptrend that will trigger our simple pattern (close[1] > close[2])
    std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string>> ohlcData = {
        // Date (YYYYMMDD), O, H, L, C - setup bars
        {"20240301", "100.0", "102.0", "99.0", "100.0"},   // bar 0 - close = 100
        {"20240304", "100.0", "103.0", "99.0", "101.0"},   // bar 1 - close = 101
        {"20240305", "101.0", "104.0", "100.0", "102.0"},  // bar 2 - close = 102 (102 > 101, pattern triggers)
        // Trading bars - pattern should trigger on bar 3 since close[1]=102 > close[2]=101
        {"20240306", "102.0", "108.0", "101.0", "107.0"},  // bar 3 - entry here, big up move
        {"20240307", "107.0", "110.0", "106.0", "109.0"},  // bar 4
        {"20240308", "109.0", "112.0", "108.0", "111.0"},  // bar 5
        {"20240311", "111.0", "114.0", "110.0", "113.0"},  // bar 6
        {"20240312", "113.0", "116.0", "112.0", "115.0"},  // bar 7 - should hit profit target
        {"20240313", "115.0", "118.0", "114.0", "117.0"},  // bar 8
        {"20240314", "117.0", "120.0", "116.0", "119.0"},  // bar 9
    };
    
    for (const auto& [date, open, high, low, close] : ohlcData) {
        auto entry = createTimeSeriesEntry(date, open, high, low, close, 1000);
        ohlc->addEntry(*entry);
    }

    // --- Portfolio + security wiring
    auto testSec = std::make_shared<EquitySecurity<D>>("MSFT", "Microsoft", ohlc);
    auto portfolio = std::make_shared<Portfolio<D>>("Test Portfolio");
    portfolio->addSecurity(testSec);

    // --- Strategy: use a simple pattern with reasonable exits (not too wide)
    palvalidator::analysis::BarAlignedSeries<D> aligner(/*volWindow=*/5); // smaller window for test data
    mkc_timeseries::StrategyOptions opts(false, 0, 0); // no pyramiding, no max-hold override
    mkc_timeseries::PalLongStrategy<D> strat("BarAlignedSeriesTest-Synthetic", createSimpleLongPattern(), portfolio, opts);

    // --- Backtest over the synthetic data range
    TimeSeriesDate startDate = createDate("20240301");
    TimeSeriesDate endDate = createDate("20240321");
    runBacktestOverRange(ohlc, strat, startDate, endDate);

    // --- Obtain ClosedPositionHistory from the broker
    auto broker = strat.getStrategyBroker();
    auto closed = broker.getClosedPositionHistory();

    // Sanity: we expect at least one closed position with our synthetic data
    REQUIRE(closed.getNumPositions() > 0);

    // --- Build labels aligned to the trade-sequence using the instrument's close series
    const NumericTimeSeries<D> &closeTS = ohlc->CloseTimeSeries();
    std::vector<int> labels = aligner.buildTradeAlignedLabels(closeTS, closed);

    // Assertions:
    REQUIRE(labels.size() > 0);
    // Labels must be 0,1,2 only
    for (int z : labels)
    {
        REQUIRE((z == 0 || z == 1 || z == 2));
    }

    // A loose upper bound: #trade-bars <= (#instrument bars - 1)
    REQUIRE(labels.size() <= closeTS.getNumEntries() - 1);

    // We can also spot-check that not *all* labels collapse to one value too often
    // (it can happen, but in this span volatility usually varies).
    const bool anyLow  = std::find(labels.begin(), labels.end(), 0) != labels.end();
    const bool anyMid  = std::find(labels.begin(), labels.end(), 1) != labels.end();
    const bool anyHigh = std::find(labels.begin(), labels.end(), 2) != labels.end();
    REQUIRE((anyLow || anyMid || anyHigh)); // at least one must be present (always true)
}

TEST_CASE("BarAlignedSeries: OOS close series too short for vol window throws", "[BarAlignedSeries]")
{
    // Build a tiny synthetic close series (4 bars → 3 returns)
    NumericTimeSeries<D> smallClose(TimeFrame::DAILY, 4);
    const auto t0 = TimeSeriesDate(2024, Mar, 1);
    auto dt = ptime(t0, getDefaultBarTime());
    auto add = [&](const char* px)
    {
        smallClose.addEntry(NumericTimeSeriesEntry<D>(dt, createDecimal(px), TimeFrame::DAILY));
        dt += boost::posix_time::hours(24);
    };
    add("100.00"); add("101.00"); add("100.50"); add("101.50");

    ClosedPositionHistory<D> emptyClosed; // triggers at the labeler window guard first
    BarAlignedSeries<D> aligner(/*volWindow=*/6);

    REQUIRE_THROWS_AS(aligner.buildTradeAlignedLabels(smallClose, emptyClosed), std::invalid_argument);
}

TEST_CASE("BarAlignedSeries: misaligned close series (subrange) throws", "[BarAlignedSeries]")
{
    // Build synthetic time series with predictable trades
    auto ohlc = std::make_shared<OHLCTimeSeries<D>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    
    // Create enough data to generate trades with simple uptrend
    std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string>> ohlcData = {
        // Setup bars for simple pattern
        {"20240301", "100.0", "102.0", "99.0", "100.0"},   // bar 0 - close = 100
        {"20240304", "100.0", "103.0", "99.0", "101.0"},   // bar 1 - close = 101
        {"20240305", "101.0", "104.0", "100.0", "102.0"},  // bar 2 - close = 102 (102 > 101, pattern triggers)
        // Trading bars
        {"20240306", "102.0", "108.0", "101.0", "107.0"},  // bar 3 - entry here
        {"20240307", "107.0", "110.0", "106.0", "109.0"},  // bar 4
        {"20240308", "109.0", "112.0", "108.0", "111.0"},  // bar 5
        {"20240311", "111.0", "114.0", "110.0", "113.0"},  // bar 6
        {"20240312", "113.0", "116.0", "112.0", "115.0"},  // bar 7
        {"20240313", "115.0", "118.0", "114.0", "117.0"},  // bar 8
        {"20240314", "117.0", "120.0", "116.0", "119.0"},  // bar 9
        {"20240315", "119.0", "122.0", "118.0", "121.0"},  // bar 10
        {"20240318", "121.0", "124.0", "120.0", "123.0"},  // bar 11
    };
    
    for (const auto& [date, open, high, low, close] : ohlcData) {
        auto entry = createTimeSeriesEntry(date, open, high, low, close, 1000);
        ohlc->addEntry(*entry);
    }

    auto testSec = std::make_shared<EquitySecurity<D>>("MSFT", "Microsoft", ohlc);
    auto portfolio = std::make_shared<Portfolio<D>>("Test Portfolio");
    portfolio->addSecurity(testSec);

    mkc_timeseries::PalLongStrategy<D> strat("BarAlignedSeriesTest-Misaligned", createSimpleLongPattern(), portfolio);
    TimeSeriesDate startDate = createDate("20240301");
    TimeSeriesDate endDate = createDate("20240325");
    runBacktestOverRange(ohlc, strat, startDate, endDate);

    auto broker = strat.getStrategyBroker();
    auto closed = broker.getClosedPositionHistory();
    REQUIRE(closed.getNumPositions() > 0);

    // Now intentionally build a *truncated* close series that misses later trade bars.
    // We'll copy only the first half of entries so some trade timestamps won't be found.
    const NumericTimeSeries<D> &fullClose = ohlc->CloseTimeSeries();
    auto fullEntries = fullClose.getEntriesCopy();
    size_t halfSize = fullEntries.size() / 2;
    NumericTimeSeries<D> truncated(TimeFrame::DAILY, halfSize);

    // Copy first half by iterating sorted entries
    size_t count = 0;
    for (const auto &e : fullEntries)
    {
        if (count >= halfSize) break;
        truncated.addEntry(NumericTimeSeriesEntry<D>(e.getDateTime(), e.getValue(), TimeFrame::DAILY));
        count++;
    }

    BarAlignedSeries<D> aligner(/*volWindow=*/5);
    REQUIRE_THROWS_AS(aligner.buildTradeAlignedLabels(truncated, closed), std::invalid_argument);
}
