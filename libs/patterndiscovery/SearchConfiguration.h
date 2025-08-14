// SearchConfiguration.h
#ifndef SEARCH_CONFIGURATION_H
#define SEARCH_CONFIGURATION_H

#include <memory>
#include <string>
#include <stdexcept>
#include <utility> // For std::pair

#include <boost/date_time/posix_time/posix_time.hpp>

#include "PerformanceCriteria.h"
#include "Security.h"
#include "TimeFrame.h"
#include "number.h"

/**
 * @brief Exception class for errors related to SearchConfiguration.
 */
class SearchConfigurationException : public std::runtime_error
{
public:
    explicit SearchConfigurationException(const std::string& msg)
        : std::runtime_error(msg) {}

    ~SearchConfigurationException() noexcept override = default;
};

/**
 * @brief Defines the main group of exact patterns to be searched, based on PAL manual.
 *
 * Each type implies a specific range of bar sequence lengths and a specific
 * set of price components (O,H,L,C) to use in the combinatorial search.
 */
enum class SearchType
{
    UNKNOWN,           // Add from palanalyzer
    BASIC,             // Add from palanalyzer
    EXTENDED,          // Keep from patterndiscovery
    DEEP,              // Keep from patterndiscovery
    CLOSE,             // Rename from CLOSE_ONLY (palanalyzer naming)
    HIGH_LOW,          // Rename from HIGH_LOW_ONLY (palanalyzer naming)
    OPEN_CLOSE,        // Rename from OPEN_CLOSE_ONLY (palanalyzer naming)
    MIXED              // Keep from patterndiscovery
};


/**
 * @brief Holds all configuration parameters for a single pattern search run.
 */
template <class DecimalType>
class SearchConfiguration
{
public:
    using SecurityPtr = std::shared_ptr<const mkc_timeseries::Security<DecimalType>>;
    using TimeFrameDuration = mkc_timeseries::TimeFrame::Duration;
    using Ptime = boost::posix_time::ptime;

    /**
     * @brief Constructs a SearchConfiguration object.
     *
     * @param security The financial instrument to search.
     * @param timeFrameDuration The time frame of the data.
     * @param searchType The main group of exact patterns to search for.
     * @param searchForDelayPatterns True to also search for delayed versions of found patterns.
     * @param profitTarget The profit target value.
     * @param stopLoss The stop loss value.
     * @param performanceCriteria The performance filtering criteria.
     * @param backtestStartTime The start timestamp for the backtest period.
     * @param backtestEndTime The end timestamp for the backtest period.
     */
    explicit SearchConfiguration(
        SecurityPtr security,
        TimeFrameDuration timeFrameDuration,
        SearchType searchType,
        bool searchForDelayPatterns,
        DecimalType profitTarget,
        DecimalType stopLoss,
        const PerformanceCriteria<DecimalType>& performanceCriteria,
        Ptime backtestStartTime,
        Ptime backtestEndTime)
    : mSecurity(security),
      mTimeFrameDuration(timeFrameDuration),
      mSearchType(searchType),
      mSearchForDelayPatterns(searchForDelayPatterns),
      mMinDelayBars(searchForDelayPatterns ? 1 : 0),
      mMaxDelayBars(searchForDelayPatterns ? 5 : 0),
      mProfitTarget(profitTarget),
      mStopLoss(stopLoss),
      mPerformanceCriteria(performanceCriteria),
      mBacktestStartTime(backtestStartTime),
      mBacktestEndTime(backtestEndTime)
    {
        if (!mSecurity)
        {
            throw SearchConfigurationException("SearchConfiguration: Security object cannot be null.");
        }
        if (mBacktestStartTime >= mBacktestEndTime)
        {
            throw SearchConfigurationException("SearchConfiguration: Backtest start time must be before end time.");
        }
    }

    ~SearchConfiguration() noexcept = default;

    SecurityPtr getSecurity() const { return mSecurity; }
    TimeFrameDuration getTimeFrameDuration() const { return mTimeFrameDuration; }
    SearchType getSearchType() const { return mSearchType; }
    bool isSearchingForDelayPatterns() const { return mSearchForDelayPatterns; }
    unsigned int getMinDelayBars() const { return mMinDelayBars; }
    unsigned int getMaxDelayBars() const { return mMaxDelayBars; }
    DecimalType getProfitTarget() const { return mProfitTarget; }
    DecimalType getStopLoss() const { return mStopLoss; }
    const PerformanceCriteria<DecimalType>& getPerformanceCriteria() const { return mPerformanceCriteria; }
    Ptime getBacktestStartTime() const { return mBacktestStartTime; }
    Ptime getBacktestEndTime() const { return mBacktestEndTime; }

    /**
     * @brief Gets the min and max pattern sequence length for the configured search type.
     * @return A pair where first is min length and second is max length.
     */
    std::pair<unsigned int, unsigned int> getPatternLengthRange() const
    {
        switch (mSearchType)
        {
            case SearchType::UNKNOWN:         return {2, 9}; // Default range
            case SearchType::BASIC:           return {2, 4}; // Basic patterns
            case SearchType::EXTENDED:        return {2, 6};
            case SearchType::DEEP:            return {2, 9};
            case SearchType::CLOSE:           return {3, 9}; // Renamed from CLOSE_ONLY
            case SearchType::MIXED:           return {2, 9};
            case SearchType::HIGH_LOW:        return {3, 9}; // Renamed from HIGH_LOW_ONLY
            case SearchType::OPEN_CLOSE:      return {3, 9}; // Renamed from OPEN_CLOSE_ONLY
        }
        // Should be unreachable
        throw SearchConfigurationException("Unknown SearchType in getPatternLengthRange.");
    }

private:
    SecurityPtr mSecurity;
    TimeFrameDuration mTimeFrameDuration;
    SearchType mSearchType;
    bool mSearchForDelayPatterns;
    unsigned int mMinDelayBars;
    unsigned int mMaxDelayBars;
    DecimalType mProfitTarget;
    DecimalType mStopLoss;
    PerformanceCriteria<DecimalType> mPerformanceCriteria;
    Ptime mBacktestStartTime;
    Ptime mBacktestEndTime;
};

template class SearchConfiguration<num::DefaultNumber>;

#endif // SEARCH_CONFIGURATION_H
