
#include "SearchRun.h"
#include "PatternMatcher.h"
#include "PALMonteCarloValidation.h"

using namespace mkc_searchalgo;
using Num = num::DefaultNumber;

template <template <typename> class _SurvivingStrategyPolicy, typename _McptType>
static void
validateByPermuteMarketChanges (const std::shared_ptr<McptConfiguration<Num>>& configuration, unsigned int numPermutations, PriceActionLabSystem* pal, const std::string& validationOutputFile)
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

static void validate(const std::shared_ptr<McptConfiguration<Num>>& configuration, unsigned int numPermutations, PriceActionLabSystem* pal, const std::string& validationOutputFile)
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
  std::cout << "Correct usage is:... [configFileName] [searchConfigFileName] [longonly/shortonly/longshort] (optional MODE:--see below for options--)" << std::endl;
  std::cout << "  MODE settings: (leave empty for typical runs)" << std::endl;
  std::cout << "  *  validateIS/validateOOS/validateISOOS:nowid -- example: [validateIS:1568328448]" << std::endl;
  std::cout << "      (nowid is a string to identify a run, which is a the part 1568328448 of the following example run-file: PatternsLong_1568328448_7_2.042434_2.042434_1.txt)" << std::endl;
  std::cout << "  *  threads:thread_no -- example: [threads:4]" << std::endl;
  std::cout << "      (only used for complete runs to override default-maximmum thread_no of your system. The number of parallel threads to run.)" << std::endl;
  return 2;
}

int main(int argc, char **argv)
{
  std::cout << "started..." << std::endl;
  std::vector<std::string> v(argv, argv + argc);

  if (argc == 4 || argc == 5)
    {
      int nthreads = 0;

      std::string validateISNowString;
      std::string validateOOSNowString;

      if (argc == 5) {
          std::vector<std::string> spl;
          boost::split(spl, v[4], boost::is_any_of(":"));
          if (spl[0] == "threads")
            nthreads = std::stoi(spl[1]);
          else if (spl[0] == "validateIS" || spl[0] == "validateIISOOS")
            validateISNowString = spl[1];
          else if (spl[0] == "validateOOS" || spl[0] == "validateIISOOS")
            validateOOSNowString = spl[1];
          else
            usage_error(v);

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

      runner runner_instance(static_cast<size_t>(nthreads));
      //build thread-pool-runner
      runner& Runner=runner::instance();

      SearchRun search(v[1], v[2]);

      std::string symbolStr = search.getConfig()->getSecurity()->getSymbol();

      //validation section
      for (size_t i = 0; i < search.getTargetStopSize(); i++)
        {
          if (validateISNowString.empty())
            {
              search.run(Runner, true, sideToRun, i);
              validateISNowString = std::to_string(search.getNowAsLong());
            }
          auto tspair = search.getTargetsAtIndex(i);
          std::string tsStr = std::to_string((tspair.first).getAsDouble()) + "_" + std::to_string((tspair.first).getAsDouble());
          std::string portfolioName(search.getConfig()->getSecurity()->getName() + std::string(" Portfolio"));
          std::shared_ptr<Portfolio<Num>> portfolio = std::make_shared<Portfolio<Num>>(portfolioName);
          portfolio->addSecurity(search.getConfig()->getSecurity());

          if (sideToRun != SideToRun::ShortOnly)
            {
              std::string fileName(symbolStr + "_" + tsStr + "_SelectedISLong.txt");
              std::string validatedFileName(symbolStr + "_" + tsStr + "_InSampleLongValidated.txt");
              PatternMatcher matcher(validateISNowString, tsStr, true, true, search.getSearchConfig()->getMinNumStratsBeforeValidation());
              matcher.countOccurences();
              bool exportOk = matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
              if (exportOk)
                {
                  std::unique_ptr<PriceActionLabSystem> sys = getPricePatterns(fileName);
                  if (sys->getNumPatterns() > 0)
                    validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys.get(), validatedFileName);
                }
            }
          if (sideToRun != SideToRun::LongOnly)
            {
              std::string fileName(symbolStr + "_" + tsStr +  "_SelectedISShort.txt");
              std::string validatedFileName(symbolStr + "_" + tsStr + "_InSampleShortValidated.txt");
              PatternMatcher matcher(validateISNowString, tsStr, false, true, search.getSearchConfig()->getMinNumStratsBeforeValidation());
              matcher.countOccurences();
              bool exportOk = matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
              if (exportOk)
                {
                  std::unique_ptr<PriceActionLabSystem> sys = getPricePatterns(fileName);
                  if (sys->getNumPatterns() > 0)
                    validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys.get(), validatedFileName);            }
                }
            }

      //only matching section
      for (size_t i = 0; i < search.getTargetStopSize(); i++)
        {
          if (validateOOSNowString.empty())
            {
              search.run(Runner, false, sideToRun, i);
              validateOOSNowString = std::to_string(search.getNowAsLong());
            }
          auto tspair = search.getTargetsAtIndex(i);
          std::string tsStr = std::to_string((tspair.first).getAsDouble()) + "_" + std::to_string((tspair.first).getAsDouble());
          std::string portfolioName(search.getConfig()->getSecurity()->getName() + std::string(" Portfolio"));
          std::shared_ptr<Portfolio<Num>> portfolio = std::make_shared<Portfolio<Num>>(portfolioName);
          portfolio->addSecurity(search.getConfig()->getSecurity());

          if (sideToRun != SideToRun::ShortOnly)
            {
              std::string fileName(symbolStr + "_" + tsStr +  "_SelectedOOSLong.txt");
              PatternMatcher matcher(validateOOSNowString, tsStr, true, false, search.getSearchConfig()->getMinNumStratsFullPeriod());
              matcher.countOccurences();
              matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);

            }
          if (sideToRun != SideToRun::LongOnly)
            {
              std::string fileName(symbolStr + "_" + tsStr + "_SelectedOOSShort.txt");
              PatternMatcher matcher(validateOOSNowString, tsStr, false, false,  search.getSearchConfig()->getMinNumStratsFullPeriod());
              matcher.countOccurences();
              matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
            }

        }

      return 0;

    }
  else {
      return usage_error(v);
    }


}


