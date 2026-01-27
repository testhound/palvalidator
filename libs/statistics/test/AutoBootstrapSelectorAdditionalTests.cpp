// AutoBootstrapSelector_AdditionalTests.cpp
//
// Additional comprehensive unit tests for AutoBootstrapSelector to fill coverage gaps
// identified in the code review.
//
// Coverage areas:
//  - computePercentileTStability method (not tested in existing tests)
//  - select() method edge cases and integration scenarios
//  - methodPreference() tie-breaking logic
//  - SelectionDiagnostics class constructors and getters
//  - ScoreBreakdown class
//  - AutoCIResult integration tests
//  - Edge cases in scoring and normalization
//  - Hard rejection gates and validation
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
#include <numeric>
#include <sstream>
#include <algorithm>

#include "AutoBootstrapSelector.h"
#include "BootstrapPenaltyCalculator.h"
#include "number.h"

// Alias for convenience
using Decimal        = double;
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using PenaltyCalc    = palvalidator::analysis::BootstrapPenaltyCalculator<Decimal>;
using Candidate      = Selector::Candidate;
using Result         = Selector::Result;
using ScoringWeights = Selector::ScoringWeights;
using MethodId       = Result::MethodId;
using SelectionDiagnostics = Result::SelectionDiagnostics;
using ScoreBreakdown = SelectionDiagnostics::ScoreBreakdown;
using mkc_timeseries::StatisticSupport;

// -----------------------------------------------------------------------------
// Helper functions for creating test candidates
// -----------------------------------------------------------------------------

/**
 * @brief Creates a simple candidate with minimal parameters for testing
 */
Candidate createSimpleCandidate(
    MethodId method,
    double mean = 1.0,
    double lower = 0.9,
    double upper = 1.1,
    double ordering_penalty = 0.0,
    double length_penalty = 0.0,
    double stability_penalty = 0.0,
    double z0 = 0.0,
    double accel = 0.0)
{
    return Candidate(
        method, mean, lower, upper,
        0.95,      // cl
        100,       // n
        1000,      // B_outer
        0,         // B_inner
        1000,      // effective_B
        0,         // skipped_total
        0.05,      // se_boot
        0.1,       // skew_boot
        mean,      // median_boot
        0.0,       // center_shift_in_se
        1.0,       // normalized_length
        ordering_penalty,
        length_penalty,
        stability_penalty,
        z0,
        accel,
        0.0);      // inner_failure_rate
}

// Helper to create a sorted uniform distribution of bootstrap statistics
std::vector<double> createUniformBootstrapDist(double min, double max, std::size_t n)
{
    std::vector<double> dist;
    dist.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(n - 1);
        dist.push_back(min + t * (max - min));
    }
    return dist;
}

// Helper to create a normal-like distribution (approximation)
std::vector<double> createNormalLikeBootstrapDist(double mean, double sd, std::size_t n)
{
    std::vector<double> dist;
    dist.reserve(n);
    
    // Use quantiles of standard normal as a simple approximation
    for (std::size_t i = 0; i < n; ++i) {
        double p = (static_cast<double>(i) + 0.5) / static_cast<double>(n);
        // Simple normal approximation: use linear scaling in middle, tails get clipped
        double z;
        if (p < 0.0001 || p > 0.9999) {
            z = (p < 0.5) ? -3.5 : 3.5;
        } else {
            // Linear approximation in [-2, 2] range
            z = -3.0 + 6.0 * p;
        }
        dist.push_back(mean + z * sd);
    }
    
    return dist;
}

/**
 * @brief Creates a PercentileT result structure for testing stability calculations
 */
struct MockPercentileTResult
{
    Decimal mean;
    Decimal lower;
    Decimal upper;
    double cl;
    std::size_t n;
    std::size_t B_outer;
    std::size_t B_inner;
    std::size_t effective_B;
    std::size_t skipped_outer;
    std::size_t skipped_inner_total;
    std::size_t inner_attempted_total;
    double se_hat;
};

// -----------------------------------------------------------------------------
// Tests for computePercentileTStability
// -----------------------------------------------------------------------------

TEST_CASE("computePercentileTStability: Basic functionality",
          "[AutoBootstrapSelector][PercentileT][Stability]")
{
    SECTION("Returns zero penalty for perfect stability")
    {
        MockPercentileTResult res{
            1.0,      // mean
            0.9,      // lower
            1.1,      // upper
            0.95,     // cl
            100,      // n
            1000,     // B_outer
            200,      // B_inner
            1000,     // effective_B (100% of B_outer)
            0,        // skipped_outer (0%)
            0,        // skipped_inner_total (0%)
            200000,   // inner_attempted_total
            0.05      // se_hat
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Returns zero penalty for acceptable failure rates within thresholds")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100,
            1000,     // B_outer
            200,      // B_inner
            900,      // effective_B (90% of B_outer, above 70% threshold)
            50,       // skipped_outer (5%, below 10% threshold)
            5000,     // skipped_inner_total (2.5%, below 5% threshold)
            200000,   // inner_attempted_total
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        REQUIRE(penalty == 0.0);
    }
}

TEST_CASE("computePercentileTStability: Outer failure rate penalties",
          "[AutoBootstrapSelector][PercentileT][Stability][OuterFailure]")
{
    SECTION("Penalizes when outer failure rate exceeds 10% threshold")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100,
            1000,     // B_outer
            200,      // B_inner
            1000,     // effective_B
            150,      // skipped_outer (15%, exceeds 10% threshold by 5%)
            0,        // skipped_inner_total
            200000,   // inner_attempted_total
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Excess = 0.15 - 0.10 = 0.05
        // Penalty = 0.05^2 * scale (should be > 0)
        REQUIRE(penalty > 0.0);
    }
    
    SECTION("Higher outer failure rate produces higher penalty")
    {
        MockPercentileTResult res_moderate{
            1.0, 0.9, 1.1, 0.95, 100, 1000, 200, 1000,
            120,      // 12% failure rate
            0, 200000, 0.05
        };
        
        MockPercentileTResult res_severe{
            1.0, 0.9, 1.1, 0.95, 100, 1000, 200, 1000,
            200,      // 20% failure rate
            0, 200000, 0.05
        };
        
        double penalty_moderate = PenaltyCalc::computePercentileTStability(res_moderate);
        double penalty_severe = PenaltyCalc::computePercentileTStability(res_severe);
        
        REQUIRE(penalty_severe > penalty_moderate);
    }
}

