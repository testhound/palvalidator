#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <set>

// Project headers
#include "PriceComponentDescriptor.h"
#include "PatternCondition.h"
#include "PatternTemplate.h"

using namespace Catch;

// =============================================================================
// PriceComponentDescriptor Tests
// =============================================================================

TEST_CASE("PriceComponentDescriptor - Basic Construction and Access", "[PriceComponentDescriptor]") {
    SECTION("Valid construction with all component types") {
        PriceComponentDescriptor open_desc(PriceComponentType::Open, 0);
        REQUIRE(open_desc.getComponentType() == PriceComponentType::Open);
        REQUIRE(open_desc.getBarOffset() == 0);
        
        PriceComponentDescriptor high_desc(PriceComponentType::High, 5);
        REQUIRE(high_desc.getComponentType() == PriceComponentType::High);
        REQUIRE(high_desc.getBarOffset() == 5);
        
        PriceComponentDescriptor low_desc(PriceComponentType::Low, 12);
        REQUIRE(low_desc.getComponentType() == PriceComponentType::Low);
        REQUIRE(low_desc.getBarOffset() == 12);
        
        PriceComponentDescriptor close_desc(PriceComponentType::Close, 255);
        REQUIRE(close_desc.getComponentType() == PriceComponentType::Close);
        REQUIRE(close_desc.getBarOffset() == 255);
    }
}

TEST_CASE("PriceComponentDescriptor - Edge Cases", "[PriceComponentDescriptor]") {
    SECTION("Maximum bar offset (uint8_t boundary)") {
        PriceComponentDescriptor max_offset(PriceComponentType::High, 255);
        REQUIRE(max_offset.getBarOffset() == 255);
        REQUIRE(max_offset.getComponentType() == PriceComponentType::High);
    }
    
    SECTION("Zero bar offset") {
        PriceComponentDescriptor zero_offset(PriceComponentType::Close, 0);
        REQUIRE(zero_offset.getBarOffset() == 0);
        REQUIRE(zero_offset.getComponentType() == PriceComponentType::Close);
    }
    
    SECTION("All component types with various offsets") {
        std::vector<PriceComponentType> types = {
            PriceComponentType::Open,
            PriceComponentType::High,
            PriceComponentType::Low,
            PriceComponentType::Close
        };
        
        std::vector<uint8_t> offsets = {0, 1, 5, 10, 50, 100, 255};
        
        for (auto type : types) {
            for (auto offset : offsets) {
                PriceComponentDescriptor desc(type, offset);
                REQUIRE(desc.getComponentType() == type);
                REQUIRE(desc.getBarOffset() == offset);
            }
        }
    }
}

TEST_CASE("PriceComponentDescriptor - Copy Construction", "[PriceComponentDescriptor]") {
    SECTION("Copy constructor preserves data") {
        PriceComponentDescriptor original(PriceComponentType::High, 42);
        PriceComponentDescriptor copy(original);
        
        REQUIRE(copy.getComponentType() == original.getComponentType());
        REQUIRE(copy.getBarOffset() == original.getBarOffset());
        REQUIRE(copy.getComponentType() == PriceComponentType::High);
        REQUIRE(copy.getBarOffset() == 42);
    }
}

TEST_CASE("PriceComponentDescriptor - Equality Operators", "[PriceComponentDescriptor]") {
    PriceComponentDescriptor desc1(PriceComponentType::High, 10);
    PriceComponentDescriptor desc2(PriceComponentType::High, 10);
    PriceComponentDescriptor desc3(PriceComponentType::Low, 10);
    PriceComponentDescriptor desc4(PriceComponentType::High, 12);

    SECTION("Equality") {
        REQUIRE(desc1 == desc2);
        REQUIRE_FALSE(desc1 == desc3); // Different type
        REQUIRE_FALSE(desc1 == desc4); // Different offset
    }

    SECTION("Inequality") {
        REQUIRE(desc1 != desc3);
        REQUIRE(desc1 != desc4);
        REQUIRE_FALSE(desc1 != desc2);
    }
}


// =============================================================================
// PatternCondition Tests
// =============================================================================

