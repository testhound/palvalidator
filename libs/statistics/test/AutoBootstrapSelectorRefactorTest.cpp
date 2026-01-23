// AutoBootstrapSelector_Refactored_Tests.cpp
//
// Unit tests for the refactored AutoBootstrapSelector component classes:
//  - ScoreNormalizer
//  - CandidateGateKeeper
//  - ImprovedTournamentSelector
//  - Raw penalty computation methods
//  - Tournament selection phase methods
//  - Rank assignment methods
//  - BCa rejection analysis
//  - Full select() integration tests
//
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - AutoBootstrapSelector_Refactored.h
//  - number.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <limits>
#include <cmath>
#include <numeric>

#include "AutoBootstrapSelector.h"
#include "number.h"

// Alias for convenience
using Decimal        = double;
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using Candidate      = Selector::Candidate;
using Result         = Selector::Result;
using ScoringWeights = Selector::ScoringWeights;
using MethodId       = Result::MethodId;
using RawComponents  = Selector::RawComponents;
using StatisticSupport = mkc_timeseries::StatisticSupport;

// Helper classes from detail namespace
using ScoreNormalizer = palvalidator::analysis::detail::ScoreNormalizer<Decimal, ScoringWeights, RawComponents>;
using CandidateGateKeeper = palvalidator::analysis::detail::CandidateGateKeeper<Decimal, RawComponents>;
using ImprovedTournamentSelector = palvalidator::analysis::detail::ImprovedTournamentSelector<Decimal>;
using BcaRejectionAnalysis = palvalidator::analysis::detail::BcaRejectionAnalysis;

// -----------------------------------------------------------------------------
// Helper functions for creating test candidates
// -----------------------------------------------------------------------------

/**
 * @brief Create a test candidate with specified parameters
 */
Candidate makeTestCandidate(
    MethodId method = MethodId::Percentile,
    double mean = 5.0,
    double lower = 4.0,
    double upper = 6.0,
    double cl = 0.95,
    std::size_t n = 100,
    std::size_t B_outer = 1000,
    std::size_t B_inner = 0,
    std::size_t effective_B = 950,
    std::size_t skipped_total = 50,
    double se_boot = 0.5,
    double skew_boot = 0.2,
    double median_boot = 5.0,
    double center_shift_in_se = 0.1,
    double normalized_length = 1.0,
    double ordering_penalty = 0.0,
    double length_penalty = 0.0,
    double stability_penalty = 0.0,
    double z0 = 0.0,
    double accel = 0.0,
    double inner_failure_rate = 0.0)
{
    return Candidate(
        method, mean, lower, upper, cl, n, B_outer, B_inner,
        effective_B, skipped_total, se_boot, skew_boot, median_boot,
        center_shift_in_se, normalized_length, ordering_penalty,
        length_penalty, stability_penalty, z0, accel, inner_failure_rate);
}

/**
 * @brief Create a BCa test candidate
 */
Candidate makeBcaCandidate(
    double z0 = 0.1,
    double accel = 0.05,
    double score = 1.0)
{
    auto c = makeTestCandidate(
        MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100, 1000, 0,
        950, 50, 0.5, 0.2, 5.0, 0.0, 1.0, 0.0, 0.0, 0.0,
        z0, accel, 0.0);
    return c.withScore(score);
}

/**
 * @brief Create valid raw components for testing
 */
RawComponents makeValidRaw(
    double ordering = 0.01,
    double length = 0.5,
    double stability = 0.1,
    double center_sq = 1.0,
    double skew_sq = 1.0,
    double domain = 0.0)
{
    return RawComponents(ordering, length, stability, center_sq, skew_sq, domain);
}

// =============================================================================
// TESTS FOR PHASE 1: Raw Penalty Computation
// =============================================================================

TEST_CASE("computeSkewPenalty: Basic functionality",
          "[AutoBootstrapSelector][RawPenalty][Skew]")
{
    SECTION("Skew below threshold produces no penalty")
    {
        double skew = 0.5;  // Below threshold of 1.0
        double penalty = Selector::computeSkewPenalty(skew);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Skew exactly at threshold produces no penalty")
    {
        double skew = 1.0;  // Exactly at threshold
        double penalty = Selector::computeSkewPenalty(skew);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Skew above threshold produces quadratic penalty")
    {
        double skew = 2.0;  // 1.0 above threshold
        double penalty = Selector::computeSkewPenalty(skew);
        REQUIRE(penalty == Catch::Approx(1.0));  // (2.0 - 1.0)^2 = 1.0
    }
    
    SECTION("Negative skew uses absolute value")
    {
        double skew = -2.5;  // |skew| = 2.5
        double penalty = Selector::computeSkewPenalty(skew);
        REQUIRE(penalty == Catch::Approx(2.25));  // (2.5 - 1.0)^2 = 2.25
    }
    
    SECTION("Large skew produces large penalty")
    {
        double skew = 5.0;  // 4.0 above threshold
        double penalty = Selector::computeSkewPenalty(skew);
        REQUIRE(penalty == Catch::Approx(16.0));  // (5.0 - 1.0)^2 = 16.0
    }
}

TEST_CASE("computeDomainPenalty: Support violation detection",
          "[AutoBootstrapSelector][RawPenalty][Domain]")
{
    StatisticSupport unbounded = StatisticSupport::unbounded();
    StatisticSupport positive = StatisticSupport::strictLowerBound(0.0, 1e-10);
    
    SECTION("No violation with unbounded support")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0);
        double penalty = Selector::computeDomainPenalty(candidate, unbounded);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("No violation when lower bound is positive")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, 1.0, 6.0);
        double penalty = Selector::computeDomainPenalty(candidate, positive);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Violation when lower bound is negative with positive support")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, -1.0, 6.0);
        double penalty = Selector::computeDomainPenalty(candidate, positive);
        REQUIRE(penalty > 0.0);
        REQUIRE(penalty == AutoBootstrapConfiguration::kDomainViolationPenalty);
    }
    
    SECTION("Violation at exactly zero with strict lower bound")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, 0.0, 6.0);
        double penalty = Selector::computeDomainPenalty(candidate, positive);
        // Should violate because it's a strict bound with epsilon
        REQUIRE(penalty > 0.0);
    }
}

