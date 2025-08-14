#include <catch2/catch_test_macros.hpp>
#include "PatternValidationEngine.h"
#include "AnalysisDatabase.h"
#include "DataStructures.h"
#include <chrono>
#include <vector>
#include <set>

using namespace palanalyzer;

/**
 * @brief Test fixture for PatternValidationEngine tests
 */
class PatternValidationEngineTestFixture {
public:
    PatternValidationEngineTestFixture()
        : database("test_validation_database.json")
    {
        setupTestDatabase();
        validationEngine = std::make_unique<PatternValidationEngine>(database);
    }

    ~PatternValidationEngineTestFixture()
    {
        // Clean up test database file
        std::remove("test_validation_database.json");
    }

protected:
    AnalysisDatabase database;
    std::unique_ptr<PatternValidationEngine> validationEngine;

    void setupTestDatabase()
    {
        // Create test index group
        std::vector<uint8_t> barCombination = {0, 1, 2};
        std::set<PriceComponentType> componentTypes = {PriceComponentType::Close, PriceComponentType::High};
        
        database.addPatternToIndexGroup(100, barCombination, componentTypes, "test_file.pal", "Deep");
        
        // Add test pattern
        std::vector<PriceComponentDescriptor> components = {
            PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
            PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
        };
        
        PatternAnalysis testPattern(
            100,                                           // index (match the index group)
            "test_file.pal",                               // sourceFile
            12345678901234567890ULL,                       // patternHash
            components,                                    // components
            "C[0] > C[1]",                                // patternString
            false,                                         // isChained
            1,                                             // maxBarOffset
            1,                                             // barSpread
            1,                                             // conditionCount
            std::chrono::system_clock::now(),             // analyzedAt
            0.65,                                          // profitabilityLong
            0.45,                                          // profitabilityShort
            100,                                           // trades
            3                                              // consecutiveLosses
        );
        
        database.addPattern(testPattern);
    }

