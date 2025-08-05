#include "UniverseGenerator.h" // This should now include the template implementation
#include "ParallelExecutors.h"
#include <iostream>
#include <string>
#include <stdexcept>
#include <map>

// Configuration struct to hold settings for a search mode
struct SearchModeConfig {
    uint8_t maxLookback;
    uint8_t maxConditions;
    std::string searchType; // Corresponds to the generation tasks to run
};

/**
 * @brief Prints usage instructions for the command-line tool.
 */
void printUsage()
{
    std::cout << "Usage: PatternUniverseGenerator --output <file_path> --mode <mode>" << std::endl;
    std::cout << "  --output: Path to the output Pattern Universe File." << std::endl;
    std::cout << "  --mode:   The generation mode. Can be 'EXTENDED' or 'DEEP'." << std::endl;
}

/**
 * @brief Main entry point for the Pattern Universe Generator tool.
 */
int main(int argc, char* argv[])
{
    // Define the hardcoded configurations for our search modes
    const std::map<std::string, SearchModeConfig> modeConfigs = {
        {"EXTENDED", {10, 5, "EXTENDED"}}, // Changed searchType to match mode
        {"DEEP",     {14, 8, "DEEP"}}      // Changed searchType to match mode
    };

    if (argc != 5) {
        printUsage();
        return 1;
    }

    std::string outputFile;
    std::string mode;

    try
    {
        for (int i = 1; i < argc; i += 2)
        {
            std::string arg = argv[i];
            std::string val = argv[i + 1];

            if (arg == "--output")
            {
                outputFile = val;
            }
            else if (arg == "--mode")
            {
                mode = val;
            }
            else
            {
                throw std::invalid_argument("Unknown argument: " + arg);
            }
        }

        if (outputFile.empty() || mode.empty())
        {
            throw std::invalid_argument("Both --output and --mode arguments are required.");
        }

        auto it = modeConfigs.find(mode);
        if (it == modeConfigs.end())
        {
            throw std::invalid_argument("Invalid mode '" + mode + "'. Must be 'EXTENDED' or 'DEEP'.");
        }
        const SearchModeConfig& config = it->second;

        std::cout << "Starting Universe Generation in '" << mode << "' mode..." << std::endl;
        std::cout << "  Output File: " << outputFile << std::endl;
        std::cout << "  Max Lookback: " << static_cast<int>(config.maxLookback) << std::endl;
        std::cout << "  Max Conditions: " << static_cast<int>(config.maxConditions) << std::endl;
        std::cout << "  Component Set: " << config.searchType << std::endl;

        // Instantiate the generator with the ThreadPoolExecutor policy
        UniverseGenerator<concurrency::ThreadPoolExecutor<>> generator(
            outputFile,
            config.maxLookback,
            config.maxConditions,
            config.searchType
        );
        generator.run();

        std::cout << "Universe Generation Completed Successfully." << std::endl;

    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}
