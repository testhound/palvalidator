#ifndef TIMESERIESVALIDATOR_H
#define TIMESERIESVALIDATOR_H

#include "TimeSeries.h"


using boost::posix_time::time_duration;

namespace mkc_timeseries
{
    class TimeSeriesValidationException : public std::runtime_error
    {
    public:
        TimeSeriesValidationException(const std::string msg)
            : std::runtime_error(msg)
        {}

        ~TimeSeriesValidationException()
        {}
    }; 

    template <class Decimal>
    class TimeSeriesValidator
    {
        public:
            TimeSeriesValidator(
                std::shared_ptr<OHLCTimeSeries<Decimal>> hourlyTimeSeries, 
                std::shared_ptr<OHLCTimeSeries<Decimal>> dailyTimeSeries, 
                int numberTimeFrames) :
            mHourlyTimeSeries(hourlyTimeSeries), 
            mDailyTimeSeries(dailyTimeSeries), 
            numberTimeFrames(numberTimeFrames)
            {}

            void validate() 
            {
                mValidateNumberTimePeriods();
                mValidateTimeStamps(); 
                mValidateAvailableDays();
            }

        private:
            std::shared_ptr<OHLCTimeSeries<Decimal>> mHourlyTimeSeries;
            std::shared_ptr<OHLCTimeSeries<Decimal>> mDailyTimeSeries;
            int numberTimeFrames;

            void mValidateTimeStamps() // warning
            {
                boost::posix_time::time_duration typicalStartTime = boost::posix_time::hours(9);
                boost::posix_time::time_duration typicalEndTime = boost::posix_time::hours(15);
                for(auto it = mHourlyTimeSeries->beginRandomAccess(); it != mHourlyTimeSeries->endRandomAccess(); it++)
                {
                    if(it->getBarTime() < typicalStartTime || it->getBarTime() > typicalEndTime)
                        std::cout << "WARNING: Bar time " << it->getBarTime() << " is out of the typical trading time range (9:00 - 15:00)" << std::endl;
                }
            }

            void mValidateNumberTimePeriods() // error 
            {
                boost::gregorian::date startDate = mHourlyTimeSeries->beginRandomAccess()->getDateValue();
                std::vector<boost::posix_time::time_duration> distinctTimeDurations;
                int completeDayCount = 0, holidayCount = 0, numberDays = 0;
                std::vector<boost::gregorian::date> daysToDelete;
                bool validateTimeDurations = true;
                for(auto it = mHourlyTimeSeries->beginRandomAccess(); it != mHourlyTimeSeries->endRandomAccess(); it++)
                {
                    if(it->getDateValue() != startDate) 
                    {
                        if(!isEarlyCloseDay(startDate))
                        {
                            if(distinctTimeDurations.size() == numberTimeFrames)    // distinct time frames per day
                                completeDayCount++;
                            
                            if(distinctTimeDurations.size() < numberTimeFrames)    // remove the entry 
                            {
                                std::cout << "WARNING: " << startDate << " contained " << distinctTimeDurations.size() 
                                          << " bars. Removing the date from the hourly and daily time series." << std::endl;
                                daysToDelete.push_back(startDate);
                                validateTimeDurations = false;
                            }

                            // time durations are 1 hour apart for each day
                            if(validateTimeDurations)
                            {
                                for(std::size_t i = 0; i < distinctTimeDurations.size() - 1; i++)
                                {
                                    boost::posix_time::time_duration start = distinctTimeDurations.at(i);
                                    boost::posix_time::time_duration end = distinctTimeDurations.at(i+1);
                                    if((end-start) != boost::posix_time::hours(1))
                                        throw TimeSeriesValidationException("ERROR: Time frames are not one hour apart on " + boost::gregorian::to_simple_string(startDate));
                                }
                            }
                        } 
                        else
                        {
                            holidayCount++;
                        }

                        numberDays++;
                        startDate = it->getDateValue();
                        distinctTimeDurations.clear();
                        validateTimeDurations = true;
                    }

                    if(std::find(distinctTimeDurations.begin(), distinctTimeDurations.end(), it->getBarTime()) == distinctTimeDurations.end()) 
                        distinctTimeDurations.push_back(it->getBarTime());
                }

                float completeDayPercent = ((float)completeDayCount/((float)(numberDays - holidayCount)));
                if (completeDayPercent < 0.99) 
                    throw TimeSeriesValidationException("ERROR: Not enough days in the hourly time series had " + std::to_string(numberTimeFrames) + " bars. Expected: at least 99% Found: " + std::to_string(completeDayPercent));
                if(completeDayPercent < 1)
                    std::cout << "WARNING: only " << completeDayPercent << " of non-holiday trading days in the hourly time series had " + std::to_string(numberTimeFrames) + "hourly bars." << std::endl;

                // remove the dates if we get this far 
                for(auto it = daysToDelete.begin(); it != daysToDelete.end(); it++)
                {
                    mHourlyTimeSeries->deleteEntryByDate((*it));
                    mDailyTimeSeries->deleteEntryByDate((*it));
                }
            }

            bool isEarlyCloseDay(boost::gregorian::date date)
            {
                typedef boost::gregorian::nth_day_of_the_week_in_month nth_dow;

                // Christmas
                if( 
                    (date.month() == 12 && date.day() == 24) ||     // weekeday => closed early Christmas Eve
                    (date.month() == 12 && date.day() == 23 && date.day_of_week() == boost::date_time::Thursday)    // Saturday Christmas => Observed Friday, Early Closure Thursday
                    // no early closure if Christmas is on Sunday
                )
                {
                    return true;
                }

                // Thanksgiving
                if((date.month() == 11 && date.day_of_week() == boost::date_time::Friday && date.day() > 20)) // potential of being after 4th Thursday in November
                {
                    nth_dow fourthThursday(nth_dow::fourth, boost::date_time::Thursday, boost::date_time::Nov);
                    return (fourthThursday.get_date(date.year()) == (date - boost::gregorian::days(1))); // Friday after 4th Thursday
                }

                // Independence day
                if(
                    (date.month() == 7 && date.day() == 3) || // Independence day is on a weekday
                    (date.month() == 7 && date.day() == 2 && date.day_of_week() == boost::date_time::Thursday)  // Independence day is on Saturday
                )
                {
                    return true;
                }

                return false;
            }

            void mValidateAvailableDays() // error 
            {                
                // ensure hourly time series days are in daily time series 
                for(auto it = mHourlyTimeSeries->beginRandomAccess(); it != mHourlyTimeSeries->endRandomAccess(); it++)
                    if(!dateInSeries(mDailyTimeSeries, it->getDateValue()))
                        throw TimeSeriesValidationException("ERROR: " + boost::gregorian::to_simple_string(it->getDateValue()) + " not found in the daily time series.");

                // ensure daily time series days are in hourly time series
                for(auto it = mDailyTimeSeries->beginRandomAccess(); it != mDailyTimeSeries->endRandomAccess(); it++) 
                    if(!dateInSeries(mHourlyTimeSeries, it->getDateValue()))
                        throw TimeSeriesValidationException("ERROR: " + boost::gregorian::to_simple_string(it->getDateValue()) + " not found in the hourly time series.");
            }
            
            bool dateInSeries(std::shared_ptr<OHLCTimeSeries<Decimal>> timeSeries, boost::gregorian::date date) 
            {
                for(auto it = timeSeries->beginRandomAccess(); it != timeSeries->endRandomAccess(); it++)
                    if(it->getDateValue() == date) 
                        return true;
                return false;
            }
    }; 
}

#endif
