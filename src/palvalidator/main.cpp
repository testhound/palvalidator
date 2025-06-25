#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <variant>
#include <algorithm>
#include <set>
#include <numeric>
#include <iomanip>
#include <cmath>
#include "ValidatorConfiguration.h"
#include "SecurityAttributesFactory.h"
#include "PALMastersMonteCarloValidation.h"
#include "PALRomanoWolfMonteCarloValidation.h" 
#include "PALMonteCarloValidation.h"
#include "MonteCarloPermutationTest.h"
#include "MultipleTestingCorrection.h"
#include "PermutationTestComputationPolicy.h"
#include "PermutationTestResultPolicy.h"
#include "MonteCarloTestPolicy.h"
#include "PermutationStatisticsCollector.h"
#include "LogPalPattern.h"
#include "number.h"
#include <cstdlib>

using namespace mkc_timeseries;

using Num = num::DefaultNumber;

//using BacktestingStatPolicy = RobustProfitFactorPolicy;
//template<typename T>
//using BacktestingStatPolicy = AllHighResLogPFPolicy<T>;

//template<typename T>
//using BacktestingStatPolicy = RobustProfitFactorPolicy<T>;

//template<typename T>
//using BacktestingStatPolicy = HybridEnhancedTradeAwarePolicy<T>;

template<typename T>
using BacktestingStatPolicy = GatedPerformanceScaledPalPolicy<T>;

enum class ValidationMethod
{
    Masters,
    RomanoWolf,
    BenjaminiHochberg
};

std::string getValidationMethodString(ValidationMethod method)
{
    switch (method)
    {
    case ValidationMethod::Masters:
      return std::string("Masters");

    case ValidationMethod::RomanoWolf:
      return std::string("RomanoWolf");

    case ValidationMethod::BenjaminiHochberg:
      return std::string("BenjaminiHochberg");

    default:
      throw std::invalid_argument("Unknown validation method");
    }
}

// Template declarations moved outside of function scope
using RomanoWolfStatCollectionPolicy = PermutationTestingMaxTestStatisticPolicy<Num>;
using RomanoWolfResultReturnPolicy = FullPermutationResultPolicy<Num>;
template<typename T>
using RomanoWolfStrategySelectionPolicy = RomanoWolfStepdownCorrection<T>;

// Now, assemble these into the full Computation Policy
using RomanoWolfComputationPolicy = DefaultPermuteMarketChangesPolicy<
    Num,
    BacktestingStatPolicy<Num>,   // Instantiated type, not template template parameter
    RomanoWolfResultReturnPolicy,           // Override the default PValueReturnPolicy
    RomanoWolfStatCollectionPolicy          // Override the default PermutationTestingNullTestStatisticPolicy
>;

// Finally, define the fully configured Monte Carlo Test type
using RomanoWolfMcpt = MonteCarloPermuteMarketChanges<
    Num,
    BacktestingStatPolicy,
    RomanoWolfComputationPolicy
>;

using BenjaminiResultReturnPolicy = PValueReturnPolicy<Num>;
using BenjaminiStatCollectionPolicy = PermutationTestingNullTestStatisticPolicy<Num>;
using BenjaminiComputationPolicy = DefaultPermuteMarketChangesPolicy<
    Num,
    BacktestingStatPolicy<Num>,
    BenjaminiResultReturnPolicy,
    BenjaminiStatCollectionPolicy
>;

using BenjaminiMcpt = MonteCarloPermuteMarketChanges<
    Num,
    BacktestingStatPolicy,
    BenjaminiComputationPolicy
>;

template<typename T>
using BenjaminiStrategySelectionPolicy = AdaptiveBenjaminiHochbergYr2000<T>;

// Base interface for validation methods
class ValidationInterface
{
public:
  using SurvivingStrategiesIterator = typename PALMastersMonteCarloValidation<Num, BacktestingStatPolicy<Num>>::SurvivingStrategiesIterator;
  
  virtual ~ValidationInterface() = default;
  virtual void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                   std::shared_ptr<PriceActionLabSystem> patterns,
                                   const DateRange& dateRange,
                                   const Num& pvalThreshold) = 0;
  virtual SurvivingStrategiesIterator beginSurvivingStrategies() const = 0;
  virtual SurvivingStrategiesIterator endSurvivingStrategies() const = 0;
  virtual int getNumSurvivingStrategies() const = 0;
  virtual const PermutationStatisticsCollector<Num>& getStatisticsCollector() const = 0;
  
  // New methods for accessing all tested strategies and their p-values
  virtual std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const = 0;
  virtual Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const = 0;
};

// Wrapper for Masters validation
class MastersValidationWrapper : public ValidationInterface
{
private:
  PALMastersMonteCarloValidation<Num, BacktestingStatPolicy<Num>> validation;
    
public:
  explicit MastersValidationWrapper(unsigned long numPermutations)
    : validation(numPermutations)
  {
  }
    
  void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold) override
  {
    validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true);
  }
    
  SurvivingStrategiesIterator beginSurvivingStrategies() const override
  {
    return validation.beginSurvivingStrategies();
  }
    
  SurvivingStrategiesIterator endSurvivingStrategies() const override
  {
    return validation.endSurvivingStrategies();
  }
    
  int getNumSurvivingStrategies() const override
  {
    return validation.getNumSurvivingStrategies();
  }

  const PermutationStatisticsCollector<Num>& getStatisticsCollector() const
  {
    return validation.getStatisticsCollector();
  }
  
  std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
  {
    return validation.getAllTestedStrategies();
  }
  
  Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const override
  {
    return validation.getStrategyPValue(strategy);
  }
};

class PALRomanoWolfValidationWrapper : public ValidationInterface
{
private:
  PALRomanoWolfMonteCarloValidation<Num, BacktestingStatPolicy<Num>> validation;
    
public:
  explicit PALRomanoWolfValidationWrapper(unsigned long numPermutations)
    : validation(numPermutations)
  {
  }
    
