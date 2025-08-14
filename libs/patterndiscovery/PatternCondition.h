#pragma once

#include "PriceComponentDescriptor.h"

/**
 * @enum ComparisonOperator
 * @brief Defines the type of comparison in a pattern condition.
 */
enum class ComparisonOperator : uint8_t
{
    GreaterThan,
    LessThan,
    GreaterThanOrEqual,
    LessThanOrEqual,
    Equal,
    NotEqual
};

/**
 * @class PatternCondition
 * @brief Represents a single logical condition within a pattern template.
 *
 * A condition is a comparison between two price components, for example:
 * "High of 3 bars ago is GreaterThan the Low of 5 bars ago".
 */
class PatternCondition
{
public:
    /**
     * @brief Constructs a PatternCondition.
     * @param lhs The left-hand side of the comparison.
     * @param op The comparison operator.
     * @param rhs The right-hand side of the comparison.
     */
    PatternCondition(
        const PriceComponentDescriptor& lhs,
        ComparisonOperator op,
        const PriceComponentDescriptor& rhs
    )
        : m_lhs(lhs)
        , m_operator(op)
        , m_rhs(rhs)
    {
    }

    /**
     * @brief Gets the left-hand side component of the condition.
     * @return A const reference to the LHS PriceComponentDescriptor.
     */
    const PriceComponentDescriptor& getLhs() const
    {
        return m_lhs;
    }

    /**
     * @brief Gets the comparison operator.
     * @return The ComparisonOperator enum value.
     */
    ComparisonOperator getOperator() const
    {
        return m_operator;
    }

    /**
     * @brief Gets the right-hand side component of the condition.
     * @return A const reference to the RHS PriceComponentDescriptor.
     */
    const PriceComponentDescriptor& getRhs() const
    {
        return m_rhs;
    }

    /**
     * @brief Equality operator.
     * @param other The object to compare against.
     * @return True if LHS, RHS, and operator are all identical.
     */
    bool operator==(const PatternCondition& other) const {
        return m_lhs == other.m_lhs && m_rhs == other.m_rhs && m_operator == other.m_operator;
    }

    /**
     * @brief Inequality operator.
     * @param other The object to compare against.
     * @return True if the objects are not equal.
     */
    bool operator!=(const PatternCondition& other) const {
        return !(*this == other);
    }

private:
    PriceComponentDescriptor m_lhs;
    ComparisonOperator m_operator;
    PriceComponentDescriptor m_rhs;
};
