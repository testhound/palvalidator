#include "PALAnalyzer.h"
#include "AnalysisSerializer.h"
#include "PalParseDriver.h"
#include "AstResourceManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace fs = boost::filesystem;

namespace palanalyzer {

PALAnalyzer::PALAnalyzer(const std::string& databasePath) 
    : databasePath(databasePath), analysisLoaded(false), 
      totalFilesProcessed(0), totalPatternsAnalyzed(0) {
    database = std::make_unique<AnalysisDatabase>(databasePath);
}

PALAnalyzer::~PALAnalyzer() {
    if (database && database->isModified()) {
        std::cout << "Auto-saving analysis database..." << std::endl;
        database->save();
    }
}

bool PALAnalyzer::analyzeFile(const std::string& filePath, SearchType explicitSearchType) {
    try {
        std::cout << "Analyzing file: " << filePath << std::endl;
        
        if (!fs::exists(filePath)) {
            std::cerr << "Error: File does not exist: " << filePath << std::endl;
            return false;
        }
        
        // Create parse driver
        mkc_palast::PalParseDriver driver(filePath);
        
        // Parse the PAL file
        if (driver.Parse() != 0) {
            std::cerr << "Error: Failed to parse PAL file: " << filePath << std::endl;
            return false;
        }
        
        // Get parsed patterns
        auto palSystem = driver.getPalStrategies();
        if (!palSystem) {
            std::cout << "Warning: No patterns found in file: " << filePath << std::endl;
            return true; // Not an error, just empty file
        }
        
        // Extract patterns from the system
        std::vector<std::shared_ptr<PriceActionLabPattern>> patterns;
        for (auto it = palSystem->allPatternsBegin(); it != palSystem->allPatternsEnd(); ++it) {
            patterns.push_back(*it);
        }
        if (patterns.empty()) {
            std::cout << "Warning: No patterns found in file: " << filePath << std::endl;
            return true; // Not an error, just empty file
        }
        
        // Determine search type (explicit override or filename inference)
        SearchType searchTypeEnum;
        if (explicitSearchType != SearchType::UNKNOWN) {
            searchTypeEnum = explicitSearchType;
        } else {
            searchTypeEnum = extractor.determineSearchType(filePath);
        }
        std::string searchType = searchTypeToString(searchTypeEnum);
        
        if (explicitSearchType != SearchType::UNKNOWN) {
            std::cout << "Found " << patterns.size() << " patterns (Search type: " << searchType << " - explicit)" << std::endl;
        } else {
            std::cout << "Found " << patterns.size() << " patterns (Search type: " << searchType << " - inferred)" << std::endl;
        }
        
        // Process each pattern
        std::set<uint32_t> uniqueIndices;
        for (const auto& pattern : patterns) {
            processPattern(pattern, filePath, searchType);
            
            // Track unique indices for this file
            auto description = pattern->getPatternDescription();
            if (description) {
                uniqueIndices.insert(description->getpatternIndex());
            }
        }
        
        // Update file analysis tracking
        updateFileAnalysis(filePath, static_cast<uint32_t>(patterns.size()),
                          static_cast<uint32_t>(uniqueIndices.size()));
        
        totalFilesProcessed++;
        totalPatternsAnalyzed += patterns.size();
        
        std::cout << "Successfully analyzed " << patterns.size() << " patterns with "
                  << uniqueIndices.size() << " unique indices" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error analyzing file " << filePath << ": " << e.what() << std::endl;
        return false;
    }
}

size_t PALAnalyzer::analyzeBatch(const std::vector<std::string>& filePaths, SearchType explicitSearchType) {
    size_t successCount = 0;
    
    std::cout << "Starting batch analysis of " << filePaths.size() << " files..." << std::endl;
    
    for (const std::string& filePath : filePaths) {
        if (analyzeFile(filePath, explicitSearchType)) {
            successCount++;
        }
    }
    
    std::cout << "Batch analysis complete: " << successCount << "/" << filePaths.size()
              << " files processed successfully" << std::endl;
    
    return successCount;
}

bool PALAnalyzer::addNewFile(const std::string& filePath, SearchType explicitSearchType) {
    if (isFileAnalyzed(filePath)) {
        std::cout << "File already analyzed: " << filePath << std::endl;
        return false;
    }
    
    return analyzeFile(filePath, explicitSearchType);
}

size_t PALAnalyzer::addNewFiles(const std::vector<std::string>& filePaths, SearchType explicitSearchType) {
    std::vector<std::string> newFiles;
    
    // Filter out already analyzed files
    for (const std::string& filePath : filePaths) {
        if (!isFileAnalyzed(filePath)) {
            newFiles.push_back(filePath);
        }
    }
    
    if (newFiles.empty()) {
        std::cout << "All files have already been analyzed." << std::endl;
        return 0;
    }
    
    std::cout << "Adding " << newFiles.size() << " new files (skipping "
              << (filePaths.size() - newFiles.size()) << " already analyzed)" << std::endl;
    
    return analyzeBatch(newFiles, explicitSearchType);
}

bool PALAnalyzer::isFileAnalyzed(const std::string& filePath) const {
    return database->isFileAnalyzed(filePath);
}

bool PALAnalyzer::loadExistingAnalysis() {
    bool success = database->load();
    if (success) {
        analysisLoaded = true;
        std::cout << "Loaded existing analysis with " << database->getTotalPatterns() 
                  << " patterns and " << database->getUniqueIndices() << " unique indices" << std::endl;
    }
    return success;
}

bool PALAnalyzer::saveAnalysis() {
    return database->save();
}

void PALAnalyzer::resetAnalysis() {
    database->clear();
    analysisLoaded = false;
    totalFilesProcessed = 0;
    totalPatternsAnalyzed = 0;
}

bool PALAnalyzer::generateIndexMappingReport(const std::string& outputPath) {
    try {
        ensureDirectoryExists(fs::path(outputPath).parent_path().string());
        
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << outputPath << std::endl;
            return false;
        }
        
        file << generateReportHeader("Index Mapping Report");
        
        // Export index mappings as JSON
        const auto& mappings = database->getIndexMappings();
        
        file << "{\n";
        file << "  \"indexMappings\": {\n";
        
        bool first = true;
        for (const auto& pair : mappings) {
            if (!first) file << ",\n";
            first = false;
            
            file << "    \"" << pair.first << "\": {\n";
            file << "      \"barOffsets\": [";
            for (size_t i = 0; i < pair.second.getBarOffsets().size(); ++i) {
                if (i > 0) file << ", ";
                file << static_cast<int>(pair.second.getBarOffsets()[i]);
            }
            file << "],\n";
            
            file << "      \"componentTypes\": [";
            bool firstComp = true;
            for (const auto& type : pair.second.getComponentTypes()) {
                if (!firstComp) file << ", ";
                firstComp = false;
                file << "\"" << componentTypeToString(type) << "\"";
            }
            file << "],\n";
            
            file << "      \"patternCount\": " << pair.second.getPatternCount() << ",\n";
            file << "      \"searchType\": \"" << pair.second.getSearchType() << "\",\n";
            file << "      \"minPatternLength\": " << static_cast<int>(pair.second.getMinPatternLength()) << ",\n";
            file << "      \"maxPatternLength\": " << static_cast<int>(pair.second.getMaxPatternLength()) << "\n";
            file << "    }";
        }
        
        file << "\n  }\n";
        file << "}\n";
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating index mapping report: " << e.what() << std::endl;
        return false;
    }
}