  void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold) override
  {
    validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true);
  }
    
  SurvivingStrategiesIterator beginSurvivingStrategies() const override
  {
    return validation.beginSurvivingStrategies();
  }
    
  SurvivingStrategiesIterator endSurvivingStrategies() const override
  {
    return validation.endSurvivingStrategies();
  }
    
  int getNumSurvivingStrategies() const override
  {
    return validation.getNumSurvivingStrategies();
  }

  const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
  {
    // NOTE: The new Romano-Wolf class does not have a statistics collector.
    // This method will throw an exception as defined in the base interface.
    throw std::runtime_error("PALRomanoWolfValidationWrapper does not support statistics collection");
  }
  
  std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
  {
    return validation.getAllTestedStrategies();
  }
  
  Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const override
  {
    return validation.getStrategyPValue(strategy);
  }
};

// Wrapper for Romano-Wolf validation
class RomanoWolfValidationWrapper : public ValidationInterface
{
private:
  PALMonteCarloValidation<Num,
  	  RomanoWolfMcpt,                      // The fully configured test runner
  	  RomanoWolfStrategySelectionPolicy         // Template template parameter
  	  > validation;
    
public:
  explicit RomanoWolfValidationWrapper(unsigned long numPermutations)
    : validation(numPermutations)
  {
  }
    
  void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold) override
  {
    validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true);
  }
    
  SurvivingStrategiesIterator beginSurvivingStrategies() const override
  {
    return validation.beginSurvivingStrategies();
  }
    
  SurvivingStrategiesIterator endSurvivingStrategies() const override
  {
    return validation.endSurvivingStrategies();
  }
    
  int getNumSurvivingStrategies() const override
  {
    return validation.getNumSurvivingStrategies();
  }

  const PermutationStatisticsCollector<Num>& getStatisticsCollector() const
  {
    return validation.getStatisticsCollector();
  }
  
  std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
  {
    return validation.getAllTestedStrategies();
  }
  
  Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const override
  {
    return validation.getStrategyPValue(strategy);
  }
};

//
// Wrapper for Romano-Wolf validation
class BenjaminiHochbergValidationWrapper : public ValidationInterface
{
private:
  PALMonteCarloValidation<Num,
  	  BenjaminiMcpt,
  	  BenjaminiStrategySelectionPolicy
  	  > validation;
    
public:
  explicit BenjaminiHochbergValidationWrapper(unsigned long numPermutations)
    : validation(numPermutations)
  {
  }
    
  void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold) override
  {
    validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true);
  }
    
  SurvivingStrategiesIterator beginSurvivingStrategies() const override
  {
    return validation.beginSurvivingStrategies();
  }
    
  SurvivingStrategiesIterator endSurvivingStrategies() const override
  {
    return validation.endSurvivingStrategies();
  }
    
  int getNumSurvivingStrategies() const override
  {
    return validation.getNumSurvivingStrategies();
  }

  const PermutationStatisticsCollector<Num>& getStatisticsCollector() const
  {
    return validation.getStatisticsCollector();
  }
  
  std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
  {
    return validation.getAllTestedStrategies();
  }
  
  Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const override
  {
    return validation.getStrategyPValue(strategy);
  }
};

//

// Factory function to create validation objects
std::unique_ptr<ValidationInterface> createValidation(ValidationMethod method, unsigned long numPermutations)
{
    switch (method)
    {
    case ValidationMethod::Masters:
      return std::make_unique<MastersValidationWrapper>(numPermutations);

    case ValidationMethod::RomanoWolf:
      return std::make_unique<PALRomanoWolfValidationWrapper>(numPermutations);

    case ValidationMethod::BenjaminiHochberg:
      return std::make_unique<BenjaminiHochbergValidationWrapper>(numPermutations);

    default:
      throw std::invalid_argument("Unknown validation method");
    }
}

void usage()
{
    printf("Usage: PalValidator <configuration file> [permutations count] [p-value threshold] [validation method]\n");
    printf("  configuration file: Required - path to the configuration file\n");
    printf("  permutations count: Optional - number of permutations (will prompt if not provided)\n");
    printf("  p-value threshold:  Optional - p-value threshold (will prompt if not provided)\n");
    printf("  validation method:  Optional - validation method: 'masters' or 'romano-wolf' (will prompt if not provided)\n\n");
}

int error_with_usage()
{
    usage();
    return 1;
}

void writeBacktestPerformanceReport(std::ofstream& file, std::shared_ptr<BackTester<Num>> backtester)
{
  auto positionHistory = backtester->getClosedPositionHistory();
  
  // Write performance metrics to file
  file << "=== Backtest Performance Report ===" << std::endl;
  file << "Total Closed Positions: " << positionHistory.getNumPositions() << std::endl;
  file << "Number of Winning Trades: " << positionHistory.getNumWinningPositions() << std::endl;
  file << "Number of Losing Trades: " << positionHistory.getNumLosingPositions() << std::endl;
  file << "Total Bars in Market: " << positionHistory.getNumBarsInMarket() << std::endl;
  file << "Percent Winners: " << positionHistory.getPercentWinners() << "%" << std::endl;
  file << "Percent Losers: " << positionHistory.getPercentLosers() << "%" << std::endl;
  file << "Profit Factor: " << positionHistory.getProfitFactor() << std::endl;
  file << "PAL Profitability: " << positionHistory.getPALProfitability() << "%" << std::endl;
  file << "===================================" << std::endl << std::endl;
}

