#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TimeSeriesCsvReader.h" 
#include "TimeFrame.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iterator>
#include <cstdio>
#include "TestUtils.h"
#include "number.h"
#include "csv.h"

using namespace mkc_timeseries;
using boost::gregorian::date;
using boost::posix_time::ptime;
using boost::posix_time::hours;
using namespace Catch;


using num::to_double;  // convert decimal to double for Approx comparisons

// ======= DecimalApprox matcher for Catch2 =======
// A simple Catch2-style approximate matcher for decimal types
template<typename Decimal>
struct DecimalApprox {
    Decimal expected;
    Decimal tolerance;
    DecimalApprox(const Decimal& e, const Decimal& t)
      : expected(e), tolerance(t) {}
};

// Overload operator== so REQUIRE(actual == decimalApprox(...)) works
template<typename Decimal>
bool operator==(const Decimal& actual, const DecimalApprox<Decimal>& approx) {
    // Use decimal's abs() to compute absolute difference
    return (actual - approx.expected).abs() <= approx.tolerance;
}

// Factory function to create DecimalApprox, analogous to Catch::Approx
template<typename Decimal>
auto decimalApprox(const Decimal& expected, const Decimal& tolerance) {
    return DecimalApprox<Decimal>(expected, tolerance);
}

// Tolerance for decimal comparisons (e.g. 0.00001 in decimal<8>)
static const DecimalType DEC_TOL = createDecimal("0.00001");

TEST_CASE("PALFormatCsvReader reads QQQ end-of-day file with known anchors", "[csv][PAL]") {
    PALFormatCsvReader<DecimalType> reader("QQQ.txt");

    REQUIRE(reader.getFileName() == "QQQ.txt");
    REQUIRE(reader.getTimeFrame() == TimeFrame::DAILY);
    REQUIRE_NOTHROW(reader.readFile());
    auto ts_ptr = reader.getTimeSeries();
    auto& series = *ts_ptr;

    REQUIRE(series.getFirstDate() == date(2021, 8, 20));
    REQUIRE(series.getLastDate()  == date(2025, 3, 31));

    auto firstEntry = *series.beginSortedAccess();
    REQUIRE(firstEntry.getOpenValue()  == decimalApprox(createDecimal("364.84"), DEC_TOL));
    REQUIRE(firstEntry.getHighValue()  == decimalApprox(createDecimal("367.92"), DEC_TOL));
    REQUIRE(firstEntry.getLowValue()   == decimalApprox(createDecimal("364.52"), DEC_TOL));
    REQUIRE(firstEntry.getCloseValue() == decimalApprox(createDecimal("367.51"), DEC_TOL));

    auto lastEntry = *std::prev(series.endSortedAccess());
    REQUIRE(lastEntry.getOpenValue()  == decimalApprox(createDecimal("461.92"), DEC_TOL));
    REQUIRE(lastEntry.getHighValue()  == decimalApprox(createDecimal("469.86"), DEC_TOL));
    REQUIRE(lastEntry.getLowValue()   == decimalApprox(createDecimal("457.33"), DEC_TOL));
    REQUIRE(lastEntry.getCloseValue() == decimalApprox(createDecimal("468.92"), DEC_TOL));
}

TEST_CASE("TradeStationFormatCsvReader reads SSO_RAD_Hourly intraday file with known anchors", "[csv][TradeStation][Intraday]") {
    TradeStationFormatCsvReader<DecimalType> reader(
        "SSO_RAD_Hourly.txt",
        TimeFrame::INTRADAY,
        TradingVolume::SHARES,
        DecimalConstants<DecimalType>::EquityTick
    );

    REQUIRE(reader.getFileName() == "SSO_RAD_Hourly.txt");
    REQUIRE(reader.getTimeFrame() == TimeFrame::INTRADAY);
    REQUIRE_NOTHROW(reader.readFile());
    auto ts_ptr = reader.getTimeSeries();
    auto& series = *ts_ptr;

    REQUIRE(series.getFirstDateTime() == ptime(date(2012, 4, 2), hours(9)));
    REQUIRE(series.getLastDateTime()  == ptime(date(2021, 4, 1), hours(15)));

    auto firstEntry = *series.beginSortedAccess();
    REQUIRE(firstEntry.getOpenValue()   == decimalApprox(createDecimal("13.93"), DEC_TOL));
    REQUIRE(firstEntry.getHighValue()   == decimalApprox(createDecimal("13.97"), DEC_TOL));
    REQUIRE(firstEntry.getLowValue()    == decimalApprox(createDecimal("13.88"), DEC_TOL));
    REQUIRE(firstEntry.getCloseValue()  == decimalApprox(createDecimal("13.93"), DEC_TOL));
    REQUIRE(firstEntry.getVolumeValue() == decimalApprox(createDecimal("0"), DEC_TOL));
}

