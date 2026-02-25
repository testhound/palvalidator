#include "PatternReporter.h"
#include <fstream>
#include <set>
#include <algorithm>
#include <iomanip>
#include "LogPalPattern.h"
#include "BackTester.h"
#include "Portfolio.h"
#include "DecimalConstants.h"
#include "utils/TimeUtils.h"
#include "utils/OutputUtils.h"
#include "reporting/PerformanceReporter.h"

namespace palvalidator
{
namespace reporting
{

void PatternReporter::writeSurvivingPatterns(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
    const std::string& securitySymbol,
    ValidationMethod method,
    bool sameDayExits)
{
    std::string filename = palvalidator::utils::createSurvivingPatternsFileName(securitySymbol, method, sameDayExits);
    std::ofstream survivingPatternsFile(filename);
    
    for (const auto& strategy : strategies)
    {
        LogPalPattern::LogPattern(strategy->getPalPattern(), survivingPatternsFile);
    }
}

void PatternReporter::writeDetailedSurvivingPatterns(
    std::shared_ptr<Security<Num>> baseSecurity,
    ValidationMethod method,
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
    const DateRange& backtestingDates,
    TimeFrame::Duration timeFrame,
    const std::string& policyName,
    const ValidationParameters& params,
    bool sameDayExits)
{
    std::string filename = palvalidator::utils::createDetailedSurvivingPatternsFileName(
        baseSecurity->getSymbol(), method, sameDayExits);
    std::ofstream survivingPatternsFile(filename);
    
    writeValidationSummary(survivingPatternsFile, baseSecurity, method, policyName, 
                          backtestingDates, params, strategies.size());
    
    for (const auto& strategy : strategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat, timeFrame, backtestingDates);
            
            survivingPatternsFile << "Surviving Pattern:" << std::endl << std::endl;
            LogPalPattern::LogPattern(strategy->getPalPattern(), survivingPatternsFile);
            survivingPatternsFile << std::endl;
            PerformanceReporter::writeBacktestReport(survivingPatternsFile, backtester);
            survivingPatternsFile << std::endl << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception " << e.what() << std::endl;
            break;
        }
    }
}

