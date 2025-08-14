#pragma once

#include "PatternTemplate.h"
#include "BinaryPatternTemplateSerializer.h"
#include "PatternUniverseSerializer.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"
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

// --- Helper Functions and Operators ---
static std::string componentTypeToString(PriceComponentType type) {
    switch (type) {
        case PriceComponentType::Open:  return "O";
        case PriceComponentType::High:  return "H";
        case PriceComponentType::Low:   return "L";
        case PriceComponentType::Close: return "C";
        case PriceComponentType::Volume: return "V";
        case PriceComponentType::Roc1: return "R";
        case PriceComponentType::Ibs1: return "I1";
        case PriceComponentType::Ibs2: return "I2";
        case PriceComponentType::Ibs3: return "I3";
        case PriceComponentType::Meander: return "M";
        case PriceComponentType::VChartLow: return "VL";
        case PriceComponentType::VChartHigh: return "VH";
    }
    return "?";
}
static std::string pcdToString(const PriceComponentDescriptor& pcd) {
    std::stringstream ss;
    ss << componentTypeToString(pcd.getComponentType())
       << "[" << static_cast<int>(pcd.getBarOffset()) << "]";
    return ss.str();
}

// --- Hashing Infrastructure ---
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
 * @brief Orchestrates the parallel generation of the pattern universe using a memory-efficient, group-based streaming architecture.
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

private:
    struct GenerationTask;

    void generateAndStreamPatterns(const GenerationTask& task, std::ofstream& outStream);
    void generateBarCombinationsRecursive(size_t, size_t, const std::vector<uint8_t>&, std::vector<uint8_t>&, std::vector<std::vector<uint8_t>>&) const;
    bool isValidCombination(const std::vector<PatternCondition>&) const;
    std::string generatePatternString(const std::vector<PriceComponentDescriptor>& permutation, uint8_t delay) const;
    PatternTemplate parsePatternFromString(const std::string& line) const;

    std::string m_outputFile;
    uint8_t m_maxLookback;
    uint8_t m_maxConditions;
    uint8_t m_maxSpread;
    std::string m_searchType;

    BinaryPatternTemplateSerializer m_patternSerializer;
    std::map<int, std::vector<std::vector<uint8_t>>> m_curatedGroups;

    Executor m_executor;
    mutable std::mutex m_writeMutex;
};

// =================================================================================
// IMPLEMENTATION
// =================================================================================

template <typename Executor>
struct UniverseGenerator<Executor>::GenerationTask {
    std::string name;
    std::vector<PriceComponentType> componentTypes;
    uint8_t minPatternBars;
    uint8_t maxPatternBars;
};

template <typename Executor>
UniverseGenerator<Executor>::UniverseGenerator(
    const std::string& outputFile, uint8_t maxLookback, uint8_t maxConditions, uint8_t maxSpread, const std::string& searchType
) : m_outputFile(outputFile), m_maxLookback(maxLookback), m_maxConditions(maxConditions), m_maxSpread(maxSpread), m_searchType(searchType), m_executor()
{
    if (outputFile.empty()) throw std::invalid_argument("Output file path cannot be empty.");
    if (maxLookback == 0 || maxConditions == 0) throw std::invalid_argument("Max lookback and max conditions must be greater than zero.");

    m_curatedGroups[2] = { {0, 1}, {0, 2} };
    m_curatedGroups[3] = { {0, 1, 2}, {0, 1, 3}, {1, 2, 3}, {0, 2, 4}, {0, 3, 6}, {1, 3, 5}, {2, 4, 6} };
    m_curatedGroups[4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {0, 2, 4, 5}, {2, 4, 6, 8}, {0, 2, 3, 5}, {1, 3, 5, 7} };
    m_curatedGroups[5] = { {0, 1, 2, 3, 4}, {1, 2, 3, 4, 5}, {0, 1, 2, 4, 5}, {0, 1, 3, 4, 5}, {0, 1, 2, 3, 5}, {0, 1, 2, 4, 6}, {0, 2, 4, 5, 6}, {0, 2, 4, 6, 7}, {0, 2, 4, 6, 8}, {1, 3, 5, 6, 7}, {1, 3, 5, 7, 8}, {1, 3, 5, 7, 9}, {2, 4, 6, 8, 9}, {2, 4, 6, 8, 10} };
    m_curatedGroups[6] = { {0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 6}, {0, 1, 2, 3, 5, 6}, {0, 1, 2, 4, 5, 6}, {0, 1, 3, 4, 5, 6}, {0, 2, 3, 4, 5, 6}, {1, 2, 3, 4, 5, 6}, {0, 1, 2, 6, 7, 8}, {0, 1, 3, 6, 7, 9}, {0, 2, 4, 6, 8, 10}, {1, 3, 5, 7, 9, 11}, {2, 4, 6, 8, 10, 12} };
    m_curatedGroups[7] = { {0, 1, 2, 3, 4, 5, 6}, {0, 1, 2, 3, 4, 5, 7}, {0, 1, 2, 3, 4, 6, 7}, {0, 1, 2, 3, 5, 6, 7}, {0, 1, 2, 4, 5, 6, 7}, {0, 1, 3, 4, 5, 6, 7}, {0, 2, 3, 4, 5, 6, 7}, {1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 7, 8, 9}, {0, 1, 2, 4, 7, 8, 10}, {0, 1, 3, 6, 7, 9, 10}, {0, 2, 4, 6, 8, 10, 12}, {1, 3, 5, 7, 9, 11, 13} };
    m_curatedGroups[8] = { {0, 1, 2, 3, 4, 5, 6, 7} };
    m_curatedGroups[9] = { {0, 1, 2, 3, 4, 5, 6, 7, 8} };
}

