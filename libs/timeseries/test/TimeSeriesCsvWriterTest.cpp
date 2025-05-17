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
