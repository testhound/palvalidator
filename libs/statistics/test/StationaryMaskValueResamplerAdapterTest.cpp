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

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife on constant series", "[Resampler][Adapter][Jackknife]")
{
    using D = num::DefaultNumber;

    // Constant series → any delete-block jackknife mean equals the constant
    const std::size_t n = 64;
    const D c = D(3.14159);
    std::vector<D> x(n, c);

    MaskValueResamplerAdapter<D> adp(/*L=*/4);
    auto meanFn = [](const std::vector<D>& v) -> D
    {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    REQUIRE(jk.size() == n);
    for (const auto& v : jk)
    {
        REQUIRE(num::to_double(v) == Catch::Approx(num::to_double(c)).epsilon(1e-12));
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife with L_eff = n-1 keeps exactly one element", "[Resampler][Adapter][Jackknife][Edge]")
{
    using D = num::DefaultNumber;

    // When L >= n-1, adapter uses L_eff = n-1; keep = 1 element at index (start + L_eff) % n
    const std::size_t n = 8;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i))); // x[i] = i

    // Choose huge L to force L_eff = n-1
    MaskValueResamplerAdapter<D> adp(/*L=*/1000);

    auto meanFn = [](const std::vector<D>& v) -> D
    {
        // keep == 1 in this regime, but write generically
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    REQUIRE(jk.size() == n);
    const std::size_t L_eff = n - 1;
    for (std::size_t start = 0; start < n; ++start)
    {
        const std::size_t kept = (start + L_eff) % n;
        // With keep=1, mean == that single kept element
        REQUIRE(num::to_double(jk[start]) == Catch::Approx(num::to_double(x[kept])).epsilon(1e-12));
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife returns n finite stats and shows variation on monotone series", "[Resampler][Adapter][Jackknife][Sanity]")
{
    using D = num::DefaultNumber;

    // Monotone values → delete-block jackknife means should vary with start
    const std::size_t n = 101; // odd length for asymmetry
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    MaskValueResamplerAdapter<D> adp(/*L=*/6);

    auto meanFn = [](const std::vector<D>& v) -> D
    {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    REQUIRE(jk.size() == n);

    // All finite and within the data range
    double minv = +std::numeric_limits<double>::infinity();
    double maxv = -std::numeric_limits<double>::infinity();
    std::size_t identical = 0;
    for (const auto& z : jk)
    {
        const double d = num::to_double(z);
        REQUIRE(std::isfinite(d));
        minv = std::min(minv, d);
        maxv = std::max(maxv, d);
    }

    // Variation: for a monotone x and delete-block L>=2, jk means should not be all identical
    REQUIRE(maxv > minv);
}