template <typename Executor>
void UniverseGenerator<Executor>::run()
{
    const std::string rawFile = m_outputFile + ".raw.tmp";
    std::cout << "\n--- Phase 1: Generating raw patterns to " << rawFile << " ---" << std::endl;
    
    std::ofstream rawStream(rawFile);
    if (!rawStream) throw std::runtime_error("Failed to open temporary raw file for writing.");

    std::vector<GenerationTask> tasks;
    if (m_searchType == "DEEP") {
        tasks = { {"Close", {PriceComponentType::Close}, 3, 9}, {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 5}, {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 5}, {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 4} };
    } else if (m_searchType == "EXTENDED") {
        tasks = {{"Close", {PriceComponentType::Close}, 2, 6}, {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 4}, {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 3}, {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 3}};
    } else {
        throw std::runtime_error("Unsupported search type: " + m_searchType);
    }

    for (const auto& task : tasks) {
        std::cout << "\n--- Starting Generation Task: " << task.name << " ---" << std::endl;
        generateAndStreamPatterns(task, rawStream);
    }
    rawStream.close();
    std::cout << "\n--- Phase 1 Complete. ---" << std::endl;

    const std::string uniqueFile = m_outputFile + ".unique.tmp";
    std::cout << "\n--- Phase 2: De-duplicating patterns using external sort... ---" << std::endl;
    
    std::string command = "sort -u " + rawFile + " -o " + uniqueFile;
    int result = std::system(command.c_str());
    if (result != 0) throw std::runtime_error("External sort command failed. Please ensure 'sort' is installed.");
    
    std::cout << "  - De-duplication complete." << std::endl;

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

    std::cout << "\n--- Phase 4: Cleaning up temporary files... ---" << std::endl;
    std::remove(rawFile.c_str());
    std::remove(uniqueFile.c_str());
    std::cout << "  - Cleanup complete." << std::endl;

    std::cout << "\nUniverse Generation Completed Successfully." << std::endl;
}

