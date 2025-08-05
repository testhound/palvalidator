#include <catch2/catch_test_macros.hpp>
// --- Include Actual Dependencies ---
#include "PriceComponentDescriptor.h"
#include "PatternCondition.h"
#include "PatternTemplate.h"

// --- Include the Class to be Tested ---
// The full implementation of UniverseGenerator is now in the header file.
#include "UniverseGenerator.h"

TEST_CASE("UniverseGenerator Initialization", "[constructor]") {
    SECTION("Throws on empty output file") {
        REQUIRE_THROWS_AS(UniverseGenerator<>("", 10, 5, "DEEP"), std::invalid_argument);
    }

    SECTION("Throws on zero max lookback") {
        REQUIRE_THROWS_AS(UniverseGenerator<>("test.bin", 0, 5, "DEEP"), std::invalid_argument);
    }

    SECTION("Throws on zero max conditions") {
        REQUIRE_THROWS_AS(UniverseGenerator<>("test.bin", 10, 0, "DEEP"), std::invalid_argument);
    }

    SECTION("Constructs successfully with valid arguments") {
        REQUIRE_NOTHROW(UniverseGenerator<>("test.bin", 10, 5, "DEEP"));
    }
}

TEST_CASE("UniverseGenerator Core Logic", "[generator]") {
    // Use the SingleThreadExecutor for deterministic testing
    UniverseGenerator<concurrency::SingleThreadExecutor> gen("test.bin", 2, 2, "EXTENDED");

    SECTION("Component Pool Generation") {
        auto components = gen.testGenerateComponentPool({PriceComponentType::Close}, 0, 1);
        REQUIRE(components.size() == 2);
        REQUIRE(components[0].getComponentType() == PriceComponentType::Close);
        REQUIRE(components[0].getBarOffset() == 0);
        REQUIRE(components[1].getComponentType() == PriceComponentType::Close);
        REQUIRE(components[1].getBarOffset() == 1);
    }

    SECTION("Condition Pool Generation") {
        auto components = gen.testGenerateComponentPool({PriceComponentType::Close, PriceComponentType::Open}, 0, 0);
        // Components are C[0], O[0]
        REQUIRE(components.size() == 2);

        auto conditions = gen.testGenerateConditionPool(components, false);
        // Should produce C[0]>O[0] and O[0]>C[0]
        REQUIRE(conditions.size() == 2);
    }
    
    SECTION("Mixed Condition Pool Generation") {
        auto components = gen.testGenerateComponentPool({PriceComponentType::Close, PriceComponentType::Open}, 0, 1);
        // C[0], C[1], O[0], O[1]
        auto conditions = gen.testGenerateConditionPool(components, true); // isMixed = true
        // It should not generate C[0]>C[1] or O[0]>O[1].
        // Total pairs: 6. isMixed should filter out 2 pairs (C-C, O-O). 4 pairs remain.
        // Each pair produces 2 conditions (A>B, B>A). Total = 8.
        REQUIRE(conditions.size() == 8);
    }

    SECTION("Delayed Template Creation") {
        PatternTemplate base("C[0]>O[1]");
        base.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 0),
            ComparisonOperator::GreaterThan, // Using the actual enum
            PriceComponentDescriptor(PriceComponentType::Open, 1)
        ));

        auto delayed = gen.testCreateDelayedTemplate(base, 3);
        REQUIRE(delayed.getName().find("[Delay: 3]") != std::string::npos);
        
        const auto& delayed_conds = delayed.getConditions();
        REQUIRE(delayed_conds.size() == 1);
        REQUIRE(delayed_conds[0].getLhs().getComponentType() == PriceComponentType::Close);
        REQUIRE(delayed_conds[0].getLhs().getBarOffset() == 3); // 0 + 3
        REQUIRE(delayed_conds[0].getRhs().getComponentType() == PriceComponentType::Open);
        REQUIRE(delayed_conds[0].getRhs().getBarOffset() == 4); // 1 + 3
    }
}

