#pragma once

#include "BinaryPatternTemplateDeserializer.h"
#include "PatternTemplate.h"
#include <iostream>
#include <vector>
#include <stdexcept>

// Forward-declare the header to avoid duplicating its definition
struct FileHeader;

/**
 * @class PatternUniverseDeserializer
 * @brief Deserializes a collection of PatternTemplate objects from a binary stream.
 *
 * This class reads and validates the file header, then reconstructs the vector
 * of patterns by delegating the deserialization of each pattern to the
 * BinaryPatternTemplateDeserializer.
 */
class PatternUniverseDeserializer {
public:
    /**
     * @brief Deserializes a vector of PatternTemplate objects from the input stream.
     * @param in The input stream to read from.
     * @return A vector of PatternTemplate objects.
     * @throws std::runtime_error if the file format is invalid or a read fails.
     */
    std::vector<PatternTemplate> deserialize(std::istream& in) const;

private:
    BinaryPatternTemplateDeserializer m_patternDeserializer;
};
