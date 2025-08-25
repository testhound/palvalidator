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

} // namespace utils
} // namespace palvalidator