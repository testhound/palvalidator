#pragma once

#include <cstdint>

/**
 * @enum PriceComponentType
 * @brief Represents the specific Open, High, Low, or Close component of a price bar.
 */
enum class PriceComponentType : uint8_t
{
    Open,
    High,
    Low,
    Close
};

/**
 * @class PriceComponentDescriptor
 * @brief A simple data object that describes a single element in a pattern's logic.
 *
 * This class identifies a specific price component (O, H, L, C) at a specific
 * historical bar offset.
 */
class PriceComponentDescriptor
{
public:
    /**
     * @brief Constructs a PriceComponentDescriptor.
     * @param componentType The price component type (e.g., High, Low).
     * @param barOffset The historical offset of the bar (e.g., 0 for the current bar).
     */
    PriceComponentDescriptor(PriceComponentType componentType, uint8_t barOffset)
        : m_componentType(componentType)
        , m_barOffset(barOffset)
    {
    }

    /**
     * @brief Gets the price component type.
     * @return The PriceComponentType enum value.
     */
    PriceComponentType getComponentType() const
    {
        return m_componentType;
    }

    /**
     * @brief Gets the bar offset.
     * @return The historical bar offset.
     */
    uint8_t getBarOffset() const
    {
        return m_barOffset;
    }

    /**
     * @brief Equality operator.
     * @param other The object to compare against.
     * @return True if both objects have the same component type and bar offset.
     */
    bool operator==(const PriceComponentDescriptor& other) const {
        return m_componentType == other.m_componentType && m_barOffset == other.m_barOffset;
    }

    /**
     * @brief Inequality operator.
     * @param other The object to compare against.
     * @return True if the objects are not equal.
     */
    bool operator!=(const PriceComponentDescriptor& other) const {
        return !(*this == other);
    }

private:
    PriceComponentType m_componentType;
    uint8_t m_barOffset;
};