TEST_CASE("PatternCondition - Basic Construction", "[PatternCondition]") {
    SECTION("Valid condition construction") {
        PriceComponentDescriptor lhs(PriceComponentType::High, 0);
        PriceComponentDescriptor rhs(PriceComponentType::Low, 3);
        
        PatternCondition condition(lhs, ComparisonOperator::GreaterThan, rhs);
        
        REQUIRE(condition.getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(condition.getLhs().getBarOffset() == 0);
        REQUIRE(condition.getOperator() == ComparisonOperator::GreaterThan);
        REQUIRE(condition.getRhs().getComponentType() == PriceComponentType::Low);
        REQUIRE(condition.getRhs().getBarOffset() == 3);
    }
    
    SECTION("Construction with same component types, different offsets") {
        PriceComponentDescriptor lhs(PriceComponentType::High, 1);
        PriceComponentDescriptor rhs(PriceComponentType::High, 4);
        
        PatternCondition condition(lhs, ComparisonOperator::GreaterThan, rhs);
        
        REQUIRE(condition.getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(condition.getLhs().getBarOffset() == 1);
        REQUIRE(condition.getRhs().getComponentType() == PriceComponentType::High);
        REQUIRE(condition.getRhs().getBarOffset() == 4);
    }
}

TEST_CASE("PatternCondition - Logical Conditions", "[PatternCondition]") {
    SECTION("Different component type combinations") {
        // H[0] > L[1]
        PatternCondition high_vs_low(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 1)
        );
        
        // C[2] > O[5]  
        PatternCondition close_vs_open(
            PriceComponentDescriptor(PriceComponentType::Close, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 5)
        );
        
        // Same component type, different offsets: H[1] > H[4]
        PatternCondition high_vs_high(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 4)
        );
        
        // Verify all conditions constructed correctly
        REQUIRE(high_vs_low.getOperator() == ComparisonOperator::GreaterThan);
        REQUIRE(close_vs_open.getOperator() == ComparisonOperator::GreaterThan);
        REQUIRE(high_vs_high.getOperator() == ComparisonOperator::GreaterThan);
        
        // Verify component details
        REQUIRE(high_vs_low.getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(high_vs_low.getRhs().getComponentType() == PriceComponentType::Low);
        REQUIRE(close_vs_open.getLhs().getComponentType() == PriceComponentType::Close);
        REQUIRE(close_vs_open.getRhs().getComponentType() == PriceComponentType::Open);
    }
}

TEST_CASE("PatternCondition - Sparse Pattern Support", "[PatternCondition]") {
    SECTION("Non-consecutive bar relationships") {
        // H[4] > H[11] - classic sparse pattern
        PatternCondition sparse_condition(
            PriceComponentDescriptor(PriceComponentType::High, 4),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 11)
        );
        
        REQUIRE(sparse_condition.getLhs().getBarOffset() == 4);
        REQUIRE(sparse_condition.getRhs().getBarOffset() == 11);
        REQUIRE(sparse_condition.getOperator() == ComparisonOperator::GreaterThan);
        REQUIRE(sparse_condition.getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(sparse_condition.getRhs().getComponentType() == PriceComponentType::High);
    }
    
    SECTION("Maximum sparse pattern (0 to 255)") {
        PatternCondition max_sparse(
            PriceComponentDescriptor(PriceComponentType::Low, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 255)
        );
        
        REQUIRE(max_sparse.getLhs().getBarOffset() == 0);
        REQUIRE(max_sparse.getRhs().getBarOffset() == 255);
    }
}

TEST_CASE("PatternCondition - Copy Construction", "[PatternCondition]") {
    SECTION("Copy constructor preserves all data") {
        PriceComponentDescriptor lhs(PriceComponentType::Close, 7);
        PriceComponentDescriptor rhs(PriceComponentType::Open, 12);
        PatternCondition original(lhs, ComparisonOperator::GreaterThan, rhs);
        
        PatternCondition copy(original);
        
        REQUIRE(copy.getLhs().getComponentType() == original.getLhs().getComponentType());
        REQUIRE(copy.getLhs().getBarOffset() == original.getLhs().getBarOffset());
        REQUIRE(copy.getOperator() == original.getOperator());
        REQUIRE(copy.getRhs().getComponentType() == original.getRhs().getComponentType());
        REQUIRE(copy.getRhs().getBarOffset() == original.getRhs().getBarOffset());
    }
}

TEST_CASE("PatternCondition - Equality Operators", "[PatternCondition]") {
    PatternCondition cond1(
        PriceComponentDescriptor(PriceComponentType::High, 1),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Low, 2)
    );
    PatternCondition cond2(
        PriceComponentDescriptor(PriceComponentType::High, 1),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Low, 2)
    );
    PatternCondition cond3_diff_lhs(
        PriceComponentDescriptor(PriceComponentType::Open, 1), // Different LHS
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Low, 2)
    );
    PatternCondition cond4_diff_rhs(
        PriceComponentDescriptor(PriceComponentType::High, 1),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Close, 2) // Different RHS
    );

    SECTION("Equality") {
        REQUIRE(cond1 == cond2);
        REQUIRE_FALSE(cond1 == cond3_diff_lhs);
        REQUIRE_FALSE(cond1 == cond4_diff_rhs);
    }

    SECTION("Inequality") {
        REQUIRE(cond1 != cond3_diff_lhs);
        REQUIRE(cond1 != cond4_diff_rhs);
        REQUIRE_FALSE(cond1 != cond2);
    }
}

