#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestUtils.h"
#include "RandomMersenne.h"

using uint32 = unsigned int;

TEST_CASE("DrawNumber(min,max) always within inclusive bounds", "[RandomMersenne]") {
    RandomMersenne rng;
    const uint32 MIN = 10;
    const uint32 MAX = 20;
    for (int i = 0; i < 1000; ++i) {
        uint32 val = rng.DrawNumber(MIN, MAX);
        REQUIRE(val >= MIN);
        REQUIRE(val <= MAX);
    }
}

TEST_CASE("DrawNumber(min,max) with equal bounds returns that value", "[RandomMersenne]") {
    RandomMersenne rng;
    REQUIRE(rng.DrawNumber(7u, 7u) == 7u);
}

TEST_CASE("DrawNumber(max) always within [0,max] inclusive", "[RandomMersenne]") {
    RandomMersenne rng;
    const uint32 MAX = 5;
    for (int i = 0; i < 1000; ++i) {
        uint32 val = rng.DrawNumber(MAX);
        // unsigned always >= 0
        REQUIRE(val <= MAX);
    }
}

TEST_CASE("DrawNumber(max) with zero returns zero", "[RandomMersenne]") {
    RandomMersenne rng;
    REQUIRE(rng.DrawNumber(0u) == 0u);
}

TEST_CASE("DrawNumberExclusive always within [0,exclusiveUpperBound)", "[RandomMersenne]") {
    RandomMersenne rng;
    const uint32 BOUND = 10;
    for (int i = 0; i < 1000; ++i) {
        uint32 val = rng.DrawNumberExclusive(BOUND);
        REQUIRE(val < BOUND);
    }
}

TEST_CASE("DrawNumberExclusive(1) always returns zero", "[RandomMersenne]") {
    RandomMersenne rng;
    REQUIRE(rng.DrawNumberExclusive(1u) == 0u);
}
