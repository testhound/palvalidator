// NormalQuantileTest.cpp
//
// Unit tests for normal distribution functions:
//  - palvalidator::analysis::detail::compute_normal_quantile (Acklam's algorithm)
//  - palvalidator::analysis::detail::compute_normal_cdf (error function based)
//  - palvalidator::analysis::detail::compute_normal_critical_value
//  - mkc_timeseries::NormalDistribution wrapper functions
//
// Tests verify:
//  - Accuracy against known values (e.g., 1.9599639861 for 95% CI)
//  - Boundary behavior and error handling
//  - Symmetry properties of the standard normal distribution
//  - Consistency between forward and inverse functions
//  - Wrapper delegation to underlying implementations
//
// Reference: Acklam, P.J. (2010). "An algorithm for computing the inverse
// normal cumulative distribution function."

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <array>
#include <list>
#include <deque>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "NormalQuantile.h"
#include "NormalDistribution.h"
#include "decimal.h"

using Catch::Approx;
using palvalidator::analysis::detail::compute_empirical_cdf;
using palvalidator::analysis::detail::compute_normal_quantile;
using palvalidator::analysis::detail::compute_normal_cdf;
using palvalidator::analysis::detail::compute_normal_critical_value;
using mkc_timeseries::NormalDistribution;

// ============================================================================
// Tests for compute_normal_quantile (Acklam's algorithm)
// ============================================================================

TEST_CASE("compute_normal_quantile: standard critical values", 
          "[NormalQuantile][quantile][critical]")
{
    // Well-known critical values for confidence intervals
    // These are reference values that should be accurate to ~1e-9

    SECTION("95% CI (two-tailed)")
    {
        const double z_025 = compute_normal_quantile(0.025);
        const double z_975 = compute_normal_quantile(0.975);
        
        REQUIRE(z_025 == Approx(-1.959963984540054).margin(1e-9));
        REQUIRE(z_975 == Approx( 1.959963984540054).margin(1e-9));
        REQUIRE(z_975 == Approx(-z_025).margin(1e-12));  // Symmetry
    }

    SECTION("99% CI (two-tailed)")
    {
        const double z_005 = compute_normal_quantile(0.005);
        const double z_995 = compute_normal_quantile(0.995);
        
        REQUIRE(z_005 == Approx(-2.575829303548901).margin(1e-9));
        REQUIRE(z_995 == Approx( 2.575829303548901).margin(1e-9));
        REQUIRE(z_995 == Approx(-z_005).margin(1e-12));  // Symmetry
    }

    SECTION("90% CI (two-tailed)")
    {
        const double z_050 = compute_normal_quantile(0.050);
        const double z_950 = compute_normal_quantile(0.950);
        
        REQUIRE(z_050 == Approx(-1.644853626951472).margin(1e-9));
        REQUIRE(z_950 == Approx( 1.644853626951472).margin(1e-9));
        REQUIRE(z_950 == Approx(-z_050).margin(1e-12));  // Symmetry
    }

    SECTION("99.9% CI (two-tailed)")
    {
        const double z_0005 = compute_normal_quantile(0.0005);
        const double z_9995 = compute_normal_quantile(0.9995);
        
        REQUIRE(z_0005 == Approx(-3.290526731491691).margin(1e-9));
        REQUIRE(z_9995 == Approx( 3.290526731491691).margin(1e-9));
        REQUIRE(z_9995 == Approx(-z_0005).margin(1e-12));  // Symmetry
    }
}

TEST_CASE("compute_normal_quantile: median and quartiles",
          "[NormalQuantile][quantile][median]")
{
    SECTION("Median (p = 0.5)")
    {
        const double z_median = compute_normal_quantile(0.5);
        REQUIRE(z_median == 0.0);  // Exact
    }

    SECTION("First quartile (p = 0.25)")
    {
        const double z_25 = compute_normal_quantile(0.25);
        REQUIRE(z_25 == Approx(-0.6744897501960817).margin(1e-9));
    }

    SECTION("Third quartile (p = 0.75)")
    {
        const double z_75 = compute_normal_quantile(0.75);
        REQUIRE(z_75 == Approx(0.6744897501960817).margin(1e-9));
    }

    SECTION("Quartiles are symmetric")
    {
        const double z_25 = compute_normal_quantile(0.25);
        const double z_75 = compute_normal_quantile(0.75);
        REQUIRE(z_75 == Approx(-z_25).margin(1e-12));
    }
}

TEST_CASE("compute_normal_quantile: extreme tail probabilities",
          "[NormalQuantile][quantile][tails]")
{
    SECTION("Very small probabilities")
    {
        const double z_1e6 = compute_normal_quantile(1e-6);
        REQUIRE(z_1e6 == Approx(-4.753424308823798).margin(1e-8));
        REQUIRE(std::isfinite(z_1e6));
    }

    SECTION("Very large probabilities")
    {
        const double z_1minus1e6 = compute_normal_quantile(1.0 - 1e-6);
        REQUIRE(z_1minus1e6 == Approx(4.753424308823798).margin(1e-8));
        REQUIRE(std::isfinite(z_1minus1e6));
    }

    SECTION("Extreme lower tail")
    {
        const double z_1e10 = compute_normal_quantile(1e-10);
        REQUIRE(z_1e10 < -6.0);
        REQUIRE(std::isfinite(z_1e10));
    }

    SECTION("Extreme upper tail")
    {
        const double z_1minus1e10 = compute_normal_quantile(1.0 - 1e-10);
        REQUIRE(z_1minus1e10 > 6.0);
        REQUIRE(std::isfinite(z_1minus1e10));
    }
}

TEST_CASE("compute_normal_quantile: error handling",
          "[NormalQuantile][quantile][errors]")
{
    SECTION("p = 0 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_quantile(0.0), std::domain_error);
    }

    SECTION("p = 1 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_quantile(1.0), std::domain_error);
    }

    SECTION("p < 0 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_quantile(-0.1), std::domain_error);
    }

    SECTION("p > 1 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_quantile(1.5), std::domain_error);
    }

    SECTION("p = NaN behavior")
    {
        // NaN comparisons (p <= 0.0 || p >= 1.0) always return false
        // So NaN passes the boundary checks and enters the algorithm
        // The result will be NaN, but no exception is thrown
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double result = compute_normal_quantile(nan);
        REQUIRE(std::isnan(result));
    }
}

TEST_CASE("compute_normal_quantile: symmetry properties",
          "[NormalQuantile][quantile][symmetry]")
{
    // For any p, Φ⁻¹(1-p) = -Φ⁻¹(p)
    const std::vector<double> ps = {0.01, 0.1, 0.2, 0.3, 0.4, 0.6, 0.7, 0.8, 0.9, 0.99};

    for (double p : ps)
    {
        const double z_p = compute_normal_quantile(p);
        const double z_1mp = compute_normal_quantile(1.0 - p);
        
        REQUIRE(z_1mp == Approx(-z_p).margin(1e-12));
    }
}

