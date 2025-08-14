/**
 * @file OptimizedDataStructuresTests.cpp
 * @brief Comprehensive unit tests for OptimizedDataStructures component
 * 
 * Tests cover data structure integrity, serialization compatibility, performance
 * characteristics, and validation of all data structures used in the pattern universe generator.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "OptimizedDataStructures.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <sstream>
#include <chrono>

using namespace pattern_universe;
using namespace rapidjson;

namespace 
{
    /**
     * @brief Create a sample CuratedGroup for testing
     */
    CuratedGroup createSampleCuratedGroup(int indexNumber = 1) 
    {
        std::set<pattern_universe::PriceComponentType> components = {
            pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH,
            pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE
        };
        
        CuratedGroup group(indexNumber, {0, 1, 2}, components, SearchType::DEEP,
                          3, 8, 1000, 0.8, true);
        return group;
    }

    /**
     * @brief Create a sample ComponentUsageStats for testing
     */
    ComponentUsageStats createSampleComponentStats() 
    {
        std::map<pattern_universe::PriceComponentType, uint32_t> usage = {
            {pattern_universe::PriceComponentType::CLOSE, 4000},
            {pattern_universe::PriceComponentType::HIGH, 3000},
            {pattern_universe::PriceComponentType::LOW, 2500},
            {pattern_universe::PriceComponentType::OPEN, 500}
        };
        
        ComponentUsageStats stats(10000, 100, usage);
        return stats;
    }

    /**
     * @brief Create a sample PatternUniverseResult for testing
     */
    PatternUniverseResult createSamplePatternUniverseResult() 
    {
        std::vector<PatternTemplate> patterns; // Empty for testing
        GenerationStatistics stats(std::chrono::duration<double>(1.5), 3600, 2400.0, 24.0, 4, true, true);
        std::map<int, size_t> delayDist = {{1, 200}, {2, 250}, {3, 150}};
        
        PatternUniverseResult result(std::move(patterns), 3600, std::chrono::duration<double>(1.5),
                                   2400.0, 24.0, std::chrono::system_clock::now(), "1.0.0",
                                   std::move(stats), 3000, 600, std::move(delayDist));
        return result;
    }
}

TEST_CASE("PriceComponentType - Enum Operations", "[OptimizedDataStructures][PriceComponentType]")
{
    SECTION("Enum values are correctly defined")
    {
        REQUIRE(static_cast<int>(pattern_universe::PriceComponentType::OPEN) == 0);
        REQUIRE(static_cast<int>(pattern_universe::PriceComponentType::HIGH) == 1);
        REQUIRE(static_cast<int>(pattern_universe::PriceComponentType::LOW) == 2);
        REQUIRE(static_cast<int>(pattern_universe::PriceComponentType::CLOSE) == 3);
    }
    
    SECTION("String conversion functions")
    {
        REQUIRE(componentTypeToString(pattern_universe::PriceComponentType::OPEN) == "OPEN");
        REQUIRE(componentTypeToString(pattern_universe::PriceComponentType::HIGH) == "HIGH");
        REQUIRE(componentTypeToString(pattern_universe::PriceComponentType::LOW) == "LOW");
        REQUIRE(componentTypeToString(pattern_universe::PriceComponentType::CLOSE) == "CLOSE");

        REQUIRE(stringToComponentType("OPEN") == pattern_universe::PriceComponentType::OPEN);
        REQUIRE(stringToComponentType("HIGH") == pattern_universe::PriceComponentType::HIGH);
        REQUIRE(stringToComponentType("LOW") == pattern_universe::PriceComponentType::LOW);
        REQUIRE(stringToComponentType("CLOSE") == pattern_universe::PriceComponentType::CLOSE);
    }
    
    SECTION("Invalid string conversion throws exception")
    {
        REQUIRE_THROWS_AS(stringToComponentType("INVALID"), std::invalid_argument);
        REQUIRE_THROWS_AS(stringToComponentType(""), std::invalid_argument);
        REQUIRE_THROWS_AS(stringToComponentType("open"), std::invalid_argument); // Case sensitive
    }
}

