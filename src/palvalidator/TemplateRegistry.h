#pragma once

#include "PolicyRegistry.h"
#include "ValidationInterface.h"
#include <functional>
#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace statistics {

/**
 * @brief Template registry for type-safe policy instantiation
 * 
 * This class maintains type information for runtime instantiation of
 * templated validation classes with different policy types.
 */
class TemplateRegistry {
public:
    /**
     * @brief Function type for creating validation wrappers
     * 
     * Takes a validation wrapper factory function and returns a ValidationInterface
     */
    template<typename ValidationWrapperType>
    using ValidationFactory = std::function<std::unique_ptr<ValidationInterface>(const ValidationWrapperType&)>;
    
    /**
     * @brief Generic instantiation function type
     */
    using GenericInstantiationFunction = std::function<std::unique_ptr<ValidationInterface>(const void*)>;
    
    /**
     * @brief Register a policy template for runtime instantiation
     * 
     * @tparam PolicyType The policy class type
     * @param policyName The name of the policy
     */
    template<typename PolicyType>
    static void registerTemplate(const std::string& policyName) {
        instantiators_[policyName] = [policyName](const void* wrapperPtr) -> std::unique_ptr<ValidationInterface> {
            // This will be called by the factory with the appropriate wrapper
            throw std::runtime_error("Template instantiation not yet implemented for: " + policyName);
        };
    }
    
    /**
     * @brief Create a validation instance for a specific policy and wrapper
     * 
     * @tparam ValidationWrapperType The type of validation wrapper
     * @param policyName The name of the policy to instantiate
     * @param wrapperFactory Factory function that creates the validation wrapper
     * @return Unique pointer to ValidationInterface
     */
    template<typename ValidationWrapperType>
    static std::unique_ptr<ValidationInterface> instantiate(
        const std::string& policyName,
        const ValidationWrapperType& wrapperFactory) {
        
        auto it = instantiators_.find(policyName);
        if (it == instantiators_.end()) {
            throw std::invalid_argument("Policy template not registered: " + policyName);
        }
        
        // Call the registered instantiation function with the wrapper
        return it->second(&wrapperFactory);
    }
    
    /**
     * @brief Check if a policy template is registered
     */
    static bool isTemplateRegistered(const std::string& policyName) {
        return instantiators_.find(policyName) != instantiators_.end();
    }
    
    /**
     * @brief Get all registered template names
     */
    static std::vector<std::string> getRegisteredTemplates() {
        std::vector<std::string> names;
        names.reserve(instantiators_.size());
        for (const auto& pair : instantiators_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    /**
     * @brief Clear all registered templates (mainly for testing)
     */
    static void clear() {
        instantiators_.clear();
    }
    
    /**
     * @brief Get total number of registered templates
     */
    static size_t size() {
        return instantiators_.size();
    }

private:
    static std::unordered_map<std::string, GenericInstantiationFunction> instantiators_;
};

/**
 * @brief Helper class for automatic template registration
 * 
 * This class is used by the REGISTER_POLICY_TEMPLATE macro to automatically
 * register policy templates when the program starts.
 */
template<typename PolicyType>
struct PolicyTemplateRegistrar {
    PolicyTemplateRegistrar(const std::string& name) {
        TemplateRegistry::registerTemplate<PolicyType>(name);
    }
};

/**
 * @brief Macro for registering a policy template
 * 
 * Usage:
 * REGISTER_POLICY_TEMPLATE(RobustProfitFactorPolicy, "RobustProfitFactorPolicy");
 */
#define REGISTER_POLICY_TEMPLATE(PolicyType, name) \
    static ::palvalidator::PolicyTemplateRegistrar<PolicyType> g_##PolicyType##_template_registrar(name);

} // namespace statistics