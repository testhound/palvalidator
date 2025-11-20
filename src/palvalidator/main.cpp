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
#include <chrono>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <streambuf>
#include <optional>
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
#include "PalParseDriver.h"
#include "PalAst.h"
#include "number.h"
#include "BidAskSpread.h"
#include "StatUtils.h"
#include "TimeSeriesIndicators.h"
#include <cstdlib>

// New policy architecture includes
#include "PolicyRegistry.h"
#include "PolicyConfiguration.h"
#include "PolicyFactory.h"
#include "PolicySelector.h"
#include "PolicyRegistration.h"
#include "ValidationInterface.h"
#include "BiasCorrectedBootstrap.h"

// Utility modules
#include "utils/ValidationTypes.h"
#include "utils/TimeUtils.h"
#include "utils/OutputUtils.h"

// Analysis modules
#include "analysis/StatisticalTypes.h"
#include "analysis/RobustnessAnalyzer.h"
#include "analysis/DivergenceAnalyzer.h"
#include "analysis/FragileEdgeAnalyzer.h"

// Filtering modules
#include "filtering/FilteringTypes.h"
#include "filtering/PerformanceFilter.h"
#include "filtering/MetaStrategyAnalyzer.h"
#include "filtering/TradingHurdleCalculator.h"

// Reporting modules
#include "reporting/PerformanceReporter.h"
#include "reporting/PatternReporter.h"

using namespace mkc_timeseries;
using namespace palvalidator::utils;
using namespace palvalidator::analysis;
using namespace palvalidator::filtering;
using namespace palvalidator::reporting;

using Num = num::DefaultNumber;

// Global risk parameters (set once from user input)
static RiskParameters g_riskParameters;

// Legacy function - now delegated to PerformanceReporter
void writeBacktestPerformanceReport(std::ofstream& file, std::shared_ptr<BackTester<Num>> backtester)
{
    PerformanceReporter::writeBacktestReport(file, backtester);
}

// Calculate theoretical PAL profitability based on strategy's risk/reward parameters
template<typename Num>
Num calculateTheoreticalPALProfitability(std::shared_ptr<PalStrategy<Num>> strategy,
                                         Num targetProfitFactor = DecimalConstants<Num>::DecimalTwo)
{
    auto pattern = strategy->getPalPattern();
    Num target = pattern->getProfitTargetAsDecimal();
    Num stop = pattern->getStopLossAsDecimal();
    
    if (stop == DecimalConstants<Num>::DecimalZero) {
        return DecimalConstants<Num>::DecimalZero;
    }
    
    Num payoffRatio = target / stop;
    Num oneHundred = DecimalConstants<Num>::DecimalOneHundred;
    
    // Formula from BootStrappedProfitabilityPFPolicy::getPermutationTestStatistic
    Num expectedPALProfitability = (targetProfitFactor / (targetProfitFactor + payoffRatio)) * oneHundred;
    
    return expectedPALProfitability;
}





// Function to get risk parameters from user input
RiskParameters getRiskParametersFromUser()
{
    RiskParameters params;
    std::string input;

    std::cout << "\nEnter risk-free rate of return in % (default: 3): ";
    std::getline(std::cin, input);
    if (input.empty())
      {
        params.riskFreeRate = Num("0.03"); // 3% default
      }
    else
      {
        double userValue = std::stod(input);
        params.riskFreeRate = Num(userValue / 100.0); // Convert percentage to decimal
      }

    std::cout << "Enter risk premium in % (default: 5): ";
    std::getline(std::cin, input);

    if (input.empty())
      {
        params.riskPremium = Num("0.05"); // 5% default
      }
    else
      {
        double userValue = std::stod(input);
        params.riskPremium = Num(userValue / 100.0); // Convert percentage to decimal
      }

    return params;
}

// Centralized function to get risk parameters
const RiskParameters& getRiskParameters()
{
    return g_riskParameters;
}

