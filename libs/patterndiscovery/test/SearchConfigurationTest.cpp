// test/SearchConfigurationTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <utility> // For std::pair

#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
#include "Security.h"
#include "Security.h"
#include "TimeFrame.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "number.h"
#include "TradingVolume.h"

// Define the decimal type to use for testing
using TestDecimalType = num::DefaultNumber;

// Helper to create a dummy Security object for tests
std::shared_ptr<const mkc_timeseries::Security<TestDecimalType>> createDummySecurity()
{
    auto series = std::make_shared<mkc_timeseries::OHLCTimeSeries<TestDecimalType>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES
    );

    series->addEntry(mkc_timeseries::OHLCTimeSeriesEntry<TestDecimalType>(
        boost::posix_time::ptime(boost::gregorian::date(2023, 1, 1), boost::posix_time::hours(9) + boost::posix_time::minutes(30)),
        TestDecimalType(100.0), TestDecimalType(101.0), TestDecimalType(99.0), TestDecimalType(100.5), TestDecimalType(1000), mkc_timeseries::TimeFrame::DAILY));

    return std::make_shared<const mkc_timeseries::EquitySecurity<TestDecimalType>>(
        "AAPL",
        "Apple Computer",
        series
    );
}

// Test cases for SearchConfiguration class
TEST_CASE("SearchConfiguration construction and getters", "[SearchConfiguration]")
{
    // Common test data
    auto dummySecurity = createDummySecurity();
    mkc_timeseries::TimeFrame::Duration timeFrame = mkc_timeseries::TimeFrame::DAILY;
    TestDecimalType profitTargetVal = TestDecimalType(2.5);
    TestDecimalType stopLossVal = TestDecimalType(1.5);
    PerformanceCriteria<TestDecimalType> perfCriteria(TestDecimalType(70.0), 50, 3, TestDecimalType(1.8));
    boost::posix_time::ptime startTime(boost::gregorian::date(2020, 1, 1), boost::posix_time::hours(9));
    boost::posix_time::ptime endTime(boost::gregorian::date(2023, 12, 31), boost::posix_time::hours(16));

    SECTION("Valid construction - no delay search")
    {
        SearchConfiguration<TestDecimalType> config(
            dummySecurity,
            timeFrame,
            SearchType::EXTENDED, // Use new enum
            false, // searchForDelayPatterns
            profitTargetVal,
            stopLossVal,
            perfCriteria,
            startTime,
            endTime
        );

        REQUIRE(config.getSecurity() == dummySecurity);
        REQUIRE(config.getTimeFrameDuration() == timeFrame);
        REQUIRE(config.getSearchType() == SearchType::EXTENDED);
        REQUIRE(config.isSearchingForDelayPatterns() == false);
        REQUIRE(config.getMinDelayBars() == 0);
        REQUIRE(config.getMaxDelayBars() == 0);
        REQUIRE(config.getProfitTarget() == profitTargetVal);
        REQUIRE(config.getStopLoss() == stopLossVal);
        REQUIRE(config.getBacktestStartTime() == startTime);
        REQUIRE(config.getBacktestEndTime() == endTime);
    }

    SECTION("Valid construction - with delay search")
    {
        SearchConfiguration<TestDecimalType> config(
            dummySecurity,
            timeFrame,
            SearchType::DEEP, // Test with a different search type
            true, // searchForDelayPatterns
            profitTargetVal,
            stopLossVal,
            perfCriteria,
            startTime,
            endTime
        );

        REQUIRE(config.isSearchingForDelayPatterns() == true);
        REQUIRE(config.getMinDelayBars() == 1);
        REQUIRE(config.getMaxDelayBars() == 5);
    }
    
    SECTION("getPatternLengthRange correctly returns ranges for each SearchType")
    {
        // Helper lambda to construct config and get range
        auto getRange = [&](SearchType type) {
            SearchConfiguration<TestDecimalType> cfg(dummySecurity, timeFrame, type, false, profitTargetVal, stopLossVal, perfCriteria, startTime, endTime);
            return cfg.getPatternLengthRange();
        };

        REQUIRE(getRange(SearchType::EXTENDED) == std::make_pair<unsigned int, unsigned int>(2, 6));
        REQUIRE(getRange(SearchType::DEEP) == std::make_pair<unsigned int, unsigned int>(2, 9));
        REQUIRE(getRange(SearchType::CLOSE) == std::make_pair<unsigned int, unsigned int>(3, 9));
        REQUIRE(getRange(SearchType::MIXED) == std::make_pair<unsigned int, unsigned int>(2, 9));
        REQUIRE(getRange(SearchType::HIGH_LOW) == std::make_pair<unsigned int, unsigned int>(3, 9));
        REQUIRE(getRange(SearchType::OPEN_CLOSE) == std::make_pair<unsigned int, unsigned int>(3, 9));
    }

    SECTION("Invalid construction - null security")
    {
        REQUIRE_THROWS_AS(
            SearchConfiguration<TestDecimalType>(
                nullptr, timeFrame, SearchType::EXTENDED, false,
                profitTargetVal, stopLossVal, perfCriteria, startTime, endTime
            ),
            SearchConfigurationException
        );
    }

    SECTION("Invalid construction - start time after end time")
    {
        boost::posix_time::ptime invalidStartTime(boost::gregorian::date(2024, 1, 1), boost::posix_time::hours(9));
        REQUIRE_THROWS_AS(
            SearchConfiguration<TestDecimalType>(
                dummySecurity, timeFrame, SearchType::EXTENDED, false,
                profitTargetVal, stopLossVal, perfCriteria, invalidStartTime, endTime
            ),
            SearchConfigurationException
        );
    }
}
