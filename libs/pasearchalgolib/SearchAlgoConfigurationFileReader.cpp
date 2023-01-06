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
              const std::shared_ptr<McptConfiguration<Decimal>>& mcptConfiguration)
  {
    const std::shared_ptr<Security<Decimal>> security = mcptConfiguration->getSecurity();
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
    
    return std::make_shared<SearchAlgoConfiguration<Decimal>>(tryCast<unsigned int>(maxDepth),
                                                              tryCast<unsigned int>(minTrades),
                                                              Decimal(tryCast<double>(activityMultiplier)),
                                                              tryCast<unsigned int>(passingStratNumPerRound),
                                                              Decimal(tryCast<double>(profitFactorCritierion)),
                                                              tryCast<unsigned int>(maxConsecutiveLosers),
                                                              tryCast<unsigned int>(maxInactivitySpan),
                                                              targetStops,
                                                              tryCast<unsigned int>(numPermutations),
                                                              tryCast<unsigned int>(numStratsFull),
                                                              tryCast<unsigned int>(numStratsBeforeValidation),
                                                              Decimal(palSafetyDbl),
                                                              Decimal(tryCast<double>(stepRedundancyMultiplier)),
                                                              Decimal(tryCast<double>(survivalFilterMultiplier))
                                                              );

  }

}
