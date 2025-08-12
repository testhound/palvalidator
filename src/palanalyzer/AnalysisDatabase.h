#pragma once

#include "DataStructures.h"
#include <string>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include <mutex>

namespace palanalyzer {

/**
 * @brief Persistent storage and retrieval of PAL pattern analysis data
 * 
 * This class manages the analysis database with incremental updates,
 * consistency validation, and efficient data access patterns.
 */
class AnalysisDatabase {
public:
    /**
     * @brief Construct database with specified file path
     * @param dbPath Path to the database file (JSON format)
     */
    explicit AnalysisDatabase(const std::string& dbPath);
    
    /**
     * @brief Destructor - ensures data is saved if modified
     */
    ~AnalysisDatabase();
    
    // Database operations
    /**
     * @brief Load database from file
     * @return True if successful, false if file doesn't exist or is corrupted
     */
    bool load();
    
    /**
     * @brief Save database to file
     * @return True if successful, false on write error
     */
    bool save();
    
    /**
     * @brief Clear all data from database
     */
    void clear();
    
    /**
     * @brief Check if database has been modified since last save
     * @return True if modified, false otherwise
     */
    bool isModified() const { return modified; }
    
    // Data access methods
    /**
     * @brief Add pattern analysis to database
     * @param pattern Pattern analysis data
     */
    void addPattern(const PatternAnalysis& pattern);
    
    /**
     * @brief Update index group information
     * @param index Index number
     * @param info Index group information
     */
    void updateIndexGroup(uint32_t index, const IndexGroupInfo& info);
    
    /**
     * @brief Add pattern to index group
     * @param index Index number
     * @param barCombination Bar combination for this pattern
     * @param componentTypes Component types used
     * @param sourceFile Source file
     */
    void addPatternToIndexGroup(uint32_t index,
                               const std::vector<uint8_t>& barCombination,
                               const std::set<PriceComponentType>& componentTypes,
                               const std::string& sourceFile,
                               const std::string& searchType);
    
    /**
     * @brief Check if index exists in database
     * @param index Index number to check
     * @return True if index exists, false otherwise
     */
    bool hasIndex(uint32_t index) const;
    
    /**
     * @brief Get index group info
     * @param index Index number
     * @return Index group information
     * @throws std::out_of_range if index doesn't exist
     */
    IndexGroupInfo getIndexGroupInfo(uint32_t index) const;
    
    /**
     * @brief Update index mapping information (legacy support)
     * @param index Index number
     * @param info Bar combination information
     */
    void updateIndexMapping(uint32_t index, const BarCombinationInfo& info);
    
    /**
     * @brief Get bar combination info for index (legacy support)
     * @param index Index number
     * @return Bar combination information
     * @throws std::out_of_range if index doesn't exist
     */
    BarCombinationInfo getIndexInfo(uint32_t index) const;
    
    /**
     * @brief Add analyzed file to tracking
     * @param fileInfo File analysis information
     */
    void addAnalyzedFile(const FileAnalysisInfo& fileInfo);
    
    /**
     * @brief Check if file has been analyzed
     * @param filePath File path to check
     * @return True if file has been analyzed, false otherwise
     */
    bool isFileAnalyzed(const std::string& filePath) const;
    
    /**
     * @brief Update search type statistics
     * @param searchType Search type name
     * @param stats Statistics to update
     */
    void updateSearchTypeStats(const std::string& searchType, const SearchTypeStats& stats);
    
    // Statistics and reporting
    /**
     * @brief Get total number of patterns analyzed
     * @return Total pattern count
     */
    size_t getTotalPatterns() const;
    
    /**
     * @brief Get number of unique indices discovered
     * @return Unique index count
     */
    size_t getUniqueIndices() const;
    
    /**
     * @brief Get set of analyzed file paths
     * @return Set of analyzed file paths
     */
    std::set<std::string> getAnalyzedFiles() const;
    
    /**
     * @brief Get overall analysis statistics
     * @return Analysis statistics
     */
    AnalysisStats getStats() const;
    
    /**
     * @brief Get search type statistics
     * @param searchType Search type name
     * @return Search type statistics
     */
    SearchTypeStats getSearchTypeStats(const std::string& searchType) const;
    
    /**
     * @brief Get all index mappings
     * @return Map of index to bar combination info
     */
    const std::map<uint32_t, BarCombinationInfo>& getIndexMappings() const {
        return indexToBarCombination;
    }
    
    /**
     * @brief Get all pattern analyses
     * @return Vector of all pattern analyses
     */
    const std::vector<PatternAnalysis>& getAllPatterns() const {
        return allPatterns;
    }
    
    /**
     * @brief Get all search type statistics
     * @return Map of search type to statistics
     */
    const std::map<std::string, SearchTypeStats>& getAllSearchTypeStats() const {
        return searchTypeAnalysis;
    }
    
    /**
     * @brief Get all index groups
     * @return Map of index to index group info
     */
    const std::map<uint32_t, IndexGroupInfo>& getIndexGroups() const {
        return indexGroups;
    }
    
    /**
     * @brief Validate index consistency
     * @param index Index number
     * @param newInfo New bar combination info
     * @return True if consistent, false if conflict detected
     */
    bool validateIndexConsistency(uint32_t index, const BarCombinationInfo& newInfo);
    
    /**
     * @brief Get database file path
     * @return Database file path
     */
    const std::string& getDbPath() const { return dbPath; }

private:
    // Database file path
    std::string dbPath;
    
    // Core data structures
    std::map<uint32_t, IndexGroupInfo> indexGroups;
    std::map<uint32_t, BarCombinationInfo> indexToBarCombination; // Legacy support
    std::map<std::string, SearchTypeStats> searchTypeAnalysis;
    std::vector<PatternAnalysis> allPatterns;
    std::map<std::string, FileAnalysisInfo> analyzedFiles;
    
    // Metadata
    std::chrono::system_clock::time_point lastUpdated;
    std::chrono::system_clock::time_point created;
    bool modified;
    
    // Thread safety (recursive to handle single-thread recursive calls)
    mutable std::recursive_mutex dataMutex;
    
    // Helper methods
    void updateLastModified();
    void mergeBarCombinationInfo(BarCombinationInfo& existing, const BarCombinationInfo& newInfo);
    void updateSearchTypeStatsFromPattern(const PatternAnalysis& pattern);
};

} // namespace palanalyzer