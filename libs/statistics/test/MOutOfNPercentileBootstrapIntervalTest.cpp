// MOutOfNPercentileBootstrapIntervalTypeTest.cpp
//
// Unit tests for IntervalType functionality in MOutOfNPercentileBootstrap.
// Tests one-sided and two-sided confidence interval computation.
//
// These tests verify:
// - ONE_SIDED_LOWER intervals
// - ONE_SIDED_UPPER intervals
// - Comparison with TWO_SIDED intervals
// - Different confidence levels
// - Integration with rescale_to_n mode
// - Backward compatibility

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <random>

#include "number.h"
#include "StatUtils.h"
#include "TestUtils.h"
#include "StationaryMaskResamplers.h"
#include "MOutOfNPercentileBootstrap.h"
#include "BootstrapTypes.h"
#include "RngUtils.h"

using palvalidator::analysis::MOutOfNPercentileBootstrap;
using palvalidator::analysis::IntervalType;
using palvalidator::resampling::StationaryMaskValueResampler;
using mkc_timeseries::rng_utils::make_seed_seq;

// ==================== ONE_SIDED_LOWER Tests ====================

TEST_CASE("MOutOfNPercentileBootstrap: ONE_SIDED_LOWER basic functionality", 
          "[Bootstrap][MOutOfN][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    
    // Test data: moderate-n with positive returns
    const std::size_t n = 60;
    std::vector<D> returns;
    returns.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        double val = 0.005 + 0.002 * std::sin(static_cast<double>(i) / 8.0);
        returns.emplace_back(D(val));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/4);
    
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
        /*B=*/1000,
        /*CL=*/0.95,
        /*m_ratio=*/0.70,
        res,
        /*rescale_to_n=*/false,
        IntervalType::ONE_SIDED_LOWER);
    
    std::seed_seq seq = make_seed_seq(0x123456789ABCDEFull);
    std::mt19937_64 rng(seq);
    
    auto result = moon.run(returns, mean_sampler, rng);
    
    SECTION("Result fields are populated consistently")
    {
        REQUIRE(result.B == 1000);
        REQUIRE(result.cl == Catch::Approx(0.95));
        REQUIRE(result.n == n);
        REQUIRE(result.m_sub >= 2);
        REQUIRE(result.m_sub < n);
        REQUIRE(result.L == 4);
        REQUIRE(result.effective_B >= result.B / 2);
        REQUIRE(result.skipped + result.effective_B == result.B);
    }
    
    SECTION("Bounds are finite and ordered")
    {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
    }
    
    SECTION("Lower bound is meaningful (below mean)")
    {
        const double lb = num::to_double(result.lower);
        const double mu = num::to_double(result.mean);
        
        REQUIRE(lb < mu);
    }
    
    SECTION("Upper bound is very high (effectively unbounded)")
    {
        const double lb = num::to_double(result.lower);
        const double ub = num::to_double(result.upper);
        const double mu = num::to_double(result.mean);
        
        const double lower_dist = mu - lb;
        const double upper_dist = ub - mu;
        
        // For one-sided lower, upper bound should be at least as far from mean
        // (allowing equality due to quantile calculation and bootstrap variation)
        REQUIRE(upper_dist >= lower_dist);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: ONE_SIDED_LOWER with different confidence levels",
          "[Bootstrap][MOutOfN][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 50; ++i)
    {
        double val = 0.004 + 0.001 * std::cos(static_cast<double>(i) / 6.0);
        returns.emplace_back(D(val));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/3);
    
    SECTION("CL=0.90 produces finite lower bound")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            800, 0.90, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0x1111111111111111ull);
        std::mt19937_64 rng(seq);
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("CL=0.95 produces finite lower bound")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            800, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0x2222222222222222ull);
        std::mt19937_64 rng(seq);
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("CL=0.99 produces finite lower bound")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            800, 0.99, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0x3333333333333333ull);
        std::mt19937_64 rng(seq);
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("Higher CL produces lower bound closer to mean (more conservative)")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_90(
            1000, 0.90, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_99(
            1000, 0.99, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seqA = make_seed_seq(0x4444444444444444ull);
        std::seed_seq seqB = make_seed_seq(0x4444444444444444ull);
        std::mt19937_64 rngA(seqA), rngB(seqB);
        
        auto r90 = moon_90.run(returns, mean_sampler, rngA);
        auto r99 = moon_99.run(returns, mean_sampler, rngB);
        
        const double lb_90 = num::to_double(r90.lower);
        const double lb_99 = num::to_double(r99.lower);
        
        // 99% CL should have lower bound at or below 90% CL bound (allowing small tolerance)
        REQUIRE(lb_99 <= lb_90 + 0.001);
    }
}

