// PerformanceCriteria.h
#ifndef PERFORMANCE_CRITERIA_H
#define PERFORMANCE_CRITERIA_H

#include <stdexcept> // For std::runtime_error
#include <string>    // For std::string
#include "number.h"  // For num::DefaultNumber
#include "DecimalConstants.h" // For DecimalConstants<DecimalType>::DecimalZero

// Prefer classes and encapsulation to structs
/**
 * @brief Exception class for errors related to PerformanceCriteria.
 */
class PerformanceCriteriaException : public std::runtime_error
{
public:
    /**
     * @brief Constructs a PerformanceCriteriaException with a descriptive message.
     * @param msg The error message.
     */
    explicit PerformanceCriteriaException(const std::string& msg)
        : std::runtime_error(msg) {}

    /**
     * @brief Destroys the PerformanceCriteriaException object.
     */
    ~PerformanceCriteriaException() noexcept override = default;
};

/**
 * @brief Represents the performance filtering criteria for discovered trading patterns.
 *
 * This class encapsulates the minimum performance thresholds that a trading pattern
 * must meet during backtesting to be considered "profitable" and worth saving.
 * It adheres to the coding standards by preferring classes and encapsulation,
 * avoiding public member variables, and providing public getter methods.
 *
 * @tparam DecimalType The decimal type used for financial calculations,
 * e.g., num::DefaultNumber.
 */
template <class DecimalType>
class PerformanceCriteria
{
public:
    /**
     * @brief Constructs a PerformanceCriteria object with specified thresholds.
     *
     * @param minProfitability The minimum required percentage of profitable trades (e.g., 80.0 for 80%).
     * @param minTrades The minimum required number of total trades for statistical significance.
     * @param maxConsecutiveLosers The maximum allowed number of consecutive losing trades.
     * @param minProfitFactor The minimum required Profit Factor (Gross Profit / Gross Loss).
     * @throws PerformanceCriteriaException if any of the input values are invalid
     * (e.g., negative profitability, min trades is zero, or profit factor <= 0).
     */
    explicit PerformanceCriteria(
        DecimalType minProfitability,
        unsigned int minTrades,
        unsigned int maxConsecutiveLosers,
        DecimalType minProfitFactor)
    // Allman style formatting: constructor definition on new line
    // When initializing private member variables in constructor initializer lists,
    // place each initialization on its own line for better readability
    : mMinProfitability(minProfitability),
      mMinTrades(minTrades),
      mMaxConsecutiveLosers(maxConsecutiveLosers),
      mMinProfitFactor(minProfitFactor)
    {
        // Prefer exceptions to asserts for validation
        // Use the decimal type in decimal.h instead of double due to financial calculations
        if (mMinProfitability < DecimalType(0) || mMinProfitability > DecimalType(100))
        {
            throw PerformanceCriteriaException("PerformanceCriteria: Minimum profitability must be between 0 and 100.");
        }
        if (mMinTrades == 0)
        {
            throw PerformanceCriteriaException("PerformanceCriteria: Minimum number of trades must be greater than 0.");
        }
        if (mMinProfitFactor <= mkc_timeseries::DecimalConstants<DecimalType>::DecimalZero) // Use DecimalConstants
        {
            throw PerformanceCriteriaException("PerformanceCriteria: Minimum profit factor must be greater than 0.");
        }
    }

    /**
     * @brief Destroys the PerformanceCriteria object.
     */
    ~PerformanceCriteria() noexcept = default;

    // No copy constructor or assignment operator explicitly defined,
    // as default ones are sufficient for simple member types.

    /**
     * @brief Gets the minimum required percentage of profitable trades.
     * @return The minimum profitability percentage.
     */
    DecimalType getMinProfitability() const
    {
        return mMinProfitability;
    }

    /**
     * @brief Gets the minimum required number of total trades.
     * @return The minimum number of trades.
     */
    unsigned int getMinTrades() const
    {
        return mMinTrades;
    }

    /**
     * @brief Gets the maximum allowed number of consecutive losing trades.
     * @return The maximum consecutive losers.
     */
    unsigned int getMaxConsecutiveLosers() const
    {
        return mMaxConsecutiveLosers;
    }

    /**
     * @brief Gets the minimum required Profit Factor.
     * @return The minimum Profit Factor.
     */
    DecimalType getMinProfitFactor() const
    {
        return mMinProfitFactor;
    }

private:
    // Private member variables, prefixed with 'm'
    DecimalType mMinProfitability;
    unsigned int mMinTrades;
    unsigned int mMaxConsecutiveLosers;
    DecimalType mMinProfitFactor;
};

// Explicit instantiation for common usage (still needed for templated classes in a .h if used in other .cpp files)
// For now, assuming num::DefaultNumber is the primary decimal type throughout the project.
template class PerformanceCriteria<num::DefaultNumber>;

#endif // PERFORMANCE_CRITERIA_H