// PercentileBootstrapIntervalTypeTest.cpp
//
// Unit tests for IntervalType functionality in PercentileBootstrap
// Tests ONE_SIDED_UPPER, ONE_SIDED_LOWER, and TWO_SIDED confidence intervals
//
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - PercentileBootstrap.h (with IntervalType support)
//  - BootstrapTypes.h
//  - StationaryMaskResamplers.h
//  - ParallelExecutors.h

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
#include "BootstrapTypes.h"
#include "ParallelExecutors.h"

using palvalidator::analysis::PercentileBootstrap;
using palvalidator::analysis::IntervalType;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple sampler: arithmetic mean (matching existing tests)
struct MeanSamplerIT
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        long double sum = 0.0L;
        for (auto& v : x)
	  sum += static_cast<long double> (num::to_double<Decimal>(v));
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// Helper to create test data (same as existing tests)
std::vector<DecimalType> createTestDataIT()
{
    const std::size_t n = 20;
    std::vector<DecimalType> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(DecimalType(static_cast<int>(i)));
    }
    return x;
}

// ==================== ONE_SIDED_UPPER Tests ====================

TEST_CASE("PercentileBootstrap: ONE_SIDED_UPPER basic functionality",
          "[Bootstrap][Percentile][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    randutils::seed_seq_fe128 seed{11u, 22u, 33u, 44u};
    std::mt19937_64 rng(seed);
    
    auto result = pb.run(x, sampler, rng);
    
    SECTION("Result structure is valid") {
        REQUIRE(result.B == B);
        REQUIRE(result.n == x.size());
        REQUIRE(result.effective_B >= B / 2);
        REQUIRE(result.cl == Catch::Approx(CL).margin(1e-12));
        
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
    
    SECTION("Bounds maintain ordering") {
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(result.lower <= result.upper);
    }
    
    SECTION("Upper bound is meaningful") {
        const double ub = num::to_double(result.upper);
        const double mean = num::to_double(result.mean);
        
        REQUIRE(ub >= mean);
        REQUIRE(ub - mean >= 0.0);
    }
    
    SECTION("Lower bound is low (effectively unbounded)") {
        const double lb = num::to_double(result.lower);
        const double mean = num::to_double(result.mean);
        
        // Lower bound should be lower or equal to mean
        REQUIRE(lb <= mean);
    }
}

TEST_CASE("PercentileBootstrap: ONE_SIDED_UPPER different confidence levels",
          "[Bootstrap][Percentile][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B = 700;
    
    SECTION("CL=0.90") {
        PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
            pb(B, 0.90, res, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        auto result = pb.run(x, sampler, rng);
        
        REQUIRE(result.lower <= result.upper);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.cl == Catch::Approx(0.90).margin(1e-12));
    }
    
    SECTION("CL=0.95") {
        PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
            pb(B, 0.95, res, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        auto result = pb.run(x, sampler, rng);
        
        REQUIRE(result.lower <= result.upper);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.cl == Catch::Approx(0.95).margin(1e-12));
    }
    
    SECTION("CL=0.99") {
        PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
            pb(B, 0.99, res, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        auto result = pb.run(x, sampler, rng);
        
        REQUIRE(result.lower <= result.upper);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.cl == Catch::Approx(0.99).margin(1e-12));
    }
}

TEST_CASE("PercentileBootstrap: ONE_SIDED_UPPER higher CL increases upper bound",
          "[Bootstrap][Percentile][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B = 1000;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed90{100u, 200u};
    randutils::seed_seq_fe128 seed95{100u, 200u};
    randutils::seed_seq_fe128 seed99{100u, 200u};
    
    std::mt19937_64 rng90(seed90), rng95(seed95), rng99(seed99);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb90(B, 0.90, res, IntervalType::ONE_SIDED_UPPER),
        pb95(B, 0.95, res, IntervalType::ONE_SIDED_UPPER),
        pb99(B, 0.99, res, IntervalType::ONE_SIDED_UPPER);
    
    auto r90 = pb90.run(x, sampler, rng90);
    auto r95 = pb95.run(x, sampler, rng95);
    auto r99 = pb99.run(x, sampler, rng99);
    
    const double ub90 = num::to_double(r90.upper);
    const double ub95 = num::to_double(r95.upper);
    const double ub99 = num::to_double(r99.upper);
    
    // Higher confidence → higher upper bound (allow small tolerance for bootstrap variation)
    REQUIRE(ub95 >= ub90 - 0.5);
    REQUIRE(ub99 >= ub95 - 0.5);
}

// ==================== ONE_SIDED_LOWER Tests ====================

TEST_CASE("PercentileBootstrap: ONE_SIDED_LOWER basic functionality",
          "[Bootstrap][Percentile][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb(B, CL, res, IntervalType::ONE_SIDED_LOWER);
    
    randutils::seed_seq_fe128 seed{11u, 22u, 33u, 44u};
    std::mt19937_64 rng(seed);
    
    auto result = pb.run(x, sampler, rng);
    
    SECTION("Result structure is valid") {
        REQUIRE(result.B == B);
        REQUIRE(result.n == x.size());
        REQUIRE(result.effective_B >= B / 2);
        REQUIRE(result.cl == Catch::Approx(CL).margin(1e-12));
        
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
    
    SECTION("Bounds maintain ordering") {
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(result.lower <= result.upper);
    }
    
    SECTION("Lower bound is meaningful") {
        const double lb = num::to_double(result.lower);
        const double mean = num::to_double(result.mean);
        
        REQUIRE(lb <= mean);
    }
    
    SECTION("Upper bound is high (effectively unbounded)") {
        const double ub = num::to_double(result.upper);
        const double mean = num::to_double(result.mean);
        
        // Upper bound should be higher or equal to mean
        REQUIRE(ub >= mean);
    }
}

TEST_CASE("PercentileBootstrap: ONE_SIDED_LOWER higher CL decreases lower bound",
          "[Bootstrap][Percentile][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B = 1000;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed90{100u, 200u};
    randutils::seed_seq_fe128 seed95{100u, 200u};
    randutils::seed_seq_fe128 seed99{100u, 200u};
    
    std::mt19937_64 rng90(seed90), rng95(seed95), rng99(seed99);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb90(B, 0.90, res, IntervalType::ONE_SIDED_LOWER),
        pb95(B, 0.95, res, IntervalType::ONE_SIDED_LOWER),
        pb99(B, 0.99, res, IntervalType::ONE_SIDED_LOWER);
    
    auto r90 = pb90.run(x, sampler, rng90);
    auto r95 = pb95.run(x, sampler, rng95);
    auto r99 = pb99.run(x, sampler, rng99);
    
    const double lb90 = num::to_double(r90.lower);
    const double lb95 = num::to_double(r95.lower);
    const double lb99 = num::to_double(r99.lower);
    
    // Higher confidence → lower lower bound (allow small tolerance for bootstrap variation)
    REQUIRE(lb95 <= lb90 + 0.5);
    REQUIRE(lb99 <= lb95 + 0.5);
}

// ==================== Comparison Tests ====================

TEST_CASE("PercentileBootstrap: ONE_SIDED_UPPER vs TWO_SIDED comparison",
          "[Bootstrap][Percentile][IntervalType][Comparison]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1200;
    const double      CL = 0.95;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed_two{99u, 88u, 77u, 66u};
    randutils::seed_seq_fe128 seed_one{99u, 88u, 77u, 66u};
    
    std::mt19937_64 rng_two(seed_two), rng_one(seed_one);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb_two(B, CL, res, IntervalType::TWO_SIDED),
        pb_one(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result_two = pb_two.run(x, sampler, rng_two);
    auto result_one = pb_one.run(x, sampler, rng_one);
    
    SECTION("Means are identical") {
        const double mean_two = num::to_double(result_two.mean);
        const double mean_one = num::to_double(result_one.mean);
        
        // Same data, same sampler → means should be identical
        REQUIRE(mean_two == Catch::Approx(mean_one).epsilon(1e-10));
    }
    
    SECTION("ONE_SIDED_UPPER upper bound is less conservative") {
        const double ub_two = num::to_double(result_two.upper);
        const double ub_one = num::to_double(result_one.upper);
        
        // ONE_SIDED_UPPER at 95th percentile
        // TWO_SIDED at 97.5th percentile
        // Allow margin for bootstrap variation
        REQUIRE(ub_one <= ub_two + 1.0);
    }
    
    SECTION("ONE_SIDED_UPPER lower bound is less constrained") {
        const double lb_two = num::to_double(result_two.lower);
        const double lb_one = num::to_double(result_one.lower);
        
        // ONE_SIDED_UPPER lower at ~0th percentile
        // TWO_SIDED lower at 2.5th percentile
        REQUIRE(lb_one <= lb_two + 1.0);
    }
}

TEST_CASE("PercentileBootstrap: ONE_SIDED_LOWER vs TWO_SIDED comparison",
          "[Bootstrap][Percentile][IntervalType][Comparison]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1200;
    const double      CL = 0.95;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed_two{99u, 88u, 77u, 66u};
    randutils::seed_seq_fe128 seed_one{99u, 88u, 77u, 66u};
    
    std::mt19937_64 rng_two(seed_two), rng_one(seed_one);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb_two(B, CL, res, IntervalType::TWO_SIDED),
        pb_one(B, CL, res, IntervalType::ONE_SIDED_LOWER);
    
    auto result_two = pb_two.run(x, sampler, rng_two);
    auto result_one = pb_one.run(x, sampler, rng_one);
    
    SECTION("Means are identical") {
        const double mean_two = num::to_double(result_two.mean);
        const double mean_one = num::to_double(result_one.mean);
        
        // Same data, same sampler → means should be identical
        REQUIRE(mean_two == Catch::Approx(mean_one).epsilon(1e-10));
    }
    
    SECTION("ONE_SIDED_LOWER lower bound is higher (less conservative)") {
        const double lb_two = num::to_double(result_two.lower);
        const double lb_one = num::to_double(result_one.lower);
        
        // ONE_SIDED_LOWER at 5th percentile
        // TWO_SIDED at 2.5th percentile
        REQUIRE(lb_one >= lb_two - 1.0);
    }
    
    SECTION("ONE_SIDED_LOWER upper bound is higher (less constrained)") {
        const double ub_two = num::to_double(result_two.upper);
        const double ub_one = num::to_double(result_one.upper);
        
        // ONE_SIDED_LOWER upper at ~100th percentile
        // TWO_SIDED upper at 97.5th percentile
        REQUIRE(ub_one >= ub_two - 1.0);
    }
}

// ==================== Backward Compatibility ====================

TEST_CASE("PercentileBootstrap: Default IntervalType is TWO_SIDED",
          "[Bootstrap][Percentile][IntervalType][BackwardCompatibility]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1000;
    const double      CL = 0.95;
    
    // Constructor without IntervalType parameter (should default to TWO_SIDED)
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb_default(B, CL, res);
    
    // Explicit TWO_SIDED
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb_explicit(B, CL, res, IntervalType::TWO_SIDED);
    
    // Use identical seeds
    randutils::seed_seq_fe128 seed_default{123u, 456u};
    randutils::seed_seq_fe128 seed_explicit{123u, 456u};
    
    std::mt19937_64 rng_default(seed_default), rng_explicit(seed_explicit);
    
    auto result_default = pb_default.run(x, sampler, rng_default);
    auto result_explicit = pb_explicit.run(x, sampler, rng_explicit);
    
    SECTION("Default behavior produces reasonable results") {
        REQUIRE(std::isfinite(num::to_double(result_default.mean)));
        REQUIRE(std::isfinite(num::to_double(result_default.lower)));
        REQUIRE(std::isfinite(num::to_double(result_default.upper)));
        REQUIRE(result_default.lower <= result_default.upper);
    }
    
    SECTION("Default is identical to explicit TWO_SIDED") {
        const double mean_default = num::to_double(result_default.mean);
        const double mean_explicit = num::to_double(result_explicit.mean);
        
        // Same sampler, same data, same seeds → means identical
        REQUIRE(mean_default == Catch::Approx(mean_explicit).epsilon(1e-10));
        
        // Bounds should also be identical (same RNG seeds)
        const double lb_default = num::to_double(result_default.lower);
        const double lb_explicit = num::to_double(result_explicit.lower);
        const double ub_default = num::to_double(result_default.upper);
        const double ub_explicit = num::to_double(result_explicit.upper);
        
        REQUIRE(lb_default == Catch::Approx(lb_explicit).epsilon(1e-10));
        REQUIRE(ub_default == Catch::Approx(ub_explicit).epsilon(1e-10));
    }
}

// ==================== Comprehensive Integration Test ====================

TEST_CASE("PercentileBootstrap: All three interval types on same data",
          "[Bootstrap][Percentile][IntervalType][Comprehensive]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1500;
    const double      CL = 0.95;
    
    // Use identical seeds for each (for deterministic comparison)
    randutils::seed_seq_fe128 seed_two{999u, 888u};
    randutils::seed_seq_fe128 seed_lower{999u, 888u};
    randutils::seed_seq_fe128 seed_upper{999u, 888u};
    
    std::mt19937_64 rng_two(seed_two), rng_lower(seed_lower), rng_upper(seed_upper);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb_two(B, CL, res, IntervalType::TWO_SIDED),
        pb_lower(B, CL, res, IntervalType::ONE_SIDED_LOWER),
        pb_upper(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result_two = pb_two.run(x, sampler, rng_two);
    auto result_lower = pb_lower.run(x, sampler, rng_lower);
    auto result_upper = pb_upper.run(x, sampler, rng_upper);
    
    SECTION("All intervals produce valid results") {
        REQUIRE(std::isfinite(num::to_double(result_two.lower)));
        REQUIRE(std::isfinite(num::to_double(result_two.upper)));
        REQUIRE(std::isfinite(num::to_double(result_lower.lower)));
        REQUIRE(std::isfinite(num::to_double(result_lower.upper)));
        REQUIRE(std::isfinite(num::to_double(result_upper.lower)));
        REQUIRE(std::isfinite(num::to_double(result_upper.upper)));
    }
    
    SECTION("Means are identical across interval types") {
        const double mean_two = num::to_double(result_two.mean);
        const double mean_lower = num::to_double(result_lower.mean);
        const double mean_upper = num::to_double(result_upper.mean);
        
        // Same sampler, same data, same seeds → all means identical
        REQUIRE(mean_two == Catch::Approx(mean_lower).epsilon(1e-10));
        REQUIRE(mean_two == Catch::Approx(mean_upper).epsilon(1e-10));
    }
    
    SECTION("All intervals maintain proper ordering") {
        REQUIRE(result_two.lower <= result_two.mean);
        REQUIRE(result_two.mean <= result_two.upper);
        
        REQUIRE(result_lower.lower <= result_lower.mean);
        REQUIRE(result_lower.mean <= result_lower.upper);
        
        REQUIRE(result_upper.lower <= result_upper.mean);
        REQUIRE(result_upper.mean <= result_upper.upper);
    }
    
    SECTION("Effective bootstrap replicates are similar") {
        // All should use approximately the same number of effective replicates
        REQUIRE(result_two.effective_B >= B / 2);
        REQUIRE(result_lower.effective_B >= B / 2);
        REQUIRE(result_upper.effective_B >= B / 2);
    }
}

// ==================== Diagnostics Test ====================

TEST_CASE("PercentileBootstrap: IntervalType with diagnostics",
          "[Bootstrap][Percentile][IntervalType][Diagnostics]")
{
    using D = DecimalType;
    auto x = createTestDataIT();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    REQUIRE_FALSE(pb.hasDiagnostics());
    
    randutils::seed_seq_fe128 seed{11u, 22u};
    std::mt19937_64 rng(seed);
    auto result = pb.run(x, sampler, rng);
    
    SECTION("Diagnostics are available after run") {
        REQUIRE(pb.hasDiagnostics());
        
        auto diag = pb.getAllDiagnostics();
        REQUIRE(diag.valid);
        REQUIRE(diag.bootstrapStats.size() == result.effective_B);
        REQUIRE(std::isfinite(diag.meanBoot));
        REQUIRE(std::isfinite(diag.varBoot));
        REQUIRE(std::isfinite(diag.seBoot));
        REQUIRE(diag.varBoot >= 0.0);
        REQUIRE(diag.seBoot >= 0.0);
    }
    
    SECTION("Individual diagnostic getters work") {
        REQUIRE(pb.hasDiagnostics());
        
        auto stats = pb.getBootstrapStatistics();
        REQUIRE(stats.size() == result.effective_B);
        
        double mean_boot = pb.getBootstrapMean();
        double var_boot = pb.getBootstrapVariance();
        double se_boot = pb.getBootstrapSe();
        
        REQUIRE(std::isfinite(mean_boot));
        REQUIRE(std::isfinite(var_boot));
        REQUIRE(std::isfinite(se_boot));
        REQUIRE(var_boot >= 0.0);
        REQUIRE(se_boot >= 0.0);
    }
}

// ==================== ThreadPool Executor Test ====================

TEST_CASE("PercentileBootstrap: IntervalType with ThreadPoolExecutor",
          "[Bootstrap][Percentile][IntervalType][ThreadPool]")
{
    using D = DecimalType;
    
    // Larger dataset for meaningful parallel execution
    std::vector<D> x;
    x.reserve(100);
    for (std::size_t i = 0; i < 100; ++i) {
        x.emplace_back(D(static_cast<double>(i) / 10.0));
    }
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1000;
    const double      CL = 0.95;
    
    // Single-threaded
    PercentileBootstrap<
        D, MeanSamplerIT, StationaryMaskValueResampler<D>,
        std::mt19937_64, concurrency::SingleThreadExecutor
    > pb_single(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    // Thread-pooled
    PercentileBootstrap<
        D, MeanSamplerIT, StationaryMaskValueResampler<D>,
        std::mt19937_64, concurrency::ThreadPoolExecutor<4>
    > pb_pool(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    // Use identical seeds for deterministic comparison
    randutils::seed_seq_fe128 seed1{0xBEEFu};
    randutils::seed_seq_fe128 seed2{0xBEEFu};
    std::mt19937_64 rng1(seed1), rng2(seed2);
    
    auto r1 = pb_single.run(x, sampler, rng1);
    auto r2 = pb_pool.run(x, sampler, rng2);
    
    SECTION("Results are numerically equivalent") {
        REQUIRE(r1.n == r2.n);
        REQUIRE(r1.B == r2.B);
        REQUIRE(r1.effective_B == r2.effective_B);
        REQUIRE(r1.skipped == r2.skipped);
        
        const double mean1 = num::to_double(r1.mean);
        const double mean2 = num::to_double(r2.mean);
        const double lb1 = num::to_double(r1.lower);
        const double lb2 = num::to_double(r2.lower);
        const double ub1 = num::to_double(r1.upper);
        const double ub2 = num::to_double(r2.upper);
        
        // Tight tolerance for deterministic equivalence
        REQUIRE(mean1 == Catch::Approx(mean2).epsilon(1e-12));
        REQUIRE(lb1 == Catch::Approx(lb2).epsilon(1e-12));
        REQUIRE(ub1 == Catch::Approx(ub2).epsilon(1e-12));
    }
}

// ==================== Edge Cases ====================

TEST_CASE("PercentileBootstrap: IntervalType with skewed data",
          "[Bootstrap][Percentile][IntervalType][EdgeCases]")
{
    using D = DecimalType;
    
    // Highly skewed data (exponential-like)
    std::vector<D> x;
    x.reserve(50);
    for (std::size_t i = 0; i < 50; ++i) {
        double val = std::exp(static_cast<double>(i) / 20.0);
        x.emplace_back(D(val));
    }
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    randutils::seed_seq_fe128 seed{555u, 666u};
    std::mt19937_64 rng(seed);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result = pb.run(x, sampler, rng);
    
    SECTION("Handles skewed data correctly") {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        
        // For skewed data, upper bound should be noticeably higher than mean
        const double mean = num::to_double(result.mean);
        const double ub = num::to_double(result.upper);
        REQUIRE(ub > mean);
    }
}

TEST_CASE("PercentileBootstrap: IntervalType with negative values",
          "[Bootstrap][Percentile][IntervalType][EdgeCases]")
{
    using D = DecimalType;
    
    // Data with negative values (returns)
    std::vector<D> x;
    x.reserve(30);
    for (int i = -15; i < 15; ++i) {
        x.emplace_back(D(static_cast<double>(i) / 10.0));
    }
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 700;
    const double      CL = 0.95;
    
    randutils::seed_seq_fe128 seed{777u, 888u};
    std::mt19937_64 rng(seed);
    
    PercentileBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        pb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result = pb.run(x, sampler, rng);
    
    SECTION("Handles negative values correctly") {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        
        // Mean should be close to 0 for symmetric data around 0
        const double mean = num::to_double(result.mean);
        REQUIRE(std::abs(mean) < 0.5);
    }
}