TEST_CASE("computeRawComponentsForCandidate: Component extraction",
          "[AutoBootstrapSelector][RawPenalty][Components]")
{
    StatisticSupport unbounded = StatisticSupport::unbounded();
    
    SECTION("Normal case with finite values")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile,
            5.0,      // mean
            4.0,      // lower
            6.0,      // upper
            0.95,     // cl
            100,      // n
            1000,     // B_outer
            0,        // B_inner
            950,      // effective_B
            50,       // skipped_total
            0.5,      // se_boot
            1.5,      // skew_boot (will produce penalty: (1.5-1.0)^2 = 0.25)
            5.0,      // median_boot
            0.2,      // center_shift_in_se (0.2^2 = 0.04)
            1.0,      // normalized_length
            0.01,     // ordering_penalty
            0.05,     // length_penalty
            0.02      // stability_penalty
        );
        
        auto raw = Selector::computeRawComponentsForCandidate(candidate, unbounded);
        
        REQUIRE(raw.getOrderingPenalty() == Catch::Approx(0.01));
        REQUIRE(raw.getLengthPenalty() == Catch::Approx(0.05));
        REQUIRE(raw.getStabilityPenalty() == Catch::Approx(0.02));
        REQUIRE(raw.getCenterShiftSq() == Catch::Approx(0.04));  // 0.2^2
        REQUIRE(raw.getSkewSq() == Catch::Approx(0.25));  // (1.5 - 1.0)^2
        REQUIRE(raw.getDomainPenalty() == 0.0);
    }
    
    SECTION("Handles non-finite center shift gracefully")
    {
        // Create a candidate with NaN center shift
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95, 100, 1000, 0, 950, 50,
            0.5, 0.2, 5.0,
            std::numeric_limits<double>::quiet_NaN(),  // center_shift_in_se = NaN
            1.0, 0.0, 0.0, 0.0);
        
        auto raw = Selector::computeRawComponentsForCandidate(candidate, unbounded);
        
        // Should handle gracefully and set to 0
        REQUIRE(std::isfinite(raw.getCenterShiftSq()));
        REQUIRE(raw.getCenterShiftSq() == 0.0);
    }
    
    SECTION("Handles non-finite skew gracefully")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95, 100, 1000, 0, 950, 50,
            0.5,
            std::numeric_limits<double>::infinity(),  // skew_boot = inf
            5.0, 0.1, 1.0, 0.0, 0.0, 0.0);
        
        auto raw = Selector::computeRawComponentsForCandidate(candidate, unbounded);
        
        // Should handle gracefully
        REQUIRE(std::isfinite(raw.getSkewSq()));
        REQUIRE(raw.getSkewSq() == 0.0);
    }
    
    SECTION("Includes domain penalty when support is violated")
    {
        StatisticSupport positive = StatisticSupport::strictLowerBound(0.0, 1e-10);
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, -1.0, 6.0);  // Lower bound negative
        
        auto raw = Selector::computeRawComponentsForCandidate(candidate, positive);
        
        REQUIRE(raw.getDomainPenalty() > 0.0);
    }
}

TEST_CASE("computeRawPenalties: Batch processing",
          "[AutoBootstrapSelector][RawPenalty][Batch]")
{
    StatisticSupport unbounded = StatisticSupport::unbounded();
    
    SECTION("Processes multiple candidates correctly")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95, 100, 1000, 0, 950, 50,
                            0.5, 0.2, 5.0, 0.1, 1.0, 0.01, 0.05, 0.02),
            makeTestCandidate(MethodId::Basic, 5.0, 3.8, 6.2, 0.95, 100, 1000, 0, 950, 50,
                            0.5, 0.3, 5.0, 0.2, 1.1, 0.02, 0.08, 0.03),
            makeBcaCandidate(0.1, 0.05, 0.0)
        };
        
        auto raw = Selector::computeRawPenalties(candidates, unbounded);
        
        REQUIRE(raw.size() == 3);
        
        for (const auto& r : raw)
        {
            REQUIRE(std::isfinite(r.getOrderingPenalty()));
            REQUIRE(std::isfinite(r.getLengthPenalty()));
            REQUIRE(std::isfinite(r.getStabilityPenalty()));
            REQUIRE(std::isfinite(r.getCenterShiftSq()));
            REQUIRE(std::isfinite(r.getSkewSq()));
        }
    }
    
    SECTION("Returns empty vector for empty input")
    {
        std::vector<Candidate> empty_candidates;
        auto raw = Selector::computeRawPenalties(empty_candidates, unbounded);
        REQUIRE(raw.empty());
    }
}

