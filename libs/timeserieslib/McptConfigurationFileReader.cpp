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

using namespace boost::filesystem;
//extern PriceActionLabSystem* parsePALCode();
//extern FILE *yyin;

using Decimal7 = dec::decimal<7>;

namespace mkc_timeseries
{
  static std::shared_ptr<TimeSeriesCsvReader<Decimal7>> 
  getHistoricDataFileReader(const std::string& historicDataFilePath,
			    const std::string& dataFileFormatStr, 
			    TimeFrame::Duration timeFrame,
			    TradingVolume::VolumeUnit unitsOfVolume);

  static std::shared_ptr<SecurityAttributes<Decimal7>> createSecurityAttributes (const std::string &tickerSymbol);
  static TradingVolume::VolumeUnit getVolumeUnit (std::shared_ptr<SecurityAttributes<Decimal7>> attributesOfSecurity);
  static std::shared_ptr<mkc_timeseries::Security<Decimal7>> 
  createSecurity (std::shared_ptr<SecurityAttributes<Decimal7>> attributes,
		  std::shared_ptr<TimeSeriesCsvReader<Decimal7>> aReader);

  static std::shared_ptr<BackTester<Decimal7>> getBackTester(TimeFrame::Duration theTimeFrame,
							 const DateRange& backtestingDates);

  McptConfigurationFileReader::McptConfigurationFileReader (const std::string& configurationFileName) 
    : mConfigurationFileName(configurationFileName)
  {}

  std::shared_ptr<McptConfiguration<Decimal7>> McptConfigurationFileReader::readConfigurationFile()
  {
    io::CSVReader<9> csvConfigFile(mConfigurationFileName.c_str());

    csvConfigFile.set_header("Symbol", "IRPath", "DataPath","FileFormat","ISDateStart",
			     "ISDateEnd", "OOSDateStart", "OOSDateEnd", "TimeFrame");

    std::string tickerSymbol, palIRFilePathStr, historicDataFilePathStr, historicDataFormatStr;
    std::string inSampleStartDate, inSampleEndDate, oosStartDate, oosEndDate;
    std::string timeFrameStr;

    boost::gregorian::date insampleDateStart, insampleDateEnd, oosDateStart, oosDateEnd;

    csvConfigFile.read_row (tickerSymbol, palIRFilePathStr, historicDataFilePathStr,
			    historicDataFormatStr, inSampleStartDate, inSampleEndDate,
			    oosStartDate, oosEndDate, timeFrameStr);

  
    insampleDateStart = boost::gregorian::from_undelimited_string(inSampleStartDate);
    insampleDateEnd = boost::gregorian::from_undelimited_string(inSampleEndDate);

    DateRange inSampleDates(insampleDateStart, insampleDateEnd);

    oosDateStart = boost::gregorian::from_undelimited_string(oosStartDate);
    oosDateEnd = boost::gregorian::from_undelimited_string(oosEndDate);

    DateRange ooSampleDates( oosDateStart, oosDateEnd);
 
    if (oosDateStart <= insampleDateEnd)
      throw McptConfigurationFileReaderException("McptConfigurationFileReader::readConfigurationFile - OOS start date starts before insample end date");

    boost::filesystem::path irFilePath (palIRFilePathStr);

    if (!exists (irFilePath))
      throw McptConfigurationFileReaderException("PAL IR path " +irFilePath.string() +" does not exist");

    boost::filesystem::path historicDataFilePath (historicDataFilePathStr);
    if (!exists (historicDataFilePath))
      throw McptConfigurationFileReaderException("Historic data file path " +historicDataFilePath.string() +" does not exist");

    std::shared_ptr<SecurityAttributes<Decimal7>> attributes = createSecurityAttributes (tickerSymbol);
    TimeFrame::Duration backTestingTimeFrame = getTimeFrameFromString(timeFrameStr);

    std::shared_ptr<TimeSeriesCsvReader<Decimal7>> reader = getHistoricDataFileReader(historicDataFilePathStr,
									       historicDataFormatStr, 
									       backTestingTimeFrame,
									       getVolumeUnit(attributes));
    reader->readFile();

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

    // Constructor driver (facade) that will parse the IR and return
    // and AST representation
    mkc_palast::PalParseDriver driver (irFilePath.string());

    // Read the IR file
    
    driver.Parse();

    std::cout << "Parsing successfully completed." << std::endl << std::endl;
    PriceActionLabSystem* system = driver.getPalStrategies();
    std::cout << "Total number IR patterns = " << system->getNumPatterns() << std::endl;
    std::cout << "Total long IR patterns = " << system->getNumLongPatterns() << std::endl;
    std::cout << "Total short IR patterns = " << system->getNumShortPatterns() << std::endl;   
    //yyin = fopen (irFilePath.string().c_str(), "r");
    //PriceActionLabSystem* system = parsePALCode();

    //fclose (yyin);

    return std::make_shared<McptConfiguration<Decimal7>>(getBackTester(backTestingTimeFrame, ooSampleDates),
						  getBackTester(backTestingTimeFrame, inSampleDates),
						  createSecurity (attributes, reader),
						  system, inSampleDates, ooSampleDates);
  }

