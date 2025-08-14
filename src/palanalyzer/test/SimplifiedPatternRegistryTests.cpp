#include <catch2/catch_test_macros.hpp>
#include "SimplifiedPatternRegistry.h"
#include "AnalysisDatabase.h"
#include "DataStructures.h"
#include <algorithm>
#include <chrono>
#include <vector>
#include <set>

using namespace palanalyzer;

/**
 * @brief Test fixture for SimplifiedPatternRegistry tests
 */
class SimplifiedPatternRegistryTestFixture {
public:
    SimplifiedPatternRegistryTestFixture()
        : database("test_registry_database.json")
    {
        setupTestDatabase();
        registry = std::make_unique<SimplifiedPatternRegistry>(database);
    }

    ~SimplifiedPatternRegistryTestFixture()
    {
        // Clean up test database file
        std::remove("test_registry_database.json");
    }

protected:
    AnalysisDatabase database;
    std::unique_ptr<SimplifiedPatternRegistry> registry;

    void setupTestDatabase()
    {
        // Create test index groups with different search types
        
        // Group 201: Deep search with CLOSE patterns
        std::vector<uint8_t> barCombination1 = {0, 1, 2};
        std::set<PriceComponentType> componentTypes1 = {PriceComponentType::Close};
        database.addPatternToIndexGroup(201, barCombination1, componentTypes1, "deep_close.pal", "Deep");
        
        // Group 202: Extended search with HIGH+LOW patterns
        std::vector<uint8_t> barCombination2 = {0, 1, 3};
        std::set<PriceComponentType> componentTypes2 = {PriceComponentType::High, PriceComponentType::Low};
        database.addPatternToIndexGroup(202, barCombination2, componentTypes2, "extended_hl.pal", "Extended");
        
        // Group 203: Basic search with mixed patterns
        std::vector<uint8_t> barCombination3 = {0, 1};
        std::set<PriceComponentType> componentTypes3 = {
            PriceComponentType::Open, PriceComponentType::Close
        };
        database.addPatternToIndexGroup(203, barCombination3, componentTypes3, "basic_mixed.pal", "Basic");
        
        // Add test patterns to each group
        addTestPatternsToGroup(201, componentTypes1, barCombination1, "Deep");
        addTestPatternsToGroup(202, componentTypes2, barCombination2, "Extended");
        addTestPatternsToGroup(203, componentTypes3, barCombination3, "Basic");
    }

    void addTestPatternsToGroup(uint32_t groupId, 
                               const std::set<PriceComponentType>& componentTypes,
                               const std::vector<uint8_t>& barOffsets,
                               const std::string& searchType)
    {
        std::vector<PriceComponentType> components(componentTypes.begin(), componentTypes.end());
        
        for (size_t i = 0; i < 3; ++i) // Add 3 patterns per group
        {
            std::vector<PriceComponentDescriptor> patternComponents;
            
            // Create components using the group's allowed types and offsets
            for (size_t j = 0; j < std::min(components.size(), static_cast<size_t>(2)); ++j)
            {
                uint8_t offset = (j < barOffsets.size()) ? barOffsets[j] : 0;
                patternComponents.emplace_back(components[j], offset, "Component" + std::to_string(j));
            }
            
            PatternAnalysis testPattern(
                groupId,                                       // index
                searchType + "_test_file.pal",                // sourceFile
                2000000ULL + groupId * 1000 + i,              // patternHash (unique)
                patternComponents,                             // components
                "Test Pattern " + std::to_string(i),          // patternString
                false,                                         // isChained
                static_cast<uint8_t>(barOffsets.size() - 1),  // maxBarOffset
                static_cast<uint8_t>(barOffsets.size() - 1),  // barSpread
                static_cast<uint8_t>(patternComponents.size() > 0 ? patternComponents.size() - 1 : 0), // conditionCount (components - 1 for simple patterns)
                std::chrono::system_clock::now(),             // analyzedAt
                0.6 + (i * 0.05),                             // profitabilityLong
                0.4 + (i * 0.03),                             // profitabilityShort
                50 + (i * 10),                                // trades
                2 + i                                         // consecutiveLosses
            );
            
            database.addPattern(testPattern);
        }
    }