    PatternStructure createValidPatternStructure()
    {
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            )
        };
        
        return PatternStructure(
            12345678901234567890ULL,                       // patternHash
            100,                                           // groupId (matches index group)
            conditions,                                    // conditions
            {"CLOSE"},                                     // componentsUsed
            {0, 1}                                         // barOffsetsUsed
        );
    }

    PatternStructure createInvalidPatternStructure()
    {
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan, // Use valid operator for testing
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]") // Self-comparison
            )
        };
        
        return PatternStructure(
            0ULL,                                          // Invalid hash
            -1,                                            // Invalid groupId
            conditions,                                    // conditions
            {"INVALID_COMPONENT"},                         // Invalid components
            {-1, 300}                                      // Invalid bar offsets
        );
    }
};

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "PatternValidationEngine Construction", "[PatternValidationEngine]")
{
    SECTION("Constructor creates valid engine")
    {
        REQUIRE(validationEngine != nullptr);
        
        // Verify initial statistics
        ValidationStats stats = validationEngine->getValidationStats();
        REQUIRE(stats.getTotalValidations() == 0);
        REQUIRE(stats.getSuccessfulValidations() == 0);
        REQUIRE(stats.getFailedValidations() == 0);
    }
}

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "Pattern Existence Validation", "[PatternValidationEngine]")
{
    SECTION("Validate existing pattern returns VALID")
    {
        ValidationResult result = validationEngine->validatePatternExistence(12345678901234567890ULL);
        REQUIRE(result == ValidationResult::VALID);
        
        // Check statistics updated
        ValidationStats stats = validationEngine->getValidationStats();
        REQUIRE(stats.getTotalValidations() == 1);
        REQUIRE(stats.getSuccessfulValidations() == 1);
        REQUIRE(stats.getFailedValidations() == 0);
    }
    
    SECTION("Validate non-existing pattern returns PATTERN_NOT_FOUND")
    {
        ValidationResult result = validationEngine->validatePatternExistence(99999999999999999ULL);
        REQUIRE(result == ValidationResult::PATTERN_NOT_FOUND);
        
        // Check statistics updated
        ValidationStats stats = validationEngine->getValidationStats();
        REQUIRE(stats.getTotalValidations() == 1);
        REQUIRE(stats.getSuccessfulValidations() == 0);
        REQUIRE(stats.getFailedValidations() == 1);
    }
}

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "Pattern Structure Validation", "[PatternValidationEngine]")
{
    SECTION("Valid pattern structure returns VALID")
    {
        PatternStructure validPattern = createValidPatternStructure();
        ValidationResult result = validationEngine->validatePatternStructure(validPattern);
        REQUIRE(result == ValidationResult::VALID);
    }
    
    SECTION("Invalid pattern structure returns appropriate error")
    {
        PatternStructure invalidPattern = createInvalidPatternStructure();
        ValidationResult result = validationEngine->validatePatternStructure(invalidPattern);
        REQUIRE(result != ValidationResult::VALID);
        REQUIRE(result != ValidationResult::VALID_WITH_WARNINGS);
    }
    
    SECTION("Empty conditions returns INVALID_STRUCTURE_MALFORMED")
    {
        PatternStructure emptyPattern(
            12345ULL,                                      // patternHash
            100,                                           // groupId
            {},                                            // Empty conditions
            {"CLOSE"},                                     // componentsUsed
            {0}                                            // barOffsetsUsed
        );
        
        ValidationResult result = validationEngine->validatePatternStructure(emptyPattern);
        REQUIRE(result == ValidationResult::INVALID_STRUCTURE_MALFORMED);
    }
}

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "Pattern Group Validation", "[PatternValidationEngine]")
{
    SECTION("Valid pattern in correct group returns VALID")
    {
        ValidationResult result = validationEngine->validatePatternInGroup(12345678901234567890ULL, 100);
        REQUIRE(result == ValidationResult::VALID);
    }
    
    SECTION("Pattern in non-existing group returns GROUP_NOT_FOUND")
    {
        ValidationResult result = validationEngine->validatePatternInGroup(12345678901234567890ULL, 999);
        REQUIRE(result == ValidationResult::GROUP_NOT_FOUND);
    }
    
    SECTION("Non-existing pattern returns PATTERN_NOT_FOUND")
    {
        ValidationResult result = validationEngine->validatePatternInGroup(99999999999999999ULL, 100);
        REQUIRE(result == ValidationResult::PATTERN_NOT_FOUND);
    }
}

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "Batch Validation", "[PatternValidationEngine]")
{
    SECTION("Batch validation returns correct results")
    {
        std::vector<unsigned long long> hashes = {
            12345678901234567890ULL,  // Valid pattern
            99999999999999999ULL,     // Invalid pattern
            88888888888888888ULL      // Another invalid pattern
        };
        
        std::vector<ValidationResult> results = validationEngine->validatePatternBatch(hashes);
        
        REQUIRE(results.size() == 3);
        REQUIRE(results[0] == ValidationResult::VALID);
        REQUIRE(results[1] == ValidationResult::PATTERN_NOT_FOUND);
        REQUIRE(results[2] == ValidationResult::PATTERN_NOT_FOUND);
    }
    
    SECTION("Empty batch returns empty results")
    {
        std::vector<unsigned long long> emptyHashes;
        std::vector<ValidationResult> results = validationEngine->validatePatternBatch(emptyHashes);
        REQUIRE(results.empty());
    }
}

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "Pattern Lookup", "[PatternValidationEngine]")
{
    SECTION("Find existing pattern by hash")
    {
        auto pattern = validationEngine->findPatternByHash(12345678901234567890ULL);
        REQUIRE(pattern.has_value());
        REQUIRE(pattern->getPatternHash() == 12345678901234567890ULL);
        REQUIRE(pattern->getGroupId() == 100);
    }
    
    SECTION("Find non-existing pattern returns nullopt")
    {
        auto pattern = validationEngine->findPatternByHash(99999999999999999ULL);
        REQUIRE_FALSE(pattern.has_value());
    }
    
    SECTION("Find patterns in group")
    {
        std::vector<PatternStructure> patterns = validationEngine->findPatternsInGroup(100);
        REQUIRE_FALSE(patterns.empty());
        
        // Verify all patterns belong to the correct group
        for (const auto& pattern : patterns)
        {
            REQUIRE(pattern.getGroupId() == 100);
        }
    }
    
    SECTION("Find patterns in non-existing group returns empty vector")
    {
        std::vector<PatternStructure> patterns = validationEngine->findPatternsInGroup(999);
        REQUIRE(patterns.empty());
    }
}

