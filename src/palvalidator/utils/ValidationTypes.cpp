#include "ValidationTypes.h"
#include <stdexcept>

namespace palvalidator
{
namespace utils
{

std::string getValidationMethodString(ValidationMethod method)
{
    switch (method)
    {
        case ValidationMethod::Masters:
            return "Masters";
        case ValidationMethod::RomanoWolf:
            return "RomanoWolf";
        case ValidationMethod::BenjaminiHochberg:
            return "BenjaminiHochberg";
        case ValidationMethod::Unadjusted:
            return "Unadjusted";
        default:
            throw std::invalid_argument("Unknown validation method");
    }
}

std::string getValidationMethodDirectoryName(ValidationMethod method, bool sameDayExits)
{
    std::string baseName = getValidationMethodString(method);
    
    // Apply sameDayExits suffix for all validation methods so that runs with
    // different exit modes are always written to distinct directories, preventing
    // silent overwrites regardless of which validation method was chosen.
    baseName += "_SameDayExit_";
    baseName += sameDayExits ? "True" : "False";
    
    return baseName;
}

std::string getPipelineModeString(PipelineMode mode)
{
    switch (mode)
    {
        case PipelineMode::PermutationAndBootstrap:
            return "PermutationAndBootstrap";
        case PipelineMode::PermutationOnly:
            return "PermutationOnly";
        case PipelineMode::BootstrapOnly:
            return "BootstrapOnly";
        default:
            throw std::invalid_argument("Unknown pipeline mode");
    }
}

} // namespace utils
} // namespace palvalidator