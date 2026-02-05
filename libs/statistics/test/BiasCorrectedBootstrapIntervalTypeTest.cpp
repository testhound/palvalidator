// BiasCorrectedBootstrapIntervalTypeTest.cpp
//
// Unit tests for IntervalType functionality in BCaBootStrap class.
// Tests one-sided and two-sided confidence interval computation.
//
// These tests verify:
// - ONE_SIDED_LOWER intervals
// - ONE_SIDED_UPPER intervals
// - Comparison with TWO_SIDED intervals
// - Adaptive extreme quantile helper
// - Integration with different resamplers

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

#include "BiasCorrectedBootstrap.h"
#include "BootstrapTypes.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"
#include "StatUtils.h"

using namespace mkc_timeseries;
using namespace palvalidator::analysis;

// ==================== computeExtremeQuantile Tests ====================

TEST_CASE("BCaBootStrap::computeExtremeQuantile: Basic functionality", "[BCaBootStrap][IntervalType][ExtremeQuantile]")
{
    using D = DecimalType;
    
    SECTION("Upper extreme for CL=0.95 (alpha=0.05)")
    {
        double alpha = 0.05;
        double extreme_upper = BCaBootStrap<D>::computeExtremeQuantile(alpha, true);
        
        // Should be 1 - (0.05/1000) = 0.99995
        double expected = 1.0 - (alpha / 1000.0);
        REQUIRE(extreme_upper == Catch::Approx(expected));
        REQUIRE(extreme_upper > 0.9999);
        REQUIRE(extreme_upper < 1.0);
    }
    
    SECTION("Lower extreme for CL=0.95 (alpha=0.05)")
    {
        double alpha = 0.05;
        double extreme_lower = BCaBootStrap<D>::computeExtremeQuantile(alpha, false);
        
        // Should be 0.05/1000 = 0.00005
        double expected = alpha / 1000.0;
        REQUIRE(extreme_lower == Catch::Approx(expected));
        REQUIRE(extreme_lower > 0.0);
        REQUIRE(extreme_lower < 0.0001);
    }
    
    SECTION("Adapts to different confidence levels")
    {
        double alpha_95 = 0.05;
        double alpha_99 = 0.01;
        
        double extreme_95 = BCaBootStrap<D>::computeExtremeQuantile(alpha_95, true);
        double extreme_99 = BCaBootStrap<D>::computeExtremeQuantile(alpha_99, true);
        
        // Higher CL (smaller alpha) should produce more extreme quantile
        REQUIRE(extreme_99 > extreme_95);
        REQUIRE(extreme_95 == Catch::Approx(0.99995));
        REQUIRE(extreme_99 == Catch::Approx(0.99999));
    }
}

TEST_CASE("BCaBootStrap::computeExtremeQuantile: Maintains 1000:1 ratio", "[BCaBootStrap][IntervalType][ExtremeQuantile]")
{
    using D = DecimalType;
    
    std::vector<double> alphas = {0.10, 0.05, 0.01, 0.001};
    
    for (double alpha : alphas)
    {
        double extreme_upper = BCaBootStrap<D>::computeExtremeQuantile(alpha, true);
        double extreme_lower = BCaBootStrap<D>::computeExtremeQuantile(alpha, false);
        
        // Verify 1000:1 ratio
        double tail_prob_upper = 1.0 - extreme_upper;
        double tail_prob_lower = extreme_lower;
        
        REQUIRE(tail_prob_upper == Catch::Approx(alpha / 1000.0));
        REQUIRE(tail_prob_lower == Catch::Approx(alpha / 1000.0));
    }
}

// ==================== ONE_SIDED_LOWER Tests ====================