TEST_CASE("SearchType - Enum Operations", "[OptimizedDataStructures][SearchType]")
{
    SECTION("Enum values are correctly defined")
    {
        REQUIRE(static_cast<int>(SearchType::EXTENDED) == 0);
        REQUIRE(static_cast<int>(SearchType::DEEP) == 1);
        REQUIRE(static_cast<int>(SearchType::CLOSE_ONLY) == 2);
    }
    
    SECTION("String conversion functions")
    {
        REQUIRE(searchTypeToString(SearchType::DEEP) == "DEEP");
        REQUIRE(searchTypeToString(SearchType::EXTENDED) == "EXTENDED");
        REQUIRE(searchTypeToString(SearchType::CLOSE_ONLY) == "CLOSE_ONLY");
        
        REQUIRE(stringToSearchType("DEEP") == SearchType::DEEP);
        REQUIRE(stringToSearchType("EXTENDED") == SearchType::EXTENDED);
        REQUIRE(stringToSearchType("CLOSE_ONLY") == SearchType::CLOSE_ONLY);
    }
    
    SECTION("Invalid string conversion throws exception")
    {
        REQUIRE_THROWS_AS(stringToSearchType("INVALID"), std::invalid_argument);
        REQUIRE_THROWS_AS(stringToSearchType("deep"), std::invalid_argument); // Case sensitive
    }
}

TEST_CASE("ComponentComplexity - Enum Operations", "[OptimizedDataStructures][ComponentComplexity]")
{
    SECTION("Enum values are correctly defined")
    {
        REQUIRE(static_cast<int>(ComponentComplexity::Simple) == 0);
        REQUIRE(static_cast<int>(ComponentComplexity::Moderate) == 1);
        REQUIRE(static_cast<int>(ComponentComplexity::Complex) == 2);
        REQUIRE(static_cast<int>(ComponentComplexity::Full) == 3);
    }
    
    SECTION("String conversion functions")
    {
        REQUIRE(componentComplexityToString(ComponentComplexity::Simple) == "Simple");
        REQUIRE(componentComplexityToString(ComponentComplexity::Moderate) == "Moderate");
        REQUIRE(componentComplexityToString(ComponentComplexity::Complex) == "Complex");
        REQUIRE(componentComplexityToString(ComponentComplexity::Full) == "Full");
        
        REQUIRE(stringToComponentComplexity("Simple") == ComponentComplexity::Simple);
        REQUIRE(stringToComponentComplexity("Moderate") == ComponentComplexity::Moderate);
        REQUIRE(stringToComponentComplexity("Complex") == ComponentComplexity::Complex);
        REQUIRE(stringToComponentComplexity("Full") == ComponentComplexity::Full);
    }
}

TEST_CASE("CuratedGroup - Structure and Operations", "[OptimizedDataStructures][CuratedGroup]")
{
    SECTION("Default constructor creates valid structure")
    {
        CuratedGroup group;
        REQUIRE(group.getIndexNumber() == 0);
        REQUIRE(group.getComponentTypes().empty());
        REQUIRE(group.getPatternCount() == 0);
        REQUIRE(group.getMinPatternLength() == 0);
        REQUIRE(group.getMaxPatternLength() == 0);
        REQUIRE(group.isSupportingChaining() == false);
        REQUIRE(group.getBarOffsets().empty());
    }
    
    SECTION("Constructor with parameters creates valid structure")
    {
        auto group = createSampleCuratedGroup(123);
        
        REQUIRE(group.getIndexNumber() == 123);
        REQUIRE(group.getComponentTypes().size() == 4);
        REQUIRE(group.getPatternCount() == 1000);
        REQUIRE(group.isSupportingChaining() == true);
        REQUIRE(group.getSearchType() == SearchType::DEEP);
        REQUIRE(group.getMinPatternLength() == 3);
        REQUIRE(group.getMaxPatternLength() == 8);
        REQUIRE(group.getBarOffsets().size() == 3);
    }
    
    SECTION("Component type operations")
    {
        auto group = createSampleCuratedGroup();
        
        // Verify all OHLC components are present
        const auto& components = group.getComponentTypes();
        REQUIRE(components.find(pattern_universe::PriceComponentType::OPEN) != components.end());
        REQUIRE(components.find(pattern_universe::PriceComponentType::HIGH) != components.end());
        REQUIRE(components.find(pattern_universe::PriceComponentType::LOW) != components.end());
        REQUIRE(components.find(pattern_universe::PriceComponentType::CLOSE) != components.end());
    }
    
    SECTION("Bar offset operations")
    {
        auto group = createSampleCuratedGroup();
        
        const auto& offsets = group.getBarOffsets();
        REQUIRE(offsets.size() == 3);
        REQUIRE(offsets[0] == 0);
        REQUIRE(offsets[1] == 1);
        REQUIRE(offsets[2] == 2);
    }
}

