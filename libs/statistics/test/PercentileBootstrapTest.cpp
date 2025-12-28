// PercentileBootstrapTest.cpp
//
// Unit tests for PercentileBootstrap (standard percentile CI) with a composable resampler.
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, num::to_double)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - PercentileBootstrap.h
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
#include "PercentileBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::PercentileBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple sampler: arithmetic mean
struct MeanSamplerPB
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(v);
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// Minimal IID value resampler for tests (no blocks); matches (src, dst, m, rng) + getL()
struct IIDResamplerForTestPB
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
        for (std::size_t i = 0; i < m; ++i)
        {
            dst[i] = src[pick(rng)];
        }
    }
};

// Convenience alias for executor-parametrized tests
template <typename Exec>
using PctBootstrapExec = PercentileBootstrap<
    double,                // Decimal
    MeanSamplerPB,         // Sampler
    IIDResamplerForTestPB, // Resampler
    std::mt19937_64,       // Rng
    Exec                   // Executor
>;


TEST_CASE("PercentileBootstrap: constructor validation", "[Bootstrap][Percentile]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    // B < 400
    REQUIRE_THROWS_AS(
        (PercentileBootstrap<D, std::function<D(const std::vector<D>&)>, StationaryMaskValueResampler<D>>(399, 0.95, res)),
        std::invalid_argument
    );

    // CL out of range
    REQUIRE_THROWS_AS(
        (PercentileBootstrap<D, std::function<D(const std::vector<D>&)>, StationaryMaskValueResampler<D>>(500, 0.5, res)),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (PercentileBootstrap<D, std::function<D(const std::vector<D>&)>, StationaryMaskValueResampler<D>>(500, 1.0, res)),
        std::invalid_argument
    );
}

TEST_CASE("PercentileBootstrap: run() input validation", "[Bootstrap][Percentile]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> tiny{ D(1), D(2) };
    randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
    std::mt19937_64 rng(seed);

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(500, 0.95, res);

    // n < 3 → invalid_argument
    REQUIRE_THROWS_AS(pb.run(tiny, mean_sampler, rng), std::invalid_argument);
}

TEST_CASE("PercentileBootstrap: basic behavior with mean sampler (small-n)",
          "[Bootstrap][Percentile][Mean][SmallN]")
{
    using D = DecimalType;

    // Small-n series (n=20), simple linear data 0..19 for deterministic baseline mean
    const std::size_t n = 20;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(/*L=*/3);
    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    std::mt19937_64 rng(seed);

    const std::size_t B  = 500;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    auto out = pb.run(x, mean_sampler, rng);

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
    }

    SECTION("Higher CL widens the interval (90% vs 95%)")
    {
        randutils::seed_seq_fe128 seedA{11u,22u,33u,44u};
        randutils::seed_seq_fe128 seedB{11u,22u,33u,44u};
        std::mt19937_64 rngA(seedA), rngB(seedB);

        PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pb90(B, 0.90, res),
            pb95(B, 0.95, res);

        auto r90 = pb90.run(x, mean_sampler, rngA);
        auto r95 = pb95.run(x, mean_sampler, rngB);

        const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
        const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);

        REQUIRE(w95 >= w90 - 1e-12);
    }
}

TEST_CASE("PercentileBootstrap: runs correctly with ThreadPoolExecutor",
          "[Bootstrap][Percentile][ThreadPool]")
{
    // Synthetic data: mildly non-Gaussian to exercise percentile CI
    std::mt19937_64 gen_data(12345);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> x; x.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        double v = g(gen_data);
        if ((i % 25) == 0) v *= 1.5;     // small heteroskedastic bumps
        x.push_back(v);
    }

    const double CL = 0.95;
    const std::size_t B = 500;

    // Resampler + sampler
    IIDResamplerForTestPB resampler{};
    MeanSamplerPB         sampler{};

    // Construct single-threaded and thread-pooled variants
    PctBootstrapExec<concurrency::SingleThreadExecutor>  pct_single(B, CL, resampler);
    PctBootstrapExec<concurrency::ThreadPoolExecutor<4>> pct_pool  (B, CL, resampler);

    // IMPORTANT: use identical RNG seeds for deterministic equivalence
    std::mt19937_64 rng1(0xBEEFu);
    std::mt19937_64 rng2(0xBEEFu);

    auto R1 = pct_single.run(x, sampler, rng1);
    auto R2 = pct_pool.run  (x, sampler, rng2);

    // Basic invariants
    REQUIRE(R1.n == R2.n);
    REQUIRE(R1.B == R2.B);
    REQUIRE(R1.effective_B > R1.B / 2);
    REQUIRE(R2.effective_B == R1.effective_B);
    REQUIRE(R1.skipped == R2.skipped);

    // Numeric equivalence (parallel outer should be bit-stable here; allow tiny tolerance)
    auto near = [](double a, double b, double tol) {
        return std::fabs(a - b) <= tol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    };

    const double tight = 1e-12;

    REQUIRE( near(static_cast<double>(R1.mean),  static_cast<double>(R2.mean),  tight) );
    REQUIRE( near(static_cast<double>(R1.lower), static_cast<double>(R2.lower), tight) );
    REQUIRE( near(static_cast<double>(R1.upper), static_cast<double>(R2.upper), tight) );

    // CI ordering sanity
    REQUIRE(static_cast<double>(R1.lower) <= static_cast<double>(R1.upper));
    REQUIRE(static_cast<double>(R2.lower) <= static_cast<double>(R2.upper));
}

