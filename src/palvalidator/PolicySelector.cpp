#include "PolicySelector.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <limits>

namespace statistics {

std::string PolicySelector::selectPolicy(const std::vector<std::string>& availablePolicies,
                                         const palvalidator::PolicyConfiguration* config)
{
    if (availablePolicies.empty()) {
        throw std::runtime_error("No policies available for selection");
    }
    
    std::cout << "\n=== Policy Selection ===" << std::endl;
    
    // Filter policies based on configuration if provided
    std::vector<std::string> filteredPolicies = availablePolicies;
    if (config) {
        // Only show enabled policies from configuration
        std::vector<std::string> enabledPolicies;
        for (const std::string& policy : availablePolicies) {
            if (config->isPolicyEnabled(policy)) {
                enabledPolicies.push_back(policy);
            }
        }
        if (!enabledPolicies.empty()) {
            filteredPolicies = enabledPolicies;
        }
        
        // Apply configuration settings
        const auto& settings = config->getPolicySettings();
        if (settings.filterExperimental) {
            filteredPolicies = filterExperimentalPolicies(filteredPolicies);
        }
        filteredPolicies = sortPolicies(filteredPolicies, settings.sortBy);
    }
    
    if (filteredPolicies.empty()) {
        std::cout << "No enabled policies found in configuration. Using all available policies." << std::endl;
        filteredPolicies = availablePolicies;
    }
    
    // Show policy recommendations
    displayPolicyRecommendations(filteredPolicies);
    
    // Display policy menu
    bool showDescriptions = config ? config->getPolicySettings().showDescriptions : true;
    displayPolicyMenu(filteredPolicies, showDescriptions);
    
    // Get user choice
    int choice = getUserChoice("Select a policy", filteredPolicies, 1);
    
    if (choice >= 0 && choice < static_cast<int>(filteredPolicies.size())) {
        std::string selectedPolicy = filteredPolicies[choice];
        std::cout << "\nSelected policy: " << getPolicyDisplayName(selectedPolicy) << std::endl;
        displayPolicyInfo(selectedPolicy);
        return selectedPolicy;
    }
    
    throw std::runtime_error("Invalid policy selection");
}

std::string PolicySelector::selectPolicyFromCategory(const std::string& category)
{
    auto availablePolicies = palvalidator::PolicyRegistry::getAvailablePolicies();
    auto categoryPolicies = filterPoliciesByCategory(availablePolicies, category);
    
    if (categoryPolicies.empty()) {
        throw std::runtime_error("No policies available in category: " + category);
    }
    
    std::cout << "\n=== Policies in Category: " << category << " ===" << std::endl;
    return selectPolicy(categoryPolicies);
}

std::string PolicySelector::selectPolicyFromGroup(const palvalidator::PolicyConfiguration& config,
                                                  const std::string& groupName)
{
    auto groupPolicies = config.getPoliciesInGroup(groupName);
    
    if (groupPolicies.empty()) {
        throw std::runtime_error("No policies available in group: " + groupName);
    }
    
    std::cout << "\n=== Policies in Group: " << groupName << " ===" << std::endl;
    return selectPolicy(groupPolicies, &config);
}

void PolicySelector::displayPolicyInfo(const std::string& policyName)
{
    if (!palvalidator::PolicyRegistry::isPolicyAvailable(policyName)) {
        std::cout << "Policy not found: " << policyName << std::endl;
        return;
    }
    
    try {
        const auto& metadata = palvalidator::PolicyRegistry::getPolicyMetadata(policyName);
        
        std::cout << "\n--- Policy Information ---" << std::endl;
        std::cout << "Name: " << metadata.name << std::endl;
        std::cout << "Display Name: " << metadata.displayName << std::endl;
        std::cout << "Description: " << metadata.description << std::endl;
        std::cout << "Category: " << metadata.category << std::endl;
        std::cout << "Version: " << metadata.version << std::endl;
        
        if (!metadata.author.empty()) {
            std::cout << "Author: " << metadata.author << std::endl;
        }
        
        if (metadata.isExperimental) {
            std::cout << "Status: EXPERIMENTAL" << std::endl;
        }
        
        if (!metadata.tags.empty()) {
            std::cout << "Tags: ";
            for (size_t i = 0; i < metadata.tags.size(); ++i) {
                std::cout << metadata.tags[i];
                if (i < metadata.tags.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
        
        if (!metadata.requirements.empty()) {
            std::cout << "Requirements: ";
            for (size_t i = 0; i < metadata.requirements.size(); ++i) {
                std::cout << metadata.requirements[i];
                if (i < metadata.requirements.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "-------------------------" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error retrieving policy information: " << e.what() << std::endl;
    }
}

void PolicySelector::displayPolicyMenu(const std::vector<std::string>& policies,
                                       bool showDescriptions)
{
    std::cout << "\nAvailable Policies:" << std::endl;
    displayNumberedPolicyList(policies, showDescriptions);
}

void PolicySelector::displayPolicyGroups(const palvalidator::PolicyConfiguration& config)
{
    const auto& groups = config.getPolicyGroups();
    
    if (groups.empty()) {
        std::cout << "No policy groups defined in configuration." << std::endl;
        return;
    }
    
    std::cout << "\n=== Policy Groups ===" << std::endl;
    int groupIndex = 1;
    for (const auto& [groupName, group] : groups) {
        std::cout << groupIndex << ". " << groupName;
        if (!group.description.empty()) {
            std::cout << " - " << group.description;
        }
        std::cout << " (" << group.policies.size() << " policies)" << std::endl;
        groupIndex++;
    }
    std::cout << "=====================" << std::endl;
}

std::vector<std::string> PolicySelector::filterPoliciesByCategory(
    const std::vector<std::string>& policies,
    const std::string& category)
{
    std::vector<std::string> filtered;
    for (const std::string& policy : policies) {
        if (palvalidator::PolicyRegistry::isPolicyAvailable(policy)) {
            try {
                const auto& metadata = palvalidator::PolicyRegistry::getPolicyMetadata(policy);
                if (metadata.category == category) {
                    filtered.push_back(policy);
                }
            } catch (const std::exception&) {
                // Skip policies with missing metadata
            }
        }
    }
    return filtered;
}

std::vector<std::string> PolicySelector::filterPoliciesByTag(
    const std::vector<std::string>& policies,
    const std::string& tag)
{
    std::vector<std::string> filtered;
    for (const std::string& policy : policies) {
        if (palvalidator::PolicyRegistry::isPolicyAvailable(policy)) {
            try {
                const auto& metadata = palvalidator::PolicyRegistry::getPolicyMetadata(policy);
                if (metadata.hasTag(tag)) {
                    filtered.push_back(policy);
                }
            } catch (const std::exception&) {
                // Skip policies with missing metadata
            }
        }
    }
    return filtered;
}

std::vector<std::string> PolicySelector::filterExperimentalPolicies(
    const std::vector<std::string>& policies)
{
    std::vector<std::string> filtered;
    for (const std::string& policy : policies) {
        if (palvalidator::PolicyRegistry::isPolicyAvailable(policy)) {
            try {
                const auto& metadata = palvalidator::PolicyRegistry::getPolicyMetadata(policy);
                if (!metadata.isExperimental) {
                    filtered.push_back(policy);
                }
            } catch (const std::exception&) {
                // Include policies with missing metadata (assume non-experimental)
                filtered.push_back(policy);
            }
        }
    }
    return filtered;
}

std::vector<std::string> PolicySelector::sortPolicies(
    const std::vector<std::string>& policies,
    const std::string& sortBy)
{
    std::vector<std::string> sorted = policies;
    
    if (sortBy == "name") {
        std::sort(sorted.begin(), sorted.end());
    } else if (sortBy == "category") {
        std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
            try {
                const auto& metaA = palvalidator::PolicyRegistry::getPolicyMetadata(a);
                const auto& metaB = palvalidator::PolicyRegistry::getPolicyMetadata(b);
                if (metaA.category != metaB.category) {
                    return metaA.category < metaB.category;
                }
                return a < b; // Secondary sort by name
            } catch (const std::exception&) {
                return a < b; // Fallback to name sort
            }
        });
    } else if (sortBy == "version") {
        std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
            try {
                const auto& metaA = palvalidator::PolicyRegistry::getPolicyMetadata(a);
                const auto& metaB = palvalidator::PolicyRegistry::getPolicyMetadata(b);
                if (metaA.version != metaB.version) {
                    return metaA.version < metaB.version;
                }
                return a < b; // Secondary sort by name
            } catch (const std::exception&) {
                return a < b; // Fallback to name sort
            }
        });
    }
    
    return sorted;
}

std::string PolicySelector::getUserInput(const std::string& prompt,
                                         const std::string& defaultValue)
{
    std::string input;
    std::cout << prompt;
    if (!defaultValue.empty()) {
        std::cout << " (default: " << defaultValue << ")";
    }
    std::cout << ": ";
    
    std::getline(std::cin, input);
    
    if (input.empty() && !defaultValue.empty()) {
        return defaultValue;
    }
    
    return input;
}

int PolicySelector::getUserChoice(const std::string& prompt,
                                 const std::vector<std::string>& options,
                                 int defaultChoice)
{
    std::string input;
    int choice = -1;
    
    while (choice < 0) {
        std::cout << "\n" << prompt;
        if (defaultChoice > 0 && defaultChoice <= static_cast<int>(options.size())) {
            std::cout << " (default: " << defaultChoice << ")";
        }
        std::cout << ": ";
        
        std::getline(std::cin, input);
        
        if (input.empty() && defaultChoice > 0 && defaultChoice <= static_cast<int>(options.size())) {
            choice = defaultChoice - 1; // Convert to 0-based
        } else {
            choice = validateChoice(input, static_cast<int>(options.size()));
            if (choice >= 0) {
                choice--; // Convert to 0-based
            }
        }
        
        if (choice < 0) {
            std::cout << "Invalid choice. Please enter a number between 1 and " 
                      << options.size() << "." << std::endl;
        }
    }
    
    return choice;
}

void PolicySelector::displayPolicyRecommendations(const std::vector<std::string>& availablePolicies)
{
    std::cout << "\n--- Policy Recommendations ---" << std::endl;
    
    // Find recommended policies
    std::vector<std::string> recommended;
    std::vector<std::string> basic;
    std::vector<std::string> advanced;
    
    for (const std::string& policy : availablePolicies) {
        if (policy == "BootStrappedLogProfitFactorPolicy") {
            recommended.push_back(policy + " (Recommended for most users)");
        } else if (policy == "RobustProfitFactorPolicy") {
            basic.push_back(policy + " (Good for beginners)");
        } else if (policy == "AllHighResLogPFPolicy") {
            basic.push_back(policy + " (High-resolution analysis)");
        } else if (policy.find("Enhanced") != std::string::npos || 
                   policy.find("Hybrid") != std::string::npos) {
            advanced.push_back(policy + " (Advanced users)");
        }
    }
    
    if (!recommended.empty()) {
        std::cout << "Recommended: " << recommended[0] << std::endl;
    }
    
    if (!basic.empty()) {
        std::cout << "For beginners: ";
        for (size_t i = 0; i < basic.size(); ++i) {
            std::cout << basic[i];
            if (i < basic.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    
    if (!advanced.empty() && advanced.size() <= 3) {
        std::cout << "Advanced options: ";
        for (size_t i = 0; i < advanced.size(); ++i) {
            std::cout << advanced[i];
            if (i < advanced.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "------------------------------" << std::endl;
}

void PolicySelector::displayNumberedPolicyList(const std::vector<std::string>& policies,
                                               bool showDescriptions,
                                               int startIndex)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        const std::string& policy = policies[i];
        std::cout << std::setw(3) << (startIndex + i) << ". " << getPolicyDisplayName(policy);
        
        if (showDescriptions && palvalidator::PolicyRegistry::isPolicyAvailable(policy)) {
            try {
                const auto& metadata = palvalidator::PolicyRegistry::getPolicyMetadata(policy);
                if (!metadata.description.empty()) {
                    std::cout << " - " << metadata.description;
                }
                if (metadata.isExperimental) {
                    std::cout << " [EXPERIMENTAL]";
                }
            } catch (const std::exception&) {
                // Skip description if metadata not available
            }
        }
        
        std::cout << std::endl;
    }
}

std::string PolicySelector::getPolicyDisplayName(const std::string& policyName)
{
    if (palvalidator::PolicyRegistry::isPolicyAvailable(policyName)) {
        try {
            const auto& metadata = palvalidator::PolicyRegistry::getPolicyMetadata(policyName);
            if (!metadata.displayName.empty()) {
                return metadata.displayName;
            }
        } catch (const std::exception&) {
            // Fall through to return policy name
        }
    }
    return policyName;
}

int PolicySelector::validateChoice(const std::string& input, int maxChoice)
{
    try {
        int choice = std::stoi(input);
        if (choice >= 1 && choice <= maxChoice) {
            return choice;
        }
    } catch (const std::exception&) {
        // Invalid input
    }
    return -1;
}

} // namespace statistics