// NOTE: This is a simplified integration test that verifies exact, split, and delay pattern generation
// using the public test methods. We test with highly constrained parameters to verify the different
// generation stages are working correctly.
TEST_CASE("UniverseGenerator Pattern Generation Verification", "[integration]")
{
    UniverseGenerator<concurrency::SingleThreadExecutor> gen("test.bin", 5, 2, "EXTENDED");

    SECTION("Exact Pattern Generation") {
        // Test component pool generation for exact patterns
        auto components = gen.testGenerateComponentPool({PriceComponentType::Close}, 0, 2);
        REQUIRE(components.size() == 3); // C[0], C[1], C[2]
        
        // Test condition pool generation
        auto conditions = gen.testGenerateConditionPool(components, false);
        REQUIRE(conditions.size() > 0); // Should generate some conditions
        
        // Verify we have the expected component types and offsets
        bool foundClose0 = false, foundClose1 = false, foundClose2 = false;
        for (const auto& comp : components) {
            if (comp.getComponentType() == PriceComponentType::Close) {
                if (comp.getBarOffset() == 0) foundClose0 = true;
                if (comp.getBarOffset() == 1) foundClose1 = true;
                if (comp.getBarOffset() == 2) foundClose2 = true;
            }
        }
        REQUIRE(foundClose0);
        REQUIRE(foundClose1);
        REQUIRE(foundClose2);
    }

    SECTION("Split Pattern Generation") {
        // Create some base exact patterns to test split generation
        std::vector<PatternTemplate> exactPatterns;
        
        // Create first exact pattern: C[0] > C[1]
        PatternTemplate pattern1("Exact1");
        pattern1.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Close, 1)
        ));
        exactPatterns.push_back(pattern1);
        
        // Create second exact pattern: C[1] > C[2]
        PatternTemplate pattern2("Exact2");
        pattern2.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Close, 2)
        ));
        exactPatterns.push_back(pattern2);
        
        // Generate split patterns
        auto splitPatterns = gen.testGenerateSplitTemplates(exactPatterns);
        REQUIRE(splitPatterns.size() > 0);
        
        // Verify that split patterns have "Split" in their names
        bool foundSplitName = false;
        for (const auto& pattern : splitPatterns) {
            if (pattern.getName().find("Split") != std::string::npos) {
                foundSplitName = true;
                break;
            }
        }
        REQUIRE(foundSplitName);
    }

    SECTION("Delay Pattern Generation") {
        // Create a base pattern to test delay generation
        PatternTemplate basePattern("BasePattern");
        basePattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 1)
        ));
        
        // Test delay pattern creation
        auto delayedPattern = gen.testCreateDelayedTemplate(basePattern, 2);
        
        // Verify delay pattern properties
        REQUIRE(delayedPattern.getName().find("[Delay: 2]") != std::string::npos);
        REQUIRE(delayedPattern.getConditions().size() == 1);
        
        const auto& delayedCondition = delayedPattern.getConditions()[0];
        REQUIRE(delayedCondition.getLhs().getComponentType() == PriceComponentType::Close);
        REQUIRE(delayedCondition.getLhs().getBarOffset() == 2); // 0 + 2
        REQUIRE(delayedCondition.getRhs().getComponentType() == PriceComponentType::Open);
        REQUIRE(delayedCondition.getRhs().getBarOffset() == 3); // 1 + 2
    }

    SECTION("Integration Test - Full Run") {
        // Test that the full run completes without errors
        REQUIRE_NOTHROW(gen.run());
    }
}

TEST_CASE("UniverseGenerator Search Type Validation", "[search_types]")
{
    SECTION("EXTENDED search type runs successfully") {
        UniverseGenerator<concurrency::SingleThreadExecutor> gen("test_extended.bin", 3, 2, "EXTENDED");
        REQUIRE_NOTHROW(gen.run());
    }
    
    SECTION("DEEP search type runs successfully") {
        UniverseGenerator<concurrency::SingleThreadExecutor> gen("test_deep.bin", 3, 2, "DEEP");
        REQUIRE_NOTHROW(gen.run());
    }
    
    SECTION("Unsupported search type throws exception") {
        UniverseGenerator<concurrency::SingleThreadExecutor> gen("test_invalid.bin", 3, 2, "INVALID");
        REQUIRE_THROWS_AS(gen.run(), std::runtime_error);
    }
}