TEST_CASE_METHOD(PatternValidationEngineTestFixture, "Validation Statistics", "[PatternValidationEngine]")
{
    SECTION("Statistics track validation operations correctly")
    {
        // Perform several validations
        validationEngine->validatePatternExistence(12345678901234567890ULL); // Valid
        validationEngine->validatePatternExistence(99999999999999999ULL);     // Invalid
        validationEngine->validatePatternExistence(88888888888888888ULL);     // Invalid
        
        ValidationStats stats = validationEngine->getValidationStats();
        REQUIRE(stats.getTotalValidations() == 3);
        REQUIRE(stats.getSuccessfulValidations() == 1);
        REQUIRE(stats.getFailedValidations() == 2);
        
        // Check result breakdown
        const auto& breakdown = stats.getResultBreakdown();
        REQUIRE(breakdown.at(ValidationResult::VALID) == 1);
        REQUIRE(breakdown.at(ValidationResult::PATTERN_NOT_FOUND) == 2);
    }
    
    SECTION("Reset statistics clears all counters")
    {
        // Perform validation to generate stats
        validationEngine->validatePatternExistence(12345678901234567890ULL);
        
        // Reset and verify
        validationEngine->resetValidationStats();
        ValidationStats stats = validationEngine->getValidationStats();
        
        REQUIRE(stats.getTotalValidations() == 0);
        REQUIRE(stats.getSuccessfulValidations() == 0);
        REQUIRE(stats.getFailedValidations() == 0);
        REQUIRE(stats.getResultBreakdown().empty());
    }
}

TEST_CASE("ValidationResult String Conversion", "[PatternValidationEngine]")
{
    SECTION("Valid results have correct string representations")
    {
        REQUIRE(PatternValidationEngine::validationResultToString(ValidationResult::VALID) == "Valid");
        REQUIRE(PatternValidationEngine::validationResultToString(ValidationResult::PATTERN_NOT_FOUND) == "Pattern not found");
        REQUIRE(PatternValidationEngine::validationResultToString(ValidationResult::INVALID_STRUCTURE_MALFORMED) == "Invalid structure: malformed");
    }
    
    SECTION("Error messages provide helpful guidance")
    {
        std::string errorMsg = PatternValidationEngine::getValidationErrorMessage(ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE);
        REQUIRE_FALSE(errorMsg.empty());
        REQUIRE(errorMsg.find("component") != std::string::npos);
        
        std::string successMsg = PatternValidationEngine::getValidationErrorMessage(ValidationResult::VALID);
        REQUIRE(successMsg.find("successful") != std::string::npos);
    }
}

TEST_CASE("Component Validation", "[PatternValidationEngine]")
{
    SECTION("Valid components pass validation")
    {
        AnalysisDatabase testDb("temp_test.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<std::string> validComponents = {"OPEN", "HIGH", "LOW", "CLOSE"};
        // Access private method through public interface by creating a pattern
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::High, 1, "H[1]")
            )
        };
        
        PatternStructure testPattern(
            123ULL,
            1,
            conditions,
            validComponents,
            {0, 1}
        );
        
        ValidationResult result = engine.validatePatternStructure(testPattern);
        // Should not fail due to component validation
        REQUIRE(result != ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE);
        
        std::remove("temp_test.json");
    }
    
    SECTION("Invalid components fail validation")
    {
        AnalysisDatabase testDb("temp_test2.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<std::string> invalidComponents = {"INVALID_COMPONENT"};
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            )
        };
        
        PatternStructure testPattern(
            123ULL,
            1,
            conditions,
            invalidComponents,
            {0, 1}
        );
        
        ValidationResult result = engine.validatePatternStructure(testPattern);
        REQUIRE(result == ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE);
        
        std::remove("temp_test2.json");
    }
}

