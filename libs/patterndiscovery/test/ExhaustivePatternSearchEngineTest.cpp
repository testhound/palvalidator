#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <cstdio> // For std::remove

// Include the class to be tested and its direct dependencies
#include "ExhaustivePatternSearchEngine.h"
#include "PatternTemplate.h"
#include "PatternUniverseSerializer.h"
#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
#include "Security.h"
#include "TimeSeries.h"
#include "number.h"
#include "TimeFrame.h"
#include "TimeSeriesEntry.h"
#include "PatternCondition.h"
#include "PriceComponentDescriptor.h"

// Include shared test utilities
#include "TestUtilities.h"

// --- Helper Functions and Test Fixtures ---

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;


/**
 * @brief Creates a temporary binary pattern universe file for testing.
 *
 * @param filePath The path where the temporary file will be created.
 * @param templates A vector of PatternTemplate objects to serialize into the file.
 */
void createTestUniverseFile(const std::string& filePath, const std::vector<PatternTemplate>& templates)
{
    std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        throw std::runtime_error("Failed to create test universe file at: " + filePath);
    }
    PatternUniverseSerializer serializer;
    serializer.serialize(outFile, templates);
    outFile.close();
}


// --- Test Cases for Refactored Engine ---

TEST_CASE("ExhaustivePatternSearchEngine loads and evaluates a pattern universe", "[ExhaustivePatternSearchEngine]")
{
    // ARRANGE
    const std::string testUniverseFile = "test_universe.bin";
    
    // Create a universe with one pattern that will be profitable long, and one that won't.
    PatternTemplate profitableTemplate("UpDay_C0_gt_O0");
    profitableTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 0)
    ));

    PatternTemplate unprofitableTemplate("Unprofitable_O0_gt_C0");
     unprofitableTemplate.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Open, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Close, 0)
    ));

    std::vector<PatternTemplate> templates = { profitableTemplate, unprofitableTemplate };
    createTestUniverseFile(testUniverseFile, templates);

    auto mockSecurity = createMockSecurity(SeriesType::ProfitableLong);
    auto config = createTestConfig(mockSecurity, 5);

    // ACT
    ExhaustivePatternSearchEngine<Decimal> engine(config, testUniverseFile);
    auto results = engine.run();

    // ASSERT
    REQUIRE(results != nullptr);
    // We expect only the profitable long version of the "UpDay" template to be found.
    REQUIRE(results->getNumPatterns() == 1);
    REQUIRE(results->getNumLongPatterns() == 1);
    REQUIRE(results->getNumShortPatterns() == 0);

    auto foundPattern = *results->allPatternsBegin();
    REQUIRE(foundPattern->getFileName().find("UpDay_C0_gt_O0_Long") != std::string::npos);

    // CLEANUP
    std::remove(testUniverseFile.c_str());
}

TEST_CASE("ExhaustivePatternSearchEngine handles an empty universe file", "[ExhaustivePatternSearchEngine]")
{
    // ARRANGE
    const std::string emptyUniverseFile = "empty_universe.bin";
    createTestUniverseFile(emptyUniverseFile, {}); // Create file with 0 patterns

    auto mockSecurity = createMockSecurity(SeriesType::Unprofitable);
    auto config = createTestConfig(mockSecurity);

    // ACT
    ExhaustivePatternSearchEngine<Decimal> engine(config, emptyUniverseFile);
    auto results = engine.run();

    // ASSERT
    REQUIRE(results != nullptr);
    REQUIRE(results->getNumPatterns() == 0);

    // CLEANUP
    std::remove(emptyUniverseFile.c_str());
}

TEST_CASE("ExhaustivePatternSearchEngine throws for a missing universe file", "[ExhaustivePatternSearchEngine]")
{
    // ARRANGE
    const std::string missingFile = "non_existent_universe.bin";
    auto mockSecurity = createMockSecurity(SeriesType::Unprofitable);
    auto config = createTestConfig(mockSecurity);

    // ACT & ASSERT
    ExhaustivePatternSearchEngine<Decimal> engine(config, missingFile);
    REQUIRE_THROWS_AS(engine.run(), ExhaustivePatternSearchEngineException);
}

TEST_CASE("ExhaustivePatternSearchEngine works with different executors", "[ExhaustivePatternSearchEngine][executors]")
{
    // ARRANGE
    const std::string universeFile = "executor_test_universe.bin";
    
    PatternTemplate t1("T1");
    t1.addCondition(PatternCondition(PriceComponentDescriptor(PriceComponentType::Close, 0), ComparisonOperator::GreaterThan, PriceComponentDescriptor(PriceComponentType::Open, 0)));
    
    PatternTemplate t2("T2");
    t2.addCondition(PatternCondition(PriceComponentDescriptor(PriceComponentType::Open, 0), ComparisonOperator::GreaterThan, PriceComponentDescriptor(PriceComponentType::Close, 0)));

    createTestUniverseFile(universeFile, {t1, t2});

    auto mockSecurity = createMockSecurity(SeriesType::ProfitableLong); // Profitable for t1
    auto config = createTestConfig(mockSecurity, 5);

    // ACT & ASSERT
    SECTION("SingleThreadExecutor")
    {
        ExhaustivePatternSearchEngine<Decimal, concurrency::SingleThreadExecutor> engine(config, universeFile);
        auto results = engine.run();
        REQUIRE(results->getNumPatterns() == 1);
        REQUIRE(results->getNumLongPatterns() == 1);
    }

    SECTION("ThreadPoolExecutor")
    {
        ExhaustivePatternSearchEngine<Decimal, concurrency::ThreadPoolExecutor<>> engine(config, universeFile);
        auto results = engine.run();
        REQUIRE(results->getNumPatterns() == 1);
        REQUIRE(results->getNumLongPatterns() == 1);
    }

    // CLEANUP
    std::remove(universeFile.c_str());
}


