#include "PatternUniverseSerializer.h"
#include <stdexcept>

/**
 * @brief Serializes a vector of PatternTemplate objects to the output stream.
 * @param out The output stream to write to.
 * @param patterns The vector of patterns to serialize.
 * @throws std::runtime_error if there is a failure writing to the stream.
 */
void PatternUniverseSerializer::serialize(std::ostream& out, const std::vector<PatternTemplate>& patterns) const
{
    // 1. Prepare and write the file header.
    FileHeader header;
    header.patternCount = static_cast<uint32_t>(patterns.size());
    
    // Write theh file header
    
    write_binary(out, header);

    if (!out)
    {
        throw std::runtime_error("Failed to write pattern universe file header.");
    }

    // 2. Iterate and serialize each pattern using the member serializer.
    for (const auto& pattern : patterns)
    {
        m_patternSerializer.serialize(out, pattern);
    }

    if (!out)
    {
        throw std::runtime_error("A failure occurred while writing a pattern to the stream.");
    }
}