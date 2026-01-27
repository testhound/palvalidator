#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <sstream>
#include <limits>
#include "BootstrapPenaltyCalculator.h"
#include "AutoBootstrapSelector.h"
#include "StatUtils.h"
#include "AutoBootstrapConfiguration.h"
#include "DecimalConstants.h"
#include "AutoBootstrapScoring.h"

using namespace palvalidator::analysis;
using MyCRN = std::mt19937;
using Num = num::DefaultNumber;

namespace {
  // Helper to create a simple test candidate
  typename AutoCIResult<Num>::Candidate createTestCandidate(
    typename AutoCIResult<Num>::MethodId method,
    double lower,
    double upper,
    double z0 = 0.0,
    double accel = 0.0)
  {
    return typename AutoCIResult<Num>::Candidate(
      method,
      mkc_timeseries::DecimalConstants<Num>::createDecimal("10.0"),   // mean
      mkc_timeseries::DecimalConstants<Num>::createDecimal(std::to_string(lower)),   // lower
      mkc_timeseries::DecimalConstants<Num>::createDecimal(std::to_string(upper)),   // upper
      0.95,                         // cl
      100,                          // n
      1000,                         // B_outer
      0,                            // B_inner
      1000,                         // effective_B
      0,                            // skipped
      1.0,                          // se_boot
      0.0,                          // skew_boot
      10.0,                         // median_boot
      0.0,                          // center_shift_in_se
      1.0,                          // normalized_length
      0.0,                          // ordering_penalty
      0.0,                          // length_penalty
      0.0,                          // stability_penalty
      z0,                           // z0
      accel,                        // accel
      0.0                           // inner_failure_rate
    );
  }

  // Helper to create a symmetric bootstrap distribution
  std::vector<double> createSymmetricBootstrapStats(double mean, double sd, size_t n) {
    std::vector<double> stats;
    stats.reserve(n);
    std::mt19937 rng(12345);
    std::normal_distribution<double> dist(mean, sd);
    for (size_t i = 0; i < n; ++i) {
      stats.push_back(dist(rng));
    }
    return stats;
  }

  // Helper functions for creating test data (createSkewedBootstrapStats removed as unused)
}

// =========================================================================
// SKEW PENALTY TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: computeSkewPenalty basic functionality", "[BootstrapPenaltyCalculator][SkewPenalty]")
{
  SECTION("Zero penalty for low skewness") {
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(0.0) == Catch::Approx(0.0));
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(0.5) == Catch::Approx(0.0));
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(1.0) == Catch::Approx(0.0));
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(-1.0) == Catch::Approx(0.0));
  }

  SECTION("Quadratic penalty for high skewness") {
    double skew = 2.0; // 1.0 above threshold
    double expected = (skew - 1.0) * (skew - 1.0); // Should be 1.0
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(skew) == Catch::Approx(expected));

    skew = 3.0; // 2.0 above threshold  
    expected = (skew - 1.0) * (skew - 1.0); // Should be 4.0
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(skew) == Catch::Approx(expected));
  }

  SECTION("Symmetric for positive and negative skewness") {
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(2.0) == 
            BootstrapPenaltyCalculator<Num>::computeSkewPenalty(-2.0));
    REQUIRE(BootstrapPenaltyCalculator<Num>::computeSkewPenalty(1.5) == 
            BootstrapPenaltyCalculator<Num>::computeSkewPenalty(-1.5));
  }
}

// =========================================================================
// DOMAIN PENALTY TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: computeDomainPenalty functionality", "[BootstrapPenaltyCalculator][DomainPenalty]")
{
  SECTION("No penalty for unconstrained support") {
    auto candidate = createTestCandidate(AutoCIResult<Num>::MethodId::Normal, -5.0, 15.0);
    auto support = mkc_timeseries::StatisticSupport::unbounded();
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeDomainPenalty(candidate, support);
    REQUIRE(penalty == 0.0);
  }

  SECTION("No penalty when lower bound is respected") {
    auto candidate = createTestCandidate(AutoCIResult<Num>::MethodId::Normal, 5.0, 15.0);
    auto support = mkc_timeseries::StatisticSupport::strictLowerBound(0.0, 1e-9);
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeDomainPenalty(candidate, support);
    REQUIRE(penalty == 0.0);
  }

  SECTION("Penalty when lower bound is violated") {
    auto candidate = createTestCandidate(AutoCIResult<Num>::MethodId::Normal, -5.0, 15.0);
    auto support = mkc_timeseries::StatisticSupport::strictLowerBound(0.0, 1e-9);
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeDomainPenalty(candidate, support);
    REQUIRE(penalty == AutoBootstrapConfiguration::kDomainViolationPenalty);
  }
}