TEST_CASE("Bar Offset Validation", "[PatternValidationEngine]")
{
    SECTION("Valid bar offsets pass validation")
    {
        AnalysisDatabase testDb("temp_test3.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 5, "C[5]")
            )
        };
        
        PatternStructure testPattern(
            123ULL,
            1,
            conditions,
            {"CLOSE"},
            {0, 5}  // Valid offsets
        );
        
        ValidationResult result = engine.validatePatternStructure(testPattern);
        REQUIRE(result != ValidationResult::INVALID_COMPONENTS_INVALID_OFFSET);
        
        std::remove("temp_test3.json");
    }
    
    SECTION("Invalid bar offsets fail validation")
    {
        AnalysisDatabase testDb("temp_test4.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            )
        };
        
        PatternStructure testPattern(
            123ULL,
            1,
            conditions,
            {"CLOSE"},
            {-1, 300}  // Invalid offsets (negative and too large)
        );
        
        ValidationResult result = engine.validatePatternStructure(testPattern);
        REQUIRE(result == ValidationResult::INVALID_COMPONENTS_INVALID_OFFSET);
        
        std::remove("temp_test4.json");
    }
}

TEST_CASE("Circular Reference Detection", "[PatternValidationEngine]")
{
    SECTION("No circular references in simple pattern")
    {
        AnalysisDatabase testDb("temp_test5.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            ),
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 2, "C[2]")
            )
        };
        
        PatternStructure testPattern(
            123ULL,
            1,
            conditions,
            {"CLOSE"},
            {0, 1, 2}
        );
        
        ValidationResult result = engine.validatePatternStructure(testPattern);
        REQUIRE(result != ValidationResult::INVALID_CONDITIONS_CIRCULAR_REFERENCE);
        
        std::remove("temp_test5.json");
    }
}

TEST_CASE("Self-Comparison Detection", "[PatternValidationEngine]")
{
    SECTION("Self-comparison conditions are invalid")
    {
        AnalysisDatabase testDb("temp_test6.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]") // Self-comparison
            )
        };
        
        PatternStructure testPattern(
            123ULL,
            1,
            conditions,
            {"CLOSE"},
            {0}
        );
        
        ValidationResult result = engine.validatePatternStructure(testPattern);
        REQUIRE(result == ValidationResult::INVALID_CONDITIONS_LOGICAL_ERROR);
        
        std::remove("temp_test6.json");
    }
}

TEST_CASE("Validation Result Messages", "[PatternValidationEngine]")
{
    SECTION("All validation results have string representations")
    {
        // Test a few key validation results
        std::vector<ValidationResult> results = {
            ValidationResult::VALID,
            ValidationResult::PATTERN_NOT_FOUND,
            ValidationResult::INVALID_STRUCTURE_MALFORMED,
            ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE,
            ValidationResult::INVALID_CONDITIONS_LOGICAL_ERROR
        };
        
        for (ValidationResult result : results)
        {
            std::string resultStr = PatternValidationEngine::validationResultToString(result);
            REQUIRE_FALSE(resultStr.empty());
            REQUIRE(resultStr != "Unknown validation result");
            
            std::string errorMsg = PatternValidationEngine::getValidationErrorMessage(result);
            REQUIRE_FALSE(errorMsg.empty());
        }
    }
}

TEST_CASE("Pattern Structure Consistency", "[PatternValidationEngine]")
{
    SECTION("Condition count mismatch detected")
    {
        AnalysisDatabase testDb("temp_test7.json");
        PatternValidationEngine engine(testDb);
        
        std::vector<PatternCondition> conditions = {
            PatternCondition(
                PriceComponentDescriptor(PriceComponentType::Close, 0, "C[0]"),
                ComparisonOperator::GreaterThan,
                PriceComponentDescriptor(PriceComponentType::Close, 1, "C[1]")
            )
        };
        
        PatternStructure inconsistentPattern(
            123ULL,
            1,
            conditions,
            {"CLOSE"},
            {0, 1}
        );
        
        ValidationResult result = engine.validatePatternStructure(inconsistentPattern);
        REQUIRE(result == ValidationResult::INVALID_STRUCTURE_MALFORMED);
        
        std::remove("temp_test7.json");
    }
}