#include "ComponentUsageAnalyzer.h"
#include "AnalysisDatabase.h"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace palanalyzer {

ComponentUsageStats::ComponentUsageStats(std::map<PriceComponentType, uint32_t> componentFrequency,
                                         std::map<PriceComponentType, double> componentPercentage,
                                         std::map<uint8_t, uint32_t> barOffsetFrequency,
                                         std::map<std::pair<PriceComponentType, uint8_t>, uint32_t> componentBarCombinations,
                                         uint32_t totalComponents,
                                         std::chrono::system_clock::time_point lastAnalyzed)
    : m_componentFrequency(std::move(componentFrequency)),
      m_componentPercentage(std::move(componentPercentage)),
      m_barOffsetFrequency(std::move(barOffsetFrequency)),
      m_componentBarCombinations(std::move(componentBarCombinations)),
      m_totalComponents(totalComponents),
      m_lastAnalyzed(lastAnalyzed)
{
}

ComponentOptimizationRecommendations::ComponentOptimizationRecommendations(
    std::vector<PriceComponentType> highValueComponents,
    std::vector<PriceComponentType> underutilizedComponents,
    std::vector<uint8_t> optimalBarOffsets,
    std::map<SearchType, ComponentUsageStats> searchTypeBreakdown,
    double optimizationPotential)
    : m_highValueComponents(std::move(highValueComponents)),
      m_underutilizedComponents(std::move(underutilizedComponents)),
      m_optimalBarOffsets(std::move(optimalBarOffsets)),
      m_searchTypeBreakdown(std::move(searchTypeBreakdown)),
      m_optimizationPotential(optimizationPotential)
{
}

UsageTrend::UsageTrend(PriceComponentType component,
                      std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>> timeline,
                      double growthRate)
    : m_component(component),
      m_timeline(std::move(timeline)),
      m_growthRate(growthRate)
{
}

ComponentUsageAnalyzer::ComponentUsageAnalyzer(const AnalysisDatabase& database)
    : m_database(database)
{
}

ComponentUsageStats ComponentUsageAnalyzer::analyzeOverallUsage() const
{
    std::vector<PatternStructure> allPatterns = extractAllPatterns();
    return calculateStatsForPatterns(allPatterns);
}

ComponentUsageStats ComponentUsageAnalyzer::analyzeUsageBySearchType(SearchType searchType) const
{
    std::vector<PatternStructure> patterns = extractPatternsBySearchType(searchType);
    return calculateStatsForPatterns(patterns);
}

ComponentUsageStats ComponentUsageAnalyzer::analyzeUsageByGroup(uint32_t groupId) const
{
    std::vector<PatternStructure> patterns = extractPatternsByGroup(groupId);
    return calculateStatsForPatterns(patterns);
}

ComponentOptimizationRecommendations ComponentUsageAnalyzer::generateOptimizationRecommendations() const
{
    ComponentUsageStats overallStats = analyzeOverallUsage();
    
    // Identify high-value components (>10% usage)
    std::vector<PriceComponentType> highValueComponents = identifyHighValueComponents(0.1);
    
    // Identify underutilized components (<2% usage)
    std::vector<PriceComponentType> underutilizedComponents;
    for (const auto& [component, percentage] : overallStats.getComponentPercentage())
    {
        if (percentage < 0.02)
        {
            underutilizedComponents.push_back(component);
        }
    }
    
    // Get optimal bar offsets
    std::vector<uint8_t> optimalBarOffsets = identifyOptimalBarOffsets(10);
    
    // Get search type breakdown
    std::map<SearchType, ComponentUsageStats> searchTypeBreakdown = compareSearchTypes();
    
    // Calculate optimization potential
    double optimizationPotential = calculateOptimizationPotential(overallStats);
    
    return ComponentOptimizationRecommendations(
        std::move(highValueComponents),
        std::move(underutilizedComponents),
        std::move(optimalBarOffsets),
        std::move(searchTypeBreakdown),
        optimizationPotential
    );
}

