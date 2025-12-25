// AutoBootstrapSelector_computeLengthPenalty_Tests.cpp
//
// Unit tests for AutoBootstrapSelector::computeLengthPenalty method
//
// This test file provides comprehensive coverage of the computeLengthPenalty
// static method, including:
//  - Edge cases (empty data, degenerate distributions)
//  - Normal operation (within acceptable bounds)
//  - Penalty calculations (too short, too long intervals)
//  - Method-specific behavior (MOutOfN vs standard methods)
//  - Output parameter verification (normalized_length, median_val)
//
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - AutoBootstrapSelector.h
//  - number.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <limits>
#include <cmath>

#include "AutoBootstrapSelector.h"
#include "number.h"

// Alias for convenience
using Decimal        = double;
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using MethodId       = Selector::Result::MethodId;

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

/**
 * @brief Creates a simple bootstrap distribution with specified mean and spread
 * 
 * Generates n bootstrap statistics normally distributed around a mean value
 * with a specified standard deviation.
 */
std::vector<double> createBootstrapStats(double mean, double std_dev, size_t n)
{
    std::vector<double> stats;
    stats.reserve(n);
    
    // Simple deterministic generation for testing
    for (size_t i = 0; i < n; ++i) {
        double z = -3.0 + 6.0 * i / (n - 1);  // Range from -3 to +3
        stats.push_back(mean + z * std_dev);
    }
    
    return stats;
}

/**
 * @brief Creates a uniform bootstrap distribution between min and max
 */
std::vector<double> createUniformBootstrap(double min_val, double max_val, size_t n)
{
    std::vector<double> stats;
    stats.reserve(n);
    
    for (size_t i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / (n - 1);
        stats.push_back(min_val + t * (max_val - min_val));
    }
    
    return stats;
}