void writeMonteCarloPermutationStats(const PermutationStatisticsCollector<Num>& monteCarloStats,
				     std::ofstream& file, std::shared_ptr<PalStrategy<Num>> strategy)
{
  using MetricType = PermutationTestObserver<Num>::MetricType;
  
  // DEBUG: Add diagnostic logging for Monte Carlo statistics
  const PalStrategy<Num>* strategyPtr = strategy.get();
  //std::cout << "DEBUG: Monte Carlo Statistics Analysis:" << std::endl;
  //std::cout << "DEBUG: Strategy pointer: " << strategyPtr << std::endl;
  //std::cout << "DEBUG: Strategy name: " << strategy->getStrategyName() << std::endl;
  
  file << "=== Monte Carlo Permutation Statistics ===" << std::endl;
  
  // Get raw pointer from shared_ptr for the statistics methods
  
  // PERMUTED_TEST_STATISTIC metrics
  file << "Permuted Test Statistic:" << std::endl;
  auto median = monteCarloStats.getMedianMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
  auto min = monteCarloStats.getMinMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
  auto max = monteCarloStats.getMaxMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
  auto stdDev = monteCarloStats.getStdDevMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
  
  // DEBUG: Log what we got from the statistics collector
  //std::cout << "DEBUG: PERMUTED_TEST_STATISTIC - median: " << (median ? "has value" : "N/A") << std::endl;
  //std::cout << "DEBUG: PERMUTED_TEST_STATISTIC - min: " << (min ? "has value" : "N/A") << std::endl;
  //std::cout << "DEBUG: PERMUTED_TEST_STATISTIC - max: " << (max ? "has value" : "N/A") << std::endl;
  //std::cout << "DEBUG: PERMUTED_TEST_STATISTIC - stdDev: " << (stdDev ? "has value" : "N/A") << std::endl;
  
  file << "  Median: " << (median ? std::to_string(*median) : "N/A") << std::endl;
  file << "  Minimum: " << (min ? num::toString(*min) : "N/A") << std::endl;
  file << "  Maximum: " << (max ? num::toString(*max) : "N/A") << std::endl;
  file << "  Standard Deviation: " << (stdDev ? std::to_string(*stdDev) : "N/A") << std::endl;
  file << std::endl;
  
  // NUM_TRADES metrics
  file << "Number of Trades:" << std::endl;
  median = monteCarloStats.getMedianMetric(strategyPtr, MetricType::NUM_TRADES);
  min = monteCarloStats.getMinMetric(strategyPtr, MetricType::NUM_TRADES);
  max = monteCarloStats.getMaxMetric(strategyPtr, MetricType::NUM_TRADES);
  stdDev = monteCarloStats.getStdDevMetric(strategyPtr, MetricType::NUM_TRADES);
  
  file << "  Median: " << (median ? std::to_string(*median) : "N/A") << std::endl;
  file << "  Minimum: " << (min ? num::toString(*min) : "N/A") << std::endl;
  file << "  Maximum: " << (max ? num::toString(*max) : "N/A") << std::endl;
  file << "  Standard Deviation: " << (stdDev ? std::to_string(*stdDev) : "N/A") << std::endl;
  file << std::endl;
  
  // NUM_BARS_IN_TRADES metrics
  file << "Number of Bars in Trades:" << std::endl;
  median = monteCarloStats.getMedianMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
  min = monteCarloStats.getMinMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
  max = monteCarloStats.getMaxMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
  stdDev = monteCarloStats.getStdDevMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
  
  file << "  Median: " << (median ? std::to_string(*median) : "N/A") << std::endl;
  file << "  Minimum: " << (min ? num::toString(*min) : "N/A") << std::endl;
  file << "  Maximum: " << (max ? num::toString(*max) : "N/A") << std::endl;
  file << "  Standard Deviation: " << (stdDev ? std::to_string(*stdDev) : "N/A") << std::endl;
  file << std::endl;
  
  // BASELINE_STAT_EXCEEDANCE_RATE metric (single value, not a distribution)
  file << "Baseline Statistic Exceedance Rate:" << std::endl;
  auto exceedanceRate = monteCarloStats.getMinMetric(strategyPtr, MetricType::BASELINE_STAT_EXCEEDANCE_RATE);
  file << "  Rate: " << (exceedanceRate ? num::toString(*exceedanceRate) + "%" : "N/A") << std::endl;
  file << "===========================================" << std::endl << std::endl;
}

// Structure to hold extracted Monte Carlo statistics before cloning
struct ExtractedMonteCarloStats {
    std::optional<double> testStatMedian;
    std::optional<Num> testStatMin;
    std::optional<Num> testStatMax;
    std::optional<double> testStatStdDev;
    
    std::optional<double> numTradesMedian;
    std::optional<Num> numTradesMin;
    std::optional<Num> numTradesMax;
    std::optional<double> numTradesStdDev;
    
    std::optional<double> numBarsMedian;
    std::optional<Num> numBarsMin;
    std::optional<Num> numBarsMax;
    std::optional<double> numBarsStdDev;
    
    std::optional<Num> baselineExceedanceRate;
};

// Function to extract Monte Carlo statistics before cloning
ExtractedMonteCarloStats extractMonteCarloStats(const PermutationStatisticsCollector<Num>& monteCarloStats,
                                                std::shared_ptr<PalStrategy<Num>> strategy) {
    using MetricType = PermutationTestObserver<Num>::MetricType;
    const PalStrategy<Num>* strategyPtr = strategy.get();
    
    ExtractedMonteCarloStats extracted;
    
    // Extract PERMUTED_TEST_STATISTIC metrics
    extracted.testStatMedian = monteCarloStats.getMedianMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
    extracted.testStatMin = monteCarloStats.getMinMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
    extracted.testStatMax = monteCarloStats.getMaxMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
    extracted.testStatStdDev = monteCarloStats.getStdDevMetric(strategyPtr, MetricType::PERMUTED_TEST_STATISTIC);
    
    // Extract NUM_TRADES metrics
    extracted.numTradesMedian = monteCarloStats.getMedianMetric(strategyPtr, MetricType::NUM_TRADES);
    extracted.numTradesMin = monteCarloStats.getMinMetric(strategyPtr, MetricType::NUM_TRADES);
    extracted.numTradesMax = monteCarloStats.getMaxMetric(strategyPtr, MetricType::NUM_TRADES);
    extracted.numTradesStdDev = monteCarloStats.getStdDevMetric(strategyPtr, MetricType::NUM_TRADES);
    
    // Extract NUM_BARS_IN_TRADES metrics
    extracted.numBarsMedian = monteCarloStats.getMedianMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
    extracted.numBarsMin = monteCarloStats.getMinMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
    extracted.numBarsMax = monteCarloStats.getMaxMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
    extracted.numBarsStdDev = monteCarloStats.getStdDevMetric(strategyPtr, MetricType::NUM_BARS_IN_TRADES);
    
    // Extract BASELINE_STAT_EXCEEDANCE_RATE metric (single value)
    extracted.baselineExceedanceRate = monteCarloStats.getMinMetric(strategyPtr, MetricType::BASELINE_STAT_EXCEEDANCE_RATE);
    
    return extracted;
}

