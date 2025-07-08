#include <catch2/catch_test_macros.hpp>
#include "PolicyRegistry.h"
#include "PolicyConfiguration.h"
#include "PolicyFactory.h"
#include "PolicyRegistration.h"
#include "PolicySelector.h"

using namespace palvalidator;
using namespace statistics;

// Helper function to set up tests
void setupPolicyArchitectureTest() {
    // Clear any existing registrations
    PolicyRegistry::clear();
    PolicyFactory::clear();
    
    // Initialize the policy registry
    initializePolicyRegistry();
}

void teardownPolicyArchitectureTest() {
    PolicyRegistry::clear();
    PolicyFactory::clear();
}

// Test Policy Registry functionality
TEST_CASE("Policy Registry Basic Functionality", "[PolicyArchitecture][Registry]") {
    setupPolicyArchitectureTest();
    
    auto availablePolicies = PolicyRegistry::getAvailablePolicies();
    
    // Should have all 18 policies registered
    REQUIRE(availablePolicies.size() == 18);
    
    // Check that specific policies are available
    REQUIRE(PolicyRegistry::isPolicyAvailable("GatedPerformanceScaledPalPolicy"));
    REQUIRE(PolicyRegistry::isPolicyAvailable("RobustProfitFactorPolicy"));
    REQUIRE(PolicyRegistry::isPolicyAvailable("AllHighResLogPFPolicy"));
    REQUIRE(PolicyRegistry::isPolicyAvailable("BootStrappedProfitFactorPolicy"));
    REQUIRE(PolicyRegistry::isPolicyAvailable("BootStrappedLogProfitFactorPolicy"));
    REQUIRE(PolicyRegistry::isPolicyAvailable("BootStrappedProfitabilityPFPolicy"));
    REQUIRE(PolicyRegistry::isPolicyAvailable("BootStrappedLogProfitabilityPFPolicy"));
    
    // Check that non-existent policy is not available
    REQUIRE_FALSE(PolicyRegistry::isPolicyAvailable("NonExistentPolicy"));
    
    teardownPolicyArchitectureTest();
}

TEST_CASE("Policy Metadata Retrieval", "[PolicyArchitecture][Metadata]") {
    setupPolicyArchitectureTest();
    
    // Test metadata retrieval for a known policy
    auto metadata = PolicyRegistry::getPolicyMetadata("GatedPerformanceScaledPalPolicy");
    
    REQUIRE(metadata.name == "GatedPerformanceScaledPalPolicy");
    REQUIRE(metadata.displayName == "Gated Performance Scaled PAL");
    REQUIRE(metadata.category == "advanced");
    REQUIRE_FALSE(metadata.isExperimental);
    REQUIRE(metadata.hasTag("recommended"));
    
    // Test experimental policy
    auto expMetadata = PolicyRegistry::getPolicyMetadata("BootStrappedProfitFactorPolicy");
    REQUIRE(expMetadata.isExperimental);
    REQUIRE(expMetadata.category == "experimental");
    
    teardownPolicyArchitectureTest();
}

TEST_CASE("Policy Categorization", "[PolicyArchitecture][Categories]") {
    setupPolicyArchitectureTest();
    
    auto basicPolicies = PolicyRegistry::getPoliciesByCategory("basic");
    auto advancedPolicies = PolicyRegistry::getPoliciesByCategory("advanced");
    auto experimentalPolicies = PolicyRegistry::getPoliciesByCategory("experimental");
    
    REQUIRE(basicPolicies.size() > 0);
    REQUIRE(advancedPolicies.size() > 0);
    REQUIRE(experimentalPolicies.size() > 0);
    
    // Check specific categorizations
    auto categories = PolicyRegistry::getAvailableCategories();
    REQUIRE(std::find(categories.begin(), categories.end(), "basic") != categories.end());
    REQUIRE(std::find(categories.begin(), categories.end(), "advanced") != categories.end());
    REQUIRE(std::find(categories.begin(), categories.end(), "experimental") != categories.end());
    
    teardownPolicyArchitectureTest();
}

