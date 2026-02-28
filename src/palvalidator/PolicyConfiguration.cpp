#include "PolicyConfiguration.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace palvalidator {

PolicyConfiguration::PolicyConfiguration() {
    // Set sensible defaults
    policySettings_ = PolicySettings();
    defaultPolicy_ = "";
}

bool PolicyConfiguration::loadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        setError("Could not open configuration file: " + configPath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return loadFromString(buffer.str());
}

bool PolicyConfiguration::loadFromString(const std::string& jsonContent) {
    return parseJson(jsonContent);
}

bool PolicyConfiguration::saveToFile(const std::string& configPath) const {
    std::ofstream file(configPath);
    if (!file.is_open()) {
        setError("Could not open file for writing: " + configPath);
        return false;
    }
    
    file << toJsonString();
    file.close();
    return true;
}

bool PolicyConfiguration::isPolicyEnabled(const std::string& policyName) const {
    return std::find(enabledPolicies_.begin(), enabledPolicies_.end(), policyName) != enabledPolicies_.end();
}

std::vector<std::string> PolicyConfiguration::getPoliciesInGroup(const std::string& groupName) const {
    auto it = policyGroups_.find(groupName);
    if (it != policyGroups_.end()) {
        return it->second.policies;
    }
    return {};
}

std::vector<std::string> PolicyConfiguration::getGroupNames() const {
    std::vector<std::string> names;
    names.reserve(policyGroups_.size());
    for (const auto& pair : policyGroups_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> PolicyConfiguration::validate(const std::vector<std::string>& availablePolicies) const {
    std::vector<std::string> errors;
    
    // Check if enabled policies are available
    for (const std::string& policy : enabledPolicies_) {
        if (std::find(availablePolicies.begin(), availablePolicies.end(), policy) == availablePolicies.end()) {
            errors.push_back("Enabled policy not available: " + policy);
        }
    }
    
    // Check if default policy is available and enabled
    if (!defaultPolicy_.empty()) {
        if (std::find(availablePolicies.begin(), availablePolicies.end(), defaultPolicy_) == availablePolicies.end()) {
            errors.push_back("Default policy not available: " + defaultPolicy_);
        } else if (!isPolicyEnabled(defaultPolicy_)) {
            errors.push_back("Default policy not enabled: " + defaultPolicy_);
        }
    }
    
    // Check policies in groups
    for (const auto& groupPair : policyGroups_) {
        for (const std::string& policy : groupPair.second.policies) {
            if (std::find(availablePolicies.begin(), availablePolicies.end(), policy) == availablePolicies.end()) {
                errors.push_back("Policy in group '" + groupPair.first + "' not available: " + policy);
            }
        }
    }
    
    return errors;
}

PolicyConfiguration PolicyConfiguration::createDefault() {
    PolicyConfiguration config;
    
    // Default enabled policies (including Bootstrap policies)
    config.enabledPolicies_ = {
        "RobustProfitFactorPolicy",
        "AllHighResLogPFPolicy",
        "GatedPerformanceScaledPalPolicy",
        "BootStrappedProfitFactorPolicy",
        "BootStrappedLogProfitFactorPolicy",
        "BootStrappedProfitabilityPFPolicy",
        "BootStrappedLogProfitabilityPFPolicy",
        "BootStrappedSharpeRatioPolicy"
    };
    
    config.defaultPolicy_ = "BootStrappedLogProfitFactorPolicy";
    
    // Set policy settings to show experimental policies by default
    config.policySettings_.filterExperimental = false;
    config.policySettings_.showDescriptions = true;
    config.policySettings_.interactiveMode = false;
    
    // Create default groups
    config.policyGroups_["recommended"] = PolicyGroup(
        {"BootStrappedProfitFactorPolicy", "BootStrappedLogProfitFactorPolicy", "BootStrappedProfitabilityPFPolicy", "BootStrappedLogProfitabilityPFPolicy", "BootStrappedSharpeRatioPolicy"},
        "Primary bootstrap-based policies for robust statistical analysis"
    );
    
    config.policyGroups_["basic"] = PolicyGroup(
        {"BootStrappedProfitFactorPolicy", "BootStrappedLogProfitFactorPolicy", "BootStrappedProfitabilityPFPolicy", "BootStrappedLogProfitabilityPFPolicy", "RobustProfitFactorPolicy", "AllHighResLogPFPolicy"},
        "Bootstrap and basic profit factor policies for standard analysis"
    );
    
    config.policyGroups_["advanced"] = PolicyGroup(
        {"GatedPerformanceScaledPalPolicy"},
        "Advanced PAL analysis with performance gating and scaling"
    );
    
    config.policyGroups_["experimental"] = PolicyGroup(
        {},
        "Experimental policies for testing new approaches"
    );
    
    return config;
}

PolicyConfiguration PolicyConfiguration::createWithAllPolicies(const std::vector<std::string>& availablePolicies) {
    PolicyConfiguration config;
    
    config.enabledPolicies_ = availablePolicies;
    if (!availablePolicies.empty()) {
        config.defaultPolicy_ = availablePolicies[0];
    }
    
    // Group policies by category (this is a simple heuristic)
    std::vector<std::string> basicPolicies;
    std::vector<std::string> advancedPolicies;
    std::vector<std::string> experimentalPolicies;
    
    for (const std::string& policy : availablePolicies) {
        if (policy.find("Basic") != std::string::npos ||
            policy.find("Simple") != std::string::npos ||
            policy == "RobustProfitFactorPolicy" ||
            policy == "AllHighResLogPFPolicy" ||
            policy.find("Bootstrap") != std::string::npos ||
            policy.find("BootStrapped") != std::string::npos) {
            basicPolicies.push_back(policy);
        } else if (policy.find("Experimental") != std::string::npos) {
            experimentalPolicies.push_back(policy);
        } else {
            advancedPolicies.push_back(policy);
        }
    }
    
    if (!basicPolicies.empty()) {
        config.policyGroups_["basic"] = PolicyGroup(basicPolicies, "Basic policies");
    }
    if (!advancedPolicies.empty()) {
        config.policyGroups_["advanced"] = PolicyGroup(advancedPolicies, "Advanced policies");
    }
    if (!experimentalPolicies.empty()) {
        config.policyGroups_["experimental"] = PolicyGroup(experimentalPolicies, "Experimental policies");
    }
    
    return config;
}

// Simple JSON parser implementation
bool PolicyConfiguration::parseJson(const std::string& jsonContent) {
    // This is a simplified JSON parser for our specific use case
    // In a production system, you might want to use a proper JSON library
    
    try {
        // Clear existing data
        enabledPolicies_.clear();
        policyGroups_.clear();
        defaultPolicy_.clear();
        
        // Find computation_policies section
        size_t policiesStart = jsonContent.find("\"computation_policies\"");
        if (policiesStart == std::string::npos) {
            setError("Missing 'computation_policies' section in configuration");
            return false;
        }
        
        // Parse enabled policies
        size_t enabledStart = jsonContent.find("\"enabled\"", policiesStart);
        if (enabledStart != std::string::npos) {
            size_t arrayStart = jsonContent.find("[", enabledStart);
            size_t arrayEnd = jsonContent.find("]", arrayStart);
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                std::string arrayContent = jsonContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                
                // Parse array elements
                size_t pos = 0;
                while (pos < arrayContent.length()) {
                    size_t quoteStart = arrayContent.find("\"", pos);
                    if (quoteStart == std::string::npos) break;
                    
                    size_t quoteEnd = arrayContent.find("\"", quoteStart + 1);
                    if (quoteEnd == std::string::npos) break;
                    
                    std::string policy = arrayContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    enabledPolicies_.push_back(policy);
                    
                    pos = quoteEnd + 1;
                }
            }
        }
        
        // Parse default policy
        size_t defaultStart = jsonContent.find("\"default\"", policiesStart);
        if (defaultStart != std::string::npos) {
            size_t quoteStart = jsonContent.find("\"", defaultStart + 9);
            if (quoteStart != std::string::npos) {
                size_t quoteEnd = jsonContent.find("\"", quoteStart + 1);
                if (quoteEnd != std::string::npos) {
                    defaultPolicy_ = jsonContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Parse groups
        size_t groupsStart = jsonContent.find("\"groups\"", policiesStart);
        if (groupsStart != std::string::npos) {
            size_t groupsObjectStart = jsonContent.find("{", groupsStart);
            if (groupsObjectStart != std::string::npos) {
                // Find the matching closing brace for the groups object
                int braceCount = 1;
                size_t pos = groupsObjectStart + 1;
                size_t groupsObjectEnd = std::string::npos;
                
                while (pos < jsonContent.length() && braceCount > 0) {
                    if (jsonContent[pos] == '{') {
                        braceCount++;
                    } else if (jsonContent[pos] == '}') {
                        braceCount--;
                        if (braceCount == 0) {
                            groupsObjectEnd = pos;
                            break;
                        }
                    }
                    pos++;
                }
                
                if (groupsObjectEnd != std::string::npos) {
                    std::string groupsContent = jsonContent.substr(groupsObjectStart + 1, groupsObjectEnd - groupsObjectStart - 1);
                    
                    // Parse each group
                    size_t groupPos = 0;
                    while (groupPos < groupsContent.length()) {
                        // Find group name
                        size_t groupNameStart = groupsContent.find("\"", groupPos);
                        if (groupNameStart == std::string::npos) break;
                        
                        size_t groupNameEnd = groupsContent.find("\"", groupNameStart + 1);
                        if (groupNameEnd == std::string::npos) break;
                        
                        std::string groupName = groupsContent.substr(groupNameStart + 1, groupNameEnd - groupNameStart - 1);
                        
                        // Find group object
                        size_t groupObjectStart = groupsContent.find("{", groupNameEnd);
                        if (groupObjectStart == std::string::npos) break;
                        
                        // Find matching closing brace for this group
                        int groupBraceCount = 1;
                        size_t groupObjectPos = groupObjectStart + 1;
                        size_t groupObjectEnd = std::string::npos;
                        
                        while (groupObjectPos < groupsContent.length() && groupBraceCount > 0) {
                            if (groupsContent[groupObjectPos] == '{') {
                                groupBraceCount++;
                            } else if (groupsContent[groupObjectPos] == '}') {
                                groupBraceCount--;
                                if (groupBraceCount == 0) {
                                    groupObjectEnd = groupObjectPos;
                                    break;
                                }
                            }
                            groupObjectPos++;
                        }
                        
                        if (groupObjectEnd != std::string::npos) {
                            std::string groupObjectContent = groupsContent.substr(groupObjectStart + 1, groupObjectEnd - groupObjectStart - 1);
                            
                            // Parse policies array
                            std::vector<std::string> groupPolicies;
                            size_t policiesArrayStart = groupObjectContent.find("\"policies\"");
                            if (policiesArrayStart != std::string::npos) {
                                size_t arrayStart = groupObjectContent.find("[", policiesArrayStart);
                                size_t arrayEnd = groupObjectContent.find("]", arrayStart);
                                if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                                    std::string arrayContent = groupObjectContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                                    
                                    // Parse array elements
                                    size_t policyPos = 0;
                                    while (policyPos < arrayContent.length()) {
                                        size_t quoteStart = arrayContent.find("\"", policyPos);
                                        if (quoteStart == std::string::npos) break;
                                        
                                        size_t quoteEnd = arrayContent.find("\"", quoteStart + 1);
                                        if (quoteEnd == std::string::npos) break;
                                        
                                        std::string policy = arrayContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                                        groupPolicies.push_back(policy);
                                        
                                        policyPos = quoteEnd + 1;
                                    }
                                }
                            }
                            
                            // Parse description
                            std::string description;
                            size_t descStart = groupObjectContent.find("\"description\"");
                            if (descStart != std::string::npos) {
                                size_t quoteStart = groupObjectContent.find("\"", descStart + 13);
                                if (quoteStart != std::string::npos) {
                                    size_t quoteEnd = groupObjectContent.find("\"", quoteStart + 1);
                                    if (quoteEnd != std::string::npos) {
                                        description = groupObjectContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                                    }
                                }
                            }
                            
                            // Add the group
                            policyGroups_[groupName] = PolicyGroup(groupPolicies, description);
                        }
                        
                        groupPos = groupObjectEnd + 1;
                    }
                }
            }
        }
        
        // Parse policy settings
        size_t settingsStart = jsonContent.find("\"policy_settings\"");
        if (settingsStart != std::string::npos) {
            // Parse show_descriptions
            size_t showDescStart = jsonContent.find("\"show_descriptions\"", settingsStart);
            if (showDescStart != std::string::npos) {
                size_t colonPos = jsonContent.find(":", showDescStart);
                if (colonPos != std::string::npos) {
                    size_t valueStart = jsonContent.find_first_not_of(" \t\n", colonPos + 1);
                    if (valueStart != std::string::npos) {
                        policySettings_.showDescriptions = (jsonContent.substr(valueStart, 4) == "true");
                    }
                }
            }
            
            // Parse filter_experimental
            size_t filterExpStart = jsonContent.find("\"filter_experimental\"", settingsStart);
            if (filterExpStart != std::string::npos) {
                size_t colonPos = jsonContent.find(":", filterExpStart);
                if (colonPos != std::string::npos) {
                    size_t valueStart = jsonContent.find_first_not_of(" \t\n", colonPos + 1);
                    if (valueStart != std::string::npos) {
                        policySettings_.filterExperimental = (jsonContent.substr(valueStart, 4) == "true");
                    }
                }
            }
            
            // Parse interactive_mode
            size_t interactiveStart = jsonContent.find("\"interactive_mode\"", settingsStart);
            if (interactiveStart != std::string::npos) {
                size_t colonPos = jsonContent.find(":", interactiveStart);
                if (colonPos != std::string::npos) {
                    size_t valueStart = jsonContent.find_first_not_of(" \t\n", colonPos + 1);
                    if (valueStart != std::string::npos) {
                        policySettings_.interactiveMode = (jsonContent.substr(valueStart, 4) == "true");
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        setError("JSON parsing error: " + std::string(e.what()));
        return false;
    }
}

std::string PolicyConfiguration::toJsonString() const {
    std::stringstream json;
    
    json << "{\n";
    json << "  \"computation_policies\": {\n";
    
    // Enabled policies
    json << "    \"enabled\": [\n";
    for (size_t i = 0; i < enabledPolicies_.size(); ++i) {
        json << "      \"" << enabledPolicies_[i] << "\"";
        if (i < enabledPolicies_.size() - 1) json << ",";
        json << "\n";
    }
    json << "    ],\n";
    
    // Default policy
    json << "    \"default\": \"" << defaultPolicy_ << "\",\n";
    
    // Groups
    json << "    \"groups\": {\n";
    size_t groupIndex = 0;
    for (const auto& groupPair : policyGroups_) {
        json << "      \"" << groupPair.first << "\": {\n";
        json << "        \"policies\": [\n";
        
        for (size_t i = 0; i < groupPair.second.policies.size(); ++i) {
            json << "          \"" << groupPair.second.policies[i] << "\"";
            if (i < groupPair.second.policies.size() - 1) json << ",";
            json << "\n";
        }
        
        json << "        ],\n";
        json << "        \"description\": \"" << groupPair.second.description << "\"\n";
        json << "      }";
        
        if (groupIndex < policyGroups_.size() - 1) json << ",";
        json << "\n";
        groupIndex++;
    }
    json << "    }\n";
    
    json << "  },\n";
    
    // Policy settings
    json << "  \"policy_settings\": {\n";
    json << "    \"show_descriptions\": " << (policySettings_.showDescriptions ? "true" : "false") << ",\n";
    json << "    \"allow_multiple_selection\": " << (policySettings_.allowMultipleSelection ? "true" : "false") << ",\n";
    json << "    \"sort_by\": \"" << policySettings_.sortBy << "\",\n";
    json << "    \"filter_experimental\": " << (policySettings_.filterExperimental ? "true" : "false") << ",\n";
    json << "    \"interactive_mode\": " << (policySettings_.interactiveMode ? "true" : "false") << "\n";
    json << "  }\n";
    
    json << "}\n";
    
    return json.str();
}

} // namespace palvalidator