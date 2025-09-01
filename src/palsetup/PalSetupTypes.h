#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include "TimeFrame.h"
#include "TimeSeries.h"
#include "TradingVolume.h"
#include "number.h"

namespace fs = std::filesystem;
using Num = num::DefaultNumber;

/**
 * @brief Configuration for quantization-aware clean start detection
 */
class CleanStartConfig {
public:
    CleanStartConfig(int windowBars = 252, 
                    int stabilityBufferBars = 60,
                    double maxRelTick = 0.005,
                    double maxZeroFrac = 0.30,
                    int minUniqueLevels = 120,
                    int intradayMinutesPerBar = 0)
        : windowBars_(windowBars)
        , stabilityBufferBars_(stabilityBufferBars)
        , maxRelTick_(maxRelTick)
        , maxZeroFrac_(maxZeroFrac)
        , minUniqueLevels_(minUniqueLevels)
        , intradayMinutesPerBar_(intradayMinutesPerBar) {}
    
    // Getters
    int getWindowBars() const { return windowBars_; }
    int getStabilityBufferBars() const { return stabilityBufferBars_; }
    double getMaxRelTick() const { return maxRelTick_; }
    double getMaxZeroFrac() const { return maxZeroFrac_; }
    int getMinUniqueLevels() const { return minUniqueLevels_; }
    int getIntradayMinutesPerBar() const { return intradayMinutesPerBar_; }
    
    // Modification methods (for timeframe-specific adjustments)
    void adjustForTimeFrame(mkc_timeseries::TimeFrame::Duration timeFrame, size_t totalBars, int intradayMinutes);

private:
    int windowBars_;
    int stabilityBufferBars_;
    double maxRelTick_;
    double maxZeroFrac_;
    int minUniqueLevels_;
    int intradayMinutesPerBar_;
};

/**
 * @brief Result of clean start index detection
 */
class CleanStartResult {
public:
    CleanStartResult(size_t startIndex = 0, 
                    double tick = 0.01, 
                    double relTick = 0.0, 
                    double zeroFrac = 0.0, 
                    bool found = false)
        : startIndex_(startIndex)
        , tick_(tick)
        , relTick_(relTick)
        , zeroFrac_(zeroFrac)
        , found_(found) {}
    
    // Getters
    size_t getStartIndex() const { return startIndex_; }
    double getTick() const { return tick_; }
    double getRelTick() const { return relTick_; }
    double getZeroFrac() const { return zeroFrac_; }
    bool isFound() const { return found_; }

private:
    size_t startIndex_;
    double tick_;
    double relTick_;
    double zeroFrac_;
    bool found_;
};

/**
 * @brief Window parameters for analysis
 */
class WindowParameters {
public:
    WindowParameters(int windowBars, int stabilityBufferBars)
        : windowBars_(windowBars)
        , stabilityBufferBars_(stabilityBufferBars) {}
    
    int getWindowBars() const { return windowBars_; }
    int getStabilityBufferBars() const { return stabilityBufferBars_; }

private:
    int windowBars_;
    int stabilityBufferBars_;
};

/**
 * @brief Complete setup configuration
 */
class SetupConfiguration {
public:
    SetupConfiguration(const std::string& tickerSymbol,
                      const std::string& timeFrameStr,
                      mkc_timeseries::TimeFrame::Duration timeFrame,
                      int fileType,
                      const std::string& historicDataFileName,
                      const Num& securityTick,
                      int intradayMinutes = 0,
                      bool indicatorMode = false,
                      const std::string& selectedIndicator = "",
                      double insamplePercent = 60.0,
                      double outOfSamplePercent = 40.0,
                      double reservedPercent = 0.0,
                      int holdingPeriod = 1,
                      bool statsOnlyMode = false)
        : tickerSymbol_(tickerSymbol)
        , timeFrameStr_(timeFrameStr)
        , timeFrame_(timeFrame)
        , fileType_(fileType)
        , historicDataFileName_(historicDataFileName)
        , securityTick_(securityTick)
        , intradayMinutes_(intradayMinutes)
        , indicatorMode_(indicatorMode)
        , selectedIndicator_(selectedIndicator)
        , insamplePercent_(insamplePercent)
        , outOfSamplePercent_(outOfSamplePercent)
        , reservedPercent_(reservedPercent)
        , holdingPeriod_(holdingPeriod)
        , statsOnlyMode_(statsOnlyMode) {}
    
