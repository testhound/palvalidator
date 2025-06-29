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

// ---- Enums, Structs, and Helper Functions ----

enum class ValidationMethod
{
    Masters,
    RomanoWolf,
    BenjaminiHochberg
};

enum class ComputationPolicy
{
    RobustProfitFactor,
    AllHighResLogPF,
    GatedPerformanceScaledPal
};

struct ValidationParameters
{
    unsigned long permutations;
    Num pValueThreshold;
    Num falseDiscoveryRate; // For Benjamini-Hochberg
};

std::string getValidationMethodString(ValidationMethod method)
{
    switch (method)
    {
        case ValidationMethod::Masters:
            return "Masters";
        case ValidationMethod::RomanoWolf:
            return "RomanoWolf";
        case ValidationMethod::BenjaminiHochberg:
            return "BenjaminiHochberg";
        default:
            throw std::invalid_argument("Unknown validation method");
    }
}

std::string getComputationPolicyString(ComputationPolicy policy)
{
    switch (policy)
    {
        case ComputationPolicy::RobustProfitFactor:
            return "RobustProfitFactorPolicy";
        case ComputationPolicy::AllHighResLogPF:
            return "AllHighResLogPFPolicy";
        case ComputationPolicy::GatedPerformanceScaledPal:
            return "GatedPerformanceScaledPalPolicy";
        default:
            throw std::invalid_argument("Unknown computation policy");
    }
}


// ---- Abstract Validation Interface ----

class ValidationInterface
{
public:
    virtual ~ValidationInterface() = default;
    virtual void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                     std::shared_ptr<PriceActionLabSystem> patterns,
                                     const DateRange& dateRange,
                                     const Num& pvalThreshold) = 0;
    virtual std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const = 0;
    virtual int getNumSurvivingStrategies() const = 0;
    virtual const PermutationStatisticsCollector<Num>& getStatisticsCollector() const = 0;
    
    virtual std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const = 0;
    virtual Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const = 0;
};

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol, ValidationMethod method)
{
    return securitySymbol + "_" + getValidationMethodString(method) + "_SurvivingPatterns.txt";
}

static std::string createDetailedSurvivingPatternsFileName (const std::string& securitySymbol,
                                                            ValidationMethod method)
{
    return securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_SurvivingPatterns.txt";
}