// Legacy function - now delegated to PerformanceFilter
template<typename Num>
std::vector<std::shared_ptr<PalStrategy<Num>>>
filterSurvivingStrategiesByPerformance(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& inSampleBacktestingDates,
    const DateRange& oosBacktestingDates,
    TimeFrame::Duration theTimeFrame,
    std::ostream& os,
    unsigned int numResamples,
    std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats = std::nullopt)
{
    const Num confidenceLevel = Num("0.95");
    
    // The RiskParameters are no longer needed for PerformanceFilter construction
    PerformanceFilter filter(confidenceLevel, numResamples);
    return filter.filterByPerformance(survivingStrategies, baseSecurity, inSampleBacktestingDates,
                                      oosBacktestingDates, theTimeFrame, os, oosSpreadStats);
}

// Analyze meta-strategy performance using unified PalMetaStrategy approach
template<typename Num>
void filterMetaStrategy(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame,
    std::ostream& os,
    unsigned int numResamples,
    ValidationMethod validationMethod,
    std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats,
    const DateRange& inSampleDates)
{
    const Num confidenceLevel = Num("0.95");
    
    os << "\n" << std::string(80, '=') << std::endl;
    os << "META-STRATEGY ANALYSIS" << std::endl;
    os << std::string(80, '=') << std::endl;
    
    // The MetaStrategyAnalyzer might need to be updated as well, but for now,
    // we will pass a default-constructed RiskParameters object.
    MetaStrategyAnalyzer analyzer(getRiskParameters(), confidenceLevel, numResamples);
    analyzer.analyzeMetaStrategy(survivingStrategies, baseSecurity,
     backtestingDates, theTimeFrame,
				 os, validationMethod,
				 oosSpreadStats,
				 inSampleDates);
    
    os << std::string(80, '=') << std::endl;
}


// Legacy function - now delegated to PatternReporter
void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                        ValidationMethod method,
                                        ValidationInterface* validation,
                                        const DateRange& backtestingDates,
                                        TimeFrame::Duration theTimeFrame)
{
    auto survivingStrategies = validation->getSurvivingStrategies();
    PatternReporter::writeSurvivingPatterns(survivingStrategies, baseSecurity->getSymbol(), method);
}

// Legacy function - now delegated to PatternReporter (overloaded version)
void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                         ValidationMethod method,
                                         const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                                         const DateRange& backtestingDates,
                                         TimeFrame::Duration theTimeFrame,
                                         const std::string& policyName,
                                         const ValidationParameters& params)
{
    PatternReporter reporter;
    reporter.writeDetailedSurvivingPatterns(baseSecurity, method, strategies, backtestingDates, theTimeFrame, policyName, params);
}

// Legacy function - now delegated to PatternReporter
void writeDetailedRejectedPatternsFile(const std::string& securitySymbol,
                                       ValidationMethod method,
                                       ValidationInterface* validation,
                                       const DateRange& backtestingDates,
                                       TimeFrame::Duration theTimeFrame,
                                       const Num& pValueThreshold,
                                       std::shared_ptr<Security<Num>> baseSecurity,
                                       const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies = {})
{
    PatternReporter::writeRejectedPatterns(securitySymbol, method, validation, backtestingDates,
                                         theTimeFrame, pValueThreshold, baseSecurity, performanceFilteredStrategies);
}



// ---- Core Logic ----

