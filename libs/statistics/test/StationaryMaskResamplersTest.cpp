#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "StationaryMaskResamplers.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"

using palvalidator::resampling::make_restart_mask;
using palvalidator::resampling::StationaryMaskValueResampler;
using palvalidator::resampling::StationaryMaskIndexResampler;

TEST_CASE("make_restart_mask: basic invariants", "[Resampler][Mask]")
{
    randutils::seed_seq_fe128 seed{123u,456u,789u,42u};
    randutils::mt19937_rng rng(seed);

    SECTION("Length and first-bit invariant")
    {
        const std::size_t m = 200;
        const double L = 4.0;
        auto mask = make_restart_mask(m, L, rng);
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        // mask is 0/1 only
        for (auto b : mask)
        {
            REQUIRE((b == 0u || b == 1u));
        }
    }

    SECTION("Empirical restart rate ~ 1/L")
    {
        // Average across several draws for stability
        const std::size_t m = 2000;
        const double L = 5.0;
        const double p = 1.0 / L;

        const int R = 50;
        double total_restarts = 0.0;

        for (int r = 0; r < R; ++r)
        {
            auto mask = make_restart_mask(m, L, rng);
            // Count restarts (1 bits)
            std::size_t restarts = 0;
            for (auto b : mask) restarts += (b ? 1 : 0);
            total_restarts += static_cast<double>(restarts);
        }

        const double mean_restarts = total_restarts / static_cast<double>(R);
        // Expected restarts ≈ m * p (first bit is 1 by design, but negligible at large m)
        const double expected = m * p;
        // Allow generous tolerance for randomness
        REQUIRE(mean_restarts == Catch::Approx(expected).margin(0.10 * expected));
    }

    SECTION("Invalid inputs throw")
    {
        // m < 2
        REQUIRE_THROWS_AS(make_restart_mask(1, 3.0, rng), std::invalid_argument);
        // L < 1
        REQUIRE_THROWS_AS(make_restart_mask(10, 0.0, rng), std::invalid_argument);
        // Non-finite L
        REQUIRE_THROWS_AS(make_restart_mask(10, std::numeric_limits<double>::infinity(), rng), std::invalid_argument);
    }
}

TEST_CASE("StationaryMaskValueResampler: shape, domain, and contiguity", "[Resampler][Value]")
{
    using D = DecimalType;

    // Build a monotone source so we can infer indices from values
    const std::size_t n = 250;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{111u,222u,333u,444u};
    randutils::mt19937_rng rng(seed);

    SECTION("Invalid inputs throw")
    {
        StationaryMaskValueResampler<D> res(3);
        std::vector<D> y;

        // x.size() < 2
        std::vector<D> tiny{ D(1) };
        REQUIRE_THROWS_AS(res(tiny, y, 10, rng), std::invalid_argument);

        // m < 2
        REQUIRE_THROWS_AS(res(x, y, 1, rng), std::invalid_argument);

        // L < 1 at construction
        REQUIRE_THROWS_AS(StationaryMaskValueResampler<D>(0), std::invalid_argument);
    }

    SECTION("Output has correct length; values within domain; contiguity ~ 1 - 1/L")
    {
        const std::size_t m = 400;
        const std::size_t L = 4;
        StationaryMaskValueResampler<D> res(L);
        std::vector<D> y;
        res(x, y, m, rng);

        REQUIRE(y.size() == m);

        // All values in 0..n-1
        for (const auto& v : y)
        {
            const double vd = num::to_double(v);
            REQUIRE(vd >= 0.0);
            REQUIRE(vd < static_cast<double>(n));
        }

        // Contiguity fraction should be roughly (1 - 1/L)
        // i.e., next == (cur + 1) % n happens often
        std::size_t adjacent = 0;
        for (std::size_t t = 0; t + 1 < y.size(); ++t)
        {
            int cur = static_cast<int>(num::to_double(y[t]));
            int nxt = static_cast<int>(num::to_double(y[t + 1]));
            if (nxt == (cur + 1) % static_cast<int>(n)) adjacent++;
        }
        const double frac_adj = static_cast<double>(adjacent) / static_cast<double>(m - 1);
        // Expect near 0.75 for L=4; be conservative to avoid flakiness
        REQUIRE(frac_adj > 0.60);
    }
}

