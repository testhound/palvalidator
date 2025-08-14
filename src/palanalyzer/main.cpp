#include "PALAnalyzer.h"
#include <iostream>
#include <vector>
#include <string>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

void printUsage(const po::options_description& desc) {
    std::cout << "PAL Pattern Analyzer - Reverse-engineer PAL search algorithm\n\n";
    std::cout << "Usage: palanalyzer [options]\n\n";
    std::cout << desc << std::endl;
    
    std::cout << "\nExamples:\n";
    std::cout << "  # Initial analysis (single file)\n";
    std::cout << "  palanalyzer --analyze Sample_IR/APP_Extended_NoDelay.txt\n\n";
    std::cout << "  # Initial analysis with explicit search type\n";
    std::cout << "  palanalyzer --analyze Sample_IR/APP_patterns.txt --search-type Extended\n\n";
    std::cout << "  # Initial analysis (multiple files)\n";
    std::cout << "  palanalyzer --analyze-batch \"Sample_IR/*_Extended_*.txt\"\n\n";
    std::cout << "  # Add more files incrementally\n";
    std::cout << "  palanalyzer --add Sample_IR/PLTR_Extended_NoDelay.txt\n\n";
    std::cout << "  # Batch add with explicit search type\n";
    std::cout << "  palanalyzer --add-batch \"Sample_IR/*.txt\" --search-type Deep\n\n";
    std::cout << "  # Generate all reports\n";
    std::cout << "  palanalyzer --report-all --db pal_analysis.db --output reports/\n\n";
    std::cout << "  # Check analysis status\n";
    std::cout << "  palanalyzer --status --db pal_analysis.db\n";
}

