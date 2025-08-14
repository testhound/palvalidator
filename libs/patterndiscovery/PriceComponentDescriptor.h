#pragma once

#include <cstdint>
#include <string>

/**
 * @enum PriceComponentType
 * @brief Represents the specific Open, High, Low, or Close component of a price bar.
 */
enum class PriceComponentType : uint8_t
{
    Open,
    High,
    Low,
    Close,
    Volume,        // Add from palanalyzer
    Roc1,          // Add from palanalyzer
    Ibs1,          // Add from palanalyzer
    Ibs2,          // Add from palanalyzer
    Ibs3,          // Add from palanalyzer
    Meander,       // Add from palanalyzer
    VChartLow,     // Add from palanalyzer
    VChartHigh     // Add from palanalyzer
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
     * @param description Optional description for the component (default empty).
     */
    PriceComponentDescriptor(PriceComponentType componentType, uint8_t barOffset, const std::string& description = "")
        : m_componentType(componentType)
        , m_barOffset(barOffset)
        , m_description(description)
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
     * @brief Gets the description.
     * @return The description string.
     */
    const std::string& getDescription() const
    {
        return m_description;
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

    /**
     * @brief Less-than operator for sorting (needed by palanalyzer).
     * @param other The object to compare against.
     * @return True if this object is less than the other.
     */
    bool operator<(const PriceComponentDescriptor& other) const {
        if (m_componentType != other.m_componentType) return m_componentType < other.m_componentType;
        return m_barOffset < other.m_barOffset;
    }

private:
    PriceComponentType m_componentType;
    uint8_t m_barOffset;
    std::string m_description;  // Add optional description
};
