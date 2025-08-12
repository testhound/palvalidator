#include "AnalysisSerializer.h"
#include "AnalysisDatabase.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace rapidjson;

namespace palanalyzer {

bool AnalysisSerializer::saveToFile(const AnalysisDatabase& db, const std::string& filePath) {
    try {
        std::string jsonStr = exportToJson(db);
        
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << filePath << std::endl;
            return false;
        }
        
        file << jsonStr;
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving to file: " << e.what() << std::endl;
        return false;
    }
}

bool AnalysisSerializer::loadFromFile(AnalysisDatabase& db, const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false; // File doesn't exist, not an error
        }
        
        std::string jsonStr((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        return importFromJson(db, jsonStr);
    } catch (const std::exception& e) {
        std::cerr << "Error loading from file: " << e.what() << std::endl;
        return false;
    }
}

std::string AnalysisSerializer::exportToJson(const AnalysisDatabase& db) {
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();
    
    // Metadata
    Value metadata(kObjectType);
    metadata.AddMember("version", "2.0", allocator);
    metadata.AddMember("created", Value(timePointToString(std::chrono::system_clock::now()).c_str(), allocator), allocator);
    metadata.AddMember("totalPatterns", static_cast<uint64_t>(db.getTotalPatterns()), allocator);
    metadata.AddMember("uniqueIndices", static_cast<uint64_t>(db.getUniqueIndices()), allocator);
    metadata.AddMember("analyzedFiles", static_cast<uint64_t>(db.getAnalyzedFiles().size()), allocator);
    doc.AddMember("metadata", metadata, allocator);
    
    // Index groups (new format)
    Value indexGroups(kObjectType);
    for (const auto& pair : db.getIndexGroups()) {
        std::string indexStr = std::to_string(pair.first);
        Value indexValue = serializeIndexGroupInfo(pair.second);
        indexGroups.AddMember(Value(indexStr.c_str(), allocator), indexValue, allocator);
    }
    doc.AddMember("indexGroups", indexGroups, allocator);
    
    // Convert to string
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}


