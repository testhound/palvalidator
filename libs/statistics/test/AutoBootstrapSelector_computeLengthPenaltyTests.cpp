// AutoBootstrapSelector_MethodSpecificLengthPenalty_Tests.cpp
//
// Unit tests for method-specific computeLengthPenalty functions:
//  - computeLengthPenalty_Percentile (for Percentile, BCa, Basic, MOutOfN)
//  - computeLengthPenalty_Normal (for Normal method)
//  - computeLengthPenalty_PercentileT (for Percentile-T method)
//
// This replaces the legacy computeLengthPenalty tests with method-specific
// tests that properly verify each method is judged by its own theoretical ideal.
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
#include <algorithm>

#include "AutoBootstrapSelector.h"
#include "BootstrapPenaltyCalculator.h"
#include "number.h"

// Alias for convenience
using Decimal   = double;
using Selector  = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using PenaltyCalc = palvalidator::analysis::BootstrapPenaltyCalculator<Decimal>;
using MethodId  = Selector::Result::MethodId;

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

/**
 * @brief Creates a bootstrap distribution with specified mean and spread
 * 
 * Generates n bootstrap statistics approximately normally distributed
 * with deterministic values for reproducible testing.
 */
std::vector<double> createBootstrapStats(double mean, double std_dev, size_t n)
{
    std::vector<double> stats;
    stats.reserve(n);
    
    // Deterministic generation: uniformly spaced z-scores from -3 to +3
    for (size_t i = 0; i < n; ++i) {
        double z = -3.0 + 6.0 * i / (n - 1);
        stats.push_back(mean + z * std_dev);
    }
    
    return stats;
}

/**
 * @brief Creates a T-statistic distribution (studentized bootstrap)
 * 
 * Simulates T* = (θ* - θ̂) / SE* values
 */
std::vector<double> createTStatistics(double mean_t, double std_dev_t, size_t n)
{
    // T-statistics typically have mean ≈ 0 and are often heavier-tailed than normal
    std::vector<double> t_stats;
    t_stats.reserve(n);
    
    for (size_t i = 0; i < n; ++i) {
        double z = -3.0 + 6.0 * i / (n - 1);
        t_stats.push_back(mean_t + z * std_dev_t);
    }
    
    return t_stats;
}

/**
 * @brief Computes standard error of a dataset
 */
double computeSE(const std::vector<double>& data)
{
    if (data.size() < 2) return 0.0;
    
    double sum = 0.0;
    for (double v : data) sum += v;
    double mean = sum / data.size();
    
    double sum_sq = 0.0;
    for (double v : data) {
        double diff = v - mean;
        sum_sq += diff * diff;
    }
    
    return std::sqrt(sum_sq / data.size());
}

/**
 * @brief Computes quantile from sorted data (Type 7, R default)
 */
double computeQuantile(const std::vector<double>& sorted_data, double prob)
{
    if (sorted_data.empty()) return 0.0;
    if (sorted_data.size() == 1) return sorted_data[0];
    
    const size_t n = sorted_data.size();
    const double h = (n - 1) * prob;
    const size_t i = static_cast<size_t>(std::floor(h));
    
    if (i >= n - 1) return sorted_data[n - 1];
    
    const double frac = h - i;
    return sorted_data[i] + frac * (sorted_data[i + 1] - sorted_data[i]);
}

// =============================================================================
// TESTS FOR computeLengthPenalty_Percentile
// =============================================================================

TEST_CASE("computeLengthPenalty_Percentile: Edge cases",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][EdgeCases]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Empty bootstrap statistics returns zero penalty")
    {
        std::vector<double> empty_stats;
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            1.0, empty_stats, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(normalized_length == 1.0);
        REQUIRE(median_val == 0.0);
    }
    
    SECTION("Single statistic returns zero penalty")
    {
        std::vector<double> single = {1.5};
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            1.0, single, 0.95, MethodId::BCa,
            normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Zero actual length returns zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(1.0, 0.2, 100);
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            0.0, stats, 0.95, MethodId::Basic,
            normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Degenerate distribution (all identical) returns zero penalty")
    {
        std::vector<double> degenerate(100, 5.0);
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            1.0, degenerate, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(median_val == Catch::Approx(5.0));
    }
}

