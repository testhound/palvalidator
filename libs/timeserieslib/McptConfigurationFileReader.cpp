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
#include "SecurityFactory.h"
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include <cstdio>
#include "number.h"
#include "DataSourceReader.h"
#include "HistoricDataReader.h"

using namespace boost::filesystem;
//extern PriceActionLabSystem* parsePALCode();
//extern FILE *yyin;

using Decimal = num::DefaultNumber;

namespace mkc_timeseries
{
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

    std::shared_ptr<SecurityAttributes<Decimal>> attributes = getSecurityAttributes<Decimal> (tickerSymbol);
    TimeFrame::Duration backTestingTimeFrame = getTimeFrameFromString(timeFrameStr);

    std::string dataFilename = mRunParameters->getEodDataFilePath();
    std::shared_ptr<HistoricDataReader<Decimal>> historicDataReader;

    if (!mRunParameters->shouldUseApi())
    {
      enum HistoricDataReader<Decimal>::HistoricDataFileFormat historicFileFormat =
        HistoricDataReaderFactory<Decimal>::getFileFormatFromString(fileFormat);
      historicDataReader = HistoricDataReaderFactory<Decimal>::createHistoricDataReader(mRunParameters->getEodDataFilePath(),
                                                                                        historicFileFormat, backTestingTimeFrame,
                                                                                        attributes->getVolumeUnits(),
                                                                                        attributes->getTick());
    }
    else
    {
      std::string token = DataSourceReaderFactory::getApiTokenFromFile(mRunParameters->getApiConfigFilePath(), mRunParameters->getApiSource());
      enum HistoricDataReader<Decimal>::HistoricDataApi apiSource =
        HistoricDataReaderFactory<Decimal>::getApiFromString(mRunParameters->getApiSource());
      historicDataReader = HistoricDataReaderFactory<Decimal>::createHistoricDataReader(tickerSymbol,
                                                                                        apiSource, token,
                                                                                        DateRange(insampleDateStart, oosDateEnd),
                                                                                        backTestingTimeFrame);
    }

    historicDataReader->read();

    //dataSourceReader->destroyFiles(); // TODO: delete temp files

    //  insampleDateStart
    boost::gregorian::date timeSeriesStartDate = historicDataReader->getTimeSeries()->getFirstDate();
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

    return std::make_shared<McptConfiguration<Decimal>>(BackTesterFactory<Decimal>::getBackTester(backTestingTimeFrame, ooSampleDates),
							BackTesterFactory<Decimal>::getBackTester(backTestingTimeFrame, inSampleDates),
							SecurityFactory<Decimal>::createSecurity (tickerSymbol,
												  historicDataReader->getTimeSeries()),
							system, inSampleDates, ooSampleDates,
							dataFilename);
  }
}
