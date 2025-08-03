#pragma once

#include "BinaryPatternTemplateSerializer.h"
#include "PatternTemplate.h"
#include <iostream>
#include <vector>
#include <cstdint>

/**
 * @struct FileHeader
 * @brief Defines the structure of the binary pattern universe file header.
 */
struct FileHeader {
    uint32_t magicNumber = 0x50415455; // "PATU" in little-endian
    uint16_t version = 1;
    uint32_t patternCount = 0;
};

/**
 * @class PatternUniverseSerializer
 * @brief Serializes a collection of PatternTemplate objects to a binary stream.
 *
 * This class is responsible for writing the file header and then iterating
 * through a vector of patterns, delegating the serialization of each individual
 * pattern to the BinaryPatternTemplateSerializer.
 */
class PatternUniverseSerializer {
public:
    /**
     * @brief Serializes a vector of PatternTemplate objects to the output stream.
     * @param out The output stream to write to.
     * @param patterns The vector of patterns to serialize.
     * @throws std::runtime_error if there is a failure writing to the stream.
     */
    void serialize(std::ostream& out, const std::vector<PatternTemplate>& patterns) const;

private:
    BinaryPatternTemplateSerializer m_patternSerializer;
};
