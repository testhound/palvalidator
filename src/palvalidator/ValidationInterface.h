#pragma once

#include <memory>
#include <vector>
#include "Security.h"
#include "PalAst.h"
#include "DateRange.h"
#include "PalStrategy.h"
#include "PermutationStatisticsCollector.h"
#include "number.h"

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

/**
 * @brief Abstract interface for validation implementations
 * 
 * This interface provides a common abstraction for different validation methods
 * (Masters, Romano-Wolf, Benjamini-Hochberg) while allowing them to work with
 * different computation policies through the factory pattern.
 */
class ValidationInterface
{
public:
    virtual ~ValidationInterface() = default;
    
    /**
     * @brief Run permutation tests on the given security and patterns
     *
     * @param baseSecurity The security to test against
     * @param patterns The price action lab patterns to validate
     * @param dateRange The date range for out-of-sample testing
     * @param pvalThreshold The p-value threshold for significance testing
     * @param verbose Enable verbose logging
     * @param partitionByFamily Whether to partition patterns by detailed family (Masters/Romano-Wolf only)
     */
    virtual void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                     std::shared_ptr<PriceActionLabSystem> patterns,
                                     const DateRange& dateRange,
                                     const Num& pvalThreshold,
                                     bool verbose = false,
                                     bool partitionByFamily = false) = 0;
    
    /**
     * @brief Get strategies that survived the validation process
     * 
     * @return Vector of strategies that passed the statistical tests
     */
    virtual std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const = 0;
    
    /**
     * @brief Get the number of strategies that survived validation
     * 
     * @return Number of surviving strategies
     */
    virtual int getNumSurvivingStrategies() const = 0;
    
    /**
     * @brief Get the statistics collector for detailed analysis
     * 
     * @return Reference to the statistics collector
     * @note Not all validation methods support this (e.g., Romano-Wolf may throw)
     */
    virtual const PermutationStatisticsCollector<Num>& getStatisticsCollector() const = 0;
    
    /**
     * @brief Get all tested strategies with their p-values
     * 
     * @return Vector of strategy-pvalue pairs for all tested strategies
     */
    virtual std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const = 0;
    
    /**
     * @brief Get the p-value for a specific strategy
     * 
     * @param strategy The strategy to get the p-value for
     * @return The p-value for the given strategy
     */
    virtual Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> strategy) const = 0;
};
