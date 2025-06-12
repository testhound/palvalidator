// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include <catch2/catch_test_macros.hpp>
#include "BackTester.h"
#include "PalStrategy.h"
#include "TimeSeriesCsvReader.h"
#include "Security.h"
#include "SecurityFactory.h"
#include "Portfolio.h"
#include "StrategyBroker.h"
#include "TestUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <stdexcept>

using namespace mkc_timeseries;
using namespace boost::posix_time;
using namespace boost::gregorian;

// A complete implementation of the test strategy class, including all pure virtual methods.
class TestPtimeStrategy : public BacktesterStrategy<DecimalType>
{
private:
    std::vector<ptime> mProcessedTimestamps;
    
public:
    // Constructor updated to correctly call the base class constructor.
    // It no longer takes a StrategyBroker, as the base class manages it internally.
    TestPtimeStrategy(const std::string& name,
                      std::shared_ptr<Portfolio<DecimalType>> portfolio)
        : BacktesterStrategy<DecimalType>(name, portfolio, defaultStrategyOptions)
    {}
    
    // --- Overridden event handlers ---
    void eventEntryOrders(Security<DecimalType>* security,
                          const InstrumentPosition<DecimalType>& instrPos,
                          const boost::posix_time::ptime& processingDateTime) override
    {
        mProcessedTimestamps.push_back(processingDateTime);
    }
    
    void eventExitOrders(Security<DecimalType>* security,
                         const InstrumentPosition<DecimalType>& instrPos,
                         const boost::posix_time::ptime& processingDateTime) override
    {
        // This is intentionally left empty for these tests, but must be implemented.
    }
    
    // Remove the eventProcessPendingOrders override since it's not virtual
    
    // --- Implementation of pure virtual functions from BacktesterStrategy ---

    const TradingVolume& getSizeForOrder(const Security<DecimalType>& aSecurity) const override
    {
        // Use static TradingVolume instances instead of private members
        static const TradingVolume oneShare(1, TradingVolume::SHARES);
        static const TradingVolume oneContract(1, TradingVolume::CONTRACTS);
        
        if (aSecurity.isEquitySecurity())
            return oneShare;
        else
            return oneContract;
    }

    std::shared_ptr<BacktesterStrategy<DecimalType>>
    clone (const std::shared_ptr<Portfolio<DecimalType>>& portfolio) const override
    {
      return std::make_shared<TestPtimeStrategy>(this->getStrategyName(), portfolio);
    }

    std::shared_ptr<BacktesterStrategy<DecimalType>> 
    cloneForBackTesting () const override
    {
      return std::make_shared<TestPtimeStrategy>(this->getStrategyName(), this->getPortfolio());
    }

    std::vector<int> getPositionDirectionVector() const override
    {
        throw std::runtime_error("getPositionDirectionVector is not implemented for TestPtimeStrategy.");
    }

    std::vector<DecimalType> getPositionReturnsVector() const override
    {
        throw std::runtime_error("getPositionReturnsVector is not implemented for TestPtimeStrategy.");
    }

    unsigned long numTradingOpportunities() const override
    {
        throw std::runtime_error("numTradingOpportunities is not implemented for TestPtimeStrategy.");
    }
    
    // --- Accessors for test verification ---
    const std::vector<ptime>& getProcessedTimestamps() const { return mProcessedTimestamps; }
};

