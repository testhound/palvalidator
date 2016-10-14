#define CSV_IO_NO_THREAD 1

#include <string>
#include <vector>
#include <memory>
#include <stdio.h>
#include "McptConfigurationFileReader.h"
#include "PALMonteCarloValidation.h"
#include "RobustnessTester.h"
#include "LogPalPattern.h"
#include "LogRobustnessTest.h"

using namespace mkc_timeseries;
using std::shared_ptr;

template <int Prec, typename McptType>
static void exportSurvivingMCPTPatterns (const PALMonteCarloValidation<Prec,McptType>& monteCarloValidation,
					 const std::string& securitySymbol);

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol);
static std::string createSurvivingPatternsAndRobustFileName (const std::string& securitySymbol);
static std::string createRejectedPatternsAndRobustFileName (const std::string& securitySymbol);
static std::string createMCPTSurvivingPatternsFileName (const std::string& securitySymbol);
static void validateUsingVersionOneMCPT (std::shared_ptr<McptConfiguration<7>> configuration,
					 int numPermutations);
static void validateUsingVersionTwoMCPT (std::shared_ptr<McptConfiguration<7>> configuration,
					 int numPermutations);
template <int Prec, typename McptType>
static shared_ptr<PalRobustnessTester<Prec>> 
runRobustnessTests (const PALMonteCarloValidation<Prec, McptType>& monteCarloValidation,
		    shared_ptr<McptConfiguration<Prec>> aConfiguration);

static void exportRejectedPatternsAndRobustness(shared_ptr<PalRobustnessTester<7>> aRobustnessTester,
				   const std::string& securitySymbol);

static void exportSurvivingPatterns(shared_ptr<PalRobustnessTester<7>> aRobustnessTester,
				    const std::string& securitySymbol);
static void exportSurvivingPatternsAndRobustness(shared_ptr<PalRobustnessTester<7>> aRobustnessTester,
				    const std::string& securitySymbol);

void usage()
{
  printf("Usage: PalValidation <configuration file> [Number of Permutation Tests] [Version # of MCPT]\n");
}

int main(int argc, char **argv)
{
  std::vector<std::string> v(argv, argv + argc);
  if (argc == 4)
    {
      std::string configurationFileName (v[1]);
      int numPermutations = std::stoi(v[2]);
      int typeOfPermutationTest = std::stoi(v[3]);

	
      printf ("Number of permutation = %d\n", numPermutations);

      McptConfigurationFileReader reader(configurationFileName);
      std::shared_ptr<McptConfiguration<7>> configuration = reader.readConfigurationFile();

      if (typeOfPermutationTest == 1)
	validateUsingVersionOneMCPT (configuration, numPermutations);
      else if (typeOfPermutationTest == 2)
	validateUsingVersionTwoMCPT (configuration, numPermutations);
      else
	{
	  usage();
	  return 1;
	}
    }
  else
    {
      usage();
      return 1;
    }
 
  return 0;
}

static
void validateUsingVersionOneMCPT (std::shared_ptr<McptConfiguration<7>> configuration,
				  int numPermutations)
{
  PALMonteCarloValidation<7, OriginalMCPT<7>> validation(configuration, numPermutations);
  printf ("Starting Monte Carlo Validation tests (Version: One)\n\n");

  validation.runPermutationTests();

  printf ("Exporting surviving MCPT strategies\n");

  exportSurvivingMCPTPatterns<7,OriginalMCPT<7>>  (validation, configuration->getSecurity()->getSymbol());

  // Run robustness tests on the patterns that survived Monte Carlo Permutation Testing
  printf ("Running robustness tests for %lu patterns\n\n", 
	  validation.getNumSurvivingStrategies());

  std::shared_ptr<PalRobustnessTester<7>> robust = 
	runRobustnessTests<7, OriginalMCPT<7>> (validation, configuration);

      // Now export the pattern in PAL format

  printf ("Exporting %lu surviving strategies\n", robust->getNumSurvivingStrategies());

  exportSurvivingPatterns (robust, configuration->getSecurity()->getSymbol());
  exportSurvivingPatternsAndRobustness (robust, configuration->getSecurity()->getSymbol());

  printf ("Exporting %lu rejected strategies\n", robust->getNumRejectedStrategies());

  exportRejectedPatternsAndRobustness (robust, configuration->getSecurity()->getSymbol());
}