// =============================================================================
// PatternTemplate Tests
// =============================================================================

TEST_CASE("PatternTemplate - Basic Construction", "[PatternTemplate]") {
    SECTION("Empty template construction") {
        PatternTemplate template1("TestPattern1");
        
        REQUIRE(template1.getName() == "TestPattern1");
        REQUIRE(template1.getConditions().empty());
        REQUIRE(template1.getMaxBarOffset() == 0);
        REQUIRE(template1.getNumUniqueComponents() == 0);
    }
    
    SECTION("Template with descriptive names") {
        PatternTemplate template2("H[0]>L[3]_AND_C[1]>O[5]");
        REQUIRE(template2.getName() == "H[0]>L[3]_AND_C[1]>O[5]");
        REQUIRE(template2.getConditions().empty());
        REQUIRE(template2.getMaxBarOffset() == 0);
    }
    
    SECTION("Template with empty name") {
        PatternTemplate empty_name("");
        REQUIRE(empty_name.getName() == "");
        REQUIRE(empty_name.getConditions().empty());
    }
}

TEST_CASE("PatternTemplate - Equality Operators", "[PatternTemplate]") {
    PatternCondition condA(
        PriceComponentDescriptor(PriceComponentType::High, 0),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Low, 1)
    );
    PatternCondition condB(
        PriceComponentDescriptor(PriceComponentType::Close, 2),
        ComparisonOperator::GreaterThan,
        PriceComponentDescriptor(PriceComponentType::Open, 3)
    );

    SECTION("Identical templates are equal") {
        PatternTemplate p1("MyPattern");
        p1.addCondition(condA);
        p1.addCondition(condB);

        PatternTemplate p2("MyPattern");
        p2.addCondition(condA);
        p2.addCondition(condB);

        REQUIRE(p1 == p2);
        REQUIRE_FALSE(p1 != p2);
    }

    SECTION("Templates with same conditions in different order are equal") {
        PatternTemplate p1("MyPattern");
        p1.addCondition(condA);
        p1.addCondition(condB);

        PatternTemplate p2_shuffled("MyPattern");
        p2_shuffled.addCondition(condB); // Order is swapped
        p2_shuffled.addCondition(condA);

        REQUIRE(p1 == p2_shuffled);
        REQUIRE_FALSE(p1 != p2_shuffled);
    }

    SECTION("Templates with different names are not equal") {
        PatternTemplate p1("MyPattern");
        p1.addCondition(condA);

        PatternTemplate p2_diff_name("AnotherPattern"); // Different name
        p2_diff_name.addCondition(condA);

        REQUIRE(p1 != p2_diff_name);
        REQUIRE_FALSE(p1 == p2_diff_name);
    }

    SECTION("Templates with different conditions are not equal") {
        PatternTemplate p1("MyPattern");
        p1.addCondition(condA);

        PatternTemplate p2_diff_cond("MyPattern");
        p2_diff_cond.addCondition(condB); // Different condition

        REQUIRE(p1 != p2_diff_cond);
        REQUIRE_FALSE(p1 == p2_diff_cond);
    }

    SECTION("Templates with a different number of conditions are not equal") {
        PatternTemplate p1("MyPattern");
        p1.addCondition(condA);
        p1.addCondition(condB);

        PatternTemplate p2_less_conds("MyPattern");
        p2_less_conds.addCondition(condA); // Has fewer conditions

        REQUIRE(p1 != p2_less_conds);
        REQUIRE_FALSE(p1 == p2_less_conds);
    }

    SECTION("Two empty templates with the same name are equal") {
        PatternTemplate p1("Empty");
        PatternTemplate p2("Empty");
        REQUIRE(p1 == p2);
    }
}