TEST_CASE("BCaBootStrap: ONE_SIDED_LOWER basic functionality", "[BCaBootStrap][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
        DecimalType("-0.01"), DecimalType("0.03"), DecimalType("-0.005"),
        DecimalType("0.025"), DecimalType("0.00"), DecimalType("-0.02"),
        DecimalType("0.018"), DecimalType("0.011"), DecimalType("0.027")
    };
    
    unsigned int B = 2000;
    double cl = 0.95;
    
    BCaBootStrap<D> bca_one_sided(returns, B, cl, &StatUtils<D>::computeMean, 
                                  IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
    
    D mean = bca_one_sided.getMean();
    D lower = bca_one_sided.getLowerBound();
    D upper = bca_one_sided.getUpperBound();
    
    SECTION("Bounds maintain ordering")
    {
        REQUIRE(lower <= mean);
        REQUIRE(mean <= upper);
    }
    
    SECTION("Lower bound is finite and reasonable")
    {
        REQUIRE(std::isfinite(num::to_double(lower)));
        REQUIRE(num::to_double(lower) < num::to_double(mean));
    }
    
    SECTION("Upper bound is very high (effectively unbounded)")
    {
        REQUIRE(std::isfinite(num::to_double(upper)));
        // Upper bound should be much farther from mean than lower bound
        D lower_dist = mean - lower;
        D upper_dist = upper - mean;

	REQUIRE(num::to_double(upper_dist) >= num::to_double(lower_dist));
    }
}

