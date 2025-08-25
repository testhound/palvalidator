#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include "number.h"
#include "Security.h"
#include "DateRange.h"
#include "PalStrategy.h"
#include "TimeFrame.h"
#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"
#include "analysis/RobustnessAnalyzer.h"
#include "analysis/DivergenceAnalyzer.h"
#include "analysis/FragileEdgeAnalyzer.h"

namespace palvalidator
{
namespace filtering
{

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

/**
 * @brief Performance-based filtering of trading strategies using BCa bootstrap analysis
 * 
 * This class implements comprehensive performance filtering that includes:
 * - Statistical viability checks (annualized lower bound > 0)
 * - Economic significance tests (returns exceed cost hurdles)
 * - Risk-adjusted return requirements (returns exceed risk-free rate + premium)
 * - Robustness analysis for flagged strategies
 * - Fragile edge advisory analysis
 */
class PerformanceFilter
{
public:
    /**
     * @brief Constructor with risk parameters and bootstrap configuration
     * @param riskParams Risk parameters including risk-free rate and premium
     * @param confidenceLevel Confidence level for BCa bootstrap analysis (e.g., 0.95)
     * @param numResamples Number of bootstrap resamples (e.g., 2000)
     */
    PerformanceFilter(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples);

    /**
     * @brief Filter strategies based on BCa bootstrap performance analysis
     * @param survivingStrategies Vector of strategies that survived Monte Carlo validation
     * @param baseSecurity Security to test strategies against
     * @param backtestingDates Date range for backtesting
     * @param timeFrame Time frame for analysis
     * @param outputStream Output stream for logging (typically a TeeStream)
     * @return Vector of strategies that passed performance filtering
     */
    std::vector<std::shared_ptr<PalStrategy<Num>>> filterByPerformance(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream
    );

    /**
     * @brief Get the filtering summary from the last run
     * @return Summary statistics of the filtering process
     */
    const FilteringSummary& getFilteringSummary() const
    {
        return mFilteringSummary;
    }

private:
    /**
     * @brief Calculate the required return hurdle for a strategy
     * @param annualizedTrades Number of trades per year
     * @param costBasedRequiredReturn Cost-based required return
     * @param riskFreeHurdle Risk-free hurdle rate
     * @return The higher of cost-based or risk-free hurdle
     */
    Num calculateRequiredReturn(
        const Num& annualizedTrades,
        const Num& costBasedRequiredReturn,
        const Num& riskFreeHurdle
    ) const;

    /**
     * @brief Check if a strategy passes the basic hurdle requirements
     * @param annualizedLowerBoundGeo Annualized geometric mean lower bound
     * @param finalRequiredReturn Required return hurdle
     * @return True if strategy passes hurdle requirements
     */
    bool passesHurdleRequirements(
        const Num& annualizedLowerBoundGeo,
        const Num& finalRequiredReturn
    ) const;

    /**
     * @brief Process a strategy that requires robustness checking
     * @param strategy Strategy to analyze
     * @param strategyName Name of the strategy
     * @param highResReturns High-resolution returns for the strategy
     * @param L Block length for bootstrap
     * @param annualizationFactor Factor for annualizing returns
     * @param finalRequiredReturn Required return hurdle
     * @param divergence Divergence analysis result
     * @param nearHurdle Whether strategy is near the hurdle
     * @param smallN Whether strategy has small sample size
     * @param outputStream Output stream for logging
     * @return True if strategy passes robustness checks
     */
    bool processRobustnessChecks(
        std::shared_ptr<PalStrategy<Num>> strategy,
        const std::string& strategyName,
        const std::vector<Num>& highResReturns,
        size_t L,
        double annualizationFactor,
        const Num& finalRequiredReturn,
        const palvalidator::analysis::DivergenceResult<Num>& divergence,
        bool nearHurdle,
        bool smallN,
        std::ostream& outputStream
    );

    /**
     * @brief Process fragile edge advisory analysis
     * @param lbGeoPeriod Per-period geometric mean lower bound
     * @param annualizedLowerBoundGeo Annualized geometric mean lower bound
     * @param finalRequiredReturn Required return hurdle
     * @param lSensitivityRelVar Relative variability from L-sensitivity analysis
     * @param highResReturns High-resolution returns for tail analysis
     * @param outputStream Output stream for logging
     * @return True if strategy should be kept (not dropped by fragile edge policy)
     */
    bool processFragileEdgeAnalysis(
        const Num& lbGeoPeriod,
        const Num& annualizedLowerBoundGeo,
        const Num& finalRequiredReturn,
        double lSensitivityRelVar,
        const std::vector<Num>& highResReturns,
        std::ostream& outputStream
    );

    /**
     * @brief Count survivors by direction (Long/Short)
     * @param filteredStrategies Vector of filtered strategies
     * @return Pair of (Long count, Short count)
     */
    std::pair<size_t, size_t> countSurvivorsByDirection(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& filteredStrategies
    ) const;

private:
    TradingHurdleCalculator mHurdleCalculator;     ///< Calculator for trading hurdles
    Num mConfidenceLevel;                          ///< Confidence level for BCa bootstrap
    unsigned int mNumResamples;                    ///< Number of bootstrap resamples
    RobustnessChecksConfig mRobustnessConfig;      ///< Configuration for robustness checks
    FragileEdgePolicy mFragileEdgePolicy;          ///< Policy for fragile edge analysis
    FilteringSummary mFilteringSummary;            ///< Summary of filtering results
    bool mApplyFragileAdvice;                      ///< Whether to apply fragile edge advice
};

} // namespace filtering
} // namespace palvalidator