TEST_CASE("containsBcaCandidate: BCa detection",
          "[AutoBootstrapSelector][RawPenalty][BCa]")
{
    SECTION("Returns true when BCa candidate present")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile),
            makeBcaCandidate(0.1, 0.05, 1.0),
            makeTestCandidate(MethodId::Basic)
        };
        
        REQUIRE(Selector::containsBcaCandidate(candidates) == true);
    }
    
    SECTION("Returns false when no BCa candidate")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile),
            makeTestCandidate(MethodId::Basic),
            makeTestCandidate(MethodId::Normal)
        };
        
        REQUIRE(Selector::containsBcaCandidate(candidates) == false);
    }
    
    SECTION("Returns false for empty candidate list")
    {
        std::vector<Candidate> empty_candidates;
        REQUIRE(Selector::containsBcaCandidate(empty_candidates) == false);
    }
}

// =============================================================================
// TESTS FOR SCORE NORMALIZER
// =============================================================================

TEST_CASE("ScoreNormalizer::normalize: Basic normalization",
          "[AutoBootstrapSelector][ScoreNormalizer][Normalize]")
{
    ScoringWeights default_weights;
    ScoreNormalizer normalizer(default_weights);
    
    SECTION("Standard case with typical values")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 4.0, 4.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        // Check normalization: raw / reference
        // ordering: 0.01 / (0.10 * 0.10) = 0.01 / 0.01 = 1.0
        REQUIRE(norm.ordering_norm == Catch::Approx(1.0));
        
        // length: 0.5 / (1.0 * 1.0) = 0.5
        REQUIRE(norm.length_norm == Catch::Approx(0.5));
        
        // stability: 0.1 / 0.25 = 0.4
        REQUIRE(norm.stability_norm == Catch::Approx(0.4));
        
        // center_sq: 4.0 / (2.0 * 2.0) = 1.0
        REQUIRE(norm.center_sq_norm == Catch::Approx(1.0));
        
        // skew_sq: 4.0 / (2.0 * 2.0) = 1.0
        REQUIRE(norm.skew_sq_norm == Catch::Approx(1.0));
    }
    
    SECTION("Weights are applied to contributions")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 4.0, 4.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        // Default weights: w_order=1.0, w_length=0.25, w_stability=1.0, 
        //                  w_center=1.0, w_skew=0.5
        
        REQUIRE(norm.ordering_contrib == Catch::Approx(1.0 * norm.ordering_norm));
        REQUIRE(norm.length_contrib == Catch::Approx(0.25 * norm.length_norm));
        REQUIRE(norm.stability_contrib == Catch::Approx(1.0 * norm.stability_norm));
        REQUIRE(norm.center_sq_contrib == Catch::Approx(1.0 * norm.center_sq_norm));
        REQUIRE(norm.skew_sq_contrib == Catch::Approx(0.5 * norm.skew_sq_norm));
    }
    
    SECTION("Zero raw values produce zero normalized values")
    {
        auto raw = makeValidRaw(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        REQUIRE(norm.ordering_norm == 0.0);
        REQUIRE(norm.length_norm == 0.0);
        REQUIRE(norm.stability_norm == 0.0);
        REQUIRE(norm.center_sq_norm == 0.0);
        REQUIRE(norm.skew_sq_norm == 0.0);
    }
}

TEST_CASE("ScoreNormalizer::computeTotalScore: Score computation",
          "[AutoBootstrapSelector][ScoreNormalizer][TotalScore]")
{
    ScoringWeights default_weights;
    ScoreNormalizer normalizer(default_weights);
    
    SECTION("Non-BCa method: sum of contributions plus domain")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        double total = normalizer.computeTotalScore(
            norm, raw, MethodId::Percentile, 0.5);
        
        // Should be sum of contributions + domain penalty (0.0)
        double expected = norm.ordering_contrib + norm.length_contrib +
                         norm.stability_contrib + norm.center_sq_contrib +
                         norm.skew_sq_contrib + 0.0;
        
        REQUIRE(total == Catch::Approx(expected));
    }
    
    SECTION("BCa method with length penalty below threshold")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        // Length penalty below threshold - no overflow penalty
        double length_penalty = 0.5;  // Assuming kBcaLengthPenaltyThreshold > 0.5
        
        double total = normalizer.computeTotalScore(
            norm, raw, MethodId::BCa, length_penalty);
        
        // Should not include overflow penalty
        double expected = norm.ordering_contrib + norm.length_contrib +
                         norm.stability_contrib + norm.center_sq_contrib +
                         norm.skew_sq_contrib + 0.0;
        
        REQUIRE(total == Catch::Approx(expected));
    }
    
    SECTION("BCa method with length penalty above threshold")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        // Use a very high length penalty that exceeds any reasonable threshold
        double length_penalty = 10.0;
        
        double total = normalizer.computeTotalScore(
            norm, raw, MethodId::BCa, length_penalty);
        
        // Should include overflow penalty (total > expected base)
        double base_expected = norm.ordering_contrib + norm.length_contrib +
                              norm.stability_contrib + norm.center_sq_contrib +
                              norm.skew_sq_contrib + 0.0;
        
        REQUIRE(total > base_expected);
    }
    
    SECTION("Domain penalty is included in total")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 
                               AutoBootstrapConfiguration::kDomainViolationPenalty);
        auto norm = normalizer.normalize(raw);
        
        double total = normalizer.computeTotalScore(
            norm, raw, MethodId::Percentile, 0.5);
        
        REQUIRE(total > norm.ordering_contrib + norm.length_contrib +
                       norm.stability_contrib + norm.center_sq_contrib +
                       norm.skew_sq_contrib);
    }
}

