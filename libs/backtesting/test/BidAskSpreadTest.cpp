#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "BidAskSpread.h"
#include "TimeSeries.h"
#include "TestUtils.h"
#include <numeric>

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

// Define types consistent with the testing environment
using OHLCEntry = mkc_timeseries::OHLCTimeSeriesEntry<DecimalType>;
using OHLCSeries = mkc_timeseries::OHLCTimeSeries<DecimalType>;
using SpreadCalc = mkc_timeseries::CorwinSchultzSpreadCalculator<DecimalType>;

TEST_CASE("CorwinSchultzSpreadCalculator operations", "[BidAskSpread]")
{
    // Setup a time series with known data for testing
    OHLCSeries series(TimeFrame::DAILY, TradingVolume::SHARES);
    
    // Test data designed to give a positive spread
    auto entry1 = createEquityEntry("20230102", "101.0", "104.0", "100.0", "101.0", 10000); // Day t0
    auto entry2 = createEquityEntry("20230103", "101.0", "105.0", "101.0", "104.0", 12000); // Day t1
    
    // Test data designed to give a negative spread (due to overnight gap)
    auto entry3 = createEquityEntry("20230104", "106.0", "108.0", "106.0", "107.0", 11000); // Day t2
    
    // A day with zero range (High == Low)
    auto entry4 = createEquityEntry("20230105", "107.0", "107.0", "107.0", "107.0", 15000); // Day t3
    
    series.addEntry(*entry1);
    series.addEntry(*entry2);
    series.addEntry(*entry3);
    series.addEntry(*entry4);

    SECTION("Proportional Spread Calculation (Single Period)")
    {
        SECTION("Calculate with typical data giving a positive spread")
        {
            // Calculation for the period ending 2023-01-03, using data from 01-02 and 01-03
            DecimalType proportionalSpread = SpreadCalc::calculateProportionalSpread(*entry1, *entry2);
            // Expected value from manual calculation: ~0.015478
            REQUIRE_THAT(proportionalSpread.getAsDouble(), Catch::Matchers::WithinAbs(0.015478, 0.0001));

            // Test the series-based overload to ensure it gives the same result
            // FIX: Use getDefaultBarTime() to match how entries are stored
            ptime lookupTime(date(2023, 1, 3), getDefaultBarTime());
            DecimalType seriesSpread = SpreadCalc::calculateProportionalSpread(series, lookupTime);
            REQUIRE(seriesSpread == proportionalSpread);
        }

        SECTION("Calculate with data giving a negative spread")
        {
            // The model can produce a negative spread if the two-day volatility (gamma) is
            // sufficiently larger than the single-day volatility components (beta), often due to overnight gaps.
            DecimalType proportionalSpread = SpreadCalc::calculateProportionalSpread(*entry2, *entry3);
            REQUIRE(proportionalSpread < DecimalType(0.0));
        }

        SECTION("Throw exception for missing data")
        {
            // Requesting a date that requires a prior date not in the series
             ptime lookupTime(date(2023, 1, 2), getDefaultBarTime());
            REQUIRE_THROWS_AS(SpreadCalc::calculateProportionalSpread(series, lookupTime), std::runtime_error);
        }
    }

    SECTION("Dollar Spread Calculation (Single Period)")
    {
        DecimalType dollarSpread = SpreadCalc::calculateDollarSpread(*entry1, *entry2);
        DecimalType expectedProportional = DecimalType("0.015478");
        DecimalType expectedDollar = expectedProportional * entry2->getCloseValue(); // 0.015478 * 104.0
        
        REQUIRE_THAT(dollarSpread.getAsDouble(), Catch::Matchers::WithinAbs(expectedDollar.getAsDouble(), 0.01));
        
        // Test the series-based overload
        // FIX: Use getDefaultBarTime() to match how entries are stored
        ptime lookupTime(date(2023, 1, 3), getDefaultBarTime());
        DecimalType seriesDollarSpread = SpreadCalc::calculateDollarSpread(series, lookupTime);
        REQUIRE(seriesDollarSpread == dollarSpread);
    }
    
    SECTION("Vector Calculation")
    {
        SECTION("Calculate Proportional Spreads Vector")
        {
            auto spreads = SpreadCalc::calculateProportionalSpreadsVector(series);
            // We have 4 entries, so 3 overlapping two-day periods
            REQUIRE(spreads.size() == 3);

            // Period 1 (entry1, entry2): Should be positive ~0.015478
            REQUIRE_THAT(spreads[0].getAsDouble(), Catch::Matchers::WithinAbs(0.015478, 0.0001));

            // Period 2 (entry2, entry3): Should be negative, but floored to 0.0
            REQUIRE(spreads[1] == DecimalType(0.0));
            
            // Period 3 (entry3, entry4): Should also result in a zero spread after flooring
            // FIX: Use a tolerance for floating point comparison instead of direct equality.
            REQUIRE_THAT(spreads[2].getAsDouble(), Catch::Matchers::WithinAbs(0.0, 0.000001));
        }
        
        SECTION("Calculate Dollar Spreads Vector")
        {
            auto dollarSpreads = SpreadCalc::calculateDollarSpreadsVector(series);
            REQUIRE(dollarSpreads.size() == 3);

            // Period 1
            DecimalType expectedDollar1 = DecimalType("0.015478") * entry2->getCloseValue();
            REQUIRE_THAT(dollarSpreads[0].getAsDouble(), Catch::Matchers::WithinAbs(expectedDollar1.getAsDouble(), 0.01));
            
            // Period 2 (floored to 0)
            REQUIRE(dollarSpreads[1] == DecimalType(0.0));
            
            // Period 3 (floored to 0)
            // FIX: Use a tolerance for floating point comparison instead of direct equality.
            REQUIRE_THAT(dollarSpreads[2].getAsDouble(), Catch::Matchers::WithinAbs(0.0, 0.00001));
        }
    }

    SECTION("Average Calculation")
    {
        SECTION("Calculate Average Proportional Spread")
        {
             auto spreads = SpreadCalc::calculateProportionalSpreadsVector(series);
             DecimalType expectedAverage = (spreads[0] + spreads[1] + spreads[2]) / DecimalType(3.0);
             DecimalType calculatedAverage = SpreadCalc::calculateAverageProportionalSpread(series);
             REQUIRE_THAT(calculatedAverage.getAsDouble(), Catch::Matchers::WithinAbs(expectedAverage.getAsDouble(), 0.000001));
        }

        SECTION("Calculate Average Dollar Spread")
        {
            auto dollarSpreads = SpreadCalc::calculateDollarSpreadsVector(series);
            DecimalType expectedAverage = (dollarSpreads[0] + dollarSpreads[1] + dollarSpreads[2]) / DecimalType(3.0);
            DecimalType calculatedAverage = SpreadCalc::calculateAverageDollarSpread(series);
            REQUIRE_THAT(calculatedAverage.getAsDouble(), Catch::Matchers::WithinAbs(expectedAverage.getAsDouble(), 0.00001));
        }
    }

    SECTION("Edge Cases")
    {
        SECTION("Series with less than 2 entries")
        {
            OHLCSeries shortSeries(TimeFrame::DAILY, TradingVolume::SHARES);
            shortSeries.addEntry(*entry1);

            REQUIRE(SpreadCalc::calculateProportionalSpreadsVector(shortSeries).empty());
            REQUIRE(SpreadCalc::calculateAverageProportionalSpread(shortSeries) == DecimalType(0.0));
            REQUIRE(SpreadCalc::calculateDollarSpreadsVector(shortSeries).empty());
            REQUIRE(SpreadCalc::calculateAverageDollarSpread(shortSeries) == DecimalType(0.0));
        }

        SECTION("Series with 0 entries")
        {
            OHLCSeries emptySeries(TimeFrame::DAILY, TradingVolume::SHARES);
            REQUIRE(SpreadCalc::calculateProportionalSpreadsVector(emptySeries).empty());
            REQUIRE(SpreadCalc::calculateAverageProportionalSpread(emptySeries) == DecimalType(0.0));
        }

        SECTION("Low price is zero should throw domain_error")
        {
            auto badEntry1 = createEquityEntry("20240101", "10", "12", "0", "11", 1000);
            auto goodEntry2 = createEquityEntry("20240102", "11", "13", "10", "12", 1000);
            REQUIRE_THROWS_AS(SpreadCalc::calculateProportionalSpread(*badEntry1, *goodEntry2), std::domain_error);
        }
    }
    
    SECTION("Calculations with a larger, more realistic data set")
    {
        OHLCSeries monthSeries(TimeFrame::DAILY, TradingVolume::SHARES);
        monthSeries.addEntry(*createEquityEntry("20230301", "150.1", "152.3", "149.8", "152.1", 1.2e6));
        monthSeries.addEntry(*createEquityEntry("20230302", "152.0", "153.1", "151.5", "152.9", 1.1e6));
        monthSeries.addEntry(*createEquityEntry("20230303", "152.8", "155.0", "152.5", "154.8", 1.5e6));
        monthSeries.addEntry(*createEquityEntry("20230306", "154.9", "155.2", "153.0", "153.5", 1.3e6));
        monthSeries.addEntry(*createEquityEntry("20230307", "153.6", "153.8", "151.9", "152.2", 1.6e6));
        monthSeries.addEntry(*createEquityEntry("20230308", "152.1", "153.4", "151.1", "153.0", 1.4e6));
        monthSeries.addEntry(*createEquityEntry("20230309", "153.2", "154.8", "152.8", "154.5", 1.2e6));
        monthSeries.addEntry(*createEquityEntry("20230310", "154.6", "156.2", "154.3", "156.0", 1.7e6));
        monthSeries.addEntry(*createEquityEntry("20230313", "155.8", "157.0", "155.5", "156.5", 1.5e6));
        monthSeries.addEntry(*createEquityEntry("20230314", "156.5", "156.6", "155.0", "155.2", 1.8e6)); // Low volatility day
        monthSeries.addEntry(*createEquityEntry("20230315", "155.1", "155.5", "152.0", "152.5", 2.2e6)); // High volatility day
        monthSeries.addEntry(*createEquityEntry("20230316", "152.8", "155.0", "152.6", "154.9", 1.9e6));
        monthSeries.addEntry(*createEquityEntry("20230317", "155.0", "158.0", "154.8", "157.8", 2.5e6));
        monthSeries.addEntry(*createEquityEntry("20230320", "157.5", "157.6", "156.0", "156.2", 1.6e6));
        monthSeries.addEntry(*createEquityEntry("20230321", "156.3", "157.2", "155.8", "157.0", 1.4e6));
        monthSeries.addEntry(*createEquityEntry("20230322", "157.1", "158.5", "156.9", "158.2", 1.3e6));
        monthSeries.addEntry(*createEquityEntry("20230323", "158.3", "160.1", "158.1", "160.0", 2.0e6));
        monthSeries.addEntry(*createEquityEntry("20230324", "160.0", "160.2", "158.5", "158.8", 1.8e6));
        monthSeries.addEntry(*createEquityEntry("20230327", "158.9", "159.5", "158.0", "159.2", 1.5e6));
        monthSeries.addEntry(*createEquityEntry("20230328", "159.1", "159.3", "157.5", "157.9", 1.7e6));
        monthSeries.addEntry(*createEquityEntry("20230329", "158.0", "161.0", "157.8", "160.8", 2.1e6));
        monthSeries.addEntry(*createEquityEntry("20230330", "160.9", "162.5", "160.5", "162.2", 1.9e6));

        // There are 22 entries, so we expect 21 spread calculations
        
        SECTION("Vector calculation on larger series")
        {
            auto spreads = SpreadCalc::calculateProportionalSpreadsVector(monthSeries);
            REQUIRE(spreads.size() == 21);

            // Spot check a few values to ensure they are being calculated.
            // A value greater than 0 indicates a positive spread was found.
            // A value of 0 indicates a negative spread was calculated and floored.
            bool hasPositiveSpread = false;
            
            for(const auto& s : spreads)
            {
                REQUIRE(s >= DecimalType(0.0)); // All values should be non-negative
                if (s > DecimalType(0.0)) hasPositiveSpread = true;
            }
            REQUIRE(hasPositiveSpread); // We expect some positive spreads in this data
            // We might or might not have zero spreads, so no REQUIRE for that.
        }

        SECTION("Average calculation on larger series")
        {
            auto spreads = SpreadCalc::calculateProportionalSpreadsVector(monthSeries);
            DecimalType manualSum = std::accumulate(spreads.begin(), spreads.end(), DecimalType(0.0));
            DecimalType expectedAverage = manualSum / DecimalType(spreads.size());

            DecimalType calculatedAverage = SpreadCalc::calculateAverageProportionalSpread(monthSeries);
            
            REQUIRE(calculatedAverage > DecimalType(0.0)); // Should be a positive average spread
            REQUIRE_THAT(calculatedAverage.getAsDouble(), Catch::Matchers::WithinAbs(expectedAverage.getAsDouble(), 0.000001));
        }
    }
}