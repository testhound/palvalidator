// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#include "SearchAlgoConfigurationFileReader.h"


using namespace boost::filesystem;
//extern PriceActionLabSystem* parsePALCode();
//extern FILE *yyin;

using Decimal = num::DefaultNumber;

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  static TradingVolume::VolumeUnit getVolumeUnit (std::shared_ptr<Security<Decimal>> security)
  {
    if (security->isEquitySecurity())
      return TradingVolume::SHARES;
    else
      return TradingVolume::CONTRACTS;
  }

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


  SearchAlgoConfigurationFileReader::SearchAlgoConfigurationFileReader (
    const std::shared_ptr<RunParameters>& RunParameters)
    : mRunParameters(RunParameters)
  {}

  std::shared_ptr<SearchAlgoConfiguration<Decimal>> SearchAlgoConfigurationFileReader::readConfigurationFile(
              const std::shared_ptr<McptConfiguration<Decimal>>& mcptConfiguration, 
              int timeFrameIdToLoad, 
              bool downloadFile)
  {
    const std::shared_ptr<Security<Decimal>> security = mcptConfiguration->getSecurity();
    std::cout << "Time frame id started: " << timeFrameIdToLoad << std::endl;
    //io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    io::CSVReader<12, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> csvConfigFile(mRunParameters->getSearchConfigFilePath().c_str());

    csvConfigFile.read_header(io::ignore_extra_column, "MaxDepth", "MinTrades", "ActivityMultiplier","PassingStratNumPerRound","ProfitFactorCriterion", "MaxConsecutiveLosers",
                             "MaxInactivitySpan", "TargetsToSearchConfigFilePath", "ValidationConfigFilePath", "PALSafetyFactor",
                              "StepRedundancyMultiplier", "SurvivalFilterMultiplier");

    std::string maxDepth, minTrades, activityMultiplier, passingStratNumPerRound, profitFactorCritierion, maxConsecutiveLosers;
    std::string maxInactivitySpan, targetsToSearchConfigFilePath;
    std::string validationConfigFilePath, palSafetyFactor, stepRedundancyMultiplier, survivalFilterMultiplier;

    csvConfigFile.read_row (maxDepth, minTrades, activityMultiplier, passingStratNumPerRound, profitFactorCritierion, maxConsecutiveLosers,
                            maxInactivitySpan, targetsToSearchConfigFilePath,
                            validationConfigFilePath, palSafetyFactor, stepRedundancyMultiplier, survivalFilterMultiplier);

    double palSafetyDbl = tryCast<double>(palSafetyFactor);
    if (palSafetyDbl > 0.9 || palSafetyDbl < 0.7)
      throw SearchAlgoConfigurationFileReaderException("PALSafetyFactor needs to be in the range of 0.7 and 0.9. Factor provided " + std::to_string(palSafetyDbl));

    boost::filesystem::path validationFile (validationConfigFilePath);
    if (!exists (validationFile))
      throw SearchAlgoConfigurationFileReaderException("Validation config file path: " + validationFile.string() + " does not exist");

    std::string numPermutations, numStratsFull, numStratsBeforeValidation;
    io::CSVReader<3, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> validationCsv(validationConfigFilePath);
    //validationCsv.set_header("NumPermutations", "NumStratsFullPeriod", "NumStratsBeforeValidation");
    validationCsv.read_header(io::ignore_extra_column, "NumPermutations", "NumStratsFullPeriod", "NumStratsBeforeValidation");
    validationCsv.read_row(numPermutations, numStratsFull, numStratsBeforeValidation);


    boost::filesystem::path targetsFile (targetsToSearchConfigFilePath);
    if (!exists (targetsFile))
      throw SearchAlgoConfigurationFileReaderException("Targets to search config file path: " + targetsFile.string() + " does not exist");

    std::vector<std::pair<Decimal, Decimal>> targetStops;
    io::CSVReader<2, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> targetsCsv(targetsToSearchConfigFilePath);
    //targetsCsv.set_header("TargetMultiplier", "StopMultiplier");
    targetsCsv.read_header(io::ignore_extra_column, "TargetMultiplier", "StopMultiplier");

    std::string target, stop;
    while (targetsCsv.read_row(target, stop))
      {
        targetStops.push_back(std::make_pair(Decimal(tryCast<float>(target)), Decimal(tryCast<float>(stop))));
      }
    
    std::string hourlyDataFilePath = mRunParameters->getHourlyDataFilePath();
    if(mRunParameters->shouldUseApi()) 
    {
      // read data from API from the start of the IS data to the end of the OOS data
      std::string token = getApiTokenFromFile(mRunParameters->getApiConfigFilePath(), mRunParameters->getApiSource());
      DateRange dataReaderDateRange(mcptConfiguration->getInsampleDateRange().getFirstDate(), mcptConfiguration->getOosDateRange().getLastDate());
      std::shared_ptr<DataSourceReader> dataSourceReader = getDataSourceReader(mRunParameters->getApiSource(), token);
      hourlyDataFilePath = dataSourceReader->createTemporaryFile(security->getSymbol(), "hourly", dataReaderDateRange, dataReaderDateRange, downloadFile);
    }

    if (static_cast<size_t>(timeFrameIdToLoad) > mRunParameters->getTimeFrames().size() || timeFrameIdToLoad < 0)
      throw SearchAlgoConfigurationFileReaderException("Invalid timeFrameIdToLoad: " + std::to_string(timeFrameIdToLoad) + " timeframes size: " + std::to_string(mRunParameters->getTimeFrames().size()) + ".");

    std::shared_ptr<OHLCTimeSeries<Decimal>> series;
    if (timeFrameIdToLoad == 0)
    {
      series = std::make_shared<OHLCTimeSeries<Decimal>>(*security->getTimeSeries());

      // read the data file, infer the time frames, and create all of the synthetic files - once
      if(downloadFile) 
      {
        std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
          hourlyDataFilePath, TimeFrame::INTRADAY, getVolumeUnit(security), security->getTick()
        );
        reader->readFile();

        std::shared_ptr<TimeFrameDiscovery<Decimal>> timeFrameDiscovery = std::make_shared<TimeFrameDiscovery<Decimal>>(reader->getTimeSeries());
        timeFrameDiscovery->inferTimeFrames();
        mRunParameters->setTimeFrames(timeFrameDiscovery->getTimeFrames());

        std::shared_ptr<SyntheticTimeSeriesCreator<Decimal>> syntheticTimeSeriesCreator = 
          std::make_shared<SyntheticTimeSeriesCreator<Decimal>>(reader->getTimeSeries(), hourlyDataFilePath);
        std::shared_ptr<TimeSeriesValidator> validator = std::make_shared<TimeSeriesValidator>(reader->getTimeSeries(), security->getTimeSeries());
        validator->validate();

        for(int i = 0; i < timeFrameDiscovery->numTimeFrames(); i++) 
        {
          boost::posix_time::time_duration timeStamp = timeFrameDiscovery->getTimeFrame(i);
          syntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, timeStamp);
          syntheticTimeSeriesCreator->writeTimeFrameFile(i+1);
        }
      }
    }
    else 
    {
      std::string timeFrameFilename = hourlyDataFilePath + "_timeframe_" + std::to_string(timeFrameIdToLoad);
      std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<PALFormatCsvReader<Decimal>>(
          timeFrameFilename, TimeFrame::DAILY, getVolumeUnit(security), security->getTick()
      );
      reader->readFile();
      series = reader->getTimeSeries();

      typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator it = security->getTimeSeries()->beginRandomAccess();
      for (; it != security->getTimeSeries()->endRandomAccess(); it++)
      {
        const Decimal& cOpen = security->getTimeSeries()->getOpenValue (it, 0);
        const Decimal& cHigh = security->getTimeSeries()->getHighValue (it, 0);
        const Decimal& cLow = security->getTimeSeries()->getLowValue (it, 0);
        const Decimal& cClose = security->getTimeSeries()->getCloseValue (it, 0);

        auto dt = security->getTimeSeries()->getDateValue(it, 0);
        if (!series->isDateFound(dt))
          series->addEntry(OHLCTimeSeriesEntry<Decimal>(dt, cOpen, cHigh, cLow, cClose, DecimalConstants<Decimal>::DecimalZero, security->getTimeSeries()->getTimeFrame()));
        else
        {
          std::cout << "First date found in file: " << dt << ", no more mixing." << std::endl;
          break;
        }
      }
      std::cout << "First date random access: " << series->getDateValue(series->beginRandomAccess(),0) << std::endl;
      std::cout << "First date sorted access: " << series->getFirstDate() << std::endl;
      series->syncronizeMapAndArray();
    }

    return std::make_shared<SearchAlgoConfiguration<Decimal>>(tryCast<unsigned int>(maxDepth),
                                                              tryCast<unsigned int>(minTrades),
                                                              Decimal(tryCast<double>(activityMultiplier)),
                                                              tryCast<unsigned int>(passingStratNumPerRound),
                                                              Decimal(tryCast<double>(profitFactorCritierion)),
                                                              tryCast<unsigned int>(maxConsecutiveLosers),
                                                              tryCast<unsigned int>(maxInactivitySpan),
                                                              targetStops,
                                                              mRunParameters->getTimeFrames(),
                                                              series,
                                                              tryCast<unsigned int>(numPermutations),
                                                              tryCast<unsigned int>(numStratsFull),
                                                              tryCast<unsigned int>(numStratsBeforeValidation),
                                                              Decimal(palSafetyDbl),
                                                              Decimal(tryCast<double>(stepRedundancyMultiplier)),
                                                              Decimal(tryCast<double>(survivalFilterMultiplier))
                                                              );

  }

}