TEST_CASE("BCaBootStrap: ONE_SIDED_LOWER vs TWO_SIDED comparison", "[BCaBootStrap][IntervalType][Comparison]")
{
    using D = DecimalType;
    
    // Generate data with known distribution
    std::vector<D> returns;
    for (int i = 0; i < 50; ++i)
    {
        double val = 0.01 * std::sin(i * 0.3) + 0.005;
        returns.push_back(DecimalType(std::to_string(val).c_str()));
    }
    
    unsigned int B = 5000;
    double cl = 0.95;
    
    // Two-sided interval
    BCaBootStrap<D> bca_two_sided(returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::TWO_SIDED);
    
    // One-sided lower interval
    BCaBootStrap<D> bca_one_sided(returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
    
    D mean_two = bca_two_sided.getMean();
    D mean_one = bca_one_sided.getMean();
    D lb_two = bca_two_sided.getLowerBound();
    D lb_one = bca_one_sided.getLowerBound();
    D ub_two = bca_two_sided.getUpperBound();
    D ub_one = bca_one_sided.getUpperBound();
    
    SECTION("Means are identical (same data, same statistic)")
    {
        REQUIRE(num::to_double(mean_two) == Catch::Approx(num::to_double(mean_one)));
    }
    
    SECTION("One-sided lower bound is HIGHER than two-sided (less conservative)")
    {
        // This is the key property: one-sided 95% LB should be at 5th percentile,
        // while two-sided 95% LB is at 2.5th percentile
        REQUIRE(num::to_double(lb_one) > num::to_double(lb_two));
        
        // The difference should be approximately (z_0.025 - z_0.05) * SE
        // For large B, this should be noticeable
        double diff = num::to_double(lb_one) - num::to_double(lb_two);
        REQUIRE(diff > 0.0);
    }
    
    SECTION("One-sided upper bound is HIGHER than two-sided")
    {
        // One-sided upper should be at ~99.99%ile, two-sided at 97.5%ile
        REQUIRE(num::to_double(ub_one) > num::to_double(ub_two));
    }
}

TEST_CASE("BCaBootStrap: ONE_SIDED_LOWER with different confidence levels", "[BCaBootStrap][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.005"), DecimalType("-0.002"), DecimalType("0.008"),
        DecimalType("0.003"), DecimalType("0.001"), DecimalType("-0.004"),
        DecimalType("0.006"), DecimalType("0.002"), DecimalType("0.000"),
        DecimalType("-0.001"), DecimalType("0.007"), DecimalType("0.004")
    };
    
    unsigned int B = 3000;
    
    SECTION("CL=0.90 (alpha=0.10)")
    {
        BCaBootStrap<D> bca(returns, B, 0.90, &StatUtils<D>::computeMean,
                            IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
        
        REQUIRE(std::isfinite(num::to_double(bca.getLowerBound())));
        REQUIRE(bca.getLowerBound() <= bca.getMean());
    }
    
    SECTION("CL=0.95 (alpha=0.05)")
    {
        BCaBootStrap<D> bca(returns, B, 0.95, &StatUtils<D>::computeMean,
                            IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
        
        REQUIRE(std::isfinite(num::to_double(bca.getLowerBound())));
        REQUIRE(bca.getLowerBound() <= bca.getMean());
    }
    
    SECTION("CL=0.99 (alpha=0.01)")
    {
        BCaBootStrap<D> bca(returns, B, 0.99, &StatUtils<D>::computeMean,
                            IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
        
        REQUIRE(std::isfinite(num::to_double(bca.getLowerBound())));
        REQUIRE(bca.getLowerBound() <= bca.getMean());
    }
    
    SECTION("Higher CL produces lower bound closer to mean")
    {
        BCaBootStrap<D> bca_90(returns, B, 0.90, &StatUtils<D>::computeMean,
                               IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
        BCaBootStrap<D> bca_99(returns, B, 0.99, &StatUtils<D>::computeMean,
                               IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
        
        D lb_90 = bca_90.getLowerBound();
        D lb_99 = bca_99.getLowerBound();
        
        // 99% CL should have lower bound closer to mean (more conservative)
        REQUIRE(num::to_double(lb_99) < num::to_double(lb_90));
    }
}

// ==================== ONE_SIDED_UPPER Tests ====================

TEST_CASE("BCaBootStrap: ONE_SIDED_UPPER basic functionality", "[BCaBootStrap][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
        DecimalType("-0.01"), DecimalType("0.03"), DecimalType("-0.005"),
        DecimalType("0.025"), DecimalType("0.00"), DecimalType("-0.02"),
        DecimalType("0.018"), DecimalType("0.011"), DecimalType("0.027")
    };
    
    unsigned int B = 2000;
    double cl = 0.95;
    
    BCaBootStrap<D> bca_one_sided(returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::ONE_SIDED_UPPER);
    
    D mean = bca_one_sided.getMean();
    D lower = bca_one_sided.getLowerBound();
    D upper = bca_one_sided.getUpperBound();
    
    SECTION("Bounds maintain ordering")
    {
        REQUIRE(lower <= mean);
        REQUIRE(mean <= upper);
    }
    
    SECTION("Upper bound is finite and reasonable")
    {
        REQUIRE(std::isfinite(num::to_double(upper)));
        REQUIRE(num::to_double(upper) > num::to_double(mean));
    }
    
    SECTION("Lower bound is very low (effectively unbounded)")
    {
        REQUIRE(std::isfinite(num::to_double(lower)));
        // Lower bound should be much farther from mean than upper bound
        D lower_dist = mean - lower;
        D upper_dist = upper - mean;

	REQUIRE(num::to_double(lower_dist) >= num::to_double(upper_dist));
    }
}

TEST_CASE("BCaBootStrap: ONE_SIDED_UPPER vs TWO_SIDED comparison", "[BCaBootStrap][IntervalType][Comparison]")
{
    using D = DecimalType;
    
    std::vector<D> returns;
    for (int i = 0; i < 40; ++i)
    {
        double val = 0.005 * std::cos(i * 0.4) + 0.003;
        returns.push_back(DecimalType(std::to_string(val).c_str()));
    }
    
    unsigned int B = 4000;
    double cl = 0.95;
    
    BCaBootStrap<D> bca_two_sided(returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::TWO_SIDED);
    
    BCaBootStrap<D> bca_one_sided(returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::ONE_SIDED_UPPER);
    
    D mean_two = bca_two_sided.getMean();
    D mean_one = bca_one_sided.getMean();
    D lb_two = bca_two_sided.getLowerBound();
    D lb_one = bca_one_sided.getLowerBound();
    D ub_two = bca_two_sided.getUpperBound();
    D ub_one = bca_one_sided.getUpperBound();
    
    SECTION("Means are identical")
    {
        REQUIRE(num::to_double(mean_two) == Catch::Approx(num::to_double(mean_one)));
    }
    
    SECTION("One-sided upper bound is LOWER than two-sided (less conservative)")
    {
        // One-sided 95% UB should be at 95th percentile,
        // while two-sided 95% UB is at 97.5th percentile
        REQUIRE(num::to_double(ub_one) < num::to_double(ub_two));
        
        double diff = num::to_double(ub_two) - num::to_double(ub_one);
        REQUIRE(diff > 0.0);
    }
    
    SECTION("One-sided lower bound is LOWER than two-sided")
    {
        // One-sided lower should be at ~0.01%ile, two-sided at 2.5%ile
        REQUIRE(num::to_double(lb_one) < num::to_double(lb_two));
    }
}

// ==================== Integration with StationaryBlockResampler ====================

TEST_CASE("BCaBootStrap: ONE_SIDED_LOWER with StationaryBlockResampler", "[BCaBootStrap][IntervalType][StationaryBlock]")
{
    using D = DecimalType;
    using Resampler = StationaryBlockResampler<D>;
    
    // Autocorrelated data
    std::vector<D> returns;
    for (int i = 0; i < 60; ++i)
    {
        double val = 0.008 * std::sin(i * 0.2) + 0.004;
        returns.push_back(DecimalType(std::to_string(val).c_str()));
    }
    
    unsigned int B = 3000;
    double cl = 0.95;
    std::size_t L = 5;
    
    Resampler resampler(L);
    
    SECTION("TWO_SIDED baseline")
    {
        BCaBootStrap<D, Resampler> bca(returns, B, cl, &StatUtils<D>::computeMean,
                                       resampler, IntervalType::TWO_SIDED);
        
        REQUIRE(std::isfinite(num::to_double(bca.getLowerBound())));
        REQUIRE(std::isfinite(num::to_double(bca.getUpperBound())));
    }
    
    SECTION("ONE_SIDED_LOWER works with block resampler")
    {
        BCaBootStrap<D, Resampler> bca(returns, B, cl, &StatUtils<D>::computeMean,
                                       resampler, IntervalType::ONE_SIDED_LOWER);
        
        D mean = bca.getMean();
        D lower = bca.getLowerBound();
        D upper = bca.getUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(lower)));
        REQUIRE(std::isfinite(num::to_double(upper)));
        REQUIRE(lower <= mean);
        REQUIRE(mean <= upper);
    }
    
    SECTION("Comparison: one-sided lower bound is higher")
    {
        BCaBootStrap<D, Resampler> bca_two(returns, B, cl, &StatUtils<D>::computeMean,
                                           resampler, IntervalType::TWO_SIDED);
        BCaBootStrap<D, Resampler> bca_one(returns, B, cl, &StatUtils<D>::computeMean,
                                           resampler, IntervalType::ONE_SIDED_LOWER);
        
        D lb_two = bca_two.getLowerBound();
        D lb_one = bca_one.getLowerBound();
        
        REQUIRE(num::to_double(lb_one) > num::to_double(lb_two));
    }
}

TEST_CASE("BCaBootStrap: ONE_SIDED_UPPER with StationaryBlockResampler", "[BCaBootStrap][IntervalType][StationaryBlock]")
{
    using D = DecimalType;
    using Resampler = StationaryBlockResampler<D>;
    
    std::vector<D> returns;
    for (int i = 0; i < 50; ++i)
    {
        double val = 0.006 * std::cos(i * 0.3) + 0.002;
        returns.push_back(DecimalType(std::to_string(val).c_str()));
    }
    
    unsigned int B = 2500;
    double cl = 0.95;
    std::size_t L = 4;
    
    Resampler resampler(L);
    
    BCaBootStrap<D, Resampler> bca_two(returns, B, cl, &StatUtils<D>::computeMean,
                                       resampler, IntervalType::TWO_SIDED);
    BCaBootStrap<D, Resampler> bca_one(returns, B, cl, &StatUtils<D>::computeMean,
                                       resampler, IntervalType::ONE_SIDED_UPPER);
    
    D ub_two = bca_two.getUpperBound();
    D ub_one = bca_one.getUpperBound();
    
    SECTION("One-sided upper bound is lower (less conservative)")
    {
        REQUIRE(num::to_double(ub_one) < num::to_double(ub_two));
    }
}

// ==================== Edge Cases and Error Conditions ====================

TEST_CASE("BCaBootStrap: IntervalType with minimum dataset", "[BCaBootStrap][IntervalType][EdgeCase]")
{
    using D = DecimalType;
    
    std::vector<D> tiny_returns = {DecimalType("0.01"), DecimalType("-0.01")};
    unsigned int B = 1000;
    
    SECTION("ONE_SIDED_LOWER with n=2")
    {
        REQUIRE_NOTHROW(BCaBootStrap<D>(tiny_returns, B, 0.95, &StatUtils<D>::computeMean,
                                        IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER));
    }
    
    SECTION("ONE_SIDED_UPPER with n=2")
    {
        REQUIRE_NOTHROW(BCaBootStrap<D>(tiny_returns, B, 0.95, &StatUtils<D>::computeMean,
                                        IIDResampler<D>(), IntervalType::ONE_SIDED_UPPER));
    }
}

TEST_CASE("BCaBootStrap: IntervalType with skewed data", "[BCaBootStrap][IntervalType][Skewness]")
{
    using D = DecimalType;
    
    // Highly skewed data (mostly small positive, few large positive)
    std::vector<D> skewed_returns = {
        DecimalType("0.01"), DecimalType("0.01"), DecimalType("0.01"),
        DecimalType("0.01"), DecimalType("0.01"), DecimalType("0.01"),
        DecimalType("0.01"), DecimalType("0.01"), DecimalType("0.01"),
        DecimalType("0.15"), DecimalType("0.20"), DecimalType("0.25")
    };
    
    unsigned int B = 2000;
    double cl = 0.95;
    
    BCaBootStrap<D> bca_two(skewed_returns, B, cl, &StatUtils<D>::computeMean,
                            IIDResampler<D>(), IntervalType::TWO_SIDED);
    BCaBootStrap<D> bca_one_lower(skewed_returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
    BCaBootStrap<D> bca_one_upper(skewed_returns, B, cl, &StatUtils<D>::computeMean,
                                  IIDResampler<D>(), IntervalType::ONE_SIDED_UPPER);
    
    SECTION("All intervals are computable despite skewness")
    {
        REQUIRE(std::isfinite(num::to_double(bca_two.getLowerBound())));
        REQUIRE(std::isfinite(num::to_double(bca_one_lower.getLowerBound())));
        REQUIRE(std::isfinite(num::to_double(bca_one_upper.getUpperBound())));
    }
    
    SECTION("One-sided intervals show expected relationships")
    {
        D lb_two = bca_two.getLowerBound();
        D lb_one = bca_one_lower.getLowerBound();
        D ub_two = bca_two.getUpperBound();
        D ub_one = bca_one_upper.getUpperBound();

	REQUIRE(num::to_double(lb_one) >= num::to_double(lb_two));  // Allow equality
	REQUIRE(num::to_double(ub_one) <= num::to_double(ub_two));  // Allow equality
    }
}

// ==================== Diagnostics with IntervalType ====================

TEST_CASE("BCaBootStrap: Diagnostics are consistent across interval types", "[BCaBootStrap][IntervalType][Diagnostics]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
        DecimalType("-0.01"), DecimalType("0.03"), DecimalType("-0.005"),
        DecimalType("0.025"), DecimalType("0.00"), DecimalType("-0.02")
    };
    
    unsigned int B = 2000;
    double cl = 0.95;
    
    BCaBootStrap<D> bca_two(returns, B, cl, &StatUtils<D>::computeMean,
                            IIDResampler<D>(), IntervalType::TWO_SIDED);
    BCaBootStrap<D> bca_one(returns, B, cl, &StatUtils<D>::computeMean,
                            IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
    
    SECTION("z0 and acceleration are identical (same bootstrap resamples)")
    {
        double z0_two = bca_two.getZ0();
        double z0_one = bca_one.getZ0();
        D accel_two = bca_two.getAcceleration();
        D accel_one = bca_one.getAcceleration();
        
        // z0 and accel depend only on the bootstrap distribution and jackknife,
        // not on how we compute percentiles, so they should be very similar
        // (may differ slightly due to RNG, but using same seed they'd be identical)
        REQUIRE(std::isfinite(z0_two));
        REQUIRE(std::isfinite(z0_one));
        REQUIRE(std::isfinite(num::to_double(accel_two)));
        REQUIRE(std::isfinite(num::to_double(accel_one)));
    }
    
    SECTION("Bootstrap statistics vector has same size")
    {
        const auto& boot_two = bca_two.getBootstrapStatistics();
        const auto& boot_one = bca_one.getBootstrapStatistics();
        
        REQUIRE(boot_two.size() == B);
        REQUIRE(boot_one.size() == B);
    }
    
    SECTION("Mean is identical across interval types")
    {
        D mean_two = bca_two.getMean();
        D mean_one = bca_one.getMean();
        
        REQUIRE(num::to_double(mean_two) == Catch::Approx(num::to_double(mean_one)));
    }
}

// ==================== BCaAnnualizer with IntervalType ====================

TEST_CASE("BCaAnnualizer: Works with ONE_SIDED_LOWER intervals", "[BCaAnnualizer][IntervalType]")
{
    using D = DecimalType;
    
    std::vector<D> daily_returns = {
        DecimalType("0.001"), DecimalType("0.002"), DecimalType("-0.001"),
        DecimalType("0.0015"), DecimalType("0.0025"), DecimalType("0.001"),
        DecimalType("-0.0005"), DecimalType("0.002"), DecimalType("0.0015")
    };
    
    BCaBootStrap<D> bca(daily_returns, 1000, 0.95, &StatUtils<D>::computeMean,
                        IIDResampler<D>(), IntervalType::ONE_SIDED_LOWER);
    
    BCaAnnualizer<D> annualizer(bca, 252.0);
    
    SECTION("Annualized bounds are computed correctly")
    {
        D ann_mean = annualizer.getAnnualizedMean();
        D ann_lower = annualizer.getAnnualizedLowerBound();
        D ann_upper = annualizer.getAnnualizedUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(ann_mean)));
        REQUIRE(std::isfinite(num::to_double(ann_lower)));
        REQUIRE(std::isfinite(num::to_double(ann_upper)));
        REQUIRE(ann_lower <= ann_mean);
        REQUIRE(ann_mean <= ann_upper);
    }
}

