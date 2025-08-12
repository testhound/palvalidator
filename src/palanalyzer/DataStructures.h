#pragma once

#include <vector>
#include <set>
#include <map>
#include <string>
#include <chrono>
#include <cstdint>
#include <memory>

namespace palanalyzer {

// Search type enumeration for pattern analysis
enum class SearchType {
    UNKNOWN,
    BASIC,
    EXTENDED,
    DEEP,
    CLOSE,
    HIGH_LOW,
    OPEN_CLOSE,
    MIXED
};

// Price component types for pattern analysis
enum class PriceComponentType {
    OPEN,
    HIGH,
    LOW,
    CLOSE,
    VOLUME,
    ROC1,
    IBS1,
    IBS2,
    IBS3,
    MEANDER,
    VCHARTLOW,
    VCHARTHIGH
};

// Price component descriptor for detailed analysis
class PriceComponentDescriptor {
public:
    PriceComponentDescriptor(PriceComponentType type, uint8_t barOffset, std::string description)
        : m_type(type), m_barOffset(barOffset), m_description(std::move(description)) {}

    PriceComponentType getType() const { return m_type; }
    uint8_t getBarOffset() const { return m_barOffset; }
    const std::string& getDescription() const { return m_description; }

    bool operator<(const PriceComponentDescriptor& other) const {
        if (m_type != other.m_type) return m_type < other.m_type;
        return m_barOffset < other.m_barOffset;
    }

private:
    PriceComponentType m_type;
    uint8_t m_barOffset;
    std::string m_description;
};

// Represents a single condition in a pattern (e.g., C[0] > C[1])
class PatternCondition {
public:
    PatternCondition(std::string type, PriceComponentDescriptor lhs, PriceComponentDescriptor rhs)
        : m_type(std::move(type)), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}

    const std::string& getType() const { return m_type; }
    const PriceComponentDescriptor& getLhs() const { return m_lhs; }
    const PriceComponentDescriptor& getRhs() const { return m_rhs; }

private:
    std::string m_type;
    PriceComponentDescriptor m_lhs;
    PriceComponentDescriptor m_rhs;
};

// Represents the structural properties of a unique pattern
class PatternStructure {
public:
    PatternStructure(unsigned long long patternHash, int groupId, std::vector<PatternCondition> conditions,
                     int conditionCount, std::vector<std::string> componentsUsed, std::vector<int> barOffsetsUsed)
        : m_patternHash(patternHash), m_groupId(groupId), m_conditions(std::move(conditions)),
          m_conditionCount(conditionCount), m_componentsUsed(std::move(componentsUsed)),
          m_barOffsetsUsed(std::move(barOffsetsUsed)) {}

    unsigned long long getPatternHash() const { return m_patternHash; }
    int getGroupId() const { return m_groupId; }
    const std::vector<PatternCondition>& getConditions() const { return m_conditions; }
    int getConditionCount() const { return m_conditionCount; }
    const std::vector<std::string>& getComponentsUsed() const { return m_componentsUsed; }
    const std::vector<int>& getBarOffsetsUsed() const { return m_barOffsetsUsed; }

private:
    unsigned long long m_patternHash;
    int m_groupId;
    std::vector<PatternCondition> m_conditions;
    int m_conditionCount;
    std::vector<std::string> m_componentsUsed;
    std::vector<int> m_barOffsetsUsed;
};

// Index group information - represents pattern classification groups
class IndexGroupMetadata {
public:
    IndexGroupMetadata(std::vector<int> barOffsets, std::vector<std::string> componentTypes, std::string searchType,
                       int minPatternLength, int maxPatternLength, int totalPatterns)
        : m_barOffsets(std::move(barOffsets)), m_componentTypes(std::move(componentTypes)),
          m_searchType(std::move(searchType)), m_minPatternLength(minPatternLength),
          m_maxPatternLength(maxPatternLength), m_totalPatterns(totalPatterns) {}