// Function to write extracted Monte Carlo statistics
void writeExtractedMonteCarloStats(const ExtractedMonteCarloStats& extractedStats, std::ofstream& file) {
    file << "=== Monte Carlo Permutation Statistics ===" << std::endl;
    
    // PERMUTED_TEST_STATISTIC metrics
    file << "Permuted Test Statistic:" << std::endl;
    file << "  Median: " << (extractedStats.testStatMedian ? std::to_string(*extractedStats.testStatMedian) : "N/A") << std::endl;
    file << "  Minimum: " << (extractedStats.testStatMin ? num::toString(*extractedStats.testStatMin) : "N/A") << std::endl;
    file << "  Maximum: " << (extractedStats.testStatMax ? num::toString(*extractedStats.testStatMax) : "N/A") << std::endl;
    file << "  Standard Deviation: " << (extractedStats.testStatStdDev ? std::to_string(*extractedStats.testStatStdDev) : "N/A") << std::endl;
    file << std::endl;
    
    // NUM_TRADES metrics
    file << "Number of Trades:" << std::endl;
    file << "  Median: " << (extractedStats.numTradesMedian ? std::to_string(*extractedStats.numTradesMedian) : "N/A") << std::endl;
    file << "  Minimum: " << (extractedStats.numTradesMin ? num::toString(*extractedStats.numTradesMin) : "N/A") << std::endl;
    file << "  Maximum: " << (extractedStats.numTradesMax ? num::toString(*extractedStats.numTradesMax) : "N/A") << std::endl;
    file << "  Standard Deviation: " << (extractedStats.numTradesStdDev ? std::to_string(*extractedStats.numTradesStdDev) : "N/A") << std::endl;
    file << std::endl;
    
    // NUM_BARS_IN_TRADES metrics
    file << "Number of Bars in Trades:" << std::endl;
    file << "  Median: " << (extractedStats.numBarsMedian ? std::to_string(*extractedStats.numBarsMedian) : "N/A") << std::endl;
    file << "  Minimum: " << (extractedStats.numBarsMin ? num::toString(*extractedStats.numBarsMin) : "N/A") << std::endl;
    file << "  Maximum: " << (extractedStats.numBarsMax ? num::toString(*extractedStats.numBarsMax) : "N/A") << std::endl;
    file << "  Standard Deviation: " << (extractedStats.numBarsStdDev ? std::to_string(*extractedStats.numBarsStdDev) : "N/A") << std::endl;
    file << std::endl;
    
    // BASELINE_STAT_EXCEEDANCE_RATE metric (single value)
    file << "Baseline Statistic Exceedance Rate:" << std::endl;
    file << "  Rate: " << (extractedStats.baselineExceedanceRate ? num::toString(*extractedStats.baselineExceedanceRate) + "%" : "N/A") << std::endl;
    file << "===========================================" << std::endl << std::endl;
}

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol, ValidationMethod method)
{
  std::string methodString;
  std::string underScore("_");
  
  if (method == ValidationMethod::Masters)
    methodString = "Masters";
  else if (method == ValidationMethod::RomanoWolf)
    methodString = "RomanoWolf";
  else
    methodString = "BenjaminiHochberg";
	    
  return (securitySymbol + underScore + methodString + underScore + std::string("SurvivingPatterns.txt"));
}

static std::string createDetailedSurvivingPatternsFileName (const std::string& securitySymbol,
							    ValidationMethod method)
{
  std::string methodString;
  std::string underScore("_");
  std::string detailed("Detailed");
  
  if (method == ValidationMethod::Masters)
    methodString = "Masters_";
  else if (method == ValidationMethod::RomanoWolf)
    methodString = "RomanoWolf_";
  else
    methodString = "BenjaminiHochberg_";
	    
  return (securitySymbol + underScore + methodString + detailed + std::string("_SurvivingPatterns.txt"));
}

static std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                          ValidationMethod method)
{
  std::string methodString;
  std::string underScore("_");
  std::string detailed("Detailed");
  
  if (method == ValidationMethod::Masters)
    methodString = "Masters_";
  else if (method == ValidationMethod::RomanoWolf)
    methodString = "RomanoWolf_";
  else
    methodString = "BenjaminiHochberg_";

  return (securitySymbol + underScore + methodString + detailed + std::string("_RejectedPatterns.txt"));
}

// Statistical analysis structures for rejected patterns
struct PValueDistributionStats {
    Num minPValue;
    Num maxPValue;
    Num meanPValue;
    Num medianPValue;
    Num stdDevPValue;
    std::vector<std::pair<Num, int>> pValueRanges; // range, count
    int totalRejected;
};

struct RejectionReasonStats {
    int totalPatterns;
    int survivingPatterns;
    int rejectedPatterns;
    double rejectionRate;
    
    // Breakdown by pattern type
    int rejectedLongPatterns;
    int rejectedShortPatterns;
    double longRejectionRate;
    double shortRejectionRate;
    
