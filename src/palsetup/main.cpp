#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>        // for ofstream
#include <filesystem>     // for directory operations
#include <limits>         // for numeric_limits
#include <cctype>         // for std::toupper
#include <boost/date_time/gregorian/gregorian.hpp>  // for to_iso_string

#include "TimeSeriesCsvReader.h"
#include "TimeSeriesCsvWriter.h"
#include "TimeSeriesIndicators.h"
#include "DecimalConstants.h"
#include "TimeFrameUtility.h"

using namespace mkc_timeseries;
using std::shared_ptr;
using namespace boost::gregorian;
namespace fs = std::filesystem;

using Num = num::DefaultNumber;

// Writes a CSV configuration file for permutation testing into the given output directory
void writeConfigFile(const std::string& outputDir,
                     const std::string& tickerSymbol,
                     const OHLCTimeSeries<Num>& insampleSeries,
                     const OHLCTimeSeries<Num>& outOfSampleSeries,
                     const std::string& timeFrame)
{
    std::string configFileName = outputDir + "/" + tickerSymbol + "_config.csv";
    std::ofstream configFile(configFileName);
    if (!configFile.is_open()) {
        std::cerr << "Error: Unable to open config file " << configFileName << std::endl;
        return;
    }

    std::string irPath     = "./" + tickerSymbol + "_IR.txt";
    std::string dataPath   = "./" + tickerSymbol + "_Data.txt";
    std::string fileFormat = "PAL";

    // Dates in YYYYMMDD format
    std::string isDateStart  = to_iso_string(insampleSeries.getFirstDate());
    std::string isDateEnd    = to_iso_string(insampleSeries.getLastDate());
    std::string oosDateStart = to_iso_string(outOfSampleSeries.getFirstDate());
    std::string oosDateEnd   = to_iso_string(outOfSampleSeries.getLastDate());

    // Write CSV line: Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame
    configFile << tickerSymbol << ","
               << irPath       << ","
               << dataPath     << ","
               << fileFormat   << ","
               << isDateStart  << ","
               << isDateEnd    << ","
               << oosDateStart << ","
               << oosDateEnd   << ","
               << timeFrame    << std::endl;

    configFile.close();
    std::cout << "Configuration file written: " << configFileName << std::endl;
}

// Factory for CSV readers using specified time frame
shared_ptr< TimeSeriesCsvReader<Num> >
createTimeSeriesReader(int fileType,
                       const std::string& fileName,
                       const Num& tick,
                       TimeFrame::Duration timeFrame)
{
    switch (fileType) {
    case 1:
        return std::make_shared<CSIErrorCheckingFuturesCsvReader<Num>>(fileName,
                                                                        timeFrame,
                                                                        TradingVolume::SHARES,
                                                                        tick);
    case 2:
        return std::make_shared<CSIErrorCheckingExtendedFuturesCsvReader<Num>>(fileName,
                                                                               timeFrame,
                                                                               TradingVolume::SHARES,
                                                                               tick);
    case 3:
        return std::make_shared<TradeStationErrorCheckingFormatCsvReader<Num>>(fileName,
                                                                                timeFrame,
                                                                                TradingVolume::SHARES,
                                                                                tick);
    case 4:
        return std::make_shared<PinnacleErrorCheckingFormatCsvReader<Num>>(fileName,
                                                                            timeFrame,
                                                                            TradingVolume::SHARES,
                                                                            tick);
    case 5:
        return std::make_shared<PALFormatCsvReader<Num>>(fileName,
                                                           timeFrame,
                                                           TradingVolume::SHARES,
                                                           tick);
    default:
        throw std::out_of_range("Invalid file type");
    }
}

