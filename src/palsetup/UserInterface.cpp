#include "UserInterface.h"
#include "TimeFrameUtility.h"
#include "DecimalConstants.h"
#include "TimeSeriesIndicators.h"
#include "TimeSeriesProcessor.h"
#include <iostream>
#include <filesystem>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace fs = std::filesystem;

UserInterface::UserInterface() : indicatorMode_(false), statsOnlyMode_(false) {}

SetupConfiguration UserInterface::parseCommandLineArgs(int argc, char** argv) {
    // Parse command line arguments for flags
    positionalArgs_.clear();
    indicatorMode_ = false;
    statsOnlyMode_ = false;
    
    // Separate flags from positional arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = std::string(argv[i]);
        if (arg == "-indicator" || arg == "--indicator") {
            indicatorMode_ = true;
        } else if (arg == "-stats-only" || arg == "--stats-only") {
            statsOnlyMode_ = true;
        } else {
            positionalArgs_.push_back(arg);
        }
    }
    
    if (positionalArgs_.size() != 2) {
        displayUsage();
        throw std::invalid_argument("Invalid number of command line arguments");
    }
    
    // Extract basic parameters from command line
    std::string historicDataFileName = positionalArgs_[0];
    int fileType = std::stoi(positionalArgs_[1]);
    Num securityTick(mkc_timeseries::DecimalConstants<Num>::EquityTick);
    
    // Extract default ticker symbol from filename
    std::string defaultTicker = extractDefaultTicker(historicDataFileName);
    
    // Display data file date range before asking for user input
    try {
        TimeSeriesProcessor tsProcessor;
        auto reader = tsProcessor.createTimeSeriesReader(
            fileType,
            historicDataFileName,
            securityTick,
            mkc_timeseries::TimeFrame::DAILY); // Use default timeframe for preview
        auto timeSeries = tsProcessor.loadTimeSeries(reader);
        
        if (timeSeries->getNumEntries() > 0) {
            auto firstDate = timeSeries->getFirstDate();
            auto lastDate = timeSeries->getLastDate();
            std::cout << "[Data Range] " << historicDataFileName
                      << " contains " << timeSeries->getNumEntries() << " entries"
                      << " from " << boost::gregorian::to_iso_extended_string(firstDate)
                      << " to " << boost::gregorian::to_iso_extended_string(lastDate) << std::endl;
        } else {
            std::cout << "[Data Range] " << historicDataFileName
                      << " contains no data entries" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[Data Range] Could not read " << historicDataFileName
                  << " - " << e.what() << std::endl;
    }
    
    // Collect user input interactively
    std::string tickerSymbol = getTickerSymbol(defaultTicker);
    auto [timeFrameStr, timeFrame] = getTimeFrameInput();
    
    int intradayMinutes = 0;
    if (timeFrameStr == "Intraday") {
        intradayMinutes = getIntradayMinutes();
    }
    
    auto [indicatorModeSelected, selectedIndicator] = getIndicatorSelection();
    auto [insamplePercent, outOfSamplePercent, reservedPercent] = getDataSplitInput();
    int holdingPeriod = getHoldingPeriodInput();
    
    return SetupConfiguration(
        tickerSymbol,
        timeFrameStr,
        timeFrame,
        fileType,
        historicDataFileName,
        securityTick,
        intradayMinutes,
        indicatorModeSelected,
        selectedIndicator,
        insamplePercent,
        outOfSamplePercent,
        reservedPercent,
        holdingPeriod,
        statsOnlyMode_
    );
}

void UserInterface::displayResults(const StatisticsResults& stats, const CleanStartResult& cleanStart) {
    std::cout << "Median = " << stats.getMedianOfRoc() << std::endl;
    std::cout << "Qn  = " << stats.getRobustQn() << std::endl;
    std::cout << "MAD = " << stats.getMAD() << std::endl;
    std::cout << "Std = " << stats.getStdDev() << std::endl;
    std::cout << "Profit Target = " << stats.getProfitTargetValue() << std::endl;
    std::cout << "Stop = " << stats.getStopValue() << std::endl;
    std::cout << "Skew = " << stats.getSkew() << std::endl;
}

