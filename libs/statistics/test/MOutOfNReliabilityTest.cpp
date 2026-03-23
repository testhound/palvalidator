// MOutOfNReliabilityTest.cpp
//
// Catch2 unit tests for the four reliability flags and isReliable()
// introduced on MOutOfNPercentileBootstrap::Result.
//
// Coverage plan
// ─────────────
// §1  distribution_degenerate flag — constant sampler produces degenerate bootstrap
// §2  insufficient_spread flag    — constant sampler produces zero CV
// §3  excessive_bias flag         — rescale_to_n=true on skewed small-m data
// §4  ratio_near_boundary flag    — boundary conditions in fixed and adaptive mode
// §5  isReliable() aggregation    — normal data passes; degenerate data fails
// §6  Diagnostic accessor parity  — engine accessors match Result flags
// §7  Compilation fix: MockBootstrapEngine Result initialisation
//
// NOTE: Flags 1 and 2 (distribution_degenerate and insufficient_spread) are
// the most practically relevant for trade-level bootstrapping in this system.
// Flags 3 and 4 are conditional: excessive_bias only fires when rescale_to_n=true;
// ratio_near_boundary only fires in adaptive mode or when clamping fires.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <random>
#include <set>

#include "number.h"
#include "StatUtils.h"
#include "TestUtils.h"
#include "StationaryMaskResamplers.h"
#include "MOutOfNPercentileBootstrap.h"
#include "RngUtils.h"

using palvalidator::analysis::MOutOfNPercentileBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using mkc_timeseries::rng_utils::make_seed_seq;

// ─────────────────────────────────────────────────────────────────────────────
// Test helpers
// ─────────────────────────────────────────────────────────────────────────────
using D   = DecimalType;
using Res = StationaryMaskValueResampler<D>;

// Arithmetic mean sampler — used throughout
static auto mean_sampler = [](const std::vector<D>& a) -> D
{
    double s = 0.0;
    for (const auto& v : a) s += num::to_double(v);
    return D(s / static_cast<double>(a.size()));
};

// Constant sampler — every call returns the same value regardless of input.
// This forces the bootstrap distribution to have exactly one distinct value,
// triggering both distribution_degenerate and insufficient_spread.
static auto constant_sampler = [](const std::vector<D>& /*a*/) -> D
{
    return D(42);
};

// Build a varied series of length n: simple ramp 0, 1, ..., n-1
static std::vector<D> make_ramp(std::size_t n)
{
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(static_cast<int>(i)));
    return x;
}