std::vector<PriceComponentType> ComponentUsageAnalyzer::identifyHighValueComponents(double threshold) const
{
    ComponentUsageStats stats = analyzeOverallUsage();
    std::vector<PriceComponentType> highValueComponents;
    
    for (const auto& [component, percentage] : stats.getComponentPercentage())
    {
        if (percentage >= threshold)
        {
            highValueComponents.push_back(component);
        }
    }
    
    // Sort by usage percentage (descending)
    std::sort(highValueComponents.begin(), highValueComponents.end(),
              [&stats](PriceComponentType a, PriceComponentType b)
              {
                  return stats.getComponentPercentage().at(a) > stats.getComponentPercentage().at(b);
              });
    
    return highValueComponents;
}

std::vector<uint8_t> ComponentUsageAnalyzer::identifyOptimalBarOffsets(size_t topN) const
{
    ComponentUsageStats stats = analyzeOverallUsage();
    
    // Convert to vector for sorting
    std::vector<std::pair<uint8_t, uint32_t>> offsetFrequencies(
        stats.getBarOffsetFrequency().begin(),
        stats.getBarOffsetFrequency().end()
    );
    
    // Sort by frequency (descending)
    std::sort(offsetFrequencies.begin(), offsetFrequencies.end(),
              [](const auto& a, const auto& b)
              {
                  return a.second > b.second;
              });
    
    // Extract top N offsets
    std::vector<uint8_t> optimalOffsets;
    size_t count = std::min(topN, offsetFrequencies.size());
    optimalOffsets.reserve(count);
    
    for (size_t i = 0; i < count; ++i)
    {
        optimalOffsets.push_back(offsetFrequencies[i].first);
    }
    
    return optimalOffsets;
}

std::map<SearchType, ComponentUsageStats> ComponentUsageAnalyzer::compareSearchTypes() const
{
    std::map<SearchType, ComponentUsageStats> comparison;
    
    const std::vector<SearchType> searchTypes = {
        SearchType::BASIC,
        SearchType::EXTENDED,
        SearchType::DEEP,
        SearchType::CLOSE,
        SearchType::HIGH_LOW,
        SearchType::OPEN_CLOSE,
        SearchType::MIXED
    };
    
    for (SearchType searchType : searchTypes)
    {
        ComponentUsageStats stats = analyzeUsageBySearchType(searchType);
        if (stats.getTotalComponents() > 0)
        {
            comparison.emplace(searchType, std::move(stats));
        }
    }
    
    return comparison;
}

std::map<uint32_t, ComponentUsageStats> ComponentUsageAnalyzer::compareGroups() const
{
    std::map<uint32_t, ComponentUsageStats> comparison;
    
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            ComponentUsageStats stats = analyzeUsageByGroup(groupId);
            if (stats.getTotalComponents() > 0)
            {
                comparison.emplace(groupId, std::move(stats));
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during group comparison: " << e.what() << std::endl;
    }
    
    return comparison;
}

std::vector<UsageTrend> ComponentUsageAnalyzer::analyzeUsageTrends() const
{
    std::vector<UsageTrend> trends;
    
    // For now, create basic trends based on current usage
    // In a full implementation, this would analyze historical data
    ComponentUsageStats overallStats = analyzeOverallUsage();
    
    for (const auto& [component, frequency] : overallStats.getComponentFrequency())
    {
        std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>> timeline;
        timeline.emplace_back(overallStats.getLastAnalyzed(), frequency);
        
        // Simple growth rate calculation (would be more sophisticated with historical data)
        double growthRate = 0.0;
        
        UsageTrend trend(component, std::move(timeline), growthRate);
        trends.push_back(std::move(trend));
    }
    
    return trends;
}

