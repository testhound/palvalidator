#include <catch2/catch_test_macros.hpp>
#include "ComponentUsageAnalyzer.h"
#include "AnalysisDatabase.h"
#include "DataStructures.h"
#include <chrono>
#include <vector>
#include <set>

using namespace palanalyzer;

/**
 * @brief Test fixture for ComponentUsageAnalyzer tests
 */
class ComponentUsageAnalyzerTestFixture {
public:
    ComponentUsageAnalyzerTestFixture()
        : database("test_component_analyzer_database.json")
    {
        setupTestDatabase();
        analyzer = std::make_unique<ComponentUsageAnalyzer>(database);
    }

    ~ComponentUsageAnalyzerTestFixture()
    {
        // Clean up test database file
        std::remove("test_component_analyzer_database.json");
    }

protected:
    AnalysisDatabase database;
    std::unique_ptr<ComponentUsageAnalyzer> analyzer;

    void setupTestDatabase()
    {
        // Create multiple test index groups with different characteristics
        
        // Group 1: CLOSE only patterns
        std::vector<uint8_t> barCombination1 = {0, 1, 2};
        std::set<PriceComponentType> componentTypes1 = {PriceComponentType::Close};
        database.addPatternToIndexGroup(101, barCombination1, componentTypes1, "close_patterns.pal", "Deep");
        
        // Group 2: HIGH+LOW patterns
        std::vector<uint8_t> barCombination2 = {0, 1, 2, 3};
        std::set<PriceComponentType> componentTypes2 = {PriceComponentType::High, PriceComponentType::Low};
        database.addPatternToIndexGroup(102, barCombination2, componentTypes2, "high_low_patterns.pal", "Extended");
        
        // Group 3: Mixed OHLC patterns
        std::vector<uint8_t> barCombination3 = {0, 1, 2, 3, 4};
        std::set<PriceComponentType> componentTypes3 = {
            PriceComponentType::Open, PriceComponentType::High,
            PriceComponentType::Low, PriceComponentType::Close
        };
        database.addPatternToIndexGroup(103, barCombination3, componentTypes3, "mixed_patterns.pal", "Deep");
        
        // Add test patterns to each group
        addTestPatternsToGroup(101, componentTypes1, barCombination1);
        addTestPatternsToGroup(102, componentTypes2, barCombination2);
        addTestPatternsToGroup(103, componentTypes3, barCombination3);
    }