bool PALAnalyzer::generateComponentAnalysisReport(const std::string& outputPath) {
    try {
        ensureDirectoryExists(fs::path(outputPath).parent_path().string());
        
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << outputPath << std::endl;
            return false;
        }
        
        file << generateReportHeader("Component Analysis Report");
        
        // Analyze component usage across search types
        const auto& searchTypeStats = database->getAllSearchTypeStats();
        
        file << "{\n";
        file << "  \"componentAnalysis\": {\n";
        
        bool first = true;
        for (const auto& pair : searchTypeStats) {
            if (!first) file << ",\n";
            first = false;
            
            file << "    \"" << pair.first << "\": {\n";
            file << "      \"totalPatterns\": " << pair.second.getTotalPatterns() << ",\n";
            file << "      \"uniqueIndices\": " << pair.second.getUniqueIndices().size() << ",\n";
            
            file << "      \"componentUsage\": {\n";
            bool firstComp = true;
            for (const auto& compPair : pair.second.getComponentUsage()) {
                if (!firstComp) file << ",\n";
                firstComp = false;
                file << "        \"" << componentTypeToString(compPair.first) 
                     << "\": " << compPair.second;
            }
            file << "\n      },\n";
            
            file << "      \"patternLengthDistribution\": {\n";
            bool firstLen = true;
            for (const auto& lenPair : pair.second.getPatternLengthDistribution()) {
                if (!firstLen) file << ",\n";
                firstLen = false;
                file << "        \"" << static_cast<int>(lenPair.first) 
                     << "\": " << lenPair.second;
            }
            file << "\n      }\n";
            file << "    }";
        }
        
        file << "\n  }\n";
        file << "}\n";
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating component analysis report: " << e.what() << std::endl;
        return false;
    }
}

