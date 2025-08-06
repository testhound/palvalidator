#pragma once

#include "PatternTemplate.h"
#include "BinaryPatternTemplateSerializer.h"
#include "PatternUniverseSerializer.h"
#include "ParallelExecutors.h"
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>
#include <fstream>
#include <cstdlib>
#include <cstdio>

// --- Helper Functions and Operators (Unchanged) ---
static std::string componentTypeToString(PriceComponentType type) {
    switch (type) {
        case PriceComponentType::Open:  return "O";
        case PriceComponentType::High:  return "H";
        case PriceComponentType::Low:   return "L";
        case PriceComponentType::Close: return "C";
    }
    return "?";
}
static std::string pcdToString(const PriceComponentDescriptor& pcd) {
    std::stringstream ss;
    ss << componentTypeToString(pcd.getComponentType())
       << "[" << static_cast<int>(pcd.getBarOffset()) << "]";
    return ss.str();
}
inline bool operator<(const PriceComponentDescriptor& lhs, const PriceComponentDescriptor& rhs) {
    if (lhs.getBarOffset() < rhs.getBarOffset()) return true;
    if (lhs.getBarOffset() > rhs.getBarOffset()) return false;
    return lhs.getComponentType() < rhs.getComponentType();
}

// --- Hashing Infrastructure (Unchanged) ---
inline void hash_combine(unsigned long long &seed, unsigned long long value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}
namespace std {
    template <> struct hash<PriceComponentDescriptor> {
        size_t operator()(const PriceComponentDescriptor& pcd) const {
            unsigned long long seed = 0;
            hash_combine(seed, static_cast<unsigned long long>(pcd.getComponentType()));
            hash_combine(seed, static_cast<unsigned long long>(pcd.getBarOffset()));
            return seed;
        }
    };
    template <> struct hash<PatternCondition> {
        size_t operator()(const PatternCondition& cond) const {
            unsigned long long seed = 0;
            auto h1 = std::hash<PriceComponentDescriptor>{}(cond.getLhs());
            auto h2 = std::hash<PriceComponentDescriptor>{}(cond.getRhs());
            hash_combine(seed, std::min(h1, h2));
            hash_combine(seed, std::max(h1, h2));
            hash_combine(seed, static_cast<unsigned long long>(cond.getOperator()));
            return seed;
        }
    };
    template <> struct hash<PatternTemplate> {
        size_t operator()(const PatternTemplate& pt) const {
            if (pt.getConditions().empty()) return 0;
            std::vector<unsigned long long> conditionHashes;
            conditionHashes.reserve(pt.getConditions().size());
            for (const auto& cond : pt.getConditions()) {
                conditionHashes.push_back(std::hash<PatternCondition>{}(cond));
            }
            std::sort(conditionHashes.begin(), conditionHashes.end());
            unsigned long long seed = 0;
            for (const auto& h : conditionHashes) {
                hash_combine(seed, h);
            }
            return seed;
        }
    };
}


/**
 * @class UniverseGenerator
 * @brief Orchestrates the generation of the pattern universe using a memory-efficient streaming architecture.
 */
template <typename Executor = concurrency::ThreadPoolExecutor<>>
class UniverseGenerator
{
public:
    UniverseGenerator(
        const std::string& outputFile,
        uint8_t maxLookback,
        uint8_t maxConditions,
        uint8_t maxSpread,
        const std::string& searchType
    );

    void run();

    PatternTemplate testParsePatternFromString(const std::string& line) const {
        return parsePatternFromString(line);
    }

private:
    struct GenerationTask;

    void generateAndStreamPatterns(const GenerationTask& task, std::ofstream& outStream);
    void generateBarCombinationsRecursive(size_t, size_t, const std::vector<uint8_t>&, std::vector<uint8_t>&, std::vector<std::vector<uint8_t>>&) const;
    void generateComponentCombinationsRecursive(size_t, size_t, const std::vector<PriceComponentDescriptor>&, std::vector<PriceComponentDescriptor>&, std::vector<std::vector<PriceComponentDescriptor>>&) const;
    bool isValidCombination(const std::vector<PatternCondition>&) const;
    std::string generatePatternString(const std::vector<PriceComponentDescriptor>& permutation) const;
    PatternTemplate parsePatternFromString(const std::string& line) const;

