#include "BinaryPatternTemplateDeserializer.h"
#include "PatternTemplate.h"
#include <vector>
#include <stdexcept>



/**
 * @brief Deserializes a single PatternTemplate object from the given input stream.
 * @param in The input stream to read from.
 * @return A new PatternTemplate object constructed from the stream data.
 * @throws std::runtime_error if there is a failure reading from the stream or
 * if the data is malformed.
 */
PatternTemplate BinaryPatternTemplateDeserializer::deserialize(std::istream& in) const
{
    // Read Name
    uint16_t nameLength = 0;
    read_binary(in, nameLength);

    if (!in)
    {
        throw std::runtime_error("Failed to read name length from stream.");
    }

    std::vector<char> nameBuffer(nameLength);
    in.read(nameBuffer.data(), nameLength);
    std::string name(nameBuffer.begin(), nameBuffer.end());

    PatternTemplate pattern(name);

    // Read Conditions
    uint8_t conditionCount = 0;
    read_binary(in, conditionCount);

    // Read and discard reserved byte
    uint8_t reserved = 0;
    read_binary(in, reserved);

    for (uint8_t i = 0; i < conditionCount; ++i)
    {
        uint8_t lhsType_u8, lhsOffset, op_u8, rhsType_u8, rhsOffset;

        read_binary(in, lhsType_u8);
        read_binary(in, lhsOffset);
        read_binary(in, op_u8);
        read_binary(in, rhsType_u8);
        read_binary(in, rhsOffset);

        if (!in)
        {
            throw std::runtime_error("Failed to read condition data from stream.");
        }

        PriceComponentDescriptor lhs(static_cast<PriceComponentType>(lhsType_u8), lhsOffset);
        PriceComponentDescriptor rhs(static_cast<PriceComponentType>(rhsType_u8), rhsOffset);
        ComparisonOperator op = static_cast<ComparisonOperator>(op_u8);

        pattern.addCondition(PatternCondition(lhs, op, rhs));
    }

    return pattern;
}