bool PALAnalyzer::generateSearchAlgorithmReport(const std::string& outputPath) {
    try {
        ensureDirectoryExists(fs::path(outputPath).parent_path().string());
        
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << outputPath << std::endl;
            return false;
        }
        
        file << generateReportHeader("Search Algorithm Analysis Report");
        
        file << "{\n";
        file << "  \"algorithmInsights\": {\n";
        file << "    \"curatedGroups\": \"PAL uses pre-defined bar combinations, not brute force\",\n";
        file << "    \"componentConstraints\": \"Patterns grouped by component types (Close-only, Mixed, Full OHLC)\",\n";
        file << "    \"searchSpaceReduction\": \"Length limits, semantic validation, transitive chaining\"\n";
        file << "  },\n";
        
        // Analyze chaining patterns
        const auto& patterns = database->getAllPatterns();
        size_t chainedCount = 0;
        std::map<uint8_t, size_t> barSpreadDistribution;
        std::map<uint8_t, size_t> maxOffsetDistribution;
        
        for (const auto& pattern : patterns) {
            if (pattern.isChained()) chainedCount++;
            barSpreadDistribution[pattern.getBarSpread()]++;
            maxOffsetDistribution[pattern.getMaxBarOffset()]++;
        }
        
        file << "  \"patternStructureAnalysis\": {\n";
        file << "    \"totalPatterns\": " << patterns.size() << ",\n";
        file << "    \"chainedPatterns\": " << chainedCount << ",\n";
        file << "    \"chainingPercentage\": " << (patterns.empty() ? 0.0 : (100.0 * chainedCount / patterns.size())) << ",\n";
        
        file << "    \"barSpreadDistribution\": {\n";
        bool first = true;
        for (const auto& pair : barSpreadDistribution) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << static_cast<int>(pair.first) << "\": " << pair.second;
        }
        file << "\n    },\n";
        
        file << "    \"maxOffsetDistribution\": {\n";
        first = true;
        for (const auto& pair : maxOffsetDistribution) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << static_cast<int>(pair.first) << "\": " << pair.second;
        }
        file << "\n    }\n";
        file << "  }\n";
        file << "}\n";
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating search algorithm report: " << e.what() << std::endl;
        return false;
    }
}

bool PALAnalyzer::generateProgressReport(const std::string& outputPath) {
    try {
        ensureDirectoryExists(fs::path(outputPath).parent_path().string());
        
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << outputPath << std::endl;
            return false;
        }
        
        auto stats = getStats();
        
        file << "PAL Analysis Progress Report\n";
        file << "===========================\n";
        file << "Generated: " << formatTimePoint(std::chrono::system_clock::now()) << "\n\n";
        
        file << "Database: " << databasePath << "\n";
        file << "Analysis Period: " << formatTimePoint(stats.getFirstAnalysis()) 
             << " to " << formatTimePoint(stats.getLastAnalysis()) << "\n\n";
        
        file << "Summary Statistics:\n";
        file << "- Total Patterns Analyzed: " << stats.getTotalPatterns() << "\n";
        file << "- Unique Index Numbers: " << stats.getUniqueIndices() << "\n";
        file << "- Files Analyzed: " << stats.getAnalyzedFiles() << "\n";
        
        if (!stats.getSearchTypeBreakdown().empty()) {
            file << "- Search Types Covered: ";
            bool first = true;
            for (const auto& pair : stats.getSearchTypeBreakdown()) {
                if (!first) file << ", ";
                first = false;
                file << pair.first;
            }
            file << "\n\n";
            
            file << "Search Type Breakdown:\n";
            for (const auto& pair : stats.getSearchTypeBreakdown()) {
                file << "- " << pair.first << ": " << pair.second << " patterns\n";
            }
        }
        
        // Index coverage analysis
        file << "\nIndex Coverage Progress:\n";
        const auto& searchTypeStats = database->getAllSearchTypeStats();
        
        // Known targets for different search types
        std::map<std::string, size_t> searchTypeTargets = {
            {"Basic", 20}, {"Extended", 120}, {"Deep", 545}, 
            {"Close", 67}, {"High-Low", 153}, {"Open-Close", 153}
        };
        
        for (const auto& pair : searchTypeStats) {
            const std::string& searchType = pair.first;
            const SearchTypeStats& stats = pair.second;
            
            auto targetIt = searchTypeTargets.find(searchType);
            if (targetIt != searchTypeTargets.end()) {
                size_t target = targetIt->second;
                size_t current = stats.getUniqueIndices().size();
                double percentage = (100.0 * current) / target;
                
                file << "- " << searchType << " Search (Target: " << target << "): " 
                     << current << "/" << target << " (" << std::fixed << std::setprecision(1) 
                     << percentage << "%)\n";
            } else {
                file << "- " << searchType << " Search: " << stats.getUniqueIndices().size() 
                     << " unique indices\n";
            }
        }
        
        file << "\nRecently Analyzed Files:\n";
        auto analyzedFiles = database->getAnalyzedFiles();
        size_t count = 0;
        for (const std::string& filePath : analyzedFiles) {
            if (count++ >= 10) break; // Show last 10 files
            file << "- " << fs::path(filePath).filename().string() << "\n";
        }
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating progress report: " << e.what() << std::endl;
        return false;
    }
}

