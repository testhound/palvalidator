#pragma once

#include "PolicyRegistry.h"
#include "PolicyConfiguration.h"
#include <string>
#include <vector>
#include <iostream>

namespace statistics {

/**
 * @brief Interactive policy selector for user-friendly policy selection
 * 
 * This class provides a command-line interface for users to select
 * computation policies from the available registered policies.
 */
class PolicySelector {
public:
    /**
     * @brief Select a policy interactively from available policies
     * 
     * @param availablePolicies List of available policy names
     * @param config Optional configuration to filter policies
     * @return Selected policy name
     */
    static std::string selectPolicy(const std::vector<std::string>& availablePolicies,
                                   const palvalidator::PolicyConfiguration* config = nullptr);
    
    /**
     * @brief Select a policy from a specific category
     * 
     * @param category The category to filter by
     * @return Selected policy name
     */
    static std::string selectPolicyFromCategory(const std::string& category);
    
    /**
     * @brief Select a policy from a configuration group
     * 
     * @param config The configuration containing groups
     * @param groupName The group name to select from
     * @return Selected policy name
     */
    static std::string selectPolicyFromGroup(const palvalidator::PolicyConfiguration& config,
                                            const std::string& groupName);
    
    /**
     * @brief Display detailed information about a policy
     * 
     * @param policyName The name of the policy
     */
    static void displayPolicyInfo(const std::string& policyName);
    
    /**
     * @brief Display a menu of available policies
     * 
     * @param policies List of policy names to display
     * @param showDescriptions Whether to show policy descriptions
     */
    static void displayPolicyMenu(const std::vector<std::string>& policies,
                                 bool showDescriptions = true);
    
    /**
     * @brief Display available policy groups from configuration
     * 
     * @param config The configuration containing groups
     */
    static void displayPolicyGroups(const palvalidator::PolicyConfiguration& config);
    
    /**
     * @brief Filter policies by category
     * 
     * @param policies List of policy names to filter
     * @param category The category to filter by
     * @return Filtered list of policy names
     */
    static std::vector<std::string> filterPoliciesByCategory(
        const std::vector<std::string>& policies,
        const std::string& category);
    
    /**
     * @brief Filter policies by tag
     * 
     * @param policies List of policy names to filter
     * @param tag The tag to filter by
     * @return Filtered list of policy names
     */
    static std::vector<std::string> filterPoliciesByTag(
        const std::vector<std::string>& policies,
        const std::string& tag);
    
    /**
     * @brief Filter out experimental policies
     * 
     * @param policies List of policy names to filter
     * @return Filtered list of non-experimental policy names
     */
    static std::vector<std::string> filterExperimentalPolicies(
        const std::vector<std::string>& policies);
    
    /**
     * @brief Sort policies by different criteria
     * 
     * @param policies List of policy names to sort
     * @param sortBy Sort criteria ("name", "category", "version")
     * @return Sorted list of policy names
     */
    static std::vector<std::string> sortPolicies(
        const std::vector<std::string>& policies,
        const std::string& sortBy = "name");
    
    /**
     * @brief Get user input with validation
     * 
     * @param prompt The prompt to display
     * @param defaultValue Default value if user enters nothing
     * @return User input or default value
     */
    static std::string getUserInput(const std::string& prompt,
                                   const std::string& defaultValue = "");
    
    /**
     * @brief Get user choice from a numbered list
     * 
     * @param prompt The prompt to display
     * @param options List of options
     * @param defaultChoice Default choice index (1-based)
     * @return Selected option index (0-based)
     */
    static int getUserChoice(const std::string& prompt,
                            const std::vector<std::string>& options,
                            int defaultChoice = 1);
    
    /**
     * @brief Display policy recommendations based on use case
     * 
     * @param availablePolicies List of available policies
     */
    static void displayPolicyRecommendations(const std::vector<std::string>& availablePolicies);

private:
    /**
     * @brief Display a formatted policy list with numbers
     * 
     * @param policies List of policy names
     * @param showDescriptions Whether to show descriptions
     * @param startIndex Starting index for numbering
     */
    static void displayNumberedPolicyList(const std::vector<std::string>& policies,
                                         bool showDescriptions = true,
                                         int startIndex = 1);
    
    /**
     * @brief Get policy display name or fallback to policy name
     * 
     * @param policyName The policy name
     * @return Display name or policy name
     */
    static std::string getPolicyDisplayName(const std::string& policyName);
    
    /**
     * @brief Validate user choice input
     * 
     * @param input User input string
     * @param maxChoice Maximum valid choice
     * @return Validated choice or -1 if invalid
     */
    static int validateChoice(const std::string& input, int maxChoice);
};

} // namespace statistics