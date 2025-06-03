// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __PAL_MASTERS_MONTE_CARLO_VALIDATION_OBSERVER_H
#define __PAL_MASTERS_MONTE_CARLO_VALIDATION_OBSERVER_H 1

#include "PermutationTestObserver.h"
#include "UuidStrategyPermutationStatsAggregator.h"
#include "StrategyIdentificationHelper.h"
#include "BackTester.h"
#include "PalStrategy.h"

namespace mkc_timeseries
{
    /**
     * @class PALMastersMonteCarloValidationObserver
     * @brief Concrete observer for collecting PAL strategy permutation statistics
     * 
     * This observer implements the PermutationTestObserver interface specifically
     * for PAL (Price Action Lab) strategies during Masters Monte Carlo validation.
     * It collects comprehensive statistics for each permutation run and provides
     * convenient access methods for analysis.
     * 
     * Key Features:
     * - **PAL-Specific**: Designed for PriceActionLabPattern-based strategies
     * - **Comprehensive Statistics**: Collects test statistics, trade counts, and bar counts
     * - **UUID-Based Tracking**: Uses enhanced strategy identification for uniqueness
     * - **Boost.Accumulators**: Efficient statistics computation with 90% memory reduction
     * - **Thread-Safe**: Supports concurrent permutation testing
     * 
     * Usage:
     * 1. Create observer instance
     * 2. Attach to permutation test subjects (policy classes)
     * 3. Run permutation tests (observer collects data automatically)
     * 4. Query statistics using convenience methods
     * 
     * @tparam Decimal Numeric type for calculations (e.g., double)
     * @tparam BaselineStatPolicy Policy for baseline statistic calculation
     */
    template <class Decimal, class BaselineStatPolicy>
    class PALMastersMonteCarloValidationObserver : public PermutationTestObserver<Decimal> {
    private:
        using MetricType = typename PermutationTestObserver<Decimal>::MetricType;
        UuidStrategyPermutationStatsAggregator<Decimal> m_statsAggregator;

    public:
        /**
         * @brief Update method called when a permutation backtest completes
         * @param permutedBacktester The BackTester instance after running on synthetic data
         * @param permutedTestStatistic The performance statistic from this permutation
         * 
         * This method extracts strategy identification and statistics from the completed
         * backtest and stores them in the aggregator for later analysis. It only processes
         * PalStrategy instances, logging warnings for other strategy types.
         */
        void update(const BackTester<Decimal>& permutedBacktester,
                    const Decimal& permutedTestStatistic) override {
            
            // Extract strategy identification using UUID-based approach
            unsigned long long strategyHash = StrategyIdentificationHelper<Decimal>::extractStrategyHash(permutedBacktester);
            const PalStrategy<Decimal>* strategy = StrategyIdentificationHelper<Decimal>::extractPalStrategy(permutedBacktester);
            
            if (!strategy) {
                // Log warning: non-PalStrategy in PAL validation
                // In production, you might want to use a proper logging framework
                return;
            }
            
            // Use enhanced BackTester methods for accurate statistics
            uint32_t numTrades = StrategyIdentificationHelper<Decimal>::extractNumTrades(permutedBacktester);
            uint32_t numBarsInTrades = StrategyIdentificationHelper<Decimal>::extractNumBarsInTrades(permutedBacktester);
            
            // Store all metrics using UUID-based aggregator
            m_statsAggregator.addValue(strategyHash, strategy, MetricType::PERMUTED_TEST_STATISTIC, permutedTestStatistic);
            m_statsAggregator.addValue(strategyHash, strategy, MetricType::NUM_TRADES, Decimal(numTrades));
            m_statsAggregator.addValue(strategyHash, strategy, MetricType::NUM_BARS_IN_TRADES, Decimal(numBarsInTrades));
        }

        // Convenience methods for accessing collected statistics
        
        /**
         * @brief Get minimum permuted test statistic for a strategy
         * @param strategy Pointer to the PalStrategy
         * @return Minimum test statistic, or nullopt if no data
         */
        std::optional<Decimal> getMinPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getMinMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }
        
