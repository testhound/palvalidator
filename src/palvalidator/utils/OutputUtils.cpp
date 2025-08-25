#include "OutputUtils.h"
#include "TimeUtils.h"
#include <filesystem>

namespace palvalidator
{
namespace utils
{

TeeBuf::TeeBuf(std::streambuf* sb1, std::streambuf* sb2)
    : mStreamBuf1(sb1),
      mStreamBuf2(sb2)
{
}

int TeeBuf::overflow(int c)
{
    if (c == EOF)
    {
        return !EOF;
    }
    
    const int r1 = mStreamBuf1->sputc(static_cast<char>(c));
    const int r2 = mStreamBuf2->sputc(static_cast<char>(c));
    return (r1 == EOF || r2 == EOF) ? EOF : c;
}

int TeeBuf::sync()
{
    const int r1 = mStreamBuf1->pubsync();
    const int r2 = mStreamBuf2->pubsync();
    return (r1 == 0 && r2 == 0) ? 0 : -1;
}

TeeStream::TeeStream(std::ostream& streamA, std::ostream& streamB)
    : std::ostream(nullptr),
      mTeeBuf(streamA.rdbuf(), streamB.rdbuf())
{
    this->rdbuf(&mTeeBuf);
}

std::string createBootstrapFileName(const std::string& securitySymbol,
                                   ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_Bootstrap_Results_" + getCurrentTimestamp() + ".txt";
}

std::string createSurvivingPatternsFileName(const std::string& securitySymbol,
                                           ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

std::string createDetailedSurvivingPatternsFileName(const std::string& securitySymbol,
                                                   ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_Detailed_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                  ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method)
        + "_Detailed_RejectedPatterns_" + getCurrentTimestamp() + ".txt";
}

} // namespace utils
} // namespace palvalidator