static
void validateUsingVersionTwoMCPT (std::shared_ptr<McptConfiguration<7>> configuration,
				  int numPermutations)
{
  PALMonteCarloValidation<7,
			  MonteCarloPermuteMarketChanges<7, PessimisticReturnRatioPolicy>> validation(configuration, numPermutations);
  printf ("Starting Monte Carlo Validation tests (Version:Two)\n\n");

  validation.runPermutationTests();

  printf ("Exporting surviving MCPT strategies\n");

  exportSurvivingMCPTPatterns<7,MonteCarloPermuteMarketChanges<7, PessimisticReturnRatioPolicy>>  (validation, configuration->getSecurity()->getSymbol());

  // Run robustness tests on the patterns that survived Monte Carlo Permutation Testing
  printf ("Running robustness tests for %lu patterns\n\n", 
	  validation.getNumSurvivingStrategies());

  std::shared_ptr<PalRobustnessTester<7>> robust = 
    runRobustnessTests<7, MonteCarloPermuteMarketChanges<7, PessimisticReturnRatioPolicy>> (validation, configuration);

  // Now export the pattern in PAL format

  printf ("Exporting %lu surviving strategies\n", robust->getNumSurvivingStrategies());

  exportSurvivingPatterns (robust, configuration->getSecurity()->getSymbol());
  exportSurvivingPatternsAndRobustness (robust, configuration->getSecurity()->getSymbol());
      
  printf ("Exporting %lu rejected strategies\n", robust->getNumRejectedStrategies());

  exportRejectedPatternsAndRobustness (robust, configuration->getSecurity()->getSymbol());
}

template <int Prec, typename McptType>
static shared_ptr<PalRobustnessTester<Prec>> 
runRobustnessTests (const PALMonteCarloValidation<Prec, McptType>& monteCarloValidation,
		    shared_ptr<McptConfiguration<Prec>> aConfiguration)
{
  typename PALMonteCarloValidation<Prec, McptType>::SurvivingStrategiesIterator it = 
    monteCarloValidation.beginSurvivingStrategies();

  // We were robustness testing on OOS by mistake

  // auto robustnessTester = 
  //  std::make_shared<StatisticallySignificantRobustnessTester<7>>(aConfiguration->getBackTester());

  // Conduct robustness testing on insample data
  auto robustnessTester = 
    std::make_shared<StatisticallySignificantRobustnessTester<7>>(aConfiguration->getInSampleBackTester());

  for (; it != monteCarloValidation.endSurvivingStrategies(); it++)
    {
      robustnessTester->addStrategy(*it);
    }

  robustnessTester->runRobustnessTests();

  return robustnessTester;
}

template <int Prec, typename McptType>
static void exportSurvivingMCPTPatterns (const PALMonteCarloValidation<Prec,McptType>& monteCarloValidation,
					 const std::string& securitySymbol)
{
  typename PALMonteCarloValidation<Prec,McptType>::SurvivingStrategiesIterator it = 
    monteCarloValidation.beginSurvivingStrategies();

  std::ofstream mcptPatternsFile(createMCPTSurvivingPatternsFileName(securitySymbol));

  for (; it != monteCarloValidation.endSurvivingStrategies(); it++)
    {
      LogPalPattern::LogPattern ((*it)->getPalPattern(), mcptPatternsFile);
    }
}

