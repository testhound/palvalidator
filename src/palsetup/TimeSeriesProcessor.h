#pragma once

#include "PalSetupTypes.h"
#include "TimeSeriesCsvReader.h"
#include "TimeSeries.h"
#include <memory>

/**
 * @brief Handles time series data processing including loading, splitting, and indicator calculations
 */
class TimeSeriesProcessor {
public:
    TimeSeriesProcessor();
    ~TimeSeriesProcessor() = default;
    
    /**
     * @brief Factory function for creating appropriate time series readers based on file type
     */
    std::shared_ptr<mkc_timeseries::TimeSeriesCsvReader<Num>> createTimeSeriesReader(
        int fileType,
        const std::string& fileName,
        const Num& tick,
        mkc_timeseries::TimeFrame::Duration timeFrame);
    
    /**
     * @brief Load time series from file using the provided reader
     */
    std::shared_ptr<mkc_timeseries::OHLCTimeSeries<Num>> loadTimeSeries(
        std::shared_ptr<mkc_timeseries::TimeSeriesCsvReader<Num>> reader);
    
    /**
     * @brief Split time series into in-sample, out-of-sample, and reserved portions
     */
    SplitTimeSeriesData splitTimeSeries(
        const mkc_timeseries::OHLCTimeSeries<Num>& series,
        const CleanStartResult& cleanStart,
        const SetupConfiguration& config);
    
    /**
     * @brief Calculate technical indicators for the given series
     */
    mkc_timeseries::NumericTimeSeries<Num> calculateIndicators(
        const mkc_timeseries::OHLCTimeSeries<Num>& series,
        const std::string& indicatorType);

private:
    /**
     * @brief Validate time series data for common issues
     */
    void validateTimeSeries(const mkc_timeseries::OHLCTimeSeries<Num>& series);
    
    /**
     * @brief Calculate split size based on total size and percentage
     */
    size_t calculateSplitSize(size_t totalSize, double percentage);
};