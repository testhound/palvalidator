// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "csv.h"
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
    // Check if the file has a header row by reading the first line
    io::CSVReader<9> csvConfigFileCheck(mConfigurationFileName.c_str());
    char* firstLine = csvConfigFileCheck.next_line();
    bool hasHeader = false;
    if (firstLine) {
        std::string firstLineStr(firstLine);
        // Check if first line contains header keywords
        hasHeader = (firstLineStr.find("Symbol") != std::string::npos &&
                    firstLineStr.find("IRPath") != std::string::npos &&
                    firstLineStr.find("DataPath") != std::string::npos);
    }
    
    // Create the main CSV reader
    io::CSVReader<9> csvConfigFile(mConfigurationFileName.c_str());
    
    if (hasHeader) {
        csvConfigFile.read_header(io::ignore_no_column, "Symbol", "IRPath", "DataPath","FileFormat","ISDateStart",
                     "ISDateEnd", "OOSDateStart", "OOSDateEnd", "TimeFrame");
    } else {
        csvConfigFile.set_header("Symbol", "IRPath", "DataPath","FileFormat","ISDateStart",
                     "ISDateEnd", "OOSDateStart", "OOSDateEnd", "TimeFrame");
    }

    std::string tickerSymbol, palIRFilePathStr, historicDataFilePathStr, historicDataFormatStr;
    std::string inSampleStartDate, inSampleEndDate, oosStartDate, oosEndDate;
    std::string timeFrameStr;


    csvConfigFile.read_row (tickerSymbol, palIRFilePathStr, historicDataFilePathStr,
       historicDataFormatStr, inSampleStartDate, inSampleEndDate,
       oosStartDate, oosEndDate, timeFrameStr);


    // Parse dates - handle both YYYYMMDD (gregorian) and YYYYMMDDTHHMMSS (ptime) formats
    // Enforce format consistency - all dates must use the same format
    bool isInSamplePtimeFormat = (inSampleStartDate.length() > 8 || inSampleEndDate.length() > 8);
    bool isOosPtimeFormat = (oosStartDate.length() > 8 || oosEndDate.length() > 8);
    
    // Check for format consistency
    if (isInSamplePtimeFormat != isOosPtimeFormat) {
        throw ValidatorConfigurationException("ValidatorConfigurationFileReader::readConfigurationFile - Date format inconsistency: all dates must use either YYYYMMDD or YYYYMMDDTHHMMSS format");
    }
    
    // Check that within each date range, both dates use the same format
    if ((inSampleStartDate.length() > 8) != (inSampleEndDate.length() > 8)) {
        throw ValidatorConfigurationException("ValidatorConfigurationFileReader::readConfigurationFile - In-sample date format inconsistency: start and end dates must use the same format");
    }
    
    if ((oosStartDate.length() > 8) != (oosEndDate.length() > 8)) {
        throw ValidatorConfigurationException("ValidatorConfigurationFileReader::readConfigurationFile - Out-of-sample date format inconsistency: start and end dates must use the same format");
    }
    
    // NEW: Preserve ptime information when detected
    DateRange inSampleDates = [&]() {
        if (isInSamplePtimeFormat) {
            boost::posix_time::ptime ptimeStart = boost::posix_time::from_iso_string(inSampleStartDate);
            boost::posix_time::ptime ptimeEnd = boost::posix_time::from_iso_string(inSampleEndDate);
            return DateRange(ptimeStart, ptimeEnd);  // Preserves time precision
        } else {
            // Backward compatibility for gregorian dates
            boost::gregorian::date insampleDateStart = boost::gregorian::from_undelimited_string(inSampleStartDate);
            boost::gregorian::date insampleDateEnd = boost::gregorian::from_undelimited_string(inSampleEndDate);
            return DateRange(insampleDateStart, insampleDateEnd);
        }
    }();

    DateRange ooSampleDates = [&]() {
        if (isOosPtimeFormat) {
            boost::posix_time::ptime ptimeStart = boost::posix_time::from_iso_string(oosStartDate);
            boost::posix_time::ptime ptimeEnd = boost::posix_time::from_iso_string(oosEndDate);
            return DateRange(ptimeStart, ptimeEnd);  // Preserves time precision
        } else {
            // Backward compatibility for gregorian dates
            boost::gregorian::date oosDateStart = boost::gregorian::from_undelimited_string(oosStartDate);
            boost::gregorian::date oosDateEnd = boost::gregorian::from_undelimited_string(oosEndDate);
            return DateRange(oosDateStart, oosDateEnd);
        }
    }();

    if (ooSampleDates.getFirstDateTime() <= inSampleDates.getLastDateTime())
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

    // Validate that the in-sample start time is not too far before the time series start
    boost::posix_time::ptime timeSeriesStartDateTime = reader->getTimeSeries()->getFirstDateTime();
    if (inSampleDates.getFirstDateTime() < timeSeriesStartDateTime)
      {
        boost::posix_time::time_duration timeBetween = timeSeriesStartDateTime - inSampleDates.getFirstDateTime();
        
        // Calculate maximum allowed gap based on the time series interval
        boost::posix_time::time_duration maxAllowedGap;
        if (backTestingTimeFrame == TimeFrame::INTRADAY && reader->getTimeSeries()->getNumEntries() >= 2) {
            // For intraday data, use the actual interval and allow up to 10 intervals worth of gap
            try {
                boost::posix_time::time_duration interval = reader->getTimeSeries()->getIntradayTimeFrameDuration();
                maxAllowedGap = interval * 10;  // Allow up to 10 intervals
            } catch (const std::exception&) {
                // Fallback to 10 days if interval calculation fails
                maxAllowedGap = boost::posix_time::hours(240);
            }
        } else {
            // For EOD data or when interval can't be determined, use 10 days
            maxAllowedGap = boost::posix_time::hours(240);
        }
        
        if (timeBetween > maxAllowedGap)
          {
            std::string inSampleDateStr = boost::posix_time::to_simple_string(inSampleDates.getFirstDateTime());
            std::string timeSeriesDateStr = boost::posix_time::to_simple_string(timeSeriesStartDateTime);

            throw ValidatorConfigurationException (std::string("Time gap between configuration file IS start time of ") +inSampleDateStr +std::string(" and TimeSeries start time of ") +timeSeriesDateStr +std::string(" is greater than allowed maximum"));
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

    // SIMPLIFIED: Constructor call without BackTester creation
    return std::make_shared<ValidatorConfiguration<Decimal>>(security, system, inSampleDates, ooSampleDates);
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
    else if (upperCaseFormatStr == std::string("INTRADAY::TRADESTATION"))
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