    const std::vector<int>& getBarOffsets() const { return m_barOffsets; }
    const std::vector<std::string>& getComponentTypes() const { return m_componentTypes; }
    const std::string& getSearchType() const { return m_searchType; }
    int getMinPatternLength() const { return m_minPatternLength; }
    int getMaxPatternLength() const { return m_maxPatternLength; }
    int getTotalPatterns() const { return m_totalPatterns; }

private:
    std::vector<int> m_barOffsets;
    std::vector<std::string> m_componentTypes;
    std::string m_searchType;
    int m_minPatternLength;
    int m_maxPatternLength;
    int m_totalPatterns;
};

class IndexGroupInfo {
public:
    IndexGroupInfo(uint32_t indexNumber, const std::string& searchType,
                   const std::string& sourceFile, const std::vector<uint8_t>& barCombination,
                   const std::set<PriceComponentType>& componentTypes)
        : m_indexNumber(indexNumber),
          m_searchType(searchType),
          m_patternCount(1),
          m_firstSeen(std::chrono::system_clock::now()),
          m_lastSeen(m_firstSeen),
          m_minPatternLength(static_cast<uint8_t>(barCombination.size())),
          m_maxPatternLength(static_cast<uint8_t>(barCombination.size()))
    {
        m_searchTypes.insert(searchType);
        m_sourceFiles.insert(sourceFile);
        m_uniqueBarCombinations.insert(barCombination);
        m_allComponentTypes.insert(componentTypes.begin(), componentTypes.end());
        m_barCombinationFrequency[barCombination] = 1;
        
        for (const auto& compType : componentTypes) {
            m_componentTypeFrequency[compType]++;
        }
    }

    const std::shared_ptr<IndexGroupMetadata>& getGroupMetadata() const { return m_groupMetadata; }
    const std::map<std::string, PatternStructure>& getPatterns() const { return m_patterns; }
    uint32_t getIndexNumber() const { return m_indexNumber; }
    const std::string& getSearchType() const { return m_searchType; }
    const std::set<std::string>& getSearchTypes() const { return m_searchTypes; }
    uint32_t getPatternCount() const { return m_patternCount; }
    const std::chrono::system_clock::time_point& getFirstSeen() const { return m_firstSeen; }
    const std::chrono::system_clock::time_point& getLastSeen() const { return m_lastSeen; }
    const std::set<std::string>& getSourceFiles() const { return m_sourceFiles; }
    const std::set<std::vector<uint8_t>>& getUniqueBarCombinations() const { return m_uniqueBarCombinations; }
    const std::set<PriceComponentType>& getAllComponentTypes() const { return m_allComponentTypes; }
    const std::map<std::vector<uint8_t>, uint32_t>& getBarCombinationFrequency() const { return m_barCombinationFrequency; }
    const std::map<PriceComponentType, uint32_t>& getComponentTypeFrequency() const { return m_componentTypeFrequency; }
    uint8_t getMinPatternLength() const { return m_minPatternLength; }
    uint8_t getMaxPatternLength() const { return m_maxPatternLength; }

    // Public methods for controlled modification
    void addPattern(const std::string& patternHash, const PatternStructure& pattern)
    {
        m_patterns.emplace(patternHash, pattern);
    }

