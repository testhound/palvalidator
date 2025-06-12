#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include "TimeSeriesCsvReader.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "BackTester.h" // As per the implementation plan
#include "BoostDateHelper.h"
#include "TestUtils.h"
#include "PalStrategyTestHelpers.h"
#include "number.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

// Helper function to create a simple intraday long pattern
// Pattern: Enters long if the close of the current bar is greater than the high of the previous bar.
std::shared_ptr<PriceActionLabPattern> createIntradayLongPattern()
{
    // Create description using shared_ptr
    auto percentLong = std::make_shared<DecimalType>(createDecimal("60.00"));
    auto percentShort = std::make_shared<DecimalType>(createDecimal("40.00"));
    auto desc = std::make_shared<PatternDescription>("IntradayLong.txt", 1, 20230103,
                                                     percentLong, percentShort, 10, 2);
    
    auto close0 = std::make_shared<PriceBarClose>(0); // Current bar's close
    auto high1 = std::make_shared<PriceBarHigh>(1);   // Previous bar's high
    auto longPattern = std::make_shared<GreaterThanExpr>(close0, high1);

    auto entry = createLongOnOpen();
    // Using smaller percentages suitable for intraday volatility
    auto target = createLongProfitTarget("0.50"); // 0.50%
    auto stop = createLongStopLoss("0.25");       // 0.25% (Note: Fixed to use LongStopLoss, not ShortStopLoss)

    return std::make_shared<PriceActionLabPattern>(desc, longPattern,
                                                   entry,
                                                   target,
                                                   stop);
}

// Helper function to create a simple intraday short pattern
// Pattern: Enters short if the close of the current bar is less than the low of the previous bar.
std::shared_ptr<PriceActionLabPattern> createIntradayShortPattern()
{
    // Create description using shared_ptr
    auto percentLong = std::make_shared<DecimalType>(createDecimal("40.00"));
    auto percentShort = std::make_shared<DecimalType>(createDecimal("60.00"));
    auto desc = std::make_shared<PatternDescription>("IntradayShort.txt", 1, 20230103,
                                                     percentLong, percentShort, 10, 2);

    auto low1 = std::make_shared<PriceBarLow>(1);     // Previous bar's low
    auto close0 = std::make_shared<PriceBarClose>(0); // Current bar's close
    auto shortPattern = std::make_shared<GreaterThanExpr>(low1, close0); // low1 > close0 is equivalent to close0 < low1

    auto entry = createShortOnOpen();
    auto target = createShortProfitTarget("0.50");
    auto stop = createShortStopLoss("0.25");

    return std::make_shared<PriceActionLabPattern>(desc, shortPattern,
                                                   entry,
                                                   target,
                                                   stop);
}