TEST_CASE("UnifiedPtimeBackTester maintains ptime precision", "[backtesting]")
{
    SECTION("Daily data processed with full ptime precision")
    {
        auto startDate = date(2022, 1, 3);
        auto endDate = date(2022, 1, 7);
        DailyBackTester<DecimalType> backtester(startDate, endDate);
        
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "100.0", "105.0", "99.0", "103.0", "1000000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220104", "103.0", "108.0", "102.0", "107.0", "1100000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220105", "107.0", "110.0", "106.0", "109.0", "1200000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220106", "109.0", "112.0", "108.0", "111.0", "1300000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220107", "111.0", "114.0", "110.0", "113.0", "1400000"));
        
        auto testSecurity = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "QQQ Security", timeSeries);

        ptime jan3(date(2022, 1, 3), time_duration(15, 0, 0));
        ptime jan4(date(2022, 1, 4), time_duration(15, 0, 0));
        ptime jan5(date(2022, 1, 5), time_duration(15, 0, 0));
        ptime jan6(date(2022, 1, 6), time_duration(15, 0, 0));
        
        auto portfolio = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
        portfolio->addSecurity(testSecurity);
        
        // Updated strategy creation
        auto strategy = std::make_shared<TestPtimeStrategy>("TestStrategy", portfolio);
        
        backtester.addStrategy(strategy);
        backtester.backtest();
        
        auto processedTimestamps = strategy->getProcessedTimestamps();
        REQUIRE(processedTimestamps.size() == 4);
        REQUIRE(processedTimestamps[0] == jan3);
        REQUIRE(processedTimestamps[1] == jan4);
        REQUIRE(processedTimestamps[2] == jan5);
        REQUIRE(processedTimestamps[3] == jan6);
    }
    
    SECTION("Intraday data processed with minute-level precision")
    {
        ptime startDateTime(date(2022, 1, 3), time_duration(9, 30, 0));
        ptime endDateTime(date(2022, 1, 3), time_duration(16, 0, 0));
        IntradayBackTester<DecimalType> backtester(startDateTime, endDateTime);
        
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "09:30:00", "100.0", "100.5", "99.8", "100.2", "50000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "09:35:00", "100.2", "100.8", "100.1", "100.6", "55000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "09:40:00", "100.6", "101.0", "100.5", "100.9", "60000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "09:45:00", "100.9", "101.2", "100.8", "101.1", "65000"));
        
        auto testSecurity = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "QQQ Security", timeSeries);
        
        ptime bar1(date(2022, 1, 3), time_duration(9, 30, 0));
        ptime bar2(date(2022, 1, 3), time_duration(9, 35, 0));
        ptime bar3(date(2022, 1, 3), time_duration(9, 40, 0));
        
        auto portfolio = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
        portfolio->addSecurity(testSecurity);
        
        // Updated strategy creation
        auto strategy = std::make_shared<TestPtimeStrategy>("TestStrategy", portfolio);
        
        backtester.addStrategy(strategy);
        backtester.backtest();
        
        auto processedTimestamps = strategy->getProcessedTimestamps();
        REQUIRE(processedTimestamps.size() == 3);
        REQUIRE(processedTimestamps[0] == bar1);
        REQUIRE(processedTimestamps[1] == bar2);
        REQUIRE(processedTimestamps[2] == bar3);
        REQUIRE((processedTimestamps[1] - processedTimestamps[0]).total_seconds() == 300);
        REQUIRE((processedTimestamps[2] - processedTimestamps[1]).total_seconds() == 300);
    }
}

