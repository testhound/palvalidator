#pragma once

#include "PalSetupTypes.h"
#include "TimeSeries.h"

/**
 * @brief Handles statistical calculations for stop/target values and performance metrics
 */
class StatisticsCalculator {
public:
    StatisticsCalculator();
    ~StatisticsCalculator() = default;
    
    /**
     * @brief Calculate robust stop and target values using asymmetric method
     */
    StatisticsResults calculateRobustStopAndTarget(
        const mkc_timeseries::OHLCTimeSeries<Num>& inSampleSeries,
        int holdingPeriod);
    
    /**
     * @brief Calculate separate long and short stop and target values
     */
    CombinedStatisticsResults calculateSeparateStopAndTarget(
        const mkc_timeseries::OHLCTimeSeries<Num>& inSampleSeries,
        int holdingPeriod);
    
    /**
     * @brief Validate statistical results and display warnings if needed
     */
    void validateStatistics(const StatisticsResults& stats);

private:
    /**
     * @brief Calculate traditional statistics for reporting
     */
    void calculateTraditionalStatistics(
        const mkc_timeseries::OHLCTimeSeries<Num>& series,
        int holdingPeriod,
        Num& medianOfRoc,
        Num& robustQn,
        Num& MAD,
        Num& stdDev,
        Num& skew);
    
    /**
     * @brief Compute asymmetric stop and target values
     */
    std::pair<Num, Num> computeAsymmetricStopAndTarget(
        const mkc_timeseries::OHLCTimeSeries<Num>& series,
        int holdingPeriod);
    
    /**
     * @brief Compute long position stop and target values with partitioned statistics
     */
    LongStatisticsResults computeLongStopAndTarget(
        const mkc_timeseries::OHLCTimeSeries<Num>& series,
        int holdingPeriod);
    
    /**
     * @brief Compute short position stop and target values with partitioned statistics
     */
    ShortStatisticsResults computeShortStopAndTarget(
        const mkc_timeseries::OHLCTimeSeries<Num>& series,
        int holdingPeriod);
    
    /**
     * @brief Check for statistical warnings and display them
     */
    void checkStatisticalWarnings(const StatisticsResults& stats);
};