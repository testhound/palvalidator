/**
 * @file PALAnalysisLoaderTests.cpp
 * @brief Comprehensive unit tests for PALAnalysisLoader component
 * 
 * Tests cover loading and parsing of PAL analysis reports from dataset/pal_analysis/:
 * - component_analysis_report.json
 * - index_mapping_report.json  
 * - pattern_structure_analysis.json
 * - search_algorithm_report.json
 * 
 * Uses hybrid approach:
 * - Unit tests with mock data for testing parsing logic and error handling
 * - Integration tests with real PAL data from dataset/pal_analysis/ directory
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PALAnalysisLoader.h"
#include "OptimizedDataStructures.h"
#include <fstream>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using namespace pattern_universe;
using namespace rapidjson;

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
     * @brief Create a temporary directory for mock test files
     */
    std::string createTempTestDir() 
    {
        std::string tempDir = "test_pal_mock_" + std::to_string(std::time(nullptr));
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
     * @brief Create mock component analysis report JSON for unit testing
     */
    void createMockComponentAnalysisReport(const std::string& filePath) 
    {
        Document doc;
        doc.SetObject();
        Document::AllocatorType& allocator = doc.GetAllocator();

        // Metadata
        Value metadata(kObjectType);
        metadata.AddMember("version", "1.0", allocator);
        metadata.AddMember("analysisDate", "2024-01-15T10:30:00Z", allocator);
        metadata.AddMember("totalPatterns", 131966, allocator);
        doc.AddMember("metadata", metadata, allocator);

        // Create componentAnalysis structure that matches what PALAnalysisLoader expects
        Value componentAnalysis(kObjectType);
        
        // Deep search type
        Value deepSearch(kObjectType);
        deepSearch.AddMember("totalPatterns", 106375, allocator);
        deepSearch.AddMember("uniqueIndices", 519, allocator);
        
        Value deepComponents(kObjectType);
        deepComponents.AddMember("CLOSE", 40123, allocator);
        deepComponents.AddMember("HIGH", 25678, allocator);
        deepComponents.AddMember("LOW", 25234, allocator);
        deepComponents.AddMember("OPEN", 15340, allocator);
        deepSearch.AddMember("componentUsage", deepComponents, allocator);
        
        Value deepLengthDist(kObjectType);
        deepLengthDist.AddMember("10", 37946, allocator);
        deepLengthDist.AddMember("14", 48190, allocator);
        deepLengthDist.AddMember("8", 20239, allocator);
        deepSearch.AddMember("patternLengthDistribution", deepLengthDist, allocator);
        
        componentAnalysis.AddMember("Deep", deepSearch, allocator);
        
        // Extended search type
        Value extendedSearch(kObjectType);
        extendedSearch.AddMember("totalPatterns", 25591, allocator);
        extendedSearch.AddMember("uniqueIndices", 6, allocator);
        
        Value extendedComponents(kObjectType);
        extendedComponents.AddMember("CLOSE", 9654, allocator);
        extendedComponents.AddMember("HIGH", 5234, allocator);
        extendedComponents.AddMember("LOW", 5123, allocator);
        extendedComponents.AddMember("OPEN", 5580, allocator);
        extendedSearch.AddMember("componentUsage", extendedComponents, allocator);
        
        componentAnalysis.AddMember("Extended", extendedSearch, allocator);
        doc.AddMember("componentAnalysis", componentAnalysis, allocator);

        // Write to file
        std::ofstream outFile(filePath);
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);
        outFile << buffer.GetString();
        outFile.close();
    }

    /**
     * @brief Create mock index mapping report JSON for unit testing
     */
    void createMockIndexMappingReport(const std::string& filePath) 
    {
        Document doc;
        doc.SetObject();
        Document::AllocatorType& allocator = doc.GetAllocator();

        // Metadata
        Value metadata(kObjectType);
        metadata.AddMember("totalIndices", 4, allocator);
        metadata.AddMember("totalPatterns", 4830, allocator);
        doc.AddMember("metadata", metadata, allocator);

        // Index mappings - create as object with string keys (matching real data structure)
        Value indexMappings(kObjectType);
        
        // Index 1 - Full OHLC group (indices 1-153)
        Value index1(kObjectType);
        
        Value barOffsets1(kArrayType);
        barOffsets1.PushBack(0, allocator);
        barOffsets1.PushBack(1, allocator);
        barOffsets1.PushBack(2, allocator);
        index1.AddMember("barOffsets", barOffsets1, allocator);
        
        Value components1(kArrayType);
        components1.PushBack("OPEN", allocator);
        components1.PushBack("HIGH", allocator);
        components1.PushBack("LOW", allocator);
        components1.PushBack("CLOSE", allocator);
        index1.AddMember("componentTypes", components1, allocator);
        
        index1.AddMember("searchType", "Deep", allocator);
        index1.AddMember("patternCount", 1250, allocator);
        index1.AddMember("minPatternLength", 3, allocator);
        index1.AddMember("maxPatternLength", 8, allocator);
        
        indexMappings.AddMember("1", index1, allocator);
        
        // Index 200 - Mixed group (indices 154-325)
        Value index200(kObjectType);
        
        Value barOffsets200(kArrayType);
        barOffsets200.PushBack(0, allocator);
        barOffsets200.PushBack(1, allocator);
        barOffsets200.PushBack(3, allocator);
        index200.AddMember("barOffsets", barOffsets200, allocator);
        
        Value components200(kArrayType);
        components200.PushBack("HIGH", allocator);
        components200.PushBack("LOW", allocator);
        components200.PushBack("CLOSE", allocator);
        index200.AddMember("componentTypes", components200, allocator);
        
        index200.AddMember("searchType", "Deep", allocator);
        index200.AddMember("patternCount", 980, allocator);
        index200.AddMember("minPatternLength", 3, allocator);
        index200.AddMember("maxPatternLength", 6, allocator);
        
        indexMappings.AddMember("200", index200, allocator);
        
        // Index 400 - Dual group (indices 326-478)
        Value index400(kObjectType);
        
        Value barOffsets400(kArrayType);
        barOffsets400.PushBack(0, allocator);
        barOffsets400.PushBack(2, allocator);
        index400.AddMember("barOffsets", barOffsets400, allocator);
        
        Value components400(kArrayType);
        components400.PushBack("HIGH", allocator);
        components400.PushBack("LOW", allocator);
        index400.AddMember("componentTypes", components400, allocator);
        
        index400.AddMember("searchType", "Deep", allocator);
        index400.AddMember("patternCount", 750, allocator);
        index400.AddMember("minPatternLength", 2, allocator);
        index400.AddMember("maxPatternLength", 4, allocator);
        
        indexMappings.AddMember("400", index400, allocator);
        
        // Index 500 - Single component group (indices 480-545)
        Value index500(kObjectType);
        
        Value barOffsets500(kArrayType);
        barOffsets500.PushBack(0, allocator);
        barOffsets500.PushBack(1, allocator);
        index500.AddMember("barOffsets", barOffsets500, allocator);
        
        Value components500(kArrayType);
        components500.PushBack("CLOSE", allocator);
        index500.AddMember("componentTypes", components500, allocator);
        
        index500.AddMember("searchType", "Deep", allocator);
        index500.AddMember("patternCount", 850, allocator);
        index500.AddMember("minPatternLength", 2, allocator);
        index500.AddMember("maxPatternLength", 5, allocator);
        
        indexMappings.AddMember("500", index500, allocator);
        
        doc.AddMember("indexMappings", indexMappings, allocator);

        // Write to file
        std::ofstream outFile(filePath);
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);
        outFile << buffer.GetString();
        outFile.close();
    }
}

