#pragma once

#include <memory>
#include <string>
#include "Security.h"
#include "SearchConfiguration.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "PerformanceCriteria.h"
#include "number.h"
#include "TimeFrame.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;

/**
 * @brief Enum to control the type of mock data generated for testing.
 */
enum class SeriesType { ProfitableLong, ProfitableShort, Unprofitable };

/**
 * @brief Creates a mock security with a predictable time series for testing.
 * @param seriesType An enum to control the type of price action generated.
 * @return A shared pointer to a Security object populated with test data.
 */
inline std::shared_ptr<Security<Decimal>> createMockSecurity(SeriesType seriesType)
{
    // FIX: Provide required arguments to OHLCTimeSeries constructor
    auto timeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(TimeFrame::DAILY, TradingVolume::SHARES);
    boost::posix_time::ptime startTime = boost::posix_time::time_from_string("2025-01-01 09:30:00.000");

    // FIX: Initialize Decimal from a string literal
    Decimal open("100.0");
    for (int i = 0; i < 50; ++i)
    {
        boost::posix_time::ptime barTime = startTime + boost::gregorian::days(i);
        Decimal high, low, close;

        switch (seriesType)
        {
            case SeriesType::ProfitableLong:
                // Create a clear uptrend where C > O, and next bar's open is higher
                high = open + Decimal("5");
                low = open - Decimal("1");
                close = open + Decimal("4"); // C > O is true
                break;
            case SeriesType::ProfitableShort:
                // Create a clear downtrend where O > C, and next bar's open is lower
                high = open + Decimal("1");
                low = open - Decimal("5");
                close = open - Decimal("4"); // O > C is true
                break;
            case SeriesType::Unprofitable:
                // True sideways market - no trend, small random movements that don't favor long or short
                high = open + Decimal("0.5");
                low = open - Decimal("0.5");
                // Alternate between tiny gains and losses, but overall flat
                close = open + (i % 4 == 0 ? Decimal("0.1") :
                               i % 4 == 1 ? Decimal("-0.1") :
                               i % 4 == 2 ? Decimal("0.05") : Decimal("-0.05"));
                break;
        }

        // FIX: Add missing TimeFrame argument and ensure volume is a Decimal
        timeSeries->addEntry(OHLCTimeSeriesEntry<Decimal>(barTime, open, high, low, close, Decimal("1000"), TimeFrame::DAILY));
        
        // Set next bar's open
        open = close;
    }

    // FIX: Instantiate a concrete class (EquitySecurity) instead of the abstract base class
    return std::make_shared<EquitySecurity<Decimal>>("AAPL", "Apple Computer", timeSeries);
}

/**
 * @brief Creates a default search configuration for tests.
 * @param security The mock security to use in the configuration.
 * @param minTrades The minimum number of trades required for a pattern to be valid.
 * @return A SearchConfiguration object.
 */
inline SearchConfiguration<Decimal> createTestConfig(std::shared_ptr<const Security<Decimal>> security, unsigned int minTrades = 1)
{
    // Performance criteria: 70% win rate, `minTrades`, max 5 losers, 1.1 profit factor
    auto criteria = std::make_shared<PerformanceCriteria<Decimal>>(Decimal("70.0"), minTrades, 5, Decimal("1.1"));
    
    boost::posix_time::ptime startTime = boost::posix_time::time_from_string("2025-01-02 09:30:00.000");
    boost::posix_time::ptime endTime = boost::posix_time::time_from_string("2025-01-20 09:30:00.000");

    // FIX: Reorder arguments, add missing ones, dereference criteria ptr, and use string literals for Decimals
    return SearchConfiguration<Decimal>(
        security,
        TimeFrame::DAILY,
        SearchType::EXTENDED, // Added missing argument
        false,                // Added missing argument
        Decimal("2.0"),       // Profit Target %
        Decimal("2.0"),       // Stop Loss %
        *criteria,            // Dereference pointer
        startTime,
        endTime
    );
}