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
    UniverseGenerator(
        const std::string& outputFile,
        uint8_t maxLookback,
        uint8_t maxConditions,
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

    std::vector<PatternTemplate> generatePatternsForTask(const GenerationTask& task);
    std::vector<PatternTemplate> generateSplitTemplates(const std::vector<PatternTemplate>& exactTemplates, std::unordered_set<unsigned long long>& seenHashes);
    PatternTemplate createDelayedTemplate(const PatternTemplate& baseTemplate, uint8_t delay) const;

    std::vector<PriceComponentDescriptor> generateComponentPool(
        const std::vector<PriceComponentType>& typesToUse,
        uint8_t minOffset,
        uint8_t maxOffset) const;

    std::vector<PatternCondition> generateConditionPool(
        const std::vector<PriceComponentDescriptor>& componentPool,
        bool isMixed) const;
    
    void findCombinationsRecursive(
        int offset,
        int k,
        const std::vector<PatternCondition>& conditionPool,
        std::vector<PatternCondition>& currentCombination,
        std::vector<PatternTemplate>& finalTemplates,
        std::unordered_set<unsigned long long>& seenHashes
    );

    bool isValidCombination(const std::vector<PatternCondition>& conditions) const;
    std::string generatePatternName(const std::vector<PatternCondition>& conditions, uint8_t delay, const std::string& prefix) const;

    std::string m_outputFile;
    uint8_t m_maxLookback;
    uint8_t m_maxConditions;
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

template <typename Executor>
UniverseGenerator<Executor>::UniverseGenerator(
    const std::string& outputFile,
    uint8_t maxLookback,
    uint8_t maxConditions,
    const std::string& searchType)
    : m_outputFile(outputFile),
      m_maxLookback(maxLookback),
      m_maxConditions(maxConditions),
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
            {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 9, false},      // CHANGED: minPatternLength from 3 to 2
            {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 9, false} // CHANGED: minPatternLength from 3 to 2
        };
    }
    else if (m_searchType == "EXTENDED")
    {
         tasks = {
            {"Close", {PriceComponentType::Close}, 2, 6, false},
            {"Mixed", {PriceComponentType::Open, PriceComponentType::High, PriceComponentType::Low, PriceComponentType::Close}, 2, 6, true},
            {"HighLow", {PriceComponentType::High, PriceComponentType::Low}, 2, 6, false},      // CHANGED: minPatternLength from 3 to 2
            {"OpenClose", {PriceComponentType::Open, PriceComponentType::Close}, 2, 6, false} // CHANGED: minPatternLength from 3 to 2
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
    
    // Populate the master hash set with the unique exact patterns
    for(const auto& tpl : exactTemplates) {
        seenHashes.insert(std::hash<PatternTemplate>{}(tpl));
    }
    
    std::vector<PatternTemplate> allGeneratedTemplates = exactTemplates;

    // --- Stage 2: Generate Split Patterns from the exact patterns (in parallel) ---
    std::cout << "\n--- Generating Split Pattern Variations ---" << std::endl;
    auto splitTemplates = generateSplitTemplates(exactTemplates, seenHashes);
    std::cout << "  - Generated " << splitTemplates.size() << " unique split patterns." << std::endl;
    allGeneratedTemplates.insert(allGeneratedTemplates.end(), splitTemplates.begin(), splitTemplates.end());


    // --- Stage 3: Generate Explicit Delay Patterns (in parallel) ---
    std::cout << "\n--- Generating Delayed Pattern Variations ---" << std::endl;
    std::vector<PatternTemplate> delayedTemplates;
    
    Executor executor{};
    concurrency::parallel_for(static_cast<uint32_t>(allGeneratedTemplates.size()), executor, [&](uint32_t i) {
        const auto& baseTemplate = allGeneratedTemplates[i];
        
        for (uint8_t delay = 1; delay <= 5; ++delay)
        {
            if (baseTemplate.getMaxBarOffset() + delay <= m_maxLookback)
            {
                auto delayedTpl = createDelayedTemplate(baseTemplate, delay);
                auto hash = std::hash<PatternTemplate>{}(delayedTpl);
                
                std::lock_guard<std::mutex> lock(m_resultsMutex);
                if(seenHashes.find(hash) == seenHashes.end())
                {
                    seenHashes.insert(hash);
                    delayedTemplates.push_back(delayedTpl);
                }
            }
        }
    });

    std::cout << "  - Generated " << delayedTemplates.size() << " unique delayed patterns." << std::endl;
    allGeneratedTemplates.insert(allGeneratedTemplates.end(), delayedTemplates.begin(), delayedTemplates.end());


    std::cout << "\nTotal patterns generated (including all variations): " << allGeneratedTemplates.size() << std::endl;

    std::cout << "\n--- Sample of Generated Patterns ---" << std::endl;
    for(size_t i = 0; i < 20 && i < allGeneratedTemplates.size(); ++i) {
        std::cout << allGeneratedTemplates[i].getName() << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Universe generation logic complete. File serialization is the next step." << std::endl;
}

template <typename Executor>
std::vector<PatternTemplate> UniverseGenerator<Executor>::generatePatternsForTask(const GenerationTask& task)
{
    auto componentPool = generateComponentPool(task.componentTypes, 0, m_maxLookback);
    std::cout << "  - Generated " << componentPool.size() << " components for task." << std::endl;

    auto conditionPool = generateConditionPool(componentPool, task.isMixed);
    std::cout << "  - Generated " << conditionPool.size() << " conditions for task." << std::endl;

    std::vector<PatternTemplate> taskTemplates;
    std::vector<PatternCondition> currentCombination;
    std::unordered_set<unsigned long long> seenHashes;

    for (uint8_t k = task.minPatternLength; k <= m_maxConditions && k <= task.maxPatternLength; ++k)
    {
        findCombinationsRecursive(0, k, conditionPool, currentCombination, taskTemplates, seenHashes);
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
    std::map<PriceComponentDescriptor, std::set<PriceComponentDescriptor>> greaterThanMap;

    for (const auto& cond : conditions)
    {
        greaterThanMap[cond.getLhs()].insert(cond.getRhs());
    }

    for (const auto& cond : conditions)
    {
        auto it = greaterThanMap.find(cond.getRhs());
        if (it != greaterThanMap.end() && it->second.count(cond.getLhs()))
        {
            return false;
        }

        auto it_b = greaterThanMap.find(cond.getRhs());
        if (it_b != greaterThanMap.end())
        {
            for (const auto& c_node : it_b->second)
            {
                auto it_a = greaterThanMap.find(cond.getLhs());
                if (it_a != greaterThanMap.end() && it_a->second.count(c_node))
                {
                    return false;
                }
            }
        }
    }

    return true;
}


template <typename Executor>
void UniverseGenerator<Executor>::findCombinationsRecursive(
    int offset,
    int k,
    const std::vector<PatternCondition>& conditionPool,
    std::vector<PatternCondition>& currentCombination,
    std::vector<PatternTemplate>& finalTemplates,
    std::unordered_set<unsigned long long>& seenHashes)
{
    if (k == 0)
    {
        if (isValidCombination(currentCombination))
        {
            PatternTemplate newTemplate("temp");
            for (const auto& cond : currentCombination)
            {
                newTemplate.addCondition(cond);
            }

            auto hash = std::hash<PatternTemplate>{}(newTemplate);
            
            std::lock_guard<std::mutex> lock(m_resultsMutex);
            if(seenHashes.find(hash) == seenHashes.end())
            {
                seenHashes.insert(hash);
                std::string name = generatePatternName(currentCombination, 0, "");
                PatternTemplate finalTemplate(name);
                for(const auto& cond : currentCombination) finalTemplate.addCondition(cond);
                finalTemplates.push_back(finalTemplate);
            }
        }
        return;
    }

    for (int i = offset; i <= static_cast<int>(conditionPool.size()) - k; ++i)
    {
        currentCombination.push_back(conditionPool[i]);
        findCombinationsRecursive(i + 1, k - 1, conditionPool, currentCombination, finalTemplates, seenHashes);
        currentCombination.pop_back();
    }
}

template <typename Executor>
std::vector<PatternTemplate> UniverseGenerator<Executor>::generateSplitTemplates(const std::vector<PatternTemplate>& exactTemplates, std::unordered_set<unsigned long long>& seenHashes)
{
    std::vector<PatternTemplate> splitTemplates;
    
    Executor executor{};
    concurrency::parallel_for(static_cast<uint32_t>(exactTemplates.size()), executor, [&](uint32_t i) {
        const auto& p2 = exactTemplates[i];
        
        for (size_t j = 0; j < exactTemplates.size(); ++j)
        {
            const auto& p1_base = exactTemplates[j];

            uint8_t p2_length = p2.getMaxBarOffset() + 1;
            uint8_t p1_length = p1_base.getMaxBarOffset() + 1;

            if (p1_length + p2_length > m_maxLookback)
            {
                continue;
            }

            std::vector<PatternCondition> p1_shifted_conditions;
            for (const auto& cond : p1_base.getConditions())
            {
                uint8_t newLhsOffset = cond.getLhs().getBarOffset() + p2_length;
                uint8_t newRhsOffset = cond.getRhs().getBarOffset() + p2_length;
                
                PriceComponentDescriptor newLhs(cond.getLhs().getComponentType(), newLhsOffset);
                PriceComponentDescriptor newRhs(cond.getRhs().getComponentType(), newRhsOffset);
                p1_shifted_conditions.emplace_back(newLhs, ComparisonOperator::GreaterThan, newRhs);
            }

            std::vector<PatternCondition> combinedConditions = p2.getConditions();
            combinedConditions.insert(combinedConditions.end(), p1_shifted_conditions.begin(), p1_shifted_conditions.end());

            std::string name = generatePatternName(combinedConditions, 0, "Split");
            PatternTemplate splitTemplate(name);
            for (const auto& cond : combinedConditions)
            {
                splitTemplate.addCondition(cond);
            }
            
            auto hash = std::hash<PatternTemplate>{}(splitTemplate);
            std::lock_guard<std::mutex> lock(m_resultsMutex);
            if(seenHashes.find(hash) == seenHashes.end())
            {
                seenHashes.insert(hash);
                splitTemplates.push_back(splitTemplate);
            }
        }
    });

    return splitTemplates;
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

    std::vector<std::string> conditionStrings;
    conditionStrings.reserve(conditions.size());

    for (const auto& cond : conditions)
    {
        std::stringstream ss;
        ss << pcdToString(cond.getLhs()) << ">" << pcdToString(cond.getRhs());
        conditionStrings.push_back(ss.str());
    }

    std::sort(conditionStrings.begin(), conditionStrings.end());

    std::stringstream finalName;
    if (!prefix.empty())
    {
        finalName << prefix << ": ";
    }

    for (size_t i = 0; i < conditionStrings.size(); ++i)
    {
        finalName << conditionStrings[i];
        if (i < conditionStrings.size() - 1)
        {
            finalName << " && ";
        }
    }

    if (delay > 0)
    {
        finalName << " [Delay: " << static_cast<int>(delay) << "]";
    }

    return finalName.str();
}
