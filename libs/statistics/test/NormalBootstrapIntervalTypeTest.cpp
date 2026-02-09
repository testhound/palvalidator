// NormalBootstrapIntervalTypeTest.cpp
//
// Unit tests for IntervalType functionality in NormalBootstrap
// Tests ONE_SIDED_UPPER, ONE_SIDED_LOWER, and TWO_SIDED confidence intervals
//
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - NormalBootstrap.h (with IntervalType support)
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
#include "NormalBootstrap.h"
#include "BootstrapTypes.h"
#include "ParallelExecutors.h"

using palvalidator::analysis::NormalBootstrap;
using palvalidator::analysis::IntervalType;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple arithmetic mean sampler (matching existing tests)
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
std::vector<DecimalType> createTestDataNB()
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

TEST_CASE("NormalBootstrap: ONE_SIDED_UPPER basic functionality",
          "[Bootstrap][Normal][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    randutils::seed_seq_fe128 seed{11u, 22u, 33u, 44u};
    std::mt19937_64 rng(seed);
    
    auto result = nb.run(x, sampler, rng);
    
    SECTION("Result structure is valid") {
        REQUIRE(result.B == B);
        REQUIRE(result.n == x.size());
        REQUIRE(result.effective_B >= B / 2);
        REQUIRE(result.cl == Catch::Approx(CL).margin(1e-12));
        
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(std::isfinite(result.se_boot));
        REQUIRE(result.se_boot > 0.0);
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
    
    SECTION("Lower bound is very low (effectively unbounded)") {
        const double lb = num::to_double(result.lower);
        const double mean = num::to_double(result.mean);
        const double se = result.se_boot;
        
        // Lower bound should be far below mean (order of 1e6 * se)
        REQUIRE(lb <= mean);
        REQUIRE(mean - lb > 1000.0 * se);  // Very far below
    }
}

TEST_CASE("NormalBootstrap: ONE_SIDED_UPPER different confidence levels",
          "[Bootstrap][Normal][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B = 700;
    
    SECTION("CL=0.90") {
        NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
            nb(B, 0.90, res, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        auto result = nb.run(x, sampler, rng);
        
        REQUIRE(result.lower <= result.upper);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.se_boot > 0.0);
        REQUIRE(result.cl == Catch::Approx(0.90).margin(1e-12));
    }
    
    SECTION("CL=0.95") {
        NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
            nb(B, 0.95, res, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        auto result = nb.run(x, sampler, rng);
        
        REQUIRE(result.lower <= result.upper);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.se_boot > 0.0);
        REQUIRE(result.cl == Catch::Approx(0.95).margin(1e-12));
    }
    
    SECTION("CL=0.99") {
        NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
            nb(B, 0.99, res, IntervalType::ONE_SIDED_UPPER);
        
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        auto result = nb.run(x, sampler, rng);
        
        REQUIRE(result.lower <= result.upper);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.se_boot > 0.0);
        REQUIRE(result.cl == Catch::Approx(0.99).margin(1e-12));
    }
}

TEST_CASE("NormalBootstrap: ONE_SIDED_UPPER higher CL increases upper bound",
          "[Bootstrap][Normal][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B = 1000;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed90{100u, 200u};
    randutils::seed_seq_fe128 seed95{100u, 200u};
    randutils::seed_seq_fe128 seed99{100u, 200u};
    
    std::mt19937_64 rng90(seed90), rng95(seed95), rng99(seed99);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb90(B, 0.90, res, IntervalType::ONE_SIDED_UPPER),
        nb95(B, 0.95, res, IntervalType::ONE_SIDED_UPPER),
        nb99(B, 0.99, res, IntervalType::ONE_SIDED_UPPER);
    
    auto r90 = nb90.run(x, sampler, rng90);
    auto r95 = nb95.run(x, sampler, rng95);
    auto r99 = nb99.run(x, sampler, rng99);
    
    const double ub90 = num::to_double(r90.upper);
    const double ub95 = num::to_double(r95.upper);
    const double ub99 = num::to_double(r99.upper);
    
    // Higher confidence → higher upper bound
    // Allow small tolerance for bootstrap SE variation
    REQUIRE(ub95 >= ub90 - 0.5);
    REQUIRE(ub99 >= ub95 - 0.5);
}

