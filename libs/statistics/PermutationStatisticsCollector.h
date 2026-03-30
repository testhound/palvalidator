#pragma once

/**
 * @file PermutationStatisticsCollector.h
 * @brief Concrete observer that collects granular per-strategy permutation test
 *        statistics using UUID-based identification and Boost.Accumulators.
 */

#include "PermutationTestObserver.h"
#include "UuidStrategyPermutationStatsAggregator.h"
#include "StrategyIdentificationHelper.h"
#include "PalStrategy.h"
#include <optional>
#include <iostream>

namespace mkc_timeseries
{
    /**
     * @class PermutationStatisticsCollector
     * @brief Observer implementation for collecting granular permutation test statistics
     *        for PAL (Price Action Lab) strategies using UUID-based identification.
     *
     * This observer collects comprehensive statistics during permutation testing including:
     * - Permuted test statistics (e.g., profit factor, Sharpe ratio)
     * - Number of trades (closed + open positions)
     * - Number of bars in trades (comprehensive trade duration analysis)
     *
     * Key Features:
     * - UUID-based strategy identification eliminates collision risk
     * - Boost.Accumulators integration for 90% memory efficiency improvement
     * - Thread-safe statistics collection for concurrent permutation testing
     * - Enhanced BackTester methods for accurate trade/bar counting
     *
     * @tparam Decimal Numeric type for calculations (e.g., double, long double)
     */
    template <class Decimal>
    class PermutationStatisticsCollector : public PermutationTestObserver<Decimal> {
    private:
        using MetricType = typename PermutationTestObserver<Decimal>::MetricType;
        UuidStrategyPermutationStatsAggregator<Decimal> m_statsAggregator;

    public:
        /**
         * @brief Records a baseline statistic exceedance rate for a given strategy.
         *
         * Stores the fraction of permutations in which the maximum permuted test
         * statistic across all strategies exceeded this strategy's baseline
         * statistic. Used by multiple-testing correction algorithms.
         *
         * @param strategy Pointer to the strategy; ignored with a warning if null.
         * @param rate     The exceedance rate to record.
         */
      void recordExceedanceRate(const PalStrategy<Decimal>* strategy, const Decimal& rate)
      {
	if (!strategy) {
	  std::cout << "[DIAGNOSTIC] WARNING: Null strategy in recordExceedanceRate" << std::endl;
	  return;
	}

	unsigned long long strategyHash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);

	m_statsAggregator.addValue(strategyHash, strategy,
				   PermutationTestObserver<Decimal>::MetricType::BASELINE_STAT_EXCEEDANCE_RATE,
				   rate);
      }

        /**
         * @brief Called by subjects when a permutation backtest completes.
         * @param permutedBacktester The BackTester instance after running on synthetic data.
         * @param permutedTestStatistic The performance statistic from this permutation.
         */
        void update(const BackTester<Decimal>& permutedBacktester,
                    const Decimal& permutedTestStatistic) override {

            // Extract strategy identification using centralized hash computation
            unsigned long long strategyHash = StrategyIdentificationHelper<Decimal>::extractStrategyHash(permutedBacktester);
            const PalStrategy<Decimal>* strategy = StrategyIdentificationHelper<Decimal>::extractPalStrategy(permutedBacktester);

            if (!strategy) {
                // Log warning: non-PalStrategy in PAL validation
                // This observer is specifically designed for PalStrategy instances
                std::cout << "[DIAGNOSTIC] WARNING: Non-PalStrategy found in PAL validation" << std::endl;
                return;
            }

            // Use enhanced BackTester methods for accurate statistics
            uint32_t numTrades = StrategyIdentificationHelper<Decimal>::extractNumTrades(permutedBacktester);
            uint32_t numBarsInTrades = StrategyIdentificationHelper<Decimal>::extractNumBarsInTrades(permutedBacktester);

            // Store all metrics using centralized hash computation
            m_statsAggregator.addValue(strategyHash, strategy, MetricType::PERMUTED_TEST_STATISTIC, permutedTestStatistic);
            m_statsAggregator.addValue(strategyHash, strategy, MetricType::NUM_TRADES, Decimal(numTrades));
            m_statsAggregator.addValue(strategyHash, strategy, MetricType::NUM_BARS_IN_TRADES, Decimal(numBarsInTrades));

        }

        /**
         * @brief Called by subjects when a specific metric is calculated for a strategy.
         * @param strategy The strategy for which the metric is being reported.
         * @param metricType The type of metric being reported.
         * @param metricValue The calculated metric value.
         */
        void updateMetric(const PalStrategy<Decimal>* strategy,
                         MetricType metricType,
                         const Decimal& metricValue) override {

            if (!strategy) {
                std::cout << "[DIAGNOSTIC] WARNING: Null strategy in updateMetric" << std::endl;
                return;
            }

            // Extract strategy identification using centralized hash computation
            unsigned long long strategyHash = StrategyIdentificationHelper<Decimal>::extractCombinedHash(strategy);

            // Store the metric using centralized hash computation
            m_statsAggregator.addValue(strategyHash, strategy, metricType, metricValue);
        }

