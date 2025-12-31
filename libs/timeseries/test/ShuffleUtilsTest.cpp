#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cstdint>
#include "ShuffleUtils.h"
#include "RandomMersenne.h"

using namespace mkc_timeseries;

TEST_CASE("inplaceShuffle handles empty vector", "[ShuffleUtils]") {
    std::vector<int> v;
    RandomMersenne rng;
    
    inplaceShuffle(v, rng);
    
    REQUIRE(v.empty());
}

TEST_CASE("inplaceShuffle handles single element vector", "[ShuffleUtils]") {
    std::vector<int> v = {42};
    RandomMersenne rng;
    
    inplaceShuffle(v, rng);
    
    REQUIRE(v.size() == 1);
    REQUIRE(v[0] == 42);
}

TEST_CASE("inplaceShuffle preserves all elements", "[ShuffleUtils]") {
    std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> v = original;
    RandomMersenne rng;
    
    inplaceShuffle(v, rng);
    
    // Should have same size
    REQUIRE(v.size() == original.size());
    
    // Should contain all the same elements (order may differ)
    std::sort(v.begin(), v.end());
    REQUIRE(v == original);
}

TEST_CASE("inplaceShuffle with duplicate elements preserves counts", "[ShuffleUtils]") {
    std::vector<int> original = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};
    std::vector<int> v = original;
    RandomMersenne rng;
    
    inplaceShuffle(v, rng);
    
    std::sort(v.begin(), v.end());
    std::sort(original.begin(), original.end());
    REQUIRE(v == original);
}

TEST_CASE("inplaceShuffle actually shuffles (not identity)", "[ShuffleUtils]") {
    std::vector<int> original = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    RandomMersenne rng;
    
    int identicalCount = 0;
    const int TRIALS = 100;
    
    for (int trial = 0; trial < TRIALS; ++trial) {
        std::vector<int> v = original;
        inplaceShuffle(v, rng);
        
        if (v == original) {
            identicalCount++;
        }
    }
    
    // With 16 elements, probability of getting original order is 1/16! ≈ 4.8e-14
    // So getting it even once in 100 trials is extremely unlikely
    REQUIRE(identicalCount < TRIALS);
}

TEST_CASE("inplaceShuffle produces different permutations", "[ShuffleUtils]") {
    std::vector<int> original = {1, 2, 3, 4, 5};
    RandomMersenne rng;
    
    std::set<std::vector<int>> uniquePermutations;
    const int TRIALS = 500;
    
    for (int trial = 0; trial < TRIALS; ++trial) {
        std::vector<int> v = original;
        inplaceShuffle(v, rng);
        uniquePermutations.insert(v);
    }
    
    // With 5 elements, there are 5! = 120 possible permutations
    // We should see many different ones in 500 trials
    REQUIRE(uniquePermutations.size() > 50);
}

TEST_CASE("inplaceShuffle with deterministic seed produces same result", "[ShuffleUtils]") {
    std::vector<int> v1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> v2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    RandomMersenne rng1(12345);
    RandomMersenne rng2(12345);
    
    inplaceShuffle(v1, rng1);
    inplaceShuffle(v2, rng2);
    
    REQUIRE(v1 == v2);
}

TEST_CASE("inplaceShuffle with different seeds produces different results", "[ShuffleUtils]") {
    std::vector<int> v1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> v2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    RandomMersenne rng1(12345);
    RandomMersenne rng2(54321);
    
    inplaceShuffle(v1, rng1);
    inplaceShuffle(v2, rng2);
    
    // With different seeds, results should be different (with very high probability)
    REQUIRE(v1 != v2);
}

TEST_CASE("inplaceShuffle works with different data types", "[ShuffleUtils]") {
    SECTION("double") {
        std::vector<double> v = {1.1, 2.2, 3.3, 4.4, 5.5};
        std::vector<double> original = v;
        RandomMersenne rng;
        
        inplaceShuffle(v, rng);
        
        std::sort(v.begin(), v.end());
        std::sort(original.begin(), original.end());
        REQUIRE(v == original);
    }
    
    SECTION("string") {
        std::vector<std::string> v = {"alpha", "beta", "gamma", "delta", "epsilon"};
        std::vector<std::string> original = v;
        RandomMersenne rng;
        
        inplaceShuffle(v, rng);
        
        std::sort(v.begin(), v.end());
        std::sort(original.begin(), original.end());
        REQUIRE(v == original);
    }
    
    SECTION("char") {
        std::vector<char> v = {'a', 'b', 'c', 'd', 'e', 'f'};
        std::vector<char> original = v;
        RandomMersenne rng;
        
        inplaceShuffle(v, rng);
        
        std::sort(v.begin(), v.end());
        std::sort(original.begin(), original.end());
        REQUIRE(v == original);
    }
}

TEST_CASE("inplaceShuffle distribution appears uniform", "[ShuffleUtils]") {
    // Test that first element ends up in each position roughly equally
    const int TRIALS = 10000;
    const int SIZE = 5;
    std::vector<int> v = {0, 1, 2, 3, 4};
    std::map<int, int> positionCounts; // Count how many times element 0 ends up at each position
    
    RandomMersenne rng;
    
    for (int trial = 0; trial < TRIALS; ++trial) {
        std::vector<int> temp = v;
        inplaceShuffle(temp, rng);
        
        // Find where element 0 ended up
        for (int pos = 0; pos < SIZE; ++pos) {
            if (temp[pos] == 0) {
                positionCounts[pos]++;
                break;
            }
        }
    }
    
    // Each position should get roughly TRIALS/SIZE occurrences
    const double expected = TRIALS / static_cast<double>(SIZE);
    const double tolerance = expected * 0.15; // 15% tolerance
    
    for (int pos = 0; pos < SIZE; ++pos) {
        double count = static_cast<double>(positionCounts[pos]);
        REQUIRE(count >= expected - tolerance);
        REQUIRE(count <= expected + tolerance);
    }
}

TEST_CASE("inplaceShuffle with two elements", "[ShuffleUtils]") {
    std::vector<int> original = {1, 2};
    RandomMersenne rng;
    
    std::map<std::vector<int>, int> outcomes;
    const int TRIALS = 1000;
    
    for (int trial = 0; trial < TRIALS; ++trial) {
        std::vector<int> v = original;
        inplaceShuffle(v, rng);
        outcomes[v]++;
    }
    
    // Should see both [1,2] and [2,1], each roughly 50% of the time
    REQUIRE(outcomes.size() == 2);
    REQUIRE(outcomes[{1, 2}] > 400);
    REQUIRE(outcomes[{1, 2}] < 600);
    REQUIRE(outcomes[{2, 1}] > 400);
    REQUIRE(outcomes[{2, 1}] < 600);
}

TEST_CASE("inplaceShuffle with large vector", "[ShuffleUtils]") {
    const int SIZE = 10000;
    std::vector<int> v(SIZE);
    for (int i = 0; i < SIZE; ++i) {
        v[i] = i;
    }
    
    std::vector<int> original = v;
    RandomMersenne rng;
    
    inplaceShuffle(v, rng);
    
    // Should still contain all elements
    std::sort(v.begin(), v.end());
    REQUIRE(v == original);
}

TEST_CASE("inplaceShuffle multiple shuffles on same vector", "[ShuffleUtils]") {
    std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int> v = original;
    RandomMersenne rng;
    
    // Shuffle multiple times
    inplaceShuffle(v, rng);
    inplaceShuffle(v, rng);
    inplaceShuffle(v, rng);
    
    // Should still contain all elements
    std::sort(v.begin(), v.end());
    REQUIRE(v == original);
}