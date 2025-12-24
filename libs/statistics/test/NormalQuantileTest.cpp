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
#include <cmath>
#include <limits>
#include <stdexcept>
#include "NormalQuantile.h"
#include "NormalDistribution.h"

using Catch::Approx;

// Convenience aliases
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