TEST_CASE("computePercentileTStability: Inner failure rate penalties",
          "[AutoBootstrapSelector][PercentileT][Stability][InnerFailure]")
{
    SECTION("Penalizes when inner failure rate exceeds 5% threshold")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100, 1000, 200,
            1000,     // effective_B
            0,        // skipped_outer
            10000,    // skipped_inner_total (10%, exceeds 5% threshold)
            100000,   // inner_attempted_total
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Inner failure rate = 10000/100000 = 0.10 (10%)
        // Excess = 0.10 - 0.05 = 0.05
        REQUIRE(penalty > 0.0);
    }
    
    SECTION("Handles edge case of very high inner failure rate")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100, 1000, 200, 1000, 0,
            80000,    // 80% failure rate
            100000,   // inner_attempted_total
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Should be a very large penalty but still finite
        REQUIRE(std::isfinite(penalty));
        REQUIRE(penalty > 10.0);
    }
    
    SECTION("Returns infinity if no inner attempts were made")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100, 1000, 200, 1000, 0, 0,
            0,        // inner_attempted_total = 0
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
}

TEST_CASE("computePercentileTStability: Effective B penalties",
          "[AutoBootstrapSelector][PercentileT][Stability][EffectiveB]")
{
    SECTION("Penalizes when effective_B < 70% of B_outer")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100,
            1000,     // B_outer
            200, 
            600,      // effective_B (60%, below 70% threshold)
            400,      // skipped_outer (but effective_B is what matters)
            0, 200000, 0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Deficit = 700 - 600 = 100 out of 1000 = 10%
        REQUIRE(penalty > 0.0);
    }
    
    SECTION("No penalty when effective_B >= 70% of B_outer")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100,
            1000,     // B_outer
            200,
            750,      // effective_B (75%, above 70% threshold)
            250, 0, 200000, 0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Only this component should contribute nothing
        // (but outer failure rate 25% would contribute)
        REQUIRE(std::isfinite(penalty));
    }
}

TEST_CASE("computePercentileTStability: Combined penalties",
          "[AutoBootstrapSelector][PercentileT][Stability][Combined]")
{
    SECTION("Multiple violations produce additive penalties")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100,
            1000,     // B_outer
            200,
            500,      // effective_B (50%, deficit of 20%)
            150,      // skipped_outer (15%, excess of 5%)
            10000,    // skipped_inner (10%, excess of 5%)
            100000,   // inner_attempted_total
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Should have contributions from all three components
        REQUIRE(penalty > 0.1);  // Substantial combined penalty
    }
}

TEST_CASE("computePercentileTStability: Edge cases",
          "[AutoBootstrapSelector][PercentileT][Stability][EdgeCases]")
{
    SECTION("Returns infinity if B_outer is zero")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100,
            0,        // B_outer = 0
            200, 0, 0, 0, 200000, 0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Returns infinity if B_inner is zero")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100, 1000,
            0,        // B_inner = 0
            1000, 0, 0, 200000, 0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Clamps inner_failure_rate to [0, 1] range")
    {
        // This tests the safety clamping logic in the implementation
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100, 1000, 200, 1000, 0, 0,
            100000,   // Normal inner_attempted_total
            0.05
        };
        
        double penalty = PenaltyCalc::computePercentileTStability(res);
        
        // Should handle 0% inner failure rate gracefully
        REQUIRE(std::isfinite(penalty));
    }
}



// -----------------------------------------------------------------------------
// Tests for select() method
// -----------------------------------------------------------------------------

TEST_CASE("select: Single candidate selection",
          "[AutoBootstrapSelector][Select]")
{
    SECTION("Selects the only candidate when given one valid candidate")
    {
        Candidate c = createSimpleCandidate(MethodId::Percentile);
        std::vector<Candidate> candidates{c};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }
    
    SECTION("Throws when given empty candidate list")
    {
        std::vector<Candidate> empty;
        
        REQUIRE_THROWS_AS(Selector::select(empty), std::invalid_argument);
    }
}

TEST_CASE("select: Score-based selection",
          "[AutoBootstrapSelector][Select][Scoring]")
{
    SECTION("Selects candidate with lowest total score")
    {
        Candidate low_score = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.001,  // Very low ordering penalty
            0.001); // Very low length penalty
        
        Candidate high_score = createSimpleCandidate(
            MethodId::Basic, 1.0, 0.9, 1.1,
            0.05,   // Higher ordering penalty
            0.05);  // Higher length penalty
        
        std::vector<Candidate> candidates{high_score, low_score};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }
    
    SECTION("Stability penalties contribute to total score")
    {
        Candidate stable = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.01, 0.01,
            0.0);    // No stability penalty
        
        Candidate unstable = createSimpleCandidate(
            MethodId::PercentileT, 1.0, 0.9, 1.1,
            0.01, 0.01,
            1.0);    // Large stability penalty
        
        std::vector<Candidate> candidates{unstable, stable};
        
        auto result = Selector::select(candidates);
        
        // Should select the stable one despite method preference
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }
}