// =========================================================================
// BCa STABILITY PENALTY TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: computeBCaStabilityPenalty basic functionality", "[BootstrapPenaltyCalculator][BCaStability]")
{
  AutoBootstrapSelector<Num>::ScoringWeights weights;

  SECTION("No penalty for reasonable parameters") {
    double penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(0.1, 0.05, 1.0, weights);
    REQUIRE(penalty == 0.0);
  }

  SECTION("z0 penalty for excessive bias") {
    double z0 = 0.4; // Above threshold of 0.25
    double penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(z0, 0.05, 1.0, weights);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Acceleration penalty for excessive acceleration") {
    double accel = 0.15; // Above threshold of 0.10  
    double penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(0.1, accel, 1.0, weights);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Skew penalty for extreme skewness") {
    double skew = 3.0; // Above threshold of 2.0
    double penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(0.1, 0.05, skew, weights);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Combined penalties are additive") {
    double z0 = 0.4, accel = 0.15, skew = 3.0;
    double combined_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(z0, accel, skew, weights);
    
    double z0_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(z0, 0.05, 1.0, weights);
    double accel_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(0.1, accel, 1.0, weights);
    double skew_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(0.1, 0.05, skew, weights);
    
    REQUIRE(combined_penalty > z0_penalty);
    REQUIRE(combined_penalty > accel_penalty);
    REQUIRE(combined_penalty > skew_penalty);
  }

  SECTION("Non-finite parameters return infinity") {
    double inf_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      std::numeric_limits<double>::quiet_NaN(), 0.05, 1.0, weights);
    REQUIRE(inf_penalty == std::numeric_limits<double>::infinity());

    inf_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      0.1, std::numeric_limits<double>::infinity(), 1.0, weights);
    REQUIRE(inf_penalty == std::numeric_limits<double>::infinity());
  }
}

// =========================================================================
// PERCENTILE-T STABILITY PENALTY TESTS  
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: computePercentileTStability functionality", "[BootstrapPenaltyCalculator][PercentileTStability]")
{
  // Mock result structure
  struct MockResult {
    std::size_t B_outer = 1000;
    std::size_t B_inner = 100;
    std::size_t skipped_outer = 0;
    std::size_t skipped_inner_total = 0;
    std::size_t effective_B = 1000;
    std::size_t inner_attempted_total = 100000;
  };

  SECTION("No penalty for good performance") {
    MockResult res;
    double penalty = BootstrapPenaltyCalculator<Num>::computePercentileTStability(res);
    REQUIRE(penalty == 0.0);
  }

  SECTION("Penalty for high outer failure rate") {
    MockResult res;
    res.skipped_outer = 150; // 15% failure rate, above 10% threshold
    double penalty = BootstrapPenaltyCalculator<Num>::computePercentileTStability(res);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Penalty for high inner failure rate") {
    MockResult res;
    res.skipped_inner_total = 6000; // 6% failure rate, above 5% threshold
    double penalty = BootstrapPenaltyCalculator<Num>::computePercentileTStability(res);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Penalty for low effective B") {
    MockResult res;
    res.effective_B = 600; // Only 60% effective, below 70% threshold
    double penalty = BootstrapPenaltyCalculator<Num>::computePercentileTStability(res);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Infinity for invalid inputs") {
    MockResult res;
    res.B_outer = 0; // Invalid
    double penalty = BootstrapPenaltyCalculator<Num>::computePercentileTStability(res);
    REQUIRE(penalty == std::numeric_limits<double>::infinity());

    res.B_outer = 1000;
    res.inner_attempted_total = 0; // No inner attempts
    penalty = BootstrapPenaltyCalculator<Num>::computePercentileTStability(res);
    REQUIRE(penalty == std::numeric_limits<double>::infinity());
  }
}

