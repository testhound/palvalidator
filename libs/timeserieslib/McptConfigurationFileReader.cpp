// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "McptConfigurationFileReader.h"
#include "PalParseDriver.h"
#include "TimeFrameUtility.h"
#include "TimeSeriesEntry.h"
#include "TimeSeriesCsvReader.h"
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include <cstdio>
#include "number.h"
#include "DataSourceReader.h"

using namespace boost::filesystem;
//extern PriceActionLabSystem* parsePALCode();
//extern FILE *yyin;

using Decimal = num::DefaultNumber;

namespace mkc_timeseries
{
  static std::shared_ptr<TimeSeriesCsvReader<Decimal>>
  getHistoricDataFileReader(
          const std::string& historicalDataPath,
          const std::string& dataFileFormat,
			    TimeFrame::Duration timeFrame,
			    TradingVolume::VolumeUnit unitsOfVolume,
			    const Decimal& tickValue);

  static std::shared_ptr<SecurityAttributes<Decimal>> createSecurityAttributes (const std::string &tickerSymbol);
  static TradingVolume::VolumeUnit getVolumeUnit (std::shared_ptr<SecurityAttributes<Decimal>> attributesOfSecurity);
  static std::shared_ptr<mkc_timeseries::Security<Decimal>>
  createSecurity (std::shared_ptr<SecurityAttributes<Decimal>> attributes,
		  std::shared_ptr<TimeSeriesCsvReader<Decimal>> aReader);

