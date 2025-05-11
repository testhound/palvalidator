#ifndef TIMEFRAMEDISCOVERY_H
#define TIMEFRAMEDISCOVERY_H

#include "TimeSeries.h"

using boost::posix_time::time_duration;

namespace mkc_timeseries
{
  template <class Decimal>
  class TimeFrameDiscovery
  {
  public:
    typedef typename std::vector<time_duration> TimeFrameCollection;
    typedef typename TimeFrameCollection::const_iterator TimeFrameIterator;

    TimeFrameDiscovery(std::shared_ptr<OHLCTimeSeries<Decimal>> timeSeries) :
      mTimeFrames(),
      mHourlyTimeSeries(timeSeries)
    {}

    // Infer how many bars per trading day
    // For the stock market which opens at 9:30 AM EST and closes at 4:00 PM
    // and using 60 minute bars, there would be seven (7) time frames per day
      
    void inferTimeFrames()
    {
      for(auto it = mHourlyTimeSeries->beginRandomAccess(); it != mHourlyTimeSeries->endRandomAccess(); it++)
	{
	  if(std::find(mTimeFrames.begin(), mTimeFrames.end(), it->getBarTime()) == mTimeFrames.end())
	    mTimeFrames.push_back(it->getBarTime());
	  else 
	    break;
	}
    }

    time_duration getTimeFrame(int position) 
    { 
      if((TimeFrameCollection::size_type) position >= mTimeFrames.size())
	throw McptConfigurationFileReaderException("Timeframe does not exists: id=" + std::to_string(position) + " number of time frames=" + std::to_string(mTimeFrames.size()));
      return mTimeFrames.at(position); 
    };

    int numTimeFrames() const { return mTimeFrames.size(); };
    TimeFrameIterator getTimeFramesBegin() const { return mTimeFrames.begin(); };
    TimeFrameIterator getTimeFramesEnd() const { return mTimeFrames.end(); };
    TimeFrameCollection getTimeFrames() const { return mTimeFrames; }
  private:
    std::vector<time_duration> mTimeFrames;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mHourlyTimeSeries;

  };
}

#endif