  static std::shared_ptr<BackTester<Decimal7>> getBackTester(TimeFrame::Duration theTimeFrame,
							 const DateRange& backtestingDates)
  {
    if (theTimeFrame == TimeFrame::DAILY)
      return std::make_shared<DailyBackTester<Decimal7>>(backtestingDates.getFirstDate(), 
						  backtestingDates.getLastDate());
    else if (theTimeFrame == TimeFrame::WEEKLY)
      return std::make_shared<WeeklyBackTester<Decimal7>>(backtestingDates.getFirstDate(), 
						   backtestingDates.getLastDate());
    else if (theTimeFrame == TimeFrame::MONTHLY)
      return std::make_shared<MonthlyBackTester<Decimal7>>(backtestingDates.getFirstDate(), 
						    backtestingDates.getLastDate());
    else
      throw McptConfigurationFileReaderException("getBacktester - cannot create backtester for time frame other than daily or monthly");
  }

  static std::shared_ptr<mkc_timeseries::Security<Decimal7>> 
  createSecurity (std::shared_ptr<SecurityAttributes<Decimal7>> attributes,
		  std::shared_ptr<TimeSeriesCsvReader<Decimal7>> aReader)
  {
    if (attributes->isEquitySecurity())
      {
	if (attributes->isFund())
	  {
	    return std::make_shared<EquitySecurity<Decimal7>>(attributes->getSymbol(), 
						       attributes->getName(),
						       aReader->getTimeSeries());
	  }
	else if (attributes->isCommonStock())
	  {
	    return std::make_shared<EquitySecurity<Decimal7>>(attributes->getSymbol(), 
						       attributes->getName(),
						       aReader->getTimeSeries());
	  }
	else
	  throw McptConfigurationFileReaderException("Unknown security attribute");
      }
    else
      return std::make_shared<FuturesSecurity<Decimal7>>(attributes->getSymbol(), 
						  attributes->getName(),
						  attributes->getBigPointValue(),
						  attributes->getTick(),
						  aReader->getTimeSeries());
      
  }

		
  static TradingVolume::VolumeUnit getVolumeUnit (std::shared_ptr<SecurityAttributes<Decimal7>> attributesOfSecurity)
  {
    if (attributesOfSecurity->isEquitySecurity())
      return TradingVolume::SHARES;
    else
      return TradingVolume::CONTRACTS;
  }

  static std::shared_ptr<SecurityAttributes<Decimal7>> createSecurityAttributes (const std::string &symbol)
  {
    SecurityAttributesFactory<Decimal7> factory;
    SecurityAttributesFactory<Decimal7>::SecurityAttributesIterator it = factory.getSecurityAttributes (symbol);

    if (it != factory.endSecurityAttributes())
      return it->second;
    else
      throw McptConfigurationFileReaderException("createSecurityAttributes - ticker symbol " +symbol +" is unkown");
  }

  
  static std::shared_ptr<TimeSeriesCsvReader<Decimal7>> 
  getHistoricDataFileReader(const std::string& historicDataFilePath,
			    const std::string& dataFileFormatStr, 
			    TimeFrame::Duration timeFrame,
			    TradingVolume::VolumeUnit unitsOfVolume)
  {
    std::string upperCaseFormatStr = boost::to_upper_copy(dataFileFormatStr);

    if (upperCaseFormatStr == std::string("PAL"))
      return std::make_shared<PALFormatCsvReader<Decimal7>>(historicDataFilePath, timeFrame, unitsOfVolume);
    else if (upperCaseFormatStr == std::string("TradeStation"))
            return std::make_shared<TradeStationFormatCsvReader<Decimal7>>(historicDataFilePath, timeFrame, unitsOfVolume);
    else
      throw McptConfigurationFileReaderException("Historic data file format " +dataFileFormatStr +" not recognized");
  }
}