static std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                          ValidationMethod method)
{
    return securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_RejectedPatterns.txt";
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

void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                        ValidationMethod method,
                                        ValidationInterface* validation,
                                        const DateRange& backtestingDates,
                                        TimeFrame::Duration theTimeFrame)
{
    std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(baseSecurity->getSymbol(),
                                                                                 method));
    std::ofstream survivingPatternsFile(detailedPatternsFileName);
    
    auto survivingStrategies = validation->getSurvivingStrategies();
    for (const auto& strategy : survivingStrategies)
    {
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

void writeDetailedRejectedPatternsFile(const std::string& securitySymbol,
                                       ValidationMethod method,
                                       ValidationInterface* validation,
                                       const DateRange& backtestingDates,
                                       TimeFrame::Duration theTimeFrame,
                                       const Num& pValueThreshold,
                                       std::shared_ptr<Security<Num>> baseSecurity)
{
    std::string detailedPatternsFileName = createDetailedRejectedPatternsFileName(securitySymbol, method);
    std::ofstream rejectedPatternsFile(detailedPatternsFileName);
    
    // Get all strategies and identify rejected ones with their p-values
    auto allStrategies = validation->getAllTestedStrategies();
    std::set<std::shared_ptr<PalStrategy<Num>>> survivingSet;
    auto survivingStrategies = validation->getSurvivingStrategies();
    for (const auto& strategy : survivingStrategies)
    {
        survivingSet.insert(strategy);
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> rejectedStrategiesWithPValues;
    for (const auto& [strategy, pValue] : allStrategies)
    {
        if (survivingSet.find(strategy) == survivingSet.end())
        {
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
    
    if (rejectedStrategiesWithPValues.empty())
    {
        rejectedPatternsFile << "No rejected patterns found." << std::endl;
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "All " << validation->getNumSurvivingStrategies()
                            << " tested patterns survived the validation process." << std::endl;
        rejectedPatternsFile << "This indicates very strong patterns or a lenient p-value threshold." << std::endl;
        
        // Write basic summary statistics even when no rejected patterns are found
        struct RejectionReasonStats {
            int totalPatterns = 0;
            int survivingPatterns = 0;
            int rejectedPatterns = 0;
            double rejectionRate = 0.0;
        };
        
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
    for (const auto& [strategy, pValue] : rejectedStrategiesWithPValues)
    {
        // Write rejected pattern details inline since function is not defined
        rejectedPatternsFile << "Rejected Pattern (p-value: " << pValue << "):" << std::endl;
        LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
        rejectedPatternsFile << "P-Value: " << pValue << std::endl;
        rejectedPatternsFile << "Threshold: " << pValueThreshold << std::endl;
        rejectedPatternsFile << "Reason: P-value exceeds threshold" << std::endl;
        rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
    }
    
    // Calculate and write summary statistics
    // Write summary statistics inline since functions are not defined
    rejectedPatternsFile << std::endl << "=== Summary Statistics ===" << std::endl;
    rejectedPatternsFile << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    rejectedPatternsFile << "Validation Method: " << getValidationMethodString(method) << std::endl;
    rejectedPatternsFile << "P-Value Threshold: " << pValueThreshold << std::endl;
    
    if (!rejectedStrategiesWithPValues.empty()) {
        auto minPValue = std::min_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        auto maxPValue = std::max_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        rejectedPatternsFile << "Min P-Value: " << minPValue << std::endl;
        rejectedPatternsFile << "Max P-Value: " << maxPValue << std::endl;
    }
}



// ---- Core Logic ----

// This is the common worker function that runs the validation and prints results.
// It is called by the higher-level functions AFTER the validation object has been created.
void runValidationWorker(std::unique_ptr<ValidationInterface> validation,
                         std::shared_ptr<ValidatorConfiguration<Num>> config,
                         const ValidationParameters& params,
                         ValidationMethod validationMethod)
{
    std::cout << "Starting Monte Carlo validation...\n" << std::endl;

    validation->runPermutationTests(config->getSecurity(),
				    config->getPricePatterns(),
				    config->getOosDateRange(),
				    params.pValueThreshold);

    std::cout << "\nMonte Carlo validation completed." << std::endl;
    std::cout << "Number of surviving strategies = " << validation->getNumSurvivingStrategies() << std::endl;

    // -- Output --
    if (validation->getNumSurvivingStrategies() > 0)
    {
        std::string fn = createSurvivingPatternsFileName(config->getSecurity()->getSymbol(), validationMethod);
        std::ofstream survivingPatternsFile(fn);
        std::cout << "Writing surviving patterns to file: " << fn << std::endl;
        
        auto survivingStrategies = validation->getSurvivingStrategies();
        for (const auto& strategy : survivingStrategies)
        {
            LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
        }

        auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
        writeDetailedSurvivingPatternsFile(config->getSecurity(), validationMethod, validation.get(),
                                           config->getOosDateRange(), timeFrame);
    }

    std::cout << "Writing detailed rejected patterns report..." << std::endl;
    auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
    writeDetailedRejectedPatternsFile(config->getSecurity()->getSymbol(), validationMethod, validation.get(),
                                      config->getOosDateRange(), timeFrame, params.pValueThreshold,
                                      config->getSecurity());
    
    std::cout << "Validation run finished." << std::endl;
}

// ---- Validation Method Specific Orchestrators ----

// Orchestrator for Masters Validation
void runValidationForMasters(std::shared_ptr<ValidatorConfiguration<Num>> config,
                             const ValidationParameters& params,
                             ComputationPolicy computationPolicy)
{
    std::cout << "\nUsing Masters validation with " << getComputationPolicyString(computationPolicy)
              << " and " << params.permutations << " permutations." << std::endl;
    
    // This templated lambda creates the specific validation wrapper based on the policy.
    auto createAndRun = [&]<template<typename> class Pol>()
    {
        // ValidationWrapper type alias removed as it's not used
        
        // This is a concrete implementation of the interface, not part of the public API.
        class MastersValidationWrapper : public ValidationInterface
        {
        private:
            PALMastersMonteCarloValidation<Num, Pol<Num>> validation;
        public:
            explicit MastersValidationWrapper(unsigned long p) : validation(p)
            {
            }

            void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                   std::shared_ptr<PriceActionLabSystem> patterns,
                                   const DateRange& dateRange,
                                   const Num& pvalThreshold) override
            {
                validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true, true);
            }
            
            std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
            {
                std::vector<std::shared_ptr<PalStrategy<Num>>> s;
                for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
                    s.push_back(*it);

                return s;
            }
            int getNumSurvivingStrategies() const override
            {
                return validation.getNumSurvivingStrategies();
            }
            
            const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
            {
                return validation.getStatisticsCollector();
            }
            
            std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
            {
                return validation.getAllTestedStrategies();
            }

            Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
            {
                return validation.getStrategyPValue(s);
            }
        };
        
        auto validation = std::make_unique<MastersValidationWrapper>(params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Masters);
    };

    // Dispatch to the correct template instantiation based on runtime policy choice
    switch (computationPolicy)
    {
        case ComputationPolicy::RobustProfitFactor:
            createAndRun.template operator()<RobustProfitFactorPolicy>();
            break;
        
        case ComputationPolicy::AllHighResLogPF:
            createAndRun.template operator()<AllHighResLogPFPolicy>();
            break;
        
        case ComputationPolicy::GatedPerformanceScaledPal:
            createAndRun.template operator()<GatedPerformanceScaledPalPolicy>();
            break;
    }
}


// Orchestrator for Romano-Wolf Validation
void runValidationForRomanoWolf(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                ComputationPolicy computationPolicy)
{
    std::cout << "\nUsing Romano-Wolf validation with " << getComputationPolicyString(computationPolicy)
              << " and " << params.permutations << " permutations." << std::endl;

    auto createAndRun = [&]<template<typename> class Pol>()
    {
        class RomanoWolfValidationWrapper : public ValidationInterface
        {
        private:
            PALRomanoWolfMonteCarloValidation<Num, Pol<Num>> validation;
        public:
            explicit RomanoWolfValidationWrapper(unsigned long p) : validation(p)
            {
            }
            void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                   std::shared_ptr<PriceActionLabSystem> patterns,
                                   const DateRange& dateRange,
                                   const Num& pvalThreshold) override
            {
                validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true, true);
            }
            std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
            {
                std::vector<std::shared_ptr<PalStrategy<Num>>> s;
                for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
                    s.push_back(*it);
                return s;
            }
            int getNumSurvivingStrategies() const override
            {
                return validation.getNumSurvivingStrategies();
            }
            const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
            {
                throw std::runtime_error("Not supported for RomanoWolf");
            }
            std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
            {
                return validation.getAllTestedStrategies();
            }
            Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
            {
                return validation.getStrategyPValue(s);
            }
        };
        
        auto validation = std::make_unique<RomanoWolfValidationWrapper>(params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::RomanoWolf);
    };

    switch (computationPolicy)
    {
        case ComputationPolicy::RobustProfitFactor:
            createAndRun.template operator()<RobustProfitFactorPolicy>();
            break;
        case ComputationPolicy::AllHighResLogPF:
            createAndRun.template operator()<AllHighResLogPFPolicy>();
            break;
        case ComputationPolicy::GatedPerformanceScaledPal:
            createAndRun.template operator()<GatedPerformanceScaledPalPolicy>();
            break;
    }
}


// Orchestrator for Benjamini-Hochberg Validation
void runValidationForBenjaminiHochberg(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                       const ValidationParameters& params,
                                       ComputationPolicy computationPolicy)
{
    std::cout << "\nUsing Benjamini-Hochberg validation with " << getComputationPolicyString(computationPolicy)
              << " and " << params.permutations << " permutations." << std::endl;
    
    // NOTE: This shows how the FDR parameter from `params` is now available in the correct context.
    std::cout << "[INFO] False Discovery Rate (FDR) set to: " << params.falseDiscoveryRate << std::endl;
    std::cout << "[WARNING] The underlying PALMonteCarloValidation class does not currently allow passing this FDR." << std::endl;
    std::cout << "          To enable this, the library itself would need modification." << std::endl;


    auto createAndRun = [&]<template<typename> class BacktestingStatPol>()
    {
        // Define policy-specific types here
        // Define the Monte Carlo Permutation Test type with correct template parameters
        using BenjaminiMcpt = MonteCarloPermuteMarketChanges<
            Num,
            BacktestingStatPol,
            DefaultPermuteMarketChangesPolicy<Num, BacktestingStatPol<Num>>
        >;
        
        class BenjaminiHochbergValidationWrapper : public ValidationInterface
        {
        private:
            PALMonteCarloValidation<Num, BenjaminiMcpt, AdaptiveBenjaminiHochbergYr2000> validation;
        public:
            explicit BenjaminiHochbergValidationWrapper(unsigned long p, const Num& fdr)
                : validation(p, fdr)
            {
            }
            void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                   std::shared_ptr<PriceActionLabSystem> patterns,
                                   const DateRange& dateRange,
                                   const Num& pvalThreshold) override
            {
                validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, true);
            }
            std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
            {
                std::vector<std::shared_ptr<PalStrategy<Num>>> s;
                for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
                    s.push_back(*it);
                return s;
            }
            int getNumSurvivingStrategies() const override
            {
                return validation.getNumSurvivingStrategies();
            }
            const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
            {
                return validation.getStatisticsCollector();
            }
            std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
            {
                return validation.getAllTestedStrategies();
            }
            Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
            {
                return validation.getStrategyPValue(s);
            }
        };

        auto validation = std::make_unique<BenjaminiHochbergValidationWrapper>(
            params.permutations, params.falseDiscoveryRate);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::BenjaminiHochberg);
    };

    switch (computationPolicy)
    {
        case ComputationPolicy::RobustProfitFactor:
            createAndRun.template operator()<RobustProfitFactorPolicy>();
            break;
        case ComputationPolicy::AllHighResLogPF:
            createAndRun.template operator()<AllHighResLogPFPolicy>();
            break;
        case ComputationPolicy::GatedPerformanceScaledPal:
            createAndRun.template operator()<GatedPerformanceScaledPalPolicy>();
            break;
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
    
    // -- Get parameters interactively --
    ValidationParameters params;
    std::string input;

    std::cout << "Enter number of permutations (default: 5000): ";
    std::getline(std::cin, input);
    params.permutations = input.empty() ? 5000 : std::stoul(input);

    std::cout << "Enter p-value threshold (default: 0.05): ";
    std::getline(std::cin, input);
    params.pValueThreshold = input.empty() ? Num(0.05) : Num(std::stod(input));
    
    // Ask for Validation Method
    std::cout << "\nChoose validation method:" << std::endl;
    std::cout << "  1. Masters (default)" << std::endl;
    std::cout << "  2. Romano-Wolf" << std::endl;
    std::cout << "  3. Benjamini-Hochberg" << std::endl;
    std::cout << "Enter choice (1, 2, or 3): ";
    std::getline(std::cin, input);
    
    ValidationMethod validationMethod = ValidationMethod::Masters;
    if (input == "2") {
        validationMethod = ValidationMethod::RomanoWolf;
    } else if (input == "3") {
        validationMethod = ValidationMethod::BenjaminiHochberg;
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

    // Ask for Computation Policy
    std::cout << "\nChoose computation policy:" << std::endl;
    std::cout << "  1. GatedPerformanceScaledPalPolicy (default, recommended)" << std::endl;
    std::cout << "  2. RobustProfitFactorPolicy" << std::endl;
    std::cout << "  3. AllHighResLogPFPolicy" << std::endl;
    std::cout << "Enter choice (1, 2, or 3): ";
    std::getline(std::cin, input);
    
    ComputationPolicy computationPolicy = ComputationPolicy::GatedPerformanceScaledPal;
    if (input == "2") {
        computationPolicy = ComputationPolicy::RobustProfitFactor;
    } else if (input == "3") {
        computationPolicy = ComputationPolicy::AllHighResLogPF;
    }

    // -- Configuration File Reading --
    const auto configurationFileName = std::string(argv[1]);
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
    
    // -- Summary --
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    std::cout << "Security Ticker: " << config->getSecurity()->getSymbol() << std::endl;
    std::cout << "In-Sample Range: " << config->getInsampleDateRange().getFirstDateTime()
              << " to " << config->getInsampleDateRange().getLastDateTime() << std::endl;
    std::cout << "=============================" << std::endl;

    // -- Top-level dispatch based on the VALIDATION METHOD --
    switch (validationMethod)
    {
        case ValidationMethod::Masters:
            runValidationForMasters(config, params, computationPolicy);
            break;
        case ValidationMethod::RomanoWolf:
            runValidationForRomanoWolf(config, params, computationPolicy);
            break;
        case ValidationMethod::BenjaminiHochberg:
            runValidationForBenjaminiHochberg(config, params, computationPolicy);
            break;
    }
    
    return 0;
}
