#include "OptimizedPatternUniverseGenerator.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <chrono>

using namespace pattern_universe;

// Helper function to print usage instructions.
void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " -o <output_file> --pal-analysis <dir> [-m <mode>] [options]" << std::endl;
    std::cerr << "Required:" << std::endl;
    std::cerr << "  -o, --output       : Output file path" << std::endl;
    std::cerr << "  --pal-analysis     : Directory containing PAL analysis reports" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -m, --mode         : Search mode (EXTENDED or DEEP). Default is EXTENDED." << std::endl;
    std::cerr << "  --threads          : Number of threads for parallel processing (default: auto-detect)" << std::endl;
    std::cerr << "  --no-chaining      : Disable pattern chaining optimization" << std::endl;
    std::cerr << "  --no-precompute    : Disable pre-computation optimization" << std::endl;
    std::cerr << "  --format           : Output format (binary, json, csv). Default is binary." << std::endl;
    std::cerr << "  --help             : Show this help message" << std::endl;
}

int main(int argc, char** argv)
{
    std::string outputFile;
    std::string mode = "EXTENDED"; // Default mode
    std::string palAnalysisDir;
    size_t numThreads = 0; // 0 = auto-detect
    bool enableChaining = true;
    bool enablePreCompute = true;
    std::string outputFormat = "binary";

    // Command-line argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            outputFile = argv[++i];
        } else if ((arg == "-m" || arg == "--mode") && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--pal-analysis" && i + 1 < argc) {
            palAnalysisDir = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            numThreads = std::stoul(argv[++i]);
        } else if (arg == "--no-chaining") {
            enableChaining = false;
        } else if (arg == "--no-precompute") {
            enablePreCompute = false;
        } else if (arg == "--format" && i + 1 < argc) {
            outputFormat = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Error: Unknown or invalid argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (outputFile.empty()) {
        std::cerr << "Error: Output file must be specified with -o or --output." << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    if (palAnalysisDir.empty()) {
        std::cerr << "Error: PAL analysis directory must be specified with --pal-analysis." << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Validate mode
    if (mode != "EXTENDED" && mode != "DEEP") {
        std::cerr << "Error: Invalid mode specified. Use 'EXTENDED' or 'DEEP'." << std::endl;
        return 1;
    }

    // Validate output format
    ExportFormat exportFormat = ExportFormat::Binary;
    if (outputFormat == "binary") {
        exportFormat = ExportFormat::Binary;
    } else if (outputFormat == "json") {
        exportFormat = ExportFormat::JSON;
    } else if (outputFormat == "csv") {
        exportFormat = ExportFormat::CSV;
    } else {
        std::cerr << "Error: Invalid output format. Use 'binary', 'json', or 'csv'." << std::endl;
        return 1;
    }

    try {
        std::cout << "=== Optimized Pattern Universe Generator ===" << std::endl;
        std::cout << "PAL Analysis Directory: " << palAnalysisDir << std::endl;
        std::cout << "Search Mode: " << mode << std::endl;
        std::cout << "Output File: " << outputFile << std::endl;
        std::cout << "Output Format: " << outputFormat << std::endl;
        std::cout << "Threads: " << (numThreads == 0 ? "auto-detect" : std::to_string(numThreads)) << std::endl;
        std::cout << "Chaining: " << (enableChaining ? "enabled" : "disabled") << std::endl;
        std::cout << "Pre-computation: " << (enablePreCompute ? "enabled" : "disabled") << std::endl;
        std::cout << std::endl;
        
        // Create optimized generator
        std::cout << "Loading PAL analysis data..." << std::endl;
        auto generator = OptimizedPatternUniverseGeneratorFactory::createFromAnalysisDir(palAnalysisDir);
        
        // Display loaded analysis information
        const auto& analysisData = generator->getPALAnalysisData();
        std::cout << "Loaded PAL Analysis:" << std::endl;
        std::cout << "- Total indices: " << analysisData.getIndexMappings().getTotalIndices() << std::endl;
        std::cout << "- Total patterns: " << analysisData.getIndexMappings().getTotalPatterns() << std::endl;
        std::cout << "- Analysis version: " << analysisData.getAnalysisVersion() << std::endl;
        
        // Display curated group information
        const auto& groupManager = generator->getCuratedGroupManager();
        std::cout << "- Total pattern count: " << groupManager.getTotalPatternCount() << std::endl;
        
        // Validate group integrity
        bool integrityValid = groupManager.validateGroupIntegrity();
        std::cout << "- Group integrity: " << (integrityValid ? "VALID" : "INVALID") << std::endl;
        
        if (!integrityValid) {
            std::cerr << "Warning: Group integrity validation failed. Results may be incomplete." << std::endl;
        }
        
        // Configure generation
        // Progress callback
        auto progressCallback = [](const GenerationProgress& progress) {
            std::cout << "Progress: " << progress.getCompletedGroups() << "/" << progress.getTotalGroups()
                      << " groups (" << std::fixed << std::setprecision(1) << progress.getPercentComplete()
                      << "%) - " << progress.getPatternsGenerated() << " patterns generated" << std::endl;
        };
        
        // Log callback
        auto logCallback = [](const std::string& message) {
            std::cout << "[LOG] " << message << std::endl;
        };

        OptimizedPatternUniverseGenerator::GenerationConfig config(
            (mode == "DEEP") ? SearchType::DEEP : SearchType::EXTENDED,
            true,  // enableParallelProcessing
            numThreads,  // maxThreads
            enableChaining,  // enableChaining
            enablePreCompute,  // enablePreComputation
            500,  // preComputationThreshold
            true,  // prioritizeHighYield
            24.0,  // targetSpeedupFactor
            false,  // enableDelayPatterns
            5,  // maxDelayBars
            15,  // maxLookbackWithDelay
            progressCallback,
            logCallback
        );
        
        // Get performance estimate
        std::cout << "\nPerformance Estimation:" << std::endl;
        auto estimate = generator->estimatePerformance(config);
        std::cout << "- Estimated time: " << std::fixed << std::setprecision(1) << estimate.getEstimatedTime().count() << " seconds" << std::endl;
        std::cout << "- Estimated patterns: " << estimate.getEstimatedPatterns() << std::endl;
        std::cout << "- Estimated speedup: " << std::fixed << std::setprecision(1) << estimate.getEstimatedSpeedup() << "x" << std::endl;
        std::cout << "- Recommended threads: " << estimate.getRecommendedThreads() << std::endl;
        std::cout << "- Estimated memory usage: " << estimate.getEstimatedMemoryUsageMB() << " MB" << std::endl;
        
        // Generate patterns
        std::cout << "\nStarting pattern generation..." << std::endl;
        auto startTime = std::chrono::high_resolution_clock::now();
        auto result = generator->generatePatternUniverse(config);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        // Export results
        std::cout << "\nExporting patterns to " << outputFile << "..." << std::endl;
        bool exportSuccess = generator->exportPatterns(result, outputFile, exportFormat);
        if (!exportSuccess) {
            std::cerr << "Error: Failed to export patterns to " << outputFile << std::endl;
            return 1;
        }
        
        // Display results
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        std::cout << "\n=== Generation Complete ===" << std::endl;
        std::cout << "Total patterns generated: " << result.getTotalPatternsGenerated() << std::endl;
        std::cout << "Generation time: " << std::fixed << std::setprecision(3) << (duration.count() / 1000.0) << " seconds" << std::endl;
        std::cout << "Speedup factor: " << std::fixed << std::setprecision(1) << result.getSpeedupFactor() << "x" << std::endl;
        std::cout << "Patterns per second: " << std::fixed << std::setprecision(0) << result.getPatternsPerSecond() << std::endl;
        
        // Note: validationSummary, palAccuracyPercentage, validationErrors, and qualityWarnings
        // are not part of the current PatternUniverseResult class definition
        // These would need to be added if validation functionality is implemented
        
        std::cout << "\nPattern universe generation completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError during universe generation: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}