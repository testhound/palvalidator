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
    // For two templates to be logically equal, they must have the same name
    // and the same set of conditions, regardless of order.

    if (m_name != other.m_name) {
        return false;
    }

    if (m_conditions.size() != other.m_conditions.size()) {
        return false;
    }

    // Use std::is_permutation for an order-agnostic comparison of the conditions.
    // This algorithm checks if one collection is a permutation of the other.
    // It uses the underlying PatternCondition::operator== for comparison.
    return std::is_permutation(m_conditions.begin(), m_conditions.end(), other.m_conditions.begin());
}

bool PatternTemplate::operator!=(const PatternTemplate& other) const {
    return !(*this == other);
}


