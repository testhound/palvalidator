// BasicBootstrapTest.cpp
//
// Unit tests for BasicBootstrap (reverse percentile CI) with a composable resampler.
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, num::to_double)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - BasicBootstrap.h
//  - ParallelExecutors.h
//  - ParallelFor.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <random>

#include "number.h"
#include "randutils.hpp"
#include "StationaryMaskResamplers.h"
#include "BasicBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::BasicBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple arithmetic mean sampler
struct MeanSamplerBB
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(v);
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// IID-with-replacement resampler for tests
struct IIDResamplerForTestBB
{
    std::size_t getL() const noexcept { return 0; }

    template <typename Decimal, typename Rng>
    void operator()(const std::vector<Decimal>& src,
                    std::vector<Decimal>&       dst,
                    std::size_t                 m,
                    Rng&                         rng) const
    {
        std::uniform_int_distribution<std::size_t> pick(0, src.size() - 1);
        dst.resize(m);
        for (std::size_t i = 0; i < m; ++i) {
            dst[i] = src[pick(rng)];
        }
    }
};

template <typename Exec>
using BasicBootstrapExec = BasicBootstrap<
    double,                // Decimal
    MeanSamplerBB,         // Sampler
    IIDResamplerForTestBB, // Resampler
    std::mt19937_64,       // Rng
    Exec                   // Executor
>;

TEST_CASE("BasicBootstrap: constructor validation", "[Bootstrap][Basic]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    // B < 400
    REQUIRE_THROWS_AS(
        (BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>(399, 0.95, res)),
        std::invalid_argument
    );

    // CL out of range
    REQUIRE_THROWS_AS(
        (BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>(500, 0.5, res)),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>(500, 1.0, res)),
        std::invalid_argument
    );
}

TEST_CASE("BasicBootstrap: run() input validation", "[Bootstrap][Basic]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> tiny{ D(1), D(2) };
    randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
    std::mt19937_64 rng(seed);

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(500, 0.95, res);

    REQUIRE_THROWS_AS(bb.run(tiny, mean_sampler, rng), std::invalid_argument);
}

TEST_CASE("BasicBootstrap: basic behavior with mean sampler", "[Bootstrap][Basic][Mean]")
{
    using D = DecimalType;

    const std::size_t n = 20;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);

    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    std::mt19937_64 rng(seed);

    const std::size_t B  = 500;
    const double      CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    auto out = bb.run(x, mean_sampler, rng);

    SECTION("Invariants and finiteness")
    {
        REQUIRE(out.B == B);
        REQUIRE(out.n == n);
        REQUIRE(out.effective_B + out.skipped == out.B);
        REQUIRE(out.effective_B >= out.B / 2);

        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));

        REQUIRE(out.cl == Catch::Approx(CL).margin(1e-12));
        REQUIRE(out.lower <= out.upper);
    }

    SECTION("Higher CL widens the interval (90% vs 95%)")
    {
        randutils::seed_seq_fe128 seedA{11u,22u,33u,44u};
        randutils::seed_seq_fe128 seedB{11u,22u,33u,44u};
        std::mt19937_64 rngA(seedA), rngB(seedB);

        BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            bb90(B, 0.90, res),
            bb95(B, 0.95, res);

        auto r90 = bb90.run(x, mean_sampler, rngA);
        auto r95 = bb95.run(x, mean_sampler, rngB);

        const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
        const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);

        REQUIRE(w95 >= w90 - 1e-12);
    }
}