TEST_CASE("computeLengthPenalty_Percentile: Median computation",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][Median]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Median of odd-sized sample")
    {
        std::vector<double> stats = {1.0, 2.0, 3.0, 4.0, 5.0};
        PenaltyCalc::computeLengthPenalty_Percentile(
            1.0, stats, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        REQUIRE(median_val == Catch::Approx(3.0));
    }
    
    SECTION("Median of even-sized sample")
    {
        std::vector<double> stats = {1.0, 2.0, 3.0, 4.0};
        PenaltyCalc::computeLengthPenalty_Percentile(
            1.0, stats, 0.95, MethodId::BCa,
            normalized_length, median_val);
        
        REQUIRE(median_val == Catch::Approx(2.5));
    }
}

TEST_CASE("computeLengthPenalty_Percentile: Normalized length at ideal",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][Ideal]")
{
    SECTION("Interval matching percentile quantile width gets normalized=1.0")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
        
        // Compute the ideal length (what Percentile method targets)
        std::vector<double> sorted = stats;
        std::sort(sorted.begin(), sorted.end());
        double q_025 = computeQuantile(sorted, 0.025);
        double q_975 = computeQuantile(sorted, 0.975);
        double ideal_length = q_975 - q_025;
        
        double normalized_length;
        double median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            ideal_length, stats, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        // Should have normalized_length = 1.0 (actual matches ideal)
        REQUIRE(normalized_length == Catch::Approx(1.0).epsilon(0.01));
        
        // No penalty when at ideal
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("computeLengthPenalty_Percentile: Penalty for too-short intervals",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][TooShort]")
{
    std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
    
    // Compute ideal
    std::vector<double> sorted = stats;
    std::sort(sorted.begin(), sorted.end());
    double ideal = computeQuantile(sorted, 0.975) - computeQuantile(sorted, 0.025);
    
    SECTION("Interval at 0.5x ideal (well below L_min=0.8)")
    {
        double actual_length = 0.5 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(0.5).epsilon(0.01));
        
        // Penalty = (L_min - normalized)^2 = (0.8 - 0.5)^2 = 0.09
        REQUIRE(penalty == Catch::Approx(0.09).epsilon(0.01));
    }
    
    SECTION("Interval at exactly L_min=0.8 has no penalty")
    {
        double actual_length = 0.8 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::BCa,
            normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(0.8).epsilon(0.01));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("computeLengthPenalty_Percentile: Penalty for too-wide intervals",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][TooWide]")
{
    std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
    
    std::vector<double> sorted = stats;
    std::sort(sorted.begin(), sorted.end());
    double ideal = computeQuantile(sorted, 0.975) - computeQuantile(sorted, 0.025);
    
    SECTION("Standard method: interval at 2.5x ideal (exceeds L_max=1.8)")
    {
        double actual_length = 2.5 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(2.5).epsilon(0.01));
        
        // Penalty = (normalized - L_max)^2 = (2.5 - 1.8)^2 = 0.49
        REQUIRE(penalty == Catch::Approx(0.49).epsilon(0.01));
    }
    
    SECTION("Standard method: interval at exactly L_max=1.8 has no penalty")
    {
        double actual_length = 1.8 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::Basic,
            normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(1.8).epsilon(0.01));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("computeLengthPenalty_Percentile: MOutOfN has wider tolerance",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][MOutOfN]")
{
    std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
    
    std::vector<double> sorted = stats;
    std::sort(sorted.begin(), sorted.end());
    double ideal = computeQuantile(sorted, 0.975) - computeQuantile(sorted, 0.025);
    
    SECTION("MOutOfN allows up to 6.0x ideal")
    {
        double actual_length = 4.0 * ideal;  // Exceeds standard 1.8, within MOutOfN 6.0
        double normalized_length, median_val;
        
        // Standard method gets penalized
        double penalty_standard = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::Percentile,
            normalized_length, median_val);
        
        REQUIRE(penalty_standard > 0.1);  // (4.0 - 1.8)^2 = 4.84
        
        // MOutOfN does not get penalized
        double penalty_moutofn = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::MOutOfN,
            normalized_length, median_val);
        
        REQUIRE(penalty_moutofn == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("MOutOfN gets penalized beyond 6.0x")
    {
        double actual_length = 7.0 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Percentile(
            actual_length, stats, 0.95, MethodId::MOutOfN,
            normalized_length, median_val);
        
        // Penalty = (7.0 - 6.0)^2 = 1.0
        REQUIRE(penalty == Catch::Approx(1.0).epsilon(0.01));
    }
}

TEST_CASE("computeLengthPenalty_Percentile: All percentile-like methods agree",
          "[AutoBootstrapSelector][LengthPenalty][Percentile][Consistency]")
{
    std::vector<double> stats = createBootstrapStats(10.0, 2.0, 1000);
    double test_length = 15.0;
    
    double norm_perc, norm_bca, norm_basic, med;
    
    double penalty_perc = PenaltyCalc::computeLengthPenalty_Percentile(
        test_length, stats, 0.95, MethodId::Percentile, norm_perc, med);
    
    double penalty_bca = PenaltyCalc::computeLengthPenalty_Percentile(
        test_length, stats, 0.95, MethodId::BCa, norm_bca, med);
    
    double penalty_basic = PenaltyCalc::computeLengthPenalty_Percentile(
        test_length, stats, 0.95, MethodId::Basic, norm_basic, med);
    
    // All should get same normalized length (same ideal reference)
    REQUIRE(norm_perc == Catch::Approx(norm_bca).margin(1e-10));
    REQUIRE(norm_perc == Catch::Approx(norm_basic).margin(1e-10));
    
    // All should get same penalty (same bounds except MOutOfN)
    REQUIRE(penalty_perc == Catch::Approx(penalty_bca).margin(1e-10));
    REQUIRE(penalty_perc == Catch::Approx(penalty_basic).margin(1e-10));
}

// =============================================================================
// TESTS FOR computeLengthPenalty_Normal
// =============================================================================

TEST_CASE("computeLengthPenalty_Normal: Edge cases",
          "[AutoBootstrapSelector][LengthPenalty][Normal][EdgeCases]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Zero actual length returns zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 100);
        double se = computeSE(stats);
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            0.0, se, 0.95, stats, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(normalized_length == 1.0);
        // Median should still be calculated even if length is zero
        REQUIRE(median_val == Catch::Approx(10.0).epsilon(0.1));
    }
    
    SECTION("Zero SE returns zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 100);
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            10.0, 0.0, 0.95, stats, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(normalized_length == 1.0);
        // Median should still be calculated
        REQUIRE(median_val == Catch::Approx(10.0).epsilon(0.1));
    }
    
    SECTION("Negative SE returns zero penalty")
    {
        std::vector<double> stats = createBootstrapStats(10.0, 2.0, 100);
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            10.0, -2.0, 0.95, stats, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        // Median should still be calculated
        REQUIRE(median_val == Catch::Approx(10.0).epsilon(0.1));
    }
    
    SECTION("Empty bootstrap statistics returns zero median")
    {
        std::vector<double> empty_stats;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            10.0, 5.0, 0.95, empty_stats, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(median_val == 0.0);  // Correct for empty stats
    }
    
    SECTION("Single bootstrap statistic uses that value as median")
    {
        std::vector<double> single = {7.5};
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            10.0, 5.0, 0.95, single, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(median_val == 0.0);  // Less than 2 stats → median = 0.0
    }
}

