#include "TimeSeriesProcessor.h"
#include "TimeSeriesCsvReader.h"
#include "TimeSeriesIndicators.h"
#include "TimeSeriesException.h"
#include "SecurityAttributesFactory.h"
#include <stdexcept>
#include <iostream>

using namespace mkc_timeseries;

TimeSeriesProcessor::TimeSeriesProcessor() = default;

std::shared_ptr<TimeSeriesCsvReader<Num>> TimeSeriesProcessor::createTimeSeriesReader(
    int fileType,
    const std::string& fileName,
    const Num& tick,
    TimeFrame::Duration timeFrame) {
    
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
            // Use TradeStationFormatCsvReader for intraday, TradeStationErrorCheckingFormatCsvReader for others
            if (timeFrame == TimeFrame::INTRADAY) {
                return std::make_shared<TradeStationFormatCsvReader<Num>>(fileName,
                                                                         timeFrame,
                                                                         TradingVolume::SHARES,
                                                                         tick);
            } else {
                return std::make_shared<TradeStationErrorCheckingFormatCsvReader<Num>>(fileName,
                                                                                      timeFrame,
                                                                                      TradingVolume::SHARES,
                                                                                      tick);
            }
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
        case 6:
            return std::make_shared<WealthLabCsvReader<Num>>(fileName,
                                                            timeFrame,
                                                            TradingVolume::SHARES,
                                                            tick);
        default:
            throw std::out_of_range("Invalid file type: " + std::to_string(fileType));
    }
}

std::shared_ptr<OHLCTimeSeries<Num>> TimeSeriesProcessor::loadTimeSeries(
    std::shared_ptr<TimeSeriesCsvReader<Num>> reader) {
    
    try {
        reader->readFile();
    } catch (const TimeSeriesException& e) {
        std::cerr << "ERROR: Data file contains duplicate timestamps." << std::endl;
        std::cerr << "Details: " << e.what() << std::endl;
        std::cerr << "Action: Please clean the data file and remove any duplicate entries." << std::endl;
        std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
        throw;
    } catch (const TimeSeriesEntryException& e) {
        std::cerr << "ERROR: Data file contains invalid OHLC price relationships." << std::endl;
        std::cerr << "Details: " << e.what() << std::endl;
        std::cerr << "Action: Please check and correct the data file for invalid price entries." << std::endl;
        std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
        throw;
    }
    
    return reader->getTimeSeries();
}

SplitTimeSeriesData TimeSeriesProcessor::splitTimeSeries(
    const OHLCTimeSeries<Num>& series,
    const CleanStartResult& cleanStart,
    const SetupConfiguration& config) {
    
    // Create split data container
    SplitTimeSeriesData splitData(series.getTimeFrame(), series.getVolumeUnits());
    
    // Calculate sizes based on clean start
    size_t totalSize = series.getNumEntries();
    size_t cleanStartIndex = cleanStart.isFound() ? cleanStart.getStartIndex() : 0;
    size_t usableSize = (cleanStartIndex < totalSize) ? (totalSize - cleanStartIndex) : 0;

    size_t insampleSize = calculateSplitSize(usableSize, config.getInsamplePercent());
    size_t oosSize = calculateSplitSize(usableSize, config.getOutOfSamplePercent());
    
    // Split the data
    size_t globalIdx = 0;
    size_t usedIdx = 0;
    
    for (auto it = series.beginSortedAccess(); it != series.endSortedAccess(); ++it, ++globalIdx) {
        if (globalIdx < cleanStartIndex) continue; // skip early distorted data
        
        const auto& entry = *it;
        if (usedIdx < insampleSize) {
            splitData.getInSample().addEntry(entry);
        } else if (usedIdx < insampleSize + oosSize) {
            splitData.getOutOfSample().addEntry(entry);
        } else {
            splitData.getReserved().addEntry(entry);
        }
        ++usedIdx;
    }
    
    // Calculate indicators if in indicator mode
    if (config.isIndicatorMode() && config.getSelectedIndicator() == "IBS") {
        std::cout << "Calculating Internal Bar Strength (IBS) indicator..." << std::endl;
        splitData.getInSampleIndicator() = calculateIndicators(splitData.getInSample(), "IBS");
        std::cout << "Generated " << splitData.getInSampleIndicator().getNumEntries() 
                  << " IBS values for insample data." << std::endl;
    }
    
    return splitData;
}

NumericTimeSeries<Num> TimeSeriesProcessor::calculateIndicators(
    const OHLCTimeSeries<Num>& series,
    const std::string& indicatorType) {
    
    if (indicatorType == "IBS") {
        return IBS1Series(series);
    }
    
    throw std::invalid_argument("Unsupported indicator type: " + indicatorType);
}

void TimeSeriesProcessor::validateTimeSeries(const OHLCTimeSeries<Num>& series) {
    if (series.getNumEntries() == 0) {
        throw std::runtime_error("Time series is empty");
    }
    
    // Additional validation could be added here
}

size_t TimeSeriesProcessor::calculateSplitSize(size_t totalSize, double percentage) {
    return static_cast<size_t>(totalSize * (percentage / 100.0));
}