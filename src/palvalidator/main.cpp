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

// New policy architecture includes
#include "PolicyRegistry.h"
#include "PolicyConfiguration.h"
#include "PolicyFactory.h"
#include "PolicySelector.h"
#include "PolicyRegistration.h"
#include "ValidationInterface.h"

using namespace mkc_timeseries;

using Num = num::DefaultNumber;

// ---- Enums, Structs, and Helper Functions ----

enum class ValidationMethod
{
    Masters,
    RomanoWolf,
    BenjaminiHochberg
};

// ComputationPolicy enum removed - now using dynamic policy selection

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

// getComputationPolicyString function removed - now using dynamic policy names

static std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M");
    return ss.str();
}

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol, ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

static std::string createDetailedSurvivingPatternsFileName (const std::string& securitySymbol,
                                                            ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

static std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                          ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_RejectedPatterns_" + getCurrentTimestamp() + ".txt";
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
    file << "High Resolution Profit Factor: " << positionHistory.getHighResProfitFactor() << std::endl;
    file << "PAL Profitability: " << positionHistory.getPALProfitability() << "%" << std::endl;
    file << "High Resolution Profitability: " << positionHistory.getHighResProfitability() << std::endl;
    file << "===================================" << std::endl << std::endl;
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

// Filter surviving strategies based on backtesting performance criteria
template<typename Num>
std::vector<std::shared_ptr<PalStrategy<Num>>> filterSurvivingStrategiesByPerformance(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame)
{
    std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;
    
    std::cout << "Filtering " << survivingStrategies.size() << " surviving strategies by performance criteria..." << std::endl;
    std::cout << "Criteria: Profit Factor >= 1.75, PAL Profitability >= 85% of theoretical" << std::endl;
    
    for (const auto& strategy : survivingStrategies)
    {
        try
        {
            // Create fresh portfolio and clone strategy for backtesting
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            
            // Run backtest
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                       theTimeFrame,
                                                                       backtestingDates);
            
            // Extract performance metrics
            auto positionHistory = backtester->getClosedPositionHistory();
            Num profitFactor = positionHistory.getProfitFactor();
            Num actualPALProfitability = positionHistory.getPALProfitability();
            
            // Calculate theoretical PAL profitability
            Num theoreticalPALProfitability = calculateTheoreticalPALProfitability(strategy);
            
            // Apply filtering criteria
            bool profitFactorMet = profitFactor >= DecimalConstants<Num>::DecimalOnePointSevenFive;
            bool palProfitabilityMet = false;
            
            if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                Num eightyFivePercent = DecimalConstants<Num>::createDecimal("0.85");
                palProfitabilityMet = palRatio >= eightyFivePercent; // 85%
            }
            
            if (profitFactorMet && palProfitabilityMet) {
                filteredStrategies.push_back(strategy);
                std::cout << "✓ Strategy passed: PF=" << profitFactor
                         << ", PAL=" << actualPALProfitability << "% (vs "
                         << theoreticalPALProfitability << "% theoretical)" << std::endl;
            } else {
                std::cout << "✗ Strategy filtered out: PF=" << profitFactor
                         << " (req: >=1.75), PAL=" << actualPALProfitability << "% vs "
                         << theoreticalPALProfitability << "% theoretical (req: >=85%)" << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "Warning: Failed to evaluate strategy performance: " << e.what() << std::endl;
            std::cout << "Excluding strategy from filtered results." << std::endl;
        }
    }
    
    std::cout << "Filtering complete: " << filteredStrategies.size() << "/"
              << survivingStrategies.size() << " strategies passed criteria." << std::endl;
    
    return filteredStrategies;
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

// Overloaded version that takes a filtered strategies list directly with validation summary
void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                         ValidationMethod method,
                                         const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                                         const DateRange& backtestingDates,
                                         TimeFrame::Duration theTimeFrame,
                                         const std::string& policyName,
                                         const ValidationParameters& params)
{
    std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(baseSecurity->getSymbol(),
                                                                                 method));
    std::ofstream survivingPatternsFile(detailedPatternsFileName);
    
    // Write validation summary header
    survivingPatternsFile << "=== VALIDATION SUMMARY ===" << std::endl;
    survivingPatternsFile << "Security Ticker: " << baseSecurity->getSymbol() << std::endl;
    survivingPatternsFile << "Validation Method: " << getValidationMethodString(method) << std::endl;
    survivingPatternsFile << "Computation Policy: " << policyName << std::endl;
    survivingPatternsFile << "Out-of-Sample Range: " << backtestingDates.getFirstDateTime()
                          << " to " << backtestingDates.getLastDateTime() << std::endl;
    survivingPatternsFile << "Number of Permutations: " << params.permutations << std::endl;
    survivingPatternsFile << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (method == ValidationMethod::BenjaminiHochberg) {
        survivingPatternsFile << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    survivingPatternsFile << "Total Surviving Strategies (Performance Filtered): " << strategies.size() << std::endl;
    survivingPatternsFile << "===========================" << std::endl << std::endl;
    
    for (const auto& strategy : strategies)
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
                                       std::shared_ptr<Security<Num>> baseSecurity,
                                       const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies = {})
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
    
    // Add performance-filtered strategies section
    if (!performanceFilteredStrategies.empty()) {
        rejectedPatternsFile << std::endl << std::endl;
        rejectedPatternsFile << "=== PERFORMANCE-FILTERED PATTERNS ===" << std::endl;
        rejectedPatternsFile << "These patterns survived Monte Carlo validation but were filtered out due to insufficient backtesting performance." << std::endl;
        rejectedPatternsFile << "Total Performance-Filtered Patterns: " << performanceFilteredStrategies.size() << std::endl;
        rejectedPatternsFile << "Filtering Criteria: Profit Factor >= 1.75 AND PAL Profitability >= 85% of theoretical" << std::endl;
        rejectedPatternsFile << "=======================================" << std::endl << std::endl;
        
        for (const auto& strategy : performanceFilteredStrategies) {
            try {
                // Create fresh portfolio and clone strategy for backtesting
                auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
                freshPortfolio->addSecurity(baseSecurity);
                auto clonedStrat = strategy->clone2(freshPortfolio);
                
                // Run backtest to get performance metrics for reporting
                auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                           theTimeFrame,
                                                                           backtestingDates);
                
                // Extract performance metrics
                auto positionHistory = backtester->getClosedPositionHistory();
                Num profitFactor = positionHistory.getProfitFactor();
                Num actualPALProfitability = positionHistory.getPALProfitability();
                
                // Calculate theoretical PAL profitability
                Num theoreticalPALProfitability = calculateTheoreticalPALProfitability(strategy);
                
                // Write pattern details
                rejectedPatternsFile << "Performance-Filtered Pattern:" << std::endl;
                LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
                rejectedPatternsFile << std::endl;
                
                // Write performance metrics that caused rejection
                rejectedPatternsFile << "=== Performance Metrics ===" << std::endl;
                rejectedPatternsFile << "Profit Factor: " << profitFactor << " (Required: >= 1.75)" << std::endl;
                rejectedPatternsFile << "PAL Profitability: " << actualPALProfitability << "%" << std::endl;
                rejectedPatternsFile << "Theoretical PAL Profitability: " << theoreticalPALProfitability << "%" << std::endl;
                
                if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                    Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                    rejectedPatternsFile << "PAL Ratio: " << (palRatio * DecimalConstants<Num>::DecimalOneHundred) << "% (Required: >= 85%)" << std::endl;
                }
                
                rejectedPatternsFile << "Reason: ";
                bool profitFactorFailed = profitFactor < DecimalConstants<Num>::DecimalOnePointSevenFive;
                bool palProfitabilityFailed = false;
                
                if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                    Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                    Num eightyFivePercent = DecimalConstants<Num>::createDecimal("0.85");
                    palProfitabilityFailed = palRatio < eightyFivePercent;
                }
                
                if (profitFactorFailed && palProfitabilityFailed) {
                    rejectedPatternsFile << "Both Profit Factor and PAL Profitability criteria failed";
                } else if (profitFactorFailed) {
                    rejectedPatternsFile << "Profit Factor below threshold";
                } else if (palProfitabilityFailed) {
                    rejectedPatternsFile << "PAL Profitability below 85% of theoretical";
                }
                
                rejectedPatternsFile << std::endl << std::endl << "---" << std::endl << std::endl;
                
            } catch (const std::exception& e) {
                rejectedPatternsFile << "Performance-Filtered Pattern (Error in analysis):" << std::endl;
                LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
                rejectedPatternsFile << "Error: " << e.what() << std::endl;
                rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
            }
        }
    }
}



