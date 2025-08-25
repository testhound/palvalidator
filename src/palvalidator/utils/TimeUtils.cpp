#include "TimeUtils.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace palvalidator
{
namespace utils
{

std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%b_%d_%Y_%H%M");
    return ss.str();
}

} // namespace utils
} // namespace palvalidator