void UserInterface::displaySetupSummary(const SetupConfiguration& config) {
    std::cout << "\n=== Setup Configuration ===" << std::endl;
    std::cout << "Ticker: " << config.getTickerSymbol() << std::endl;
    std::cout << "Time Frame: " << config.getTimeFrameStr();
    if (config.getTimeFrameStr() == "Intraday") {
        std::cout << " (" << config.getIntradayMinutes() << " minutes)";
    }
    std::cout << std::endl;
    std::cout << "File Type: " << config.getFileType() << std::endl;
    std::cout << "Indicator Mode: " << (config.isIndicatorMode() ? "Yes (" + config.getSelectedIndicator() + ")" : "No") << std::endl;
    std::cout << "Data Split: " << config.getInsamplePercent() << "% / "
              << config.getOutOfSamplePercent() << "% / "
              << config.getReservedPercent() << "%" << std::endl;
    std::cout << "Holding Period: " << config.getHoldingPeriod() << std::endl;
    std::cout << "=========================" << std::endl;
}

void UserInterface::displaySetupSummary(const SetupConfiguration& config,
                                       const mkc_timeseries::OHLCTimeSeries<Num>& timeSeries) {
    std::cout << "\n=== Setup Configuration ===" << std::endl;
    std::cout << "Ticker: " << config.getTickerSymbol() << std::endl;
    std::cout << "Time Frame: " << config.getTimeFrameStr();
    if (config.getTimeFrameStr() == "Intraday") {
        std::cout << " (" << config.getIntradayMinutes() << " minutes)";
    }
    std::cout << std::endl;
    std::cout << "File Type: " << config.getFileType() << std::endl;
    std::cout << "Indicator Mode: " << (config.isIndicatorMode() ? "Yes (" + config.getSelectedIndicator() + ")" : "No") << std::endl;
    
    // Calculate date ranges for data splits
    if (timeSeries.getNumEntries() > 0) {
        auto entries = timeSeries.getEntriesCopy();
        size_t totalEntries = entries.size();
        
        // Calculate split sizes (same logic as in TimeSeriesProcessor::splitTimeSeries)
        size_t inSampleSize = static_cast<size_t>((config.getInsamplePercent() / 100.0) * totalEntries);
        size_t outOfSampleSize = static_cast<size_t>((config.getOutOfSamplePercent() / 100.0) * totalEntries);
        size_t reservedSize = static_cast<size_t>((config.getReservedPercent() / 100.0) * totalEntries);
        
        // Ensure we don't exceed total entries
        if (inSampleSize + outOfSampleSize + reservedSize > totalEntries) {
            size_t excess = (inSampleSize + outOfSampleSize + reservedSize) - totalEntries;
            if (reservedSize >= excess) {
                reservedSize -= excess;
            } else if (outOfSampleSize >= excess) {
                outOfSampleSize -= excess;
            } else {
                inSampleSize -= excess;
            }
        }
        
        std::cout << "Data Split: " << config.getInsamplePercent() << "% / "
                  << config.getOutOfSamplePercent() << "% / "
                  << config.getReservedPercent() << "%" << std::endl;
        
        // Display date ranges for each split
        if (inSampleSize > 0) {
            auto inSampleStart = entries[0].getDateTime().date();
            auto inSampleEnd = entries[inSampleSize - 1].getDateTime().date();
            std::cout << "  In-Sample:     " << boost::gregorian::to_iso_extended_string(inSampleStart)
                      << " to " << boost::gregorian::to_iso_extended_string(inSampleEnd)
                      << " (" << inSampleSize << " entries)" << std::endl;
        }
        
        if (outOfSampleSize > 0) {
            auto outOfSampleStart = entries[inSampleSize].getDateTime().date();
            auto outOfSampleEnd = entries[inSampleSize + outOfSampleSize - 1].getDateTime().date();
            std::cout << "  Out-of-Sample: " << boost::gregorian::to_iso_extended_string(outOfSampleStart)
                      << " to " << boost::gregorian::to_iso_extended_string(outOfSampleEnd)
                      << " (" << outOfSampleSize << " entries)" << std::endl;
        }
        
        if (reservedSize > 0) {
            auto reservedStart = entries[inSampleSize + outOfSampleSize].getDateTime().date();
            auto reservedEnd = entries[inSampleSize + outOfSampleSize + reservedSize - 1].getDateTime().date();
            std::cout << "  Reserved:      " << boost::gregorian::to_iso_extended_string(reservedStart)
                      << " to " << boost::gregorian::to_iso_extended_string(reservedEnd)
                      << " (" << reservedSize << " entries)" << std::endl;
        }
    } else {
        std::cout << "Data Split: " << config.getInsamplePercent() << "% / "
                  << config.getOutOfSamplePercent() << "% / "
                  << config.getReservedPercent() << "% (no data available)" << std::endl;
    }
    
    std::cout << "Holding Period: " << config.getHoldingPeriod() << std::endl;
    std::cout << "=========================" << std::endl;
}