// -----------------------------------------------------------------------------
// Edge Case Tests
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Edge cases return zero penalty",
          "[AutoBootstrapSelector][computeLengthPenalty][EdgeCases]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Empty bootstrap statistics returns zero penalty")
    {
        std::vector<double> empty_stats;
        double penalty = Selector::computeLengthPenalty(
            1.0,                    // actual_length
            empty_stats,            // boot_stats (empty)
            0.95,                   // confidence_level
            MethodId::Percentile,   // method
            normalized_length,      // output
            median_val);            // output
        
        REQUIRE(penalty == 0.0);
        REQUIRE(normalized_length == 1.0);  // Default value
        REQUIRE(median_val == 0.0);         // Default value
    }
    
    SECTION("Single bootstrap statistic returns zero penalty")
    {
        std::vector<double> single_stat = {1.5};
        double penalty = Selector::computeLengthPenalty(
            1.0,
            single_stat,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(normalized_length == 1.0);
        REQUIRE(median_val == 0.0);
    }
    
    SECTION("Zero actual length returns zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(1.0, 0.2, 100);
        double penalty = Selector::computeLengthPenalty(
            0.0,                    // actual_length (zero)
            stats,
            0.95,
            MethodId::Basic,
            normalized_length,
            median_val);
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Negative actual length returns zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(1.0, 0.2, 100);
        double penalty = Selector::computeLengthPenalty(
            -0.5,                   // actual_length (negative)
            stats,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Degenerate bootstrap distribution (all identical) returns zero penalty")
    {
        // All bootstrap statistics are the same value
        std::vector<double> degenerate_stats(100, 1.5);
        double penalty = Selector::computeLengthPenalty(
            0.5,
            degenerate_stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(median_val == Catch::Approx(1.5));  // Median should still be computed
    }
}

// -----------------------------------------------------------------------------
// Median Computation Tests
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Median computation is correct",
          "[AutoBootstrapSelector][computeLengthPenalty][Median]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Median of odd-sized bootstrap sample")
    {
        std::vector<double> stats = {1.0, 2.0, 3.0, 4.0, 5.0};  // Median = 3.0
        
        Selector::computeLengthPenalty(
            1.0,
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        REQUIRE(median_val == Catch::Approx(3.0));
    }
    
    SECTION("Median of even-sized bootstrap sample")
    {
        std::vector<double> stats = {1.0, 2.0, 3.0, 4.0};  // Median = (2.0 + 3.0) / 2 = 2.5
        
        Selector::computeLengthPenalty(
            1.0,
            stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        REQUIRE(median_val == Catch::Approx(2.5));
    }
    
    SECTION("Median with unsorted input data")
    {
        std::vector<double> unsorted = {5.0, 1.0, 3.0, 2.0, 4.0};  // Median should be 3.0
        
        Selector::computeLengthPenalty(
            1.0,
            unsorted,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        REQUIRE(median_val == Catch::Approx(3.0));
    }
    
    SECTION("Median with negative values")
    {
        std::vector<double> stats = {-5.0, -3.0, -1.0, 1.0, 3.0};  // Median = -1.0
        
        Selector::computeLengthPenalty(
            2.0,
            stats,
            0.95,
            MethodId::Basic,
            normalized_length,
            median_val);
        
        REQUIRE(median_val == Catch::Approx(-1.0));
    }
    
    SECTION("Median with large dataset")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
        
        Selector::computeLengthPenalty(
            5.0,
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        // For a symmetric distribution, median should be close to mean
        REQUIRE(median_val == Catch::Approx(10.0).margin(0.1));
    }
}

// -----------------------------------------------------------------------------
// Normalized Length Tests
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Normalized length calculation",
          "[AutoBootstrapSelector][computeLengthPenalty][NormalizedLength]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Normalized length equals 1.0 when actual equals ideal")
    {
        // Create bootstrap distribution with known quantiles
        std::vector<double> stats = createUniformBootstrap(0.0, 10.0, 1000);
        
        // For 95% CI, alpha = 0.05, alphaL = 0.025, alphaU = 0.975
        // Ideal length ≈ q(0.975) - q(0.025) ≈ 9.75 - 0.25 = 9.5
        const double ideal_length = 9.5;
        
        double penalty = Selector::computeLengthPenalty(
            ideal_length,           // actual_length matches ideal
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(1.0).epsilon(0.01));
        REQUIRE(penalty == 0.0);  // No penalty when within bounds
    }
    
    SECTION("Normalized length < 1.0 when actual is shorter than ideal")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        // Use a short actual length
        double penalty = Selector::computeLengthPenalty(
            1.0,                    // short actual length
            stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length < 1.0);
        // Penalty should exist if normalized length is below minimum (0.8)
        // or be zero if it's between 0.8 and 1.0
        REQUIRE(std::isfinite(penalty));
        REQUIRE(penalty >= 0.0);
    }
    
    SECTION("Normalized length > 1.0 when actual is longer than ideal")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        // Use a very long actual length
        double penalty = Selector::computeLengthPenalty(
            20.0,                   // long actual length
            stats,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length > 1.0);
        // With such a long interval, penalty should exist (exceeds max of 1.8)
        REQUIRE(std::isfinite(penalty));
        REQUIRE(penalty > 0.0);
    }
    
    SECTION("Different confidence levels affect ideal length")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
        double normalized_95, normalized_90;
        double median_95, median_90;
        
        // Same actual length, different confidence levels
        const double actual_length = 8.0;
        
        Selector::computeLengthPenalty(
            actual_length,
            stats,
            0.95,                   // 95% CI
            MethodId::Percentile,
            normalized_95,
            median_95);
        
        Selector::computeLengthPenalty(
            actual_length,
            stats,
            0.90,                   // 90% CI (narrower ideal)
            MethodId::Percentile,
            normalized_90,
            median_90);
        
        // Same actual length, but 90% CI has narrower ideal, so normalized_90 > normalized_95
        REQUIRE(normalized_90 > normalized_95);
    }
}

