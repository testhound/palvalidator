// NormalBootstrapTest.cpp
//
// Unit tests for NormalBootstrap (Wald CI using bootstrap SD) with a composable resampler.
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, num::to_double)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - NormalBootstrap.h
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
#include "NormalBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::NormalBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple arithmetic mean sampler
struct MeanSamplerNB
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
struct IIDResamplerForTestNB
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
using NormalBootstrapExec = NormalBootstrap<
    double,                // Decimal
    MeanSamplerNB,         // Sampler
    IIDResamplerForTestNB, // Resampler
    std::mt19937_64,       // Rng
    Exec                   // Executor
>;

TEST_CASE("NormalBootstrap: constructor validation", "[Bootstrap][Normal]")
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
        (NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>(399, 0.95, res)),
        std::invalid_argument
    );

    // CL out of range
    REQUIRE_THROWS_AS(
        (NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>(500, 0.5, res)),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>(500, 1.0, res)),
        std::invalid_argument
    );
}

TEST_CASE("NormalBootstrap: run() input validation", "[Bootstrap][Normal]")
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

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(500, 0.95, res);

    REQUIRE_THROWS_AS(nb.run(tiny, mean_sampler, rng), std::invalid_argument);
}

TEST_CASE("NormalBootstrap: basic behavior with mean sampler", "[Bootstrap][Normal][Mean]")
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

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    auto out = nb.run(x, mean_sampler, rng);

    SECTION("Invariants and finiteness")
    {
        REQUIRE(out.B == B);
        REQUIRE(out.n == n);
        REQUIRE(out.effective_B + out.skipped == out.B);
        REQUIRE(out.effective_B >= out.B / 2);

        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));

        REQUIRE(out.lower <= out.mean);
        REQUIRE(out.mean  <= out.upper);
        REQUIRE(out.cl == Catch::Approx(CL).margin(1e-12));

        REQUIRE(out.se_boot >= 0.0);
    }

    SECTION("Higher CL widens the interval (90% vs 95%)")
    {
        randutils::seed_seq_fe128 seedA{11u,22u,33u,44u};
        randutils::seed_seq_fe128 seedB{11u,22u,33u,44u};
        std::mt19937_64 rngA(seedA), rngB(seedB);

        NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            nb90(B, 0.90, res),
            nb95(B, 0.95, res);

        auto r90 = nb90.run(x, mean_sampler, rngA);
        auto r95 = nb95.run(x, mean_sampler, rngB);

        const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
        const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);

        REQUIRE(w95 >= w90 - 1e-12);
    }
}

TEST_CASE("NormalBootstrap: ThreadPoolExecutor consistency", "[Bootstrap][Normal][ThreadPool]")
{
    // non-trivial data
    std::mt19937_64 gen_data(12345);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> x; x.reserve(500);
    for (int i = 0; i < 500; ++i)
    {
        double v = g(gen_data);
        if ((i % 17) == 0) v *= 1.3;
        x.push_back(v);
    }

    const double      CL = 0.95;
    const std::size_t B  = 500;

    IIDResamplerForTestNB resampler{};
    MeanSamplerNB         sampler{};

    NormalBootstrapExec<concurrency::SingleThreadExecutor>  nb_single(B, CL, resampler);
    NormalBootstrapExec<concurrency::ThreadPoolExecutor<4>> nb_pool  (B, CL, resampler);

    std::mt19937_64 rng1(0xCAFEu);
    std::mt19937_64 rng2(0xCAFEu);

    auto R1 = nb_single.run(x, sampler, rng1);
    auto R2 = nb_pool.run  (x, sampler, rng2);

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

TEST_CASE("NormalBootstrap: diagnostics unavailable before run", "[Bootstrap][Normal][Diagnostics]")
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

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    SECTION("hasDiagnostics is false before any run()")
    {
        REQUIRE_FALSE(nb.hasDiagnostics());
    }

    SECTION("Diagnostic getters throw before run()")
    {
        REQUIRE_THROWS_AS(nb.getBootstrapStatistics(), std::logic_error);
        REQUIRE_THROWS_AS(nb.getBootstrapMean(),       std::logic_error);
        REQUIRE_THROWS_AS(nb.getBootstrapVariance(),   std::logic_error);
        REQUIRE_THROWS_AS(nb.getBootstrapSe(),         std::logic_error);
    }
}