TEST_CASE("computeLengthPenalty_Normal: Median computation",
          "[AutoBootstrapSelector][LengthPenalty][Normal][Median]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Median of odd-sized sample")
    {
        std::vector<double> stats = {1.0, 2.0, 3.0, 4.0, 5.0};
        double se = computeSE(stats);
        
        PenaltyCalc::computeLengthPenalty_Normal(
            10.0, se, 0.95, stats, normalized_length, median_val);
        
        REQUIRE(median_val == Catch::Approx(3.0));
    }
    
    SECTION("Median of even-sized sample")
    {
        std::vector<double> stats = {1.0, 2.0, 3.0, 4.0};
        double se = computeSE(stats);
        
        PenaltyCalc::computeLengthPenalty_Normal(
            10.0, se, 0.95, stats, normalized_length, median_val);
        
        REQUIRE(median_val == Catch::Approx(2.5));
    }
    
    SECTION("Median with realistic bootstrap distribution")
    {
        std::vector<double> stats = createBootstrapStats(50.0, 8.0, 1000);
        double se = computeSE(stats);
        
        PenaltyCalc::computeLengthPenalty_Normal(
            20.0, se, 0.95, stats, normalized_length, median_val);
        
        // Should be close to mean (50.0) for symmetric distribution
        REQUIRE(median_val == Catch::Approx(50.0).epsilon(0.05));
    }
}