// -----------------------------------------------------------------------------
// Penalty Calculation Tests - Within Bounds
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Zero penalty within acceptable bounds",
          "[AutoBootstrapSelector][computeLengthPenalty][WithinBounds]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Normalized length at minimum bound (0.8) has zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        // We need to engineer actual_length such that normalized = 0.8
        // First, get the ideal length
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Percentile, temp_norm, temp_med);
        
        // Now we know 1.0 / ideal = temp_norm, so ideal = 1.0 / temp_norm
        double ideal_length = 1.0 / temp_norm;
        double actual_at_min = 0.8 * ideal_length;
        
        double penalty = Selector::computeLengthPenalty(
            actual_at_min,
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(0.8).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Normalized length at standard maximum bound (1.8) has zero penalty for BCa")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::BCa, temp_norm, temp_med);
        
        double ideal_length = 1.0 / temp_norm;
        double actual_at_max = 1.8 * ideal_length;
        
        double penalty = Selector::computeLengthPenalty(
            actual_at_max,
            stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(1.8).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Normalized length = 1.0 (ideal) has zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::PercentileT, temp_norm, temp_med);
        
        double ideal_length = 1.0 / temp_norm;
        
        double penalty = Selector::computeLengthPenalty(
            ideal_length,           // actual = ideal
            stats,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Normalized length in middle of acceptable range has zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Basic, temp_norm, temp_med);
        
        double ideal_length = 1.0 / temp_norm;
        double actual_middle = 1.3 * ideal_length;  // Between 0.8 and 1.8
        
        double penalty = Selector::computeLengthPenalty(
            actual_middle,
            stats,
            0.95,
            MethodId::Basic,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(1.3).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
}

// -----------------------------------------------------------------------------
// Penalty Calculation Tests - Too Short
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Quadratic penalty when interval too short",
          "[AutoBootstrapSelector][computeLengthPenalty][TooShort]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Penalty increases quadratically as interval gets shorter")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        // Get ideal length
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Percentile, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        // Test several lengths below minimum (0.8)
        double actual_0_7 = 0.7 * ideal_length;  // normalized = 0.7, deficit = 0.1
        double actual_0_6 = 0.6 * ideal_length;  // normalized = 0.6, deficit = 0.2
        
        double penalty_0_7 = Selector::computeLengthPenalty(
            actual_0_7, stats, 0.95, MethodId::Percentile, normalized_length, median_val);
        
        double penalty_0_6 = Selector::computeLengthPenalty(
            actual_0_6, stats, 0.95, MethodId::Percentile, normalized_length, median_val);
        
        // Expected penalties: (0.8 - 0.7)^2 = 0.01 and (0.8 - 0.6)^2 = 0.04
        REQUIRE(penalty_0_7 == Catch::Approx(0.01).epsilon(0.001));
        REQUIRE(penalty_0_6 == Catch::Approx(0.04).epsilon(0.001));
        
        // Quadratic relationship: penalty should quadruple when deficit doubles
        REQUIRE(penalty_0_6 == Catch::Approx(4.0 * penalty_0_7).epsilon(0.01));
    }
    
    SECTION("Very short interval has large penalty")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::BCa, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        // Normalized = 0.3, deficit = 0.5
        double actual_very_short = 0.3 * ideal_length;
        
        double penalty = Selector::computeLengthPenalty(
            actual_very_short,
            stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(0.3).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.25).epsilon(0.001));  // (0.8 - 0.3)^2 = 0.25
    }
}

// -----------------------------------------------------------------------------
// Penalty Calculation Tests - Too Long
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Quadratic penalty when interval too long",
          "[AutoBootstrapSelector][computeLengthPenalty][TooLong]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Penalty for standard methods when exceeding 1.8x ideal")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Percentile, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        // Test lengths above maximum (1.8)
        double actual_2_0 = 2.0 * ideal_length;  // normalized = 2.0, excess = 0.2
        double actual_2_4 = 2.4 * ideal_length;  // normalized = 2.4, excess = 0.6
        
        double penalty_2_0 = Selector::computeLengthPenalty(
            actual_2_0, stats, 0.95, MethodId::Percentile, normalized_length, median_val);
        
        double penalty_2_4 = Selector::computeLengthPenalty(
            actual_2_4, stats, 0.95, MethodId::Percentile, normalized_length, median_val);
        
        // Expected penalties: (2.0 - 1.8)^2 = 0.04 and (2.4 - 1.8)^2 = 0.36
        REQUIRE(penalty_2_0 == Catch::Approx(0.04).epsilon(0.001));
        REQUIRE(penalty_2_4 == Catch::Approx(0.36).epsilon(0.001));
        
        // Quadratic relationship
        REQUIRE(penalty_2_4 == Catch::Approx(9.0 * penalty_2_0).epsilon(0.01));
    }
    
    SECTION("BCa method uses standard maximum (1.8)")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::BCa, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        double actual_2_5 = 2.5 * ideal_length;  // Above 1.8
        
        double penalty = Selector::computeLengthPenalty(
            actual_2_5,
            stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        // Excess = 2.5 - 1.8 = 0.7, penalty = 0.49
        REQUIRE(penalty == Catch::Approx(0.49).epsilon(0.001));
    }
    
    SECTION("PercentileT method uses standard maximum (1.8)")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
        
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::PercentileT, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        double actual_3_0 = 3.0 * ideal_length;  // Well above 1.8
        
        double penalty = Selector::computeLengthPenalty(
            actual_3_0,
            stats,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        // Excess = 3.0 - 1.8 = 1.2, penalty = 1.44
        REQUIRE(penalty == Catch::Approx(1.44).epsilon(0.001));
    }
}

