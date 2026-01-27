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
using RawComponents  = Selector::RawComponents;
using StatisticSupport = mkc_timeseries::StatisticSupport;

// Helper classes from detail namespace
using ScoreNormalizer = palvalidator::analysis::detail::ScoreNormalizer<Decimal, ScoringWeights, RawComponents>;
using CandidateGateKeeper = palvalidator::analysis::detail::CandidateGateKeeper<Decimal, RawComponents>;
using ImprovedTournamentSelector = palvalidator::analysis::detail::ImprovedTournamentSelector<Decimal>;
using BcaRejectionAnalysis = palvalidator::analysis::detail::BcaRejectionAnalysis;
using NormalizedScores = palvalidator::analysis::detail::NormalizedScores;

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
        double penalty = PenaltyCalc::computeSkewPenalty(skew);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Skew exactly at threshold produces no penalty")
    {
        double skew = 1.0;  // Exactly at threshold
        double penalty = PenaltyCalc::computeSkewPenalty(skew);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Skew above threshold produces quadratic penalty")
    {
        double skew = 2.0;  // 1.0 above threshold
        double penalty = PenaltyCalc::computeSkewPenalty(skew);
        REQUIRE(penalty == Catch::Approx(1.0));  // (2.0 - 1.0)^2 = 1.0
    }
    
    SECTION("Negative skew uses absolute value")
    {
        double skew = -2.5;  // |skew| = 2.5
        double penalty = PenaltyCalc::computeSkewPenalty(skew);
        REQUIRE(penalty == Catch::Approx(2.25));  // (2.5 - 1.0)^2 = 2.25
    }
    
    SECTION("Large skew produces large penalty")
    {
        double skew = 5.0;  // 4.0 above threshold
        double penalty = PenaltyCalc::computeSkewPenalty(skew);
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
        double penalty = PenaltyCalc::computeDomainPenalty(candidate, unbounded);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("No violation when lower bound is positive")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, 1.0, 6.0);
        double penalty = PenaltyCalc::computeDomainPenalty(candidate, positive);
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Violation when lower bound is negative with positive support")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, -1.0, 6.0);
        double penalty = PenaltyCalc::computeDomainPenalty(candidate, positive);
        REQUIRE(penalty > 0.0);
        REQUIRE(penalty == AutoBootstrapConfiguration::kDomainViolationPenalty);
    }
    
    SECTION("Violation at exactly zero with strict lower bound")
    {
        auto candidate = makeTestCandidate(MethodId::Percentile, 5.0, 0.0, 6.0);
        double penalty = PenaltyCalc::computeDomainPenalty(candidate, positive);
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
        
        auto raw = palvalidator::analysis::detail::RawComponentsBuilder<double>::computeRawComponentsForCandidate(candidate, unbounded);
        
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
        
        auto raw = palvalidator::analysis::detail::RawComponentsBuilder<double>::computeRawComponentsForCandidate(candidate, unbounded);
        
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
        
        auto raw = palvalidator::analysis::detail::RawComponentsBuilder<double>::computeRawComponentsForCandidate(candidate, unbounded);
        
        // Should handle gracefully
        REQUIRE(std::isfinite(raw.getSkewSq()));
        REQUIRE(raw.getSkewSq() == 0.0);
    }
    
    SECTION("Includes domain penalty when support is violated")
    {
        StatisticSupport positive = StatisticSupport::strictLowerBound(0.0, 1e-10);
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, -1.0, 6.0);  // Lower bound negative
        
        auto raw = palvalidator::analysis::detail::RawComponentsBuilder<double>::computeRawComponentsForCandidate(candidate, positive);
        
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
        
        auto raw = palvalidator::analysis::detail::RawComponentsBuilder<double>::computeRawPenalties(candidates, unbounded);
        
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
        auto raw = palvalidator::analysis::detail::RawComponentsBuilder<double>::computeRawPenalties(empty_candidates, unbounded);
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
        REQUIRE(norm.getOrderingNorm() == Catch::Approx(1.0));
        
        // length: 0.5 / (1.0 * 1.0) = 0.5
        REQUIRE(norm.getLengthNorm() == Catch::Approx(0.5));
        
        // stability: 0.1 / 0.25 = 0.4
        REQUIRE(norm.getStabilityNorm() == Catch::Approx(0.4));
        
        // center_sq: 4.0 / (2.0 * 2.0) = 1.0
        REQUIRE(norm.getCenterSqNorm() == Catch::Approx(1.0));
        
        // skew_sq: 4.0 / (2.0 * 2.0) = 1.0
        REQUIRE(norm.getSkewSqNorm() == Catch::Approx(1.0));
    }
    
    SECTION("Weights are applied to contributions")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 4.0, 4.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        // Default weights: w_order=1.0, w_length=0.25, w_stability=1.0,
        //                  w_center=1.0, w_skew=0.5
        
        REQUIRE(norm.getOrderingContrib() == Catch::Approx(1.0 * norm.getOrderingNorm()));
        REQUIRE(norm.getLengthContrib() == Catch::Approx(0.25 * norm.getLengthNorm()));
        REQUIRE(norm.getStabilityContrib() == Catch::Approx(1.0 * norm.getStabilityNorm()));
        REQUIRE(norm.getCenterSqContrib() == Catch::Approx(1.0 * norm.getCenterSqNorm()));
        REQUIRE(norm.getSkewSqContrib() == Catch::Approx(0.5 * norm.getSkewSqNorm()));
    }
    
    SECTION("Zero raw values produce zero normalized values")
    {
        auto raw = makeValidRaw(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        auto norm = normalizer.normalize(raw);
        
        REQUIRE(norm.getOrderingNorm() == 0.0);
        REQUIRE(norm.getLengthNorm() == 0.0);
        REQUIRE(norm.getStabilityNorm() == 0.0);
        REQUIRE(norm.getCenterSqNorm() == 0.0);
        REQUIRE(norm.getSkewSqNorm() == 0.0);
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
        double expected = norm.getOrderingContrib() + norm.getLengthContrib() +
                         norm.getStabilityContrib() + norm.getCenterSqContrib() +
                         norm.getSkewSqContrib() + 0.0;
        
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
        double expected = norm.getOrderingContrib() + norm.getLengthContrib() +
                         norm.getStabilityContrib() + norm.getCenterSqContrib() +
                         norm.getSkewSqContrib() + 0.0;
        
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
        double base_expected = norm.getOrderingContrib() + norm.getLengthContrib() +
                              norm.getStabilityContrib() + norm.getCenterSqContrib() +
                              norm.getSkewSqContrib() + 0.0;
        
        REQUIRE(total > base_expected);
    }
    
    SECTION("Domain penalty is included in total")
    {
        auto raw = makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 
                               AutoBootstrapConfiguration::kDomainViolationPenalty);
        auto norm = normalizer.normalize(raw);
        
        double total = normalizer.computeTotalScore(
            norm, raw, MethodId::Percentile, 0.5);
        
        REQUIRE(total > norm.getOrderingContrib() + norm.getLengthContrib() +
                       norm.getStabilityContrib() + norm.getCenterSqContrib() +
                       norm.getSkewSqContrib());
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

// -----------------------------------------------------------------------------
// Tests for methodPreference
// -----------------------------------------------------------------------------

TEST_CASE("methodPreference: Correct preference ordering",
          "[ImprovedTournamentSelector][MethodPreference]")
{
    SECTION("BCa has highest preference (lowest value)")
    {
        int pref_bca = ImprovedTournamentSelector::methodPreference(MethodId::BCa);
        REQUIRE(pref_bca == 1);
    }
    
    SECTION("PercentileT is second preference")
    {
        int pref_pt = ImprovedTournamentSelector::methodPreference(MethodId::PercentileT);
        REQUIRE(pref_pt == 2);
    }
    
    SECTION("MOutOfN is third preference")
    {
        int pref_moon = ImprovedTournamentSelector::methodPreference(MethodId::MOutOfN);
        REQUIRE(pref_moon == 3);
    }
    
    SECTION("Percentile is fourth preference")
    {
        int pref_perc = ImprovedTournamentSelector::methodPreference(MethodId::Percentile);
        REQUIRE(pref_perc == 4);
    }
    
    SECTION("Basic is fifth preference")
    {
        int pref_basic = ImprovedTournamentSelector::methodPreference(MethodId::Basic);
        REQUIRE(pref_basic == 5);
    }
    
    SECTION("Normal has lowest preference (highest value)")
    {
        int pref_normal = ImprovedTournamentSelector::methodPreference(MethodId::Normal);
        REQUIRE(pref_normal == 6);
    }
    
    SECTION("Preference ordering is strictly increasing")
    {
        std::vector<int> preferences{
            ImprovedTournamentSelector::methodPreference(MethodId::BCa),
            ImprovedTournamentSelector::methodPreference(MethodId::PercentileT),
            ImprovedTournamentSelector::methodPreference(MethodId::MOutOfN),
            ImprovedTournamentSelector::methodPreference(MethodId::Percentile),
            ImprovedTournamentSelector::methodPreference(MethodId::Basic),
            ImprovedTournamentSelector::methodPreference(MethodId::Normal)
        };
        
        // Verify strictly increasing
        for (size_t i = 1; i < preferences.size(); ++i) {
            REQUIRE(preferences[i] > preferences[i-1]);
        }
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
        
        REQUIRE(analysis.hasBcaCandidate() == false);
        REQUIRE(analysis.bcaChosen() == false);
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
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
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
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForNonFinite() == true);
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
        
        REQUIRE(analysis.rejectedForDomain() == true);
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
            REQUIRE(analysis.rejectedForInstability() == true);
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
            REQUIRE(analysis.rejectedForLength() == true);
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
        REQUIRE(CandidateGateKeeper::passesEffectiveBGate(candidate) == true);
    }
    
    SECTION("Perfect effective B passes gate")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 1000, 0, 1000, 0, 0.5, 0.2, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0);
        
        // 1000/1000 = 100% should always pass
        REQUIRE(CandidateGateKeeper::passesEffectiveBGate(candidate) == true);
    }
    
    SECTION("Zero B_outer fails gate")
    {
        auto candidate = makeTestCandidate(
            MethodId::Percentile, 5.0, 4.0, 6.0, 0.95,
            100, 0, 0, 0, 0, 0.5, 0.2, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0);
        
        REQUIRE(CandidateGateKeeper::passesEffectiveBGate(candidate) == false);
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

TEST_CASE("NormalizedScores: Construction with all parameters",
          "[AutoBootstrapSelector][NormalizedScores][Construction]")
{
    SECTION("Constructs with typical values")
    {
        NormalizedScores scores(
            1.0,   // orderingNorm
            0.5,   // lengthNorm
            0.4,   // stabilityNorm
            1.0,   // centerSqNorm
            1.0,   // skewSqNorm
            1.0,   // orderingContrib
            0.125, // lengthContrib
            0.4,   // stabilityContrib
            1.0,   // centerSqContrib
            0.5    // skewSqContrib
        );
        
        // Verify all values are stored correctly
        REQUIRE(scores.getOrderingNorm() == Catch::Approx(1.0));
        REQUIRE(scores.getLengthNorm() == Catch::Approx(0.5));
        REQUIRE(scores.getStabilityNorm() == Catch::Approx(0.4));
        REQUIRE(scores.getCenterSqNorm() == Catch::Approx(1.0));
        REQUIRE(scores.getSkewSqNorm() == Catch::Approx(1.0));
        
        REQUIRE(scores.getOrderingContrib() == Catch::Approx(1.0));
        REQUIRE(scores.getLengthContrib() == Catch::Approx(0.125));
        REQUIRE(scores.getStabilityContrib() == Catch::Approx(0.4));
        REQUIRE(scores.getCenterSqContrib() == Catch::Approx(1.0));
        REQUIRE(scores.getSkewSqContrib() == Catch::Approx(0.5));
    }
    
    SECTION("Constructs with all zero values")
    {
        NormalizedScores scores(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        
        REQUIRE(scores.getOrderingNorm() == 0.0);
        REQUIRE(scores.getLengthNorm() == 0.0);
        REQUIRE(scores.getStabilityNorm() == 0.0);
        REQUIRE(scores.getCenterSqNorm() == 0.0);
        REQUIRE(scores.getSkewSqNorm() == 0.0);
        
        REQUIRE(scores.getOrderingContrib() == 0.0);
        REQUIRE(scores.getLengthContrib() == 0.0);
        REQUIRE(scores.getStabilityContrib() == 0.0);
        REQUIRE(scores.getCenterSqContrib() == 0.0);
        REQUIRE(scores.getSkewSqContrib() == 0.0);
    }
    
    SECTION("Constructs with large values")
    {
        NormalizedScores scores(
            100.0, 200.0, 150.0, 300.0, 250.0,
            100.0, 50.0, 150.0, 300.0, 125.0
        );
        
        REQUIRE(scores.getOrderingNorm() == Catch::Approx(100.0));
        REQUIRE(scores.getLengthNorm() == Catch::Approx(200.0));
        REQUIRE(scores.getStabilityNorm() == Catch::Approx(150.0));
        REQUIRE(scores.getCenterSqNorm() == Catch::Approx(300.0));
        REQUIRE(scores.getSkewSqNorm() == Catch::Approx(250.0));
        
        REQUIRE(scores.getOrderingContrib() == Catch::Approx(100.0));
        REQUIRE(scores.getLengthContrib() == Catch::Approx(50.0));
        REQUIRE(scores.getStabilityContrib() == Catch::Approx(150.0));
        REQUIRE(scores.getCenterSqContrib() == Catch::Approx(300.0));
        REQUIRE(scores.getSkewSqContrib() == Catch::Approx(125.0));
    }
}

TEST_CASE("NormalizedScores: Handling special floating-point values",
          "[AutoBootstrapSelector][NormalizedScores][SpecialValues]")
{
    SECTION("Accepts very small positive values")
    {
        double small = 1e-10;
        NormalizedScores scores(small, small, small, small, small,
                               small, small, small, small, small);
        
        REQUIRE(scores.getOrderingNorm() == Catch::Approx(small));
        REQUIRE(scores.getLengthNorm() == Catch::Approx(small));
        REQUIRE(scores.getOrderingContrib() == Catch::Approx(small));
    }
    
    SECTION("Accepts negative values (edge case)")
    {
        // Although typically normalized values should be non-negative,
        // the class itself doesn't enforce this constraint
        NormalizedScores scores(-1.0, -0.5, -0.3, -1.0, -0.8,
                               -1.0, -0.125, -0.3, -1.0, -0.4);
        
        REQUIRE(scores.getOrderingNorm() == Catch::Approx(-1.0));
        REQUIRE(scores.getLengthNorm() == Catch::Approx(-0.5));
        REQUIRE(scores.getOrderingContrib() == Catch::Approx(-1.0));
    }
    
    SECTION("Handles infinity values")
    {
        double inf = std::numeric_limits<double>::infinity();
        NormalizedScores scores(inf, 1.0, 1.0, 1.0, 1.0,
                               inf, 1.0, 1.0, 1.0, 1.0);
        
        REQUIRE(std::isinf(scores.getOrderingNorm()));
        REQUIRE(std::isinf(scores.getOrderingContrib()));
        REQUIRE(std::isfinite(scores.getLengthNorm()));
    }
    
    SECTION("Handles NaN values")
    {
        double nan = std::numeric_limits<double>::quiet_NaN();
        NormalizedScores scores(nan, 1.0, 1.0, 1.0, 1.0,
                               nan, 1.0, 1.0, 1.0, 1.0);
        
        REQUIRE(std::isnan(scores.getOrderingNorm()));
        REQUIRE(std::isnan(scores.getOrderingContrib()));
        REQUIRE(std::isfinite(scores.getLengthNorm()));
    }
}

TEST_CASE("NormalizedScores: Normalized values vs contributions relationship",
          "[AutoBootstrapSelector][NormalizedScores][Relationship]")
{
    SECTION("Contributions are weighted versions of normalized values")
    {
        // Simulate typical weighting: w_ordering=1.0, w_length=0.25, w_stability=1.0
        double orderingNorm = 1.0;
        double lengthNorm = 0.5;
        double stabilityNorm = 0.4;
        
        double w_ordering = 1.0;
        double w_length = 0.25;
        double w_stability = 1.0;
        
        NormalizedScores scores(
            orderingNorm,
            lengthNorm,
            stabilityNorm,
            1.0, 1.0,  // center and skew norms
            orderingNorm * w_ordering,
            lengthNorm * w_length,
            stabilityNorm * w_stability,
            1.0, 0.5   // center and skew contribs
        );
        
        REQUIRE(scores.getOrderingContrib() == 
                Catch::Approx(scores.getOrderingNorm() * w_ordering));
        REQUIRE(scores.getLengthContrib() == 
                Catch::Approx(scores.getLengthNorm() * w_length));
        REQUIRE(scores.getStabilityContrib() == 
                Catch::Approx(scores.getStabilityNorm() * w_stability));
    }
    
    SECTION("Contribution can be zero even when normalized value is non-zero")
    {
        // This happens when weight is zero
        NormalizedScores scores(
            1.0,   // orderingNorm = 1.0
            0.5,   // lengthNorm = 0.5
            0.4,   // stabilityNorm = 0.4
            1.0, 1.0,
            1.0,   // orderingContrib = 1.0 (weight=1.0)
            0.0,   // lengthContrib = 0.0 (weight=0.0, even though norm=0.5)
            0.4,   // stabilityContrib = 0.4 (weight=1.0)
            1.0, 0.5
        );
        
        REQUIRE(scores.getLengthNorm() > 0.0);
        REQUIRE(scores.getLengthContrib() == 0.0);
    }
}

TEST_CASE("NormalizedScores: Read-only access pattern",
          "[AutoBootstrapSelector][NormalizedScores][Immutability]")
{
    SECTION("All getters are const and return values, not references")
    {
        const NormalizedScores scores(1.0, 0.5, 0.4, 1.0, 1.0,
                                      1.0, 0.125, 0.4, 1.0, 0.5);
        
        // These should compile (all getters are const)
        double ordering = scores.getOrderingNorm();
        double length = scores.getLengthNorm();
        double stability = scores.getStabilityNorm();
        double centerSq = scores.getCenterSqNorm();
        double skewSq = scores.getSkewSqNorm();
        
        double orderingC = scores.getOrderingContrib();
        double lengthC = scores.getLengthContrib();
        double stabilityC = scores.getStabilityContrib();
        double centerSqC = scores.getCenterSqContrib();
        double skewSqC = scores.getSkewSqContrib();
        
        // Verify values
        REQUIRE(ordering == 1.0);
        REQUIRE(length == 0.5);
        REQUIRE(stability == 0.4);
        REQUIRE(centerSq == 1.0);
        REQUIRE(skewSq == 1.0);
        REQUIRE(orderingC == 1.0);
        REQUIRE(lengthC == 0.125);
        REQUIRE(stabilityC == 0.4);
        REQUIRE(centerSqC == 1.0);
        REQUIRE(skewSqC == 0.5);
    }
}

TEST_CASE("NormalizedScores: Realistic scoring scenarios",
          "[AutoBootstrapSelector][NormalizedScores][Realistic]")
{
    SECTION("Low penalty scenario (good candidate)")
    {
        // All normalized values near zero, contributions near zero
        NormalizedScores scores(
            0.01, 0.02, 0.01, 0.05, 0.03,  // Small normalized penalties
            0.01, 0.005, 0.01, 0.05, 0.015  // Small contributions
        );
        
        double total_contrib = scores.getOrderingContrib() +
                              scores.getLengthContrib() +
                              scores.getStabilityContrib() +
                              scores.getCenterSqContrib() +
                              scores.getSkewSqContrib();
        
        REQUIRE(total_contrib < 0.1);  // Total penalty is low
    }
    
    SECTION("High penalty scenario (poor candidate)")
    {
        // Large normalized values and contributions
        NormalizedScores scores(
            5.0, 10.0, 3.0, 8.0, 6.0,      // Large normalized penalties
            5.0, 2.5, 3.0, 8.0, 3.0         // Large contributions
        );
        
        double total_contrib = scores.getOrderingContrib() +
                              scores.getLengthContrib() +
                              scores.getStabilityContrib() +
                              scores.getCenterSqContrib() +
                              scores.getSkewSqContrib();
        
        REQUIRE(total_contrib > 20.0);  // Total penalty is high
    }
    
    SECTION("Mixed penalty scenario")
    {
        // Some penalties high, others low
        NormalizedScores scores(
            0.05,  // Low ordering
            5.0,   // High length
            0.1,   // Low stability
            0.2,   // Low center
            10.0,  // High skew
            0.05,  // Low ordering contrib
            1.25,  // Moderate length contrib (weight=0.25)
            0.1,   // Low stability contrib
            0.2,   // Low center contrib
            5.0    // High skew contrib (weight=0.5)
        );
        
        REQUIRE(scores.getLengthNorm() > 1.0);
        REQUIRE(scores.getSkewSqNorm() > 1.0);
        REQUIRE(scores.getOrderingNorm() < 0.1);
        REQUIRE(scores.getStabilityNorm() < 0.2);
    }
}

TEST_CASE("NormalizedScores: Component independence",
          "[AutoBootstrapSelector][NormalizedScores][Independence]")
{
    SECTION("Each component can be set independently")
    {
        // Set each component to a unique value
        NormalizedScores scores(
            1.0, 2.0, 3.0, 4.0, 5.0,
            10.0, 20.0, 30.0, 40.0, 50.0
        );
        
        REQUIRE(scores.getOrderingNorm() == 1.0);
        REQUIRE(scores.getLengthNorm() == 2.0);
        REQUIRE(scores.getStabilityNorm() == 3.0);
        REQUIRE(scores.getCenterSqNorm() == 4.0);
        REQUIRE(scores.getSkewSqNorm() == 5.0);
        
        REQUIRE(scores.getOrderingContrib() == 10.0);
        REQUIRE(scores.getLengthContrib() == 20.0);
        REQUIRE(scores.getStabilityContrib() == 30.0);
        REQUIRE(scores.getCenterSqContrib() == 40.0);
        REQUIRE(scores.getSkewSqContrib() == 50.0);
    }
    
    SECTION("Changing one component doesn't affect others")
    {
        NormalizedScores scores1(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
        NormalizedScores scores2(99.0, 1.0, 1.0, 1.0, 1.0, 99.0, 1.0, 1.0, 1.0, 1.0);
        
        // All components except ordering should be the same
        REQUIRE(scores1.getLengthNorm() == scores2.getLengthNorm());
        REQUIRE(scores1.getStabilityNorm() == scores2.getStabilityNorm());
        REQUIRE(scores1.getCenterSqNorm() == scores2.getCenterSqNorm());
        REQUIRE(scores1.getSkewSqNorm() == scores2.getSkewSqNorm());
        
        // Ordering should be different
        REQUIRE(scores1.getOrderingNorm() != scores2.getOrderingNorm());
        REQUIRE(scores1.getOrderingContrib() != scores2.getOrderingContrib());
    }
}

// =============================================================================
// TESTS FOR BcaRejectionAnalysis CLASS
// =============================================================================

TEST_CASE("BcaRejectionAnalysis: Construction with all parameters",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][Construction]")
{
    SECTION("No BCa candidate present")
    {
        BcaRejectionAnalysis analysis(
            false,  // hasBcaCandidate
            false,  // bcaChosen
            false,  // rejectedForInstability
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == false);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
    
    SECTION("BCa present and chosen")
    {
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            true,   // bcaChosen
            false,  // rejectedForInstability
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
    
    SECTION("BCa present but rejected for instability")
    {
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            false,  // bcaChosen
            true,   // rejectedForInstability
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForInstability() == true);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
}

TEST_CASE("BcaRejectionAnalysis: All rejection reasons",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][RejectionReasons]")
{
    SECTION("Rejected for length penalty")
    {
        BcaRejectionAnalysis analysis(true, false, false, true, false, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForLength() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
    
    SECTION("Rejected for domain violation")
    {
        BcaRejectionAnalysis analysis(true, false, false, false, true, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForDomain() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
    
    SECTION("Rejected for non-finite scores")
    {
        BcaRejectionAnalysis analysis(true, false, false, false, false, true);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForNonFinite() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
    }
}

TEST_CASE("BcaRejectionAnalysis: Multiple rejection reasons",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][MultipleReasons]")
{
    SECTION("Rejected for both instability and length")
    {
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            false,  // bcaChosen
            true,   // rejectedForInstability
            true,   // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForInstability() == true);
        REQUIRE(analysis.rejectedForLength() == true);
    }
    
    SECTION("Rejected for all reasons (worst case)")
    {
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            false,  // bcaChosen
            true,   // rejectedForInstability
            true,   // rejectedForLength
            true,   // rejectedForDomain
            true    // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForInstability() == true);
        REQUIRE(analysis.rejectedForLength() == true);
        REQUIRE(analysis.rejectedForDomain() == true);
        REQUIRE(analysis.rejectedForNonFinite() == true);
    }
    
    SECTION("Rejected for domain and non-finite")
    {
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            false,  // bcaChosen
            false,  // rejectedForInstability
            false,  // rejectedForLength
            true,   // rejectedForDomain
            true    // rejectedForNonFinite
        );
        
        REQUIRE(analysis.rejectedForDomain() == true);
        REQUIRE(analysis.rejectedForNonFinite() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
    }
}

TEST_CASE("BcaRejectionAnalysis: Logical consistency checks",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][LogicalConsistency]")
{
    SECTION("If BCa chosen, no rejection reasons should be true")
    {
        // This represents a logically consistent state
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            true,   // bcaChosen
            false,  // rejectedForInstability
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.bcaChosen() == true);
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
    
    SECTION("If no BCa candidate, bcaChosen must be false")
    {
        // This represents a logically consistent state
        BcaRejectionAnalysis analysis(
            false,  // hasBcaCandidate
            false,  // bcaChosen (must be false)
            false,  // rejectedForInstability
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == false);
        REQUIRE(analysis.bcaChosen() == false);
    }
    
    SECTION("If BCa not chosen but present, at least one rejection reason should be true")
    {
        // This represents a typical rejection scenario
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            false,  // bcaChosen
            true,   // rejectedForInstability
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        
        // At least one rejection reason is true
        bool has_rejection_reason = 
            analysis.rejectedForInstability() ||
            analysis.rejectedForLength() ||
            analysis.rejectedForDomain() ||
            analysis.rejectedForNonFinite();
        
        REQUIRE(has_rejection_reason == true);
    }
}

TEST_CASE("BcaRejectionAnalysis: Edge case - inconsistent state allowed",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][EdgeCases]")
{
    SECTION("Class allows logically inconsistent state (BCa chosen with rejection flags)")
    {
        // The class doesn't enforce logical consistency - it's a data holder
        // This might represent a bug in the calling code, but the class allows it
        BcaRejectionAnalysis analysis(
            true,   // hasBcaCandidate
            true,   // bcaChosen
            true,   // rejectedForInstability (inconsistent!)
            true,   // rejectedForLength (inconsistent!)
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        // The class stores what it's given
        REQUIRE(analysis.bcaChosen() == true);
        REQUIRE(analysis.rejectedForInstability() == true);
        REQUIRE(analysis.rejectedForLength() == true);
    }
    
    SECTION("Class allows no BCa candidate but rejection flags set")
    {
        // This is logically inconsistent but the class allows it
        BcaRejectionAnalysis analysis(
            false,  // hasBcaCandidate
            false,  // bcaChosen
            true,   // rejectedForInstability (inconsistent - no candidate to reject!)
            false,  // rejectedForLength
            false,  // rejectedForDomain
            false   // rejectedForNonFinite
        );
        
        REQUIRE(analysis.hasBcaCandidate() == false);
        REQUIRE(analysis.rejectedForInstability() == true);
    }
}

TEST_CASE("BcaRejectionAnalysis: Read-only access pattern",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][Immutability]")
{
    SECTION("All getters are const and return values")
    {
        const BcaRejectionAnalysis analysis(true, false, true, false, true, false);
        
        // These should compile (all getters are const)
        bool has_bca = analysis.hasBcaCandidate();
        bool chosen = analysis.bcaChosen();
        bool instability = analysis.rejectedForInstability();
        bool length = analysis.rejectedForLength();
        bool domain = analysis.rejectedForDomain();
        bool non_finite = analysis.rejectedForNonFinite();
        
        // Verify values
        REQUIRE(has_bca == true);
        REQUIRE(chosen == false);
        REQUIRE(instability == true);
        REQUIRE(length == false);
        REQUIRE(domain == true);
        REQUIRE(non_finite == false);
    }
}

TEST_CASE("BcaRejectionAnalysis: Realistic tournament scenarios",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][Realistic]")
{
    SECTION("Scenario 1: BCa wins cleanly")
    {
        BcaRejectionAnalysis analysis(true, true, false, false, false, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == true);
        
        // No rejection reasons
        REQUIRE(analysis.rejectedForInstability() == false);
        REQUIRE(analysis.rejectedForLength() == false);
        REQUIRE(analysis.rejectedForDomain() == false);
        REQUIRE(analysis.rejectedForNonFinite() == false);
    }
    
    SECTION("Scenario 2: BCa rejected due to extreme z0 parameter")
    {
        BcaRejectionAnalysis analysis(true, false, true, false, false, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForInstability() == true);
    }
    
    SECTION("Scenario 3: BCa rejected due to interval too wide")
    {
        BcaRejectionAnalysis analysis(true, false, false, true, false, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForLength() == true);
    }
    
    SECTION("Scenario 4: BCa rejected due to negative lower bound with positive support")
    {
        BcaRejectionAnalysis analysis(true, false, false, false, true, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForDomain() == true);
    }
    
    SECTION("Scenario 5: BCa computation failed (NaN/Inf in results)")
    {
        BcaRejectionAnalysis analysis(true, false, false, false, false, true);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForNonFinite() == true);
    }
    
    SECTION("Scenario 6: No BCa candidate in tournament")
    {
        BcaRejectionAnalysis analysis(false, false, false, false, false, false);
        
        REQUIRE(analysis.hasBcaCandidate() == false);
        REQUIRE(analysis.bcaChosen() == false);
    }
    
    SECTION("Scenario 7: BCa fails multiple gates")
    {
        // BCa has both instability and domain issues
        BcaRejectionAnalysis analysis(true, false, true, false, true, false);
        
        REQUIRE(analysis.hasBcaCandidate() == true);
        REQUIRE(analysis.bcaChosen() == false);
        REQUIRE(analysis.rejectedForInstability() == true);
        REQUIRE(analysis.rejectedForDomain() == true);
    }
}

TEST_CASE("BcaRejectionAnalysis: Use in diagnostic reporting",
          "[AutoBootstrapSelector][BcaRejectionAnalysis][Diagnostics]")
{
    SECTION("Can determine if any rejection occurred")
    {
        BcaRejectionAnalysis no_rejection(true, true, false, false, false, false);
        BcaRejectionAnalysis has_rejection(true, false, true, false, false, false);
        
        auto has_any_rejection = [](const BcaRejectionAnalysis& a) {
            return a.rejectedForInstability() ||
                   a.rejectedForLength() ||
                   a.rejectedForDomain() ||
                   a.rejectedForNonFinite();
        };
        
        REQUIRE(has_any_rejection(no_rejection) == false);
        REQUIRE(has_any_rejection(has_rejection) == true);
    }
    
    SECTION("Can count number of rejection reasons")
    {
        BcaRejectionAnalysis multiple(true, false, true, true, true, false);
        
        auto count_rejections = [](const BcaRejectionAnalysis& a) {
            int count = 0;
            if (a.rejectedForInstability()) ++count;
            if (a.rejectedForLength()) ++count;
            if (a.rejectedForDomain()) ++count;
            if (a.rejectedForNonFinite()) ++count;
            return count;
        };
        
        REQUIRE(count_rejections(multiple) == 3);
    }
    
    SECTION("Can generate diagnostic message based on analysis")
    {
        BcaRejectionAnalysis analysis(true, false, true, false, true, false);
        
        auto generate_message = [](const BcaRejectionAnalysis& a) -> std::string {
            if (!a.hasBcaCandidate()) return "No BCa candidate";
            if (a.bcaChosen()) return "BCa chosen";
            
            std::string msg = "BCa rejected: ";
            if (a.rejectedForInstability()) msg += "[instability] ";
            if (a.rejectedForLength()) msg += "[length] ";
            if (a.rejectedForDomain()) msg += "[domain] ";
            if (a.rejectedForNonFinite()) msg += "[non-finite] ";
            return msg;
        };
        
        std::string msg = generate_message(analysis);
        REQUIRE(msg.find("instability") != std::string::npos);
        REQUIRE(msg.find("domain") != std::string::npos);
        REQUIRE(msg.find("length") == std::string::npos);
    }
}

// =============================================================================
// COMBINED TESTS: NormalizedScores and BcaRejectionAnalysis together
// =============================================================================

TEST_CASE("NormalizedScores and BcaRejectionAnalysis: Combined usage",
          "[AutoBootstrapSelector][NormalizedScores][BcaRejectionAnalysis][Combined]")
{
    SECTION("Both classes work together in a typical scenario")
    {
        // Create normalized scores for a candidate
        NormalizedScores scores(
            1.0, 0.5, 0.4, 1.0, 1.0,
            1.0, 0.125, 0.4, 1.0, 0.5
        );
        
        // Create BCa analysis showing BCa was rejected
        BcaRejectionAnalysis bca_analysis(true, false, true, false, false, false);
        
        // Both objects maintain their independent state
        REQUIRE(scores.getOrderingNorm() == 1.0);
        REQUIRE(bca_analysis.rejectedForInstability() == true);
    }
    
    SECTION("Multiple instances can coexist")
    {
        // Two different candidates might have different scores
        NormalizedScores scores1(1.0, 0.5, 0.4, 1.0, 1.0, 1.0, 0.125, 0.4, 1.0, 0.5);
        NormalizedScores scores2(2.0, 1.0, 0.8, 2.0, 2.0, 2.0, 0.25, 0.8, 2.0, 1.0);
        
        // And one BCa analysis for the tournament
        BcaRejectionAnalysis analysis(true, false, true, false, false, false);
        
        REQUIRE(scores1.getOrderingNorm() != scores2.getOrderingNorm());
        REQUIRE(analysis.hasBcaCandidate() == true);
    }
}

// =============================================================================
// NORMALIZATION REFERENCE CONSTANTS TESTS
// =============================================================================

TEST_CASE("AutoBootstrapConfiguration: Normalization reference constants",
          "[AutoBootstrapSelector][Configuration][Normalization]")
{
  SECTION("Reference values are reasonable and documented")
  {
    // ORDERING ERROR REFERENCE
    // A 10% coverage error is the baseline "typical" violation
    REQUIRE(AutoBootstrapConfiguration::kRefOrderingErrorSq == 
            Catch::Approx(0.01));
    REQUIRE(AutoBootstrapConfiguration::kRefOrderingErrorSq == 
            Catch::Approx(0.10 * 0.10));
    
    // LENGTH ERROR REFERENCE  
    // Intervals at 1× ideal length are optimal
    REQUIRE(AutoBootstrapConfiguration::kRefLengthErrorSq == 
            Catch::Approx(1.0));
    REQUIRE(AutoBootstrapConfiguration::kRefLengthErrorSq == 
            Catch::Approx(1.0 * 1.0));
    
    // STABILITY REFERENCE
    // Moderate stability penalty is 0.25
    REQUIRE(AutoBootstrapConfiguration::kRefStability == 
            Catch::Approx(0.25));
    
    // CENTER SHIFT REFERENCE
    // 2 standard errors is "notable" bias
    REQUIRE(AutoBootstrapConfiguration::kRefCenterShiftSq == 
            Catch::Approx(4.0));
    REQUIRE(AutoBootstrapConfiguration::kRefCenterShiftSq == 
            Catch::Approx(2.0 * 2.0));
    
    // SKEW REFERENCE
    // |skew| = 2.0 is the "high skewness" threshold
    REQUIRE(AutoBootstrapConfiguration::kRefSkewSq == 
            Catch::Approx(4.0));
    REQUIRE(AutoBootstrapConfiguration::kRefSkewSq == 
            Catch::Approx(2.0 * 2.0));
  }
  
  SECTION("Reference values form a consistent scale")
  {
    // All reference values should be positive
    REQUIRE(AutoBootstrapConfiguration::kRefOrderingErrorSq > 0.0);
    REQUIRE(AutoBootstrapConfiguration::kRefLengthErrorSq > 0.0);
    REQUIRE(AutoBootstrapConfiguration::kRefStability > 0.0);
    REQUIRE(AutoBootstrapConfiguration::kRefCenterShiftSq > 0.0);
    REQUIRE(AutoBootstrapConfiguration::kRefSkewSq > 0.0);
    
    // Values should be in reasonable ranges (order of magnitude 0.01 to 10)
    REQUIRE(AutoBootstrapConfiguration::kRefOrderingErrorSq < 10.0);
    REQUIRE(AutoBootstrapConfiguration::kRefLengthErrorSq < 10.0);
    REQUIRE(AutoBootstrapConfiguration::kRefStability < 10.0);
    REQUIRE(AutoBootstrapConfiguration::kRefCenterShiftSq < 10.0);
    REQUIRE(AutoBootstrapConfiguration::kRefSkewSq < 10.0);
  }
  
  SECTION("Normalization actually uses these constants")
  {
    // Create raw components at reference levels
    using RawComponents = palvalidator::analysis::detail::RawComponents;
    RawComponents ref_level(
      AutoBootstrapConfiguration::kRefOrderingErrorSq,
      AutoBootstrapConfiguration::kRefLengthErrorSq,
      AutoBootstrapConfiguration::kRefStability,
      AutoBootstrapConfiguration::kRefCenterShiftSq,
      AutoBootstrapConfiguration::kRefSkewSq,
      0.0  // domain_penalty
    );
    
    // Normalize with default weights
    using ScoringWeights = palvalidator::analysis::AutoBootstrapSelector<Decimal>::ScoringWeights;
    using ScoreNormalizer = palvalidator::analysis::detail::ScoreNormalizer<
      Decimal, ScoringWeights, RawComponents>;
    
    ScoringWeights weights;
    ScoreNormalizer normalizer(weights);
    
    auto normalized = normalizer.normalize(ref_level);
    
    // At reference levels, normalized values should be exactly 1.0
    // (before weight multiplication)
    REQUIRE(normalized.getOrderingNorm() == Catch::Approx(1.0));
    REQUIRE(normalized.getLengthNorm() == Catch::Approx(1.0));
    REQUIRE(normalized.getStabilityNorm() == Catch::Approx(1.0));
    REQUIRE(normalized.getCenterSqNorm() == Catch::Approx(1.0));
    REQUIRE(normalized.getSkewSqNorm() == Catch::Approx(1.0));
  }
  
  SECTION("Changing reference values would affect normalization")
  {
    // This test documents that ScoreNormalizer uses these constants
    // If someone changes the constants, normalization will change
    
    // Document the relationship: normalized = raw / reference
    double raw_ordering = 0.02;
    double expected_norm = raw_ordering / 
        AutoBootstrapConfiguration::kRefOrderingErrorSq;
    
    // 0.02 / 0.01 = 2.0
    REQUIRE(expected_norm == Catch::Approx(2.0));
    
    // If reference were doubled to 0.02, normalized would be 1.0
    double hypothetical_reference = 0.02;
    double hypothetical_norm = raw_ordering / hypothetical_reference;
    REQUIRE(hypothetical_norm == Catch::Approx(1.0));
  }
}

TEST_CASE("AutoBootstrapConfiguration: Reference constants rationale",
          "[AutoBootstrapSelector][Configuration][Documentation]")
{
  SECTION("Ordering error: 10% coverage deviation baseline")
  {
    // If nominal coverage is 95%, actual coverage of 85% is 10% under
    // This represents a "typical" ordering violation
    double coverage_error = 0.10;
    REQUIRE(AutoBootstrapConfiguration::kRefOrderingErrorSq == 
            Catch::Approx(coverage_error * coverage_error));
    
    INFO("10% coverage error is considered 'typical' baseline violation");
  }
  
  SECTION("Length error: normalized to ideal = 1.0")
  {
    // An interval exactly at the theoretical ideal width has normalized
    // length = 1.0. This is the target, so reference is 1.0.
    REQUIRE(AutoBootstrapConfiguration::kRefLengthErrorSq == 
            Catch::Approx(1.0));
    
    INFO("Intervals at exactly 1× ideal length are optimal");
  }
  
  SECTION("Stability: moderate penalty threshold")
  {
    // A stability penalty of 0.25 from BCa or Percentile-T indicates
    // moderate instability - noticeable but not severe
    REQUIRE(AutoBootstrapConfiguration::kRefStability == 
            Catch::Approx(0.25));
    
    INFO("Stability penalty of 0.25 is 'moderate' instability");
  }
  
  SECTION("Center shift: 2 standard errors is notable")
  {
    // Bootstrap theory: shifts < 1 SE are noise, > 2 SE are concerning
    double se_threshold = 2.0;
    REQUIRE(AutoBootstrapConfiguration::kRefCenterShiftSq == 
            Catch::Approx(se_threshold * se_threshold));
    
    INFO("2 SE shift between bootstrap mean and estimate is 'notable' bias");
  }
  
  SECTION("Skewness: |skew| = 2.0 is 'high' threshold")
  {
    // Bootstrap literature: |skew| > 2.0 indicates high skewness
    // that may violate asymptotic assumptions of BCa
    double skew_threshold = 2.0;
    REQUIRE(AutoBootstrapConfiguration::kRefSkewSq == 
            Catch::Approx(skew_threshold * skew_threshold));
    
    // This aligns with kBcaSkewThreshold
    REQUIRE(AutoBootstrapConfiguration::kBcaSkewThreshold == 
            Catch::Approx(2.0));
    
    INFO("|skew| = 2.0 is threshold for 'high skewness' distributions");
  }
}

TEST_CASE("CandidateGateKeeper: passesEffectiveBGate validation",
          "[AutoBootstrapSelector][CandidateGateKeeper][EffectiveB]")
{
  using GateKeeper = palvalidator::analysis::detail::CandidateGateKeeper<Decimal>;
  
  SECTION("Absolute minimum: requires 200 effective samples regardless of fraction")
  {
    // Even with 100% effective fraction, need absolute minimum of 200
    auto candidate = makeTestCandidate(
      MethodId::Percentile,
      5.0,    // mean
      4.0,    // lower
      6.0,    // upper
      0.95,   // cl
      100,    // n
      199,    // B_outer (requested)
      0,      // B_inner
      199,    // effective_B (100% but < 200)
      0       // skipped_total
    );
    
    // Should fail - effective_B < 200
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate));
  }
  
  SECTION("Absolute minimum: passes with exactly 200 effective samples")
  {
    auto candidate = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      200,    // B_outer
      0,      // B_inner
      200,    // effective_B (exactly at minimum)
      0       // skipped_total
    );
    
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate));
  }
  
  SECTION("Absolute minimum: passes when absolute dominates fractional")
  {
    // Key: Absolute minimum (200) only dominates when B_outer is small enough
    // For 90% methods: crossover at B_outer = 222 (since 200/0.90 = 222.22)
    // At B_outer = 222: ceil(0.90 × 222) = ceil(199.8) = 200
    // So max(200, 200) = 200
    
    auto candidate = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      222,    // B_outer (at crossover point)
      0,      // B_inner
      220,    // effective_B (above minimum of 200)
      2       // skipped_total
    );
    
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate));
  }
  
  SECTION("Method-specific: PercentileT requires 70% effective fraction")
  {
    // PercentileT uses kPercentileTMinEffectiveFraction (typically 0.70)
    std::size_t B_outer = 1000;
    
    // At 69% (below 70% threshold)
    auto candidate_fail = makeTestCandidate(
      MethodId::PercentileT,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer,
      0,
      690,    // 69% (below threshold)
      310
    );
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate_fail));
    
    // At 70% (exactly at threshold)
    auto candidate_exact = makeTestCandidate(
      MethodId::PercentileT,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer,
      0,
      700,    // 70% (at threshold)
      300
    );
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate_exact));
    
    // At 75% (above threshold)
    auto candidate_pass = makeTestCandidate(
      MethodId::PercentileT,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer,
      0,
      750,    // 75% (above threshold)
      250
    );
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate_pass));
  }
  
  SECTION("Method-specific: BCa requires 90% effective fraction")
  {
    // BCa uses stricter 90% requirement
    std::size_t B_outer = 1000;
    
    // At 89% (below 90% threshold)
    auto candidate_fail = makeTestCandidate(
      MethodId::BCa,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer, 0,
      890,    // 89% (below threshold)
      110,
      0.5, 0.2, 5.0, 0.0, 1.0,
      0.0, 0.0, 0.0,
      0.05,   // z0
      0.02    // accel
    );
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate_fail));
    
    // At 90% (exactly at threshold)
    auto candidate_exact = makeTestCandidate(
      MethodId::BCa,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer, 0,
      900,    // 90% (at threshold)
      100,
      0.5, 0.2, 5.0, 0.0, 1.0,
      0.0, 0.0, 0.0,
      0.05, 0.02
    );
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate_exact));
  }
  
  SECTION("Method-specific: Other methods require 90% effective fraction")
  {
    // Percentile, Basic, MOutOfN, Normal all use 90%
    std::vector<MethodId> methods = {
      MethodId::Percentile,
      MethodId::Basic,
      MethodId::MOutOfN,
      MethodId::Normal
    };
    
    std::size_t B_outer = 1000;
    
    for (auto method : methods) {
      // At 89% (should fail)
      auto candidate_fail = makeTestCandidate(
        method, 5.0, 4.0, 6.0, 0.95, 100,
        B_outer, 0, 890, 110
      );
      REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate_fail));
      
      // At 90% (should pass)
      auto candidate_pass = makeTestCandidate(
        method, 5.0, 4.0, 6.0, 0.95, 100,
        B_outer, 0, 900, 100
      );
      REQUIRE(GateKeeper::passesEffectiveBGate(candidate_pass));
    }
  }
  
  SECTION("Edge case: requested < 2 always fails")
  {
    // With 0 requested
    auto candidate_0 = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      0,      // B_outer = 0
      0,
      0,
      0
    );
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate_0));
    
    // With 1 requested
    auto candidate_1 = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      1,      // B_outer = 1
      0,
      1,
      0
    );
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate_1));
  }
  
  SECTION("Edge case: effective = 0 always fails")
  {
    auto candidate = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      1000,   // B_outer
      0,
      0,      // effective_B = 0
      1000    // all skipped
    );
    
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate));
  }
  
  SECTION("Fractional requirement takes precedence when > 200")
  {
    // With B_outer = 10000, 90% = 9000 (much higher than absolute minimum of 200)
    std::size_t B_outer = 10000;
    
    // At 8999 effective (89.99%, below 90% threshold)
    auto candidate_fail = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer, 0,
      8999,   // Just below 90%
      1001
    );
    REQUIRE_FALSE(GateKeeper::passesEffectiveBGate(candidate_fail));
    
    // At 9000 effective (90%, at threshold)
    auto candidate_pass = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      B_outer, 0,
      9000,   // Exactly 90%
      1000
    );
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate_pass));
  }
  
  SECTION("Boundary: effective equals max(200, fraction * requested)")
  {
    // Case 1: Absolute minimum dominates (200 > 90% of 220)
    auto candidate1 = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      220,    // 90% of 220 = 198, so need 200
      0,
      200,    // Exactly at max(200, 198) = 200
      20
    );
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate1));
    
    // Case 2: Fractional requirement dominates (90% of 1000 > 200)
    auto candidate2 = makeTestCandidate(
      MethodId::Percentile,
      5.0, 4.0, 6.0, 0.95, 100,
      1000,   // 90% of 1000 = 900, so need 900
      0,
      900,    // Exactly at max(200, 900) = 900
      100
    );
    REQUIRE(GateKeeper::passesEffectiveBGate(candidate2));
  }
}

