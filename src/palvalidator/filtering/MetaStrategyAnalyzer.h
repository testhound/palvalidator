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
#include "utils/ValidationTypes.h"

namespace palvalidator
{
namespace filtering
{

using namespace mkc_timeseries;
using Num = num::DefaultNumber;
using ValidationMethod = palvalidator::utils::ValidationMethod;

/**
 * @brief Analyzer for meta-strategy (portfolio) performance using equal-weight combination
 * 
 * This class implements meta-strategy analysis that:
 * - Combines multiple surviving strategies into an equal-weight portfolio
 * - Performs BCa bootstrap analysis on the portfolio returns
 * - Calculates portfolio-level cost hurdles and risk-adjusted returns
 * - Determines if the meta-strategy passes performance criteria
 */
class MetaStrategyAnalyzer
{
public:
    /**
     * @brief Constructor with risk parameters and bootstrap configuration
     * @param riskParams Risk parameters including risk-free rate and premium
     * @param confidenceLevel Confidence level for BCa bootstrap analysis (e.g., 0.95)
     * @param numResamples Number of bootstrap resamples (e.g., 2000)
     */
    MetaStrategyAnalyzer(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples, bool usePalMetaStrategy = false);

    /**
     * @brief Analyze meta-strategy performance using equal-weight portfolio
     * @param survivingStrategies Vector of strategies that survived individual filtering
     * @param baseSecurity Security to test strategies against
     * @param backtestingDates Date range for backtesting
     * @param timeFrame Time frame for analysis
     * @param outputStream Output stream for logging (typically a TeeStream)
     */
    void analyzeMetaStrategy(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream,
        ValidationMethod validationMethod = ValidationMethod::Unadjusted
    );

    /**
     * @brief Check if the last analyzed meta-strategy passed performance criteria
     * @return True if meta-strategy passed, false otherwise
     */
    bool didMetaStrategyPass() const
    {
        return mMetaStrategyPassed;
    }

    /**
     * @brief Get the annualized lower bound from the last meta-strategy analysis
     * @return Annualized geometric mean lower bound for the portfolio
     */
    const Num& getAnnualizedLowerBound() const
    {
        return mAnnualizedLowerBound;
    }

    /**
     * @brief Get the required return hurdle from the last meta-strategy analysis
     * @return Required return hurdle for the portfolio
     */
    const Num& getRequiredReturn() const
    {
        return mRequiredReturn;
    }

private:
    /**
     * @brief Gather per-strategy returns and trade statistics
     * @param strategies Vector of strategies to analyze
     * @param baseSecurity Security to test against
     * @param backtestingDates Date range for backtesting
     * @param timeFrame Time frame for analysis
     * @param outputStream Output stream for logging
     * @return Tuple of (returns vectors, annualized trades, median holds, min length)
     */
    std::tuple<std::vector<std::vector<Num>>, std::vector<Num>, std::vector<unsigned int>, size_t>
    gatherStrategyData(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream
    );

    /**
     * @brief Create equal-weight portfolio returns from individual strategy returns
     * @param survivorReturns Vector of return vectors for each strategy
     * @param minLength Minimum length to truncate all series to
     * @return Vector of portfolio returns
     */
    std::vector<Num> createEqualWeightPortfolio(
        const std::vector<std::vector<Num>>& survivorReturns,
        size_t minLength
    );

    /**
     * @brief Calculate median of median holding periods with round-half-up
     * @param survivorMedianHolds Vector of median holding periods
     * @return Median of medians, clamped to >= 2
     */
    size_t calculateMetaBlockLength(const std::vector<unsigned int>& survivorMedianHolds);

    /**
     * @brief Calculate portfolio-level annualized trades
     * @param survivorAnnualizedTrades Vector of annualized trades per strategy
     * @param numStrategies Number of strategies in portfolio
     * @return Portfolio annualized trades (equal-weighted)
     */
    Num calculatePortfolioAnnualizedTrades(
        const std::vector<Num>& survivorAnnualizedTrades,
        size_t numStrategies
    );

    /**
     * @brief Analyze meta-strategy using unified PalMetaStrategy approach
     * @param survivingStrategies Vector of strategies that survived individual filtering
     * @param baseSecurity Security to test strategies against
     * @param backtestingDates Date range for backtesting
     * @param timeFrame Time frame for analysis
     * @param outputStream Output stream for logging
     */
    void analyzeMetaStrategyUnified(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream,
        ValidationMethod validationMethod
    );

    /**
     * @brief Perform statistical analysis on meta-strategy returns
     * @param metaReturns Vector of portfolio returns
     * @param baseSecurity Security for annualization factor calculation
     * @param timeFrame Time frame for analysis
     * @param blockLength Block length for bootstrap resampling
     * @param annualizedTrades Annualized trades for cost hurdle calculation
     * @param strategyCount Number of strategies (for reporting)
     * @param strategyType Description of strategy type (for reporting)
     * @param outputStream Output stream for logging
     */
    void performStatisticalAnalysis(
        const std::vector<Num>& metaReturns,
        std::shared_ptr<Security<Num>> baseSecurity,
        TimeFrame::Duration timeFrame,
        size_t blockLength,
        const Num& annualizedTrades,
        size_t strategyCount,
        const std::string& strategyType,
        std::ostream& outputStream
    );

private:
    TradingHurdleCalculator mHurdleCalculator; ///< Calculator for trading hurdles
    Num mConfidenceLevel;                      ///< Confidence level for BCa bootstrap
    unsigned int mNumResamples;                ///< Number of bootstrap resamples
    bool mMetaStrategyPassed;                  ///< Result of last meta-strategy analysis
    Num mAnnualizedLowerBound;                 ///< Last calculated annualized lower bound
    Num mRequiredReturn;                       ///< Last calculated required return
    bool mUsePalMetaStrategy;                  ///< Flag to use PalMetaStrategy approach
};

} // namespace filtering
} // namespace palvalidator