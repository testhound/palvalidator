#ifndef SYNTHETICTIMESERIESCREATOR_H
#define SYNTHETICTIMESERIESCREATOR_H

#include "TimeSeriesCsvWriter.h"
#include "TimeSeries.h"
#include "McptConfigurationFileReader.h"
#include "TimeSeriesCsvReader.h"
#include <map>
#include <cassert>

using boost::posix_time::time_duration;

namespace mkc_timeseries
{
    template <class Decimal>
    class SyntheticTimeSeriesCreator
    {
    public:
        typedef typename std::map<int, std::shared_ptr<OHLCTimeSeries<Decimal>>> SyntheticTimeSeriesMap;

        SyntheticTimeSeriesCreator(std::shared_ptr<OHLCTimeSeries<Decimal>> timeSeries) : 
            mOriginalHourlyTimeSeries(timeSeries),
            mTimeSeriesMap(),
            mPartialDayMap(),
            mEntryDate() // Constructs date with value not_a_date_time
        {
        }

        void createSyntheticTimeSeries(int timeFrameId, time_duration filterTime)
        {
            std::shared_ptr<OHLCTimeSeries<Decimal>> syntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
                TimeFrame::DAILY,
                mOriginalHourlyTimeSeries->getVolumeUnits());

            boost::gregorian::date dailyDate(mOriginalHourlyTimeSeries->getFirstDate());
            boost::gregorian::date intradayDate(dailyDate);
            time_duration partialDayFilterTime;
            bool partialDayFound = false;
            int numPartialDays = 0;

            for (auto it = mOriginalHourlyTimeSeries->beginRandomAccess(); it != mOriginalHourlyTimeSeries->endRandomAccess(); it++)
            {
                intradayDate = it->getDateValue();
                if (intradayDate != dailyDate)
                {
                    dailyDate = intradayDate;
                    // Check for partial days
                    boost::posix_time::ptime newDateWithTimeFilter(intradayDate, filterTime);

                    typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it2 =
                        mOriginalHourlyTimeSeries->getTimeSeriesEntry(newDateWithTimeFilter);

                    if (it2 == mOriginalHourlyTimeSeries->endSortedAccess())
                    {
                        // We found a partial day
                        partialDayFilterTime = getLastTimeForPartialDay(it, mOriginalHourlyTimeSeries->endRandomAccess(), intradayDate);
                        partialDayFound = true;
                        numPartialDays++;
                    }
                }
                // this 'if' prevents the last bar for the first time value from being added to the time series
                // since it won't be added until the next date is found, i.e. if the file ends on 12/29/2020
                // the 12/29/2020 9:00 bar will be missing since it won't be added until the values are aggregated
                // and the 9:00 timestamp is found again on 12/30/2020.

                if (filterTime == it->getBarTime() || ((partialDayFound == true) && (partialDayFilterTime == it->getBarTime())))
                {
                    if (mOpen != DecimalConstants<Decimal>::DecimalZero)
                    {
                        syntheticTimeSeries->addEntry(OHLCTimeSeriesEntry<Decimal>(mEntryDate,
                                                                                   mOpen,
                                                                                   mHigh,
                                                                                   mLow,
                                                                                   mClose,
                                                                                   it->getVolumeValue(),
                                                                                   TimeFrame::DAILY));
                    }

                    if (!partialDayFound)
                        mOpen = it->getOpenValue();
                    else
                        mOpen = it->getCloseValue();

                    mHigh = DecimalConstants<Decimal>::DecimalZero;
                    mLow = it->getHighValue() * DecimalConstants<Decimal>::DecimalOneHundred;
                    mEntryDate = it->getDateValue();
                    partialDayFound = false;
                }

                if (it->getHighValue() > mHigh)
                    mHigh = it->getHighValue();
                if (it->getLowValue() < mLow)
                    mLow = it->getLowValue();

                mClose = it->getCloseValue();
            }

            // need to add the missing bar for the first time frame, the aggregated values should be set
            // correctly for this bar -- tested this on KC and MSFT data to make sure
            if (timeFrameId == 1)
                syntheticTimeSeries->addEntry(OHLCTimeSeriesEntry<Decimal>(
                    mEntryDate, mOpen, mHigh, mLow, mClose, mOriginalHourlyTimeSeries->getVolumeValue(mOriginalHourlyTimeSeries->beginRandomAccess(), 0), TimeFrame::DAILY));

            mTimeSeriesMap.insert(std::make_pair(timeFrameId, syntheticTimeSeries));
            mPartialDayMap.insert(std::make_pair(timeFrameId, numPartialDays));
        }

        std::shared_ptr<OHLCTimeSeries<Decimal>> getSyntheticTimeSeries(int timeFrameId)
        {
            return mTimeSeriesMap.at(timeFrameId);
        }

        int getNumPartialDays(int timeFrameId) const
        {
            return mPartialDayMap.at(timeFrameId);
        }

/*
        void writeTimeFrameFile(int timeFrameId)
        {
            std::shared_ptr<OHLCTimeSeries<Decimal>> series = mTimeSeriesMap.at(timeFrameId);
            std::string timeFrameFilename = getTimeFrameFilename(timeFrameId);
            PalTimeSeriesCsvWriter<Decimal> csvWriter(timeFrameFilename, *series);
            csvWriter.writeFile();
        }
*/
        SyntheticTimeSeriesMap getSyntheticTimeSeriesMap() { return mTimeSeriesMap; }

    private:
        std::shared_ptr<OHLCTimeSeries<Decimal>> mOriginalHourlyTimeSeries;
        SyntheticTimeSeriesMap mTimeSeriesMap;
        std::map<int, int> mPartialDayMap;
        Decimal mOpen = DecimalConstants<Decimal>::DecimalZero;
        Decimal mHigh = DecimalConstants<Decimal>::DecimalZero;
        Decimal mLow = DecimalConstants<Decimal>::DecimalZero;
        Decimal mClose = DecimalConstants<Decimal>::DecimalZero;
        boost::gregorian::date mEntryDate;

        time_duration getLastTimeForPartialDay(typename OHLCTimeSeries<Decimal>::RandomAccessIterator partialDayIt,
                                               typename OHLCTimeSeries<Decimal>::RandomAccessIterator endIt,
                                               boost::gregorian::date intradayDate)
        {
            boost::gregorian::date currentDate(intradayDate);
            time_duration lastTimeStamp;

            assert(partialDayIt != endIt);
            assert(partialDayIt->getDateValue() == currentDate);

            lastTimeStamp = partialDayIt->getBarTime();

            while (partialDayIt != endIt)
            {
                if (partialDayIt->getDateValue() != currentDate)
                {
                    return lastTimeStamp;
                }
                else
                {
                    lastTimeStamp = partialDayIt->getBarTime();
                    partialDayIt++;
                }
            }

            return lastTimeStamp;
        }

/*
        std::string getTimeFrameFilename(int timeFrameId)
        {
            return std::string(mFilename + std::string("_timeframe_") + std::to_string(timeFrameId));
        }
*/
    };
}

#endif
