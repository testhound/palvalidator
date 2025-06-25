// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __PERMUTATION_TEST_SUBJECT_H
#define __PERMUTATION_TEST_SUBJECT_H 1

#include <vector>
#include <shared_mutex>
#include <algorithm>
#include "PermutationTestObserver.h"

namespace mkc_timeseries
{
    // Forward declarations
    template <class Decimal> class BackTester;

    /**
     * @class PermutationTestSubject
     * @brief Subject base class for the Observer pattern in permutation testing
     * 
     * This class provides the standard Subject implementation for the Gang of Four
     * Observer pattern. It manages a list of observers and provides thread-safe
     * notification mechanisms for concurrent permutation testing.
     * 
     * Key Features:
     * - Thread-safe observer management using shared_mutex
     * - Clean separation: subjects only notify, observers handle identification
     * - Standard attach/detach interface for observer registration
     * - Protected notification method for derived classes
     * 
     * Usage:
     * Policy classes (e.g., DefaultPermuteMarketChangesPolicy) inherit from this
     * class and call notifyObservers() when permutation backtests complete.
     * 
     * @tparam Decimal Numeric type for calculations (e.g., double)
     */
    template <class Decimal>
    class PermutationTestSubject {
    protected:
        mutable std::shared_mutex m_observersMutex;
        std::vector<PermutationTestObserver<Decimal>*> m_observers;

    public:
        virtual ~PermutationTestSubject() = default;

        /**
         * @brief Attach an observer to receive notifications
         * @param observer Pointer to observer instance (must remain valid)
         */
        virtual void attach(PermutationTestObserver<Decimal>* observer) {
            std::unique_lock<std::shared_mutex> lock(m_observersMutex);
            m_observers.push_back(observer);
        }

        /**
         * @brief Detach an observer from notifications
         * @param observer Pointer to observer to remove
         */
        virtual void detach(PermutationTestObserver<Decimal>* observer) {
            std::unique_lock<std::shared_mutex> lock(m_observersMutex);
            m_observers.erase(
                std::remove(m_observers.begin(), m_observers.end(), observer),
                m_observers.end()
            );
        }

    protected:
        /**
         * @brief Notify all attached observers of a completed permutation
         * @param permutedBacktester The BackTester after running on synthetic data
         * @param permutedTestStatistic The performance statistic from this permutation
         * 
         * This method is called by derived policy classes when a permutation backtest
         * completes successfully. It uses a shared lock to allow concurrent reads
         * while preventing observer list modifications during notification.
         */
        virtual void notifyObservers(
            const BackTester<Decimal>& permutedBacktester,
            const Decimal& permutedTestStatistic) {
            
            std::shared_lock<std::shared_mutex> lock(m_observersMutex);
            for (auto* observer : m_observers) {
                if (observer) {
                    observer->update(permutedBacktester, permutedTestStatistic);
                }
            }
        }

        /**
         * @brief Notify observers with a specific metric value for a strategy
         * @param strategy The strategy for which the metric is being reported
         * @param metricType The type of metric being reported
         * @param metricValue The calculated metric value
         *
         * This overloaded method allows notifying observers about specific metrics
         * that are calculated after permutation testing completes, such as
         * baseline statistic exceedance rates.
         */
        virtual void notifyObservers(
            const PalStrategy<Decimal>* strategy,
            typename PermutationTestObserver<Decimal>::MetricType metricType,
            const Decimal& metricValue) {
            
            std::shared_lock<std::shared_mutex> lock(m_observersMutex);
            for (auto* observer : m_observers) {
                if (observer) {
                    observer->updateMetric(strategy, metricType, metricValue);
                }
            }
        }
    };
}

#endif