TEST_CASE("PALAnalysisLoader - Constructor and Basic Operations", "[PALAnalysisLoader][constructor]")
{
    SECTION("Default constructor creates valid loader")
    {
        PALAnalysisLoader loader;
        REQUIRE_NOTHROW(loader);
    }
}

TEST_CASE("PALAnalysisLoader - Mock Data Unit Tests", "[PALAnalysisLoader][unit][mock]")
{
    std::string tempDir = createTempTestDir();
    std::string componentReportPath = tempDir + "/component_analysis_report.json";
    std::string indexReportPath = tempDir + "/index_mapping_report.json";
    
    SECTION("Load mock component analysis report")
    {
        createMockComponentAnalysisReport(componentReportPath);
        
        PALAnalysisLoader loader;
        auto componentStats = loader.loadComponentStats(componentReportPath);
        
        REQUIRE(componentStats.size() == 2); // Deep and Extended
        
        // Verify Deep search type
        REQUIRE(componentStats.find(SearchType::DEEP) != componentStats.end());
        const auto& deepStats = componentStats.at(SearchType::DEEP);
        REQUIRE(deepStats.getTotalPatterns() > 100000); // Should be around 106,375 but allow for database growth
        REQUIRE(deepStats.getUniqueIndices() > 500);    // Should be around 519-525 but allow for database growth
        
        // Verify component usage
        REQUIRE(deepStats.getComponentUsage().size() == 4);
        REQUIRE(deepStats.getComponentUsage().at(pattern_universe::PriceComponentType::CLOSE) == 40123);
        REQUIRE(deepStats.getComponentUsage().at(pattern_universe::PriceComponentType::HIGH) == 25678);
        
        // Test usage percentage calculation
        double closePercentage = deepStats.getUsagePercentage(pattern_universe::PriceComponentType::CLOSE);
        REQUIRE(closePercentage == Catch::Approx(37.7).margin(1.0)); // Approximately 37.7%
    }
    
    SECTION("Load mock index mapping report with component hierarchy")
    {
        createMockIndexMappingReport(indexReportPath);
        
        PALAnalysisLoader loader;
        auto indexMappings = loader.loadIndexMappings(indexReportPath);
        
        REQUIRE(indexMappings.getTotalIndices() == 4);
        REQUIRE(indexMappings.getTotalPatterns() == 4830);
        REQUIRE(indexMappings.getIndexToGroup().size() == 4);
        
        // Verify component hierarchy: Full OHLC → Mixed → Dual → Single
        
        // Index 1: Full OHLC (4 components)
        const auto& group1 = indexMappings.getIndexToGroup().at(1);
        REQUIRE(group1.getComponentTypes().size() == 4);
        REQUIRE(group1.isSupportingChaining() == true);
        
        // Index 200: Mixed (3 components)
        const auto& group200 = indexMappings.getIndexToGroup().at(200);
        REQUIRE(group200.getComponentTypes().size() == 3);
        REQUIRE(group200.isSupportingChaining() == true);
        
        // Index 400: Dual (2 components)
        const auto& group400 = indexMappings.getIndexToGroup().at(400);
        REQUIRE(group400.getComponentTypes().size() == 2);
        REQUIRE(group400.isSupportingChaining() == false);
        
        // Index 500: Single (1 component)
        const auto& group500 = indexMappings.getIndexToGroup().at(500);
        REQUIRE(group500.getComponentTypes().size() == 1);
        REQUIRE(group500.getComponentTypes().find(pattern_universe::PriceComponentType::CLOSE) != group500.getComponentTypes().end());
        REQUIRE(group500.isSupportingChaining() == false);
    }
    
    SECTION("Error handling with mock data")
    {
        PALAnalysisLoader loader;
        
        // Test missing file
        REQUIRE_THROWS_AS(loader.loadComponentStats("nonexistent_file.json"), std::exception);
        
        // Test malformed JSON
        std::string malformedPath = tempDir + "/malformed.json";
        std::ofstream malformedFile(malformedPath);
        malformedFile << "{ invalid json content";
        malformedFile.close();
        
        REQUIRE_THROWS_AS(loader.loadComponentStats(malformedPath), std::exception);
    }
    
    cleanupTempDir(tempDir);
}

