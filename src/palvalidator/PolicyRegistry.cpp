#include "PolicyRegistry.h"

namespace palvalidator {

// Static member definitions
std::unordered_map<std::string, PolicyMetadata> PolicyRegistry::policies_;
std::unordered_map<std::string, PolicyFactoryFunction> PolicyRegistry::factories_;

} // namespace palvalidator