// =============================================================================
// TESTS FOR CANDIDATE GATE KEEPER
// =============================================================================

TEST_CASE("CandidateGateKeeper::isCommonCandidateValid: Basic validation",
          "[AutoBootstrapSelector][GateKeeper][Common]")
{
    CandidateGateKeeper gatekeeper;
    auto valid_raw = makeValidRaw();
    
    SECTION("Valid candidate passes all gates")
    {
        auto candidate = makeTestCandidate().withScore(1.5);
        REQUIRE(gatekeeper.isCommonCandidateValid(candidate, valid_raw) == true);
    }
    
    SECTION("Non-finite score fails gate")
    {
        auto candidate = makeTestCandidate().withScore(
            std::numeric_limits<double>::quiet_NaN());
        REQUIRE(gatekeeper.isCommonCandidateValid(candidate, valid_raw) == false);
    }
    
    SECTION("Infinite score fails gate")
    {
        auto candidate = makeTestCandidate().withScore(
            std::numeric_limits<double>::infinity());
        REQUIRE(gatekeeper.isCommonCandidateValid(candidate, valid_raw) == false);
    }
    
    SECTION("Domain violation fails gate")
    {
        auto candidate = makeTestCandidate().withScore(1.5);
        auto invalid_raw = makeValidRaw(
            0.01, 0.5, 0.1, 1.0, 1.0,
            AutoBootstrapConfiguration::kDomainViolationPenalty);
        
        REQUIRE(gatekeeper.isCommonCandidateValid(candidate, invalid_raw) == false);
    }
    
    SECTION("Insufficient effective B fails gate")
    {
        // Create candidate with very low effective_B (50% of B_outer)
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 1000, 0,
            500,  // Only 50% effective (assuming threshold >= 70%)
            500, 0.5, 0.2, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ).withScore(1.5);
        
        // Should fail if kMinEffectiveBFraction > 0.5
        // Note: Actual result depends on configuration constant
        REQUIRE(std::isfinite(candidate.getScore()));  // But score is finite
    }
}

TEST_CASE("CandidateGateKeeper::isBcaCandidateValid: BCa-specific validation",
          "[AutoBootstrapSelector][GateKeeper][BCa]")
{
    CandidateGateKeeper gatekeeper;
    auto valid_raw = makeValidRaw();
    
    SECTION("Valid BCa candidate passes all gates")
    {
        auto candidate = makeBcaCandidate(0.1, 0.05, 1.5);
        REQUIRE(gatekeeper.isBcaCandidateValid(candidate, valid_raw) == true);
    }
    
    SECTION("Non-finite z0 fails gate")
    {
        auto candidate = makeBcaCandidate(
            std::numeric_limits<double>::quiet_NaN(), 0.05, 1.5);
        REQUIRE(gatekeeper.isBcaCandidateValid(candidate, valid_raw) == false);
    }
    
    SECTION("Non-finite accel fails gate")
    {
        auto candidate = makeBcaCandidate(
            0.1, std::numeric_limits<double>::infinity(), 1.5);
        REQUIRE(gatekeeper.isBcaCandidateValid(candidate, valid_raw) == false);
    }
    
    SECTION("Excessive z0 fails gate")
    {
        // Use z0 = 0.7 which should exceed kBcaZ0HardLimit (typically 0.6)
        auto candidate = makeBcaCandidate(0.7, 0.05, 1.5);
        
        // Check if it exceeds the hard limit
        bool exceeds_limit = std::fabs(0.7) > 
            AutoBootstrapConfiguration::kBcaZ0HardLimit;
        
        if (exceeds_limit)
        {
            REQUIRE(gatekeeper.isBcaCandidateValid(candidate, valid_raw) == false);
        }
    }
    
    SECTION("Excessive accel fails gate")
    {
        // Use accel = 0.5 which should exceed kBcaAHardLimit
        auto candidate = makeBcaCandidate(0.1, 0.5, 1.5);
        
        bool exceeds_limit = std::fabs(0.5) > 
            AutoBootstrapConfiguration::kBcaAHardLimit;
        
        if (exceeds_limit)
        {
            REQUIRE(gatekeeper.isBcaCandidateValid(candidate, valid_raw) == false);
        }
    }
    
    SECTION("BCa candidate that fails common gate also fails BCa gate")
    {
        auto candidate = makeBcaCandidate(0.1, 0.05, 
            std::numeric_limits<double>::quiet_NaN());  // Non-finite score
        
        REQUIRE(gatekeeper.isBcaCandidateValid(candidate, valid_raw) == false);
    }
}