    // Getters
    const std::string& getTickerSymbol() const { return tickerSymbol_; }
    const std::string& getTimeFrameStr() const { return timeFrameStr_; }
    mkc_timeseries::TimeFrame::Duration getTimeFrame() const { return timeFrame_; }
    int getIntradayMinutes() const { return intradayMinutes_; }
    bool isIndicatorMode() const { return indicatorMode_; }
    const std::string& getSelectedIndicator() const { return selectedIndicator_; }
    double getInsamplePercent() const { return insamplePercent_; }
    double getOutOfSamplePercent() const { return outOfSamplePercent_; }
    double getReservedPercent() const { return reservedPercent_; }
    int getHoldingPeriod() const { return holdingPeriod_; }
    int getFileType() const { return fileType_; }
    const std::string& getHistoricDataFileName() const { return historicDataFileName_; }
    const Num& getSecurityTick() const { return securityTick_; }
    bool isStatsOnlyMode() const { return statsOnlyMode_; }
    
    // Validation
    bool validatePercentages() const;

private:
    std::string tickerSymbol_;
    std::string timeFrameStr_;
    mkc_timeseries::TimeFrame::Duration timeFrame_;
    int fileType_;
    std::string historicDataFileName_;
    Num securityTick_;
    int intradayMinutes_;
    bool indicatorMode_;
    std::string selectedIndicator_;
    double insamplePercent_;
    double outOfSamplePercent_;
    double reservedPercent_;
    int holdingPeriod_;
    bool statsOnlyMode_;
};

/**
 * @brief Container for split time series data
 */
class SplitTimeSeriesData {
public:
    SplitTimeSeriesData(mkc_timeseries::TimeFrame::Duration timeFrame,
                       mkc_timeseries::TradingVolume::VolumeUnit volumeUnits)
        : inSample_(timeFrame, volumeUnits)
        , outOfSample_(timeFrame, volumeUnits)
        , reserved_(timeFrame, volumeUnits)
        , inSampleIndicator_(timeFrame) {}
    
    // Getters
    const mkc_timeseries::OHLCTimeSeries<Num>& getInSample() const { return inSample_; }
    const mkc_timeseries::OHLCTimeSeries<Num>& getOutOfSample() const { return outOfSample_; }
    const mkc_timeseries::OHLCTimeSeries<Num>& getReserved() const { return reserved_; }
    const mkc_timeseries::NumericTimeSeries<Num>& getInSampleIndicator() const { return inSampleIndicator_; }
    
    // Non-const getters for modification during splitting
    mkc_timeseries::OHLCTimeSeries<Num>& getInSample() { return inSample_; }
    mkc_timeseries::OHLCTimeSeries<Num>& getOutOfSample() { return outOfSample_; }
    mkc_timeseries::OHLCTimeSeries<Num>& getReserved() { return reserved_; }
    mkc_timeseries::NumericTimeSeries<Num>& getInSampleIndicator() { return inSampleIndicator_; }

private:
    mkc_timeseries::OHLCTimeSeries<Num> inSample_;
    mkc_timeseries::OHLCTimeSeries<Num> outOfSample_;
    mkc_timeseries::OHLCTimeSeries<Num> reserved_;
    mkc_timeseries::NumericTimeSeries<Num> inSampleIndicator_;
};

/**
 * @brief Directory paths for output structure
 */
class DirectoryPaths {
public:
    DirectoryPaths(const fs::path& baseDir,
                  const fs::path& timeFrameDir,
                  const fs::path& rocDir,
                  const fs::path& palDir,
                  const fs::path& valDir,
                  const std::vector<fs::path>& riskRewardDirs,
                  const std::vector<fs::path>& palSubDirs)
        : baseDir_(baseDir)
        , timeFrameDir_(timeFrameDir)
        , rocDir_(rocDir)
        , palDir_(palDir)
        , valDir_(valDir)
        , riskRewardDirs_(riskRewardDirs)
        , palSubDirs_(palSubDirs) {}
    
    // Getters
    const fs::path& getBaseDir() const { return baseDir_; }
    const fs::path& getTimeFrameDir() const { return timeFrameDir_; }
    const fs::path& getRocDir() const { return rocDir_; }
    const fs::path& getPalDir() const { return palDir_; }
    const fs::path& getValDir() const { return valDir_; }
    const std::vector<fs::path>& getRiskRewardDirs() const { return riskRewardDirs_; }
    const std::vector<fs::path>& getPalSubDirs() const { return palSubDirs_; }

private:
    fs::path baseDir_;
    fs::path timeFrameDir_;
    fs::path rocDir_;
    fs::path palDir_;
    fs::path valDir_;
    std::vector<fs::path> riskRewardDirs_;
    std::vector<fs::path> palSubDirs_;
};

/**
 * @brief Statistical calculation results
 */