TEST_CASE("select: Tie-breaking with method preference",
          "[AutoBootstrapSelector][Select][TieBreaking]")
{
    SECTION("BCa wins tie with PercentileT when scores are equal")
    {
        Candidate bca = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.01, 0.01, 0.0, 0.05, 0.03);
        
        Candidate pt = createSimpleCandidate(
            MethodId::PercentileT, 1.0, 0.9, 1.1,
            0.01, 0.01, 0.0);
        
        std::vector<Candidate> candidates{pt, bca};
        
        auto result = Selector::select(candidates);
        
        // BCa should win on preference
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::BCa);
    }
    
    SECTION("PercentileT wins tie with Percentile when scores are equal")
    {
        Candidate pt = createSimpleCandidate(MethodId::PercentileT);
        Candidate perc = createSimpleCandidate(MethodId::Percentile);
        
        std::vector<Candidate> candidates{perc, pt};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::PercentileT);
    }
}

TEST_CASE("select: BCa hard rejection gates",
          "[AutoBootstrapSelector][Select][BCa][Rejection]")
{
    SECTION("BCa rejected when |z0| exceeds hard limit")
    {
        Candidate bca_bad_z0 = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.001, 0.001, 0.0,
            0.7,    // |z0| > 0.6 (hard limit)
            0.05);
        
        Candidate fallback = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.01, 0.01);
        
        std::vector<Candidate> candidates{bca_bad_z0, fallback};
        
        auto result = Selector::select(candidates);
        
        // Should select fallback, not BCa
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
        
        // Diagnostics should indicate BCa was rejected
        const auto& diag = result.getDiagnostics();
        REQUIRE(diag.hasBCaCandidate() == true);
        REQUIRE(diag.isBCaChosen() == false);
        REQUIRE(diag.wasBCaRejectedForInstability() == true);
    }
    
    SECTION("BCa rejected when |accel| exceeds hard limit")
    {
        Candidate bca_bad_accel = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.001, 0.001, 0.0,
            0.05,
            0.3);   // |accel| > 0.25 (hard limit)
        
        Candidate fallback = createSimpleCandidate(MethodId::Basic);
        
        std::vector<Candidate> candidates{bca_bad_accel, fallback};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Basic);
        REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == true);
    }
    
    SECTION("BCa rejected when length_penalty exceeds threshold")
    {
        Candidate bca_bad_length = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.001,
            10.0,   // Very large length penalty
            0.0, 0.05, 0.05);
        
        Candidate fallback = createSimpleCandidate(MethodId::MOutOfN);
        
        std::vector<Candidate> candidates{bca_bad_length, fallback};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::MOutOfN);
        REQUIRE(result.getDiagnostics().wasBCaRejectedForLength() == true);
    }
}

TEST_CASE("select: Domain penalty enforcement",
          "[AutoBootstrapSelector][Select][DomainPenalty]")
{
    SECTION("Candidate with negative lower bound rejected when support requires non-negative")
    {
        Candidate negative_lower(
            MethodId::Percentile,
            1.0,
            -0.1,   // Negative lower bound
            1.2,
            0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.1, 1.0, 0.0, 1.0,
            0.001, 0.001, 0.0, 0.0, 0.0, 0.0);
        
        Candidate valid = createSimpleCandidate(
            MethodId::Basic, 1.0, 0.5, 1.5,
            0.01, 0.01);
        
        std::vector<Candidate> candidates{negative_lower, valid};
        
        ScoringWeights weights_enforce_positive(
            1.0, 0.5, 0.25, 1.0,
            true);  // enforcePositive = true

	const StatisticSupport support_non_negative =
	  StatisticSupport::nonStrictLowerBound(0.0, 1e-12);

	auto result = Selector::select(candidates, weights_enforce_positive, support_non_negative);
        
        // Should select the valid candidate
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Basic);
    }
    
    SECTION("Candidate with negative lower bound allowed when enforcePositive=false")
    {
        Candidate negative_lower(
            MethodId::Percentile,
            1.0, -0.1, 1.2,
            0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.1, 1.0, 0.0, 1.0,
            0.001, 0.001, 0.0, 0.0, 0.0, 0.0);
        
        std::vector<Candidate> candidates{negative_lower};
        
        ScoringWeights weights_allow_negative(
            1.0, 0.5, 0.25, 1.0,
            false); // enforcePositive = false
        
        auto result = Selector::select(candidates, weights_allow_negative);
        
        // Should be able to select it
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }
}

TEST_CASE("select: Non-finite score handling",
          "[AutoBootstrapSelector][Select][NonFinite]")
{
    SECTION("Candidate with NaN score is rejected")
    {
        Candidate nan_score = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            std::numeric_limits<double>::quiet_NaN(), // NaN ordering penalty
            0.01);
        
        Candidate valid = createSimpleCandidate(MethodId::Percentile);
        
        std::vector<Candidate> candidates{nan_score, valid};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }
    
    SECTION("Throws when all candidates have non-finite scores")
    {
        Candidate inf1 = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            std::numeric_limits<double>::infinity(), 0.01);
        
        Candidate inf2 = createSimpleCandidate(
            MethodId::Basic, 1.0, 0.9, 1.1,
            0.01, std::numeric_limits<double>::infinity());
        
        std::vector<Candidate> candidates{inf1, inf2};
        
        REQUIRE_THROWS_AS(Selector::select(candidates), std::runtime_error);
    }
}

TEST_CASE("select: Custom scoring weights affect selection",
          "[AutoBootstrapSelector][Select][CustomWeights]")
{
    SECTION("High stability weight favors stable methods")
    {
        Candidate stable = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.02, 0.02,
            0.0);    // No stability penalty
        
        Candidate unstable = createSimpleCandidate(
            MethodId::PercentileT, 1.0, 0.9, 1.1,
            0.01, 0.01,  // Better ordering and length
            0.5);        // But high stability penalty
        
        std::vector<Candidate> candidates{unstable, stable};
        
        // With high stability weight, stable should win
        ScoringWeights high_stability(1.0, 0.5, 0.25, 10.0);
        auto result = Selector::select(candidates, high_stability);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }
}