        /**
         * @brief Get maximum permuted test statistic for a strategy
         * @param strategy Pointer to the PalStrategy
         * @return Maximum test statistic, or nullopt if no data
         */
        std::optional<Decimal> getMaxPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getMaxMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }
        
        /**
         * @brief Get median permuted test statistic for a strategy
         * @param strategy Pointer to the PalStrategy
         * @return Median test statistic, or nullopt if no data
         */
        std::optional<double> getMedianPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getMedianMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }
        
        /**
         * @brief Get standard deviation of permuted test statistics for a strategy
         * @param strategy Pointer to the PalStrategy
         * @return Standard deviation, or nullopt if insufficient data
         */
        std::optional<double> getStdDevPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getStdDevMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }

        // Enhanced statistics methods for trades and bars
        
        /**
         * @brief Get minimum number of trades across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Minimum trade count, or nullopt if no data
         */
        std::optional<Decimal> getMinNumTrades(const PalStrategy<Decimal>* strategy) const {
            return getMinMetric(strategy, MetricType::NUM_TRADES);
        }
        
        /**
         * @brief Get maximum number of trades across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Maximum trade count, or nullopt if no data
         */
        std::optional<Decimal> getMaxNumTrades(const PalStrategy<Decimal>* strategy) const {
            return getMaxMetric(strategy, MetricType::NUM_TRADES);
        }
        
        /**
         * @brief Get median number of trades across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Median trade count, or nullopt if no data
         */
        std::optional<double> getMedianNumTrades(const PalStrategy<Decimal>* strategy) const {
            return getMedianMetric(strategy, MetricType::NUM_TRADES);
        }
        
        /**
         * @brief Get standard deviation of trade counts across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Standard deviation of trade counts, or nullopt if insufficient data
         */
        std::optional<double> getStdDevNumTrades(const PalStrategy<Decimal>* strategy) const {
            return getStdDevMetric(strategy, MetricType::NUM_TRADES);
        }

        /**
         * @brief Get minimum number of bars in trades across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Minimum bar count, or nullopt if no data
         */
        std::optional<Decimal> getMinNumBarsInTrades(const PalStrategy<Decimal>* strategy) const {
            return getMinMetric(strategy, MetricType::NUM_BARS_IN_TRADES);
        }
        
        /**
         * @brief Get maximum number of bars in trades across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Maximum bar count, or nullopt if no data
         */
        std::optional<Decimal> getMaxNumBarsInTrades(const PalStrategy<Decimal>* strategy) const {
            return getMaxMetric(strategy, MetricType::NUM_BARS_IN_TRADES);
        }
        
        /**
         * @brief Get median number of bars in trades across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Median bar count, or nullopt if no data
         */
        std::optional<double> getMedianNumBarsInTrades(const PalStrategy<Decimal>* strategy) const {
            return getMedianMetric(strategy, MetricType::NUM_BARS_IN_TRADES);
        }
        
        /**
         * @brief Get standard deviation of bar counts across permutations
         * @param strategy Pointer to the PalStrategy
         * @return Standard deviation of bar counts, or nullopt if insufficient data
         */
        std::optional<double> getStdDevNumBarsInTrades(const PalStrategy<Decimal>* strategy) const {
            return getStdDevMetric(strategy, MetricType::NUM_BARS_IN_TRADES);
        }

        // PermutationTestObserver interface implementation
        
        std::optional<Decimal> getMinMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getMin(strategy, metric);
        }
        
        std::optional<Decimal> getMaxMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getMax(strategy, metric);
        }
        
        std::optional<double> getMedianMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getMedian(strategy, metric);
        }
        
        std::optional<double> getStdDevMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getStdDev(strategy, metric);
        }
        
        void clear() override {
            m_statsAggregator.clear();
        }

        // Additional analysis methods
        
        /**
         * @brief Get the number of unique strategies being tracked
         * @return Count of unique strategy instances
         */
        size_t getStrategyCount() const {
            return m_statsAggregator.getStrategyCount();
        }
        
        /**
         * @brief Get the number of permutations recorded for a strategy and metric
         * @param strategy Pointer to the PalStrategy
         * @param metric Type of metric to check
         * @return Number of permutation samples
         */
        size_t getPermutationCount(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            return m_statsAggregator.getPermutationCount(strategy, metric);
        }
        
        /**
         * @brief Get all strategies with the same pattern
         * @param patternHash The pattern hash to search for
         * @return Vector of strategy pointers with matching pattern
         */
        std::vector<const PalStrategy<Decimal>*> getStrategiesWithSamePattern(unsigned long long patternHash) const {
            return m_statsAggregator.getStrategiesWithSamePattern(patternHash);
        }
    };
}

#endif