// -----------------------------------------------------------------------------
// Method-Specific Tests - MOutOfN
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: MOutOfN method uses higher maximum (6.0)",
          "[AutoBootstrapSelector][computeLengthPenalty][MOutOfN]")
{
    double normalized_length;
    double median_val;
    std::vector<double> stats = createBootstrapStats(5.0, 1.0, 1000);
    
    double temp_norm, temp_med;
    Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::MOutOfN, temp_norm, temp_med);
    double ideal_length = 1.0 / temp_norm;
    
    SECTION("Length at 5.0x ideal is within bounds for MOutOfN (zero penalty)")
    {
        double actual_5_0 = 5.0 * ideal_length;
        
        double penalty = Selector::computeLengthPenalty(
            actual_5_0,
            stats,
            0.95,
            MethodId::MOutOfN,       // MOutOfN uses L_max = 6.0
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(5.0).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));  // Within bounds
    }
    
    SECTION("Length at 6.0x ideal is at boundary for MOutOfN (zero penalty)")
    {
        double actual_6_0 = 6.0 * ideal_length;
        
        double penalty = Selector::computeLengthPenalty(
            actual_6_0,
            stats,
            0.95,
            MethodId::MOutOfN,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(6.0).epsilon(0.001));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Length at 7.0x ideal exceeds MOutOfN maximum (has penalty)")
    {
        double actual_7_0 = 7.0 * ideal_length;
        
        double penalty = Selector::computeLengthPenalty(
            actual_7_0,
            stats,
            0.95,
            MethodId::MOutOfN,
            normalized_length,
            median_val);
        
        REQUIRE(normalized_length == Catch::Approx(7.0).epsilon(0.001));
        // Excess = 7.0 - 6.0 = 1.0, penalty = 1.0
        REQUIRE(penalty == Catch::Approx(1.0).epsilon(0.001));
    }
    
    SECTION("Same length penalized for standard method but not MOutOfN")
    {
        // Test length at 3.0x ideal
        double actual_3_0 = 3.0 * ideal_length;
        
        // For standard methods (max = 1.8), this should have penalty
        double penalty_standard = Selector::computeLengthPenalty(
            actual_3_0,
            stats,
            0.95,
            MethodId::Percentile,    // Standard method
            normalized_length,
            median_val);
        
        // For MOutOfN (max = 6.0), this should have NO penalty
        double penalty_moutofn = Selector::computeLengthPenalty(
            actual_3_0,
            stats,
            0.95,
            MethodId::MOutOfN,
            normalized_length,
            median_val);
        
        // Standard method: excess = 3.0 - 1.8 = 1.2, penalty = 1.44
        REQUIRE(penalty_standard == Catch::Approx(1.44).epsilon(0.001));
        
        // MOutOfN: within bounds, penalty = 0
        REQUIRE(penalty_moutofn == Catch::Approx(0.0).margin(1e-6));
    }
}