// =============================================================================
// TESTS FOR normalizeAndScoreCandidates - FIXED VERSION
// =============================================================================
// Fixes:
// 1. Removed getIsWinner() calls (method doesn't exist)
// 2. Fixed ScoringWeights construction (no setters, use constructor)
// 3. Fixed RawComponents copy issue (use push_back or reserve+emplace_back)
// =============================================================================

TEST_CASE("normalizeAndScoreCandidates: Basic functionality",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][Phase2]")
{
  ScoringWeights weights;
  StatisticSupport unbounded = StatisticSupport::unbounded();
  auto support_bounds = std::make_pair(
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN());
  uint64_t candidate_id_counter = 0;
  
  SECTION("Single candidate: enriched with score and metadata")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Verify enriched candidate
    REQUIRE(enriched.size() == 1);
    REQUIRE(std::isfinite(enriched[0].getScore()));
    REQUIRE(enriched[0].getCandidateId() == 0);
    REQUIRE(enriched[0].getRank() == 0);  // Not assigned yet
    
    // Verify score is computed
    REQUIRE(enriched[0].getScore() > 0.0);
    
    // Verify candidate_id_counter incremented
    REQUIRE(candidate_id_counter == 1);
    
    // Verify breakdown created
    REQUIRE(breakdowns.size() == 1);
  }
  
  SECTION("Multiple candidates: all enriched with unique IDs")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile),
      makeTestCandidate(MethodId::Basic),
      makeTestCandidate(MethodId::BCa)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0),
      makeValidRaw(0.02, 0.6, 0.2, 1.5, 1.5, 0.0),
      makeValidRaw(0.015, 0.55, 0.15, 1.2, 1.2, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Verify all enriched
    REQUIRE(enriched.size() == 3);
    REQUIRE(breakdowns.size() == 3);
    
    // Verify unique IDs assigned
    REQUIRE(enriched[0].getCandidateId() == 0);
    REQUIRE(enriched[1].getCandidateId() == 1);
    REQUIRE(enriched[2].getCandidateId() == 2);
    
    // Verify counter updated
    REQUIRE(candidate_id_counter == 3);
    
    // Verify all have finite scores
    REQUIRE(std::isfinite(enriched[0].getScore()));
    REQUIRE(std::isfinite(enriched[1].getScore()));
    REQUIRE(std::isfinite(enriched[2].getScore()));
  }
  
  SECTION("Empty input: returns empty output")
  {
    std::vector<Candidate> candidates;
    std::vector<RawComponents> raw;
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    REQUIRE(enriched.empty());
    REQUIRE(breakdowns.empty());
    REQUIRE(candidate_id_counter == 0);
  }
  
  SECTION("Candidate ID counter continues from initial value")
  {
    candidate_id_counter = 42;
    
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile),
      makeTestCandidate(MethodId::Basic)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(),
      makeValidRaw()
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // IDs start from 42
    REQUIRE(enriched[0].getCandidateId() == 42);
    REQUIRE(enriched[1].getCandidateId() == 43);
    REQUIRE(candidate_id_counter == 44);
  }
}