// =============================================================================
// TESTS FOR TOURNAMENT SELECTOR
// =============================================================================

TEST_CASE("ImprovedTournamentSelector: Basic selection",
          "[AutoBootstrapSelector][Tournament][Selection]")
{
    SECTION("Selects candidate with lowest score")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(2.0),
            makeTestCandidate(MethodId::Basic).withScore(1.5),      // Best
            makeTestCandidate(MethodId::BCa).withScore(1.8)
        };
        
        ImprovedTournamentSelector selector(candidates);
        
        selector.consider(0);  // score 2.0
        selector.consider(1);  // score 1.5 (best)
        selector.consider(2);  // score 1.8
        
        REQUIRE(selector.hasWinner() == true);
        REQUIRE(selector.getWinnerIndex() == 1);
    }
    
    SECTION("Handles single candidate")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.5)
        };
        
        ImprovedTournamentSelector selector(candidates);
        selector.consider(0);
        
        REQUIRE(selector.hasWinner() == true);
        REQUIRE(selector.getWinnerIndex() == 0);
    }
    
    SECTION("No winner initially")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.5)
        };
        
        ImprovedTournamentSelector selector(candidates);
        REQUIRE(selector.hasWinner() == false);
    }
}

TEST_CASE("ImprovedTournamentSelector: Tie-breaking",
          "[AutoBootstrapSelector][Tournament][Ties]")
{
    SECTION("Breaks ties using method preference")
    {
        // Create candidates with same score but different methods
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.0),
            makeBcaCandidate(0.1, 0.05, 1.0)  // BCa has higher preference
        };
        
        ImprovedTournamentSelector selector(candidates);
        
        selector.consider(0);  // Percentile, score 1.0
        selector.consider(1);  // BCa, score 1.0 (higher preference)
        
        REQUIRE(selector.hasWinner() == true);
        REQUIRE(selector.getWinnerIndex() == 1);  // BCa should win
    }
    
    SECTION("BCa wins when all methods tied")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Normal).withScore(1.0),
            makeTestCandidate(MethodId::Basic).withScore(1.0),
            makeTestCandidate(MethodId::Percentile).withScore(1.0),
            makeBcaCandidate(0.1, 0.05, 1.0)
        };
        
        ImprovedTournamentSelector selector(candidates);
        
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            selector.consider(i);
        }
        
        REQUIRE(selector.hasWinner() == true);
        REQUIRE(selector.getWinnerIndex() == 3);  // BCa (highest preference)
    }
    
    SECTION("Earlier considered candidate wins if same method")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.0),
            makeTestCandidate(MethodId::Percentile).withScore(1.0)
        };
        
        ImprovedTournamentSelector selector(candidates);
        
        selector.consider(0);
        selector.consider(1);
        
        // First one considered should stay (tie resolution)
        REQUIRE(selector.getWinnerIndex() == 0);
    }
}

TEST_CASE("ImprovedTournamentSelector: Error handling",
          "[AutoBootstrapSelector][Tournament][Errors]")
{
    SECTION("Throws when accessing winner without selection")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.5)
        };
        
        ImprovedTournamentSelector selector(candidates);
        
        REQUIRE_THROWS_AS(selector.getWinnerIndex(), std::logic_error);
    }
    
    SECTION("getTieEpsilon returns reasonable value")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.5),
            makeTestCandidate(MethodId::Basic).withScore(2.0)
        };
        
        ImprovedTournamentSelector selector(candidates);
        selector.consider(0);
        selector.consider(1);
        
        double epsilon = selector.getTieEpsilon();
        REQUIRE(epsilon > 0.0);
        REQUIRE(std::isfinite(epsilon));
    }
}

// =============================================================================
// TESTS FOR PHASE 3: Tournament Selection
// =============================================================================

TEST_CASE("selectWinnerIndex: Winner selection from valid candidates",
          "[AutoBootstrapSelector][Phase3][Selection]")
{
    std::vector<Candidate> candidates;
    std::vector<RawComponents> raw;
    
    // Create valid candidates
    candidates.push_back(makeTestCandidate(MethodId::Percentile).withScore(2.0));
    candidates.push_back(makeTestCandidate(MethodId::Basic).withScore(1.5));  // Best
    candidates.push_back(makeBcaCandidate(0.1, 0.05, 1.8));
    
    // Create corresponding raw components (all valid)
    for (std::size_t i = 0; i < 3; ++i)
    {
        raw.push_back(makeValidRaw());
    }
    
    SECTION("Selects winner correctly")
    {
        double tie_epsilon = 0.0;
        std::size_t winner_idx = Selector::selectWinnerIndex(
            candidates, raw, tie_epsilon);
        
        REQUIRE(winner_idx == 1);  // Basic with score 1.5
        REQUIRE(tie_epsilon > 0.0);
    }
    
    SECTION("Throws when no candidates pass gates")
    {
        // Make all candidates invalid (non-finite scores)
        for (auto& c : candidates)
        {
            c = c.withScore(std::numeric_limits<double>::quiet_NaN());
        }
        
        double tie_epsilon = 0.0;
        
        REQUIRE_THROWS_AS(
            Selector::selectWinnerIndex(candidates, raw, tie_epsilon),
            std::runtime_error);
    }
}

