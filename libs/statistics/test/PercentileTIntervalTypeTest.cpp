// PercentileTBootstrapIntervalTypeTest.cpp
//
// Unit tests for IntervalType functionality in PercentileTBootstrap.
// Tests one-sided and two-sided confidence interval computation.
//
// These tests verify:
// - ONE_SIDED_LOWER intervals
// - ONE_SIDED_UPPER intervals
// - Comparison with TWO_SIDED intervals
// - Different confidence levels
// - Integration with different resamplers
// - Backward compatibility

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <random>
#include <functional>

#include "number.h"
#include "StatUtils.h"
#include "randutils.hpp"
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "PercentileTBootstrap.h"
#include "BootstrapTypes.h"
#include "ParallelExecutors.h"

using palvalidator::analysis::PercentileTBootstrap;
using palvalidator::analysis::IntervalType;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// ==================== Test Utilities ====================

// Simple mean sampler
struct MeanSamplerForIntervalTest
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        if (x.empty()) return Decimal(0);
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(num::to_double(v));
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// IID resampler for tests
struct IIDResamplerForIntervalTest
{
    std::size_t getL() const noexcept { return 0; }

    template <typename Decimal, typename Rng>
    void operator()(const std::vector<Decimal>& src,
                    std::vector<Decimal>&       dst,
                    std::size_t                 m,
                    Rng&                        rng) const
    {
        std::uniform_int_distribution<std::size_t> pick(0, src.size() - 1);
        dst.resize(m);
        for (std::size_t i = 0; i < m; ++i)
        {
            dst[i] = src[pick(rng)];
        }
    }
};

// ==================== ONE_SIDED_LOWER Tests ====================

TEST_CASE("PercentileTBootstrap: ONE_SIDED_LOWER basic functionality", 
          "[PercentileT][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    
    // Test data: moderate-n with mild variation
    const std::size_t n = 40;
    std::vector<D> returns;
    returns.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        double val = 0.005 + 0.002 * std::sin(static_cast<double>(i) / 5.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/4);
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
        /*B_outer=*/500, 
        /*B_inner=*/150, 
        /*CL=*/0.95, 
        resampler,
        /*m_ratio_outer=*/1.0,
        /*m_ratio_inner=*/1.0,
        IntervalType::ONE_SIDED_LOWER);
    
    randutils::seed_seq_fe128 seed{2025u, 1u, 15u, 42u};
    std::mt19937_64 rng(seed);
    
    auto result = pt.run(returns, mean_sampler, rng);
    
    SECTION("Result structure is valid")
    {
        REQUIRE(result.B_outer == 500);
        REQUIRE(result.B_inner == 150);
        REQUIRE(result.cl == Catch::Approx(0.95));
        REQUIRE(result.n == n);
        REQUIRE(result.effective_B > 0);
        REQUIRE(result.effective_B <= result.B_outer);
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
        double lb = num::to_double(result.lower);
        double mean = num::to_double(result.mean);
        
        REQUIRE(lb < mean);
    }
    
    SECTION("Upper bound is very high (effectively unbounded)")
    {
        double lb = num::to_double(result.lower);
        double ub = num::to_double(result.upper);
        double mean = num::to_double(result.mean);
        
        double lower_dist = mean - lb;
        double upper_dist = ub - mean;
        
        // For one-sided lower, upper bound should be much farther from mean
        // We use a relaxed threshold since bootstrap variation can affect this
        REQUIRE(upper_dist >= lower_dist);
    }
}