int main(int argc, char** argv) {
    if ((argc == 3) || (argc == 4)) {
        // Command-line args: datafile, file-type, [securityTick]
        std::string historicDataFileName = argv[1];
        int fileType = std::stoi(argv[2]);
        Num securityTick(DecimalConstants<Num>::EquityTick);
        if (argc == 4)
            securityTick = Num(std::stof(argv[3]));

        // 1. Read ticker symbol
        std::string tickerSymbol;
        std::cout << "Enter ticker symbol: ";
        std::cin >> tickerSymbol;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // 2. Read and parse time frame (default Daily)
        std::string timeFrameStr;
        TimeFrame::Duration timeFrame;
        bool validFrame = false;
        while (!validFrame) {
            std::cout << "Enter time frame ([D]aily, [W]eekly, [M]onthly, [I]ntraday) [default D]: ";
            std::string tfInput;
            std::getline(std::cin, tfInput);
            if (tfInput.empty()) tfInput = "D";
            char c = std::toupper(tfInput[0]);
            switch (c) {
                case 'D': timeFrameStr = "Daily"; validFrame = true; break;
                case 'W': timeFrameStr = "Weekly"; validFrame = true; break;
                case 'M': timeFrameStr = "Monthly"; validFrame = true; break;
                case 'I': timeFrameStr = "Intraday"; validFrame = true; break;
                default:
                    std::cerr << "Invalid time frame. Please enter D, W, M, or I." << std::endl;
            }
        }
        timeFrame = getTimeFrameFromString(timeFrameStr);

        // 3. Read and parse reserve percentage (default 5%)
        double reservedPercent = 5.0;
        std::cout << "Enter percent of data to reserve (0-100, default 5): ";
        std::string reservedPercentStr;
        std::getline(std::cin, reservedPercentStr);
        if (!reservedPercentStr.empty()) {
            try {
                reservedPercent = std::stod(reservedPercentStr);
            } catch (...) {
                std::cerr << "Invalid input for reserved percent. Using default 5%." << std::endl;
                reservedPercent = 5.0;
            }
        }
        reservedPercent = std::clamp(reservedPercent, 0.0, 100.0);

        // 4. Prepare output directories
        fs::path baseDir = tickerSymbol + "_Validation";
        if (fs::exists(baseDir))
            fs::remove_all(baseDir);
        fs::path palDir = baseDir / "PAL_Files";
        fs::path valDir = baseDir / "Validation_Files";
        fs::create_directories(palDir);
        fs::create_directories(valDir);

        // 5. Create and read time series
        shared_ptr<TimeSeriesCsvReader<Num>> reader;
        if (fileType >= 1 && fileType <= 5) {
            reader = createTimeSeriesReader(fileType,
                                            historicDataFileName,
                                            securityTick,
                                            timeFrame);
        } else {
            throw std::out_of_range("Invalid file type");
        }
        reader->readFile();
        auto aTimeSeries = reader->getTimeSeries();

        // 6. Split into insample, out-of-sample, and reserved (last)
        size_t totalSize    = aTimeSeries->getNumEntries();
        size_t reservedSize = static_cast<size_t>(totalSize * (reservedPercent / 100.0));
        size_t remaining    = totalSize - reservedSize;
        size_t insampleSize = static_cast<size_t>(remaining * 0.8);
        size_t oosSize      = remaining - insampleSize;

        OHLCTimeSeries<Num> reservedSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());
        OHLCTimeSeries<Num> insampleSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());
        OHLCTimeSeries<Num> outOfSampleSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());

        size_t count = 0;
        for (auto it = aTimeSeries->beginSortedAccess(); it != aTimeSeries->endSortedAccess(); ++it, ++count) {
            const auto& entry = *it;
            if (count < insampleSize)
                insampleSeries.addEntry(entry);
            else if (count < insampleSize + oosSize)
                outOfSampleSeries.addEntry(entry);
            else
                reservedSeries.addEntry(entry);
        }

        // 7. Insample stop calculation
        NumericTimeSeries<Num> closingPrices(insampleSeries.CloseTimeSeries());
        NumericTimeSeries<Num> rocOfClosingPrices(RocSeries(closingPrices, 1));
        Num medianOfRoc(Median(rocOfClosingPrices));
        Num robustQn = RobustQn<Num>(rocOfClosingPrices).getRobustQn();
        Num MAD(MedianAbsoluteDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector()));
        Num StdDev(StandardDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector()));
        Num stopValue = medianOfRoc + robustQn;

        // 8. Generate target/stop files
        std::ofstream tsFile1((palDir / (tickerSymbol + "_0_5_.TRS")).string());
        tsFile1 << (stopValue * Num(0.5)) << std::endl << stopValue << std::endl;
        tsFile1.close();

        std::ofstream tsFile2((palDir / (tickerSymbol + "_1_0_.TRS")).string());
        tsFile2 << stopValue << std::endl << stopValue << std::endl;
        tsFile2.close();

        // 9. Write data files
        PalTimeSeriesCsvWriter<Num> insampleWriter((palDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
        insampleWriter.writeFile();
        PalTimeSeriesCsvWriter<Num> allWriter((valDir / (tickerSymbol + "_ALL.txt")).string(), *aTimeSeries);
        allWriter.writeFile();
        PalTimeSeriesCsvWriter<Num> oosWriter((valDir / (tickerSymbol + "_OOS.txt")).string(), outOfSampleSeries);
        oosWriter.writeFile();
        if (reservedSize > 0) {
            PalTimeSeriesCsvWriter<Num> reservedWriter((valDir / (tickerSymbol + "_reserved.txt")).string(), reservedSeries);
            reservedWriter.writeFile();
        }

        // 10. Output statistics
        std::cout << "Reserved% = " << reservedPercent << "%\n";
        std::cout << "Median = " << medianOfRoc << std::endl;
        std::cout << "Qn  = " << robustQn << std::endl;
        std::cout << "MAD = " << MAD << std::endl;
        std::cout << "Std = " << StdDev << std::endl;
        std::cout << "Stop = " << stopValue << std::endl;

        // 11. Write configuration file
        writeConfigFile(valDir.string(), tickerSymbol, insampleSeries, outOfSampleSeries, timeFrameStr);

    } else {
        std::cout << "Usage (beta):: PalSetup datafile file-type (1=CSI,2=CSI Ext,3=TradeStation,4=Pinnacle,5=PAL) [tick]" << std::endl;
    }
    return 0;
}
