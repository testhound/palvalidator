#include "BinaryPatternTemplateSerializer.h"
#include <stdexcept>


/**
 * @brief Serializes a single PatternTemplate object to the given output stream.
 * @param out The output stream to write to.
 * @param pattern The PatternTemplate object to serialize.
 * @throws std::runtime_error if there is a failure writing to the stream.
 */
void BinaryPatternTemplateSerializer::serialize(std::ostream& out, const PatternTemplate& pattern) const
{
    // Write Name
    const std::string& name = pattern.getName();
    const uint16_t nameLength = static_cast<uint16_t>(name.length());
    write_binary(out, nameLength);
    out.write(name.c_str(), nameLength);

    // Write Conditions
    const auto& conditions = pattern.getConditions();
    const uint8_t conditionCount = static_cast<uint8_t>(conditions.size());
    write_binary(out, conditionCount);

    // Write reserved byte for padding
    const uint8_t reserved = 0;
    write_binary(out, reserved);

    for (const auto& condition : conditions)
    {
        // Write LHS
        const auto& lhs = condition.getLhs();
        write_binary(out, static_cast<uint8_t>(lhs.getComponentType()));
        write_binary(out, lhs.getBarOffset());

        // Write Operator
        write_binary(out, static_cast<uint8_t>(condition.getOperator()));

        // Write RHS
        const auto& rhs = condition.getRhs();
        write_binary(out, static_cast<uint8_t>(rhs.getComponentType()));
        write_binary(out, rhs.getBarOffset());
    }

    if (!out)
    {
        throw std::runtime_error("Failed to write PatternTemplate to stream.");
    }
}