// Test Policy Configuration functionality
TEST_CASE("Policy Configuration Default", "[PolicyArchitecture][Configuration]") {
    setupPolicyArchitectureTest();
    
    auto config = PolicyConfiguration::createDefault();
    
    auto enabledPolicies = config.getEnabledPolicies();
    REQUIRE(enabledPolicies.size() > 0);
    
    auto defaultPolicy = config.getDefaultPolicy();
    REQUIRE_FALSE(defaultPolicy.empty());
    
    // Default policy should be in enabled list
    REQUIRE(config.isPolicyEnabled(defaultPolicy));
    
    teardownPolicyArchitectureTest();
}

TEST_CASE("Policy Configuration Validation", "[PolicyArchitecture][Validation]") {
    setupPolicyArchitectureTest();
    
    auto config = PolicyConfiguration::createDefault();
    auto availablePolicies = PolicyRegistry::getAvailablePolicies();
    
    auto errors = config.validate(availablePolicies);
    REQUIRE(errors.empty());
    
    teardownPolicyArchitectureTest();
}

TEST_CASE("Policy Configuration From JSON", "[PolicyArchitecture][JSON]") {
    setupPolicyArchitectureTest();
    
    std::string jsonConfig = R"({
        "computation_policies": {
            "enabled": ["GatedPerformanceScaledPalPolicy", "RobustProfitFactorPolicy"],
            "default": "GatedPerformanceScaledPalPolicy",
            "groups": {
                "test": {
                    "policies": ["GatedPerformanceScaledPalPolicy"],
                    "description": "Test group"
                }
            }
        },
        "policy_settings": {
            "show_descriptions": true,
            "interactive_mode": false
        }
    })";
    
    PolicyConfiguration config;
    REQUIRE(config.loadFromString(jsonConfig));
    
    auto enabledPolicies = config.getEnabledPolicies();
    REQUIRE(enabledPolicies.size() == 2);
    REQUIRE(config.isPolicyEnabled("GatedPerformanceScaledPalPolicy"));
    REQUIRE(config.isPolicyEnabled("RobustProfitFactorPolicy"));
    REQUIRE_FALSE(config.isPolicyEnabled("AllHighResLogPFPolicy"));
    
    REQUIRE(config.getDefaultPolicy() == "GatedPerformanceScaledPalPolicy");
    
    auto groupNames = config.getGroupNames();
    REQUIRE(groupNames.size() == 1);
    REQUIRE(groupNames[0] == "test");
    
    teardownPolicyArchitectureTest();
}

// Test Policy Factory functionality
TEST_CASE("Policy Factory Registration", "[PolicyArchitecture][Factory]") {
    setupPolicyArchitectureTest();
    
    // Check that policies are registered with the factory
    REQUIRE(PolicyFactory::isMastersPolicyRegistered("GatedPerformanceScaledPalPolicy"));
    REQUIRE(PolicyFactory::isRomanoWolfPolicyRegistered("RobustProfitFactorPolicy"));
    REQUIRE(PolicyFactory::isBenjaminiHochbergPolicyRegistered("AllHighResLogPFPolicy"));
    
    // Check that non-existent policy is not registered
    REQUIRE_FALSE(PolicyFactory::isMastersPolicyRegistered("NonExistentPolicy"));
    
    teardownPolicyArchitectureTest();
}

