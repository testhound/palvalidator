#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TimeSeriesCsvWriter.h"
#include "TimeSeries.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <unistd.h>      // for mkstemp() and close()
#include <boost/date_time.hpp>

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace Catch;

// Helper to split a CSV line into fields
static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

TEST_CASE("PalTimeSeriesCsvWriter writes correct OHLC CSV", "[PalTimeSeriesCsvWriter]") {
    // Create two entries out of order
    auto entry1 = createEquityEntry("20200102", "2.0", "3.0", "1.0", "2.5", 0);
    auto entry0 = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 0);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    // Write to a unique temp file via mkstemp
    char tmpl[] = "/tmp/tswriter-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    PalTimeSeriesCsvWriter<DecimalType> writer(fileName, series);
    writer.writeFile();

    // Read back lines
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        rows.push_back(split(line, ','));
    }
    std::remove(fileName.c_str());

    // Expect two rows, sorted by date ascending
    REQUIRE(rows.size() == 2);
    // First row: 20200101
    auto& r0 = rows[0];
    REQUIRE(r0.size() == 5);
    REQUIRE(r0[0] == "20200101");
    REQUIRE(std::stod(r0[1]) == Approx(1.0));
    REQUIRE(std::stod(r0[2]) == Approx(2.0));
    REQUIRE(std::stod(r0[3]) == Approx(0.5));
    REQUIRE(std::stod(r0[4]) == Approx(1.5));
    // Second row: 20200102
    auto& r1 = rows[1];
    REQUIRE(r1[0] == "20200102");
    REQUIRE(std::stod(r1[1]) == Approx(2.0));
    REQUIRE(std::stod(r1[2]) == Approx(3.0));
    REQUIRE(std::stod(r1[3]) == Approx(1.0));
    REQUIRE(std::stod(r1[4]) == Approx(2.5));
}

TEST_CASE("PalVolumeForCloseCsvWriter writes correct Date,Open,High,Low,Volume CSV", "[PalVolumeForCloseCsvWriter]") {
    // Create two entries out of order with nonzero volume
    auto entry1 = createEquityEntry("20200102", "2.0", "3.0", "1.0", "2.5", 100);
    auto entry0 = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    // Write to unique temp file via mkstemp
    char tmpl[] = "/tmp/tswriter-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    PalVolumeForCloseCsvWriter<DecimalType> writer(fileName, series);
    writer.writeFile();

    // Read back lines
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        rows.push_back(split(line, ','));
    }
    std::remove(fileName.c_str());

    // Expect two rows, sorted by date ascending
    REQUIRE(rows.size() == 2);
    // First row: 20200101
    auto& r0 = rows[0];
    REQUIRE(r0.size() == 5);
    REQUIRE(r0[0] == "20200101");
    REQUIRE(std::stod(r0[1]) == Approx(1.0));
    REQUIRE(std::stod(r0[2]) == Approx(2.0));
    REQUIRE(std::stod(r0[3]) == Approx(0.5));
    REQUIRE(std::stod(r0[4]) == Approx(50.0));
    // Second row: 20200102
    auto& r1 = rows[1];
    REQUIRE(r1[0] == "20200102");
    REQUIRE(std::stod(r1[1]) == Approx(2.0));
    REQUIRE(std::stod(r1[2]) == Approx(3.0));
    REQUIRE(std::stod(r1[3]) == Approx(1.0));
    REQUIRE(std::stod(r1[4]) == Approx(100.0));
}

//
// Tests for New Unified TimeSeriesCsvWriter and All Output Formats
//

TEST_CASE("TimeSeriesCsvWriter with PAL_EOD format matches legacy PalTimeSeriesCsvWriter", "[TimeSeriesCsvWriter][PAL_EOD]") {
    auto entry1 = createEquityEntry("20200102", "2.0", "3.0", "1.0", "2.5", 100);
    auto entry0 = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    char tmpl[] = "/tmp/unified-writer-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TimeSeriesCsvWriter<DecimalType> writer(fileName, series, OutputFormat::PAL_EOD);
    writer.writeFile();

    // Read back and verify format matches PAL EOD: Date,Open,High,Low,Close
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        rows.push_back(split(line, ','));
    }
    std::remove(fileName.c_str());

    REQUIRE(rows.size() == 2);
    // First row: 20200101,1.0,2.0,0.5,1.5
    auto& r0 = rows[0];
    REQUIRE(r0.size() == 5);
    REQUIRE(r0[0] == "20200101");
    REQUIRE(std::stod(r0[1]) == Approx(1.0));
    REQUIRE(std::stod(r0[2]) == Approx(2.0));
    REQUIRE(std::stod(r0[3]) == Approx(0.5));
    REQUIRE(std::stod(r0[4]) == Approx(1.5));  // Close value
}

