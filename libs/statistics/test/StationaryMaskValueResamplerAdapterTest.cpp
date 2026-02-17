#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "StationaryMaskResamplers.h"  // contains StationaryMaskValueResampler and the Adapter
#include "number.h"
#include "randutils.hpp"

using palvalidator::resampling::StationaryMaskValueResampler;
using palvalidator::resampling::StationaryMaskIndexResampler;

// Bring the adapter into scope (defined in StationaryMaskResamplers.h per our patch)
template <class Decimal, class Rng = randutils::mt19937_rng>
using MaskValueResamplerAdapter = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal, Rng>;

TEST_CASE("StationaryMaskValueResamplerAdapter::operator() matches value-resampler output under identical RNG", "[Resampler][Adapter][operator()]")
{
    using D = num::DefaultNumber;

    // Monotone source so we can reason about indices/values easily
    const std::size_t n = 300;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    const std::size_t m = n;   // typical bootstrap replicate = same length
    const std::size_t L = 5;   // mean block length

    // Identical RNG seeds so both paths consume the same stream
    randutils::seed_seq_fe128 seed{2025u, 10u, 31u, 99u};
    randutils::mt19937_rng rng_val(seed);
    randutils::mt19937_rng rng_adp(seed);

    // Baseline: value-resampler
    StationaryMaskValueResampler<D> valRes(L);
    std::vector<D> y_val;
    valRes(x, y_val, m, rng_val);

    // Adapter wraps value-resampler but returns by value and exposes jackknife
    MaskValueResamplerAdapter<D> adp(L);
    std::vector<D> y_adp = adp(x, m, rng_adp);

    REQUIRE(y_adp.size() == m);
    REQUIRE(y_val.size() == m);
    // With identical RNG and same implementation underneath, outputs should match exactly
    REQUIRE(y_adp == y_val);
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife on constant series",
          "[Resampler][Adapter][Jackknife]")
{
    using D = num::DefaultNumber;

    // Constant series → any delete-block jackknife mean equals the constant,
    // regardless of which block is deleted or how many pseudo-values are produced.
    const std::size_t n = 64;
    const D c = D(3.14159);
    std::vector<D> x(n, c);

    MaskValueResamplerAdapter<D> adp(/*L=*/4);

    auto meanFn = [](const std::vector<D>& v) -> D {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    // Non-overlapping Künsch jackknife: numBlocks = floor(n / L_eff)
    // n=64, L=4 → L_eff = min(4, 64-2) = 4 → numBlocks = floor(64/4) = 16
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min<std::size_t>(adp.getL(), n - minKeep); // 4
    const std::size_t numBlocks = n / L_eff;                                        // 16
    REQUIRE(jk.size() == numBlocks);

    // Every pseudo-value must equal the constant regardless of which block
    // was deleted — the mean of any subset of a constant series is that constant.
    for (const auto& v : jk)
        REQUIRE(num::to_double(v) ==
                Catch::Approx(num::to_double(c)).epsilon(1e-12));
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife with large L clamps to n-minKeep",
          "[Resampler][Adapter][Jackknife][Edge]")
{
    using D = num::DefaultNumber;

    // n=8, L=1000 → L_eff = min(1000, n - minKeep) = min(1000, 6) = 6
    // keep=2, numBlocks = floor(8/6) = 1
    //
    // Old behaviour clamped to n-1=7, giving keep=1 — degenerate: statistics
    // such as Sharpe or variance are undefined on a single observation, and
    // the test was explicitly verifying that broken single-element regime.
    // The new minKeep=2 clamp guarantees keep >= 2 for all valid inputs.
    const std::size_t n = 8;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(static_cast<int>(i))); // x = [0,1,2,3,4,5,6,7]

    MaskValueResamplerAdapter<D> adp(/*L=*/1000);

    auto meanFn = [](const std::vector<D>& v) -> D {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    // numBlocks = floor(n / L_eff) = floor(8/6) = 1
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min<std::size_t>(adp.getL(), n - minKeep); // 6
    const std::size_t numBlocks = n / L_eff;                                        // 1
    REQUIRE(jk.size() == numBlocks);

    // b=0: start=0, delete [0,1,2,3,4,5], start_keep=6
    //      tail = min(2, 8-6) = 2, head = 0 → y = [6, 7], mean = 6.5
    const double expected = 6.5;
    REQUIRE(num::to_double(jk[0]) == Catch::Approx(expected).epsilon(1e-12));
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife returns floor(n/L) finite stats and shows variation on monotone series",
          "[Resampler][Adapter][Jackknife][Sanity]")
{
    using D = num::DefaultNumber;

    // n=101, L=6 → L_eff = min(6, 101-2) = 6, numBlocks = floor(101/6) = 16
    // Odd length chosen for asymmetry; numBlocks=16 with 5 observations unused
    // (101 mod 6 = 5) — this is expected and correct for non-overlapping blocks.
    const std::size_t n = 101;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(static_cast<int>(i)));

    MaskValueResamplerAdapter<D> adp(/*L=*/6);

    auto meanFn = [](const std::vector<D>& v) -> D {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    // Non-overlapping Künsch jackknife: floor(n / L_eff) = floor(101/6) = 16
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min<std::size_t>(adp.getL(), n - minKeep); // 6
    const std::size_t numBlocks = n / L_eff;                                        // 16
    REQUIRE(jk.size() == numBlocks);

    // All pseudo-values must be finite and within the data range [0, 100]
    double minv = +std::numeric_limits<double>::infinity();
    double maxv = -std::numeric_limits<double>::infinity();
    for (const auto& z : jk)
    {
        const double d = num::to_double(z);
        REQUIRE(std::isfinite(d));
        REQUIRE(d >= 0.0);
        REQUIRE(d <= 100.0);
        minv = std::min(minv, d);
        maxv = std::max(maxv, d);
    }

    // Variation: deleting different non-overlapping blocks from a monotone series
    // produces different means — earlier blocks have lower values, later blocks
    // have higher values, so pseudo-values must not all be identical.
    REQUIRE(maxv > minv);
}

TEST_CASE("StationaryMaskValueResamplerAdapter: L=1 reports correctly", "[Resampler][Adapter][L=1][Report]")
{
    using D = num::DefaultNumber;

    // Construct adapter with L=1. After the ctor change, it should report exactly 1.
    MaskValueResamplerAdapter<D> adp(/*L=*/1);

    // These accessors are provided by the adapter (forwarding the inner resampler's config)
    REQUIRE(adp.getL() == 1);
    REQUIRE(adp.meanBlockLen() == 1);
}

TEST_CASE("StationaryMaskValueResamplerAdapter: L=1 yields IID-like no-continuation", "[Resampler][Adapter][L=1][IID]")
{
    using D = num::DefaultNumber;

    // Monotone source so we can detect block continuation via value adjacency.
    const std::size_t n = 997; // prime length to avoid trivial periodic artifacts
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    const std::size_t m = 5000; // long replicate to get a good signal
    MaskValueResamplerAdapter<D> adp(/*L=*/1);

    randutils::seed_seq_fe128 seed{2025u, 11u, 12u, 1u};
    randutils::mt19937_rng rng(seed);

    // Generate one long replicate
    std::vector<D> y = adp(x, m, rng);
    REQUIRE(y.size() == m);

    // Count how many times the resampler "continues" the previous block:
    // for monotone x, continuation means y[k] == (y[k-1] + 1) % n
    std::size_t continuations = 0;
    for (std::size_t k = 1; k < m; ++k)
    {
        const int prev = static_cast<int>(num::to_double(y[k-1]));
        const int curr = static_cast<int>(num::to_double(y[k]));
        const int cont = (prev + 1) % static_cast<int>(n);
        if (curr == cont) ++continuations;
    }

    // With L=1, every step is a restart → indices at t-1 and t are independent uniforms.
    // There's still a 1/n chance the new start equals (prev+1)%n purely by coincidence.
    // Model as Binomial(m-1, p=1/n) and allow a generous sigma band.
    const double p  = 1.0 / static_cast<double>(n);
    const double N  = static_cast<double>(m - 1);
    const double mu = N * p;
    const double sd = std::sqrt(N * p * (1.0 - p));
    // 6σ is very generous and should be plenty stable across platforms/seeds.
    REQUIRE(std::abs(static_cast<double>(continuations) - mu) <= 6.0 * sd);
}
