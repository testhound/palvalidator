// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __INTRADAY_INTERVAL_CALCULATOR_H
#define __INTRADAY_INTERVAL_CALCULATOR_H 1

#include <vector>
#include <map>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/container/flat_map.hpp>
#include "TimeSeriesException.h"

namespace mkc_timeseries
{
    /**
     * @brief Utility class for calculating the most common time interval in intraday time series data.
     *
     * This class provides static methods to analyze collections of timestamps and determine
     * the predominant time interval, handling irregular gaps due to holidays or early market closures.
     */
    class IntradayIntervalCalculator
    {
    public:
        /**
         * @brief Calculate the most common time interval from a vector of timestamps.
         * 
         * @tparam TimestampContainer Container type that provides iterators to timestamps
         * @param timestamps Container of boost::posix_time::ptime objects in chronological order
         * @return boost::posix_time::time_duration representing the most common interval
         * @throws TimeSeriesException if no valid intervals are found
         */
        template<typename TimestampContainer>
        static boost::posix_time::time_duration calculateMostCommonInterval(const TimestampContainer& timestamps)
        {
            if (timestamps.size() < 2)
            {
                throw TimeSeriesException("IntradayIntervalCalculator: Need at least 2 timestamps");
            }
            
            std::map<boost::posix_time::time_duration, unsigned int> intervalCounts;
            
            auto it = timestamps.begin();
            auto prev_it = it++;
            
            for (; it != timestamps.end(); ++it, ++prev_it)
            {
                auto timeDiff = getTimestamp(*it) - getTimestamp(*prev_it);
                
                if (!timeDiff.is_negative() && !timeDiff.is_zero())
                {
                    intervalCounts[timeDiff]++;
                }
            }
            
            if (intervalCounts.empty())
            {
                throw TimeSeriesException("IntradayIntervalCalculator: No valid intervals found");
            }
            
            // Find the most common interval (mode)
            auto maxElement = std::max_element(intervalCounts.begin(), intervalCounts.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });
            
            return maxElement->first;
        }
        
        /**
         * @brief Calculate the most common time interval from OHLC time series entries.
         * 
         * @tparam Decimal The numeric type used in the OHLC entries
         * @param entries Vector of OHLCTimeSeriesEntry objects
         * @return boost::posix_time::time_duration representing the most common interval
         */
        template<typename Decimal>
        static boost::posix_time::time_duration calculateFromOHLCEntries(
            const std::vector<OHLCTimeSeriesEntry<Decimal>>& entries)
        {
            return calculateMostCommonInterval(entries);
        }
        
        /**
         * @brief Calculate the most common time interval from a sorted map of timestamps.
         * 
         * @tparam ValueType The value type stored in the map
         * @param sortedMap Map with ptime keys in chronological order
         * @return boost::posix_time::time_duration representing the most common interval
         */
        template<typename ValueType>
        static boost::posix_time::time_duration calculateFromSortedMap(
            const std::map<boost::posix_time::ptime, ValueType>& sortedMap)
        {
            std::vector<boost::posix_time::ptime> timestamps;
            timestamps.reserve(sortedMap.size());
            
            for (const auto& pair : sortedMap)
            {
                timestamps.push_back(pair.first);
            }
            
            return calculateMostCommonInterval(timestamps);
        }
        
        /**
         * @brief Calculate the most common time interval from a sorted flat_map of timestamps.
         *
         * @tparam ValueType The value type stored in the flat_map
         * @param sortedMap flat_map with ptime keys in chronological order
         * @return boost::posix_time::time_duration representing the most common interval
         */
        template<typename ValueType>
        static boost::posix_time::time_duration calculateFromSortedMap(
            const boost::container::flat_map<boost::posix_time::ptime, ValueType>& sortedMap)
        {
            std::vector<boost::posix_time::ptime> timestamps;
            timestamps.reserve(sortedMap.size());
            
            for (const auto& pair : sortedMap)
            {
                timestamps.push_back(pair.first);
            }
            
            return calculateMostCommonInterval(timestamps);
        }

        /**
         * @brief Calculate the most common time interval in minutes from a vector of timestamps.
         *
         * @tparam TimestampContainer Container type that provides iterators to timestamps
         * @param timestamps Container of boost::posix_time::ptime objects in chronological order
         * @return long representing the most common interval in minutes
         * @throws TimeSeriesException if no valid intervals are found
         */
        template<typename TimestampContainer>
        static long calculateMostCommonIntervalInMinutes(const TimestampContainer& timestamps)
        {
            auto duration = calculateMostCommonInterval(timestamps);
            return duration.total_seconds() / 60;
        }

        /**
         * @brief Calculate the most common time interval in minutes from OHLC time series entries.
         *
         * @tparam Decimal The numeric type used in the OHLC entries
         * @param entries Vector of OHLCTimeSeriesEntry objects
         * @return long representing the most common interval in minutes
         */
        template<typename Decimal>
        static long calculateFromOHLCEntriesInMinutes(
            const std::vector<OHLCTimeSeriesEntry<Decimal>>& entries)
        {
            return calculateMostCommonIntervalInMinutes(entries);
        }

        /**
         * @brief Calculate the most common time interval in minutes from a sorted map of timestamps.
         *
         * @tparam ValueType The value type stored in the map
         * @param sortedMap Map with ptime keys in chronological order
         * @return long representing the most common interval in minutes
         */
        template<typename ValueType>
        static long calculateFromSortedMapInMinutes(
            const std::map<boost::posix_time::ptime, ValueType>& sortedMap)
        {
            auto duration = calculateFromSortedMap(sortedMap);
            return duration.total_seconds() / 60;
        }

        /**
         * @brief Calculate the most common time interval in minutes from a sorted flat_map of timestamps.
         *
         * @tparam ValueType The value type stored in the flat_map
         * @param sortedMap flat_map with ptime keys in chronological order
         * @return long representing the most common interval in minutes
         */
        template<typename ValueType>
        static long calculateFromSortedMapInMinutes(
            const boost::container::flat_map<boost::posix_time::ptime, ValueType>& sortedMap)
        {
            auto duration = calculateFromSortedMap(sortedMap);
            return duration.total_seconds() / 60;
        }
        
    private:
        // Helper function to extract timestamp from different types
        template<typename T>
        static boost::posix_time::ptime getTimestamp(const T& item)
        {
            if constexpr (std::is_same_v<T, boost::posix_time::ptime>)
            {
                return item;
            }
            else
            {
                return item.getDateTime(); // For OHLC entries and similar
            }
        }
    };
}

#endif // __INTRADAY_INTERVAL_CALCULATOR_H