std::vector<std::string> expandGlobPattern(const std::string& pattern) {
    std::vector<std::string> files;
    
    try {
        // Simple glob expansion using boost::filesystem
        fs::path patternPath(pattern);
        fs::path parentDir = patternPath.parent_path();
        std::string filename = patternPath.filename().string();
        
        if (parentDir.empty()) {
            parentDir = fs::current_path();
        }
        
        if (fs::exists(parentDir) && fs::is_directory(parentDir)) {
            for (fs::directory_iterator it(parentDir); it != fs::directory_iterator(); ++it) {
                if (fs::is_regular_file(it->status())) {
                    std::string currentFile = it->path().filename().string();
                    
                    // Simple wildcard matching (* and ?)
                    if (filename.find('*') != std::string::npos || filename.find('?') != std::string::npos) {
                        // Convert glob pattern to regex-like matching
                        std::string regexPattern = filename;
                        boost::replace_all(regexPattern, "*", ".*");
                        boost::replace_all(regexPattern, "?", ".");
                        
                        // Simple contains check for now
                        if (filename == "*" || 
                            (filename.find("*") != std::string::npos && 
                             currentFile.find(filename.substr(0, filename.find("*"))) != std::string::npos)) {
                            files.push_back(it->path().string());
                        }
                    } else if (currentFile == filename) {
                        files.push_back(it->path().string());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error expanding pattern '" << pattern << "': " << e.what() << std::endl;
    }
    
    return files;
}

int main(int argc, char* argv[]) {
    try {
        po::options_description desc("Options");
        desc.add_options()
            ("help,h", "Show help message")
            ("analyze", po::value<std::string>(), "Analyze single PAL file")
            ("analyze-batch", po::value<std::string>(), "Analyze multiple files (glob pattern)")
            ("add", po::value<std::string>(), "Add single file to existing analysis")
            ("add-batch", po::value<std::string>(), "Add multiple files (glob pattern)")
            ("search-type,s", po::value<std::string>(), "Explicitly specify search type (Extended, Deep, Close, High-Low, Open-Close, Basic, Mixed). If not specified, will infer from filename.")
            ("db", po::value<std::string>()->default_value("pal_analysis.db"), "Database file path")
            ("output,o", po::value<std::string>()->default_value("reports"), "Output directory for reports")
            ("report-all", "Generate all reports")
            ("report-index", "Generate index mapping report")
            ("report-component", "Generate component analysis report")
            ("report-algorithm", "Generate search algorithm report")
            ("report-structure", "Generate pattern structure analysis report")
            ("report-structure-db", "Generate simplified pattern database report")
            ("report-progress", "Generate progress report")
            ("status", "Show analysis status")
            ("validate", "Validate analysis consistency")
            ("reset", "Reset analysis database")
            ("export", po::value<std::string>(), "Export analysis to JSON file")
            ("import", po::value<std::string>(), "Import analysis from JSON file")
            ("verbose,v", "Verbose output");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        
        if (vm.count("help")) {
            printUsage(desc);
            return 0;
        }
        
        std::string dbPath = vm["db"].as<std::string>();
        std::string outputDir = vm["output"].as<std::string>();
        bool verbose = vm.count("verbose") > 0;
        
        // Get explicit search type if provided
        palanalyzer::SearchType explicitSearchType = palanalyzer::SearchType::UNKNOWN;
        if (vm.count("search-type")) {
            std::string searchTypeStr = vm["search-type"].as<std::string>();
            explicitSearchType = palanalyzer::stringToSearchType(searchTypeStr);
            if (verbose) {
                std::cout << "Using explicit search type: " << searchTypeStr << std::endl;
            }
            if (explicitSearchType == palanalyzer::SearchType::UNKNOWN) {
                std::cerr << "Warning: Unknown search type '" << searchTypeStr << "', will use filename inference" << std::endl;
            }
        }
        
        palanalyzer::PALAnalyzer analyzer(dbPath);
        
        // Load existing analysis if database exists
        if (fs::exists(dbPath)) {
            if (verbose) std::cout << "Loading existing analysis from " << dbPath << std::endl;
            if (!analyzer.loadExistingAnalysis()) {
                std::cerr << "Warning: Failed to load existing analysis" << std::endl;
            }
        }
        
        // Handle reset command
        if (vm.count("reset")) {
            std::cout << "Resetting analysis database..." << std::endl;
            analyzer.resetAnalysis();
            std::cout << "Analysis database reset successfully." << std::endl;
            return 0;
        }
        
        // Handle import command
        if (vm.count("import")) {
            std::string importPath = vm["import"].as<std::string>();
            std::cout << "Importing analysis from " << importPath << "..." << std::endl;
            if (analyzer.importAnalysis(importPath)) {
                std::cout << "Analysis imported successfully." << std::endl;
            } else {
                std::cerr << "Error: Failed to import analysis" << std::endl;
                return 1;
            }
        }
        
        // Handle analyze command
        if (vm.count("analyze")) {
            std::string filePath = vm["analyze"].as<std::string>();
            std::cout << "Analyzing file: " << filePath << std::endl;
            
            if (analyzer.analyzeFile(filePath, explicitSearchType)) {
                std::cout << "Analysis completed successfully." << std::endl;
            } else {
                std::cerr << "Error: Failed to analyze file" << std::endl;
                return 1;
            }
        }
        
        // Handle analyze-batch command
        if (vm.count("analyze-batch")) {
            std::string pattern = vm["analyze-batch"].as<std::string>();
            std::cout << "Analyzing files matching pattern: " << pattern << std::endl;
            
            std::vector<std::string> files = expandGlobPattern(pattern);
            if (files.empty()) {
                std::cout << "No files found matching pattern." << std::endl;
            } else {
                std::cout << "Found " << files.size() << " files to analyze." << std::endl;
                size_t analyzed = analyzer.analyzeBatch(files, explicitSearchType);
                std::cout << "Successfully analyzed " << analyzed << "/" << files.size() << " files." << std::endl;
            }
        }
        
        // Handle add command
        if (vm.count("add")) {
            std::string filePath = vm["add"].as<std::string>();
            std::cout << "Adding file to analysis: " << filePath << std::endl;
            
            if (analyzer.addNewFile(filePath, explicitSearchType)) {
                std::cout << "File added successfully." << std::endl;
            } else {
                std::cout << "File was already analyzed or failed to analyze." << std::endl;
            }
        }
        
        // Handle add-batch command
        if (vm.count("add-batch")) {
            std::string pattern = vm["add-batch"].as<std::string>();
            std::cout << "Adding files matching pattern: " << pattern << std::endl;
            
            std::vector<std::string> files = expandGlobPattern(pattern);
            if (files.empty()) {
                std::cout << "No files found matching pattern." << std::endl;
            } else {
                std::cout << "Found " << files.size() << " files to analyze." << std::endl;
                size_t added = analyzer.addNewFiles(files, explicitSearchType);
                std::cout << "Added " << added << " new files to analysis." << std::endl;
            }
        }
        
        // Handle status command
        if (vm.count("status")) {
            auto stats = analyzer.getStats();
            std::cout << "\nPAL Analysis Status\n";
            std::cout << "==================\n";
            std::cout << "Database: " << dbPath << std::endl;
            std::cout << "Total Patterns: " << stats.getTotalPatterns() << std::endl;
            std::cout << "Unique Indices: " << stats.getUniqueIndices() << std::endl;
            std::cout << "Analyzed Files: " << stats.getAnalyzedFiles() << std::endl;
            
            if (!stats.getSearchTypeBreakdown().empty()) {
                std::cout << "\nSearch Type Breakdown:\n";
                for (const auto& pair : stats.getSearchTypeBreakdown()) {
                    std::cout << "  " << pair.first << ": " << pair.second << " patterns\n";
                }
            }
            std::cout << std::endl;
        }
        
        // Handle validate command
        if (vm.count("validate")) {
            std::cout << "Validating analysis consistency..." << std::endl;
            if (analyzer.validateAnalysis()) {
                std::cout << "Analysis is consistent." << std::endl;
            } else {
                std::cout << "Warning: Inconsistencies detected in analysis." << std::endl;
            }
        }
        
        // Handle export command
        if (vm.count("export")) {
            std::string exportPath = vm["export"].as<std::string>();
            std::cout << "Exporting analysis to " << exportPath << "..." << std::endl;
            if (analyzer.exportAnalysis(exportPath)) {
                std::cout << "Analysis exported successfully." << std::endl;
            } else {
                std::cerr << "Error: Failed to export analysis" << std::endl;
                return 1;
            }
        }
        
        // Handle report generation
        bool generateReports = false;
        
        if (vm.count("report-all")) {
            std::cout << "Generating all reports to " << outputDir << "..." << std::endl;
            if (analyzer.generateAllReports(outputDir)) {
                std::cout << "All reports generated successfully." << std::endl;
            } else {
                std::cerr << "Error: Failed to generate some reports" << std::endl;
                return 1;
            }
            generateReports = true;
        }
        
        if (vm.count("report-index")) {
            std::string reportPath = outputDir + "/index_mapping_report.json";
            std::cout << "Generating index mapping report..." << std::endl;
            if (analyzer.generateIndexMappingReport(reportPath)) {
                std::cout << "Index mapping report generated: " << reportPath << std::endl;
            } else {
                std::cerr << "Error: Failed to generate index mapping report" << std::endl;
            }
            generateReports = true;
        }
        
        if (vm.count("report-component")) {
            std::string reportPath = outputDir + "/component_analysis_report.json";
            std::cout << "Generating component analysis report..." << std::endl;
            if (analyzer.generateComponentAnalysisReport(reportPath)) {
                std::cout << "Component analysis report generated: " << reportPath << std::endl;
            } else {
                std::cerr << "Error: Failed to generate component analysis report" << std::endl;
            }
            generateReports = true;
        }
        
        if (vm.count("report-algorithm")) {
            std::string reportPath = outputDir + "/search_algorithm_report.json";
            std::cout << "Generating search algorithm report..." << std::endl;
            if (analyzer.generateSearchAlgorithmReport(reportPath)) {
                std::cout << "Search algorithm report generated: " << reportPath << std::endl;
            } else {
                std::cerr << "Error: Failed to generate search algorithm report" << std::endl;
            }
            generateReports = true;
        }
        
        if (vm.count("report-structure")) {
            std::string reportPath = outputDir + "/pattern_structure_analysis.json";
            std::cout << "Generating pattern structure analysis report..." << std::endl;
            if (analyzer.generatePatternStructureReport(reportPath)) {
                std::cout << "Pattern structure analysis report generated: " << reportPath << std::endl;
            } else {
                std::cerr << "Error: Failed to generate pattern structure analysis report" << std::endl;
            }
            generateReports = true;
        }
        
        if (vm.count("report-progress")) {
            std::string reportPath = outputDir + "/progress_report.txt";
            std::cout << "Generating progress report..." << std::endl;
            if (analyzer.generateProgressReport(reportPath)) {
                std::cout << "Progress report generated: " << reportPath << std::endl;
            } else {
                std::cerr << "Error: Failed to generate progress report" << std::endl;
            }
            generateReports = true;
        }

        if (vm.count("report-structure-db")) {
            std::string reportPath = outputDir + "/simplified_pattern_database.json";
            std::cout << "Generating simplified pattern database report..." << std::endl;
            if (analyzer.generateSimplifiedPatternDatabase(reportPath)) {
                std::cout << "Simplified pattern database report generated: " << reportPath << std::endl;
            } else {
                std::cerr << "Error: Failed to generate simplified pattern database report" << std::endl;
            }
            generateReports = true;
        }
        
        // Save analysis if any changes were made
        if (vm.count("analyze") || vm.count("analyze-batch") || vm.count("add") || vm.count("add-batch") || vm.count("import")) {
            if (verbose) std::cout << "Saving analysis to database..." << std::endl;
            if (!analyzer.saveAnalysis()) {
                std::cerr << "Warning: Failed to save analysis to database" << std::endl;
            }
        }
        
        // If no specific command was given, show help
        if (!vm.count("analyze") && !vm.count("analyze-batch") && !vm.count("add") && !vm.count("add-batch") &&
            !vm.count("status") && !vm.count("validate") && !vm.count("reset") &&
            !vm.count("export") && !vm.count("import") && !generateReports) {
            printUsage(desc);
            return 0;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}