std::vector<std::pair<std::pair<PriceComponentType, uint8_t>, uint32_t>>
ComponentUsageAnalyzer::getMostFrequentCombinations(size_t topN) const
{
    ComponentUsageStats stats = analyzeOverallUsage();
    
    // Convert to vector for sorting
    std::vector<std::pair<std::pair<PriceComponentType, uint8_t>, uint32_t>> combinations(
        stats.getComponentBarCombinations().begin(),
        stats.getComponentBarCombinations().end()
    );
    
    // Sort by frequency (descending)
    std::sort(combinations.begin(), combinations.end(),
              [](const auto& a, const auto& b)
              {
                  return a.second > b.second;
              });
    
    // Return top N combinations
    if (combinations.size() > topN)
    {
        combinations.resize(topN);
    }
    
    return combinations;
}

std::map<uint32_t, double> ComponentUsageAnalyzer::analyzeComponentDiversity() const
{
    std::map<uint32_t, double> diversityScores;
    
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            ComponentUsageStats groupStats = analyzeUsageByGroup(groupId);
            double diversityScore = calculateDiversityScore(
                groupStats.getComponentFrequency(),
                groupStats.getTotalComponents()
            );
            diversityScores[groupId] = diversityScore;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during diversity analysis: " << e.what() << std::endl;
    }
    
    return diversityScores;
}

std::map<std::pair<PriceComponentType, PriceComponentType>, double>
ComponentUsageAnalyzer::getComponentCorrelationMatrix() const
{
    std::map<std::pair<PriceComponentType, PriceComponentType>, double> correlationMatrix;
    
    ComponentUsageStats overallStats = analyzeOverallUsage();
    const auto& componentFreq = overallStats.getComponentFrequency();
    
    // Calculate simple correlation based on co-occurrence
    std::vector<PriceComponentType> components;
    for (const auto& [component, freq] : componentFreq)
    {
        components.push_back(component);
    }
    
    for (size_t i = 0; i < components.size(); ++i)
    {
        for (size_t j = i + 1; j < components.size(); ++j)
        {
            PriceComponentType comp1 = components[i];
            PriceComponentType comp2 = components[j];
            
            // Simple correlation calculation based on frequency similarity
            uint32_t freq1 = componentFreq.at(comp1);
            uint32_t freq2 = componentFreq.at(comp2);
            
            double correlation = 1.0 - (std::abs(static_cast<double>(freq1) - static_cast<double>(freq2)) / 
                                       std::max(static_cast<double>(freq1), static_cast<double>(freq2)));
            
            correlationMatrix[{comp1, comp2}] = correlation;
            correlationMatrix[{comp2, comp1}] = correlation; // Symmetric
        }
    }
    
    return correlationMatrix;
}

ComponentUsageStats ComponentUsageAnalyzer::calculateStatsForPatterns(const std::vector<PatternStructure>& patterns) const
{
    std::map<PriceComponentType, uint32_t> componentFrequency;
    std::map<uint8_t, uint32_t> barOffsetFrequency;
    std::map<std::pair<PriceComponentType, uint8_t>, uint32_t> componentBarCombinations;
    uint32_t totalComponents = 0;
    
    for (const auto& pattern : patterns)
    {
        // Count components used in this pattern
        for (const auto& componentStr : pattern.getComponentsUsed())
        {
            try
            {
                PriceComponentType componentType = stringToComponentType(componentStr);
                componentFrequency[componentType]++;
                totalComponents++;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Warning: Unknown component type: " << componentStr << std::endl;
            }
        }
        
        // Count bar offsets used
        for (int offset : pattern.getBarOffsetsUsed())
        {
            if (offset >= 0 && offset <= 255)
            {
                barOffsetFrequency[static_cast<uint8_t>(offset)]++;
            }
        }
        
        // Count component-bar combinations from conditions
        for (const auto& condition : pattern.getConditions())
        {
            const auto& lhs = condition.getLhs();
            const auto& rhs = condition.getRhs();
            
            std::pair<PriceComponentType, uint8_t> lhsCombination = {lhs.getComponentType(), lhs.getBarOffset()};
            std::pair<PriceComponentType, uint8_t> rhsCombination = {rhs.getComponentType(), rhs.getBarOffset()};
            
            componentBarCombinations[lhsCombination]++;
            componentBarCombinations[rhsCombination]++;
        }
    }
    
    // Calculate percentages
    std::map<PriceComponentType, double> componentPercentage;
    if (totalComponents > 0)
    {
        for (const auto& [component, frequency] : componentFrequency)
        {
            componentPercentage[component] = static_cast<double>(frequency) / static_cast<double>(totalComponents);
        }
    }
    
    return ComponentUsageStats(
        std::move(componentFrequency),
        std::move(componentPercentage),
        std::move(barOffsetFrequency),
        std::move(componentBarCombinations),
        totalComponents,
        std::chrono::system_clock::now()
    );
}