// -----------------------------------------------------------------------------
// Integration Tests - Realistic Scenarios
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Realistic bootstrap scenarios",
          "[AutoBootstrapSelector][computeLengthPenalty][Integration]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Scenario: Well-behaved symmetric distribution")
    {
        // Symmetric normal-like bootstrap distribution
        std::vector<double> stats = createBootstrapStats(100.0, 10.0, 1000);
        
        // First, determine the ideal length for this distribution
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Percentile, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        // Use an actual interval close to ideal (within acceptable range)
        double actual_length = 1.1 * ideal_length;  // Slightly wider than ideal
        
        double penalty = Selector::computeLengthPenalty(
            actual_length,
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        // Should have median near mean
        REQUIRE(median_val == Catch::Approx(100.0).margin(1.0));
        
        // Should have normalized length close to 1.0
        REQUIRE(normalized_length == Catch::Approx(1.1).epsilon(0.01));
        
        // Should have low or zero penalty (1.1 is within [0.8, 1.8])
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Scenario: Skewed distribution")
    {
        // Right-skewed distribution (log-normal like)
        std::vector<double> skewed_stats;
        for (size_t i = 0; i < 1000; ++i) {
            double u = static_cast<double>(i) / 999.0;
            // Exponential-like transformation
            skewed_stats.push_back(std::exp(u * 2.0));
        }
        
        double penalty = Selector::computeLengthPenalty(
            5.0,
            skewed_stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        // For skewed data, median should be less than mean
        double mean = std::accumulate(skewed_stats.begin(), skewed_stats.end(), 0.0) 
                     / skewed_stats.size();
        REQUIRE(median_val < mean);
        
        // Penalty should be finite and non-negative
        REQUIRE(std::isfinite(penalty));
        REQUIRE(penalty >= 0.0);
    }
    
    SECTION("Scenario: Tight confidence interval (anti-conservative)")
    {
        std::vector<double> stats = createBootstrapStats(50.0, 5.0, 1000);
        
        // Interval much narrower than ideal (anti-conservative)
        double penalty = Selector::computeLengthPenalty(
            5.0,                    // Very narrow
            stats,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        // Normalized length should be well below 1.0
        REQUIRE(normalized_length < 0.8);
        
        // Should have substantial penalty for being too narrow
        REQUIRE(penalty > 0.01);
    }
    
    SECTION("Scenario: Wide confidence interval (conservative)")
    {
        std::vector<double> stats = createBootstrapStats(50.0, 5.0, 1000);
        
        // First, determine the ideal length for this distribution
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Basic, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        // Use an interval much wider than ideal (exceeds maximum of 1.8)
        double actual_length = 2.5 * ideal_length;  // Well above 1.8x
        
        double penalty = Selector::computeLengthPenalty(
            actual_length,
            stats,
            0.95,
            MethodId::Basic,
            normalized_length,
            median_val);
        
        // Normalized length should be well above 1.8
        REQUIRE(normalized_length == Catch::Approx(2.5).epsilon(0.01));
        
        // Should have substantial penalty for being too wide
        // Excess = 2.5 - 1.8 = 0.7, penalty = 0.49
        REQUIRE(penalty == Catch::Approx(0.49).epsilon(0.01));
    }
}

// -----------------------------------------------------------------------------
// Boundary and Numerical Stability Tests
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Numerical stability and boundary conditions",
          "[AutoBootstrapSelector][computeLengthPenalty][Stability]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Very small bootstrap values")
    {
        std::vector<double> stats = createBootstrapStats(1e-8, 1e-9, 1000);
        
        double penalty = Selector::computeLengthPenalty(
            1e-9,
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        // Should handle small values without numerical issues
        REQUIRE(std::isfinite(penalty));
        REQUIRE(std::isfinite(normalized_length));
        REQUIRE(std::isfinite(median_val));
    }
    
    SECTION("Very large bootstrap values")
    {
        std::vector<double> stats = createBootstrapStats(1e8, 1e7, 1000);
        
        double penalty = Selector::computeLengthPenalty(
            5e7,
            stats,
            0.95,
            MethodId::BCa,
            normalized_length,
            median_val);
        
        // Should handle large values without overflow
        REQUIRE(std::isfinite(penalty));
        REQUIRE(std::isfinite(normalized_length));
        REQUIRE(std::isfinite(median_val));
    }
    
    SECTION("Bootstrap statistics with extreme outliers")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 998);
        // Add extreme outliers
        stats.push_back(-1000.0);
        stats.push_back(1000.0);
        
        double penalty = Selector::computeLengthPenalty(
            15.0,
            stats,
            0.95,
            MethodId::PercentileT,
            normalized_length,
            median_val);
        
        // Median should be robust to outliers
        REQUIRE(std::abs(median_val - 10.0) < 3.0);
        
        // Should still compute valid penalty
        REQUIRE(std::isfinite(penalty));
    }
}