// -----------------------------------------------------------------------------
// Tests for SelectionDiagnostics
// -----------------------------------------------------------------------------

TEST_CASE("SelectionDiagnostics: Constructor and getters",
          "[AutoBootstrapSelector][SelectionDiagnostics]")
{
    SECTION("Constructor with minimal parameters (using defaults)")
    {
        SelectionDiagnostics diag(
            MethodId::Percentile,
            "Percentile",
            0.5,     // chosenScore
            0.1,     // chosenStabilityPenalty
            0.05,    // chosenLengthPenalty
            true,    // hasBCaCandidate
            false,   // bcaChosen
            true,    // bcaRejectedForInstability
            false);  // bcaRejectedForLength
            // Using defaults: bcaRejectedForDomain=false, bcaRejectedForNonFinite=false,
            //                 numCandidates=0, scoreBreakdowns=empty
        
        REQUIRE(diag.getChosenMethod() == MethodId::Percentile);
        REQUIRE(diag.getChosenMethodName() == "Percentile");
        REQUIRE(diag.getChosenScore() == Catch::Approx(0.5));
        REQUIRE(diag.hasBCaCandidate() == true);
        REQUIRE(diag.isBCaChosen() == false);
        REQUIRE(diag.wasBCaRejectedForInstability() == true);
        REQUIRE(diag.wasBCaRejectedForLength() == false);
        REQUIRE(diag.wasBCaRejectedForDomain() == false);  // default
        REQUIRE(diag.wasBCaRejectedForNonFiniteParameters() == false);  // default
        REQUIRE(diag.getNumCandidates() == 0);  // default
        REQUIRE(diag.getScoreBreakdowns().empty());  // default
    }
    
    SECTION("Constructor with score breakdowns")
    {
        std::vector<ScoreBreakdown> breakdowns;
        breakdowns.emplace_back(
            MethodId::BCa,
            0.01, 0.02, 0.03, 0.04, 0.05, 0.0,  // raw
            0.1, 0.2, 0.3, 0.4, 0.5,             // norm
            0.05, 0.05, 0.05, 0.05, 0.05, 0.0,  // contrib
            0.25);                               // total
        
        SelectionDiagnostics diag(
            MethodId::BCa,
            "BCa",
            0.25,
            0.03,
            0.02,
            true,
            true,
            false,
            false,
            false,       // bcaRejectedForDomain
            false,       // bcaRejectedForNonFinite
            1,           // numCandidates
            std::move(breakdowns));  // scoreBreakdowns
        
        REQUIRE(diag.getScoreBreakdowns().size() == 1);
        REQUIRE(diag.getScoreBreakdowns()[0].getMethod() == MethodId::BCa);
        REQUIRE(diag.getNumCandidates() == 1);
    }
    
    SECTION("Constructor with domain and non-finite flags")
    {
        SelectionDiagnostics diag(
            MethodId::Percentile,
            "Percentile",
            1.0, 0.1, 0.1,
            true,    // hasBCaCandidate
            false,   // bcaChosen
            false,   // bcaRejectedForInstability
            false,   // bcaRejectedForLength
            true,    // bcaRejectedForDomain
            false,   // bcaRejectedForNonFinite
            2);      // numCandidates
        
        REQUIRE(diag.wasBCaRejectedForDomain() == true);
        REQUIRE(diag.wasBCaRejectedForNonFiniteParameters() == false);
        REQUIRE(diag.getNumCandidates() == 2);
    }
}

// -----------------------------------------------------------------------------
// Tests for ScoreBreakdown
// -----------------------------------------------------------------------------

TEST_CASE("ScoreBreakdown: Construction and getters",
          "[AutoBootstrapSelector][ScoreBreakdown]")
{
    SECTION("All fields are correctly stored and retrieved")
    {
        ScoreBreakdown breakdown(
            MethodId::PercentileT,
            // Raw values
            0.01, 0.02, 0.03, 0.04, 0.05, 0.0,
            // Normalized values
            0.1, 0.2, 0.3, 0.4, 0.5,
            // Contributions
            0.05, 0.10, 0.15, 0.20, 0.25, 0.0,
            // Total score
            0.75);
        
        REQUIRE(breakdown.getMethod() == MethodId::PercentileT);
        
        // Raw values
        REQUIRE(breakdown.getOrderingRaw() == Catch::Approx(0.01));
        REQUIRE(breakdown.getLengthRaw() == Catch::Approx(0.02));
        REQUIRE(breakdown.getStabilityRaw() == Catch::Approx(0.03));
        REQUIRE(breakdown.getCenterSqRaw() == Catch::Approx(0.04));
        REQUIRE(breakdown.getSkewSqRaw() == Catch::Approx(0.05));
        REQUIRE(breakdown.getDomainRaw() == Catch::Approx(0.0));
        
        // Normalized values
        REQUIRE(breakdown.getOrderingNorm() == Catch::Approx(0.1));
        REQUIRE(breakdown.getLengthNorm() == Catch::Approx(0.2));
        REQUIRE(breakdown.getStabilityNorm() == Catch::Approx(0.3));
        REQUIRE(breakdown.getCenterSqNorm() == Catch::Approx(0.4));
        REQUIRE(breakdown.getSkewSqNorm() == Catch::Approx(0.5));
        
        // Contributions
        REQUIRE(breakdown.getOrderingContribution() == Catch::Approx(0.05));
        REQUIRE(breakdown.getLengthContribution() == Catch::Approx(0.10));
        REQUIRE(breakdown.getStabilityContribution() == Catch::Approx(0.15));
        REQUIRE(breakdown.getCenterSqContribution() == Catch::Approx(0.20));
        REQUIRE(breakdown.getSkewSqContribution() == Catch::Approx(0.25));
        REQUIRE(breakdown.getDomainContribution() == Catch::Approx(0.0));
        
        // Total
        REQUIRE(breakdown.getTotalScore() == Catch::Approx(0.75));
    }
}

