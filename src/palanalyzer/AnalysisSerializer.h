#pragma once

#include "DataStructures.h"
#include <string>
#include <memory>

// RapidJSON includes
#include <rapidjson/document.h>

using namespace rapidjson;

namespace palanalyzer {

class AnalysisDatabase; // Forward declaration

/**
 * @brief Handles serialization and deserialization of analysis data to/from JSON
 * 
 * This class provides static methods for converting analysis data structures
 * to and from JSON format for persistent storage.
 */
class AnalysisSerializer {
public:
    /**
     * @brief Save analysis database to JSON file
     * @param db The analysis database to save
     * @param filePath Output file path
     * @return True if successful, false otherwise
     */
    static bool saveToFile(const AnalysisDatabase& db, const std::string& filePath);
    
    /**
     * @brief Load analysis database from JSON file
     * @param db The analysis database to populate
     * @param filePath Input file path
     * @return True if successful, false otherwise
     */
    static bool loadFromFile(AnalysisDatabase& db, const std::string& filePath);
    
    /**
     * @brief Export analysis database to JSON string
     * @param db The analysis database to export
     * @return JSON string representation
     */
    static std::string exportToJson(const AnalysisDatabase& db);
    
    /**
     * @brief Import analysis database from JSON string
     * @param db The analysis database to populate
     * @param jsonStr JSON string to parse
     * @return True if successful, false otherwise
     */
    static bool importFromJson(AnalysisDatabase& db, const std::string& jsonStr);

private:
    // Serialization methods for individual data structures
    static Value serializeBarCombinationInfo(const BarCombinationInfo& info);
    static BarCombinationInfo deserializeBarCombinationInfo(const Value& json);
    
    static Value serializeIndexGroupInfo(const IndexGroupInfo& info);
    static IndexGroupInfo deserializeIndexGroupInfo(const Value& json);
    
    static Value serializePatternAnalysis(const PatternAnalysis& pattern);
    static PatternAnalysis deserializePatternAnalysis(const Value& json);
    
    static Value serializeSearchTypeStats(const SearchTypeStats& stats);
    static SearchTypeStats deserializeSearchTypeStats(const Value& json);
    
    static Value serializeFileAnalysisInfo(const FileAnalysisInfo& info);
    static FileAnalysisInfo deserializeFileAnalysisInfo(const Value& json);
    
    static Value serializePriceComponentDescriptor(const PriceComponentDescriptor& comp);
    static PriceComponentDescriptor deserializePriceComponentDescriptor(const Value& json);

    static Value serializePatternCondition(const PatternCondition& cond);
    static PatternCondition deserializePatternCondition(const Value& json);

    static Value serializePatternStructure(const PatternStructure& structure);
    static PatternStructure deserializePatternStructure(const Value& json);
    
    // Helper methods for time point serialization
    static std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point stringToTimePoint(const std::string& str);
    
    // Helper methods for set/vector serialization
    static Value serializeStringSet(const std::set<std::string>& stringSet);
    static std::set<std::string> deserializeStringSet(const Value& json);
    
    static Value serializeUint32Set(const std::set<uint32_t>& uint32Set);
    static std::set<uint32_t> deserializeUint32Set(const Value& json);
    
    static Value serializeComponentTypeSet(const std::set<PriceComponentType>& compSet);
    static std::set<PriceComponentType> deserializeComponentTypeSet(const Value& json);
    
    static Value serializeUint8Vector(const std::vector<uint8_t>& vec);
    static std::vector<uint8_t> deserializeUint8Vector(const Value& json);
    
    static Value serializeIntVector(const std::vector<int>& vec);
    static std::vector<int> deserializeIntVector(const Value& json);
    
    static Value serializeStringVector(const std::vector<std::string>& vec);
    static std::vector<std::string> deserializeStringVector(const Value& json);
    
    // Additional helper methods for IndexGroupInfo serialization
    static Value serializeUint8VectorSet(const std::set<std::vector<uint8_t>>& vecSet);
    static std::set<std::vector<uint8_t>> deserializeUint8VectorSet(const Value& json);
    
    static Value serializeUint8VectorFrequencyMap(const std::map<std::vector<uint8_t>, uint32_t>& freqMap);
    static std::map<std::vector<uint8_t>, uint32_t> deserializeUint8VectorFrequencyMap(const Value& json);
    
    static Value serializeComponentTypeFrequencyMap(const std::map<PriceComponentType, uint32_t>& freqMap);
    static std::map<PriceComponentType, uint32_t> deserializeComponentTypeFrequencyMap(const Value& json);
};

} // namespace palanalyzer