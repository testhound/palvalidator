#include "PatternTemplate.h"
#include <algorithm>
#include <set>

/**
 * @brief Constructs a PatternTemplate with a given name.
 * @param name A unique, human-readable name for the template.
 */
PatternTemplate::PatternTemplate(const std::string& name)
    : m_name(name)
    , m_maxBarOffset(0)
{
}

/**
 * @brief Adds a new logical condition to the pattern.
 *
 * All conditions added to the template are implicitly ANDed together.
 * @param condition The PatternCondition to add.
 */
void PatternTemplate::addCondition(const PatternCondition& condition)
{
    m_conditions.push_back(condition);

    // After adding, recalculate the metadata for the entire pattern.
    recalculateMetadata();
}

/**
 * @brief Gets the collection of logical conditions that define the pattern.
 * @return A const reference to the vector of PatternConditions.
 */
const std::vector<PatternCondition>& PatternTemplate::getConditions() const
{
    return m_conditions;
}

/**
 * @brief Gets the name of the pattern template.
 * @return A const reference to the pattern's name.
 */
const std::string& PatternTemplate::getName() const
{
    return m_name;
}

/**
 * @brief Gets the maximum bar offset, defining the lookback period required
 * by this pattern.
 * @return The largest bar offset used in any component across all conditions.
 */
uint8_t PatternTemplate::getMaxBarOffset() const
{
    return m_maxBarOffset;
}

/**
 * @brief Gets the number of unique price components involved in this pattern.
 * @return The count of unique PriceComponentDescriptors.
 */
size_t PatternTemplate::getNumUniqueComponents() const
{
    // This is calculated on the fly as it's less frequently needed than
    // max bar offset.
    std::set<std::pair<PriceComponentType, uint8_t>> uniqueComponents;
    for (const auto& condition : m_conditions)
    {
        const auto& lhs = condition.getLhs();
        const auto& rhs = condition.getRhs();
        uniqueComponents.insert({lhs.getComponentType(), lhs.getBarOffset()});
        uniqueComponents.insert({rhs.getComponentType(), rhs.getBarOffset()});
    }
    return uniqueComponents.size();
}

/**
 * @brief Recalculates metadata like maxBarOffset after a condition is added.
 * @private
 */
void PatternTemplate::recalculateMetadata()
{
    m_maxBarOffset = 0;
    for (const auto& condition : m_conditions)
    {
        m_maxBarOffset = std::max(
            m_maxBarOffset,
            condition.getLhs().getBarOffset()
        );
        m_maxBarOffset = std::max(
            m_maxBarOffset,
            condition.getRhs().getBarOffset()
        );
    }
}

bool PatternTemplate::operator==(const PatternTemplate& other) const {
    // For consistency with operator<, we need deterministic comparison
    // First compare by name
    if (m_name != other.m_name) {
        return false;
    }

    if (m_conditions.size() != other.m_conditions.size()) {
        return false;
    }

    // Create sorted copies for deterministic comparison (same logic as operator<)
    auto lhsSorted = m_conditions;
    auto rhsSorted = other.m_conditions;
    
    // Sort conditions by their components for deterministic ordering
    auto conditionComparator = [](const PatternCondition& a, const PatternCondition& b) {
        // Compare LHS component first
        if (a.getLhs().getBarOffset() != b.getLhs().getBarOffset()) {
            return a.getLhs().getBarOffset() < b.getLhs().getBarOffset();
        }
        if (a.getLhs().getComponentType() != b.getLhs().getComponentType()) {
            return a.getLhs().getComponentType() < b.getLhs().getComponentType();
        }
        
        // Then compare RHS component
        if (a.getRhs().getBarOffset() != b.getRhs().getBarOffset()) {
            return a.getRhs().getBarOffset() < b.getRhs().getBarOffset();
        }
        if (a.getRhs().getComponentType() != b.getRhs().getComponentType()) {
            return a.getRhs().getComponentType() < b.getRhs().getComponentType();
        }
        
        // Finally compare operator
        return a.getOperator() < b.getOperator();
    };
    
    std::sort(lhsSorted.begin(), lhsSorted.end(), conditionComparator);
    std::sort(rhsSorted.begin(), rhsSorted.end(), conditionComparator);
    
    // Compare sorted conditions element by element
    return lhsSorted == rhsSorted;
}

bool PatternTemplate::operator!=(const PatternTemplate& other) const {
    return !(*this == other);
}

bool PatternTemplate::operator<(const PatternTemplate& other) const {
    // First compare by number of conditions
    if (m_conditions.size() != other.m_conditions.size()) {
        return m_conditions.size() < other.m_conditions.size();
    }
    
    // Create sorted vectors of conditions for deterministic comparison
    auto lhsSorted = m_conditions;
    auto rhsSorted = other.m_conditions;
    
    // Sort conditions by their components for deterministic ordering
    auto conditionComparator = [](const PatternCondition& a, const PatternCondition& b) {
        // Compare LHS components first
        if (a.getLhs().getBarOffset() != b.getLhs().getBarOffset()) {
            return a.getLhs().getBarOffset() < b.getLhs().getBarOffset();
        }
        if (a.getLhs().getComponentType() != b.getLhs().getComponentType()) {
            return a.getLhs().getComponentType() < b.getLhs().getComponentType();
        }
        
        // Then compare RHS components
        if (a.getRhs().getBarOffset() != b.getRhs().getBarOffset()) {
            return a.getRhs().getBarOffset() < b.getRhs().getBarOffset();
        }
        if (a.getRhs().getComponentType() != b.getRhs().getComponentType()) {
            return a.getRhs().getComponentType() < b.getRhs().getComponentType();
        }
        
        // Finally compare operators
        return a.getOperator() < b.getOperator();
    };
    
    std::sort(lhsSorted.begin(), lhsSorted.end(), conditionComparator);
    std::sort(rhsSorted.begin(), rhsSorted.end(), conditionComparator);
    
    // Lexicographic comparison of sorted conditions
    for (size_t i = 0; i < lhsSorted.size(); ++i) {
        const auto& lhsCond = lhsSorted[i];
        const auto& rhsCond = rhsSorted[i];
        
        // Compare LHS components
        if (lhsCond.getLhs().getBarOffset() != rhsCond.getLhs().getBarOffset()) {
            return lhsCond.getLhs().getBarOffset() < rhsCond.getLhs().getBarOffset();
        }
        if (lhsCond.getLhs().getComponentType() != rhsCond.getLhs().getComponentType()) {
            return lhsCond.getLhs().getComponentType() < rhsCond.getLhs().getComponentType();
        }
        
        // Compare RHS components
        if (lhsCond.getRhs().getBarOffset() != rhsCond.getRhs().getBarOffset()) {
            return lhsCond.getRhs().getBarOffset() < rhsCond.getRhs().getBarOffset();
        }
        if (lhsCond.getRhs().getComponentType() != rhsCond.getRhs().getComponentType()) {
            return lhsCond.getRhs().getComponentType() < rhsCond.getRhs().getComponentType();
        }
        
        // Compare operators
        if (lhsCond.getOperator() != rhsCond.getOperator()) {
            return lhsCond.getOperator() < rhsCond.getOperator();
        }
    }
    
    return false; // They are equal
}