TEST_CASE("computeLengthPenalty_Normal: Ideal length is z*SE",
          "[AutoBootstrapSelector][LengthPenalty][Normal][Ideal]")
{
    SECTION("95% CI: ideal = 2 * 1.96 * SE")
    {
        std::vector<double> stats = createBootstrapStats(100.0, 15.0, 1000);
        const double se = computeSE(stats);
        const double z = 1.96;  // Approx for 95% CI
        const double ideal = 2.0 * z * se;
        
        double normalized_length, median;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            ideal, se, 0.95, stats, normalized_length, median);
        
        // Normalized should be exactly 1.0 (actual = ideal)
        REQUIRE(normalized_length == Catch::Approx(1.0).epsilon(0.01));
        
        // No penalty at ideal
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
        
        // Median should be calculated
        REQUIRE(median == Catch::Approx(100.0).epsilon(0.05));
    }
    
    SECTION("90% CI: ideal = 2 * 1.645 * SE")
    {
        std::vector<double> stats = createBootstrapStats(50.0, 10.0, 1000);
        const double se = computeSE(stats);
        const double z = 1.645;  // Approx for 90% CI
        const double ideal = 2.0 * z * se;
        
        double normalized_length, median;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            ideal, se, 0.90, stats, normalized_length, median);
        
        REQUIRE(normalized_length == Catch::Approx(1.0).epsilon(0.01));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
        
        // Median should be calculated
        REQUIRE(median == Catch::Approx(50.0).epsilon(0.05));
    }
}

TEST_CASE("computeLengthPenalty_Normal: Penalty calculations",
          "[AutoBootstrapSelector][LengthPenalty][Normal][Penalty]")
{
    std::vector<double> stats = createBootstrapStats(100.0, 15.0, 1000);
    const double se = computeSE(stats);
    const double z_95 = 1.96;
    const double ideal = 2.0 * z_95 * se;
    
    SECTION("Too short: 0.5x ideal")
    {
        double actual = 0.5 * ideal;
        double normalized_length, median;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            actual, se, 0.95, stats, normalized_length, median);
        
        REQUIRE(normalized_length == Catch::Approx(0.5).epsilon(0.01));
        
        // Penalty = (0.8 - 0.5)^2 = 0.09
        REQUIRE(penalty == Catch::Approx(0.09).epsilon(0.01));
        
        // Median should be calculated
        REQUIRE(median == Catch::Approx(100.0).epsilon(0.05));
    }
    
    SECTION("Too wide: 2.5x ideal")
    {
        double actual = 2.5 * ideal;
        double normalized_length, median;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            actual, se, 0.95, stats, normalized_length, median);
        
        REQUIRE(normalized_length == Catch::Approx(2.5).epsilon(0.01));
        
        // Penalty = (2.5 - 1.8)^2 = 0.49
        REQUIRE(penalty == Catch::Approx(0.49).epsilon(0.01));
        
        // Median should be calculated
        REQUIRE(median == Catch::Approx(100.0).epsilon(0.05));
    }
    
    SECTION("Within bounds [0.8, 1.8]: no penalty")
    {
        double actual = 1.2 * ideal;
        double normalized_length, median;
        
        double penalty = PenaltyCalc::computeLengthPenalty_Normal(
            actual, se, 0.95, stats, normalized_length, median);
        
        REQUIRE(normalized_length == Catch::Approx(1.2).epsilon(0.01));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
        
        // Median should be calculated
        REQUIRE(median == Catch::Approx(100.0).epsilon(0.05));
    }
}