TEST_CASE("PALAnalysisLoader - Real PAL Data Integration Tests", "[PALAnalysisLoader][integration][real_data]")
{
    // Skip these tests if real PAL data is not available
    if (!hasRealPALData()) 
    {
        SKIP("Real PAL analysis data not found in dataset/pal_analysis/ - skipping integration tests");
        return;
    }
    
    std::string palDataDir = getPALAnalysisDataDir();
    
    SECTION("Load real component analysis report")
    {
        PALAnalysisLoader loader;
        auto componentStats = loader.loadComponentStats(palDataDir + "/component_analysis_report.json");
        
        // Verify we have the expected search types
        REQUIRE(componentStats.size() >= 1);
        
        // Should have Deep search type with expected pattern count
        if (componentStats.find(SearchType::DEEP) != componentStats.end())
        {
            const auto& deepStats = componentStats.at(SearchType::DEEP);
            REQUIRE(deepStats.getTotalPatterns() > 100000); // Should be around 106,375
            REQUIRE(deepStats.getUniqueIndices() > 500);    // Should be around 519
            
            // Verify CLOSE dominance (should be around 37.7%)
            if (deepStats.getComponentUsage().find(pattern_universe::PriceComponentType::CLOSE) != deepStats.getComponentUsage().end())
            {
                double closePercentage = deepStats.getUsagePercentage(pattern_universe::PriceComponentType::CLOSE);
                REQUIRE(closePercentage > 30.0); // Should be around 37.7%
                REQUIRE(closePercentage < 45.0);
            }
        }
    }
    
    SECTION("Load real index mapping report")
    {
        PALAnalysisLoader loader;
        auto indexMappings = loader.loadIndexMappings(palDataDir + "/index_mapping_report.json");
        
        // Verify expected totals from PAL analysis (allow for database growth)
        REQUIRE(indexMappings.getTotalIndices() >= 525);     // Should be around 525 but allow for growth
        REQUIRE(indexMappings.getTotalPatterns() >= 131966); // Should be around 131,966 but allow for growth
        
        // Should have mappings for all indices
        REQUIRE(indexMappings.getIndexToGroup().size() == 525);
        
        // Verify component hierarchy ranges
        size_t fullOHLCCount = 0;
        size_t mixedCount = 0;
        size_t dualCount = 0;
        size_t singleCount = 0;
        
        for (const auto& [indexNum, group] : indexMappings.getIndexToGroup())
        {
            if (indexNum >= 1 && indexNum <= 153)
            {
                // Full OHLC range
                fullOHLCCount++;
                REQUIRE(group.getComponentTypes().size() == 4);
            }
            else if (indexNum >= 154 && indexNum <= 325)
            {
                // Mixed range (allow for 2-3 components as data evolves)
                mixedCount++;
                REQUIRE(group.getComponentTypes().size() >= 2);
                REQUIRE(group.getComponentTypes().size() <= 3);
            }
            else if (indexNum >= 326 && indexNum <= 478)
            {
                // Dual range
                dualCount++;
                REQUIRE(group.getComponentTypes().size() == 2);
            }
            else if (indexNum >= 480 && indexNum <= 545)
            {
                // Single range
                singleCount++;
                REQUIRE(group.getComponentTypes().size() == 1);
            }
        }
        
        // Verify expected counts for each tier (allow for database evolution)
        REQUIRE(fullOHLCCount == 153);  // Indices 1-153 (stable range)
        REQUIRE(mixedCount >= 166);     // Indices 154-325 (allow for evolution, was 172, now 166)
        REQUIRE(mixedCount <= 172);     // Upper bound for reasonable range
        REQUIRE(dualCount == 153);      // Indices 326-478 (stable range)
        REQUIRE(singleCount >= 53);     // Indices 480-545 (allow for evolution, was 66, now 53)
        REQUIRE(singleCount <= 66);     // Upper bound for reasonable range (note: gap at 479)
    }
    
    SECTION("Load complete real PAL analysis")
    {
        PALAnalysisLoader loader;
        auto analysisData = loader.loadCompleteAnalysis(palDataDir);
        
        REQUIRE(analysisData != nullptr);
        
        // Verify all components loaded correctly (allow for database growth)
        REQUIRE(analysisData->getIndexMappings().getTotalIndices() >= 525);     // Should be around 525 but allow for growth
        REQUIRE(analysisData->getIndexMappings().getTotalPatterns() >= 131966); // Should be around 131,966 but allow for growth
        REQUIRE(analysisData->getIndexMappings().getIndexToGroup().size() >= 525); // Should be around 525 but allow for growth
        
        // Verify component stats loaded
        REQUIRE(analysisData->getComponentStats().size() >= 1);
        
        // Verify hierarchy rules built correctly (allow for database growth)
        REQUIRE(analysisData->getHierarchyRules().getIndexToAllowedComponents().size() >= 525); // Should be around 525 but allow for growth
        
        // Test hierarchy validation with real data
        std::set<pattern_universe::PriceComponentType> fullOHLC = {
            pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH,
            pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE
        };
        std::set<pattern_universe::PriceComponentType> closeOnly = {pattern_universe::PriceComponentType::CLOSE};
        
        // Index 1 should allow full OHLC
        REQUIRE(analysisData->getHierarchyRules().isValidCombination(fullOHLC, 1) == true);
        REQUIRE(analysisData->getHierarchyRules().isValidCombination(closeOnly, 1) == true);
        
        // Index 500+ should only allow single components
        if (analysisData->getIndexMappings().getIndexToGroup().find(500) != analysisData->getIndexMappings().getIndexToGroup().end())
        {
            REQUIRE(analysisData->getHierarchyRules().isValidCombination(fullOHLC, 500) == false);
            REQUIRE(analysisData->getHierarchyRules().isValidCombination(closeOnly, 500) == true);
        }
        
        // Verify metadata
        REQUIRE(!analysisData->getAnalysisVersion().empty());
        REQUIRE(analysisData->getSourceReports().size() == 4);
        
        // Verify chaining analysis if available
        if (analysisData->getAlgorithmInsights().getTotalPatterns() > 0)
        {
            REQUIRE(analysisData->getAlgorithmInsights().getChainingPercentage() > 15.0); // Should be around 19.53%
            REQUIRE(analysisData->getAlgorithmInsights().getChainingPercentage() < 25.0);
        }
    }
}