TEST_CASE("PatternTemplate - Adding Conditions", "[PatternTemplate]") {
    SECTION("Single condition addition") {
        PatternTemplate pattern("SingleCondition");
        
        PatternCondition condition(
            PriceComponentDescriptor(PriceComponentType::High, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 5)
        );
        
        pattern.addCondition(condition);
        
        REQUIRE(pattern.getConditions().size() == 1);
        REQUIRE(pattern.getMaxBarOffset() == 5);
        REQUIRE(pattern.getNumUniqueComponents() == 2);
        
        // Verify the condition was stored correctly
        const auto& stored_condition = pattern.getConditions()[0];
        REQUIRE(stored_condition.getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(stored_condition.getLhs().getBarOffset() == 2);
        REQUIRE(stored_condition.getRhs().getComponentType() == PriceComponentType::Low);
        REQUIRE(stored_condition.getRhs().getBarOffset() == 5);
    }
    
    SECTION("Multiple condition addition") {
        PatternTemplate pattern("MultipleConditions");
        
        // Add H[0] > L[3]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 3)
        ));
        
        // Add C[1] > O[7]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 7)
        ));
        
        REQUIRE(pattern.getConditions().size() == 2);
        REQUIRE(pattern.getMaxBarOffset() == 7);
        REQUIRE(pattern.getNumUniqueComponents() == 4);
        
        // Verify both conditions are stored correctly
        const auto& conditions = pattern.getConditions();
        REQUIRE(conditions[0].getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(conditions[1].getLhs().getComponentType() == PriceComponentType::Close);
    }
    
    SECTION("Sequential condition addition") {
        PatternTemplate pattern("SequentialTest");
        
        // Add conditions one by one and verify metadata updates
        REQUIRE(pattern.getMaxBarOffset() == 0);
        REQUIRE(pattern.getNumUniqueComponents() == 0);
        
        // First condition: H[2] > L[4]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 4)
        ));
        
        REQUIRE(pattern.getMaxBarOffset() == 4);
        REQUIRE(pattern.getNumUniqueComponents() == 2);
        
        // Second condition: C[6] > O[1]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 6),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 1)
        ));
        
        REQUIRE(pattern.getMaxBarOffset() == 6);
        REQUIRE(pattern.getNumUniqueComponents() == 4);
    }
}

TEST_CASE("PatternTemplate - Metadata Calculation", "[PatternTemplate]") {
    SECTION("Max bar offset calculation") {
        PatternTemplate pattern("MaxOffsetTest");
        
        // Add conditions with various offsets
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 8)
        ));
        
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 12),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 4)
        ));
        
        REQUIRE(pattern.getMaxBarOffset() == 12);
        
        // Add condition with even higher offset
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 20),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 15)
        ));
        
        REQUIRE(pattern.getMaxBarOffset() == 20);
    }
    
    SECTION("Unique component counting") {
        PatternTemplate pattern("UniqueComponentTest");
        
        // Add H[1] > L[3]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 3)
        ));
        
        REQUIRE(pattern.getNumUniqueComponents() == 2);
        
        // Add H[1] > C[5] (reuses H[1])
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Close, 5)
        ));
        
        // Should have 3 unique components: H[1], L[3], C[5]
        REQUIRE(pattern.getNumUniqueComponents() == 3);
        
        // Add L[3] > O[2] (reuses L[3])
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Low, 3),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 2)
        ));
        
        // Should have 4 unique components: H[1], L[3], C[5], O[2]
        REQUIRE(pattern.getNumUniqueComponents() == 4);
    }
    
    SECTION("Unique component counting with same type, different offsets") {
        PatternTemplate pattern("SameTypeDifferentOffsets");
        
        // Add H[0] > H[1]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 1)
        ));
        
        // Add H[1] > H[2]
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 2)
        ));
        
        // Should have 3 unique components: H[0], H[1], H[2]
        REQUIRE(pattern.getNumUniqueComponents() == 3);
    }
}