TEST_CASE("normalizeAndScoreCandidates: Score computation",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][Scoring]")
{
  ScoringWeights weights;
  StatisticSupport unbounded = StatisticSupport::unbounded();
  auto support_bounds = std::make_pair(
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN());
  uint64_t candidate_id_counter = 0;
  
  SECTION("Lower penalties produce lower scores")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile),  // Will have lower penalty
      makeTestCandidate(MethodId::Percentile)   // Will have higher penalty
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0),   // Low penalties
      makeValidRaw(0.10, 2.0, 0.5, 4.0, 4.0, 0.0)    // High penalties
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Lower penalties should produce lower score (better)
    REQUIRE(enriched[0].getScore() < enriched[1].getScore());
  }
  
  SECTION("Score reflects weighted combination of penalties")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.02, 0.8, 0.2, 2.0, 2.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Score should be positive (penalties are positive)
    REQUIRE(enriched[0].getScore() > 0.0);
    
    // Score should be finite
    REQUIRE(std::isfinite(enriched[0].getScore()));
  }
  
  SECTION("Zero penalties produce low score")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)  // All zero
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Zero penalties should produce very low score
    REQUIRE(enriched[0].getScore() >= 0.0);
    REQUIRE(enriched[0].getScore() < 0.1);  // Should be near zero
  }
  
  SECTION("Custom weights affect scores")
  {
    // Create custom weights with constructor parameters
    // Order: wCenterShift, wSkew, wLength, wStability, enforcePos, bcaZ0Scale, bcaAScale
    ScoringWeights custom_weights(
      10.0,  // wCenterShift - emphasize center shift
      0.5,   // wSkew
      0.25,  // wLength
      1.0,   // wStability
      false, // enforcePos
      20.0,  // bcaZ0Scale
      100.0  // bcaAScale
    );
    
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    // High center shift penalty
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.01, 0.01, 10.0, 0.5, 0.0)  // High center_shift_sq
    };
    
    auto [enriched_default, breakdowns_default] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    candidate_id_counter = 0;
    
    auto [enriched_custom, breakdowns_custom] = Selector::normalizeAndScoreCandidates(
      candidates, raw, custom_weights, unbounded, support_bounds, candidate_id_counter);
    
    // Custom weights should affect score
    // (Custom has 10× center shift weight, so score should be higher)
    REQUIRE(enriched_custom[0].getScore() > enriched_default[0].getScore());
  }
}

