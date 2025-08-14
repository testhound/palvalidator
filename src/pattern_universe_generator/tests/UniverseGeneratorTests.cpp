#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <cstdio>
#include <set>

// --- Include Actual Dependencies ---
// NOTE: You must ensure the path to these headers is correct in your build system.
#include "PatternUniverseDeserializer.h"
#include "PatternTemplate.h"
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


// MODIFIED: This is a full black-box integration test for the new streaming architecture.
// It also validates that the parallel implementation is correct.
TEST_CASE("UniverseGenerator Full Run and Concurrency Validation", "[integration]")
{
    const std::string singleCoreFile = "singlecore_output.pat";
    const std::string multiCoreFile = "multicore_output.pat";
    const std::string singleCoreRaw = singleCoreFile + ".raw.tmp";
    const std::string singleCoreUnique = singleCoreFile + ".unique.tmp";
    const std::string multiCoreRaw = multiCoreFile + ".raw.tmp";
    const std::string multiCoreUnique = multiCoreFile + ".unique.tmp";

    // Cleanup any files from previous failed runs before starting
    std::remove(singleCoreFile.c_str());
    std::remove(multiCoreFile.c_str());
    std::remove(singleCoreRaw.c_str());
    std::remove(singleCoreUnique.c_str());
    std::remove(multiCoreRaw.c_str());
    std::remove(multiCoreUnique.c_str());

    // Use a small but non-trivial parameter set for the test
    const uint8_t maxLookback = 4;
    const uint8_t maxConditions = 3;
    const uint8_t maxSpread = 4;

    SECTION("Single-threaded and multi-threaded runs produce identical results") {
        // --- Step 1: Generate the pattern universe using a single thread ---
        INFO("Running single-threaded generation...");
        UniverseGenerator<concurrency::SingleThreadExecutor> gen_single(singleCoreFile, maxLookback, maxConditions, maxSpread, "EXTENDED");
        REQUIRE_NOTHROW(gen_single.run());
        
        // Verify final file exists and temporary files are cleaned up
        REQUIRE(file_exists(singleCoreFile));
        REQUIRE_FALSE(file_exists(singleCoreRaw));
        REQUIRE_FALSE(file_exists(singleCoreUnique));

        // --- Step 2: Generate the pattern universe using the thread pool ---
        INFO("Running multi-threaded generation...");
        UniverseGenerator<concurrency::ThreadPoolExecutor<>> gen_multi(multiCoreFile, maxLookback, maxConditions, maxSpread, "EXTENDED");
        REQUIRE_NOTHROW(gen_multi.run());

        // Verify final file exists and temporary files are cleaned up
        REQUIRE(file_exists(multiCoreFile));
        REQUIRE_FALSE(file_exists(multiCoreRaw));
        REQUIRE_FALSE(file_exists(multiCoreUnique));

        // --- Step 3: Deserialize both files and compare their contents ---
        INFO("Deserializing and comparing results...");
        PatternUniverseDeserializer deserializer;

        std::ifstream single_stream(singleCoreFile, std::ios::binary);
        REQUIRE(single_stream.is_open());
        auto single_patterns = deserializer.deserialize(single_stream);
        single_stream.close();

        std::ifstream multi_stream(multiCoreFile, std::ios::binary);
        REQUIRE(multi_stream.is_open());
        auto multi_patterns = deserializer.deserialize(multi_stream);
        multi_stream.close();

        // The number of unique patterns must be identical.
        REQUIRE(single_patterns.size() == multi_patterns.size());
        REQUIRE(single_patterns.size() > 0); // Sanity check that patterns were generated

        // To compare contents regardless of order, load them into sets.
        std::set<PatternTemplate> single_set(single_patterns.begin(), single_patterns.end());
        std::set<PatternTemplate> multi_set(multi_patterns.begin(), multi_patterns.end());
        
        // The sets must be identical, proving the parallel version is correct.
        REQUIRE(single_set == multi_set);
    }
    
    // --- Step 4: Final Cleanup ---
    std::remove(singleCoreFile.c_str());
    std::remove(multiCoreFile.c_str());
}