TEST_CASE("ComponentUsageStats - Structure and Operations", "[OptimizedDataStructures][ComponentUsageStats]")
{
    SECTION("Constructor creates valid structure")
    {
        auto stats = createSampleComponentStats();
        
        REQUIRE(stats.getTotalPatterns() == 10000);
        REQUIRE(stats.getUniqueIndices() == 100);
        REQUIRE(stats.getComponentUsage().size() == 4);
        
        // Verify component usage values
        const auto& usage = stats.getComponentUsage();
        REQUIRE(usage.at(pattern_universe::PriceComponentType::CLOSE) == 4000);
        REQUIRE(usage.at(pattern_universe::PriceComponentType::HIGH) == 3000);
        REQUIRE(usage.at(pattern_universe::PriceComponentType::LOW) == 2500);
        REQUIRE(usage.at(pattern_universe::PriceComponentType::OPEN) == 500);
    }
    
    SECTION("Usage percentage calculation")
    {
        auto stats = createSampleComponentStats();
        
        // Test getUsagePercentage method
        double closePercentage = stats.getUsagePercentage(pattern_universe::PriceComponentType::CLOSE);
        REQUIRE(closePercentage == Catch::Approx(40.0).margin(0.1)); // 4000/10000 = 40%
        
        double highPercentage = stats.getUsagePercentage(pattern_universe::PriceComponentType::HIGH);
        REQUIRE(highPercentage == Catch::Approx(30.0).margin(0.1)); // 3000/10000 = 30%
        
        double openPercentage = stats.getUsagePercentage(pattern_universe::PriceComponentType::OPEN);
        REQUIRE(openPercentage == Catch::Approx(5.0).margin(0.1)); // 500/10000 = 5%
    }
    
    SECTION("Handle missing component")
    {
        std::map<pattern_universe::PriceComponentType, uint32_t> usage = {
            {pattern_universe::PriceComponentType::CLOSE, 4000},
            {pattern_universe::PriceComponentType::HIGH, 3000},
            {pattern_universe::PriceComponentType::LOW, 2500}
            // OPEN is missing
        };
        
        ComponentUsageStats stats(10000, 100, usage);
        double openPercentage = stats.getUsagePercentage(pattern_universe::PriceComponentType::OPEN);
        REQUIRE(openPercentage == 0.0);
    }
}

TEST_CASE("PALIndexMappings - Structure and Operations", "[OptimizedDataStructures][PALIndexMappings]")
{
    SECTION("Default constructor creates valid structure")
    {
        PALIndexMappings mappings;
        REQUIRE(mappings.getTotalIndices() == 0);
        REQUIRE(mappings.getTotalPatterns() == 0);
        REQUIRE(mappings.getIndexToGroup().empty());
    }
    
    SECTION("Add and retrieve index groups")
    {
        PALIndexMappings mappings;
        
        // Add groups
        for (int i = 1; i <= 3; ++i) 
        {
            mappings.addGroup(i, createSampleCuratedGroup(i));
        }
        
        REQUIRE(mappings.getIndexToGroup().size() == 3);
        
        // Verify retrieval
        const auto& indexToGroup = mappings.getIndexToGroup();
        REQUIRE(indexToGroup.at(1).getIndexNumber() == 1);
        REQUIRE(indexToGroup.at(2).getIndexNumber() == 2);
        REQUIRE(indexToGroup.at(3).getIndexNumber() == 3);
        
        // Verify pattern counts
        REQUIRE(indexToGroup.at(1).getPatternCount() == 1000);
        REQUIRE(indexToGroup.at(2).getPatternCount() == 1000);
        REQUIRE(indexToGroup.at(3).getPatternCount() == 1000);
    }
    
    SECTION("Index lookup operations")
    {
        PALIndexMappings mappings;
        mappings.addGroup(100, createSampleCuratedGroup(100));
        mappings.addGroup(200, createSampleCuratedGroup(200));
        
        const auto& indexToGroup = mappings.getIndexToGroup();
        
        // Test existence
        REQUIRE(indexToGroup.find(100) != indexToGroup.end());
        REQUIRE(indexToGroup.find(200) != indexToGroup.end());
        REQUIRE(indexToGroup.find(300) == indexToGroup.end());
        
        // Test retrieval
        const auto& group100 = indexToGroup.at(100);
        REQUIRE(group100.getIndexNumber() == 100);
        REQUIRE(group100.getPatternCount() == 1000);
    }
}