TEST_CASE("PercentileBootstrap: diagnostics unavailable before run",
          "[Bootstrap][Percentile][Diagnostics]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B  = 500;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    SECTION("hasDiagnostics is false before any run()")
    {
        REQUIRE_FALSE(pb.hasDiagnostics());
    }

    SECTION("Diagnostic getters throw before run()")
    {
        REQUIRE_THROWS_AS(pb.getBootstrapStatistics(), std::logic_error);
        REQUIRE_THROWS_AS(pb.getBootstrapMean(),       std::logic_error);
        REQUIRE_THROWS_AS(pb.getBootstrapVariance(),   std::logic_error);
        REQUIRE_THROWS_AS(pb.getBootstrapSe(),         std::logic_error);
    }
}

TEST_CASE("PercentileBootstrap: diagnostics consistent with Result",
          "[Bootstrap][Percentile][Diagnostics]")
{
    using D = DecimalType;

    // Simple nontrivial data: 0..19
    const std::size_t n = 20;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);

    const std::size_t B  = 500;
    const double      CL = 0.95;

    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    std::mt19937_64 rng(seed);

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    auto out = pb.run(x, mean_sampler, rng);

    REQUIRE(pb.hasDiagnostics());

    const auto& stats = pb.getBootstrapStatistics();
    const double mean_boot = pb.getBootstrapMean();
    const double var_boot  = pb.getBootstrapVariance();
    const double se_boot   = pb.getBootstrapSe();

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

TEST_CASE("PercentileBootstrap: copy constructor", "[Bootstrap][Percentile][CopyConstructor]")
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

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb_original(B, CL, res);

    SECTION("Copy constructor creates independent object")
    {
        auto pb_copy = pb_original;  // Copy constructor

        // Basic properties should match
        REQUIRE(pb_copy.B() == pb_original.B());
        REQUIRE(pb_copy.CL() == pb_original.CL());

        // Diagnostics should not be available on copy (fresh state)
        REQUIRE_FALSE(pb_copy.hasDiagnostics());
        REQUIRE_FALSE(pb_original.hasDiagnostics());

        // Run on original
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
        std::mt19937_64 rng(seed);
        
        (void)pb_original.run(x, mean_sampler, rng);
        
        // Original should have diagnostics now, copy should not
        REQUIRE(pb_original.hasDiagnostics());
        REQUIRE_FALSE(pb_copy.hasDiagnostics());
    }
}

TEST_CASE("PercentileBootstrap: move constructor", "[Bootstrap][Percentile][MoveConstructor]")
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
        PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pb_original(B, CL, res);

        // Run once to populate diagnostics
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
        std::mt19937_64 rng(seed);
        
        (void)pb_original.run(x, mean_sampler, rng);
        REQUIRE(pb_original.hasDiagnostics());
        
        // Move construct
        auto pb_moved = std::move(pb_original);
        
        // Basic properties should be transferred
        REQUIRE(pb_moved.B() == B);
        REQUIRE(pb_moved.CL() == CL);
        
        // Diagnostics should be transferred, moved-from object should have validity reset
        REQUIRE(pb_moved.hasDiagnostics());
        // Note: We don't test pb_original state after move since it's in unspecified state
    }
}

TEST_CASE("PercentileBootstrap: copy assignment operator", "[Bootstrap][Percentile][CopyAssignment]")
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

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb_source(B1, CL, res);
    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb_dest(B2, CL, res);

    SECTION("Copy assignment replaces configuration")
    {
        REQUIRE(pb_dest.B() == B2);  // Initial state
        
        pb_dest = pb_source;  // Copy assignment
        
        REQUIRE(pb_dest.B() == B1);  // Should match source
        REQUIRE(pb_dest.CL() == CL);
        REQUIRE_FALSE(pb_dest.hasDiagnostics());  // Should be clear state
    }
}

TEST_CASE("PercentileBootstrap: move assignment operator", "[Bootstrap][Percentile][MoveAssignment]")
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
        PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pb_source(B1, CL, res);
        PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pb_dest(B2, CL, res);

        // Run on source to get diagnostics
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
        std::mt19937_64 rng(seed);
        
        (void)pb_source.run(x, mean_sampler, rng);
        REQUIRE(pb_source.hasDiagnostics());
        REQUIRE(pb_dest.B() == B2);  // Initial dest state
        
        // Move assign
        pb_dest = std::move(pb_source);
        
        REQUIRE(pb_dest.B() == B1);  // Should match moved source
        REQUIRE(pb_dest.CL() == CL);
        REQUIRE(pb_dest.hasDiagnostics());  // Should transfer diagnostics
    }
}