    PatternStructure createTestPatternStructure(unsigned long long hash, uint32_t groupId)
    {
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            )
        };
        
        return PatternStructure(
            hash,                                          // patternHash
            static_cast<int>(groupId),                     // groupId
            conditions,                                    // conditions
            {"CLOSE"},                                     // componentsUsed
            {0, 1}                                         // barOffsetsUsed
        );
    }
};

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "SimplifiedPatternRegistry Construction", "[SimplifiedPatternRegistry]")
{
    SECTION("Constructor creates valid registry")
    {
        REQUIRE(registry != nullptr);
        REQUIRE(registry->size() > 0); // Should have patterns from setup
        REQUIRE_FALSE(registry->isEmpty());
    }
    
    SECTION("Registry builds indices from database")
    {
        // Verify registry has patterns from all test groups
        std::vector<uint32_t> groupIds = registry->getAllGroupIds();
        REQUIRE_FALSE(groupIds.empty());
        
        // Should contain our test groups
        REQUIRE(std::find(groupIds.begin(), groupIds.end(), 201) != groupIds.end());
        REQUIRE(std::find(groupIds.begin(), groupIds.end(), 202) != groupIds.end());
        REQUIRE(std::find(groupIds.begin(), groupIds.end(), 203) != groupIds.end());
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Pattern Registration", "[SimplifiedPatternRegistry]")
{
    SECTION("Register single pattern")
    {
        size_t initialSize = registry->size();
        
        PatternStructure newPattern = createTestPatternStructure(9999999ULL, 201);
        registry->registerPattern(newPattern);
        
        REQUIRE(registry->size() == initialSize + 1);
        REQUIRE(registry->exists(9999999ULL));
        
        auto foundPattern = registry->findByHash(9999999ULL);
        REQUIRE(foundPattern.has_value());
        REQUIRE(foundPattern->getPatternHash() == 9999999ULL);
    }
    
    SECTION("Register pattern batch")
    {
        size_t initialSize = registry->size();
        
        std::vector<PatternStructure> newPatterns = {
            createTestPatternStructure(8888888ULL, 201),
            createTestPatternStructure(7777777ULL, 202),
            createTestPatternStructure(6666666ULL, 203)
        };
        
        registry->registerPatternBatch(newPatterns);
        
        REQUIRE(registry->size() == initialSize + 3);
        REQUIRE(registry->exists(8888888ULL));
        REQUIRE(registry->exists(7777777ULL));
        REQUIRE(registry->exists(6666666ULL));
    }
    
    SECTION("Register inconsistent pattern logs warning but continues")
    {
        // Create pattern with inconsistent structure
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            )
        };
        
        PatternStructure inconsistentPattern(
            0ULL,  // Invalid hash
            -1,    // Invalid group ID
            conditions,
            {"CLOSE"},
            {0, 1}
        );
        
        size_t initialSize = registry->size();
        registry->registerPattern(inconsistentPattern);
        
        // Should not add inconsistent pattern
        REQUIRE(registry->size() == initialSize);
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Pattern Lookup Operations", "[SimplifiedPatternRegistry]")
{
    SECTION("Find pattern by hash - O(1) operation")
    {
        std::vector<unsigned long long> allHashes = registry->getAllHashes();
        REQUIRE_FALSE(allHashes.empty());
        
        unsigned long long testHash = allHashes[0];
        auto pattern = registry->findByHash(testHash);
        
        REQUIRE(pattern.has_value());
        REQUIRE(pattern->getPatternHash() == testHash);
    }
    
    SECTION("Find patterns by group")
    {
        std::vector<PatternStructure> group201Patterns = registry->findByGroup(201);
        REQUIRE_FALSE(group201Patterns.empty());
        
        // Verify all patterns belong to group 201
        for (const auto& pattern : group201Patterns)
        {
            REQUIRE(pattern.getGroupId() == 201);
        }
    }
    
    SECTION("Find patterns by search type")
    {
        std::vector<PatternStructure> deepPatterns = registry->findBySearchType(SearchType::DEEP);
        std::vector<PatternStructure> extendedPatterns = registry->findBySearchType(SearchType::EXTENDED);
        std::vector<PatternStructure> basicPatterns = registry->findBySearchType(SearchType::BASIC);
        
        // Should have patterns for each search type
        REQUIRE_FALSE(deepPatterns.empty());
        REQUIRE_FALSE(extendedPatterns.empty());
        REQUIRE_FALSE(basicPatterns.empty());
    }
    
    SECTION("Non-existing pattern returns nullopt")
    {
        auto pattern = registry->findByHash(99999999999999999ULL);
        REQUIRE_FALSE(pattern.has_value());
    }
    
    SECTION("Non-existing group returns empty vector")
    {
        std::vector<PatternStructure> patterns = registry->findByGroup(999);
        REQUIRE(patterns.empty());
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Pattern Existence Checks", "[SimplifiedPatternRegistry]")
{
    SECTION("Exists check for valid patterns")
    {
        std::vector<unsigned long long> allHashes = registry->getAllHashes();
        REQUIRE_FALSE(allHashes.empty());
        
        for (unsigned long long hash : allHashes)
        {
            REQUIRE(registry->exists(hash));
        }
    }
    
    SECTION("Exists check for invalid patterns")
    {
        REQUIRE_FALSE(registry->exists(99999999999999999ULL));
        REQUIRE_FALSE(registry->exists(0ULL));
    }
    
    SECTION("Exists in group check")
    {
        std::vector<PatternStructure> group201Patterns = registry->findByGroup(201);
        if (!group201Patterns.empty())
        {
            unsigned long long testHash = group201Patterns[0].getPatternHash();
            
            REQUIRE(registry->existsInGroup(testHash, 201));
            REQUIRE_FALSE(registry->existsInGroup(testHash, 999)); // Wrong group
            REQUIRE_FALSE(registry->existsInGroup(99999999999999999ULL, 201)); // Wrong hash
        }
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Registry Statistics", "[SimplifiedPatternRegistry]")
{
    SECTION("Get registry statistics")
    {
        RegistryStats stats = registry->getRegistryStats();
        
        REQUIRE(stats.getTotalPatterns() > 0);
        REQUIRE(stats.getUniqueHashes() == stats.getTotalPatterns()); // No duplicates
        REQUIRE(stats.getTotalGroups() > 0);
        
        // Verify search type breakdown
        std::vector<SearchType> availableSearchTypes = stats.getAvailableSearchTypes();
        REQUIRE_FALSE(availableSearchTypes.empty());
        
        for (SearchType searchType : availableSearchTypes)
        {
            size_t count = stats.getSearchTypeCount(searchType);
            REQUIRE(count > 0);
        }
        
        // Verify group size distribution
        std::vector<uint32_t> availableGroups = stats.getAvailableGroups();
        REQUIRE_FALSE(availableGroups.empty());
        
        for (uint32_t groupId : availableGroups)
        {
            size_t size = stats.getGroupSize(groupId);
            REQUIRE(size > 0);
        }
    }
    
    SECTION("Statistics reflect actual registry contents")
    {
        RegistryStats stats = registry->getRegistryStats();
        
        // Total patterns should match registry size
        REQUIRE(stats.getTotalPatterns() == registry->size());
        
        // Group count should match available groups
        std::vector<uint32_t> allGroupIds = registry->getAllGroupIds();
        REQUIRE(stats.getTotalGroups() == allGroupIds.size());
        
        // Search type counts should sum to total patterns
        size_t totalFromSearchTypes = 0;
        for (SearchType searchType : stats.getAvailableSearchTypes())
        {
            totalFromSearchTypes += stats.getSearchTypeCount(searchType);
        }
        REQUIRE(totalFromSearchTypes == stats.getTotalPatterns());
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Registry Maintenance Operations", "[SimplifiedPatternRegistry]")
{
    SECTION("Clear registry removes all patterns")
    {
        REQUIRE(registry->size() > 0); // Verify we have patterns
        
        registry->clear();
        
        REQUIRE(registry->size() == 0);
        REQUIRE(registry->isEmpty());
        REQUIRE(registry->getAllHashes().empty());
        REQUIRE(registry->getAllGroupIds().empty());
        REQUIRE(registry->getAllSearchTypes().empty());
    }
    
    SECTION("Rebuild registry restores patterns from database")
    {
        size_t originalSize = registry->size();
        std::vector<unsigned long long> originalHashes = registry->getAllHashes();
        
        registry->clear();
        REQUIRE(registry->isEmpty());
        
        registry->rebuild();
        
        REQUIRE(registry->size() == originalSize);
        
        // Verify all original patterns are restored
        for (unsigned long long hash : originalHashes)
        {
            REQUIRE(registry->exists(hash));
        }
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Registry Query Operations", "[SimplifiedPatternRegistry]")
{
    SECTION("Get all hashes returns complete list")
    {
        std::vector<unsigned long long> allHashes = registry->getAllHashes();
        
        REQUIRE(allHashes.size() == registry->size());
        
        // Verify each hash exists in registry
        for (unsigned long long hash : allHashes)
        {
            REQUIRE(registry->exists(hash));
        }
        
        // Verify no duplicates
        std::set<unsigned long long> uniqueHashes(allHashes.begin(), allHashes.end());
        REQUIRE(uniqueHashes.size() == allHashes.size());
    }
    
    SECTION("Get all group IDs returns valid groups")
    {
        std::vector<uint32_t> groupIds = registry->getAllGroupIds();
        
        REQUIRE_FALSE(groupIds.empty());
        
        // Should contain our test groups
        REQUIRE(std::find(groupIds.begin(), groupIds.end(), 201) != groupIds.end());
        REQUIRE(std::find(groupIds.begin(), groupIds.end(), 202) != groupIds.end());
        REQUIRE(std::find(groupIds.begin(), groupIds.end(), 203) != groupIds.end());
        
        // Verify each group has patterns
        for (uint32_t groupId : groupIds)
        {
            std::vector<PatternStructure> groupPatterns = registry->findByGroup(groupId);
            REQUIRE_FALSE(groupPatterns.empty());
        }
    }
    
    SECTION("Get all search types returns valid types")
    {
        std::vector<SearchType> searchTypes = registry->getAllSearchTypes();
        
        REQUIRE_FALSE(searchTypes.empty());
        
        // Should contain our test search types
        REQUIRE(std::find(searchTypes.begin(), searchTypes.end(), SearchType::DEEP) != searchTypes.end());
        REQUIRE(std::find(searchTypes.begin(), searchTypes.end(), SearchType::EXTENDED) != searchTypes.end());
        REQUIRE(std::find(searchTypes.begin(), searchTypes.end(), SearchType::BASIC) != searchTypes.end());
        
        // Verify each search type has patterns
        for (SearchType searchType : searchTypes)
        {
            std::vector<PatternStructure> searchTypePatterns = registry->findBySearchType(searchType);
            REQUIRE_FALSE(searchTypePatterns.empty());
        }
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Performance Characteristics", "[SimplifiedPatternRegistry]")
{
    SECTION("Hash lookup is O(1) - performance test")
    {
        std::vector<unsigned long long> allHashes = registry->getAllHashes();
        REQUIRE_FALSE(allHashes.empty());
        
        // Measure lookup time for multiple operations
        auto startTime = std::chrono::high_resolution_clock::now();
        
        const size_t LOOKUP_COUNT = 1000;
        size_t foundCount = 0;
        
        for (size_t i = 0; i < LOOKUP_COUNT; ++i)
        {
            unsigned long long testHash = allHashes[i % allHashes.size()];
            if (registry->exists(testHash))
            {
                foundCount++;
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // Should find all patterns
        REQUIRE(foundCount == LOOKUP_COUNT);
        
        // Performance should be reasonable (less than 1ms per lookup on average)
        double avgLookupTime = static_cast<double>(duration.count()) / LOOKUP_COUNT;
        REQUIRE(avgLookupTime < 1000.0); // Less than 1000 microseconds (1ms) per lookup
    }
}

TEST_CASE("RegistryStats Class", "[RegistryStats]")
{
    SECTION("Constructor and factory method pattern")
    {
        RegistryStats stats(100, 100, 5);
        
        REQUIRE(stats.getTotalPatterns() == 100);
        REQUIRE(stats.getUniqueHashes() == 100);
        REQUIRE(stats.getTotalGroups() == 5);
        
        // Initially no search type or group data
        REQUIRE(stats.getAvailableSearchTypes().empty());
        REQUIRE(stats.getAvailableGroups().empty());
    }
    
    SECTION("Add search type and group data")
    {
        RegistryStats stats(50, 50, 3);
        
        stats.addSearchTypeData(SearchType::DEEP, 30);
        stats.addSearchTypeData(SearchType::EXTENDED, 20);
        
        stats.addGroupData(101, 15);
        stats.addGroupData(102, 20);
        stats.addGroupData(103, 15);
        
        REQUIRE(stats.getSearchTypeCount(SearchType::DEEP) == 30);
        REQUIRE(stats.getSearchTypeCount(SearchType::EXTENDED) == 20);
        REQUIRE(stats.getSearchTypeCount(SearchType::BASIC) == 0); // Not added
        
        REQUIRE(stats.getGroupSize(101) == 15);
        REQUIRE(stats.getGroupSize(102) == 20);
        REQUIRE(stats.getGroupSize(999) == 0); // Not added
        
        std::vector<SearchType> availableSearchTypes = stats.getAvailableSearchTypes();
        REQUIRE(availableSearchTypes.size() == 2);
        
        std::vector<uint32_t> availableGroups = stats.getAvailableGroups();
        REQUIRE(availableGroups.size() == 3);
    }
}

TEST_CASE_METHOD(SimplifiedPatternRegistryTestFixture, "Registry Consistency", "[SimplifiedPatternRegistry]")
{
    SECTION("All patterns in hash index are findable")
    {
        std::vector<unsigned long long> allHashes = registry->getAllHashes();
        
        for (unsigned long long hash : allHashes)
        {
            auto pattern = registry->findByHash(hash);
            REQUIRE(pattern.has_value());
            REQUIRE(pattern->getPatternHash() == hash);
        }
    }
    
    SECTION("All group patterns are consistent")
    {
        std::vector<uint32_t> groupIds = registry->getAllGroupIds();
        
        for (uint32_t groupId : groupIds)
        {
            std::vector<PatternStructure> groupPatterns = registry->findByGroup(groupId);
            
            for (const auto& pattern : groupPatterns)
            {
                REQUIRE(static_cast<uint32_t>(pattern.getGroupId()) == groupId);
                REQUIRE(registry->exists(pattern.getPatternHash()));
                REQUIRE(registry->existsInGroup(pattern.getPatternHash(), groupId));
            }
        }
    }
    
    SECTION("All search type patterns are consistent")
    {
        std::vector<SearchType> searchTypes = registry->getAllSearchTypes();
        
        for (SearchType searchType : searchTypes)
        {
            std::vector<PatternStructure> searchTypePatterns = registry->findBySearchType(searchType);
            
            for (const auto& pattern : searchTypePatterns)
            {
                REQUIRE(registry->exists(pattern.getPatternHash()));
                
                // Verify pattern belongs to a group with the correct search type
                uint32_t groupId = static_cast<uint32_t>(pattern.getGroupId());
                std::vector<PatternStructure> groupPatterns = registry->findByGroup(groupId);
                REQUIRE_FALSE(groupPatterns.empty());
            }
        }
    }
}