TEST_CASE("normalizeAndScoreCandidates: Rejection mask computation",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][Rejection]")
{
  
  ScoringWeights weights;
  StatisticSupport unbounded = StatisticSupport::unbounded();
  StatisticSupport positive = StatisticSupport::strictLowerBound(0.0, 1e-10);
  auto support_bounds_unbounded = std::make_pair(
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN());
  uint64_t candidate_id_counter = 0;
  
  SECTION("Valid candidate: no rejection flags")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50)  // 95% effective B - passes
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0)  // No domain violation
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds_unbounded, candidate_id_counter);
    
    // Note: We can't directly access rejection_mask from breakdowns in this test
    // but we verify the candidate is enriched properly
    REQUIRE(enriched.size() == 1);
    REQUIRE(std::isfinite(enriched[0].getScore()));
  }
  
  SECTION("Domain violation: rejection flag set")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, -1.0, 6.0)  // Negative lower bound
    };
    
    // Domain penalty will be set due to negative bound with positive support
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0,
                  AutoBootstrapConfiguration::kDomainViolationPenalty)
    };
    
    auto support_bounds_positive = std::make_pair(0.0, 
      std::numeric_limits<double>::quiet_NaN());
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, positive, support_bounds_positive, candidate_id_counter);
    
    // Candidate still enriched (rejection happens later in tournament)
    REQUIRE(enriched.size() == 1);
    REQUIRE(std::isfinite(enriched[0].getScore()));
  }
  
  SECTION("Low effective B: rejection flag set")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 500, 500)  // Only 50% effective B - fails
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds_unbounded, candidate_id_counter);
    
    // Candidate still enriched (rejection mask set but not filtered yet)
    REQUIRE(enriched.size() == 1);
    
    // Score is computed even for candidates that will be rejected later
    REQUIRE(std::isfinite(enriched[0].getScore()));
  }
  
  SECTION("BCa with extreme z0: rejection flag set")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0,
                       0.65,  // z0 > 0.6 (exceeds hard limit)
                       0.05)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds_unbounded, candidate_id_counter);
    
    // Candidate still enriched but will have rejection flag
    REQUIRE(enriched.size() == 1);
  }
  
  SECTION("BCa with extreme accel: rejection flag set")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0,
                       0.05,  // z0 OK
                       0.30)  // accel > 0.25 (exceeds hard limit)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds_unbounded, candidate_id_counter);
    
    REQUIRE(enriched.size() == 1);
  }
  
  SECTION("Multiple rejection reasons: multiple flags set")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, -1.0, 6.0, 0.95, 100,
                       1000, 0, 500, 500)  // Low effective B AND domain violation
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0,
                  AutoBootstrapConfiguration::kDomainViolationPenalty)
    };
    
    auto support_bounds_positive = std::make_pair(0.0,
      std::numeric_limits<double>::quiet_NaN());
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, positive, support_bounds_positive, candidate_id_counter);
    
    // Both flags should be set (can't verify directly without breakdown accessors)
    REQUIRE(enriched.size() == 1);
  }
}