TEST_CASE("PatternTemplate - Complex Patterns", "[PatternTemplate]") {
    SECTION("Dense pattern (consecutive bars)") {
        PatternTemplate dense_pattern("DensePattern_3Bars");
        
        // H[0] > H[1] AND H[1] > H[2] AND L[0] > L[2]
        dense_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 1)
        ));
        
        dense_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 2)
        ));
        
        dense_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Low, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 2)
        ));
        
        REQUIRE(dense_pattern.getConditions().size() == 3);
        REQUIRE(dense_pattern.getMaxBarOffset() == 2);
        REQUIRE(dense_pattern.getNumUniqueComponents() == 5); // H[0], H[1], H[2], L[0], L[2]
    }
    
    SECTION("Sparse pattern (non-consecutive bars)") {
        PatternTemplate sparse_pattern("SparsePattern_H4_H11");
        
        // H[4] > H[11] AND L[2] > L[9]
        sparse_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 4),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 11)
        ));
        
        sparse_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Low, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 9)
        ));
        
        REQUIRE(sparse_pattern.getConditions().size() == 2);
        REQUIRE(sparse_pattern.getMaxBarOffset() == 11);
        REQUIRE(sparse_pattern.getNumUniqueComponents() == 4); // H[4], H[11], L[2], L[9]
    }
    
    SECTION("Mixed dense and sparse pattern") {
        PatternTemplate mixed_pattern("MixedPattern");
        
        // Dense: H[0] > H[1]
        mixed_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 1)
        ));
        
        // Sparse: L[3] > L[8]
        mixed_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Low, 3),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 8)
        ));
        
        // Cross-component: C[2] > O[6]
        mixed_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 6)
        ));
        
        REQUIRE(mixed_pattern.getConditions().size() == 3);
        REQUIRE(mixed_pattern.getMaxBarOffset() == 8);
        REQUIRE(mixed_pattern.getNumUniqueComponents() == 6); // H[0], H[1], L[3], L[8], C[2], O[6]
    }
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_CASE("Integration - Cross-Component Functionality", "[Integration]") {
    SECTION("Complete pattern construction workflow") {
        // Simulate the workflow from design document
        PatternTemplate pattern("IntegrationTest_Workflow");
        
        // Step 1: Create price components
        PriceComponentDescriptor h0(PriceComponentType::High, 0);
        PriceComponentDescriptor l3(PriceComponentType::Low, 3);
        PriceComponentDescriptor c1(PriceComponentType::Close, 1);
        PriceComponentDescriptor o5(PriceComponentType::Open, 5);
        
        // Step 2: Create conditions
        PatternCondition cond1(h0, ComparisonOperator::GreaterThan, l3);
        PatternCondition cond2(c1, ComparisonOperator::GreaterThan, o5);
        
        // Step 3: Build pattern
        pattern.addCondition(cond1);
        pattern.addCondition(cond2);
        
        // Step 4: Verify complete pattern
        REQUIRE(pattern.getConditions().size() == 2);
        REQUIRE(pattern.getMaxBarOffset() == 5);
        REQUIRE(pattern.getNumUniqueComponents() == 4);
        
        // Verify condition integrity
        const auto& conditions = pattern.getConditions();
        REQUIRE(conditions[0].getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(conditions[0].getRhs().getComponentType() == PriceComponentType::Low);
        REQUIRE(conditions[1].getLhs().getComponentType() == PriceComponentType::Close);
        REQUIRE(conditions[1].getRhs().getComponentType() == PriceComponentType::Open);
    }
}

TEST_CASE("Integration - Serialization Readiness", "[Integration]") {
    SECTION("Pattern template serialization data access") {
        PatternTemplate pattern("SerializationTest");
        
        // Add multiple conditions to test data access
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 4)
        ));
        
        pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 6)
        ));
        
        // Verify all data accessible via public getters (required for Task 1.2)
        REQUIRE_FALSE(pattern.getName().empty());
        REQUIRE_FALSE(pattern.getConditions().empty());
        REQUIRE(pattern.getMaxBarOffset() > 0);
        REQUIRE(pattern.getNumUniqueComponents() > 0);
        
        // Verify condition data accessible
        for (const auto& condition : pattern.getConditions()) {
            REQUIRE(condition.getLhs().getBarOffset() <= pattern.getMaxBarOffset());
            REQUIRE(condition.getRhs().getBarOffset() <= pattern.getMaxBarOffset());
            REQUIRE(condition.getOperator() == ComparisonOperator::GreaterThan);
        }
    }
}

