#pragma once

#include <iostream>

// Forward declaration to avoid including the full PatternTemplate header here.
class PatternTemplate;

/**
 * @class BinaryPatternTemplateDeserializer
 * @brief Handles the deserialization of a PatternTemplate object from a binary stream.
 *
 * This class reads from a stream according to the defined binary file format
 * and constructs a new PatternTemplate object. It is a stateless utility class.
 */
class BinaryPatternTemplateDeserializer
{
public:
    /**
     * @brief Deserializes a single PatternTemplate object from the given input stream.
     * @param in The input stream to read from.
     * @return A new PatternTemplate object constructed from the stream data.
     * @throws std::runtime_error if there is a failure reading from the stream or
     * if the data is malformed.
     */
    PatternTemplate deserialize(std::istream& in) const;
};

// Helper function to read raw bytes from the stream.
template<typename T>
static void read_binary(std::istream& in, T& value)
{
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
}