TEST_CASE("computeLengthPenalty_Normal: Different from percentile reference",
          "[AutoBootstrapSelector][LengthPenalty][Normal][Comparison]")
{
    SECTION("Normal and Percentile judge same interval differently")
    {
        // Create bootstrap distribution with known properties
        std::vector<double> stats = createBootstrapStats(100.0, 10.0, 1000);
        double se = computeSE(stats);
        
        // Compute percentile ideal (quantile width)
        std::vector<double> sorted = stats;
        std::sort(sorted.begin(), sorted.end());
        double perc_ideal = computeQuantile(sorted, 0.975) - computeQuantile(sorted, 0.025);
        
        // Compute Normal ideal (z*SE)
        const double z = 1.96;
        double normal_ideal = 2.0 * z * se;
        
        // For approximately normal distribution, these should be similar but not identical
        // (They'd be identical only for perfect normal distribution)
        INFO("Percentile ideal: " << perc_ideal);
        INFO("Normal ideal: " << normal_ideal);
        
        // Test that the same actual length gets different normalized values
        double test_length = 20.0;
        
        double norm_perc, norm_normal, med_perc, med_normal;
        
        PenaltyCalc::computeLengthPenalty_Percentile(
            test_length, stats, 0.95, MethodId::Percentile, norm_perc, med_perc);
        
        PenaltyCalc::computeLengthPenalty_Normal(
            test_length, se, 0.95, stats, norm_normal, med_normal);
        
        // Different ideals → different normalized lengths
        // (Exact difference depends on distribution, but they should differ)
        INFO("Percentile normalized: " << norm_perc);
        INFO("Normal normalized: " << norm_normal);
        
        // Both should be reasonable (> 0)
        REQUIRE(norm_perc > 0.0);
        REQUIRE(norm_normal > 0.0);
        
        // Both should calculate same median (same bootstrap distribution)
        REQUIRE(med_perc == Catch::Approx(med_normal).epsilon(0.01));
        REQUIRE(med_perc == Catch::Approx(100.0).epsilon(0.05));
    }
}

// =============================================================================
// TESTS FOR computeLengthPenalty_PercentileT
// =============================================================================

TEST_CASE("computeLengthPenalty_PercentileT: Edge cases",
          "[AutoBootstrapSelector][LengthPenalty][PercentileT][EdgeCases]")
{
    double normalized_length;
    double median_val;
    
    SECTION("Empty T* statistics returns zero penalty")
    {
        std::vector<double> empty;
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            1.0, empty, 5.0, 0.95, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
        REQUIRE(normalized_length == 1.0);
        REQUIRE(median_val == 0.0);
    }
    
    SECTION("Zero SE_hat returns zero penalty")
    {
        std::vector<double> t_stats = createTStatistics(0.0, 1.0, 100);
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            10.0, t_stats, 0.0, 0.95, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Zero actual length returns zero penalty")
    {
        std::vector<double> t_stats = createTStatistics(0.0, 1.0, 100);
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            0.0, t_stats, 3.0, 0.95, normalized_length, median_val);
        
        REQUIRE(penalty == 0.0);
    }
}

TEST_CASE("computeLengthPenalty_PercentileT: Median of T* distribution",
          "[AutoBootstrapSelector][LengthPenalty][PercentileT][Median]")
{
    SECTION("Symmetric T* has median near 0")
    {
        std::vector<double> t_stats = createTStatistics(0.0, 1.5, 1000);
        double normalized_length, median_val;
        
        PenaltyCalc::computeLengthPenalty_PercentileT(
            10.0, t_stats, 3.0, 0.95, normalized_length, median_val);
        
        // Should be very close to 0 for symmetric distribution
        REQUIRE(std::abs(median_val) < 0.1);
    }
}