bool AnalysisSerializer::importFromJson(AnalysisDatabase& db, const std::string& jsonStr) {
    try {
        Document doc;
        doc.Parse(jsonStr.c_str());
        
        if (doc.HasParseError()) {
            std::cerr << "JSON parse error" << std::endl;
            return false;
        }
        
        // Clear existing data
        db.clear();
        
        // Import index groups (new format)
        if (doc.HasMember("indexGroups") && doc["indexGroups"].IsObject()) {
            const Value& indexGroups = doc["indexGroups"];
            for (Value::ConstMemberIterator it = indexGroups.MemberBegin();
                 it != indexGroups.MemberEnd(); ++it) {
                uint32_t index = std::stoul(it->name.GetString());
                IndexGroupInfo info = deserializeIndexGroupInfo(it->value);
                db.updateIndexGroup(index, info);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error importing JSON: " << e.what() << std::endl;
        return false;
    }
}

Value AnalysisSerializer::serializeBarCombinationInfo(const BarCombinationInfo& info) {
    // Note: This method should only be called from within exportToJson where allocator is available
    // For now, create a temporary document to avoid crashes, but this is not ideal
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    // obj.AddMember("barOffsets", serializeUint8Vector(info.getBarOffsets()), allocator);
    // obj.AddMember("componentTypes", serializeComponentTypeSet(info.getComponentTypes()), allocator);
    // obj.AddMember("patternCount", info.getPatternCount(), allocator);
    // obj.AddMember("searchType", Value(info.getSearchType().c_str(), allocator), allocator);
    // obj.AddMember("minPatternLength", info.getMinPatternLength(), allocator);
    // obj.AddMember("maxPatternLength", info.getMaxPatternLength(), allocator);
    // obj.AddMember("firstSeen", Value(timePointToString(info.getFirstSeen()).c_str(), allocator), allocator);
    // obj.AddMember("lastSeen", Value(timePointToString(info.getLastSeen()).c_str(), allocator), allocator);
    // obj.AddMember("sourceFiles", serializeStringSet(info.getSourceFiles()), allocator);
    
    return obj;
}

BarCombinationInfo AnalysisSerializer::deserializeBarCombinationInfo(const Value& json) {
    // Extract all required parameters for BarCombinationInfo constructor
    std::vector<uint8_t> barOffsets;
    std::set<PriceComponentType> componentTypes;
    uint32_t patternCount = 1;
    std::string searchType = "";
    uint8_t minPatternLength = 0;
    uint8_t maxPatternLength = 0;
    auto firstSeen = std::chrono::system_clock::now();
    auto lastSeen = firstSeen;
    std::set<std::string> sourceFiles;
    
    if (json.HasMember("barOffsets")) {
        barOffsets = deserializeUint8Vector(json["barOffsets"]);
    }
    if (json.HasMember("componentTypes")) {
        componentTypes = deserializeComponentTypeSet(json["componentTypes"]);
    }
    if (json.HasMember("patternCount")) {
        patternCount = json["patternCount"].GetUint();
    }
    if (json.HasMember("searchType")) {
        searchType = json["searchType"].GetString();
    }
    if (json.HasMember("minPatternLength")) {
        minPatternLength = json["minPatternLength"].GetUint();
    }
    if (json.HasMember("maxPatternLength")) {
        maxPatternLength = json["maxPatternLength"].GetUint();
    }
    if (json.HasMember("firstSeen")) {
        firstSeen = stringToTimePoint(json["firstSeen"].GetString());
    }
    if (json.HasMember("lastSeen")) {
        lastSeen = stringToTimePoint(json["lastSeen"].GetString());
    }
    if (json.HasMember("sourceFiles")) {
        sourceFiles = deserializeStringSet(json["sourceFiles"]);
    }
    
    BarCombinationInfo info(barOffsets, componentTypes, patternCount, searchType,
                           minPatternLength, maxPatternLength, firstSeen, lastSeen, sourceFiles);
    
    // if (json.HasMember("barOffsets")) {
    //     info.barOffsets = deserializeUint8Vector(json["barOffsets"]);
    // }
    // if (json.HasMember("componentTypes")) {
    //     info.componentTypes = deserializeComponentTypeSet(json["componentTypes"]);
    // }
    // if (json.HasMember("patternCount")) {
    //     info.patternCount = json["patternCount"].GetUint();
    // }
    // if (json.HasMember("searchType")) {
    //     info.searchType = json["searchType"].GetString();
    // }
    // if (json.HasMember("minPatternLength")) {
    //     info.minPatternLength = json["minPatternLength"].GetUint();
    // }
    // if (json.HasMember("maxPatternLength")) {
    //     info.maxPatternLength = json["maxPatternLength"].GetUint();
    // }
    // if (json.HasMember("firstSeen")) {
    //     info.firstSeen = stringToTimePoint(json["firstSeen"].GetString());
    // }
    // if (json.HasMember("lastSeen")) {
    //     info.lastSeen = stringToTimePoint(json["lastSeen"].GetString());
    // }
    // if (json.HasMember("sourceFiles")) {
    //     info.sourceFiles = deserializeStringSet(json["sourceFiles"]);
    // }
    
    return info;
}

Value AnalysisSerializer::serializePatternAnalysis(const PatternAnalysis& pattern) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    obj.AddMember("index", pattern.getIndex(), allocator);
    obj.AddMember("sourceFile", Value(pattern.getSourceFile().c_str(), allocator), allocator);
    obj.AddMember("patternHash", static_cast<uint64_t>(pattern.getPatternHash()), allocator);
    obj.AddMember("patternString", Value(pattern.getPatternString().c_str(), allocator), allocator);
    obj.AddMember("isChained", pattern.isChained(), allocator);
    obj.AddMember("maxBarOffset", pattern.getMaxBarOffset(), allocator);
    obj.AddMember("barSpread", pattern.getBarSpread(), allocator);
    obj.AddMember("conditionCount", pattern.getConditionCount(), allocator);
    obj.AddMember("analyzedAt", Value(timePointToString(pattern.getAnalyzedAt()).c_str(), allocator), allocator);
    obj.AddMember("profitabilityLong", pattern.getProfitabilityLong(), allocator);
    obj.AddMember("profitabilityShort", pattern.getProfitabilityShort(), allocator);
    obj.AddMember("trades", pattern.getTrades(), allocator);
    obj.AddMember("consecutiveLosses", pattern.getConsecutiveLosses(), allocator);
    
    // Serialize components
    Value components(kArrayType);
    for (const auto& comp : pattern.getComponents()) {
        Value compValue = serializePriceComponentDescriptor(comp);
        components.PushBack(compValue, allocator);
    }
    obj.AddMember("components", components, allocator);
    
    return obj;
}

PatternAnalysis AnalysisSerializer::deserializePatternAnalysis(const Value& json) {
    uint32_t index = json.HasMember("index") ? json["index"].GetUint() : 0;
    std::string sourceFile = json.HasMember("sourceFile") ? json["sourceFile"].GetString() : "";
    unsigned long long patternHash = json.HasMember("patternHash") ? json["patternHash"].GetUint64() : 0;
    std::string patternString = json.HasMember("patternString") ? json["patternString"].GetString() : "";
    bool isChained = json.HasMember("isChained") ? json["isChained"].GetBool() : false;
    uint8_t maxBarOffset = json.HasMember("maxBarOffset") ? json["maxBarOffset"].GetUint() : 0;
    uint8_t barSpread = json.HasMember("barSpread") ? json["barSpread"].GetUint() : 0;
    uint8_t conditionCount = json.HasMember("conditionCount") ? static_cast<uint8_t>(json["conditionCount"].GetUint()) : 0;
    auto analyzedAt = json.HasMember("analyzedAt") ? stringToTimePoint(json["analyzedAt"].GetString()) : std::chrono::system_clock::now();
    double profitabilityLong = json.HasMember("profitabilityLong") ? json["profitabilityLong"].GetDouble() : 0.0;
    double profitabilityShort = json.HasMember("profitabilityShort") ? json["profitabilityShort"].GetDouble() : 0.0;
    uint32_t trades = json.HasMember("trades") ? json["trades"].GetUint() : 0;
    uint32_t consecutiveLosses = json.HasMember("consecutiveLosses") ? json["consecutiveLosses"].GetUint() : 0;
    
    std::vector<PriceComponentDescriptor> components;
    if (json.HasMember("components") && json["components"].IsArray()) {
        const Value& componentsJson = json["components"];
        for (SizeType i = 0; i < componentsJson.Size(); ++i) {
            components.push_back(deserializePriceComponentDescriptor(componentsJson[i]));
        }
    }

    return PatternAnalysis(index, sourceFile, patternHash, components, patternString, isChained, maxBarOffset,
                           barSpread, conditionCount, analyzedAt, profitabilityLong, profitabilityShort,
                           trades, consecutiveLosses);
}

Value AnalysisSerializer::serializeSearchTypeStats(const SearchTypeStats& stats) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    // obj.AddMember("uniqueIndices", serializeUint32Set(stats.getUniqueIndices()), allocator);
    // obj.AddMember("totalPatterns", stats.getTotalPatterns(), allocator);
    // obj.AddMember("lastUpdated", Value(timePointToString(stats.getLastUpdated()).c_str(), allocator), allocator);
    
    // Pattern length distribution
    Value lengthDist(kObjectType);
    // for (const auto& pair : stats.getPatternLengthDistribution()) {
    //     std::string lengthStr = std::to_string(pair.first);
    //     Value keyValue(lengthStr.c_str(), allocator);
    //     lengthDist.AddMember(keyValue, pair.second, allocator);
    // }
    obj.AddMember("patternLengthDistribution", lengthDist, allocator);
    
    // Component usage
    Value compUsage(kObjectType);
    // for (const auto& pair : stats.getComponentUsage()) {
    //     std::string compStr = componentTypeToString(pair.first);
    //     Value keyValue(compStr.c_str(), allocator);
    //     compUsage.AddMember(keyValue, pair.second, allocator);
    // }
    obj.AddMember("componentUsage", compUsage, allocator);
    
    return obj;
}