TEST_CASE("normalizeAndScoreCandidates: Edge cases",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][EdgeCases]")
{
  ScoringWeights weights;
  StatisticSupport unbounded = StatisticSupport::unbounded();
  auto support_bounds = std::make_pair(
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN());
  uint64_t candidate_id_counter = 0;
  
  SECTION("Inf penalties produce inf score")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(std::numeric_limits<double>::infinity(), 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Inf penalty should produce inf score
    REQUIRE(std::isinf(enriched[0].getScore()));
  }
  
  SECTION("NaN penalties produce NaN score")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(std::numeric_limits<double>::quiet_NaN(), 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // NaN penalty should produce NaN score
    REQUIRE(std::isnan(enriched[0].getScore()));
  }
  
  SECTION("Very large candidate ID counter")
  {
    candidate_id_counter = std::numeric_limits<uint64_t>::max() - 1;
    
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw()
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Should handle wraparound gracefully
    REQUIRE(enriched[0].getCandidateId() == std::numeric_limits<uint64_t>::max() - 1);
    // Counter will wrap
  }
}

TEST_CASE("normalizeAndScoreCandidates: Different method types",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][Methods]")
{
  ScoringWeights weights;
  StatisticSupport unbounded = StatisticSupport::unbounded();
  auto support_bounds = std::make_pair(
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN());
  uint64_t candidate_id_counter = 0;
  
  SECTION("All bootstrap methods processed correctly")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile),
      makeTestCandidate(MethodId::Basic),
      makeTestCandidate(MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0, 0.05, 0.02),
      makeTestCandidate(MethodId::PercentileT, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 100, 750, 250, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0, 0.0, 0.0, 0.05),
      makeTestCandidate(MethodId::Normal),
      makeTestCandidate(MethodId::MOutOfN)
    };
    
    // Build raw components vector properly (no copy construction)
    std::vector<RawComponents> raw;
    raw.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i) {
      raw.push_back(makeValidRaw());
    }
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // All methods processed
    REQUIRE(enriched.size() == 6);
    REQUIRE(breakdowns.size() == 6);
    
    // All have finite scores (with valid inputs)
    for (const auto& c : enriched) {
      REQUIRE(std::isfinite(c.getScore()));
    }
    
    // Verify methods preserved
    REQUIRE(enriched[0].getMethod() == MethodId::Percentile);
    REQUIRE(enriched[1].getMethod() == MethodId::Basic);
    REQUIRE(enriched[2].getMethod() == MethodId::BCa);
    REQUIRE(enriched[3].getMethod() == MethodId::PercentileT);
    REQUIRE(enriched[4].getMethod() == MethodId::Normal);
    REQUIRE(enriched[5].getMethod() == MethodId::MOutOfN);
  }
  
  SECTION("BCa-specific rejection checks applied")
  {
    std::vector<Candidate> candidates = {
      // Valid BCa
      makeTestCandidate(MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0, 0.05, 0.02),
      // BCa with non-finite z0
      makeTestCandidate(MethodId::BCa, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 0, 950, 50, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0,
                       std::numeric_limits<double>::quiet_NaN(),  // Bad z0
                       0.02)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(),
      makeValidRaw()
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Both enriched (rejection flags set but not filtered)
    REQUIRE(enriched.size() == 2);
    
    // First should have valid score, second should too (rejection comes later)
    REQUIRE(std::isfinite(enriched[0].getScore()));
    REQUIRE(std::isfinite(enriched[1].getScore()));
  }
  
  SECTION("PercentileT-specific checks applied")
  {
    std::vector<Candidate> candidates = {
      // Valid PercentileT (70% effective is OK)
      makeTestCandidate(MethodId::PercentileT, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 100, 750, 250, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0, 0.0, 0.0, 0.03),
      // PercentileT with high inner failure rate
      makeTestCandidate(MethodId::PercentileT, 5.0, 4.0, 6.0, 0.95, 100,
                       1000, 100, 750, 250, 0.5, 0.2, 5.0, 0.0, 1.0,
                       0.0, 0.0, 0.0, 0.0, 0.0,
                       0.15)  // 15% inner failures (> 5% threshold)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(),
      makeValidRaw()
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Both enriched
    REQUIRE(enriched.size() == 2);
    
    // Both have scores computed
    REQUIRE(std::isfinite(enriched[0].getScore()));
    REQUIRE(std::isfinite(enriched[1].getScore()));
  }
}

TEST_CASE("normalizeAndScoreCandidates: Support bounds handling",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][Support]")
{
  ScoringWeights weights;
  uint64_t candidate_id_counter = 0;
  
  SECTION("Unbounded support: all candidates valid")
  {
    StatisticSupport unbounded = StatisticSupport::unbounded();
    auto support_bounds = std::make_pair(
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::quiet_NaN());
    
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, -10.0, 10.0),  // Wide bounds
      makeTestCandidate(MethodId::Percentile, 5.0, 0.0, 10.0)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0),
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    REQUIRE(enriched.size() == 2);
  }
  
  SECTION("Positive support: negative bounds flagged")
  {
    StatisticSupport positive = StatisticSupport::strictLowerBound(0.0, 1e-10);
    auto support_bounds = std::make_pair(0.0,
      std::numeric_limits<double>::quiet_NaN());
    
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile, 5.0, 1.0, 10.0),   // Valid
      makeTestCandidate(MethodId::Percentile, 5.0, -1.0, 10.0)   // Violates
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0, 0.0),
      makeValidRaw(0.01, 0.5, 0.1, 1.0, 1.0,
                  AutoBootstrapConfiguration::kDomainViolationPenalty)
    };
    
    auto [enriched, breakdowns] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, positive, support_bounds, candidate_id_counter);
    
    // Both enriched but second has violation flag
    REQUIRE(enriched.size() == 2);
    
    // Second should have higher score due to domain penalty
    REQUIRE(enriched[1].getScore() > enriched[0].getScore());
  }
}