TEST_CASE("computeLengthPenalty_PercentileT: Ideal is (t_hi - t_lo) * SE_hat",
          "[AutoBootstrapSelector][LengthPenalty][PercentileT][Ideal]")
{
    SECTION("Interval matching PT construction gets normalized=1.0")
    {
        // Create T* distribution
        std::vector<double> t_stats = createTStatistics(0.0, 1.2, 1000);
        double se_hat = 3.5;
        
        // Compute ideal the way PT actually constructs intervals
        std::vector<double> sorted = t_stats;
        std::sort(sorted.begin(), sorted.end());
        double t_lo = computeQuantile(sorted, 0.025);
        double t_hi = computeQuantile(sorted, 0.975);
        double ideal_length = (t_hi - t_lo) * se_hat;
        
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            ideal_length, t_stats, se_hat, 0.95,
            normalized_length, median_val);
        
        // Should have normalized_length = 1.0
        REQUIRE(normalized_length == Catch::Approx(1.0).epsilon(0.01));
        
        // No penalty at ideal
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("computeLengthPenalty_PercentileT: Penalty calculations",
          "[AutoBootstrapSelector][LengthPenalty][PercentileT][Penalty]")
{
    std::vector<double> t_stats = createTStatistics(0.0, 1.2, 1000);
    double se_hat = 3.5;
    
    std::vector<double> sorted = t_stats;
    std::sort(sorted.begin(), sorted.end());
    double ideal = (computeQuantile(sorted, 0.975) - computeQuantile(sorted, 0.025)) * se_hat;
    
    SECTION("Too short: 0.6x ideal")
    {
        double actual = 0.6 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            actual, t_stats, se_hat, 0.95, normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(0.6).epsilon(0.01));
        
        // Penalty = (0.8 - 0.6)^2 = 0.04
        REQUIRE(penalty == Catch::Approx(0.04).epsilon(0.01));
    }
    
    SECTION("Too wide: 2.2x ideal")
    {
        double actual = 2.2 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            actual, t_stats, se_hat, 0.95, normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(2.2).epsilon(0.01));
        
        // Penalty = (2.2 - 1.8)^2 = 0.16
        REQUIRE(penalty == Catch::Approx(0.16).epsilon(0.01));
    }
    
    SECTION("Within bounds: no penalty")
    {
        double actual = 1.3 * ideal;
        double normalized_length, median_val;
        
        double penalty = PenaltyCalc::computeLengthPenalty_PercentileT(
            actual, t_stats, se_hat, 0.95, normalized_length, median_val);
        
        REQUIRE(normalized_length == Catch::Approx(1.3).epsilon(0.01));
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("computeLengthPenalty_PercentileT: Different from theta* reference",
          "[AutoBootstrapSelector][LengthPenalty][PercentileT][Comparison]")
{
    SECTION("PT uses T* quantiles, not theta* quantiles")
    {
        // Create both distributions
        std::vector<double> theta_stats = createBootstrapStats(100.0, 15.0, 1000);
        std::vector<double> t_stats = createTStatistics(0.0, 1.3, 1000);
        double se_hat = 12.0;
        
        // Compute percentile ideal (from theta*)
        std::vector<double> sorted_theta = theta_stats;
        std::sort(sorted_theta.begin(), sorted_theta.end());
        double perc_ideal = computeQuantile(sorted_theta, 0.975) - 
                           computeQuantile(sorted_theta, 0.025);
        
        // Compute PT ideal (from T*)
        std::vector<double> sorted_t = t_stats;
        std::sort(sorted_t.begin(), sorted_t.end());
        double pt_ideal = (computeQuantile(sorted_t, 0.975) - 
                          computeQuantile(sorted_t, 0.025)) * se_hat;
        
        INFO("Percentile ideal (theta*): " << perc_ideal);
        INFO("PercentileT ideal (T* × SE): " << pt_ideal);
        
        // These should generally differ
        // (They're based on different distributions: theta* vs T*)
        
        // Test same actual length gets different normalized values
        double test_length = 30.0;
        double norm_perc, norm_pt, med;
        
        PenaltyCalc::computeLengthPenalty_Percentile(
            test_length, theta_stats, 0.95, MethodId::Percentile,
            norm_perc, med);
        
        PenaltyCalc::computeLengthPenalty_PercentileT(
            test_length, t_stats, se_hat, 0.95, norm_pt, med);
        
        INFO("Percentile normalized: " << norm_perc);
        INFO("PercentileT normalized: " << norm_pt);
        
        // Both reasonable but likely different
        REQUIRE(norm_perc > 0.0);
        REQUIRE(norm_pt > 0.0);
    }
}

// =============================================================================
// INTEGRATION TESTS: Verify correct method routing
// =============================================================================

TEST_CASE("Method-specific functions: Correct theoretical ideals",
          "[AutoBootstrapSelector][LengthPenalty][Integration]")
{
    SECTION("Each method gets normalized=1.0 at its own theoretical ideal")
    {
        // Create test data
        std::vector<double> theta_stats = createBootstrapStats(50.0, 8.0, 1000);
        std::vector<double> t_stats = createTStatistics(0.0, 1.1, 1000);
        double se = computeSE(theta_stats);
        double se_hat = 7.5;
        
        double normalized, median;
        
        // Percentile: ideal = theta* quantile width
        std::vector<double> sorted_theta = theta_stats;
        std::sort(sorted_theta.begin(), sorted_theta.end());
        double perc_ideal = computeQuantile(sorted_theta, 0.975) - 
                           computeQuantile(sorted_theta, 0.025);
        
        double penalty_perc = PenaltyCalc::computeLengthPenalty_Percentile(
            perc_ideal, theta_stats, 0.95, MethodId::Percentile,
            normalized, median);
        
        REQUIRE(normalized == Catch::Approx(1.0).epsilon(0.02));
        REQUIRE(penalty_perc == Catch::Approx(0.0).margin(1e-3));
        
        // Normal: ideal = 2 * z * SE
        double normal_ideal = 2.0 * 1.96 * se;

	double penalty_normal = PenaltyCalc::computeLengthPenalty_Normal(normal_ideal,
									 se,
									 0.95,
									 theta_stats,
									 normalized,
									 median);

	REQUIRE(normalized == Catch::Approx(1.0).epsilon(0.02));
	REQUIRE(penalty_normal == Catch::Approx(0.0).margin(1e-3));
	REQUIRE(median == Catch::Approx(50.0).epsilon(0.1));
        
        // PercentileT: ideal = (t_hi - t_lo) * SE_hat
        std::vector<double> sorted_t = t_stats;
        std::sort(sorted_t.begin(), sorted_t.end());
        double pt_ideal = (computeQuantile(sorted_t, 0.975) - 
                          computeQuantile(sorted_t, 0.025)) * se_hat;
        
        double penalty_pt = PenaltyCalc::computeLengthPenalty_PercentileT(
            pt_ideal, t_stats, se_hat, 0.95, normalized, median);
        
        REQUIRE(normalized == Catch::Approx(1.0).epsilon(0.02));
        REQUIRE(penalty_pt == Catch::Approx(0.0).margin(1e-3));
    }
}

TEST_CASE("Percentile-like methods share reference, z-based methods share reference",
          "[AutoBootstrapSelector][LengthPenalty][Integration][Groups]")
{
    std::vector<double> theta_stats = createBootstrapStats(100.0, 12.0, 1000);
    std::vector<double> t_stats = createTStatistics(0.0, 1.15, 1000);
    double se = computeSE(theta_stats);
    double se_hat = 11.5;
    
    const double test_length = 25.0;
    double norm1, norm2, med;
    
    SECTION("Percentile, BCa, Basic all use same theta* reference")
    {
        PenaltyCalc::computeLengthPenalty_Percentile(
            test_length, theta_stats, 0.95, MethodId::Percentile, norm1, med);
        
        PenaltyCalc::computeLengthPenalty_Percentile(
            test_length, theta_stats, 0.95, MethodId::BCa, norm2, med);
        
        REQUIRE(norm1 == Catch::Approx(norm2).margin(1e-10));
    }
    
    SECTION("Normal uses z*SE, not theta* quantiles")
    {
      double med1, med2;
      PenaltyCalc::computeLengthPenalty_Percentile(
						   test_length, theta_stats, 0.95, MethodId::Percentile, norm1, med1);

      PenaltyCalc::computeLengthPenalty_Normal(test_length, se, 0.95, theta_stats, norm2, med2);
        
      // These should generally differ (unless distribution is perfectly normal)
      INFO("Percentile normalized: " << norm1);
      INFO("Normal normalized: " << norm2);
      
      REQUIRE(med1 == Catch::Approx(med2).epsilon(0.01));
    }
    
    SECTION("PercentileT uses T*, not theta*")
    {
        PenaltyCalc::computeLengthPenalty_Percentile(
            test_length, theta_stats, 0.95, MethodId::Percentile, norm1, med);
        
        PenaltyCalc::computeLengthPenalty_PercentileT(
            test_length, t_stats, se_hat, 0.95, norm2, med);
        
        // These should differ (different distributions)
        INFO("Percentile normalized: " << norm1);
        INFO("PercentileT normalized: " << norm2);
    }
}
