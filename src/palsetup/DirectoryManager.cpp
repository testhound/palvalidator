#include "DirectoryManager.h"
#include <iostream>

DirectoryManager::DirectoryManager() = default;

DirectoryPaths DirectoryManager::createDirectoryStructure(const SetupConfiguration& config) {
    // Create base directory structure
    fs::path baseDir = config.getTickerSymbol() + "_Validation";
    
    // Create timeframe-specific subdirectory
    std::string timeFrameDirName = createTimeFrameDirectoryName(config.getTimeFrameStr(), config.getIntradayMinutes());
    fs::path timeFrameDir = baseDir / timeFrameDirName;
    
    // Create Roc<holdingPeriod> subdirectory
    fs::path rocDir = timeFrameDir / ("Roc" + std::to_string(config.getHoldingPeriod()));
    fs::path palDir = rocDir / "PAL_Files";
    fs::path valDir = rocDir / "Validation_Files";
    
    // Ensure all directories exist
    ensureDirectoryExists(palDir);
    ensureDirectoryExists(valDir);
    
    // Create risk-reward subdirectories
    auto riskRewardDirs = createRiskRewardDirectories(valDir);
    
    // Create PAL subdirectories for parallel processing
    auto palSubDirs = createPalSubdirectories(palDir);
    
    return DirectoryPaths(
        baseDir,
        timeFrameDir,
        rocDir,
        palDir,
        valDir,
        riskRewardDirs,
        palSubDirs
    );
}

std::string DirectoryManager::createTimeFrameDirectoryName(const std::string& timeFrameStr, int intradayMinutes) {
    if (timeFrameStr == "Intraday") {
        return "Intraday_" + std::to_string(intradayMinutes);
    }
    return timeFrameStr;
}

std::vector<fs::path> DirectoryManager::createPalSubdirectories(const fs::path& palDir) {
    std::vector<fs::path> palSubDirs;
    
    // Create 8 subdirectories under palDir for parallel processing
    for (int i = 1; i <= 8; ++i) {
        fs::path subDir = palDir / ("pal_" + std::to_string(i));
        ensureDirectoryExists(subDir);
        palSubDirs.push_back(subDir);
    }
    
    return palSubDirs;
}

std::vector<fs::path> DirectoryManager::createRiskRewardDirectories(const fs::path& valDir) {
    std::vector<fs::path> riskRewardDirs;
    
    // Create risk-reward subdirectories within validation directory
    fs::path riskReward05Dir = valDir / "Risk_Reward_0_5";
    fs::path riskReward11Dir = valDir / "Risk_Reward_1_1";
    fs::path riskReward21Dir = valDir / "Risk_Reward_2_1";
    
    ensureDirectoryExists(riskReward05Dir);
    ensureDirectoryExists(riskReward11Dir);
    ensureDirectoryExists(riskReward21Dir);
    
    riskRewardDirs.push_back(riskReward05Dir);
    riskRewardDirs.push_back(riskReward11Dir);
    riskRewardDirs.push_back(riskReward21Dir);
    
    return riskRewardDirs;
}

void DirectoryManager::ensureDirectoryExists(const fs::path& path) {
    try {
        fs::create_directories(path);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directory " << path << ": " << e.what() << std::endl;
        throw;
    }
}