// =============================================================================
// Edge Cases and Error Conditions
// =============================================================================

TEST_CASE("Edge Cases - Boundary Values", "[EdgeCases]") {
    SECTION("Maximum values") {
        // Test uint8_t maximum for bar offsets
        PriceComponentDescriptor max_desc(PriceComponentType::High, 255);
        REQUIRE(max_desc.getBarOffset() == 255);
        
        PatternTemplate max_pattern("MaxBoundaryTest");
        max_pattern.addCondition(PatternCondition(
            max_desc,
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 254)
        ));
        
        REQUIRE(max_pattern.getMaxBarOffset() == 255);
        REQUIRE(max_pattern.getNumUniqueComponents() == 2);
    }
    
    SECTION("Empty pattern edge cases") {
        PatternTemplate empty_pattern("EmptyPattern");
        
        REQUIRE(empty_pattern.getConditions().empty());
        REQUIRE(empty_pattern.getMaxBarOffset() == 0);
        REQUIRE(empty_pattern.getNumUniqueComponents() == 0);
        REQUIRE(empty_pattern.getName() == "EmptyPattern");
    }
    
    SECTION("Single condition with zero offsets") {
        PatternTemplate zero_pattern("ZeroOffsetPattern");
        
        zero_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 0)
        ));
        
        REQUIRE(zero_pattern.getMaxBarOffset() == 0);
        REQUIRE(zero_pattern.getNumUniqueComponents() == 2); // H[0] and L[0] are different
    }
}

