// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __MARKET_HOURS_H
#define __MARKET_HOURS_H 1

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace mkc_timeseries
{
    /**
     * @brief Simple market hours interface for future extension.
     * 
     * This abstract base class defines the interface for market hours validation
     * and trading time generation. It can be extended for different markets
     * (equities, futures, forex) with their specific trading schedules.
     */
    class MarketHours {
    public:
        virtual ~MarketHours() = default;
        
        /**
         * @brief Check if the market is open at the specified datetime.
         * @param dateTime The datetime to check
         * @return true if market is open, false otherwise
         */
        virtual bool isMarketOpen(const boost::posix_time::ptime& dateTime) const = 0;
        
        /**
         * @brief Get the next valid trading time from a given datetime.
         * @param from Starting datetime
         * @param interval Time interval to add
         * @return Next valid trading datetime
         */
        virtual boost::posix_time::ptime getNextTradingTime(
            const boost::posix_time::ptime& from,
            boost::posix_time::time_duration interval) const = 0;
            
        /**
         * @brief Get the next valid trading time using discovered timeframes.
         * @param from Starting datetime
         * @param timeFrames Vector of time-of-day values that represent valid trading times
         * @return Next valid trading datetime
         */
        virtual boost::posix_time::ptime getNextTradingTimeFromFrames(
            const boost::posix_time::ptime& from,
            const std::vector<boost::posix_time::time_duration>& timeFrames) const = 0;
            
        /**
         * @brief Get the previous valid trading time using discovered timeframes.
         * @param from Starting datetime
         * @param timeFrames Vector of time-of-day values that represent valid trading times
         * @return Previous valid trading datetime
         */
        virtual boost::posix_time::ptime getPreviousTradingTimeFromFrames(
            const boost::posix_time::ptime& from,
            const std::vector<boost::posix_time::time_duration>& timeFrames) const = 0;
    };

    /**
     * @brief Built-in US Equities implementation (9:30 AM - 4:00 PM ET, Mon-Fri).
     * 
     * This class provides zero-setup market hours for US equity markets.
     * It handles standard trading hours and automatically skips weekends.
     * Holidays are not currently handled but could be added in the future.
     */
    class USEquitiesMarketHours : public MarketHours {
    private:
        static constexpr int MARKET_OPEN_HOUR = 9;
        static constexpr int MARKET_OPEN_MINUTE = 30;
        static constexpr int MARKET_CLOSE_HOUR = 16;
        static constexpr int MARKET_CLOSE_MINUTE = 0;
        
    public:
        /**
         * @brief Check if US equity market is open at the specified datetime.
         * @param dateTime The datetime to check
         * @return true if market is open (9:30 AM - 4:00 PM ET, Mon-Fri), false otherwise
         */
        bool isMarketOpen(const boost::posix_time::ptime& dateTime) const override {
            auto weekday = dateTime.date().day_of_week();
            if (weekday == boost::gregorian::Saturday || weekday == boost::gregorian::Sunday) {
                return false;
            }
            
            auto time_of_day = dateTime.time_of_day();
            auto market_open = boost::posix_time::time_duration(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE, 0);
            auto market_close = boost::posix_time::time_duration(MARKET_CLOSE_HOUR, MARKET_CLOSE_MINUTE, 0);
            
            return time_of_day >= market_open && time_of_day < market_close;
        }
        
        /**
         * @brief Get the next valid US equity trading time.
         * @param from Starting datetime
         * @param interval Time interval to add
         * @return Next valid trading datetime, skipping to market open if necessary
         */
        boost::posix_time::ptime getNextTradingTime(
            const boost::posix_time::ptime& from,
            boost::posix_time::time_duration interval) const override {
            
            auto next = from + interval;
            
            // Skip to next trading day if needed
            while (!isMarketOpen(next))
	      {
                if (next.time_of_day() >= boost::posix_time::time_duration(MARKET_CLOSE_HOUR, 0, 0)) {
		  // After market close, jump to next day's open
		  next = boost::posix_time::ptime(next.date() + boost::gregorian::days(1),
                                                  boost::posix_time::time_duration(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE, 0));
                }
		else
		  {
                    // Before market open, jump to today's open
                    next = boost::posix_time::ptime(next.date(),
						    boost::posix_time::time_duration(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE, 0));
		  }
                
                // Check if we landed on a weekend and need to advance to Monday
                auto weekday = next.date().day_of_week();
                if (weekday == boost::gregorian::Saturday)
		  {
                    next = boost::posix_time::ptime(next.date() + boost::gregorian::days(2),
						    boost::posix_time::time_duration(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE, 0));
		  }
		else if (weekday == boost::gregorian::Sunday)
		  {
                    next = boost::posix_time::ptime(next.date() + boost::gregorian::days(1),
						    boost::posix_time::time_duration(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE, 0));
		  }
	      }
            
            return next;
        }
        
        /**
         * @brief Get the next valid US equity trading time using discovered timeframes.
         * @param from Starting datetime
         * @param timeFrames Vector of time-of-day values that represent valid trading times
         * @return Next valid trading datetime, or not_a_date_time if none found
         */
        boost::posix_time::ptime getNextTradingTimeFromFrames(
            const boost::posix_time::ptime& from,
            const std::vector<boost::posix_time::time_duration>& timeFrames) const override {
            
            if (timeFrames.empty()) {
                return boost::posix_time::ptime(); // not_a_date_time
            }
            
            auto currentDate = from.date();
            auto currentTime = from.time_of_day();
            
            // First, try to find the next timeframe on the same day
            for (const auto& timeFrame : timeFrames) {
                if (timeFrame > currentTime) {
                    auto candidate = boost::posix_time::ptime(currentDate, timeFrame);
                    if (isMarketOpen(candidate)) {
                        return candidate;
                    }
                }
            }
            
            // If no timeframe found on current day, move to next trading day
            auto nextDate = currentDate + boost::gregorian::days(1);
            
            // Skip weekends
            while (nextDate.day_of_week() == boost::gregorian::Saturday ||
                   nextDate.day_of_week() == boost::gregorian::Sunday) {
                nextDate += boost::gregorian::days(1);
            }
            
            // Return the first timeframe of the next trading day
            if (!timeFrames.empty()) {
                auto candidate = boost::posix_time::ptime(nextDate, timeFrames.front());
                if (isMarketOpen(candidate)) {
                    return candidate;
                }
            }
            
            return boost::posix_time::ptime(); // not_a_date_time
        }
        
        /**
         * @brief Get the previous valid US equity trading time using discovered timeframes.
         * @param from Starting datetime
         * @param timeFrames Vector of time-of-day values that represent valid trading times
         * @return Previous valid trading datetime, or not_a_date_time if none found
         */
        boost::posix_time::ptime getPreviousTradingTimeFromFrames(
            const boost::posix_time::ptime& from,
            const std::vector<boost::posix_time::time_duration>& timeFrames) const override {
            
            if (timeFrames.empty()) {
                return boost::posix_time::ptime(); // not_a_date_time
            }
            
            auto currentDate = from.date();
            auto currentTime = from.time_of_day();
            
            // First, try to find the previous timeframe on the same day (search backwards)
            for (auto it = timeFrames.rbegin(); it != timeFrames.rend(); ++it) {
                if (*it < currentTime) {
                    auto candidate = boost::posix_time::ptime(currentDate, *it);
                    if (isMarketOpen(candidate)) {
                        return candidate;
                    }
                }
            }
            
            // If no timeframe found on current day, move to previous trading day
            auto prevDate = currentDate - boost::gregorian::days(1);
            
            // Skip weekends
            while (prevDate.day_of_week() == boost::gregorian::Saturday ||
                   prevDate.day_of_week() == boost::gregorian::Sunday) {
                prevDate -= boost::gregorian::days(1);
            }
            
            // Return the last timeframe of the previous trading day
            if (!timeFrames.empty()) {
                auto candidate = boost::posix_time::ptime(prevDate, timeFrames.back());
                if (isMarketOpen(candidate)) {
                    return candidate;
                }
            }
            
            return boost::posix_time::ptime(); // not_a_date_time
        }
    };

} // End namespace mkc_timeseries

#endif // __MARKET_HOURS_H
