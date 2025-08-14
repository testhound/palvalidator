#pragma once

#include "OptimizedDataStructures.h"
#include "PALAnalysisLoader.h"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>

namespace pattern_universe {

/**
 * @brief Manages PAL's discovered curated group system for optimized pattern generation
 * 
 * Based on reverse-engineering PAL's algorithm, this class implements the sophisticated
 * curated group system that organizes patterns into hierarchical component specializations:
 * - Full OHLC groups (indices 1-153): All four price components
 * - Mixed groups (indices 154-325): Three components with strategic combinations
 * - Dual groups (indices 326-478): Two-component focused patterns
 * - Single groups (indices 480-545): Single-component specialized patterns
 */
class CuratedGroupManager {
public:
    /**
     * @brief Initialize the curated group manager with PAL analysis data
     * @param analysisData Complete PAL analysis containing index mappings and hierarchy rules
     */
    explicit CuratedGroupManager(const PALAnalysisData& analysisData);
    
    /**
     * @brief Get all curated groups for a specific search type
     * @param searchType The search type (Deep, Extended, etc.)
     * @return Vector of curated groups sorted by generation priority
     */
    std::vector<CuratedGroup> getGroupsForSearchType(SearchType searchType) const;
    
    /**
     * @brief Get curated group by index number
     * @param indexNumber The PAL index number
     * @return Pointer to curated group, or nullptr if not found
     */
    const CuratedGroup* getGroupByIndex(uint32_t indexNumber) const;
    
    /**
     * @brief Get all groups that use a specific component type
     * @param componentType The price component type to search for
     * @return Vector of groups that include this component type
     */
    std::vector<const CuratedGroup*> getGroupsUsingComponent(PriceComponentType componentType) const;
    
    /**
     * @brief Get groups within a specific component count range
     * @param minComponents Minimum number of components
     * @param maxComponents Maximum number of components
     * @return Vector of matching groups
     */
    std::vector<const CuratedGroup*> getGroupsByComponentCount(size_t minComponents, size_t maxComponents) const;
    
    /**
     * @brief Get the component specialization tier for a group
     * @param indexNumber The PAL index number
     * @return Component tier (Full OHLC, Mixed, Dual, Single)
     */
    ComponentTier getComponentTier(uint32_t indexNumber) const;
    
    /**
     * @brief Get all groups in a specific component tier
     * @param tier The component specialization tier
     * @return Vector of groups in this tier
     */
    std::vector<const CuratedGroup*> getGroupsByTier(ComponentTier tier) const;
    
    /**
     * @brief Get generation order for optimal pattern universe creation
     * @param searchType Target search type
     * @param prioritizeHighYield If true, prioritize groups with higher pattern counts
     * @return Ordered vector of group indices for generation
     */
    std::vector<uint32_t> getOptimalGenerationOrder(SearchType searchType, bool prioritizeHighYield = true) const;
    
    /**
     * @brief Check if a group supports pattern chaining
     * @param indexNumber The PAL index number
     * @return True if group supports chaining optimization
     */
    bool supportsChaining(uint32_t indexNumber) const;
    
    /**
     * @brief Get recommended batch size for parallel generation
     * @param indexNumber The PAL index number
     * @param availableThreads Number of available processing threads
     * @return Optimal batch size for this group
     */
    size_t getRecommendedBatchSize(uint32_t indexNumber, size_t availableThreads) const;
    
    /**
     * @brief Get component usage statistics for optimization
     * @return Map of component types to their usage frequency across all groups
     */
    std::map<PriceComponentType, ComponentUsageInfo> getComponentUsageStats() const;
    
    /**
     * @brief Validate that all PAL groups are properly loaded
     * @return True if all expected groups are present and valid
     */
    bool validateGroupIntegrity() const;
    
    /**
     * @brief Get total number of patterns across all managed groups
     * @return Total pattern count
     */
    size_t getTotalPatternCount() const;
    
    /**
     * @brief Get groups that are likely to benefit from pre-computation
     * @param minPatternCount Minimum pattern count threshold
     * @return Vector of high-yield groups suitable for pre-computation
     */
    std::vector<const CuratedGroup*> getPreComputationCandidates(uint32_t minPatternCount = 1000) const;

private:
    // Core data structures
    std::map<uint32_t, CuratedGroup> groups_;
    std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices_;
    std::map<ComponentTier, std::vector<uint32_t>> tierToIndices_;
    std::map<PriceComponentType, std::vector<uint32_t>> componentToIndices_;
    
    // Optimization data
    ComponentHierarchyRules hierarchyRules_;
    std::map<PriceComponentType, ComponentUsageInfo> componentUsageStats_;
    
    // Analysis metadata
    std::string analysisVersion_;
    std::chrono::system_clock::time_point loadTime_;
    
    // Private helper methods
    void buildSearchTypeIndex();
    void buildComponentTierIndex();
    void buildComponentUsageIndex();
    void calculateComponentUsageStats(const std::map<SearchType, ComponentUsageStats>& palStats);
    ComponentTier determineComponentTier(const std::set<PriceComponentType>& components) const;
    double calculateGroupPriority(const CuratedGroup& group) const;
    void validateGroupConsistency() const;
    
    // Constants for PAL's discovered algorithm structure
    static constexpr uint32_t FULL_OHLC_START = 1;
    static constexpr uint32_t FULL_OHLC_END = 153;
    static constexpr uint32_t MIXED_START = 154;
    static constexpr uint32_t MIXED_END = 325;
    static constexpr uint32_t DUAL_START = 326;
    static constexpr uint32_t DUAL_END = 478;
    static constexpr uint32_t SINGLE_START = 480;
    static constexpr uint32_t SINGLE_END = 545;
    
    // Performance thresholds based on PAL analysis
    static constexpr uint32_t HIGH_YIELD_THRESHOLD = 500;
    static constexpr uint32_t CHAINING_THRESHOLD = 100;
    static constexpr double PRIORITY_WEIGHT_PATTERN_COUNT = 0.6;
    static constexpr double PRIORITY_WEIGHT_COMPONENT_EFFICIENCY = 0.4;
};

/**
 * @brief Factory for creating optimized curated group managers
 */
class CuratedGroupManagerFactory {
public:
    /**
     * @brief Create a curated group manager from PAL analysis data
     * @param analysisData Complete PAL analysis
     * @return Unique pointer to configured manager
     */
    static std::unique_ptr<CuratedGroupManager> createFromPALAnalysis(const PALAnalysisData& analysisData);
    
    /**
     * @brief Create a curated group manager with custom optimization settings
     * @param analysisData PAL analysis data
     * @param optimizationSettings Custom optimization parameters
     * @return Unique pointer to configured manager
     */
    static std::unique_ptr<CuratedGroupManager> createWithOptimization(
        const PALAnalysisData& analysisData,
        const GroupOptimizationSettings& optimizationSettings);
};

} // namespace pattern_universe