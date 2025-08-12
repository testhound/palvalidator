#include "AnalysisDatabase.h"
#include "AnalysisSerializer.h"
#include <iostream>
#include <algorithm>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace palanalyzer {

AnalysisDatabase::AnalysisDatabase(const std::string& dbPath) 
    : dbPath(dbPath), modified(false) {
    created = std::chrono::system_clock::now();
    lastUpdated = created;
}

AnalysisDatabase::~AnalysisDatabase() {
    if (modified) {
        std::cout << "Warning: Database has unsaved changes" << std::endl;
    }
}

bool AnalysisDatabase::load() {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    if (!fs::exists(dbPath)) {
        return false; // File doesn't exist, not an error
    }
    
    bool success = AnalysisSerializer::loadFromFile(*this, dbPath);
    if (success) {
        modified = false;
    }
    
    return success;
}

bool AnalysisDatabase::save() {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    // Ensure directory exists
    fs::path dbFilePath(dbPath);
    fs::path parentDir = dbFilePath.parent_path();
    
    if (!parentDir.empty() && !fs::exists(parentDir)) {
        try {
            fs::create_directories(parentDir);
        } catch (const std::exception& e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
            return false;
        }
    }
    
    updateLastModified();
    bool success = AnalysisSerializer::saveToFile(*this, dbPath);
    if (success) {
        modified = false;
    }
    
    return success;
}

void AnalysisDatabase::clear() {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    indexToBarCombination.clear();
    searchTypeAnalysis.clear();
    allPatterns.clear();
    analyzedFiles.clear();
    
    created = std::chrono::system_clock::now();
    updateLastModified();
}

void AnalysisDatabase::addPattern(const PatternAnalysis& pattern) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    allPatterns.push_back(pattern);
    updateSearchTypeStatsFromPattern(pattern);

    auto it = indexGroups.find(pattern.getIndex());
    if (it != indexGroups.end()) {
        std::vector<PatternCondition> conditions;
        const auto& components = pattern.getComponents();
        for (size_t i = 0; i + 1 < components.size(); i += 2) {
            conditions.emplace_back("GreaterThan", components[i], components[i+1]);
        }

        std::vector<std::string> componentsUsed;
        std::vector<int> barOffsetsUsed;
        std::set<std::string> uniqueComponents;
        std::set<int> uniqueBarOffsets;

        for(const auto& component : pattern.getComponents()) {
            if(uniqueComponents.find(componentTypeToString(component.getType())) == uniqueComponents.end()) {
                uniqueComponents.insert(componentTypeToString(component.getType()));
                componentsUsed.push_back(componentTypeToString(component.getType()));
            }
            if(uniqueBarOffsets.find(component.getBarOffset()) == uniqueBarOffsets.end()) {
                uniqueBarOffsets.insert(component.getBarOffset());
                barOffsetsUsed.push_back(component.getBarOffset());
            }
        }

        PatternStructure patternStructure(
            pattern.getPatternHash(),
            pattern.getIndex(),
            conditions,
            pattern.getConditionCount(),
            componentsUsed,
            barOffsetsUsed
        );
        it->second.addPattern(std::to_string(pattern.getPatternHash()), patternStructure);
    }

    updateLastModified();
}

void AnalysisDatabase::updateIndexMapping(uint32_t index, const BarCombinationInfo& info) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    auto it = indexToBarCombination.find(index);
    if (it != indexToBarCombination.end()) {
        // Merge with existing info
        mergeBarCombinationInfo(it->second, info);
    } else {
        // Add new mapping
        indexToBarCombination.emplace(index, info);
    }
    
    updateLastModified();
}

void AnalysisDatabase::updateIndexGroup(uint32_t index, const IndexGroupInfo& info) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    indexGroups.emplace(index, info);
    updateLastModified();
}

void AnalysisDatabase::addPatternToIndexGroup(uint32_t index,
                                             const std::vector<uint8_t>& barCombination,
                                             const std::set<PriceComponentType>& componentTypes,
                                             const std::string& sourceFile,
                                             const std::string& searchType) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    auto it = indexGroups.find(index);
    if (it == indexGroups.end()) {
        // Create new index group
        IndexGroupInfo newGroup(index, searchType, sourceFile, barCombination, componentTypes);
        
        indexGroups.emplace(index, newGroup);
    } else {
        // Update existing index group
        IndexGroupInfo& group = it->second;
        group.updateExistingGroup(searchType, sourceFile, barCombination, componentTypes);
    }
    
    updateLastModified();
}

IndexGroupInfo AnalysisDatabase::getIndexGroupInfo(uint32_t index) const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    auto it = indexGroups.find(index);
    if (it == indexGroups.end()) {
        throw std::out_of_range("Index group not found: " + std::to_string(index));
    }
    
    return it->second;
}

bool AnalysisDatabase::hasIndex(uint32_t index) const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    // Check both legacy and new index group storage
    return indexToBarCombination.find(index) != indexToBarCombination.end() ||
           indexGroups.find(index) != indexGroups.end();
}

BarCombinationInfo AnalysisDatabase::getIndexInfo(uint32_t index) const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    auto it = indexToBarCombination.find(index);
    if (it == indexToBarCombination.end()) {
        throw std::out_of_range("Index not found: " + std::to_string(index));
    }
    
    return it->second;
}

void AnalysisDatabase::addAnalyzedFile(const FileAnalysisInfo& fileInfo) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    analyzedFiles.emplace(fileInfo.getPath(), fileInfo);
    updateLastModified();
}

bool AnalysisDatabase::isFileAnalyzed(const std::string& filePath) const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    return analyzedFiles.find(filePath) != analyzedFiles.end();
}