std::string UserInterface::extractDefaultTicker(const std::string& filename) {
    fs::path filePath(filename);
    std::string baseName = filePath.stem().string(); // Gets filename without extension
    
    // Extract only alphabetic characters from the beginning until first non-alphabetic character
    std::string ticker;
    for (char c : baseName) {
        if (std::isalpha(c)) {
            ticker += c;
        } else {
            // Stop at first non-alphabetic character
            break;
        }
    }
    
    // If we found alphabetic characters, return them; otherwise return the whole base name
    return ticker.empty() ? baseName : ticker;
}

std::string UserInterface::getTickerSymbol(const std::string& defaultTicker) {
    std::string tickerSymbol;
    std::cout << "Enter ticker symbol [default " << defaultTicker << "]: ";
    std::getline(std::cin, tickerSymbol);
    if (tickerSymbol.empty()) {
        tickerSymbol = defaultTicker;
    }
    return tickerSymbol;
}

std::pair<std::string, mkc_timeseries::TimeFrame::Duration> UserInterface::getTimeFrameInput() {
    std::string timeFrameStr;
    mkc_timeseries::TimeFrame::Duration timeFrame;
    bool validFrame = false;
    
    while (!validFrame) {
        std::cout << "Enter time frame ([D]aily, [W]eekly, [M]onthly, [I]ntraday) [default D]: ";
        std::string tfInput;
        std::getline(std::cin, tfInput);
        if (tfInput.empty()) tfInput = "D";
        
        char c = std::toupper(tfInput[0]);
        switch (c) {
            case 'D':
                timeFrameStr = "Daily";
                validFrame = true;
                break;
            case 'W':
                timeFrameStr = "Weekly";
                validFrame = true;
                break;
            case 'M':
                timeFrameStr = "Monthly";
                validFrame = true;
                break;
            case 'I':
                timeFrameStr = "Intraday";
                validFrame = true;
                break;
            default:
                std::cerr << "Invalid time frame. Please enter D, W, M, or I." << std::endl;
        }
    }
    
    timeFrame = mkc_timeseries::getTimeFrameFromString(timeFrameStr);
    return {timeFrameStr, timeFrame};
}

int UserInterface::getIntradayMinutes() {
    std::cout << "Enter number of minutes for intraday timeframe (1-1440, default 90): ";
    std::string minutesInput;
    std::getline(std::cin, minutesInput);
    
    int intradayMinutes = 90; // default
    if (!minutesInput.empty()) {
        try {
            intradayMinutes = std::stoi(minutesInput);
            intradayMinutes = std::clamp(intradayMinutes, 1, 1440);
        } catch (...) {
            std::cerr << "Invalid input for minutes. Using default 90." << std::endl;
            intradayMinutes = 90;
        }
    }
    return intradayMinutes;
}

std::pair<bool, std::string> UserInterface::getIndicatorSelection() {
    if (!indicatorMode_) {
        return {false, ""};
    }
    
    std::cout << "Select indicator ([I]BS - Internal Bar Strength): ";
    std::string indicatorChoice;
    std::getline(std::cin, indicatorChoice);
    if (indicatorChoice.empty()) indicatorChoice = "I";
    
    char c = std::toupper(indicatorChoice[0]);
    switch (c) {
        case 'I':
            std::cout << "Selected: Internal Bar Strength (IBS)" << std::endl;
            return {true, "IBS"};
        default:
            std::cerr << "Invalid indicator selection. Defaulting to IBS." << std::endl;
            return {true, "IBS"};
    }
}