TEST_CASE("UnifiedPtimeBackTester data-driven iteration", "[backtesting]")
{
    SECTION("Backtester iterates over actual data timestamps only")
    {
        auto startDate = date(2022, 1, 3);
        auto endDate = date(2022, 1, 14);
        DailyBackTester<DecimalType> backtester(startDate, endDate);
        
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "100.0", "101.0", "99.0", "100.5", "1000000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220104", "100.5", "102.0", "100.0", "101.5", "1100000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220105", "101.5", "103.0", "101.0", "102.5", "1200000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220107", "102.5", "104.0", "102.0", "103.5", "1300000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220110", "103.5", "105.0", "103.0", "104.5", "1400000"));
        
        auto testSecurity = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "QQQ Security", timeSeries);
        
        auto portfolio = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
        portfolio->addSecurity(testSecurity);
        
        // Updated strategy creation
        auto strategy = std::make_shared<TestPtimeStrategy>("TestStrategy", portfolio);
        
        backtester.addStrategy(strategy);
        backtester.backtest();
        
        auto processedTimestamps = strategy->getProcessedTimestamps();
        REQUIRE(processedTimestamps.size() == 4);
        
        for (const auto& ts : processedTimestamps) {
            REQUIRE(ts.date() != date(2022, 1, 6)); // Thursday
        }
        
        auto gap1 = processedTimestamps[3] - processedTimestamps[2];
        REQUIRE(gap1.hours() > 24);
    }
    
    SECTION("Multiple securities with different timestamps are unified")
    {
        ptime startDateTime(date(2022, 1, 3), time_duration(9, 30, 0));
        ptime endDateTime(date(2022, 1, 3), time_duration(10, 0, 0));
        IntradayBackTester<DecimalType> backtester(startDateTime, endDateTime);
        
        auto timeSeries1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        timeSeries1->addEntry(*createTimeSeriesEntry("20220103", "09:30:00", "100.0", "100.5", "99.8", "100.2", "50000"));
        timeSeries1->addEntry(*createTimeSeriesEntry("20220103", "09:35:00", "100.2", "100.8", "100.1", "100.6", "55000"));
        timeSeries1->addEntry(*createTimeSeriesEntry("20220103", "09:40:00", "100.6", "101.0", "100.5", "100.9", "60000"));
        timeSeries1->addEntry(*createTimeSeriesEntry("20220103", "09:45:00", "100.9", "101.2", "100.8", "101.1", "65000"));
        auto security1 = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "QQQ Security", timeSeries1);

        auto timeSeries2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        timeSeries2->addEntry(*createTimeSeriesEntry("20220103", "09:32:00", "200.0", "200.5", "199.8", "200.2", "80000"));
        timeSeries2->addEntry(*createTimeSeriesEntry("20220103", "09:37:00", "200.2", "200.8", "200.1", "200.6", "85000"));
        timeSeries2->addEntry(*createTimeSeriesEntry("20220103", "09:42:00", "200.6", "201.0", "200.5", "200.9", "90000"));
        auto security2 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPY Security", timeSeries2);
        
        auto portfolio = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
        portfolio->addSecurity(security1);
        portfolio->addSecurity(security2);
        
        // Updated strategy creation
        auto strategy = std::make_shared<TestPtimeStrategy>("TestStrategy", portfolio);
        
        backtester.addStrategy(strategy);
        backtester.backtest();
        
        auto processedTimestamps = strategy->getProcessedTimestamps();
        
        // Debug output
        INFO("Processed timestamps count: " << processedTimestamps.size());
        
        // Verify that we processed timestamps from both securities
        // We should have 6 processed timestamps (3 from each security, skipping the first bar)
        REQUIRE(processedTimestamps.size() == 6);
        
        // Verify that we have timestamps from both securities
        std::set<ptime> uniqueTimestamps(processedTimestamps.begin(), processedTimestamps.end());
        REQUIRE(uniqueTimestamps.size() == 6); // All should be unique since they're at different times
    }
}

TEST_CASE("DateRange filtering with ptime precision", "[backtesting]")
{
    SECTION("DateRange respects exact ptime boundaries")
    {
        ptime startDateTime(date(2022, 1, 3), time_duration(10, 0, 0));
        ptime endDateTime(date(2022, 1, 3), time_duration(14, 0, 0));
        IntradayBackTester<DecimalType> backtester(startDateTime, endDateTime);
        
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "09:30:00", "100.0", "100.5", "99.8", "100.2", "50000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "09:45:00", "100.2", "100.6", "100.0", "100.4", "51000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "10:00:00", "100.4", "100.8", "100.2", "100.6", "52000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "10:30:00", "100.6", "101.0", "100.4", "100.8", "53000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "11:00:00", "100.8", "101.2", "100.6", "101.0", "54000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "13:30:00", "101.0", "101.4", "100.8", "101.2", "55000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "14:00:00", "101.2", "101.6", "101.0", "101.4", "56000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "14:30:00", "101.4", "101.8", "101.2", "101.6", "57000"));
        timeSeries->addEntry(*createTimeSeriesEntry("20220103", "15:00:00", "101.6", "102.0", "101.4", "101.8", "58000"));
        
        auto testSecurity = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "QQQ Security", timeSeries);
        
        auto portfolio = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
        portfolio->addSecurity(testSecurity);
        
        // Updated strategy creation
        auto strategy = std::make_shared<TestPtimeStrategy>("TestStrategy", portfolio);
        
        backtester.addStrategy(strategy);
        backtester.backtest();
        
        auto processedTimestamps = strategy->getProcessedTimestamps();
        REQUIRE(processedTimestamps.size() == 4);
        
        for (const auto& ts : processedTimestamps) {
            REQUIRE(ts >= startDateTime);
            REQUIRE(ts < endDateTime);
        }
    }
}