// -----------------------------------------------------------------------------
// Output Parameter Tests
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Output parameters are correctly populated",
          "[AutoBootstrapSelector][computeLengthPenalty][Outputs]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Both output parameters are modified")
    {
        std::vector<double> stats = createBootstrapStats(5.0, 1.0, 100);
        
        // Set outputs to sentinel values
        normalized_length = -999.0;
        median_val = -999.0;
        
        Selector::computeLengthPenalty(
            3.0,
            stats,
            0.95,
            MethodId::Percentile,
            normalized_length,
            median_val);
        
        // Both should be changed from sentinel values
        REQUIRE(normalized_length != -999.0);
        REQUIRE(median_val != -999.0);
        
        // And should be reasonable values
        REQUIRE(normalized_length > 0.0);
        REQUIRE(std::abs(median_val - 5.0) < 2.0);  // Near the mean
    }
    
    SECTION("Outputs are independent of each other")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
        
        double norm1, median1, norm2, median2;
        
        // Same stats, different actual lengths
        Selector::computeLengthPenalty(5.0, stats, 0.95, MethodId::BCa, norm1, median1);
        Selector::computeLengthPenalty(10.0, stats, 0.95, MethodId::BCa, norm2, median2);
        
        // Normalized lengths should differ
        REQUIRE(norm1 != norm2);
        
        // But medians should be the same (same bootstrap distribution)
        REQUIRE(median1 == Catch::Approx(median2));
    }
}

// -----------------------------------------------------------------------------
// Comprehensive Comparison Tests
// -----------------------------------------------------------------------------

TEST_CASE("computeLengthPenalty: Comparative behavior across methods",
          "[AutoBootstrapSelector][computeLengthPenalty][Comparison]")
{
    std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
    double normalized_length;
    double median_val;
    
    SECTION("All standard methods have same bounds except MOutOfN")
    {
        const double test_length = 10.0;
        
        double penalty_percentile = Selector::computeLengthPenalty(
            test_length, stats, 0.95, MethodId::Percentile, normalized_length, median_val);
        
        double penalty_bca = Selector::computeLengthPenalty(
            test_length, stats, 0.95, MethodId::BCa, normalized_length, median_val);
        
        double penalty_perct = Selector::computeLengthPenalty(
            test_length, stats, 0.95, MethodId::PercentileT, normalized_length, median_val);
        
        double penalty_basic = Selector::computeLengthPenalty(
            test_length, stats, 0.95, MethodId::Basic, normalized_length, median_val);
        
        // All standard methods should give same penalty for same length
        REQUIRE(penalty_percentile == Catch::Approx(penalty_bca).margin(1e-6));
        REQUIRE(penalty_percentile == Catch::Approx(penalty_perct).margin(1e-6));
        REQUIRE(penalty_percentile == Catch::Approx(penalty_basic).margin(1e-6));
    }
    
    SECTION("MOutOfN is more lenient for wide intervals")
    {
        double temp_norm, temp_med;
        Selector::computeLengthPenalty(1.0, stats, 0.95, MethodId::Percentile, temp_norm, temp_med);
        double ideal_length = 1.0 / temp_norm;
        
        // Test at 4.0x ideal (exceeds standard max of 1.8, within MOutOfN max of 6.0)
        double wide_length = 4.0 * ideal_length;
        
        double penalty_standard = Selector::computeLengthPenalty(
            wide_length, stats, 0.95, MethodId::Percentile, normalized_length, median_val);
        
        double penalty_moutofn = Selector::computeLengthPenalty(
            wide_length, stats, 0.95, MethodId::MOutOfN, normalized_length, median_val);
        
        // Standard should penalize, MOutOfN should not
        REQUIRE(penalty_standard > 0.1);
        REQUIRE(penalty_moutofn == Catch::Approx(0.0).margin(1e-6));
    }
}