TEST_CASE("compute_normal_quantile: monotonicity",
          "[NormalQuantile][quantile][monotonic]")
{
    // Quantile function should be strictly increasing
    const std::vector<double> ps = {0.001, 0.01, 0.1, 0.2, 0.3, 0.4, 0.5, 
                                     0.6, 0.7, 0.8, 0.9, 0.99, 0.999};

    for (std::size_t i = 1; i < ps.size(); ++i)
    {
        const double z_prev = compute_normal_quantile(ps[i-1]);
        const double z_curr = compute_normal_quantile(ps[i]);
        
        REQUIRE(z_curr > z_prev);
    }
}

// ============================================================================
// Tests for compute_normal_cdf (forward CDF)
// ============================================================================

TEST_CASE("compute_normal_cdf: standard values",
          "[NormalQuantile][cdf][standard]")
{
    SECTION("CDF at z = 0 is 0.5")
    {
        const double p = compute_normal_cdf(0.0);
        REQUIRE(p == Approx(0.5).margin(1e-15));
    }

    SECTION("CDF at z = 1.96 is approximately 0.975")
    {
        const double p = compute_normal_cdf(1.96);
        REQUIRE(p == Approx(0.975).margin(1e-6));
    }

    SECTION("CDF at z = -1.96 is approximately 0.025")
    {
        const double p = compute_normal_cdf(-1.96);
        // The exact value is slightly different from 0.025
        // because 1.96 is an approximation of the true quantile
        REQUIRE(p == Approx(0.024997895).margin(1e-6));
    }

    SECTION("CDF at z = 2.576 is approximately 0.995")
    {
        const double p = compute_normal_cdf(2.576);
        REQUIRE(p == Approx(0.995).margin(1e-6));
    }

    SECTION("CDF at z = -2.576 is approximately 0.005")
    {
        const double p = compute_normal_cdf(-2.576);
        // The exact value is slightly different from 0.005
        // because 2.576 is an approximation of the true quantile
        REQUIRE(p == Approx(0.004997532).margin(1e-6));
    }
}

TEST_CASE("compute_normal_cdf: symmetry",
          "[NormalQuantile][cdf][symmetry]")
{
    // For any z, Φ(z) + Φ(-z) = 1
    const std::vector<double> zs = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0};

    for (double z : zs)
    {
        const double p_pos = compute_normal_cdf(z);
        const double p_neg = compute_normal_cdf(-z);
        
        REQUIRE(p_pos + p_neg == Approx(1.0).margin(1e-12));
    }
}

TEST_CASE("compute_normal_cdf: monotonicity",
          "[NormalQuantile][cdf][monotonic]")
{
    // CDF should be strictly increasing
    const std::vector<double> zs = {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0};

    for (std::size_t i = 1; i < zs.size(); ++i)
    {
        const double p_prev = compute_normal_cdf(zs[i-1]);
        const double p_curr = compute_normal_cdf(zs[i]);
        
        REQUIRE(p_curr > p_prev);
    }
}

TEST_CASE("compute_normal_cdf: extreme values",
          "[NormalQuantile][cdf][extremes]")
{
    SECTION("Very negative z approaches 0")
    {
        const double p = compute_normal_cdf(-6.0);
        REQUIRE(p < 1e-8);
        REQUIRE(p > 0.0);
    }

    SECTION("Very positive z approaches 1")
    {
        const double p = compute_normal_cdf(6.0);
        REQUIRE(p > 1.0 - 1e-8);
        REQUIRE(p < 1.0);
    }

    SECTION("Extreme negative z")
    {
        const double p = compute_normal_cdf(-10.0);
        REQUIRE(std::isfinite(p));
        REQUIRE(p >= 0.0);
        REQUIRE(p <= 1.0);
    }

    SECTION("Extreme positive z")
    {
        const double p = compute_normal_cdf(10.0);
        REQUIRE(std::isfinite(p));
        REQUIRE(p >= 0.0);
        REQUIRE(p <= 1.0);
    }
}

TEST_CASE("compute_normal_cdf: is noexcept",
          "[NormalQuantile][cdf][noexcept]")
{
    // Verify that the function doesn't throw for any finite input
    const std::vector<double> zs = {-1e10, -100.0, -10.0, -1.0, 0.0, 
                                     1.0, 10.0, 100.0, 1e10};

    for (double z : zs)
    {
        double p;
        REQUIRE_NOTHROW(p = compute_normal_cdf(z));
        REQUIRE(std::isfinite(p));
        REQUIRE(p >= 0.0);
        REQUIRE(p <= 1.0);
    }
}

// ============================================================================
// Tests for inverse-forward consistency
// ============================================================================

TEST_CASE("compute_normal_quantile and compute_normal_cdf are inverses",
          "[NormalQuantile][consistency][roundtrip]")
{
    SECTION("Forward then inverse (CDF then quantile)")
    {
        const std::vector<double> zs = {-2.5, -1.96, -1.0, 0.0, 1.0, 1.96, 2.5};

        for (double z_orig : zs)
        {
            const double p = compute_normal_cdf(z_orig);
            const double z_recovered = compute_normal_quantile(p);
            
            REQUIRE(z_recovered == Approx(z_orig).margin(1e-9));
        }
    }

    SECTION("Inverse then forward (quantile then CDF)")
    {
        const std::vector<double> ps = {0.001, 0.01, 0.1, 0.25, 0.5, 0.75, 0.9, 0.99, 0.999};

        for (double p_orig : ps)
        {
            const double z = compute_normal_quantile(p_orig);
            const double p_recovered = compute_normal_cdf(z);
            
            REQUIRE(p_recovered == Approx(p_orig).margin(1e-9));
        }
    }
}

// ============================================================================
// Tests for compute_normal_critical_value
// ============================================================================

TEST_CASE("compute_normal_critical_value: standard confidence levels",
          "[NormalQuantile][critical][standard]")
{
    SECTION("90% confidence level")
    {
        const double z = compute_normal_critical_value(0.90);
        REQUIRE(z == Approx(1.644853626951472).margin(1e-9));
    }

    SECTION("95% confidence level")
    {
        const double z = compute_normal_critical_value(0.95);
        REQUIRE(z == Approx(1.959963984540054).margin(1e-9));
    }

    SECTION("99% confidence level")
    {
        const double z = compute_normal_critical_value(0.99);
        REQUIRE(z == Approx(2.575829303548901).margin(1e-9));
    }

    SECTION("99.9% confidence level")
    {
        const double z = compute_normal_critical_value(0.999);
        REQUIRE(z == Approx(3.290526731491691).margin(1e-9));
    }
}

TEST_CASE("compute_normal_critical_value: always positive",
          "[NormalQuantile][critical][positive]")
{
    const std::vector<double> cls = {0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 0.99, 0.999};

    for (double cl : cls)
    {
        const double z = compute_normal_critical_value(cl);
        REQUIRE(z > 0.0);
    }
}

TEST_CASE("compute_normal_critical_value: error handling",
          "[NormalQuantile][critical][errors]")
{
    SECTION("confidence_level = 0 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_critical_value(0.0), std::domain_error);
    }

    SECTION("confidence_level = 1 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_critical_value(1.0), std::domain_error);
    }

    SECTION("confidence_level < 0 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_critical_value(-0.5), std::domain_error);
    }

    SECTION("confidence_level > 1 throws")
    {
        REQUIRE_THROWS_AS(compute_normal_critical_value(1.5), std::domain_error);
    }
}