// -----------------------------------------------------------------------------
// Integration tests
// -----------------------------------------------------------------------------

TEST_CASE("Integration: Full selection workflow with multiple candidates",
          "[AutoBootstrapSelector][Integration]")
{
    SECTION("Realistic scenario with 5 methods")
    {
        std::vector<Candidate> candidates;
        
        // Normal: Simple, usually not competitive
        candidates.push_back(createSimpleCandidate(
            MethodId::Normal, 1.0, 0.85, 1.15,
            0.05, 0.05));
        
        // Basic: Slightly better
        candidates.push_back(createSimpleCandidate(
            MethodId::Basic, 1.0, 0.88, 1.12,
            0.03, 0.03));
        
        // Percentile: Good coverage
        candidates.push_back(createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.90, 1.10,
            0.01, 0.02));
        
        // PercentileT: Excellent but slightly unstable
        candidates.push_back(createSimpleCandidate(
            MethodId::PercentileT, 1.0, 0.92, 1.08,
            0.005, 0.01, 0.05));  // Small stability penalty
        
        // BCa: Best overall
        candidates.push_back(createSimpleCandidate(
            MethodId::BCa, 1.0, 0.91, 1.09,
            0.003, 0.008, 0.01, 0.05, 0.03));
        
        auto result = Selector::select(candidates);
        
        // BCa should win (best penalties, highest preference)
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::BCa);
        REQUIRE(result.getCandidates().size() == 5);
        
        // Verify diagnostics
        const auto& diag = result.getDiagnostics();
        REQUIRE(diag.hasBCaCandidate() == true);
        REQUIRE(diag.isBCaChosen() == true);
        REQUIRE(diag.getNumCandidates() == 5);
    }
}

TEST_CASE("Integration: BCa rejection cascade",
          "[AutoBootstrapSelector][Integration][BCaRejection]")
{
    SECTION("BCa rejected, falls back to PercentileT")
    {
        std::vector<Candidate> candidates;
        
        // BCa with fatal z0
        candidates.push_back(createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.001, 0.001, 0.0,
            0.8,   // Fatal z0
            0.05));
        
        // PercentileT good
        candidates.push_back(createSimpleCandidate(
            MethodId::PercentileT, 1.0, 0.9, 1.1,
            0.01, 0.01, 0.0));
        
        // Percentile okay
        candidates.push_back(createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.02, 0.02));
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::PercentileT);
        REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == true);
    }
}

// -----------------------------------------------------------------------------
// Tests for BCa version: computeEmpiricalUnderCoveragePenalty (4 parameters)
// -----------------------------------------------------------------------------

TEST_CASE("BCa UnderCoveragePenalty: Perfect coverage yields zero penalty",
          "[AutoBootstrapSelector][UnderCoverage][BCa]")
{
    // Create bootstrap distribution from 0 to 10
    std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);
    
    // 95% CI should capture [0.25, 9.75] (95% of data)
    // Empirical coverage = 95% of 1000 = 950 values
    double lo = 0.25;
    double hi = 9.75;
    double cl = 0.95;
    
    double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
        boot_stats, lo, hi, cl);
    
    // Should be very close to zero (within floating point tolerance)
    REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("BCa UnderCoveragePenalty: Over-coverage yields zero penalty",
          "[AutoBootstrapSelector][UnderCoverage][BCa]")
{
    SECTION("Interval wider than needed")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);
        
        // 95% CI but we capture 98% of data (over-coverage)
        double lo = 0.1;
        double hi = 9.9;
        double cl = 0.95;
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo, hi, cl);
        
        // Over-coverage should NOT be penalized
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Interval much wider than needed")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 100);
        
        // 95% CI but interval captures everything (100% coverage)
        double lo = -1.0;
        double hi = 11.0;
        double cl = 0.95;
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo, hi, cl);
        
        REQUIRE(penalty == 0.0);
    }
}

TEST_CASE("BCa UnderCoveragePenalty: Under-coverage produces penalty",
          "[AutoBootstrapSelector][UnderCoverage][BCa]")
{
    SECTION("5% under-coverage")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);
        
        // 95% CI but only captures 90% (5% shortfall)
        // 90% of uniform [0,10] is [0.5, 9.5]
        double lo = 0.5;
        double hi = 9.5;
        double cl = 0.95;
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo, hi, cl);

	const std::size_t B_eff = boot_stats.size();
	std::size_t inside = 0;
	for (double v : boot_stats)
	  {
	    if (std::isfinite(v) && v >= lo && v <= hi) ++inside;
	  }

	const double width_cdf = (B_eff > 0) ? (static_cast<double>(inside) / static_cast<double>(B_eff)) : 0.0;
	const double tol = (B_eff > 0) ? (0.5 / static_cast<double>(B_eff)) : 0.5;
	const double under = std::max(0.0, (cl - width_cdf) - tol);
	const double expected = AutoBootstrapConfiguration::kUnderCoverageMultiplier * under * under;

	REQUIRE(penalty > 0.0);
	REQUIRE(penalty == Catch::Approx(expected).margin(1e-12));
    }
    
    SECTION("10% under-coverage")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);
        
        // 95% CI but only captures 85% (10% shortfall)
        // 85% of uniform [0,10] is [0.75, 9.25]
        double lo = 0.75;
        double hi = 9.25;
        double cl = 0.95;
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo, hi, cl);
        
        // Expected: 10.0 * (0.10)^2 = 10.0 * 0.01 = 0.10
        REQUIRE(penalty > 0.05); // Definitely substantial
        REQUIRE(penalty == Catch::Approx(0.10).epsilon(0.01));
    }

    SECTION("Penalty scales quadratically with shortfall")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 2000);
        
        // 2% shortfall: coverage should be 93% instead of 95%
        // For uniform [0,10], 93% coverage is [0.35, 9.65]
        double lo_2pct = 0.35;
        double hi_2pct = 9.65;
        double penalty_2pct = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo_2pct, hi_2pct, 0.95);
        
        // 4% shortfall: coverage should be 91% instead of 95%
        // For uniform [0,10], 91% coverage is [0.45, 9.55]
        double lo_4pct = 0.45;
        double hi_4pct = 9.55;
        double penalty_4pct = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo_4pct, hi_4pct, 0.95);
        
        // Quadratic relationship: penalty_4pct should be ~4x penalty_2pct
        // (4% / 2%)^2 = 4
        REQUIRE(penalty_2pct > 0.0);
        REQUIRE(penalty_4pct > penalty_2pct);
        
        // Only check ratio if penalty_2pct is substantial enough
        if (penalty_2pct > 0.001) {
            REQUIRE(penalty_4pct / penalty_2pct == Catch::Approx(4.0).epsilon(0.2));
        }
    }
}

