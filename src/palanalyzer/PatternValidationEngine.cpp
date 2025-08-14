#include "PatternValidationEngine.h"
#include "AnalysisDatabase.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <functional>
#include <PatternUtilities.h>

namespace palanalyzer {

PatternValidationEngine::PatternValidationEngine(const AnalysisDatabase& database)
    : m_database(database)
{
}

ValidationResult PatternValidationEngine::validatePatternExistence(unsigned long long patternHash) const
{
    ValidationResult result = ValidationResult::PATTERN_NOT_FOUND;
    
    try
    {
        // Search through all index groups for the pattern
        const auto& indexGroups = m_database.getIndexGroups();
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            const auto& patterns = groupInfo.getPatterns();
            std::string hashStr = std::to_string(patternHash);
            
            if (patterns.find(hashStr) != patterns.end())
            {
                result = ValidationResult::VALID;
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during pattern existence validation: " << e.what() << std::endl;
        result = ValidationResult::DATABASE_ERROR;
    }
    
    recordValidationResult(result);
    return result;
}

ValidationResult PatternValidationEngine::validatePatternStructure(const PatternStructure& pattern) const
{
    ValidationResult result = ValidationResult::VALID;
    
    // Validate basic structure
    if (!isValidPatternStructure(pattern))
    {
        result = ValidationResult::INVALID_STRUCTURE_MALFORMED;
    }
    // Validate conditions
    else if (!areValidConditions(pattern.getConditions()))
    {
        result = ValidationResult::INVALID_CONDITIONS_LOGICAL_ERROR;
    }
    // Validate components
    else if (!areValidComponents(pattern.getComponentsUsed()))
    {
        result = ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE;
    }
    // Validate bar offsets
    else if (!areValidBarOffsets(pattern.getBarOffsetsUsed()))
    {
        result = ValidationResult::INVALID_COMPONENTS_INVALID_OFFSET;
    }
    // Check for circular references
    else if (!hasNoCircularReferences(pattern.getConditions()))
    {
        result = ValidationResult::INVALID_CONDITIONS_CIRCULAR_REFERENCE;
    }
    
    recordValidationResult(result);
    return result;
}

ValidationResult PatternValidationEngine::validatePatternInGroup(unsigned long long patternHash, uint32_t groupId) const
{
    ValidationResult result = ValidationResult::PATTERN_NOT_FOUND;
    
    try
    {
        if (!m_database.hasIndex(groupId))
        {
            result = ValidationResult::GROUP_NOT_FOUND;
        }
        else
        {
            const auto& indexGroups = m_database.getIndexGroups();
            auto groupIt = indexGroups.find(groupId);
            
            if (groupIt != indexGroups.end())
            {
                const auto& patterns = groupIt->second.getPatterns();
                std::string hashStr = std::to_string(patternHash);
                
                if (patterns.find(hashStr) != patterns.end())
                {
                    result = ValidationResult::VALID;
                }
            }
            else
            {
                result = ValidationResult::GROUP_NOT_FOUND;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during pattern group validation: " << e.what() << std::endl;
        result = ValidationResult::DATABASE_ERROR;
    }
    
    recordValidationResult(result);
    return result;
}

std::vector<ValidationResult> PatternValidationEngine::validatePatternBatch(const std::vector<unsigned long long>& hashes) const
{
    std::vector<ValidationResult> results;
    results.reserve(hashes.size());
    
    for (unsigned long long hash : hashes)
    {
        results.push_back(validatePatternExistence(hash));
    }
    
    return results;
}

std::optional<PatternStructure> PatternValidationEngine::findPatternByHash(unsigned long long patternHash) const
{
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        std::string hashStr = std::to_string(patternHash);
        
        for (const auto& [groupId, groupInfo] : indexGroups)
        {
            const auto& patterns = groupInfo.getPatterns();
            auto patternIt = patterns.find(hashStr);
            
            if (patternIt != patterns.end())
            {
                return patternIt->second;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during pattern lookup: " << e.what() << std::endl;
    }
    
    return std::nullopt;
}

std::vector<PatternStructure> PatternValidationEngine::findPatternsInGroup(uint32_t groupId) const
{
    std::vector<PatternStructure> patterns;
    
    try
    {
        const auto& indexGroups = m_database.getIndexGroups();
        auto groupIt = indexGroups.find(groupId);
        
        if (groupIt != indexGroups.end())
        {
            const auto& groupPatterns = groupIt->second.getPatterns();
            patterns.reserve(groupPatterns.size());
            
            for (const auto& [hashStr, pattern] : groupPatterns)
            {
                patterns.push_back(pattern);
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during group pattern lookup: " << e.what() << std::endl;
    }
    
    return patterns;
}

std::string PatternValidationEngine::validationResultToString(ValidationResult result)
{
    switch (result)
    {
        case ValidationResult::VALID:
            return "Valid";
        case ValidationResult::VALID_WITH_WARNINGS:
            return "Valid with warnings";
        case ValidationResult::INVALID_HASH_MISMATCH:
            return "Invalid hash mismatch";
        case ValidationResult::INVALID_HASH_COLLISION:
            return "Invalid hash collision";
        case ValidationResult::INVALID_HASH_FORMAT:
            return "Invalid hash format";
        case ValidationResult::INVALID_STRUCTURE_EMPTY_CONDITIONS:
            return "Invalid structure: empty conditions";
        case ValidationResult::INVALID_STRUCTURE_TOO_MANY_CONDITIONS:
            return "Invalid structure: too many conditions";
        case ValidationResult::INVALID_STRUCTURE_MALFORMED:
            return "Invalid structure: malformed";
        case ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE:
            return "Invalid components: unknown type";
        case ValidationResult::INVALID_COMPONENTS_INVALID_OFFSET:
            return "Invalid components: invalid offset";
        case ValidationResult::INVALID_COMPONENTS_MISSING_REQUIRED:
            return "Invalid components: missing required";
        case ValidationResult::INVALID_CONDITIONS_LOGICAL_ERROR:
            return "Invalid conditions: logical error";
        case ValidationResult::INVALID_CONDITIONS_CIRCULAR_REFERENCE:
            return "Invalid conditions: circular reference";
        case ValidationResult::INVALID_CONDITIONS_UNSUPPORTED_OPERATOR:
            return "Invalid conditions: unsupported operator";
        case ValidationResult::PATTERN_NOT_FOUND:
            return "Pattern not found";
        case ValidationResult::GROUP_NOT_FOUND:
            return "Group not found";
        case ValidationResult::DATABASE_ERROR:
            return "Database error";
        case ValidationResult::WARNING_COMPLEX_PATTERN:
            return "Warning: complex pattern";
        case ValidationResult::WARNING_RARE_COMPONENTS:
            return "Warning: rare components";
        case ValidationResult::WARNING_DEEP_NESTING:
            return "Warning: deep nesting";
        default:
            return "Unknown validation result";
    }
}

std::string PatternValidationEngine::getValidationErrorMessage(ValidationResult result)
{
    switch (result)
    {
        case ValidationResult::VALID:
            return "Pattern validation successful.";
        case ValidationResult::VALID_WITH_WARNINGS:
            return "Pattern validation successful with warnings. Review pattern complexity.";
        case ValidationResult::INVALID_HASH_MISMATCH:
            return "Pattern hash does not match expected value. Verify pattern structure and recalculate hash.";
        case ValidationResult::INVALID_HASH_COLLISION:
            return "Hash collision detected. Use alternative hash generation method.";
        case ValidationResult::INVALID_HASH_FORMAT:
            return "Invalid hash format. Ensure hash is a valid unsigned long long value.";
        case ValidationResult::INVALID_STRUCTURE_EMPTY_CONDITIONS:
            return "Pattern has no conditions. Add at least one valid condition.";
        case ValidationResult::INVALID_STRUCTURE_TOO_MANY_CONDITIONS:
            return "Pattern has too many conditions. Reduce condition count to acceptable limits.";
        case ValidationResult::INVALID_STRUCTURE_MALFORMED:
            return "Pattern structure is malformed. Verify all required fields are present and valid.";
        case ValidationResult::INVALID_COMPONENTS_UNKNOWN_TYPE:
            return "Pattern uses unknown component types. Use only OPEN, HIGH, LOW, CLOSE, or other supported types.";
        case ValidationResult::INVALID_COMPONENTS_INVALID_OFFSET:
            return "Pattern uses invalid bar offsets. Ensure all offsets are non-negative and within reasonable bounds.";
        case ValidationResult::INVALID_COMPONENTS_MISSING_REQUIRED:
            return "Pattern is missing required components. Verify pattern meets minimum component requirements.";
        case ValidationResult::INVALID_CONDITIONS_LOGICAL_ERROR:
            return "Pattern contains logical errors in conditions. Review condition logic for contradictions.";
        case ValidationResult::INVALID_CONDITIONS_CIRCULAR_REFERENCE:
            return "Pattern contains circular references. Remove circular dependencies between conditions.";
        case ValidationResult::INVALID_CONDITIONS_UNSUPPORTED_OPERATOR:
            return "Pattern uses unsupported operators. Use only supported comparison operators.";
        case ValidationResult::PATTERN_NOT_FOUND:
            return "Pattern not found in database. Verify pattern hash and database contents.";
        case ValidationResult::GROUP_NOT_FOUND:
            return "Pattern group not found in database. Verify group ID and database contents.";
        case ValidationResult::DATABASE_ERROR:
            return "Database access error occurred. Check database connectivity and integrity.";
        case ValidationResult::WARNING_COMPLEX_PATTERN:
            return "Warning: Pattern is complex and may impact performance. Consider simplification.";
        case ValidationResult::WARNING_RARE_COMPONENTS:
            return "Warning: Pattern uses rare components that may have limited data. Verify component availability.";
        case ValidationResult::WARNING_DEEP_NESTING:
            return "Warning: Pattern has deep nesting that may impact readability. Consider restructuring.";
        default:
            return "Unknown validation error. Contact system administrator.";
    }
}

bool PatternValidationEngine::isValidPatternStructure(const PatternStructure& pattern) const
{
    // Check basic structure requirements
    if (pattern.getPatternHash() == 0)
    {
        std::cerr << "Validation failed: Pattern hash is 0." << std::endl;
        return false;
    }

    if (pattern.getGroupId() < 0)
    {
        std::cerr << "Validation failed: Group ID is negative." << std::endl;
        return false;
    }

    if (pattern.getConditions().empty())
    {
        std::cerr << "Validation failed: Conditions are empty." << std::endl;
        return false;
    }

    if (pattern.getConditionCount() != static_cast<int>(pattern.getConditions().size()))
    {
        std::cerr << "Validation failed: Condition count mismatch." << std::endl;
        return false;
    }

    if (pattern.getComponentsUsed().empty())
    {
        std::cerr << "Validation failed: Components used are empty." << std::endl;
        return false;
    }

    if (pattern.getBarOffsetsUsed().empty())
    {
        std::cerr << "Validation failed: Bar offsets used are empty." << std::endl;
        return false;
    }

    // Check for reasonable limits
    const size_t MAX_CONDITIONS = 50;
    const size_t MAX_COMPONENTS = 20;
    const size_t MAX_BAR_OFFSETS = 100;

    if (pattern.getConditions().size() > MAX_CONDITIONS)
    {
        std::cerr << "Validation failed: Too many conditions." << std::endl;
        return false;
    }

    if (pattern.getComponentsUsed().size() > MAX_COMPONENTS)
    {
        std::cerr << "Validation failed: Too many components used." << std::endl;
        return false;
    }

    if (pattern.getBarOffsetsUsed().size() > MAX_BAR_OFFSETS)
    {
        std::cerr << "Validation failed: Too many bar offsets used." << std::endl;
        return false;
    }

    return true;
}

bool PatternValidationEngine::areValidComponents(const std::vector<std::string>& components) const
{
    const std::unordered_set<std::string> validComponents = {
        "OPEN", "HIGH", "LOW", "CLOSE", "VOLUME", 
        "ROC1", "IBS1", "IBS2", "IBS3", "MEANDER", 
        "VCHARTLOW", "VCHARTHIGH"
    };
    
    for (const auto& component : components)
    {
        if (validComponents.find(component) == validComponents.end())
        {
            return false;
        }
    }
    
    return !components.empty();
}

bool PatternValidationEngine::areValidConditions(const std::vector<PatternCondition>& conditions) const
{
    if (conditions.empty())
    {
        return false;
    }
    
    for (const auto& condition : conditions)
    {
        // Check operator validity using enum-based approach
        // All defined ComparisonOperator values are valid, no UNKNOWN value exists
        // Just continue with validation
        
        // Check that LHS and RHS are different (no self-comparison)
        const auto& lhs = condition.getLhs();
        const auto& rhs = condition.getRhs();
        
        if (lhs.getComponentType() == rhs.getComponentType() && lhs.getBarOffset() == rhs.getBarOffset())
        {
            return false; // Self-comparison is invalid
        }
        
        // Validate component types in condition
        std::vector<std::string> conditionComponents = {
            componentTypeToString(lhs.getComponentType()),
            componentTypeToString(rhs.getComponentType())
        };
        
        if (!areValidComponents(conditionComponents))
        {
            return false;
        }
    }
    
    return true;
}

bool PatternValidationEngine::areValidBarOffsets(const std::vector<int>& barOffsets) const
{
    if (barOffsets.empty())
    {
        return false;
    }
    
    const int MAX_BAR_OFFSET = 255;
    const int MIN_BAR_OFFSET = 0;
    
    for (int offset : barOffsets)
    {
        if (offset < MIN_BAR_OFFSET || offset > MAX_BAR_OFFSET)
        {
            return false;
        }
    }
    
    return true;
}

bool PatternValidationEngine::hasNoCircularReferences(const std::vector<PatternCondition>& conditions) const
{
    // Build a dependency graph to detect cycles
    std::map<std::pair<PriceComponentType, uint8_t>, std::vector<std::pair<PriceComponentType, uint8_t>>> dependencies;
    
    for (const auto& condition : conditions)
    {
        const auto& lhs = condition.getLhs();
        const auto& rhs = condition.getRhs();
        
        std::pair<PriceComponentType, uint8_t> lhsKey = {lhs.getComponentType(), lhs.getBarOffset()};
        std::pair<PriceComponentType, uint8_t> rhsKey = {rhs.getComponentType(), rhs.getBarOffset()};
        
        // For GreaterThan conditions, LHS depends on RHS
        auto op = condition.getOperator();
        if (op == ComparisonOperator::GreaterThan ||
            op == ComparisonOperator::GreaterThanOrEqual)
        {
            dependencies[lhsKey].push_back(rhsKey);
        }
        // For LessThan conditions, RHS depends on LHS
        else if (op == ComparisonOperator::LessThan ||
                 op == ComparisonOperator::LessThanOrEqual)
        {
            dependencies[rhsKey].push_back(lhsKey);
        }
    }
    
    // Simple cycle detection using DFS
    std::set<std::pair<PriceComponentType, uint8_t>> visited;
    std::set<std::pair<PriceComponentType, uint8_t>> recursionStack;
    
    std::function<bool(const std::pair<PriceComponentType, uint8_t>&)> hasCycle = 
        [&](const std::pair<PriceComponentType, uint8_t>& node) -> bool
        {
            if (recursionStack.find(node) != recursionStack.end())
            {
                return true; // Cycle detected
            }
            
            if (visited.find(node) != visited.end())
            {
                return false; // Already processed
            }
            
            visited.insert(node);
            recursionStack.insert(node);
            
            auto depIt = dependencies.find(node);
            if (depIt != dependencies.end())
            {
                for (const auto& dependency : depIt->second)
                {
                    if (hasCycle(dependency))
                    {
                        return true;
                    }
                }
            }
            
            recursionStack.erase(node);
            return false;
        };
    
    for (const auto& [node, deps] : dependencies)
    {
        if (visited.find(node) == visited.end())
        {
            if (hasCycle(node))
            {
                return false; // Circular reference found
            }
        }
    }
    
    return true; // No circular references
}

void PatternValidationEngine::recordValidationResult(ValidationResult result) const
{
    m_stats.recordValidation(result);
}

} // namespace palanalyzer