    // Performance-based rejection analysis
    int rejectedDueToLowProfitFactor;
    int rejectedDueToHighPValue;
    int rejectedDueToInsufficientTrades;
};

PValueDistributionStats calculatePValueDistribution(
    const std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>>& rejectedStrategiesWithPValues) {
    
    PValueDistributionStats stats = {};
    std::vector<Num> pValues;
    
    for (const auto& [strategy, pValue] : rejectedStrategiesWithPValues) {
        pValues.push_back(pValue);
    }
    
    if (pValues.empty()) return stats;
    
    std::sort(pValues.begin(), pValues.end());
    
    stats.totalRejected = static_cast<int>(pValues.size());
    stats.minPValue = pValues.front();
    stats.maxPValue = pValues.back();
    
    // Calculate mean
    Num sum = std::accumulate(pValues.begin(), pValues.end(), Num(0));
    stats.meanPValue = sum / Num(pValues.size());
    
    // Calculate median
    size_t mid = pValues.size() / 2;
    if (pValues.size() % 2 == 0) {
        stats.medianPValue = (pValues[mid-1] + pValues[mid]) / Num(2);
    } else {
        stats.medianPValue = pValues[mid];
    }
    
    // Calculate standard deviation
    Num variance = Num(0);
    for (const auto& pVal : pValues) {
        Num diff = pVal - stats.meanPValue;
        variance += diff * diff;
    }
    variance /= Num(pValues.size());
    stats.stdDevPValue = Num(sqrt(num::to_double(variance)));
    
    // Create p-value range distribution
    std::vector<Num> ranges = {Num(0.05), Num(0.1), Num(0.2), Num(0.3), Num(0.5), Num(0.7), Num(1.0)};
    for (size_t i = 0; i < ranges.size(); ++i) {
        Num lowerBound = (i == 0) ? Num(0) : ranges[i-1];
        Num upperBound = ranges[i];
        
        int count = static_cast<int>(std::count_if(pValues.begin(), pValues.end(),
            [lowerBound, upperBound](const Num& pVal) {
                return pVal > lowerBound && pVal <= upperBound;
            }));
        
        stats.pValueRanges.emplace_back(upperBound, count);
    }
    
    return stats;
}

RejectionReasonStats analyzeRejectionReasons(
    ValidationInterface* validation,
    const std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>>& rejectedStrategiesWithPValues,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame,
    std::shared_ptr<Security<Num>> baseSecurity) {
    
    RejectionReasonStats stats = {};
    
    auto allStrategies = validation->getAllTestedStrategies();
    stats.totalPatterns = static_cast<int>(allStrategies.size());
    stats.survivingPatterns = validation->getNumSurvivingStrategies();
    stats.rejectedPatterns = static_cast<int>(rejectedStrategiesWithPValues.size());
    stats.rejectionRate = (double)stats.rejectedPatterns / stats.totalPatterns * 100.0;
    
    // Analyze by pattern type
    for (const auto& [originalStrategy, pValue] : rejectedStrategiesWithPValues) {
        if (originalStrategy->getPalPattern()->isLongPattern()) {
            stats.rejectedLongPatterns++;
        } else {
            stats.rejectedShortPatterns++;
        }
        
        // Analyze rejection reasons by backtesting the strategy (with error handling)
        try {
            // Create a fresh portfolio and clone the strategy to avoid state conflicts
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(baseSecurity->getName() + " Analysis Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrategy = originalStrategy->clone2(freshPortfolio);
            
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrategy, theTimeFrame, backtestingDates);
            auto positionHistory = backtester->getClosedPositionHistory();
            
            if (positionHistory.getNumPositions() < 10) {
                stats.rejectedDueToInsufficientTrades++;
            }
            if (positionHistory.getProfitFactor() < 1.0) {
                stats.rejectedDueToLowProfitFactor++;
            }
        }
        catch (const std::exception& e) {
            std::cout << "WARNING: Could not analyze rejection reasons for strategy "
                      << originalStrategy->getStrategyName() << ": " << e.what() << std::endl;
            // Count as insufficient trades since we can't analyze it
            stats.rejectedDueToInsufficientTrades++;
        }
        
        if (pValue > Num(0.1)) {
            stats.rejectedDueToHighPValue++;
        }
    }
    
    // Calculate rates
    int totalLongPatterns = 0, totalShortPatterns = 0;
    for (const auto& [strategy, pValue] : allStrategies) {
        if (strategy->getPalPattern()->isLongPattern()) {
            totalLongPatterns++;
        } else {
            totalShortPatterns++;
        }
    }
    
    stats.longRejectionRate = totalLongPatterns > 0 ?
        (double)stats.rejectedLongPatterns / totalLongPatterns * 100.0 : 0.0;
    stats.shortRejectionRate = totalShortPatterns > 0 ?
        (double)stats.rejectedShortPatterns / totalShortPatterns * 100.0 : 0.0;
    
    return stats;
}

