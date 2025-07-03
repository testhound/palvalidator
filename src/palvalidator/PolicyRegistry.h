#pragma once

#include "PolicyMetadata.h"
#include "ValidationInterface.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <stdexcept>

namespace palvalidator {

/**
 * @brief Function type for creating validation objects with a specific policy
 */
using PolicyFactoryFunction = std::function<std::unique_ptr<ValidationInterface>(const void*)>;

/**
 * @brief Central registry for all computation policies
 * 
 * This class manages the registration and discovery of computation policies,
 * providing a centralized way to access policy metadata and create policy instances.
 */
class PolicyRegistry {
public:
    /**
     * @brief Register a policy type with metadata
     * 
     * @tparam PolicyType The policy class type
     * @param name The policy name (should match class name)
     * @param metadata The policy metadata
     */
    template<template<typename> class PolicyType>
    static void registerPolicy(const std::string& name, const PolicyMetadata& metadata) {
        policies_[name] = metadata;
        
        // Store a factory function for this policy type
        factories_[name] = [name](const void* wrapper) -> std::unique_ptr<ValidationInterface> {
            // This will be implemented when we create the factory system
            throw std::runtime_error("Policy factory not yet implemented for: " + name);
        };
    }
    
    /**
     * @brief Get all available policy names
     * 
     * @return Vector of policy names
     */
    static std::vector<std::string> getAvailablePolicies() {
        std::vector<std::string> names;
        names.reserve(policies_.size());
        for (const auto& pair : policies_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    /**
     * @brief Get metadata for a specific policy
     * 
     * @param name The policy name
     * @return The policy metadata
     * @throws std::invalid_argument if policy not found
     */
    static const PolicyMetadata& getPolicyMetadata(const std::string& name) {
        auto it = policies_.find(name);
        if (it == policies_.end()) {
            throw std::invalid_argument("Policy not found: " + name);
        }
        return it->second;
    }
    
    /**
     * @brief Check if a policy is available
     * 
     * @param name The policy name
     * @return true if policy is registered, false otherwise
     */
    static bool isPolicyAvailable(const std::string& name) {
        return policies_.find(name) != policies_.end();
    }
    
    /**
     * @brief Get policies by category
     * 
     * @param category The category to filter by
     * @return Vector of policy names in the specified category
     */
    static std::vector<std::string> getPoliciesByCategory(const std::string& category) {
        std::vector<std::string> result;
        for (const auto& pair : policies_) {
            if (pair.second.category == category) {
                result.push_back(pair.first);
            }
        }
        return result;
    }
    
    /**
     * @brief Get policies by tag
     * 
     * @param tag The tag to filter by
     * @return Vector of policy names that have the specified tag
     */
    static std::vector<std::string> getPoliciesByTag(const std::string& tag) {
        std::vector<std::string> result;
        for (const auto& pair : policies_) {
            if (pair.second.hasTag(tag)) {
                result.push_back(pair.first);
            }
        }
        return result;
    }
    
    /**
     * @brief Get all available categories
     * 
     * @return Vector of unique category names
     */
    static std::vector<std::string> getAvailableCategories() {
        std::vector<std::string> categories;
        for (const auto& pair : policies_) {
            const std::string& category = pair.second.category;
            if (std::find(categories.begin(), categories.end(), category) == categories.end()) {
                categories.push_back(category);
            }
        }
        return categories;
    }
    
    /**
     * @brief Filter out experimental policies
     * 
     * @param policies Vector of policy names to filter
     * @return Vector of non-experimental policy names
     */
    static std::vector<std::string> filterExperimental(const std::vector<std::string>& policies) {
        std::vector<std::string> result;
        for (const std::string& name : policies) {
            auto it = policies_.find(name);
            if (it != policies_.end() && !it->second.isExperimental) {
                result.push_back(name);
            }
        }
        return result;
    }
    
    /**
     * @brief Get factory function for a policy
     * 
     * @param name The policy name
     * @return The factory function
     * @throws std::invalid_argument if policy not found
     */
    static const PolicyFactoryFunction& getFactoryFunction(const std::string& name) {
        auto it = factories_.find(name);
        if (it == factories_.end()) {
            throw std::invalid_argument("Policy factory not found: " + name);
        }
        return it->second;
    }
    
    /**
     * @brief Clear all registered policies (mainly for testing)
     */
    static void clear() {
        policies_.clear();
        factories_.clear();
    }
    
    /**
     * @brief Get total number of registered policies
     */
    static size_t size() {
        return policies_.size();
    }

private:
    static std::unordered_map<std::string, PolicyMetadata> policies_;
    static std::unordered_map<std::string, PolicyFactoryFunction> factories_;
};

/**
 * @brief Helper class for automatic policy registration
 * 
 * This class is used by the REGISTER_POLICY macro to automatically
 * register policies when the program starts.
 */
template<template<typename> class PolicyType>
struct PolicyRegistrar {
    PolicyRegistrar(const std::string& name, const PolicyMetadata& metadata) {
        PolicyRegistry::registerPolicy<PolicyType>(name, metadata);
    }
};

/**
 * @brief Macro for registering a policy with metadata
 * 
 * Usage:
 * REGISTER_POLICY(RobustProfitFactorPolicy, "RobustProfitFactorPolicy", 
 *                 PolicyMetadata("RobustProfitFactorPolicy", "Robust Profit Factor", 
 *                               "Robust profit factor with outlier handling", "basic"));
 */
#define REGISTER_POLICY(PolicyType, name, metadata) \
    static ::palvalidator::PolicyRegistrar<PolicyType> g_##PolicyType##_registrar(name, metadata);

} // namespace palvalidator