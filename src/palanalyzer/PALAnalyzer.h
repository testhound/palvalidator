#pragma once

#include "DataStructures.h"
#include "AnalysisDatabase.h"
#include "PatternStructureExtractor.h"
#include "PalParseDriver.h"
#include <string>
#include <vector>
#include <memory>

namespace palanalyzer {

/**
 * @brief Main analysis engine for PAL pattern files
 * 
 * This class orchestrates the analysis of PAL pattern files using the existing
 * PAL parser infrastructure. It supports incremental analysis with persistent
 * storage and comprehensive reporting.
 */
class PALAnalyzer {
public:
    /**
     * @brief Construct analyzer with database path
     * @param databasePath Path to analysis database file
     */
    explicit PALAnalyzer(const std::string& databasePath = "pal_analysis.db");
    
    /**
     * @brief Destructor - ensures analysis is saved
     */
    ~PALAnalyzer();
    
    // Analysis functions
    /**
     * @brief Analyze a single PAL pattern file
     * @param filePath Path to PAL file
     * @param explicitSearchType Optional explicit search type (UNKNOWN means use filename inference)
     * @return True if successful, false on error
     */
    bool analyzeFile(const std::string& filePath, SearchType explicitSearchType = SearchType::UNKNOWN);
    
    /**
     * @brief Analyze multiple PAL pattern files
     * @param filePaths Vector of file paths
     * @param explicitSearchType Optional explicit search type (UNKNOWN means use filename inference)
     * @return Number of successfully analyzed files
     */
    size_t analyzeBatch(const std::vector<std::string>& filePaths, SearchType explicitSearchType = SearchType::UNKNOWN);
    
    // Incremental analysis
    /**
     * @brief Add new file to existing analysis
     * @param filePath Path to new PAL file
     * @param explicitSearchType Optional explicit search type (UNKNOWN means use filename inference)
     * @return True if successful, false if already analyzed or error
     */
    bool addNewFile(const std::string& filePath, SearchType explicitSearchType = SearchType::UNKNOWN);
    
    /**
     * @brief Add multiple new files to existing analysis
     * @param filePaths Vector of file paths
     * @param explicitSearchType Optional explicit search type (UNKNOWN means use filename inference)
     * @return Number of newly analyzed files
     */
    size_t addNewFiles(const std::vector<std::string>& filePaths, SearchType explicitSearchType = SearchType::UNKNOWN);
    
    /**
     * @brief Check if file has been analyzed
     * @param filePath File path to check
     * @return True if file has been analyzed, false otherwise
     */
    bool isFileAnalyzed(const std::string& filePath) const;
    
    // Database operations
    /**
     * @brief Load existing analysis from database
     * @return True if successful, false if no database exists
     */
    bool loadExistingAnalysis();
    
    /**
     * @brief Save current analysis to database
     * @return True if successful, false on write error
     */
    bool saveAnalysis();
    
    /**
     * @brief Reset analysis database (clear all data)
     */
    void resetAnalysis();
    
    // Report generation
    /**
     * @brief Generate index mapping report
     * @param outputPath Output file path
     * @return True if successful, false on write error
     */
    bool generateIndexMappingReport(const std::string& outputPath);
    
    /**
     * @brief Generate component analysis report
     * @param outputPath Output file path
     * @return True if successful, false on write error
     */
    bool generateComponentAnalysisReport(const std::string& outputPath);
    
    /**
     * @brief Generate search algorithm insights report
     * @param outputPath Output file path
     * @return True if successful, false on write error
     */
    bool generateSearchAlgorithmReport(const std::string& outputPath);
    
    /**
     * @brief Generate progress report
     * @param outputPath Output file path
     * @return True if successful, false on write error
     */
    bool generateProgressReport(const std::string& outputPath);
    
    /**
     * @brief Generate pattern structure analysis report
     * @param outputPath Output file path
     * @return True if successful, false on write error
     */
    bool generatePatternStructureReport(const std::string& outputPath);
    
    /**
     * @brief Generate all reports to specified directory
     * @param outputDir Output directory path
     * @return True if all reports generated successfully
     */
    bool generateAllReports(const std::string& outputDir);

    /**
     * @brief Generate simplified pattern database report
     * @param outputPath Output file path
     * @return True if successful, false on write error
     */
    bool generateSimplifiedPatternDatabase(const std::string& outputPath);
    
    // Statistics and validation
    /**
     * @brief Get overall analysis statistics
     * @return Analysis statistics
     */
    AnalysisStats getStats() const;
    
    /**
     * @brief Validate analysis consistency
     * @return True if consistent, false if conflicts detected
     */
    bool validateAnalysis();
    
    /**
     * @brief Export analysis database to JSON file
     * @param exportPath Export file path
     * @return True if successful, false on error
     */
    bool exportAnalysis(const std::string& exportPath);
    
    /**
     * @brief Import analysis database from JSON file
     * @param importPath Import file path
     * @return True if successful, false on error
     */
    bool importAnalysis(const std::string& importPath);
    
    /**
     * @brief Get database path
     * @return Database file path
     */
    const std::string& getDatabasePath() const { return databasePath; }

private:
    // Core components
    std::unique_ptr<AnalysisDatabase> database;
    PatternStructureExtractor extractor;
    std::string databasePath;
    
    // Analysis state
    bool analysisLoaded;
    size_t totalFilesProcessed;
    size_t totalPatternsAnalyzed;
    
    // Helper methods
    /**
     * @brief Process a single pattern from parsed file
     * @param pattern PAL pattern to analyze
     * @param sourceFile Source file path
     * @param searchType Determined search type
     */
    void processPattern(std::shared_ptr<PriceActionLabPattern> pattern,
                       const std::string& sourceFile,
                       const std::string& searchType);
    
    /**
     * @brief Validate index consistency during analysis
     * @param index Index number
     * @param newInfo New bar combination info
     * @param sourceFile Source file for error reporting
     */
    void validateIndexConsistency(uint32_t index, 
                                 const BarCombinationInfo& newInfo,
                                 const std::string& sourceFile);
    
    /**
     * @brief Update file analysis tracking
     * @param filePath File path
     * @param patternCount Number of patterns processed
     * @param uniqueIndices Number of unique indices found
     */
    void updateFileAnalysis(const std::string& filePath,
                           uint32_t patternCount,
                           uint32_t uniqueIndices);
    
    /**
     * @brief Generate report header with metadata
     * @param title Report title
     * @return Header string
     */
    std::string generateReportHeader(const std::string& title);
    
    /**
     * @brief Format time point for reports
     * @param tp Time point to format
     * @return Formatted time string
     */
    std::string formatTimePoint(const std::chrono::system_clock::time_point& tp);
    
    /**
     * @brief Ensure output directory exists
     * @param path Directory path
     * @return True if directory exists or was created
     */
    bool ensureDirectoryExists(const std::string& path);
};

} // namespace palanalyzer