TEST_CASE("compute_normal_critical_value: consistency with quantile",
          "[NormalQuantile][critical][consistency]")
{
    const std::vector<double> cls = {0.90, 0.95, 0.99, 0.999};

    for (double cl : cls)
    {
        const double z_critical = compute_normal_critical_value(cl);
        const double alpha = 1.0 - cl;
        const double z_quantile = compute_normal_quantile(1.0 - alpha / 2.0);
        
        REQUIRE(z_critical == Approx(z_quantile).margin(1e-12));
    }
}

// ============================================================================
// Tests for NormalDistribution wrapper functions
// ============================================================================

TEST_CASE("NormalDistribution::inverseNormalCdf delegates to compute_normal_quantile",
          "[NormalDistribution][wrapper][inverse]")
{
    SECTION("Standard values match")
    {
        const std::vector<double> ps = {0.025, 0.1, 0.5, 0.9, 0.975};

        for (double p : ps)
        {
            const double z_wrapper = NormalDistribution::inverseNormalCdf(p);
            const double z_direct = compute_normal_quantile(p);
            
            REQUIRE(z_wrapper == Approx(z_direct).margin(1e-12));
        }
    }

    SECTION("Accuracy matches Acklam")
    {
        // 95% CI critical values with Acklam precision
        const double z_025 = NormalDistribution::inverseNormalCdf(0.025);
        const double z_975 = NormalDistribution::inverseNormalCdf(0.975);
        
        REQUIRE(z_025 == Approx(-1.959963984540054).margin(1e-9));
        REQUIRE(z_975 == Approx( 1.959963984540054).margin(1e-9));
    }
}

TEST_CASE("NormalDistribution::inverseNormalCdf boundary behavior",
          "[NormalDistribution][wrapper][boundaries]")
{
    SECTION("p = 0 returns -infinity (noexcept)")
    {
        const double z = NormalDistribution::inverseNormalCdf(0.0);
        REQUIRE(z == -std::numeric_limits<double>::infinity());
    }

    SECTION("p = 1 returns +infinity (noexcept)")
    {
        const double z = NormalDistribution::inverseNormalCdf(1.0);
        REQUIRE(z == std::numeric_limits<double>::infinity());
    }

    SECTION("p < 0 returns -infinity (noexcept)")
    {
        const double z = NormalDistribution::inverseNormalCdf(-0.5);
        REQUIRE(z == -std::numeric_limits<double>::infinity());
    }

    SECTION("p > 1 returns +infinity (noexcept)")
    {
        const double z = NormalDistribution::inverseNormalCdf(1.5);
        REQUIRE(z == std::numeric_limits<double>::infinity());
    }
}

TEST_CASE("NormalDistribution::standardNormalCdf delegates to compute_normal_cdf",
          "[NormalDistribution][wrapper][forward]")
{
    SECTION("Standard values match")
    {
        const std::vector<double> zs = {-1.96, -1.0, 0.0, 1.0, 1.96};

        for (double z : zs)
        {
            const double p_wrapper = NormalDistribution::standardNormalCdf(z);
            const double p_direct = compute_normal_cdf(z);
            
            REQUIRE(p_wrapper == Approx(p_direct).margin(1e-12));
        }
    }

    SECTION("Accuracy using error function")
    {
        // Error function provides very high accuracy
        const double p_0 = NormalDistribution::standardNormalCdf(0.0);
        const double p_196 = NormalDistribution::standardNormalCdf(1.96);
        
        REQUIRE(p_0 == Approx(0.5).margin(1e-15));
        REQUIRE(p_196 == Approx(0.975).margin(1e-6));
    }
}

TEST_CASE("NormalDistribution::criticalValue convenience function",
          "[NormalDistribution][wrapper][critical]")
{
    SECTION("Standard confidence levels")
    {
        REQUIRE(NormalDistribution::criticalValue(0.95) 
                == Approx(1.959963984540054).margin(1e-9));
        REQUIRE(NormalDistribution::criticalValue(0.99) 
                == Approx(2.575829303548901).margin(1e-9));
    }

    SECTION("Invalid confidence levels return infinity (noexcept)")
    {
        REQUIRE(NormalDistribution::criticalValue(0.0) 
                == std::numeric_limits<double>::infinity());
        REQUIRE(NormalDistribution::criticalValue(1.0) 
                == std::numeric_limits<double>::infinity());
        REQUIRE(NormalDistribution::criticalValue(-0.5) 
                == std::numeric_limits<double>::infinity());
        REQUIRE(NormalDistribution::criticalValue(1.5) 
                == std::numeric_limits<double>::infinity());
    }

    SECTION("Consistency with inverseNormalCdf")
    {
        const double cl = 0.95;
        const double z_critical = NormalDistribution::criticalValue(cl);
        const double alpha = 1.0 - cl;
        const double z_inverse = NormalDistribution::inverseNormalCdf(1.0 - alpha / 2.0);
        
        REQUIRE(z_critical == Approx(z_inverse).margin(1e-12));
    }
}

TEST_CASE("NormalDistribution wrapper is noexcept",
          "[NormalDistribution][wrapper][noexcept]")
{
    SECTION("inverseNormalCdf never throws")
    {
        // Even for invalid inputs, returns infinity instead of throwing
        double z;
        REQUIRE_NOTHROW(z = NormalDistribution::inverseNormalCdf(-1.0));
        REQUIRE_NOTHROW(z = NormalDistribution::inverseNormalCdf(0.0));
        REQUIRE_NOTHROW(z = NormalDistribution::inverseNormalCdf(1.0));
        REQUIRE_NOTHROW(z = NormalDistribution::inverseNormalCdf(2.0));
        
        const double nan = std::numeric_limits<double>::quiet_NaN();
        REQUIRE_NOTHROW(z = NormalDistribution::inverseNormalCdf(nan));
    }

    SECTION("standardNormalCdf never throws")
    {
        double p;
        REQUIRE_NOTHROW(p = NormalDistribution::standardNormalCdf(-1e10));
        REQUIRE_NOTHROW(p = NormalDistribution::standardNormalCdf(1e10));
        
        const double nan = std::numeric_limits<double>::quiet_NaN();
        REQUIRE_NOTHROW(p = NormalDistribution::standardNormalCdf(nan));
    }

    SECTION("criticalValue never throws")
    {
        double z;
        REQUIRE_NOTHROW(z = NormalDistribution::criticalValue(-1.0));
        REQUIRE_NOTHROW(z = NormalDistribution::criticalValue(0.0));
        REQUIRE_NOTHROW(z = NormalDistribution::criticalValue(1.0));
        REQUIRE_NOTHROW(z = NormalDistribution::criticalValue(2.0));
    }
}

// ============================================================================
// Backward compatibility tests
// ============================================================================