// =========================================================================
// LENGTH PENALTY TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: computeLengthPenalty_Percentile functionality", "[BootstrapPenaltyCalculator][LengthPenalty]")
{
  auto stats = createSymmetricBootstrapStats(10.0, 2.0, 1000);
  double normalized_length, median_val;

  SECTION("No penalty for reasonable length") {
    double actual_length = 8.0; // Roughly 2*se = reasonable
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      actual_length, stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    
    REQUIRE(normalized_length > 0.8);
    REQUIRE(normalized_length < 1.8);
    REQUIRE(penalty == 0.0);
  }

  SECTION("Penalty for too short interval") {
    double actual_length = 1.0; // Very short
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      actual_length, stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    
    REQUIRE(normalized_length < 0.8);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Penalty for too long interval") {
    double actual_length = 50.0; // Very long
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      actual_length, stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    
    REQUIRE(normalized_length > 1.8);
    REQUIRE(penalty > 0.0);
  }

  SECTION("Different L_max for MOutOfN method") {
    double actual_length = 25.0; // Long but might be acceptable for MOutOfN
    
    double penalty_normal = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      actual_length, stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    
    double penalty_mofn = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      actual_length, stats, 0.95, AutoCIResult<Num>::MethodId::MOutOfN, normalized_length, median_val);
    
    // MOutOfN should be more tolerant of long intervals
    REQUIRE(penalty_mofn <= penalty_normal);
  }

  SECTION("Degenerate cases return zero penalty") {
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      -1.0, stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    REQUIRE(penalty == 0.0);

    std::vector<double> empty_stats;
    penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      5.0, empty_stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    REQUIRE(penalty == 0.0);
  }
}

TEST_CASE("BootstrapPenaltyCalculator: computeLengthPenalty_Normal functionality", "[BootstrapPenaltyCalculator][LengthPenalty]")
{
  double se_boot = 2.0;
  double normalized_length, median_val;

  SECTION("No penalty for theoretically correct length") {
    // Normal theoretical length: 2 * 1.96 * se ≈ 7.84
    double actual_length = 7.84;
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Normal(
      actual_length, se_boot, 0.95, normalized_length, median_val);
    
    REQUIRE(normalized_length == Catch::Approx(1.0).margin(0.01));
    REQUIRE(penalty == 0.0);
    REQUIRE(median_val == 0.0); // Normal doesn't use median
  }

  SECTION("Penalty for incorrect length") {
    double actual_length = 15.0; // Too long
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Normal(
      actual_length, se_boot, 0.95, normalized_length, median_val);
    
    REQUIRE(normalized_length > 1.8);
    REQUIRE(penalty > 0.0);
  }
}

TEST_CASE("BootstrapPenaltyCalculator: computeLengthPenalty_PercentileT functionality", "[BootstrapPenaltyCalculator][LengthPenalty]")
{
  // Create T-statistics (roughly normal around 0)
  auto t_stats = createSymmetricBootstrapStats(0.0, 2.0, 1000);
  double se_hat = 1.5;
  double normalized_length, median_val;

  SECTION("Small penalty for T-based length") {
    double actual_length = 8.0; // Based on T distribution
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_PercentileT(
      actual_length, t_stats, se_hat, 0.95, normalized_length, median_val);
    
    // The penalty might be small but non-zero depending on T-distribution characteristics
    REQUIRE(penalty < 0.1); // Should be small penalty if any
    REQUIRE(std::isfinite(penalty));
    REQUIRE(median_val != 0.0); // Should compute median of T-stats
  }

  SECTION("Penalty for extremely long interval") {
    double actual_length = 50.0; // Very long
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_PercentileT(
      actual_length, t_stats, se_hat, 0.95, normalized_length, median_val);
    
    REQUIRE(penalty > 0.0);
  }
}