    std::string m_outputFile;
    uint8_t m_maxLookback;
    uint8_t m_maxConditions;
    uint8_t m_maxSpread;
    std::string m_searchType;
    mutable std::mutex m_writeMutex;
    BinaryPatternTemplateSerializer m_patternSerializer;
};

// =================================================================================
// IMPLEMENTATION
// =================================================================================

template <typename Executor>
struct UniverseGenerator<Executor>::GenerationTask {
    std::string name;
    std::vector<PriceComponentType> componentTypes;
    uint8_t minPatternLength;
    uint8_t maxPatternLength;
};

template <typename Executor>
UniverseGenerator<Executor>::UniverseGenerator(
    const std::string& outputFile, uint8_t maxLookback, uint8_t maxConditions, uint8_t maxSpread, const std::string& searchType
) : m_outputFile(outputFile), m_maxLookback(maxLookback), m_maxConditions(maxConditions), m_maxSpread(maxSpread), m_searchType(searchType)
{
    if (outputFile.empty()) throw std::invalid_argument("Output file path cannot be empty.");
    if (maxLookback == 0 || maxConditions == 0) throw std::invalid_argument("Max lookback and max conditions must be greater than zero.");
}

template <typename Executor>
void UniverseGenerator<Executor>::run()
{
    // === PHASE 1: GENERATE RAW PATTERNS TO TEXT FILE ===
    const std::string rawFile = m_outputFile + ".raw.tmp";
    std::cout << "\n--- Phase 1: Generating raw patterns to " << rawFile << " ---" << std::endl;
    
    std::vector<GenerationTask> tasks;
    // --- MODIFIED: Restored proper search type validation ---
    if (m_searchType == "DEEP") {
        tasks = {{"Close", {PriceComponentType::Close}, 3, 9}, {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 9}, {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 9}, {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 9}};
    } else if (m_searchType == "EXTENDED") {
        tasks = {{"Close", {PriceComponentType::Close}, 2, 6}, {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 6}, {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 6}, {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 6}};
    } else {
        // This was the missing block that caused the test to fail.
        throw std::runtime_error("Unsupported search type: " + m_searchType);
    }

    std::ofstream rawStream(rawFile);
    if (!rawStream) throw std::runtime_error("Failed to open temporary raw file for writing.");

    for (const auto& task : tasks) {
        std::cout << "\n--- Starting Generation Task: " << task.name << " ---" << std::endl;
        generateAndStreamPatterns(task, rawStream);
    }
    rawStream.close();
    std::cout << "\n--- Phase 1 Complete. ---" << std::endl;

    // === PHASE 2: DE-DUPLICATE VIA EXTERNAL SORT ===
    const std::string uniqueFile = m_outputFile + ".unique.tmp";
    std::cout << "\n--- Phase 2: De-duplicating patterns using external sort... ---" << std::endl;
    
    std::string command = "sort -u " + rawFile + " -o " + uniqueFile;
    int result = std::system(command.c_str());
    if (result != 0) throw std::runtime_error("External sort command failed. Please ensure 'sort' is installed.");
    
    std::cout << "  - De-duplication complete." << std::endl;

    // === PHASE 3: SERIALIZE UNIQUE PATTERNS TO BINARY FILE ===
    std::cout << "\n--- Phase 3: Serializing unique patterns to " << m_outputFile << " ---" << std::endl;
    
    std::ifstream uniqueStream(uniqueFile);
    if (!uniqueStream) throw std::runtime_error("Failed to open temporary unique file for reading.");
    
    std::ofstream binaryOutStream(m_outputFile, std::ios::binary);
    if (!binaryOutStream) throw std::runtime_error("Failed to open final binary file for writing.");
    
    FileHeader header;
    binaryOutStream.write(reinterpret_cast<const char*>(&header), sizeof(header));

    std::string line;
    uint32_t patternCount = 0;
    while (std::getline(uniqueStream, line)) {
        if (line.empty()) continue;
        PatternTemplate tpl = parsePatternFromString(line);
        m_patternSerializer.serialize(binaryOutStream, tpl);
        patternCount++;
    }

    header.patternCount = patternCount;
    binaryOutStream.seekp(0);
    binaryOutStream.write(reinterpret_cast<const char*>(&header), sizeof(header));

    uniqueStream.close();
    binaryOutStream.close();
    std::cout << "  - Serialized " << patternCount << " unique patterns." << std::endl;

    // === PHASE 4: CLEANUP ===
    std::cout << "\n--- Phase 4: Cleaning up temporary files... ---" << std::endl;
    std::remove(rawFile.c_str());
    std::remove(uniqueFile.c_str());
    std::cout << "  - Cleanup complete." << std::endl;

    std::cout << "\nUniverse Generation Completed Successfully." << std::endl;
}

