#include "SimplifiedPatternRegistry.h"
#include "AnalysisDatabase.h"
#include <algorithm>
#include <iostream>

namespace palanalyzer {

RegistryStats::RegistryStats(size_t totalPatterns, size_t uniqueHashes, size_t totalGroups)
    : m_totalPatterns(totalPatterns),
      m_uniqueHashes(uniqueHashes),
      m_totalGroups(totalGroups)
{
}

size_t RegistryStats::getSearchTypeCount(SearchType searchType) const
{
    auto it = m_searchTypeBreakdown.find(searchType);
    return (it != m_searchTypeBreakdown.end()) ? it->second : 0;
}

std::vector<SearchType> RegistryStats::getAvailableSearchTypes() const
{
    std::vector<SearchType> searchTypes;
    searchTypes.reserve(m_searchTypeBreakdown.size());
    
    for (const auto& [searchType, count] : m_searchTypeBreakdown)
    {
        searchTypes.push_back(searchType);
    }
    
    return searchTypes;
}

size_t RegistryStats::getGroupSize(uint32_t groupId) const
{
    auto it = m_groupSizeDistribution.find(groupId);
    return (it != m_groupSizeDistribution.end()) ? it->second : 0;
}

std::vector<uint32_t> RegistryStats::getAvailableGroups() const
{
    std::vector<uint32_t> groupIds;
    groupIds.reserve(m_groupSizeDistribution.size());
    
    for (const auto& [groupId, size] : m_groupSizeDistribution)
    {
        groupIds.push_back(groupId);
    }
    
    return groupIds;
}

void RegistryStats::addSearchTypeData(SearchType searchType, size_t count)
{
    m_searchTypeBreakdown[searchType] = count;
}

void RegistryStats::addGroupData(uint32_t groupId, size_t size)
{
    m_groupSizeDistribution[groupId] = size;
}

SimplifiedPatternRegistry::SimplifiedPatternRegistry(const AnalysisDatabase& database)
    : m_database(database)
{
    buildIndices();
}

void SimplifiedPatternRegistry::registerPattern(const PatternStructure& pattern)
{
    if (!isPatternConsistent(pattern))
    {
        std::cerr << "Warning: Inconsistent pattern structure detected for hash: " 
                  << pattern.getPatternHash() << std::endl;
        return;
    }
    
    updateIndices(pattern);
}

void SimplifiedPatternRegistry::registerPatternBatch(const std::vector<PatternStructure>& patterns)
{
    for (const auto& pattern : patterns)
    {
        registerPattern(pattern);
    }
}

std::optional<PatternStructure> SimplifiedPatternRegistry::findByHash(unsigned long long patternHash) const
{
    auto it = m_hashIndex.find(patternHash);
    if (it != m_hashIndex.end())
    {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<PatternStructure> SimplifiedPatternRegistry::findByGroup(uint32_t groupId) const
{
    std::vector<PatternStructure> patterns;
    
    auto groupIt = m_groupIndex.find(groupId);
    if (groupIt != m_groupIndex.end())
    {
        const auto& hashSet = groupIt->second;
        patterns.reserve(hashSet.size());
        
        for (unsigned long long hash : hashSet)
        {
            auto patternIt = m_hashIndex.find(hash);
            if (patternIt != m_hashIndex.end())
            {
                patterns.push_back(patternIt->second);
            }
        }
    }
    
    return patterns;
}

std::vector<PatternStructure> SimplifiedPatternRegistry::findBySearchType(SearchType searchType) const
{
    std::vector<PatternStructure> patterns;
    
    auto searchIt = m_searchTypeIndex.find(searchType);
    if (searchIt != m_searchTypeIndex.end())
    {
        const auto& hashSet = searchIt->second;
        patterns.reserve(hashSet.size());
        
        for (unsigned long long hash : hashSet)
        {
            auto patternIt = m_hashIndex.find(hash);
            if (patternIt != m_hashIndex.end())
            {
                patterns.push_back(patternIt->second);
            }
        }
    }
    
    return patterns;
}

bool SimplifiedPatternRegistry::exists(unsigned long long patternHash) const
{
    return m_hashIndex.find(patternHash) != m_hashIndex.end();
}

bool SimplifiedPatternRegistry::existsInGroup(unsigned long long patternHash, uint32_t groupId) const
{
    auto groupIt = m_groupIndex.find(groupId);
    if (groupIt != m_groupIndex.end())
    {
        const auto& hashSet = groupIt->second;
        return hashSet.find(patternHash) != hashSet.end();
    }
    
    return false;
}

RegistryStats SimplifiedPatternRegistry::getRegistryStats() const
{
    // Factory method - calculate statistics internally
    RegistryStats stats(
        m_hashIndex.size(),
        m_hashIndex.size(), // Unique hashes same as total patterns (no duplicates)
        m_groupIndex.size()
    );
    
    // Add search type breakdown
    for (const auto& [searchType, hashSet] : m_searchTypeIndex)
    {
        stats.addSearchTypeData(searchType, hashSet.size());
    }
    
    // Add group size distribution
    for (const auto& [groupId, hashSet] : m_groupIndex)
    {
        stats.addGroupData(groupId, hashSet.size());
    }
    
    return stats;
}

void SimplifiedPatternRegistry::rebuild()
{
    clear();
    buildIndices();
}

void SimplifiedPatternRegistry::clear()
{
    m_hashIndex.clear();
    m_groupIndex.clear();
    m_searchTypeIndex.clear();
}

size_t SimplifiedPatternRegistry::size() const
{
    return m_hashIndex.size();
}

bool SimplifiedPatternRegistry::isEmpty() const
{
    return m_hashIndex.empty();
}

std::vector<unsigned long long> SimplifiedPatternRegistry::getAllHashes() const
{
    std::vector<unsigned long long> hashes;
    hashes.reserve(m_hashIndex.size());
    
    for (const auto& [hash, pattern] : m_hashIndex)
    {
        hashes.push_back(hash);
    }
    
    return hashes;
}

std::vector<uint32_t> SimplifiedPatternRegistry::getAllGroupIds() const
{
    std::vector<uint32_t> groupIds;
    groupIds.reserve(m_groupIndex.size());
    
    for (const auto& [groupId, hashSet] : m_groupIndex)
    {
        groupIds.push_back(groupId);
    }
    
    return groupIds;
}

std::vector<SearchType> SimplifiedPatternRegistry::getAllSearchTypes() const
{
    std::vector<SearchType> searchTypes;
    searchTypes.reserve(m_searchTypeIndex.size());
    
    for (const auto& [searchType, hashSet] : m_searchTypeIndex)
    {
        searchTypes.push_back(searchType);
    }
    
    return searchTypes;
}

void SimplifiedPatternRegistry::buildIndices()
{
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            const auto& patterns = groupInfo.getPatterns();
            SearchType searchType = parseSearchType(groupInfo.getSearchType());
            
            for (const auto& [hashStr, pattern] : patterns)
            {
                // Add to hash index
                m_hashIndex.emplace(pattern.getPatternHash(), pattern);
                
                // Add to group index
                m_groupIndex[groupId].insert(pattern.getPatternHash());
                
                // Add to search type index
                m_searchTypeIndex[searchType].insert(pattern.getPatternHash());
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error building registry indices: " << e.what() << std::endl;
    }
}

void SimplifiedPatternRegistry::updateIndices(const PatternStructure& pattern)
{
    unsigned long long hash = pattern.getPatternHash();
    uint32_t groupId = static_cast<uint32_t>(pattern.getGroupId());
    
    // Add to hash index
    m_hashIndex.emplace(hash, pattern);
    
    // Add to group index
    m_groupIndex[groupId].insert(hash);
    
    // Determine search type from group info
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        auto groupIt = indexGroups.find(groupId);
        
        if (groupIt != indexGroups.end())
        {
            SearchType searchType = parseSearchType(groupIt->second.getSearchType());
            m_searchTypeIndex[searchType].insert(hash);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Warning: Could not determine search type for pattern hash: " << hash << std::endl;
    }
}

void SimplifiedPatternRegistry::removeFromIndices(unsigned long long patternHash)
{
    // Remove from hash index
    auto hashIt = m_hashIndex.find(patternHash);
    if (hashIt == m_hashIndex.end())
    {
        return; // Pattern not found
    }
    
    uint32_t groupId = static_cast<uint32_t>(hashIt->second.getGroupId());
    m_hashIndex.erase(hashIt);
    
    // Remove from group index
    auto groupIt = m_groupIndex.find(groupId);
    if (groupIt != m_groupIndex.end())
    {
        groupIt->second.erase(patternHash);
        
        // Remove empty group
        if (groupIt->second.empty())
        {
            m_groupIndex.erase(groupIt);
        }
    }
    
    // Remove from search type indices
    for (auto& [searchType, hashSet] : m_searchTypeIndex)
    {
        hashSet.erase(patternHash);
    }
    
    // Clean up empty search type entries
    auto searchIt = m_searchTypeIndex.begin();
    while (searchIt != m_searchTypeIndex.end())
    {
        if (searchIt->second.empty())
        {
            searchIt = m_searchTypeIndex.erase(searchIt);
        }
        else
        {
            ++searchIt;
        }
    }
}

SearchType SimplifiedPatternRegistry::parseSearchType(const std::string& searchTypeStr) const
{
    return stringToSearchType(searchTypeStr);
}

bool SimplifiedPatternRegistry::isPatternConsistent(const PatternStructure& pattern) const
{
    // Basic consistency checks
    if (pattern.getPatternHash() == 0)
    {
        return false;
    }
    
    if (pattern.getGroupId() < 0)
    {
        return false;
    }
    
    if (pattern.getConditions().empty())
    {
        return false;
    }
    
    if (pattern.getConditionCount() != static_cast<int>(pattern.getConditions().size()))
    {
        return false;
    }
    
    if (pattern.getComponentsUsed().empty())
    {
        return false;
    }
    
    if (pattern.getBarOffsetsUsed().empty())
    {
        return false;
    }
    
    return true;
}

} // namespace palanalyzer