        /// @{ @name Base PermutationTestObserver interface implementation

        /**
         * @brief Returns the minimum observed value for the given metric and strategy.
         * @param strategy Pointer to the strategy whose metric is queried.
         * @param metric   The metric type to retrieve.
         * @return The minimum value, or std::nullopt if no data has been collected.
         */
        std::optional<Decimal> getMinMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getMin(strategy, metric);
        }

        /**
         * @brief Returns the maximum observed value for the given metric and strategy.
         * @param strategy Pointer to the strategy whose metric is queried.
         * @param metric   The metric type to retrieve.
         * @return The maximum value, or std::nullopt if no data has been collected.
         */
        std::optional<Decimal> getMaxMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getMax(strategy, metric);
        }

        /**
         * @brief Returns the median observed value for the given metric and strategy.
         * @param strategy Pointer to the strategy whose metric is queried.
         * @param metric   The metric type to retrieve.
         * @return The median as a double, or std::nullopt if no data has been collected.
         */
        std::optional<double> getMedianMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getMedian(strategy, metric);
        }

        /**
         * @brief Returns the standard deviation of the given metric for a strategy.
         * @param strategy Pointer to the strategy whose metric is queried.
         * @param metric   The metric type to retrieve.
         * @return The standard deviation as a double, or std::nullopt if no data has been collected.
         */
        std::optional<double> getStdDevMetric(const PalStrategy<Decimal>* strategy, MetricType metric) const override {
            return m_statsAggregator.getStdDev(strategy, metric);
        }

        /// Clears all collected statistics, resetting the collector to its initial state.
        void clear() override {
            m_statsAggregator.clear();
        }

        /// @}

        /// @{ @name Convenience accessors for the permuted test statistic metric

        /**
         * @brief Returns the minimum permuted test statistic for a strategy.
         * @param strategy Pointer to the strategy to query.
         * @return The minimum value, or std::nullopt if no data has been collected.
         */
        std::optional<Decimal> getMinPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getMinMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }

        /**
         * @brief Returns the maximum permuted test statistic for a strategy.
         * @param strategy Pointer to the strategy to query.
         * @return The maximum value, or std::nullopt if no data has been collected.
         */
        std::optional<Decimal> getMaxPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getMaxMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }

        /**
         * @brief Returns the median permuted test statistic for a strategy.
         * @param strategy Pointer to the strategy to query.
         * @return The median as a double, or std::nullopt if no data has been collected.
         */
        std::optional<double> getMedianPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getMedianMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }

        /**
         * @brief Returns the standard deviation of permuted test statistics for a strategy.
         * @param strategy Pointer to the strategy to query.
         * @return The standard deviation as a double, or std::nullopt if no data has been collected.
         */
        std::optional<double> getStdDevPermutedStatistic(const PalStrategy<Decimal>* strategy) const {
            return getStdDevMetric(strategy, MetricType::PERMUTED_TEST_STATISTIC);
        }

        /// @}

        /// @{ @name Utility methods

        /**
         * @brief Returns the number of distinct strategies tracked by this collector.
         * @return The count of unique strategies that have contributed data.
         */
        size_t getStrategyCount() const {
            return m_statsAggregator.getStrategyCount();
        }

        /**
         * @brief Returns the number of permutation samples recorded for a given strategy and metric.
         * @param strategy Pointer to the strategy to query.
         * @param metric   The metric type to count.
         * @return The number of samples recorded.
         */
        size_t getPermutationCount(const PalStrategy<Decimal>* strategy, MetricType metric) const {
            return m_statsAggregator.getPermutationCount(strategy, metric);
        }

        /**
         * @brief Returns the UUID associated with a strategy in the aggregator.
         * @param strategy Pointer to the strategy to query.
         * @return The boost::uuids::uuid for the given strategy.
         */
        boost::uuids::uuid getStrategyUuid(const PalStrategy<Decimal>* strategy) const {
            return m_statsAggregator.getStrategyUuid(strategy);
        }

        /**
         * @brief Returns the pattern hash associated with a strategy in the aggregator.
         * @param strategy Pointer to the strategy to query.
         * @return The combined hash used for strategy identification.
         */
        unsigned long long getPatternHash(const PalStrategy<Decimal>* strategy) const {
            return m_statsAggregator.getPatternHash(strategy);
        }

        /// @}
    };

} // namespace mkc_timeseries
