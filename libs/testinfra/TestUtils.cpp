#include <exception>
#include <random> 
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
#include "Security.h"

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

///
/// Returns a shared_ptr to a randomly chosen PalStrategy<DecimalType>.
/// Internally calls getRandomPricePatterns() (which loads "QQQ_IR.txt" :contentReference[oaicite:1]{index=1}),
/// then picks one PriceActionLabPattern at random, and finally uses makePalStrategy<DecimalType>
/// to wrap it in either PalLongStrategy or PalShortStrategy (with an empty Portfolio).
///
std::shared_ptr< PalStrategy<DecimalType> >
getRandomPalStrategy()
{
    // Create a default QQQ security using the random price series
    auto timeSeries = getRandomPriceSeries();
    auto security = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "NASDAQ QQQ Trust", timeSeries);
    
    // Use the overloaded version that accepts a security
    return getRandomPalStrategy(security);
}

///
/// Overloaded version that accepts a Security to add to the portfolio.
/// This ensures the strategy has a populated portfolio for use in permutation tests.
///
std::shared_ptr< PalStrategy<DecimalType> >
getRandomPalStrategy(std::shared_ptr<Security<DecimalType>> security)
{
    // 1. Load the PriceActionLabSystem from "QQQ_IR.txt"
    PriceActionLabSystem* sys = getRandomPricePatterns();
    if (!sys) {
        throw std::runtime_error("Failed to load PriceActionLabSystem from getRandomPricePatterns()");
    }

    // 2. Get total number of patterns
    unsigned long total = sys->getNumPatterns();
    if (total == 0) {
        throw std::runtime_error("No patterns available in PriceActionLabSystem");
    }

    // 3. Choose a random index in [0, total-1]
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned long> dist(0, total - 1);
    unsigned long idx = dist(gen);

    // 4. Advance the iterator to the chosen index
    auto it = sys->allPatternsBegin();
    std::advance(it, static_cast<std::ptrdiff_t>(idx));
    auto chosenPattern = *it;
    // chosenPattern is std::shared_ptr<PriceActionLabPattern>

    // 5. Build a named Portfolio<DecimalType> and add the provided security
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("RandomPortfolio");
    if (security) {
        portfolio->addSecurity(security);
    }

    // 6. Build the PalStrategy using the one‐line factory:
    auto strategy = makePalStrategy<DecimalType>(
                        "RandomPalStrategy",   // strategy name
                        chosenPattern,
                        portfolio
                    );

    return strategy;
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
