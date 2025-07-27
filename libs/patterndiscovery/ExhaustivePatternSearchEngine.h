// ExhaustivePatternSearchEngine.h
#ifndef EXHAUSTIVE_PATTERN_SEARCH_ENGINE_H
#define EXHAUSTIVE_PATTERN_SEARCH_ENGINE_H

#include <memory>
#include <vector>
#include <stdexcept>
#include <string>
#include <mutex>

#include "SearchConfiguration.h"
#include "PatternDiscoveryTask.h"
#include "AstResourceManager.h"
#include "PalAst.h"                 // For PriceActionLabSystem
#include "Security.h"
#include "TimeSeries.h"
#include "number.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

/**
 * @brief Exception class for errors related to the ExhaustivePatternSearchEngine.
 */
class ExhaustivePatternSearchEngineException : public std::runtime_error
{
public:
    /**
     * @brief Constructs an ExhaustivePatternSearchEngineException.
     * @param msg The error message.
     */
    explicit ExhaustivePatternSearchEngineException(const std::string& msg)
        : std::runtime_error(msg) {}

    /**
     * @brief Destroys the ExhaustivePatternSearchEngineException object.
     */
    ~ExhaustivePatternSearchEngineException() noexcept override = default;
};

/**
 * @brief Manages pattern discovery using a configurable execution policy.
 *
 * This class orchestrates the search by iterating through time series data,
 * creating, and executing a PatternDiscoveryTask for each window. The execution
 * is handled by the provided Executor policy (e.g., SingleThreadExecutor or
 * ThreadPoolExecutor), allowing for both sequential and parallel processing.
 *
 * @tparam DecimalType The decimal type for financial calculations.
 * @tparam Executor A policy class that defines the execution model.
 */
template <class DecimalType, typename Executor = concurrency::SingleThreadExecutor>
class ExhaustivePatternSearchEngine
{
public:
    /**
     * @brief Constructs an ExhaustivePatternSearchEngine.
     * @param config The search configuration that defines all parameters for the run.
     */
    explicit ExhaustivePatternSearchEngine(
        const SearchConfiguration<DecimalType>& config)
    : mConfig(config),
      mAstResourceManager(std::make_unique<mkc_palast::AstResourceManager>())
    {
    }

    /**
     * @brief Destroys the ExhaustivePatternSearchEngine object.
     */
    ~ExhaustivePatternSearchEngine() noexcept = default;

    /**
     * @brief Executes the pattern search using the configured Executor policy.
     *
     * Iterates through the time series, submitting a PatternDiscoveryTask for each
     * time step to the executor. It aggregates all profitable patterns found into
     * a single system in a thread-safe manner.
     *
     * @return A shared pointer to a PriceActionLabSystem containing all profitable patterns.
     */
    std::shared_ptr<PriceActionLabSystem> run()
    {
        auto security = mConfig.getSecurity();
        const auto& timeSeries = security->getTimeSeries();
        
        // Correctly instantiate PriceActionLabSystem using its default constructor.
        auto palSystem = std::make_shared<PriceActionLabSystem>();

        // Collect valid window end times to iterate over
        std::vector<boost::posix_time::ptime> windowEndTimes;
        auto searchLoopStartTime = mConfig.getBacktestStartTime();
        auto searchLoopEndTime = mConfig.getBacktestEndTime();

        // Determine the maximum lookback needed based on search type
        auto patternLengthRange = mConfig.getPatternLengthRange();
        unsigned int maxLookback = patternLengthRange.second; // Maximum pattern length
        
        // Collect all dates first
        std::vector<boost::posix_time::ptime> allDates;
        for (auto it = timeSeries->beginSortedAccess(); it != timeSeries->endSortedAccess(); ++it)
        {
            const auto& dt = it->getDateTime();
            if (dt >= searchLoopStartTime && dt <= searchLoopEndTime)
            {
                allDates.push_back(dt);
            }
        }
        
        // Skip the first maxLookback dates to ensure patterns have sufficient historical context
        if (allDates.size() > maxLookback)
        {
            windowEndTimes.assign(allDates.begin() + maxLookback, allDates.end());
        }

        if (windowEndTimes.empty())
        {
            return palSystem; // No data in the specified range
        }

        // The work lambda to be executed for each time window
        auto work = [this, &palSystem, &windowEndTimes](uint32_t i) {
            boost::posix_time::ptime windowEndTime = windowEndTimes[i];

            // Create and execute the task
            PatternDiscoveryTask<DecimalType> task(mConfig, windowEndTime, *mAstResourceManager);
            auto profitablePatternsForWindow = task.findPatterns();

            // Aggregate results in a thread-safe manner
            if (!profitablePatternsForWindow.empty())
            {
                std::lock_guard<std::mutex> guard(mResultsMutex);
                for (const auto& patternAndBacktesterPair : profitablePatternsForWindow)
                {
                    palSystem->addPattern(patternAndBacktesterPair.first);
                }
            }
        };

        // Execute using the chosen policy
        Executor executor{};
        concurrency::parallel_for(static_cast<uint32_t>(windowEndTimes.size()), executor, work);

        return palSystem;
    }

private:
    const SearchConfiguration<DecimalType>& mConfig;
    std::unique_ptr<mkc_palast::AstResourceManager> mAstResourceManager;
    std::mutex mResultsMutex;
};
#endif