SearchTypeStats AnalysisSerializer::deserializeSearchTypeStats(const Value& json) {
    // Extract required parameters for SearchTypeStats constructor
    uint32_t totalPatterns = 0;
    auto lastUpdated = std::chrono::system_clock::now();
    
    if (json.HasMember("totalPatterns")) {
        totalPatterns = json["totalPatterns"].GetUint();
    }
    if (json.HasMember("lastUpdated")) {
        lastUpdated = stringToTimePoint(json["lastUpdated"].GetString());
    }
    
    SearchTypeStats stats(totalPatterns, lastUpdated);
    
    // Add unique indices after construction
    if (json.HasMember("uniqueIndices")) {
        std::set<uint32_t> uniqueIndices = deserializeUint32Set(json["uniqueIndices"]);
        for (uint32_t index : uniqueIndices) {
            stats.addUniqueIndex(index);
        }
    }
    
    // if (json.HasMember("uniqueIndices")) {
    //     stats.uniqueIndices = deserializeUint32Set(json["uniqueIndices"]);
    // }
    // if (json.HasMember("totalPatterns")) {
    //     stats.totalPatterns = json["totalPatterns"].GetUint();
    // }
    // if (json.HasMember("lastUpdated")) {
    //     stats.lastUpdated = stringToTimePoint(json["lastUpdated"].GetString());
    // }
    
    // // Pattern length distribution
    // if (json.HasMember("patternLengthDistribution") && json["patternLengthDistribution"].IsObject()) {
    //     const Value& lengthDist = json["patternLengthDistribution"];
    //     for (Value::ConstMemberIterator it = lengthDist.MemberBegin(); 
    //          it != lengthDist.MemberEnd(); ++it) {
    //         uint8_t length = std::stoul(it->name.GetString());
    //         uint32_t count = it->value.GetUint();
    //         stats.patternLengthDistribution[length] = count;
    //     }
    // }
    
    // // Component usage
    // if (json.HasMember("componentUsage") && json["componentUsage"].IsObject()) {
    //     const Value& compUsage = json["componentUsage"];
    //     for (Value::ConstMemberIterator it = compUsage.MemberBegin(); 
    //          it != compUsage.MemberEnd(); ++it) {
    //         PriceComponentType type = stringToComponentType(it->name.GetString());
    //         uint32_t count = it->value.GetUint();
    //         stats.componentUsage[type] = count;
    //     }
    // }
    
    return stats;
}

