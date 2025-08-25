#pragma once

#include "number.h"
#include "utils/ValidationTypes.h"
#include "analysis/StatisticalTypes.h"

namespace palvalidator
{
namespace filtering
{

using Num = num::DefaultNumber;
using RiskParameters = palvalidator::utils::RiskParameters;
using RobustnessChecksConfig = palvalidator::analysis::RobustnessChecksConfig<Num>;
using FragileEdgePolicy = palvalidator::analysis::FragileEdgePolicy;

/**
 * @brief Summary statistics for performance filtering results
 */
class FilteringSummary
{
public:
    /**
     * @brief Default constructor initializing all counters to zero
     */
    FilteringSummary();

    /**
     * @brief Get the number of strategies with insufficient sample size
     * @return Number of strategies filtered due to insufficient returns
     */
    size_t getInsufficientCount() const
    {
        return mInsufficientCount;
    }

    /**
     * @brief Get the number of strategies flagged for divergence
     * @return Number of strategies flagged for AM vs GM divergence
     */
    size_t getFlaggedCount() const
    {
        return mFlaggedCount;
    }

    /**
     * @brief Get the number of flagged strategies that passed robustness checks
     * @return Number of flagged strategies that passed robustness
     */
    size_t getFlagPassCount() const
    {
        return mFlagPassCount;
    }

    /**
     * @brief Get the number of strategies that failed L-bound checks
     * @return Number of strategies that failed L-sensitivity bound checks
     */
    size_t getFailLBoundCount() const
    {
        return mFailLBoundCount;
    }

    /**
     * @brief Get the number of strategies that failed L-variability checks
     * @return Number of strategies that failed L-sensitivity variability checks
     */
    size_t getFailLVarCount() const
    {
        return mFailLVarCount;
    }

    /**
     * @brief Get the number of strategies that failed split-sample checks
     * @return Number of strategies that failed split-sample tests
     */
    size_t getFailSplitCount() const
    {
        return mFailSplitCount;
    }

    /**
     * @brief Get the number of strategies that failed tail-risk checks
     * @return Number of strategies that failed tail-risk tests
     */
    size_t getFailTailCount() const
    {
        return mFailTailCount;
    }

    /**
     * @brief Increment the insufficient sample count
     */
    void incrementInsufficientCount()
    {
        ++mInsufficientCount;
    }

    /**
     * @brief Increment the flagged count
     */
    void incrementFlaggedCount()
    {
        ++mFlaggedCount;
    }

    /**
     * @brief Increment the flag pass count
     */
    void incrementFlagPassCount()
    {
        ++mFlagPassCount;
    }

    /**
     * @brief Increment the L-bound failure count
     */
    void incrementFailLBoundCount()
    {
        ++mFailLBoundCount;
    }

    /**
     * @brief Increment the L-variability failure count
     */
    void incrementFailLVarCount()
    {
        ++mFailLVarCount;
    }

    /**
     * @brief Increment the split-sample failure count
     */
    void incrementFailSplitCount()
    {
        ++mFailSplitCount;
    }

    /**
     * @brief Increment the tail-risk failure count
     */
    void incrementFailTailCount()
    {
        ++mFailTailCount;
    }

private:
    size_t mInsufficientCount;  ///< Number of strategies with insufficient sample size
    size_t mFlaggedCount;       ///< Number of strategies flagged for divergence
    size_t mFlagPassCount;      ///< Number of flagged strategies that passed robustness
    size_t mFailLBoundCount;    ///< Number of strategies that failed L-bound checks
    size_t mFailLVarCount;      ///< Number of strategies that failed L-variability checks
    size_t mFailSplitCount;     ///< Number of strategies that failed split-sample checks
    size_t mFailTailCount;      ///< Number of strategies that failed tail-risk checks
};

} // namespace filtering
} // namespace palvalidator