TEST_CASE("Intraday PalStrategy operations", "[PalStrategy][Intraday]")
{
    // Sample 5-minute intraday data for a fictional security
    const std::string intraday_data =
        "DateTime,Open,High,Low,Close,Volume\n"
        "2023-01-03 09:30:00,100.00,100.50,99.80,100.20,1000\n"
        "2023-01-03 09:35:00,100.20,100.60,100.10,100.55,1200\n" // Prior bar for first test
        "2023-01-03 09:40:00,100.55,101.20,100.50,101.10,1500\n" // << LONG TRIGGER: Close (101.10) > High of prior (100.60)
        "2023-01-03 09:45:00,101.10,101.30,100.90,101.00,1100\n" // Entry takes place on open of this bar
        "2023-01-03 09:50:00,101.00,101.85,100.95,101.80,1800\n" // << PROFIT TARGET HIT: 101.10 * 1.005 = 101.605. High (101.85) > 101.605
        "2023-01-03 09:55:00,101.80,102.00,101.50,101.60,2000\n"
        "2023-01-03 10:00:00,101.60,101.70,100.80,100.85,2200\n" // Prior bar for short test
        "2023-01-03 10:05:00,100.85,100.90,100.20,100.30,2500\n" // << SHORT TRIGGER: Close (100.30) < Low of prior (100.80)
        "2023-01-03 10:10:00,100.30,100.40,99.50,99.60,2100\n";   // Entry takes place on open of this bar

    std::stringstream stream(intraday_data);
    
    // Assume a CsvReader capable of parsing "YYYY-MM-DD HH:MM:SS" format
    // For this test, we manually create the time series to ensure correctness.
    auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:30:00", "100.00", "100.50", "99.80", "100.20", "1000"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:35:00", "100.20", "100.60", "100.10", "100.55", "1200"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:40:00", "100.55", "101.20", "100.50", "101.10", "1500"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:45:00", "101.10", "101.30", "100.90", "101.00", "1100"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:50:00", "101.00", "101.85", "100.95", "101.80", "1800"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:55:00", "101.80", "102.00", "101.50", "101.60", "2000"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:00:00", "101.60", "101.70", "100.80", "100.85", "2200"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:05:00", "100.85", "100.90", "100.20", "100.30", "2500"));
    timeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:10:00", "100.30", "100.40", "99.50", "99.60", "2100"));


    std::string equitySymbol("QQQ");
    auto security = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, "nasdaq 100", timeSeries);
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("Intraday Portfolio");
    portfolio->addSecurity(security);

    SECTION("PalLongStrategy enters and exits on intraday signals")
    {
        PalLongStrategy<DecimalType> longStrategy("Intraday PAL Long", createIntradayLongPattern(), portfolio);

        // 1. Step up to the bar where the pattern should trigger
        ptime processingDateTime(date(2023, 1, 3), time_duration(9, 40, 0));
        
        // Simulate processing the earlier bars to build up history
        // Bar 1: 09:30:00, Bar 2: 09:35:00, Bar 3: 09:40:00 (current)
        longStrategy.eventUpdateSecurityBarNumber(equitySymbol); // Bar 1
        longStrategy.eventUpdateSecurityBarNumber(equitySymbol); // Bar 2
        longStrategy.eventUpdateSecurityBarNumber(equitySymbol); // Bar 3 (current)
        
        // Debug: Check bar number and pattern requirements
        uint32_t barNumber = longStrategy.getSecurityBarNumber(equitySymbol);
        uint32_t maxBarsBack = longStrategy.getPalPattern()->getMaxBarsBack();
        
        longStrategy.eventEntryOrders(security.get(), longStrategy.getInstrumentPosition(equitySymbol), processingDateTime);

        // 2. Check that an entry order was created (position is still flat until order is processed)
        REQUIRE(longStrategy.isFlatPosition(equitySymbol)); // Still flat - order not yet processed
        auto& broker = longStrategy.getStrategyBroker();
        
        // Debug: Check if we have enough bars and if pattern should trigger
        REQUIRE(barNumber > maxBarsBack); // Must have enough history
        REQUIRE(broker.beginPendingOrders() != broker.endPendingOrders()); // But pending order exists

        // 3. Process the order on the next bar's open
        ptime entryBarDateTime(date(2023, 1, 3), time_duration(9, 45, 0));
        longStrategy.eventProcessPendingOrders(entryBarDateTime);

        // 4. Verify the position is now long
        REQUIRE(longStrategy.isLongPosition(equitySymbol));
        REQUIRE(broker.getOpenTrades() == 1);

        auto entryTransaction = broker.beginStrategyTransactions()->second;
        REQUIRE(entryTransaction->getEntryTradingOrder()->getFillDateTime() == entryBarDateTime);
        DecimalType fillPrice = entryTransaction->getTradingPosition()->getEntryPrice();
        REQUIRE(fillPrice == createDecimal("101.10")); // Filled on the Open of the 09:45 bar

        // 5. Step to the bar where the profit target is hit
        ptime exitProcessingDateTime(date(2023, 1, 3), time_duration(9, 50, 0));
        
        longStrategy.eventUpdateSecurityBarNumber(equitySymbol);
        longStrategy.eventExitOrders(security.get(), longStrategy.getInstrumentPosition(equitySymbol), exitProcessingDateTime);
        
        // 6. Process the exit order on the next bar
        ptime exitBarDateTime(date(2023, 1, 3), time_duration(9, 55, 0));
        longStrategy.eventProcessPendingOrders(exitBarDateTime);
        
        // 7. Verify the position is now flat
        REQUIRE(longStrategy.isFlatPosition(equitySymbol));
        REQUIRE(broker.getOpenTrades() == 0);
        REQUIRE(broker.getClosedTrades() == 1);
        
        auto closedPosition = broker.getClosedPositionHistory().beginTradingPositions()->second;
        REQUIRE(closedPosition->getExitDateTime() == exitBarDateTime);
        // Note: No exit reason enum available in TradingPosition, so we verify the position was closed successfully
        // Profit target was 0.5%, Entry price 101.10. Target price = 101.10 * 1.005 = 101.6055
        REQUIRE(closedPosition->getExitPrice() > createDecimal("101.60"));
    }

    SECTION("PalShortStrategy enters and exits on intraday signals")
    {
        PalShortStrategy<DecimalType> shortStrategy("Intraday PAL Short", createIntradayShortPattern(), portfolio);

        // 1. Step up to the bar where the pattern should trigger a short
        ptime processingDateTime(date(2023, 1, 3), time_duration(10, 5, 0));

        // Manually advance bar counter to have enough history for the pattern
        for (int i=0; i<8; ++i)
            shortStrategy.eventUpdateSecurityBarNumber(equitySymbol);
            
        shortStrategy.eventEntryOrders(security.get(), shortStrategy.getInstrumentPosition(equitySymbol), processingDateTime);
        
        // 2. Check that a short entry order was created (position is still flat until order is processed)
        REQUIRE(shortStrategy.isFlatPosition(equitySymbol)); // Still flat - order not yet processed
        auto& broker = shortStrategy.getStrategyBroker();
        REQUIRE(broker.beginPendingOrders() != broker.endPendingOrders()); // But pending order exists
        
        // 3. Process the order on the next bar's open
        ptime entryBarDateTime(date(2023, 1, 3), time_duration(10, 10, 0));
        shortStrategy.eventProcessPendingOrders(entryBarDateTime);
        
        // 4. Verify the position is now short
        REQUIRE(shortStrategy.isShortPosition(equitySymbol));
        REQUIRE(broker.getOpenTrades() == 1);

        auto entryTransaction = broker.beginStrategyTransactions()->second;
        REQUIRE(entryTransaction->getEntryTradingOrder()->getFillDateTime() == entryBarDateTime);
        DecimalType fillPrice = entryTransaction->getTradingPosition()->getEntryPrice();
        REQUIRE(fillPrice == createDecimal("100.30")); // Filled on the Open of the 10:10 bar

        // NOTE: The sample data does not contain a profit-target or stop-loss hit for the short position.
        // A more extensive test would include data points to verify the short exit as well.
        // For this example, we confirm the entry is correct.
    }

    SECTION("PalLongStrategy stop loss functionality")
    {
        // Create a pattern that will trigger and then hit stop loss
        PalLongStrategy<DecimalType> longStrategy("Stop Loss Test", createIntradayLongPattern(), portfolio);

        // Step up to trigger bar
        ptime processingDateTime(date(2023, 1, 3), time_duration(9, 40, 0));
        
        // Build up history
        for (int i = 0; i < 3; ++i)
            longStrategy.eventUpdateSecurityBarNumber(equitySymbol);
            
        longStrategy.eventEntryOrders(security.get(), longStrategy.getInstrumentPosition(equitySymbol), processingDateTime);
        
        // Process entry order
        ptime entryBarDateTime(date(2023, 1, 3), time_duration(9, 45, 0));
        longStrategy.eventProcessPendingOrders(entryBarDateTime);
        
        REQUIRE(longStrategy.isLongPosition(equitySymbol));
        
        // Create a scenario where stop loss would be hit
        // Entry price is 101.10, stop loss is 0.25%, so stop price = 101.10 * 0.9975 = 100.8473
        // We need to add data where low goes below this level
        auto stopLossTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        // Copy existing data
        for (auto it = timeSeries->beginRandomAccess(); it != timeSeries->endRandomAccess(); ++it) {
            stopLossTimeSeries->addEntry(*it);
        }
        // Add a bar that hits stop loss - make sure the low is well below the stop loss price
        stopLossTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:15:00", "101.80", "102.00", "100.50", "100.60", "2000")); // Low 100.50 < 100.8473
        // Add the next bar for order processing
        stopLossTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:20:00", "100.60", "100.70", "100.40", "100.50", "1800")); // Bar for order execution
        
        auto stopLossSecurity = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, "nasdaq 100", stopLossTimeSeries);
        
        // Update portfolio with new security data
        auto stopLossPortfolio = std::make_shared<Portfolio<DecimalType>>("Stop Loss Portfolio");
        stopLossPortfolio->addSecurity(stopLossSecurity);
        
        PalLongStrategy<DecimalType> stopLossStrategy("Stop Loss Strategy", createIntradayLongPattern(), stopLossPortfolio);
        
        // Simulate the same entry process
        for (int i = 0; i < 3; ++i)
            stopLossStrategy.eventUpdateSecurityBarNumber(equitySymbol);
        stopLossStrategy.eventEntryOrders(stopLossSecurity.get(), stopLossStrategy.getInstrumentPosition(equitySymbol), processingDateTime);
        stopLossStrategy.eventProcessPendingOrders(entryBarDateTime);
        
        REQUIRE(stopLossStrategy.isLongPosition(equitySymbol));
        
        // Process the stop loss bar
        ptime stopLossDateTime(date(2023, 1, 3), time_duration(10, 15, 0));
        stopLossStrategy.eventUpdateSecurityBarNumber(equitySymbol);
        stopLossStrategy.eventExitOrders(stopLossSecurity.get(), stopLossStrategy.getInstrumentPosition(equitySymbol), stopLossDateTime);
        
        // Verify stop loss order was created
        auto& broker = stopLossStrategy.getStrategyBroker();
        REQUIRE(broker.beginPendingOrders() != broker.endPendingOrders());
        
        // Process the stop loss order on the next bar
        ptime stopLossExecutionDateTime(date(2023, 1, 3), time_duration(10, 20, 0));
        stopLossStrategy.eventProcessPendingOrders(stopLossExecutionDateTime);
        
        // Verify position was closed by stop loss
        REQUIRE(stopLossStrategy.isFlatPosition(equitySymbol));
        REQUIRE(broker.getClosedTrades() == 1);
    }

    SECTION("PalShortStrategy complete entry and exit cycle")
    {
        PalShortStrategy<DecimalType> shortStrategy("Complete Short Test", createIntradayShortPattern(), portfolio);

        // Create extended data with profit target hit for short
        auto extendedTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        // Copy existing data
        for (auto it = timeSeries->beginRandomAccess(); it != timeSeries->endRandomAccess(); ++it) {
            extendedTimeSeries->addEntry(*it);
        }
        // Add bars where short profit target is hit
        // Entry price would be 100.30, profit target 0.50%, so target = 100.30 * 0.995 = 99.7985
        extendedTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:15:00", "99.60", "100.00", "99.50", "99.70", "1800")); // Low 99.50 < 99.7985
        extendedTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "10:20:00", "99.70", "99.80", "99.40", "99.50", "1500")); // Additional bar for exit processing
        
        auto extendedSecurity = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, "nasdaq 100", extendedTimeSeries);
        auto extendedPortfolio = std::make_shared<Portfolio<DecimalType>>("Extended Portfolio");
        extendedPortfolio->addSecurity(extendedSecurity);
        
        PalShortStrategy<DecimalType> completeShortStrategy("Complete Short", createIntradayShortPattern(), extendedPortfolio);

        // Step to trigger bar
        ptime processingDateTime(date(2023, 1, 3), time_duration(10, 5, 0));
        
        // Build history
        for (int i = 0; i < 8; ++i)
            completeShortStrategy.eventUpdateSecurityBarNumber(equitySymbol);
            
        completeShortStrategy.eventEntryOrders(extendedSecurity.get(), completeShortStrategy.getInstrumentPosition(equitySymbol), processingDateTime);
        
        // Process entry
        ptime entryBarDateTime(date(2023, 1, 3), time_duration(10, 10, 0));
        completeShortStrategy.eventProcessPendingOrders(entryBarDateTime);
        
        REQUIRE(completeShortStrategy.isShortPosition(equitySymbol));
        
        // Process to profit target bar
        ptime exitProcessingDateTime(date(2023, 1, 3), time_duration(10, 15, 0));
        completeShortStrategy.eventUpdateSecurityBarNumber(equitySymbol);
        completeShortStrategy.eventExitOrders(extendedSecurity.get(), completeShortStrategy.getInstrumentPosition(equitySymbol), exitProcessingDateTime);
        
        // Verify exit order was created
        auto& broker = completeShortStrategy.getStrategyBroker();
        REQUIRE(broker.beginPendingOrders() != broker.endPendingOrders());
        
        // Process exit order on the next bar
        ptime exitBarDateTime(date(2023, 1, 3), time_duration(10, 20, 0));
        completeShortStrategy.eventProcessPendingOrders(exitBarDateTime);
        
        // Verify position closed
        REQUIRE(completeShortStrategy.isFlatPosition(equitySymbol));
        REQUIRE(broker.getClosedTrades() == 1);
    }

    SECTION("Pattern evaluation with insufficient history")
    {
        PalLongStrategy<DecimalType> longStrategy("Insufficient History Test", createIntradayLongPattern(), portfolio);

        // Try to evaluate pattern with insufficient bars
        ptime processingDateTime(date(2023, 1, 3), time_duration(9, 35, 0)); // Only 2 bars, need more for pattern
        
        // Only advance 1 bar (insufficient for pattern requiring lookback)
        longStrategy.eventUpdateSecurityBarNumber(equitySymbol);
        
        uint32_t barNumber = longStrategy.getSecurityBarNumber(equitySymbol);
        uint32_t maxBarsBack = longStrategy.getPalPattern()->getMaxBarsBack();
        
        // Should not have enough history
        REQUIRE(barNumber <= maxBarsBack);
        
        longStrategy.eventEntryOrders(security.get(), longStrategy.getInstrumentPosition(equitySymbol), processingDateTime);
        
        // Should remain flat - no order should be created
        REQUIRE(longStrategy.isFlatPosition(equitySymbol));
        auto& broker = longStrategy.getStrategyBroker();
        REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders()); // No pending orders
    }

    SECTION("Intraday time precision validation")
    {
        // Test that strategies work correctly with precise intraday timestamps
        // This validates the ptime precision mentioned in the implementation plan
        auto precisionTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Create bars with precise minute-level timestamps
        precisionTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:30:00", "100.00", "100.50", "99.80", "100.20", "1000"));
        precisionTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:31:00", "100.20", "100.60", "100.10", "100.55", "1200")); // 1-minute precision
        precisionTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:32:00", "100.55", "101.20", "100.50", "101.10", "1500")); // Pattern trigger
        precisionTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:33:00", "101.10", "101.30", "100.90", "101.00", "1100"));
        
        auto precisionSecurity = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol + "_precision", "precision test", precisionTimeSeries);
        auto precisionPortfolio = std::make_shared<Portfolio<DecimalType>>("Precision Portfolio");
        precisionPortfolio->addSecurity(precisionSecurity);
        
        PalLongStrategy<DecimalType> precisionStrategy("Precision Strategy", createIntradayLongPattern(), precisionPortfolio);
        
        // Test with exact minute-level precision
        ptime preciseTime(date(2023, 1, 3), time_duration(9, 32, 0)); // Exact minute
        
        // Build history
        for (int i = 0; i < 3; ++i)
            precisionStrategy.eventUpdateSecurityBarNumber(equitySymbol + "_precision");
            
        precisionStrategy.eventEntryOrders(precisionSecurity.get(), precisionStrategy.getInstrumentPosition(equitySymbol + "_precision"), preciseTime);
        
        // Verify the strategy can handle precise timestamps
        REQUIRE(precisionStrategy.isFlatPosition(equitySymbol + "_precision")); // Still flat until order processed
        auto& broker = precisionStrategy.getStrategyBroker();
        
        // Should have created an order with precise timing
        bool hasOrders = (broker.beginPendingOrders() != broker.endPendingOrders());
        REQUIRE(hasOrders); // Pattern should trigger with sufficient history
    }

    SECTION("Market boundary conditions")
    {
        // Test behavior at market open and close times
        auto boundaryTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Market open scenario
        boundaryTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:30:00", "100.00", "100.50", "99.80", "100.20", "1000")); // Market open
        boundaryTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:35:00", "100.20", "100.60", "100.10", "100.55", "1200"));
        
        // Market close scenario
        boundaryTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "15:55:00", "101.00", "101.20", "100.80", "101.10", "800"));  // Near close
        boundaryTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "16:00:00", "101.10", "101.15", "101.00", "101.05", "500"));  // Market close
        
        auto boundarySecurity = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol + "_boundary", "boundary test", boundaryTimeSeries);
        auto boundaryPortfolio = std::make_shared<Portfolio<DecimalType>>("Boundary Portfolio");
        boundaryPortfolio->addSecurity(boundarySecurity);
        
        PalLongStrategy<DecimalType> boundaryStrategy("Boundary Strategy", createIntradayLongPattern(), boundaryPortfolio);
        
        // Test that strategy can handle market boundary times
        ptime marketOpenTime(date(2023, 1, 3), time_duration(9, 30, 0));
        ptime marketCloseTime(date(2023, 1, 3), time_duration(16, 0, 0));
        
        // Should be able to process at market boundaries without errors
        boundaryStrategy.eventUpdateSecurityBarNumber(equitySymbol + "_boundary");
        boundaryStrategy.eventEntryOrders(boundarySecurity.get(), boundaryStrategy.getInstrumentPosition(equitySymbol + "_boundary"), marketOpenTime);
        
        // No specific assertions here - mainly testing that no exceptions are thrown
        REQUIRE(boundaryStrategy.isFlatPosition(equitySymbol + "_boundary"));
    }


    SECTION("Performance with large intraday datasets")
    {
        // Test strategy performance with larger datasets
        auto largeTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Create a full trading day of 1-minute bars (390 bars from 9:30 AM to 4:00 PM)
        ptime startTime(date(2023, 1, 3), time_duration(9, 30, 0));
        DecimalType price = createDecimal("100.00");
        
        for (int i = 0; i < 390; ++i) {
            ptime barTime = startTime + minutes(i);
            std::string timeStr = to_simple_string(barTime.time_of_day());
            
            // Create some price movement
            DecimalType open = price;
            DecimalType high = price + createDecimal("0.10");
            DecimalType low = price - createDecimal("0.05");
            DecimalType close = price + createDecimal("0.02");
            price = close; // Update for next bar
            
            largeTimeSeries->addEntry(*createTimeSeriesEntry("20230103", timeStr,
                num::toString(open), num::toString(high), num::toString(low), num::toString(close), "100"));
        }
        
        auto largeSecurity = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol + "_large", "large dataset", largeTimeSeries);
        auto largePortfolio = std::make_shared<Portfolio<DecimalType>>("Large Portfolio");
        largePortfolio->addSecurity(largeSecurity);
        
        PalLongStrategy<DecimalType> largeStrategy("Large Dataset Strategy", createIntradayLongPattern(), largePortfolio);
        
        // Test that strategy can handle large datasets efficiently
        REQUIRE(largeSecurity->getTimeSeries()->getNumEntries() == 390);
        
        // Process multiple bars to test performance
        for (int i = 0; i < 10; ++i) {
            largeStrategy.eventUpdateSecurityBarNumber(equitySymbol + "_large");
        }
        
        ptime testTime(date(2023, 1, 3), time_duration(10, 0, 0));
        largeStrategy.eventEntryOrders(largeSecurity.get(), largeStrategy.getInstrumentPosition(equitySymbol + "_large"), testTime);
        
        // Should complete without performance issues
        REQUIRE(largeStrategy.getSecurityBarNumber(equitySymbol + "_large") == 10);
    }

    SECTION("Error handling with invalid data")
    {
        // Test strategy behavior with edge cases and invalid data
        auto invalidTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        
        // Add only one bar (insufficient for most patterns)
        invalidTimeSeries->addEntry(*createTimeSeriesEntry("20230103", "09:30:00", "100.00", "100.50", "99.80", "100.20", "1000"));
        
        auto invalidSecurity = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol + "_invalid", "invalid data", invalidTimeSeries);
        auto invalidPortfolio = std::make_shared<Portfolio<DecimalType>>("Invalid Portfolio");
        invalidPortfolio->addSecurity(invalidSecurity);
        
        PalLongStrategy<DecimalType> invalidStrategy("Invalid Data Strategy", createIntradayLongPattern(), invalidPortfolio);
        
        ptime testTime(date(2023, 1, 3), time_duration(9, 30, 0));
        
        // Should handle insufficient data gracefully
        invalidStrategy.eventEntryOrders(invalidSecurity.get(), invalidStrategy.getInstrumentPosition(equitySymbol + "_invalid"), testTime);
        
        // Should remain flat with no orders
        REQUIRE(invalidStrategy.isFlatPosition(equitySymbol + "_invalid"));
        auto& broker = invalidStrategy.getStrategyBroker();
        REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
    }
}
