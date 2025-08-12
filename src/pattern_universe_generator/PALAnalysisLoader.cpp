#include "PALAnalysisLoader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/istreamwrapper.h>

namespace fs = boost::filesystem;
using namespace rapidjson;

namespace pattern_universe {

PALIndexMappings PALAnalysisLoader::loadIndexMappings(const std::string& reportPath) {
    std::map<uint32_t, CuratedGroup> indexToGroup;
    std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices;
    size_t totalPatterns = 0;
    size_t totalIndices = 0;
    
    try {
        Document doc = parseJsonFile(reportPath);
        
        if (!doc.HasMember("indexGroups")) {
            if (doc.HasMember("indexMappings")) {
                // Fallback to old format for backward compatibility
            } else {
                throw std::runtime_error("Index groups not found in report");
            }
        }
        
        // Extract metadata if available
        if (doc.HasMember("metadata")) {
            const Value& metadata = doc["metadata"];
            if (metadata.HasMember("totalPatterns")) {
                totalPatterns = metadata["totalPatterns"].GetUint();
                std::cout << "DEBUG: Found metadata totalPatterns: " << totalPatterns << std::endl;
            } else {
                std::cout << "DEBUG: metadata does not have totalPatterns field" << std::endl;
            }
            if (metadata.HasMember("totalIndices")) {
                totalIndices = metadata["totalIndices"].GetUint();
                std::cout << "DEBUG: Found metadata totalIndices: " << totalIndices << std::endl;
            } else {
                std::cout << "DEBUG: metadata does not have totalIndices field" << std::endl;
            }
        } else {
            // Document does not have metadata section
        }
        
        const Value& indexGroupsJson = doc["indexGroups"];
        
        for (Value::ConstMemberIterator it = indexGroupsJson.MemberBegin();
             it != indexGroupsJson.MemberEnd(); ++it) {
            uint32_t indexNumber = std::stoul(it->name.GetString());
            const Value& groupData = it->value;

            if (!groupData.HasMember("groupMetadata")) continue;
            const Value& meta = groupData["groupMetadata"];

            auto barOffsets = parseBarOffsets(meta["barOffsets"]);
            
            std::set<PriceComponentType> componentTypes;
            if (meta.HasMember("componentTypes") && meta["componentTypes"].IsArray()) {
                for (const auto& typeStr : meta["componentTypes"].GetArray()) {
                    componentTypes.insert(stringToComponentType(typeStr.GetString()));
                }
            }

            auto searchType = stringToSearchType(meta["searchType"].GetString());
            auto minPatternLength = meta["minPatternLength"].GetUint();
            auto maxPatternLength = meta["maxPatternLength"].GetUint();
            auto patternCount = meta["totalPatterns"].GetUint();
            auto generationPriority = calculateGenerationPriority(patternCount, totalPatterns > 0 ? totalPatterns : 100000);
            auto supportsChaining = (componentTypes.size() >= 3 && patternCount > 500);

            CuratedGroup group(indexNumber, barOffsets, componentTypes, searchType,
                               minPatternLength, maxPatternLength, patternCount,
                               generationPriority, supportsChaining);
            
            indexToGroup[indexNumber] = std::move(group);
            searchTypeToIndices[searchType].push_back(indexNumber);
        }
        
        // Use metadata totals if available, otherwise calculate from actual data
        if (totalIndices == 0) {
            totalIndices = indexToGroup.size();
        }
        
        // Calculate total patterns from all groups if metadata didn't provide it
        if (totalPatterns == 0) {
            for (const auto& [indexNum, group] : indexToGroup) {
                totalPatterns += group.getPatternCount();
            }
            // Calculated totalPatterns from groups
        }
        
        PALIndexMappings mappings(std::move(indexToGroup), std::move(searchTypeToIndices), {},
                                 totalPatterns, totalIndices, std::chrono::system_clock::now());
        // Created PALIndexMappings
        
        // Loaded index mappings from PAL analysis
        
        return mappings;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load index mappings: " + std::string(e.what()));
    }
}

std::map<SearchType, ComponentUsageStats> PALAnalysisLoader::loadComponentStats(const std::string& reportPath) {
    std::map<SearchType, ComponentUsageStats> statsMap;
    
    try {
        Document doc = parseJsonFile(reportPath);
        
        if (!doc.HasMember("componentAnalysis")) {
            throw std::runtime_error("Component analysis not found in report");
        }
        
        const Value& componentAnalysis = doc["componentAnalysis"];
        
        for (Value::ConstMemberIterator it = componentAnalysis.MemberBegin();
             it != componentAnalysis.MemberEnd(); ++it) {
            SearchType searchType = stringToSearchType(it->name.GetString());
            const Value& searchData = it->value;
            
            uint32_t totalPatterns = 0;
            if (searchData.HasMember("totalPatterns")) {
                if (searchData["totalPatterns"].IsUint()) {
                    totalPatterns = searchData["totalPatterns"].GetUint();
                } else if (searchData["totalPatterns"].IsInt()) {
                    totalPatterns = static_cast<uint32_t>(searchData["totalPatterns"].GetInt());
                } else {
                    std::cout << "DEBUG: totalPatterns field exists but is not a number type" << std::endl;
                }
            } else {
                std::cout << "DEBUG: totalPatterns field not found in " << it->name.GetString() << std::endl;
            }
            
            size_t uniqueIndices = 0;
            if (searchData.HasMember("uniqueIndices")) {
                if (searchData["uniqueIndices"].IsUint()) {
                    uniqueIndices = searchData["uniqueIndices"].GetUint();
                } else if (searchData["uniqueIndices"].IsInt()) {
                    uniqueIndices = static_cast<size_t>(searchData["uniqueIndices"].GetInt());
                } else {
                    std::cout << "DEBUG: uniqueIndices field exists but is not a number type" << std::endl;
                }
            } else {
                std::cout << "DEBUG: uniqueIndices field not found in " << it->name.GetString() << std::endl;
            }
            
            // Loading component statistics
            
            // Load component usage
            std::map<PriceComponentType, uint32_t> componentUsage;
            if (searchData.HasMember("componentUsage") && searchData["componentUsage"].IsObject()) {
                const Value& componentUsageJson = searchData["componentUsage"];
                for (Value::ConstMemberIterator compIt = componentUsageJson.MemberBegin();
                     compIt != componentUsageJson.MemberEnd(); ++compIt) {
                    PriceComponentType compType = stringToComponentType(compIt->name.GetString());
                    componentUsage[compType] = compIt->value.GetUint();
                }
            }
            
            // Load pattern length distribution
            std::map<uint8_t, uint32_t> patternLengthDistribution;
            if (searchData.HasMember("patternLengthDistribution") && searchData["patternLengthDistribution"].IsObject()) {
                const Value& lengthDist = searchData["patternLengthDistribution"];
                for (Value::ConstMemberIterator lengthIt = lengthDist.MemberBegin();
                     lengthIt != lengthDist.MemberEnd(); ++lengthIt) {
                    uint8_t length = std::stoul(lengthIt->name.GetString());
                    patternLengthDistribution[length] = lengthIt->value.GetUint();
                }
            }
            
            ComponentUsageStats stats(totalPatterns, uniqueIndices, std::move(componentUsage), std::move(patternLengthDistribution));
            // Created ComponentUsageStats
            statsMap.emplace(searchType, std::move(stats));
            // Added to statsMap
        }
        
        // Loaded component statistics
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load component stats: " + std::string(e.what()));
    }
    
    return statsMap;
}

AlgorithmInsights PALAnalysisLoader::loadAlgorithmInsights(const std::string& reportPath) {
    size_t totalPatterns = 0;
    size_t chainedPatterns = 0;
    double chainingPercentage = 0.0;
    std::string curatedGroups;
    std::string componentConstraints;
    std::string searchSpaceReduction;
    std::map<uint8_t, size_t> barSpreadDistribution;
    std::map<uint8_t, size_t> maxOffsetDistribution;
    
    try {
        Document doc = parseJsonFile(reportPath);
        
        if (doc.HasMember("algorithmInsights")) {
            const Value& algoInsights = doc["algorithmInsights"];
            if (algoInsights.HasMember("curatedGroups") && algoInsights["curatedGroups"].IsString()) {
                curatedGroups = algoInsights["curatedGroups"].GetString();
            }
            if (algoInsights.HasMember("componentConstraints") && algoInsights["componentConstraints"].IsString()) {
                componentConstraints = algoInsights["componentConstraints"].GetString();
            }
            if (algoInsights.HasMember("searchSpaceReduction") && algoInsights["searchSpaceReduction"].IsString()) {
                searchSpaceReduction = algoInsights["searchSpaceReduction"].GetString();
            }
        }
        
        if (doc.HasMember("patternStructureAnalysis")) {
            const Value& structAnalysis = doc["patternStructureAnalysis"];
            if (structAnalysis.HasMember("totalPatterns")) {
                totalPatterns = structAnalysis["totalPatterns"].GetUint();
            }
            if (structAnalysis.HasMember("chainedPatterns")) {
                chainedPatterns = structAnalysis["chainedPatterns"].GetUint();
            }
            if (structAnalysis.HasMember("chainingPercentage")) {
                chainingPercentage = structAnalysis["chainingPercentage"].GetDouble();
            }
            
            // Load bar spread distribution
            if (structAnalysis.HasMember("barSpreadDistribution") && structAnalysis["barSpreadDistribution"].IsObject()) {
                const Value& barSpreadDist = structAnalysis["barSpreadDistribution"];
                for (Value::ConstMemberIterator it = barSpreadDist.MemberBegin();
                     it != barSpreadDist.MemberEnd(); ++it) {
                    uint8_t spread = std::stoul(it->name.GetString());
                    barSpreadDistribution[spread] = it->value.GetUint();
                }
            }
            
            // Load max offset distribution
            if (structAnalysis.HasMember("maxOffsetDistribution") && structAnalysis["maxOffsetDistribution"].IsObject()) {
                const Value& maxOffsetDist = structAnalysis["maxOffsetDistribution"];
                for (Value::ConstMemberIterator it = maxOffsetDist.MemberBegin();
                     it != maxOffsetDist.MemberEnd(); ++it) {
                    uint8_t offset = std::stoul(it->name.GetString());
                    maxOffsetDistribution[offset] = it->value.GetUint();
                }
            }
        }
        
        AlgorithmInsights insights(totalPatterns, chainedPatterns, chainingPercentage,
                                  curatedGroups, componentConstraints, searchSpaceReduction,
                                  std::move(barSpreadDistribution), std::move(maxOffsetDistribution));
        
        // Loaded algorithm insights
        
        return insights;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load algorithm insights: " + std::string(e.what()));
    }
}

AlgorithmInsights PALAnalysisLoader::loadPatternStructureAnalysis(const std::string& reportPath) {
    size_t totalPatterns = 0;
    
    try {
        Document doc = parseJsonFile(reportPath);
        
        if (!doc.HasMember("patternStructureAnalysis")) {
            throw std::runtime_error("Pattern structure analysis not found in report");
        }
        
        const Value& structAnalysis = doc["patternStructureAnalysis"];
        if (structAnalysis.HasMember("totalPatterns")) {
            totalPatterns = structAnalysis["totalPatterns"].GetUint();
        }
        
        // Load complexity distribution, component combinations, etc.
        // This would be used for additional pattern generation optimization
        
        AlgorithmInsights insights(totalPatterns);
        
        // Loaded pattern structure analysis
        
        return insights;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load pattern structure analysis: " + std::string(e.what()));
    }
}

std::unique_ptr<PALAnalysisData> PALAnalysisLoader::loadCompleteAnalysis(const std::string& reportDir) {
    PALIndexMappings indexMappings;
    std::map<SearchType, ComponentUsageStats> componentStats;
    AlgorithmInsights algorithmInsights;
    std::vector<std::string> sourceReports;
    
    try {
        fs::path dirPath(reportDir);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            throw std::runtime_error("Report directory does not exist: " + reportDir);
        }
        
        // Load index mappings
        fs::path indexMappingPath = dirPath / "index_mapping_report.json";
        if (fs::exists(indexMappingPath)) {
            indexMappings = loadIndexMappings(indexMappingPath.string());
            sourceReports.push_back(indexMappingPath.string());
        }
        
        // Load component statistics
        fs::path componentStatsPath = dirPath / "component_analysis_report.json";
        if (fs::exists(componentStatsPath)) {
            componentStats = loadComponentStats(componentStatsPath.string());
            sourceReports.push_back(componentStatsPath.string());
        }
        
        // Load algorithm insights
        fs::path algorithmInsightsPath = dirPath / "search_algorithm_report.json";
        if (fs::exists(algorithmInsightsPath)) {
            algorithmInsights = loadAlgorithmInsights(algorithmInsightsPath.string());
            sourceReports.push_back(algorithmInsightsPath.string());
        }
        
        // Load pattern structure analysis (if available)
        fs::path patternStructurePath = dirPath / "pattern_structure_analysis.json";
        if (fs::exists(patternStructurePath)) {
            auto structureInsights = loadPatternStructureAnalysis(patternStructurePath.string());
            // Create new insights with merged data
            algorithmInsights = AlgorithmInsights(
                std::max(algorithmInsights.getTotalPatterns(), structureInsights.getTotalPatterns()),
                algorithmInsights.getChainedPatterns(),
                algorithmInsights.getChainingPercentage(),
                algorithmInsights.getCuratedGroups(),
                algorithmInsights.getComponentConstraints(),
                algorithmInsights.getSearchSpaceReduction(),
                algorithmInsights.getBarSpreadDistribution(),
                algorithmInsights.getMaxOffsetDistribution()
            );
            sourceReports.push_back(patternStructurePath.string());
        }
        
        // Build component hierarchy rules
        ComponentHierarchyRules hierarchyRules = buildComponentHierarchy(indexMappings);
        
        // Keep the original total patterns from index mappings metadata
        // Only update if component stats provide a higher total
        size_t componentStatsTotalPatterns = 0;
        for (const auto& [searchType, stats] : componentStats) {
            componentStatsTotalPatterns += stats.getTotalPatterns();
        }
        
        size_t finalTotalPatterns = std::max(indexMappings.getTotalPatterns(), componentStatsTotalPatterns);
        
        // Always create new index mappings with component stats included
        indexMappings = PALIndexMappings(
            indexMappings.getIndexToGroup(),
            indexMappings.getSearchTypeToIndices(),
            componentStats,
            finalTotalPatterns,
            indexMappings.getTotalIndices(),
            indexMappings.getAnalysisDate()
        );
        
        auto analysisData = std::make_unique<PALAnalysisData>(
            std::move(indexMappings),
            std::move(componentStats),
            std::move(algorithmInsights),
            std::move(hierarchyRules),
            "1.0",
            sourceReports
        );
        
        // Loaded complete PAL analysis
        
        return analysisData;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load complete analysis: " + std::string(e.what()));
    }
}