Value AnalysisSerializer::serializePriceComponentDescriptor(const PriceComponentDescriptor& comp) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    obj.AddMember("type", Value(componentTypeToString(comp.getType()).c_str(), allocator), allocator);
    obj.AddMember("barOffset", comp.getBarOffset(), allocator);
    obj.AddMember("description", Value(comp.getDescription().c_str(), allocator), allocator);
    
    return obj;
}

PriceComponentDescriptor AnalysisSerializer::deserializePriceComponentDescriptor(const Value& json) {
    PriceComponentType type = PriceComponentType::CLOSE;
    uint8_t barOffset = 0;
    std::string description = "";

    if (json.HasMember("type")) {
        type = stringToComponentType(json["type"].GetString());
    }
    if (json.HasMember("barOffset")) {
        barOffset = json["barOffset"].GetUint();
    }
    if (json.HasMember("description")) {
        description = json["description"].GetString();
    }
    
    return PriceComponentDescriptor(type, barOffset, description);
}

Value AnalysisSerializer::serializePatternCondition(const PatternCondition& cond) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();

    obj.AddMember("type", Value(cond.getType().c_str(), allocator), allocator);
    obj.AddMember("lhs", serializePriceComponentDescriptor(cond.getLhs()), allocator);
    obj.AddMember("rhs", serializePriceComponentDescriptor(cond.getRhs()), allocator);

    return obj;
}