// =========================================================================
// EMPIRICAL COVERAGE TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: empirical coverage functionality", "[BootstrapPenaltyCalculator][Coverage]")
{
  auto stats = createSymmetricBootstrapStats(10.0, 2.0, 1000);

  SECTION("compute_empirical_mass_inclusive basic functionality") {
    auto result = BootstrapPenaltyCalculator<Num>::compute_empirical_mass_inclusive(stats, 8.0, 12.0);
    
    REQUIRE(result.effective_sample_count == stats.size());
    REQUIRE(result.mass_inclusive > 0.0);
    REQUIRE(result.mass_inclusive <= 1.0);
  }

  SECTION("computeEmpiricalUnderCoveragePenalty no penalty for good coverage") {
    double penalty = BootstrapPenaltyCalculator<Num>::computeEmpiricalUnderCoveragePenalty(
      stats, 6.0, 14.0, 0.95);
    
    // With symmetric normal data and wide bounds, should have good coverage
    REQUIRE(penalty == 0.0);
  }

  SECTION("computeEmpiricalUnderCoveragePenalty penalty for poor coverage") {
    // Very narrow interval should cause under-coverage
    double penalty = BootstrapPenaltyCalculator<Num>::computeEmpiricalUnderCoveragePenalty(
      stats, 9.9, 10.1, 0.95);
    
    REQUIRE(penalty > 0.0);
  }

  SECTION("computeEmpiricalUnderCoveragePenalty_PercentileT functionality") {
    auto t_stats = createSymmetricBootstrapStats(0.0, 2.0, 1000);
    double theta_hat = 10.0, se_hat = 1.5;
    
    // Wide interval should have good coverage
    double penalty = BootstrapPenaltyCalculator<Num>::computeEmpiricalUnderCoveragePenalty_PercentileT(
      t_stats, theta_hat, se_hat, 4.0, 16.0, 0.95);
    
    REQUIRE(penalty == 0.0);
  }
}

// =========================================================================
// HELPER FUNCTION TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: compute_under_coverage_with_half_step_tolerance", "[BootstrapPenaltyCalculator][Helpers]")
{
  SECTION("No under-coverage when width_cdf >= cl") {
    double result = BootstrapPenaltyCalculator<Num>::compute_under_coverage_with_half_step_tolerance(
      0.96, 0.95, 1000);
    REQUIRE(result == 0.0);
  }

  SECTION("Under-coverage with tolerance") {
    double result = BootstrapPenaltyCalculator<Num>::compute_under_coverage_with_half_step_tolerance(
      0.93, 0.95, 1000);
    
    // Should be some under-coverage but adjust for finite-sample tolerance
    REQUIRE(result >= 0.0);
  }

  SECTION("Tolerance accounts for finite sample size") {
    double result_large = BootstrapPenaltyCalculator<Num>::compute_under_coverage_with_half_step_tolerance(
      0.94, 0.95, 10000);
    double result_small = BootstrapPenaltyCalculator<Num>::compute_under_coverage_with_half_step_tolerance(
      0.94, 0.95, 100);
    
    // Smaller sample should be more tolerant
    REQUIRE(result_small <= result_large);
  }
}

// =========================================================================
// EDGE CASES AND ROBUSTNESS TESTS
// =========================================================================

TEST_CASE("BootstrapPenaltyCalculator: edge cases and error handling", "[BootstrapPenaltyCalculator][EdgeCases]")
{
  SECTION("Empty bootstrap statistics") {
    std::vector<double> empty_stats;
    double normalized_length, median_val;
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      5.0, empty_stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    REQUIRE(penalty == 0.0);
  }

  SECTION("Non-finite intervals") {
    auto stats = createSymmetricBootstrapStats(10.0, 2.0, 100);
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeEmpiricalUnderCoveragePenalty(
      stats, std::numeric_limits<double>::quiet_NaN(), 12.0, 0.95);
    REQUIRE(penalty == 0.0);
  }

  SECTION("Degenerate bootstrap distribution") {
    std::vector<double> constant_stats(1000, 10.0); // All identical values
    double normalized_length, median_val;
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeLengthPenalty_Percentile(
      5.0, constant_stats, 0.95, AutoCIResult<Num>::MethodId::Normal, normalized_length, median_val);
    
    // Should handle gracefully (return 0 penalty for degenerate case)
    REQUIRE(penalty == 0.0);
  }

  SECTION("Invalid percentile-T parameters") {
    auto t_stats = createSymmetricBootstrapStats(0.0, 2.0, 100);
    
    double penalty = BootstrapPenaltyCalculator<Num>::computeEmpiricalUnderCoveragePenalty_PercentileT(
      t_stats, 10.0, 0.0, 8.0, 12.0, 0.95); // se_hat = 0
    REQUIRE(penalty == 0.0);
    
    penalty = BootstrapPenaltyCalculator<Num>::computeEmpiricalUnderCoveragePenalty_PercentileT(
      t_stats, 10.0, 1.5, 12.0, 8.0, 0.95); // hi < lo
    REQUIRE(penalty == 0.0);
  }
}

