#pragma once

#include "DataStructures.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>
#include <map>

namespace palanalyzer {

// Forward declaration
class AnalysisDatabase;

/**
 * @brief Enumeration of validation results for pattern validation operations
 */
enum class ValidationResult {
    // Success cases
    VALID,
    VALID_WITH_WARNINGS,
    
    // Hash-related failures
    INVALID_HASH_MISMATCH,
    INVALID_HASH_COLLISION,
    INVALID_HASH_FORMAT,
    
    // Structure failures
    INVALID_STRUCTURE_EMPTY_CONDITIONS,
    INVALID_STRUCTURE_TOO_MANY_CONDITIONS,
    INVALID_STRUCTURE_MALFORMED,
    
    // Component failures
    INVALID_COMPONENTS_UNKNOWN_TYPE,
    INVALID_COMPONENTS_INVALID_OFFSET,
    INVALID_COMPONENTS_MISSING_REQUIRED,
    
    // Condition failures
    INVALID_CONDITIONS_LOGICAL_ERROR,
    INVALID_CONDITIONS_CIRCULAR_REFERENCE,
    INVALID_CONDITIONS_UNSUPPORTED_OPERATOR,
    
    // Lookup failures
    PATTERN_NOT_FOUND,
    GROUP_NOT_FOUND,
    DATABASE_ERROR,
    
    // Performance warnings
    WARNING_COMPLEX_PATTERN,
    WARNING_RARE_COMPONENTS,
    WARNING_DEEP_NESTING
};

/**
 * @brief Statistics for pattern validation operations
 */
class ValidationStats {
public:
    ValidationStats()
        : m_totalValidations(0),
          m_successfulValidations(0),
          m_failedValidations(0) {}

    size_t getTotalValidations() const
    {
        return m_totalValidations;
    }

    size_t getSuccessfulValidations() const
    {
        return m_successfulValidations;
    }

    size_t getFailedValidations() const
    {
        return m_failedValidations;
    }

    const std::map<ValidationResult, size_t>& getResultBreakdown() const
    {
        return m_resultBreakdown;
    }

    void recordValidation(ValidationResult result)
    {
        m_totalValidations++;
        m_resultBreakdown[result]++;
        
        if (result == ValidationResult::VALID || result == ValidationResult::VALID_WITH_WARNINGS)
        {
            m_successfulValidations++;
        }
        else
        {
            m_failedValidations++;
        }
    }

    void reset()
    {
        m_totalValidations = 0;
        m_successfulValidations = 0;
        m_failedValidations = 0;
        m_resultBreakdown.clear();
    }

private:
    size_t m_totalValidations;
    size_t m_successfulValidations;
    size_t m_failedValidations;
    std::map<ValidationResult, size_t> m_resultBreakdown;
};

/**
 * @brief Centralized pattern validation engine for validating pattern structures and existence
 * 
 * This class provides comprehensive validation capabilities for trading patterns,
 * including structure validation, component validation, and database consistency checks.
 * It centralizes validation logic that was previously scattered across multiple classes.
 */
class PatternValidationEngine {
public:
    /**
     * @brief Construct a new Pattern Validation Engine
     * 
     * @param database Reference to the analysis database for pattern lookups
     */
    explicit PatternValidationEngine(const AnalysisDatabase& database);

    /**
     * @brief Validate that a pattern exists in the database by hash
     * 
     * @param patternHash The hash code of the pattern to validate
     * @return ValidationResult indicating success or specific failure reason
     */
    ValidationResult validatePatternExistence(unsigned long long patternHash) const;

    /**
     * @brief Validate the structural integrity of a pattern
     * 
     * @param pattern The pattern structure to validate
     * @return ValidationResult indicating success or specific failure reason
     */
    ValidationResult validatePatternStructure(const PatternStructure& pattern) const;

    /**
     * @brief Validate that a pattern belongs to a specific group
     * 
     * @param patternHash The hash code of the pattern
     * @param groupId The group ID to validate against
     * @return ValidationResult indicating success or specific failure reason
     */
    ValidationResult validatePatternInGroup(unsigned long long patternHash, uint32_t groupId) const;

    /**
     * @brief Validate multiple patterns in a batch operation
     * 
     * @param hashes Vector of pattern hash codes to validate
     * @return Vector of validation results corresponding to each hash
     */
    std::vector<ValidationResult> validatePatternBatch(const std::vector<unsigned long long>& hashes) const;

    /**
     * @brief Find a pattern by its hash code
     * 
     * @param patternHash The hash code to search for
     * @return Optional pattern structure if found, empty if not found
     */
    std::optional<PatternStructure> findPatternByHash(unsigned long long patternHash) const;

    /**
     * @brief Find all patterns belonging to a specific group
     * 
     * @param groupId The group ID to search for
     * @return Vector of pattern structures in the group
     */
    std::vector<PatternStructure> findPatternsInGroup(uint32_t groupId) const;

    /**
     * @brief Get validation statistics
     * 
     * @return Current validation statistics
     */
    ValidationStats getValidationStats() const
    {
        return m_stats;
    }

    /**
     * @brief Reset validation statistics
     */
    void resetValidationStats()
    {
        m_stats.reset();
    }

    /**
     * @brief Convert validation result to human-readable string
     * 
     * @param result The validation result to convert
     * @return String description of the validation result
     */
    static std::string validationResultToString(ValidationResult result);

    /**
     * @brief Get detailed error message for a validation result
     * 
     * @param result The validation result
     * @return Detailed error message with suggestions for fixing
     */
    static std::string getValidationErrorMessage(ValidationResult result);

private:
    const AnalysisDatabase& m_database;
    mutable ValidationStats m_stats;

    /**
     * @brief Internal validation helper for pattern structure
     * 
     * @param pattern The pattern to validate
     * @return True if structure is valid, false otherwise
     */
    bool isValidPatternStructure(const PatternStructure& pattern) const;

    /**
     * @brief Internal validation helper for component types
     * 
     * @param components Vector of component type strings to validate
     * @return True if all components are valid, false otherwise
     */
    bool areValidComponents(const std::vector<std::string>& components) const;

    /**
     * @brief Internal validation helper for pattern conditions
     * 
     * @param conditions Vector of pattern conditions to validate
     * @return True if all conditions are valid, false otherwise
     */
    bool areValidConditions(const std::vector<PatternCondition>& conditions) const;

    /**
     * @brief Validate that bar offsets are within reasonable bounds
     * 
     * @param barOffsets Vector of bar offsets to validate
     * @return True if all offsets are valid, false otherwise
     */
    bool areValidBarOffsets(const std::vector<int>& barOffsets) const;

    /**
     * @brief Check for circular references in pattern conditions
     * 
     * @param conditions Vector of pattern conditions to check
     * @return True if no circular references found, false otherwise
     */
    bool hasNoCircularReferences(const std::vector<PatternCondition>& conditions) const;

    /**
     * @brief Record validation result in statistics
     * 
     * @param result The validation result to record
     */
    void recordValidationResult(ValidationResult result) const;
};

} // namespace palanalyzer