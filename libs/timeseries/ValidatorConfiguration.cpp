// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "ValidatorConfiguration.h"
#include "PalParseDriver.h"
#include "TimeFrameUtility.h"
#include "TimeSeriesEntry.h"
#include "TimeSeriesCsvReader.h"
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include "SecurityFactory.h"
#include <cstdio>
#include "number.h"

using namespace boost::filesystem;

using Decimal = num::DefaultNumber;

namespace mkc_timeseries
{
  static std::shared_ptr<TimeSeriesCsvReader<Decimal>>
  getHistoricDataFileReader(const std::string& tickerSymbol,
			    const std::string& historicDataFilePath,
			    const std::string& dataFileFormatStr,
			    TimeFrame::Duration timeFrame);

  static std::shared_ptr<TimeSeriesCsvReader<Decimal>>
  getHistoricDataFileReader(const std::string& historicDataFilePath,
			    const std::string& dataFileFormatStr,
			    TimeFrame::Duration timeFrame,
			    TradingVolume::VolumeUnit unitsOfVolume,
			    const Decimal& tickValue);

  ValidatorConfigurationFileReader::ValidatorConfigurationFileReader (const std::string& configurationFileName)
    : mConfigurationFileName(configurationFileName)
  {}

  std::shared_ptr<ValidatorConfiguration<Decimal>> ValidatorConfigurationFileReader::readConfigurationFile()
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
      throw ValidatorConfigurationException("ValidatorConfigurationFileReader::readConfigurationFile - OOS start date starts before insample end date");

    boost::filesystem::path irFilePath (palIRFilePathStr);

    if (!exists (irFilePath))
      throw ValidatorConfigurationException("PAL IR path " +irFilePath.string() +" does not exist");

    boost::filesystem::path historicDataFilePath (historicDataFilePathStr);
    if (!exists (historicDataFilePath))
      throw ValidatorConfigurationException("Historic data file path " +historicDataFilePath.string() +" does not exist");


    
    TimeFrame::Duration backTestingTimeFrame = getTimeFrameFromString(timeFrameStr);

    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = getHistoricDataFileReader(tickerSymbol,
										     historicDataFilePathStr,
										     historicDataFormatStr,
										     backTestingTimeFrame);

    reader->readFile();

    auto security = SecurityFactory<Decimal>::createSecurity(tickerSymbol, reader->getTimeSeries());

    //  insampleDateStart
    boost::gregorian::date timeSeriesStartDate = reader->getTimeSeries()->getFirstDate();
    if (insampleDateStart < timeSeriesStartDate)

      {
	boost::gregorian::date_period daysBetween(insampleDateStart, timeSeriesStartDate);
	if (daysBetween.length().days() > 10)
	  {
	    std::string inSampleDateStr(boost::gregorian::to_simple_string (insampleDateStart));
	    std::string timeSeriesDateStr(boost::gregorian::to_simple_string (timeSeriesStartDate));

	    throw ValidatorConfigurationException (std::string("Number of days between configuration file IS start date of ") +inSampleDateStr +std::string(" and TimeSeries start date of ") +timeSeriesDateStr +std::string(" is greater than 10 days"));
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

    auto oosBackTester = BackTesterFactory<Decimal>::getBackTester(backTestingTimeFrame, ooSampleDates);
    auto isBackTester  = BackTesterFactory<Decimal>::getBackTester(backTestingTimeFrame, inSampleDates);
    return std::make_shared<ValidatorConfiguration<Decimal>>(oosBackTester, isBackTester, security,
							     system, inSampleDates, ooSampleDates);
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
      throw ValidatorConfigurationException("Historic data file format " +dataFileFormatStr +" not recognized");
  }

  static std::shared_ptr<TimeSeriesCsvReader<Decimal>>
  getHistoricDataFileReader(const std::string& tickerSymbol,
			    const std::string& historicDataFilePath,
			    const std::string& dataFileFormatStr,
			    TimeFrame::Duration timeFrame)
  {
    std::shared_ptr<SecurityAttributes<Decimal>> securityAttributes = getSecurityAttributes<Decimal> (tickerSymbol);

    return getHistoricDataFileReader(historicDataFilePath,
				     dataFileFormatStr,
				     timeFrame,
				     securityAttributes->getVolumeUnits(),
				     securityAttributes->getTick());
  }
  
}