TEST_CASE("TradeStationFormatCsvReader reads SSO_RAD_Daily daily file with known anchors", "[csv][TradeStation][Daily]") {
    TradeStationFormatCsvReader<DecimalType> reader(
        "SSO_RAD_Daily.txt",
        TimeFrame::DAILY,
        TradingVolume::SHARES,
        DecimalConstants<DecimalType>::EquityTick
    );

    REQUIRE(reader.getFileName() == "SSO_RAD_Daily.txt");
    REQUIRE(reader.getTimeFrame() == TimeFrame::DAILY);
    REQUIRE_NOTHROW(reader.readFile());
    auto ts_ptr = reader.getTimeSeries();
    auto& series = *ts_ptr;

    REQUIRE(series.getFirstDateTime() == ptime(date(2012, 4, 2), hours(0)));
    REQUIRE(series.getLastDateTime()  == ptime(date(2021, 4, 1), hours(0)));

    auto firstEntry = *series.beginSortedAccess();
    REQUIRE(firstEntry.getOpenValue()   == decimalApprox(createDecimal("13.93"), DEC_TOL));
    REQUIRE(firstEntry.getHighValue()   == decimalApprox(createDecimal("14.25"), DEC_TOL));
    REQUIRE(firstEntry.getLowValue()    == decimalApprox(createDecimal("13.88"), DEC_TOL));
    REQUIRE(firstEntry.getCloseValue()  == decimalApprox(createDecimal("14.16"), DEC_TOL));
    REQUIRE(firstEntry.getVolumeValue() == decimalApprox(createDecimal("0"), DEC_TOL));

    auto lastEntry = *std::prev(series.endSortedAccess());
    REQUIRE(lastEntry.getOpenValue()   == decimalApprox(createDecimal("103.32"), DEC_TOL));
    REQUIRE(lastEntry.getHighValue()   == decimalApprox(createDecimal("104.53"), DEC_TOL));
    REQUIRE(lastEntry.getLowValue()    == decimalApprox(createDecimal("103.21"), DEC_TOL));
    REQUIRE(lastEntry.getCloseValue()  == decimalApprox(createDecimal("104.45"), DEC_TOL));
    REQUIRE(lastEntry.getVolumeValue() == decimalApprox(createDecimal("0"), DEC_TOL));
}


TEST_CASE("TradeStationFormatCsvReader throws on too‐few‐columns", "[csv][error]") {
    // 1) write a file with exactly the eight columns the reader expects
    const std::string badPath = "bad_trade_station.csv";
    {
        std::ofstream out(badPath);
        out << "Date,Time,Open,High,Low,Close,Up,Down\n";  
        // 2) but only 5 fields in the data row
        out << "04/01/2021,15:00,100.0,101.0,99.0\n";
    }

    TradeStationFormatCsvReader<DecimalType> reader(
        badPath,
        TimeFrame::INTRADAY,
        TradingVolume::SHARES,
        DecimalConstants<DecimalType>::EquityTick
    );

    // 3) assert that the CSV library's "too few columns" error bubbles up
    REQUIRE_THROWS_AS(reader.readFile(), ::io::error::too_few_columns);
    std::remove(badPath.c_str());
}

TEST_CASE("PALFormatCsvReader throws on intraday timeframe", "[csv][PAL][error]")
{
    // prepare a minimal PAL‐style file
    const std::string path = "pal_intraday.csv";
    std::ofstream out(path);
    out << "Date,Open,High,Low,Close\n";
    out.close();

    // construct with INTRADAY should defer error until readFile()
    PALFormatCsvReader<DecimalType> reader(path, TimeFrame::INTRADAY);
    REQUIRE_THROWS_AS(reader.readFile(), std::runtime_error);
    // now clean up
    std::remove(path.c_str());
}

TEST_CASE("TradeStationFormatCsvReader throws if INTRADAY but file is daily format",
          "[csv][TradeStation][error]") 
{
    // 1) Write a daily‐style CSV: header with Vol,OI
    const std::string path = "daily_as_intraday.csv";
    {
        std::ofstream fout(path);
        fout << "Date,Time,Open,High,Low,Close,Vol,OI\n";
        fout << "04/01/2021,00:00,100.0,101.0,99.0,100.5,1234,0\n";
    }

    // 2) Construct reader asking for INTRADAY
    TradeStationFormatCsvReader<DecimalType> reader(
        path,
        TimeFrame::INTRADAY,
        TradingVolume::SHARES,
        DecimalConstants<DecimalType>::EquityTick
    );

    // 3) Expect the CSV-library to complain about missing "Up"/"Down" columns
    REQUIRE_THROWS_AS(reader.readFile(), io::error::missing_column_in_header);
}