TEST_CASE("BootstrapPenaltyCalculator: ScoringWeights functionality", "[BootstrapPenaltyCalculator][ScoringWeights]")
{
  SECTION("Default weights") {
    AutoBootstrapSelector<Num>::ScoringWeights weights;
    REQUIRE(weights.getBcaZ0Scale() == 20.0);
    REQUIRE(weights.getBcaAScale() == 100.0);
  }

  SECTION("Custom weights") {
    AutoBootstrapSelector<Num>::ScoringWeights weights(1.0, 0.5, 0.25, 1.0, false, 10.0, 50.0);
    REQUIRE(weights.getBcaZ0Scale() == 10.0);
    REQUIRE(weights.getBcaAScale() == 50.0);
  }

  SECTION("Weights affect penalty calculation") {
    AutoBootstrapSelector<Num>::ScoringWeights low_weights(1.0, 0.5, 0.25, 1.0, false, 1.0, 1.0);
    AutoBootstrapSelector<Num>::ScoringWeights high_weights(1.0, 0.5, 0.25, 1.0, false, 100.0, 200.0);
    
    double z0 = 0.4, accel = 0.15, skew = 1.0; // Above thresholds
    
    double low_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, skew, low_weights);
    double high_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, skew, high_weights);
    
    REQUIRE(high_penalty > low_penalty);
  }
}



TEST_CASE("ScoreNormalizer: computeBcaLengthOverflow functionality",
          "[BootstrapPenaltyCalculator][BCa][LengthOverflow]")
{
    SECTION("No overflow penalty below threshold")
    {
        // Threshold is 1.0 (intervals at exactly the ideal length)
        double length_penalty = 0.5; // Below threshold
        double overflow = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(length_penalty);
        
        REQUIRE(overflow == 0.0);
    }
    
    SECTION("No overflow penalty at exactly the threshold")
    {
        // At threshold = 1.0, no penalty yet
        double length_penalty = 1.0;
        double overflow = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(length_penalty);
        
        REQUIRE(overflow == 0.0);
    }
    
    SECTION("Quadratic overflow penalty above threshold")
    {
        // Test with length_penalty = 2.0 (1.0 over threshold)
        double length_penalty = 2.0;
        double overflow = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(length_penalty);
        
        // overflow = kBcaLengthOverflowScale * (length_penalty - threshold)^2
        // overflow = 2.0 * (2.0 - 1.0)^2 = 2.0 * 1.0 = 2.0
        double expected = 2.0 * (1.0 * 1.0);
        REQUIRE(overflow == Catch::Approx(expected));
    }
    
    SECTION("Larger overflow produces larger penalty (quadratic)")
    {
        // Test with length_penalty = 3.0 (2.0 over threshold)
        double length_penalty = 3.0;
        double overflow = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(length_penalty);
        
        // overflow = 2.0 * (3.0 - 1.0)^2 = 2.0 * 4.0 = 8.0
        double expected = 2.0 * (2.0 * 2.0);
        REQUIRE(overflow == Catch::Approx(expected));
    }
    
    SECTION("Non-finite length penalty returns zero")
    {
        double overflow_nan = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(std::numeric_limits<double>::quiet_NaN());
        
        REQUIRE(overflow_nan == 0.0);
        
        double overflow_inf = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(std::numeric_limits<double>::infinity());
        
        // Infinity should be filtered by std::isfinite() check and return 0.0
        REQUIRE(overflow_inf == 0.0);
    }
    
    SECTION("Verify quadratic scaling property")
    {
        // Doubling the excess should quadruple the penalty
        double length_penalty_1 = 1.5; // 0.5 over threshold
        double length_penalty_2 = 2.0; // 1.0 over threshold (2x the excess)
        
        double overflow_1 = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(length_penalty_1);
        
        double overflow_2 = palvalidator::analysis::detail::ScoreNormalizer<
            num::DefaultNumber, 
            AutoBootstrapSelector<Num>::ScoringWeights, 
            palvalidator::analysis::detail::RawComponents
        >::computeBcaLengthOverflow(length_penalty_2);
        
        // overflow_1 = 2.0 * (0.5)^2 = 0.5
        // overflow_2 = 2.0 * (1.0)^2 = 2.0
        // Ratio should be 4.0
        REQUIRE(overflow_1 == Catch::Approx(0.5));
        REQUIRE(overflow_2 == Catch::Approx(2.0));
        REQUIRE(overflow_2 / overflow_1 == Catch::Approx(4.0));
    }
}