    void updateExistingGroup(const std::string& searchType, const std::string& sourceFile,
                            const std::vector<uint8_t>& barCombination,
                            const std::set<PriceComponentType>& componentTypes)
    {
        m_patternCount++;
        m_lastSeen = std::chrono::system_clock::now();
        m_sourceFiles.insert(sourceFile);
        m_uniqueBarCombinations.insert(barCombination);
        m_allComponentTypes.insert(componentTypes.begin(), componentTypes.end());
        m_searchTypes.insert(searchType);
        
        m_barCombinationFrequency[barCombination]++;
        for (const auto& compType : componentTypes) {
            m_componentTypeFrequency[compType]++;
        }
        
        uint8_t patternLength = static_cast<uint8_t>(barCombination.size());
        m_minPatternLength = std::min(m_minPatternLength, patternLength);
        m_maxPatternLength = std::max(m_maxPatternLength, patternLength);
    }

private:
    std::shared_ptr<IndexGroupMetadata> m_groupMetadata;
    std::map<std::string, PatternStructure> m_patterns;
    uint32_t m_indexNumber;
    std::string m_searchType;
    std::set<std::string> m_searchTypes;
    uint32_t m_patternCount;
    std::chrono::system_clock::time_point m_firstSeen;
    std::chrono::system_clock::time_point m_lastSeen;
    std::set<std::string> m_sourceFiles;
    std::set<std::vector<uint8_t>> m_uniqueBarCombinations;
    std::set<PriceComponentType> m_allComponentTypes;
    std::map<std::vector<uint8_t>, uint32_t> m_barCombinationFrequency;
    std::map<PriceComponentType, uint32_t> m_componentTypeFrequency;
    uint8_t m_minPatternLength;
    uint8_t m_maxPatternLength;
};

// Individual bar combination information (for detailed analysis)
class BarCombinationInfo {
public:
    BarCombinationInfo(std::vector<uint8_t> barOffsets, std::set<PriceComponentType> componentTypes,
                       uint32_t patternCount, std::string searchType, uint8_t minPatternLength,
                       uint8_t maxPatternLength, std::chrono::system_clock::time_point firstSeen,
                       std::chrono::system_clock::time_point lastSeen, std::set<std::string> sourceFiles)
        : m_barOffsets(std::move(barOffsets)), m_componentTypes(std::move(componentTypes)),
          m_patternCount(patternCount), m_searchType(std::move(searchType)),
          m_minPatternLength(minPatternLength), m_maxPatternLength(maxPatternLength),
          m_firstSeen(firstSeen), m_lastSeen(lastSeen), m_sourceFiles(std::move(sourceFiles)) {}

    const std::vector<uint8_t>& getBarOffsets() const { return m_barOffsets; }
    const std::set<PriceComponentType>& getComponentTypes() const { return m_componentTypes; }
    uint32_t getPatternCount() const { return m_patternCount; }
    const std::string& getSearchType() const { return m_searchType; }
    uint8_t getMinPatternLength() const { return m_minPatternLength; }
    uint8_t getMaxPatternLength() const { return m_maxPatternLength; }
    const std::chrono::system_clock::time_point& getFirstSeen() const { return m_firstSeen; }
    const std::chrono::system_clock::time_point& getLastSeen() const { return m_lastSeen; }
    const std::set<std::string>& getSourceFiles() const { return m_sourceFiles; }

    // Public methods for controlled modification
    void updatePatternCount(uint32_t additionalCount)
    {
        m_patternCount += additionalCount;
    }

    void updateTimeStamps(const std::chrono::system_clock::time_point& newFirstSeen,
                         const std::chrono::system_clock::time_point& newLastSeen)
    {
        if (newFirstSeen < m_firstSeen) {
            m_firstSeen = newFirstSeen;
        }
        if (newLastSeen > m_lastSeen) {
            m_lastSeen = newLastSeen;
        }
    }

    void updatePatternLengthBounds(uint8_t newMinLength, uint8_t newMaxLength)
    {
        m_minPatternLength = std::min(m_minPatternLength, newMinLength);
        m_maxPatternLength = std::max(m_maxPatternLength, newMaxLength);
    }

    void mergeSourceFiles(const std::set<std::string>& newSourceFiles)
    {
        m_sourceFiles.insert(newSourceFiles.begin(), newSourceFiles.end());
    }

    void mergeComponentTypes(const std::set<PriceComponentType>& newComponentTypes)
    {
        m_componentTypes.insert(newComponentTypes.begin(), newComponentTypes.end());
    }

