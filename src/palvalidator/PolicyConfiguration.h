#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace palvalidator {

/**
 * @brief Configuration for a policy group
 */
struct PolicyGroup {
    std::vector<std::string> policies;
    std::string description;
    
    PolicyGroup() = default;
    PolicyGroup(const std::vector<std::string>& policies, const std::string& description)
        : policies(policies), description(description) {}
};

/**
 * @brief Settings for policy selection behavior
 */
struct PolicySettings {
    bool showDescriptions = true;
    bool allowMultipleSelection = false;
    std::string sortBy = "name";  // "name", "category", "version"
    bool filterExperimental = false;
    bool interactiveMode = true;
    
    PolicySettings() = default;
};

/**
 * @brief Main configuration class for computation policies
 * 
 * Handles loading and parsing of policy configuration from JSON files,
 * providing access to enabled policies, groups, and settings.
 */
class PolicyConfiguration {
public:
    /**
     * @brief Default constructor with sensible defaults
     */
    PolicyConfiguration();
    
    /**
     * @brief Load configuration from JSON file
     * 
     * @param configPath Path to the configuration file
     * @return true if loaded successfully, false otherwise
     */
    bool loadFromFile(const std::string& configPath);
    
    /**
     * @brief Load configuration from JSON string
     * 
     * @param jsonContent JSON content as string
     * @return true if parsed successfully, false otherwise
     */
    bool loadFromString(const std::string& jsonContent);
    
    /**
     * @brief Save current configuration to JSON file
     * 
     * @param configPath Path to save the configuration
     * @return true if saved successfully, false otherwise
     */
    bool saveToFile(const std::string& configPath) const;
    
    /**
     * @brief Get enabled policy names
     */
    const std::vector<std::string>& getEnabledPolicies() const { return enabledPolicies_; }
    
    /**
     * @brief Set enabled policy names
     */
    void setEnabledPolicies(const std::vector<std::string>& policies) { enabledPolicies_ = policies; }
    
    /**
     * @brief Get default policy name
     */
    const std::string& getDefaultPolicy() const { return defaultPolicy_; }
    
    /**
     * @brief Set default policy name
     */
    void setDefaultPolicy(const std::string& policy) { defaultPolicy_ = policy; }
    
    /**
     * @brief Get policy groups
     */
    const std::unordered_map<std::string, PolicyGroup>& getPolicyGroups() const { return policyGroups_; }
    
    /**
     * @brief Add a policy group
     */
    void addPolicyGroup(const std::string& name, const PolicyGroup& group) {
        policyGroups_[name] = group;
    }
    
    /**
     * @brief Get policy settings
     */
    const PolicySettings& getPolicySettings() const { return policySettings_; }
    
    /**
     * @brief Set policy settings
     */
    void setPolicySettings(const PolicySettings& settings) { policySettings_ = settings; }
    
    /**
     * @brief Check if a policy is enabled
     */
    bool isPolicyEnabled(const std::string& policyName) const;
    
    /**
     * @brief Get policies in a specific group
     */
    std::vector<std::string> getPoliciesInGroup(const std::string& groupName) const;
    
    /**
     * @brief Get all group names
     */
    std::vector<std::string> getGroupNames() const;
    
    /**
     * @brief Validate configuration against available policies
     * 
     * @param availablePolicies List of available policy names
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validate(const std::vector<std::string>& availablePolicies) const;
    
    /**
     * @brief Get last error message
     */
    const std::string& getLastError() const { return lastError_; }
    
    /**
     * @brief Create default configuration
     */
    static PolicyConfiguration createDefault();
    
    /**
     * @brief Create configuration with all available policies
     */
    static PolicyConfiguration createWithAllPolicies(const std::vector<std::string>& availablePolicies);

private:
    std::vector<std::string> enabledPolicies_;
    std::string defaultPolicy_;
    std::unordered_map<std::string, PolicyGroup> policyGroups_;
    PolicySettings policySettings_;
    mutable std::string lastError_;
    
    /**
     * @brief Parse JSON content into configuration
     */
    bool parseJson(const std::string& jsonContent);
    
    /**
     * @brief Convert configuration to JSON string
     */
    std::string toJsonString() const;
    
    /**
     * @brief Set error message
     */
    void setError(const std::string& error) const { lastError_ = error; }
};

} // namespace palvalidator