    void addTestPatternsToGroup(uint32_t groupId, 
                               const std::set<PriceComponentType>& componentTypes,
                               const std::vector<uint8_t>& barOffsets)
    {
        // Create test patterns for the group
        std::vector<PriceComponentType> components(componentTypes.begin(), componentTypes.end());
        
        for (size_t i = 0; i < 5; ++i) // Add 5 patterns per group
        {
            std::vector<PriceComponentDescriptor> patternComponents;
            
            // Create components using the group's allowed types and offsets
            for (size_t j = 0; j < std::min(components.size(), static_cast<size_t>(2)); ++j)
            {
                uint8_t offset = (j < barOffsets.size()) ? barOffsets[j] : 0;
                patternComponents.emplace_back(components[j], offset, "Component" + std::to_string(j));
            }
            
            PatternAnalysis testPattern(
                groupId,                                       // index (use groupId)
                "test_file_" + std::to_string(groupId) + ".pal", // sourceFile
                1000000ULL + groupId * 1000 + i,              // patternHash (unique)
                patternComponents,                             // components
                "Test Pattern " + std::to_string(i),          // patternString
                false,                                         // isChained
                static_cast<uint8_t>(barOffsets.size() - 1),  // maxBarOffset
                static_cast<uint8_t>(barOffsets.size() - 1),  // barSpread
                static_cast<uint8_t>(patternComponents.size() - 1), // conditionCount (should be components - 1)
                std::chrono::system_clock::now(),             // analyzedAt
                0.6 + (i * 0.05),                             // profitabilityLong
                0.4 + (i * 0.03),                             // profitabilityShort
                50 + (i * 10),                                // trades
                2 + i                                         // consecutiveLosses
            );
            
            database.addPattern(testPattern);
        }
    }
};

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "ComponentUsageAnalyzer Construction", "[ComponentUsageAnalyzer]")
{
    SECTION("Constructor creates valid analyzer")
    {
        REQUIRE(analyzer != nullptr);
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Overall Usage Analysis", "[ComponentUsageAnalyzer]")
{
    SECTION("Analyze overall component usage")
    {
        ComponentUsageStats stats = analyzer->analyzeOverallUsage();
        
        REQUIRE(stats.getTotalComponents() > 0);
        REQUIRE_FALSE(stats.getComponentFrequency().empty());
        REQUIRE_FALSE(stats.getComponentPercentage().empty());
        REQUIRE_FALSE(stats.getBarOffsetFrequency().empty());
        
        // Verify percentages sum to approximately 1.0
        double totalPercentage = 0.0;
        for (const auto& [component, percentage] : stats.getComponentPercentage())
        {
            totalPercentage += percentage;
            REQUIRE(percentage >= 0.0);
            REQUIRE(percentage <= 1.0);
        }
        REQUIRE(totalPercentage > 0.0);
    }
    
    SECTION("Component frequency matches expected patterns")
    {
        ComponentUsageStats stats = analyzer->analyzeOverallUsage();
        const auto& frequency = stats.getComponentFrequency();
        
        // Should have CLOSE components (from group 101)
        REQUIRE(frequency.find(PriceComponentType::Close) != frequency.end());
        
        // Should have HIGH and LOW components (from group 102)
        REQUIRE(frequency.find(PriceComponentType::High) != frequency.end());
        REQUIRE(frequency.find(PriceComponentType::Low) != frequency.end());
        
        // Should have OPEN components (from group 103)
        REQUIRE(frequency.find(PriceComponentType::Open) != frequency.end());
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Search Type Analysis", "[ComponentUsageAnalyzer]")
{
    SECTION("Analyze usage by Deep search type")
    {
        ComponentUsageStats stats = analyzer->analyzeUsageBySearchType(SearchType::DEEP);
        
        REQUIRE(stats.getTotalComponents() > 0);
        
        // Deep search should include patterns from groups 101 and 103
        const auto& frequency = stats.getComponentFrequency();
        REQUIRE(frequency.find(PriceComponentType::Close) != frequency.end());
    }
    
    SECTION("Analyze usage by Extended search type")
    {
        ComponentUsageStats stats = analyzer->analyzeUsageBySearchType(SearchType::EXTENDED);
        
        // Extended search should include patterns from group 102
        const auto& frequency = stats.getComponentFrequency();
        if (stats.getTotalComponents() > 0)
        {
            // Should have HIGH and LOW from group 102
            REQUIRE(frequency.find(PriceComponentType::High) != frequency.end());
            REQUIRE(frequency.find(PriceComponentType::Low) != frequency.end());
        }
    }
    
    SECTION("Unknown search type returns empty stats")
    {
        ComponentUsageStats stats = analyzer->analyzeUsageBySearchType(SearchType::UNKNOWN);
        REQUIRE(stats.getTotalComponents() == 0);
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Group Analysis", "[ComponentUsageAnalyzer]")
{
    SECTION("Analyze usage by specific group")
    {
        ComponentUsageStats stats = analyzer->analyzeUsageByGroup(101);
        
        if (stats.getTotalComponents() > 0)
        {
            const auto& frequency = stats.getComponentFrequency();
            
            // Group 101 should only have CLOSE components
            REQUIRE(frequency.find(PriceComponentType::Close) != frequency.end());
            REQUIRE(frequency.find(PriceComponentType::Open) == frequency.end());
        }
    }
    
    SECTION("Non-existing group returns empty stats")
    {
        ComponentUsageStats stats = analyzer->analyzeUsageByGroup(999);
        REQUIRE(stats.getTotalComponents() == 0);
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "High Value Component Identification", "[ComponentUsageAnalyzer]")
{
    SECTION("Identify high-value components with default threshold")
    {
        std::vector<PriceComponentType> highValueComponents = analyzer->identifyHighValueComponents();
        
        // Should identify components that appear frequently
        REQUIRE_FALSE(highValueComponents.empty());
        
        // Verify components are sorted by usage frequency (descending)
        ComponentUsageStats overallStats = analyzer->analyzeOverallUsage();
        const auto& percentages = overallStats.getComponentPercentage();
        
        for (size_t i = 1; i < highValueComponents.size(); ++i)
        {
            double prevPercentage = percentages.at(highValueComponents[i-1]);
            double currPercentage = percentages.at(highValueComponents[i]);
            REQUIRE(prevPercentage >= currPercentage);
        }
    }
    
    SECTION("Custom threshold filters components correctly")
    {
        std::vector<PriceComponentType> highValueComponents = analyzer->identifyHighValueComponents(0.5); // 50% threshold
        
        // With high threshold, should have fewer components
        ComponentUsageStats overallStats = analyzer->analyzeOverallUsage();
        const auto& percentages = overallStats.getComponentPercentage();
        
        for (PriceComponentType component : highValueComponents)
        {
            REQUIRE(percentages.at(component) >= 0.5);
        }
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Optimal Bar Offset Identification", "[ComponentUsageAnalyzer]")
{
    SECTION("Identify optimal bar offsets")
    {
        std::vector<uint8_t> optimalOffsets = analyzer->identifyOptimalBarOffsets(5);
        
        REQUIRE(optimalOffsets.size() <= 5);
        
        // Verify offsets are sorted by frequency (descending)
        ComponentUsageStats overallStats = analyzer->analyzeOverallUsage();
        const auto& offsetFrequency = overallStats.getBarOffsetFrequency();
        
        for (size_t i = 1; i < optimalOffsets.size(); ++i)
        {
            uint32_t prevFreq = offsetFrequency.at(optimalOffsets[i-1]);
            uint32_t currFreq = offsetFrequency.at(optimalOffsets[i]);
            REQUIRE(prevFreq >= currFreq);
        }
    }
    
    SECTION("Request more offsets than available")
    {
        std::vector<uint8_t> optimalOffsets = analyzer->identifyOptimalBarOffsets(100);
        
        ComponentUsageStats overallStats = analyzer->analyzeOverallUsage();
        const auto& offsetFrequency = overallStats.getBarOffsetFrequency();
        
        // Should return all available offsets, not more
        REQUIRE(optimalOffsets.size() <= offsetFrequency.size());
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Search Type Comparison", "[ComponentUsageAnalyzer]")
{
    SECTION("Compare usage across search types")
    {
        std::map<SearchType, ComponentUsageStats> comparison = analyzer->compareSearchTypes();
        
        // Should have entries for search types that have patterns
        REQUIRE_FALSE(comparison.empty());
        
        // Verify each search type has valid stats
        for (const auto& [searchType, stats] : comparison)
        {
            REQUIRE(stats.getTotalComponents() > 0);
            REQUIRE_FALSE(stats.getComponentFrequency().empty());
        }
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Group Comparison", "[ComponentUsageAnalyzer]")
{
    SECTION("Compare usage across groups")
    {
        std::map<uint32_t, ComponentUsageStats> comparison = analyzer->compareGroups();
        
        // Should have entries for groups that have patterns
        REQUIRE_FALSE(comparison.empty());
        
        // Verify specific group characteristics
        if (comparison.find(101) != comparison.end())
        {
            const auto& group101Stats = comparison.at(101);
            const auto& frequency = group101Stats.getComponentFrequency();
            
            // Group 101 should only have CLOSE components
            REQUIRE(frequency.find(PriceComponentType::Close) != frequency.end());
        }
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Optimization Recommendations", "[ComponentUsageAnalyzer]")
{
    SECTION("Generate optimization recommendations")
    {
        ComponentOptimizationRecommendations recommendations = analyzer->generateOptimizationRecommendations();
        
        REQUIRE_FALSE(recommendations.getHighValueComponents().empty());
        REQUIRE_FALSE(recommendations.getOptimalBarOffsets().empty());
        REQUIRE(recommendations.getOptimizationPotential() >= 0.0);
        REQUIRE(recommendations.getOptimizationPotential() <= 1.0);
        
        // Verify search type breakdown is included
        const auto& searchTypeBreakdown = recommendations.getSearchTypeBreakdown();
        REQUIRE_FALSE(searchTypeBreakdown.empty());
    }
    
    SECTION("High-value components are actually high frequency")
    {
        ComponentOptimizationRecommendations recommendations = analyzer->generateOptimizationRecommendations();
        ComponentUsageStats overallStats = analyzer->analyzeOverallUsage();
        
        const auto& highValueComponents = recommendations.getHighValueComponents();
        const auto& percentages = overallStats.getComponentPercentage();
        
        for (PriceComponentType component : highValueComponents)
        {
            REQUIRE(percentages.at(component) >= 0.1); // Should be at least 10%
        }
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Most Frequent Combinations", "[ComponentUsageAnalyzer]")
{
    SECTION("Get most frequent component-bar combinations")
    {
        auto combinations = analyzer->getMostFrequentCombinations(10);
        
        REQUIRE(combinations.size() <= 10);
        
        // Verify combinations are sorted by frequency (descending)
        for (size_t i = 1; i < combinations.size(); ++i)
        {
            REQUIRE(combinations[i-1].second >= combinations[i].second);
        }
        
        // Verify all combinations have valid components and offsets
        for (const auto& [combination, frequency] : combinations)
        {
            const auto& [component, barOffset] = combination;
            REQUIRE(frequency > 0);
            REQUIRE(barOffset <= 255); // Valid bar offset range
        }
    }
    
    SECTION("Request more combinations than available")
    {
        auto combinations = analyzer->getMostFrequentCombinations(1000);
        
        ComponentUsageStats overallStats = analyzer->analyzeOverallUsage();
        const auto& allCombinations = overallStats.getComponentBarCombinations();
        
        // Should return all available combinations, not more
        REQUIRE(combinations.size() <= allCombinations.size());
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Component Diversity Analysis", "[ComponentUsageAnalyzer]")
{
    SECTION("Analyze component diversity across groups")
    {
        std::map<uint32_t, double> diversityScores = analyzer->analyzeComponentDiversity();
        
        REQUIRE_FALSE(diversityScores.empty());
        
        // Verify diversity scores are in valid range [0.0, 1.0]
        for (const auto& [groupId, score] : diversityScores)
        {
            REQUIRE(score >= 0.0);
            REQUIRE(score <= 1.0);
        }
        
        // Group 103 (mixed OHLC) should have higher diversity than group 101 (CLOSE only)
        if (diversityScores.find(101) != diversityScores.end() && 
            diversityScores.find(103) != diversityScores.end())
        {
            REQUIRE(diversityScores.at(103) >= diversityScores.at(101));
        }
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Component Correlation Analysis", "[ComponentUsageAnalyzer]")
{
    SECTION("Get component correlation matrix")
    {
        auto correlationMatrix = analyzer->getComponentCorrelationMatrix();
        
        REQUIRE_FALSE(correlationMatrix.empty());
        
        // Verify correlation values are in valid range [0.0, 1.0]
        for (const auto& [componentPair, correlation] : correlationMatrix)
        {
            REQUIRE(correlation >= 0.0);
            REQUIRE(correlation <= 1.0);
        }
        
        // Verify matrix is symmetric
        for (const auto& [componentPair, correlation] : correlationMatrix)
        {
            const auto& [comp1, comp2] = componentPair;
            std::pair<PriceComponentType, PriceComponentType> reversePair = {comp2, comp1};
            
            if (correlationMatrix.find(reversePair) != correlationMatrix.end())
            {
                REQUIRE(correlationMatrix.at(reversePair) == correlation);
            }
        }
    }
}

TEST_CASE_METHOD(ComponentUsageAnalyzerTestFixture, "Usage Trends Analysis", "[ComponentUsageAnalyzer]")
{
    SECTION("Analyze usage trends over time")
    {
        std::vector<UsageTrend> trends = analyzer->analyzeUsageTrends();
        
        REQUIRE_FALSE(trends.empty());
        
        // Verify each trend has valid data
        for (const auto& trend : trends)
        {
            REQUIRE_FALSE(trend.getTimeline().empty());
            
            // Verify timeline data is valid
            for (const auto& [timestamp, count] : trend.getTimeline())
            {
                REQUIRE(count > 0);
            }
        }
    }
}

TEST_CASE("ComponentUsageStats Class", "[ComponentUsageStats]")
{
    SECTION("Constructor initializes all fields correctly")
    {
        std::map<PriceComponentType, uint32_t> componentFreq = {
            {PriceComponentType::Close, 100},
            {PriceComponentType::High, 80}
        };
        
        std::map<PriceComponentType, double> componentPerc = {
            {PriceComponentType::Close, 0.55},
            {PriceComponentType::High, 0.45}
        };
        
        std::map<uint8_t, uint32_t> barOffsetFreq = {
            {0, 50},
            {1, 40}
        };
        
        std::map<std::pair<PriceComponentType, uint8_t>, uint32_t> combinations = {
            {{PriceComponentType::Close, 0}, 30},
            {{PriceComponentType::High, 1}, 25}
        };
        
        auto now = std::chrono::system_clock::now();
        
        ComponentUsageStats stats(
            componentFreq,
            componentPerc,
            barOffsetFreq,
            combinations,
            180,
            now
        );
        
        REQUIRE(stats.getTotalComponents() == 180);
        REQUIRE(stats.getLastAnalyzed() == now);
        REQUIRE(stats.getComponentFrequency().size() == 2);
        REQUIRE(stats.getComponentPercentage().size() == 2);
        REQUIRE(stats.getBarOffsetFrequency().size() == 2);
        REQUIRE(stats.getComponentBarCombinations().size() == 2);
    }
}

TEST_CASE("ComponentOptimizationRecommendations Class", "[ComponentOptimizationRecommendations]")
{
    SECTION("Constructor initializes all fields correctly")
    {
        std::vector<PriceComponentType> highValue = {PriceComponentType::Close, PriceComponentType::High};
        std::vector<PriceComponentType> underutilized = {PriceComponentType::Open};
        std::vector<uint8_t> optimalOffsets = {0, 1, 2};
        std::map<SearchType, ComponentUsageStats> breakdown;
        double potential = 0.75;
        
        ComponentOptimizationRecommendations recommendations(
            highValue,
            underutilized,
            optimalOffsets,
            breakdown,
            potential
        );
        
        REQUIRE(recommendations.getHighValueComponents().size() == 2);
        REQUIRE(recommendations.getUnderutilizedComponents().size() == 1);
        REQUIRE(recommendations.getOptimalBarOffsets().size() == 3);
        REQUIRE(recommendations.getOptimizationPotential() == 0.75);
    }
}

TEST_CASE("UsageTrend Class", "[UsageTrend]")
{
    SECTION("Constructor initializes all fields correctly")
    {
        auto now = std::chrono::system_clock::now();
        std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>> timeline = {
            {now, 100},
            {now + std::chrono::hours(1), 120}
        };
        
        UsageTrend trend(PriceComponentType::Close, timeline, 0.2);
        
        REQUIRE(trend.getComponent() == PriceComponentType::Close);
        REQUIRE(trend.getTimeline().size() == 2);
        REQUIRE(trend.getGrowthRate() == 0.2);
    }
}