PatternCondition AnalysisSerializer::deserializePatternCondition(const Value& json) {
    std::string type = json.HasMember("type") ? json["type"].GetString() : "";
    PriceComponentDescriptor lhs(PriceComponentType::CLOSE, 0, "");
    if (json.HasMember("lhs")) {
        lhs = deserializePriceComponentDescriptor(json["lhs"]);
    }
    PriceComponentDescriptor rhs(PriceComponentType::CLOSE, 0, "");
    if (json.HasMember("rhs")) {
        rhs = deserializePriceComponentDescriptor(json["rhs"]);
    }
    return PatternCondition(type, lhs, rhs);
}

Value AnalysisSerializer::serializePatternStructure(const PatternStructure& structure) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();

    obj.AddMember("patternHash", Value(std::to_string(structure.getPatternHash()).c_str(), allocator), allocator);
    obj.AddMember("groupId", structure.getGroupId(), allocator);
    
    Value conditions(kArrayType);
    for (const auto& cond : structure.getConditions()) {
        conditions.PushBack(serializePatternCondition(cond), allocator);
    }
    obj.AddMember("conditions", conditions, allocator);
    
    obj.AddMember("conditionCount", structure.getConditionCount(), allocator);
    obj.AddMember("componentsUsed", serializeStringVector(structure.getComponentsUsed()), allocator);
    obj.AddMember("barOffsetsUsed", serializeIntVector(structure.getBarOffsetsUsed()), allocator);

    return obj;
}

PatternStructure AnalysisSerializer::deserializePatternStructure(const Value& json) {
    unsigned long long patternHash = json.HasMember("patternHash") ? std::stoull(json["patternHash"].GetString()) : 0;
    int groupId = json.HasMember("groupId") ? json["groupId"].GetInt() : 0;
    
    std::vector<PatternCondition> conditions;
    if (json.HasMember("conditions") && json["conditions"].IsArray()) {
        const Value& conds = json["conditions"];
        for (SizeType i = 0; i < conds.Size(); ++i) {
            conditions.push_back(deserializePatternCondition(conds[i]));
        }
    }

    int conditionCount = json.HasMember("conditionCount") ? json["conditionCount"].GetInt() : 0;
    
    std::vector<std::string> componentsUsed = deserializeStringVector(json["componentsUsed"]);
    std::vector<int> barOffsetsUsed = deserializeIntVector(json["barOffsetsUsed"]);

    return PatternStructure(patternHash, groupId, conditions, conditionCount, componentsUsed, barOffsetsUsed);
}

Value AnalysisSerializer::serializeIntVector(const std::vector<int>& vec) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (int val : vec) {
        arr.PushBack(val, allocator);
    }
    
    return arr;
}

std::vector<int> AnalysisSerializer::deserializeIntVector(const Value& json) {
    std::vector<int> result;
    if (json.IsArray()) {
        for (const auto& v : json.GetArray()) {
            if (v.IsInt()) {
                result.push_back(v.GetInt());
            }
        }
    }
    return result;
}

Value AnalysisSerializer::serializeStringVector(const std::vector<std::string>& vec) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (const auto& str : vec) {
        arr.PushBack(Value(str.c_str(), allocator), allocator);
    }
    
    return arr;
}

std::vector<std::string> AnalysisSerializer::deserializeStringVector(const Value& json) {
    std::vector<std::string> result;
    if (json.IsArray()) {
        for (const auto& v : json.GetArray()) {
            if (v.IsString()) {
                result.push_back(v.GetString());
            }
        }
    }
    return result;
}

std::string AnalysisSerializer::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::chrono::system_clock::time_point AnalysisSerializer::stringToTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

Value AnalysisSerializer::serializeStringSet(const std::set<std::string>& stringSet) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (const std::string& str : stringSet) {
        arr.PushBack(Value(str.c_str(), allocator), allocator);
    }
    
    return arr;
}