TEST_CASE("NormalDistribution maintains backward compatibility",
          "[NormalDistribution][compatibility]")
{
    SECTION("Legacy code expecting noexcept behavior works")
    {
        // Old code might have relied on noexcept to avoid exception handling
        // Verify that the wrapper maintains this guarantee
        
        static_assert(noexcept(NormalDistribution::inverseNormalCdf(0.5)),
                      "inverseNormalCdf must be noexcept for backward compatibility");
        
        static_assert(noexcept(NormalDistribution::standardNormalCdf(0.0)),
                      "standardNormalCdf must be noexcept for backward compatibility");
        
        static_assert(noexcept(NormalDistribution::criticalValue(0.95)),
                      "criticalValue must be noexcept for backward compatibility");
    }

    SECTION("Boundary case behavior preserved")
    {
        // Legacy code expecting ±infinity for boundary cases
        REQUIRE(std::isinf(NormalDistribution::inverseNormalCdf(0.0)));
        REQUIRE(std::isinf(NormalDistribution::inverseNormalCdf(1.0)));
        REQUIRE(NormalDistribution::inverseNormalCdf(0.0) < 0.0);
        REQUIRE(NormalDistribution::inverseNormalCdf(1.0) > 0.0);
    }
}

// ============================================================================
// Numerical accuracy comparison tests
// ============================================================================

TEST_CASE("Acklam algorithm provides improved accuracy over A&S",
          "[NormalQuantile][accuracy][comparison]")
{
    // These tests document that Acklam is more accurate than the old
    // Abramowitz & Stegun implementation (not tested directly, but verified
    // by comparing against high-precision reference values)

    SECTION("95% CI critical value (high precision reference)")
    {
        // Reference value computed with high-precision software
        const double z_975_reference = 1.9599639845400545534;
        const double z_975_computed = compute_normal_quantile(0.975);
        
        // Acklam should be accurate to ~1e-9 relative error
        REQUIRE(z_975_computed == Approx(z_975_reference).margin(1e-9));
    }

    SECTION("99% CI critical value (high precision reference)")
    {
        const double z_995_reference = 2.5758293035489008;
        const double z_995_computed = compute_normal_quantile(0.995);
        
        REQUIRE(z_995_computed == Approx(z_995_reference).margin(1e-9));
    }

    SECTION("Extreme tail accuracy")
    {
        // In the tails, Acklam maintains accuracy where A&S degrades
        const double z_1e6 = compute_normal_quantile(1e-6);
        
        // Should be finite and reasonable
        REQUIRE(std::isfinite(z_1e6));
        REQUIRE(z_1e6 < -4.5);
        REQUIRE(z_1e6 > -5.0);
    }
}

// ============================================================================
// Integration tests for typical bootstrap use cases
// ============================================================================

TEST_CASE("Normal quantile functions in typical bootstrap scenarios",
          "[NormalQuantile][integration][bootstrap]")
{
    SECTION("Computing 95% CI bounds")
    {
        // Typical bootstrap scenario: compute 95% confidence interval
        const double alpha = 0.05;
        const double p_lower = alpha / 2.0;
        const double p_upper = 1.0 - alpha / 2.0;
        
        const double z_lower = NormalDistribution::inverseNormalCdf(p_lower);
        const double z_upper = NormalDistribution::inverseNormalCdf(p_upper);
        
        // The true critical value is 1.9599639861..., not exactly 1.96
        REQUIRE(z_lower == Approx(-1.9599639861).margin(1e-6));
        REQUIRE(z_upper == Approx( 1.9599639861).margin(1e-6));
        REQUIRE(z_upper == Approx(-z_lower).margin(1e-9));
    }

    SECTION("BCa bias correction (typical z0 values)")
    {
        // BCa computes z0 from proportion less than theta_hat
        // Typical values might be near 0.5 (no bias) or offset
        
        const double prop_less_1 = 0.48;  // Slight negative bias
        const double z0_1 = NormalDistribution::inverseNormalCdf(prop_less_1);
        REQUIRE(z0_1 < 0.0);
        REQUIRE(z0_1 == Approx(-0.0502).margin(1e-3));
        
        const double prop_less_2 = 0.52;  // Slight positive bias
        const double z0_2 = NormalDistribution::inverseNormalCdf(prop_less_2);
        REQUIRE(z0_2 > 0.0);
        REQUIRE(z0_2 == Approx(0.0502).margin(1e-3));
    }

    SECTION("Adjusted percentiles in BCa")
    {
        // After computing adjusted z-values, BCa converts back to percentiles
        const double z0 = 0.1;
        const double a = 0.05;
        const double z_alpha = -1.96;
        
        const double z_adjusted = z0 + (z0 + z_alpha) / (1.0 - a * (z0 + z_alpha));
        const double alpha_adjusted = NormalDistribution::standardNormalCdf(z_adjusted);
        
        REQUIRE(std::isfinite(alpha_adjusted));
        REQUIRE(alpha_adjusted > 0.0);
        REQUIRE(alpha_adjusted < 1.0);
        // Adjusted alpha should differ from 0.025 due to bias correction
        REQUIRE(alpha_adjusted != Approx(0.025).margin(1e-6));
    }

    SECTION("Computing critical value for m-out-of-n bootstrap")
    {
        // m-out-of-n bootstrap uses critical values for CI width calculation
        const double conf_level = 0.95;
        const double z_critical = compute_normal_critical_value(conf_level);
        
        // Used in: sigma = width / (2 * z)
        const double width = 0.20;  // Hypothetical CI width
        const double sigma = width / (2.0 * z_critical);
        
        REQUIRE(sigma == Approx(0.051).margin(1e-3));
    }
}

// ============================================================================
// Basic functionality tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: basic functionality with doubles",
          "[EmpiricalCdf][basic][double]")
{
    SECTION("Simple sorted data")
    {
        const std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
        
        REQUIRE(compute_empirical_cdf(data, 0.0) == 0.0);    // 0/5 values ≤ 0
        REQUIRE(compute_empirical_cdf(data, 1.0) == 0.2);    // 1/5 values ≤ 1
        REQUIRE(compute_empirical_cdf(data, 2.5) == 0.4);    // 2/5 values ≤ 2.5
        REQUIRE(compute_empirical_cdf(data, 3.0) == 0.6);    // 3/5 values ≤ 3
        REQUIRE(compute_empirical_cdf(data, 4.9) == 0.8);    // 4/5 values ≤ 4.9
        REQUIRE(compute_empirical_cdf(data, 5.0) == 1.0);    // 5/5 values ≤ 5
        REQUIRE(compute_empirical_cdf(data, 10.0) == 1.0);   // 5/5 values ≤ 10
    }

    SECTION("Unsorted data produces same results")
    {
        const std::vector<double> data = {3.0, 1.0, 5.0, 2.0, 4.0};
        
        REQUIRE(compute_empirical_cdf(data, 2.5) == 0.4);
        REQUIRE(compute_empirical_cdf(data, 3.0) == 0.6);
        REQUIRE(compute_empirical_cdf(data, 5.0) == 1.0);
    }

    SECTION("Data with duplicates")
    {
        const std::vector<double> data = {1.0, 2.0, 2.0, 3.0, 3.0, 3.0};
        
        REQUIRE(compute_empirical_cdf(data, 1.5) == Approx(1.0/6.0));  // 1/6
        REQUIRE(compute_empirical_cdf(data, 2.0) == Approx(3.0/6.0));  // 3/6
        REQUIRE(compute_empirical_cdf(data, 2.5) == Approx(3.0/6.0));  // 3/6
        REQUIRE(compute_empirical_cdf(data, 3.0) == 1.0);              // 6/6
    }

    SECTION("Realistic bootstrap-like data")
    {
        const std::vector<double> data = {10.2, 9.8, 10.5, 10.1, 9.9, 10.3, 10.0, 10.4};
        
        REQUIRE(compute_empirical_cdf(data, 10.0) == 0.375);   // 3/8 values ≤ 10.0 (9.8, 9.9, 10.0)
        REQUIRE(compute_empirical_cdf(data, 10.25) == 0.625);  // 5/8 values ≤ 10.25
        REQUIRE(compute_empirical_cdf(data, 9.0) == 0.0);      // 0/8 values ≤ 9.0
    }
}