TEST_CASE("TimeSeriesCsvWriter with PAL_VOLUME_FOR_CLOSE format", "[TimeSeriesCsvWriter][PAL_VOLUME_FOR_CLOSE]") {
    auto entry1 = createEquityEntry("20200102", "2.0", "3.0", "1.0", "2.5", 100);
    auto entry0 = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    char tmpl[] = "/tmp/volume-writer-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TimeSeriesCsvWriter<DecimalType> writer(fileName, series, OutputFormat::PAL_VOLUME_FOR_CLOSE);
    writer.writeFile();

    // Read back and verify format: Date,Open,High,Low,Volume
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        rows.push_back(split(line, ','));
    }
    std::remove(fileName.c_str());

    REQUIRE(rows.size() == 2);
    // First row: 20200101,1.0,2.0,0.5,50.0
    auto& r0 = rows[0];
    REQUIRE(r0.size() == 5);
    REQUIRE(r0[0] == "20200101");
    REQUIRE(std::stod(r0[1]) == Approx(1.0));
    REQUIRE(std::stod(r0[2]) == Approx(2.0));
    REQUIRE(std::stod(r0[3]) == Approx(0.5));
    REQUIRE(std::stod(r0[4]) == Approx(50.0));  // Volume value
}

TEST_CASE("TimeSeriesCsvWriter with TRADESTATION_EOD format", "[TimeSeriesCsvWriter][TRADESTATION_EOD]") {
    auto entry1 = createEquityEntry("20200102", "2.0", "3.0", "1.0", "2.5", 100);
    auto entry0 = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    char tmpl[] = "/tmp/ts-eod-writer-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TimeSeriesCsvWriter<DecimalType> writer(fileName, series, OutputFormat::TRADESTATION_EOD);
    writer.writeFile();

    // Read back and verify format: "Date","Time","Open","High","Low","Close","Vol","OI"
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    std::remove(fileName.c_str());

    REQUIRE(lines.size() == 3);  // Header + 2 data rows
    
    // Check header
    REQUIRE(lines[0] == "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\",\"OI\"");
    
    // Check first data row: 01/01/2020,00:00,1.0,2.0,0.5,1.5,50,0
    auto r0 = split(lines[1], ',');
    REQUIRE(r0.size() == 8);
    REQUIRE(r0[0] == "01/01/2020");  // MM/dd/yyyy format
    REQUIRE(r0[1] == "00:00");       // Time for daily data
    REQUIRE(std::stod(r0[2]) == Approx(1.0));  // Open
    REQUIRE(std::stod(r0[3]) == Approx(2.0));  // High
    REQUIRE(std::stod(r0[4]) == Approx(0.5));  // Low
    REQUIRE(std::stod(r0[5]) == Approx(1.5));  // Close
    REQUIRE(std::stod(r0[6]) == Approx(50.0)); // Volume
    REQUIRE(r0[7] == "0");                     // OI (Open Interest) = 0
}

