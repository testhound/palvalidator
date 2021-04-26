#ifndef SYNTHETICTIMESERIESCREATOR_H
#define SYNTHETICTIMESERIESCREATOR_H

#include "TimeSeriesCsvWriter.h"
#include "TimeSeries.h"
#include "McptConfigurationFileReader.h"
#include "TimeSeriesCsvReader.h"

#include <map>

using boost::posix_time::time_duration;

namespace mkc_timeseries
{
    template <class Decimal>
    class SyntheticTimeSeriesCreator
    {
        public:
            typedef typename std::map<int, std::shared_ptr<OHLCTimeSeries<Decimal>>> SyntheticTimeSeriesMap;

            SyntheticTimeSeriesCreator(std::shared_ptr<OHLCTimeSeries<Decimal>> timeSeries, std::string hourlyDataFilename) : 
                mOriginalHourlyTimeSeries(timeSeries),
                mFilename(hourlyDataFilename)
            {}

            void createSyntheticTimeSeries(int timeFrameId, time_duration filterTime) 
            {
                std::shared_ptr<OHLCTimeSeries<Decimal>> syntheticTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>>(
                    TimeFrame::DAILY, 
                    mOriginalHourlyTimeSeries->getVolumeUnits()
                );

                for(auto it = mOriginalHourlyTimeSeries->beginRandomAccess(); it != mOriginalHourlyTimeSeries->endRandomAccess(); it++)
                {
                    if(filterTime == it->getBarTime()) 
                    {
                        if(mOpen != DecimalConstants<Decimal>::DecimalZero)
                        {
                            syntheticTimeSeries->addEntry(OHLCTimeSeriesEntry<Decimal>(
                                mEntryDate, mOpen, mHigh, mLow, mClose, it->getVolumeValue(), TimeFrame::DAILY));
                        }
                        mOpen = it->getOpenValue();
                        mHigh = DecimalConstants<Decimal>::DecimalZero;
                        mLow = it->getHighValue() * DecimalConstants<Decimal>::DecimalOneHundred;
                        mEntryDate = it->getDateValue();
                    }
                    if(it->getHighValue() > mHigh)
                        mHigh = it->getHighValue();
                    if(it->getLowValue() < mLow)
                        mLow = it->getLowValue();
                    
                    mClose = it->getCloseValue();
                }

                std::string timeFrameFilename = getTimeFrameFilename(timeFrameId);
                PalTimeSeriesCsvWriter<Decimal> csvWriter(timeFrameFilename, *syntheticTimeSeries);
                csvWriter.writeFile();
                mTimeSeriesMap.insert(std::make_pair(timeFrameId, syntheticTimeSeries));

                std::cout << timeFrameId << " " << syntheticTimeSeries->getNumEntries() << std::endl;
            }
            
            std::shared_ptr<OHLCTimeSeries<Decimal>> getSyntheticTimeSeries(int timeFrameId)
            {
                return nullptr;
            }

            void writeTimeFrameFile(int timeFrameId) 
            {
                OHLCTimeSeries<Decimal> series = mTimeSeriesMap.at(timeFrameId);
                std::string timeFrameFilename = this->getTimeFrameFilename(timeFrameId);
                PalTimeSeriesCsvWriter<Decimal> csvWriter(timeFrameFilename, *series);
                csvWriter.writeFile();
            }

            SyntheticTimeSeriesMap getSyntheticTimeSeriesMap() { return mTimeSeriesMap; }

        private:
            std::shared_ptr<OHLCTimeSeries<Decimal>> mOriginalHourlyTimeSeries;
            std::string mFilename;
            SyntheticTimeSeriesMap mTimeSeriesMap;
            
            Decimal mOpen = DecimalConstants<Decimal>::DecimalZero;
            Decimal mHigh = DecimalConstants<Decimal>::DecimalZero;
            Decimal mLow = DecimalConstants<Decimal>::DecimalZero;
            Decimal mClose = DecimalConstants<Decimal>::DecimalZero;
            boost::gregorian::date mEntryDate;

            std::string getTimeFrameFilename(int timeFrameId) 
            {
                return std::string(mFilename + std::string("_timeframe_") + std::to_string(timeFrameId));
            }

    };  
}

#endif
