#include <string>
#include <memory>

#include <boost/date_time.hpp>
#include "../BoostDateHelper.h"
#include "../PercentNumber.h"
#include "../TimeSeriesEntry.h"
#include "../TradingVolume.h"

typedef dec::decimal<7> DecimalType;
typedef mkc_timeseries::OHLCTimeSeriesEntry<DecimalType> EntryType;

class PriceActionLabSystem;
namespace mkc_timeseries { template <class Decimal> class OHLCTimeSeries; }

std::shared_ptr< mkc_timeseries::OHLCTimeSeries<DecimalType> >
readPALDataFile(const std::string &filename);

std::shared_ptr< mkc_timeseries::OHLCTimeSeries<DecimalType> > getRandomPriceSeries();
  
PriceActionLabSystem* getPricePatterns(const std::string &irFileName);

PriceActionLabSystem* getRandomPricePatterns();

// helper that your .cpp defines:
std::shared_ptr< mkc_timeseries::OHLCTimeSeries<DecimalType> >
getRandomPriceSeries();

std::shared_ptr<EntryType>
createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       const std::string& vol);

std::shared_ptr<EntryType>
createTimeSeriesEntry (const std::string& dateString,
		       const std::string& timeString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       const std::string& vol);

std::shared_ptr<EntryType>
createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       const std::string& vol,
		       mkc_timeseries::TimeFrame::Duration timeFrame);

std::shared_ptr<EntryType>
createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       mkc_timeseries::volume_t vol);

std::shared_ptr<EntryType>
createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       mkc_timeseries::volume_t vol);

std::shared_ptr<EntryType>
createTimeSeriesEntry (const mkc_timeseries::TimeSeriesDate& aDate,
			const DecimalType& openPrice,
			const DecimalType& highPrice,
			const DecimalType& lowPrice,
			const DecimalType& closePrice,
			mkc_timeseries::volume_t vol);



std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString);

DecimalType *
createRawDecimalPtr(const std::string& valueString);


DecimalType
createDecimal(const std::string& valueString);

boost::gregorian::date createDate (const std::string& dateString);