std::set<std::string> AnalysisSerializer::deserializeStringSet(const Value& json) {
    std::set<std::string> result;
    
    if (json.IsArray()) {
        for (SizeType i = 0; i < json.Size(); ++i) {
            if (json[i].IsString()) {
                result.insert(json[i].GetString());
            }
        }
    }
    
    return result;
}

Value AnalysisSerializer::serializeUint32Set(const std::set<uint32_t>& uint32Set) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (uint32_t val : uint32Set) {
        arr.PushBack(val, allocator);
    }
    
    return arr;
}

std::set<uint32_t> AnalysisSerializer::deserializeUint32Set(const Value& json) {
    std::set<uint32_t> result;
    
    if (json.IsArray()) {
        for (SizeType i = 0; i < json.Size(); ++i) {
            if (json[i].IsUint()) {
                result.insert(json[i].GetUint());
            }
        }
    }
    
    return result;
}

Value AnalysisSerializer::serializeComponentTypeSet(const std::set<PriceComponentType>& compSet) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (PriceComponentType type : compSet) {
        arr.PushBack(Value(componentTypeToString(type).c_str(), allocator), allocator);
    }
    
    return arr;
}

std::set<PriceComponentType> AnalysisSerializer::deserializeComponentTypeSet(const Value& json) {
    std::set<PriceComponentType> result;
    
    if (json.IsArray()) {
        for (SizeType i = 0; i < json.Size(); ++i) {
            if (json[i].IsString()) {
                result.insert(stringToComponentType(json[i].GetString()));
            }
        }
    }
    
    return result;
}

Value AnalysisSerializer::serializeUint8Vector(const std::vector<uint8_t>& vec) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (uint8_t val : vec) {
        arr.PushBack(val, allocator);
    }
    
    return arr;
}

std::vector<uint8_t> AnalysisSerializer::deserializeUint8Vector(const Value& json) {
    std::vector<uint8_t> result;
    
    if (json.IsArray()) {
        for (SizeType i = 0; i < json.Size(); ++i) {
            if (json[i].IsUint()) {
                result.push_back(static_cast<uint8_t>(json[i].GetUint()));
            }
        }
    }
    
    return result;
}

Value AnalysisSerializer::serializeIndexGroupInfo(const IndexGroupInfo& info) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    // Serialize metadata
    Value metadata(kObjectType);
    if (info.getGroupMetadata()) {
        const auto& meta = info.getGroupMetadata();
        metadata.AddMember("barOffsets", serializeIntVector(meta->getBarOffsets()), allocator);
        metadata.AddMember("componentTypes", serializeStringVector(meta->getComponentTypes()), allocator);
        metadata.AddMember("searchType", Value(meta->getSearchType().c_str(), allocator), allocator);
        metadata.AddMember("minPatternLength", meta->getMinPatternLength(), allocator);
        metadata.AddMember("maxPatternLength", meta->getMaxPatternLength(), allocator);
        metadata.AddMember("totalPatterns", meta->getTotalPatterns(), allocator);
    }
    obj.AddMember("groupMetadata", metadata, allocator);

    // Serialize patterns
    Value patterns(kObjectType);
    for (const auto& pair : info.getPatterns()) {
        patterns.AddMember(Value(pair.first.c_str(), allocator), serializePatternStructure(pair.second), allocator);
    }
    obj.AddMember("patterns", patterns, allocator);
    
    return obj;
}