  static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
							 const DateRange& backtestingDates);

  McptConfigurationFileReader::McptConfigurationFileReader (const std::shared_ptr<RunParameters>& runParameters)
    : mRunParameters(runParameters)
  {}

  std::shared_ptr<McptConfiguration<Decimal>> McptConfigurationFileReader::readConfigurationFile(bool skipPatterns, bool downloadFile)
  {
    io::CSVReader<8> csvConfigFile(mRunParameters->getConfigFile1Path().c_str());
    csvConfigFile.set_header("Symbol", "IRPath", "FileFormat", "ISDateStart", "ISDateEnd", "OOSDateStart", "OOSDateEnd", "TimeFrame");

    std::string tickerSymbol, palIRFilePathStr, fileFormat;
    std::string inSampleStartDate, inSampleEndDate, oosStartDate, oosEndDate;
    std::string timeFrameStr;

    boost::gregorian::date insampleDateStart, insampleDateEnd, oosDateStart, oosDateEnd;

    csvConfigFile.read_row (tickerSymbol, palIRFilePathStr, fileFormat, inSampleStartDate, inSampleEndDate,
			    oosStartDate, oosEndDate, timeFrameStr);

    insampleDateStart = boost::gregorian::from_undelimited_string(inSampleStartDate);
    insampleDateEnd = boost::gregorian::from_undelimited_string(inSampleEndDate);

    DateRange inSampleDates(insampleDateStart, insampleDateEnd);
    oosDateStart = boost::gregorian::from_undelimited_string(oosStartDate);
    oosDateEnd = boost::gregorian::from_undelimited_string(oosEndDate);

    DateRange ooSampleDates( oosDateStart, oosDateEnd);
    if (oosDateStart <= insampleDateEnd)
      std::cout << "******** Warning OOS start date is before IS start date **********" << std::endl << std::endl;
    //throw McptConfigurationFileReaderException("McptConfigurationFileReader::readConfigurationFile - OOS start date starts before insample end date");

    std::shared_ptr<SecurityAttributes<Decimal>> attributes = createSecurityAttributes (tickerSymbol);
    TimeFrame::Duration backTestingTimeFrame = getTimeFrameFromString(timeFrameStr);

    std::string dataFilename = mRunParameters->getEodDataFilePath();
    if(mRunParameters->shouldUseApi()) 
    {
      std::string token = getApiTokenFromFile(mRunParameters->getApiConfigFilePath(), mRunParameters->getApiSource());
      std::shared_ptr<DataSourceReader> dataSourceReader = getDataSourceReader(mRunParameters->getApiSource(), token);
      dataFilename = dataSourceReader->createTemporaryFile(tickerSymbol, timeFrameStr, inSampleDates, ooSampleDates, downloadFile);
      fileFormat = "TRADESTATION";
    }

    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = getHistoricDataFileReader(
                dataFilename,
                fileFormat,
								backTestingTimeFrame,
								getVolumeUnit(attributes),
								attributes->getTick());
    reader->readFile();

    //dataSourceReader->destroyFiles(); // TODO: delete temp files

    //  insampleDateStart
    boost::gregorian::date timeSeriesStartDate = reader->getTimeSeries()->getFirstDate();
    if (insampleDateStart < timeSeriesStartDate)
      {
	boost::gregorian::date_period daysBetween(insampleDateStart, timeSeriesStartDate);
	if (daysBetween.length().days() > 10)
	  {
	    std::string inSampleDateStr(boost::gregorian::to_simple_string (insampleDateStart));
	    std::string timeSeriesDateStr(boost::gregorian::to_simple_string (timeSeriesStartDate));

	    throw McptConfigurationFileReaderException (std::string("Number of days between configuration file IS start date of ") +inSampleDateStr +std::string(" and TimeSeries start date of ") +timeSeriesDateStr +std::string(" is greater than 10 days"));
	  }
      }
    PriceActionLabSystem* system;
    if (!skipPatterns)
      {

	boost::filesystem::path irFilePath (palIRFilePathStr);

	if (!exists (irFilePath))
	  throw McptConfigurationFileReaderException("PAL IR path " +irFilePath.string() +" does not exist");

	// Constructor driver (facade) that will parse the IR and return
	// and AST representation
	mkc_palast::PalParseDriver driver (irFilePath.string());

	// Read the IR file

	driver.Parse();

	std::cout << "Parsing successfully completed." << std::endl << std::endl;
	system = driver.getPalStrategies();
	std::cout << "Total number IR patterns = " << system->getNumPatterns() << std::endl;
	std::cout << "Total long IR patterns = " << system->getNumLongPatterns() << std::endl;
	std::cout << "Total short IR patterns = " << system->getNumShortPatterns() << std::endl;
	//yyin = fopen (irFilePath.string().c_str(), "r");
	//PriceActionLabSystem* system = parsePALCode();
	
	//fclose (yyin);
      }
    else
      {
        std::cout << "McptConfiguration: Skipping PalPattern reading section." << std::endl;
        system = nullptr;
      }

    return std::make_shared<McptConfiguration<Decimal>>(getBackTester(backTestingTimeFrame, ooSampleDates),
						  getBackTester(backTestingTimeFrame, inSampleDates),
						  createSecurity (attributes, reader),
						  system, inSampleDates, ooSampleDates,
              dataFilename);
  }

  static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
							 const DateRange& backtestingDates)
  {
    if (theTimeFrame == TimeFrame::DAILY)
      return std::make_shared<DailyBackTester<Decimal>>(backtestingDates.getFirstDate(),
						  backtestingDates.getLastDate());
    else if (theTimeFrame == TimeFrame::WEEKLY)
      return std::make_shared<WeeklyBackTester<Decimal>>(backtestingDates.getFirstDate(),
						   backtestingDates.getLastDate());
    else if (theTimeFrame == TimeFrame::MONTHLY)
      return std::make_shared<MonthlyBackTester<Decimal>>(backtestingDates.getFirstDate(),
						    backtestingDates.getLastDate());
    else
      throw McptConfigurationFileReaderException("getBacktester - cannot create backtester for time frame other than daily or monthly");
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
	  throw McptConfigurationFileReaderException("Unknown security attribute");
      }
    else
      return std::make_shared<FuturesSecurity<Decimal>>(attributes->getSymbol(),
						  attributes->getName(),
						  attributes->getBigPointValue(),
						  attributes->getTick(),
						  aReader->getTimeSeries());

  }


  static TradingVolume::VolumeUnit getVolumeUnit (std::shared_ptr<SecurityAttributes<Decimal>> attributesOfSecurity)
  {
    if (attributesOfSecurity->isEquitySecurity())
      return TradingVolume::SHARES;
    else
      return TradingVolume::CONTRACTS;
  }

  static std::shared_ptr<SecurityAttributes<Decimal>> createSecurityAttributes (const std::string &symbol)
  {
    auto& factory = SecurityAttributesFactory<Decimal>::instance();
    auto it = factory.getSecurityAttributes (symbol);

    if (it != factory.endSecurityAttributes())
      return it->second;
    else
      throw McptConfigurationFileReaderException("createSecurityAttributes - ticker symbol " +symbol +" is unkown");
  }

  static std::shared_ptr<TimeSeriesCsvReader<Decimal>> getHistoricDataFileReader(
              const std::string& historicDataFilePath,
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
      throw McptConfigurationFileReaderException("Historic data file format " +dataFileFormatStr +" not recognized");

    return std::make_shared<TradeStationFormatCsvReader<Decimal>>(
            historicDataFilePath, timeFrame, unitsOfVolume, tickValue);
   }

  std::shared_ptr<DataSourceReader> getDataSourceReader(
        std::string dataSourceName, 
        std::string apiKey) 
  {
    if(boost::iequals(dataSourceName, "finnhub")) 
      return std::make_shared<FinnhubIOReader>(apiKey);
    else if(boost::iequals(dataSourceName, "barchart"))
      return std::make_shared<BarchartReader>(apiKey);
    else
      throw McptConfigurationFileReaderException("Data source " + dataSourceName + " not recognized");
  }

  std::string getApiTokenFromFile(std::string apiConfigFilename, std::string dataSourceName) 
  {
    std::string source, token = "";
    io::CSVReader<2> csvApiConfig(apiConfigFilename.c_str());
    csvApiConfig.set_header("Source", "Token");

    while(csvApiConfig.read_row(source, token))
      if(boost::iequals(dataSourceName, source)) 
        break;

    if(token.empty()) 
      throw McptConfigurationFileReaderException(
          "Source " + dataSourceName + " does not exist in " + apiConfigFilename);

    return token;
  }
}