// Helper function for bid/ask spread analysis
template<typename Num>
std::tuple<Num, Num> computeBidAskSpreadAnalysis(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                                 unsigned int numBootstrapSamples,
                                                 std::ostream& logStream)
{
    logStream << "\n=== Bid/Ask Spread Analysis ===" << std::endl;
    
    try {
        // Extract out-of-sample time series
        auto timeSeries = config->getSecurity()->getTimeSeries();
        auto oosTimeSeries = FilterTimeSeries(*timeSeries, config->getOosDateRange());
        
        logStream << "Out-of-sample period: " << config->getOosDateRange().getFirstDateTime()
                  << " to " << config->getOosDateRange().getLastDateTime() << std::endl;
        logStream << "Out-of-sample entries: " << oosTimeSeries.getNumEntries() << std::endl;
        
        // Check if we have sufficient data for spread calculation
        if (oosTimeSeries.getNumEntries() < 2) {
            logStream << "Warning: Insufficient data for bid/ask spread calculation (need at least 2 entries)" << std::endl;
            return std::make_tuple(DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero);
        }
        
        // Calculate bid/ask spreads using Corwin-Schultz method

	using CorwinSchultzCalc = mkc_timeseries::CorwinSchultzSpreadCalculator<Num>;
	auto spreads = CorwinSchultzCalc::calculateProportionalSpreadsVector(oosTimeSeries,
									     config->getSecurity()->getTick(),
									     CorwinSchultzCalc::NegativePolicy::Epsilon);
        
        logStream << "Calculated " << spreads.size() << " bid/ask spread measurements" << std::endl;
        
        if (spreads.empty()) {
            logStream << "Warning: No valid spread calculations could be performed" << std::endl;
            return std::make_tuple(DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero);
        }
        
        // Calculate basic statistics
        auto actualMean = mkc_timeseries::StatUtils<Num>::computeMean(spreads);
	auto actualMedian = mkc_timeseries::MedianOfVec(spreads);
	mkc_timeseries::RobustQn<Num> qnCalc;
	const Num spreadQn = qnCalc.getRobustQn(spreads);
        
        logStream << "Raw spread statistics:" << std::endl;
        logStream << "  Mean: " << actualMean << std::endl;
	logStream << "  Median: " << actualMedian << std::endl;
        logStream << "  Robust Qn: " << spreadQn << std::endl;
        
        
        // Convert to percentage terms for easier interpretation (multiply by 100)
        auto meanPercent = actualMean * DecimalConstants<Num>::DecimalOneHundred;
	auto medianPercent = actualMedian * DecimalConstants<Num>::DecimalOneHundred;
        auto spreadQnPercent = spreadQn * DecimalConstants<Num>::DecimalOneHundred;
        
        logStream << "Results in percentage terms:" << std::endl;
        logStream << "  Mean: " << meanPercent << "%" << std::endl;
	logStream << "  Median: " << medianPercent << "%" << std::endl;
        logStream << "  Robust Qn: " << spreadQnPercent << "%" << std::endl;
        logStream << "  (Current slippage estimate assumption: 0.10%)" << std::endl;
        
        logStream << "=== End Bid/Ask Spread Analysis ===" << std::endl;
        
        return std::make_tuple(actualMedian, spreadQn);
        
    } catch (const std::exception& e) {
        logStream << "Error in bid/ask spread analysis: " << e.what() << std::endl;
        return std::make_tuple(DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero);
    }
}

