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

// Tolerance for decimal comparisons (e.g. 0.00001 in decimal<7>)
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

    TradeStationFormatCsvReader<dec::decimal<7>> reader(
        badPath,
        TimeFrame::INTRADAY,
        TradingVolume::SHARES,
        DecimalConstants<dec::decimal<7>>::EquityTick
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
    TradeStationFormatCsvReader<dec::decimal<7>> reader(
        path,
        TimeFrame::INTRADAY,
        TradingVolume::SHARES,
        DecimalConstants<dec::decimal<7>>::EquityTick
    );

    // 3) Expect the CSV-library to complain about missing "Up"/"Down" columns
    REQUIRE_THROWS_AS(reader.readFile(), io::error::missing_column_in_header);
}