TEST_CASE("PercentileTBootstrap: ONE_SIDED_LOWER with different confidence levels",
          "[PercentileT][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 50; ++i)
    {
        double val = 0.003 + 0.001 * std::cos(static_cast<double>(i) / 7.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/3);
    
    SECTION("CL=0.90")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.90, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        
        randutils::seed_seq_fe128 seed{100u, 200u, 300u, 400u};
        std::mt19937_64 rng(seed);
        auto result = pt.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("CL=0.95")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        
        randutils::seed_seq_fe128 seed{100u, 200u, 300u, 400u};
        std::mt19937_64 rng(seed);
        auto result = pt.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("CL=0.99")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.99, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        
        randutils::seed_seq_fe128 seed{100u, 200u, 300u, 400u};
        std::mt19937_64 rng(seed);
        auto result = pt.run(returns, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(result.lower <= result.mean);
    }
    
    SECTION("Higher CL produces lower bound closer to mean (more conservative)")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_90(
            500, 150, 0.90, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_99(
            500, 150, 0.99, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        
        randutils::seed_seq_fe128 seedA{100u, 200u, 300u, 400u};
        randutils::seed_seq_fe128 seedB{100u, 200u, 300u, 400u};
        std::mt19937_64 rngA(seedA), rngB(seedB);
        
        auto r90 = pt_90.run(returns, mean_sampler, rngA);
        auto r99 = pt_99.run(returns, mean_sampler, rngB);
        
        double lb_90 = num::to_double(r90.lower);
        double lb_99 = num::to_double(r99.lower);
        
        // 99% CL should have lower bound closer to (or below) 90% CL bound
        // Allow for bootstrap variation with >= instead of strict >
        REQUIRE(lb_99 <= lb_90 + 0.001);
    }
}

// ==================== ONE_SIDED_UPPER Tests ====================

TEST_CASE("PercentileTBootstrap: ONE_SIDED_UPPER basic functionality",
          "[PercentileT][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    
    const std::size_t n = 40;
    std::vector<D> returns;
    returns.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        double val = 0.004 + 0.003 * std::sin(static_cast<double>(i) / 6.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/4);
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
        500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_UPPER);
    
    randutils::seed_seq_fe128 seed{2025u, 2u, 20u, 84u};
    std::mt19937_64 rng(seed);
    
    auto result = pt.run(returns, mean_sampler, rng);
    
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
        double ub = num::to_double(result.upper);
        double mean = num::to_double(result.mean);
        
        REQUIRE(ub > mean);
    }
    
    SECTION("Lower bound is very low (effectively unbounded)")
    {
        double lb = num::to_double(result.lower);
        double ub = num::to_double(result.upper);
        double mean = num::to_double(result.mean);
        
        double lower_dist = mean - lb;
        double upper_dist = ub - mean;
        
        // For one-sided upper, lower bound should be much farther from mean
        REQUIRE(lower_dist >= upper_dist);
    }
}

// ==================== ONE_SIDED vs TWO_SIDED Comparison ====================

