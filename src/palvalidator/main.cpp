//#define CSV_IO_NO_THREAD 1
//
//#include <string>
//#include <vector>
//#include <memory>
//#include <stdio.h>
//#include "McptConfigurationFileReader.h"
//#include "PALMonteCarloValidation.h"
//#include "RobustnessTester.h"
//#include "LogPalPattern.h"
//#include "LogRobustnessTest.h"
//#include "number.h"
//#include <cstdlib>
//
//using namespace mkc_timeseries;
//using std::shared_ptr;
//
//using Num = num::DefaultNumber;
//
//enum MCPTValidationComputationPolicy {UNADJUSTED_PVALUE, ADJUSTED_PVALUE, BESTOF_PVALUE, COMPUTATION_NONE};
//enum MCPTTestStatistic {CUMULATIVE_RETURN, PESSIMISTIC_RETURN_RATIO, PAL_PROFITABILITY, NORMALIZED_RETURN, STATISTIC_NONE};
//
//template <template <typename> class _SurvivingStrategyPolicy, typename McptType>
//static void validateByPermuteMarketChanges (std::shared_ptr<McptConfiguration<Num>> configuration,
//                                            int numPermutations);
//
//template <class Decimal, typename McptType, template <typename> class _SurvivingStrategyPolicy>
//static void exportSurvivingMCPTPatterns (const PALMonteCarloValidation<Decimal,
//                                         McptType, _SurvivingStrategyPolicy>& monteCarloValidation,
//                                         const std::string& securitySymbol);
//
//static std::string createSurvivingPatternsFileName (const std::string& securitySymbol);
//static std::string createSurvivingPatternsAndRobustFileName (const std::string& securitySymbol);
//static std::string createRejectedPatternsAndRobustFileName (const std::string& securitySymbol);
//static std::string createMCPTSurvivingPatternsFileName (const std::string& securitySymbol);
//template <class Decimal, typename McptType, template <typename> class _SurvivingStrategyPolicy>
//static shared_ptr<PalRobustnessTester<Decimal>>
//runRobustnessTests (const PALMonteCarloValidation<Decimal,McptType,_SurvivingStrategyPolicy>& monteCarloValidation,
//                    shared_ptr<McptConfiguration<Decimal>> aConfiguration);
//
//static void exportRejectedPatternsAndRobustness(shared_ptr<PalRobustnessTester<Num>> aRobustnessTester,
//                                                const std::string& securitySymbol);
//
//static void exportSurvivingPatterns(shared_ptr<PalRobustnessTester<Num>> aRobustnessTester,
//                                    const std::string& securitySymbol);
//static void exportSurvivingPatternsAndRobustness(shared_ptr<PalRobustnessTester<Num>> aRobustnessTester,
//                                                 const std::string& securitySymbol);
//
//void usage()
//{
//  printf("Usage: PalValidator <configuration file> [Number of Permutation Tests] <P-Value adjustment policy, 1 = None, 2 = adjusted, 3 = BestofPValue>, <Test Stat, 1 = Cumulative Return, 2 = PRR, 3 = Profitability, 4 = Normalized Return, <Num Threads>(optional)\n\n");
//}
//
//
//int error_with_usage()
//{
//  usage();
//  return 1;
//}
//
//int main(int argc, char **argv)
//{
//  std::vector<std::string> v(argv, argv + argc);
//
//  if (argc >= 4)
//    {
//      std::string configurationFileName (v[1]);
//      int numPermutations = std::stoi(v[2]);
//      int typeOfPermutationTest = std::stoi(v[3]);
//
//      size_t nthreads = 0;
//      int intTestStatistic = 0;
//      MCPTValidationComputationPolicy validationPolicy = COMPUTATION_NONE;
//      MCPTTestStatistic testStatistic = STATISTIC_NONE;
//
//      if (typeOfPermutationTest == 1)
//        validationPolicy = UNADJUSTED_PVALUE;
//      else if (typeOfPermutationTest == 2)
//        validationPolicy = ADJUSTED_PVALUE;
//      else if (typeOfPermutationTest == 3)
//        validationPolicy = BESTOF_PVALUE;
//      else
//        {
//          error_with_usage();
//        }
//
//      if (argc >= 5)
//        {
//          intTestStatistic = std::stoi(v[4]);
//          if (intTestStatistic == 1)
//            testStatistic = CUMULATIVE_RETURN;
//          else if (intTestStatistic == 2)
//            testStatistic = PESSIMISTIC_RETURN_RATIO;
//          else if (intTestStatistic == 3)
//            testStatistic = PAL_PROFITABILITY;
//          else if (intTestStatistic == 4)
//            testStatistic = NORMALIZED_RETURN;
//          else
//            error_with_usage();
//        }
//      else
//        testStatistic = PESSIMISTIC_RETURN_RATIO;
//
//      if (argc == 6) {
//          nthreads = std::stoi(v[5]);
//        }
//
//      runner runner_instance(nthreads);
//
//      printf ("Number of permutations = %d\n", numPermutations);
//
//      McptConfigurationFileReader reader(configurationFileName);
//      std::shared_ptr<McptConfiguration<Num>> configuration = reader.readConfigurationFile();
//
//      if (validationPolicy == UNADJUSTED_PVALUE)
//        {
//          printf ("Validation Policy = unadjusted P-Value\n");
//          switch (testStatistic)
//            {
//            case CUMULATIVE_RETURN:
//              printf ("Test stat = cumulative return\n");
//              break;
//
//            case PESSIMISTIC_RETURN_RATIO:
//              printf ("Test stat = PRR\n");
//              break;
//
//            case PAL_PROFITABILITY:
//              printf ("Test stat = PAL Profitability\n");
//              break;
//
//            case NORMALIZED_RETURN:
//              printf ("Test stat = Normalized Return\n");
//              break;
//
//            case STATISTIC_NONE:
//              printf ("Test stat = none\n");
//              break;
//
//            }
//
//          if (testStatistic == CUMULATIVE_RETURN)
//            validateByPermuteMarketChanges <UnadjustedPValueStrategySelection,
//                MonteCarloPermuteMarketChanges<Num,
//                CumulativeReturnPolicy,
//                DefaultPermuteMarketChangesPolicy<Num,
//                CumulativeReturnPolicy<Num>>>>
//                (configuration,
//                 numPermutations);
//          else if (testStatistic == PESSIMISTIC_RETURN_RATIO)
//            validateByPermuteMarketChanges <UnadjustedPValueStrategySelection,
//                MonteCarloPermuteMarketChanges<Num,
//                PessimisticReturnRatioPolicy,
//                DefaultPermuteMarketChangesPolicy<Num,
//                PessimisticReturnRatioPolicy<Num>>>>
//                (configuration,
//                 numPermutations);
//          else if (testStatistic == PAL_PROFITABILITY)
//            validateByPermuteMarketChanges <UnadjustedPValueStrategySelection,
//                MonteCarloPermuteMarketChanges<Num,
//                PalProfitabilityPolicy,
//                DefaultPermuteMarketChangesPolicy<Num,
//                PalProfitabilityPolicy<Num>>>>
//                (configuration,
//                 numPermutations);
//          else if (testStatistic == NORMALIZED_RETURN)
//            validateByPermuteMarketChanges <UnadjustedPValueStrategySelection,
//                MonteCarloPermuteMarketChanges<Num,
//                NormalizedReturnPolicy,
//                DefaultPermuteMarketChangesPolicy<Num,
//                NormalizedReturnPolicy<Num>>>>
//                (configuration,
//                 numPermutations);
//
//        }
//      else if (validationPolicy == ADJUSTED_PVALUE)
//        {
//          if (testStatistic == CUMULATIVE_RETURN)
//            validateByPermuteMarketChanges <AdaptiveBenjaminiHochbergYr2000,
//                MonteCarloPermuteMarketChanges<Num,
//                CumulativeReturnPolicy,
//                DefaultPermuteMarketChangesPolicy<Num,
//                CumulativeReturnPolicy<Num>>>>
//                (configuration,
//                 numPermutations);
//          else if (testStatistic == PESSIMISTIC_RETURN_RATIO)
//            validateByPermuteMarketChanges <AdaptiveBenjaminiHochbergYr2000,
//                MonteCarloPermuteMarketChanges<Num,
//                PessimisticReturnRatioPolicy,
//                DefaultPermuteMarketChangesPolicy<Num,
//                PessimisticReturnRatioPolicy<Num>>>>
//                (configuration, numPermutations);
//
//        }
//    }
//  else
//    error_with_usage();
//
//  return 0;
//}
//
//
//template <template <typename> class _SurvivingStrategyPolicy, typename _McptType>
//static void
//validateByPermuteMarketChanges (std::shared_ptr<McptConfiguration<Num>> configuration,
//                                int numPermutations)
//{
//  std::cout << "starting validation." << std::endl;
//
//  PALMonteCarloValidation<Num,_McptType,_SurvivingStrategyPolicy> validation(configuration, numPermutations);
//
//  printf ("Starting Monte Carlo Validation tests (Using Permute Market Changes)\n\n");
//
//  validation.runPermutationTests();
//
//  printf ("Exporting surviving MCPT strategies\n");
//
//  exportSurvivingMCPTPatterns<Num, _McptType, _SurvivingStrategyPolicy>  (validation, configuration->getSecurity()->getSymbol());
//
//  //temporarily
//  return;
//
//  // Run robustness tests on the patterns that survived Monte Carlo Permutation Testing
//  printf ("Running robustness tests for %lu patterns\n\n",
//          validation.getNumSurvivingStrategies());
//
//  std::shared_ptr<PalRobustnessTester<Num>> robust =
//      runRobustnessTests<Num, _McptType> (validation, configuration);
//
//  // Now export the pattern in PAL format
//
//  printf ("Exporting %lu surviving strategies\n", robust->getNumSurvivingStrategies());
//
//  exportSurvivingPatterns (robust, configuration->getSecurity()->getSymbol());
//  exportSurvivingPatternsAndRobustness (robust, configuration->getSecurity()->getSymbol());
//
//  printf ("Exporting %lu rejected strategies\n", robust->getNumRejectedStrategies());
//
//  exportRejectedPatternsAndRobustness (robust, configuration->getSecurity()->getSymbol());
//
//}
//
//template <class Decimal>
//std::shared_ptr<PalStrategy<Decimal>>
//createStrategyForRobustnessTest (std::shared_ptr<PalStrategy<Decimal>> aStrategy,
//                                 std::shared_ptr<Security<Decimal>> securityForRobustness)
//{
//  // Create new empty portfolio
//  auto newPortfolio (aStrategy->getPortfolio()->clone());
//
//  newPortfolio->addSecurity (securityForRobustness);
//  return aStrategy->clone2 (newPortfolio);
//}
//
//template <class Decimal, typename McptType, template <typename> class _SurvivingStrategyPolicy>
//static shared_ptr<PalRobustnessTester<Decimal>>
//runRobustnessTests (const PALMonteCarloValidation<Decimal, McptType, _SurvivingStrategyPolicy>& monteCarloValidation,
//                    shared_ptr<McptConfiguration<Decimal>> aConfiguration)
//{
//  typename PALMonteCarloValidation<Decimal, McptType, _SurvivingStrategyPolicy>::SurvivingStrategiesIterator it =
//      monteCarloValidation.beginSurvivingStrategies();
//
//
//  // Conduct robustness testing on insample data
//  auto robustnessTester =
//      std::make_shared<StatisticallySignificantRobustnessTester<Num>>(aConfiguration->getInSampleBackTester());
//
//  auto securityUnderTest = aConfiguration->getSecurity();
//
//  for (; it != monteCarloValidation.endSurvivingStrategies(); it++)
//    {
//      robustnessTester->addStrategy(createStrategyForRobustnessTest<Decimal> (*it, securityUnderTest));
//      //robustnessTester->addStrategy(*it);
//    }
//
//  robustnessTester->runRobustnessTests();
//
//  return robustnessTester;
//}
//
//template <class Decimal, typename McptType, template <typename> class _SurvivingStrategyPolicy>
//static void exportSurvivingMCPTPatterns (const PALMonteCarloValidation<Decimal,
//                                         McptType,_SurvivingStrategyPolicy>& monteCarloValidation,
//                                         const std::string& securitySymbol)
//{
//  typename PALMonteCarloValidation<Decimal,McptType,_SurvivingStrategyPolicy>::SurvivingStrategiesIterator it =
//      monteCarloValidation.beginSurvivingStrategies();
//
//  std::ofstream mcptPatternsFile(createMCPTSurvivingPatternsFileName(securitySymbol));
//
//  for (; it != monteCarloValidation.endSurvivingStrategies(); it++)
//    {
//      LogPalPattern::LogPattern ((*it)->getPalPattern(), mcptPatternsFile);
//    }
//}
//
//static void exportSurvivingPatterns(shared_ptr<PalRobustnessTester<Num>> aRobustnessTester,
//                                    const std::string& securitySymbol)
//{
//  shared_ptr<RobustnessCalculator<Num>> robustnessResults;
//  shared_ptr<PalStrategy<Num>> aStrategy;
//
//  PalRobustnessTester<Num>::SurvivingStrategiesIterator surviveIt =
//      aRobustnessTester->beginSurvivingStrategies();
//
//  PalRobustnessTester<Num>::RobustnessResultsIterator robustResultIt;
//
//  std::ofstream survivingPatternsFile(createSurvivingPatternsFileName(securitySymbol));
//
//  for (; surviveIt != aRobustnessTester->endSurvivingStrategies(); surviveIt++)
//    {
//      aStrategy = *surviveIt;
//      LogPalPattern::LogPattern (aStrategy->getPalPattern(), survivingPatternsFile);
//
//      survivingPatternsFile << std::endl << std::endl;
//    }
//}
//
//static void exportSurvivingPatternsAndRobustness(shared_ptr<PalRobustnessTester<Num>> aRobustnessTester,
//                                                 const std::string& securitySymbol)
//{
//  shared_ptr<RobustnessCalculator<Num>> robustnessResults;
//  shared_ptr<PalStrategy<Num>> aStrategy;
//
//  PalRobustnessTester<Num>::SurvivingStrategiesIterator surviveIt =
//      aRobustnessTester->beginSurvivingStrategies();
//
//  PalRobustnessTester<Num>::RobustnessResultsIterator robustResultIt;
//
//  std::ofstream survivingPatternsFile(createSurvivingPatternsAndRobustFileName(securitySymbol));
//
//  for (; surviveIt != aRobustnessTester->endSurvivingStrategies(); surviveIt++)
//    {
//      aStrategy = *surviveIt;
//      LogPalPattern::LogPattern (aStrategy->getPalPattern(), survivingPatternsFile);
//
//      robustResultIt = aRobustnessTester->findSurvivingRobustnessResults(aStrategy);
//
//      if (robustResultIt !=
//          aRobustnessTester->endSurvivingRobustnessResults())
//        {
//          robustnessResults = robustResultIt->second;
//          LogRobustnessTest<Num>::logRobustnessTestResults (*robustnessResults,
//                                                            survivingPatternsFile);
//          survivingPatternsFile << std::endl << std::endl;
//        }
//    }
//}
//
//static void exportRejectedPatternsAndRobustness(shared_ptr<PalRobustnessTester<Num>> aRobustnessTester,
//                                                const std::string& securitySymbol)
//{
//  shared_ptr<RobustnessCalculator<Num>> robustnessResults;
//  shared_ptr<PalStrategy<Num>> aStrategy;
//
//  PalRobustnessTester<Num>::RejectedStrategiesIterator rejIt =
//      aRobustnessTester->beginRejectedStrategies();
//
//  PalRobustnessTester<Num>::RobustnessResultsIterator robustResultIt;
//
//  std::ofstream rejectedPatternsFile(createRejectedPatternsAndRobustFileName(securitySymbol));
//
//  for (; rejIt != aRobustnessTester->endRejectedStrategies(); rejIt++)
//    {
//      aStrategy = *rejIt;
//      LogPalPattern::LogPattern (aStrategy->getPalPattern(), rejectedPatternsFile);
//
//      robustResultIt = aRobustnessTester->findFailedRobustnessResults(aStrategy);
//
//      if (robustResultIt !=
//          aRobustnessTester->endFailedRobustnessResults())
//        {
//          robustnessResults = robustResultIt->second;
//          LogRobustnessTest<Num>::logRobustnessTestResults (*robustnessResults,
//                                                            rejectedPatternsFile);
//          rejectedPatternsFile << std::endl << std::endl;
//        }
//    }
//}
//
//static std::string createSurvivingPatternsFileName (const std::string& securitySymbol)
//{
//  return (securitySymbol + std::string("_SurvivingPatterns.txt"));
//}
//
//static std::string createSurvivingPatternsAndRobustFileName (const std::string& securitySymbol)
//{
//  return (securitySymbol + std::string("_SurvivingPatternsAndRobust.txt"));
//}
//
//static std::string createRejectedPatternsAndRobustFileName (const std::string& securitySymbol)
//{
//  return (securitySymbol + std::string("_RejectedPatternsAndRobust.txt"));
//}
//
//static std::string createMCPTSurvivingPatternsFileName (const std::string& securitySymbol)
//{
//  return (securitySymbol + std::string("_MCPT_SurvivingPatterns.txt"));
//}
//
