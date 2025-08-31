#include "StatisticsCalculator.h"
#include "TimeSeriesIndicators.h"
#include "DecimalConstants.h"
#include <iostream>
#include <stdexcept>

using namespace mkc_timeseries;

StatisticsCalculator::StatisticsCalculator() = default;

StatisticsResults StatisticsCalculator::calculateRobustStopAndTarget(
    const OHLCTimeSeries<Num>& inSampleSeries,
    int holdingPeriod) {
    
    try {
        // Compute asymmetric profit target and stop values
        auto targetStopPair = computeAsymmetricStopAndTarget(inSampleSeries, holdingPeriod);
        Num profitTargetValue = targetStopPair.first;
        Num stopValue = targetStopPair.second;

        // Calculate traditional statistics for reporting
        Num medianOfRoc, robustQn, MAD, stdDev, skew;
        calculateTraditionalStatistics(inSampleSeries, holdingPeriod, medianOfRoc, robustQn, MAD, stdDev, skew);

        // Create results object
        StatisticsResults results(profitTargetValue, stopValue, medianOfRoc, robustQn, MAD, stdDev, skew);
        
        // Validate and display warnings
        validateStatistics(results);
        
        return results;
        
    } catch (const std::domain_error& e) {
        std::cerr << "ERROR: Intraday data contains duplicate timestamps preventing stop calculation." << std::endl;
        std::cerr << "Details: " << e.what() << std::endl;
        std::cerr << "Cause: NumericTimeSeries cannot handle multiple intraday bars with identical timestamps." << std::endl;
        std::cerr << "Action: Clean the intraday data to ensure unique timestamps for each bar." << std::endl;
        std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
        throw;
    }
}

void StatisticsCalculator::validateStatistics(const StatisticsResults& stats) {
    checkStatisticalWarnings(stats);
}

void StatisticsCalculator::calculateTraditionalStatistics(
    const OHLCTimeSeries<Num>& series,
    int holdingPeriod,
    Num& medianOfRoc,
    Num& robustQn,
    Num& MAD,
    Num& stdDev,
    Num& skew) {
    
    // Calculate traditional statistics for reporting
    NumericTimeSeries<Num> closingPrices(series.CloseTimeSeries());
    NumericTimeSeries<Num> rocOfClosingPrices(RocSeries(closingPrices, holdingPeriod));
    
    medianOfRoc = Median(rocOfClosingPrices);
    robustQn = RobustQn<Num>(rocOfClosingPrices).getRobustQn();
    MAD = MedianAbsoluteDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
    stdDev = StandardDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
    skew = RobustSkewMedcouple(rocOfClosingPrices);
}

std::pair<Num, Num> StatisticsCalculator::computeAsymmetricStopAndTarget(
    const OHLCTimeSeries<Num>& series,
    int holdingPeriod) {
    
    // Use the robust asymmetric method from TimeSeriesIndicators
    return ComputeRobustStopAndTargetFromSeries(series, holdingPeriod);
}

void StatisticsCalculator::checkStatisticalWarnings(const StatisticsResults& stats) {
    // Check if Standard Deviation is > (2 * Qn)
    if ((stats.getRobustQn() * mkc_timeseries::DecimalConstants<Num>::DecimalTwo) < stats.getStdDev()) {
        std::cout << "***** Warning Standard Deviation is > (2 * Qn) *****" << std::endl;
    }
}

CombinedStatisticsResults StatisticsCalculator::calculateSeparateStopAndTarget(
    const OHLCTimeSeries<Num>& inSampleSeries,
    int holdingPeriod) {
    
    try {
        // Compute separate long and short results
        LongStatisticsResults longResults = computeLongStopAndTarget(inSampleSeries, holdingPeriod);
        ShortStatisticsResults shortResults = computeShortStopAndTarget(inSampleSeries, holdingPeriod);
        
        return CombinedStatisticsResults(longResults, shortResults);
        
    } catch (const std::domain_error& e) {
        std::cerr << "ERROR: Intraday data contains duplicate timestamps preventing stop calculation." << std::endl;
        std::cerr << "Details: " << e.what() << std::endl;
        std::cerr << "Cause: NumericTimeSeries cannot handle multiple intraday bars with identical timestamps." << std::endl;
        std::cerr << "Action: Clean the intraday data to ensure unique timestamps for each bar." << std::endl;
        std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
        throw;
    }
}