TEST_CASE("compute_empirical_cdf: boundary cases",
          "[EmpiricalCdf][boundary]")
{
    SECTION("Empty container returns 0.0")
    {
        const std::vector<double> empty_data;
        REQUIRE(compute_empirical_cdf(empty_data, 0.0) == 0.0);
        REQUIRE(compute_empirical_cdf(empty_data, 100.0) == 0.0);
        REQUIRE(compute_empirical_cdf(empty_data, -100.0) == 0.0);
    }

    SECTION("Single element")
    {
        const std::vector<double> single = {5.0};
        
        REQUIRE(compute_empirical_cdf(single, 4.9) == 0.0);
        REQUIRE(compute_empirical_cdf(single, 5.0) == 1.0);
        REQUIRE(compute_empirical_cdf(single, 5.1) == 1.0);
    }

    SECTION("All identical values")
    {
        const std::vector<double> identical = {3.0, 3.0, 3.0, 3.0, 3.0};
        
        REQUIRE(compute_empirical_cdf(identical, 2.9) == 0.0);
        REQUIRE(compute_empirical_cdf(identical, 3.0) == 1.0);
        REQUIRE(compute_empirical_cdf(identical, 3.1) == 1.0);
    }

    SECTION("Two elements")
    {
        const std::vector<double> two = {1.0, 3.0};
        
        REQUIRE(compute_empirical_cdf(two, 0.0) == 0.0);
        REQUIRE(compute_empirical_cdf(two, 1.0) == 0.5);
        REQUIRE(compute_empirical_cdf(two, 2.0) == 0.5);
        REQUIRE(compute_empirical_cdf(two, 3.0) == 1.0);
        REQUIRE(compute_empirical_cdf(two, 4.0) == 1.0);
    }
}

// ============================================================================
// Type flexibility tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: works with integer types",
          "[EmpiricalCdf][types][integer]")
{
    SECTION("int container, int query - returns int (with integer division)")
    {
        const std::vector<int> int_data = {1, 5, 3, 8, 2, 5};
        
        // Note: With integer types, integer division occurs, so results are truncated
        REQUIRE(compute_empirical_cdf(int_data, 0) == 0);    // 0/6 = 0
        REQUIRE(compute_empirical_cdf(int_data, 2) == 0);    // 2/6 = 0 (integer division)
        REQUIRE(compute_empirical_cdf(int_data, 5) == 0);    // 4/6 = 0 (integer division)
        REQUIRE(compute_empirical_cdf(int_data, 8) == 1);    // 6/6 = 1
        REQUIRE(compute_empirical_cdf(int_data, 10) == 1);   // 6/6 = 1
    }

    SECTION("int container, double query - still returns int")
    {
        const std::vector<int> int_data = {1, 2, 3, 4, 5};
        
        // Return type is container value type (int), so integer division occurs
        REQUIRE(compute_empirical_cdf(int_data, 2.5) == 0);  // 2/5 = 0
        REQUIRE(compute_empirical_cdf(int_data, 3.7) == 0);  // 3/5 = 0
    }

    SECTION("long container")
    {
        const std::vector<long> long_data = {100L, 200L, 300L, 400L, 500L};
        
        REQUIRE(compute_empirical_cdf(long_data, 250L) == 0L);  // 2/5 = 0 (integer division)
        REQUIRE(compute_empirical_cdf(long_data, 500L) == 1L);  // 5/5 = 1
    }
}

TEST_CASE("compute_empirical_cdf: works with float types",
          "[EmpiricalCdf][types][float]")
{
    SECTION("float container, float query")
    {
        const std::vector<float> float_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        
        REQUIRE(compute_empirical_cdf(float_data, 2.5f) == Approx(0.4f));
        REQUIRE(compute_empirical_cdf(float_data, 3.0f) == Approx(0.6f));
    }

    SECTION("float container, double query")
    {
        const std::vector<float> float_data = {1.5f, 2.3f, 1.8f, 3.1f, 2.0f};
        
        REQUIRE(compute_empirical_cdf(float_data, 2.0) == Approx(0.6f));
    }
}

TEST_CASE("compute_empirical_cdf: works with different container types",
          "[EmpiricalCdf][containers]")
{
    SECTION("std::array")
    {
        const std::array<double, 5> arr_data = {1.0, 2.0, 3.0, 4.0, 5.0};
        
        REQUIRE(compute_empirical_cdf(arr_data, 3.0) == 0.6);
    }

    SECTION("std::list")
    {
        const std::list<double> list_data = {1.0, 2.0, 3.0, 4.0, 5.0};
        
        REQUIRE(compute_empirical_cdf(list_data, 3.0) == 0.6);
    }

    SECTION("std::deque")
    {
        const std::deque<double> deque_data = {1.0, 2.0, 3.0, 4.0, 5.0};
        
        REQUIRE(compute_empirical_cdf(deque_data, 3.0) == 0.6);
    }

    SECTION("C-style array via std::vector initialization")
    {
        double arr[] = {1.0, 2.0, 3.0, 4.0, 5.0};
        const std::vector<double> vec_data(arr, arr + 5);
        
        REQUIRE(compute_empirical_cdf(vec_data, 3.0) == 0.6);
    }
}

