#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <cstdio>

// --- Include Actual Dependencies ---
#include "PriceComponentDescriptor.h"
#include "PatternCondition.h"
#include "PatternTemplate.h"

// --- Include the Class to be Tested ---
#include "UniverseGenerator.h"

// This helper function checks if a file exists on disk.
bool file_exists(const std::string& filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

// This test case is still valid as the constructor signature has not changed.
TEST_CASE("UniverseGenerator Initialization", "[constructor]") {
    SECTION("Throws on empty output file") {
        REQUIRE_THROWS_AS(UniverseGenerator<>("", 10, 5, 10, "DEEP"), std::invalid_argument);
    }
    SECTION("Throws on zero max lookback") {
        REQUIRE_THROWS_AS(UniverseGenerator<>("test.bin", 0, 5, 5, "DEEP"), std::invalid_argument);
    }
    SECTION("Throws on zero max conditions") {
        REQUIRE_THROWS_AS(UniverseGenerator<>("test.bin", 10, 0, 10, "DEEP"), std::invalid_argument);
    }
    SECTION("Constructs successfully with valid arguments") {
        REQUIRE_NOTHROW(UniverseGenerator<>("test.bin", 10, 5, 10, "DEEP"));
    }
}

// NEW: A test case for the new string parsing logic.
// NOTE: This requires making `parsePatternFromString` public for direct testing,
// or creating a public test wrapper for it.
TEST_CASE("UniverseGenerator Helpers", "[helpers]") {
    UniverseGenerator<concurrency::SingleThreadExecutor> gen("test.bin", 2, 2, 2, "EXTENDED");
    
    SECTION("Pattern String Parsing") {
        std::string line = "C[0] > H[1] > L[2]";
        PatternTemplate tpl = gen.testParsePatternFromString(line); // Assumes a public test wrapper

        REQUIRE(tpl.getName() == line);
        const auto& conds = tpl.getConditions();
        REQUIRE(conds.size() == 2);

        // Check first condition: C[0] > H[1]
        REQUIRE(conds[0].getLhs().getComponentType() == PriceComponentType::Close);
        REQUIRE(conds[0].getLhs().getBarOffset() == 0);
        REQUIRE(conds[0].getRhs().getComponentType() == PriceComponentType::High);
        REQUIRE(conds[0].getRhs().getBarOffset() == 1);
        
        // Check second condition: H[1] > L[2]
        REQUIRE(conds[1].getLhs().getComponentType() == PriceComponentType::High);
        REQUIRE(conds[1].getLhs().getBarOffset() == 1);
        REQUIRE(conds[1].getRhs().getComponentType() == PriceComponentType::Low);
        REQUIRE(conds[1].getRhs().getBarOffset() == 2);
    }

    SECTION("Pattern String Parsing with Delay") {
        std::string line = "O[2] > C[3] [Delay: 2]";
        PatternTemplate tpl = gen.testParsePatternFromString(line); // Assumes a public test wrapper
        REQUIRE(tpl.getName() == line);
        const auto& conds = tpl.getConditions();
        REQUIRE(conds.size() == 1);

        REQUIRE(conds[0].getLhs().getComponentType() == PriceComponentType::Open);
        REQUIRE(conds[0].getLhs().getBarOffset() == 2);
        REQUIRE(conds[0].getRhs().getComponentType() == PriceComponentType::Close);
        REQUIRE(conds[0].getRhs().getBarOffset() == 3);
    }
}


// NEW: This is a full black-box integration test for the new streaming architecture.
TEST_CASE("UniverseGenerator Full Run", "[integration]")
{
    const std::string outputFile = "test_output.pat";
    const std::string rawFile = outputFile + ".raw.tmp";
    const std::string uniqueFile = outputFile + ".unique.tmp";

    // Cleanup any files from previous failed runs before starting
    std::remove(outputFile.c_str());
    std::remove(rawFile.c_str());
    std::remove(uniqueFile.c_str());

    SECTION("Successful run creates output file and cleans up temporary files") {
        // Use very small parameters to ensure the test runs quickly
        UniverseGenerator<concurrency::SingleThreadExecutor> gen(outputFile, 3, 2, 3, "EXTENDED");
        
        // The run method should complete without throwing any exceptions
        REQUIRE_NOTHROW(gen.run());
        
        // Verify that the final output file was created
        REQUIRE(file_exists(outputFile));

        // Verify that the temporary files were cleaned up
        REQUIRE_FALSE(file_exists(rawFile));
        REQUIRE_FALSE(file_exists(uniqueFile));

        // Clean up the created output file
        std::remove(outputFile.c_str());
    }
    
    SECTION("Unsupported search type throws exception") {
        // Use valid parameters but an invalid search type
        UniverseGenerator<concurrency::SingleThreadExecutor> gen(outputFile, 3, 2, 3, "INVALID_MODE");
        
        // The run method should throw a runtime_error
        REQUIRE_THROWS_AS(gen.run(), std::runtime_error);

        // Verify that no orphaned files were left behind
        REQUIRE_FALSE(file_exists(outputFile));
        REQUIRE_FALSE(file_exists(rawFile));
        REQUIRE_FALSE(file_exists(uniqueFile));
    }
}