double ComponentUsageAnalyzer::calculateOptimizationPotential(const ComponentUsageStats& stats) const
{
    if (stats.getTotalComponents() == 0)
    {
        return 0.0;
    }
    
    // Calculate optimization potential based on component distribution
    double entropy = 0.0;
    double maxEntropy = std::log2(static_cast<double>(stats.getComponentFrequency().size()));
    
    for (const auto& [component, percentage] : stats.getComponentPercentage())
    {
        if (percentage > 0.0)
        {
            entropy -= percentage * std::log2(percentage);
        }
    }
    
    // Normalize entropy to get optimization potential
    double optimizationPotential = (maxEntropy > 0.0) ? (entropy / maxEntropy) : 0.0;
    
    // Clamp to [0.0, 1.0] range
    return std::max(0.0, std::min(1.0, optimizationPotential));
}

std::vector<PatternStructure> ComponentUsageAnalyzer::extractAllPatterns() const
{
    std::vector<PatternStructure> allPatterns;
    
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            const auto& patterns = groupInfo.getPatterns();
            
            for (const auto& [hashStr, pattern] : patterns)
            {
                allPatterns.push_back(pattern);
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error extracting all patterns: " << e.what() << std::endl;
    }
    
    return allPatterns;
}

std::vector<PatternStructure> ComponentUsageAnalyzer::extractPatternsBySearchType(SearchType searchType) const
{
    std::vector<PatternStructure> patterns;
    
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        std::string searchTypeStr = searchTypeToString(searchType);
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            if (groupInfo.getSearchType() == searchTypeStr)
            {
                const auto& groupPatterns = groupInfo.getPatterns();
                
                for (const auto& [hashStr, pattern] : groupPatterns)
                {
                    patterns.push_back(pattern);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error extracting patterns by search type: " << e.what() << std::endl;
    }
    
    return patterns;
}

std::vector<PatternStructure> ComponentUsageAnalyzer::extractPatternsByGroup(uint32_t groupId) const
{
    std::vector<PatternStructure> patterns;
    
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        auto groupIt = indexGroups.find(groupId);
        
        if (groupIt != indexGroups.end())
        {
            const auto& groupPatterns = groupIt->second.getPatterns();
            
            for (const auto& [hashStr, pattern] : groupPatterns)
            {
                patterns.push_back(pattern);
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error extracting patterns by group: " << e.what() << std::endl;
    }
    
    return patterns;
}

double ComponentUsageAnalyzer::calculateDiversityScore(const std::map<PriceComponentType, uint32_t>& componentFrequency,
                                                      uint32_t totalComponents) const
{
    if (totalComponents == 0 || componentFrequency.empty())
    {
        return 0.0;
    }
    
    // Calculate Shannon diversity index
    double diversity = 0.0;
    
    for (const auto& [component, frequency] : componentFrequency)
    {
        double proportion = static_cast<double>(frequency) / static_cast<double>(totalComponents);
        if (proportion > 0.0)
        {
            diversity -= proportion * std::log2(proportion);
        }
    }
    
    // Normalize by maximum possible diversity
    double maxDiversity = std::log2(static_cast<double>(componentFrequency.size()));
    
    return (maxDiversity > 0.0) ? (diversity / maxDiversity) : 0.0;
}

SearchType ComponentUsageAnalyzer::parseSearchType(const std::string& searchTypeStr) const
{
    return stringToSearchType(searchTypeStr);
}

} // namespace palanalyzer