ComponentHierarchyRules PALAnalysisLoader::buildComponentHierarchy(const PALIndexMappings& mappings) {
    ComponentHierarchyRules rules;
    
    // Build component hierarchy based on PAL's discovered patterns:
    // Full OHLC (1-153) → Mixed (154-325) → Dual (326-478) → Single (480-545)
    
    for (const auto& [indexNumber, group] : mappings.getIndexToGroup()) {
        rules.addAllowedComponents(indexNumber, group.getComponentTypes());
        rules.addComponentSetIndex(group.getComponentTypes(), indexNumber);
    }
    
    // Built component hierarchy rules
    
    return rules;
}

Document PALAnalysisLoader::parseJsonFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
    
    // Skip header lines (PAL reports have header comments)
    std::string line;
    std::string jsonContent;
    bool foundJsonStart = false;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Look for JSON start
        if (!foundJsonStart && (line[0] == '{' || line[0] == '[')) {
            foundJsonStart = true;
        }
        
        if (foundJsonStart) {
            jsonContent += line + "\n";
        }
    }
    
    Document doc;
    doc.Parse(jsonContent.c_str());
    
    if (doc.HasParseError()) {
        throw std::runtime_error("JSON parse error in file: " + filePath);
    }
    
    return doc;
}

std::vector<uint8_t> PALAnalysisLoader::parseBarOffsets(const Value& jsonArray) {
    std::vector<uint8_t> offsets;
    if (jsonArray.IsArray()) {
        for (SizeType i = 0; i < jsonArray.Size(); ++i) {
            if (jsonArray[i].IsUint()) {
                offsets.push_back(static_cast<uint8_t>(jsonArray[i].GetUint()));
            }
        }
    }
    return offsets;
}

std::set<PriceComponentType> PALAnalysisLoader::parseComponentTypes(const Value& jsonArray) {
    std::set<PriceComponentType> types;
    if (jsonArray.IsArray()) {
        for (SizeType i = 0; i < jsonArray.Size(); ++i) {
            if (jsonArray[i].IsString()) {
                types.insert(stringToComponentType(jsonArray[i].GetString()));
            }
        }
    }
    return types;
}

double PALAnalysisLoader::calculateGenerationPriority(uint32_t patternCount, uint32_t totalPatterns) {
    if (totalPatterns == 0) return 0.0;
    
    // Higher pattern count = higher priority
    // Normalize to 0.0-1.0 range with logarithmic scaling for better distribution
    double ratio = static_cast<double>(patternCount) / totalPatterns;
    return std::min(1.0, std::log10(1.0 + 9.0 * ratio));
}

} // namespace pattern_universe