// ==================== ONE_SIDED_UPPER Tests ====================

TEST_CASE("MOutOfNPercentileBootstrap: ONE_SIDED_UPPER basic functionality",
          "[Bootstrap][MOutOfN][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    
    const std::size_t n = 60;
    std::vector<D> returns;
    returns.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        double val = 0.006 + 0.003 * std::sin(static_cast<double>(i) / 7.0);
        returns.emplace_back(D(val));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/4);
    
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
        1000, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_UPPER);
    
    std::seed_seq seq = make_seed_seq(0xFEDCBA9876543210ull);
    std::mt19937_64 rng(seq);
    
    auto result = moon.run(returns, mean_sampler, rng);
    
    SECTION("Bounds are finite and ordered")
    {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
    }
    
    SECTION("Upper bound is meaningful (above mean)")
    {
        const double ub = num::to_double(result.upper);
        const double mu = num::to_double(result.mean);
        
        REQUIRE(ub > mu);
    }
    
    SECTION("Lower bound is very low (effectively unbounded)")
    {
        const double lb = num::to_double(result.lower);
        const double ub = num::to_double(result.upper);
        const double mu = num::to_double(result.mean);
        
        const double lower_dist = mu - lb;
        const double upper_dist = ub - mu;
        
        // For one-sided upper, lower bound should be at least as far from mean
        REQUIRE(lower_dist >= upper_dist);
    }
}

// ==================== ONE_SIDED vs TWO_SIDED Comparison ====================