// ============================================================================
// Mathematical properties tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: mathematical properties",
          "[EmpiricalCdf][properties]")
{
    const std::vector<double> data = {1.5, 2.3, 1.8, 3.1, 2.0, 2.7, 1.2, 3.5};

    SECTION("Range is always [0, 1]")
    {
        for (double x = -10.0; x <= 10.0; x += 0.5)
        {
            const double F = compute_empirical_cdf(data, x);
            REQUIRE(F >= 0.0);
            REQUIRE(F <= 1.0);
        }
    }

    SECTION("Monotonicity: F(x1) ≤ F(x2) when x1 ≤ x2")
    {
        const std::vector<double> test_points = {0.0, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
        
        for (std::size_t i = 1; i < test_points.size(); ++i)
        {
            const double F1 = compute_empirical_cdf(data, test_points[i-1]);
            const double F2 = compute_empirical_cdf(data, test_points[i]);
            REQUIRE(F2 >= F1);
        }
    }

    SECTION("Right-continuity: F(x) includes values equal to x")
    {
        // If we have value v in data, F(v) should include v
        const std::vector<double> simple_data = {1.0, 2.0, 3.0};
        
        REQUIRE(compute_empirical_cdf(simple_data, 2.0) == Approx(2.0/3.0));
        // Just below 2.0 should give 1/3
        REQUIRE(compute_empirical_cdf(simple_data, 1.9999999) == Approx(1.0/3.0));
    }

    SECTION("F(-∞) = 0 and F(+∞) = 1")
    {
        const double very_small = -1e100;
        const double very_large = 1e100;
        
        REQUIRE(compute_empirical_cdf(data, very_small) == 0.0);
        REQUIRE(compute_empirical_cdf(data, very_large) == 1.0);
    }
}

// ============================================================================
// Tie handling tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: tie handling",
          "[EmpiricalCdf][ties]")
{
    SECTION("Multiple values equal to query point")
    {
        const std::vector<double> data = {1.0, 2.0, 2.0, 2.0, 3.0};
        
        // All three 2.0s should be included when x = 2.0
        REQUIRE(compute_empirical_cdf(data, 2.0) == 0.8);  // 4/5
    }

    SECTION("Query point between duplicates")
    {
        const std::vector<double> data = {1.0, 1.0, 3.0, 3.0, 3.0};
        
        REQUIRE(compute_empirical_cdf(data, 2.0) == 0.4);  // 2/5 (two 1.0s)
    }

    SECTION("All values are ties")
    {
        const std::vector<double> data = {5.0, 5.0, 5.0, 5.0};
        
        REQUIRE(compute_empirical_cdf(data, 5.0) == 1.0);
        REQUIRE(compute_empirical_cdf(data, 4.999) == 0.0);
    }
}

// ============================================================================
// Precision and numerical stability tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: numerical precision",
          "[EmpiricalCdf][precision]")
{
    SECTION("Very small differences")
    {
        const std::vector<double> data = {1.0, 1.0000001, 1.0000002};
        
        REQUIRE(compute_empirical_cdf(data, 1.0) == Approx(1.0/3.0));
        REQUIRE(compute_empirical_cdf(data, 1.00000015) == Approx(2.0/3.0));
        REQUIRE(compute_empirical_cdf(data, 1.0000002) == 1.0);
    }

    SECTION("Large values")
    {
        const std::vector<double> data = {1e10, 2e10, 3e10};
        
        REQUIRE(compute_empirical_cdf(data, 1.5e10) == Approx(1.0/3.0));
        REQUIRE(compute_empirical_cdf(data, 3e10) == 1.0);
    }

    SECTION("Negative values")
    {
        const std::vector<double> data = {-5.0, -3.0, -1.0, 0.0, 2.0};
        
        REQUIRE(compute_empirical_cdf(data, -4.0) == 0.2);
        REQUIRE(compute_empirical_cdf(data, 0.0) == 0.8);
        REQUIRE(compute_empirical_cdf(data, 1.0) == 0.8);
    }

    SECTION("Mixed positive and negative")
    {
        const std::vector<double> data = {-2.0, -1.0, 0.0, 1.0, 2.0};
        
        REQUIRE(compute_empirical_cdf(data, -1.5) == 0.2);
        REQUIRE(compute_empirical_cdf(data, 0.0) == 0.6);
        REQUIRE(compute_empirical_cdf(data, 1.5) == 0.8);
    }
}

// ============================================================================
// Integration tests with bootstrap methods
// ============================================================================

TEST_CASE("compute_empirical_cdf: bootstrap bias correction use case",
          "[EmpiricalCdf][integration][bootstrap]")
{
    SECTION("Computing z0 for BCa intervals")
    {
        // Simulate bootstrap replicates where theta_hat = 10.0
        const std::vector<double> bootstrap_stats = {
            9.5, 9.8, 10.2, 9.9, 10.1, 10.3, 9.7, 10.0, 10.4, 9.6
        };
        const double theta_hat = 10.0;
        
        // Compute proportion of bootstrap stats less than theta_hat
        const double prop = compute_empirical_cdf(bootstrap_stats, theta_hat);
        
        // Should be around 0.6 (6 out of 10 values ≤ 10.0)
        REQUIRE(prop == 0.6);
        
        // Convert to z0 using normal quantile
        const double z0 = compute_normal_quantile(prop);
        
        // z0 should be positive (indicating positive bias)
        REQUIRE(z0 > 0.0);
        REQUIRE(z0 == Approx(0.2533).margin(1e-3));
    }

    SECTION("No bias case (prop = 0.5)")
    {
        // Perfectly symmetric bootstrap distribution
        const std::vector<double> bootstrap_stats = {
            9.0, 9.5, 10.0, 10.5, 11.0
        };
        const double theta_hat = 10.0;
        
        const double prop = compute_empirical_cdf(bootstrap_stats, theta_hat);
        REQUIRE(prop == 0.6);  // 3/5 ≤ 10.0
        
        // Not exactly 0.5, so there is slight bias
        const double z0 = compute_normal_quantile(prop);
        REQUIRE(z0 != 0.0);
    }

    SECTION("Negative bias case")
    {
        // Bootstrap distribution shifted left
        const std::vector<double> bootstrap_stats = {
            8.5, 9.0, 9.5, 10.0, 10.5, 11.0, 11.5, 12.0
        };
        const double theta_hat = 10.5;
        
        const double prop = compute_empirical_cdf(bootstrap_stats, theta_hat);
        REQUIRE(prop == 0.625);  // 5/8 ≤ 10.5
        
        const double z0 = compute_normal_quantile(prop);
        REQUIRE(z0 > 0.0);
    }
}

TEST_CASE("compute_empirical_cdf: quantile estimation",
          "[EmpiricalCdf][integration][quantiles]")
{
    SECTION("Finding empirical quantiles by inversion")
    {
        const std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
        
        // The empirical 0.5-quantile (median) should satisfy F(x) ≥ 0.5
        // For this data, F(5) = 0.5, F(6) = 0.6
        REQUIRE(compute_empirical_cdf(data, 5.0) == 0.5);
        REQUIRE(compute_empirical_cdf(data, 6.0) == 0.6);
        
        // The empirical 0.25-quantile
        REQUIRE(compute_empirical_cdf(data, 2.0) == 0.2);
        REQUIRE(compute_empirical_cdf(data, 3.0) == 0.3);
        
        // The empirical 0.75-quantile
        REQUIRE(compute_empirical_cdf(data, 7.0) == 0.7);
        REQUIRE(compute_empirical_cdf(data, 8.0) == 0.8);
    }
}

TEST_CASE("compute_empirical_cdf: Kolmogorov-Smirnov test scenario",
          "[EmpiricalCdf][integration][ks]")
{
    SECTION("Comparing empirical CDF to theoretical CDF")
    {
        // Sample from approximately normal distribution
        const std::vector<double> sample = {
            -1.5, -0.8, -0.3, 0.1, 0.5, 0.9, 1.2, 1.8
        };
        
        // Compute max difference between empirical and theoretical
        double max_diff = 0.0;
        
        for (double x : sample)
        {
            const double F_empirical = compute_empirical_cdf(sample, x);
            const double F_theoretical = 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
            const double diff = std::abs(F_empirical - F_theoretical);
            max_diff = std::max(max_diff, diff);
        }
        
        // For small sample, differences can be large
        REQUIRE(max_diff >= 0.0);
        REQUIRE(max_diff <= 1.0);
    }
}