// Helper function for bootstrap analysis
template<typename Num>
void runBootstrapAnalysis(const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
                         std::shared_ptr<ValidatorConfiguration<Num>> config,
                         ValidationMethod validationMethod,
                         const std::string& policyName,
                         const ValidationParameters& params,
                         unsigned int numBootstrapSamples,
                         ValidationInterface* validation)
{
    // Create bootstrap log file and tee to both cout and file
    const std::string bootstrapPath = createBootstrapFileName(
        config->getSecurity()->getSymbol(), validationMethod);
    std::ofstream bootstrapFile(bootstrapPath);
    TeeStream bootlog(std::cout, bootstrapFile);

    bootlog << "\nApplying performance-based filtering to Monte Carlo surviving strategies..." << std::endl;

    // Perform bid/ask spread analysis on out-of-sample data
    [[maybe_unused]] auto [spreadMedian, spreadQn] = computeBidAskSpreadAnalysis<Num>(config, numBootstrapSamples, bootlog);

    // Bundle as optional stats to pass down the pipeline
    std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats;
    oosSpreadStats.emplace(palvalidator::filtering::OOSSpreadStats{ spreadMedian, spreadQn });
    
    // Apply performance-based filtering to Monte Carlo surviving strategies
    auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
    auto filteredStrategies = filterSurvivingStrategiesByPerformance<Num>(
        survivingStrategies,
        config->getSecurity(),
	config->getInsampleDateRange(),
        config->getOosDateRange(),
        timeFrame,
        bootlog,
        numBootstrapSamples,
	oosSpreadStats
    );
    
    // Identify strategies that were filtered out due to performance criteria
    std::vector<std::shared_ptr<PalStrategy<Num>>> performanceFilteredStrategies;
    std::set<std::shared_ptr<PalStrategy<Num>>> filteredSet(filteredStrategies.begin(), filteredStrategies.end());
    for (const auto& strategy : survivingStrategies) {
        if (filteredSet.find(strategy) == filteredSet.end()) {
            performanceFilteredStrategies.push_back(strategy);
        }
    }

    if (!filteredStrategies.empty())
    {
        filterMetaStrategy<Num>(filteredStrategies,
				config->getSecurity(),
				config->getOosDateRange(),
				timeFrame,
				bootlog,
				numBootstrapSamples,
				validationMethod,
				oosSpreadStats,
				config->getInsampleDateRange());
    }
    
    bootlog << "Performance filtering results: " << filteredStrategies.size() << " passed, "
              << performanceFilteredStrategies.size() << " filtered out" << std::endl;
    bootlog << "Bootstrap details written to: " << bootstrapPath << std::endl;
    
    // Write the performance-filtered surviving patterns to the basic file
    if (!filteredStrategies.empty()) {
        std::string fn = createSurvivingPatternsFileName(config->getSecurity()->getSymbol(), validationMethod);
        std::ofstream survivingPatternsFile(fn);
        std::cout << "Writing surviving patterns to file: " << fn << std::endl;
        
        for (const auto& strategy : filteredStrategies)
        {
            LogPalPattern::LogPattern(strategy->getPalPattern(), survivingPatternsFile);
        }
    }

    // Write detailed report using filtered strategies
    if (!filteredStrategies.empty()) {
        std::cout << "Writing detailed surviving patterns report for " << filteredStrategies.size()
                  << " performance-filtered strategies..." << std::endl;
        writeDetailedSurvivingPatternsFile(config->getSecurity(), validationMethod, filteredStrategies,
                                           config->getOosDateRange(), timeFrame, policyName, params);
    } else {
        std::cout << "No strategies passed performance filtering criteria. Skipping detailed report." << std::endl;
    }
}

// Helper function for generating reports
void generateReports(const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                    ValidationInterface* validation,
                    std::shared_ptr<ValidatorConfiguration<Num>> config,
                    ValidationMethod validationMethod,
                    const ValidationParameters& params)
{
    // Rejected patterns file writing has been disabled per user request
    // The function is kept for potential future reporting functionality
}

// This is the common worker function that runs the validation and prints results.
// It is called by the higher-level functions AFTER the validation object has been created.
void runValidationWorker(std::unique_ptr<ValidationInterface> validation,
                         std::shared_ptr<ValidatorConfiguration<Num>> config,
                         const ValidationParameters& params,
                         ValidationMethod validationMethod,
                         const std::string& policyName,
                         unsigned int numBootstrapSamples,
                         bool partitionByFamily = false)
{
    std::vector<std::shared_ptr<PalStrategy<Num>>> survivingStrategies;
    std::string survivorFileName;
    
    // Phase 1: Permutation Testing (if required)
    if (params.pipelineMode == PipelineMode::PermutationAndBootstrap ||
        params.pipelineMode == PipelineMode::PermutationOnly)
    {
        std::cout << "Starting Monte Carlo validation...\n" << std::endl;
        
        validation->runPermutationTests(config->getSecurity(),
            config->getPricePatterns(),
            config->getOosDateRange(),
            params.pValueThreshold,
            true, // Enable verbose logging by default
            partitionByFamily);
            
        std::cout << "\nMonte Carlo validation completed." << std::endl;
        std::cout << "Number of surviving strategies = " << validation->getNumSurvivingStrategies() << std::endl;
        
        if (validation->getNumSurvivingStrategies() > 0)
        {
            survivingStrategies = validation->getSurvivingStrategies();
            
            // Write Monte Carlo permutation survivors to file for potential future use
            survivorFileName = createPermutationTestSurvivorsFileName(
                config->getSecurity()->getSymbol(), validationMethod);
            writePermutationTestSurvivors(survivingStrategies, survivorFileName);
            std::cout << "Monte Carlo permutation survivors written to: " << survivorFileName << std::endl;
            std::cout << "These can be used later for bootstrap-only analysis." << std::endl;
        }
        else
        {
            std::cout << "No strategies survived Monte Carlo permutation testing." << std::endl;
        }
    }
    
    // Phase 2: Load survivors (if bootstrap-only mode)
    else if (params.pipelineMode == PipelineMode::BootstrapOnly)
    {
        if (params.survivorInputFile.empty())
        {
            throw std::invalid_argument("Bootstrap-only mode requires survivor input file");
        }
        
        if (!validateSurvivorFile(params.survivorInputFile))
        {
            throw std::invalid_argument("Invalid or missing survivor file: " + params.survivorInputFile);
        }
        
        std::cout << "Loading Monte Carlo permutation survivors from: " << params.survivorInputFile << std::endl;
        survivingStrategies = loadPermutationTestSurvivors<Num>(params.survivorInputFile, config->getSecurity());
        std::cout << "Loaded " << survivingStrategies.size() << " Monte Carlo surviving strategies" << std::endl;
    }
    
    // Phase 3: Bootstrap Analysis (if required)
    if (params.pipelineMode == PipelineMode::PermutationAndBootstrap ||
        params.pipelineMode == PipelineMode::BootstrapOnly)
    {
        if (!survivingStrategies.empty())
        {
            runBootstrapAnalysis(survivingStrategies, config, validationMethod,
                               policyName, params, numBootstrapSamples, validation.get());
        }
        else
        {
            std::cout << "No Monte Carlo surviving strategies available for bootstrap analysis." << std::endl;
        }
    }
    
    // Generate reports based on available data
    generateReports(survivingStrategies, validation.get(), config, validationMethod, params);
    
    std::cout << "Validation run finished." << std::endl;
}