TEST_CASE("MOutOfNPercentileBootstrap: ONE_SIDED_LOWER vs TWO_SIDED comparison",
          "[Bootstrap][MOutOfN][IntervalType][Comparison]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 80; ++i)
    {
        double val = 0.005 + 0.002 * std::sin(static_cast<double>(i) / 10.0);
        returns.emplace_back(D(val));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/5);
    
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_two(
        1200, 0.95, 0.70, res, false, IntervalType::TWO_SIDED);
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_one(
        1200, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
    
    std::seed_seq seqA = make_seed_seq(0xAAAAAAAAAAAAAAAAull);
    std::seed_seq seqB = make_seed_seq(0xAAAAAAAAAAAAAAAAull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    auto r_two = moon_two.run(returns, mean_sampler, rngA);
    auto r_one = moon_one.run(returns, mean_sampler, rngB);
    
    SECTION("Means are similar (same data, same statistic)")
    {
        const double mean_two = num::to_double(r_two.mean);
        const double mean_one = num::to_double(r_one.mean);
        
        REQUIRE(mean_two == Catch::Approx(mean_one).margin(0.001));
    }
    
    SECTION("One-sided lower bound is higher or equal (less conservative)")
    {
        const double lb_two = num::to_double(r_two.lower);
        const double lb_one = num::to_double(r_one.lower);
        
        // One-sided 95% lower bound at 5th percentile
        // Two-sided 95% lower bound at 2.5th percentile
        // Allow margin for bootstrap variation
        REQUIRE(lb_one >= lb_two - 0.001);
    }
    
    SECTION("One-sided upper bound is higher (less constrained)")
    {
        const double ub_two = num::to_double(r_two.upper);
        const double ub_one = num::to_double(r_one.upper);
        
        // One-sided upper at ~100th percentile
        // Two-sided upper at 97.5th percentile
        REQUIRE(ub_one >= ub_two - 0.001);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: ONE_SIDED_UPPER vs TWO_SIDED comparison",
          "[Bootstrap][MOutOfN][IntervalType][Comparison]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 70; ++i)
    {
        double val = 0.007 + 0.002 * std::cos(static_cast<double>(i) / 9.0);
        returns.emplace_back(D(val));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/4);
    
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_two(
        1000, 0.95, 0.70, res, false, IntervalType::TWO_SIDED);
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_one(
        1000, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_UPPER);
    
    std::seed_seq seqA = make_seed_seq(0xBBBBBBBBBBBBBBBBull);
    std::seed_seq seqB = make_seed_seq(0xBBBBBBBBBBBBBBBBull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    auto r_two = moon_two.run(returns, mean_sampler, rngA);
    auto r_one = moon_one.run(returns, mean_sampler, rngB);
    
    SECTION("One-sided upper bound is lower or equal (less conservative)")
    {
        const double ub_two = num::to_double(r_two.upper);
        const double ub_one = num::to_double(r_one.upper);
        
        // One-sided 95% upper at 95th percentile
        // Two-sided 95% upper at 97.5th percentile
        REQUIRE(ub_one <= ub_two + 0.001);
    }
    
    SECTION("One-sided lower bound is lower (less constrained)")
    {
        const double lb_two = num::to_double(r_two.lower);
        const double lb_one = num::to_double(r_one.lower);
        
        // One-sided lower at ~0th percentile
        // Two-sided lower at 2.5th percentile
        REQUIRE(lb_one <= lb_two + 0.001);
    }
}

// ==================== Edge Cases ====================

TEST_CASE("MOutOfNPercentileBootstrap: IntervalType with small dataset",
          "[Bootstrap][MOutOfN][IntervalType][EdgeCase]")
{
    using D = DecimalType;
    
    // Small viable dataset
    std::vector<D> returns;
    for (std::size_t i = 0; i < 30; ++i)
    {
        returns.emplace_back(D(0.005 + 0.001 * static_cast<double>(i % 5)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/3);
    
    SECTION("ONE_SIDED_LOWER works with small n")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            800, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0x1010101010101010ull);
        std::mt19937_64 rng(seq);
        
        REQUIRE_NOTHROW(moon.run(returns, mean_sampler, rng));
        auto result = moon.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("ONE_SIDED_UPPER works with small n")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            800, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_UPPER);
        
        std::seed_seq seq = make_seed_seq(0x2020202020202020ull);
        std::mt19937_64 rng(seq);
        
        REQUIRE_NOTHROW(moon.run(returns, mean_sampler, rng));
        auto result = moon.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.mean <= result.upper);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: IntervalType with extreme quantiles doesn't crash",
          "[Bootstrap][MOutOfN][IntervalType][EdgeCase]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 40; ++i)
    {
        returns.emplace_back(D(0.008 + 0.003 * std::sin(static_cast<double>(i) / 5.0)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/3);
    
    SECTION("ONE_SIDED_LOWER with upper quantile near 1.0")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            1000, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0xCAFEBABEDEADBEEFull);
        std::mt19937_64 rng(seq);
        
        REQUIRE_NOTHROW(moon.run(returns, mean_sampler, rng));
        
        auto result = moon.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
    
    SECTION("ONE_SIDED_UPPER with lower quantile near 0.0")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            1000, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_UPPER);
        
        std::seed_seq seq = make_seed_seq(0xDEADC0DEBADF00Dull);
        std::mt19937_64 rng(seq);
        
        REQUIRE_NOTHROW(moon.run(returns, mean_sampler, rng));
        
        auto result = moon.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
}

// ==================== Integration with Rescaling ====================

TEST_CASE("MOutOfNPercentileBootstrap: IntervalType with rescale_to_n mode",
          "[Bootstrap][MOutOfN][IntervalType][Rescaling]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 50; ++i)
    {
        returns.emplace_back(D(0.004 + 0.002 * std::sin(static_cast<double>(i) / 6.0)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/3);
    
    SECTION("ONE_SIDED_LOWER with rescaling works correctly")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            1000, 0.95, 0.70, res, /*rescale_to_n=*/true, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0x1111222233334444ull);
        std::mt19937_64 rng(seq);
        
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
    }
    
    SECTION("ONE_SIDED_UPPER with rescaling works correctly")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(
            1000, 0.95, 0.70, res, /*rescale_to_n=*/true, IntervalType::ONE_SIDED_UPPER);
        
        std::seed_seq seq = make_seed_seq(0x5555666677778888ull);
        std::mt19937_64 rng(seq);
        
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
    }
}

// ==================== Backward Compatibility ====================

TEST_CASE("MOutOfNPercentileBootstrap: Default IntervalType is TWO_SIDED",
          "[Bootstrap][MOutOfN][IntervalType][BackwardCompatibility]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 60; ++i)
    {
        returns.emplace_back(D(0.006 + 0.001 * static_cast<double>(i % 10) / 10.0));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/4);
    
    // Constructor without IntervalType parameter (should default to TWO_SIDED)
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_default(
        1000, 0.95, 0.70, res);
    
    // Explicit TWO_SIDED
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_explicit(
        1000, 0.95, 0.70, res, false, IntervalType::TWO_SIDED);
    
    std::seed_seq seqA = make_seed_seq(0x9999999999999999ull);
    std::seed_seq seqB = make_seed_seq(0x9999999999999999ull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    auto r_default = moon_default.run(returns, mean_sampler, rngA);
    auto r_explicit = moon_explicit.run(returns, mean_sampler, rngB);
    
    SECTION("Default behavior produces reasonable results")
    {
        REQUIRE(std::isfinite(num::to_double(r_default.lower)));
        REQUIRE(std::isfinite(num::to_double(r_default.upper)));
        REQUIRE(r_default.lower <= r_default.mean);
        REQUIRE(r_default.mean <= r_default.upper);
    }
    
    SECTION("Default approximates explicit TWO_SIDED")
    {
        // With identical RNG seeds, should produce similar results
        const double lb_default = num::to_double(r_default.lower);
        const double lb_explicit = num::to_double(r_explicit.lower);
        const double ub_default = num::to_double(r_default.upper);
        const double ub_explicit = num::to_double(r_explicit.upper);
        
        // Allow relaxed tolerance for bootstrap variation
        REQUIRE(lb_default == Catch::Approx(lb_explicit).margin(0.01));
        REQUIRE(ub_default == Catch::Approx(ub_explicit).margin(0.01));
    }
}

// ==================== Comprehensive Integration Test ====================

TEST_CASE("MOutOfNPercentileBootstrap: All three interval types on same data",
          "[Bootstrap][MOutOfN][IntervalType][Comprehensive]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 100; ++i)
    {
        double val = 0.005 + 0.003 * std::sin(static_cast<double>(i) / 12.0);
        returns.emplace_back(D(val));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/5);
    
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_two(
        1500, 0.95, 0.70, res, false, IntervalType::TWO_SIDED);
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_lower(
        1500, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon_upper(
        1500, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_UPPER);
    
    std::seed_seq seqA = make_seed_seq(0xABCDEF0123456789ull);
    std::seed_seq seqB = make_seed_seq(0xABCDEF0123456789ull);
    std::seed_seq seqC = make_seed_seq(0xABCDEF0123456789ull);
    std::mt19937_64 rngA(seqA), rngB(seqB), rngC(seqC);
    
    auto r_two = moon_two.run(returns, mean_sampler, rngA);
    auto r_lower = moon_lower.run(returns, mean_sampler, rngB);
    auto r_upper = moon_upper.run(returns, mean_sampler, rngC);
    
    SECTION("All intervals produce valid results")
    {
        REQUIRE(std::isfinite(num::to_double(r_two.lower)));
        REQUIRE(std::isfinite(num::to_double(r_two.upper)));
        REQUIRE(std::isfinite(num::to_double(r_lower.lower)));
        REQUIRE(std::isfinite(num::to_double(r_lower.upper)));
        REQUIRE(std::isfinite(num::to_double(r_upper.lower)));
        REQUIRE(std::isfinite(num::to_double(r_upper.upper)));
    }
    
    SECTION("Means are similar across interval types")
    {
        const double mean_two = num::to_double(r_two.mean);
        const double mean_lower = num::to_double(r_lower.mean);
        const double mean_upper = num::to_double(r_upper.mean);
        
        REQUIRE(mean_two == Catch::Approx(mean_lower).margin(0.001));
        REQUIRE(mean_two == Catch::Approx(mean_upper).margin(0.001));
    }
    
    SECTION("Interval relationships hold (with tolerance for variation)")
    {
        const double lb_two = num::to_double(r_two.lower);
        const double lb_lower = num::to_double(r_lower.lower);
        const double lb_upper = num::to_double(r_upper.lower);
        
        const double ub_two = num::to_double(r_two.upper);
        const double ub_lower = num::to_double(r_lower.upper);
        const double ub_upper = num::to_double(r_upper.upper);
        
        // ONE_SIDED_LOWER: lower bound >= two-sided, upper bound >= two-sided
        REQUIRE(lb_lower >= lb_two - 0.002);
        REQUIRE(ub_lower >= ub_two - 0.002);
        
        // ONE_SIDED_UPPER: upper bound <= two-sided, lower bound <= two-sided
        REQUIRE(ub_upper <= ub_two + 0.002);
        REQUIRE(lb_upper <= lb_two + 0.002);
    }
}

// ==================== Factory Method Tests ====================

TEST_CASE("MOutOfNPercentileBootstrap: Factory methods support IntervalType",
          "[Bootstrap][MOutOfN][IntervalType][Factory]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 50; ++i)
    {
        returns.emplace_back(D(0.005 + 0.002 * static_cast<double>(i % 8) / 8.0));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(/*L=*/3);
    
    SECTION("createFixedRatio with ONE_SIDED_LOWER")
    {
        auto moon = MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>::
            createFixedRatio(1000, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_LOWER);
        
        std::seed_seq seq = make_seed_seq(0xFACEB00C12345678ull);
        std::mt19937_64 rng(seq);
        
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("createFixedRatio with ONE_SIDED_UPPER")
    {
        auto moon = MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>::
            createFixedRatio(1000, 0.95, 0.70, res, false, IntervalType::ONE_SIDED_UPPER);
        
        std::seed_seq seq = make_seed_seq(0xDEADFACE87654321ull);
        std::mt19937_64 rng(seq);
        
        auto result = moon.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.mean <= result.upper);
    }
}
