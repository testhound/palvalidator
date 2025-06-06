
#include "SearchRun.h"
#include "PatternMatcher.h"
#include "PALMonteCarloValidation.h"
#include "RunParameters.h"

//#include <chrono>
//#include <ctime>

using namespace mkc_searchalgo;
using Num = num::DefaultNumber;

template <template <typename> class _SurvivingStrategyPolicy, typename _McptType>
static void
validateByPermuteMarketChanges (const std::shared_ptr<McptConfiguration<Num>>& configuration, unsigned int numPermutations, std::shared_ptr<PriceActionLabSystem> pal, const std::string& validationOutputFile)
{
  std::cout << "starting validation." << std::endl;
  PALMonteCarloValidation<Num,_McptType,_SurvivingStrategyPolicy> validation(configuration, numPermutations);
  printf ("Starting Monte Carlo Validation tests (Using Permute Market Changes)\n\n");
  validation.runPermutationTests(pal);
  printf ("Exporting surviving MCPT strategies\n");
  typename PALMonteCarloValidation<Num,_McptType,_SurvivingStrategyPolicy>::SurvivingStrategiesIterator it =
      validation.beginSurvivingStrategies();
  std::ofstream mcptPatternsFile(validationOutputFile);
  for (; it != validation.endSurvivingStrategies(); it++)
    LogPalPattern::LogPattern ((*it)->getPalPattern(), mcptPatternsFile);

}

static void validate(const std::shared_ptr<McptConfiguration<Num>>& configuration, unsigned int numPermutations, std::shared_ptr<PriceActionLabSystem> pal, const std::string& validationOutputFile)
{
  validateByPermuteMarketChanges <UnadjustedPValueStrategySelection,
      BestOfMonteCarloPermuteMarketChanges<Num,
      NormalizedReturnPolicy,
      MultiStrategyPermuteMarketChangesPolicy<Num,
      NormalizedReturnPolicy<Num>>>>
      (configuration,
       numPermutations, pal, validationOutputFile);
}

static int usage_error(const std::vector<std::string>& args)
{
  std::cout << "wrong usage, " << args.size() << " arguments specified: ";
  for (auto arg: args)
    std::cout << arg << ".";
  std::cout << std::endl;
  std::cout << "Correct usage is:... [configFileName] [searchConfigFileName] [longonly/shortonly/longshort] [IS/OOS/ISOOS] [PATTERN_SEARCH_TYPE] [MODE] [--LOCAL/API:{SOURCE}] [[API Config file] OR [Daily File] [Hourly File]]" << std::endl << std::endl;
  std::cout << "  Where a typical run could be something like: "<< std::endl;
  std::cout << "     ./PalValidator %config1.txt %conig2.txt longshortIS 4 threads:8 --api:finnhub api.config" << std::endl << std::endl;

  std::cout << "  IS - In-Sample only" << std::endl;
  std::cout << "  OOS - Out of Sample only" << std::endl;
  std::cout << "  ISOOS - In Sample and Out of Sample in a single run" << std::endl << std::endl;

  std::cout << "PATTERN_SEARCH_TYPE possible values: " << std::endl;
  std::cout << "  0 - CloseOnly" << std::endl;
  std::cout << "  1 - OpenClose" << std::endl;
  std::cout << "  2 - HighLow" << std::endl;
  std::cout << "  3 - OHLC" << std::endl;
  std::cout << "  4 - Extended (all of the above)" << std::endl << std::endl;

  std::cout << "  MODE possible values (2 variants):" << std::endl;
  std::cout << "  *  validate:nowid -- example: [validate:1568328448]" << std::endl;
  std::cout << "      (nowid is a string to identify a run, which is a the part 1568328448 of the following example run-file: PatternsLong_1568328448_7_2.042434_2.042434_1.txt)" << std::endl;
  std::cout << "  *  threads:thread_no -- example: [threads:4]" << std::endl;
  std::cout << "      The number of parallel threads to run." << std::endl;
  std::cout << "      (use numbers 0 through n. Zero(0) is interpreted as the maximmum thread_no of your system.)" << std::endl;

  std::cout << "  --API:Source : " << std::endl;
  std::cout << "  *  Instructs the program to get hourly and EOD data from an API " << std::endl;
  std::cout << "  *  Source must be a valid, implemented data source with a REST API " << std::endl;
  std::cout << "  *  If API:Source is specified the next parameter will be the api.config file which contains \"source,api token\" pairs" << std::endl;
  
  std::cout << "  --LOCAL: " << std::endl;
  std::cout << "  * Instructs the program to get hourly and EOD data from local files. " << std::endl;
  std::cout << "  * If --local is specified the next two parameters are the daily file and hourly file. " << std::endl;
  
  return 2;
}