// MODIFIED: Implements the final "paired-component" and "subset-of-group" heuristic.
template <typename Executor>
void UniverseGenerator<Executor>::generateAndStreamPatterns(const GenerationTask& task, std::ofstream& outStream)
{
    uint8_t maxSearchDepth = m_searchType == "DEEP" ? 9 : 6;

    for (uint8_t numBarsInGroup = 2; numBarsInGroup <= maxSearchDepth; ++numBarsInGroup) {
        auto it = m_curatedGroups.find(numBarsInGroup);
        if (it == m_curatedGroups.end() || it->second.empty()) continue;
        
        const auto& barCombinations = it->second;
        std::cout << "  - Searching patterns with " << static_cast<int>(numBarsInGroup) << " unique bars using " << barCombinations.size() << " pre-defined groups..." << std::endl;
        
        concurrency::parallel_for_each(m_executor, barCombinations, [&](const auto& barCombo) {
            std::unordered_set<unsigned long long> seenHashesInThread;

            for (uint8_t k = task.minPatternBars; k <= task.maxPatternBars && k <= numBarsInGroup; ++k) {
                std::vector<std::vector<uint8_t>> subBarCombos;
                std::vector<uint8_t> currentSubCombo;
                generateBarCombinationsRecursive(0, k, barCombo, currentSubCombo, subBarCombos);

                for (const auto& subBarCombo : subBarCombos) {
                    uint8_t maxOffsetInGroup = 0;
                    for(uint8_t offset : subBarCombo) maxOffsetInGroup = std::max(maxOffsetInGroup, offset);
                    if (maxOffsetInGroup > m_maxLookback) continue;

                    std::vector<PriceComponentDescriptor> componentPool;
                    for (uint8_t barOffset : subBarCombo) {
                        for (const auto& type : task.componentTypes) componentPool.emplace_back(type, barOffset);
                    }
                    
                    if (componentPool.size() > static_cast<size_t>(m_maxConditions + 1)) continue;

                    std::sort(componentPool.begin(), componentPool.end());
                    do {
                        std::vector<PatternCondition> conditions;
                        for (size_t j = 0; j < componentPool.size() - 1; ++j) {
                            conditions.emplace_back(componentPool[j], ComparisonOperator::GreaterThan, componentPool[j+1]);
                        }

                        if (isValidCombination(conditions)) {
                            PatternTemplate newTemplate("temp");
                            for(const auto& cond : conditions) newTemplate.addCondition(cond);
                            auto hash = std::hash<PatternTemplate>{}(newTemplate);
                            
                            if(seenHashesInThread.find(hash) == seenHashesInThread.end()) {
                                seenHashesInThread.insert(hash);
                                
                                std::lock_guard<std::mutex> lock(m_writeMutex);
                                outStream << generatePatternString(componentPool, 0) << "\n";

                                for (uint8_t delay = 1; delay <= 5; ++delay) {
                                    if (maxOffsetInGroup + delay > m_maxLookback) continue;
                                    
                                    std::vector<PriceComponentDescriptor> delayedPcds;
                                    for(const auto& pcd : componentPool) delayedPcds.emplace_back(pcd.getComponentType(), pcd.getBarOffset() + delay);
                                    outStream << generatePatternString(delayedPcds, delay) << "\n";
                                }
                            }
                        }
                    } while (std::next_permutation(componentPool.begin(), componentPool.end()));
                }
            }
        });
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
std::string UniverseGenerator<Executor>::generatePatternString(const std::vector<PriceComponentDescriptor>& sequence, uint8_t delay) const {
    std::stringstream ss;
    for (size_t i = 0; i < sequence.size(); ++i) {
        ss << pcdToString(sequence[i]);
        if (i < sequence.size() - 1) {
            ss << " > ";
        }
    }
     if (delay > 0) {
        ss << " [Delay: " << static_cast<int>(delay) << "]";
    }
    return ss.str();
}

template <typename Executor>
void UniverseGenerator<Executor>::generateBarCombinationsRecursive(size_t offset, size_t k, const std::vector<uint8_t>& items, std::vector<uint8_t>& currentCombination, std::vector<std::vector<uint8_t>>& results) const {
    if (k == 0) { results.push_back(currentCombination); return; }
    for (size_t i = offset; i <= items.size() - k; ++i) {
        currentCombination.push_back(items[i]);
        generateBarCombinationsRecursive(i + 1, k - 1, items, currentCombination, results);
        currentCombination.pop_back();
    }
}

template <typename Executor>
bool UniverseGenerator<Executor>::isValidCombination(const std::vector<PatternCondition>& conditions) const {
    // 1. Original Structural Check: Ensure it's a simple, non-branching chain.
    std::set<PriceComponentDescriptor> components;
    for (const auto& cond : conditions) {
        components.insert(cond.getLhs());
        components.insert(cond.getRhs());
    }
    if (components.size() != conditions.size() + 1) {
        return false;
    }

    // 2. New Semantic Check: Prevent tautologies and contradictions.
    std::vector<PriceComponentDescriptor> sequence;
    sequence.push_back(conditions.front().getLhs());
    for(const auto& cond : conditions) {
        sequence.push_back(cond.getRhs());
    }

    std::unordered_set<uint8_t> seenHighs;
    std::unordered_set<uint8_t> seenLows;

    for (const auto& pcd : sequence) {
        uint8_t bar = pcd.getBarOffset();
        PriceComponentType type = pcd.getComponentType();

        if (type == PriceComponentType::High) {
            // CONTRADICTION CHECK: If we see a High[x] after already seeing Low[x],
            // it implies L[x] > H[x], which is impossible.
            if (seenLows.count(bar)) {
                return false; 
            }
            seenHighs.insert(bar);
        }
        else if (type == PriceComponentType::Low) {
            // TAUTOLOGY CHECK: If we see a Low[x] after already seeing High[x],
            // it implies H[x] > L[x], which is always true and provides no value.
            if (seenHighs.count(bar)) {
                return false;
            }
            seenLows.insert(bar);
        }
    }

    // If we've passed all checks, the combination is valid.
    return true;
}