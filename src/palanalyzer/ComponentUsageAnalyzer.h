#pragma once

#include "DataStructures.h"
#include <map>
#include <vector>
#include <chrono>
#include <memory>

namespace palanalyzer {

// Forward declaration
class AnalysisDatabase;

/**
 * @brief Statistics for component usage analysis
 */
class ComponentUsageStats {
public:
    /**
     * @brief Construct component usage statistics
     * 
     * @param componentFrequency Map of component types to their usage frequencies
     * @param componentPercentage Map of component types to their usage percentages
     * @param barOffsetFrequency Map of bar offsets to their usage frequencies
     * @param componentBarCombinations Map of component-bar combinations to frequencies
     * @param totalComponents Total number of component instances analyzed
     * @param lastAnalyzed Timestamp when analysis was performed
     */
    ComponentUsageStats(std::map<PriceComponentType, uint32_t> componentFrequency,
                       std::map<PriceComponentType, double> componentPercentage,
                       std::map<uint8_t, uint32_t> barOffsetFrequency,
                       std::map<std::pair<PriceComponentType, uint8_t>, uint32_t> componentBarCombinations,
                       uint32_t totalComponents,
                       std::chrono::system_clock::time_point lastAnalyzed);

    const std::map<PriceComponentType, uint32_t>& getComponentFrequency() const
    {
        return m_componentFrequency;
    }

    const std::map<PriceComponentType, double>& getComponentPercentage() const
    {
        return m_componentPercentage;
    }

    const std::map<uint8_t, uint32_t>& getBarOffsetFrequency() const
    {
        return m_barOffsetFrequency;
    }

    const std::map<std::pair<PriceComponentType, uint8_t>, uint32_t>& getComponentBarCombinations() const
    {
        return m_componentBarCombinations;
    }

    uint32_t getTotalComponents() const
    {
        return m_totalComponents;
    }

    const std::chrono::system_clock::time_point& getLastAnalyzed() const
    {
        return m_lastAnalyzed;
    }

private:
    std::map<PriceComponentType, uint32_t> m_componentFrequency;
    std::map<PriceComponentType, double> m_componentPercentage;
    std::map<uint8_t, uint32_t> m_barOffsetFrequency;
    std::map<std::pair<PriceComponentType, uint8_t>, uint32_t> m_componentBarCombinations;
    uint32_t m_totalComponents;
    std::chrono::system_clock::time_point m_lastAnalyzed;
};

/**
 * @brief Optimization recommendations based on component usage analysis
 */
class ComponentOptimizationRecommendations {
public:
    /**
     * @brief Construct optimization recommendations
     * 
     * @param highValueComponents Components with high usage frequency
     * @param underutilizedComponents Components with low usage frequency
     * @param optimalBarOffsets Most frequently used bar offsets
     * @param searchTypeBreakdown Usage breakdown by search type
     * @param optimizationPotential Overall optimization potential score (0.0 to 1.0)
     */
    ComponentOptimizationRecommendations(std::vector<PriceComponentType> highValueComponents,
                                       std::vector<PriceComponentType> underutilizedComponents,
                                       std::vector<uint8_t> optimalBarOffsets,
                                       std::map<SearchType, ComponentUsageStats> searchTypeBreakdown,
                                       double optimizationPotential);

    const std::vector<PriceComponentType>& getHighValueComponents() const
    {
        return m_highValueComponents;
    }

    const std::vector<PriceComponentType>& getUnderutilizedComponents() const
    {
        return m_underutilizedComponents;
    }

    const std::vector<uint8_t>& getOptimalBarOffsets() const
    {
        return m_optimalBarOffsets;
    }

    const std::map<SearchType, ComponentUsageStats>& getSearchTypeBreakdown() const
    {
        return m_searchTypeBreakdown;
    }

    double getOptimizationPotential() const
    {
        return m_optimizationPotential;
    }

private:
    std::vector<PriceComponentType> m_highValueComponents;
    std::vector<PriceComponentType> m_underutilizedComponents;
    std::vector<uint8_t> m_optimalBarOffsets;
    std::map<SearchType, ComponentUsageStats> m_searchTypeBreakdown;
    double m_optimizationPotential;
};

/**
 * @brief Usage trend analysis for components over time
 */
class UsageTrend {
public:
    /**
     * @brief Construct usage trend analysis
     * 
     * @param component The component type being analyzed
     * @param timeline Vector of timestamp-count pairs showing usage over time
     * @param growthRate Calculated growth rate for the component usage
     */
    UsageTrend(PriceComponentType component,
              std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>> timeline,
              double growthRate);

    PriceComponentType getComponent() const
    {
        return m_component;
    }

    const std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>>& getTimeline() const
    {
        return m_timeline;
    }

    double getGrowthRate() const
    {
        return m_growthRate;
    }

private:
    PriceComponentType m_component;
    std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>> m_timeline;
    double m_growthRate;
};

/**
 * @brief Comprehensive component usage analyzer for pattern optimization and insights
 * 
 * This class provides detailed analysis of how price components are used across
 * patterns in the database, enabling optimization recommendations and usage insights.
 */
class ComponentUsageAnalyzer {
public:
    /**
     * @brief Construct a new Component Usage Analyzer
     * 
     * @param database Reference to the analysis database for pattern data
     */
    explicit ComponentUsageAnalyzer(const AnalysisDatabase& database);