TEST_CASE("Edge Cases - Large Patterns", "[EdgeCases]") {
    SECTION("Pattern with maximum conditions (8 conditions as per design)") {
        PatternTemplate large_pattern("MaxConditionsTest");
        
        // Add 8 conditions (design document maximum)
        for (int i = 0; i < 8; ++i) {
            large_pattern.addCondition(PatternCondition(
                PriceComponentDescriptor(PriceComponentType::High, static_cast<uint8_t>(i)),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Low, static_cast<uint8_t>(i + 1))
            ));
        }
        
        REQUIRE(large_pattern.getConditions().size() == 8);
        REQUIRE(large_pattern.getMaxBarOffset() == 8);
        REQUIRE(large_pattern.getNumUniqueComponents() == 16); // 8 H[i] + 8 L[i+1]
    }
    
    SECTION("Pattern with many repeated components") {
        PatternTemplate repeated_pattern("RepeatedComponentsTest");
        
        PriceComponentDescriptor h0(PriceComponentType::High, 0);
        PriceComponentDescriptor l1(PriceComponentType::Low, 1);
        
        // Add multiple conditions using the same components
        for (int i = 0; i < 5; ++i) {
            repeated_pattern.addCondition(PatternCondition(h0, ComparisonOperator::GreaterThan, l1));
        }
        
        REQUIRE(repeated_pattern.getConditions().size() == 5);
        REQUIRE(repeated_pattern.getMaxBarOffset() == 1);
        REQUIRE(repeated_pattern.getNumUniqueComponents() == 2); // Only H[0] and L[1]
    }
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_CASE("Performance - Large Pattern Construction", "[Performance]") {
    SECTION("Construction time for complex patterns") {
        auto start = std::chrono::high_resolution_clock::now();
        
        PatternTemplate perf_pattern("PerformanceTest");
        for (int i = 0; i < 100; ++i) {
            perf_pattern.addCondition(PatternCondition(
                PriceComponentDescriptor(PriceComponentType::High, static_cast<uint8_t>(i % 12)),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Low, static_cast<uint8_t>((i + 1) % 12))
            ));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should complete within reasonable time
        REQUIRE(duration.count() < 100); // Less than 100ms
        REQUIRE(perf_pattern.getConditions().size() == 100);
        
        // Verify metadata is still calculated correctly
        REQUIRE(perf_pattern.getMaxBarOffset() == 11);
        REQUIRE(perf_pattern.getNumUniqueComponents() == 24); // H[0-11] and L[0-11] = 24 unique components
    }
    
    SECTION("Metadata calculation performance") {
        PatternTemplate metadata_pattern("MetadataPerformanceTest");
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Add conditions that will require metadata recalculation each time
        for (int i = 0; i < 50; ++i) {
            metadata_pattern.addCondition(PatternCondition(
                PriceComponentDescriptor(PriceComponentType::High, static_cast<uint8_t>(i)),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Low, static_cast<uint8_t>(i + 50))
            ));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should complete within reasonable time
        REQUIRE(duration.count() < 50); // Less than 50ms
        REQUIRE(metadata_pattern.getMaxBarOffset() == 99);
        REQUIRE(metadata_pattern.getNumUniqueComponents() == 100);
    }
}

// =============================================================================
// Comprehensive Integration Tests
// =============================================================================

TEST_CASE("Comprehensive Integration - Real-world Pattern Examples", "[Integration]") {
    SECTION("Price Action Lab style pattern: H[0] > H[1] AND L[2] > L[5]") {
        PatternTemplate pal_pattern("PAL_H0_H1_L2_L5");
        
        // Add H[0] > H[1]
        pal_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 1)
        ));
        
        // Add L[2] > L[5]
        pal_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Low, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 5)
        ));
        
        REQUIRE(pal_pattern.getName() == "PAL_H0_H1_L2_L5");
        REQUIRE(pal_pattern.getConditions().size() == 2);
        REQUIRE(pal_pattern.getMaxBarOffset() == 5);
        REQUIRE(pal_pattern.getNumUniqueComponents() == 4);
        
        // Verify pattern can be traversed for evaluation
        for (const auto& condition : pal_pattern.getConditions()) {
            REQUIRE(condition.getOperator() == ComparisonOperator::GreaterThan);
            REQUIRE(condition.getLhs().getBarOffset() <= pal_pattern.getMaxBarOffset());
            REQUIRE(condition.getRhs().getBarOffset() <= pal_pattern.getMaxBarOffset());
        }
    }
    
    SECTION("Complex multi-component pattern") {
        PatternTemplate complex_pattern("Complex_OHLC_Pattern");
        
        // O[0] > C[1]
        complex_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Open, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Close, 1)
        ));
        
        // H[2] > L[3]
        complex_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 2),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 3)
        ));
        
        // C[4] > O[7]
        complex_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 4),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 7)
        ));
        
        // L[1] > H[8]
        complex_pattern.addCondition(PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Low, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::High, 8)
        ));
        
        REQUIRE(complex_pattern.getConditions().size() == 4);
        REQUIRE(complex_pattern.getMaxBarOffset() == 8);
        REQUIRE(complex_pattern.getNumUniqueComponents() == 8);
        
        // Verify all OHLC components are represented
        std::set<PriceComponentType> used_types;
        for (const auto& condition : complex_pattern.getConditions()) {
            used_types.insert(condition.getLhs().getComponentType());
            used_types.insert(condition.getRhs().getComponentType());
        }
        
        REQUIRE(used_types.size() == 4); // All OHLC types used
        REQUIRE(used_types.count(PriceComponentType::Open) == 1);
        REQUIRE(used_types.count(PriceComponentType::High) == 1);
        REQUIRE(used_types.count(PriceComponentType::Low) == 1);
        REQUIRE(used_types.count(PriceComponentType::Close) == 1);
    }
}

