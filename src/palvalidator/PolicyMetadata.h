#pragma once

#include <string>
#include <vector>
#include <algorithm>

namespace palvalidator {

/**
 * @brief Metadata structure for computation policies
 * 
 * Contains rich information about each policy including description,
 * categorization, requirements, and versioning information.
 */
struct PolicyMetadata {
    std::string name;                    ///< Policy class name
    std::string displayName;             ///< Human-readable display name
    std::string description;             ///< Detailed description of the policy
    std::string category;                ///< Policy category (e.g., "basic", "advanced", "experimental")
    std::vector<std::string> requirements; ///< System or data requirements
    std::vector<std::string> tags;       ///< Searchable tags
    bool isExperimental;                 ///< Whether policy is experimental
    std::string version;                 ///< Policy version
    std::string author;                  ///< Policy author/maintainer
    
    /**
     * @brief Default constructor
     */
    PolicyMetadata() : isExperimental(false), version("1.0.0") {}
    
    /**
     * @brief Constructor with basic information
     */
    PolicyMetadata(const std::string& name, 
                   const std::string& displayName,
                   const std::string& description,
                   const std::string& category = "basic",
                   bool experimental = false)
        : name(name)
        , displayName(displayName)
        , description(description)
        , category(category)
        , isExperimental(experimental)
        , version("1.0.0") {}
    
    /**
     * @brief Check if policy has a specific tag
     */
    bool hasTag(const std::string& tag) const {
        return std::find(tags.begin(), tags.end(), tag) != tags.end();
    }
    
    /**
     * @brief Add a tag to the policy
     */
    void addTag(const std::string& tag) {
        if (!hasTag(tag)) {
            tags.push_back(tag);
        }
    }
    
    /**
     * @brief Add a requirement to the policy
     */
    void addRequirement(const std::string& requirement) {
        requirements.push_back(requirement);
    }
};

} // namespace palvalidator