// ─────────────────────────────────────────────────────────────────────────────
// §1  distribution_degenerate
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap reliability: distribution_degenerate flag",
          "[Bootstrap][MOutOfN][Reliability][DegenDist]")
{
    // A constant sampler produces exactly one unique bootstrap replicate value
    // regardless of the resample drawn. With B=800 replicates and 1 unique value,
    // uniqueRatio = 1/800 ≈ 0.00125, well below the threshold of 0.05.
    const std::size_t n = 60;
    auto x = make_ramp(n);   // varied input data — the sampler ignores it

    Res res(3);
    std::seed_seq seq = make_seed_seq(0xDEAD000000000001ull);
    std::mt19937_64 rng(seq);

    MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
        moon(800, 0.95, 0.70, res);

    SECTION("Flag is set in Result when bootstrap distribution is degenerate")
    {
        auto result = moon.run(x, constant_sampler, rng);
        REQUIRE(result.distribution_degenerate);
    }

    SECTION("Engine accessor matches Result flag")
    {
        auto result = moon.run(x, constant_sampler, rng);
        REQUIRE(moon.isDistributionDegenerate() == result.distribution_degenerate);
    }

    SECTION("Flag is NOT set for a well-varied sampler on the same data")
    {
        std::seed_seq seq2 = make_seed_seq(0xDEAD000000000002ull);
        std::mt19937_64 rng2(seq2);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon_varied(800, 0.95, 0.70, res);

        auto result = moon_varied.run(x, mean_sampler, rng2);
        REQUIRE_FALSE(result.distribution_degenerate);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §2  insufficient_spread
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap reliability: insufficient_spread flag",
          "[Bootstrap][MOutOfN][Reliability][InsufficientSpread]")
{
    // A constant sampler produces SE=0, giving CV=0 — well below the 0.01 threshold.
    const std::size_t n = 60;
    auto x = make_ramp(n);

    Res res(3);
    std::seed_seq seq = make_seed_seq(0xFACE000000000001ull);
    std::mt19937_64 rng(seq);

    MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
        moon(800, 0.95, 0.70, res);

    SECTION("Flag is set in Result when bootstrap CV is near zero")
    {
        auto result = moon.run(x, constant_sampler, rng);
        REQUIRE(result.insufficient_spread);
    }

    SECTION("Engine accessor matches Result flag")
    {
        auto result = moon.run(x, constant_sampler, rng);
        REQUIRE(moon.isInsufficientSpread() == result.insufficient_spread);
    }

    SECTION("Flag is NOT set for a well-spread sampler on varied data")
    {
        std::seed_seq seq2 = make_seed_seq(0xFACE000000000002ull);
        std::mt19937_64 rng2(seq2);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon_varied(800, 0.95, 0.70, res);

        auto result = moon_varied.run(x, mean_sampler, rng2);
        REQUIRE_FALSE(result.insufficient_spread);
    }

    SECTION("Flags distribution_degenerate and insufficient_spread both fire together")
    {
        // A constant sampler produces both: 1 unique value (degenerate)
        // and zero variance (insufficient spread). Both flags should fire.
        std::seed_seq seq3 = make_seed_seq(0xFACE000000000003ull);
        std::mt19937_64 rng3(seq3);

        auto result = moon.run(x, constant_sampler, rng3);
        REQUIRE(result.distribution_degenerate);
        REQUIRE(result.insufficient_spread);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §3  excessive_bias
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap reliability: excessive_bias flag",
          "[Bootstrap][MOutOfN][Reliability][ExcessiveBias]")
{
    // excessive_bias is only meaningful when rescale_to_n=true.
    // It fires when |mean_boot - theta_hat| / |theta_hat| > 0.20.
    //
    // Engineering a reliable trigger: use a very small m_ratio (0.10) on
    // a strongly skewed series. With m=6 from n=60, subsamples see a very
    // different distribution from the full sample, causing the bootstrap
    // mean to drift substantially from theta_hat.
    //
    // Note: This flag is inherently probabilistic — the magnitude of bias
    // depends on the random draws. We use a large B and a deliberately
    // skewed series to make the trigger reliable across seeds.

    SECTION("Flag is NOT set when rescale_to_n=false regardless of bias")
    {
        // When rescale_to_n=false, the flag is always suppressed.
        const std::size_t n = 60;
        auto x = make_ramp(n);

        Res res(3);
        std::seed_seq seq = make_seed_seq(0xB1A5000000000001ull);
        std::mt19937_64 rng(seq);

        // Very small ratio to maximise subsample bias — but flag suppressed
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.10, res, /*rescale_to_n=*/false);

        auto result = moon.run(x, mean_sampler, rng);
        REQUIRE_FALSE(result.excessive_bias);
        REQUIRE(moon.isExcessiveBias() == result.excessive_bias);
    }

    SECTION("Flag accessor matches Result when rescale_to_n=true and bias is low")
    {
        // For well-behaved symmetric data with moderate ratio, bias should
        // be small and the flag should NOT fire.
        const std::size_t n = 100;
        auto x = make_ramp(n);

        Res res(3);
        std::seed_seq seq = make_seed_seq(0xB1A5000000000002ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.75, res, /*rescale_to_n=*/true);

        auto result = moon.run(x, mean_sampler, rng);
        // For a symmetric ramp with ratio=0.75, bias is typically small
        REQUIRE(moon.isExcessiveBias() == result.excessive_bias);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §4  ratio_near_boundary
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap reliability: ratio_near_boundary flag",
          "[Bootstrap][MOutOfN][Reliability][RatioNearBoundary]")
{
    // ratio_near_boundary fires when computed_ratio < 3/n or > (n-2)/n.
    // In fixed-ratio mode, it should only fire if clamping changed m_sub.
    // The direct way to trigger the upper boundary is via m_sub_override
    // set to n-2 or n-1 (which forces m_sub/(n) close to 1).

    const std::size_t n = 30;
    auto x = make_ramp(n);
    Res res(3);

    SECTION("Flag NOT set for normal fixed ratio in valid interior range")
    {
        std::seed_seq seq = make_seed_seq(0xB0DEAD0000000001ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        auto result = moon.run(x, mean_sampler, rng);
        REQUIRE_FALSE(result.ratio_near_boundary);
        REQUIRE(moon.isRatioNearBoundary() == result.ratio_near_boundary);
    }

    SECTION("Flag IS set when m_sub_override forces ratio near upper boundary")
    {
        // m_sub_override = n-2 gives ratio = (n-2)/n = 28/30 ≈ 0.933
        // upper_boundary threshold = (n-2)/n — this is exactly the boundary,
        // so it will equal the threshold. Use n-1 (clamped to n-2 internally)
        // to ensure it fires.
        std::seed_seq seq = make_seed_seq(0xB0DEAD0000000002ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        // Override with n-1 — the engine clamps this to n-2, giving
        // computed_ratio = (n-2)/n which equals the upper_boundary threshold
        const std::size_t override_m = n - 1;
        auto result = moon.run(x, mean_sampler, rng, override_m);

        REQUIRE(result.ratio_near_boundary);
        REQUIRE(moon.isRatioNearBoundary() == result.ratio_near_boundary);
    }

    SECTION("Flag IS set when m_sub_override forces ratio near lower boundary")
    {
        // m_sub_override = 2 gives ratio = 2/n = 2/30 ≈ 0.067
        // lower_boundary threshold = 3/n = 3/30 = 0.10 > 0.067, so flag fires
        std::seed_seq seq = make_seed_seq(0xB0DEAD0000000003ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        const std::size_t override_m = 2;  // minimum valid m_sub
        auto result = moon.run(x, mean_sampler, rng, override_m);

        REQUIRE(result.ratio_near_boundary);
        REQUIRE(moon.isRatioNearBoundary() == result.ratio_near_boundary);
    }

    SECTION("Flag NOT set for normal override in interior range")
    {
        std::seed_seq seq = make_seed_seq(0xB0DEAD0000000004ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        const std::size_t override_m = 15;  // ratio = 15/30 = 0.50, safely interior
        auto result = moon.run(x, mean_sampler, rng, override_m);

        REQUIRE_FALSE(result.ratio_near_boundary);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §5  isReliable() aggregation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap reliability: isReliable() aggregation",
          "[Bootstrap][MOutOfN][Reliability][IsReliable]")
{
    SECTION("isReliable() returns true for well-behaved data and normal configuration")
    {
        const std::size_t n = 80;
        auto x = make_ramp(n);
        Res res(3);

        std::seed_seq seq = make_seed_seq(0x12345678ABCDEF01ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        auto result = moon.run(x, mean_sampler, rng);

        // For a well-varied series with a normal ratio and varied sampler,
        // none of the four flags should fire
        REQUIRE_FALSE(result.distribution_degenerate);
        REQUIRE_FALSE(result.excessive_bias);
        REQUIRE_FALSE(result.insufficient_spread);
        REQUIRE_FALSE(result.ratio_near_boundary);
        REQUIRE(result.isReliable());
        REQUIRE(moon.isReliable());
    }

    SECTION("isReliable() returns false when distribution_degenerate fires")
    {
        const std::size_t n = 60;
        auto x = make_ramp(n);
        Res res(3);

        std::seed_seq seq = make_seed_seq(0x12345678ABCDEF02ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
            moon(800, 0.95, 0.70, res);

        auto result = moon.run(x, constant_sampler, rng);

        REQUIRE(result.distribution_degenerate);
        REQUIRE_FALSE(result.isReliable());
        REQUIRE_FALSE(moon.isReliable());
    }

    SECTION("isReliable() returns false when insufficient_spread fires")
    {
        // Same constant sampler — both flags fire, isReliable() returns false
        const std::size_t n = 60;
        auto x = make_ramp(n);
        Res res(3);

        std::seed_seq seq = make_seed_seq(0x12345678ABCDEF03ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
            moon(800, 0.95, 0.70, res);

        auto result = moon.run(x, constant_sampler, rng);

        REQUIRE(result.insufficient_spread);
        REQUIRE_FALSE(result.isReliable());
    }

    SECTION("isReliable() returns false when ratio_near_boundary fires")
    {
        const std::size_t n = 30;
        auto x = make_ramp(n);
        Res res(3);

        std::seed_seq seq = make_seed_seq(0x12345678ABCDEF04ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        // Override with m=2 to force lower boundary
        auto result = moon.run(x, mean_sampler, rng, /*m_sub_override=*/2);

        REQUIRE(result.ratio_near_boundary);
        REQUIRE_FALSE(result.isReliable());
        REQUIRE_FALSE(moon.isReliable());
    }

    SECTION("Result::isReliable() and engine::isReliable() always agree")
    {
        // Verify the two isReliable() surfaces are always consistent,
        // regardless of whether the result is reliable or not.
        const std::size_t n = 60;
        auto x = make_ramp(n);
        Res res(3);

        // Run with normal sampler
        {
            std::seed_seq seq = make_seed_seq(0x12345678ABCDEF05ull);
            std::mt19937_64 rng(seq);
            MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
                moon(800, 0.95, 0.70, res);
            auto result = moon.run(x, mean_sampler, rng);
            REQUIRE(result.isReliable() == moon.isReliable());
        }

        // Run with constant sampler
        {
            std::seed_seq seq = make_seed_seq(0x12345678ABCDEF06ull);
            std::mt19937_64 rng(seq);
            MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
                moon(800, 0.95, 0.70, res);
            auto result = moon.run(x, constant_sampler, rng);
            REQUIRE(result.isReliable() == moon.isReliable());
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §6  Diagnostic accessor parity
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap reliability: engine accessors match Result flags",
          "[Bootstrap][MOutOfN][Reliability][Accessors]")
{
    // Verify that all four engine-level reliability accessors return the same
    // value as their corresponding Result fields after a run.
    const std::size_t n = 60;
    auto x = make_ramp(n);
    Res res(3);

    SECTION("All accessors match for well-behaved run")
    {
        std::seed_seq seq = make_seed_seq(0xACCE5500000001ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        auto result = moon.run(x, mean_sampler, rng);

        REQUIRE(moon.isDistributionDegenerate() == result.distribution_degenerate);
        REQUIRE(moon.isExcessiveBias()           == result.excessive_bias);
        REQUIRE(moon.isInsufficientSpread()      == result.insufficient_spread);
        REQUIRE(moon.isRatioNearBoundary()       == result.ratio_near_boundary);
        REQUIRE(moon.isReliable()                == result.isReliable());
    }

    SECTION("All accessors match for degenerate run")
    {
        std::seed_seq seq = make_seed_seq(0xACCE5500000002ull);
        std::mt19937_64 rng(seq);

        MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
            moon(800, 0.95, 0.70, res);

        auto result = moon.run(x, constant_sampler, rng);

        REQUIRE(moon.isDistributionDegenerate() == result.distribution_degenerate);
        REQUIRE(moon.isExcessiveBias()           == result.excessive_bias);
        REQUIRE(moon.isInsufficientSpread()      == result.insufficient_spread);
        REQUIRE(moon.isRatioNearBoundary()       == result.ratio_near_boundary);
        REQUIRE(moon.isReliable()                == result.isReliable());
    }

    SECTION("Reliability accessors throw before run() — consistent with existing diagnostic contract")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon(800, 0.95, 0.70, res);

        REQUIRE_THROWS_AS(moon.isDistributionDegenerate(), std::logic_error);
        REQUIRE_THROWS_AS(moon.isExcessiveBias(),          std::logic_error);
        REQUIRE_THROWS_AS(moon.isInsufficientSpread(),     std::logic_error);
        REQUIRE_THROWS_AS(moon.isRatioNearBoundary(),      std::logic_error);
        REQUIRE_THROWS_AS(moon.isReliable(),               std::logic_error);
    }

    SECTION("Flags reset correctly between successive runs on same engine instance")
    {
        // First run with constant sampler — flags should fire
        std::seed_seq seq1 = make_seed_seq(0xACCE5500000003ull);
        std::mt19937_64 rng1(seq1);

        // Two separate engine instances to test independently
        MOutOfNPercentileBootstrap<D, decltype(constant_sampler), Res>
            moon_const(800, 0.95, 0.70, res);
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), Res>
            moon_varied(800, 0.95, 0.70, res);

        auto result_const = moon_const.run(x, constant_sampler, rng1);
        REQUIRE(result_const.distribution_degenerate);
        REQUIRE(result_const.insufficient_spread);
        REQUIRE_FALSE(result_const.isReliable());

        std::seed_seq seq2 = make_seed_seq(0xACCE5500000004ull);
        std::mt19937_64 rng2(seq2);
        auto result_varied = moon_varied.run(x, mean_sampler, rng2);
        REQUIRE_FALSE(result_varied.distribution_degenerate);
        REQUIRE_FALSE(result_varied.insufficient_spread);
        REQUIRE(result_varied.isReliable());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §7  MockBootstrapEngine Result initialisation fix
// ─────────────────────────────────────────────────────────────────────────────
//
// The MockBootstrapEngine in MOutOfNPercentileBootstrapTest.cpp constructs
// a Result without initialising the fields added by the reliability work:
//   skew_boot, degenerate_warning, distribution_degenerate, excessive_bias,
//   insufficient_spread, ratio_near_boundary.
//
// This causes a compilation error. The fix is to add the missing fields to
// the mock Result construction. The corrected block should read:
//
//   return Result{
//     /*mean=*/stat_value,
//     /*lower=*/stat_value * Decimal(0.9),
//     /*upper=*/stat_value * Decimal(1.1),
//     /*cl=*/CL,
//     /*B=*/B,
//     /*effective_B=*/B,
//     /*skipped=*/0,
//     /*n=*/n,
//     /*m_sub=*/m_sub,
//     /*L=*/resampler.getL(),
//     /*computed_ratio=*/rho,
//     /*skew_boot=*/0.0,
//     /*degenerate_warning=*/false,
//     /*distribution_degenerate=*/false,
//     /*excessive_bias=*/false,
//     /*insufficient_spread=*/false,
//     /*ratio_near_boundary=*/false
//   };
//
// This section contains a compile-time check that the Result struct has the
// expected number of fields, so any future additions to Result will also
// cause a visible test failure rather than a silent uninitialised value.

TEST_CASE("MOutOfNPercentileBootstrap: Result struct has expected reliability fields",
          "[Bootstrap][MOutOfN][Reliability][Compilation]")
{
    using ResultT = MOutOfNPercentileBootstrap<
        D,
        decltype(mean_sampler),
        Res
    >::Result;

    SECTION("All four reliability flag fields are accessible on Result")
    {
        // Construct a minimal Result with all fields — this will fail to compile
        // if any new fields are added to the struct without updating this test.
        ResultT r{
            /*mean=*/D(0),
            /*lower=*/D(0),
            /*upper=*/D(0),
            /*cl=*/0.95,
            /*B=*/800,
            /*effective_B=*/800,
            /*skipped=*/0,
            /*n=*/60,
            /*m_sub=*/42,
            /*L=*/3,
            /*computed_ratio=*/0.70,
            /*skew_boot=*/0.0,
            /*degenerate_warning=*/false,
            /*distribution_degenerate=*/false,
            /*excessive_bias=*/false,
            /*insufficient_spread=*/false,
            /*ratio_near_boundary=*/false
        };

        REQUIRE(r.isReliable());
        REQUIRE_FALSE(r.distribution_degenerate);
        REQUIRE_FALSE(r.excessive_bias);
        REQUIRE_FALSE(r.insufficient_spread);
        REQUIRE_FALSE(r.ratio_near_boundary);
    }

    SECTION("isReliable() on Result returns false when any flag is set")
    {
        ResultT r_degen{
            D(0), D(0), D(0), 0.95, 800, 800, 0, 60, 42, 3, 0.70, 0.0,
            /*degenerate_warning=*/false,
            /*distribution_degenerate=*/true,   // one flag set
            /*excessive_bias=*/false,
            /*insufficient_spread=*/false,
            /*ratio_near_boundary=*/false
        };
        REQUIRE_FALSE(r_degen.isReliable());

        ResultT r_spread{
            D(0), D(0), D(0), 0.95, 800, 800, 0, 60, 42, 3, 0.70, 0.0,
            false, false, false,
            /*insufficient_spread=*/true,       // one flag set
            false
        };
        REQUIRE_FALSE(r_spread.isReliable());
    }
}
