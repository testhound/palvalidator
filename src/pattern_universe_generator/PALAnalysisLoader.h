#pragma once

#include "OptimizedDataStructures.h"
#include <string>
#include <memory>
#include <rapidjson/document.h>

namespace pattern_universe {

// Forward declarations
class PALAnalysisData;

/**
 * @brief Loads PAL analysis data from generated reports
 *
 * This class reads the PAL analyzer reports and converts them into
 * the data structures needed for optimized pattern generation.
 */
class PALAnalysisLoader {
public:
    PALAnalysisLoader() = default;
    ~PALAnalysisLoader() = default;

    /**
     * @brief Load PAL index mappings from index_mapping_report.json
     * @param reportPath Path to the index mapping report
     * @return PAL index mappings structure
     */
    PALIndexMappings loadIndexMappings(const std::string& reportPath);

    /**
     * @brief Load component usage statistics from component_analysis_report.json
     * @param reportPath Path to the component analysis report
     * @return Component usage statistics by search type
     */
    std::map<SearchType, ComponentUsageStats> loadComponentStats(const std::string& reportPath);

    /**
     * @brief Load algorithm insights from search_algorithm_report.json
     * @param reportPath Path to the search algorithm report
     * @return Algorithm insights structure
     */
    AlgorithmInsights loadAlgorithmInsights(const std::string& reportPath);

    /**
     * @brief Load pattern structure analysis from pattern_structure_analysis.json
     * @param reportPath Path to the pattern structure analysis report
     * @return Pattern structure insights
     */
    AlgorithmInsights loadPatternStructureAnalysis(const std::string& reportPath);

    /**
     * @brief Load complete PAL analysis from report directory
     * @param reportDir Directory containing all PAL analysis reports
     * @return Complete PAL analysis data
     */
    std::unique_ptr<PALAnalysisData> loadCompleteAnalysis(const std::string& reportDir);

    /**
     * @brief Build component hierarchy rules from index mappings
     * @param mappings PAL index mappings
     * @return Component hierarchy rules
     */
    ComponentHierarchyRules buildComponentHierarchy(const PALIndexMappings& mappings);

private:
    /**
     * @brief Parse JSON file and return parsed object
     * @param filePath Path to JSON file
     * @return Parsed JSON object
     */
    rapidjson::Document parseJsonFile(const std::string& filePath);

    /**
     * @brief Parse bar offsets from JSON array
     * @param jsonArray JSON array of bar offsets
     * @return Vector of bar offsets
     */
    std::vector<uint8_t> parseBarOffsets(const rapidjson::Value& jsonArray);

    /**
     * @brief Parse component types from JSON array
     * @param jsonArray JSON array of component type strings
     * @return Set of component types
     */
    std::set<PriceComponentType> parseComponentTypes(const rapidjson::Value& jsonArray);

    /**
     * @brief Calculate generation priority based on pattern count
     * @param patternCount Number of patterns for this index
     * @param totalPatterns Total patterns in search type
     * @return Priority value (0.0 to 1.0)
     */
    double calculateGenerationPriority(uint32_t patternCount, uint32_t totalPatterns);
};

/**
 * @brief Complete PAL analysis data structure
 */
class PALAnalysisData {
public:
    PALAnalysisData() : mLoadedAt(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Constructor for creating PALAnalysisData with initial values
     * @param indexMappings Initial index mappings
     * @param componentStats Component usage statistics by search type
     * @param algorithmInsights Algorithm insights from PAL analysis
     * @param hierarchyRules Component hierarchy rules
     * @param analysisVersion Version string for the analysis
     * @param sourceReports List of source report files
     */
    PALAnalysisData(PALIndexMappings indexMappings,
                   std::map<SearchType, ComponentUsageStats> componentStats,
                   AlgorithmInsights algorithmInsights,
                   ComponentHierarchyRules hierarchyRules,
                   const std::string& analysisVersion,
                   const std::vector<std::string>& sourceReports)
        : mIndexMappings(std::move(indexMappings))
        , mComponentStats(std::move(componentStats))
        , mAlgorithmInsights(std::move(algorithmInsights))
        , mHierarchyRules(std::move(hierarchyRules))
        , mAnalysisVersion(analysisVersion)
        , mLoadedAt(std::chrono::system_clock::now())
        , mSourceReports(sourceReports) {}

    const PALIndexMappings& getIndexMappings() const { return mIndexMappings; }
    PALIndexMappings& getIndexMappings() { return mIndexMappings; }

    const std::map<SearchType, ComponentUsageStats>& getComponentStats() const { return mComponentStats; }
    std::map<SearchType, ComponentUsageStats>& getComponentStats() { return mComponentStats; }

    const AlgorithmInsights& getAlgorithmInsights() const { return mAlgorithmInsights; }
    AlgorithmInsights& getAlgorithmInsights() { return mAlgorithmInsights; }

    const ComponentHierarchyRules& getHierarchyRules() const { return mHierarchyRules; }

    const std::string& getAnalysisVersion() const { return mAnalysisVersion; }

    std::chrono::system_clock::time_point getLoadedAt() const { return mLoadedAt; }

    const std::vector<std::string>& getSourceReports() const { return mSourceReports; }
    std::vector<std::string>& getSourceReports() { return mSourceReports; }

private:
    PALIndexMappings mIndexMappings;
    std::map<SearchType, ComponentUsageStats> mComponentStats;
    AlgorithmInsights mAlgorithmInsights;
    ComponentHierarchyRules mHierarchyRules;
    std::string mAnalysisVersion;
    std::chrono::system_clock::time_point mLoadedAt;
    std::vector<std::string> mSourceReports;
};

} // namespace pattern_universe