// ---- Core Logic ----

// This is the common worker function that runs the validation and prints results.
// It is called by the higher-level functions AFTER the validation object has been created.
void runValidationWorker(std::unique_ptr<ValidationInterface> validation,
                         std::shared_ptr<ValidatorConfiguration<Num>> config,
                         const ValidationParameters& params,
                         ValidationMethod validationMethod,
                         const std::string& policyName,
                         bool partitionByFamily = false)
{
    std::cout << "Starting Monte Carlo validation...\n" << std::endl;

    validation->runPermutationTests(config->getSecurity(),
        config->getPricePatterns(),
        config->getOosDateRange(),
        params.pValueThreshold,
        true, // Enable verbose logging by default
        partitionByFamily); // Pass the partitioning preference

    std::cout << "\nMonte Carlo validation completed." << std::endl;
    std::cout << "Number of surviving strategies = " << validation->getNumSurvivingStrategies() << std::endl;

    // -- Output --
    std::vector<std::shared_ptr<PalStrategy<Num>>> performanceFilteredStrategies;
    
    if (validation->getNumSurvivingStrategies() > 0)
    {
        std::string fn = createSurvivingPatternsFileName(config->getSecurity()->getSymbol(), validationMethod);
        std::ofstream survivingPatternsFile(fn);
        std::cout << "Writing surviving patterns to file: " << fn << std::endl;
        
        auto survivingStrategies = validation->getSurvivingStrategies();
        
        // Apply performance-based filtering to surviving strategies
        std::cout << "\nApplying performance-based filtering to surviving strategies..." << std::endl;
        auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
        auto filteredStrategies = filterSurvivingStrategiesByPerformance<Num>(
            survivingStrategies,
            config->getSecurity(),
            config->getOosDateRange(),
            timeFrame
        );
        
        // Identify strategies that were filtered out due to performance criteria
        std::set<std::shared_ptr<PalStrategy<Num>>> filteredSet(filteredStrategies.begin(), filteredStrategies.end());
        for (const auto& strategy : survivingStrategies) {
            if (filteredSet.find(strategy) == filteredSet.end()) {
                performanceFilteredStrategies.push_back(strategy);
            }
        }
        
        std::cout << "Performance filtering results: " << filteredStrategies.size() << " passed, "
                  << performanceFilteredStrategies.size() << " filtered out" << std::endl;
        
        // Write the original surviving patterns (before filtering) to the basic file
        for (const auto& strategy : survivingStrategies)
        {
            LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
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

    std::cout << "Writing detailed rejected patterns report..." << std::endl;
    auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
    writeDetailedRejectedPatternsFile(config->getSecurity()->getSymbol(), validationMethod, validation.get(),
                                      config->getOosDateRange(), timeFrame, params.pValueThreshold,
                                      config->getSecurity(), performanceFilteredStrategies);
    
    std::cout << "Validation run finished." << std::endl;
}

// ---- Validation Method Specific Orchestrators ----

// Orchestrator for Masters Validation
void runValidationForMasters(std::shared_ptr<ValidatorConfiguration<Num>> config,
                             const ValidationParameters& params,
                             const std::string& policyName,
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
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Masters, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Masters validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Romano-Wolf Validation
void runValidationForRomanoWolf(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
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
        runValidationWorker(std::move(validation), config, params, ValidationMethod::RomanoWolf, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Romano-Wolf validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Benjamini-Hochberg Validation
void runValidationForBenjaminiHochberg(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                       const ValidationParameters& params,
                                       const std::string& policyName)
{
    std::cout << "\nUsing Benjamini-Hochberg validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    std::cout << "[INFO] False Discovery Rate (FDR) set to: " << params.falseDiscoveryRate << std::endl;

    try {
        auto validation = statistics::PolicyFactory::createBenjaminiHochbergValidation(
            policyName, params.permutations, params.falseDiscoveryRate.getAsDouble());
        runValidationWorker(std::move(validation), config, params, ValidationMethod::BenjaminiHochberg, policyName);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Benjamini-Hochberg validation with policy '" << policyName << "': " << e.what() << std::endl;
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
    
    // -- Get parameters interactively --
    ValidationParameters params;
    std::string input;

    std::cout << "\nEnter number of permutations (default: 5000): ";
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
    
    // Ask about pattern partitioning for Masters and Romano-Wolf methods
    bool partitionByFamily = false;
    if (validationMethod == ValidationMethod::Masters || validationMethod == ValidationMethod::RomanoWolf) {
        std::cout << "\nPattern Partitioning Options:" << std::endl;
        std::cout << "  1. By Direction Only (Long vs Short) - Default" << std::endl;
        std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
        std::cout << "Choose partitioning method (1 or 2): ";
        std::getline(std::cin, input);
        
        if (input == "2") {
            partitionByFamily = true;
            std::cout << "Selected: Detailed family partitioning" << std::endl;
        } else {
            std::cout << "Selected: Direction-only partitioning (default)" << std::endl;
        }
    }
    
    // Interactive policy selection using the new system
    std::cout << "\n=== Policy Selection ===" << std::endl;
    auto availablePolicies = palvalidator::PolicyRegistry::getAvailablePolicies();
    std::cout << "Available policies: " << availablePolicies.size() << std::endl;
    
    std::string selectedPolicy;
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
    std::cout << "Validation Method: " << getValidationMethodString(validationMethod) << std::endl;
    std::cout << "Computation Policy: " << selectedPolicy << std::endl;
    if (validationMethod == ValidationMethod::Masters || validationMethod == ValidationMethod::RomanoWolf) {
        std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "By Direction Only") << std::endl;
    }
    std::cout << "Permutations: " << params.permutations << std::endl;
    std::cout << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    std::cout << "=============================" << std::endl;

    // -- Top-level dispatch based on the VALIDATION METHOD --
    try {
        switch (validationMethod)
        {
            case ValidationMethod::Masters:
                runValidationForMasters(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::RomanoWolf:
                runValidationForRomanoWolf(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::BenjaminiHochberg:
                runValidationForBenjaminiHochberg(config, params, selectedPolicy);
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Validation failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
