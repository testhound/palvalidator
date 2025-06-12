#include <catch2/catch_test_macros.hpp>
#include "BackTester.h"
#include "PalStrategy.h"
#include "TimeSeriesCsvReader.h"
#include "Security.h"
#include "Portfolio.h"
#include "TestUtils.h"
#include "DecimalConstants.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace mkc_timeseries;
using namespace boost::posix_time;
using namespace boost::gregorian;

// Helper to create a simple long pattern that fires if the close is greater than the open.
std::shared_ptr<PriceActionLabPattern> createSimpleIntradayLongPattern()
{
    // Create decimal pointers for percentLong and percentShort
    auto percentLong = new decimal7(createDecimal("100"));
    auto percentShort = new decimal7(createDecimal("0"));
    
    auto desc = new PatternDescription("dummy.txt", 1, 20240101, percentLong, percentShort, 1, 0);
    auto co = new PriceBarClose(0);
    auto o = new PriceBarOpen(0);
    auto expr = new GreaterThanExpr(co, o);
    auto entry = new LongMarketEntryOnOpen();
    
    // Create decimal pointers for profit target and stop loss
    auto targetDecimal = new decimal7(createDecimal("1.0"));
    auto stopDecimal = new decimal7(createDecimal("0.5"));
    
    auto target = new LongSideProfitTargetInPercent(targetDecimal); // 1% target
    auto stop = new LongSideStopLossInPercent(stopDecimal);     // 0.5% stop

    return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
}

// Helper to create a simple short pattern that fires if the close is less than the open.
std::shared_ptr<PriceActionLabPattern> createSimpleIntradayShortPattern()
{
    // Create decimal pointers for percentLong and percentShort
    auto percentLong = new decimal7(createDecimal("0"));
    auto percentShort = new decimal7(createDecimal("100"));
    
    auto desc = new PatternDescription("dummy.txt", 1, 20240101, percentLong, percentShort, 1, 0);
    auto co = new PriceBarClose(0);
    auto o = new PriceBarOpen(0);
    // Use GreaterThanExpr with swapped arguments to simulate "less than" (o > co means co < o)
    auto expr = new GreaterThanExpr(o, co);
    auto entry = new ShortMarketEntryOnOpen();
    
    // Create decimal pointers for profit target and stop loss
    auto targetDecimal = new decimal7(createDecimal("1.0"));
    auto stopDecimal = new decimal7(createDecimal("0.5"));
    
    auto target = new ShortSideProfitTargetInPercent(targetDecimal); // 1% target
    auto stop = new ShortSideStopLossInPercent(stopDecimal);    // 0.5% stop

    return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
}


