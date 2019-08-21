

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "SearchAlgoConfigurationFileReader.h"
#include "PalParseDriver.h"
#include "TimeFrameUtility.h"
#include "TimeSeriesEntry.h"
#include "TimeSeriesCsvReader.h"
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include <cstdio>
#include "number.h"
#include "boost/lexical_cast.hpp"
#include "boost/lexical_cast/bad_lexical_cast.hpp"
#include "typeinfo"

using namespace boost::filesystem;
//extern PriceActionLabSystem* parsePALCode();
//extern FILE *yyin;

using Decimal = num::DefaultNumber;

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  template <class T>
  static T tryCast(std::string inputString)
  {
    try
    {
      return boost::lexical_cast<T>(inputString);
    }
    catch (const boost::bad_lexical_cast & e)
    {
        std::string exceptionStr(std::string("Exception caught when trying to cast: ") + inputString + std::string(" as ") + typeid(T).name() + std::string("."));
        std::cout << exceptionStr << "\nException: " << e.what() << "!" << std::endl;
        throw SearchAlgoConfigurationFileReaderException(exceptionStr);
    }
    catch (const std::exception& e)
    {
      throw SearchAlgoConfigurationFileReaderException(std::string("Undefined exception encountered in tryCast. Exception details: " + std::string(e.what())));
    }
  }


  SearchAlgoConfigurationFileReader::SearchAlgoConfigurationFileReader (const std::string& configurationFileName)
    : mConfigurationFileName(configurationFileName)
  {}

  std::shared_ptr<SearchAlgoConfiguration<Decimal>> SearchAlgoConfigurationFileReader::readConfigurationFile()
  {
    io::CSVReader<9> csvConfigFile(mConfigurationFileName.c_str());

    csvConfigFile.set_header("MaxDepth", "MinTrades", "SortMultiplier","PassingStratNumPerRound","ProfitFactorCriterion", "MaxConsecutiveLosers",
                             "MaxInactivitySpan", "TargetsToSearchConfigFilePath", "TimeFramesToSearchConfigFilePath");

    std::string maxDepth, minTrades, sortMultiplier, passingStratNumPerRound, profitFactorCritierion, maxConsecutiveLosers;
    std::string maxInactivitySpan, targetsToSearchConfigFilePath, timeFramesToSearchConfigFilePath;


    csvConfigFile.read_row (maxDepth, minTrades, sortMultiplier, passingStratNumPerRound, profitFactorCritierion, maxConsecutiveLosers,
                            maxInactivitySpan, targetsToSearchConfigFilePath, timeFramesToSearchConfigFilePath);


    boost::filesystem::path targetsFile (targetsToSearchConfigFilePath);
    if (!exists (targetsFile))
      throw SearchAlgoConfigurationFileReaderException("Targets to search config file path: " + targetsFile.string() + " does not exist");


    bool reading = true;
    std::vector<std::pair<Decimal, Decimal>> targetStops;
    io::CSVReader<2> targetsCsv(targetsToSearchConfigFilePath);
    targetsCsv.set_header("TargetMultiplier", "StopMultiplier");

    while (reading)
      {
        std::string target, stop;
        reading = targetsCsv.read_row(target, stop);
        if (reading)
          targetStops.push_back(std::make_pair(Decimal(tryCast<float>(target)), Decimal(tryCast<float>(stop))));
      }

    boost::filesystem::path timeFramesFile (timeFramesToSearchConfigFilePath);
    if (!exists (timeFramesFile))
      throw SearchAlgoConfigurationFileReaderException("Timeframe to search config file path: " +  timeFramesFile.string() + " does not exist");

    reading = true;
    std::vector<time_t> timeFrames;
    io::CSVReader<1> timesCsv(timeFramesToSearchConfigFilePath);
    timesCsv.set_header("TimeFrame");

    while (reading)
      {
        std::string timeFrame;
        reading = timesCsv.read_row(timeFrame);
        if (!reading)
          break;
        struct std::tm tm;
        try
        {
          strptime(timeFrame.c_str(), "%H:%M", &tm);
          timeFrames.push_back(std::mktime(&tm));
        }
        catch (const std::exception& e)
        {
            std::cout << "Time conversion exception." << std::endl;
            throw SearchAlgoConfigurationFileReaderException("Time conversion exception in file: " + timeFramesFile.string() + ", when converting: " + timeFrame + "\nException details: " + std::string(e.what()));
        }
      }


    return std::make_shared<SearchAlgoConfiguration<Decimal>>(tryCast<unsigned int>(maxDepth),
                                                              tryCast<unsigned int>(minTrades),
                                                              Decimal(tryCast<float>(sortMultiplier)),
                                                              tryCast<unsigned int>(passingStratNumPerRound),
                                                              Decimal(tryCast<float>(profitFactorCritierion)),
                                                              tryCast<unsigned int>(maxConsecutiveLosers),
                                                              tryCast<unsigned int>(maxInactivitySpan),
                                                              targetStops,
                                                              timeFrames);

  }

}
