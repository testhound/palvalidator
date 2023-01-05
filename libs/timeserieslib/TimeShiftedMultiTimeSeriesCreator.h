#ifndef TIME_SHIFTED_MULTI_TIME_SERIES_CREATOR_H
#define TIME_SHIFTED_MULTI_TIME_SERIES_CREATOR_H

#include "SyntheticTimeSeriesCreator.h"
#include "TimeFrameDiscovery.h"
#include "TimeSeries.h"
#include "TimeSeriesCsvReader.h"
#include "Security.h"
#include "TimeSeriesValidator.h"

namespace mkc_timeseries
{
  template <class Decimal>
  class TimeShiftedMultiTimeSeriesCreator
  {
  public:
    typedef typename std::vector<std::shared_ptr<OHLCTimeSeries<Decimal>>> ShiftedTimeSeriesContainer;
    typedef typename ShiftedTimeSeriesContainer::const_iterator ShiftedTimeSeriesIterator;

    TimeShiftedMultiTimeSeriesCreator(std::shared_ptr<Security<Decimal>> security) :
      mShiftedTimeSeriesContainer(),
      mSecurity(security)
    {}
      
    virtual ~TimeShiftedMultiTimeSeriesCreator()
    {}

    virtual void createShiftedTimeSeries() = 0;

    ShiftedTimeSeriesIterator beginShiftedTimeSeries() const
    {
      return mShiftedTimeSeriesContainer.begin();
    }

    ShiftedTimeSeriesIterator endShiftedTimeSeries() const
    {
      return mShiftedTimeSeriesContainer.end();
    }
    
    int numTimeSeriesCreated() const
      {
	return mShiftedTimeSeriesContainer.size();
      }

  protected:
    std::shared_ptr<Security<Decimal>> getSecurity() const
      {
	return mSecurity;
      }

    void addTimeSeries(std::shared_ptr<OHLCTimeSeries<Decimal>> series)
      {
	mShiftedTimeSeriesContainer.push_back(series);
      }

  private:
    ShiftedTimeSeriesContainer mShiftedTimeSeriesContainer;
    std::shared_ptr<Security<Decimal>> mSecurity;
};

  // This class creates multiple, time shifted end of day time series from
  // a input intraday time series.

  template <class Decimal>
  class DailyTimeShiftedMultiTimeSeriesCreator : public TimeShiftedMultiTimeSeriesCreator<Decimal>
  {
  public:
    DailyTimeShiftedMultiTimeSeriesCreator(const std::string intradayDataFilePath, const std::shared_ptr<Security<Decimal>> security)
      : TimeShiftedMultiTimeSeriesCreator<Decimal>(security),
      mIntradayDataFilePath(intradayDataFilePath)
    {}

    void createShiftedTimeSeries()
      {
	std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = 
	  std::make_shared<TradeStationFormatCsvReader<Decimal>>(mIntradayDataFilePath,
								 TimeFrame::INTRADAY,
								 this->getSecurity()->getVolumeUnit(),
								 this->getSecurity()->getTick());
        reader->readFile();

        std::shared_ptr<TimeFrameDiscovery<Decimal>> timeFrameDiscovery = 
	  std::make_shared<TimeFrameDiscovery<Decimal>>(reader->getTimeSeries());

        timeFrameDiscovery->inferTimeFrames();

        std::shared_ptr<SyntheticTimeSeriesCreator<Decimal>> syntheticTimeSeriesCreator = 
          std::make_shared<SyntheticTimeSeriesCreator<Decimal>>(reader->getTimeSeries(), mIntradayDataFilePath);

        std::shared_ptr<TimeSeriesValidator<Decimal>> validator = 
	  std::make_shared<TimeSeriesValidator<Decimal>>(reader->getTimeSeries(),
							 this->getSecurity()->getTimeSeries(),
							 timeFrameDiscovery->numTimeFrames());
        validator->validate();

	int timeFrameId = 0;
        for(int i = 0; i < timeFrameDiscovery->numTimeFrames(); i++) 
        {
	  timeFrameId = i + 1;
          boost::posix_time::time_duration timeStamp = timeFrameDiscovery->getTimeFrame(i);
          syntheticTimeSeriesCreator->createSyntheticTimeSeries(timeFrameId, timeStamp);

	  this->addTimeSeries (syntheticTimeSeriesCreator->getSyntheticTimeSeries(timeFrameId));
	  //NOTE: It should no longer be necessary to write the files since we have
	  // the in-memory time series avaialble

          //syntheticTimeSeriesCreator->writeTimeFrameFile(timeFrameId);
        }
      }

  private:
    std::string mIntradayDataFilePath;
  };
}

#endif