TEST_CASE("ComponentHierarchyRules - Structure and Operations", "[OptimizedDataStructures][ComponentHierarchyRules]")
{
    SECTION("Default constructor creates valid structure")
    {
        ComponentHierarchyRules rules;
        REQUIRE(rules.getIndexToAllowedComponents().empty());
    }
    
    SECTION("Add and validate component rules")
    {
        std::map<uint32_t, std::set<pattern_universe::PriceComponentType>> indexToComponents = {
            {1, {pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}}, // Full OHLC
            {2, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE}}, // Mixed
            {3, {pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW}}, // Dual
            {4, {pattern_universe::PriceComponentType::CLOSE}} // Single
        };
        
        ComponentHierarchyRules rules(indexToComponents);
        
        REQUIRE(rules.getIndexToAllowedComponents().size() == 4);
        
        // Test validation
        std::set<pattern_universe::PriceComponentType> fullOHLC = {
            pattern_universe::PriceComponentType::OPEN, pattern_universe::PriceComponentType::HIGH,
            pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE
        };
        std::set<pattern_universe::PriceComponentType> closeOnly = {pattern_universe::PriceComponentType::CLOSE};
        
        REQUIRE(rules.isValidCombination(fullOHLC, 1) == true);  // Full OHLC allows full OHLC
        REQUIRE(rules.isValidCombination(closeOnly, 1) == true); // Full OHLC allows subset
        REQUIRE(rules.isValidCombination(fullOHLC, 4) == false); // Single doesn't allow full OHLC
        REQUIRE(rules.isValidCombination(closeOnly, 4) == true);  // Single allows close only
    }
    
    SECTION("Handle missing index rules")
    {
        ComponentHierarchyRules rules;
        
        std::set<pattern_universe::PriceComponentType> anyComponents = {pattern_universe::PriceComponentType::CLOSE};
        
        // Should return false for missing index
        REQUIRE(rules.isValidCombination(anyComponents, 999) == false);
    }
}

TEST_CASE("PatternUniverseResult - Structure and Operations", "[OptimizedDataStructures][PatternUniverseResult]")
{
    SECTION("Constructor creates valid structure")
    {
        auto result = createSamplePatternUniverseResult();
        
        REQUIRE(result.getTotalPatternsGenerated() == 3600);
        REQUIRE(result.getBasePatterns() == 3000);
        REQUIRE(result.getDelayPatterns() == 600);
        REQUIRE(result.getTotalGenerationTime().count() == Catch::Approx(1.5).margin(0.1));
        REQUIRE(result.getDelayDistribution().size() == 3);
        
        // Verify delay distribution
        const auto& delayDist = result.getDelayDistribution();
        REQUIRE(delayDist.at(1) == 200);
        REQUIRE(delayDist.at(2) == 250);
        REQUIRE(delayDist.at(3) == 150);
        
        // Verify total delay patterns match distribution
        size_t totalFromDistribution = 0;
        for (const auto& [delay, count] : delayDist) 
        {
            totalFromDistribution += count;
        }
        REQUIRE(totalFromDistribution == result.getDelayPatterns());
    }
    
    SECTION("Pattern consistency")
    {
        auto result = createSamplePatternUniverseResult();
        
        // Verify total patterns equals base + delay
        REQUIRE(result.getTotalPatternsGenerated() == result.getBasePatterns() + result.getDelayPatterns());
        
        // Verify patterns vector
        REQUIRE(result.getPatterns().size() == 0); // Empty for testing
    }
}