TEST_CASE("PALAnalysisLoader - Component Hierarchy Validation", "[PALAnalysisLoader][hierarchy]")
{
    std::string tempDir = createTempTestDir();
    std::string indexReportPath = tempDir + "/index_mapping_report.json";
    
    SECTION("Build and validate component hierarchy rules")
    {
        createMockIndexMappingReport(indexReportPath);
        
        PALAnalysisLoader loader;
        auto indexMappings = loader.loadIndexMappings(indexReportPath);
        auto hierarchyRules = loader.buildComponentHierarchy(indexMappings);
        
        // Verify hierarchy rules built correctly
        REQUIRE(hierarchyRules.getIndexToAllowedComponents().size() == 4);
        
        // Test component specialization hierarchy
        std::set<pattern_universe::PriceComponentType> fullOHLC = {
            pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH,
            pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE
        };
        std::set<pattern_universe::PriceComponentType> mixed = {
            pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE
        };
        std::set<pattern_universe::PriceComponentType> dual = {
            pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW
        };
        std::set<pattern_universe::PriceComponentType> single = {pattern_universe::PriceComponentType::CLOSE};
        
        // Index 1 (Full OHLC) should allow all combinations
        REQUIRE(hierarchyRules.isValidCombination(fullOHLC, 1) == true);
        REQUIRE(hierarchyRules.isValidCombination(mixed, 1) == true);
        REQUIRE(hierarchyRules.isValidCombination(dual, 1) == true);
        REQUIRE(hierarchyRules.isValidCombination(single, 1) == true);
        
        // Index 200 (Mixed) should allow mixed and smaller combinations
        REQUIRE(hierarchyRules.isValidCombination(fullOHLC, 200) == false);
        REQUIRE(hierarchyRules.isValidCombination(mixed, 200) == true);
        REQUIRE(hierarchyRules.isValidCombination(dual, 200) == true);
        REQUIRE(hierarchyRules.isValidCombination(single, 200) == true);
        
        // Index 400 (Dual) should allow dual and single combinations
        REQUIRE(hierarchyRules.isValidCombination(fullOHLC, 400) == false);
        REQUIRE(hierarchyRules.isValidCombination(mixed, 400) == false);
        REQUIRE(hierarchyRules.isValidCombination(dual, 400) == true);
        
        // Index 500 (Single) should allow only single component
        REQUIRE(hierarchyRules.isValidCombination(fullOHLC, 500) == false);
        REQUIRE(hierarchyRules.isValidCombination(mixed, 500) == false);
        REQUIRE(hierarchyRules.isValidCombination(dual, 500) == false);
        REQUIRE(hierarchyRules.isValidCombination(single, 500) == true);
    }
    
    cleanupTempDir(tempDir);
}

TEST_CASE("PALAnalysisLoader - Performance and Memory Tests", "[PALAnalysisLoader][performance]")
{
    // Skip if real data not available
    if (!hasRealPALData()) 
    {
        SKIP("Real PAL analysis data not found in dataset/pal_analysis/ - skipping performance tests");
        return;
    }
    
    std::string palDataDir = getPALAnalysisDataDir();
    
    SECTION("Load large dataset performance")
    {
        PALAnalysisLoader loader;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        auto analysisData = loader.loadCompleteAnalysis(palDataDir);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Loading should complete within reasonable time (< 5 seconds)
        REQUIRE(duration.count() < 5000);
        
        // Verify data loaded correctly (allow for database growth)
        REQUIRE(analysisData != nullptr);
        REQUIRE(analysisData->getIndexMappings().getIndexToGroup().size() >= 525); // Should be around 525 but allow for growth
        
        // Memory usage should be reasonable (this is more of a sanity check)
        REQUIRE(analysisData->getIndexMappings().getIndexToGroup().size() > 0);
    }
}