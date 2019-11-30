#ifndef PATTERNRERUNNER_H
#define PATTERNRERUNNER_H

#endif // PATTERNRERUNNER_H

#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include "PalParseDriver.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "TimeSeriesCsvReader.h"
#include <boost/filesystem.hpp>
#include <exception>
#include "LogPalPattern.h"
#include <runner.hpp>

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;

static std::shared_ptr<SecurityAttributes<Decimal>> createSecurityAttributes (const std::string &symbol)
{
  SecurityAttributesFactory<Decimal> factory;
  SecurityAttributesFactory<Decimal>::SecurityAttributesIterator it = factory.getSecurityAttributes(symbol);

  if (it != factory.endSecurityAttributes())
    return it->second;
  else
    throw runtime_error("createSecurityAttributes - ticker symbol " +symbol +" is unkown");
}

static TradingVolume::VolumeUnit getVolumeUnit (std::shared_ptr<SecurityAttributes<Decimal>> attributesOfSecurity)
{
  if (attributesOfSecurity->isEquitySecurity())
    return TradingVolume::SHARES;
  else
    return TradingVolume::CONTRACTS;
}

static std::shared_ptr<TimeSeriesCsvReader<Decimal>>
getHistoricDataFileReader(const std::string& historicDataFilePath,
			  const std::string& dataFileFormatStr,
			  TimeFrame::Duration timeFrame,
			  TradingVolume::VolumeUnit unitsOfVolume,
			  const Decimal& tickValue)
{
  std::string upperCaseFormatStr = boost::to_upper_copy(dataFileFormatStr);

  if (upperCaseFormatStr == std::string("PAL"))
    return std::make_shared<PALFormatCsvReader<Decimal>>(historicDataFilePath, timeFrame,
                                                         unitsOfVolume, tickValue);
  else if (upperCaseFormatStr == std::string("TRADESTATION"))
    return std::make_shared<TradeStationFormatCsvReader<Decimal>>(historicDataFilePath, timeFrame,
                                                                  unitsOfVolume, tickValue);
  else if (upperCaseFormatStr == std::string("CSIEXTENDED"))
    return std::make_shared<CSIExtendedFuturesCsvReader<Decimal>>(historicDataFilePath, timeFrame,
                                                                  unitsOfVolume, tickValue);
  else if (upperCaseFormatStr == std::string("CSI"))
    return std::make_shared<CSIFuturesCsvReader<Decimal>>(historicDataFilePath, timeFrame,
                                                          unitsOfVolume, tickValue);
  else if (upperCaseFormatStr == std::string("TRADESTATIONINDICATOR1"))
    return std::make_shared<TradeStationIndicator1CsvReader<Decimal>>(historicDataFilePath,
                                                                      timeFrame,
                                                                      unitsOfVolume,
                                                                      tickValue);

  else
    throw runtime_error("Historic data file format " +dataFileFormatStr +" not recognized");
}


static std::shared_ptr<mkc_timeseries::Security<Decimal>>
createSecurity (std::shared_ptr<SecurityAttributes<Decimal>> attributes,
                std::shared_ptr<TimeSeriesCsvReader<Decimal>> aReader)
{
  if (attributes->isEquitySecurity())
    {
      if (attributes->isFund())
        {
          return std::make_shared<EquitySecurity<Decimal>>(attributes->getSymbol(),
                                                           attributes->getName(),
                                                           aReader->getTimeSeries());
        }
      else if (attributes->isCommonStock())
        {
          return std::make_shared<EquitySecurity<Decimal>>(attributes->getSymbol(),
                                                           attributes->getName(),
                                                           aReader->getTimeSeries());
        }
      else
        throw runtime_error("Unknown security attribute");
    }
  else
    return std::make_shared<FuturesSecurity<Decimal>>(attributes->getSymbol(),
                                                      attributes->getName(),
                                                      attributes->getBigPointValue(),
                                                      attributes->getTick(),
                                                      aReader->getTimeSeries());

}

class PatternReRunner
{
public:
  PatternReRunner(const std::string& irPath, const std::string& historicDataFilePathStr, const std::string& tickerSymbol,
                  DateRange backtestingDates, Decimal criterion, const std::string& exportFileName):
    mPatternsToTest(readFile(irPath)),
    mSecurity(makeSecurity(historicDataFilePathStr, tickerSymbol)),
    mCriterion(criterion),
    mExportFile(exportFileName)
  {
    mBacktester = std::make_shared<DailyBackTester<Decimal>>(backtestingDates.getFirstDate(),
                                                             backtestingDates.getLastDate());
  }
  std::shared_ptr<mkc_timeseries::Security<Decimal>> makeSecurity(const std::string& historicDataFilePathStr, const std::string& tickerSymbol)
  {
    std::cout << "Reading timeseries file: " << historicDataFilePathStr << " for symbol: " << tickerSymbol << std::endl;
    boost::filesystem::path historicDataFilePath (historicDataFilePathStr);
    if (!exists (historicDataFilePath))
      throw runtime_error("Historic data file path " +historicDataFilePath.string() +" does not exist");

    std::shared_ptr<SecurityAttributes<Decimal>> attributes = createSecurityAttributes (tickerSymbol);
    TimeFrame::Duration backTestingTimeFrame = TimeFrame::Duration::DAILY;

    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = getHistoricDataFileReader(historicDataFilePathStr,
                                                                                     "PAL",
                                                                                     backTestingTimeFrame,
                                                                                     getVolumeUnit(attributes),
                                                                                     attributes->getTick());
    reader->readFile();

    return createSecurity(attributes, reader);
  }