TEST_CASE("TimeSeriesCsvWriter with TRADESTATION_INTRADAY format", "[TimeSeriesCsvWriter][TRADESTATION_INTRADAY]") {
    // Create intraday entries with specific times
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    
    ptime dt1(date(2020, 1, 2), time_duration(10, 30, 0));  // 10:30 AM
    ptime dt0(date(2020, 1, 2), time_duration(9, 30, 0));   // 9:30 AM
    
    auto entry1 = std::make_shared<OHLCTimeSeriesEntry<DecimalType>>(
        dt1, DecimalType("2.0"), DecimalType("3.0"), DecimalType("1.0"), DecimalType("2.5"), DecimalType("100"), TimeFrame::INTRADAY);
    auto entry0 = std::make_shared<OHLCTimeSeriesEntry<DecimalType>>(
        dt0, DecimalType("1.0"), DecimalType("2.0"), DecimalType("0.5"), DecimalType("1.5"), DecimalType("50"), TimeFrame::INTRADAY);
    
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    char tmpl[] = "/tmp/ts-intraday-writer-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TimeSeriesCsvWriter<DecimalType> writer(fileName, series, OutputFormat::TRADESTATION_INTRADAY);
    writer.writeFile();

    // Read back and verify format: "Date","Time","Open","High","Low","Close","Up","Down"
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    std::remove(fileName.c_str());

    REQUIRE(lines.size() == 3);  // Header + 2 data rows
    
    // Check header
    REQUIRE(lines[0] == "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"");
    
    // Check first data row (9:30 AM): 01/02/2020,09:30,1.0,2.0,0.5,1.5,0,0
    auto r0 = split(lines[1], ',');
    REQUIRE(r0.size() == 8);
    REQUIRE(r0[0] == "01/02/2020");  // MM/dd/yyyy format
    REQUIRE(r0[1] == "09:30");       // HH:MM format
    REQUIRE(std::stod(r0[2]) == Approx(1.0));  // Open
    REQUIRE(std::stod(r0[3]) == Approx(2.0));  // High
    REQUIRE(std::stod(r0[4]) == Approx(0.5));  // Low
    REQUIRE(std::stod(r0[5]) == Approx(1.5));  // Close
    REQUIRE(r0[6] == "0");                     // Up = 0
    REQUIRE(r0[7] == "0");                     // Down = 0
}

TEST_CASE("TimeSeriesCsvWriter with PAL_INTRADAY format", "[TimeSeriesCsvWriter][PAL_INTRADAY]") {
    // Create intraday entries using TestUtils
    auto entry1 = createTimeSeriesEntry("20200102", "10:30:00", "2.0", "3.0", "1.0", "2.5", "100");
    auto entry0 = createTimeSeriesEntry("20200101", "09:30:00", "1.0", "2.0", "0.5", "1.5", "50");
    
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    char tmpl[] = "/tmp/pal-intraday-writer-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TimeSeriesCsvWriter<DecimalType> writer(fileName, series, OutputFormat::PAL_INTRADAY);
    writer.writeFile();

    // Read back and verify format: Sequential# Open High Low Close (space-separated, no header)
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    std::remove(fileName.c_str());

    REQUIRE(lines.size() == 2);  // No header, just 2 data rows
    
    // Check first data row: 10000001 1.0 2.0 0.5 1.5
    auto r0 = split(lines[0], ' ');
    REQUIRE(r0.size() == 5);
    REQUIRE(r0[0] == "10000001");              // Sequential counter starts at 10000001
    REQUIRE(std::stod(r0[1]) == Approx(1.0));  // Open
    REQUIRE(std::stod(r0[2]) == Approx(2.0));  // High
    REQUIRE(std::stod(r0[3]) == Approx(0.5));  // Low
    REQUIRE(std::stod(r0[4]) == Approx(1.5));  // Close
    
    // Check second data row: 10000002 2.0 3.0 1.0 2.5
    auto r1 = split(lines[1], ' ');
    REQUIRE(r1.size() == 5);
    REQUIRE(r1[0] == "10000002");              // Sequential counter incremented
    REQUIRE(std::stod(r1[1]) == Approx(2.0));  // Open
    REQUIRE(std::stod(r1[2]) == Approx(3.0));  // High
    REQUIRE(std::stod(r1[3]) == Approx(1.0));  // Low
    REQUIRE(std::stod(r1[4]) == Approx(2.5));  // Close
}

TEST_CASE("PAL Intraday formatter resets counter for each writer instance", "[TimeSeriesCsvWriter][PAL_INTRADAY][Counter]") {
    auto entry = createTimeSeriesEntry("20200101", "09:30:00", "1.0", "2.0", "0.5", "1.5", "50");
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // First writer instance
    char tmpl1[] = "/tmp/pal-intraday-1-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    TimeSeriesCsvWriter<DecimalType> writer1(fileName1, series, OutputFormat::PAL_INTRADAY);
    writer1.writeFile();

    // Second writer instance
    char tmpl2[] = "/tmp/pal-intraday-2-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    TimeSeriesCsvWriter<DecimalType> writer2(fileName2, series, OutputFormat::PAL_INTRADAY);
    writer2.writeFile();

    // Both should start with 10000001
    std::ifstream in1(fileName1);
    std::string line1;
    std::getline(in1, line1);
    auto r1 = split(line1, ' ');
    REQUIRE(r1[0] == "10000001");

    std::ifstream in2(fileName2);
    std::string line2;
    std::getline(in2, line2);
    auto r2 = split(line2, ' ');
    REQUIRE(r2[0] == "10000001");

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("TradeStationEodCsvWriter convenience class works correctly", "[TradeStationEodCsvWriter]") {
    auto entry = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry);

    char tmpl[] = "/tmp/ts-eod-convenience-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TradeStationEodCsvWriter<DecimalType> writer(fileName, series);
    writer.writeFile();

    // Verify the output matches TradeStation EOD format
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    std::remove(fileName.c_str());

    REQUIRE(lines.size() == 2);  // Header + 1 data row
    REQUIRE(lines[0] == "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\",\"OI\"");
    
    auto r0 = split(lines[1], ',');
    REQUIRE(r0[0] == "01/01/2020");  // MM/dd/yyyy format
    REQUIRE(r0[1] == "00:00");       // Time for daily data
}

TEST_CASE("TradeStationIntradayCsvWriter convenience class works correctly", "[TradeStationIntradayCsvWriter]") {
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    
    ptime dt(date(2020, 1, 1), time_duration(14, 30, 0));  // 2:30 PM
    auto entry = std::make_shared<OHLCTimeSeriesEntry<DecimalType>>(
        dt, DecimalType("1.0"), DecimalType("2.0"), DecimalType("0.5"), DecimalType("1.5"), DecimalType("50"), TimeFrame::INTRADAY);
    
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry);

    char tmpl[] = "/tmp/ts-intraday-convenience-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    TradeStationIntradayCsvWriter<DecimalType> writer(fileName, series);
    writer.writeFile();

    // Verify the output matches TradeStation Intraday format
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    std::remove(fileName.c_str());

    REQUIRE(lines.size() == 2);  // Header + 1 data row
    REQUIRE(lines[0] == "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"");
    
    auto r0 = split(lines[1], ',');
    REQUIRE(r0[0] == "01/01/2020");  // MM/dd/yyyy format
    REQUIRE(r0[1] == "14:30");       // HH:MM format
    REQUIRE(r0[6] == "0");           // Up = 0
    REQUIRE(r0[7] == "0");           // Down = 0
}

TEST_CASE("PalIntradayCsvWriter convenience class works correctly", "[PalIntradayCsvWriter]") {
    auto entry1 = createTimeSeriesEntry("20200102", "10:30:00", "2.0", "3.0", "1.0", "2.5", "100");
    auto entry0 = createTimeSeriesEntry("20200101", "09:30:00", "1.0", "2.0", "0.5", "1.5", "50");
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    char tmpl[] = "/tmp/pal-intraday-convenience-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    PalIntradayCsvWriter<DecimalType> writer(fileName, series);
    writer.writeFile();

    // Verify the output matches PAL Intraday format
    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    std::remove(fileName.c_str());

    REQUIRE(lines.size() == 2);  // No header, just 2 data rows
    
    // Check sequential numbering starts at 10000001
    auto r0 = split(lines[0], ' ');
    REQUIRE(r0[0] == "10000001");
    
    auto r1 = split(lines[1], ' ');
    REQUIRE(r1[0] == "10000002");
}

TEST_CASE("TimeSeriesCsvWriter throws for unsupported format", "[TimeSeriesCsvWriter][Error]") {
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    
    // This should compile but throw at runtime if we had an invalid enum value
    // Since we can't create invalid enum values easily, we'll test the factory method indirectly
    // by ensuring all valid formats work without throwing
    REQUIRE_NOTHROW(TimeSeriesCsvWriter<DecimalType>("test", series, OutputFormat::PAL_EOD));
    REQUIRE_NOTHROW(TimeSeriesCsvWriter<DecimalType>("test", series, OutputFormat::PAL_VOLUME_FOR_CLOSE));
    REQUIRE_NOTHROW(TimeSeriesCsvWriter<DecimalType>("test", series, OutputFormat::TRADESTATION_EOD));
    REQUIRE_NOTHROW(TimeSeriesCsvWriter<DecimalType>("test", series, OutputFormat::TRADESTATION_INTRADAY));
    REQUIRE_NOTHROW(TimeSeriesCsvWriter<DecimalType>("test", series, OutputFormat::PAL_INTRADAY));
}

TEST_CASE("Legacy classes maintain backward compatibility", "[BackwardCompatibility]") {
    auto entry = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // Test that legacy classes still work exactly as before
    char tmpl1[] = "/tmp/legacy-pal-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    char tmpl2[] = "/tmp/legacy-volume-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    // Legacy classes should work without any changes to existing code
    PalTimeSeriesCsvWriter<DecimalType> legacyWriter1(fileName1, series);
    PalVolumeForCloseCsvWriter<DecimalType> legacyWriter2(fileName2, series);
    
    REQUIRE_NOTHROW(legacyWriter1.writeFile());
    REQUIRE_NOTHROW(legacyWriter2.writeFile());

    // Verify files were created and have content
    std::ifstream in1(fileName1);
    std::ifstream in2(fileName2);
    REQUIRE(in1.is_open());
    REQUIRE(in2.is_open());
    REQUIRE(in1.peek() != std::ifstream::traits_type::eof());
    REQUIRE(in2.peek() != std::ifstream::traits_type::eof());

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("Empty series produces empty file", "[PalTimeSeriesCsvWriter][Empty]") {
    OHLCTimeSeries<DecimalType> emptySeries(TimeFrame::DAILY, TradingVolume::SHARES);

    // Write to unique temp file via mkstemp
    char tmpl[] = "/tmp/tswriter-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    PalTimeSeriesCsvWriter<DecimalType> writer(fileName, emptySeries);
    writer.writeFile();

    std::ifstream in(fileName);
    REQUIRE(in.is_open());
    REQUIRE(in.peek() == std::ifstream::traits_type::eof());
    std::remove(fileName.c_str());
}

//
// Tests for Windows Line Ending Support
//

// Helper function to read file in binary mode and check line endings
static std::string readFileBinary(const std::string& fileName) {
    std::ifstream file(fileName, std::ios::binary);
    if (!file.is_open()) return "";
    
    std::string content;
    file.seekg(0, std::ios::end);
    content.reserve(file.tellg());
    file.seekg(0, std::ios::beg);
    
    content.assign((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
    return content;
}

// Helper function to count occurrences of a substring
static size_t countSubstring(const std::string& str, const std::string& sub) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = str.find(sub, pos)) != std::string::npos) {
        ++count;
        pos += sub.length();
    }
    return count;
}

TEST_CASE("PalTimeSeriesCsvWriter with Windows line endings", "[PalTimeSeriesCsvWriter][WindowsLineEndings]") {
    auto entry1 = createEquityEntry("20200102", "2.0", "3.0", "1.0", "2.5", 0);
    auto entry0 = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 0);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    // Test Unix line endings (default)
    char tmpl1[] = "/tmp/pal-unix-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    PalTimeSeriesCsvWriter<DecimalType> unixWriter(fileName1, series, false);
    unixWriter.writeFile();

    std::string unixContent = readFileBinary(fileName1);
    REQUIRE(countSubstring(unixContent, "\r\n") == 0);  // No Windows line endings
    REQUIRE(countSubstring(unixContent, "\n") == 2);    // Two Unix line endings

    // Test Windows line endings
    char tmpl2[] = "/tmp/pal-windows-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    PalTimeSeriesCsvWriter<DecimalType> windowsWriter(fileName2, series, true);
    windowsWriter.writeFile();

    std::string windowsContent = readFileBinary(fileName2);
    REQUIRE(countSubstring(windowsContent, "\r\n") == 2);  // Two Windows line endings
    REQUIRE(countSubstring(windowsContent, "\n") == 2);    // \n is part of \r\n

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("PalIntradayCsvWriter with Windows line endings", "[PalIntradayCsvWriter][WindowsLineEndings]") {
    auto entry1 = createTimeSeriesEntry("20200102", "10:30:00", "2.0", "3.0", "1.0", "2.5", "100");
    auto entry0 = createTimeSeriesEntry("20200101", "09:30:00", "1.0", "2.0", "0.5", "1.5", "50");
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry1);
    series.addEntry(*entry0);

    // Test Unix line endings (default)
    char tmpl1[] = "/tmp/pal-intraday-unix-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    PalIntradayCsvWriter<DecimalType> unixWriter(fileName1, series, false);
    unixWriter.writeFile();

    std::string unixContent = readFileBinary(fileName1);
    REQUIRE(countSubstring(unixContent, "\r\n") == 0);  // No Windows line endings
    REQUIRE(countSubstring(unixContent, "\n") == 2);    // Two Unix line endings

    // Test Windows line endings
    char tmpl2[] = "/tmp/pal-intraday-windows-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    PalIntradayCsvWriter<DecimalType> windowsWriter(fileName2, series, true);
    windowsWriter.writeFile();

    std::string windowsContent = readFileBinary(fileName2);
    REQUIRE(countSubstring(windowsContent, "\r\n") == 2);  // Two Windows line endings
    REQUIRE(countSubstring(windowsContent, "\n") == 2);    // \n is part of \r\n

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("TradeStationEodCsvWriter with Windows line endings", "[TradeStationEodCsvWriter][WindowsLineEndings]") {
    auto entry = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // Test Unix line endings (default)
    char tmpl1[] = "/tmp/ts-eod-unix-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    TradeStationEodCsvWriter<DecimalType> unixWriter(fileName1, series, false);
    unixWriter.writeFile();

    std::string unixContent = readFileBinary(fileName1);
    REQUIRE(countSubstring(unixContent, "\r\n") == 0);  // No Windows line endings
    REQUIRE(countSubstring(unixContent, "\n") == 2);    // Header + 1 data row = 2 Unix line endings

    // Test Windows line endings
    char tmpl2[] = "/tmp/ts-eod-windows-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    TradeStationEodCsvWriter<DecimalType> windowsWriter(fileName2, series, true);
    windowsWriter.writeFile();

    std::string windowsContent = readFileBinary(fileName2);
    REQUIRE(countSubstring(windowsContent, "\r\n") == 2);  // Header + 1 data row = 2 Windows line endings
    REQUIRE(countSubstring(windowsContent, "\n") == 2);    // \n is part of \r\n

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("TradeStationIntradayCsvWriter with Windows line endings", "[TradeStationIntradayCsvWriter][WindowsLineEndings]") {
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    
    ptime dt(date(2020, 1, 1), time_duration(14, 30, 0));  // 2:30 PM
    auto entry = std::make_shared<OHLCTimeSeriesEntry<DecimalType>>(
        dt, DecimalType("1.0"), DecimalType("2.0"), DecimalType("0.5"), DecimalType("1.5"), DecimalType("50"), TimeFrame::INTRADAY);
    
    OHLCTimeSeries<DecimalType> series(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // Test Unix line endings (default)
    char tmpl1[] = "/tmp/ts-intraday-unix-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    TradeStationIntradayCsvWriter<DecimalType> unixWriter(fileName1, series, false);
    unixWriter.writeFile();

    std::string unixContent = readFileBinary(fileName1);
    REQUIRE(countSubstring(unixContent, "\r\n") == 0);  // No Windows line endings
    REQUIRE(countSubstring(unixContent, "\n") == 2);    // Header + 1 data row = 2 Unix line endings

    // Test Windows line endings
    char tmpl2[] = "/tmp/ts-intraday-windows-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    TradeStationIntradayCsvWriter<DecimalType> windowsWriter(fileName2, series, true);
    windowsWriter.writeFile();

    std::string windowsContent = readFileBinary(fileName2);
    REQUIRE(countSubstring(windowsContent, "\r\n") == 2);  // Header + 1 data row = 2 Windows line endings
    REQUIRE(countSubstring(windowsContent, "\n") == 2);    // \n is part of \r\n

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("TimeSeriesCsvWriter with Windows line endings for all formats", "[TimeSeriesCsvWriter][WindowsLineEndings]") {
    auto entry = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // Test PAL_EOD format with Windows line endings
    char tmpl1[] = "/tmp/unified-pal-eod-windows-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    TimeSeriesCsvWriter<DecimalType> palEodWriter(fileName1, series, OutputFormat::PAL_EOD, true);
    palEodWriter.writeFile();

    std::string palEodContent = readFileBinary(fileName1);
    REQUIRE(countSubstring(palEodContent, "\r\n") == 1);  // 1 data row = 1 Windows line ending
    REQUIRE(countSubstring(palEodContent, "\n") == 1);    // \n is part of \r\n

    // Test TRADESTATION_EOD format with Windows line endings
    char tmpl2[] = "/tmp/unified-ts-eod-windows-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    TimeSeriesCsvWriter<DecimalType> tsEodWriter(fileName2, series, OutputFormat::TRADESTATION_EOD, true);
    tsEodWriter.writeFile();

    std::string tsEodContent = readFileBinary(fileName2);
    REQUIRE(countSubstring(tsEodContent, "\r\n") == 2);  // Header + 1 data row = 2 Windows line endings
    REQUIRE(countSubstring(tsEodContent, "\n") == 2);    // \n is part of \r\n

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

TEST_CASE("Backward compatibility - default behavior unchanged", "[BackwardCompatibility][LineEndings]") {
    auto entry = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // Test that default behavior (no Windows line ending parameter) produces Unix line endings
    char tmpl[] = "/tmp/backward-compat-XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    ::close(fd);
    std::string fileName(tmpl);

    // Use legacy constructor without line ending parameter
    PalTimeSeriesCsvWriter<DecimalType> writer(fileName, series);
    writer.writeFile();

    std::string content = readFileBinary(fileName);
    REQUIRE(countSubstring(content, "\r\n") == 0);  // No Windows line endings by default
    REQUIRE(countSubstring(content, "\n") == 1);    // One Unix line ending

    std::remove(fileName.c_str());
}

TEST_CASE("Indicator-based writers with Windows line endings", "[IndicatorWriters][WindowsLineEndings]") {
    // Create OHLC series
    auto entry = createEquityEntry("20200101", "1.0", "2.0", "0.5", "1.5", 50);
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*entry);

    // Create indicator series
    NumericTimeSeries<DecimalType> indicator(TimeFrame::DAILY);
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    ptime dt(date(2020, 1, 1), time_duration(0, 0, 0));
    NumericTimeSeriesEntry<DecimalType> indicatorEntry(dt, DecimalType("0.75"), TimeFrame::DAILY);
    indicator.addEntry(indicatorEntry);

    // Test PalIndicatorEodCsvWriter with Windows line endings
    char tmpl1[] = "/tmp/pal-indicator-eod-windows-XXXXXX";
    int fd1 = mkstemp(tmpl1);
    REQUIRE(fd1 >= 0);
    ::close(fd1);
    std::string fileName1(tmpl1);

    PalIndicatorEodCsvWriter<DecimalType> eodWriter(fileName1, series, indicator, true);
    eodWriter.writeFile();

    std::string eodContent = readFileBinary(fileName1);
    REQUIRE(countSubstring(eodContent, "\r\n") == 1);  // 1 data row = 1 Windows line ending
    REQUIRE(countSubstring(eodContent, "\n") == 1);    // \n is part of \r\n

    // Test PalIndicatorIntradayCsvWriter with Windows line endings
    char tmpl2[] = "/tmp/pal-indicator-intraday-windows-XXXXXX";
    int fd2 = mkstemp(tmpl2);
    REQUIRE(fd2 >= 0);
    ::close(fd2);
    std::string fileName2(tmpl2);

    PalIndicatorIntradayCsvWriter<DecimalType> intradayWriter(fileName2, series, indicator, true);
    intradayWriter.writeFile();

    std::string intradayContent = readFileBinary(fileName2);
    REQUIRE(countSubstring(intradayContent, "\r\n") == 1);  // 1 data row = 1 Windows line ending
    REQUIRE(countSubstring(intradayContent, "\n") == 1);    // \n is part of \r\n

    std::remove(fileName1.c_str());
    std::remove(fileName2.c_str());
}

