#include "CuratedGroupManager.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <numeric>

namespace pattern_universe {

CuratedGroupManager::CuratedGroupManager(const PALAnalysisData& analysisData)
    : hierarchyRules_(analysisData.getHierarchyRules())
    , analysisVersion_(analysisData.getAnalysisVersion())
    , loadTime_(std::chrono::system_clock::now()) {
    
    // Load all curated groups from PAL analysis
    groups_ = analysisData.getIndexMappings().getIndexToGroup();
    
    // Build optimization indices
    buildSearchTypeIndex();
    buildComponentTierIndex();
    buildComponentUsageIndex();
    calculateComponentUsageStats(analysisData.getComponentStats());
    
    // Validate loaded data
    validateGroupConsistency();
    
    // CuratedGroupManager initialized
}

std::vector<CuratedGroup> CuratedGroupManager::getGroupsForSearchType(SearchType searchType) const {
    std::vector<CuratedGroup> result;
    
    auto it = searchTypeToIndices_.find(searchType);
    if (it != searchTypeToIndices_.end()) {
        for (uint32_t index : it->second) {
            auto groupIt = groups_.find(index);
            if (groupIt != groups_.end()) {
                result.push_back(groupIt->second);
            }
        }
        
        // Sort by generation priority (highest first)
        std::sort(result.begin(), result.end(), 
                  [this](const CuratedGroup& a, const CuratedGroup& b) {
                      return calculateGroupPriority(a) > calculateGroupPriority(b);
                  });
    }
    
    return result;
}

const CuratedGroup* CuratedGroupManager::getGroupByIndex(uint32_t indexNumber) const {
    auto it = groups_.find(indexNumber);
    return (it != groups_.end()) ? &it->second : nullptr;
}

std::vector<const CuratedGroup*> CuratedGroupManager::getGroupsUsingComponent(PriceComponentType componentType) const {
    std::vector<const CuratedGroup*> result;
    
    auto it = componentToIndices_.find(componentType);
    if (it != componentToIndices_.end()) {
        for (uint32_t index : it->second) {
            auto groupIt = groups_.find(index);
            if (groupIt != groups_.end()) {
                result.push_back(&groupIt->second);
            }
        }
    }
    
    return result;
}

std::vector<const CuratedGroup*> CuratedGroupManager::getGroupsByComponentCount(size_t minComponents, size_t maxComponents) const {
    std::vector<const CuratedGroup*> result;
    
    for (const auto& [index, group] : groups_) {
        size_t componentCount = group.getComponentTypes().size();
        if (componentCount >= minComponents && componentCount <= maxComponents) {
            result.push_back(&group);
        }
    }
    
    return result;
}

ComponentTier CuratedGroupManager::getComponentTier(uint32_t indexNumber) const {
    // Based on PAL's discovered algorithm structure
    if (indexNumber >= FULL_OHLC_START && indexNumber <= FULL_OHLC_END) {
        return ComponentTier::FullOHLC;
    } else if (indexNumber >= MIXED_START && indexNumber <= MIXED_END) {
        return ComponentTier::Mixed;
    } else if (indexNumber >= DUAL_START && indexNumber <= DUAL_END) {
        return ComponentTier::Dual;
    } else if (indexNumber >= SINGLE_START && indexNumber <= SINGLE_END) {
        return ComponentTier::Single;
    }
    
    // Fallback: determine by component count
    auto it = groups_.find(indexNumber);
    if (it != groups_.end()) {
        return determineComponentTier(it->second.getComponentTypes());
    }
    
    return ComponentTier::Unknown;
}

std::vector<const CuratedGroup*> CuratedGroupManager::getGroupsByTier(ComponentTier tier) const {
    std::vector<const CuratedGroup*> result;
    
    auto it = tierToIndices_.find(tier);
    if (it != tierToIndices_.end()) {
        for (uint32_t index : it->second) {
            auto groupIt = groups_.find(index);
            if (groupIt != groups_.end()) {
                result.push_back(&groupIt->second);
            }
        }
    }
    
    return result;
}

std::vector<uint32_t> CuratedGroupManager::getOptimalGenerationOrder(SearchType searchType, bool prioritizeHighYield) const {
    std::vector<uint32_t> result;
    
    auto it = searchTypeToIndices_.find(searchType);
    if (it != searchTypeToIndices_.end()) {
        result = it->second;
        
        if (prioritizeHighYield) {
            // Sort by pattern count (descending) and then by component efficiency
            std::sort(result.begin(), result.end(), 
                      [this](uint32_t a, uint32_t b) {
                          auto groupA = groups_.find(a);
                          auto groupB = groups_.find(b);
                          if (groupA == groups_.end() || groupB == groups_.end()) return false;
                          
                          // Primary sort: pattern count
                          if (groupA->second.getPatternCount() != groupB->second.getPatternCount()) {
                              return groupA->second.getPatternCount() > groupB->second.getPatternCount();
                          }
                          
                          // Secondary sort: generation priority
                          return calculateGroupPriority(groupA->second) > calculateGroupPriority(groupB->second);
                      });
        } else {
            // Sort by component tier hierarchy (Full OHLC first, then Mixed, etc.)
            std::sort(result.begin(), result.end(), 
                      [this](uint32_t a, uint32_t b) {
                          ComponentTier tierA = getComponentTier(a);
                          ComponentTier tierB = getComponentTier(b);
                          
                          if (tierA != tierB) {
                              return static_cast<int>(tierA) < static_cast<int>(tierB);
                          }
                          
                          // Within same tier, sort by index number
                          return a < b;
                      });
        }
    }
    
    return result;
}

bool CuratedGroupManager::supportsChaining(uint32_t indexNumber) const {
    auto it = groups_.find(indexNumber);
    if (it != groups_.end()) {
        return it->second.isSupportingChaining();
    }
    return false;
}

size_t CuratedGroupManager::getRecommendedBatchSize(uint32_t indexNumber, size_t availableThreads) const {
    auto it = groups_.find(indexNumber);
    if (it == groups_.end() || availableThreads == 0) {
        return 1;
    }
    
    const CuratedGroup& group = it->second;
    
    // Base batch size on pattern count and available threads
    size_t baseBatchSize = std::max(static_cast<size_t>(1), static_cast<size_t>(group.getPatternCount()) / (availableThreads * 4));
    
    // Adjust based on component complexity
    size_t componentMultiplier = group.getComponentTypes().size();
    size_t adjustedBatchSize = baseBatchSize / componentMultiplier;
    
    // Ensure reasonable bounds
    return std::max(1ul, std::min(adjustedBatchSize, 10000ul));
}

std::map<PriceComponentType, ComponentUsageInfo> CuratedGroupManager::getComponentUsageStats() const {
    return componentUsageStats_;
}

bool CuratedGroupManager::validateGroupIntegrity() const {
    try {
        validateGroupConsistency();
        
        // Check that all expected PAL indices are present
        std::set<uint32_t> expectedIndices;
        
        // Full OHLC range
        for (uint32_t i = FULL_OHLC_START; i <= FULL_OHLC_END; ++i) {
            expectedIndices.insert(i);
        }
        
        // Mixed range
        for (uint32_t i = MIXED_START; i <= MIXED_END; ++i) {
            expectedIndices.insert(i);
        }
        
        // Dual range
        for (uint32_t i = DUAL_START; i <= DUAL_END; ++i) {
            expectedIndices.insert(i);
        }
        
        // Single range
        for (uint32_t i = SINGLE_START; i <= SINGLE_END; ++i) {
            expectedIndices.insert(i);
        }
        
        // Check coverage (allow for some missing indices as PAL may not use all)
        size_t foundCount = 0;
        for (uint32_t expected : expectedIndices) {
            if (groups_.find(expected) != groups_.end()) {
                foundCount++;
            }
        }
        
        double coverage = static_cast<double>(foundCount) / expectedIndices.size();
        bool hasGoodCoverage = coverage >= 0.8; // At least 80% coverage
        
        std::cout << "Group integrity check: " << foundCount << "/" << expectedIndices.size() 
                  << " indices (" << (coverage * 100.0) << "% coverage)" << std::endl;
        
        return hasGoodCoverage;
        
    } catch (const std::exception& e) {
        std::cerr << "Group integrity validation failed: " << e.what() << std::endl;
        return false;
    }
}

size_t CuratedGroupManager::getTotalPatternCount() const {
    return std::accumulate(groups_.begin(), groups_.end(), 0ul,
                          [](size_t sum, const auto& pair) {
                              return sum + pair.second.getPatternCount();
                          });
}

std::vector<const CuratedGroup*> CuratedGroupManager::getPreComputationCandidates(uint32_t minPatternCount) const {
    std::vector<const CuratedGroup*> candidates;
    
    for (const auto& [index, group] : groups_) {
        if (group.getPatternCount() >= minPatternCount) {
            candidates.push_back(&group);
        }
    }
    
    // Sort by pattern count (descending)
    std::sort(candidates.begin(), candidates.end(),
              [](const CuratedGroup* a, const CuratedGroup* b) {
                  return a->getPatternCount() > b->getPatternCount();
              });
    
    return candidates;
}

void CuratedGroupManager::buildSearchTypeIndex() {
    searchTypeToIndices_.clear();
    
    for (const auto& [index, group] : groups_) {
        searchTypeToIndices_[group.getSearchType()].push_back(index);
    }
    
    // Sort indices within each search type
    for (auto& [searchType, indices] : searchTypeToIndices_) {
        std::sort(indices.begin(), indices.end());
    }
}

void CuratedGroupManager::buildComponentTierIndex() {
    tierToIndices_.clear();
    
    for (const auto& [index, group] : groups_) {
        ComponentTier tier = getComponentTier(index);
        tierToIndices_[tier].push_back(index);
    }
    
    // Sort indices within each tier
    for (auto& [tier, indices] : tierToIndices_) {
        std::sort(indices.begin(), indices.end());
    }
}

void CuratedGroupManager::buildComponentUsageIndex() {
    componentToIndices_.clear();
    
    for (const auto& [index, group] : groups_) {
        for (PriceComponentType component : group.getComponentTypes()) {
            componentToIndices_[component].push_back(index);
        }
    }
    
    // Sort indices within each component type
    for (auto& [component, indices] : componentToIndices_) {
        std::sort(indices.begin(), indices.end());
    }
}

void CuratedGroupManager::calculateComponentUsageStats(const std::map<SearchType, ComponentUsageStats>& palStats) {
    componentUsageStats_.clear();
    
    // Aggregate usage across all search types
    std::map<PriceComponentType, uint32_t> totalUsage;
    uint32_t grandTotal = 0;
    
    for (const auto& [searchType, stats] : palStats) {
        for (const auto& [component, usage] : stats.getComponentUsage()) {
            totalUsage[component] += usage;
            grandTotal += usage;
        }
    }
    
    // Build component usage info
    for (const auto& [component, usage] : totalUsage) {
        double usagePercentage = (grandTotal > 0) ? (static_cast<double>(usage) / grandTotal * 100.0) : 0.0;
        std::vector<uint32_t> associatedIndices = componentToIndices_[component];
        
        // Determine primary tier based on where this component is most used
        std::map<ComponentTier, uint32_t> tierUsage;
        for (uint32_t index : associatedIndices) {
            ComponentTier tier = getComponentTier(index);
            auto groupIt = groups_.find(index);
            if (groupIt != groups_.end()) {
                tierUsage[tier] += groupIt->second.getPatternCount();
            }
        }
        
        // Find tier with highest usage
        auto maxTierIt = std::max_element(tierUsage.begin(), tierUsage.end(),
                                         [](const auto& a, const auto& b) {
                                             return a.second < b.second;
                                         });
        ComponentTier primaryTier = (maxTierIt != tierUsage.end()) ? maxTierIt->first : ComponentTier::Unknown;
        
        // Mark as high efficiency based on PAL's discovered patterns
        // CLOSE dominance: 37.7%, HIGH/LOW balance, OPEN specialization: 13.4%
        bool isHighEfficiency = (component == PriceComponentType::CLOSE && usagePercentage > 35.0) ||
                               (component == PriceComponentType::HIGH && usagePercentage > 20.0) ||
                               (component == PriceComponentType::LOW && usagePercentage > 20.0) ||
                               (component == PriceComponentType::OPEN && usagePercentage > 10.0);
        
        ComponentUsageInfo info(usage, usagePercentage, associatedIndices, primaryTier, isHighEfficiency);
        componentUsageStats_[component] = info;
    }
}

ComponentTier CuratedGroupManager::determineComponentTier(const std::set<PriceComponentType>& components) const {
    size_t componentCount = components.size();
    
    if (componentCount == 4) {
        return ComponentTier::FullOHLC;
    } else if (componentCount == 3) {
        return ComponentTier::Mixed;
    } else if (componentCount == 2) {
        return ComponentTier::Dual;
    } else if (componentCount == 1) {
        return ComponentTier::Single;
    }
    
    return ComponentTier::Unknown;
}

double CuratedGroupManager::calculateGroupPriority(const CuratedGroup& group) const {
    // Weighted priority based on pattern count and component efficiency
    double patternWeight = static_cast<double>(group.getPatternCount()) / 10000.0; // Normalize
    
    // Component efficiency based on usage statistics
    double componentEfficiency = 0.0;
    for (PriceComponentType component : group.getComponentTypes()) {
        auto it = componentUsageStats_.find(component);
        if (it != componentUsageStats_.end()) {
            componentEfficiency += it->second.getUsagePercentage() / 100.0;
        }
    }
    componentEfficiency /= group.getComponentTypes().size(); // Average efficiency
    
    return (PRIORITY_WEIGHT_PATTERN_COUNT * patternWeight) +
           (PRIORITY_WEIGHT_COMPONENT_EFFICIENCY * componentEfficiency);
}

void CuratedGroupManager::validateGroupConsistency() const {
    for (const auto& [index, group] : groups_) {
        // Validate index consistency
        if (group.getIndexNumber() != index) {
            throw std::runtime_error("Index mismatch for group " + std::to_string(index));
        }
        
        // Validate component types are not empty
        if (group.getComponentTypes().empty()) {
            throw std::runtime_error("Empty component types for group " + std::to_string(index));
        }
        
        // Validate bar offsets are not empty
        if (group.getBarOffsets().empty()) {
            throw std::runtime_error("Empty bar offsets for group " + std::to_string(index));
        }
        
        // Validate pattern length constraints
        if (group.getMinPatternLength() > group.getMaxPatternLength()) {
            throw std::runtime_error("Invalid pattern length range for group " + std::to_string(index));
        }
    }
}

// Factory implementations
std::unique_ptr<CuratedGroupManager> CuratedGroupManagerFactory::createFromPALAnalysis(const PALAnalysisData& analysisData) {
    return std::make_unique<CuratedGroupManager>(analysisData);
}

std::unique_ptr<CuratedGroupManager> CuratedGroupManagerFactory::createWithOptimization(
    const PALAnalysisData& analysisData,
    const GroupOptimizationSettings& /*optimizationSettings*/) {
    
    auto manager = std::make_unique<CuratedGroupManager>(analysisData);
    
    // Apply optimization settings (could extend this for custom optimizations)
    // For now, the standard manager handles all optimizations based on PAL analysis
    
    return manager;
}

} // namespace pattern_universe