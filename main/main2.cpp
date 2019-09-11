
#include "SearchRun.h"
#include "PatternMatcher.h"
#include "PALMonteCarloValidation.h"

using namespace mkc_searchalgo;
using Num = num::DefaultNumber;

template <template <typename> class _SurvivingStrategyPolicy, typename _McptType>
static void
validateByPermuteMarketChanges (std::shared_ptr<McptConfiguration<Num>> configuration, unsigned int numPermutations, PriceActionLabSystem* pal, const std::string& validationOutputFile)
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
    {
      LogPalPattern::LogPattern ((*it)->getPalPattern(), mcptPatternsFile);
    }
}

static void validate(std::shared_ptr<McptConfiguration<Num>> configuration, unsigned int numPermutations, PriceActionLabSystem* pal, const std::string& validationOutputFile)
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
  std::cout << "Correct usage is:... [configFileName] [searchConfigFileName] [longonly/shortonly/longshort] (optional:[number of parallel threads])" << std::endl;
  return 2;
}

int main(int argc, char **argv)
{
  std::cout << "started..." << std::endl;
  std::vector<std::string> v(argv, argv + argc);

  if (argc == 4 || argc == 5)
    {
      int nthreads = 0;
      if (argc == 5) {
          nthreads = std::stoi(v[4]);
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

      SearchRun search(v[1], v[2]);

      std::string symbolStr = search.getConfig()->getSecurity()->getSymbol();

      //validation section
      for (size_t i = 0; i < search.getTargetStopSize(); i++)
        {
          search.run(nthreads, true, sideToRun, i);
          std::string pat = std::to_string(search.getNowAsLong());
          //std::string pat = "1567937016";
          auto tspair = search.getTargetsAtIndex(i);
          std::string tsStr = std::to_string((tspair.first).getAsDouble()) + "_" + std::to_string((tspair.first).getAsDouble());
          std::string portfolioName(search.getConfig()->getSecurity()->getName() + std::string(" Portfolio"));
          std::shared_ptr<Portfolio<Num>> portfolio = std::make_shared<Portfolio<Num>>(portfolioName);
          portfolio->addSecurity(search.getConfig()->getSecurity());

          if (sideToRun != SideToRun::ShortOnly)
            {
              std::string fileName(symbolStr + "_SelectedISLong.txt");
              std::string validatedFileName(symbolStr + "_InSampleLongValidated.txt");
              PatternMatcher matcher(pat, tsStr, true, search.getSearchConfig()->getMinNumStratsBeforeValidation());
              matcher.countOccurences();
              matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
              std::unique_ptr<PriceActionLabSystem> sys = getPricePatterns(fileName);
              validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys.get(), validatedFileName);

            }
          if (sideToRun != SideToRun::LongOnly)
            {
              std::string fileName(symbolStr + "_SelectedISShort.txt");
              std::string validatedFileName(symbolStr + "_InSampleShortValidated.txt");
              PatternMatcher matcher(pat, tsStr, false, search.getSearchConfig()->getMinNumStratsBeforeValidation());
              matcher.countOccurences();
              matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);
              std::unique_ptr<PriceActionLabSystem> sys = getPricePatterns(fileName);
              validate(search.getConfig(), search.getSearchConfig()->getNumPermutations(), sys.get(), validatedFileName);            }
        }

      //only matching section
      for (size_t i = 0; i < search.getTargetStopSize(); i++)
        {
          search.run(nthreads, false, sideToRun, i);
          std::string pat = std::to_string(search.getNowAsLong());
          //std::string pat = "1567937016";
          auto tspair = search.getTargetsAtIndex(i);
          std::string tsStr = std::to_string((tspair.first).getAsDouble()) + "_" + std::to_string((tspair.first).getAsDouble());
          std::string portfolioName(search.getConfig()->getSecurity()->getName() + std::string(" Portfolio"));
          std::shared_ptr<Portfolio<Num>> portfolio = std::make_shared<Portfolio<Num>>(portfolioName);
          portfolio->addSecurity(search.getConfig()->getSecurity());

          if (sideToRun != SideToRun::ShortOnly)
            {
              std::string fileName(symbolStr + "_SelectedOOSLong.txt");
              PatternMatcher matcher(pat, tsStr, true, search.getSearchConfig()->getMinNumStratsFullPeriod());
              matcher.countOccurences();
              matcher.exportSelectPatterns<Num>(&tspair.first, &tspair.second, fileName, portfolio);

            }
          if (sideToRun != SideToRun::LongOnly)
            {
              std::string fileName(symbolStr + "_SelectedOOSShort.txt");
              PatternMatcher matcher(pat, tsStr, false, search.getSearchConfig()->getMinNumStratsFullPeriod());
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