    void mergeBarOffsets(const std::vector<uint8_t>& newBarOffsets)
    {
        std::set<uint8_t> uniqueOffsets(m_barOffsets.begin(), m_barOffsets.end());
        uniqueOffsets.insert(newBarOffsets.begin(), newBarOffsets.end());
        m_barOffsets.assign(uniqueOffsets.begin(), uniqueOffsets.end());
    }

private:
    std::vector<uint8_t> m_barOffsets;
    std::set<PriceComponentType> m_componentTypes;
    uint32_t m_patternCount;
    std::string m_searchType;
    uint8_t m_minPatternLength;
    uint8_t m_maxPatternLength;
    std::chrono::system_clock::time_point m_firstSeen;
    std::chrono::system_clock::time_point m_lastSeen;
    std::set<std::string> m_sourceFiles;
};

// Individual pattern analysis data
class PatternAnalysis {
public:
    PatternAnalysis(uint32_t index, std::string sourceFile, unsigned long long patternHash,
                    std::vector<PriceComponentDescriptor> components, std::string patternString,
                    bool isChained, uint8_t maxBarOffset, uint8_t barSpread, uint8_t conditionCount,
                    std::chrono::system_clock::time_point analyzedAt, double profitabilityLong,
                    double profitabilityShort, uint32_t trades, uint32_t consecutiveLosses)
        : m_index(index), m_sourceFile(std::move(sourceFile)), m_patternHash(patternHash),
          m_components(std::move(components)), m_patternString(std::move(patternString)),
          m_isChained(isChained), m_maxBarOffset(maxBarOffset), m_barSpread(barSpread),
          m_conditionCount(conditionCount), m_analyzedAt(analyzedAt),
          m_profitabilityLong(profitabilityLong), m_profitabilityShort(profitabilityShort),
          m_trades(trades), m_consecutiveLosses(consecutiveLosses) {}

    uint32_t getIndex() const { return m_index; }
    const std::string& getSourceFile() const { return m_sourceFile; }
    unsigned long long getPatternHash() const { return m_patternHash; }
    const std::vector<PriceComponentDescriptor>& getComponents() const { return m_components; }
    const std::string& getPatternString() const { return m_patternString; }
    bool isChained() const { return m_isChained; }
    uint8_t getMaxBarOffset() const { return m_maxBarOffset; }
    uint8_t getBarSpread() const { return m_barSpread; }
    uint8_t getConditionCount() const { return m_conditionCount; }
    const std::chrono::system_clock::time_point& getAnalyzedAt() const { return m_analyzedAt; }
    double getProfitabilityLong() const { return m_profitabilityLong; }
    double getProfitabilityShort() const { return m_profitabilityShort; }
    uint32_t getTrades() const { return m_trades; }
    uint32_t getConsecutiveLosses() const { return m_consecutiveLosses; }

private:
    uint32_t m_index;
    std::string m_sourceFile;
    unsigned long long m_patternHash;
    std::vector<PriceComponentDescriptor> m_components;
    std::string m_patternString;
    bool m_isChained;
    uint8_t m_maxBarOffset;
    uint8_t m_barSpread;
    uint8_t m_conditionCount;
    std::chrono::system_clock::time_point m_analyzedAt;
    double m_profitabilityLong;
    double m_profitabilityShort;
    uint32_t m_trades;
    uint32_t m_consecutiveLosses;
};

// Search type statistics
class SearchTypeStats {
public:
    SearchTypeStats(uint32_t totalPatterns, std::chrono::system_clock::time_point lastUpdated)
        : m_totalPatterns(totalPatterns), m_lastUpdated(lastUpdated) {}

    const std::set<uint32_t>& getUniqueIndices() const { return m_uniqueIndices; }
    const std::map<uint8_t, uint32_t>& getPatternLengthDistribution() const { return m_patternLengthDistribution; }
    const std::map<PriceComponentType, uint32_t>& getComponentUsage() const { return m_componentUsage; }
    uint32_t getTotalPatterns() const { return m_totalPatterns; }
    const std::chrono::system_clock::time_point& getLastUpdated() const { return m_lastUpdated; }