TEST_CASE("PerformanceEstimate - Structure and Operations", "[OptimizedDataStructures][PerformanceEstimate]")
{
    SECTION("Constructor creates valid structure")
    {
        std::vector<std::string> recommendations = {"Use parallel processing", "Enable pre-computation"};
        PerformanceEstimate estimate(50000, std::chrono::duration<double>(5.0), 24.0, 8, 256, recommendations);
        
        REQUIRE(estimate.getEstimatedPatterns() == 50000);
        REQUIRE(estimate.getEstimatedTime().count() == Catch::Approx(5.0).margin(0.1));
        REQUIRE(estimate.getEstimatedSpeedup() == Catch::Approx(24.0).margin(0.1));
        REQUIRE(estimate.getRecommendedThreads() == 8);
        REQUIRE(estimate.getEstimatedMemoryUsageMB() == 256);
        REQUIRE(estimate.getOptimizationRecommendations().size() == 2);
    }
    
    SECTION("Performance metrics validation")
    {
        std::vector<std::string> recommendations;
        PerformanceEstimate estimate(0, std::chrono::duration<double>(0.0), 1.0, 1, 0, recommendations);
        
        REQUIRE(estimate.getEstimatedPatterns() == 0);
        REQUIRE(estimate.getEstimatedTime().count() == 0.0);
        REQUIRE(estimate.getEstimatedSpeedup() == Catch::Approx(1.0).margin(0.01));
        REQUIRE(estimate.getRecommendedThreads() == 1);
        REQUIRE(estimate.getEstimatedMemoryUsageMB() == 0);
        REQUIRE(estimate.getOptimizationRecommendations().empty());
    }
}

TEST_CASE("Data Structure Serialization", "[OptimizedDataStructures][serialization]")
{
    SECTION("Component type set serialization")
    {
        std::set<pattern_universe::PriceComponentType> components = {
            pattern_universe::PriceComponentType::HIGH, pattern_universe::PriceComponentType::LOW, pattern_universe::PriceComponentType::CLOSE
        };
        
        // Convert to string representation
        std::ostringstream oss;
        bool first = true;
        for (const auto& component : components) 
        {
            if (!first) oss << ",";
            oss << componentTypeToString(component);
            first = false;
        }
        
        std::string serialized = oss.str();
        REQUIRE(serialized.find("HIGH") != std::string::npos);
        REQUIRE(serialized.find("LOW") != std::string::npos);
        REQUIRE(serialized.find("CLOSE") != std::string::npos);
    }
    
    SECTION("Delay distribution serialization")
    {
        std::map<int, size_t> delayDist = {{1, 100}, {2, 150}, {3, 75}};
        
        // Verify map ordering (should be sorted by key)
        auto it = delayDist.begin();
        REQUIRE(it->first == 1);
        ++it;
        REQUIRE(it->first == 2);
        ++it;
        REQUIRE(it->first == 3);
    }
}