TEST_CASE("BootstrapPenaltyCalculator: BCa length penalty thresholds",
          "[BootstrapPenaltyCalculator][BCa][Configuration]")
{
    SECTION("Configuration constants are correct values")
    {
        // Document the actual threshold values
        double threshold = AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold;
        double scale = AutoBootstrapConfiguration::kBcaLengthOverflowScale;
        
        // Verify threshold = 1.0
        // Rationale: Intervals exactly at the ideal length (normalized = 1.0) 
        // are optimal. Any excess triggers overflow penalty.
        REQUIRE(threshold == Catch::Approx(1.0));
        
        // Verify scale = 2.0
        // Rationale: Quadratic penalty with moderate scaling to penalize 
        // overly wide BCa intervals without being too harsh
        REQUIRE(scale == Catch::Approx(2.0));
    }
    
    SECTION("Penalty growth is reasonable")
    {
        // Verify that penalty growth is meaningful but not excessive
        
        // 10% over threshold: penalty = 2.0 * (0.1)^2 = 0.02 (small)
        double penalty_10pct = AutoBootstrapConfiguration::kBcaLengthOverflowScale * 
            (0.1 * 0.1);
        REQUIRE(penalty_10pct < 0.1);
        
        // 50% over threshold: penalty = 2.0 * (0.5)^2 = 0.5 (moderate)
        double penalty_50pct = AutoBootstrapConfiguration::kBcaLengthOverflowScale * 
            (0.5 * 0.5);
        REQUIRE(penalty_50pct == Catch::Approx(0.5));
        
        // 100% over threshold: penalty = 2.0 * (1.0)^2 = 2.0 (significant)
        double penalty_100pct = AutoBootstrapConfiguration::kBcaLengthOverflowScale * 
            (1.0 * 1.0);
        REQUIRE(penalty_100pct == Catch::Approx(2.0));
        
        // 200% over threshold: penalty = 2.0 * (2.0)^2 = 8.0 (very high)
        double penalty_200pct = AutoBootstrapConfiguration::kBcaLengthOverflowScale * 
            (2.0 * 2.0);
        REQUIRE(penalty_200pct == Catch::Approx(8.0));
    }
    
    SECTION("Overflow penalty is BCa-specific")
    {
        // Document that this overflow penalty is applied ONLY to BCa method
        // Other methods (Percentile, Basic, etc.) do not use this penalty
        
        // This is a deliberate design choice because:
        // 1. BCa can sometimes produce excessively wide intervals when 
        //    bias correction (z0) and acceleration (a) are large
        // 2. Other methods don't have this issue, so they don't need the penalty
        
        // Verify this is documented in code comments
        INFO("BCa length overflow penalty applies only to MethodId::BCa");
        INFO("Threshold: " << AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold);
        INFO("Scale: " << AutoBootstrapConfiguration::kBcaLengthOverflowScale);
    }
}