TEST_CASE("IntradayBackTester constructor validation", "[backtesting]")
{
    SECTION("ptime constructor creates exact range")
    {
        ptime start(date(2022, 1, 3), time_duration(9, 30, 0));
        ptime end(date(2022, 1, 3), time_duration(16, 0, 0));
        IntradayBackTester<DecimalType> backtester(start, end);
        
        auto firstRange = backtester.beginBacktestDateRange()->second;
        REQUIRE(firstRange.getFirstDateTime() == start);
        REQUIRE(firstRange.getLastDateTime() == end);
    }
    
    SECTION("ptime constructor with full-day range")
    {
        date startDate(2022, 1, 3);
        date endDate(2022, 1, 5);
        
        ptime rangeStart(startDate, time_duration(0, 0, 0));
        ptime rangeEnd(endDate, time_duration(23, 59, 59));
        IntradayBackTester<DecimalType> backtester(rangeStart, rangeEnd);
        
        auto firstRange = backtester.beginBacktestDateRange()->second;
        REQUIRE(firstRange.getFirstDateTime() == rangeStart);
        REQUIRE(firstRange.getLastDateTime() == rangeEnd);
    }
    
    SECTION("Clone maintains date ranges")
    {
        ptime start(date(2022, 1, 3), time_duration(9, 30, 0));
        ptime end(date(2022, 1, 3), time_duration(16, 0, 0));
        IntradayBackTester<DecimalType> original(start, end);
        
        ptime start2(date(2022, 1, 4), time_duration(9, 30, 0));
        ptime end2(date(2022, 1, 4), time_duration(16, 0, 0));
        original.addDateRange(DateRange(start2, end2));
        
        auto cloned = original.clone();
        
        REQUIRE(original.numBackTestRanges() == cloned->numBackTestRanges());
        
        auto origIt = original.beginBacktestDateRange();
        auto cloneIt = cloned->beginBacktestDateRange();
        
        while (origIt != original.endBacktestDateRange()) {
            REQUIRE(origIt->second.getFirstDateTime() == cloneIt->second.getFirstDateTime());
            REQUIRE(origIt->second.getLastDateTime() == cloneIt->second.getLastDateTime());
            ++origIt;
            ++cloneIt;
        }
    }
}

TEST_CASE("Performance and memory efficiency", "[backtesting]")
{
    SECTION("No date vector allocation reduces memory usage")
    {
        date startDate(2020, 1, 1);
        date endDate(2022, 12, 31);
        DailyBackTester<DecimalType> backtester(startDate, endDate);
        
        auto timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
        
        for (int i = 0; i < 10; ++i) {
            date d = startDate + days(i * 100);
            std::string dateStr = to_iso_string(d);
            // For daily time series, don't include time component
            timeSeries->addEntry(*createTimeSeriesEntry(dateStr,
                std::to_string(100.0 + i), std::to_string(101.0 + i),
                std::to_string(99.0 + i), std::to_string(100.5 + i), "1000000"));
        }
        
        auto testSecurity = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "QQQ Security", timeSeries);
        
        auto portfolio = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
        portfolio->addSecurity(testSecurity);
        
        // Updated strategy creation
        auto strategy = std::make_shared<TestPtimeStrategy>("TestStrategy", portfolio);
        
        backtester.addStrategy(strategy);
        backtester.backtest();
        
        auto processedTimestamps = strategy->getProcessedTimestamps();
        REQUIRE(processedTimestamps.size() == 9);
    }
}