void PatternReporter::writeRejectedPatterns(
    const std::string& securitySymbol,
    ValidationMethod method,
    ValidationInterface* validation,
    const DateRange& backtestingDates,
    TimeFrame::Duration timeFrame,
    const Num& pValueThreshold,
    std::shared_ptr<Security<Num>> baseSecurity,
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies,
    bool sameDayExits)
{
    std::string filename = palvalidator::utils::createDetailedRejectedPatternsFileName(securitySymbol, method, sameDayExits);
    std::ofstream rejectedPatternsFile(filename);
    
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
    
    writeRejectedPatternsHeader(rejectedPatternsFile, rejectedStrategiesWithPValues.size(), 
                               pValueThreshold, method);
    
    if (rejectedStrategiesWithPValues.empty())
    {
        rejectedPatternsFile << "No rejected patterns found." << std::endl;
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "All " << validation->getNumSurvivingStrategies()
                            << " tested patterns survived the validation process." << std::endl;
        rejectedPatternsFile << "This indicates very strong patterns or a lenient p-value threshold." << std::endl;
        
        // Write basic summary statistics even when no rejected patterns are found
        struct RejectionReasonStats
        {
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
        rejectedPatternsFile << "Rejected Pattern (p-value: " << pValue << "):" << std::endl;
        LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
        rejectedPatternsFile << "P-Value: " << pValue << std::endl;
        rejectedPatternsFile << "Threshold: " << pValueThreshold << std::endl;
        rejectedPatternsFile << "Reason: P-value exceeds threshold" << std::endl;
        rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
    }
    
    writeRejectedPatternsSummary(rejectedPatternsFile, rejectedStrategiesWithPValues, method, pValueThreshold);
    
    // Add performance-filtered strategies section
    if (!performanceFilteredStrategies.empty())
    {
        writePerformanceFilteredPatterns(rejectedPatternsFile, performanceFilteredStrategies, 
                                        baseSecurity, backtestingDates, timeFrame);
    }
}

void PatternReporter::writeValidationSummary(
    std::ofstream& file,
    std::shared_ptr<Security<Num>> baseSecurity,
    ValidationMethod method,
    const std::string& policyName,
    const DateRange& backtestingDates,
    const ValidationParameters& params,
    size_t numStrategies)
{
    file << "=== VALIDATION SUMMARY ===" << std::endl;
    file << "Security Ticker: " << baseSecurity->getSymbol() << std::endl;
    file << "Validation Method: " << palvalidator::utils::getValidationMethodString(method) << std::endl;
    file << "Computation Policy: " << policyName << std::endl;
    file << "Out-of-Sample Range: " << backtestingDates.getFirstDateTime()
         << " to " << backtestingDates.getLastDateTime() << std::endl;
    file << "Number of Permutations: " << params.permutations << std::endl;
    file << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (method == ValidationMethod::BenjaminiHochberg)
    {
        file << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    file << "Total Surviving Strategies (Performance Filtered): " << numStrategies << std::endl;
    file << "===========================" << std::endl << std::endl;
}

void PatternReporter::writeRejectedPatternsHeader(
    std::ofstream& file,
    size_t numRejected,
    const Num& pValueThreshold,
    ValidationMethod method)
{
    file << "=== REJECTED PATTERNS REPORT ===" << std::endl;
    file << "Total Rejected Patterns: " << numRejected << std::endl;
    file << "P-Value Threshold: " << pValueThreshold << std::endl;
    file << "Validation Method: " << palvalidator::utils::getValidationMethodString(method) << std::endl;
    file << "=================================" << std::endl << std::endl;
}

void PatternReporter::writeRejectedPatternsSummary(
    std::ofstream& file,
    const std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>>& rejectedStrategiesWithPValues,
    ValidationMethod method,
    const Num& pValueThreshold)
{
    file << std::endl << "=== Summary Statistics ===" << std::endl;
    file << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    file << "Validation Method: " << palvalidator::utils::getValidationMethodString(method) << std::endl;
    file << "P-Value Threshold: " << pValueThreshold << std::endl;
    
    if (!rejectedStrategiesWithPValues.empty())
    {
        auto minPValue = std::min_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        auto maxPValue = std::max_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        file << "Min P-Value: " << minPValue << std::endl;
        file << "Max P-Value: " << maxPValue << std::endl;
    }
}

void PatternReporter::writePerformanceFilteredPatterns(
    std::ofstream& file,
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration timeFrame)
{
    file << std::endl << std::endl;
    file << "=== PERFORMANCE-FILTERED PATTERNS ===" << std::endl;
    file << "These patterns survived Monte Carlo validation but were filtered out due to insufficient backtesting performance." << std::endl;
    file << "Total Performance-Filtered Patterns: " << performanceFilteredStrategies.size() << std::endl;
    file << "Filtering Criteria: Profit Factor >= 1.75 AND PAL Profitability >= 85% of theoretical" << std::endl;
    file << "=======================================" << std::endl << std::endl;
    
    for (const auto& strategy : performanceFilteredStrategies)
    {
        try
        {
            // Create fresh portfolio and clone strategy for backtesting
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            
            // Run backtest to get performance metrics for reporting
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat, timeFrame, backtestingDates);
            
            // Extract performance metrics
            auto positionHistory = backtester->getClosedPositionHistory();
            Num profitFactor = positionHistory.getProfitFactor();
            Num actualPALProfitability = positionHistory.getPALProfitability();
            
            // Calculate theoretical PAL profitability
            Num theoreticalPALProfitability = calculateTheoreticalPALProfitability(strategy);
            
            // Write pattern details
            file << "Performance-Filtered Pattern:" << std::endl;
            LogPalPattern::LogPattern(strategy->getPalPattern(), file);
            file << std::endl;
            
            // Write performance metrics that caused rejection
            file << "=== Performance Metrics ===" << std::endl;
            file << "Profit Factor: " << profitFactor << " (Required: >= 1.75)" << std::endl;
            file << "PAL Profitability: " << actualPALProfitability << "%" << std::endl;
            file << "Theoretical PAL Profitability: " << theoreticalPALProfitability << "%" << std::endl;
            
            if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero)
            {
                Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                file << "PAL Ratio: " << (palRatio * DecimalConstants<Num>::DecimalOneHundred) << "% (Required: >= 85%)" << std::endl;
            }
            
            file << "Reason: ";
            bool profitFactorFailed = profitFactor < DecimalConstants<Num>::DecimalOnePointSevenFive;
            bool palProfitabilityFailed = false;
            
            if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero)
            {
                Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                Num eightyFivePercent = DecimalConstants<Num>::createDecimal("0.85");
                palProfitabilityFailed = palRatio < eightyFivePercent;
            }
            
            if (profitFactorFailed && palProfitabilityFailed)
            {
                file << "Both Profit Factor and PAL Profitability criteria failed";
            }
            else if (profitFactorFailed)
            {
                file << "Profit Factor below threshold";
            }
            else if (palProfitabilityFailed)
            {
                file << "PAL Profitability below 85% of theoretical";
            }
            
            file << std::endl << std::endl << "---" << std::endl << std::endl;
            
        }
        catch (const std::exception& e)
        {
            file << "Performance-Filtered Pattern (Error in analysis):" << std::endl;
            LogPalPattern::LogPattern(strategy->getPalPattern(), file);
            file << "Error: " << e.what() << std::endl;
            file << std::endl << "---" << std::endl << std::endl;
        }
    }
}

Num PatternReporter::calculateTheoreticalPALProfitability(
    std::shared_ptr<PalStrategy<Num>> strategy,
    Num targetProfitFactor)
{
    auto pattern = strategy->getPalPattern();
    Num target = pattern->getProfitTargetAsDecimal();
    Num stop = pattern->getStopLossAsDecimal();
    
    if (stop == DecimalConstants<Num>::DecimalZero)
    {
        return DecimalConstants<Num>::DecimalZero;
    }
    
    Num payoffRatio = target / stop;
    Num oneHundred = DecimalConstants<Num>::DecimalOneHundred;
    
    // Formula from BootStrappedProfitabilityPFPolicy::getPermutationTestStatistic
    Num expectedPALProfitability = (targetProfitFactor / (targetProfitFactor + payoffRatio)) * oneHundred;
    
    return expectedPALProfitability;
}

} // namespace reporting
} // namespace palvalidator