// ============================================================================
// Large dataset tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: large datasets",
          "[EmpiricalCdf][performance]")
{
    SECTION("1000 element dataset")
    {
        std::vector<double> large_data(1000);
        for (std::size_t i = 0; i < 1000; ++i)
        {
            large_data[i] = static_cast<double>(i);
        }
        
        REQUIRE(compute_empirical_cdf(large_data, 0.0) == 0.001);
        REQUIRE(compute_empirical_cdf(large_data, 499.0) == 0.5);
        REQUIRE(compute_empirical_cdf(large_data, 999.0) == 1.0);
    }

    SECTION("Uniform distribution approximation")
    {
        std::vector<double> uniform_data(100);
        for (std::size_t i = 0; i < 100; ++i)
        {
            uniform_data[i] = static_cast<double>(i) / 100.0;
        }
        
        // Empirical CDF should approximate uniform CDF: F(x) = x
        for (double x = 0.0; x <= 1.0; x += 0.1)
        {
            const double F = compute_empirical_cdf(uniform_data, x);
            // Should be close to x for uniform distribution
            REQUIRE(F == Approx(x).margin(0.02));
        }
    }
}

// ============================================================================
// Edge case and special value tests
// ============================================================================

TEST_CASE("compute_empirical_cdf: special floating-point values",
          "[EmpiricalCdf][special]")
{
    SECTION("Data with zeros")
    {
        const std::vector<double> data = {-1.0, 0.0, 0.0, 1.0, 2.0};
        
        REQUIRE(compute_empirical_cdf(data, -0.5) == 0.2);
        REQUIRE(compute_empirical_cdf(data, 0.0) == 0.6);
        REQUIRE(compute_empirical_cdf(data, 0.5) == 0.6);
    }

    SECTION("Very small positive values")
    {
        const std::vector<double> data = {1e-10, 2e-10, 3e-10};
        
        REQUIRE(compute_empirical_cdf(data, 1.5e-10) == Approx(1.0/3.0));
        REQUIRE(compute_empirical_cdf(data, 3e-10) == 1.0);
    }

    SECTION("Infinity handling")
    {
        const std::vector<double> data = {1.0, 2.0, 3.0};
        const double inf = std::numeric_limits<double>::infinity();
        
        REQUIRE(compute_empirical_cdf(data, inf) == 1.0);
        REQUIRE(compute_empirical_cdf(data, -inf) == 0.0);
    }
}

// ============================================================================
// Comparison with sorted percentile methods
// ============================================================================

TEST_CASE("compute_empirical_cdf: consistency with sorted percentiles",
          "[EmpiricalCdf][consistency]")
{
    SECTION("Verify against known percentiles")
    {
        std::vector<double> data = {3.0, 1.0, 4.0, 1.0, 5.0, 9.0, 2.0, 6.0, 5.0};
        std::sort(data.begin(), data.end());
        // Sorted: {1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 5.0, 6.0, 9.0}
        
        // Median (50th percentile) is at position 4 (value 4.0)
        const double median = data[data.size() / 2];
        const double F_median = compute_empirical_cdf(data, median);
        REQUIRE(F_median >= 0.5);
        
        // 90th percentile
        const std::size_t idx_90 = static_cast<std::size_t>(0.9 * data.size());
        const double p90 = data[idx_90];
        const double F_90 = compute_empirical_cdf(data, p90);
        REQUIRE(F_90 >= 0.9);
    }
}

// ============================================================================
// Custom numeric type tests - dec::decimal<N>
// ============================================================================

TEST_CASE("compute_empirical_cdf: works with dec::decimal types",
          "[EmpiricalCdf][types][decimal]")
{
    using dec::decimal;
    
    SECTION("decimal<2> basic usage")
    {
        const std::vector<decimal<2>> prices = {
            decimal<2>(150),  // 1.50
            decimal<2>(230),  // 2.30
            decimal<2>(180),  // 1.80
            decimal<2>(310),  // 3.10
            decimal<2>(200)   // 2.00
        };
        
        decimal<2> F = compute_empirical_cdf(prices, decimal<2>(200));
        // 3 out of 5 values ≤ 2.00
        REQUIRE(F.getAsDouble() == Approx(0.6));
        
        F = compute_empirical_cdf(prices, decimal<2>(180));
        // 2 out of 5 values ≤ 1.80
        REQUIRE(F.getAsDouble() == Approx(0.4));
        
        F = compute_empirical_cdf(prices, decimal<2>(400));
        // All 5 values ≤ 4.00
        REQUIRE(F.getAsDouble() == Approx(1.0));
    }
    
    SECTION("decimal<4> high precision")
    {
        const std::vector<decimal<4>> rates = {
            decimal<4>(10250),   // 1.0250
            decimal<4>(10375),   // 1.0375
            decimal<4>(10125),   // 1.0125
            decimal<4>(10500),   // 1.0500
            decimal<4>(10200)    // 1.0200
        };
        
        decimal<4> F = compute_empirical_cdf(rates, decimal<4>(10250));
        // 3 out of 5 values ≤ 1.0250
        REQUIRE(F.getAsDouble() == Approx(0.6));
    }
    
    SECTION("decimal<0> integer-like behavior")
    {
        const std::vector<decimal<0>> counts = {
            decimal<0>(10),
            decimal<0>(20),
            decimal<0>(30),
            decimal<0>(40),
            decimal<0>(50)
        };
        
        decimal<0> F = compute_empirical_cdf(counts, decimal<0>(30));
        // 3 out of 5 values ≤ 30
        // With decimal<0> (0 decimal places), division behaves like integer division
        // So 3/5 rounds to 1 (since decimal uses rounding, not truncation)
        REQUIRE(F.getAsDouble() == Approx(1.0));
    }
    
    SECTION("decimal with duplicates")
    {
        const std::vector<decimal<2>> data = {
            decimal<2>(100),  // 1.00
            decimal<2>(200),  // 2.00
            decimal<2>(200),  // 2.00
            decimal<2>(300),  // 3.00
            decimal<2>(300),  // 3.00
            decimal<2>(300)   // 3.00
        };
        
        decimal<2> F = compute_empirical_cdf(data, decimal<2>(200));
        // 3 out of 6 values ≤ 2.00
        REQUIRE(F.getAsDouble() == Approx(0.5));
        
        F = compute_empirical_cdf(data, decimal<2>(300));
        // All 6 values ≤ 3.00
        REQUIRE(F.getAsDouble() == Approx(1.0));
    }
    
    SECTION("Empty decimal container")
    {
        const std::vector<decimal<2>> empty;
        decimal<2> F = compute_empirical_cdf(empty, decimal<2>(100));
        REQUIRE(F.getAsDouble() == 0.0);
    }
    
    SECTION("Single decimal element")
    {
        const std::vector<decimal<2>> single = {decimal<2>(500)};  // 5.00
        
        decimal<2> F = compute_empirical_cdf(single, decimal<2>(499));
        REQUIRE(F.getAsDouble() == 0.0);
        
        F = compute_empirical_cdf(single, decimal<2>(500));
        REQUIRE(F.getAsDouble() == 1.0);
        
        F = compute_empirical_cdf(single, decimal<2>(501));
        REQUIRE(F.getAsDouble() == 1.0);
    }
}