// --- WealthLabCsvReader (non-intraday) --------------------------------------------------

TEST_CASE("WealthLabCsvReader reads Wealth-Lab daily CSV", "[csv][WealthLab][Daily]") {
    // Write a tiny Wealth-Lab style daily file
    const std::string path = "wealthlab_daily.csv";
    {
        std::ofstream out(path);
        out << "Date/Time,Open,High,Low,Close,Volume\n";
        out << "5/30/2000,0.22578125,0.23463542,0.22473957,0.22890625,306210240\n";
        out << "5/31/2000,0.228125,0.24166667,0.228125,0.23776042,472905600\n";
        out << "6/1/2000,0.24479167,0.2470052,0.23828125,0.24440105,422478240\n";
        out << "6/2/2000,0.24947917,0.2770825,0.24947917,0.2740875,596280000\n";
        out << "6/5/2000,0.2736975,0.29375,0.26276,0.2799475,495115200\n";
        out << "6/6/2000,0.28125,0.291405,0.2645825,0.266015,378134400\n";
        out << "6/7/2000,0.2640625,0.266275,0.25026,0.2541675,334973280\n";
        out << "6/8/2000,0.2606775,0.2609375,0.24583332,0.25612,462656640\n";
        out << "6/9/2000,0.2609375,0.275,0.25703,0.2634125,471522240\n";
        out << "6/12/2000,0.2645825,0.2667975,0.25052,0.25638,382571040\n";
    }

    // Note: constructor defaults (DAILY, SHARES, EquityTick) are fine here.
    WealthLabCsvReader<DecimalType> reader(
        path, TimeFrame::DAILY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );

    REQUIRE(reader.getFileName() == path);
    REQUIRE(reader.getTimeFrame() == TimeFrame::DAILY);
    REQUIRE_NOTHROW(reader.readFile());

    auto ts_ptr = reader.getTimeSeries();
    auto& series = *ts_ptr;

    // Daily bars use the default bar time (00:00) for date-only rows
    REQUIRE(series.getNumEntries() == 10);
    REQUIRE(series.getFirstDateTime() == ptime(date(2000, 5, 30), getDefaultBarTime()));
    REQUIRE(series.getLastDateTime()  == ptime(date(2000, 6, 12), getDefaultBarTime()));

    // First bar (raw values from CSV, no rounding)
    {
        auto first = *series.beginSortedAccess();
        REQUIRE(first.getOpenValue()   == decimalApprox(createDecimal("0.22578125"), DEC_TOL));
        REQUIRE(first.getHighValue()   == decimalApprox(createDecimal("0.23463542"), DEC_TOL));
        REQUIRE(first.getLowValue()    == decimalApprox(createDecimal("0.22473957"), DEC_TOL));
        REQUIRE(first.getCloseValue()  == decimalApprox(createDecimal("0.22890625"), DEC_TOL));
        REQUIRE(first.getVolumeValue() == decimalApprox(createDecimal("306210240"), DEC_TOL));
    }

    // Last bar (raw values from CSV, no rounding)
    {
        auto last = *std::prev(series.endSortedAccess());
        REQUIRE(last.getOpenValue()   == decimalApprox(createDecimal("0.2645825"), DEC_TOL));
        REQUIRE(last.getHighValue()   == decimalApprox(createDecimal("0.2667975"), DEC_TOL));
        REQUIRE(last.getLowValue()    == decimalApprox(createDecimal("0.25052"), DEC_TOL));
        REQUIRE(last.getCloseValue()  == decimalApprox(createDecimal("0.25638"), DEC_TOL));
        REQUIRE(last.getVolumeValue() == decimalApprox(createDecimal("382571040"), DEC_TOL));
    }

    std::remove(path.c_str());
}