template <typename Executor>
void UniverseGenerator<Executor>::generateAndStreamPatterns(const GenerationTask& task, std::ofstream& outStream)
{
    std::unordered_set<unsigned long long> seenHashesInTask;
    std::vector<uint8_t> allBarOffsets;
    for (uint8_t i = 0; i <= m_maxLookback; ++i) allBarOffsets.push_back(i);

    const uint8_t minUniqueBars = 2;
    const uint8_t maxUniqueBars = 8; 

    for (uint8_t numBars = minUniqueBars; numBars <= maxUniqueBars; ++numBars) {
        if (numBars > allBarOffsets.size()) break;

        std::cout << "  - Searching patterns with " << static_cast<int>(numBars) << " unique bars..." << std::endl;
        
        std::vector<std::vector<uint8_t>> barCombinations;
        std::vector<uint8_t> currentBarCombination;
        generateBarCombinationsRecursive(0, numBars, allBarOffsets, currentBarCombination, barCombinations);
        
        std::vector<std::vector<uint8_t>> filteredBarCombinations;
        for (const auto& combo : barCombinations) {
            if (combo.empty()) continue;
            auto minmax = std::minmax_element(combo.begin(), combo.end());
            if ((*minmax.second - *minmax.first) <= m_maxSpread) filteredBarCombinations.push_back(combo);
        }
        
        std::cout << "    - Processing " << filteredBarCombinations.size() << " valid bar combinations." << std::endl;

        for(const auto& barCombo : filteredBarCombinations) {
            std::vector<PriceComponentDescriptor> componentPool;
            for (uint8_t barOffset : barCombo) {
                for (const auto& type : task.componentTypes) componentPool.emplace_back(type, barOffset);
            }

            for (uint8_t k = task.minPatternLength; k <= m_maxConditions && k <= task.maxPatternLength; ++k) {
                uint8_t numComponentsInPattern = k + 1;
                if (numComponentsInPattern > componentPool.size()) continue;

                std::vector<std::vector<PriceComponentDescriptor>> componentCombinations;
                std::vector<PriceComponentDescriptor> currentComponentCombination;
                generateComponentCombinationsRecursive(0, numComponentsInPattern, componentPool, currentComponentCombination, componentCombinations);

                for (auto& pcdCombo : componentCombinations) {
                    std::sort(pcdCombo.begin(), pcdCombo.end());
                    do {
                        std::vector<PatternCondition> conditions;
                        for (uint8_t j = 0; j < k; ++j) {
                            conditions.emplace_back(pcdCombo[j], ComparisonOperator::GreaterThan, pcdCombo[j+1]);
                        }

                        if (isValidCombination(conditions)) {
                            PatternTemplate newTemplate("temp");
                            for(const auto& cond : conditions) newTemplate.addCondition(cond);
                            auto hash = std::hash<PatternTemplate>{}(newTemplate);
                            
                            if(seenHashesInTask.find(hash) == seenHashesInTask.end()) {
                                seenHashesInTask.insert(hash);
                                
                                std::lock_guard<std::mutex> lock(m_writeMutex);
                                outStream << generatePatternString(pcdCombo) << "\n";

                                uint8_t maxOffsetInBase = 0;
                                for(const auto& pcd : pcdCombo) maxOffsetInBase = std::max(maxOffsetInBase, pcd.getBarOffset());

                                for (uint8_t delay = 1; delay <= 5; ++delay) {
                                    if (maxOffsetInBase + delay > m_maxLookback) continue;
                                    
                                    std::vector<PriceComponentDescriptor> delayedPcds;
                                    for(const auto& pcd : pcdCombo) delayedPcds.emplace_back(pcd.getComponentType(), pcd.getBarOffset() + delay);
                                    outStream << generatePatternString(delayedPcds) << " [Delay: " << static_cast<int>(delay) << "]\n";
                                }
                            }
                        }
                    } while (std::next_permutation(pcdCombo.begin(), pcdCombo.end()));
                }
            }
        }
    }
}

