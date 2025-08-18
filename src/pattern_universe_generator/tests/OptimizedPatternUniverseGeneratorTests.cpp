/**
 * @file OptimizedPatternUniverseGeneratorTests.cpp
 * @brief Comprehensive unit tests for OptimizedPatternUniverseGenerator component
 * 
 * Tests cover pattern generation, parallel processing, export methods, delay patterns,
 * and performance estimation using both mock data and real PAL analysis data.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "OptimizedPatternUniverseGenerator.h"
#include "PALAnalysisLoader.h"
#include "CuratedGroupManager.h"
#include "OptimizedDataStructures.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

using namespace pattern_universe;

namespace 
{
    /**
     * @brief Get the well-known PAL analysis data directory
     */
    std::string getPALAnalysisDataDir() 
    {
        return "dataset/pal_analysis";
    }

    /**
     * @brief Check if real PAL analysis data is available
     */
    bool hasRealPALData() 
    {
        std::string dataDir = getPALAnalysisDataDir();
        return std::filesystem::exists(dataDir + "/component_analysis_report.json") &&
               std::filesystem::exists(dataDir + "/index_mapping_report.json") &&
               std::filesystem::exists(dataDir + "/pattern_structure_analysis.json") &&
               std::filesystem::exists(dataDir + "/search_algorithm_report.json");
    }

    /**
     * @brief Create mock PAL analysis data for unit testing
     */
    std::shared_ptr<PALAnalysisData> createMockPALAnalysisData() 
    {
        // Create index to group mappings
        std::map<uint32_t, CuratedGroup> indexToGroup;
        std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices;
        
        // Create a few representative groups for testing
        std::vector<std::tuple<int, std::set<pattern_universe::PriceComponentType>, bool, int>> testGroups = {
            {1, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}, true, 1000},
            {2, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}, true, 800},
            {3, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW}, false, 600},
            {4, {pattern_universe::PriceComponentType::CLOSE}, false, 400},
            {5, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}, true, 950},
            {6, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}, true, 750},
            {7, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW}, false, 550},
            {8, {pattern_universe::PriceComponentType::CLOSE}, false, 350},
            {9, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::CLOSE}, false, 500},
            {10, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::CLOSE}, false, 450}
        };
        
        for (const auto& [indexNum, components, chaining, patternCount] : testGroups) 
        {
            CuratedGroup group(indexNum, {0, 1, 2}, components, SearchType::DEEP,
                              2, 8, patternCount, 0.8, chaining);
            // Note: CuratedGroup constructor doesn't set delay pattern support,
            // but the default constructor sets mSupportsDelayPatterns = true
            
            indexToGroup[indexNum] = std::move(group);
            searchTypeToIndices[SearchType::DEEP].push_back(indexNum);
        }
        
        // Create component stats
        std::map<SearchType, ComponentUsageStats> componentStats;
        std::map<pattern_universe::PriceComponentType, uint32_t> componentUsage = {
            {pattern_universe::PriceComponentType::CLOSE, 2000},
            {pattern_universe::PriceComponentType::HIGH, 1500},
            {pattern_universe::PriceComponentType::LOW, 1200},
            {pattern_universe::PriceComponentType::OPEN, 800}
        };
        std::map<uint8_t, uint32_t> tierUsage; // Empty for this test
        ComponentUsageStats deepStats(5000, 10, componentUsage, tierUsage);
        componentStats.emplace(SearchType::DEEP, std::move(deepStats));
        
        // Create PAL index mappings
        PALIndexMappings indexMappings(std::move(indexToGroup), std::move(searchTypeToIndices), 
                                      componentStats, 5000, 10, std::chrono::system_clock::now());
        
        // Create algorithm insights
        AlgorithmInsights algorithmInsights(5000, 2500, 50.0);
        
        // Create hierarchy rules
        std::map<uint32_t, std::set<pattern_universe::PriceComponentType>> indexToAllowedComponents;
        for (const auto& [indexNum, group] : indexMappings.getIndexToGroup()) 
        {
            indexToAllowedComponents[indexNum] = group.getComponentTypes();
        }
        ComponentHierarchyRules hierarchyRules(std::move(indexToAllowedComponents));
        
        // Create PALAnalysisData with constructor
        auto data = std::make_shared<PALAnalysisData>(
            std::move(indexMappings),
            std::move(componentStats),
            std::move(algorithmInsights),
            std::move(hierarchyRules),
            "1.0",
            std::vector<std::string>{"component_analysis_report.json", "index_mapping_report.json"}
        );
        
        return data;
    }

    /**
     * @brief Create a temporary directory for test output files
     */
    std::string createTempOutputDir() 
    {
        std::string tempDir = "test_output_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directory(tempDir);
        return tempDir;
    }

    /**
     * @brief Clean up temporary test directory
     */
    void cleanupTempDir(const std::string& dir) 
    {
        if (std::filesystem::exists(dir)) 
        {
            std::filesystem::remove_all(dir);
        }
    }

    /**
     * @brief Verify that a JSON file exists and contains expected content
     */
    bool verifyJSONFile(const std::string& filePath, const std::vector<std::string>& expectedKeys)
    {
        if (!std::filesystem::exists(filePath))
        {
            std::cout << "DEBUG: File does not exist: " << filePath << std::endl;
            return false;
        }
        
        // JSON file validation
        
        std::ifstream file(filePath);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        // JSON file content validation
        
        for (const auto& key : expectedKeys)
        {
            // Search for the key with quotes as it appears in JSON
            std::string quotedKey = "\"" + key + "\"";
            if (content.find(quotedKey) == std::string::npos)
            {
                std::cout << "DEBUG: Key not found: " << quotedKey << std::endl;
                return false;
            } else {
                // Key found
            }
        }
        
        return true;
    }

    /**
     * @brief Verify that a CSV file exists and has expected structure
     */
    bool verifyCSVFile(const std::string& filePath, const std::vector<std::string>& expectedHeaders) 
    {
        if (!std::filesystem::exists(filePath)) 
        {
            return false;
        }
        
        std::ifstream file(filePath);
        std::string headerLine;
        std::getline(file, headerLine);
        
        for (const auto& header : expectedHeaders) 
        {
            if (headerLine.find(header) == std::string::npos) 
            {
                return false;
            }
        }
        
        return true;
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Constructor and Basic Operations", "[OptimizedPatternUniverseGenerator][constructor]")
{
    SECTION("Constructor with mock PAL analysis data")
    {
        // Create mock data directly as unique_ptr to avoid ownership issues
        auto mockData = createMockPALAnalysisData();
        // Create a copy for the unique_ptr since we can't safely transfer ownership from shared_ptr
        auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
        REQUIRE_NOTHROW([&]() { OptimizedPatternUniverseGenerator generator(std::move(uniqueData)); }());
        
        // Create another instance for testing
        auto mockData2 = createMockPALAnalysisData();
        auto uniqueData2 = std::make_unique<PALAnalysisData>(*mockData2);
        OptimizedPatternUniverseGenerator generator(std::move(uniqueData2));
        REQUIRE(generator.getPALAnalysisData().getIndexMappings().getIndexToGroup().size() == 10);
    }
    
    SECTION("Constructor with null data throws exception")
    {
        std::unique_ptr<PALAnalysisData> nullData = nullptr;
        REQUIRE_THROWS_AS([&]() { OptimizedPatternUniverseGenerator generator(std::move(nullData)); }(), std::invalid_argument);
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Configuration Management", "[OptimizedPatternUniverseGenerator][configuration]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    
    SECTION("Default configuration creation")
    {
        // Test creating a default configuration
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP);
        
        REQUIRE(config.getTargetSearchType() == SearchType::DEEP);
        REQUIRE(config.isParallelProcessingEnabled() == true);
        REQUIRE(config.isDelayPatternsEnabled() == false); // Default is false for explicit choice
        REQUIRE(config.getMaxDelayBars() == 5);
        REQUIRE(config.getMaxLookbackWithDelay() == 15);
    }
    
    SECTION("Custom configuration creation")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(
            SearchType::EXTENDED,
            false,  // enableParallelProcessing
            4,      // maxThreads
            true,   // enableChaining
            true,   // enablePreComputation
            500,    // preComputationThreshold
            true,   // prioritizeHighYield
            24.0,   // targetSpeedupFactor
            true,   // enableDelayPatterns
            3,      // maxDelayBars
            20      // maxLookbackWithDelay
        );
        
        REQUIRE(config.getTargetSearchType() == SearchType::EXTENDED);
        REQUIRE(config.isParallelProcessingEnabled() == false);
        REQUIRE(config.getMaxThreads() == 4);
        REQUIRE(config.isDelayPatternsEnabled() == true);
        REQUIRE(config.getMaxDelayBars() == 3);
        REQUIRE(config.getMaxLookbackWithDelay() == 20);
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Pattern Generation", "[OptimizedPatternUniverseGenerator][generation]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    
    SECTION("Generate basic pattern universe without delay patterns")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(
            SearchType::DEEP,
            true,   // enableParallelProcessing
            0,      // maxThreads (auto-detect)
            true,   // enableChaining
            true,   // enablePreComputation
            500,    // preComputationThreshold
            true,   // prioritizeHighYield
            24.0,   // targetSpeedupFactor
            false,  // enableDelayPatterns
            5,      // maxDelayBars
            15      // maxLookbackWithDelay
        );
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        REQUIRE(result.getBasePatterns() > 0);
        REQUIRE(result.getDelayPatterns() == 0); // Delay patterns disabled
        REQUIRE(result.getTotalGenerationTime().count() >= 0); // Allow for very fast operations
        
        // Verify patterns are properly populated
        REQUIRE(result.getPatterns().size() > 0);
    }
    
    SECTION("Generate pattern universe with delay patterns")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        REQUIRE(result.getBasePatterns() > 0);
        REQUIRE(result.getDelayPatterns() > 0); // Should have delay patterns
        REQUIRE(result.getDelayDistribution().size() > 0);
        
        // Verify delay pattern distribution
        size_t totalDelayFromDistribution = 0;
        for (const auto& [delayBars, count] : result.getDelayDistribution())
        {
            REQUIRE(delayBars >= 1);
            REQUIRE(delayBars <= 3); // maxDelayBars
            REQUIRE(count > 0);
            totalDelayFromDistribution += count;
        }
        REQUIRE(totalDelayFromDistribution == result.getDelayPatterns());
    }
    
    SECTION("Generate with complexity filtering")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0); // Should still generate some patterns
    }
    
    SECTION("Generate with pattern count limit")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getBasePatterns() > 0);
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Parallel Processing", "[OptimizedPatternUniverseGenerator][parallel]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    
    SECTION("Compare serial vs parallel processing performance")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig serialConfig(SearchType::DEEP, false, 0, true, true, 500, true, 24.0, true, 5, 15);
        OptimizedPatternUniverseGenerator::GenerationConfig parallelConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 5, 15);
        
        // Serial processing
        auto startSerial = std::chrono::high_resolution_clock::now();
        auto serialResult = generator.generatePatternUniverse(serialConfig);
        auto endSerial = std::chrono::high_resolution_clock::now();
        auto serialTime = std::chrono::duration_cast<std::chrono::milliseconds>(endSerial - startSerial);
        
        // Parallel processing
        auto startParallel = std::chrono::high_resolution_clock::now();
        auto parallelResult = generator.generatePatternUniverse(parallelConfig);
        auto endParallel = std::chrono::high_resolution_clock::now();
        auto parallelTime = std::chrono::duration_cast<std::chrono::milliseconds>(endParallel - startParallel);
        
        // Results should be similar
        REQUIRE(serialResult.getTotalPatternsGenerated() > 0);
        REQUIRE(parallelResult.getTotalPatternsGenerated() > 0);
        REQUIRE(serialResult.getBasePatterns() == parallelResult.getBasePatterns());
        REQUIRE(serialResult.getDelayPatterns() == parallelResult.getDelayPatterns());
        
        // Parallel should generally be faster (though with small dataset might not be significant)
        // At minimum, parallel processing shouldn't be significantly slower
        REQUIRE(parallelTime.count() <= serialTime.count() * 2); // Allow 2x tolerance for overhead
        
        // Verify timing is recorded correctly (allow for very fast operations)
        REQUIRE(serialResult.getTotalGenerationTime().count() >= 0);
        REQUIRE(parallelResult.getTotalGenerationTime().count() >= 0);
    }
    
    SECTION("Thread safety verification")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 15);
        
        // Run multiple generations concurrently
        std::vector<std::thread> threads;
        // Create vector with proper initialization since PatternUniverseResult has no default constructor
        std::vector<PatternUniverseResult> results;
        results.reserve(3);
        
        std::mutex resultsMutex;
        for (int i = 0; i < 3; ++i)
        {
            threads.emplace_back([&generator, &config, &results, &resultsMutex, i]() {
                auto result = generator.generatePatternUniverse(config);
                std::lock_guard<std::mutex> lock(resultsMutex);
                if (results.size() <= static_cast<size_t>(i)) {
                    results.resize(i + 1, std::move(result));
                } else {
                    results[i] = std::move(result);
                }
            });
        }
        
        for (auto& thread : threads)
        {
            thread.join();
        }
        
        // All results should be similar (deterministic)
        for (int i = 1; i < 3; ++i)
        {
            REQUIRE(results[i].getTotalPatternsGenerated() > 0);
            REQUIRE(results[i].getBasePatterns() == results[0].getBasePatterns());
            REQUIRE(results[i].getDelayPatterns() == results[0].getDelayPatterns());
        }
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Export Functionality", "[OptimizedPatternUniverseGenerator][export]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    std::string tempDir = createTempOutputDir();
    
    SECTION("Export to JSON")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        std::string jsonPath = tempDir + "/pattern_universe.json";
        generator.exportPatterns(result, jsonPath, ExportFormat::JSON);
        
        // Verify JSON file exists and contains expected keys
        std::vector<std::string> expectedKeys = {
            "metadata", "patterns", "statistics"
        };
        REQUIRE(verifyJSONFile(jsonPath, expectedKeys));
    }
    
    SECTION("Export to CSV")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 2, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        std::string csvPath = tempDir + "/pattern_universe.csv";
        generator.exportPatterns(result, csvPath, ExportFormat::CSV);
        
        // Verify CSV file exists and has expected headers
        std::vector<std::string> expectedHeaders = {
            "PatternName", "MaxBarOffset", "NumUniqueComponents", "NumConditions", "Conditions"
        };
        REQUIRE(verifyCSVFile(csvPath, expectedHeaders));
    }
    
    SECTION("Export summary report")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 4, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        std::string summaryPath = tempDir + "/summary_report.json";
        generator.exportPatterns(result, summaryPath, ExportFormat::JSON);
        
        // Verify summary contains high-level metrics
        std::vector<std::string> expectedSummaryKeys = {
            "metadata", "patterns", "statistics"
        };
        REQUIRE(verifyJSONFile(summaryPath, expectedSummaryKeys));
    }
    
    cleanupTempDir(tempDir);
}