std::tuple<double, double, double> UserInterface::getDataSplitInput() {
    double insamplePercent, outOfSamplePercent, reservedPercent;
    bool validPercentages = false;
    
    while (!validPercentages) {
        // Get in-sample percentage (default 60%)
        insamplePercent = getValidatedDoubleInput(
            "Enter percent of data for in-sample (0-100, default 60%): ", 
            60.0, 0.0, 100.0);
        
        // Get out-of-sample percentage (default 40%)
        outOfSamplePercent = getValidatedDoubleInput(
            "Enter percent of data for out-of-sample (0-100, default 40%): ", 
            40.0, 0.0, 100.0);
        
        // Get reserved percentage (default 0%)
        reservedPercent = getValidatedDoubleInput(
            "Enter percent of data to reserve (0-100, default 0%): ", 
            0.0, 0.0, 100.0);
        
        // Validate that total doesn't exceed 100%
        if (validatePercentages(insamplePercent, outOfSamplePercent, reservedPercent)) {
            validPercentages = true;
        } else {
            double totalPercent = insamplePercent + outOfSamplePercent + reservedPercent;
            std::cerr << "Error: Total percentage (" << totalPercent
                      << "%) exceeds 100%. Please enter the percentages again." << std::endl;
        }
    }
    
    return {insamplePercent, outOfSamplePercent, reservedPercent};
}

int UserInterface::getHoldingPeriodInput() {
    return getValidatedIntInput(
        "Enter holding period (integer, default 1): ", 
        1, 1, std::numeric_limits<int>::max());
}

std::string UserInterface::getValidatedStringInput(const std::string& prompt, const std::string& defaultValue) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    if (input.empty() && !defaultValue.empty()) {
        return defaultValue;
    }
    return input;
}

int UserInterface::getValidatedIntInput(const std::string& prompt, int defaultValue, int minValue, int maxValue) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return defaultValue;
    }
    
    try {
        int value = std::stoi(input);
        return std::clamp(value, minValue, maxValue);
    } catch (...) {
        std::cerr << "Invalid input. Using default " << defaultValue << "." << std::endl;
        return defaultValue;
    }
}

double UserInterface::getValidatedDoubleInput(const std::string& prompt, double defaultValue, double minValue, double maxValue) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return defaultValue;
    }
    
    try {
        double value = std::stod(input);
        return std::clamp(value, minValue, maxValue);
    } catch (...) {
        std::cerr << "Invalid input. Using default " << defaultValue << "." << std::endl;
        return defaultValue;
    }
}

bool UserInterface::validatePercentages(double inSample, double outOfSample, double reserved) {
    double total = inSample + outOfSample + reserved;
    return total <= 100.0 && inSample >= 0.0 && outOfSample >= 0.0 && reserved >= 0.0;
}

void UserInterface::displayUsage() {
    std::cout << "Usage: PalSetup [options] datafile file-type" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -indicator|--indicator: Use indicator values (e.g., IBS) instead of close prices" << std::endl;
    std::cout << "  -stats-only|--stats-only: Print statistics only, do not write files" << std::endl;
    std::cout << "File types: 1=CSI, 2=CSI Ext, 3=TradeStation, 4=Pinnacle, 5=PAL, 6=WealthLab" << std::endl;
}

