#pragma once

#include "PalSetupTypes.h"
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Manages creation and organization of output directory structures
 */
class DirectoryManager {
public:
    DirectoryManager();
    ~DirectoryManager() = default;
    
    /**
     * @brief Create complete directory structure for validation output
     */
    DirectoryPaths createDirectoryStructure(const SetupConfiguration& config);
    
    /**
     * @brief Generate timeframe-specific directory name
     */
    std::string createTimeFrameDirectoryName(const std::string& timeFrameStr, 
                                           int intradayMinutes = 0);

private:
    /**
     * @brief Create PAL subdirectories for parallel processing (8 subdirs)
     */
    std::vector<fs::path> createPalSubdirectories(const fs::path& palDir);
    
    /**
     * @brief Create risk-reward subdirectories within validation directory
     */
    std::vector<fs::path> createRiskRewardDirectories(const fs::path& valDir);
    
    /**
     * @brief Ensure directory exists, creating it if necessary
     */
    void ensureDirectoryExists(const fs::path& path);
};