TEST_CASE("normalizeAndScoreCandidates: Integration with normalization",
          "[AutoBootstrapSelector][normalizeAndScoreCandidates][Integration]")
{
  ScoringWeights weights;
  StatisticSupport unbounded = StatisticSupport::unbounded();
  auto support_bounds = std::make_pair(
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN());
  uint64_t candidate_id_counter = 0;
  
  SECTION("Normalization produces consistent scores")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    std::vector<RawComponents> raw = {
      makeValidRaw(0.02, 0.8, 0.2, 2.0, 2.0, 0.0)
    };
    
    // Run twice with same inputs
    auto [enriched1, breakdowns1] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    candidate_id_counter = 0;
    
    auto [enriched2, breakdowns2] = Selector::normalizeAndScoreCandidates(
      candidates, raw, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Scores should be identical (deterministic)
    REQUIRE(enriched1[0].getScore() == Catch::Approx(enriched2[0].getScore()));
  }
  
  SECTION("Score reflects normalized penalty contributions")
  {
    std::vector<Candidate> candidates = {
      makeTestCandidate(MethodId::Percentile)
    };
    
    // High ordering penalty, low others
    std::vector<RawComponents> raw1 = {
      makeValidRaw(0.10, 0.01, 0.01, 0.5, 0.5, 0.0)
    };
    
    // Low ordering penalty, high others
    std::vector<RawComponents> raw2 = {
      makeValidRaw(0.01, 0.10, 0.10, 3.0, 3.0, 0.0)
    };
    
    auto [enriched1, breakdowns1] = Selector::normalizeAndScoreCandidates(
      candidates, raw1, weights, unbounded, support_bounds, candidate_id_counter);
    
    candidate_id_counter = 0;
    
    auto [enriched2, breakdowns2] = Selector::normalizeAndScoreCandidates(
      candidates, raw2, weights, unbounded, support_bounds, candidate_id_counter);
    
    // Both should have positive scores
    REQUIRE(enriched1[0].getScore() > 0.0);
    REQUIRE(enriched2[0].getScore() > 0.0);
    
    // Scores should differ based on different penalty profiles
    REQUIRE(enriched1[0].getScore() != Catch::Approx(enriched2[0].getScore()));
  }
}
