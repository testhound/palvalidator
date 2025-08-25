#pragma once

#include <vector>
#include <memory>
#include <string>
#include "number.h"
#include "Security.h"
#include "DateRange.h"
#include "PalStrategy.h"
#include "TimeFrame.h"
#include "ValidationInterface.h"
#include "utils/ValidationTypes.h"

namespace palvalidator
{
namespace reporting
{

using namespace mkc_timeseries;
using Num = num::DefaultNumber;
using ValidationMethod = palvalidator::utils::ValidationMethod;
using ValidationParameters = palvalidator::utils::ValidationParameters;

/**
 * @brief Reporter for pattern analysis results including surviving and rejected patterns
 * 
 * This class provides comprehensive reporting functionality for:
 * - Basic surviving patterns files (pattern definitions only)
 * - Detailed surviving patterns reports (with backtest performance)
 * - Detailed rejected patterns reports (with rejection reasons and statistics)
 * - Performance-filtered patterns analysis
 */
class PatternReporter
{
public:
    /**
     * @brief Write basic surviving patterns file (pattern definitions only)
     * @param strategies Vector of surviving strategies
     * @param securitySymbol Symbol of the security tested
     * @param method Validation method used
     */
    static void writeSurvivingPatterns(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
        const std::string& securitySymbol,
        ValidationMethod method
    );

    /**
     * @brief Write detailed surviving patterns report with backtest performance
     * @param baseSecurity Security that was tested
     * @param method Validation method used
     * @param strategies Vector of surviving strategies
     * @param backtestingDates Date range used for backtesting
     * @param timeFrame Time frame used for analysis
     * @param policyName Name of the computation policy used
     * @param params Validation parameters used
     */
    static void writeDetailedSurvivingPatterns(
        std::shared_ptr<Security<Num>> baseSecurity,
        ValidationMethod method,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        const std::string& policyName,
        const ValidationParameters& params
    );

    /**
     * @brief Write detailed rejected patterns report with rejection analysis
     * @param securitySymbol Symbol of the security tested
     * @param method Validation method used
     * @param validation Validation interface containing all test results
     * @param backtestingDates Date range used for backtesting
     * @param timeFrame Time frame used for analysis
     * @param pValueThreshold P-value threshold used for rejection
     * @param baseSecurity Security that was tested
     * @param performanceFilteredStrategies Optional vector of strategies filtered by performance
     */
    static void writeRejectedPatterns(
        const std::string& securitySymbol,
        ValidationMethod method,
        ValidationInterface* validation,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        const Num& pValueThreshold,
        std::shared_ptr<Security<Num>> baseSecurity,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies = {}
    );

private:
    /**
     * @brief Write validation summary header to file
     * @param file Output file stream
     * @param baseSecurity Security that was tested
     * @param method Validation method used
     * @param policyName Name of the computation policy used
     * @param backtestingDates Date range used for backtesting
     * @param params Validation parameters used
     * @param numStrategies Number of surviving strategies
     */
    static void writeValidationSummary(
        std::ofstream& file,
        std::shared_ptr<Security<Num>> baseSecurity,
        ValidationMethod method,
        const std::string& policyName,
        const DateRange& backtestingDates,
        const ValidationParameters& params,
        size_t numStrategies
    );

    /**
     * @brief Write rejected patterns header and statistics
     * @param file Output file stream
     * @param numRejected Number of rejected patterns
     * @param pValueThreshold P-value threshold used
     * @param method Validation method used
     */
    static void writeRejectedPatternsHeader(
        std::ofstream& file,
        size_t numRejected,
        const Num& pValueThreshold,
        ValidationMethod method
    );

    /**
     * @brief Write summary statistics for rejected patterns
     * @param file Output file stream
     * @param rejectedStrategiesWithPValues Vector of rejected strategies and their p-values
     * @param method Validation method used
     * @param pValueThreshold P-value threshold used
     */
    static void writeRejectedPatternsSummary(
        std::ofstream& file,
        const std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>>& rejectedStrategiesWithPValues,
        ValidationMethod method,
        const Num& pValueThreshold
    );

    /**
     * @brief Write performance-filtered patterns section
     * @param file Output file stream
     * @param performanceFilteredStrategies Vector of performance-filtered strategies
     * @param baseSecurity Security that was tested
     * @param backtestingDates Date range used for backtesting
     * @param timeFrame Time frame used for analysis
     */
    static void writePerformanceFilteredPatterns(
        std::ofstream& file,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame
    );

    /**
     * @brief Calculate theoretical PAL profitability for a strategy
     * @param strategy Strategy to analyze
     * @param targetProfitFactor Target profit factor (default 2.0)
     * @return Theoretical PAL profitability percentage
     */
    static Num calculateTheoreticalPALProfitability(
        std::shared_ptr<PalStrategy<Num>> strategy,
        Num targetProfitFactor = Num("2.0")
    );
};

} // namespace reporting
} // namespace palvalidator