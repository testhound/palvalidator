#include "TestUtils.h"
#include "../PercentNumber.h"
#include "../DecimalConstants.h"
#include "../TimeFrame.h"

using namespace boost::gregorian;

date createDate (const std::string& dateString)
{
  return boost::gregorian::from_undelimited_string(dateString);
}

std::shared_ptr<EntryType>
    createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       const std::string& vol)
{
    auto date1 = boost::gregorian::from_undelimited_string(dateString);
    auto open1 = dec::fromString<DecimalType>(openPrice);
    auto high1 = dec::fromString<DecimalType>(highPrice);
    auto low1 = dec::fromString<DecimalType>(lowPrice);
    auto close1 = dec::fromString<DecimalType>(closePrice);
    auto vol1 = dec::fromString<DecimalType>(vol);
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol1, mkc_timeseries::TimeFrame::DAILY);
}

std::shared_ptr<EntryType>
createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       mkc_timeseries::volume_t vol)
{
    auto date1 = boost::gregorian::from_undelimited_string(dateString);
    auto open1 = dec::fromString<DecimalType>(openPrice);
    auto high1 = dec::fromString<DecimalType>(highPrice);
    auto low1 = dec::fromString<DecimalType>(lowPrice);
    auto close1 = dec::fromString<DecimalType>(closePrice);
    DecimalType vol1((uint) vol);
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol1, mkc_timeseries::TimeFrame::DAILY);
}

std::shared_ptr<EntryType>
createTimeSeriesEntry (const mkc_timeseries::TimeSeriesDate& aDate,
			const dec::decimal<7>& openPrice,
			const dec::decimal<7>& highPrice,
			const dec::decimal<7>& lowPrice,
			const dec::decimal<7>& closePrice,
			mkc_timeseries::volume_t vol)
{
  DecimalType vol1((uint) vol);
  return std::make_shared<EntryType>(aDate, openPrice, highPrice, lowPrice, 
						closePrice, vol1, mkc_timeseries::TimeFrame::DAILY);

}

std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString)
{
  return std::make_shared<DecimalType> (dec::fromString<DecimalType>(valueString));
}

DecimalType
createDecimal(const std::string& valueString)
{
  return dec::fromString<DecimalType>(valueString);
}