bool PALAnalyzer::generatePatternStructureReport(const std::string& outputPath) {
    try {
        ensureDirectoryExists(fs::path(outputPath).parent_path().string());
        
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << outputPath << std::endl;
            return false;
        }
        
        file << generateReportHeader("Pattern Structure Analysis Report");
        
        const auto& patterns = database->getAllPatterns();
        const auto& indexMappings = database->getIndexMappings();
        
        file << "{\n";
        file << "  \"patternStructureAnalysis\": {\n";
        file << "    \"totalPatterns\": " << patterns.size() << ",\n";
        file << "    \"totalIndices\": " << indexMappings.size() << ",\n";
        
        // Analyze pattern complexity distribution
        std::map<uint8_t, size_t> complexityDistribution;
        std::map<uint8_t, size_t> lengthDistribution;
        std::map<std::string, size_t> componentCombinations;
        size_t chainedPatterns = 0;
        
        for (const auto& pattern : patterns) {
            complexityDistribution[pattern.getConditionCount()]++;
            lengthDistribution[pattern.getMaxBarOffset() + 1]++;
            if (pattern.isChained()) chainedPatterns++;
            
            // Analyze component combinations
            std::set<std::string> compTypes;
            for (const auto& comp : pattern.getComponents()) {
                compTypes.insert(componentTypeToString(comp.getComponentType()));
            }
            
            std::string combination;
            bool first = true;
            for (const auto& type : compTypes) {
                if (!first) combination += "+";
                first = false;
                combination += type;
            }
            componentCombinations[combination]++;
        }
        
        file << "    \"complexityDistribution\": {\n";
        bool first = true;
        for (const auto& pair : complexityDistribution) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << static_cast<int>(pair.first) << "\": " << pair.second;
        }
        file << "\n    },\n";
        
        file << "    \"patternLengthDistribution\": {\n";
        first = true;
        for (const auto& pair : lengthDistribution) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << static_cast<int>(pair.first) << "\": " << pair.second;
        }
        file << "\n    },\n";
        
        file << "    \"componentCombinations\": {\n";
        first = true;
        for (const auto& pair : componentCombinations) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << pair.first << "\": " << pair.second;
        }
        file << "\n    },\n";
        
        file << "    \"chainingAnalysis\": {\n";
        file << "      \"chainedPatterns\": " << chainedPatterns << ",\n";
        file << "      \"chainingPercentage\": " << (patterns.empty() ? 0.0 : (100.0 * chainedPatterns / patterns.size())) << "\n";
        file << "    },\n";
        
        // Index group analysis
        std::map<std::string, std::set<uint32_t>> searchTypeIndices;
        for (const auto& pair : indexMappings) {
            searchTypeIndices[pair.second.getSearchType()].insert(pair.first);
        }
        
        file << "    \"indexGroupAnalysis\": {\n";
        first = true;
        for (const auto& pair : searchTypeIndices) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << pair.first << "\": {\n";
            file << "        \"totalIndices\": " << pair.second.size() << ",\n";
            file << "        \"indexRange\": {\n";
            file << "          \"min\": " << *pair.second.begin() << ",\n";
            file << "          \"max\": " << *pair.second.rbegin() << "\n";
            file << "        }\n";
            file << "      }";
        }
        file << "\n    }\n";
        
        file << "  }\n";
        file << "}\n";
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating pattern structure report: " << e.what() << std::endl;
        return false;
    }
}