// ==================== ONE_SIDED_LOWER Tests ====================

TEST_CASE("NormalBootstrap: ONE_SIDED_LOWER basic functionality",
          "[Bootstrap][Normal][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb(B, CL, res, IntervalType::ONE_SIDED_LOWER);
    
    randutils::seed_seq_fe128 seed{11u, 22u, 33u, 44u};
    std::mt19937_64 rng(seed);
    
    auto result = nb.run(x, sampler, rng);
    
    SECTION("Result structure is valid") {
        REQUIRE(result.B == B);
        REQUIRE(result.n == x.size());
        REQUIRE(result.effective_B >= B / 2);
        REQUIRE(result.cl == Catch::Approx(CL).margin(1e-12));
        
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(std::isfinite(result.se_boot));
        REQUIRE(result.se_boot > 0.0);
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
    
    SECTION("Upper bound is very high (effectively unbounded)") {
        const double ub = num::to_double(result.upper);
        const double mean = num::to_double(result.mean);
        const double se = result.se_boot;
        
        // Upper bound should be far above mean (order of 1e6 * se)
        REQUIRE(ub >= mean);
        REQUIRE(ub - mean > 1000.0 * se);  // Very far above
    }
}

TEST_CASE("NormalBootstrap: ONE_SIDED_LOWER higher CL decreases lower bound",
          "[Bootstrap][Normal][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B = 1000;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed90{100u, 200u};
    randutils::seed_seq_fe128 seed95{100u, 200u};
    randutils::seed_seq_fe128 seed99{100u, 200u};
    
    std::mt19937_64 rng90(seed90), rng95(seed95), rng99(seed99);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb90(B, 0.90, res, IntervalType::ONE_SIDED_LOWER),
        nb95(B, 0.95, res, IntervalType::ONE_SIDED_LOWER),
        nb99(B, 0.99, res, IntervalType::ONE_SIDED_LOWER);
    
    auto r90 = nb90.run(x, sampler, rng90);
    auto r95 = nb95.run(x, sampler, rng95);
    auto r99 = nb99.run(x, sampler, rng99);
    
    const double lb90 = num::to_double(r90.lower);
    const double lb95 = num::to_double(r95.lower);
    const double lb99 = num::to_double(r99.lower);
    
    // Higher confidence → lower lower bound
    // Allow small tolerance for bootstrap SE variation
    REQUIRE(lb95 <= lb90 + 0.5);
    REQUIRE(lb99 <= lb95 + 0.5);
}

// ==================== Comparison Tests ====================

TEST_CASE("NormalBootstrap: ONE_SIDED_UPPER vs TWO_SIDED comparison",
          "[Bootstrap][Normal][IntervalType][Comparison]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1200;
    const double      CL = 0.95;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed_two{99u, 88u, 77u, 66u};
    randutils::seed_seq_fe128 seed_one{99u, 88u, 77u, 66u};
    
    std::mt19937_64 rng_two(seed_two), rng_one(seed_one);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb_two(B, CL, res, IntervalType::TWO_SIDED),
        nb_one(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result_two = nb_two.run(x, sampler, rng_two);
    auto result_one = nb_one.run(x, sampler, rng_one);
    
    SECTION("Means are identical") {
        const double mean_two = num::to_double(result_two.mean);
        const double mean_one = num::to_double(result_one.mean);
        
        // Same data, same sampler → means should be identical
        REQUIRE(mean_two == Catch::Approx(mean_one).epsilon(1e-10));
    }
    
    SECTION("Bootstrap SE is identical") {
        const double se_two = result_two.se_boot;
        const double se_one = result_one.se_boot;
        
        // Same bootstrap process → SE should be identical (or very close)
        REQUIRE(se_two == Catch::Approx(se_one).margin(0.01));
    }
    
    SECTION("ONE_SIDED_UPPER upper bound is less conservative") {
        const double ub_two = num::to_double(result_two.upper);
        const double ub_one = num::to_double(result_one.upper);
        const double mean = num::to_double(result_two.mean);
        const double se = result_two.se_boot;
        
        // ONE_SIDED_UPPER: z = 1.645 (at CL=0.95)
        // TWO_SIDED: z = 1.96
        // ONE_SIDED should be tighter
        REQUIRE(ub_one <= ub_two + 0.5);
        
        // Verify approximate z-score relationship
        const double margin_one = ub_one - mean;
        const double margin_two = ub_two - mean;
        
        // Ratio should be approximately 1.645/1.96 ≈ 0.84
        if (se > 0.001) {  // Only if SE is meaningful
            const double ratio = margin_one / margin_two;
            REQUIRE(ratio >= 0.75);  // Allow some tolerance
            REQUIRE(ratio <= 0.93);  // Should be less than 1
        }
    }
    
    SECTION("ONE_SIDED_UPPER lower bound is less constrained") {
        const double lb_two = num::to_double(result_two.lower);
        const double lb_one = num::to_double(result_one.lower);
        const double mean = num::to_double(result_two.mean);
        const double se = result_two.se_boot;
        
        // ONE_SIDED_UPPER lower bound should be far below TWO_SIDED
        REQUIRE(lb_one <= lb_two);
        REQUIRE(mean - lb_one > 100.0 * se);  // Very far below
    }
}

TEST_CASE("NormalBootstrap: ONE_SIDED_LOWER vs TWO_SIDED comparison",
          "[Bootstrap][Normal][IntervalType][Comparison]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1200;
    const double      CL = 0.95;
    
    // Use identical seeds for fair comparison
    randutils::seed_seq_fe128 seed_two{99u, 88u, 77u, 66u};
    randutils::seed_seq_fe128 seed_one{99u, 88u, 77u, 66u};
    
    std::mt19937_64 rng_two(seed_two), rng_one(seed_one);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb_two(B, CL, res, IntervalType::TWO_SIDED),
        nb_one(B, CL, res, IntervalType::ONE_SIDED_LOWER);
    
    auto result_two = nb_two.run(x, sampler, rng_two);
    auto result_one = nb_one.run(x, sampler, rng_one);
    
    SECTION("Means are identical") {
        const double mean_two = num::to_double(result_two.mean);
        const double mean_one = num::to_double(result_one.mean);
        
        // Same data, same sampler → means should be identical
        REQUIRE(mean_two == Catch::Approx(mean_one).epsilon(1e-10));
    }
    
    SECTION("Bootstrap SE is identical") {
        const double se_two = result_two.se_boot;
        const double se_one = result_one.se_boot;
        
        // Same bootstrap process → SE should be identical (or very close)
        REQUIRE(se_two == Catch::Approx(se_one).margin(0.01));
    }
    
    SECTION("ONE_SIDED_LOWER lower bound is higher (less conservative)") {
        const double lb_two = num::to_double(result_two.lower);
        const double lb_one = num::to_double(result_one.lower);
        const double mean = num::to_double(result_two.mean);
        const double se = result_two.se_boot;
        
        // ONE_SIDED_LOWER: z = 1.645 (at CL=0.95)
        // TWO_SIDED: z = 1.96
        // ONE_SIDED should be higher (less conservative)
        REQUIRE(lb_one >= lb_two - 0.5);
        
        // Verify approximate z-score relationship
        const double margin_one = mean - lb_one;
        const double margin_two = mean - lb_two;
        
        // Ratio should be approximately 1.645/1.96 ≈ 0.84
        if (se > 0.001) {  // Only if SE is meaningful
            const double ratio = margin_one / margin_two;
            REQUIRE(ratio >= 0.75);  // Allow some tolerance
            REQUIRE(ratio <= 0.93);  // Should be less than 1
        }
    }
    
    SECTION("ONE_SIDED_LOWER upper bound is less constrained") {
        const double ub_two = num::to_double(result_two.upper);
        const double ub_one = num::to_double(result_one.upper);
        const double mean = num::to_double(result_two.mean);
        const double se = result_two.se_boot;
        
        // ONE_SIDED_LOWER upper bound should be far above TWO_SIDED
        REQUIRE(ub_one >= ub_two);
        REQUIRE(ub_one - mean > 100.0 * se);  // Very far above
    }
}

// ==================== Backward Compatibility ====================

TEST_CASE("NormalBootstrap: Default IntervalType is TWO_SIDED",
          "[Bootstrap][Normal][IntervalType][BackwardCompatibility]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1000;
    const double      CL = 0.95;
    
    // Constructor without IntervalType parameter (should default to TWO_SIDED)
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb_default(B, CL, res);
    
    // Explicit TWO_SIDED
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb_explicit(B, CL, res, IntervalType::TWO_SIDED);
    
    // Use identical seeds
    randutils::seed_seq_fe128 seed_default{123u, 456u};
    randutils::seed_seq_fe128 seed_explicit{123u, 456u};
    
    std::mt19937_64 rng_default(seed_default), rng_explicit(seed_explicit);
    
    auto result_default = nb_default.run(x, sampler, rng_default);
    auto result_explicit = nb_explicit.run(x, sampler, rng_explicit);
    
    SECTION("Default behavior produces reasonable results") {
        REQUIRE(std::isfinite(num::to_double(result_default.mean)));
        REQUIRE(std::isfinite(num::to_double(result_default.lower)));
        REQUIRE(std::isfinite(num::to_double(result_default.upper)));
        REQUIRE(std::isfinite(result_default.se_boot));
        REQUIRE(result_default.se_boot > 0.0);
        REQUIRE(result_default.lower <= result_default.upper);
    }
    
    SECTION("Default is identical to explicit TWO_SIDED") {
        const double mean_default = num::to_double(result_default.mean);
        const double mean_explicit = num::to_double(result_explicit.mean);
        
        // Same sampler, same data, same seeds → means identical
        REQUIRE(mean_default == Catch::Approx(mean_explicit).epsilon(1e-10));
        
        // Bounds should also be identical (same RNG seeds, same formula)
        const double lb_default = num::to_double(result_default.lower);
        const double lb_explicit = num::to_double(result_explicit.lower);
        const double ub_default = num::to_double(result_default.upper);
        const double ub_explicit = num::to_double(result_explicit.upper);
        
        REQUIRE(lb_default == Catch::Approx(lb_explicit).margin(0.01));
        REQUIRE(ub_default == Catch::Approx(ub_explicit).margin(0.01));
        
        // SE should be identical
        REQUIRE(result_default.se_boot == Catch::Approx(result_explicit.se_boot).margin(0.001));
    }
}

// ==================== Comprehensive Integration Test ====================

TEST_CASE("NormalBootstrap: All three interval types on same data",
          "[Bootstrap][Normal][IntervalType][Comprehensive]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1500;
    const double      CL = 0.95;
    
    // Use identical seeds for each (for deterministic comparison)
    randutils::seed_seq_fe128 seed_two{999u, 888u};
    randutils::seed_seq_fe128 seed_lower{999u, 888u};
    randutils::seed_seq_fe128 seed_upper{999u, 888u};
    
    std::mt19937_64 rng_two(seed_two), rng_lower(seed_lower), rng_upper(seed_upper);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb_two(B, CL, res, IntervalType::TWO_SIDED),
        nb_lower(B, CL, res, IntervalType::ONE_SIDED_LOWER),
        nb_upper(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result_two = nb_two.run(x, sampler, rng_two);
    auto result_lower = nb_lower.run(x, sampler, rng_lower);
    auto result_upper = nb_upper.run(x, sampler, rng_upper);
    
    SECTION("All intervals produce valid results") {
        REQUIRE(std::isfinite(num::to_double(result_two.lower)));
        REQUIRE(std::isfinite(num::to_double(result_two.upper)));
        REQUIRE(std::isfinite(result_two.se_boot));
        
        REQUIRE(std::isfinite(num::to_double(result_lower.lower)));
        REQUIRE(std::isfinite(num::to_double(result_lower.upper)));
        REQUIRE(std::isfinite(result_lower.se_boot));
        
        REQUIRE(std::isfinite(num::to_double(result_upper.lower)));
        REQUIRE(std::isfinite(num::to_double(result_upper.upper)));
        REQUIRE(std::isfinite(result_upper.se_boot));
    }
    
    SECTION("Means are identical across interval types") {
        const double mean_two = num::to_double(result_two.mean);
        const double mean_lower = num::to_double(result_lower.mean);
        const double mean_upper = num::to_double(result_upper.mean);
        
        // Same sampler, same data, same seeds → all means identical
        REQUIRE(mean_two == Catch::Approx(mean_lower).epsilon(1e-10));
        REQUIRE(mean_two == Catch::Approx(mean_upper).epsilon(1e-10));
    }
    
    SECTION("Bootstrap SE is identical across interval types") {
        const double se_two = result_two.se_boot;
        const double se_lower = result_lower.se_boot;
        const double se_upper = result_upper.se_boot;
        
        // Same bootstrap process → all SE identical
        REQUIRE(se_two == Catch::Approx(se_lower).margin(0.01));
        REQUIRE(se_two == Catch::Approx(se_upper).margin(0.01));
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

// ==================== Z-Score Verification ====================

TEST_CASE("NormalBootstrap: Z-score relationships for CL=0.95",
          "[Bootstrap][Normal][IntervalType][ZScore]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 1500;
    const double      CL = 0.95;
    
    // Use identical seeds
    randutils::seed_seq_fe128 seed_two{777u, 888u};
    randutils::seed_seq_fe128 seed_one{777u, 888u};
    
    std::mt19937_64 rng_two(seed_two), rng_one(seed_one);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb_two(B, CL, res, IntervalType::TWO_SIDED),
        nb_one(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result_two = nb_two.run(x, sampler, rng_two);
    auto result_one = nb_one.run(x, sampler, rng_one);
    
    const double mean = num::to_double(result_two.mean);
    const double se = result_two.se_boot;
    
    const double ub_two = num::to_double(result_two.upper);
    const double ub_one = num::to_double(result_one.upper);
    
    SECTION("TWO_SIDED uses z ≈ 1.96") {
        const double margin = ub_two - mean;
        const double z_empirical = margin / se;
        
        // Should be approximately 1.96 for CL=0.95
        REQUIRE(z_empirical >= 1.85);
        REQUIRE(z_empirical <= 2.05);
    }
    
    SECTION("ONE_SIDED_UPPER uses z ≈ 1.645") {
        const double margin = ub_one - mean;
        const double z_empirical = margin / se;
        
        // Should be approximately 1.645 for CL=0.95
        REQUIRE(z_empirical >= 1.55);
        REQUIRE(z_empirical <= 1.75);
    }
    
    SECTION("Z-score ratio is approximately 1.645/1.96 ≈ 0.84") {
        const double margin_one = ub_one - mean;
        const double margin_two = ub_two - mean;
        
        const double ratio = margin_one / margin_two;
        
        // Should be approximately 0.84
        REQUIRE(ratio >= 0.80);
        REQUIRE(ratio <= 0.88);
    }
}

// ==================== Diagnostics Test ====================

TEST_CASE("NormalBootstrap: IntervalType with diagnostics",
          "[Bootstrap][Normal][IntervalType][Diagnostics]")
{
    using D = DecimalType;
    auto x = createTestDataNB();
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 800;
    const double      CL = 0.95;
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    REQUIRE_FALSE(nb.hasDiagnostics());
    
    randutils::seed_seq_fe128 seed{11u, 22u};
    std::mt19937_64 rng(seed);
    auto result = nb.run(x, sampler, rng);
    
    SECTION("Diagnostics are available after run") {
        REQUIRE(nb.hasDiagnostics());
        
        REQUIRE(std::isfinite(nb.getBootstrapMean()));
        REQUIRE(std::isfinite(nb.getBootstrapVariance()));
        REQUIRE(std::isfinite(nb.getBootstrapSe()));
        REQUIRE(nb.getBootstrapVariance() >= 0.0);
        REQUIRE(nb.getBootstrapSe() >= 0.0);
    }
    
    SECTION("Individual diagnostic getters work") {
        REQUIRE(nb.hasDiagnostics());
        
        auto stats = nb.getBootstrapStatistics();
        REQUIRE(stats.size() == result.effective_B);
        
        double mean_boot = nb.getBootstrapMean();
        double var_boot = nb.getBootstrapVariance();
        double se_boot = nb.getBootstrapSe();
        
        REQUIRE(std::isfinite(mean_boot));
        REQUIRE(std::isfinite(var_boot));
        REQUIRE(std::isfinite(se_boot));
        REQUIRE(var_boot >= 0.0);
        REQUIRE(se_boot >= 0.0);
        
        // SE from diagnostics should match result
        REQUIRE(se_boot == Catch::Approx(result.se_boot).margin(1e-10));
    }
}

// ==================== ThreadPool Executor Test ====================

TEST_CASE("NormalBootstrap: IntervalType with ThreadPoolExecutor",
          "[Bootstrap][Normal][IntervalType][ThreadPool]")
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
    NormalBootstrap<
        D, MeanSamplerIT, StationaryMaskValueResampler<D>,
        std::mt19937_64, concurrency::SingleThreadExecutor
    > nb_single(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    // Thread-pooled
    NormalBootstrap<
        D, MeanSamplerIT, StationaryMaskValueResampler<D>,
        std::mt19937_64, concurrency::ThreadPoolExecutor<4>
    > nb_pool(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    // Use identical seeds for deterministic comparison
    randutils::seed_seq_fe128 seed1{0xBEEFu};
    randutils::seed_seq_fe128 seed2{0xBEEFu};
    std::mt19937_64 rng1(seed1), rng2(seed2);
    
    auto r1 = nb_single.run(x, sampler, rng1);
    auto r2 = nb_pool.run(x, sampler, rng2);
    
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
        REQUIRE(r1.se_boot == Catch::Approx(r2.se_boot).margin(1e-10));
    }
}

// ==================== Edge Cases ====================

TEST_CASE("NormalBootstrap: IntervalType with negative values",
          "[Bootstrap][Normal][IntervalType][EdgeCases]")
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
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result = nb.run(x, sampler, rng);
    
    SECTION("Handles negative values correctly") {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(std::isfinite(result.se_boot));
        REQUIRE(result.se_boot > 0.0);
        
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        
        // Mean should be close to 0 for symmetric data around 0
        const double mean = num::to_double(result.mean);
        REQUIRE(std::abs(mean) < 0.5);
    }
}

TEST_CASE("NormalBootstrap: IntervalType with small SE",
          "[Bootstrap][Normal][IntervalType][EdgeCases]")
{
    using D = DecimalType;
    
    // Very homogeneous data (small SE)
    std::vector<D> x;
    x.reserve(30);
    for (std::size_t i = 0; i < 30; ++i) {
        x.emplace_back(D(10.0 + static_cast<double>(i % 3) * 0.01));  // Values: 10.00, 10.01, 10.02
    }
    
    MeanSamplerIT sampler;
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B  = 700;
    const double      CL = 0.95;
    
    randutils::seed_seq_fe128 seed{555u, 666u};
    std::mt19937_64 rng(seed);
    
    NormalBootstrap<D, MeanSamplerIT, StationaryMaskValueResampler<D>>
        nb(B, CL, res, IntervalType::ONE_SIDED_UPPER);
    
    auto result = nb.run(x, sampler, rng);
    
    SECTION("Handles small SE correctly") {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(std::isfinite(result.se_boot));
        REQUIRE(result.se_boot >= 0.0);
        
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        
        // Mean should be close to 10.01 (average of 10.00, 10.01, 10.02)
        const double mean = num::to_double(result.mean);
        REQUIRE(mean >= 10.0);
        REQUIRE(mean <= 10.02);
        
        // SE should be small but positive
        REQUIRE(result.se_boot < 0.1);
    }
}