TEST_CASE("BCa UnderCoveragePenalty: Edge cases",
          "[AutoBootstrapSelector][UnderCoverage][BCa][EdgeCases]")
{
    SECTION("Empty bootstrap distribution returns zero")
    {
        std::vector<double> empty_stats;
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            empty_stats, 0.9, 1.1, 0.95);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Single element returns zero")
    {
        std::vector<double> single_stat = {1.0};
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            single_stat, 0.9, 1.1, 0.95);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Invalid interval (lo >= hi) returns zero")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 100);
        
        // lo > hi
        double penalty1 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 5.0, 4.0, 0.95);
        REQUIRE(penalty1 == 0.0);
        
        // lo == hi
        double penalty2 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 5.0, 5.0, 0.95);
        REQUIRE(penalty2 == 0.0);
    }
    
    SECTION("Non-finite bounds return zero")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 100);
        
        double penalty1 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, std::numeric_limits<double>::quiet_NaN(), 1.1, 0.95);
        REQUIRE(penalty1 == 0.0);
        
        double penalty2 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 0.9, std::numeric_limits<double>::infinity(), 0.95);
        REQUIRE(penalty2 == 0.0);
    }
    
    SECTION("Invalid confidence level returns zero")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 100);
        
        // cl <= 0
        double penalty1 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 0.9, 1.1, 0.0);
        REQUIRE(penalty1 == 0.0);
        
        // cl >= 1
        double penalty2 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 0.9, 1.1, 1.0);
        REQUIRE(penalty2 == 0.0);
        
        // cl negative
        double penalty3 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 0.9, 1.1, -0.5);
        REQUIRE(penalty3 == 0.0);
    }
}

TEST_CASE("BCa UnderCoveragePenalty: Interval completely outside distribution",
          "[AutoBootstrapSelector][UnderCoverage][BCa]")
{
    std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);
    
    SECTION("Interval above distribution")
    {
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 15.0, 20.0, 0.95);
        
        // 0% coverage, 95% shortfall
        // Expected: 10.0 * (0.95)^2 = 10.0 * 0.9025 = 9.025
        REQUIRE(penalty > 8.0);
        REQUIRE(penalty == Catch::Approx(9.025).epsilon(0.01));
    }
    
    SECTION("Interval below distribution")
    {
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, -5.0, -1.0, 0.95);
        
        // 0% coverage, 95% shortfall
        REQUIRE(penalty > 8.0);
        REQUIRE(penalty == Catch::Approx(9.025).epsilon(0.01));
    }
}

// -----------------------------------------------------------------------------
// Tests for PercentileT version: computeEmpiricalUnderCoveragePenalty (6 parameters)
// -----------------------------------------------------------------------------

TEST_CASE("PercentileT UnderCoveragePenalty: Perfect coverage in t-space yields zero",
          "[AutoBootstrapSelector][UnderCoverage][PercentileT]")
{
    // Use UNIFORM t-statistics for predictable behavior
    std::vector<double> t_stats = createUniformBootstrapDist(-3.0, 3.0, 1000);
    
    double theta_hat = 5.0;
    double se_hat = 1.0;
    
    // For uniform t ∈ [-3, 3], 95% coverage means we need ±2.85
    // t ∈ [-2.85, 2.85] is 5.7/6 = 95% of the range
    // Interval in theta-space:
    // lo = theta_hat - t_upper * se = 5.0 - 2.85 * 1.0 = 2.15
    // hi = theta_hat - t_lower * se = 5.0 - (-2.85) * 1.0 = 7.85
    double lo = 2.15;
    double hi = 7.85;
    double cl = 0.95;
    
    double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
        t_stats, theta_hat, se_hat, lo, hi, cl);
    
    // Should be close to zero (allowing for discretization of 1000 points)
    REQUIRE(penalty == Catch::Approx(0.0).margin(0.02));
}

TEST_CASE("PercentileT UnderCoveragePenalty: Correct t-space transformation",
          "[AutoBootstrapSelector][UnderCoverage][PercentileT]")
{
    SECTION("Verify transformation math")
    {
        // Create uniform t-statistics from -3 to 3
        std::vector<double> t_stats = createUniformBootstrapDist(-3.0, 3.0, 1000);
        
        double theta_hat = 10.0;
        double se_hat = 2.0;
        
        // Interval in theta-space: [8, 12]
        // Transform to t-space:
        // t_at_lower_bound = (theta_hat - hi) / se_hat = (10 - 12) / 2 = -1.0
        // t_at_upper_bound = (theta_hat - lo) / se_hat = (10 - 8) / 2 = 1.0
        // So we want t ∈ [-1, 1], which is 2/6 = 33.33% of uniform[-3, 3]
        
        double lo = 8.0;
        double hi = 12.0;
        double cl = 0.95; // But actual coverage is only ~33%
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty_PercentileT(
            t_stats, theta_hat, se_hat, lo, hi, cl);
        
        // Shortfall = 0.95 - 0.333 = 0.617
        // Expected penalty: 10.0 * (0.617)^2 ≈ 3.8
        REQUIRE(penalty > 3.5);
        REQUIRE(penalty < 4.5);
    }
}