TEST_CASE("Policy Factory Creation", "[PolicyArchitecture][Creation]") {
    setupPolicyArchitectureTest();
    
    SECTION("Masters validation creation") {
        REQUIRE_NOTHROW([&]() {
            auto validation = PolicyFactory::createMastersValidation("GatedPerformanceScaledPalPolicy", 1000);
            REQUIRE(validation != nullptr);
        }());
    }
    
    SECTION("Romano-Wolf validation creation") {
        REQUIRE_NOTHROW([&]() {
            auto validation = PolicyFactory::createRomanoWolfValidation("RobustProfitFactorPolicy", 1000);
            REQUIRE(validation != nullptr);
        }());
    }
    
    SECTION("Benjamini-Hochberg validation creation") {
        REQUIRE_NOTHROW([&]() {
            auto validation = PolicyFactory::createBenjaminiHochbergValidation("AllHighResLogPFPolicy", 1000, 0.1);
            REQUIRE(validation != nullptr);
        }());
    }
    
    SECTION("Invalid policy name throws exception") {
        REQUIRE_THROWS_AS([&]() {
            auto validation = PolicyFactory::createMastersValidation("NonExistentPolicy", 1000);
        }(), std::invalid_argument);
    }
    
    teardownPolicyArchitectureTest();
}

// Test Policy Selector functionality
TEST_CASE("Policy Selector Filtering", "[PolicyArchitecture][Selector]") {
    setupPolicyArchitectureTest();
    
    auto allPolicies = PolicyRegistry::getAvailablePolicies();
    
    SECTION("Category filtering") {
        auto basicPolicies = PolicySelector::filterPoliciesByCategory(allPolicies, "basic");
        for (const auto& policy : basicPolicies) {
            auto metadata = PolicyRegistry::getPolicyMetadata(policy);
            REQUIRE(metadata.category == "basic");
        }
    }
    
    SECTION("Experimental filtering") {
        auto nonExperimentalPolicies = PolicySelector::filterExperimentalPolicies(allPolicies);
        for (const auto& policy : nonExperimentalPolicies) {
            auto metadata = PolicyRegistry::getPolicyMetadata(policy);
            REQUIRE_FALSE(metadata.isExperimental);
        }
    }
    
    teardownPolicyArchitectureTest();
}

TEST_CASE("Policy Selector Sorting", "[PolicyArchitecture][Sorting]") {
    setupPolicyArchitectureTest();
    
    auto allPolicies = PolicyRegistry::getAvailablePolicies();
    
    SECTION("Name sorting") {
        auto sortedByName = PolicySelector::sortPolicies(allPolicies, "name");
        REQUIRE(std::is_sorted(sortedByName.begin(), sortedByName.end()));
    }
    
    SECTION("Category sorting") {
        auto sortedByCategory = PolicySelector::sortPolicies(allPolicies, "category");
        REQUIRE(sortedByCategory.size() == allPolicies.size());
    }
    
    teardownPolicyArchitectureTest();
}

// Integration test
TEST_CASE("End-to-End Integration", "[PolicyArchitecture][Integration]") {
    setupPolicyArchitectureTest();
    
    // Load configuration
    PolicyConfiguration config;
    std::string jsonConfig = R"({
        "computation_policies": {
            "enabled": ["GatedPerformanceScaledPalPolicy", "RobustProfitFactorPolicy"],
            "default": "GatedPerformanceScaledPalPolicy"
        }
    })";
    REQUIRE(config.loadFromString(jsonConfig));
    
    // Get enabled policies
    auto enabledPolicies = config.getEnabledPolicies();
    REQUIRE(enabledPolicies.size() == 2);
    
    // Verify all enabled policies are available in registry
    for (const auto& policy : enabledPolicies) {
        REQUIRE(PolicyRegistry::isPolicyAvailable(policy));
    }
    
    // Create validation objects for each enabled policy
    for (const auto& policy : enabledPolicies) {
        REQUIRE_NOTHROW([&]() {
            auto mastersValidation = PolicyFactory::createMastersValidation(policy, 100);
            REQUIRE(mastersValidation != nullptr);
            
            auto romanoWolfValidation = PolicyFactory::createRomanoWolfValidation(policy, 100);
            REQUIRE(romanoWolfValidation != nullptr);
            
            auto benjaminiValidation = PolicyFactory::createBenjaminiHochbergValidation(policy, 100, 0.1);
            REQUIRE(benjaminiValidation != nullptr);
        }());
    }
    
    teardownPolicyArchitectureTest();
}