#pragma once

#include "PatternTemplate.h"
#include <iostream>

/**
 * @class BinaryPatternTemplateSerializer
 * @brief Handles the serialization of a PatternTemplate object to a binary stream.
 *
 * This class writes a PatternTemplate to a stream according to the defined
 * binary file format. It is a stateless utility class.
 */
class BinaryPatternTemplateSerializer
{
public:
    /**
     * @brief Serializes a single PatternTemplate object to the given output stream.
     * @param out The output stream to write to.
     * @param pattern The PatternTemplate object to serialize.
     * @throws std::runtime_error if there is a failure writing to the stream.
     */
    void serialize(std::ostream& out, const PatternTemplate& pattern) const;
};

// Helper function to write raw bytes to the stream.
template<typename T>
static void write_binary(std::ostream& out, const T& value)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}