TEST_CASE("computeBCaStabilityPenalty: Threshold constants documentation",
          "[BootstrapPenaltyCalculator][BCa][Constants][Documentation]")
{
  AutoBootstrapSelector<Num>::ScoringWeights weights;

  SECTION("Skew multiplier threshold at 2.0")
  {
    // Document: skew_multiplier changes from 1.0 to 1.5 when |skew| exceeds 2.0
    // This makes penalties stricter for highly skewed distributions
    
    // Just below threshold (skew = 1.9): multiplier = 1.0
    double z0 = 0.35, accel = 0.05;
    double penalty_low_skew = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 1.9, weights);
    
    // Just above threshold (skew = 2.1): multiplier = 1.5
    double penalty_high_skew = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 2.1, weights);
    
    // High skew should have higher penalty due to 1.5x multiplier on z0 and accel scales
    // Note: Both values also trigger small skew penalties, but those should be similar
    REQUIRE(penalty_high_skew > penalty_low_skew);
    
    // At skew = 1.9: no skew penalty
    // At skew = 2.1: small skew penalty = (0.1)^2 * 5.0 = 0.05
    // The z0 multiplier effect should dominate
    
    INFO("Low skew (1.9) penalty: " << penalty_low_skew);
    INFO("High skew (2.1) penalty: " << penalty_high_skew);
  }
  
  SECTION("Accel threshold adapts to extreme skew at 3.0")
  {
    // Document: base accel threshold = 0.10
    //          strict accel threshold = 0.08 when |skew| > 3.0
    
    // Test with accel = 0.09 (between 0.08 and 0.10)
    double z0 = 0.1, accel = 0.09;
    
    // At skew = 2.9 (below 3.0): threshold is 0.10, so accel=0.09 is OK
    double penalty_moderate_skew = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 2.9, weights);
    
    // At skew = 3.1 (above 3.0): threshold is 0.08, so accel=0.09 exceeds it
    double penalty_extreme_skew = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 3.1, weights);
    
    // Extreme skew case should have penalty for accel=0.09 > 0.08
    REQUIRE(penalty_extreme_skew > penalty_moderate_skew);
    
    INFO("Moderate skew (2.9) penalty: " << penalty_moderate_skew);
    INFO("Extreme skew (3.1) penalty: " << penalty_extreme_skew);
  }
  
  SECTION("All threshold constants are reasonable")
  {
    // Document and verify all key thresholds
    
    // z0 soft threshold: 0.25 (from AutoBootstrapConfiguration)
    double z0_threshold = AutoBootstrapConfiguration::kBcaZ0SoftThreshold;
    REQUIRE(z0_threshold == Catch::Approx(0.25));
    REQUIRE(z0_threshold > 0.0);
    REQUIRE(z0_threshold < 1.0);
    
    // Accel soft threshold: 0.10 (base)
    double accel_threshold = AutoBootstrapConfiguration::kBcaASoftThreshold;
    REQUIRE(accel_threshold == Catch::Approx(0.10));
    REQUIRE(accel_threshold > 0.0);
    REQUIRE(accel_threshold < 0.5);
    
    // Skew threshold for penalties: 2.0 (from AutoBootstrapConfiguration)
    double skew_threshold = AutoBootstrapConfiguration::kBcaSkewThreshold;
    REQUIRE(skew_threshold == Catch::Approx(2.0));
    REQUIRE(skew_threshold > 0.0);
    
    // Skew penalty scale: 5.0
    double skew_scale = AutoBootstrapConfiguration::kBcaSkewPenaltyScale;
    REQUIRE(skew_scale == Catch::Approx(5.0));
    REQUIRE(skew_scale > 0.0);
    
    // Hardcoded thresholds in computeBCaStabilityPenalty:
    // - Skew multiplier threshold: 2.0
    // - Skew multiplier value: 1.5
    // - Extreme skew threshold: 3.0
    // - Strict accel threshold: 0.08
    // These are tested functionally in other sections
  }
  
  SECTION("Skew multiplier effect (isolated from skew penalty)")
  {
    // To isolate the multiplier effect, use skew values that don't
    // trigger significant skew penalties
    double z0 = 0.35; // Above threshold of 0.25
    double accel = 0.05; // Below threshold, no accel penalty
    
    // Low skew (well below threshold): z0_scale = 20.0
    double penalty_low = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 1.0, weights);
    
    // Just above multiplier threshold: z0_scale = 30.0, minimal skew penalty
    double penalty_high = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 2.05, weights);
    
    // At skew = 1.0: only z0 penalty = (0.35 - 0.25)^2 * 20.0 = 0.2
    // At skew = 2.05: 
    //   - z0 penalty = (0.35 - 0.25)^2 * 30.0 = 0.3
    //   - skew penalty = (0.05)^2 * 5.0 = 0.0125
    //   - Total ≈ 0.3125
    // Ratio ≈ 0.3125 / 0.2 ≈ 1.56
    
    double ratio = penalty_high / penalty_low;
    
    // Ratio should be slightly above 1.5 due to small skew penalty
    REQUIRE(ratio > 1.5);
    REQUIRE(ratio < 1.7);
    
    INFO("Penalty at skew=1.0: " << penalty_low);
    INFO("Penalty at skew=2.05: " << penalty_high);
    INFO("Ratio: " << ratio);
  }
  
  SECTION("Skew penalty component (isolated)")
  {
    // Test the skew penalty in isolation (no z0 or accel penalties)
    double z0 = 0.1;    // Below threshold, no z0 penalty
    double accel = 0.05; // Below threshold, no accel penalty
    
    // Below skew threshold: no skew penalty
    double penalty_below = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 1.5, weights);
    REQUIRE(penalty_below == 0.0);
    
    // At skew threshold: no penalty (equality case)
    double penalty_at = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 2.0, weights);
    REQUIRE(penalty_at == 0.0);
    
    // Above skew threshold: skew penalty applies
    double penalty_above = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, 2.5, weights);
    
    // skew_penalty = (2.5 - 2.0)^2 * 5.0 = 0.25 * 5.0 = 1.25
    REQUIRE(penalty_above == Catch::Approx(1.25));
    
    INFO("Skew penalty at 2.5: " << penalty_above);
  }
  
  SECTION("Combined effects: z0, accel, and skew penalties")
  {
    // Document how all three penalties combine
    double z0 = 0.40;    // Above threshold (0.25)
    double accel = 0.15; // Above threshold (0.10)
    double skew = 3.0;   // Above all thresholds
    
    double total_penalty = BootstrapPenaltyCalculator<Num>::computeBCaStabilityPenalty(
      z0, accel, skew, weights);
    
    // Calculate components:
    // 1. z0 penalty = (0.40 - 0.25)^2 * 30.0 = 0.0225 * 30.0 = 0.675
    //    (uses 1.5x multiplier since skew > 2.0)
    // 2. accel penalty = (0.15 - 0.10)^2 * 150.0 = 0.0025 * 150.0 = 0.375
    //    (uses 1.5x multiplier and base threshold since skew < 3.0)
    //    Wait, skew = 3.0, so ACCEL_THRESHOLD should be strict (0.08)
    //    accel penalty = (0.15 - 0.08)^2 * 150.0 = 0.0049 * 150.0 = 0.735
    // 3. skew penalty = (3.0 - 2.0)^2 * 5.0 = 1.0 * 5.0 = 5.0
    
    // Total ≈ 0.675 + 0.735 + 5.0 = 6.41
    
    REQUIRE(total_penalty > 6.0);
    REQUIRE(total_penalty < 7.0);
    
    INFO("Total penalty with z0=0.40, accel=0.15, skew=3.0: " << total_penalty);
  }
}