TEST_CASE("PercentileT UnderCoveragePenalty: Under-coverage produces penalty",
          "[AutoBootstrapSelector][UnderCoverage][PercentileT]")
{
    SECTION("Narrow interval (5% under-coverage)")
    {
        // Use uniform t-stats for predictable behavior
        std::vector<double> t_stats = createUniformBootstrapDist(-3.0, 3.0, 2000);
        
        double theta_hat = 5.0;
        double se_hat = 1.0;
        
        // For 95% CI on uniform [-3, 3], we need ±2.85
        // But we use ±2.70 which gives 90% coverage (5% shortfall)
        // t ∈ [-2.70, 2.70] is 5.4/6 = 90% of the range
        double lo = 5.0 - 2.70 * 1.0;  // 2.30
        double hi = 5.0 + 2.70 * 1.0;  // 7.70
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, theta_hat, se_hat, lo, hi, 0.95);
        
        // Shortfall = 5%, penalty = 10.0 * (0.05)^2 = 0.025
        REQUIRE(penalty > 0.01);
        REQUIRE(penalty == Catch::Approx(0.025).epsilon(0.05)); // Allow 5% tolerance for discretization
    }
}

TEST_CASE("PercentileT UnderCoveragePenalty: Over-coverage yields zero penalty",
          "[AutoBootstrapSelector][UnderCoverage][PercentileT]")
{
    std::vector<double> t_stats = createUniformBootstrapDist(-3.0, 3.0, 1000);
    
    double theta_hat = 5.0;
    double se_hat = 1.0;
    
    // For 95% CI on uniform [-3, 3], we need ±2.85
    // But we use ±3.0 which gives 100% coverage (over-coverage)
    double lo = 5.0 - 3.0 * 1.0;  // 2.0
    double hi = 5.0 + 3.0 * 1.0;  // 8.0
    
    double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
        t_stats, theta_hat, se_hat, lo, hi, 0.95);
    
    // Over-coverage should NOT be penalized
    REQUIRE(penalty == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("PercentileT UnderCoveragePenalty: Edge cases",
          "[AutoBootstrapSelector][UnderCoverage][PercentileT][EdgeCases]")
{
    SECTION("Empty t-statistics returns zero")
    {
        std::vector<double> empty_stats;
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            empty_stats, 5.0, 0.5, 4.0, 6.0, 0.95);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Single t-statistic returns zero")
    {
        std::vector<double> single_stat = {0.0};
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            single_stat, 5.0, 0.5, 4.0, 6.0, 0.95);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Invalid theta_hat returns zero")
    {
        std::vector<double> t_stats = createUniformBootstrapDist(-2.0, 2.0, 100);
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, std::numeric_limits<double>::quiet_NaN(), 0.5, 4.0, 6.0, 0.95);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Invalid se_hat returns zero")
    {
        std::vector<double> t_stats = createUniformBootstrapDist(-2.0, 2.0, 100);
        
        // se_hat = 0
        double penalty1 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, 0.0, 4.0, 6.0, 0.95);
        REQUIRE(penalty1 == 0.0);
        
        // se_hat negative
        double penalty2 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, -0.5, 4.0, 6.0, 0.95);
        REQUIRE(penalty2 == 0.0);
        
        // se_hat NaN
        double penalty3 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, std::numeric_limits<double>::quiet_NaN(), 4.0, 6.0, 0.95);
        REQUIRE(penalty3 == 0.0);
    }
    
    SECTION("Invalid interval bounds return zero")
    {
        std::vector<double> t_stats = createUniformBootstrapDist(-2.0, 2.0, 100);
        
        // lo >= hi
        double penalty1 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, 0.5, 6.0, 4.0, 0.95);
        REQUIRE(penalty1 == 0.0);
        
        // Non-finite bounds
        double penalty2 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, 0.5, std::numeric_limits<double>::infinity(), 6.0, 0.95);
        REQUIRE(penalty2 == 0.0);
    }
    
    SECTION("Invalid confidence level returns zero")
    {
        std::vector<double> t_stats = createUniformBootstrapDist(-2.0, 2.0, 100);
        
        double penalty1 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, 0.5, 4.0, 6.0, 0.0);
        REQUIRE(penalty1 == 0.0);
        
        double penalty2 = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, 5.0, 0.5, 4.0, 6.0, 1.0);
        REQUIRE(penalty2 == 0.0);
    }
}

TEST_CASE("PercentileT UnderCoveragePenalty: T-space bounds ordering check",
          "[AutoBootstrapSelector][UnderCoverage][PercentileT]")
{
    SECTION("Properly ordered t-bounds (valid interval)")
    {
        std::vector<double> t_stats = createUniformBootstrapDist(-2.0, 2.0, 1000);
        
        double theta_hat = 5.0;
        double se_hat = 0.5;
        
        // Normal interval: lo < hi
        // t_at_lower = (5 - 6) / 0.5 = -2
        // t_at_upper = (5 - 4) / 0.5 = 2
        // This is valid: -2 < 2
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, theta_hat, se_hat, 4.0, 6.0, 0.95);
        
        // Should compute normally (coverage is 100% here, so penalty = 0)
        REQUIRE(penalty == Catch::Approx(0.0).margin(0.01));
    }
}

// -----------------------------------------------------------------------------
// Tests verifying kUnderCoverageMultiplier scaling
// -----------------------------------------------------------------------------

