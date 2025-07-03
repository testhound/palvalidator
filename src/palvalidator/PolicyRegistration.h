#pragma once

#include "PolicyRegistry.h"
#include "PolicyFactory.h"
#include "MonteCarloTestPolicy.h"

namespace statistics {

/**
 * @brief Initialize and register all available computation policies
 * 
 * This function registers all policy classes with their metadata
 * and factory functions. It should be called once at program startup.
 */
void initializePolicyRegistry();

/**
 * @brief Register a specific policy with comprehensive metadata
 * 
 * @tparam PolicyType The policy class type
 * @param name Policy name (should match class name)
 * @param displayName Human-readable display name
 * @param description Detailed description
 * @param category Policy category
 * @param isExperimental Whether the policy is experimental
 * @param version Policy version
 * @param author Policy author
 * @param tags List of tags for categorization
 * @param requirements List of requirements
 */
template<template<typename> class PolicyType>
void registerPolicyWithMetadata(
    const std::string& name,
    const std::string& displayName,
    const std::string& description,
    const std::string& category = "basic",
    bool isExperimental = false,
    const std::string& version = "1.0.0",
    const std::string& author = "",
    const std::vector<std::string>& tags = {},
    const std::vector<std::string>& requirements = {})
{
    // Create metadata
    palvalidator::PolicyMetadata metadata(name, displayName, description, category, isExperimental);
    metadata.version = version;
    metadata.author = author;
    for (const auto& tag : tags) {
        metadata.addTag(tag);
    }
    for (const auto& req : requirements) {
        metadata.addRequirement(req);
    }
    
    // Register with policy registry
    palvalidator::PolicyRegistry::registerPolicy<PolicyType>(name, metadata);
    
    // Register with policy factory
    PolicyFactory::registerPolicy<PolicyType>(name);
}

} // namespace statistics