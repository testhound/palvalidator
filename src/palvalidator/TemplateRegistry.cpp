#include "TemplateRegistry.h"

namespace statistics {

// Static member definition
std::unordered_map<std::string, TemplateRegistry::GenericInstantiationFunction> TemplateRegistry::instantiators_;

} // namespace statistics