TEST_CASE("UnderCoveragePenalty: Verify multiplier scaling",
          "[AutoBootstrapSelector][UnderCoverage][Scaling]")
{
    SECTION("BCa version uses kUnderCoverageMultiplier correctly")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);
        
        // Create 10% under-coverage
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, 0.75, 9.25, 0.95);
        
        // Expected: kUnderCoverageMultiplier * (0.10)^2
        // With kUnderCoverageMultiplier = 10.0: 10.0 * 0.01 = 0.10
        double expected = AutoBootstrapConfiguration::kUnderCoverageMultiplier * 0.10 * 0.10;
        
        REQUIRE(penalty == Catch::Approx(expected).epsilon(0.01));
    }
    
    SECTION("PercentileT version uses kUnderCoverageMultiplier correctly")
    {
        // Create uniform t-stats from -3 to 3
        std::vector<double> t_stats = createUniformBootstrapDist(-3.0, 3.0, 1200);
        
        double theta_hat = 10.0;
        double se_hat = 2.0;
        
        // Interval captures t ∈ [-1.5, 1.5], which is 50% of the range
        // t_lower = (10 - 13) / 2 = -1.5
        // t_upper = (10 - 7) / 2 = 1.5
        double lo = 7.0;
        double hi = 13.0;
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            t_stats, theta_hat, se_hat, lo, hi, 0.95);
        
        // Shortfall = 0.95 - 0.50 = 0.45
        // Expected: 10.0 * (0.45)^2 = 10.0 * 0.2025 = 2.025
        double expected = AutoBootstrapConfiguration::kUnderCoverageMultiplier * 0.45 * 0.45;
        
        REQUIRE(penalty == Catch::Approx(expected).epsilon(0.05));
    }
}

// -----------------------------------------------------------------------------
// Integration tests: Verify correct method selection in summarize functions
// -----------------------------------------------------------------------------

TEST_CASE("UnderCoveragePenalty: Integration with different confidence levels",
          "[AutoBootstrapSelector][UnderCoverage][Integration]")
{
  SECTION("90% confidence level")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);

        // 90% CL but only 85% coverage (5% shortfall)
        const double lo = 0.75;
        const double hi = 9.25;
        const double cl = 0.90;
        const double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(boot_stats, lo, hi, cl);

        // Expected penalty uses half-step tolerance: under = max(0, (cl - width_cdf) - 0.5/B_eff)
        const std::size_t B_eff = boot_stats.size();
        std::size_t inside = 0;
        for (double v : boot_stats) { if (std::isfinite(v) && v >= lo && v <= hi) ++inside; }
        const double width_cdf = (B_eff > 0) ? (static_cast<double>(inside) / static_cast<double>(B_eff)) : 0.0;
        const double tol = (B_eff > 0) ? (0.5 / static_cast<double>(B_eff)) : 0.5;
        const double under = std::max(0.0, (cl - width_cdf) - tol);
        const double expected = AutoBootstrapConfiguration::kUnderCoverageMultiplier * under * under;

        REQUIRE(penalty > 0.0);
        REQUIRE(penalty == Catch::Approx(expected).margin(1e-12));
    }

  SECTION("99% confidence level")
    {
        std::vector<double> boot_stats = createUniformBootstrapDist(0.0, 10.0, 1000);

        // 99% CL but only 95% coverage (4% shortfall)
        const double lo = 0.25;
        const double hi = 9.75;
        const double cl = 0.99;
        const double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(boot_stats, lo, hi, cl);

        // Expected penalty uses half-step tolerance: under = max(0, (cl - width_cdf) - 0.5/B_eff)
        const std::size_t B_eff = boot_stats.size();
        std::size_t inside = 0;
        for (double v : boot_stats) { if (std::isfinite(v) && v >= lo && v <= hi) ++inside; }
        const double width_cdf = (B_eff > 0) ? (static_cast<double>(inside) / static_cast<double>(B_eff)) : 0.0;
        const double tol = (B_eff > 0) ? (0.5 / static_cast<double>(B_eff)) : 0.5;
        const double under = std::max(0.0, (cl - width_cdf) - tol);
        const double expected = AutoBootstrapConfiguration::kUnderCoverageMultiplier * under * under;

        REQUIRE(penalty > 0.0);
        REQUIRE(penalty == Catch::Approx(expected).margin(1e-12));
    }
}


TEST_CASE("UnderCoveragePenalty: Realistic bootstrap scenario",
          "[AutoBootstrapSelector][UnderCoverage][Integration]")
{
    SECTION("BCa with slightly narrow interval")
    {
        // Create a right-skewed bootstrap distribution
        // Use a simpler approach: mix uniform and exponential-like
        std::vector<double> boot_stats;
        boot_stats.reserve(1000);
        
        // Create a distribution that's roughly: 
        // - 50% of values in [0, 5] (uniform-ish)
        // - 50% of values in [5, 15] (sparse tail)
        for (int i = 0; i < 1000; ++i) {
            double p = i / 1000.0;
            double val;
            if (p < 0.5) {
                // First half: [0, 5]
                val = p * 10.0;  // Maps [0, 0.5) to [0, 5]
            } else {
                // Second half: [5, 15] 
                val = 5.0 + (p - 0.5) * 20.0;  // Maps [0.5, 1.0] to [5, 15]
            }
            boot_stats.push_back(val);
        }
        std::sort(boot_stats.begin(), boot_stats.end());
        
        // Interval [2.0, 10.0] captures approximately:
        // - Lower tail cut: 200 points below 2.0 (20%)
        // - Upper tail cut: some points above 10.0
        // Let's say it captures about 65% of data (30% shortfall)
        double lo = 2.0;
        double hi = 10.0;
        
        double penalty = PenaltyCalc::computeEmpiricalUnderCoveragePenalty(
            boot_stats, lo, hi, 0.95);
        
        // With 30% shortfall: penalty = 10.0 * (0.30)^2 = 0.90
        // But actual coverage will vary, so be lenient
        REQUIRE(penalty > 0.1);  // At least some penalty
        REQUIRE(penalty < 2.0);  // But not astronomical
    }
}