TEST_CASE("StationaryMaskIndexResampler: shape, domain, contiguity, determinism", "[Resampler][Index]")
{
    using D = DecimalType;

    // Same monotone source
    const std::size_t n = 180;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{9u,8u,7u,6u};
    randutils::mt19937_rng rng1(seed);
    randutils::mt19937_rng rng2(seed); // identical seed for determinism test

    SECTION("Invalid inputs throw")
    {
        StationaryMaskIndexResampler rL3(3);

        std::vector<std::size_t> idx;

        // n < 2
        REQUIRE_THROWS_AS(rL3(1, idx, 10, rng1), std::invalid_argument);

        // m < 2
        REQUIRE_THROWS_AS(rL3(n, idx, 1, rng1), std::invalid_argument);

        // L < 1 at construction
        REQUIRE_THROWS_AS(StationaryMaskIndexResampler(0), std::invalid_argument);
    }

    SECTION("Output indices in range; contiguity increases with L")
    {
        const std::size_t m = 360;

        auto frac_adjacent = [&](std::size_t L, randutils::mt19937_rng& r) {
            StationaryMaskIndexResampler res(L);
            std::vector<std::size_t> idx;
            res(n, idx, m, r);

            REQUIRE(idx.size() == m);
            // In-range
            for (auto k : idx)
            {
                REQUIRE(k < n);
            }
            // Contiguity fraction
            std::size_t adj = 0;
            for (std::size_t t = 0; t + 1 < idx.size(); ++t)
            {
                if (idx[t + 1] == (idx[t] + 1) % n) adj++;
            }
            return static_cast<double>(adj) / static_cast<double>(m - 1);
        };

        // Two separate RNGs to keep streams independent per L
        randutils::mt19937_rng rL2 = rng1; // copy state
        randutils::mt19937_rng rL6 = rng2; // identical seed to its own copy

        const double f2 = frac_adjacent(2, rL2);
        const double f6 = frac_adjacent(6, rL6);

        REQUIRE(f6 > f2 + 0.15); // clear separation
    }

    SECTION("Determinism under identical seeds (index mode)")
    {
        const std::size_t m = 300;
        StationaryMaskIndexResampler res(4);
        std::vector<std::size_t> idx1, idx2;

        res(n, idx1, m, rng1);
        res(n, idx2, m, rng2);

        REQUIRE(idx1.size() == m);
        REQUIRE(idx2.size() == m);
        REQUIRE(idx1 == idx2);
    }

    SECTION("Index-mode + gather reproduces value-mode output (under identical seeds)")
    {
        const std::size_t m = 300;
        const std::size_t L = 5;

        // Two RNGs with the same seed so each path sees the same random stream
        randutils::seed_seq_fe128 s2{2024u, 10u, 31u, 77u};
        randutils::mt19937_rng rng_idx(s2);
        randutils::mt19937_rng rng_val(s2);

        StationaryMaskIndexResampler  rIdx(L);
        StationaryMaskValueResampler<D> rVal(L);

        // Indices path
        std::vector<std::size_t> idx;
        rIdx(n, idx, m, rng_idx);
        std::vector<D> y_from_idx; y_from_idx.resize(m);
        for (std::size_t t = 0; t < m; ++t) y_from_idx[t] = x[idx[t]];

        // Value path
        std::vector<D> y_value;
        rVal(x, y_value, m, rng_val);

        // Should match exactly with identical RNG streams
        REQUIRE(y_value.size() == m);
        REQUIRE(y_from_idx == y_value);
    }
}

TEST_CASE("make_restart_mask: empirical mean block length ~ L", "[Resampler][MaskStats]")
{
    randutils::seed_seq_fe128 seed{42u, 99u, 7u, 21u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 5000;
    const double L = 6.0;

    const int R = 200;
    double total_blocks = 0.0;

    for (int r = 0; r < R; ++r)
    {
        auto mask = make_restart_mask(m, L, rng);
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        total_blocks += static_cast<double>(restarts);
    }

    const double mean_blocks = total_blocks / R;
    const double empirical_L = static_cast<double>(m) / mean_blocks;

    // Expect empirical mean block length close to theoretical L
    REQUIRE(empirical_L == Catch::Approx(L).margin(0.15 * L));
}