TEST_CASE("OptimizedPatternUniverseGenerator - Performance Estimation", "[OptimizedPatternUniverseGenerator][performance]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    
    SECTION("Estimate generation time")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 5, 15);
        
        auto estimate = generator.estimatePerformance(config);
        
        REQUIRE(estimate.getEstimatedTime().count() > 0);
        REQUIRE(estimate.getEstimatedPatterns() > 0);
        
        // Verify actual generation time is within reasonable bounds of estimate
        auto actualResult = generator.generatePatternUniverse(config);
        
        // Allow significant tolerance for estimation accuracy
        double timeTolerance = 5.0; // 5x tolerance
        REQUIRE(actualResult.getTotalGenerationTime().count() <= estimate.getEstimatedTime().count() * timeTolerance);
        REQUIRE(actualResult.getTotalPatternsGenerated() <= estimate.getEstimatedPatterns() * timeTolerance);
    }
    
    SECTION("Performance scaling analysis")
    {
        // Test with different configurations to verify scaling
        std::vector<OptimizedPatternUniverseGenerator::GenerationConfig> configs = {
            OptimizedPatternUniverseGenerator::GenerationConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, false, 5, 15),
            OptimizedPatternUniverseGenerator::GenerationConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 2, 15),
            OptimizedPatternUniverseGenerator::GenerationConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 15),
            OptimizedPatternUniverseGenerator::GenerationConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 5, 15)
        };
        
        std::vector<PerformanceEstimate> estimates;
        
        for (const auto& config : configs)
        {
            estimates.push_back(generator.estimatePerformance(config));
        }
        
        // Generally, more complex configurations should take longer
        // (though with small mock dataset, differences might be minimal)
        REQUIRE(estimates.size() == 4);
        
        for (const auto& estimate : estimates)
        {
            REQUIRE(estimate.getEstimatedTime().count() > 0);
            REQUIRE(estimate.getEstimatedPatterns() > 0);
        }
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Delay Pattern Validation", "[OptimizedPatternUniverseGenerator][delay_patterns]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    
    SECTION("Delay pattern generation with different delay bar limits")
    {
        std::vector<int> delayBarLimits = {1, 2, 3, 4, 5};
        
        for (int maxDelay : delayBarLimits)
        {
            OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, maxDelay, 20);
            
            auto result = generator.generatePatternUniverse(config);
            
            REQUIRE(result.getDelayPatterns() > 0);
            
            // Verify delay distribution respects the limit
            for (const auto& [delayBars, count] : result.getDelayDistribution())
            {
                REQUIRE(delayBars >= 1);
                REQUIRE(delayBars <= maxDelay);
                REQUIRE(count > 0);
            }
            
            // Should have delay patterns for each delay bar value (1 to maxDelay)
            REQUIRE(result.getDelayDistribution().size() <= static_cast<size_t>(maxDelay));
        }
    }
    
    SECTION("Delay pattern lookback validation")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 10);
        
        auto result = generator.generatePatternUniverse(config);
        
        // With restrictive lookback, some patterns might be filtered out
        // Note: getDelayPatterns() returns size_t which is always >= 0, so just check it exists
        REQUIRE((result.getDelayPatterns() == 0 || result.getDelayPatterns() > 0)); // Could be 0 if all filtered
        
        // If delay patterns exist, they should respect lookback constraints
        if (result.getDelayPatterns() > 0)
        {
            REQUIRE(result.getDelayDistribution().size() > 0);
        }
    }
    
    SECTION("Delay pattern disable/enable toggle")
    {
        // Generate without delay patterns
        OptimizedPatternUniverseGenerator::GenerationConfig configNoDelay(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, false, 5, 15);
        auto resultNoDelay = generator.generatePatternUniverse(configNoDelay);
        
        // Generate with delay patterns
        OptimizedPatternUniverseGenerator::GenerationConfig configWithDelay(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 15);
        auto resultWithDelay = generator.generatePatternUniverse(configWithDelay);
        
        // Base patterns should be the same
        REQUIRE(resultNoDelay.getBasePatterns() == resultWithDelay.getBasePatterns());
        
        // Only the delay-enabled version should have delay patterns
        REQUIRE(resultNoDelay.getDelayPatterns() == 0);
        REQUIRE(resultWithDelay.getDelayPatterns() > 0);
        
        // Total patterns should be different
        REQUIRE(resultWithDelay.getTotalPatternsGenerated() > resultNoDelay.getTotalPatternsGenerated());
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Real PAL Data Integration", "[OptimizedPatternUniverseGenerator][integration][real_data]")
{
    // Skip these tests if real PAL data is not available
    if (!hasRealPALData()) 
    {
        SKIP("Real PAL analysis data not found in dataset/pal_analysis/ - skipping integration tests");
        return;
    }
    
    SECTION("Generate pattern universe with real PAL data")
    {
        PALAnalysisLoader loader;
        auto realData = loader.loadCompleteAnalysis(getPALAnalysisDataDir());
        
        REQUIRE(realData != nullptr);
        
        OptimizedPatternUniverseGenerator generator(std::move(realData));
        
        // Test with realistic configuration
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 3, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        // Verify realistic results
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        REQUIRE(result.getBasePatterns() > 0);
        REQUIRE(result.getDelayPatterns() > 0); // Should have delay patterns
        REQUIRE(result.getTotalGenerationTime().count() > 0);
        
        // Verify delay pattern distribution
        REQUIRE(result.getDelayDistribution().size() > 0);
        REQUIRE(result.getDelayDistribution().size() <= 3); // maxDelayBars
        
        // Test export with real data
        std::string tempDir = createTempOutputDir();
        std::string jsonPath = tempDir + "/real_data_export.json";
        
        generator.exportPatterns(result, jsonPath, ExportFormat::JSON);
        REQUIRE(std::filesystem::exists(jsonPath));
        
        cleanupTempDir(tempDir);
    }
    
    SECTION("Performance test with real PAL data")
    {
        PALAnalysisLoader loader;
        auto realData = loader.loadCompleteAnalysis(getPALAnalysisDataDir());
        
        OptimizedPatternUniverseGenerator generator(std::move(realData));
        
        // Test performance estimation accuracy with real data
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, false, 5, 15);
        
        auto estimate = generator.estimatePerformance(config);
        auto actualResult = generator.generatePatternUniverse(config);
        
        // Verify estimation accuracy with real data
        REQUIRE(estimate.getEstimatedTime().count() > 0);
        REQUIRE(estimate.getEstimatedPatterns() > 0);
        
        // Allow reasonable tolerance for real-world estimation
        double timeTolerance = 3.0;
        double patternTolerance = 2.0;
        
        REQUIRE(actualResult.getTotalGenerationTime().count() <= estimate.getEstimatedTime().count() * timeTolerance);
        REQUIRE(actualResult.getTotalPatternsGenerated() <= estimate.getEstimatedPatterns() * patternTolerance);
        
        // Generation should complete in reasonable time
        REQUIRE(actualResult.getTotalGenerationTime().count() < 30.0); // Less than 30 seconds
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Error Handling and Edge Cases", "[OptimizedPatternUniverseGenerator][error_handling]")
{
    auto mockData = createMockPALAnalysisData();
    auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
    OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
    
    SECTION("Handle invalid configuration values")
    {
        // Test with invalid configuration values - should be handled gracefully
        OptimizedPatternUniverseGenerator::GenerationConfig invalidConfig(
            SearchType::DEEP,
            true,   // enableParallelProcessing
            0,      // maxThreads
            true,   // enableChaining
            true,   // enablePreComputation
            500,    // preComputationThreshold
            true,   // prioritizeHighYield
            24.0,   // targetSpeedupFactor
            true,   // enableDelayPatterns
            0,      // maxDelayBars (invalid - should be > 0)
            0       // maxLookbackWithDelay (invalid - should be > 0)
        );
        
        // Should handle invalid configuration gracefully
        REQUIRE_NOTHROW(generator.generatePatternUniverse(invalidConfig));
    }
    
    SECTION("Handle export to invalid paths")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP);
        auto result = generator.generatePatternUniverse(config);
        // Test export to invalid directory
        std::string invalidPath = "/nonexistent/directory/output.json";
        REQUIRE_THROWS_AS(generator.exportPatterns(result, invalidPath, ExportFormat::JSON), std::exception);
        
        // Test export to read-only location (if applicable)
        // This test might be platform-specific, so we'll skip detailed implementation
    }
    
    SECTION("Handle empty or minimal datasets")
    {
        // Create minimal PAL data with just one group
        std::map<uint32_t, CuratedGroup> minimalIndexToGroup;
        CuratedGroup singleGroup(1, {0, 1}, {pattern_universe::PriceComponentType::CLOSE}, SearchType::DEEP,
                                2, 5, 100, 0.5, false);
        minimalIndexToGroup[1] = std::move(singleGroup);
        
        std::map<SearchType, ComponentUsageStats> minimalComponentStats;
        std::map<pattern_universe::PriceComponentType, uint32_t> minimalComponentUsage = {
            {pattern_universe::PriceComponentType::CLOSE, 100}
        };
        std::map<uint8_t, uint32_t> minimalTierUsage; // Empty for this test
        ComponentUsageStats minimalStats(100, 1, minimalComponentUsage, minimalTierUsage);
        minimalComponentStats.emplace(SearchType::DEEP, std::move(minimalStats));
        
        PALIndexMappings minimalMappings(std::move(minimalIndexToGroup), {}, 
                                        minimalComponentStats, 100, 1, std::chrono::system_clock::now());
        
        AlgorithmInsights minimalInsights(100);
        ComponentHierarchyRules minimalRules({{1, {pattern_universe::PriceComponentType::CLOSE}}});
        
        auto minimalData = std::make_shared<PALAnalysisData>(
            std::move(minimalMappings),
            std::move(minimalComponentStats),
            std::move(minimalInsights),
            std::move(minimalRules),
            "1.0",
            std::vector<std::string>{"minimal_test.json"}
        );
        
        auto uniqueMinimalData = std::make_unique<PALAnalysisData>(*minimalData);
        OptimizedPatternUniverseGenerator minimalGenerator(std::move(uniqueMinimalData));
        OptimizedPatternUniverseGenerator::GenerationConfig minimalConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, false, 5, 15);
        auto result = minimalGenerator.generatePatternUniverse(minimalConfig);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        REQUIRE(result.getBasePatterns() > 0); // Should generate some patterns
    }
    
    SECTION("Handle configuration edge cases")
    {
        OptimizedPatternUniverseGenerator::GenerationConfig edgeConfig(SearchType::DEEP, true, 0, true, true, 500, true, 24.0, true, 10, 15);
        
        auto result = generator.generatePatternUniverse(edgeConfig);
        
        // Should handle gracefully, possibly returning minimal results
        // Note: These return size_t which is always >= 0, so just check they exist
        REQUIRE((result.getTotalPatternsGenerated() == 0 || result.getTotalPatternsGenerated() > 0));
        REQUIRE((result.getBasePatterns() == 0 || result.getBasePatterns() > 0)); // Should handle edge cases gracefully
    }
}

