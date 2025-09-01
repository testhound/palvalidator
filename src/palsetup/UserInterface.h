#pragma once

#include "PalSetupTypes.h"
#include <string>
#include <vector>

/**
 * @brief Handles all user interface operations including command-line parsing and interactive input
 */
class UserInterface {
public:
    UserInterface();
    ~UserInterface() = default;
    
    /**
     * @brief Parse command line arguments and collect user input to build complete configuration
     */
    SetupConfiguration parseCommandLineArgs(int argc, char** argv);
    
    /**
     * @brief Display final results to the user
     */
    void displayResults(const StatisticsResults& stats, const CleanStartResult& cleanStart);
    
    /**
     * @brief Display separate long and short results
     */
    void displaySeparateResults(const CombinedStatisticsResults& stats, const CleanStartResult& cleanStart);
    
    /**
     * @brief Display setup configuration summary
     */
    void displaySetupSummary(const SetupConfiguration& config);
    
    /**
     * @brief Display statistics only without writing files
     */
    void displayStatisticsOnly(const mkc_timeseries::OHLCTimeSeries<Num>& inSampleSeries,
                              const SetupConfiguration& config);

private:
    // Helper methods for building configuration
    std::string extractDefaultTicker(const std::string& filename);
    std::string getTickerSymbol(const std::string& defaultTicker);
    std::pair<std::string, mkc_timeseries::TimeFrame::Duration> getTimeFrameInput();
    int getIntradayMinutes();
    std::pair<bool, std::string> getIndicatorSelection();
    std::tuple<double, double, double> getDataSplitInput();
    int getHoldingPeriodInput();
    
    // Input validation helpers
    std::string getValidatedStringInput(const std::string& prompt, const std::string& defaultValue = "");
    int getValidatedIntInput(const std::string& prompt, int defaultValue, int minValue, int maxValue);
    double getValidatedDoubleInput(const std::string& prompt, double defaultValue, double minValue, double maxValue);
    bool validatePercentages(double inSample, double outOfSample, double reserved);
    
    // Display helpers
    void displayUsage();
    void displayCleanStartInfo(const CleanStartResult& cleanStart, 
                              const std::string& tickerSymbol,
                              const mkc_timeseries::OHLCTimeSeries<Num>& series,
                              std::optional<double> knownTick);
    
    // Command line parsing state
    bool indicatorMode_;
    bool statsOnlyMode_;
    std::vector<std::string> positionalArgs_;
};