class StatisticsResults {
public:
    StatisticsResults(const Num& profitTargetValue,
                     const Num& stopValue,
                     const Num& medianOfRoc,
                     const Num& robustQn,
                     const Num& MAD,
                     const Num& stdDev,
                     const Num& skew)
        : profitTargetValue_(profitTargetValue)
        , stopValue_(stopValue)
        , medianOfRoc_(medianOfRoc)
        , robustQn_(robustQn)
        , MAD_(MAD)
        , stdDev_(stdDev)
        , skew_(skew) {}
    
    // Getters
    const Num& getProfitTargetValue() const { return profitTargetValue_; }
    const Num& getStopValue() const { return stopValue_; }
    const Num& getMedianOfRoc() const { return medianOfRoc_; }
    const Num& getRobustQn() const { return robustQn_; }
    const Num& getMAD() const { return MAD_; }
    const Num& getStdDev() const { return stdDev_; }
    const Num& getSkew() const { return skew_; }

private:
    Num profitTargetValue_;
    Num stopValue_;
    Num medianOfRoc_;
    Num robustQn_;
    Num MAD_;
    Num stdDev_;
    Num skew_;
};

/**
 * @brief Statistical results for long position calculations
 */
class LongStatisticsResults {
public:
    LongStatisticsResults(const Num& profitTargetValue,
                         const Num& stopValue,
                         const Num& posMedian,
                         const Num& posQn,
                         const Num& posSkew,
                         const Num& negMedian,
                         const Num& negSkew,
                         size_t posCount,
                         size_t negCount)
        : profitTargetValue_(profitTargetValue)
        , stopValue_(stopValue)
        , posMedian_(posMedian)
        , posQn_(posQn)
        , posSkew_(posSkew)
        , negMedian_(negMedian)
        , negSkew_(negSkew)
        , posCount_(posCount)
        , negCount_(negCount) {}
    
    // Getters
    const Num& getProfitTargetValue() const { return profitTargetValue_; }
    const Num& getStopValue() const { return stopValue_; }
    const Num& getPosMedian() const { return posMedian_; }
    const Num& getPosQn() const { return posQn_; }
    const Num& getPosSkew() const { return posSkew_; }
    const Num& getNegMedian() const { return negMedian_; }
    const Num& getNegSkew() const { return negSkew_; }
    size_t getPosCount() const { return posCount_; }
    size_t getNegCount() const { return negCount_; }

private:
    Num profitTargetValue_;
    Num stopValue_;
    Num posMedian_;
    Num posQn_;
    Num posSkew_;
    Num negMedian_;
    Num negSkew_;
    size_t posCount_;
    size_t negCount_;
};

/**
 * @brief Statistical results for short position calculations
 */
class ShortStatisticsResults {
public:
    ShortStatisticsResults(const Num& profitTargetValue,
                          const Num& stopValue,
                          const Num& negMedian,
                          const Num& negQn,
                          const Num& negSkew,
                          const Num& posMedian,
                          const Num& posSkew,
                          size_t negCount,
                          size_t posCount)
        : profitTargetValue_(profitTargetValue)
        , stopValue_(stopValue)
        , negMedian_(negMedian)
        , negQn_(negQn)
        , negSkew_(negSkew)
        , posMedian_(posMedian)
        , posSkew_(posSkew)
        , negCount_(negCount)
        , posCount_(posCount) {}
    
    // Getters
    const Num& getProfitTargetValue() const { return profitTargetValue_; }
    const Num& getStopValue() const { return stopValue_; }
    const Num& getNegMedian() const { return negMedian_; }
    const Num& getNegQn() const { return negQn_; }
    const Num& getNegSkew() const { return negSkew_; }
    const Num& getPosMedian() const { return posMedian_; }
    const Num& getPosSkew() const { return posSkew_; }
    size_t getNegCount() const { return negCount_; }
    size_t getPosCount() const { return posCount_; }

private:
    Num profitTargetValue_;
    Num stopValue_;
    Num negMedian_;
    Num negQn_;
    Num negSkew_;
    Num posMedian_;
    Num posSkew_;
    size_t negCount_;
    size_t posCount_;
};

/**
 * @brief Combined statistical results for both long and short positions
 */
class CombinedStatisticsResults {
public:
    CombinedStatisticsResults(const LongStatisticsResults& longResults,
                             const ShortStatisticsResults& shortResults)
        : longResults_(longResults)
        , shortResults_(shortResults) {}
    
    // Getters
    const LongStatisticsResults& getLongResults() const { return longResults_; }
    const ShortStatisticsResults& getShortResults() const { return shortResults_; }

private:
    LongStatisticsResults longResults_;
    ShortStatisticsResults shortResults_;
};