TEST_CASE("IntradayBackTester end-to-end operations", "[IntradayBackTester]")
{
    // 1. Setup common components: Security and Portfolio
    const std::string symbol = "@ES";
    auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::CONTRACTS);

    // Add 5-minute bars for a single day (using 0.25 tick-aligned prices)
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "09:30:00", "100.00", "102.00", "99.00", "101.00", "1000")); // Pattern fires long
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "09:35:00", "101.00", "103.00", "100.00", "102.00", "1000")); // Exit winner
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "09:40:00", "102.00", "102.00", "100.00", "101.00", "1000")); // Pattern fires short
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "09:45:00", "101.00", "101.50", "100.00", "101.50", "1000")); // Exit loser (stop loss)
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "09:50:00", "101.50", "102.00", "101.00", "102.00", "1000")); // Pattern fires long
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "09:55:00", "102.00", "103.00", "101.50", "102.50", "1000")); // Entry executes
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "10:00:00", "102.50", "103.00", "102.00", "102.75", "1000")); // Position remains open
    timeSeries->addEntry(*createTimeSeriesEntry("20240102", "10:05:00", "102.75", "103.25", "102.25", "103.00", "1000")); // Extra bar to keep position open


    auto security = std::make_shared<FuturesSecurity<DecimalType>>(symbol, "E-mini S&P",
                                                                  createDecimal("50.0"),
                                                                  createDecimal("0.25"),
                                                                  timeSeries);
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("Intraday Portfolio");
    portfolio->addSecurity(security);

    // 2. Define backtest range using ptime
    ptime backtestStart(date(2024, Jan, 2), time_duration(9, 30, 0));
    ptime backtestEnd(date(2024, Jan, 2), time_duration(10, 10, 0));


    SECTION("Intraday long-only strategy backtest")
    {
        // 3. Create IntradayBackTester and Strategy
        IntradayBackTester<DecimalType> intradayBacktester(backtestStart, backtestEnd);
        auto longPattern = createSimpleIntradayLongPattern();
        auto longStrategy = std::make_shared<PalLongStrategy<DecimalType>>("IntradayLong", longPattern, portfolio);

        intradayBacktester.addStrategy(longStrategy);

        // 4. Run the backtest
        intradayBacktester.backtest();

        // 5. Assert results
        auto strategy = *intradayBacktester.beginStrategies();
        const auto& broker = strategy->getStrategyBroker();
        const auto& history = broker.getClosedPositionHistory();


        REQUIRE(broker.getTotalTrades() == 3);
        REQUIRE(broker.getOpenTrades() == 1);
        REQUIRE(broker.getClosedTrades() == 2);

        REQUIRE(history.getNumWinningPositions() == 1);
        REQUIRE(history.getNumLosingPositions() == 1);

        // Verify the details of the first closed trade (conservative stop loss execution)
        auto closedPos = history.beginTradingPositions()->second;
        REQUIRE(closedPos->getEntryDateTime() == ptime(date(2024, Jan, 2), time_duration(9, 35, 0))); // Entry on open of next bar
        REQUIRE(closedPos->getEntryPrice() == createDecimal("101.00"));
        REQUIRE(closedPos->getExitDateTime() == ptime(date(2024, Jan, 2), time_duration(9, 40, 0)));  // Exit next bar (one bar delay)
        REQUIRE(closedPos->getExitPrice() == createDecimal("100.50")); // Filled at stop loss (conservative)
        REQUIRE(closedPos->isLosingPosition() == true);
    }

    SECTION("Intraday short-only strategy backtest")
    {
        // 3. Create IntradayBackTester and Strategy
        IntradayBackTester<DecimalType> intradayBacktester(backtestStart, backtestEnd);
        auto shortPattern = createSimpleIntradayShortPattern();
        auto shortStrategy = std::make_shared<PalShortStrategy<DecimalType>>("IntradayShort", shortPattern, portfolio);

        intradayBacktester.addStrategy(shortStrategy);

        // 4. Run the backtest
        intradayBacktester.backtest();

        // 5. Assert results
        auto strategy = *intradayBacktester.beginStrategies();
        const auto& broker = strategy->getStrategyBroker();
        const auto& history = broker.getClosedPositionHistory();

        REQUIRE(broker.getTotalTrades() == 1);
        REQUIRE(broker.getOpenTrades() == 0);
        REQUIRE(broker.getClosedTrades() == 1);

        REQUIRE(history.getNumWinningPositions() == 0);
        REQUIRE(history.getNumLosingPositions() == 1);

        // Verify the details of the closed short trade
        auto closedPos = history.beginTradingPositions()->second;
        REQUIRE(closedPos->getEntryDateTime() == ptime(date(2024, Jan, 2), time_duration(9, 45, 0))); // Entry on open of next bar
        REQUIRE(closedPos->getEntryPrice() == createDecimal("101.00"));
        REQUIRE(closedPos->getExitDateTime() == ptime(date(2024, Jan, 2), time_duration(9, 50, 0)));  // Exit on open of following bar
        REQUIRE(closedPos->getExitPrice() == createDecimal("101.50")); // Filled at stop
        REQUIRE(closedPos->isLosingPosition() == true);
    }
    
    SECTION("Intraday meta strategy backtest (long and short)")
    {
        // 3. Create IntradayBackTester and PalMetaStrategy
        IntradayBackTester<DecimalType> intradayBacktester(backtestStart, backtestEnd);
        auto metaStrategy = std::make_shared<PalMetaStrategy<DecimalType>>("IntradayMeta", portfolio);
        metaStrategy->addPricePattern(createSimpleIntradayLongPattern());
        metaStrategy->addPricePattern(createSimpleIntradayShortPattern());
        
        intradayBacktester.addStrategy(metaStrategy);
        
        // 4. Run the backtest
        intradayBacktester.backtest();
        
        // 5. Assert results
        auto strategy = *intradayBacktester.beginStrategies();
        const auto& broker = strategy->getStrategyBroker();
        const auto& history = broker.getClosedPositionHistory();
        
        
        REQUIRE(broker.getTotalTrades() == 4);
        REQUIRE(broker.getOpenTrades() == 1);
        REQUIRE(broker.getClosedTrades() == 3);
        
        REQUIRE(history.getNumWinningPositions() == 1);
        REQUIRE(history.getNumLosingPositions() == 2);
    }
}

