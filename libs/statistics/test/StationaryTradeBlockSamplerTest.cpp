#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cstddef>
#include <limits>

#include "MetaLosingStreakBootstrapBound.h"  // for StationaryTradeBlockSampler
#include "number.h"
#include "randutils.hpp"

using num::DefaultNumber;

TEST_CASE("StationaryTradeBlockSampler: basic shape and domain", "[Sampler][StationaryTradeBlock]")
{
    using D = DefaultNumber;
    using Sampler = mkc_timeseries::StationaryTradeBlockSampler<D>;

    // Build a monotone source so we can sanity-check the sampled values are from the source
    const std::size_t n = 250;
    std::vector<D> src; src.reserve(n);
    for (std::size_t i = 0; i < n; ++i) src.emplace_back(D(static_cast<int>(i)));

    // RNG with deterministic seed for test stability
    randutils::seed_seq_fe128 seed{111u, 222u, 333u, 444u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 400;
    Sampler sampler(/*expectedBlockLenTrades=*/4);

    std::vector<D> out;
    sampler.sample(src, out, rng, m);

    REQUIRE(out.size() == m);

    // Domain: all values must come from src's domain [0, n-1]
    for (const auto& v : out) {
        double vd = num::to_double(v);
        REQUIRE(vd >= 0.0);
        REQUIRE(vd < static_cast<double>(n));
    }
}

TEST_CASE("StationaryTradeBlockSampler: contiguity increases with larger L", "[Sampler][StationaryTradeBlock]")
{
    using D = DefaultNumber;
    using Sampler = mkc_timeseries::StationaryTradeBlockSampler<D>;

    const std::size_t n = 300, m = 600;
    std::vector<D> src; src.reserve(n);
    for (std::size_t i = 0; i < n; ++i) src.emplace_back(D(static_cast<int>(i)));

    // Independent RNGs (same base seed sequence) so streams are reproducible but separate per L
    randutils::seed_seq_fe128 seed{2024u, 10u, 31u, 77u};
    randutils::mt19937_rng rngL2(seed);
    randutils::mt19937_rng rngL6(seed);

    auto contiguity = [&](std::size_t L, randutils::mt19937_rng& rng) {
        Sampler sampler(L);
        std::vector<D> out;
        sampler.sample(src, out, rng, m);

        std::size_t adjacent = 0;
        for (std::size_t t = 0; t + 1 < out.size(); ++t) {
            int cur = static_cast<int>(num::to_double(out[t]));
            int nxt = static_cast<int>(num::to_double(out[t + 1]));
            if (nxt == (cur + 1) % static_cast<int>(n)) adjacent++;
        }
        return static_cast<double>(adjacent) / static_cast<double>(m - 1);
    };

    const double f2 = contiguity(2, rngL2);
    const double f6 = contiguity(6, rngL6);

    // With L=6 we expect noticeably more contiguous steps than with L=2
    REQUIRE(f6 > f2 + 0.15);
}

TEST_CASE("StationaryTradeBlockSampler: determinism under identical seeds", "[Sampler][StationaryTradeBlock]")
{
    using D = DefaultNumber;
    using Sampler = mkc_timeseries::StationaryTradeBlockSampler<D>;

    const std::size_t n = 180, m = 360;
    std::vector<D> src; src.reserve(n);
    for (std::size_t i = 0; i < n; ++i) src.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{9u, 8u, 7u, 6u};
    randutils::mt19937_rng r1(seed);
    randutils::mt19937_rng r2(seed);

    Sampler sampler(4);
    std::vector<D> y1, y2;
    sampler.sample(src, y1, r1, m);
    sampler.sample(src, y2, r2, m);

    REQUIRE(y1.size() == m);
    REQUIRE(y2.size() == m);
    REQUIRE(y1 == y2);
}

TEST_CASE("StationaryTradeBlockSampler: n==0 or m==0 yields empty output (graceful no-op)", "[Sampler][StationaryTradeBlock]")
{
    using D = DefaultNumber;
    using Sampler = mkc_timeseries::StationaryTradeBlockSampler<D>;

    randutils::mt19937_rng rng; // auto-seeded

    // n == 0 → out cleared
    {
        std::vector<D> src; // empty
        std::vector<D> out{ D(1) };
        Sampler sampler(3);
        sampler.sample(src, out, rng, /*m=*/10);
        REQUIRE(out.empty());
    }

    // m == 0 → out cleared
    {
        std::vector<D> src{ D(1), D(2), D(3) };
        std::vector<D> out{ D(1) };
        Sampler sampler(3);
        sampler.sample(src, out, rng, /*m=*/0);
        REQUIRE(out.empty());
    }
}