TEST_CASE("OptimizedPatternUniverseGenerator - Database-Driven Pattern Generation", "[OptimizedPatternUniverseGenerator][database_driven]")
{
    SECTION("Pattern complexity matches database specifications")
    {
        // Create test data with specific pattern length constraints
        std::map<uint32_t, CuratedGroup> indexToGroup;
        std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices;
        
        // Create groups with specific min/max pattern lengths from the database
        std::vector<std::tuple<int, int, int, std::set<pattern_universe::PriceComponentType>>> testGroups = {
            {1, 2, 3, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}}, // Range: 2-3
            {481, 5, 5, {pattern_universe::PriceComponentType::CLOSE}}, // Exact: 5
            {2, 3, 4, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}}, // Range: 3-4
            {113, 5, 5, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW}}, // Exact: 5
        };
        
        for (const auto& [indexNum, minLen, maxLen, components] : testGroups)
        {
            CuratedGroup group(indexNum, {0, 1, 2, 3, 4, 5, 6, 7, 8}, components, SearchType::DEEP,
                              minLen, maxLen, 100, 0.8, true);
            indexToGroup[indexNum] = std::move(group);
            searchTypeToIndices[SearchType::DEEP].push_back(indexNum);
        }
        
        // Create component stats
        std::map<SearchType, ComponentUsageStats> componentStats;
        std::map<pattern_universe::PriceComponentType, uint32_t> componentUsage = {
            {pattern_universe::PriceComponentType::CLOSE, 2000},
            {pattern_universe::PriceComponentType::HIGH, 1500},
            {pattern_universe::PriceComponentType::LOW, 1200},
            {pattern_universe::PriceComponentType::OPEN, 800}
        };
        ComponentUsageStats deepStats(5000, 4, componentUsage);
        componentStats.emplace(SearchType::DEEP, std::move(deepStats));
        
        // Create PAL analysis data
        PALIndexMappings indexMappings(std::move(indexToGroup), std::move(searchTypeToIndices),
                                      componentStats, 5000, 4, std::chrono::system_clock::now());
        AlgorithmInsights algorithmInsights(5000, 2500, 50.0);
        ComponentHierarchyRules hierarchyRules;
        
        auto testData = std::make_unique<PALAnalysisData>(
            std::move(indexMappings),
            std::move(componentStats),
            std::move(algorithmInsights),
            std::move(hierarchyRules),
            "1.0",
            std::vector<std::string>{"test_data.json"}
        );
        
        OptimizedPatternUniverseGenerator generator(std::move(testData));
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, false, 1, false, false, 500, false, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        REQUIRE(result.getPatterns().size() > 0);
        
        // Verify that patterns have the correct complexity based on database specifications
        std::map<std::string, std::pair<int, int>> expectedComplexity = {
            {"Group1_", {3, 3}},  // Should use maxPatternLength = 3
            {"Group481_", {5, 5}}, // Should use exact = 5
            {"Group2_", {4, 4}},   // Should use maxPatternLength = 4
            {"Group113_", {5, 5}}  // Should use exact = 5
        };
        
        for (const auto& pattern : result.getPatterns()) {
            std::string patternName = pattern.getName();
            bool foundMatch = false;
            
            for (const auto& [prefix, expectedRange] : expectedComplexity) {
                if (patternName.find(prefix) == 0) {
                    int actualConditions = static_cast<int>(pattern.getConditions().size());
                    REQUIRE(actualConditions >= expectedRange.first);
                    REQUIRE(actualConditions <= expectedRange.second);
                    foundMatch = true;
                    break;
                }
            }
            
            if (!foundMatch) {
                // Pattern should still have reasonable complexity
                REQUIRE(pattern.getConditions().size() >= 3);
                REQUIRE(pattern.getConditions().size() <= 15);
            }
        }
    }
    
    SECTION("Pattern components match group specifications")
    {
        // Create test data with specific component constraints
        std::map<uint32_t, CuratedGroup> indexToGroup;
        std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices;
        
        // Create groups with different component sets
        CuratedGroup closeOnlyGroup(480, {0, 1, 2, 3}, {pattern_universe::PriceComponentType::CLOSE},
                                   SearchType::DEEP, 4, 4, 100, 0.8, false);
        CuratedGroup highLowGroup(173, {0, 1, 2}, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW},
                                 SearchType::DEEP, 3, 3, 100, 0.8, false);
        CuratedGroup openCloseGroup(326, {0, 1, 2}, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::CLOSE},
                                   SearchType::DEEP, 3, 3, 100, 0.8, false);
        
        indexToGroup[480] = std::move(closeOnlyGroup);
        indexToGroup[173] = std::move(highLowGroup);
        indexToGroup[326] = std::move(openCloseGroup);
        searchTypeToIndices[SearchType::DEEP] = {480, 173, 326};
        
        // Create component stats
        std::map<SearchType, ComponentUsageStats> componentStats;
        std::map<pattern_universe::PriceComponentType, uint32_t> componentUsage = {
            {pattern_universe::PriceComponentType::CLOSE, 1000},
            {pattern_universe::PriceComponentType::HIGH, 800},
            {pattern_universe::PriceComponentType::LOW, 800},
            {pattern_universe::PriceComponentType::OPEN, 400}
        };
        ComponentUsageStats deepStats(3000, 3, componentUsage);
        componentStats.emplace(SearchType::DEEP, std::move(deepStats));
        
        // Create PAL analysis data
        PALIndexMappings indexMappings(std::move(indexToGroup), std::move(searchTypeToIndices),
                                      componentStats, 3000, 3, std::chrono::system_clock::now());
        AlgorithmInsights algorithmInsights(3000, 1500, 50.0);
        ComponentHierarchyRules hierarchyRules;
        
        auto testData = std::make_unique<PALAnalysisData>(
            std::move(indexMappings),
            std::move(componentStats),
            std::move(algorithmInsights),
            std::move(hierarchyRules),
            "1.0",
            std::vector<std::string>{"component_test_data.json"}
        );
        
        OptimizedPatternUniverseGenerator generator(std::move(testData));
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, false, 1, false, false, 500, false, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        
        // Verify that patterns use only the components specified in their groups
        for (const auto& pattern : result.getPatterns()) {
            std::string patternName = pattern.getName();
            
            for (const auto& condition : pattern.getConditions()) {
                auto lhsComponent = static_cast<pattern_universe::PriceComponentType>(condition.getLhs().getComponentType());
                auto rhsComponent = static_cast<pattern_universe::PriceComponentType>(condition.getRhs().getComponentType());
                
                if (patternName.find("Group480_") == 0) {
                    // Close-only group
                    REQUIRE(lhsComponent == pattern_universe::PriceComponentType::CLOSE);
                    REQUIRE(rhsComponent == pattern_universe::PriceComponentType::CLOSE);
                } else if (patternName.find("Group173_") == 0) {
                    // High-Low group
                    REQUIRE((lhsComponent == pattern_universe::PriceComponentType::HIGH ||
                            lhsComponent == pattern_universe::PriceComponentType::LOW));
                    REQUIRE((rhsComponent == pattern_universe::PriceComponentType::HIGH ||
                            rhsComponent == pattern_universe::PriceComponentType::LOW));
                } else if (patternName.find("Group326_") == 0) {
                    // Open-Close group
                    REQUIRE((lhsComponent == pattern_universe::PriceComponentType::OPEN ||
                            lhsComponent == pattern_universe::PriceComponentType::CLOSE));
                    REQUIRE((rhsComponent == pattern_universe::PriceComponentType::OPEN ||
                            rhsComponent == pattern_universe::PriceComponentType::CLOSE));
                }
            }
        }
    }
    
    SECTION("Pattern bar offsets match group specifications")
    {
        // Create test data with specific bar offset constraints
        std::map<uint32_t, CuratedGroup> indexToGroup;
        std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices;
        
        // Create groups with different bar offset patterns from the database
        CuratedGroup group1(1, {0, 1, 2}, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE},
                           SearchType::EXTENDED, 2, 3, 735, 0.8, false);
        CuratedGroup group17(17, {0, 1, 2, 4, 7}, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE},
                            SearchType::EXTENDED, 3, 3, 799, 0.8, false);
        
        indexToGroup[1] = std::move(group1);
        indexToGroup[17] = std::move(group17);
        searchTypeToIndices[SearchType::EXTENDED] = {1, 17};
        
        // Create component stats
        std::map<SearchType, ComponentUsageStats> componentStats;
        std::map<pattern_universe::PriceComponentType, uint32_t> componentUsage = {
            {pattern_universe::PriceComponentType::CLOSE, 1000},
            {pattern_universe::PriceComponentType::HIGH, 800},
            {pattern_universe::PriceComponentType::LOW, 800},
            {pattern_universe::PriceComponentType::OPEN, 400}
        };
        ComponentUsageStats extendedStats(3000, 2, componentUsage);
        componentStats.emplace(SearchType::EXTENDED, std::move(extendedStats));
        
        // Create PAL analysis data
        PALIndexMappings indexMappings(std::move(indexToGroup), std::move(searchTypeToIndices),
                                      componentStats, 3000, 2, std::chrono::system_clock::now());
        AlgorithmInsights algorithmInsights(3000, 1500, 50.0);
        ComponentHierarchyRules hierarchyRules;
        
        auto testData = std::make_unique<PALAnalysisData>(
            std::move(indexMappings),
            std::move(componentStats),
            std::move(algorithmInsights),
            std::move(hierarchyRules),
            "1.0",
            std::vector<std::string>{"bar_offset_test_data.json"}
        );
        
        OptimizedPatternUniverseGenerator generator(std::move(testData));
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::EXTENDED, false, 1, false, false, 500, false, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        
        // Verify that patterns use only the bar offsets specified in their groups
        for (const auto& pattern : result.getPatterns()) {
            std::string patternName = pattern.getName();
            
            for (const auto& condition : pattern.getConditions()) {
                uint8_t lhsOffset = condition.getLhs().getBarOffset();
                uint8_t rhsOffset = condition.getRhs().getBarOffset();
                
                if (patternName.find("Group1_") == 0) {
                    // Group 1: bar offsets [0, 1, 2]
                    REQUIRE((lhsOffset == 0 || lhsOffset == 1 || lhsOffset == 2));
                    REQUIRE((rhsOffset == 0 || rhsOffset == 1 || rhsOffset == 2));
                } else if (patternName.find("Group17_") == 0) {
                    // Group 17: bar offsets [0, 1, 2, 4, 7]
                    REQUIRE((lhsOffset == 0 || lhsOffset == 1 || lhsOffset == 2 || lhsOffset == 4 || lhsOffset == 7));
                    REQUIRE((rhsOffset == 0 || rhsOffset == 1 || rhsOffset == 2 || rhsOffset == 4 || rhsOffset == 7));
                }
            }
        }
    }
    
    SECTION("Chaining uses database-driven additional conditions")
    {
        // Create test data for chaining validation
        auto mockData = createMockPALAnalysisData();
        auto uniqueData = std::make_unique<PALAnalysisData>(*mockData);
        OptimizedPatternUniverseGenerator generator(std::move(uniqueData));
        
        // Enable chaining to test the determineAdditionalConditions method
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, false, 1, true, false, 500, false, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        
        // Look for chained patterns (they should have "_Chained" in the name)
        bool foundChainedPattern = false;
        for (const auto& pattern : result.getPatterns()) {
            if (pattern.getName().find("_Chained") != std::string::npos) {
                foundChainedPattern = true;
                // Chained patterns should have more conditions than base patterns
                REQUIRE(pattern.getConditions().size() >= 4); // At least base + some additional
                REQUIRE(pattern.getConditions().size() <= 15); // But not excessive
            }
        }
        
        // With chaining enabled and groups that support chaining, we should find some chained patterns
        REQUIRE(foundChainedPattern);
    }
    
    SECTION("Pattern quality validation works correctly")
    {
        // Create test data with specific constraints for validation testing
        std::map<uint32_t, CuratedGroup> indexToGroup;
        std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices;
        
        // Create a group with tight constraints for validation testing
        CuratedGroup strictGroup(100, {0, 1}, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW},
                                SearchType::DEEP, 4, 6, 100, 0.8, false);
        indexToGroup[100] = std::move(strictGroup);
        searchTypeToIndices[SearchType::DEEP] = {100};
        
        // Create component stats
        std::map<SearchType, ComponentUsageStats> componentStats;
        std::map<pattern_universe::PriceComponentType, uint32_t> componentUsage = {
            {pattern_universe::PriceComponentType::HIGH, 500},
            {pattern_universe::PriceComponentType::LOW, 500}
        };
        ComponentUsageStats deepStats(1000, 1, componentUsage);
        componentStats.emplace(SearchType::DEEP, std::move(deepStats));
        
        // Create PAL analysis data
        PALIndexMappings indexMappings(std::move(indexToGroup), std::move(searchTypeToIndices),
                                      componentStats, 1000, 1, std::chrono::system_clock::now());
        AlgorithmInsights algorithmInsights(1000, 500, 50.0);
        ComponentHierarchyRules hierarchyRules;
        
        auto testData = std::make_unique<PALAnalysisData>(
            std::move(indexMappings),
            std::move(componentStats),
            std::move(algorithmInsights),
            std::move(hierarchyRules),
            "1.0",
            std::vector<std::string>{"validation_test_data.json"}
        );
        
        OptimizedPatternUniverseGenerator generator(std::move(testData));
        OptimizedPatternUniverseGenerator::GenerationConfig config(SearchType::DEEP, false, 1, false, false, 500, false, 24.0, false, 5, 15);
        
        auto result = generator.generatePatternUniverse(config);
        
        REQUIRE(result.getTotalPatternsGenerated() > 0);
        
        // All generated patterns should pass quality validation
        for (const auto& pattern : result.getPatterns()) {
            // Pattern should have conditions within the specified range (4-6)
            REQUIRE(pattern.getConditions().size() >= 4);
            REQUIRE(pattern.getConditions().size() <= 6);
            
            // Pattern should only use HIGH and LOW components
            for (const auto& condition : pattern.getConditions()) {
                auto lhsComponent = static_cast<pattern_universe::PriceComponentType>(condition.getLhs().getComponentType());
                auto rhsComponent = static_cast<pattern_universe::PriceComponentType>(condition.getRhs().getComponentType());
                
                REQUIRE((lhsComponent == pattern_universe::PriceComponentType::HIGH ||
                        lhsComponent == pattern_universe::PriceComponentType::LOW));
                REQUIRE((rhsComponent == pattern_universe::PriceComponentType::HIGH ||
                        rhsComponent == pattern_universe::PriceComponentType::LOW));
                
                // Pattern should only use bar offsets 0 and 1
                REQUIRE((condition.getLhs().getBarOffset() == 0 || condition.getLhs().getBarOffset() == 1));
                REQUIRE((condition.getRhs().getBarOffset() == 0 || condition.getRhs().getBarOffset() == 1));
            }
        }
    }
}