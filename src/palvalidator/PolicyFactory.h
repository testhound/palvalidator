#pragma once

#include "TemplateRegistry.h"
#include "PolicyRegistry.h"
#include "ValidationInterface.h"
#include <memory>
#include <functional>
#include <stdexcept>

namespace statistics {

/**
 * @brief Factory for creating validation objects with specific policies
 * 
 * This factory uses the policy registry and template registry to dynamically
 * create validation objects based on policy names at runtime.
 */
class PolicyFactory {
public:
    /**
     * @brief Validation wrapper factory function type
     * 
     * This function type creates a validation wrapper for a specific policy type.
     * It takes the validation parameters and returns a ValidationInterface.
     */
    template<typename PolicyType>
    using ValidationWrapperFactory = std::function<std::unique_ptr<ValidationInterface>(unsigned long)>;
    
    /**
     * @brief Create a validation object for Masters validation
     * 
     * @param policyName The name of the policy to use
     * @param permutations Number of permutations for the validation
     * @return Unique pointer to ValidationInterface
     * @throws std::invalid_argument if policy not found or not registered
     */
    static std::unique_ptr<ValidationInterface> createMastersValidation(
        const std::string& policyName,
        unsigned long permutations);
    
    /**
     * @brief Create a validation object for Romano-Wolf validation
     * 
     * @param policyName The name of the policy to use
     * @param permutations Number of permutations for the validation
     * @return Unique pointer to ValidationInterface
     * @throws std::invalid_argument if policy not found or not registered
     */
    static std::unique_ptr<ValidationInterface> createRomanoWolfValidation(
        const std::string& policyName,
        unsigned long permutations);
    
    /**
     * @brief Create a validation object for Benjamini-Hochberg validation
     *
     * @param policyName The name of the policy to use
     * @param permutations Number of permutations for the validation
     * @param falseDiscoveryRate FDR parameter for Benjamini-Hochberg
     * @return Unique pointer to ValidationInterface
     * @throws std::invalid_argument if policy not found or not registered
     */
    static std::unique_ptr<ValidationInterface> createBenjaminiHochbergValidation(
        const std::string& policyName,
        unsigned long permutations,
        double falseDiscoveryRate);
    
    /**
     * @brief Create a validation object for Unadjusted validation
     *
     * @param policyName The name of the policy to use
     * @param permutations Number of permutations for the validation
     * @return Unique pointer to ValidationInterface
     * @throws std::invalid_argument if policy not found or not registered
     */
    static std::unique_ptr<ValidationInterface> createUnadjustedValidation(
        const std::string& policyName,
        unsigned long permutations);
    
    /**
     * @brief Register a policy for Masters validation
     *
     * @tparam PolicyType The policy class type
     * @param policyName The name of the policy
     */
    template<template<typename> class PolicyType>
    static void registerMastersPolicy(const std::string& policyName) {
        mastersFactories_[policyName] = [](unsigned long permutations) -> std::unique_ptr<ValidationInterface> {
            return createMastersValidationWrapper<PolicyType>(permutations);
        };
    }
    
    /**
     * @brief Register a policy for Romano-Wolf validation
     *
     * @tparam PolicyType The policy class type
     * @param policyName The name of the policy
     */
    template<template<typename> class PolicyType>
    static void registerRomanoWolfPolicy(const std::string& policyName) {
        romanoWolfFactories_[policyName] = [](unsigned long permutations) -> std::unique_ptr<ValidationInterface> {
            return createRomanoWolfValidationWrapper<PolicyType>(permutations);
        };
    }
    
    /**
     * @brief Register a policy for Benjamini-Hochberg validation
     *
     * @tparam PolicyType The policy class type
     * @param policyName The name of the policy
     */
    template<template<typename> class PolicyType>
    static void registerBenjaminiHochbergPolicy(const std::string& policyName) {
        benjaminiHochbergFactories_[policyName] = [](unsigned long permutations, double fdr) -> std::unique_ptr<ValidationInterface> {
            return createBenjaminiHochbergValidationWrapper<PolicyType>(permutations, fdr);
        };
    }
    
    /**
     * @brief Register a policy for Unadjusted validation
     *
     * @tparam PolicyType The policy class type
     * @param policyName The name of the policy
     */
    template<template<typename> class PolicyType>
    static void registerUnadjustedPolicy(const std::string& policyName) {
        unadjustedFactories_[policyName] = [](unsigned long permutations) -> std::unique_ptr<ValidationInterface> {
            return createUnadjustedValidationWrapper<PolicyType>(permutations);
        };
    }
    