TEST_CASE("NormalBootstrap: diagnostics consistent with Result", "[Bootstrap][Normal][Diagnostics]")
{
    using D = DecimalType;

    // Moderately nontrivial data: simple ramp 0..19
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

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    auto out = nb.run(x, mean_sampler, rng);

    REQUIRE(nb.hasDiagnostics());

    const auto& stats = nb.getBootstrapStatistics();
    const double mean_boot = nb.getBootstrapMean();
    const double var_boot  = nb.getBootstrapVariance();
    const double se_boot   = nb.getBootstrapSe();

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
        for (double val : stats) {
            const double d = val - m;
            v += d * d;
        }
        if (stats.size() > 1) {
            v /= static_cast<double>(stats.size() - 1);
        }

        const double se = std::sqrt(v);

        REQUIRE(mean_boot == Catch::Approx(m).margin(1e-12));
        REQUIRE(var_boot  == Catch::Approx(v).margin(1e-12));
        REQUIRE(se_boot   == Catch::Approx(se).margin(1e-12));

        // And se_boot should agree with the Result's se_boot
        REQUIRE(out.se_boot == Catch::Approx(se_boot).margin(1e-12));
    }
}

TEST_CASE("NormalBootstrap: copy constructor", "[Bootstrap][Normal][CopyConstructor]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B  = 500;
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb_original(B, CL, res);

    SECTION("Copy constructor creates independent object")
    {
        auto nb_copy = nb_original;  // Copy constructor

        // Basic properties should match
        REQUIRE(nb_copy.B() == nb_original.B());
        REQUIRE(nb_copy.CL() == nb_original.CL());

        // Diagnostics should not be available on copy (fresh state)
        REQUIRE_FALSE(nb_copy.hasDiagnostics());
        REQUIRE_FALSE(nb_original.hasDiagnostics());

        // Run on original
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
        std::mt19937_64 rng(seed);
        
        auto result_orig = nb_original.run(x, mean_sampler, rng);
        
        // Original should have diagnostics now, copy should not
        REQUIRE(nb_original.hasDiagnostics());
        REQUIRE_FALSE(nb_copy.hasDiagnostics());
    }
}

TEST_CASE("NormalBootstrap: move constructor", "[Bootstrap][Normal][MoveConstructor]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B  = 500;
    const double      CL = 0.95;

    SECTION("Move constructor transfers state correctly")
    {
        NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            nb_original(B, CL, res);

        // Run once to populate diagnostics
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
        std::mt19937_64 rng(seed);
        
        auto result_orig = nb_original.run(x, mean_sampler, rng);
        REQUIRE(nb_original.hasDiagnostics());
        
        // Move construct
        auto nb_moved = std::move(nb_original);
        
        // Basic properties should be transferred
        REQUIRE(nb_moved.B() == B);
        REQUIRE(nb_moved.CL() == CL);
        
        // Diagnostics should be transferred, moved-from object should have validity reset
        REQUIRE(nb_moved.hasDiagnostics());
        // Note: We don't test nb_original state after move since it's in unspecified state
    }
}

TEST_CASE("NormalBootstrap: copy assignment operator", "[Bootstrap][Normal][CopyAssignment]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B1 = 500;
    const std::size_t B2 = 600;
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb_source(B1, CL, res);
    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb_dest(B2, CL, res);

    SECTION("Copy assignment replaces configuration")
    {
        REQUIRE(nb_dest.B() == B2);  // Initial state
        
        nb_dest = nb_source;  // Copy assignment
        
        REQUIRE(nb_dest.B() == B1);  // Should match source
        REQUIRE(nb_dest.CL() == CL);
        REQUIRE_FALSE(nb_dest.hasDiagnostics());  // Should be clear state
    }
}

TEST_CASE("NormalBootstrap: move assignment operator", "[Bootstrap][Normal][MoveAssignment]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B1 = 500;
    const std::size_t B2 = 600;
    const double      CL = 0.95;

    SECTION("Move assignment transfers state correctly")
    {
        NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            nb_source(B1, CL, res);
        NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            nb_dest(B2, CL, res);

        // Run on source to get diagnostics
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
        std::mt19937_64 rng(seed);
        
        auto result_orig = nb_source.run(x, mean_sampler, rng);
        REQUIRE(nb_source.hasDiagnostics());
        REQUIRE(nb_dest.B() == B2);  // Initial dest state
        
        // Move assign
        nb_dest = std::move(nb_source);
        
        REQUIRE(nb_dest.B() == B1);  // Should match moved source
        REQUIRE(nb_dest.CL() == CL);
        REQUIRE(nb_dest.hasDiagnostics());  // Should transfer diagnostics
    }
}