// =============================================================================
// Data Structure Consistency Tests
// =============================================================================

TEST_CASE("Data Structure Consistency", "[Consistency]") {
    SECTION("Pattern template maintains condition order") {
        PatternTemplate ordered_pattern("OrderTest");
        
        std::vector<PatternCondition> original_conditions;
        
        // Add conditions in specific order
        for (int i = 0; i < 5; ++i) {
            PatternCondition condition(
                PriceComponentDescriptor(PriceComponentType::High, static_cast<uint8_t>(i)),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Low, static_cast<uint8_t>(i + 1))
            );
            
            original_conditions.push_back(condition);
            ordered_pattern.addCondition(condition);
        }
        
        // Verify order is maintained
        const auto& stored_conditions = ordered_pattern.getConditions();
        REQUIRE(stored_conditions.size() == original_conditions.size());
        
        for (size_t i = 0; i < stored_conditions.size(); ++i) {
            REQUIRE(stored_conditions[i].getLhs().getBarOffset() == original_conditions[i].getLhs().getBarOffset());
            REQUIRE(stored_conditions[i].getRhs().getBarOffset() == original_conditions[i].getRhs().getBarOffset());
        }
    }
    
    SECTION("Metadata consistency after multiple additions") {
        PatternTemplate consistency_pattern("ConsistencyTest");
        
        uint8_t expected_max_offset = 0;
        size_t expected_unique_components = 0;
        std::set<std::pair<PriceComponentType, uint8_t>> unique_tracker;
        
        for (int i = 0; i < 10; ++i) {
            uint8_t lhs_offset = static_cast<uint8_t>(i * 2);
            uint8_t rhs_offset = static_cast<uint8_t>(i * 2 + 1);
            
            PriceComponentDescriptor lhs(PriceComponentType::High, lhs_offset);
            PriceComponentDescriptor rhs(PriceComponentType::Low, rhs_offset);
            
            consistency_pattern.addCondition(PatternCondition(lhs, ComparisonOperator::GreaterThan, rhs));
            
            // Update expected values
            expected_max_offset = std::max(expected_max_offset, std::max(lhs_offset, rhs_offset));
            unique_tracker.insert({lhs.getComponentType(), lhs.getBarOffset()});
            unique_tracker.insert({rhs.getComponentType(), rhs.getBarOffset()});
            expected_unique_components = unique_tracker.size();
            
            // Verify metadata is consistent
            REQUIRE(consistency_pattern.getMaxBarOffset() == expected_max_offset);
            REQUIRE(consistency_pattern.getNumUniqueComponents() == expected_unique_components);
        }
    }
}

// =============================================================================
// Future Extensibility Tests
// =============================================================================

TEST_CASE("Future Extensibility - Operator Support", "[Extensibility]") {
    SECTION("Current operator support") {
        // Verify only GreaterThan is currently supported
        PriceComponentDescriptor lhs(PriceComponentType::High, 0);
        PriceComponentDescriptor rhs(PriceComponentType::Low, 1);
        
        PatternCondition condition(lhs, ComparisonOperator::GreaterThan, rhs);
        REQUIRE(condition.getOperator() == ComparisonOperator::GreaterThan);
        
        // This test documents current limitation and will need updating
        // when new operators are added (LessThan, Equals, etc.)
    }
}

TEST_CASE("Future Extensibility - Component Type Support", "[Extensibility]") {
    SECTION("All current component types supported") {
        std::vector<PriceComponentType> all_types = {
            PriceComponentType::Open,
            PriceComponentType::High,
            PriceComponentType::Low,
            PriceComponentType::Close
        };
        
        for (auto type : all_types) {
            PriceComponentDescriptor desc(type, 0);
            REQUIRE(desc.getComponentType() == type);
        }
        
        // This test documents current component types and will need updating
        // if new types are added (Volume, etc.)
    }
}