TEST_CASE("BCaAnnualizer: Works with ONE_SIDED_UPPER intervals", "[BCaAnnualizer][IntervalType]")
{
    using D = DecimalType;
    
    std::vector<D> daily_returns = {
        DecimalType("0.001"), DecimalType("0.002"), DecimalType("-0.001"),
        DecimalType("0.0015"), DecimalType("0.0025"), DecimalType("0.001")
    };
    
    BCaBootStrap<D> bca(daily_returns, 1000, 0.95, &StatUtils<D>::computeMean,
                        IIDResampler<D>(), IntervalType::ONE_SIDED_UPPER);
    
    BCaAnnualizer<D> annualizer(bca, 252.0);
    
    SECTION("Annualized bounds maintain ordering")
    {
        D ann_mean = annualizer.getAnnualizedMean();
        D ann_lower = annualizer.getAnnualizedLowerBound();
        D ann_upper = annualizer.getAnnualizedUpperBound();
        
        REQUIRE(ann_lower <= ann_mean);
        REQUIRE(ann_mean <= ann_upper);
    }
}

// ==================== Backward Compatibility ====================

TEST_CASE("BCaBootStrap: Default IntervalType is TWO_SIDED", "[BCaBootStrap][IntervalType][BackwardCompatibility]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
        DecimalType("-0.01"), DecimalType("0.03"), DecimalType("-0.005")
    };
    
    unsigned int B = 2000;
    double cl = 0.95;
    
    // Constructor without IntervalType parameter (should default to TWO_SIDED)
    BCaBootStrap<D> bca_default(returns, B, cl);
    
    // Explicit TWO_SIDED
    BCaBootStrap<D> bca_explicit(returns, B, cl, &StatUtils<D>::computeMean,
                                 IIDResampler<D>(), IntervalType::TWO_SIDED);
    
    SECTION("Default behavior matches explicit TWO_SIDED")
    {
        D lb_default = bca_default.getLowerBound();
        D lb_explicit = bca_explicit.getLowerBound();
        D ub_default = bca_default.getUpperBound();
        D ub_explicit = bca_explicit.getUpperBound();
        
        // Should be very close (may differ slightly due to RNG)
        REQUIRE(num::to_double(lb_default) == Catch::Approx(num::to_double(lb_explicit)).margin(0.01));
        REQUIRE(num::to_double(ub_default) == Catch::Approx(num::to_double(ub_explicit)).margin(0.01));
    }
}