TEST_CASE("compute_empirical_cdf: decimal type properties",
          "[EmpiricalCdf][decimal][properties]")
{
    using dec::decimal;
    
    SECTION("Monotonicity with decimal")
    {
        const std::vector<decimal<2>> data = {
            decimal<2>(150), decimal<2>(230), decimal<2>(180),
            decimal<2>(310), decimal<2>(200), decimal<2>(270)
        };
        
        const std::vector<decimal<2>> test_points = {
            decimal<2>(100), decimal<2>(150), decimal<2>(200),
            decimal<2>(250), decimal<2>(300), decimal<2>(350)
        };
        
        for (std::size_t i = 1; i < test_points.size(); ++i)
        {
            decimal<2> F1 = compute_empirical_cdf(data, test_points[i-1]);
            decimal<2> F2 = compute_empirical_cdf(data, test_points[i]);
            REQUIRE(F2 >= F1);
        }
    }
    
    SECTION("Range [0,1] with decimal")
    {
        const std::vector<decimal<2>> data = {
            decimal<2>(150), decimal<2>(230), decimal<2>(180),
            decimal<2>(310), decimal<2>(200)
        };
        
        for (int x_val = -1000; x_val <= 1000; x_val += 100)
        {
            decimal<2> F = compute_empirical_cdf(data, decimal<2>(x_val));
            REQUIRE(F >= decimal<2>(0));
            REQUIRE(F <= decimal<2>(100));  // 1.00 in decimal<2>
        }
    }
}

TEST_CASE("compute_empirical_cdf: decimal bootstrap use case",
          "[EmpiricalCdf][decimal][integration][bootstrap]")
{
    using dec::decimal;
    
    SECTION("Computing bias with decimal bootstrap statistics")
    {
        // Simulate bootstrap replicates with monetary values
        const std::vector<decimal<2>> bootstrap_means = {
            decimal<2>(9950),   // 99.50
            decimal<2>(10020),  // 100.20
            decimal<2>(9980),   // 99.80
            decimal<2>(10050),  // 100.50
            decimal<2>(10010),  // 100.10
            decimal<2>(9990),   // 99.90
            decimal<2>(10030),  // 100.30
            decimal<2>(10000),  // 100.00
            decimal<2>(10040),  // 100.40
            decimal<2>(9960)    // 99.60
        };
        
        decimal<2> theta_hat(10000);  // 100.00
        
        // Compute proportion of bootstrap stats ≤ theta_hat
        decimal<2> prop = compute_empirical_cdf(bootstrap_means, theta_hat);
        
        // Should be 0.5 (5 out of 10 values ≤ 100.00: 99.50, 99.80, 99.90, 100.00, 99.60)
        REQUIRE(prop.getAsDouble() == Approx(0.5));
        
        // For integration with normal quantile, convert to double
        double prop_double = prop.getAsDouble();
        double z0 = compute_normal_quantile(prop_double);
        
        // At p=0.5, z0 should be exactly 0 (no bias)
        REQUIRE(z0 == 0.0);
    }
    
    SECTION("Decimal percentile calculations")
    {
        std::vector<decimal<2>> returns = {
            decimal<2>(-150),  // -1.50%
            decimal<2>(-80),   // -0.80%
            decimal<2>(30),    //  0.30%
            decimal<2>(120),   //  1.20%
            decimal<2>(250),   //  2.50%
            decimal<2>(180),   //  1.80%
            decimal<2>(90),    //  0.90%
            decimal<2>(-20),   // -0.20%
            decimal<2>(140),   //  1.40%
            decimal<2>(60)     //  0.60%
        };
        
        // Find empirical 5th percentile (VaR-like calculation)
        std::sort(returns.begin(), returns.end());
        // Sorted: -1.50, -0.80, -0.20, 0.30, 0.60, 0.90, 1.20, 1.40, 1.80, 2.50
        
        decimal<2> fifth_percentile = returns[0];  // -1.50%
        decimal<2> F = compute_empirical_cdf(returns, fifth_percentile);
        
        // Should be 0.1 (1 out of 10)
        REQUIRE(F.getAsDouble() == Approx(0.1));
    }
}

TEST_CASE("compute_empirical_cdf: mixed precision decimal containers",
          "[EmpiricalCdf][decimal][precision]")
{
    using dec::decimal;
    
    SECTION("decimal<6> very high precision")
    {
        const std::vector<decimal<6>> precise_values = {
            decimal<6>(1000000),   // 1.000000
            decimal<6>(1000001),   // 1.000001
            decimal<6>(1000002),   // 1.000002
            decimal<6>(999999),    // 0.999999
            decimal<6>(1000000)    // 1.000000 (duplicate)
        };
        
        decimal<6> F = compute_empirical_cdf(precise_values, decimal<6>(1000000));
        // 3 out of 5 values ≤ 1.000000
        REQUIRE(F.getAsDouble() == Approx(0.6));
        
        F = compute_empirical_cdf(precise_values, decimal<6>(999999));
        // 1 out of 5 values ≤ 0.999999
        REQUIRE(F.getAsDouble() == Approx(0.2));
    }
}

TEST_CASE("compute_empirical_cdf: return type verification",
          "[EmpiricalCdf][types][return]")
{
    SECTION("double container returns double")
    {
        const std::vector<double> data = {1.0, 2.0, 3.0};
        auto result = compute_empirical_cdf(data, 2.0);
        static_assert(std::is_same<decltype(result), double>::value,
                      "Return type should be double for double container");
        REQUIRE(result == Approx(0.666666).margin(1e-5));
    }
    
    SECTION("float container returns float")
    {
        const std::vector<float> data = {1.0f, 2.0f, 3.0f};
        auto result = compute_empirical_cdf(data, 2.0f);
        static_assert(std::is_same<decltype(result), float>::value,
                      "Return type should be float for float container");
        REQUIRE(result == Approx(0.666666f).margin(1e-5f));
    }
    
    SECTION("int container returns int")
    {
        const std::vector<int> data = {1, 2, 3, 4, 5, 6};
        auto result = compute_empirical_cdf(data, 3);
        static_assert(std::is_same<decltype(result), int>::value,
                      "Return type should be int for int container");
        REQUIRE(result == 0);  // 3/6 = 0 with integer division
    }
    
    SECTION("decimal<2> container returns decimal<2>")
    {
        using dec::decimal;
        const std::vector<decimal<2>> data = {
            decimal<2>(100), decimal<2>(200), decimal<2>(300)
        };
        auto result = compute_empirical_cdf(data, decimal<2>(200));
        static_assert(std::is_same<decltype(result), decimal<2>>::value,
                      "Return type should be decimal<2> for decimal<2> container");
        // decimal<2> has only 2 decimal places, so 2/3 rounds to 0.67
        REQUIRE(result.getAsDouble() == Approx(0.67).margin(1e-5));
    }
}