TEST_CASE("BasicBootstrap: ThreadPoolExecutor consistency", "[Bootstrap][Basic][ThreadPool]")
{
    std::mt19937_64 gen_data(98765);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> x; x.reserve(500);
    for (int i = 0; i < 500; ++i)
    {
        double v = g(gen_data);
        if ((i % 13) == 0) v *= 1.4;
        x.push_back(v);
    }

    const double      CL = 0.95;
    const std::size_t B  = 500;

    IIDResamplerForTestBB resampler{};
    MeanSamplerBB         sampler{};

    BasicBootstrapExec<concurrency::SingleThreadExecutor>  bb_single(B, CL, resampler);
    BasicBootstrapExec<concurrency::ThreadPoolExecutor<4>> bb_pool  (B, CL, resampler);

    std::mt19937_64 rng1(0x1234u);
    std::mt19937_64 rng2(0x1234u);

    auto R1 = bb_single.run(x, sampler, rng1);
    auto R2 = bb_pool.run  (x, sampler, rng2);

    REQUIRE(R1.n == R2.n);
    REQUIRE(R1.B == R2.B);
    REQUIRE(R1.effective_B > R1.B / 2);
    REQUIRE(R2.effective_B == R1.effective_B);
    REQUIRE(R1.skipped == R2.skipped);

    auto near = [](double a, double b, double tol) {
        return std::fabs(a - b) <= tol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    };

    const double tight = 1e-12;

    REQUIRE( near(static_cast<double>(R1.mean),  static_cast<double>(R2.mean),  tight) );
    REQUIRE( near(static_cast<double>(R1.lower), static_cast<double>(R2.lower), tight) );
    REQUIRE( near(static_cast<double>(R1.upper), static_cast<double>(R2.upper), tight) );

    REQUIRE(static_cast<double>(R1.lower) <= static_cast<double>(R1.upper));
    REQUIRE(static_cast<double>(R2.lower) <= static_cast<double>(R2.upper));
}

TEST_CASE("BasicBootstrap: diagnostics unavailable before run",
          "[Bootstrap][Basic][Diagnostics]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B  = 500;
    const double      CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    SECTION("hasDiagnostics is false before any run()")
    {
        REQUIRE_FALSE(bb.hasDiagnostics());
    }

    SECTION("Diagnostic getters throw before run()")
    {
        REQUIRE_THROWS_AS(bb.getBootstrapStatistics(), std::logic_error);
        REQUIRE_THROWS_AS(bb.getBootstrapMean(),       std::logic_error);
        REQUIRE_THROWS_AS(bb.getBootstrapVariance(),   std::logic_error);
        REQUIRE_THROWS_AS(bb.getBootstrapSe(),         std::logic_error);
    }
}

TEST_CASE("BasicBootstrap: diagnostics consistent with Result",
          "[Bootstrap][Basic][Diagnostics]")
{
    using D = DecimalType;

    // Simple nontrivial data: 0..19
    const std::size_t n = 20;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);

    const std::size_t B  = 500;
    const double      CL = 0.95;

    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    std::mt19937_64 rng(seed);

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    auto out = bb.run(x, mean_sampler, rng);

    REQUIRE(bb.hasDiagnostics());

    const auto& stats = bb.getBootstrapStatistics();
    const double mean_boot = bb.getBootstrapMean();
    const double var_boot  = bb.getBootstrapVariance();
    const double se_boot   = bb.getBootstrapSe();

    SECTION("Bootstrap statistics size matches effective_B")
    {
        REQUIRE(stats.size() == out.effective_B);
        REQUIRE(out.effective_B + out.skipped == out.B);
    }

    SECTION("Bootstrap mean/variance/SE match recomputation")
    {
        REQUIRE(stats.size() == out.effective_B);
        REQUIRE(stats.size() > 0);

        double m = 0.0;
        for (double v : stats) {
            m += v;
        }
        m /= static_cast<double>(stats.size());

        double v = 0.0;
        if (stats.size() > 1) {
            for (double val : stats) {
                const double d = val - m;
                v += d * d;
            }
            v /= static_cast<double>(stats.size() - 1);
        }

        const double se = std::sqrt(v);

        REQUIRE(mean_boot == Catch::Approx(m).margin(1e-12));
        REQUIRE(var_boot  == Catch::Approx(v).margin(1e-12));
        REQUIRE(se_boot   == Catch::Approx(se).margin(1e-12));
    }
}