IndexGroupInfo AnalysisSerializer::deserializeIndexGroupInfo(const Value& json) {
    // Extract required parameters for IndexGroupInfo constructor
    uint32_t indexNumber = 0;
    std::string searchType = "";
    std::string sourceFile = "";
    std::vector<uint8_t> barCombination;
    std::set<PriceComponentType> componentTypes;
    
    // These would typically come from the calling context or metadata
    if (json.HasMember("indexNumber")) {
        indexNumber = json["indexNumber"].GetUint();
    }
    if (json.HasMember("searchType")) {
        searchType = json["searchType"].GetString();
    }
    if (json.HasMember("sourceFile")) {
        sourceFile = json["sourceFile"].GetString();
    }
    if (json.HasMember("barCombination")) {
        barCombination = deserializeUint8Vector(json["barCombination"]);
    }
    if (json.HasMember("componentTypes")) {
        componentTypes = deserializeComponentTypeSet(json["componentTypes"]);
    }
    
    IndexGroupInfo info(indexNumber, searchType, sourceFile, barCombination, componentTypes);

    if (json.HasMember("patterns") && json["patterns"].IsObject()) {
        const Value& patternsJson = json["patterns"];
        for (auto& m : patternsJson.GetObject()) {
            PatternStructure pattern = deserializePatternStructure(m.value);
            info.addPattern(m.name.GetString(), pattern);
        }
    }
    
    return info;
}

Value AnalysisSerializer::serializeUint8VectorSet(const std::set<std::vector<uint8_t>>& vecSet) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value arr(kArrayType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (const auto& vec : vecSet) {
        Value vecValue = serializeUint8Vector(vec);
        arr.PushBack(vecValue, allocator);
    }
    
    return arr;
}

std::set<std::vector<uint8_t>> AnalysisSerializer::deserializeUint8VectorSet(const Value& json) {
    std::set<std::vector<uint8_t>> result;
    
    if (json.IsArray()) {
        for (SizeType i = 0; i < json.Size(); ++i) {
            std::vector<uint8_t> vec = deserializeUint8Vector(json[i]);
            result.insert(vec);
        }
    }
    
    return result;
}

Value AnalysisSerializer::serializeUint8VectorFrequencyMap(const std::map<std::vector<uint8_t>, uint32_t>& freqMap) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (const auto& pair : freqMap) {
        std::string keyStr = vectorToString(pair.first);
        Value keyValue(keyStr.c_str(), allocator);
        obj.AddMember(keyValue, pair.second, allocator);
    }
    
    return obj;
}

std::map<std::vector<uint8_t>, uint32_t> AnalysisSerializer::deserializeUint8VectorFrequencyMap(const Value& json) {
    std::map<std::vector<uint8_t>, uint32_t> result;
    
    if (json.IsObject()) {
        for (Value::ConstMemberIterator it = json.MemberBegin(); it != json.MemberEnd(); ++it) {
            std::string keyStr = it->name.GetString();
            uint32_t frequency = it->value.GetUint();
            
            // Parse vector string back to vector<uint8_t>
            // Format is "[1,2,3]"
            std::vector<uint8_t> vec;
            if (keyStr.length() > 2 && keyStr[0] == '[' && keyStr.back() == ']') {
                std::string content = keyStr.substr(1, keyStr.length() - 2);
                std::istringstream iss(content);
                std::string token;
                
                while (std::getline(iss, token, ',')) {
                    vec.push_back(static_cast<uint8_t>(std::stoi(token)));
                }
            }
            
            result[vec] = frequency;
        }
    }
    
    return result;
}

Value AnalysisSerializer::serializeComponentTypeFrequencyMap(const std::map<PriceComponentType, uint32_t>& freqMap) {
    static thread_local Document tempDoc;
    tempDoc.SetObject();
    Value obj(kObjectType);
    Document::AllocatorType& allocator = tempDoc.GetAllocator();
    
    for (const auto& pair : freqMap) {
        std::string compStr = componentTypeToString(pair.first);
        Value keyValue(compStr.c_str(), allocator);
        obj.AddMember(keyValue, pair.second, allocator);
    }
    
    return obj;
}

std::map<PriceComponentType, uint32_t> AnalysisSerializer::deserializeComponentTypeFrequencyMap(const Value& json) {
    std::map<PriceComponentType, uint32_t> result;
    
    if (json.IsObject()) {
        for (Value::ConstMemberIterator it = json.MemberBegin(); it != json.MemberEnd(); ++it) {
            PriceComponentType type = stringToComponentType(it->name.GetString());
            uint32_t frequency = it->value.GetUint();
            result[type] = frequency;
        }
    }
    
    return result;
}


} // namespace palanalyzer