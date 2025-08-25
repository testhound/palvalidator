#pragma once

#include <string>

namespace palvalidator
{
namespace utils
{

/**
 * @brief Generate a timestamp string for file naming
 * 
 * Creates a timestamp in the format "MMM_DD_YYYY_HHMM" suitable for use in filenames.
 * Example: "Aug_25_2024_1430"
 * 
 * @return Current timestamp as a formatted string
 */
std::string getCurrentTimestamp();

} // namespace utils
} // namespace palvalidator