void AnalysisDatabase::updateSearchTypeStats(const std::string& searchType, const SearchTypeStats& stats) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    searchTypeAnalysis.emplace(searchType, stats);
    updateLastModified();
}

size_t AnalysisDatabase::getTotalPatterns() const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    return allPatterns.size();
}

size_t AnalysisDatabase::getUniqueIndices() const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    return indexToBarCombination.size();
}

std::set<std::string> AnalysisDatabase::getAnalyzedFiles() const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    std::set<std::string> files;
    for (const auto& pair : analyzedFiles) {
        files.insert(pair.first);
    }
    
    return files;
}

AnalysisStats AnalysisDatabase::getStats() const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    AnalysisStats stats(allPatterns.size(), indexToBarCombination.size(), analyzedFiles.size(), lastUpdated, created);
    
    // Calculate search type breakdown
    for (const auto& pair : searchTypeAnalysis) {
        stats.addSearchTypeBreakdown(pair.first, pair.second.getTotalPatterns());
    }
    
    return stats;
}

SearchTypeStats AnalysisDatabase::getSearchTypeStats(const std::string& searchType) const {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    auto it = searchTypeAnalysis.find(searchType);
    if (it != searchTypeAnalysis.end()) {
        return it->second;
    }
    
    // Return empty stats if not found
    return SearchTypeStats(0, std::chrono::system_clock::now());
}

bool AnalysisDatabase::validateIndexConsistency(uint32_t index, const BarCombinationInfo& newInfo) {
    std::lock_guard<std::recursive_mutex> lock(dataMutex);
    
    auto it = indexToBarCombination.find(index);
    if (it == indexToBarCombination.end()) {
        return true; // No existing info, so consistent
    }
    
    const BarCombinationInfo& existing = it->second;
    
    // For Index groups, we expect variation in patterns
    // Indices can legitimately appear in different search types (e.g., Extended is subset of Deep)
    // This is normal behavior, not an inconsistency
    if (existing.getSearchType() != newInfo.getSearchType()) {
        // This is expected - same patterns can appear in multiple search types
        // Extended search is a subset of Deep search, so overlap is normal
        // No warning needed, just continue processing
    }
    
    // Index groups can have different bar combinations and component types
    // This is expected behavior - Index represents pattern classification, not unique patterns
    return true;
}

void AnalysisDatabase::updateLastModified() {
    lastUpdated = std::chrono::system_clock::now();
    modified = true;
}

void AnalysisDatabase::mergeBarCombinationInfo(BarCombinationInfo& existing, const BarCombinationInfo& newInfo) {
    // Update pattern count
    existing.updatePatternCount(newInfo.getPatternCount());
    
    // Update time stamps
    existing.updateTimeStamps(newInfo.getFirstSeen(), newInfo.getLastSeen());
    
    // Update pattern length bounds
    existing.updatePatternLengthBounds(newInfo.getMinPatternLength(), newInfo.getMaxPatternLength());
    
    // Merge source files
    existing.mergeSourceFiles(newInfo.getSourceFiles());
    
    // Component types and bar offsets should be consistent (validated elsewhere)
    // but merge them just in case
    existing.mergeComponentTypes(newInfo.getComponentTypes());
    
    // Merge bar offsets (should be the same, but ensure uniqueness)
    existing.mergeBarOffsets(newInfo.getBarOffsets());
}

void AnalysisDatabase::updateSearchTypeStatsFromPattern(const PatternAnalysis& pattern) {
    // Determine search type from source file
    std::string searchType = "Unknown";
    std::string filename = pattern.getSourceFile();
    
    // Extract filename from path
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1);
    }
    
    // Convert to lowercase for matching
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("extended") != std::string::npos) searchType = "Extended";
    else if (lower.find("deep") != std::string::npos) searchType = "Deep";
    else if (lower.find("close") != std::string::npos) searchType = "Close";
    else if (lower.find("high-low") != std::string::npos || lower.find("highlow") != std::string::npos) searchType = "High-Low";
    else if (lower.find("open-close") != std::string::npos || lower.find("openclose") != std::string::npos) searchType = "Open-Close";
    else if (lower.find("basic") != std::string::npos) searchType = "Basic";
    else if (lower.find("mixed") != std::string::npos) searchType = "Mixed";
    
    // Update search type statistics
    auto it = searchTypeAnalysis.find(searchType);
    if (it == searchTypeAnalysis.end()) {
        // Create new SearchTypeStats
        SearchTypeStats newStats(1, std::chrono::system_clock::now());
        newStats.addUniqueIndex(pattern.getIndex());
        
        // Update pattern length distribution
        uint8_t patternLength = static_cast<uint8_t>(pattern.getComponents().size());
        newStats.updatePatternLengthDistribution(patternLength);
        
        // Update component usage
        for (const auto& comp : pattern.getComponents()) {
            newStats.updateComponentUsage(comp.getType());
        }
        
        searchTypeAnalysis.emplace(searchType, newStats);
    } else {
        // Update existing stats
        SearchTypeStats& stats = it->second;
        stats.addUniqueIndex(pattern.getIndex());
        stats.incrementTotalPatterns();
        stats.setLastUpdated(std::chrono::system_clock::now());
        
        // Update pattern length distribution
        uint8_t patternLength = static_cast<uint8_t>(pattern.getComponents().size());
        stats.updatePatternLengthDistribution(patternLength);
        
        // Update component usage
        for (const auto& comp : pattern.getComponents()) {
            stats.updateComponentUsage(comp.getType());
        }
    }
}

} // namespace palanalyzer