static void exportSurvivingPatterns(shared_ptr<PalRobustnessTester<7>> aRobustnessTester,
				    const std::string& securitySymbol)
{
  shared_ptr<RobustnessCalculator<7>> robustnessResults;
  shared_ptr<PalStrategy<7>> aStrategy;

  PalRobustnessTester<7>::SurvivingStrategiesIterator surviveIt = 
    aRobustnessTester->beginSurvivingStrategies();

  PalRobustnessTester<7>::RobustnessResultsIterator robustResultIt;

  std::ofstream survivingPatternsFile(createSurvivingPatternsFileName(securitySymbol));

  for (; surviveIt != aRobustnessTester->endSurvivingStrategies(); surviveIt++)
    {
      aStrategy = *surviveIt;
      LogPalPattern::LogPattern (aStrategy->getPalPattern(), survivingPatternsFile);
		
      survivingPatternsFile << std::endl << std::endl;
    }
}

static void exportSurvivingPatternsAndRobustness(shared_ptr<PalRobustnessTester<7>> aRobustnessTester,
				    const std::string& securitySymbol)
{
  shared_ptr<RobustnessCalculator<7>> robustnessResults;
  shared_ptr<PalStrategy<7>> aStrategy;

  PalRobustnessTester<7>::SurvivingStrategiesIterator surviveIt = 
    aRobustnessTester->beginSurvivingStrategies();

  PalRobustnessTester<7>::RobustnessResultsIterator robustResultIt;

  std::ofstream survivingPatternsFile(createSurvivingPatternsAndRobustFileName(securitySymbol));

  for (; surviveIt != aRobustnessTester->endSurvivingStrategies(); surviveIt++)
    {
      aStrategy = *surviveIt;
      LogPalPattern::LogPattern (aStrategy->getPalPattern(), survivingPatternsFile);
		
      robustResultIt = aRobustnessTester->findSurvivingRobustnessResults(aStrategy);

      if (robustResultIt != 
	  aRobustnessTester->endSurvivingRobustnessResults())
	{
	  robustnessResults = robustResultIt->second;
	  LogRobustnessTest<7>::logRobustnessTestResults (*robustnessResults, 
							  survivingPatternsFile);
	  survivingPatternsFile << std::endl << std::endl;
	}
    }
}

static void exportRejectedPatternsAndRobustness(shared_ptr<PalRobustnessTester<7>> aRobustnessTester,
						const std::string& securitySymbol)
{
  shared_ptr<RobustnessCalculator<7>> robustnessResults;
  shared_ptr<PalStrategy<7>> aStrategy;

  PalRobustnessTester<7>::RejectedStrategiesIterator rejIt = 
    aRobustnessTester->beginRejectedStrategies();

  PalRobustnessTester<7>::RobustnessResultsIterator robustResultIt;

  std::ofstream rejectedPatternsFile(createRejectedPatternsAndRobustFileName(securitySymbol));

  for (; rejIt != aRobustnessTester->endRejectedStrategies(); rejIt++)
    {
      aStrategy = *rejIt;
      LogPalPattern::LogPattern (aStrategy->getPalPattern(), rejectedPatternsFile);
		
      robustResultIt = aRobustnessTester->findFailedRobustnessResults(aStrategy);

      if (robustResultIt != 
	  aRobustnessTester->endFailedRobustnessResults())
	{
	  robustnessResults = robustResultIt->second;
	  LogRobustnessTest<7>::logRobustnessTestResults (*robustnessResults, 
							  rejectedPatternsFile);
	  rejectedPatternsFile << std::endl << std::endl;
	}
    }
}

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol)
{
  return (securitySymbol + std::string("_SurvivingPatterns.txt"));
}

static std::string createSurvivingPatternsAndRobustFileName (const std::string& securitySymbol)
{
  return (securitySymbol + std::string("_SurvivingPatternsAndRobust.txt"));
}

static std::string createRejectedPatternsAndRobustFileName (const std::string& securitySymbol)
{
  return (securitySymbol + std::string("_RejectedPatternsAndRobust.txt"));
}

static std::string createMCPTSurvivingPatternsFileName (const std::string& securitySymbol)
{
  return (securitySymbol + std::string("_MCPT_SurvivingPatterns.txt"));
}