TEST_CASE("Data Structure Performance", "[OptimizedDataStructures][performance]")
{
    SECTION("Large component type set operations")
    {
        std::set<pattern_universe::PriceComponentType> components;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Add all component types multiple times
        for (int i = 0; i < 10000; ++i) 
        {
            components.insert(pattern_universe::PriceComponentType::OPEN);
            components.insert(pattern_universe::PriceComponentType::HIGH);
            components.insert(pattern_universe::PriceComponentType::LOW);
            components.insert(pattern_universe::PriceComponentType::CLOSE);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Should still only have 4 unique elements
        REQUIRE(components.size() == 4);
        
        // Should complete quickly (less than 10ms)
        REQUIRE(duration.count() < 10000);
    }
    
    SECTION("Large index mapping operations")
    {
        PALIndexMappings mappings;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Add many index groups
        for (int i = 1; i <= 1000; ++i) 
        {
            mappings.addGroup(i, createSampleCuratedGroup(i));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        REQUIRE(mappings.getIndexToGroup().size() == 1000);
        
        // Should complete quickly (less than 100ms)
        REQUIRE(duration.count() < 100);
        
        // Test lookup performance
        start = std::chrono::high_resolution_clock::now();
        
        const auto& indexToGroup = mappings.getIndexToGroup();
        for (int i = 1; i <= 1000; ++i) 
        {
            auto it = indexToGroup.find(i);
            REQUIRE(it != indexToGroup.end());
            REQUIRE(it->second.getIndexNumber() == static_cast<uint32_t>(i));
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Lookups should be very fast (less than 10ms)
        REQUIRE(duration.count() < 10);
    }
}

TEST_CASE("Data Structure Memory Usage", "[OptimizedDataStructures][memory]")
{
    SECTION("Component type set memory efficiency")
    {
        // Test that sets don't grow unnecessarily
        std::set<pattern_universe::PriceComponentType> components;
        
        size_t initialSize = components.size();
        
        // Add duplicates
        for (int i = 0; i < 100; ++i) 
        {
            components.insert(pattern_universe::PriceComponentType::CLOSE);
        }
        
        // Size should still be 1
        REQUIRE(components.size() == initialSize + 1);
    }
    
    SECTION("Index mapping memory usage")
    {
        PALIndexMappings mappings;
        
        // Add sparse indices (should not allocate for missing indices)
        mappings.addGroup(1, createSampleCuratedGroup(1));
        mappings.addGroup(1000, createSampleCuratedGroup(1000));
        mappings.addGroup(10000, createSampleCuratedGroup(10000));
        
        // Should only have 3 entries despite large index numbers
        REQUIRE(mappings.getIndexToGroup().size() == 3);
        
        // Verify correct retrieval
        const auto& indexToGroup = mappings.getIndexToGroup();
        REQUIRE(indexToGroup.at(1).getIndexNumber() == 1);
        REQUIRE(indexToGroup.at(1000).getIndexNumber() == 1000);
        REQUIRE(indexToGroup.at(10000).getIndexNumber() == 10000);
    }
}

TEST_CASE("Data Structure Edge Cases", "[OptimizedDataStructures][edge_cases]")
{
    SECTION("Empty structures")
    {
        // Test all structures with empty/default values
        CuratedGroup emptyGroup;
        ComponentUsageStats emptyStats(0, 0, {});
        PALIndexMappings emptyMappings;
        ComponentHierarchyRules emptyRules;
        
        // Should not crash and should have sensible defaults
        REQUIRE(emptyGroup.getIndexNumber() == 0);
        REQUIRE(emptyStats.getTotalPatterns() == 0);
        REQUIRE(emptyMappings.getTotalIndices() == 0);
        REQUIRE(emptyRules.getIndexToAllowedComponents().empty());
    }
    
    SECTION("Component type boundary conditions")
    {
        std::set<pattern_universe::PriceComponentType> components;
        
        // Add all possible component types
        components.insert(pattern_universe::PriceComponentType::OPEN);
        components.insert(pattern_universe::PriceComponentType::HIGH);
        components.insert(pattern_universe::PriceComponentType::LOW);
        components.insert(pattern_universe::PriceComponentType::CLOSE);
        
        REQUIRE(components.size() == 4);
        
        // Test with single component
        std::set<pattern_universe::PriceComponentType> singleComponent = {pattern_universe::PriceComponentType::CLOSE};
        REQUIRE(singleComponent.size() == 1);
        
        // Test empty set
        std::set<pattern_universe::PriceComponentType> emptyComponents;
        REQUIRE(emptyComponents.empty());
    }
}

TEST_CASE("Data Structure Copy and Move Operations", "[OptimizedDataStructures][copy_move]")
{
    SECTION("CuratedGroup copy operations")
    {
        auto original = createSampleCuratedGroup(123);
        
        // Copy constructor
        CuratedGroup copied(original);
        REQUIRE(copied.getIndexNumber() == original.getIndexNumber());
        REQUIRE(copied.getComponentTypes() == original.getComponentTypes());
        REQUIRE(copied.getPatternCount() == original.getPatternCount());
        
        // Verify deep copy of component types
        REQUIRE(copied.getComponentTypes() == original.getComponentTypes());
    }
    
    SECTION("PatternUniverseResult copy operations")
    {
        auto original = createSamplePatternUniverseResult();
        
        // Copy constructor
        PatternUniverseResult copied(original);
        REQUIRE(copied.getTotalPatternsGenerated() == original.getTotalPatternsGenerated());
        REQUIRE(copied.getBasePatterns() == original.getBasePatterns());
        REQUIRE(copied.getDelayDistribution() == original.getDelayDistribution());
        REQUIRE(copied.getPatterns().size() == original.getPatterns().size());
    }
}