// ---- Validation Method Specific Orchestrators ----

// Orchestrator for Masters Validation
void runValidationForMasters(std::shared_ptr<ValidatorConfiguration<Num>> config,
                             const ValidationParameters& params,
                             const std::string& policyName,
                             unsigned int numBootstrapSamples,
                             bool partitionByFamily = false)
{
    std::cout << "\nUsing Masters validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }
    
    try {
        auto validation = statistics::PolicyFactory::createMastersValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Masters, policyName, numBootstrapSamples, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Masters validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Romano-Wolf Validation
void runValidationForRomanoWolf(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
                                unsigned int numBootstrapSamples,
                                bool partitionByFamily = false)
{
    std::cout << "\nUsing Romano-Wolf validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;

    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }

    try {
        auto validation = statistics::PolicyFactory::createRomanoWolfValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::RomanoWolf, policyName, numBootstrapSamples, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Romano-Wolf validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Benjamini-Hochberg Validation
void runValidationForBenjaminiHochberg(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                       const ValidationParameters& params,
                                       const std::string& policyName,
                                       unsigned int numBootstrapSamples,
                                       bool partitionByFamily = false)
{
    std::cout << "\nUsing Benjamini-Hochberg validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    std::cout << "[INFO] False Discovery Rate (FDR) set to: " << params.falseDiscoveryRate << std::endl;

    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: None (all patterns tested together)" << std::endl;
    }

    try {
        auto validation = statistics::PolicyFactory::createBenjaminiHochbergValidation(
            policyName, params.permutations, params.falseDiscoveryRate.getAsDouble());
        runValidationWorker(std::move(validation), config, params, ValidationMethod::BenjaminiHochberg, policyName, numBootstrapSamples, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Benjamini-Hochberg validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Unadjusted Validation
void runValidationForUnadjusted(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
                                unsigned int numBootstrapSamples,
                                bool partitionByFamily = false)
{
    std::cout << "\nUsing Unadjusted validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }
    
    try {
        auto validation = statistics::PolicyFactory::createUnadjustedValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Unadjusted, policyName, numBootstrapSamples, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Unadjusted validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}


// ---- Main Application Entry Point ----

void usage()
{
    printf("Usage: PalValidator <config file>\n");
    printf("  All other parameters will be requested via interactive prompts.\n");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage();
        return 1;
    }
    
    // Initialize the policy registry with all available policies
    std::cout << "Initializing policy registry..." << std::endl;
    statistics::initializePolicyRegistry();
    
    // Load policy configuration (optional)
    palvalidator::PolicyConfiguration policyConfig;
    std::string configPath = "policies.json";
    if (!policyConfig.loadFromFile(configPath)) {
        std::cout << "No policy configuration file found, using defaults." << std::endl;
        policyConfig = palvalidator::PolicyConfiguration::createDefault();
    }
    
    // -- Configuration File Reading with existence check --
    std::string configurationFileName = std::string(argv[1]);
    std::shared_ptr<ValidatorConfiguration<Num>> config;
    
    // Check if configuration file exists before asking for other inputs
    if (!std::filesystem::exists(configurationFileName)) {
        std::cout << "Error: Configuration file '" << configurationFileName << "' does not exist." << std::endl;
        std::cout << "Please enter the correct configuration file path: ";
        std::getline(std::cin, configurationFileName);
    }
    
    // Try to read the configuration file
    ValidatorConfigurationFileReader reader(configurationFileName);
    try {
        config = reader.readConfigurationFile();
    }
    catch (const SecurityAttributesFactoryException& e) {
        std::cout << "SecurityAttributesFactoryException: Error reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    catch (const ValidatorConfigurationException& e) {
        std::cout << "ValidatorConfigurationException thrown when reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    
    // -- Get parameters interactively --
    ValidationParameters params;
    // Initialize new fields with defaults
    params.pipelineMode = PipelineMode::PermutationAndBootstrap;
    params.survivorInputFile = "";
    std::string input;

    // Pipeline mode selection - ASK THIS FIRST
    std::cout << "\nChoose pipeline execution mode:" << std::endl;
    std::cout << "  1. Full Pipeline (Permutation + Bootstrap) - Default" << std::endl;
    std::cout << "  2. Permutation Testing Only" << std::endl;
    std::cout << "  3. Bootstrap Analysis Only" << std::endl;
    std::cout << "Enter choice (1, 2, or 3): ";
    std::getline(std::cin, input);

    PipelineMode pipelineMode = PipelineMode::PermutationAndBootstrap;
    if (input == "2") {
        pipelineMode = PipelineMode::PermutationOnly;
    } else if (input == "3") {
        pipelineMode = PipelineMode::BootstrapOnly;
    }

    params.pipelineMode = pipelineMode;

    // If bootstrap-only mode, ask for Monte Carlo survivor file
    if (pipelineMode == PipelineMode::BootstrapOnly) {
        std::cout << "Enter path to Monte Carlo survivor strategies file: ";
        std::getline(std::cin, params.survivorInputFile);
        
        if (!validateSurvivorFile(params.survivorInputFile)) {
            std::cout << "Error: Invalid or missing Monte Carlo survivor file: " << params.survivorInputFile << std::endl;
            return 1;
        }
    }

    // Only ask for permutation parameters if permutation testing will be performed
    if (pipelineMode == PipelineMode::PermutationAndBootstrap ||
        pipelineMode == PipelineMode::PermutationOnly) {
        
        std::cout << "\nEnter number of permutations (default: 10000): ";
        std::getline(std::cin, input);
        params.permutations = input.empty() ? 10000 : std::stoul(input);

        std::cout << "Enter p-value threshold (default: 0.05): ";
        std::getline(std::cin, input);
        params.pValueThreshold = input.empty() ? Num(0.05) : Num(std::stod(input));
    } else {
        // Set default values for bootstrap-only mode (won't be used)
        params.permutations = 10000;
        params.pValueThreshold = Num(0.05);
    }
    
    // Only ask for bootstrap samples if bootstrap analysis will be performed
    unsigned int numBootstrapSamples;
    if (pipelineMode == PipelineMode::PermutationAndBootstrap ||
        pipelineMode == PipelineMode::BootstrapOnly) {
        
        std::cout << "\nEnter number of bootstrap samples (default: 25000): ";
        std::getline(std::cin, input);
        numBootstrapSamples = input.empty() ? 25000 : std::stoul(input);
    } else {
        // Set default value for permutation-only mode (won't be used)
        numBootstrapSamples = 25000;
    }
    
    // Set validation method based on pipeline mode
    ValidationMethod validationMethod;
    bool partitionByFamily = false;
    std::string selectedPolicy = "GatedPerformanceScaledPalPolicy"; // Default for bootstrap-only
    
    if (pipelineMode == PipelineMode::BootstrapOnly) {
        // Bootstrap-only mode: no multiple testing correction applied
        validationMethod = ValidationMethod::Unadjusted;
        std::cout << "\nBootstrap-only mode: Using Unadjusted validation method (no multiple testing correction)" << std::endl;
    } else if (pipelineMode == PipelineMode::PermutationAndBootstrap ||
               pipelineMode == PipelineMode::PermutationOnly) {
        // Permutation testing modes: ask user for validation method
        validationMethod = ValidationMethod::Masters; // Default for permutation modes
        
        // Ask for Validation Method
        std::cout << "\nChoose validation method:" << std::endl;
        std::cout << "  1. Masters (default)" << std::endl;
        std::cout << "  2. Romano-Wolf" << std::endl;
        std::cout << "  3. Benjamini-Hochberg" << std::endl;
        std::cout << "  4. Unadjusted" << std::endl;
        std::cout << "Enter choice (1, 2, 3, or 4): ";
        std::getline(std::cin, input);
        
        if (input == "2") {
            validationMethod = ValidationMethod::RomanoWolf;
        } else if (input == "3") {
            validationMethod = ValidationMethod::BenjaminiHochberg;
        } else if (input == "4") {
            validationMethod = ValidationMethod::Unadjusted;
        }
        
        // Conditionally ask for FDR
        params.falseDiscoveryRate = Num(0.10); // Set default
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "Enter False Discovery Rate (FDR) for Benjamini-Hochberg (default: 0.10): ";
            std::getline(std::cin, input);
            if (!input.empty()) {
                params.falseDiscoveryRate = Num(std::stod(input));
            }
        }
        
        // Ask about pattern partitioning for Masters, Romano-Wolf, and Benjamini-Hochberg methods
        if (validationMethod == ValidationMethod::Masters ||
            validationMethod == ValidationMethod::RomanoWolf ||
            validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "\nPattern Partitioning Options:" << std::endl;
            
            if (validationMethod == ValidationMethod::BenjaminiHochberg) {
                std::cout << "  1. No Partitioning (all patterns tested together) - Default" << std::endl;
                std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
            } else {
                std::cout << "  1. By Direction Only (Long vs Short) - Default" << std::endl;
                std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
            }
            
            std::cout << "Choose partitioning method (1 or 2): ";
            std::getline(std::cin, input);
            
            if (input == "2") {
                partitionByFamily = true;
                std::cout << "Selected: Detailed family partitioning" << std::endl;
            } else {
                if (validationMethod == ValidationMethod::BenjaminiHochberg) {
                    std::cout << "Selected: No partitioning (default)" << std::endl;
                } else {
                    std::cout << "Selected: Direction-only partitioning (default)" << std::endl;
                }
            }
        }

        // Interactive policy selection using the new system
        std::cout << "\n=== Policy Selection ===" << std::endl;
        auto availablePolicies = palvalidator::PolicyRegistry::getAvailablePolicies();
        std::cout << "Available policies: " << availablePolicies.size() << std::endl;
        
        if (policyConfig.getPolicySettings().interactiveMode) {
            selectedPolicy = statistics::PolicySelector::selectPolicy(availablePolicies, &policyConfig);
        } else {
            // Use default policy from configuration
            selectedPolicy = policyConfig.getDefaultPolicy();
            if (selectedPolicy.empty() || !palvalidator::PolicyRegistry::isPolicyAvailable(selectedPolicy)) {
                selectedPolicy = "GatedPerformanceScaledPalPolicy"; // Fallback default
            }
            std::cout << "Using configured default policy: " << selectedPolicy << std::endl;
        }

        // Display selected policy information
        try {
            auto metadata = palvalidator::PolicyRegistry::getPolicyMetadata(selectedPolicy);
            std::cout << "\nSelected Policy: " << metadata.displayName << std::endl;
            std::cout << "Description: " << metadata.description << std::endl;
            std::cout << "Category: " << metadata.category << std::endl;
            if (metadata.isExperimental) {
                std::cout << "⚠️  WARNING: This is an experimental policy!" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not retrieve policy metadata: " << e.what() << std::endl;
        }
    } else {
        // Bootstrap-only mode: set defaults for unused parameters
        params.falseDiscoveryRate = Num(0.10);
        // No policy needed for bootstrap-only mode
    }
    
    // Get risk parameters from user and store globally
    g_riskParameters = getRiskParametersFromUser();
    
    // -- Summary --
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    std::cout << "Security Ticker: " << config->getSecurity()->getSymbol() << std::endl;
    std::cout << "In-Sample Range: " << config->getInsampleDateRange().getFirstDateTime()
              << " to " << config->getInsampleDateRange().getLastDateTime() << std::endl;
    std::cout << "Pipeline Mode: " << getPipelineModeString(params.pipelineMode) << std::endl;
    if (params.pipelineMode == PipelineMode::BootstrapOnly) {
        std::cout << "Monte Carlo Survivor Input File: " << params.survivorInputFile << std::endl;
    }
    std::cout << "Validation Method: " << getValidationMethodString(validationMethod) << std::endl;
    std::cout << "Computation Policy: " << selectedPolicy << std::endl;
    if (validationMethod == ValidationMethod::Masters ||
        validationMethod == ValidationMethod::RomanoWolf ||
        validationMethod == ValidationMethod::BenjaminiHochberg) {
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "None") << std::endl;
        } else {
            std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "By Direction Only") << std::endl;
        }
    } else if (validationMethod == ValidationMethod::Unadjusted) {
        std::cout << "Pattern Partitioning: None (not applicable for Unadjusted)" << std::endl;
    }
    
    // Only show permutation parameters if permutation testing will be performed
    if (params.pipelineMode == PipelineMode::PermutationAndBootstrap ||
        params.pipelineMode == PipelineMode::PermutationOnly) {
        std::cout << "Permutations: " << params.permutations << std::endl;
        std::cout << "P-Value Threshold: " << params.pValueThreshold << std::endl;
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
        }
    }
    
    // Only show bootstrap samples if bootstrap analysis will be performed
    if (params.pipelineMode == PipelineMode::PermutationAndBootstrap ||
        params.pipelineMode == PipelineMode::BootstrapOnly) {
        std::cout << "Bootstrap Samples: " << numBootstrapSamples << std::endl;
    }
    
    std::cout << "Risk-Free Rate: " << (g_riskParameters.riskFreeRate * DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
    std::cout << "Risk Premium: " << (g_riskParameters.riskPremium * DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
    std::cout << "=============================" << std::endl;

    // Record start time for validation pipeline
    auto validationStartTime = std::chrono::steady_clock::now();
    std::cout << "\nStarting validation pipeline..." << std::endl;

    // -- Top-level dispatch based on the VALIDATION METHOD --
    try {
        switch (validationMethod)
        {
            case ValidationMethod::Masters:
                runValidationForMasters(config, params, selectedPolicy, numBootstrapSamples, partitionByFamily);
                break;
            case ValidationMethod::RomanoWolf:
                runValidationForRomanoWolf(config, params, selectedPolicy, numBootstrapSamples, partitionByFamily);
                break;
            case ValidationMethod::BenjaminiHochberg:
                runValidationForBenjaminiHochberg(config, params, selectedPolicy, numBootstrapSamples, partitionByFamily);
                break;
            case ValidationMethod::Unadjusted:
                runValidationForUnadjusted(config, params, selectedPolicy, numBootstrapSamples, partitionByFamily);
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Validation failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Record end time and calculate elapsed time
    auto validationEndTime = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(validationEndTime - validationStartTime);
    
    // Convert to hours, minutes, seconds
    auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsedTime);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsedTime - hours);
    auto seconds = elapsedTime - hours - minutes;
    
    // Display elapsed time in HH:MM:SS format
    std::cout << "\nValidation pipeline completed." << std::endl;
    std::cout << "Total elapsed time: "
              << std::setfill('0') << std::setw(2) << hours.count() << ":"
              << std::setfill('0') << std::setw(2) << minutes.count() << ":"
              << std::setfill('0') << std::setw(2) << seconds.count() << std::endl;
    
    return 0;
}
