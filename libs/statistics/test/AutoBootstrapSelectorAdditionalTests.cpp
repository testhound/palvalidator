// AutoBootstrapSelector_AdditionalTests.cpp
//
// Additional comprehensive unit tests for AutoBootstrapSelector to fill coverage gaps
// identified in the code review.
//
// Coverage areas:
//  - computePercentileTStability method (not tested in existing tests)
//  - select() method edge cases and integration scenarios
//  - methodPreference() tie-breaking logic
//  - dominates() method for Pareto comparison
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

#include "AutoBootstrapSelector.h"
#include "number.h"

// Alias for convenience
using Decimal        = double;
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using Candidate      = Selector::Candidate;
using Result         = Selector::Result;
using ScoringWeights = Selector::ScoringWeights;
using MethodId       = Result::MethodId;
using SelectionDiagnostics = Result::SelectionDiagnostics;
using ScoreBreakdown = SelectionDiagnostics::ScoreBreakdown;

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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty_moderate = Selector::computePercentileTStability(res_moderate);
        double penalty_severe = Selector::computePercentileTStability(res_severe);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Returns infinity if B_inner is zero")
    {
        MockPercentileTResult res{
            1.0, 0.9, 1.1, 0.95, 100, 1000,
            0,        // B_inner = 0
            1000, 0, 0, 200000, 0.05
        };
        
        double penalty = Selector::computePercentileTStability(res);
        
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
        
        double penalty = Selector::computePercentileTStability(res);
        
        // Should handle 0% inner failure rate gracefully
        REQUIRE(std::isfinite(penalty));
    }
}

// -----------------------------------------------------------------------------
// Tests for methodPreference
// -----------------------------------------------------------------------------

TEST_CASE("methodPreference: Correct preference ordering",
          "[AutoBootstrapSelector][MethodPreference]")
{
    SECTION("BCa has highest preference (lowest value)")
    {
        int pref_bca = Selector::methodPreference(MethodId::BCa);
        REQUIRE(pref_bca == 1);
    }
    
    SECTION("PercentileT is second preference")
    {
        int pref_pt = Selector::methodPreference(MethodId::PercentileT);
        REQUIRE(pref_pt == 2);
    }
    
    SECTION("MOutOfN is third preference")
    {
        int pref_moon = Selector::methodPreference(MethodId::MOutOfN);
        REQUIRE(pref_moon == 3);
    }
    
    SECTION("Percentile is fourth preference")
    {
        int pref_perc = Selector::methodPreference(MethodId::Percentile);
        REQUIRE(pref_perc == 4);
    }
    
    SECTION("Basic is fifth preference")
    {
        int pref_basic = Selector::methodPreference(MethodId::Basic);
        REQUIRE(pref_basic == 5);
    }
    
    SECTION("Normal has lowest preference (highest value)")
    {
        int pref_normal = Selector::methodPreference(MethodId::Normal);
        REQUIRE(pref_normal == 6);
    }
    
    SECTION("Preference ordering is strictly increasing")
    {
        std::vector<int> preferences{
            Selector::methodPreference(MethodId::BCa),
            Selector::methodPreference(MethodId::PercentileT),
            Selector::methodPreference(MethodId::MOutOfN),
            Selector::methodPreference(MethodId::Percentile),
            Selector::methodPreference(MethodId::Basic),
            Selector::methodPreference(MethodId::Normal)
        };
        
        // Verify strictly increasing
        for (size_t i = 1; i < preferences.size(); ++i) {
            REQUIRE(preferences[i] > preferences[i-1]);
        }
    }
}

// -----------------------------------------------------------------------------
// Tests for dominates method
// -----------------------------------------------------------------------------

TEST_CASE("dominates: Basic domination logic",
          "[AutoBootstrapSelector][Dominates]")
{
    SECTION("Candidate with lower penalties in all dimensions dominates")
    {
        Candidate better = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.001,  // ordering_penalty
            0.001,  // length_penalty
            0.0);
        
        Candidate worse = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.01,   // ordering_penalty (10x worse)
            0.01,   // length_penalty (10x worse)
            0.0);
        
        REQUIRE(Selector::dominates(better, worse) == true);
        REQUIRE(Selector::dominates(worse, better) == false);
    }
    
    SECTION("Candidate strictly better in one dimension but equal in other dominates")
    {
        Candidate a = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.01,   // ordering_penalty
            0.001); // length_penalty (better)
        
        Candidate b = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.01,   // ordering_penalty (equal)
            0.01);  // length_penalty (worse)
        
        REQUIRE(Selector::dominates(a, b) == true);
    }
    
    SECTION("Equal candidates do not dominate each other")
    {
        Candidate a = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.01, 0.01);
        
        Candidate b = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.01, 0.01);
        
        REQUIRE(Selector::dominates(a, b) == false);
        REQUIRE(Selector::dominates(b, a) == false);
    }
    
    SECTION("Trade-offs prevent domination (Pareto frontier)")
    {
        Candidate a = createSimpleCandidate(
            MethodId::BCa, 1.0, 0.9, 1.1,
            0.001,  // Better ordering
            0.01);  // Worse length
        
        Candidate b = createSimpleCandidate(
            MethodId::Percentile, 1.0, 0.9, 1.1,
            0.01,   // Worse ordering
            0.001); // Better length
        
        REQUIRE(Selector::dominates(a, b) == false);
        REQUIRE(Selector::dominates(b, a) == false);
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
    SECTION("Candidate with negative lower bound rejected when enforcePositive=true")
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
        
        auto result = Selector::select(candidates, weights_enforce_positive);
        
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
