#ifndef TIMEFRAMEDISCOVERY_H
#define TIMEFRAMEDISCOVERY_H

#include "TimeSeries.h"
#include "IntradayIntervalCalculator.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <map>
#include <set>
#include <stdexcept>

using boost::posix_time::time_duration;
using boost::posix_time::ptime;

namespace mkc_timeseries
{
  class TimeFrameDiscoveryException : public std::runtime_error
  {
  public:
    TimeFrameDiscoveryException(const std::string& msg)
      : std::runtime_error(msg)
    {}
  };

  template <class Decimal>
  class TimeFrameDiscovery
  {
  public:
    typedef typename std::vector<time_duration> TimeFrameCollection;
    typedef typename TimeFrameCollection::const_iterator TimeFrameIterator;
    typedef typename std::map<boost::gregorian::date, std::vector<ptime>> DailyTimestampsMap;
    typedef typename std::set<ptime> TimestampSet;

    TimeFrameDiscovery(std::shared_ptr<OHLCTimeSeries<Decimal>> timeSeries) :
      mTimeFrames(),
      mTimeSeries(timeSeries),
      mTimestampsByDate(),
      mAllTimestamps(),
      mDiscovered(false)
    {}

    /**
     * @brief Discovers the actual timeframes and timestamps from the data
     *
     * This method analyzes the actual timestamps in the time series to:
     * 1. Extract all unique timestamps
     * 2. Group timestamps by trading day
     * 3. Determine the actual time intervals used
     * 4. Build lookup structures for previous/next timestamp calculation
     */
    void inferTimeFrames()
    {
      if (mDiscovered) return;
      
      mTimeFrames.clear();
      mTimestampsByDate.clear();
      mAllTimestamps.clear();
      
      if (!mTimeSeries || mTimeSeries->getNumEntries() == 0) {
        throw TimeFrameDiscoveryException("Cannot infer timeframes from empty time series");
      }

      // Collect all timestamps and group by date
      for (auto it = mTimeSeries->beginRandomAccess(); it != mTimeSeries->endRandomAccess(); ++it) {
        ptime timestamp = it->getDateTime();
        boost::gregorian::date date = timestamp.date();
        
        mTimestampsByDate[date].push_back(timestamp);
        mAllTimestamps.insert(timestamp);
      }

      // Sort timestamps within each day
      for (auto& dayEntry : mTimestampsByDate) {
        std::sort(dayEntry.second.begin(), dayEntry.second.end());
      }

      // Extract unique time-of-day patterns by analyzing all days to find the most complete pattern
      if (!mTimestampsByDate.empty()) {
        // Collect all unique time-of-day values across all days
        std::set<time_duration> allTimeOfDays;
        
        for (const auto& dayEntry : mTimestampsByDate) {
          for (const auto& timestamp : dayEntry.second) {
            allTimeOfDays.insert(timestamp.time_of_day());
          }
        }
        
        // Convert to vector and sort
        mTimeFrames.assign(allTimeOfDays.begin(), allTimeOfDays.end());
        std::sort(mTimeFrames.begin(), mTimeFrames.end());
      }

      mDiscovered = true;
    }

    /**
     * @brief Get the previous timestamp that exists in the actual data
     * @param current The current timestamp
     * @return The previous timestamp, or boost::posix_time::not_a_date_time if none exists
     */
    ptime getPreviousTimestamp(const ptime& current) const
    {
      if (!mDiscovered) {
        throw TimeFrameDiscoveryException("Must call inferTimeFrames() before using getPreviousTimestamp()");
      }

      auto it = mAllTimestamps.find(current);
      if (it == mAllTimestamps.end()) {
        // Current timestamp not found in data, find the largest timestamp less than current
        it = mAllTimestamps.lower_bound(current);
        if (it == mAllTimestamps.begin()) {
          return ptime(); // not_a_date_time
        }
        --it;
        return *it;
      }
      
      // Current timestamp found, get the previous one
      if (it == mAllTimestamps.begin()) {
        return ptime(); // not_a_date_time
      }
      --it;
      return *it;
    }

