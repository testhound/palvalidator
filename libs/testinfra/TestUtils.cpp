#include <exception>
#include <boost/filesystem.hpp>
#include "PalParseDriver.h"
#include "TestUtils.h"
#include "PercentNumber.h"
#include "DecimalConstants.h"
#include "TimeFrame.h"
#include "TimeSeriesCsvReader.h"
#include "PalStrategy.h"
#include "BoostDateHelper.h"
#include "PalAst.h"

using namespace boost::gregorian;
using namespace boost::posix_time;
using namespace mkc_timeseries;

using Num = num::DefaultNumber;

std::shared_ptr<OHLCTimeSeries<Num>> readPALDataFile(const std::string &filename)
{
  PALFormatCsvReader<Num> csvFile (filename);
  csvFile.readFile();

  return (csvFile.getTimeSeries());
}

PriceActionLabSystem* getPricePatterns(const std::string &irFileName)
{
  boost::filesystem::path irFilePath (irFileName);

  if (!exists (irFilePath))
    throw std::runtime_error("PAL IR path " +irFilePath.string() +" does not exist");

  // Constructor driver (facade) that will parse the IR and return
  // and AST representation
  mkc_palast::PalParseDriver driver (irFilePath.string());

  // Read the IR file

  driver.Parse();

  return (driver.getPalStrategies());
}

PriceActionLabSystem* getRandomPricePatterns()
{
  return getPricePatterns("QQQ_IR.txt");
}

std::shared_ptr<OHLCTimeSeries<DecimalType>> getRandomPriceSeries()
{
  return readPALDataFile("QQQ.txt");
}

date createDate (const std::string& dateString)
{
  return boost::gregorian::from_undelimited_string(dateString);
}

DecimalType *
createRawDecimalPtr(const std::string& valueString)
{
  return new DecimalType (dec::fromString<DecimalType>(valueString));
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
		       const std::string& timeString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       const std::string& vol)
{
    auto date1 = boost::gregorian::from_undelimited_string(dateString);
    auto time1 = duration_from_string(timeString);
    ptime dateTime(date1, time1);
    auto open1 = dec::fromString<DecimalType>(openPrice);
    auto high1 = dec::fromString<DecimalType>(highPrice);
    auto low1 = dec::fromString<DecimalType>(lowPrice);
    auto close1 = dec::fromString<DecimalType>(closePrice);
    auto vol1 = dec::fromString<DecimalType>(vol);
    return std::make_shared<EntryType>(dateTime, open1, high1, low1, 
				       close1, vol1, mkc_timeseries::TimeFrame::INTRADAY);

}

std::shared_ptr<EntryType>
    createTimeSeriesEntry (const std::string& dateString,
			   const std::string& openPrice,
			   const std::string& highPrice,
			   const std::string& lowPrice,
			   const std::string& closePrice,
			   const std::string& vol,
			   mkc_timeseries::TimeFrame::Duration timeFrame)
{
    auto date1 = boost::gregorian::from_undelimited_string(dateString);
    auto open1 = dec::fromString<DecimalType>(openPrice);
    auto high1 = dec::fromString<DecimalType>(highPrice);
    auto low1 = dec::fromString<DecimalType>(lowPrice);
    auto close1 = dec::fromString<DecimalType>(closePrice);
    auto vol1 = dec::fromString<DecimalType>(vol);
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol1, timeFrame);
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
    auto vol1 = DecimalType((uint) vol);
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol1, mkc_timeseries::TimeFrame::DAILY);

}


std::shared_ptr<EntryType>
createTimeSeriesEntry (const mkc_timeseries::TimeSeriesDate& aDate,
			const DecimalType& openPrice,
			const DecimalType& highPrice,
			const DecimalType& lowPrice,
			const DecimalType& closePrice,
			mkc_timeseries::volume_t vol)
{
  DecimalType vol1((uint) vol);
  return std::make_shared<EntryType>(aDate, openPrice, highPrice, lowPrice, 
						closePrice, vol1, mkc_timeseries::TimeFrame::DAILY);

}

std::shared_ptr<OHLCTimeSeriesEntry<DecimalType>>
createEquityEntry (const std::string& dateString,
		   const std::string& openPrice,
		   const std::string& highPrice,
		   const std::string& lowPrice,
		   const std::string& closePrice,
		   volume_t vol)
{
  return createTimeSeriesEntry(dateString, openPrice,
			       highPrice, lowPrice,
			       closePrice, vol);
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