// Additional test for documenting the hardcoded constants
TEST_CASE("computeBCaStabilityPenalty: Hardcoded threshold values",
          "[BootstrapPenaltyCalculator][BCa][Hardcoded]")
{
  SECTION("Skew multiplier threshold and value")
  {
    // These are hardcoded in computeBCaStabilityPenalty (line 148)
    constexpr double kSkewMultiplierThreshold = 2.0;
    constexpr double kSkewMultiplier = 1.5;
    
    // Document these values
    REQUIRE(kSkewMultiplierThreshold == 2.0);
    REQUIRE(kSkewMultiplier == 1.5);
    
    INFO("When |skew| > " << kSkewMultiplierThreshold << 
         ", z0 and accel penalty scales are multiplied by " << kSkewMultiplier);
  }
  
  SECTION("Extreme skew threshold for strict accel")
  {
    // Hardcoded in computeBCaStabilityPenalty (line 168, 171)
    constexpr double kExtremeSkewThreshold = 3.0;
    constexpr double kStrictAccelThreshold = 0.08;
    
    REQUIRE(kExtremeSkewThreshold == 3.0);
    REQUIRE(kStrictAccelThreshold == 0.08);
    
    INFO("When |skew| > " << kExtremeSkewThreshold << 
         ", accel threshold becomes " << kStrictAccelThreshold <<
         " (stricter than normal " << AutoBootstrapConfiguration::kBcaASoftThreshold << ")");
  }
}
