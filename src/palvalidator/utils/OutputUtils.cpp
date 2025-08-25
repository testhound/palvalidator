#include "OutputUtils.h"
#include "TimeUtils.h"
#include "LogPalPattern.h"
#include "PalParseDriver.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace mkc_timeseries;
using namespace mkc_palast;

namespace palvalidator
{
namespace utils
{

TeeBuf::TeeBuf(std::streambuf* sb1, std::streambuf* sb2)
    : mStreamBuf1(sb1),
      mStreamBuf2(sb2)
{
}

int TeeBuf::overflow(int c)
{
    if (c == EOF)
    {
        return !EOF;
    }
    
    const int r1 = mStreamBuf1->sputc(static_cast<char>(c));
    const int r2 = mStreamBuf2->sputc(static_cast<char>(c));
    return (r1 == EOF || r2 == EOF) ? EOF : c;
}

int TeeBuf::sync()
{
    const int r1 = mStreamBuf1->pubsync();
    const int r2 = mStreamBuf2->pubsync();
    return (r1 == 0 && r2 == 0) ? 0 : -1;
}

TeeStream::TeeStream(std::ostream& streamA, std::ostream& streamB)
    : std::ostream(nullptr),
      mTeeBuf(streamA.rdbuf(), streamB.rdbuf())
{
    this->rdbuf(&mTeeBuf);
}

std::string createBootstrapFileName(const std::string& securitySymbol,
                                   ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_Bootstrap_Results_" + getCurrentTimestamp() + ".txt";
}

std::string createSurvivingPatternsFileName(const std::string& securitySymbol,
                                           ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

std::string createDetailedSurvivingPatternsFileName(const std::string& securitySymbol,
                                                   ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_Detailed_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                   ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_Detailed_RejectedPatterns_" + getCurrentTimestamp() + ".txt";
}

std::string createPermutationTestSurvivorsFileName(const std::string& securitySymbol,
                                                 ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_PermutationTestSurvivors_" + getCurrentTimestamp() + ".txt";
}

template<typename Num>
void writePermutationTestSurvivors(const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                                 const std::string& filename)
{
    std::ofstream survivorFile(filename);
    if (!survivorFile.is_open()) {
        throw std::runtime_error("Cannot open survivor file for writing: " + filename);
    }
    
    //survivorFile << "# Monte Carlo Permutation Test Survivors File" << std::endl;
    //survivorFile << "# Generated: " << getCurrentTimestamp() << std::endl;
    //survivorFile << "# Count: " << strategies.size() << std::endl;
    //survivorFile << "# Format: PAL Pattern Format" << std::endl;
    //survivorFile << "# Note: These are survivors of Monte Carlo permutation testing" << std::endl;
    //survivorFile << "#       (before bootstrap performance filtering)" << std::endl;
    survivorFile << std::endl;
    
    for (const auto& strategy : strategies) {
        LogPalPattern::LogPattern(strategy->getPalPattern(), survivorFile);
    }
    
    survivorFile.close();
}

template<typename Num>
std::vector<std::shared_ptr<PalStrategy<Num>>>
loadPermutationTestSurvivors(const std::string& filename,
                           std::shared_ptr<Security<Num>> security)
{
    std::vector<std::shared_ptr<PalStrategy<Num>>> strategies;
    
    try {
        // Use PalParseDriver to parse the Monte Carlo survivor file (same as palcodegen/main.cpp:248-258)
        PalParseDriver driver(filename);
        int parseResult = driver.Parse();
        
        if (parseResult != 0) {
            throw std::runtime_error("Failed to parse Monte Carlo survivor file: " + filename +
                                   " (parse result: " + std::to_string(parseResult) + ")");
        }
        
        // Get the parsed patterns
        auto system = driver.getPalStrategies();
        if (!system) {
            throw std::runtime_error("No Monte Carlo survivor patterns found in file: " + filename);
        }
        
        // Convert PriceActionLabPattern objects to PalStrategy objects
        // These represent Monte Carlo permutation test survivors
        
        // Process long patterns
        for (auto it = system->patternLongsBegin(); it != system->patternLongsEnd(); ++it) {
            // The iterator returns a pair<key, shared_ptr<PriceActionLabPattern>>
            auto pattern = it->second;
            std::string strategyName = "LoadedLongStrategy_" + std::to_string(it->first);
            
            // Use the global makePalStrategy function with security parameter
            auto strategy = makePalStrategy<Num>(strategyName, pattern, security);
            strategies.push_back(strategy);
        }
        
        // Process short patterns
        for (auto it = system->patternShortsBegin(); it != system->patternShortsEnd(); ++it) {
            // The iterator returns a pair<key, shared_ptr<PriceActionLabPattern>>
            auto pattern = it->second;
            std::string strategyName = "LoadedShortStrategy_" + std::to_string(it->first);
            
            // Use the global makePalStrategy function with security parameter
            auto strategy = makePalStrategy<Num>(strategyName, pattern, security);
            strategies.push_back(strategy);
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Error loading Monte Carlo survivor strategies from " + filename + ": " + e.what());
    }
    
    return strategies;
}

bool validateSurvivorFile(const std::string& filename)
{
    return std::filesystem::exists(filename) &&
           std::filesystem::is_regular_file(filename) &&
           std::filesystem::file_size(filename) > 0;
}

// Explicit template instantiations for common numeric types
template void writePermutationTestSurvivors<num::DefaultNumber>(
    const std::vector<std::shared_ptr<PalStrategy<num::DefaultNumber>>>& strategies,
    const std::string& filename);

template std::vector<std::shared_ptr<PalStrategy<num::DefaultNumber>>>
loadPermutationTestSurvivors<num::DefaultNumber>(
    const std::string& filename,
    std::shared_ptr<Security<num::DefaultNumber>> security);

} // namespace utils
} // namespace palvalidator