TEST_CASE("WealthLabCsvReader reads Wealth-Lab weekly CSV", "[csv][WealthLab][Weekly]") {
    // Minimal weekly file (one row per week)
    const std::string path = "wealthlab_weekly.csv";
    {
        std::ofstream out(path);
        out << "Date/Time,Open,High,Low,Close,Volume\n";
        out << "1/7/2022,10.10,10.60,9.80,10.20,1000\n";
        out << "1/14/2022,10.30,10.80,10.00,10.50,1500\n";
        out << "1/21/2022,10.70,11.00,10.40,10.90,2000\n";
    }

    WealthLabCsvReader<DecimalType> reader(
        path, TimeFrame::WEEKLY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );

    REQUIRE(reader.getFileName() == path);
    REQUIRE(reader.getTimeFrame() == TimeFrame::WEEKLY);
    REQUIRE_NOTHROW(reader.readFile());

    auto& series = *reader.getTimeSeries();
    REQUIRE(series.getNumEntries() == 3);
    REQUIRE(series.getFirstDateTime() == ptime(date(2022, 1, 7), getDefaultBarTime()));
    REQUIRE(series.getLastDateTime()  == ptime(date(2022, 1, 21), getDefaultBarTime()));

    auto first = *series.beginSortedAccess();
    REQUIRE(first.getOpenValue()   == decimalApprox(createDecimal("10.10"), DEC_TOL));
    REQUIRE(first.getHighValue()   == decimalApprox(createDecimal("10.60"), DEC_TOL));
    REQUIRE(first.getLowValue()    == decimalApprox(createDecimal("9.80"),  DEC_TOL));
    REQUIRE(first.getCloseValue()  == decimalApprox(createDecimal("10.20"), DEC_TOL));
    REQUIRE(first.getVolumeValue() == decimalApprox(createDecimal("1000"),  DEC_TOL));

    auto last = *std::prev(series.endSortedAccess());
    REQUIRE(last.getOpenValue()   == decimalApprox(createDecimal("10.70"), DEC_TOL));
    REQUIRE(last.getHighValue()   == decimalApprox(createDecimal("11.00"), DEC_TOL));
    REQUIRE(last.getLowValue()    == decimalApprox(createDecimal("10.40"), DEC_TOL));
    REQUIRE(last.getCloseValue()  == decimalApprox(createDecimal("10.90"), DEC_TOL));
    REQUIRE(last.getVolumeValue() == decimalApprox(createDecimal("2000"),  DEC_TOL));

    std::remove(path.c_str());
}

// --- WealthLabCsvReader negative cases (non-intraday) ------------------------

TEST_CASE("WealthLabCsvReader rejects malformed input", "[csv][WealthLab][Negative]") {

    SECTION("throws on wrong header name (expects 'Date/Time')") {
        const std::string path = "wealthlab_bad_header.csv";
        {
            std::ofstream out(path);
            // Wrong first header: "Date" instead of "Date/Time"
            out << "Date,Open,High,Low,Close,Volume\n";
            out << "5/30/2000,0.22,0.23,0.22,0.23,1000\n";
        }

        WealthLabCsvReader<DecimalType> reader(
            path, TimeFrame::DAILY, TradingVolume::SHARES,
            DecimalConstants<DecimalType>::EquityTick
        );

        REQUIRE(reader.getTimeFrame() == TimeFrame::DAILY);
        REQUIRE_THROWS(reader.readFile());

        std::remove(path.c_str());
    }

    SECTION("throws when a required column is missing (Volume omitted)") {
        const std::string path = "wealthlab_missing_column.csv";
        {
            std::ofstream out(path);
            // Missing the 'Volume' column entirely
            out << "Date/Time,Open,High,Low,Close\n";
            out << "5/30/2000,0.22,0.23,0.22,0.23\n";
        }

        WealthLabCsvReader<DecimalType> reader(
            path, TimeFrame::DAILY, TradingVolume::SHARES,
            DecimalConstants<DecimalType>::EquityTick
        );

        REQUIRE(reader.getTimeFrame() == TimeFrame::DAILY);
        REQUIRE_THROWS(reader.readFile());

        std::remove(path.c_str());
    }

    SECTION("throws on malformed US date") {
        const std::string path = "wealthlab_bad_date.csv";
        {
            std::ofstream out(path);
            out << "Date/Time,Open,High,Low,Close,Volume\n";
            // Impossible month/day: from_us_string should throw
            out << "13/40/2020,10,10.5,9.5,10.1,12345\n";
        }

        WealthLabCsvReader<DecimalType> reader(
            path, TimeFrame::DAILY, TradingVolume::SHARES,
            DecimalConstants<DecimalType>::EquityTick
        );

        REQUIRE(reader.getTimeFrame() == TimeFrame::DAILY);
        REQUIRE_THROWS(reader.readFile());

        std::remove(path.c_str());
    }
}

