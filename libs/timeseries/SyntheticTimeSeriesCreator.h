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
  /**
     * @brief Creates lower-resolution (coarser-grained) OHLC time series from a higher-resolution source.
     * 
     * This template class takes an existing intraday (e.g., hourly) OHLCTimeSeries and
     * partitions it into synthetic time series at a coarser time grain (e.g., daily).
     * Each synthetic bar aggregates multiple fine-grained bars:
     * - Open = the first open price at the filter time each day
     * - High = the maximum high price within the period
     * - Low  = the minimum low price within the period
     * - Close= the last close price before the next filter time
     * 
     * The result is stored internally in a map from an integer timeFrameId
     * (user-defined) to a shared pointer of the new OHLCTimeSeries.
     * You can then write each synthetic series out to CSV.
     * 
     * @tparam Decimal  Numeric type for price values (e.g., dec::decimal<7>, double).
     */
    template <class Decimal>
    class SyntheticTimeSeriesCreator
    {
    public:
      typedef typename std::map<int, std::shared_ptr<OHLCTimeSeries<Decimal>>> SyntheticTimeSeriesMap;
      
      /**
       * @brief Construct with an existing high-frequency series and a base filename.
       * @param timeSeries            Shared pointer to the original, fine-grained OHLCTimeSeries
       * @param hourlyDataFilename    Base path/filename for output CSVs (suffixes added automatically).
       * 
       * Initializes internal state; no synthetic series are generated until createSyntheticTimeSeries() is called.
       */
      SyntheticTimeSeriesCreator(std::shared_ptr<OHLCTimeSeries<Decimal>> timeSeries, std::string hourlyDataFilename) : 
	mOriginalHourlyTimeSeries(timeSeries),
	mFilename(hourlyDataFilename),
	mTimeSeriesMap(),
	mEntryDate()	// Constructs date with value not_a_date_time
      {}

      /**
         * @brief Aggregate entries into a synthetic series at a coarser time grain.
         * @param timeFrameId  Integer key to identify the output series (e.g., 1 = daily).
         * @param filterTime   The time-of-day (e.g., 09:00) at which each aggregated bar "opens".
         * 
         * Iterates over the original hourly series: whenever the bar's time-of-day matches filterTime,
         * it closes the previous day's bar, writes an aggregated entry, and starts a new aggregation period.
         * "Coarser-grained" means fewer bars: one per filter boundary instead of one per hour.
         */
      void createSyntheticTimeSeries(int timeFrameId, time_duration filterTime) 
      {
	std::shared_ptr<OHLCTimeSeries<Decimal>> syntheticTimeSeries =
	  std::make_shared<OHLCTimeSeries<Decimal>>(TimeFrame::DAILY, 
						    mOriginalHourlyTimeSeries->getVolumeUnits());

	for(auto it = mOriginalHourlyTimeSeries->beginRandomAccess(); it != mOriginalHourlyTimeSeries->endRandomAccess(); it++)
	  {
	    // this 'if' prevents the last bar for the first time value from being added to the time series
	    // since it won't be added until the next date is found, i.e. if the file ends on 12/29/2020
	    // the 12/29/2020 9:00 bar will be missing since it won't be added until the values are aggregated
	    // and the 9:00 timestamp is found again on 12/30/2020.
	    if(filterTime == it->getBarTime()) 
	      {
		if(mOpen != DecimalConstants<Decimal>::DecimalZero)
		  {
		    syntheticTimeSeries->addEntry(OHLCTimeSeriesEntry<Decimal>(mEntryDate,
									       mOpen,
									       mHigh,
									       mLow,
									       mClose,
									       it->getVolumeValue(),
									       TimeFrame::DAILY));
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

	// need to add the missing bar for the first time frame, the aggregated values should be set
	// correctly for this bar -- tested this on KC and MSFT data to make sure
	if(timeFrameId == 1)
	  syntheticTimeSeries->addEntry(OHLCTimeSeriesEntry<Decimal>(
								     mEntryDate, mOpen, mHigh, mLow, mClose, mOriginalHourlyTimeSeries->beginRandomAccess()->getVolumeValue(), TimeFrame::DAILY));

	mTimeSeriesMap.insert(std::make_pair(timeFrameId, syntheticTimeSeries));
      }

      /**
       * @brief Retrieve a previously created synthetic series by its identifier.
       * @param timeFrameId  The key used when createSyntheticTimeSeries() was called.
       * @return Shared pointer to the synthetic OHLCTimeSeries.
       * @throws std::out_of_range if no series exists for the given ID.
       */
      std::shared_ptr<OHLCTimeSeries<Decimal>> getSyntheticTimeSeries(int timeFrameId)
      {
	return mTimeSeriesMap.at(timeFrameId);
      }

      /**
       * @brief Write the synthetic series to a CSV file.
       * @param timeFrameId  Key identifying which series to write.
       * 
       * The filename is constructed as "<base>_timeframe_<ID>".
       * Uses PalTimeSeriesCsvWriter to format OHLC data in PriceActionLab-compatible format.
       */
      void writeTimeFrameFile(int timeFrameId) 
      {
	std::shared_ptr<OHLCTimeSeries<Decimal>> series = mTimeSeriesMap.at(timeFrameId);
	std::string timeFrameFilename = getTimeFrameFilename(timeFrameId);
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