int main(int argc, char **argv)
{
  auto startTime = std::chrono::system_clock::now();
  time_t startT = std::chrono::system_clock::to_time_t(startTime);
  std::cout << "started at: " << std::ctime(&startT) << std::endl;

  std::vector<std::string> v(argv, argv + argc);

  if (argc > 8)
    {

      int nthreads = 0;

      std::string validateISNowStringInput;
      std::string validateOOSNowStringInput;
      bool iisRun = true;
      bool oosRun = true;
      
      std::string apiSource;
      std::string apiConfigFilePath;
      std::string hourlyDataFilePath;
      std::string eodDataFilePath;

      std::shared_ptr<RunParameters> parameters = std::make_shared<RunParameters>();
      parameters->setUseApi((v[7].find("api") != std::string::npos));
      parameters->setConfig1FilePath(v[1]);
      parameters->setSearchConfigFilePath(v[2]);

      if(parameters->shouldUseApi()) 
      {
          std::vector<std::string> apiSourceSplit;
          boost::split(apiSourceSplit, v[7], boost::is_any_of(":"));
          if(apiSourceSplit.size() != 2) 
            return usage_error(v);
          parameters->setApiSource(apiSourceSplit[1]);
          parameters->setApiConfigFilePath(v[8]);
      }
      else
      {
        parameters->setEodDataFilePath(v[8]);
        parameters->setHourlyDataFilePath(v[9]);
      }

      std::string longorshort = v[3];
      SideToRun sideToRun;
      if (longorshort == "longonly")
        sideToRun = SideToRun::LongOnly;
      else if (longorshort == "shortonly")
        sideToRun = SideToRun::ShortOnly;
      else if (longorshort == "longshort")
        sideToRun = SideToRun::LongShort;
      else
        return usage_error(v);

      if (v[4] == "IS")
        oosRun = false;
      else if (v[4] == "OOS")
        iisRun = false;
      else if (v[4] == "ISOOS")
        ;
      else
        return usage_error(v);

      std::vector<std::string> spl;
      boost::split(spl, v[6], boost::is_any_of(":"));
      if (spl[0] == "threads")
        {
          nthreads = std::stoi(spl[1]);
        }
      else if (spl[0] == "validate" && v[4] == "IS")
        {
          validateISNowStringInput = spl[1];
        }
      else if (spl[0] == "validate" && v[4] == "OOS")
        {
          validateOOSNowStringInput = spl[1];
          iisRun = false;
        }
      else if (spl[0] == "validate" && v[4] == "ISOOS")
        {
          validateISNowStringInput = spl[1];
          validateOOSNowStringInput = spl[1];
        }
      else
        return usage_error(v);

      std::vector<ComparisonType> patternSearchTypes;

      ComparisonType inputPatternSearchType = static_cast<ComparisonType>(std::stoi(v[5]));
      std::cout << std::to_string(inputPatternSearchType) << std::endl;
      if (inputPatternSearchType < ComparisonType::CloseOnly || inputPatternSearchType > ComparisonType::Extended)
        throw std::logic_error("PATTERN_SEARCH_TYPE out of bounds");
      if (inputPatternSearchType == ComparisonType::Extended)
        patternSearchTypes = {ComparisonType::CloseOnly, ComparisonType::OpenClose, ComparisonType::HighLow, ComparisonType::Ohlc};
      else
        patternSearchTypes = {inputPatternSearchType};

      runner runner_instance(static_cast<size_t>(nthreads));
      //build thread-pool-runner
      runner& Runner=runner::instance();

      SearchRun search(parameters);

      std::string symbolStr = search.getConfig()->getSecurity()->getSymbol();
//      std::string mergedPath = "Merged_1573400279.txt";
//      std::string histPath = "CL_RAD_Hourly.txt_timeframe_1";
//      DateRange backtestingDatesIS(search.getConfig()->getInsampleDateRange().getFirstDate(), search.getConfig()->getInsampleDateRange().getLastDate());
//      PatternReRunner rerunner(mergedPath, histPath, symbolStr, backtestingDatesIS, search.getSearchConfig()->getProfitFactorCriterion(), "testOutput.txt");
//      rerunner.backtest();

      for (ComparisonType patternSearchType: patternSearchTypes)
        {
          bool mergingPhase = false;
          if (inputPatternSearchType == ComparisonType::Extended && patternSearchType == ComparisonType::Ohlc)
            mergingPhase = true;
          if (inputPatternSearchType != ComparisonType::Extended)
            mergingPhase = true;

          std::cout << "Current SEARCHSPACE: " << std::string(ToString(patternSearchType)) << ", is merging phase: " << mergingPhase << std::endl;
          //validation section
          if (iisRun)
            {
              for (size_t i = 0; i < search.getTargetStopSize(); i++)
                {
                  std::string validateISNowString(validateISNowStringInput);
                  if (validateISNowStringInput.empty())
                    {
                      search.run(Runner, true, sideToRun, i, patternSearchType);
                      validateISNowString = std::to_string(search.getNowAsLong());
                    }
                  auto tspair = search.getTargetsAtIndex(i);
                  std::string tsStr = std::to_string((tspair.first).getAsDouble()) + "_" + std::to_string((tspair.second).getAsDouble());
                  std::cout << "IIS -- Target index: " << i << ", target string: " << tsStr << std::endl;

                  std::string portfolioName(search.getConfig()->getSecurity()->getName() + std::string(" Portfolio"));
                  std::shared_ptr<Portfolio<Num>> portfolio = std::make_shared<Portfolio<Num>>(portfolioName);
                  portfolio->addSecurity(search.getConfig()->getSecurity());

                  if (sideToRun != SideToRun::ShortOnly && mergingPhase)
                    {
                      std::string fileName(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr + "_SelectedISLong.txt");
                      std::string validatedFileName(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr + "_InSampleLongValidated.txt");
                      PatternMatcher matcher(validateISNowString, tsStr, inputPatternSearchType, true, true, search.getSearchConfig()->getMinNumStratsBeforeValidation(),
                                             search.getSearchConfig()->getNumTimeFrames(), search.getConfig(), search.getSearchConfig(), Runner);
                      matcher.countOccurences();
                      bool exportOk = matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
                      if (exportOk && sideToRun == SideToRun::LongOnly)
                        {
                          auto sys = getPricePatternsShared(fileName);
                          if (sys->getNumPatterns() > 0)
                            validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys, validatedFileName);
                        }
                    }
                  if (sideToRun != SideToRun::LongOnly && mergingPhase)
                    {
                      std::string fileName(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr +  "_SelectedISShort.txt");
                      std::string validatedFileName(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr + "_InSampleShortValidated.txt");
                      PatternMatcher matcher(validateISNowString, tsStr, inputPatternSearchType, false, true, search.getSearchConfig()->getMinNumStratsBeforeValidation(),
                                             search.getSearchConfig()->getNumTimeFrames(), search.getConfig(), search.getSearchConfig(), Runner);
                      matcher.countOccurences();
                      bool exportOk = matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
                      if (exportOk && sideToRun == SideToRun::ShortOnly)
                        {
                          auto sys = getPricePatternsShared(fileName);
                          if (sys->getNumPatterns() > 0)
                            validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys, validatedFileName);            }
                    }
                  if (sideToRun == SideToRun::LongShort && mergingPhase)
                    {
                      std::string fileName1(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr +  "_SelectedISLong.txt");
                      std::string fileName2(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr +  "_SelectedISShort.txt");
                      std::string fileName(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr +  "_SelectedIS.txt");

                      FileMatcher::mergeFiles(std::vector<boost::filesystem::path>{boost::filesystem::path(fileName1), boost::filesystem::path(fileName2)}, fileName);

                      std::string validatedFileName(symbolStr + "_" + std::string(ToString(inputPatternSearchType)) + "_" + tsStr + "_InSampleValidated.txt");
                      //PatternMatcher matcher(validateISNowString, tsStr, inputPatternSearchType, false, true, search.getSearchConfig()->getMinNumStratsBeforeValidation(), search.getSearchConfig()->getNumTimeFrames());
                      //matcher.countOccurences();
                      //bool exportOk = matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);


                      auto sys = getPricePatternsShared(fileName);
                      if (sys->getNumPatterns() > 0)
                        validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys, validatedFileName);

                    }

                }
            }


          //only matching section
          if (oosRun)
            {
              for (size_t i = 0; i < search.getTargetStopSize(); i++)
                {
                  std::string validateOOSNowString(validateOOSNowStringInput);
                  if (validateOOSNowStringInput.empty())
                    {
                      search.run(Runner, false, sideToRun, i, patternSearchType);
                      validateOOSNowString = std::to_string(search.getNowAsLong());
                    }

                  auto tspair = search.getTargetsAtIndex(i);
                  std::string tsStr = std::to_string((tspair.first).getAsDouble()) + "_" + std::to_string((tspair.second).getAsDouble());
                  std::cout << "OOS -- Target index: " << i << ", target string: " << tsStr << std::endl;

                  std::string portfolioName(search.getConfig()->getSecurity()->getName() + std::string(" Portfolio"));
                  std::shared_ptr<Portfolio<Num>> portfolio = std::make_shared<Portfolio<Num>>(portfolioName);
                  portfolio->addSecurity(search.getConfig()->getSecurity());

                  if (sideToRun != SideToRun::ShortOnly && mergingPhase)
                    {
                      std::string fileName(symbolStr + "_" + tsStr +  "_SelectedOOSLong.txt");
                      PatternMatcher matcher(validateOOSNowString, tsStr, inputPatternSearchType, true, false, search.getSearchConfig()->getMinNumStratsFullPeriod(),
                                             search.getSearchConfig()->getNumTimeFrames(), search.getConfig(), search.getSearchConfig(), Runner);
                      matcher.countOccurences();
                      matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);

                    }
                  if (sideToRun != SideToRun::LongOnly && mergingPhase)
                    {
                      std::string fileName(symbolStr + "_" + tsStr + "_SelectedOOSShort.txt");
                      PatternMatcher matcher(validateOOSNowString, tsStr, inputPatternSearchType, false, false,  search.getSearchConfig()->getMinNumStratsFullPeriod(),
                                             search.getSearchConfig()->getNumTimeFrames(), search.getConfig(), search.getSearchConfig(), Runner);
                      matcher.countOccurences();
                      matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
                    }

                }
            }
        }
      auto endTime = std::chrono::system_clock::now();
      time_t endT = std::chrono::system_clock::to_time_t(endTime);
      std::chrono::duration<double> elapsed = endTime - startTime;
      std::cout << "Run finished at: " << std::ctime(&endT) << std::endl;
      std::cout << "Seconds elapsed since strart: " << elapsed.count() << std::endl;
      return 0;
    }
  else {
      return usage_error(v);
    }


}


