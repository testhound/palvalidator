#include "PatternUniverseDeserializer.h"
#include "PatternUniverseSerializer.h" // Included for FileHeader definition
#include <vector>


/**
 * @brief Deserializes a vector of PatternTemplate objects from the input stream.
 * @param in The input stream to read from.
 * @return A vector of PatternTemplate objects.
 * @throws std::runtime_error if the file format is invalid or a read fails.
 */
std::vector<PatternTemplate> PatternUniverseDeserializer::deserialize(std::istream& in) const
{
    // 1. Read and validate the file header.
    FileHeader header;
    read_binary(in, header);

    if (!in)
    {
        throw std::runtime_error("Failed to read pattern universe file header.");
    }

    const uint32_t expectedMagicNumber = 0x50415455; // "PATU"
    if (header.magicNumber != expectedMagicNumber)
    {
        throw std::runtime_error("Invalid file format: Magic number mismatch.");
    }

    // 2. Prepare the vector and deserialize patterns.
    std::vector<PatternTemplate> patterns;
    patterns.reserve(header.patternCount); // Pre-allocate memory for efficiency.

    for (uint32_t i = 0; i < header.patternCount; ++i)
    {
        patterns.push_back(m_patternDeserializer.deserialize(in));
    }

    return patterns;
}