bool PALAnalyzer::generateAllReports(const std::string& outputDir) {
    if (!ensureDirectoryExists(outputDir)) {
        return false;
    }
    
    bool success = true;
    
    success &= generateIndexMappingReport(outputDir + "/index_mapping_report.json");
    success &= generateComponentAnalysisReport(outputDir + "/component_analysis_report.json");
    success &= generateSearchAlgorithmReport(outputDir + "/search_algorithm_report.json");
    success &= generatePatternStructureReport(outputDir + "/pattern_structure_analysis.json");
    success &= generateProgressReport(outputDir + "/progress_report.txt");
    success &= generateSimplifiedPatternDatabase(outputDir + "/simplified_pattern_database.json");
    
    return success;
}

AnalysisStats PALAnalyzer::getStats() const {
    return database->getStats();
}

bool PALAnalyzer::validateAnalysis() {
    // Validate index consistency across all patterns
    bool consistent = true;
    const auto& patterns = database->getAllPatterns();
    
    std::map<uint32_t, BarCombinationInfo> indexValidation;
    
    for (const auto& pattern : patterns) {
        SearchType searchTypeEnum = extractor.determineSearchType(pattern.getSourceFile());
        std::string searchType = searchTypeToString(searchTypeEnum);
        BarCombinationInfo info = extractor.extractBarCombinationInfo(
            pattern.getComponents(),
            searchType
        );
        
        auto it = indexValidation.find(pattern.getIndex());
        if (it != indexValidation.end()) {
            if (!database->validateIndexConsistency(pattern.getIndex(), info)) {
                consistent = false;
            }
        } else {
            indexValidation.emplace(pattern.getIndex(), std::move(info));
        }
    }
    
    return consistent;
}

bool PALAnalyzer::exportAnalysis(const std::string& exportPath) {
    return AnalysisSerializer::saveToFile(*database, exportPath);
}

bool PALAnalyzer::importAnalysis(const std::string& importPath) {
    return AnalysisSerializer::loadFromFile(*database, importPath);
}

void PALAnalyzer::processPattern(std::shared_ptr<PriceActionLabPattern> pattern,
                                const std::string& sourceFile,
                                const std::string& searchType) {
    // Extract pattern analysis
    PatternAnalysis analysis = extractor.extractPatternAnalysis(pattern, sourceFile);
    
    // Add to database
    database->addPattern(analysis);
    
    // Extract and update index mapping
    const auto& components = analysis.getComponents();
    std::set<PriceComponentType> componentTypes;
    std::vector<uint8_t> barCombination;
    for(const auto& comp : components)
    {
        componentTypes.insert(comp.getComponentType());
        barCombination.push_back(comp.getBarOffset());
    }
    std::sort(barCombination.begin(), barCombination.end());
    barCombination.erase(std::unique(barCombination.begin(), barCombination.end()), barCombination.end());

    database->addPatternToIndexGroup(analysis.getIndex(), barCombination, componentTypes, sourceFile, searchType);
}

void PALAnalyzer::validateIndexConsistency(uint32_t index, 
                                          const BarCombinationInfo& newInfo,
                                          const std::string& sourceFile) {
    // This validation logic is now part of the database, so this method may be deprecated
    // or changed to perform higher-level validation if needed.
    if (!database->validateIndexConsistency(index, newInfo)) {
        std::cerr << "Inconsistency detected for index " << index << " in file " << sourceFile << std::endl;
    }
}

void PALAnalyzer::updateFileAnalysis(const std::string& filePath,
                                    uint32_t patternCount,
                                    uint32_t uniqueIndices) {
    FileAnalysisInfo fileInfo(filePath, std::chrono::system_clock::now(), patternCount, uniqueIndices);
    database->addAnalyzedFile(fileInfo);
}

bool PALAnalyzer::generateSimplifiedPatternDatabase(const std::string& outputPath) {
    try {
        ensureDirectoryExists(fs::path(outputPath).parent_path().string());
        
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << outputPath << std::endl;
            return false;
        }
        
        file << AnalysisSerializer::exportToJson(*database);
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating simplified pattern database report: " << e.what() << std::endl;
        return false;
    }
}

std::string PALAnalyzer::generateReportHeader(const std::string& title) {
    std::ostringstream oss;
    oss << "# " << title << "\n";
    oss << "Generated: " << formatTimePoint(std::chrono::system_clock::now()) << "\n";
    oss << "Database: " << databasePath << "\n\n";
    return oss.str();
}

std::string PALAnalyzer::formatTimePoint(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool PALAnalyzer::ensureDirectoryExists(const std::string& path) {
    if (path.empty()) return true;
    
    try {
        if (!fs::exists(path)) {
            fs::create_directories(path);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory " << path << ": " << e.what() << std::endl;
        return false;
    }
}

} // namespace palanalyzer