    /**
     * @brief Get the next timestamp that exists in the actual data
     * @param current The current timestamp
     * @return The next timestamp, or boost::posix_time::not_a_date_time if none exists
     */
    ptime getNextTimestamp(const ptime& current) const
    {
      if (!mDiscovered) {
        throw TimeFrameDiscoveryException("Must call inferTimeFrames() before using getNextTimestamp()");
      }

      auto it = mAllTimestamps.find(current);
      if (it == mAllTimestamps.end()) {
        // Current timestamp not found in data, find the smallest timestamp greater than current
        it = mAllTimestamps.upper_bound(current);
        if (it == mAllTimestamps.end()) {
          return ptime(); // not_a_date_time
        }
        return *it;
      }
      
      // Current timestamp found, get the next one
      ++it;
      if (it == mAllTimestamps.end()) {
        return ptime(); // not_a_date_time
      }
      return *it;
    }

    /**
     * @brief Get the most common time interval between consecutive bars
     * @return The predominant time interval
     */
    time_duration getCommonInterval() const
    {
      if (!mDiscovered) {
        throw TimeFrameDiscoveryException("Must call inferTimeFrames() before using getCommonInterval()");
      }

      if (mTimeSeries->getTimeFrame() == TimeFrame::INTRADAY) {
        // Use the existing IntradayIntervalCalculator for intraday data
        return mTimeSeries->getIntradayTimeFrameDuration();
      }

      // For non-intraday, use IntradayIntervalCalculator with all timestamps
      std::vector<ptime> allTimestamps(mAllTimestamps.begin(), mAllTimestamps.end());
      if (allTimestamps.size() < 2) {
        throw TimeFrameDiscoveryException("Insufficient timestamps to calculate interval");
      }

      return IntradayIntervalCalculator::calculateMostCommonInterval(allTimestamps);
    }

    /**
     * @brief Check if a timestamp exists in the actual data
     * @param timestamp The timestamp to check
     * @return true if the timestamp exists in the data
     */
    bool hasTimestamp(const ptime& timestamp) const
    {
      if (!mDiscovered) {
        throw TimeFrameDiscoveryException("Must call inferTimeFrames() before using hasTimestamp()");
      }
      return mAllTimestamps.find(timestamp) != mAllTimestamps.end();
    }

    /**
     * @brief Get all timestamps for a specific date
     * @param date The date to get timestamps for
     * @return Vector of timestamps for that date, empty if date not found
     */
    std::vector<ptime> getTimestampsForDate(const boost::gregorian::date& date) const
    {
      if (!mDiscovered) {
        throw TimeFrameDiscoveryException("Must call inferTimeFrames() before using getTimestampsForDate()");
      }
      
      auto it = mTimestampsByDate.find(date);
      if (it != mTimestampsByDate.end()) {
        return it->second;
      }
      return std::vector<ptime>();
    }

    // Legacy interface for backward compatibility
    time_duration getTimeFrame(int position)
    {
      if (!mDiscovered) {
        throw TimeFrameDiscoveryException("Must call inferTimeFrames() before using getTimeFrame()");
      }
      if ((TimeFrameCollection::size_type) position >= mTimeFrames.size()) {
        throw TimeFrameDiscoveryException("Timeframe does not exist: id=" + std::to_string(position) +
                                        " number of time frames=" + std::to_string(mTimeFrames.size()));
      }
      return mTimeFrames.at(position);
    }

    int numTimeFrames() const {
      return static_cast<int>(mTimeFrames.size());
    }
    
    TimeFrameIterator getTimeFramesBegin() const {
      return mTimeFrames.begin();
    }
    
    TimeFrameIterator getTimeFramesEnd() const {
      return mTimeFrames.end();
    }
    
    TimeFrameCollection getTimeFrames() const {
      return mTimeFrames;
    }

    bool isDiscovered() const {
      return mDiscovered;
    }

  private:
    std::vector<time_duration> mTimeFrames;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mTimeSeries;
    DailyTimestampsMap mTimestampsByDate;
    TimestampSet mAllTimestamps;
    bool mDiscovered;
  };
}

#endif