    /**
     * @brief Register a policy for all validation methods
     *
     * @tparam PolicyType The policy class type
     * @param policyName The name of the policy
     */
    template<template<typename> class PolicyType>
    static void registerPolicy(const std::string& policyName) {
        registerMastersPolicy<PolicyType>(policyName);
        registerRomanoWolfPolicy<PolicyType>(policyName);
        registerBenjaminiHochbergPolicy<PolicyType>(policyName);
        registerUnadjustedPolicy<PolicyType>(policyName);
    }
    
    /**
     * @brief Check if a policy is registered for Masters validation
     */
    static bool isMastersPolicyRegistered(const std::string& policyName) {
        return mastersFactories_.find(policyName) != mastersFactories_.end();
    }
    
    /**
     * @brief Check if a policy is registered for Romano-Wolf validation
     */
    static bool isRomanoWolfPolicyRegistered(const std::string& policyName) {
        return romanoWolfFactories_.find(policyName) != romanoWolfFactories_.end();
    }
    
    /**
     * @brief Check if a policy is registered for Benjamini-Hochberg validation
     */
    static bool isBenjaminiHochbergPolicyRegistered(const std::string& policyName) {
        return benjaminiHochbergFactories_.find(policyName) != benjaminiHochbergFactories_.end();
    }
    
    /**
     * @brief Check if a policy is registered for Unadjusted validation
     */
    static bool isUnadjustedPolicyRegistered(const std::string& policyName) {
        return unadjustedFactories_.find(policyName) != unadjustedFactories_.end();
    }
    
    /**
     * @brief Clear all registered policies (mainly for testing)
     */
    static void clear() {
        mastersFactories_.clear();
        romanoWolfFactories_.clear();
        benjaminiHochbergFactories_.clear();
        unadjustedFactories_.clear();
    }

private:
    // Factory function types for each validation method
    using MastersFactoryFunction = std::function<std::unique_ptr<ValidationInterface>(unsigned long)>;
    using RomanoWolfFactoryFunction = std::function<std::unique_ptr<ValidationInterface>(unsigned long)>;
    using BenjaminiHochbergFactoryFunction = std::function<std::unique_ptr<ValidationInterface>(unsigned long, double)>;
    using UnadjustedFactoryFunction = std::function<std::unique_ptr<ValidationInterface>(unsigned long)>;
    
    // Static factory registries for each validation method
    static std::unordered_map<std::string, MastersFactoryFunction> mastersFactories_;
    static std::unordered_map<std::string, RomanoWolfFactoryFunction> romanoWolfFactories_;
    static std::unordered_map<std::string, BenjaminiHochbergFactoryFunction> benjaminiHochbergFactories_;
    static std::unordered_map<std::string, UnadjustedFactoryFunction> unadjustedFactories_;
    
    // Template wrapper creation functions (to be implemented in .cpp)
    template<template<typename> class PolicyType>
    static std::unique_ptr<ValidationInterface> createMastersValidationWrapper(unsigned long permutations);
    
    template<template<typename> class PolicyType>
    static std::unique_ptr<ValidationInterface> createRomanoWolfValidationWrapper(unsigned long permutations);
    
    template<template<typename> class PolicyType>
    static std::unique_ptr<ValidationInterface> createBenjaminiHochbergValidationWrapper(
        unsigned long permutations, double falseDiscoveryRate);
    
    template<template<typename> class PolicyType>
    static std::unique_ptr<ValidationInterface> createUnadjustedValidationWrapper(unsigned long permutations);
};

/**
 * @brief Helper class for automatic policy factory registration
 *
 * This class is used by the REGISTER_POLICY_FACTORY macro to automatically
 * register policies with the factory when the program starts.
 */
template<template<typename> class PolicyType>
struct PolicyFactoryRegistrar {
    PolicyFactoryRegistrar(const std::string& name) {
        PolicyFactory::registerPolicy<PolicyType>(name);
    }
};

/**
 * @brief Macro for registering a policy with the factory
 *
 * Usage:
 * REGISTER_POLICY_FACTORY(RobustProfitFactorPolicy, "RobustProfitFactorPolicy");
 */
#define REGISTER_POLICY_FACTORY(PolicyType, name) \
    static ::statistics::PolicyFactoryRegistrar<PolicyType> g_##PolicyType##_factory_registrar(name);

} // namespace statistics