void UserInterface::displayCleanStartInfo(const CleanStartResult& cleanStart,
                                         const std::string& tickerSymbol,
                                         const mkc_timeseries::OHLCTimeSeries<Num>& series,
                                         std::optional<double> knownTick) {
    if (cleanStart.isFound() && cleanStart.getStartIndex() > 0) {
        auto chosenDate = series.getEntriesCopy()[cleanStart.getStartIndex()].getDateTime().date();
        std::cout << "[Quantization-aware trim] Start index " << cleanStart.getStartIndex()
                  << " (" << boost::gregorian::to_iso_extended_string(chosenDate) << ")"
                  << "  tick≈" << cleanStart.getTick()
                  << "  relTick≈" << cleanStart.getRelTick()
                  << "  zeroFrac≈" << cleanStart.getZeroFrac() << std::endl;
        if (knownTick) {
            std::cout << "[Tick] from SecurityAttributes/CLI: " << *knownTick << std::endl;
        } else {
            std::cout << "[Tick] inferred from data: " << cleanStart.getTick() << std::endl;
        }
    } else {
        std::ostringstream oss;
        oss << "No clean start window found for symbol '" << tickerSymbol << "'. "
            << "Bars=" << series.getNumEntries();
        throw std::runtime_error(oss.str());
    }
}