  PriceActionLabSystem* readFile(const std::string& fileName)
  {
    std::cout << "Reading IR file: " << fileName << std::endl;
    PriceActionLabSystem* system;
    mkc_palast::PalParseDriver driver (fileName);

    // Read the IR file
    driver.Parse();

    std::cout << "Parsing successfully completed." << std::endl << std::endl;
    system = driver.getPalStrategies();
    std::cout << "Total number IR patterns = " << system->getNumPatterns() << std::endl;
    std::cout << "Total long IR patterns = " << system->getNumLongPatterns() << std::endl;
    std::cout << "Total short IR patterns = " << system->getNumShortPatterns() << std::endl;
    return system;
  }

  void backtest(runner& Runner)
  {

    mSecurity->getTimeSeries()->syncronizeMapAndArray();
    std::string portfolioName(mSecurity->getName() + std::string(" Portfolio"));

    auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
    aPortfolio->addSecurity(mSecurity);

    std::string longStrategyNameBase("PAL Long Strategy ");

    std::string strategyName;
    unsigned long strategyNumber = 1;

    std::shared_ptr<PriceActionLabPattern> patternToTest;
    std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;

    std::vector<boost::unique_future<void>> resultsOrErrorsVector;

    PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator = mPatternsToTest->patternLongsBegin();
    for (; longPatternsIterator != mPatternsToTest->patternLongsEnd(); longPatternsIterator++)
      {
        patternToTest = longPatternsIterator->second;
        strategyName = longStrategyNameBase + std::to_string(strategyNumber);
        longStrategy = std::make_shared<PalLongStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);
        //start paralel part
        resultsOrErrorsVector.emplace_back(Runner.post([ this
                                                       , patternToTest
                                                       , strategyName
                                                       , longStrategy]() -> void {


            std::shared_ptr<BackTester<Decimal>> clonedBackTester = mBacktester->clone();
            clonedBackTester->addStrategy(longStrategy);
            clonedBackTester->backtest();
            std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy = (*(clonedBackTester->beginStrategies()));
            Decimal profitFactor = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getProfitFactor();
            unsigned int tradeNum = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getNumPositions();
            if (profitFactor > mCriterion)
              {
                boost::mutex::scoped_lock Lock(mOutFileLock);
                LogPalPattern::LogPattern(patternToTest, mExportFile);
                std::cout << "Rerunning, pass: " << strategyName << ": profit factor: " << profitFactor.getAsDouble() << ", trades: " << tradeNum << std::endl;
              }
          }));
        strategyNumber++;
      }

    for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
      {
        try{
          resultsOrErrorsVector[i].wait();
          resultsOrErrorsVector[i].get();
        }
        catch(std::exception const& e)
        {
          std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
        }
      }
    //end parallel part

    resultsOrErrorsVector.clear();
    strategyNumber = 0;
    std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;
    std::string shortStrategyNameBase("PAL Short Strategy ");
    PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator = mPatternsToTest->patternShortsBegin();
    //resultsOrErrorsVector.clear();
    for (; shortPatternsIterator != mPatternsToTest->patternShortsEnd(); shortPatternsIterator++)
      {
        patternToTest = shortPatternsIterator->second;
        strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
        shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

        //start paralel part
        resultsOrErrorsVector.emplace_back(Runner.post([ this
                                                       , patternToTest
                                                       , strategyName
                                                       , shortStrategy]() -> void {


            std::shared_ptr<BackTester<Decimal>> clonedBackTester = mBacktester->clone();
            clonedBackTester->addStrategy(shortStrategy);
            clonedBackTester->backtest();
            std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy = (*(clonedBackTester->beginStrategies()));
            Decimal profitFactor = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getProfitFactor();
            unsigned int tradeNum = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getNumPositions();
            if (profitFactor > mCriterion)
              {
                boost::mutex::scoped_lock Lock(mOutFileLock);
                LogPalPattern::LogPattern(patternToTest, mExportFile);
                std::cout << "Rerunning, pass: " << strategyName << ": profit factor: " << profitFactor.getAsDouble() << ", trades: " << tradeNum << std::endl;
              }
            std::cout << strategyName << ": profit factor: " << profitFactor.getAsDouble() << ", trades: " << tradeNum << std::endl;
          }));
        strategyNumber++;
      }
    for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
      {    for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
          {
            try{
              resultsOrErrorsVector[i].wait();
              resultsOrErrorsVector[i].get();
            }
            catch(std::exception const& e)
            {
              std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
            }
          }
        //end parallel part
        try{
          resultsOrErrorsVector[i].wait();
          resultsOrErrorsVector[i].get();
        }
        catch(std::exception const& e)
        {
          std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
        }
      }
    //end parallel part

  }
private:
  PriceActionLabSystem* mPatternsToTest;
  std::shared_ptr<mkc_timeseries::Security<Decimal>> mSecurity;
  std::shared_ptr<BackTester<Decimal>> mBacktester;
  Decimal mCriterion;
  boost::mutex mOutFileLock;
  std::ofstream mExportFile;
};
