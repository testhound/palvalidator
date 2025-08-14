#include <catch2/catch_test_macros.hpp>
#include "PatternUniverseSerializer.h"
#include "PatternUniverseDeserializer.h"
#include "PatternTemplate.h"
#include "PatternCondition.h"
#include "PriceComponentDescriptor.h"

#include <sstream>
#include <vector>
#include <string>
#include <random>

// --- Helper Functions for Testing ---

/**
 * @brief Creates a small, fixed set of mock PatternTemplate objects for consistent testing.
 * @return A vector of predefined PatternTemplate objects.
 */
std::vector<PatternTemplate> createMockPatterns() {
    std::vector<PatternTemplate> patterns;

    // Pattern 1: Simple C[0] > C[1]
    PatternTemplate p1("Simple Crossover");
    p1.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Close, 1)
    ));
    patterns.push_back(p1);

    // Pattern 2: More complex pattern with conditions in a specific order
    PatternTemplate p2("Engulfing-like");
    p2.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Open, 1),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Close, 0)
    ));
    p2.addCondition(PatternCondition(
        PriceComponentDescriptor(PriceComponentType::Close, 1),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 0)
    ));
    patterns.push_back(p2);
    
    // Pattern 3: Empty pattern (edge case)
    PatternTemplate p3("Empty Pattern");
    patterns.push_back(p3);

    return patterns;
}

/**
 * @brief Creates a large, programmatically generated set of mock patterns for stress testing.
 * @param count The number of patterns to generate.
 * @return A vector of generated PatternTemplate objects.
 */
std::vector<PatternTemplate> createLargeMockPatterns(size_t count) {
    std::vector<PatternTemplate> patterns;
    patterns.reserve(count);

    std::mt19937 rng(12345); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint8_t> offset_dist(0, 50);
    std::uniform_int_distribution<int> type_dist(0, 3);
    std::uniform_int_distribution<int> cond_count_dist(1, 5);

    for (size_t i = 0; i < count; ++i) {
        std::string name = "GeneratedPattern_" + std::to_string(i);
        PatternTemplate p(name);

        int num_conditions = cond_count_dist(rng);
        for (int j = 0; j < num_conditions; ++j) {
            PriceComponentDescriptor lhs(
                static_cast<PriceComponentType>(type_dist(rng)),
                offset_dist(rng)
            );
            PriceComponentDescriptor rhs(
                static_cast<PriceComponentType>(type_dist(rng)),
                offset_dist(rng)
            );
            // Ensure lhs and rhs are not identical to form a valid condition
            if (lhs.getComponentType() == rhs.getComponentType() && lhs.getBarOffset() == rhs.getBarOffset()) {
                rhs = PriceComponentDescriptor(static_cast<PriceComponentType>((type_dist(rng) + 1) % 4), offset_dist(rng));
            }
            p.addCondition(PatternCondition(lhs, ComparisonOperator::GreaterThan, rhs));
        }
        patterns.push_back(p);
    }
    return patterns;
}


// --- Test Cases ---

TEST_CASE("Pattern Universe Serialization and Deserialization Round-Trip", "[PatternUniverse]") {
    
    // ARRANGE: Create serializers/deserializers once
    PatternUniverseSerializer serializer;
    PatternUniverseDeserializer deserializer;
    
    std::cout << "DEBUG_LOG: Testing Pattern Universe Serialization and Deserialization" << std::endl;
    
    SECTION("Serializing and then deserializing should yield identical patterns") {
        const auto originalPatterns = createMockPatterns();
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        
        serializer.serialize(ss, originalPatterns);
        REQUIRE(ss.good());
        ss.seekg(0);

        const auto deserializedPatterns = deserializer.deserialize(ss);
        
        REQUIRE(deserializedPatterns.size() == originalPatterns.size());
        for (size_t i = 0; i < originalPatterns.size(); ++i) {
            INFO("Comparing pattern at index " << i << " ('" << originalPatterns[i].getName() << "')");
            REQUIRE(deserializedPatterns[i] == originalPatterns[i]);
        }
    }

    SECTION("Serializing an empty vector should result in a valid file with zero patterns") {
        std::vector<PatternTemplate> emptyPatterns;
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        
        serializer.serialize(ss, emptyPatterns);
        REQUIRE(ss.good());
        ss.seekg(0);
        
        const auto deserializedPatterns = deserializer.deserialize(ss);
        
        REQUIRE(deserializedPatterns.empty());
    }

    SECTION("Handles a large number of patterns correctly") {
        const size_t large_count = 150;
        const auto originalPatterns = createLargeMockPatterns(large_count);
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // ACT
        serializer.serialize(ss, originalPatterns);
        REQUIRE(ss.good());
        ss.seekg(0);
        const auto deserializedPatterns = deserializer.deserialize(ss);

        // ASSERT
        REQUIRE(deserializedPatterns.size() == large_count);
        REQUIRE(deserializedPatterns == originalPatterns);
    }
}

TEST_CASE("PatternUniverseDeserializer Error Handling", "[PatternUniverse]") {
    
    PatternUniverseDeserializer deserializer;
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

    SECTION("Throws when the magic number is incorrect") {
        // Arrange: Write a header with a bad magic number
        FileHeader badHeader;
        badHeader.magicNumber = 0xDEADBEEF; 
        badHeader.patternCount = 1;
        ss.write(reinterpret_cast<const char*>(&badHeader), sizeof(FileHeader));
        ss.seekg(0);

        // Act & Assert: Expect a runtime error
        REQUIRE_THROWS_AS(deserializer.deserialize(ss), std::runtime_error);
    }

    SECTION("Throws when the stream is empty or incomplete") {
        // Arrange: An empty stream
        // Act & Assert: Expect an error when trying to read the header
        REQUIRE_THROWS(deserializer.deserialize(ss));
        
        // Arrange: A partially written header
        ss.clear();
        ss.str("");
        ss << "PART"; // Only 4 bytes, header is larger
        ss.seekg(0);
        
        // Act & Assert: Expect an error
        REQUIRE_THROWS(deserializer.deserialize(ss));
    }
}