#pragma once

#include <streambuf>
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include "ValidationTypes.h"

// Forward declarations
namespace mkc_timeseries {
    template<typename Num> class PalStrategy;
    template<typename Num> class Security;
}

using namespace mkc_timeseries;

namespace palvalidator
{
namespace utils
{

/**
 * @brief Stream buffer that mirrors output to two underlying buffers
 * 
 * This class allows writing to two different stream buffers simultaneously,
 * useful for logging to both console and file at the same time.
 */
class TeeBuf : public std::streambuf
{
public:
    /**
     * @brief Construct a TeeBuf with two target stream buffers
     * @param sb1 First stream buffer to write to
     * @param sb2 Second stream buffer to write to
     */
    TeeBuf(std::streambuf* sb1, std::streambuf* sb2);

protected:
    /**
     * @brief Handle character overflow by writing to both buffers
     * @param c Character to write
     * @return EOF on error, otherwise the character written
     */
    int overflow(int c) override;

    /**
     * @brief Synchronize both underlying buffers
     * @return 0 on success, -1 on error
     */
    int sync() override;

private:
    std::streambuf* mStreamBuf1;
    std::streambuf* mStreamBuf2;
};

/**
 * @brief Output stream that writes to two streams simultaneously
 * 
 * This class provides a convenient interface for writing to two output
 * streams at once, such as console and file output.
 */
class TeeStream : public std::ostream
{
public:
    /**
     * @brief Construct a TeeStream with two target output streams
     * @param streamA First output stream
     * @param streamB Second output stream
     */
    TeeStream(std::ostream& streamA, std::ostream& streamB);

private:
    TeeBuf mTeeBuf;
};

/**
 * @brief Create a bootstrap results filename for the given security and method
 * @param securitySymbol Symbol of the security being validated
 * @param method Validation method being used
 * @return Formatted filename with timestamp
 */
std::string createBootstrapFileName(const std::string& securitySymbol,
                                   ValidationMethod method);

/**
 * @brief Create a surviving patterns filename for the given security and method
 * @param securitySymbol Symbol of the security being validated
 * @param method Validation method being used
 * @return Formatted filename with timestamp
 */
std::string createSurvivingPatternsFileName(const std::string& securitySymbol,
                                           ValidationMethod method);

/**
 * @brief Create a detailed surviving patterns filename for the given security and method
 * @param securitySymbol Symbol of the security being validated
 * @param method Validation method being used
 * @return Formatted filename with timestamp
 */
std::string createDetailedSurvivingPatternsFileName(const std::string& securitySymbol,
                                                   ValidationMethod method);

/**
 * @brief Create a detailed rejected patterns filename for the given security and method
 * @param securitySymbol Symbol of the security being validated
 * @param method Validation method being used
 * @return Formatted filename with timestamp
 */
std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                   ValidationMethod method);

/**
 * @brief Create a permutation test survivors filename for intermediate storage
 * @param securitySymbol Symbol of the security being validated
 * @param method Validation method being used
 * @return Formatted filename with timestamp for permutation test survivors
 */
std::string createPermutationTestSurvivorsFileName(const std::string& securitySymbol,
                                                 ValidationMethod method);

/**
 * @brief Write Monte Carlo permutation test survivors to file using LogPalPattern
 * @param strategies Vector of permutation test surviving strategies to write
 * @param filename Output filename for permutation test survivor strategies
 */
template<typename Num>
void writePermutationTestSurvivors(const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                                 const std::string& filename);

/**
 * @brief Load Monte Carlo permutation test survivors from PAL pattern file using PalParseDriver
 * @param filename Input filename containing permutation test survivor patterns
 * @param security Security object for strategy reconstruction
 * @return Vector of reconstructed permutation test surviving strategies
 */
template<typename Num>
std::vector<std::shared_ptr<PalStrategy<Num>>>
loadPermutationTestSurvivors(const std::string& filename,
                           std::shared_ptr<Security<Num>> security);

/**
 * @brief Check if survivor file exists and is readable
 * @param filename Path to survivor file
 * @return true if file exists and is readable
 */
bool validateSurvivorFile(const std::string& filename);

} // namespace utils
} // namespace palvalidator