TEST_CASE("IntradayBackTester end-to-end operations for Equities", "[IntradayBackTester][Equity]")
{
    // 1. Setup common components for an Equity security
    const std::string symbol = "QQQ";
    auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

    // Add 90-minute bars for a single day.
    // O-H-L-C values are chosen to trigger specific entry/exit conditions.
    timeSeries->addEntry(*createTimeSeriesEntry("20240304", "09:30:00", "400.00", "402.00", "399.00", "401.00", "5000000")); // Bar 0: Long pattern fires
    timeSeries->addEntry(*createTimeSeriesEntry("20240304", "11:00:00", "401.00", "406.00", "400.50", "405.00", "4500000")); // Bar 1: Entry @ 401, Exit @ 405.01 (profit target)
    timeSeries->addEntry(*createTimeSeriesEntry("20240304", "12:30:00", "405.00", "405.50", "400.00", "401.00", "4000000")); // Bar 2: High reaches profit target
    timeSeries->addEntry(*createTimeSeriesEntry("20240304", "14:00:00", "401.00", "404.00", "398.00", "403.00", "5500000")); // Bar 3: Entry @ 401, Exit @ 403.00 (stop loss)
    timeSeries->addEntry(*createTimeSeriesEntry("20240304", "15:30:00", "403.00", "404.00", "402.50", "403.75", "6000000")); // Bar 4: Long pattern fires
    timeSeries->addEntry(*createTimeSeriesEntry("20240304", "17:00:00", "403.75", "405.00", "403.50", "404.50", "1000000")); // Bar 5: Final entry executes, remains open


    // Use EquitySecurity for this test case
    auto security = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Invesco QQQ Trust",
                                                                  timeSeries);
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("Equity Intraday Portfolio");
    portfolio->addSecurity(security);

    // 2. Define backtest range using ptime to cover the whole session
    ptime backtestStart(date(2024, Mar, 4), time_duration(9, 30, 0));
    ptime backtestEnd(date(2024, Mar, 4), time_duration(17, 0, 1)); // End after the last bar opens


    SECTION("90-minute bar equity long-only strategy backtest")
    {
        IntradayBackTester<DecimalType> intradayBacktester(backtestStart, backtestEnd);
        auto longPattern = createSimpleIntradayLongPattern();
        auto longStrategy = std::make_shared<PalLongStrategy<DecimalType>>("EquityIntradayLong", longPattern, portfolio);

        intradayBacktester.addStrategy(longStrategy);
        intradayBacktester.backtest();

        auto strategy = *intradayBacktester.beginStrategies();
        const auto& broker = strategy->getStrategyBroker();
        const auto& history = broker.getClosedPositionHistory();

        // One winning trade, one trade still open.
        REQUIRE(broker.getTotalTrades() == 2);
        REQUIRE(broker.getOpenTrades() == 1);
        REQUIRE(broker.getClosedTrades() == 1);

        REQUIRE(history.getNumWinningPositions() == 1);
        REQUIRE(history.getNumLosingPositions() == 0);

        // Verify the winning trade
        auto winningPos = history.beginTradingPositions()->second;
        REQUIRE(winningPos->getEntryDateTime() == ptime(date(2024, Mar, 4), time_duration(11, 0, 0))); // Entry on Bar 1 open
        REQUIRE(winningPos->getEntryPrice() == createDecimal("401.00"));
        REQUIRE(winningPos->getExitDateTime() == ptime(date(2024, Mar, 4), time_duration(12, 30, 0)));  // Exit on Bar 2 open
        REQUIRE(winningPos->getExitPrice() == createDecimal("405.01")); // Profit target: 401 * 1.01 = 405.01. Fill price is the target.
        REQUIRE(winningPos->isWinningPosition() == true);
        
        // Verify the open position
        const auto& openPos = broker.getInstrumentPosition(symbol);
        REQUIRE(openPos.getNumPositionUnits() == 1);
        auto openTrade = *openPos.getInstrumentPosition(1);

	REQUIRE(openTrade->getEntryDateTime() == ptime(date(2024, Mar, 4), time_duration(15, 30, 0))); 
	REQUIRE(openTrade->getEntryPrice() == createDecimal("403.00"));
    }

    SECTION("Equity short-only strategy validates conservative stop execution")
    {
        // This test validates that if a bar hits both the profit target and the stop loss,
        // the conservative assumption is that the stop loss is executed.

        const std::string spySymbol = "SPY";
        auto spyTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

        // Bar 0 triggers the short. Bar 1 is where entry and exit occur.
        spyTimeSeries->addEntry(*createTimeSeriesEntry("20240410", "10:00:00", "500.00", "500.00", "498.00", "499.00", "3000000")); // Short pattern fires
        
        // This bar's Low will hit the profit target, and its High will hit the stop loss.
        // Entry Price (open of this bar): 499.00
        // Profit Target (1%): 499.00 * (1 - 0.01) = 494.01
        // Stop Loss (0.5%): 499.00 * (1 + 0.005) = 501.495
        spyTimeSeries->addEntry(*createTimeSeriesEntry("20240410", "10:30:00", "499.00", "502.00", "494.00", "501.00", "3500000")); // High > Stop, Low < Target
        spyTimeSeries->addEntry(*createTimeSeriesEntry("20240410", "11:00:00", "501.00", "502.00", "500.00", "501.50", "2000000")); // Bar after exit

        auto spySecurity = std::make_shared<EquitySecurity<DecimalType>>(spySymbol, "SPDR S&P 500 ETF", spyTimeSeries);
        auto spyPortfolio = std::make_shared<Portfolio<DecimalType>>("SPY Portfolio");
        spyPortfolio->addSecurity(spySecurity);

        ptime start = ptime(date(2024, Apr, 10), time_duration(10, 0, 0));
        ptime end = ptime(date(2024, Apr, 10), time_duration(11, 0, 1));
        
        IntradayBackTester<DecimalType> intradayBacktester(start, end);
        auto shortPattern = createSimpleIntradayShortPattern();
        auto shortStrategy = std::make_shared<PalShortStrategy<DecimalType>>("SPYShort", shortPattern, spyPortfolio);
        
        intradayBacktester.addStrategy(shortStrategy);
        intradayBacktester.backtest();
        
        auto strategy = *intradayBacktester.beginStrategies();
        const auto& broker = strategy->getStrategyBroker();
        const auto& history = broker.getClosedPositionHistory();
        
        REQUIRE(broker.getTotalTrades() == 1);
        REQUIRE(broker.getOpenTrades() == 0);
        REQUIRE(broker.getClosedTrades() == 1);

        REQUIRE(history.getNumWinningPositions() == 0);
        REQUIRE(history.getNumLosingPositions() == 1);
        
        // Verify the trade was exited at the stop price, not the profit target.
        auto losingPos = history.beginTradingPositions()->second;
        REQUIRE(losingPos->getEntryDateTime() == ptime(date(2024, Apr, 10), time_duration(10, 30, 0)));
        REQUIRE(losingPos->getEntryPrice() == createDecimal("499.00"));
        REQUIRE(losingPos->getExitDateTime() == ptime(date(2024, Apr, 10), time_duration(11, 0, 0)));
        REQUIRE(losingPos->getExitPrice() == createDecimal("501.50")); // Exit at 501.495, rounded to 501.50 tick
        REQUIRE(losingPos->isLosingPosition() == true);
    }
}