void writeSummaryStatistics(std::ofstream& file,
                           const PValueDistributionStats& pValueStats,
                           const RejectionReasonStats& rejectionStats,
                           ValidationMethod method,
                           const Num& pValueThreshold) {
    
    file << std::endl << std::endl;
    file << "=========================================" << std::endl;
    file << "           SUMMARY STATISTICS            " << std::endl;
    file << "=========================================" << std::endl << std::endl;
    
    // Overall Statistics
    file << "=== Overall Pattern Analysis ===" << std::endl;
    file << "Total Patterns Tested: " << rejectionStats.totalPatterns << std::endl;
    file << "Surviving Patterns: " << rejectionStats.survivingPatterns << std::endl;
    file << "Rejected Patterns: " << rejectionStats.rejectedPatterns << std::endl;
    file << "Overall Rejection Rate: " << std::fixed << std::setprecision(2)
         << rejectionStats.rejectionRate << "%" << std::endl;
    file << "Validation Method: " << (method == ValidationMethod::Masters ? "Masters" : "Romano-Wolf") << std::endl;
    file << "P-Value Threshold: " << pValueThreshold << std::endl;
    file << std::endl;
    
    // Pattern Type Breakdown
    file << "=== Pattern Type Breakdown ===" << std::endl;
    file << "Rejected Long Patterns: " << rejectionStats.rejectedLongPatterns << std::endl;
    file << "Rejected Short Patterns: " << rejectionStats.rejectedShortPatterns << std::endl;
    file << "Long Pattern Rejection Rate: " << std::fixed << std::setprecision(2)
         << rejectionStats.longRejectionRate << "%" << std::endl;
    file << "Short Pattern Rejection Rate: " << std::fixed << std::setprecision(2)
         << rejectionStats.shortRejectionRate << "%" << std::endl;
    file << std::endl;
    
    // P-Value Distribution Statistics
    file << "=== P-Value Distribution Analysis ===" << std::endl;
    file << "Minimum P-Value: " << pValueStats.minPValue << std::endl;
    file << "Maximum P-Value: " << pValueStats.maxPValue << std::endl;
    file << "Mean P-Value: " << std::fixed << std::setprecision(6) << pValueStats.meanPValue << std::endl;
    file << "Median P-Value: " << std::fixed << std::setprecision(6) << pValueStats.medianPValue << std::endl;
    file << "P-Value Standard Deviation: " << std::fixed << std::setprecision(6) << pValueStats.stdDevPValue << std::endl;
    file << std::endl;
    
    // P-Value Range Distribution
    file << "=== P-Value Range Distribution ===" << std::endl;
    file << "Range\t\tCount\t\tPercentage" << std::endl;
    file << "-----\t\t-----\t\t----------" << std::endl;
    
    Num previousRange = Num(0);
    for (const auto& [range, count] : pValueStats.pValueRanges) {
        double percentage = (double)count / pValueStats.totalRejected * 100.0;
        file << std::fixed << std::setprecision(2) << previousRange << " - " << range
             << "\t\t" << count << "\t\t" << percentage << "%" << std::endl;
        previousRange = range;
    }
    file << std::endl;
    
    // Rejection Reason Analysis
    file << "=== Rejection Reason Analysis ===" << std::endl;
    file << "Rejected due to High P-Value (>0.1): " << rejectionStats.rejectedDueToHighPValue << std::endl;
    file << "Rejected due to Low Profit Factor (<1.0): " << rejectionStats.rejectedDueToLowProfitFactor << std::endl;
    file << "Rejected due to Insufficient Trades (<10): " << rejectionStats.rejectedDueToInsufficientTrades << std::endl;
    file << std::endl;
    
    // Recommendations
    file << "=== Recommendations ===" << std::endl;
    if (rejectionStats.rejectionRate > 90.0) {
        file << "• Very high rejection rate suggests patterns may need refinement" << std::endl;
        file << "• Consider adjusting pattern selection criteria" << std::endl;
    } else if (rejectionStats.rejectionRate > 70.0) {
        file << "• High rejection rate is normal for robust validation" << std::endl;
        file << "• Focus on improving pattern quality" << std::endl;
    } else {
        file << "• Moderate rejection rate indicates good pattern quality" << std::endl;
    }
    
    if (pValueStats.meanPValue > Num(0.5)) {
        file << "• High mean p-value suggests patterns may lack statistical significance" << std::endl;
    }
    
    if (rejectionStats.rejectedDueToInsufficientTrades > rejectionStats.rejectedPatterns * 0.3) {
        file << "• Many patterns rejected due to insufficient trades" << std::endl;
        file << "• Consider longer backtesting periods or different markets" << std::endl;
    }
    
    file << "=========================================" << std::endl;
}

void writeRejectedPatternDetails(std::ofstream& file,
                                std::shared_ptr<PalStrategy<Num>> originalStrategy,
                                const Num& strategyPValue,
                                ValidationInterface* validation,
                                const DateRange& backtestingDates,
                                TimeFrame::Duration theTimeFrame,
                                const Num& pValueThreshold,
                                std::shared_ptr<Security<Num>> baseSecurity) {
    
    file << "Rejected Pattern:" << std::endl << std::endl;
    
    // Log the pattern details
    LogPalPattern::LogPattern(originalStrategy->getPalPattern(), file);
    file << std::endl;
    
    // Add rejection information
    file << "=== Rejection Information ===" << std::endl;
    file << "P-Value: " << strategyPValue << std::endl;
    file << "P-Value Threshold: " << pValueThreshold << std::endl;
    file << "Rejection Reason: P-Value (" << strategyPValue << ") > Threshold (" << pValueThreshold << ")" << std::endl;
    file << "============================" << std::endl << std::endl;
    
    // CRITICAL: Extract Monte Carlo statistics BEFORE cloning, as cloning changes strategy UUID
    // and makes the original strategy's statistics inaccessible
    auto& monteCarloStats = validation->getStatisticsCollector();
    
    auto extractedStats = extractMonteCarloStats(monteCarloStats, originalStrategy);
    
    try {
        // Create a fresh portfolio for the cloned strategy
        auto freshPortfolio = std::make_shared<Portfolio<Num>>(baseSecurity->getName() + " Fresh Portfolio");
        freshPortfolio->addSecurity(baseSecurity);
        
        // Clone the strategy with a fresh portfolio to avoid state conflicts
        auto clonedStrategy = originalStrategy->clone2(freshPortfolio);
        
        // Create backtester for the cloned strategy
        auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrategy,
                                                                   theTimeFrame,
                                                                   backtestingDates);
        
        // Write backtest performance report
        writeBacktestPerformanceReport(file, backtester);
        file << std::endl;
        
        // Write Monte Carlo permutation statistics using extracted data (obtained BEFORE cloning)
        //std::cout << "DEBUG: Writing extracted Monte Carlo stats" << std::endl;
        writeExtractedMonteCarloStats(extractedStats, file);
    }
    catch (const std::exception& e) {
        std::cout << "ERROR: Failed to backtest rejected strategy: " << e.what() << std::endl;
        file << "=== Backtest Error ===" << std::endl;
        file << "Could not generate backtest performance report for this rejected pattern." << std::endl;
        file << "Error: " << e.what() << std::endl;
        file << "This may indicate date range conflicts or pattern-specific issues." << std::endl;
        file << "======================" << std::endl << std::endl;
        
        // Still try to write Monte Carlo stats using extracted data
        try {
            std::cout << "DEBUG: Writing extracted Monte Carlo stats (error case)" << std::endl;
            writeExtractedMonteCarloStats(extractedStats, file);
        }
        catch (const std::exception& statsError) {
            file << "Monte Carlo statistics also unavailable: " << statsError.what() << std::endl;
        }
    }
}