LongStatisticsResults StatisticsCalculator::computeLongStopAndTarget(
    const OHLCTimeSeries<Num>& series,
    int holdingPeriod) {
    
    using namespace mkc_timeseries;
    
    // Calculate long stop and target using partitioned distributions
    auto [profitWidth, stopWidth] = ComputeLongStopAndTargetFromSeries(series, static_cast<uint32_t>(holdingPeriod));
    
    // Calculate partitioned statistics (following displayStatisticsOnly pattern)
    auto rocSeries = RocSeries(series.CloseTimeSeries(), static_cast<uint32_t>(holdingPeriod));
    auto rocVec = rocSeries.getTimeSeriesAsVector();
    
    const Num zero = DecimalConstants<Num>::DecimalZero;
    std::vector<Num> positiveRocs, negativeRocs;
    for (const auto& roc : rocVec) {
        if (roc > zero) positiveRocs.push_back(roc);
        else if (roc < zero) negativeRocs.push_back(roc);
    }
    
    // Calculate positive partition statistics
    Num posMedian = positiveRocs.empty() ? zero : MedianOfVec(positiveRocs);
    RobustQn<Num> posQnEstimator;
    Num posQn = positiveRocs.empty() ? zero : posQnEstimator.getRobustQn(positiveRocs);
    
    // Calculate negative partition statistics
    Num negMedian = negativeRocs.empty() ? zero : MedianOfVec(negativeRocs);
    
    // Calculate skew for positive and negative partitions
    Num posSkew = zero, negSkew = zero;
    if (positiveRocs.size() >= 3) {
        NumericTimeSeries<Num> posRocSeries(rocSeries.getTimeFrame());
        auto baseTime = boost::posix_time::second_clock::local_time();
        for (size_t i = 0; i < positiveRocs.size(); ++i) {
            auto timestamp = baseTime + boost::posix_time::seconds(static_cast<long>(i));
            posRocSeries.addEntry(NumericTimeSeriesEntry<Num>(timestamp, positiveRocs[i], rocSeries.getTimeFrame()));
        }
        posSkew = RobustSkewMedcouple(posRocSeries);
    }
    if (negativeRocs.size() >= 3) {
        NumericTimeSeries<Num> negRocSeries(rocSeries.getTimeFrame());
        auto baseTime = boost::posix_time::second_clock::local_time();
        for (size_t i = 0; i < negativeRocs.size(); ++i) {
            auto timestamp = baseTime + boost::posix_time::seconds(static_cast<long>(i));
            negRocSeries.addEntry(NumericTimeSeriesEntry<Num>(timestamp, negativeRocs[i], rocSeries.getTimeFrame()));
        }
        negSkew = RobustSkewMedcouple(negRocSeries);
    }
    
    return LongStatisticsResults(
        profitWidth, stopWidth,
        posMedian, posQn, posSkew,
        negMedian, negSkew,
        positiveRocs.size(), negativeRocs.size()
    );
}

ShortStatisticsResults StatisticsCalculator::computeShortStopAndTarget(
    const OHLCTimeSeries<Num>& series,
    int holdingPeriod) {
    
    using namespace mkc_timeseries;
    
    // Calculate short stop and target using partitioned distributions
    auto [profitWidth, stopWidth] = ComputeShortStopAndTargetFromSeries(series, static_cast<uint32_t>(holdingPeriod));
    
    // Calculate partitioned statistics (following displayStatisticsOnly pattern)
    auto rocSeries = RocSeries(series.CloseTimeSeries(), static_cast<uint32_t>(holdingPeriod));
    auto rocVec = rocSeries.getTimeSeriesAsVector();
    
    const Num zero = DecimalConstants<Num>::DecimalZero;
    std::vector<Num> positiveRocs, negativeRocs;
    for (const auto& roc : rocVec) {
        if (roc > zero) positiveRocs.push_back(roc);
        else if (roc < zero) negativeRocs.push_back(roc);
    }
    
    // Calculate negative partition statistics (primary for short)
    Num negMedian = negativeRocs.empty() ? zero : MedianOfVec(negativeRocs);
    RobustQn<Num> negQnEstimator;
    Num negQn = negativeRocs.empty() ? zero : negQnEstimator.getRobustQn(negativeRocs);
    
    // Calculate positive partition statistics
    Num posMedian = positiveRocs.empty() ? zero : MedianOfVec(positiveRocs);
    
    // Calculate skew for positive and negative partitions
    Num posSkew = zero, negSkew = zero;
    if (positiveRocs.size() >= 3) {
        NumericTimeSeries<Num> posRocSeries(rocSeries.getTimeFrame());
        auto baseTime = boost::posix_time::second_clock::local_time();
        for (size_t i = 0; i < positiveRocs.size(); ++i) {
            auto timestamp = baseTime + boost::posix_time::seconds(static_cast<long>(i));
            posRocSeries.addEntry(NumericTimeSeriesEntry<Num>(timestamp, positiveRocs[i], rocSeries.getTimeFrame()));
        }
        posSkew = RobustSkewMedcouple(posRocSeries);
    }
    if (negativeRocs.size() >= 3) {
        NumericTimeSeries<Num> negRocSeries(rocSeries.getTimeFrame());
        auto baseTime = boost::posix_time::second_clock::local_time();
        for (size_t i = 0; i < negativeRocs.size(); ++i) {
            auto timestamp = baseTime + boost::posix_time::seconds(static_cast<long>(i));
            negRocSeries.addEntry(NumericTimeSeriesEntry<Num>(timestamp, negativeRocs[i], rocSeries.getTimeFrame()));
        }
        negSkew = RobustSkewMedcouple(negRocSeries);
    }
    
    return ShortStatisticsResults(
        profitWidth, stopWidth,
        negMedian, negQn, negSkew,
        posMedian, posSkew,
        negativeRocs.size(), positiveRocs.size()
    );
}