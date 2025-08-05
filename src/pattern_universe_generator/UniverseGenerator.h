#pragma once

#include "PatternTemplate.h"
#include "PatternCondition.h"
#include "PriceComponentDescriptor.h"
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

// --- Hashing Infrastructure ---

/**
 * @brief Combines a new value into an existing hash seed.
 * Inspired by Boost's hash_combine.
 * @param seed The current hash value (will be modified).
 * @param value The new value to incorporate into the hash.
 */
inline void hash_combine(unsigned long long &seed, unsigned long long value)
{
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

// Add hashCode() to PriceComponentDescriptor
namespace std {
    template <>
    struct hash<PriceComponentDescriptor> {
        size_t operator()(const PriceComponentDescriptor& pcd) const {
            unsigned long long seed = 0;
            hash_combine(seed, static_cast<unsigned long long>(pcd.getComponentType()));
            hash_combine(seed, static_cast<unsigned long long>(pcd.getBarOffset()));
            return seed;
        }
    };
}

// Add hashCode() to PatternCondition
namespace std {
    template <>
    struct hash<PatternCondition> {
        size_t operator()(const PatternCondition& cond) const {
            unsigned long long seed = 0;
            // Use canonical hashing for the pair to ensure A>B is consistent
            auto h1 = std::hash<PriceComponentDescriptor>{}(cond.getLhs());
            auto h2 = std::hash<PriceComponentDescriptor>{}(cond.getRhs());
            
            hash_combine(seed, std::min(h1, h2));
            hash_combine(seed, std::max(h1, h2));
            hash_combine(seed, static_cast<unsigned long long>(cond.getOperator()));
            return seed;
        }
    };
}

// Add hashCode() to PatternTemplate for canonical representation
namespace std {
    template <>
    struct hash<PatternTemplate> {
        size_t operator()(const PatternTemplate& pt) const {
            if (pt.getConditions().empty()) {
                return 0;
            }

            // To create a canonical hash, we hash the individual conditions,
            // sort them, and then combine them.
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


// Helper operator for comparing PriceComponentDescriptors, needed for validation.
inline bool operator==(const PriceComponentDescriptor& lhs, const PriceComponentDescriptor& rhs) {
    return lhs.getComponentType() == rhs.getComponentType() && lhs.getBarOffset() == rhs.getBarOffset();
}

// Helper for storing component pairs in a set for validation
inline bool operator<(const PriceComponentDescriptor& lhs, const PriceComponentDescriptor& rhs) {
    if (lhs.getBarOffset() < rhs.getBarOffset()) return true;
    if (lhs.getBarOffset() > rhs.getBarOffset()) return false;
    return lhs.getComponentType() < rhs.getComponentType();
}


/**
 * @class UniverseGenerator
 * @brief Orchestrates the parallel generation of the pattern universe.
 */
template <typename Executor = concurrency::ThreadPoolExecutor<>>
class UniverseGenerator
{
public:
    // MODIFIED: Constructor now accepts maxSpread
    UniverseGenerator(
        const std::string& outputFile,
        uint8_t maxLookback,
        uint8_t maxConditions,
        uint8_t maxSpread,
        const std::string& searchType
    );

    void run();

    // Public methods for testing - these provide access to internal functionality for unit tests
    std::vector<PriceComponentDescriptor> testGenerateComponentPool(
        const std::vector<PriceComponentType>& typesToUse,
        uint8_t minOffset,
        uint8_t maxOffset) const
    {
        return generateComponentPool(typesToUse, minOffset, maxOffset);
    }

    std::vector<PatternCondition> testGenerateConditionPool(
        const std::vector<PriceComponentDescriptor>& componentPool,
        bool isMixed) const
    {
        return generateConditionPool(componentPool, isMixed);
    }

    PatternTemplate testCreateDelayedTemplate(const PatternTemplate& baseTemplate, uint8_t delay) const
    {
        return createDelayedTemplate(baseTemplate, delay);
    }

    std::vector<PatternTemplate> testGenerateSplitTemplates(const std::vector<PatternTemplate>& exactTemplates)
    {
        std::unordered_set<unsigned long long> dummyHashSet;
        return generateSplitTemplates(exactTemplates, dummyHashSet);
    }

private:
    struct GenerationTask;

    // --- NEW CORE ALGORITHM ---
    std::vector<PatternTemplate> generatePatternsForTask(const GenerationTask& task);
    
    // --- NEW HELPER FUNCTIONS for the permutation-based algorithm ---
    void generateBarCombinationsRecursive(
        size_t offset,
        size_t k,
        const std::vector<uint8_t>& allOffsets,
        std::vector<uint8_t>& currentCombination,
        std::vector<std::vector<uint8_t>>& results
    ) const;

    void generateComponentCombinationsRecursive(
        size_t offset,
        size_t k,
        const std::vector<PriceComponentDescriptor>& components,
        std::vector<PriceComponentDescriptor>& currentCombination,
        std::vector<std::vector<PriceComponentDescriptor>>& results
    ) const;

    // --- Other Helper Functions ---
    std::vector<PatternTemplate> generateSplitTemplates(const std::vector<PatternTemplate>& exactTemplates, std::unordered_set<unsigned long long>& seenHashes);
    PatternTemplate createDelayedTemplate(const PatternTemplate& baseTemplate, uint8_t delay) const;

    std::vector<PriceComponentDescriptor> generateComponentPool(
        const std::vector<PriceComponentType>& typesToUse,
        uint8_t minOffset,
        uint8_t maxOffset) const;
    
    std::vector<PatternCondition> generateConditionPool(
        const std::vector<PriceComponentDescriptor>& componentPool,
        bool isMixed) const;
    
    bool isValidCombination(const std::vector<PatternCondition>& conditions) const;
    std::string generatePatternName(const std::vector<PatternCondition>& conditions, uint8_t delay, const std::string& prefix) const;

    std::string m_outputFile;
    uint8_t m_maxLookback;
    uint8_t m_maxConditions;
    uint8_t m_maxSpread; // <-- NEW: Max spread member
    std::string m_searchType;
    mutable std::mutex m_resultsMutex;
};

// =================================================================================
// IMPLEMENTATION
// =================================================================================

static std::string componentTypeToString(PriceComponentType type)
{
    switch (type)
    {
        case PriceComponentType::Open:  return "O";
        case PriceComponentType::High:  return "H";
        case PriceComponentType::Low:   return "L";
        case PriceComponentType::Close: return "C";
    }
    return "?";
}

static std::string pcdToString(const PriceComponentDescriptor& pcd)
{
    std::stringstream ss;
    ss << componentTypeToString(pcd.getComponentType())
       << "[" << static_cast<int>(pcd.getBarOffset()) << "]";
    return ss.str();
}

template <typename Executor>
struct UniverseGenerator<Executor>::GenerationTask
{
    std::string name;
    std::vector<PriceComponentType> componentTypes;
    uint8_t minPatternLength;
    uint8_t maxPatternLength;
    bool isMixed = false;
};

// MODIFIED: Constructor implementation updated for maxSpread
template <typename Executor>
UniverseGenerator<Executor>::UniverseGenerator(
    const std::string& outputFile,
    uint8_t maxLookback,
    uint8_t maxConditions,
    uint8_t maxSpread,
    const std::string& searchType)
    : m_outputFile(outputFile),
      m_maxLookback(maxLookback),
      m_maxConditions(maxConditions),
      m_maxSpread(maxSpread),
      m_searchType(searchType)
{
    if (outputFile.empty())
    {
        throw std::invalid_argument("Output file path cannot be empty.");
    }
    if (maxLookback == 0 || maxConditions == 0)
    {
        throw std::invalid_argument("Max lookback and max conditions must be greater than zero.");
    }
}

// MODIFIED: Re-enabled delayed pattern generation
template <typename Executor>
void UniverseGenerator<Executor>::run()
{
    std::vector<PatternTemplate> exactTemplates;
    std::unordered_set<unsigned long long> seenHashes;

    std::vector<GenerationTask> tasks;
    if (m_searchType == "DEEP")
    {
        tasks = {
            {"Close", {PriceComponentType::Close}, 3, 9, false},
            {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 9, true},
            {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 9, false},
            {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 9, false}
        };
    }
    else if (m_searchType == "EXTENDED")
    {
         tasks = {
            {"Close", {PriceComponentType::Close}, 2, 6, false},
            {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 6, true},
            {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 6, false},
            {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 6, false}
        };
    }
    else
    {
        throw std::runtime_error("Unsupported search type: " + m_searchType);
    }

    // --- Stage 1: Generate all base "Exact" patterns ---
    for (const auto& task : tasks)
    {
        std::cout << "\n--- Starting Exact Pattern Task: " << task.name << " ---" << std::endl;
        auto taskTemplates = generatePatternsForTask(task);
        exactTemplates.insert(exactTemplates.end(), taskTemplates.begin(), taskTemplates.end());
    }
    
    for(const auto& tpl : exactTemplates) {
        seenHashes.insert(std::hash<PatternTemplate>{}(tpl));
    }
    
    std::vector<PatternTemplate> allGeneratedTemplates = exactTemplates;

    // --- NEW: Stage 2: Generate Delayed Patterns ---
    std::cout << "\n--- Starting Delayed Pattern Generation (1-5 bar delay) ---" << std::endl;
    std::vector<PatternTemplate> delayedTemplates;
    const uint8_t maxDelay = 5;

    for (uint8_t delay = 1; delay <= maxDelay; ++delay) {
        for (const auto& baseTemplate : exactTemplates) {
            // Ensure the delayed pattern doesn't exceed the max lookback
            uint8_t maxOffsetInBase = 0;
            for(const auto& cond : baseTemplate.getConditions()) {
                maxOffsetInBase = std::max({maxOffsetInBase, cond.getLhs().getBarOffset(), cond.getRhs().getBarOffset()});
            }

            if (maxOffsetInBase + delay > m_maxLookback) {
                continue; // Skip creating this delayed pattern as it would be out of bounds
            }
            
            PatternTemplate newDelayedTemplate = createDelayedTemplate(baseTemplate, delay);
            auto hash = std::hash<PatternTemplate>{}(newDelayedTemplate);
            
            if (seenHashes.find(hash) == seenHashes.end()) {
                seenHashes.insert(hash);
                delayedTemplates.push_back(newDelayedTemplate);
            }
        }
    }
    std::cout << "  - Found " << delayedTemplates.size() << " unique delayed patterns." << std::endl;
    allGeneratedTemplates.insert(allGeneratedTemplates.end(), delayedTemplates.begin(), delayedTemplates.end());
    
    // --- Stage 3 is not implemented in this version for simplicity ---
    
    std::cout << "\nTotal patterns generated (including all variations): " << allGeneratedTemplates.size() << std::endl;

    std::cout << "\n--- Sample of Generated Patterns ---" << std::endl;
    for(size_t i = 0; i < 20 && i < allGeneratedTemplates.size(); ++i) {
        // To make samples more interesting, show a mix of base and delayed
        size_t index = (i < allGeneratedTemplates.size() / 2) ? i : allGeneratedTemplates.size() - i -1;
        if(index < allGeneratedTemplates.size()) {
            std::cout << allGeneratedTemplates[index].getName() << std::endl;
        }
    }
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Universe generation logic complete. File serialization is the next step." << std::endl;
}


template <typename Executor>
void UniverseGenerator<Executor>::generateBarCombinationsRecursive(
    size_t offset,
    size_t k,
    const std::vector<uint8_t>& allOffsets,
    std::vector<uint8_t>& currentCombination,
    std::vector<std::vector<uint8_t>>& results
) const {
    if (k == 0) {
        results.push_back(currentCombination);
        return;
    }
    for (size_t i = offset; i <= allOffsets.size() - k; ++i) {
        currentCombination.push_back(allOffsets[i]);
        generateBarCombinationsRecursive(i + 1, k - 1, allOffsets, currentCombination, results);
        currentCombination.pop_back();
    }
}

template <typename Executor>
void UniverseGenerator<Executor>::generateComponentCombinationsRecursive(
    size_t offset,
    size_t k,
    const std::vector<PriceComponentDescriptor>& components,
    std::vector<PriceComponentDescriptor>& currentCombination,
    std::vector<std::vector<PriceComponentDescriptor>>& results
) const {
    if (k == 0) {
        results.push_back(currentCombination);
        return;
    }
    for (size_t i = offset; i <= components.size() - k; ++i) {
        currentCombination.push_back(components[i]);
        generateComponentCombinationsRecursive(i + 1, k - 1, components, currentCombination, results);
        currentCombination.pop_back();
    }
}


// MODIFIED: This function now implements the full permutation algorithm with max-spread filtering.
template <typename Executor>
std::vector<PatternTemplate> UniverseGenerator<Executor>::generatePatternsForTask(const GenerationTask& task)
{
    std::vector<PatternTemplate> taskTemplates;
    std::unordered_set<unsigned long long> seenHashes;

    std::vector<uint8_t> allBarOffsets;
    for (uint8_t i = 0; i <= m_maxLookback; ++i) {
        allBarOffsets.push_back(i);
    }
    
    const uint8_t minUniqueBars = 2;
    const uint8_t maxUniqueBars = 8; 

    for (uint8_t numBars = minUniqueBars; numBars <= maxUniqueBars; ++numBars)
    {
        // --- FIX: Prevent trying to choose more bars than are available ---
        if (numBars > allBarOffsets.size()) {
            break; // Stop searching, as we've exhausted the available bar offsets.
        }

        std::cout << "  - Searching patterns with " << static_cast<int>(numBars) << " unique bars..." << std::endl;
        
        std::vector<std::vector<uint8_t>> barCombinations;
        std::vector<uint8_t> currentBarCombination;
        generateBarCombinationsRecursive(0, numBars, allBarOffsets, currentBarCombination, barCombinations);
        
        // --- NEW: Filter bar combinations based on the max-spread constraint ---
        std::vector<std::vector<uint8_t>> filteredBarCombinations;
        filteredBarCombinations.reserve(barCombinations.size());
        for (const auto& combo : barCombinations) {
            if (combo.empty()) continue;
            auto minmax = std::minmax_element(combo.begin(), combo.end());
            if ((*minmax.second - *minmax.first) <= m_maxSpread) {
                filteredBarCombinations.push_back(combo);
            }
        }
        
        std::cout << "    - Found " << barCombinations.size() << " raw bar combinations. Pruning with max-spread of " 
                  << static_cast<int>(m_maxSpread) << " leaves " << filteredBarCombinations.size() << " combinations." << std::endl;

        size_t totalBarCombos = filteredBarCombinations.size();
        for(size_t i = 0; i < totalBarCombos; ++i)
        {
            if (i > 0 && (i % 100 == 0 || i == totalBarCombos - 1)) {
                 std::cout << "\r      - Processing bar combination " << i + 1 << " / " << totalBarCombos << std::flush;
            }

            const auto& barCombo = filteredBarCombinations[i];
            
            std::vector<PriceComponentDescriptor> componentPool;
            for (uint8_t barOffset : barCombo) {
                for (const auto& type : task.componentTypes) {
                    componentPool.emplace_back(type, barOffset);
                }
            }

            for (uint8_t k = task.minPatternLength; k <= m_maxConditions && k <= task.maxPatternLength; ++k)
            {
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
                            
                            // This lock is less critical now as we don't use parallel_for in this specific function,
                            // but it's good practice for potential future parallelization.
                            std::lock_guard<std::mutex> lock(m_resultsMutex);
                            if(seenHashes.find(hash) == seenHashes.end()) {
                                seenHashes.insert(hash);
                                std::string name = generatePatternName(conditions, 0, "");
                                PatternTemplate finalTemplate(name);
                                for(const auto& cond : conditions) finalTemplate.addCondition(cond);
                                taskTemplates.push_back(finalTemplate);
                            }
                        }
                    } while (std::next_permutation(pcdCombo.begin(), pcdCombo.end()));
                }
            }
        }
        if (totalBarCombos > 0) std::cout << std::endl; // Newline after progress bar
    }
    
    std::cout << "  - Found " << taskTemplates.size() << " unique base patterns for task." << std::endl;
    return taskTemplates;
}


template <typename Executor>
std::vector<PriceComponentDescriptor> UniverseGenerator<Executor>::generateComponentPool(
    const std::vector<PriceComponentType>& typesToUse,
    uint8_t minOffset,
    uint8_t maxOffset) const
{
    std::vector<PriceComponentDescriptor> pool;
    pool.reserve((maxOffset - minOffset + 1) * typesToUse.size());

    for (const auto& type : typesToUse)
    {
        for (uint8_t offset = minOffset; offset <= maxOffset; ++offset)
        {
            pool.emplace_back(type, offset);
        }
    }
    return pool;
}

template <typename Executor>
std::vector<PatternCondition> UniverseGenerator<Executor>::generateConditionPool(
    const std::vector<PriceComponentDescriptor>& componentPool,
    bool isMixed) const
{
    std::vector<PatternCondition> conditions;
    if (componentPool.size() < 2)
    {
        return conditions;
    }
    
    conditions.reserve(componentPool.size() * componentPool.size());

    for (size_t i = 0; i < componentPool.size(); ++i)
    {
        for (size_t j = i + 1; j < componentPool.size(); ++j)
        {
            const auto& lhs = componentPool[i];
            const auto& rhs = componentPool[j];

            if (isMixed && lhs.getComponentType() == rhs.getComponentType())
            {
                continue;
            }
            
            bool forward_valid = true;
            if (lhs.getBarOffset() == rhs.getBarOffset())
            {
                PriceComponentType t1 = lhs.getComponentType();
                PriceComponentType t2 = rhs.getComponentType();
                if (t1 == PriceComponentType::Low || t2 == PriceComponentType::High ||
                   (t1 == PriceComponentType::Open && t2 == PriceComponentType::High) ||
                   (t1 == PriceComponentType::Close && t2 == PriceComponentType::High))
                {
                    forward_valid = false;
                }
            }
            if(forward_valid)
            {
                conditions.emplace_back(lhs, ComparisonOperator::GreaterThan, rhs);
            }

            bool reverse_valid = true;
            if (lhs.getBarOffset() == rhs.getBarOffset())
            {
                PriceComponentType t1 = rhs.getComponentType();
                PriceComponentType t2 = lhs.getComponentType();
                if (t1 == PriceComponentType::Low || t2 == PriceComponentType::High ||
                   (t1 == PriceComponentType::Open && t2 == PriceComponentType::High) ||
                   (t1 == PriceComponentType::Close && t2 == PriceComponentType::High))
                {
                    reverse_valid = false;
                }
            }
            if(reverse_valid)
            {
                conditions.emplace_back(rhs, ComparisonOperator::GreaterThan, lhs);
            }
        }
    }
    return conditions;
}

template <typename Executor>
bool UniverseGenerator<Executor>::isValidCombination(const std::vector<PatternCondition>& conditions) const
{
    // For a simple chained pattern A > B > C, transitivity issues are less likely,
    // but this check can still prevent redundant components like C[0] > C[1] > C[0].
    std::set<PriceComponentDescriptor> components;
    for (const auto& cond : conditions) {
        components.insert(cond.getLhs());
        components.insert(cond.getRhs());
    }
    // A valid chained pattern of k conditions must have k+1 unique components.
    return components.size() == conditions.size() + 1;
}

template <typename Executor>
std::vector<PatternTemplate> UniverseGenerator<Executor>::generateSplitTemplates(const std::vector<PatternTemplate>& exactTemplates, std::unordered_set<unsigned long long>& seenHashes)
{
    // This functionality can be re-integrated later if needed
    return {};
}

template <typename Executor>
PatternTemplate UniverseGenerator<Executor>::createDelayedTemplate(const PatternTemplate& baseTemplate, uint8_t delay) const
{
    std::vector<PatternCondition> delayedConditions;
    delayedConditions.reserve(baseTemplate.getConditions().size());

    for (const auto& cond : baseTemplate.getConditions())
    {
        PriceComponentDescriptor newLhs(cond.getLhs().getComponentType(), cond.getLhs().getBarOffset() + delay);
        PriceComponentDescriptor newRhs(cond.getRhs().getComponentType(), cond.getRhs().getBarOffset() + delay);
        delayedConditions.emplace_back(newLhs, ComparisonOperator::GreaterThan, newRhs);
    }

    std::string name_prefix = baseTemplate.getName().find("Split") != std::string::npos ? "Split" : "";
    std::string delayedName = generatePatternName(delayedConditions, delay, name_prefix);
    PatternTemplate newDelayedTemplate(delayedName);
    for (const auto& cond : delayedConditions)
    {
        newDelayedTemplate.addCondition(cond);
    }

    return newDelayedTemplate;
}

template <typename Executor>
std::string UniverseGenerator<Executor>::generatePatternName(const std::vector<PatternCondition>& conditions, uint8_t delay, const std::string& prefix) const
{
    if (conditions.empty())
    {
        return "EMPTY_PATTERN";
    }

    std::stringstream finalName;
    if (!prefix.empty())
    {
        finalName << prefix << ": ";
    }
    
    // Create the name from the permutation chain directly
    finalName << pcdToString(conditions[0].getLhs());
    for(const auto& cond : conditions) {
        finalName << " > " << pcdToString(cond.getRhs());
    }

    if (delay > 0)
    {
        finalName << " [Delay: " << static_cast<int>(delay) << "]";
    }

    return finalName.str();
}