// ExhaustivePatternSearchEngine.h
#ifndef EXHAUSTIVE_PATTERN_SEARCH_ENGINE_H
#define EXHAUSTIVE_PATTERN_SEARCH_ENGINE_H

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <mutex>
#include <fstream>
#include "SearchConfiguration.h"
#include "PatternEvaluationTask.h"
#include "PatternTemplate.h"
#include "PatternUniverseDeserializer.h"
#include "PricePatternFactory.h"
#include "AstResourceManager.h"
#include "PalAst.h"
#include "number.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

/**
 * @brief Exception class for errors related to the ExhaustivePatternSearchEngine.
 */
class ExhaustivePatternSearchEngineException : public std::runtime_error
{
public:
    explicit ExhaustivePatternSearchEngineException(const std::string& msg)
        : std::runtime_error(msg) {}

    ~ExhaustivePatternSearchEngineException() noexcept override = default;
};

/**
 * @brief Manages the high-throughput evaluation of a pre-generated pattern universe.
 *
 * This refactored engine loads a collection of PatternTemplate objects from a
 * binary file. It then uses a parallel execution policy to distribute the
 * evaluation of each template, orchestrating the backtesting and collection
 * of profitable patterns.
 *
 * @tparam DecimalType The decimal type for financial calculations.
 * @tparam Executor A policy class that defines the execution model (e.g., ThreadPoolExecutor).
 */
template <class DecimalType, typename Executor = concurrency::ThreadPoolExecutor<>>
class ExhaustivePatternSearchEngine
{
public:
    /**
     * @brief Constructs the pattern evaluation engine.
     * @param config The search configuration defining all backtesting parameters.
     * @param patternUniverseFilePath The full path to the binary pattern universe file.
     */
    explicit ExhaustivePatternSearchEngine(
        const SearchConfiguration<DecimalType>& config,
        const std::string& patternUniverseFilePath)
    : mConfig(config),
      mPatternUniverseFilePath(patternUniverseFilePath),
      mAstResourceManager(std::make_unique<mkc_palast::AstResourceManager>())
    {
    }

    /**
     * @brief Destroys the ExhaustivePatternSearchEngine object.
     */
    ~ExhaustivePatternSearchEngine() noexcept = default;

    /**
     * @brief Executes the parallel evaluation of the loaded pattern universe.
     *
     * This method performs three main steps:
     * 1. Opens and deserializes the pattern universe file into memory.
     * 2. Defines a work lambda that creates and runs a PatternEvaluationTask for one template.
     * 3. Uses the configured Executor policy to run the lambda in parallel for all templates.
     *
     * @return A shared pointer to a PriceActionLabSystem containing all profitable patterns found.
     */
    std::shared_ptr<PriceActionLabSystem> run()
    {
        // 1. Load the pattern universe from the specified file.
        loadPatternUniverse();

        auto palSystem = std::make_shared<PriceActionLabSystem>();

        if (mLoadedTemplates.empty())
        {
            throw ExhaustivePatternSearchEngineException(
                "Pattern universe is empty. No patterns loaded from file: " + mPatternUniverseFilePath);
        }

        // 2. Define the work lambda to be executed in parallel for each template.
        auto work = [this, &palSystem](uint32_t i) {
            const auto& currentTemplate = mLoadedTemplates[i];

            // Create a pattern factory for this task
            PricePatternFactory<DecimalType> patternFactory(*mAstResourceManager);

            // Create and execute the evaluation task for the current template.
            PatternEvaluationTask<DecimalType> task(mConfig, currentTemplate, patternFactory);

            // The task now returns a vector of profitable patterns (0, 1, or 2).
            auto profitablePatterns = task.evaluateAndBacktest();

            // Aggregate any and all profitable results in a thread-safe manner.
            if (!profitablePatterns.empty())
            {
                std::lock_guard<std::mutex> guard(mResultsMutex);
                for (const auto& profitablePattern : profitablePatterns)
                {
                    palSystem->addPattern(profitablePattern);
                }
            }
        };

        // 3. Execute the evaluation in parallel using the chosen policy.
        Executor executor{};
        concurrency::parallel_for(static_cast<uint32_t>(mLoadedTemplates.size()), executor, work);

        return palSystem;
    }

private:
    /**
     * @brief Opens and deserializes the binary pattern universe file into the
     * mLoadedTemplates member variable.
     */
    void loadPatternUniverse()
    {
        std::ifstream universeFile(mPatternUniverseFilePath, std::ios::binary);
        if (!universeFile.is_open())
        {
            throw ExhaustivePatternSearchEngineException(
                "Could not open pattern universe file: " + mPatternUniverseFilePath);
        }

        try
        {
            PatternUniverseDeserializer deserializer;
            mLoadedTemplates = deserializer.deserialize(universeFile);
        }
        catch (const std::runtime_error& e)
        {
            throw ExhaustivePatternSearchEngineException(
                "Failed to deserialize pattern universe file: " + std::string(e.what()));
        }
    }

private:
    const SearchConfiguration<DecimalType>& mConfig;
    std::string mPatternUniverseFilePath;
    std::vector<PatternTemplate> mLoadedTemplates;
    std::unique_ptr<mkc_palast::AstResourceManager> mAstResourceManager;
    std::mutex mResultsMutex;
};

#endif // EXHAUSTIVE_PATTERN_SEARCH_ENGINE_H