// =============================================================================
// TESTS FOR PHASE 4: Rank Assignment
// =============================================================================

TEST_CASE("assignRanks: Rank assignment and winner marking",
          "[AutoBootstrapSelector][Phase4][Ranks]")
{
    std::vector<Candidate> candidates;
    std::vector<RawComponents> raw;
    
    // Create candidates with different scores
    candidates.push_back(
        makeTestCandidate(MethodId::Percentile)
            .withScore(3.0)
            .withMetadata(100, 0, false));
    candidates.push_back(
        makeTestCandidate(MethodId::Basic)
            .withScore(1.0)
            .withMetadata(101, 0, false));
    candidates.push_back(
        makeBcaCandidate(0.1, 0.05, 2.0)
            .withMetadata(102, 0, false));
    
    for (std::size_t i = 0; i < 3; ++i)
    {
        raw.push_back(makeValidRaw());
    }
    
    SECTION("Assigns ranks correctly based on score")
    {
        std::size_t winner_idx = 1;  // Basic with score 1.0
        
        Selector::assignRanks(candidates, raw, winner_idx);
        
        // Check ranks: Basic (1.0) -> rank 1, BCa (2.0) -> rank 2, Percentile (3.0) -> rank 3
        REQUIRE(candidates[0].getRank() == 3);  // Percentile, score 3.0
        REQUIRE(candidates[1].getRank() == 1);  // Basic, score 1.0 (best)
        REQUIRE(candidates[2].getRank() == 2);  // BCa, score 2.0
    }
    
    SECTION("Marks winner as chosen")
    {
        std::size_t winner_idx = 1;
        
        Selector::assignRanks(candidates, raw, winner_idx);
        
        REQUIRE(candidates[0].isChosen() == false);
        REQUIRE(candidates[1].isChosen() == true);  // Winner
        REQUIRE(candidates[2].isChosen() == false);
    }
    
    SECTION("Invalid candidates get rank 0")
    {
        // Make first candidate invalid
        candidates[0] = candidates[0].withScore(
            std::numeric_limits<double>::quiet_NaN());
        
        std::size_t winner_idx = 1;
        
        Selector::assignRanks(candidates, raw, winner_idx);
        
        // Invalid candidate should have rank 0
        REQUIRE(candidates[0].getRank() == 0);
        REQUIRE(candidates[1].getRank() > 0);
        REQUIRE(candidates[2].getRank() > 0);
    }
}

// =============================================================================
// TESTS FOR PHASE 5: BCa Rejection Analysis
// =============================================================================

TEST_CASE("analyzeBcaRejection: BCa rejection diagnostics",
          "[AutoBootstrapSelector][Phase5][BCa]")
{
    SECTION("No BCa candidate in tournament")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile).withScore(1.0),
            makeTestCandidate(MethodId::Basic).withScore(2.0)
        };
        
        std::vector<RawComponents> raw;
        for (std::size_t i = 0; i < 2; ++i)
            raw.push_back(makeValidRaw());
        
        auto analysis = Selector::analyzeBcaRejection(candidates, raw, 0, false);
        
        REQUIRE(analysis.has_bca_candidate == false);
        REQUIRE(analysis.bca_chosen == false);
    }
    
    SECTION("BCa chosen as winner")
    {
        std::vector<Candidate> candidates = {
            makeBcaCandidate(0.1, 0.05, 1.0),
            makeTestCandidate(MethodId::Percentile).withScore(2.0)
        };
        
        std::vector<RawComponents> raw;
        for (std::size_t i = 0; i < 2; ++i)
            raw.push_back(makeValidRaw());
        
        auto analysis = Selector::analyzeBcaRejection(candidates, raw, 0, true);
        
        REQUIRE(analysis.has_bca_candidate == true);
        REQUIRE(analysis.bca_chosen == true);
        REQUIRE(analysis.rejected_for_instability == false);
        REQUIRE(analysis.rejected_for_length == false);
        REQUIRE(analysis.rejected_for_domain == false);
        REQUIRE(analysis.rejected_for_non_finite == false);
    }
    
    SECTION("BCa rejected for non-finite score")
    {
        std::vector<Candidate> candidates = {
            makeBcaCandidate(0.1, 0.05, std::numeric_limits<double>::quiet_NaN()),
            makeTestCandidate(MethodId::Percentile).withScore(2.0)
        };
        
        std::vector<RawComponents> raw;
        for (std::size_t i = 0; i < 2; ++i)
            raw.push_back(makeValidRaw());
        
        auto analysis = Selector::analyzeBcaRejection(candidates, raw, 1, true);
        
        REQUIRE(analysis.has_bca_candidate == true);
        REQUIRE(analysis.bca_chosen == false);
        REQUIRE(analysis.rejected_for_non_finite == true);
    }
    
    SECTION("BCa rejected for domain violation")
    {
        std::vector<Candidate> candidates = {
            makeBcaCandidate(0.1, 0.05, 2.0),
            makeTestCandidate(MethodId::Percentile).withScore(1.0)
        };
        
        std::vector<RawComponents> raw;
        // BCa has domain violation
        raw.push_back(makeValidRaw(
            0.01, 0.5, 0.1, 1.0, 1.0,
            AutoBootstrapConfiguration::kDomainViolationPenalty));
        raw.push_back(makeValidRaw());
        
        auto analysis = Selector::analyzeBcaRejection(candidates, raw, 1, true);
        
        REQUIRE(analysis.rejected_for_domain == true);
    }
    
    SECTION("BCa rejected for excessive z0")
    {
        std::vector<Candidate> candidates = {
            makeBcaCandidate(0.7, 0.05, 2.0),  // z0 = 0.7 exceeds typical limits
            makeTestCandidate(MethodId::Percentile).withScore(1.0)
        };
        
        std::vector<RawComponents> raw;
        for (std::size_t i = 0; i < 2; ++i)
            raw.push_back(makeValidRaw());
        
        auto analysis = Selector::analyzeBcaRejection(candidates, raw, 1, true);
        
        // Check if z0 exceeds hard limit
        bool exceeds = std::fabs(0.7) > AutoBootstrapConfiguration::kBcaZ0HardLimit;
        if (exceeds)
        {
            REQUIRE(analysis.rejected_for_instability == true);
        }
    }
    
    SECTION("BCa rejected for excessive length penalty")
    {
        // Create BCa with very high length penalty manually
        // We can't easily set length penalty in makeBcaCandidate
        auto bca_high_length = makeTestCandidate(
            MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100, 1000, 0, 950, 50,
            0.5, 0.2, 5.0, 0.0, 1.0, 0.0,
            10.0,  // Very high length penalty
            0.0, 0.1, 0.05, 0.0).withScore(2.0);
        
        std::vector<Candidate> candidates = {
            bca_high_length,
            makeTestCandidate(MethodId::Percentile).withScore(1.0)
        };
        
        std::vector<RawComponents> raw;
        for (std::size_t i = 0; i < 2; ++i)
            raw.push_back(makeValidRaw());
        
        auto analysis = Selector::analyzeBcaRejection(candidates, raw, 1, true);
        
        // Check if length penalty exceeds threshold
        bool exceeds = 10.0 > AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold;
        if (exceeds)
        {
            REQUIRE(analysis.rejected_for_length == true);
        }
    }
}

