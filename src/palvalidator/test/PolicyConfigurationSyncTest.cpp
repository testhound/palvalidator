#include <catch2/catch_test_macros.hpp>
#include "PolicyConfiguration.h"
#include <fstream>
#include <algorithm>
#include <iostream>

namespace palvalidator {

TEST_CASE("Policy configuration JSON and code defaults should be synchronized", "[PolicyConfiguration]") {
    
    SECTION("JSON file should load successfully") {
        PolicyConfiguration jsonConfig;
        bool loaded = jsonConfig.loadFromFile("config/policies.json");
        if (!loaded) {
            std::cout << "Error loading config: " << jsonConfig.getLastError() << std::endl;
        }
        REQUIRE(loaded);
    }
    
    SECTION("JSON config should match code defaults") {
        // Load from JSON
        PolicyConfiguration jsonConfig;
        REQUIRE(jsonConfig.loadFromFile("config/policies.json"));
        
        // Create code defaults
        PolicyConfiguration codeConfig = PolicyConfiguration::createDefault();
        
        // Compare enabled policies
        auto jsonEnabled = jsonConfig.getEnabledPolicies();
        auto codeEnabled = codeConfig.getEnabledPolicies();
        
        std::sort(jsonEnabled.begin(), jsonEnabled.end());
        std::sort(codeEnabled.begin(), codeEnabled.end());
        
        REQUIRE(jsonEnabled == codeEnabled);
        
        // Compare default policy
        REQUIRE(jsonConfig.getDefaultPolicy() == codeConfig.getDefaultPolicy());
        
        // Compare filter experimental setting
        REQUIRE(jsonConfig.getPolicySettings().filterExperimental == 
                codeConfig.getPolicySettings().filterExperimental);
        
        // Compare group names
        auto jsonGroups = jsonConfig.getGroupNames();
        auto codeGroups = codeConfig.getGroupNames();
        
        std::sort(jsonGroups.begin(), jsonGroups.end());
        std::sort(codeGroups.begin(), codeGroups.end());
        
        REQUIRE(jsonGroups == codeGroups);
        
        // Compare policies in each group
        for (const std::string& groupName : jsonGroups) {
            auto jsonGroupPolicies = jsonConfig.getPoliciesInGroup(groupName);
            auto codeGroupPolicies = codeConfig.getPoliciesInGroup(groupName);
            
            std::sort(jsonGroupPolicies.begin(), jsonGroupPolicies.end());
            std::sort(codeGroupPolicies.begin(), codeGroupPolicies.end());
            
            REQUIRE(jsonGroupPolicies == codeGroupPolicies);
        }
    }
    
    SECTION("All Bootstrap policies should be enabled") {
        PolicyConfiguration config;
        REQUIRE(config.loadFromFile("config/policies.json"));
        
        auto enabled = config.getEnabledPolicies();
        
        REQUIRE(std::find(enabled.begin(), enabled.end(), "BootStrappedProfitFactorPolicy") != enabled.end());
        REQUIRE(std::find(enabled.begin(), enabled.end(), "BootStrappedLogProfitFactorPolicy") != enabled.end());
        REQUIRE(std::find(enabled.begin(), enabled.end(), "BootStrappedProfitabilityPFPolicy") != enabled.end());
        REQUIRE(std::find(enabled.begin(), enabled.end(), "BootStrappedLogProfitabilityPFPolicy") != enabled.end());
    }
    
    SECTION("Experimental filtering should be disabled") {
        PolicyConfiguration config;
        REQUIRE(config.loadFromFile("config/policies.json"));
        
        REQUIRE_FALSE(config.getPolicySettings().filterExperimental);
    }
}

} // namespace palvalidator