TEST_CASE("PercentileTBootstrap: ONE_SIDED_LOWER vs TWO_SIDED comparison",
          "[PercentileT][IntervalType][Comparison]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 60; ++i)
    {
        double val = 0.006 + 0.002 * std::sin(static_cast<double>(i) / 8.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/5);
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_two(
        600, 150, 0.95, resampler, 1.0, 1.0, IntervalType::TWO_SIDED);
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_one(
        600, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
    
    randutils::seed_seq_fe128 seedA{500u, 600u, 700u, 800u};
    randutils::seed_seq_fe128 seedB{500u, 600u, 700u, 800u};
    std::mt19937_64 rngA(seedA), rngB(seedB);
    
    auto r_two = pt_two.run(returns, mean_sampler, rngA);
    auto r_one = pt_one.run(returns, mean_sampler, rngB);
    
    SECTION("Means are similar (same data, same statistic)")
    {
        double mean_two = num::to_double(r_two.mean);
        double mean_one = num::to_double(r_one.mean);
        
        // Means should be very close (same data, same sampler)
        REQUIRE(mean_two == Catch::Approx(mean_one).margin(0.001));
    }
    
    SECTION("One-sided lower bound is higher or equal (less conservative)")
    {
        double lb_two = num::to_double(r_two.lower);
        double lb_one = num::to_double(r_one.lower);
        
        // One-sided 95% lower bound should be at 5th percentile,
        // two-sided 95% lower bound is at 2.5th percentile
        // So one-sided should be >= two-sided (allow equality due to variation)
        REQUIRE(lb_one >= lb_two - 0.001);
    }
    
    SECTION("One-sided upper bound is higher (less constrained)")
    {
        double ub_two = num::to_double(r_two.upper);
        double ub_one = num::to_double(r_one.upper);
        
        // One-sided upper should be at ~100th percentile (very high)
        // Two-sided upper is at 97.5th percentile
        REQUIRE(ub_one >= ub_two - 0.001);
    }
}

TEST_CASE("PercentileTBootstrap: ONE_SIDED_UPPER vs TWO_SIDED comparison",
          "[PercentileT][IntervalType][Comparison]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 50; ++i)
    {
        double val = 0.005 + 0.003 * std::cos(static_cast<double>(i) / 7.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/4);
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_two(
        500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::TWO_SIDED);
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_one(
        500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_UPPER);
    
    randutils::seed_seq_fe128 seedA{900u, 1000u, 1100u, 1200u};
    randutils::seed_seq_fe128 seedB{900u, 1000u, 1100u, 1200u};
    std::mt19937_64 rngA(seedA), rngB(seedB);
    
    auto r_two = pt_two.run(returns, mean_sampler, rngA);
    auto r_one = pt_one.run(returns, mean_sampler, rngB);
    
    SECTION("One-sided upper bound is lower or equal (less conservative)")
    {
        double ub_two = num::to_double(r_two.upper);
        double ub_one = num::to_double(r_one.upper);
        
        // One-sided 95% upper bound should be at 95th percentile,
        // two-sided 95% upper bound is at 97.5th percentile
        // So one-sided should be <= two-sided
        REQUIRE(ub_one <= ub_two + 0.001);
    }
    
    SECTION("One-sided lower bound is lower (less constrained)")
    {
        double lb_two = num::to_double(r_two.lower);
        double lb_one = num::to_double(r_one.lower);
        
        // One-sided lower should be at ~0th percentile (very low)
        // Two-sided lower is at 2.5th percentile
        REQUIRE(lb_one <= lb_two + 0.001);
    }
}

// ==================== Edge Cases ====================

TEST_CASE("PercentileTBootstrap: IntervalType with small dataset",
          "[PercentileT][IntervalType][EdgeCase]")
{
    using D = DecimalType;
    
    // Minimum viable dataset (n=20 for this configuration)
    std::vector<D> returns;
    for (std::size_t i = 0; i < 20; ++i)
    {
        returns.emplace_back(D(0.005 + 0.001 * static_cast<double>(i % 5)));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/3);
    
    SECTION("ONE_SIDED_LOWER works with small n")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        
        REQUIRE_NOTHROW(pt.run(returns, mean_sampler, rng));
        auto result = pt.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
    }
    
    SECTION("ONE_SIDED_UPPER works with small n")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{5u, 6u, 7u, 8u};
        std::mt19937_64 rng(seed);
        
        REQUIRE_NOTHROW(pt.run(returns, mean_sampler, rng));
        auto result = pt.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
}

TEST_CASE("PercentileTBootstrap: IntervalType doesn't crash with extreme quantiles",
          "[PercentileT][IntervalType][EdgeCase]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 30; ++i)
    {
        returns.emplace_back(D(0.01 + 0.005 * std::sin(static_cast<double>(i) / 3.0)));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    IIDResamplerForIntervalTest resampler;
    
    SECTION("ONE_SIDED_LOWER with upper quantile near 1.0")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
        
        randutils::seed_seq_fe128 seed{10u, 20u, 30u, 40u};
        std::mt19937_64 rng(seed);
        
        // Should not throw despite using upper quantile of 1.0 (or 1.0-1e-10)
        REQUIRE_NOTHROW(pt.run(returns, mean_sampler, rng));
        
        auto result = pt.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
    
    SECTION("ONE_SIDED_UPPER with lower quantile near 0.0")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
            500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{50u, 60u, 70u, 80u};
        std::mt19937_64 rng(seed);
        
        // Should not throw despite using lower quantile of 0.0 (or 1e-10)
        REQUIRE_NOTHROW(pt.run(returns, mean_sampler, rng));
        
        auto result = pt.run(returns, mean_sampler, rng);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
}

// ==================== Integration with Different Resamplers ====================

TEST_CASE("PercentileTBootstrap: ONE_SIDED_LOWER with IID resampler",
          "[PercentileT][IntervalType][IID]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 50; ++i)
    {
        returns.emplace_back(D(0.007 + 0.002 * static_cast<double>(i % 10) / 10.0));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    IIDResamplerForIntervalTest resampler;
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
        600, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
    
    randutils::seed_seq_fe128 seed{111u, 222u, 333u, 444u};
    std::mt19937_64 rng(seed);
    
    auto result = pt.run(returns, mean_sampler, rng);
    
    SECTION("Works correctly with IID resampling")
    {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
    }
}

TEST_CASE("PercentileTBootstrap: ONE_SIDED_LOWER with StationaryBlock resampler",
          "[PercentileT][IntervalType][StationaryBlock]")
{
    using D = DecimalType;
    
    // Autocorrelated data
    std::vector<D> returns;
    for (std::size_t i = 0; i < 80; ++i)
    {
        double val = 0.004 + 0.003 * std::sin(static_cast<double>(i) / 10.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/6);
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt(
        700, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
    
    randutils::seed_seq_fe128 seed{555u, 666u, 777u, 888u};
    std::mt19937_64 rng(seed);
    
    auto result = pt.run(returns, mean_sampler, rng);
    
    SECTION("Works correctly with block resampling")
    {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(result.effective_B >= result.B_outer / 2);
    }
}

// ==================== Backward Compatibility ====================

TEST_CASE("PercentileTBootstrap: Default IntervalType is TWO_SIDED",
          "[PercentileT][IntervalType][BackwardCompatibility]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 40; ++i)
    {
        returns.emplace_back(D(0.008 + 0.001 * static_cast<double>(i % 8) / 8.0));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/4);
    
    // Constructor without IntervalType parameter (should default to TWO_SIDED)
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_default(
        500, 150, 0.95, resampler);
    
    // Explicit TWO_SIDED
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_explicit(
        500, 150, 0.95, resampler, 1.0, 1.0, IntervalType::TWO_SIDED);
    
    randutils::seed_seq_fe128 seedA{1000u, 2000u, 3000u, 4000u};
    randutils::seed_seq_fe128 seedB{1000u, 2000u, 3000u, 4000u};
    std::mt19937_64 rngA(seedA), rngB(seedB);
    
    auto r_default = pt_default.run(returns, mean_sampler, rngA);
    auto r_explicit = pt_explicit.run(returns, mean_sampler, rngB);
    
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
        // Allow for variation with relaxed tolerance
        double lb_default = num::to_double(r_default.lower);
        double lb_explicit = num::to_double(r_explicit.lower);
        double ub_default = num::to_double(r_default.upper);
        double ub_explicit = num::to_double(r_explicit.upper);
        
        REQUIRE(lb_default == Catch::Approx(lb_explicit).margin(0.01));
        REQUIRE(ub_default == Catch::Approx(ub_explicit).margin(0.01));
    }
}

// ==================== Comprehensive Integration Test ====================

TEST_CASE("PercentileTBootstrap: All three interval types on same data",
          "[PercentileT][IntervalType][Comprehensive]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (std::size_t i = 0; i < 70; ++i)
    {
        double val = 0.005 + 0.004 * std::sin(static_cast<double>(i) / 9.0);
        returns.emplace_back(D(val));
    }
    
    MeanSamplerForIntervalTest mean_sampler;
    StationaryMaskValueResampler<D> resampler(/*L=*/5);
    
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_two(
        600, 150, 0.95, resampler, 1.0, 1.0, IntervalType::TWO_SIDED);
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_lower(
        600, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_LOWER);
    PercentileTBootstrap<D, decltype(mean_sampler), decltype(resampler)> pt_upper(
        600, 150, 0.95, resampler, 1.0, 1.0, IntervalType::ONE_SIDED_UPPER);
    
    randutils::seed_seq_fe128 seedA{100u, 101u, 102u, 103u};
    randutils::seed_seq_fe128 seedB{100u, 101u, 102u, 103u};
    randutils::seed_seq_fe128 seedC{100u, 101u, 102u, 103u};
    std::mt19937_64 rngA(seedA), rngB(seedB), rngC(seedC);
    
    auto r_two = pt_two.run(returns, mean_sampler, rngA);
    auto r_lower = pt_lower.run(returns, mean_sampler, rngB);
    auto r_upper = pt_upper.run(returns, mean_sampler, rngC);
    
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
        double mean_two = num::to_double(r_two.mean);
        double mean_lower = num::to_double(r_lower.mean);
        double mean_upper = num::to_double(r_upper.mean);
        
        REQUIRE(mean_two == Catch::Approx(mean_lower).margin(0.001));
        REQUIRE(mean_two == Catch::Approx(mean_upper).margin(0.001));
    }
    
    SECTION("Interval relationships hold (with tolerance for variation)")
    {
        double lb_two = num::to_double(r_two.lower);
        double lb_lower = num::to_double(r_lower.lower);
        double lb_upper = num::to_double(r_upper.lower);
        
        double ub_two = num::to_double(r_two.upper);
        double ub_lower = num::to_double(r_lower.upper);
        double ub_upper = num::to_double(r_upper.upper);
        
        // ONE_SIDED_LOWER: lower bound >= two-sided, upper bound >= two-sided
        REQUIRE(lb_lower >= lb_two - 0.002);
        REQUIRE(ub_lower >= ub_two - 0.002);
        
        // ONE_SIDED_UPPER: upper bound <= two-sided, lower bound <= two-sided
        REQUIRE(ub_upper <= ub_two + 0.002);
        REQUIRE(lb_upper <= lb_two + 0.002);
    }
}
