#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdint>
#include "TestUtils.h"
#include "RandomMersenne.h"

using std::uint32_t;

TEST_CASE("DrawNumber(min,max) always within inclusive bounds", "[RandomMersenne]") {
    RandomMersenne rng;
    const uint32_t MIN = 10;
    const uint32_t MAX = 20;
    for (int i = 0; i < 1000; ++i) {
        uint32_t val = rng.DrawNumber(MIN, MAX);
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
    const uint32_t MAX = 5;
    for (int i = 0; i < 1000; ++i) {
        uint32_t val = rng.DrawNumber(MAX);
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
    const uint32_t BOUND = 10;
    for (int i = 0; i < 1000; ++i) {
        uint32_t val = rng.DrawNumberExclusive(BOUND);
        REQUIRE(val < BOUND);
    }
}

TEST_CASE("DrawNumberExclusive(1) always returns zero", "[RandomMersenne]") {
    RandomMersenne rng;
    REQUIRE(rng.DrawNumberExclusive(1u) == 0u);
}

TEST_CASE("withStream: different stream IDs produce different sequences", "[RandomMersenne]") {
    auto rng1 = RandomMersenne::withStream(1ULL);
    auto rng2 = RandomMersenne::withStream(2ULL);

    bool anyDifference = false;
    for (int i = 0; i < 100; ++i) {
        if (rng1.DrawNumberExclusive(1000000u) != rng2.DrawNumberExclusive(1000000u)) {
            anyDifference = true;
            break;
        }
    }
    REQUIRE(anyDifference);
}

TEST_CASE("withStream: DrawNumberExclusive within bounds", "[RandomMersenne]") {
    auto rng = RandomMersenne::withStream(42ULL);
    for (int i = 0; i < 1000; ++i) {
        uint32_t val = rng.DrawNumberExclusive(100u);
        REQUIRE(val < 100u);
    }
}

TEST_CASE("withStream: DrawNumber within inclusive bounds", "[RandomMersenne]") {
    auto rng = RandomMersenne::withStream(42ULL);
    for (int i = 0; i < 1000; ++i) {
        uint32_t val = rng.DrawNumber(10u, 20u);
        REQUIRE(val >= 10u);
        REQUIRE(val <= 20u);
    }
}

TEST_CASE("withStream: thread-id-derived streams differ across threads", "[RandomMersenne]") {
    std::mutex m;
    std::vector<uint32_t> firstDraws;

    auto threadFunc = [&]() {
        auto rng = RandomMersenne::withStream(
            static_cast<uint64_t>(
                std::hash<std::thread::id>{}(std::this_thread::get_id())));
        std::lock_guard<std::mutex> lk(m);
        firstDraws.push_back(rng.DrawNumberExclusive(
            std::numeric_limits<uint32_t>::max()));
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
        threads.emplace_back(threadFunc);
    for (auto& t : threads) t.join();

    // All first draws should be distinct (streams on different cycles)
    std::sort(firstDraws.begin(), firstDraws.end());
    auto it = std::adjacent_find(firstDraws.begin(), firstDraws.end());
    REQUIRE(it == firstDraws.end());
}

