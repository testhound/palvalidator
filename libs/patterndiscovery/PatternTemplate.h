#pragma once

#include "PatternCondition.h"
#include <vector>
#include <string>
#include <cstdint>

/**
 * @class PatternTemplate
 * @brief Represents the abstract rules of a sparse or dense price pattern.
 *
 * This class acts as a container for a set of PatternConditions that are all
 * implicitly ANDed together to form the complete pattern logic. Its state is
 * built up by adding conditions after construction.
 */
class PatternTemplate
{
public:
    /**
     * @brief Constructs a PatternTemplate with a given name.
     * @param name A unique, human-readable name for the template.
     */
    explicit PatternTemplate(const std::string& name);

    /**
     * @brief Adds a new logical condition to the pattern.
     * @param condition The PatternCondition to add.
     */
    void addCondition(const PatternCondition& condition);

    /**
     * @brief Gets the collection of logical conditions that define the pattern.
     * @return A const reference to the vector of PatternConditions.
     */
    const std::vector<PatternCondition>& getConditions() const;

    /**
     * @brief Gets the name of the pattern template.
     * @return A const reference to the pattern's name.
     */
    const std::string& getName() const;

    /**
     * @brief Gets the maximum bar offset, defining the lookback period required
     * by this pattern.
     * @return The largest bar offset used in any component across all conditions.
     */
    uint8_t getMaxBarOffset() const;

    /**
     * @brief Gets the number of unique price components involved in this pattern.
     * @return The count of unique PriceComponentDescriptors.
     */
    size_t getNumUniqueComponents() const;

    /**
     * @brief Equality operator.
     * @param other The object to compare against.
     * @return True if the name and all conditions are identical.
     */
    bool operator==(const PatternTemplate& other) const;

    /**
     * @brief Inequality operator.
     * @param other The object to compare against.
     * @return True if the objects are not equal.
     */
    bool operator!=(const PatternTemplate& other) const;

private:
    /**
     * @brief Recalculates metadata like maxBarOffset after a condition is added.
     */
    void recalculateMetadata();

    std::string m_name;
    std::vector<PatternCondition> m_conditions;
    uint8_t m_maxBarOffset;
};