    // Public methods for controlled modification
    void addUniqueIndex(uint32_t index)
    {
        m_uniqueIndices.insert(index);
    }

    void incrementTotalPatterns()
    {
        m_totalPatterns++;
    }

    void updatePatternLengthDistribution(uint8_t patternLength)
    {
        m_patternLengthDistribution[patternLength]++;
    }

    void updateComponentUsage(PriceComponentType componentType)
    {
        m_componentUsage[componentType]++;
    }

    void setLastUpdated(const std::chrono::system_clock::time_point& lastUpdated)
    {
        m_lastUpdated = lastUpdated;
    }

private:
    std::set<uint32_t> m_uniqueIndices;
    std::map<uint8_t, uint32_t> m_patternLengthDistribution;
    std::map<PriceComponentType, uint32_t> m_componentUsage;
    uint32_t m_totalPatterns;
    std::chrono::system_clock::time_point m_lastUpdated;
};

// Overall analysis statistics
class AnalysisStats {
public:
    AnalysisStats(size_t totalPatterns, size_t uniqueIndices, size_t analyzedFiles,
                 const std::chrono::system_clock::time_point& lastAnalysis,
                 const std::chrono::system_clock::time_point& firstAnalysis)
        : m_totalPatterns(totalPatterns),
          m_uniqueIndices(uniqueIndices),
          m_analyzedFiles(analyzedFiles),
          m_lastAnalysis(lastAnalysis),
          m_firstAnalysis(firstAnalysis) {}

    size_t getTotalPatterns() const { return m_totalPatterns; }
    size_t getUniqueIndices() const { return m_uniqueIndices; }
    size_t getAnalyzedFiles() const { return m_analyzedFiles; }
    const std::map<std::string, size_t>& getSearchTypeBreakdown() const { return m_searchTypeBreakdown; }
    const std::chrono::system_clock::time_point& getLastAnalysis() const { return m_lastAnalysis; }
    const std::chrono::system_clock::time_point& getFirstAnalysis() const { return m_firstAnalysis; }

    // Public method for controlled modification
    void addSearchTypeBreakdown(const std::string& searchType, size_t count)
    {
        m_searchTypeBreakdown[searchType] = count;
    }

private:
    size_t m_totalPatterns;
    size_t m_uniqueIndices;
    size_t m_analyzedFiles;
    std::map<std::string, size_t> m_searchTypeBreakdown;
    std::chrono::system_clock::time_point m_lastAnalysis;
    std::chrono::system_clock::time_point m_firstAnalysis;
};

// File analysis metadata
class FileAnalysisInfo {
public:
    FileAnalysisInfo(std::string path, std::chrono::system_clock::time_point analyzedAt,
                     uint32_t patternCount, uint32_t uniqueIndices)
        : m_path(std::move(path)), m_analyzedAt(analyzedAt), m_patternCount(patternCount),
          m_uniqueIndices(uniqueIndices) {}

    const std::string& getPath() const { return m_path; }
    const std::chrono::system_clock::time_point& getAnalyzedAt() const { return m_analyzedAt; }
    uint32_t getPatternCount() const { return m_patternCount; }
    uint32_t getUniqueIndices() const { return m_uniqueIndices; }

private:
    std::string m_path;
    std::chrono::system_clock::time_point m_analyzedAt;
    uint32_t m_patternCount;
    uint32_t m_uniqueIndices;
};

// Helper functions for search type conversion
std::string searchTypeToString(SearchType type);
SearchType stringToSearchType(const std::string& str);

// Helper functions for component type conversion
std::string componentTypeToString(PriceComponentType type);
PriceComponentType stringToComponentType(const std::string& str);
std::string vectorToString(const std::vector<uint8_t>& vec);

} // namespace palanalyzer