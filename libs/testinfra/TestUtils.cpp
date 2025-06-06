#include <exception>
#include <random>
#include <iostream>
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
#include <typeinfo>

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


// New shared_ptr version for modern code
std::shared_ptr<PriceActionLabSystem> getPricePatterns(const std::string &irFileName)
{
  boost::filesystem::path irFilePath (irFileName);

  if (!exists (irFilePath))
    throw std::runtime_error("PAL IR path " +irFilePath.string() +" does not exist");

  mkc_palast::PalParseDriver driver (irFilePath.string());
  driver.Parse();

  return driver.getPalStrategies();
}


// New shared_ptr version for modern code
std::shared_ptr<PriceActionLabSystem> getRandomPricePatterns()
{
  return getPricePatterns("QQQ_IR.txt");
}

std::shared_ptr<OHLCTimeSeries<DecimalType>> getRandomPriceSeries()
{
  return readPALDataFile("QQQ.txt");
}

///
/// Returns a shared_ptr to a randomly chosen PalStrategy<DecimalType>.
/// Internally calls getRandomPricePatterns() (which loads "QQQ_IR.txt"),
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
    // 1. Load the PriceActionLabSystem from "QQQ_IR.txt" using shared_ptr version
    // IMPORTANT: Keep the shared_ptr alive to prevent AstFactory destruction
    static thread_local std::shared_ptr<PriceActionLabSystem> cached_sys = nullptr;
    
    if (!cached_sys) {
        std::cerr << "DEBUG: Loading PriceActionLabSystem from QQQ_IR.txt" << std::endl;
        cached_sys = getRandomPricePatterns();
        if (!cached_sys) {
            throw std::runtime_error("Failed to load PriceActionLabSystem from getRandomPricePatterns()");
        }
        std::cerr << "DEBUG: PriceActionLabSystem loaded successfully" << std::endl;
        std::cerr << "  System pointer: " << cached_sys.get() << std::endl;
    } else {
        std::cerr << "DEBUG: Using cached PriceActionLabSystem" << std::endl;
        std::cerr << "  Cached system pointer: " << cached_sys.get() << std::endl;
    }

    // 2. Get total number of patterns
    unsigned long total = cached_sys->getNumPatterns();
    std::cerr << "DEBUG: Total patterns available: " << total << std::endl;
    if (total == 0) {
        throw std::runtime_error("No patterns available in PriceActionLabSystem");
    }

    // 3. Choose a random index in [0, total-1]
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned long> dist(0, total - 1);
    unsigned long idx = dist(gen);
    std::cerr << "DEBUG: Chosen pattern index: " << idx << std::endl;

    // 4. Advance the iterator to the chosen index
    auto it = cached_sys->allPatternsBegin();
    std::advance(it, static_cast<std::ptrdiff_t>(idx));
    auto chosenPattern = *it;
    // chosenPattern is std::shared_ptr<PriceActionLabPattern>
    
    std::cerr << "DEBUG: Chosen pattern details:" << std::endl;
    std::cerr << "  Pattern pointer: " << chosenPattern.get() << std::endl;
    if (chosenPattern) {
        std::cerr << "  Pattern file: " << chosenPattern->getFileName() << std::endl;
        std::cerr << "  Pattern index: " << chosenPattern->getpatternIndex() << std::endl;
        
        // Test the isLongPattern() call that's causing the issue
        std::cerr << "DEBUG: About to call chosenPattern->isLongPattern()..." << std::endl;
        
        // Add detailed debugging of the mEntry object
        auto marketEntry = chosenPattern->getMarketEntry();
        if (!marketEntry) {
            std::cerr << "ERROR: MarketEntry is null!" << std::endl;
            throw std::runtime_error("MarketEntry is null");
        }
        
        std::cerr << "DEBUG: mEntry type: " << typeid(*marketEntry).name() << std::endl;
        
        // Check if it's the abstract base class (this should never happen)
        if (typeid(*marketEntry) == typeid(MarketEntryExpression)) {
            std::cerr << "FATAL ERROR: mEntry is abstract MarketEntryExpression - this should never happen!" << std::endl;
            std::cerr << "  Pattern file: " << chosenPattern->getFileName() << std::endl;
            std::cerr << "  Pattern index: " << chosenPattern->getpatternIndex() << std::endl;
            throw std::runtime_error("Abstract MarketEntryExpression detected");
        }
        
        // Check for specific concrete types
        if (auto longEntry = std::dynamic_pointer_cast<LongMarketEntryOnOpen>(marketEntry)) {
            std::cerr << "DEBUG: Detected LongMarketEntryOnOpen" << std::endl;
        } else if (auto shortEntry = std::dynamic_pointer_cast<ShortMarketEntryOnOpen>(marketEntry)) {
            std::cerr << "DEBUG: Detected ShortMarketEntryOnOpen" << std::endl;
        } else {
            std::cerr << "WARNING: Unknown MarketEntryExpression type: " << typeid(*marketEntry).name() << std::endl;
            
            // Additional debugging for unknown types - check if it's actually abstract
            std::cerr << "=== FACTORY STATE INVESTIGATION ===" << std::endl;
            std::cerr << "MarketEntry pointer: " << marketEntry.get() << std::endl;
            std::cerr << "MarketEntry use_count: " << marketEntry.use_count() << std::endl;
            
            // Try to call the pure virtual methods to see what happens
            try {
                std::cerr << "Attempting to call isLongPattern()..." << std::endl;
                bool isLong = marketEntry->isLongPattern();
                std::cerr << "isLongPattern() returned: " << isLong << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Exception in isLongPattern(): " << e.what() << std::endl;
            }
            
            try {
                std::cerr << "Attempting to call isShortPattern()..." << std::endl;
                bool isShort = marketEntry->isShortPattern();
                std::cerr << "isShortPattern() returned: " << isShort << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Exception in isShortPattern(): " << e.what() << std::endl;
            }
            
            // Check memory around the object
            std::cerr << "Object size: " << sizeof(*marketEntry) << std::endl;
            std::cerr << "=== END FACTORY STATE INVESTIGATION ===" << std::endl;
        }
        
        try {
            bool isLong = chosenPattern->isLongPattern();
            std::cerr << "  Is long pattern: " << (isLong ? "true" : "false") << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception calling isLongPattern(): " << e.what() << std::endl;
            throw;
        }
    } else {
        std::cerr << "ERROR: Chosen pattern is null!" << std::endl;
        throw std::runtime_error("Chosen pattern is null");
    }

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
