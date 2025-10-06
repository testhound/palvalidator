#pragma once

#include "PalSetupTypes.h"
#include "TimeSeries.h"
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Handles all file I/O operations including configuration files, data files, and reports
 */
class FileOperations {
public:
    FileOperations();
    ~FileOperations() = default;
    
    /**
     * @brief Write CSV configuration file for permutation testing
     */
    void writeConfigFile(const std::string& outputDir,
                        const std::string& tickerSymbol,
                        const mkc_timeseries::OHLCTimeSeries<Num>& insampleSeries,
                        const mkc_timeseries::OHLCTimeSeries<Num>& outOfSampleSeries,
                        const std::string& timeFrame);
    
    /**
     * @brief Write target/stop files for all PAL subdirectories
     */
    void writeTargetStopFiles(const std::vector<fs::path>& palSubDirs,
                             const std::string& tickerSymbol,
                             const StatisticsResults& stats);
    
    /**
     * @brief Write separate long and short target/stop files for all PAL subdirectories
     */
    void writeSeparateTargetStopFiles(const std::vector<fs::path>& palSubDirs,
                                     const std::string& tickerSymbol,
                                     const CombinedStatisticsResults& stats);
    
    /**
     * @brief Write PAL data files to all subdirectories
     */
    void writeDataFiles(const std::vector<fs::path>& palSubDirs,
                       const SplitTimeSeriesData& splitData,
                       const SetupConfiguration& config);
    
    /**
     * @brief Write validation files to risk-reward directories
     */
    void writeValidationFiles(const DirectoryPaths& paths,
                             const SplitTimeSeriesData& splitData,
                             const SetupConfiguration& config,
                             const mkc_timeseries::OHLCTimeSeries<Num>& completeTimeSeries);
    
    /**
     * @brief Write setup details file with all configuration and results
     */
    void writeDetailsFile(const fs::path& outputPath,
                         const SetupConfiguration& config,
                         const StatisticsResults& stats,
                         const CleanStartResult& cleanStart);
    
    /**
     * @brief Write setup details file with separate long/short configuration and results
     */
    void writeSeparateDetailsFile(const fs::path& outputPath,
                                 const SetupConfiguration& config,
                                 const CombinedStatisticsResults& stats,
                                 const CleanStartResult& cleanStart,
                                 const SplitTimeSeriesData& splitData);

private:
    /**
     * @brief Write a single target/stop file
     */
    void writeTargetStopFile(const fs::path& filePath, const Num& target, const Num& stop);
    
    /**
     * @brief Write a PAL data file (either standard OHLC or indicator-based)
     */
    void writePalDataFile(const fs::path& filePath, 
                         const mkc_timeseries::OHLCTimeSeries<Num>& series,
                         const SetupConfiguration& config,
                         const mkc_timeseries::NumericTimeSeries<Num>* indicator = nullptr);
    
    /**
     * @brief Write a validation data file
     */
    void writeValidationDataFile(const fs::path& filePath,
                                const mkc_timeseries::OHLCTimeSeries<Num>& series,
                                const SetupConfiguration& config);
    
    /**
     * @brief Format date/time for configuration file based on timeframe
     */
    std::string formatDateForConfig(const boost::posix_time::ptime& dateTime, bool isIntraday);
    
    /**
     * @brief Validate that file was written successfully
     */
    void validateFileWrite(const fs::path& filePath);
};