void UserInterface::displayStatisticsOnly(const mkc_timeseries::OHLCTimeSeries<Num>& inSampleSeries,
                                         const SetupConfiguration& config) {
    using namespace mkc_timeseries;
    
    const uint32_t period = static_cast<uint32_t>(config.getHoldingPeriod());
    
    std::cout << "\n=== Statistics-Only Analysis ===" << std::endl;
    std::cout << "Ticker: " << config.getTickerSymbol() << std::endl;
    std::cout << "Time Frame: " << config.getTimeFrameStr() << std::endl;
    std::cout << "In-Sample Bars: " << inSampleSeries.getNumEntries() << std::endl;
    std::cout << "Holding Period: " << period << std::endl;
    std::cout << "=================================" << std::endl;
    
    try {
        // Calculate base ROC series for all methods
        auto rocSeries = RocSeries(inSampleSeries.CloseTimeSeries(), period);
        auto rocVec = rocSeries.getTimeSeriesAsVector();
        
        // 1. ComputeRobustStopAndTargetFromSeries (2-argument version)
        std::cout << "\n1. Robust Stop and Target (Qn + Medcouple skew):" << std::endl;
        auto [robustProfit, robustStop] = ComputeRobustStopAndTargetFromSeries(inSampleSeries, period);
        
        // Calculate statistics for robust method (uses full ROC series)
        Num robustMedian = MedianOfVec(rocVec);
        RobustQn<Num> robustQnEstimator;
        Num robustQn = robustQnEstimator.getRobustQn(rocVec);
        Num robustSkew = RobustSkewMedcouple(rocSeries);
        
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "   Statistics - Median: " << robustMedian.getAsDouble() << "%, Qn: " << robustQn.getAsDouble() << "%, Skew: " << robustSkew.getAsDouble() << std::endl;
        std::cout << std::setprecision(2);
        std::cout << "   Profit Target Width: " << robustProfit.getAsDouble() << "%" << std::endl;
        std::cout << "   Stop Loss Width:     " << robustStop.getAsDouble() << "%" << std::endl;
        
        // 2. ComputeLongStopAndTargetFromSeries
        std::cout << "\n2. Long Position Stop and Target (Partitioned distributions):" << std::endl;
        auto [longProfit, longStop] = ComputeLongStopAndTargetFromSeries(inSampleSeries, period);
        
        // Calculate statistics for long method (uses positive and negative partitions)
        const Num zero = DecimalConstants<Num>::DecimalZero;
        std::vector<Num> positiveRocs, negativeRocs;
        for (const auto& roc : rocVec) {
            if (roc > zero) positiveRocs.push_back(roc);
            else if (roc < zero) negativeRocs.push_back(roc);
        }
        
        Num longPosMedian = positiveRocs.empty() ? zero : MedianOfVec(positiveRocs);
        RobustQn<Num> longPosQnEstimator;
        Num longPosQn = positiveRocs.empty() ? zero : longPosQnEstimator.getRobustQn(positiveRocs);
        Num longNegMedian = negativeRocs.empty() ? zero : MedianOfVec(negativeRocs);
        
        // Calculate skew for positive and negative partitions
        Num longPosSkew = zero, longNegSkew = zero;
        if (positiveRocs.size() >= 3) {
            NumericTimeSeries<Num> posRocSeries(rocSeries.getTimeFrame());
            auto baseTime = boost::posix_time::second_clock::local_time();
            for (size_t i = 0; i < positiveRocs.size(); ++i) {
                auto timestamp = baseTime + boost::posix_time::seconds(static_cast<long>(i));
                posRocSeries.addEntry(NumericTimeSeriesEntry<Num>(timestamp, positiveRocs[i], rocSeries.getTimeFrame()));
            }
            longPosSkew = RobustSkewMedcouple(posRocSeries);
        }
        if (negativeRocs.size() >= 3) {
            NumericTimeSeries<Num> negRocSeries(rocSeries.getTimeFrame());
            auto baseTime = boost::posix_time::second_clock::local_time();
            for (size_t i = 0; i < negativeRocs.size(); ++i) {
                auto timestamp = baseTime + boost::posix_time::seconds(static_cast<long>(i));
                negRocSeries.addEntry(NumericTimeSeriesEntry<Num>(timestamp, negativeRocs[i], rocSeries.getTimeFrame()));
            }
            longNegSkew = RobustSkewMedcouple(negRocSeries);
        }
        
        std::cout << std::setprecision(4);
        std::cout << "   Statistics - Pos: Med=" << longPosMedian.getAsDouble() << "%, Qn=" << longPosQn.getAsDouble() << "%, Skew=" << longPosSkew.getAsDouble() << std::endl;
        std::cout << "                Neg: Med=" << longNegMedian.getAsDouble() << "%, Skew=" << longNegSkew.getAsDouble() << std::endl;
        std::cout << std::setprecision(2);
        std::cout << "   Profit Target Width: " << longProfit.getAsDouble() << "%" << std::endl;
        std::cout << "   Stop Loss Width:     " << longStop.getAsDouble() << "%" << std::endl;
        
        // 3. ComputeShortStopAndTargetFromSeries
        std::cout << "\n3. Short Position Stop and Target (Partitioned distributions):" << std::endl;
        auto [shortProfit, shortStop] = ComputeShortStopAndTargetFromSeries(inSampleSeries, period);
        
        // Calculate statistics for short method (uses negative and positive partitions)
        RobustQn<Num> shortNegQnEstimator;
        Num shortNegQn = negativeRocs.empty() ? zero : shortNegQnEstimator.getRobustQn(negativeRocs);
        Num shortPosMedian = positiveRocs.empty() ? zero : MedianOfVec(positiveRocs);
        
        std::cout << std::setprecision(4);
        std::cout << "   Statistics - Neg: Med=" << longNegMedian.getAsDouble() << "%, Qn=" << shortNegQn.getAsDouble() << "%, Skew=" << longNegSkew.getAsDouble() << std::endl;
        std::cout << "                Pos: Med=" << shortPosMedian.getAsDouble() << "%, Skew=" << longPosSkew.getAsDouble() << std::endl;
        std::cout << std::setprecision(2);
        std::cout << "   Profit Target Width: " << shortProfit.getAsDouble() << "%" << std::endl;
        std::cout << "   Stop Loss Width:     " << shortStop.getAsDouble() << "%" << std::endl;
        
        // Summary comparison
        std::cout << "\n=== Summary Comparison ===" << std::endl;
        std::cout << "Method                    | Profit Target | Stop Loss | Key Statistics" << std::endl;
        std::cout << "--------------------------|---------------|-----------|------------------" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Robust (Qn+Skew)         |        " << robustProfit.getAsDouble() << "% |    " << robustStop.getAsDouble() << "% | Full ROC (Skew: " << std::setprecision(3) << robustSkew.getAsDouble() << ")" << std::endl;
        std::cout << std::setprecision(2);
        std::cout << "Long Position             |        " << longProfit.getAsDouble() << "% |    " << longStop.getAsDouble() << "% | Pos/Neg Partition" << std::endl;
        std::cout << "Short Position            |        " << shortProfit.getAsDouble() << "% |    " << shortStop.getAsDouble() << "% | Neg/Pos Partition" << std::endl;
        
        // Additional summary statistics
        std::cout << "\n=== Data Summary ===" << std::endl;
        std::cout << std::setprecision(4);
        std::cout << "Total ROC observations:   " << rocVec.size() << std::endl;
        std::cout << "Positive ROC count:       " << positiveRocs.size() << " (" << std::setprecision(1) << (100.0 * positiveRocs.size() / rocVec.size()) << "%)" << std::endl;
        std::cout << "Negative ROC count:       " << negativeRocs.size() << " (" << (100.0 * negativeRocs.size() / rocVec.size()) << "%)" << std::endl;
        
    } catch (const std::domain_error& e) {
        std::cerr << "\nData Error: " << e.what() << std::endl;
        std::cerr << "Suggestion: Ensure sufficient data for analysis (minimum ~25 bars recommended)" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nError calculating statistics: " << e.what() << std::endl;
    }
    
    std::cout << "\n=================================" << std::endl;
    std::cout << "Note: All values are percentage widths from median/center point." << std::endl;
    std::cout << "No files were written in statistics-only mode." << std::endl;
}