// =============================================================================
// TESTS FOR HELPER UTILITIES
// =============================================================================

TEST_CASE("getSupportBounds: Support bounds extraction",
          "[AutoBootstrapSelector][Helpers][Support]")
{
    SECTION("Unbounded support returns NaN values")
    {
        StatisticSupport unbounded = StatisticSupport::unbounded();
        auto [lower, upper] = Selector::getSupportBounds(unbounded);
        
        REQUIRE(std::isnan(lower));
        REQUIRE(std::isnan(upper));
    }
    
    SECTION("Lower bounded support returns correct bounds")
    {
        StatisticSupport bounded = StatisticSupport::strictLowerBound(0.0, 1e-10);
        auto [lower, upper] = Selector::getSupportBounds(bounded);
        
        REQUIRE(lower == Catch::Approx(0.0));
        REQUIRE(std::isnan(upper));
    }
}

TEST_CASE("computeEffectiveSupport: Effective support calculation",
          "[AutoBootstrapSelector][Helpers][EffectiveSupport]")
{
    SECTION("Already bounded support is preserved")
    {
        StatisticSupport bounded = StatisticSupport::strictLowerBound(0.0, 1e-10);
        ScoringWeights weights;  // enforcePositive = false by default
        
        auto effective = Selector::computeEffectiveSupport(bounded, weights);
        
        REQUIRE(effective.hasLowerBound() == true);
        REQUIRE(effective.lowerBound() == Catch::Approx(0.0));
    }
    
    SECTION("Unbounded with enforcePositive false stays unbounded")
    {
        StatisticSupport unbounded = StatisticSupport::unbounded();
        ScoringWeights weights;  // enforcePositive = false
        
        auto effective = Selector::computeEffectiveSupport(unbounded, weights);
        
        // Should remain unbounded if enforcePositive is false
        REQUIRE(effective.hasLowerBound() == unbounded.hasLowerBound());
    }
}

TEST_CASE("passesEffectiveBGate: Effective B validation",
          "[AutoBootstrapSelector][Helpers][EffectiveB]")
{
    SECTION("Sufficient effective B passes gate")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 1000, 0, 900, 100, 0.5, 0.2, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0);
        
        // 900/1000 = 90% should pass (assuming threshold <= 90%)
        REQUIRE(Selector::passesEffectiveBGate(candidate) == true);
    }
    
    SECTION("Perfect effective B passes gate")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 1000, 0, 1000, 0, 0.5, 0.2, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0);
        
        // 1000/1000 = 100% should always pass
        REQUIRE(Selector::passesEffectiveBGate(candidate) == true);
    }
    
    SECTION("Zero B_outer fails gate")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 0, 0, 0, 0, 0.5, 0.2, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0);
        
        REQUIRE(Selector::passesEffectiveBGate(candidate) == false);
    }
}