template<typename Executor>
PatternTemplate UniverseGenerator<Executor>::parsePatternFromString(const std::string& line) const {
    std::string patternPart = line;
    
    size_t delayPos = line.find(" [Delay:");
    if (delayPos != std::string::npos) {
        patternPart = line.substr(0, delayPos);
    }
    
    PatternTemplate tpl(line);
    std::stringstream ss(patternPart);
    std::string segment;
    std::vector<PriceComponentDescriptor> pcds;

    auto parsePcd = [](const std::string& pcdStr) -> PriceComponentDescriptor {
        PriceComponentType type;
        switch (pcdStr[0]) {
            case 'O': type = PriceComponentType::Open; break;
            case 'H': type = PriceComponentType::High; break;
            case 'L': type = PriceComponentType::Low; break;
            default:  type = PriceComponentType::Close; break;
        }
        size_t openBracket = pcdStr.find('[');
        size_t closeBracket = pcdStr.find(']');
        uint8_t offset = static_cast<uint8_t>(std::stoi(pcdStr.substr(openBracket + 1, closeBracket - openBracket - 1)));
        return PriceComponentDescriptor(type, offset);
    };

    while(std::getline(ss, segment, '>')) {
        segment.erase(0, segment.find_first_not_of(" \t\n\r"));
        segment.erase(segment.find_last_not_of(" \t\n\r") + 1);
        pcds.push_back(parsePcd(segment));
    }

    if (pcds.size() >= 2) {
        for (size_t i = 0; i < pcds.size() - 1; ++i) {
            tpl.addCondition(PatternCondition(pcds[i], ComparisonOperator::GreaterThan, pcds[i+1]));
        }
    }
    return tpl;
}

template<typename Executor>
std::string UniverseGenerator<Executor>::generatePatternString(const std::vector<PriceComponentDescriptor>& permutation) const {
    std::stringstream ss;
    for (size_t i = 0; i < permutation.size(); ++i) {
        ss << pcdToString(permutation[i]);
        if (i < permutation.size() - 1) {
            ss << " > ";
        }
    }
    return ss.str();
}

template <typename Executor>
void UniverseGenerator<Executor>::generateBarCombinationsRecursive(size_t offset, size_t k, const std::vector<uint8_t>& allOffsets, std::vector<uint8_t>& currentCombination, std::vector<std::vector<uint8_t>>& results) const {
    if (k == 0) { results.push_back(currentCombination); return; }
    for (size_t i = offset; i <= allOffsets.size() - k; ++i) {
        currentCombination.push_back(allOffsets[i]);
        generateBarCombinationsRecursive(i + 1, k - 1, allOffsets, currentCombination, results);
        currentCombination.pop_back();
    }
}
template <typename Executor>
void UniverseGenerator<Executor>::generateComponentCombinationsRecursive(size_t offset, size_t k, const std::vector<PriceComponentDescriptor>& components, std::vector<PriceComponentDescriptor>& currentCombination, std::vector<std::vector<PriceComponentDescriptor>>& results) const {
    if (k == 0) { results.push_back(currentCombination); return; }
    for (size_t i = offset; i <= components.size() - k; ++i) {
        currentCombination.push_back(components[i]);
        generateComponentCombinationsRecursive(i + 1, k - 1, components, currentCombination, results);
        currentCombination.pop_back();
    }
}
template <typename Executor>
bool UniverseGenerator<Executor>::isValidCombination(const std::vector<PatternCondition>& conditions) const {
    std::set<PriceComponentDescriptor> components;
    for (const auto& cond : conditions) {
        components.insert(cond.getLhs());
        components.insert(cond.getRhs());
    }
    return components.size() == conditions.size() + 1;
}