void writeDetailedRejectedPatternsFile(const std::string& securitySymbol,
                                       ValidationMethod method,
                                       ValidationInterface* validation,
                                       const DateRange& backtestingDates,
                                       TimeFrame::Duration theTimeFrame,
                                       const Num& pValueThreshold,
                                       std::shared_ptr<Security<Num>> baseSecurity) {
    
    std::string detailedPatternsFileName = createDetailedRejectedPatternsFileName(securitySymbol, method);
    std::ofstream rejectedPatternsFile(detailedPatternsFileName);
    
    // Get all strategies and identify rejected ones with their p-values
    auto allStrategies = validation->getAllTestedStrategies();
    std::set<std::shared_ptr<PalStrategy<Num>>> survivingSet;
    for (auto it = validation->beginSurvivingStrategies();
         it != validation->endSurvivingStrategies(); ++it) {
        survivingSet.insert(*it);
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> rejectedStrategiesWithPValues;
    for (const auto& [strategy, pValue] : allStrategies) {
        if (survivingSet.find(strategy) == survivingSet.end()) {
            rejectedStrategiesWithPValues.emplace_back(strategy, pValue);
        }
    }
    
    // Write header
    rejectedPatternsFile << "=== REJECTED PATTERNS REPORT ===" << std::endl;
    rejectedPatternsFile << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    rejectedPatternsFile << "P-Value Threshold: " << pValueThreshold << std::endl;
    rejectedPatternsFile << "Validation Method: " <<
        (method == ValidationMethod::Masters ? "Masters" : "Romano-Wolf") << std::endl;
    rejectedPatternsFile << "=================================" << std::endl << std::endl;
    
    if (rejectedStrategiesWithPValues.empty()) {
        rejectedPatternsFile << "No rejected patterns found." << std::endl;
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "All " << validation->getNumSurvivingStrategies()
                            << " tested patterns survived the validation process." << std::endl;
        rejectedPatternsFile << "This indicates very strong patterns or a lenient p-value threshold." << std::endl;
        
        // Write basic summary statistics even when no rejected patterns are found
        RejectionReasonStats basicStats = {};
        basicStats.totalPatterns = static_cast<int>(allStrategies.size());
        basicStats.survivingPatterns = validation->getNumSurvivingStrategies();
        basicStats.rejectedPatterns = basicStats.totalPatterns - basicStats.survivingPatterns;
        basicStats.rejectionRate = basicStats.totalPatterns > 0 ?
            (double)basicStats.rejectedPatterns / basicStats.totalPatterns * 100.0 : 0.0;
        
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "=== Summary Statistics ===" << std::endl;
        rejectedPatternsFile << "Total Patterns Tested: " << basicStats.totalPatterns << std::endl;
        rejectedPatternsFile << "Surviving Patterns: " << basicStats.survivingPatterns << std::endl;
        rejectedPatternsFile << "Rejected Patterns: " << basicStats.rejectedPatterns << std::endl;
        rejectedPatternsFile << "Rejection Rate: " << std::fixed << std::setprecision(2)
                            << basicStats.rejectionRate << "%" << std::endl;
        
        return;
    }
    
    // Sort rejected strategies by p-value (ascending)
    std::sort(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Write detailed information for each rejected strategy
    for (const auto& [strategy, pValue] : rejectedStrategiesWithPValues) {
        writeRejectedPatternDetails(rejectedPatternsFile, strategy, pValue, validation,
                                   backtestingDates, theTimeFrame, pValueThreshold, baseSecurity);
        rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
    }
    
    // Calculate and write summary statistics
    auto pValueStats = calculatePValueDistribution(rejectedStrategiesWithPValues);
    auto rejectionStats = analyzeRejectionReasons(validation, rejectedStrategiesWithPValues,
                                                  backtestingDates, theTimeFrame, baseSecurity);
    
    writeSummaryStatistics(rejectedPatternsFile, pValueStats, rejectionStats, method, pValueThreshold);
}

void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
					ValidationMethod method,
					ValidationInterface* validation,
					const DateRange& backtestingDates,
					TimeFrame::Duration theTimeFrame)
{
  const std::string& securitySymbol = baseSecurity->getSymbol();
  std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(securitySymbol,
									       method));
  std::ofstream survivingPatternsFile(detailedPatternsFileName);
  
  for (auto it = validation->beginSurvivingStrategies();
       it != validation->endSurvivingStrategies();
       ++it)
    {
      auto strategy = *it;
      try
	{
	  auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
	  freshPortfolio->addSecurity(baseSecurity);
	  auto clonedStrat = strategy->clone2(freshPortfolio);
	  auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
								     theTimeFrame,
								     backtestingDates);
	  // Note: monteCarloStats not used for surviving patterns in this implementation
	  // auto& monteCarloStats = validation->getStatisticsCollector();
	  survivingPatternsFile << "Surviving Pattern:" << std::endl << std::endl;
	  LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
	  survivingPatternsFile << std::endl;
	  writeBacktestPerformanceReport(survivingPatternsFile, backtester);
	  survivingPatternsFile << std::endl << std::endl;
	  //BacktestingStatPolicy<Num>::printDetailedScoreBreakdown(backtester, survivingPatternsFile);
	  //writeMonteCarloPermutationStats(monteCarloStats, survivingPatternsFile, clonedStrat);
	}
      catch (const std::exception& e)
	{
	  std::cout << "Exception " << e.what() << std::endl;
	  break;
	}
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return error_with_usage();
    }

    const auto args = std::vector<std::string>(argv, argv + argc);
    const auto configurationFileName = std::string(args[1]);
    
    // Handle permutations count
    auto permutations = 5000;
    if (args.size() >= 3 && !args[2].empty())
    {
        permutations = std::stoi(args[2]);
    }
    else
    {
        std::cout << "Enter number of permutations (default 5000): ";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty())
        {
            permutations = std::stoi(input);
        }
    }
    
    // Handle p-value threshold
    auto pvalThreshold = Num(0.05);
    if (args.size() >= 4 && !args[3].empty())
    {
        pvalThreshold = Num(std::stod(args[3]));
    }
    else
    {
        std::cout << "Enter p-value threshold (default 0.05): ";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty())
        {
            pvalThreshold = Num(std::stod(input));
        }
    }
    
    // Handle validation method
    ValidationMethod validationMethod = ValidationMethod::Masters; // Default to Masters
    if (args.size() >= 5 && !args[4].empty())
    {
        std::string methodStr = args[4];
        std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(), ::tolower);
        if (methodStr == "masters")
        {
            validationMethod = ValidationMethod::Masters;
        }
        else if (methodStr == "romano-wolf")
        {
            validationMethod = ValidationMethod::RomanoWolf;
        }
        else if (methodStr == "benjamini-hochberg")
        {
            validationMethod = ValidationMethod::BenjaminiHochberg;;
        }
        else
        {
            std::cout << "Warning: Unknown validation method '" << args[4] << "'. Using Masters method." << std::endl;
        }
    }
    else
    {
        std::cout << "Choose validation method:" << std::endl;
        std::cout << "1. Masters validation (default)" << std::endl;
        std::cout << "2. Romano-Wolf validation" << std::endl;
        std::cout << "3. Benjamini-Hochberg validation" << std::endl;
        std::cout << "Enter choice (1, 2, or 3, default 1): ";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty())
        {
	  if (input == "2" || input == "romano-wolf" || input == "Romano-Wolf")
            {
	      validationMethod = ValidationMethod::RomanoWolf;
            }
	  else if (input == "3" || input == "benjamini-hochberg" || input == "Benjamini-Hochberg")
	    {
	      validationMethod = ValidationMethod::BenjaminiHochberg;
	    }
	  else if (input != "1" && input != "masters" && input != "Masters")
            {
	      std::cout << "Invalid choice. Using Masters method." << std::endl;
            }
        }
    }
    
    ValidatorConfigurationFileReader reader(configurationFileName);

    std::shared_ptr<ValidatorConfiguration<Num>> config;
    try
    {
        config = reader.readConfigurationFile();
    }
    catch (const SecurityAttributesFactoryException& e)
    {
        std::cout << "Error reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    
    // Print configuration summary
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    std::cout << "Security Ticker: " << config->getSecurity()->getSymbol() << std::endl;
    std::cout << "Number of Price Patterns: " << config->getPricePatterns()->getNumPatterns() << std::endl;
    std::cout << "In-Sample Date Range: " << config->getInsampleDateRange().getFirstDateTime()
              << " to " << config->getInsampleDateRange().getLastDateTime() << std::endl;
    std::cout << "Out-of-Sample Date Range: " << config->getOosDateRange().getFirstDateTime()
              << " to " << config->getOosDateRange().getLastDateTime() << std::endl;
    std::cout << "P-Value Threshold: " << pvalThreshold << std::endl;
    std::cout << "=============================" << std::endl;
    
    // Create validation object using factory
    auto validation = createValidation(validationMethod, permutations);
    
    // Display selected method
    std::string methodString = getValidationMethodString(validationMethod);
    std::cout << "\nUsing " << methodString
              << " validation method with " << permutations << " permutations." << std::endl;
    std::cout << "Starting Monte Carlo validation...\n" << std::endl;

    validation->runPermutationTests(config->getSecurity(),
				    config->getPricePatterns(),
				    config->getOosDateRange(),
				    pvalThreshold);
    std::cout << "Monte Carlo validation completed\n" << std::endl;
    std::cout << "Number of survving strategies = " << validation->getNumSurvivingStrategies() << std::endl;

    if (validation->getNumSurvivingStrategies() > 0)
      {
	std::string survivingFileName(createSurvivingPatternsFileName(config->getSecurity()->getSymbol(),
								     validationMethod));
	std::ofstream survivingPatternsFile(survivingFileName);

	std::cout << "Writing surviving patterns to file: " << survivingFileName << std::endl;
	
	for (auto it = validation->beginSurvivingStrategies();
	     it != validation->endSurvivingStrategies();
	     ++it)
	  {
	    LogPalPattern::LogPattern ((*it)->getPalPattern(), survivingPatternsFile);
	  }

	auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
	writeDetailedSurvivingPatternsFile(config->getSecurity(),
					   validationMethod,
					   validation.get(),
					   config->getOosDateRange(),
					   timeFrame);
      }

    // Write rejected patterns to file
    std::cout << "Writing rejected patterns to file..." << std::endl;
    auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
    writeDetailedRejectedPatternsFile(config->getSecurity()->getSymbol(),
                                      validationMethod,
                                      validation.get(),
                                      config->getOosDateRange(),
                                      timeFrame,
                                      pvalThreshold,
                                      config->getSecurity());

    return 0;
}
