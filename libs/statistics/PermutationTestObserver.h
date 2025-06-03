// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __PERMUTATION_TEST_OBSERVER_H
#define __PERMUTATION_TEST_OBSERVER_H 1

#include <optional>

namespace mkc_timeseries
{
    // Forward declarations
    template <class Decimal> class BackTester;
    template <class Decimal> class PalStrategy;

    /**
     * @class PermutationTestObserver
     * @brief Observer interface for collecting granular permutation test statistics
     * 
     * This interface defines the contract for observers that collect detailed statistics
     * from permutation testing runs. Observers receive notifications when permutation
     * backtests complete and can extract strategy-specific metrics for analysis.
     * 
     * Key Features:
     * - Strategy-specific statistics collection using strategy pointers
     * - Extensible metric types via enum (easy to add new metrics)
     * - Thread-safe implementations expected for concurrent permutation testing
     * - Boost.Accumulators integration for efficient statistics computation
     * 
     * @tparam Decimal Numeric type for calculations (e.g., double)
     */
    template <class Decimal>
    class PermutationTestObserver {
    public:
        virtual ~PermutationTestObserver() = default;

        /**
         * @brief Called by subjects when a permutation backtest completes
         * @param permutedBacktester The BackTester instance after running on synthetic data
         * @param permutedTestStatistic The performance statistic from this permutation
         */
        virtual void update(
            const BackTester<Decimal>& permutedBacktester,
            const Decimal& permutedTestStatistic
        ) = 0;

        /**
         * @brief Extensible statistics retrieval using enum for metric types
         * 
         * This enum allows adding new metrics without changing the observer interface.
         * Each metric represents a different aspect of strategy performance that can
         * be collected during permutation testing.
         */
        enum class MetricType {
            PERMUTED_TEST_STATISTIC,  ///< The main performance statistic (e.g., Sharpe ratio)
            NUM_TRADES,               ///< Total number of trades (closed + open)
            NUM_BARS_IN_TRADES        ///< Total bars across all trades (closed + open)
            // Future metrics can be added here without interface changes
        };

        // Strategy identification handled by observer implementation using strategy pointer
        virtual std::optional<Decimal> getMinMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const = 0;
        virtual std::optional<Decimal> getMaxMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const = 0;
        virtual std::optional<double> getMedianMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const = 0;
        virtual std::optional<double> getStdDevMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const = 0;
        
        virtual void clear() = 0;
    };
}

#endif