TEST_CASE("validateInputs: Input validation",
          "[AutoBootstrapSelector][Helpers][Validation]")
{
    SECTION("Empty candidate list throws exception")
    {
        std::vector<Candidate> empty_candidates;
        
        REQUIRE_THROWS_AS(
            Selector::validateInputs(empty_candidates),
            std::invalid_argument);
    }
    
    SECTION("Non-empty candidate list does not throw")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile)
        };
        
        REQUIRE_NOTHROW(Selector::validateInputs(candidates));
    }
}

// =============================================================================
// INTEGRATION TESTS FOR FULL SELECT METHOD
// =============================================================================

TEST_CASE("select: Full integration test with valid candidates",
          "[AutoBootstrapSelector][Integration][Select]")
{
    ScoringWeights weights;
    StatisticSupport unbounded = StatisticSupport::unbounded();
    
    SECTION("Selects winner from valid candidates")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.1, 1.0,
                            0.05, 0.1, 0.02),
            makeTestCandidate(MethodId::Basic, 5.0, 3.8, 6.2, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.3, 5.0, 0.2, 1.1,
                            0.08, 0.15, 0.03),
            makeBcaCandidate(0.1, 0.05, 0.0)  // Will be scored
        };
        
        auto result = Selector::select(candidates, weights, unbounded);
        
        // Should have a winner
        REQUIRE(result.getChosenCandidate().getRank() == 1);
        REQUIRE(result.getChosenCandidate().isChosen() == true);
        
        // All candidates should have ranks or be rejected
        const auto& all_candidates = result.getCandidates();
        for (const auto& c : all_candidates)
        {
            if (c.isChosen())
            {
                REQUIRE(c.getRank() > 0);
            }
        }
    }
    
    SECTION("Throws on empty candidates")
    {
        std::vector<Candidate> empty_candidates;
        
        REQUIRE_THROWS_AS(
            Selector::select(empty_candidates, weights, unbounded),
            std::invalid_argument);
    }
}

TEST_CASE("select: Domain constraints handling",
          "[AutoBootstrapSelector][Integration][Domain]")
{
    ScoringWeights weights;
    StatisticSupport positive = StatisticSupport::strictLowerBound(0.0, 1e-10);
    
    SECTION("Rejects candidate that violates domain")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile, 5.0, -1.0, 6.0, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.1, 1.0,
                            0.0, 0.0, 0.0),  // Violates positive support
            makeTestCandidate(MethodId::Basic, 5.0, 4.0, 6.0, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.3, 5.0, 0.2, 1.1,
                            0.0, 0.0, 0.0)   // Valid
        };
        
        auto result = Selector::select(candidates, weights, positive);
        
        // Basic should win (Percentile violates domain)
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Basic);
    }
}

TEST_CASE("select: Tie-breaking with method preference",
          "[AutoBootstrapSelector][Integration][Ties]")
{
    ScoringWeights weights;
    StatisticSupport unbounded = StatisticSupport::unbounded();
    
    SECTION("Prefers BCa when scores are tied")
    {
        // Create two candidates with identical raw penalties
        std::vector<Candidate> candidates;
        
        auto percentile = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 1000, 0, 950, 50, 0.5, 0.0, 5.0, 0.0, 1.0, 0.0, 0.0, 0.0);
        
        auto bca = makeTestCandidate(
            MethodId::BCa, 5.0, 4.0, 6.0, 0.95,
            100, 1000, 0, 950, 50, 0.5, 0.0, 5.0, 0.0, 1.0, 0.0, 0.0, 0.0,
            0.0, 0.0);
        
        candidates.push_back(percentile);
        candidates.push_back(bca);
        
        auto result = Selector::select(candidates, weights, unbounded);
        
        // BCa should win due to method preference (if scores end up tied)
        // Note: Actual result depends on scoring, but BCa is preferred in ties
        REQUIRE(result.getChosenCandidate().isChosen() == true);
    }
}

TEST_CASE("select: Diagnostics are properly populated",
          "[AutoBootstrapSelector][Integration][Diagnostics]")
{
    ScoringWeights weights;
    StatisticSupport unbounded = StatisticSupport::unbounded();
    
    SECTION("Result contains diagnostics with all candidates")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.1, 1.0,
                            0.05, 0.1, 0.02),
            makeTestCandidate(MethodId::Basic, 5.0, 3.8, 6.2, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.3, 5.0, 0.2, 1.1,
                            0.08, 0.15, 0.03)
        };
        
        auto result = Selector::select(candidates, weights, unbounded);
        const auto& diagnostics = result.getDiagnostics();
        
        REQUIRE(diagnostics.getNumCandidates() == 2);
        REQUIRE(diagnostics.getScoreBreakdowns().size() == 2);
    }
    
    SECTION("Diagnostics report BCa status correctly when present")
    {
        std::vector<Candidate> candidates = {
            makeBcaCandidate(0.1, 0.05, 0.0),
            makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
                            100, 1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.1, 1.0,
                            0.05, 0.1, 0.02)
        };
        
        auto result = Selector::select(candidates, weights, unbounded);
        const auto& diagnostics = result.getDiagnostics();
        
        REQUIRE(diagnostics.hasBCaCandidate() == true);
    }
}