    /**
     * @brief Analyze overall component usage across all patterns
     * 
     * @return ComponentUsageStats with comprehensive usage statistics
     */
    ComponentUsageStats analyzeOverallUsage() const;

    /**
     * @brief Analyze component usage for a specific search type
     * 
     * @param searchType The search type to analyze
     * @return ComponentUsageStats for the specified search type
     */
    ComponentUsageStats analyzeUsageBySearchType(SearchType searchType) const;

    /**
     * @brief Analyze component usage for a specific pattern group
     * 
     * @param groupId The group ID to analyze
     * @return ComponentUsageStats for the specified group
     */
    ComponentUsageStats analyzeUsageByGroup(uint32_t groupId) const;

    /**
     * @brief Generate optimization recommendations based on usage analysis
     * 
     * @return ComponentOptimizationRecommendations with actionable insights
     */
    ComponentOptimizationRecommendations generateOptimizationRecommendations() const;

    /**
     * @brief Identify high-value components based on usage frequency
     * 
     * @param threshold Minimum usage percentage to be considered high-value (default: 0.1 = 10%)
     * @return Vector of high-value component types
     */
    std::vector<PriceComponentType> identifyHighValueComponents(double threshold = 0.1) const;

    /**
     * @brief Identify optimal bar offsets based on usage patterns
     * 
     * @param topN Number of top bar offsets to return (default: 10)
     * @return Vector of optimal bar offsets sorted by usage frequency
     */
    std::vector<uint8_t> identifyOptimalBarOffsets(size_t topN = 10) const;

    /**
     * @brief Compare component usage across different search types
     * 
     * @return Map of search types to their component usage statistics
     */
    std::map<SearchType, ComponentUsageStats> compareSearchTypes() const;

    /**
     * @brief Compare component usage across different pattern groups
     * 
     * @return Map of group IDs to their component usage statistics
     */
    std::map<uint32_t, ComponentUsageStats> compareGroups() const;

    /**
     * @brief Analyze usage trends for components over time
     * 
     * @return Vector of usage trends for each component type
     */
    std::vector<UsageTrend> analyzeUsageTrends() const;

    /**
     * @brief Get the most frequently used component combinations
     * 
     * @param topN Number of top combinations to return (default: 20)
     * @return Vector of component-bar offset pairs sorted by frequency
     */
    std::vector<std::pair<std::pair<PriceComponentType, uint8_t>, uint32_t>> 
    getMostFrequentCombinations(size_t topN = 20) const;

    /**
     * @brief Analyze component diversity within pattern groups
     * 
     * @return Map of group IDs to diversity scores (0.0 to 1.0)
     */
    std::map<uint32_t, double> analyzeComponentDiversity() const;

    /**
     * @brief Get component usage correlation matrix
     * 
     * @return Map showing correlation between different component types
     */
    std::map<std::pair<PriceComponentType, PriceComponentType>, double> 
    getComponentCorrelationMatrix() const;

private:
    const AnalysisDatabase& m_database;

    /**
     * @brief Calculate statistics for a given set of patterns
     * 
     * @param patterns Vector of pattern structures to analyze
     * @return ComponentUsageStats for the provided patterns
     */
    ComponentUsageStats calculateStatsForPatterns(const std::vector<PatternStructure>& patterns) const;

    /**
     * @brief Calculate optimization potential based on usage statistics
     * 
     * @param stats Component usage statistics to analyze
     * @return Optimization potential score (0.0 to 1.0)
     */
    double calculateOptimizationPotential(const ComponentUsageStats& stats) const;

    /**
     * @brief Extract all patterns from the database
     * 
     * @return Vector of all pattern structures in the database
     */
    std::vector<PatternStructure> extractAllPatterns() const;

    /**
     * @brief Extract patterns for a specific search type
     * 
     * @param searchType The search type to filter by
     * @return Vector of pattern structures matching the search type
     */
    std::vector<PatternStructure> extractPatternsBySearchType(SearchType searchType) const;

    /**
     * @brief Extract patterns for a specific group
     * 
     * @param groupId The group ID to filter by
     * @return Vector of pattern structures in the specified group
     */
    std::vector<PatternStructure> extractPatternsByGroup(uint32_t groupId) const;

    /**
     * @brief Calculate diversity score for a set of components
     * 
     * @param componentFrequency Map of component types to their frequencies
     * @param totalComponents Total number of component instances
     * @return Diversity score (0.0 to 1.0, higher = more diverse)
     */
    double calculateDiversityScore(const std::map<PriceComponentType, uint32_t>& componentFrequency,
                                  uint32_t totalComponents) const;

    /**
     * @brief Convert search type string to SearchType enum
     * 
     * @param searchTypeStr String representation of search type
     * @return SearchType enum value
     */
    SearchType parseSearchType(const std::string& searchTypeStr) const;
};

} // namespace palanalyzer