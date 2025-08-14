#pragma once

#include "DataStructures.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>

namespace palanalyzer {

// Forward declaration
class AnalysisDatabase;

/**
 * @brief Registry statistics for pattern lookup operations
 *
 * This class provides read-only access to registry statistics. While the constructor
 * and update methods are public, the intended usage is via SimplifiedPatternRegistry::getRegistryStats()
 * factory method for proper encapsulation.
 */
class RegistryStats {
public:
    /**
     * @brief Construct registry statistics with basic counts
     *
     * @param totalPatterns Total number of patterns in registry
     * @param uniqueHashes Number of unique hash codes
     * @param totalGroups Total number of pattern groups
     */
    RegistryStats(size_t totalPatterns, size_t uniqueHashes, size_t totalGroups);

    size_t getTotalPatterns() const
    {
        return m_totalPatterns;
    }

    size_t getUniqueHashes() const
    {
        return m_uniqueHashes;
    }

    size_t getTotalGroups() const
    {
        return m_totalGroups;
    }

    /**
     * @brief Get pattern count for a specific search type
     *
     * @param searchType The search type to query
     * @return Number of patterns for the search type, 0 if not found
     */
    size_t getSearchTypeCount(SearchType searchType) const;

    /**
     * @brief Get all search types that have patterns
     *
     * @return Vector of search types with registered patterns
     */
    std::vector<SearchType> getAvailableSearchTypes() const;

    /**
     * @brief Get pattern count for a specific group
     *
     * @param groupId The group ID to query
     * @return Number of patterns in the group, 0 if not found
     */
    size_t getGroupSize(uint32_t groupId) const;

    /**
     * @brief Get all group IDs that have patterns
     *
     * @return Vector of group IDs with registered patterns
     */
    std::vector<uint32_t> getAvailableGroups() const;

    /**
     * @brief Add search type statistics during construction
     *
     * @param searchType The search type
     * @param count Number of patterns for this search type
     */
    void addSearchTypeData(SearchType searchType, size_t count);

    /**
     * @brief Add group size statistics during construction
     *
     * @param groupId The group ID
     * @param size Number of patterns in this group
     */
    void addGroupData(uint32_t groupId, size_t size);

private:
    size_t m_totalPatterns;
    size_t m_uniqueHashes;
    size_t m_totalGroups;
    std::map<SearchType, size_t> m_searchTypeBreakdown;
    std::map<uint32_t, size_t> m_groupSizeDistribution;
};

/**
 * @brief Centralized registry for efficient pattern lookup and management using hash-based indexing
 * 
 * This class provides O(1) pattern lookup operations and maintains indices for efficient
 * pattern queries by hash, group, and search type. It serves as the primary interface
 * for pattern existence checks and retrieval operations.
 */
class SimplifiedPatternRegistry {
public:
    /**
     * @brief Construct a new Simplified Pattern Registry
     * 
     * @param database Reference to the analysis database for pattern data
     */
    explicit SimplifiedPatternRegistry(const AnalysisDatabase& database);

    /**
     * @brief Register a single pattern in the registry
     * 
     * @param pattern The pattern structure to register
     */
    void registerPattern(const PatternStructure& pattern);

    /**
     * @brief Register multiple patterns in a batch operation
     * 
     * @param patterns Vector of pattern structures to register
     */
    void registerPatternBatch(const std::vector<PatternStructure>& patterns);

    /**
     * @brief Find a pattern by its hash code (O(1) operation)
     * 
     * @param patternHash The hash code to search for
     * @return Optional pattern structure if found, empty if not found
     */
    std::optional<PatternStructure> findByHash(unsigned long long patternHash) const;

    /**
     * @brief Find all patterns belonging to a specific group
     * 
     * @param groupId The group ID to search for
     * @return Vector of pattern structures in the group
     */
    std::vector<PatternStructure> findByGroup(uint32_t groupId) const;

    /**
     * @brief Find all patterns of a specific search type
     * 
     * @param searchType The search type to filter by
     * @return Vector of pattern structures matching the search type
     */
    std::vector<PatternStructure> findBySearchType(SearchType searchType) const;

    /**
     * @brief Check if a pattern exists in the registry
     * 
     * @param patternHash The hash code to check
     * @return True if pattern exists, false otherwise
     */
    bool exists(unsigned long long patternHash) const;

    /**
     * @brief Check if a pattern exists in a specific group
     * 
     * @param patternHash The hash code to check
     * @param groupId The group ID to check within
     * @return True if pattern exists in the group, false otherwise
     */
    bool existsInGroup(unsigned long long patternHash, uint32_t groupId) const;

    /**
     * @brief Get comprehensive registry statistics
     * 
     * @return RegistryStats with detailed registry information
     */
    RegistryStats getRegistryStats() const;

    /**
     * @brief Rebuild all indices from the database
     * 
     * This method reconstructs all internal indices from the current database state.
     * Use this after significant database changes or for maintenance operations.
     */
    void rebuild();

    /**
     * @brief Clear all registry data and indices
     * 
     * This method removes all patterns and indices from the registry.
     * The registry will be empty after this operation.
     */
    void clear();

    /**
     * @brief Get the total number of patterns in the registry
     * 
     * @return Number of patterns currently registered
     */
    size_t size() const;

    /**
     * @brief Check if the registry is empty
     * 
     * @return True if registry contains no patterns, false otherwise
     */
    bool isEmpty() const;

    /**
     * @brief Get all unique hash codes in the registry
     * 
     * @return Vector of all pattern hash codes
     */
    std::vector<unsigned long long> getAllHashes() const;

    /**
     * @brief Get all group IDs that have patterns
     * 
     * @return Vector of group IDs with registered patterns
     */
    std::vector<uint32_t> getAllGroupIds() const;

    /**
     * @brief Get all search types that have patterns
     * 
     * @return Vector of search types with registered patterns
     */
    std::vector<SearchType> getAllSearchTypes() const;

private:
    const AnalysisDatabase& m_database;

    // Hash-based indices for O(1) lookups
    std::unordered_map<unsigned long long, PatternStructure> m_hashIndex;
    std::unordered_map<uint32_t, std::unordered_set<unsigned long long>> m_groupIndex;
    std::unordered_map<SearchType, std::unordered_set<unsigned long long>> m_searchTypeIndex;

    /**
     * @brief Build all indices from the database
     * 
     * This method constructs the hash, group, and search type indices
     * from the current database state.
     */
    void buildIndices();

    /**
     * @brief Update indices with a new pattern
     * 
     * @param pattern The pattern to add to the indices
     */
    void updateIndices(const PatternStructure& pattern);

    /**
     * @brief Remove a pattern from all indices
     * 
     * @param patternHash The hash of the pattern to remove
     */
    void removeFromIndices(unsigned long long patternHash);

    /**
     * @brief Convert search type string to SearchType enum
     * 
     * @param searchTypeStr String representation of search type
     * @return SearchType enum value
     */
    SearchType parseSearchType(const std::string& searchTypeStr) const;

    /**
     * @brief Validate that a pattern structure is consistent
     * 
     * @param pattern The pattern structure to validate
     * @return True if pattern is consistent, false otherwise
     */
    bool isPatternConsistent(const PatternStructure& pattern) const;
};

} // namespace palanalyzer