void UserInterface::displaySeparateResults(const CombinedStatisticsResults& stats, const CleanStartResult& cleanStart) {
    const auto& longResults = stats.getLongResults();
    const auto& shortResults = stats.getShortResults();
    
    std::cout << "\nLong Position Stop and Target (Partitioned distributions):" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "   Statistics - Pos: Med=" << longResults.getPosMedian().getAsDouble() << "%, Qn=" << longResults.getPosQn().getAsDouble() << "%, Skew=" << longResults.getPosSkew().getAsDouble() << std::endl;
    std::cout << "                Neg: Med=" << longResults.getNegMedian().getAsDouble() << "%, Skew=" << longResults.getNegSkew().getAsDouble() << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "   Profit Target Width: " << longResults.getProfitTargetValue().getAsDouble() << "%" << std::endl;
    std::cout << "   Stop Loss Width:     " << longResults.getStopValue().getAsDouble() << "%" << std::endl;
    
    std::cout << "\nShort Position Stop and Target (Partitioned distributions):" << std::endl;
    std::cout << std::setprecision(4);
    std::cout << "   Statistics - Neg: Med=" << shortResults.getNegMedian().getAsDouble() << "%, Qn=" << shortResults.getNegQn().getAsDouble() << "%, Skew=" << shortResults.getNegSkew().getAsDouble() << std::endl;
    std::cout << "                Pos: Med=" << shortResults.getPosMedian().getAsDouble() << "%, Skew=" << shortResults.getPosSkew().getAsDouble() << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "   Profit Target Width: " << shortResults.getProfitTargetValue().getAsDouble() << "%" << std::endl;
    std::cout << "   Stop Loss Width:     " << shortResults.getStopValue().getAsDouble() << "%" << std::endl;
    
    // Summary comparison
    std::cout << "\n=== Summary Comparison ===" << std::endl;
    std::cout << "Position Type             | Profit Target | Stop Loss | Data Partition" << std::endl;
    std::cout << "--------------------------|---------------|-----------|------------------" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Long Position             |        " << longResults.getProfitTargetValue().getAsDouble() << "% |    " << longResults.getStopValue().getAsDouble() << "% | Pos/Neg (" << longResults.getPosCount() << "/" << longResults.getNegCount() << ")" << std::endl;
    std::cout << "Short Position            |        " << shortResults.getProfitTargetValue().getAsDouble() << "% |    " << shortResults.getStopValue().getAsDouble() << "% | Neg/Pos (" << shortResults.getNegCount() << "/" << shortResults.getPosCount() << ")" << std::endl;
    
    // Additional summary statistics
    size_t totalObs = longResults.getPosCount() + longResults.getNegCount();
    std::cout << "\n=== Data Summary ===" << std::endl;
    std::cout << "Total ROC observations:   " << totalObs << std::endl;
    std::cout << std::setprecision(1);
    std::cout << "Positive ROC count:       " << longResults.getPosCount() << " (" << (100.0 * longResults.getPosCount() / totalObs) << "%)" << std::endl;
    std::cout << "Negative ROC count:       " << longResults.getNegCount() << " (" << (100.0 * longResults.getNegCount() / totalObs) << "%)" << std::endl;
    
    std::cout << "\n=================================" << std::endl;
    std::cout << "Note: All values are percentage widths from median/center point." << std::endl;
}