// =============================================================================
// V2 ENHANCEMENTS UNIT TESTS (Catch2)
// =============================================================================
// Test the new V2 fields and methods in Candidate, ScoreBreakdown, and 
// SelectionDiagnostics classes, plus the CandidateReject bitmask enum.
//
// These tests should be run AFTER existing tests pass to verify:
// 1. New fields are stored and retrieved correctly
// 2. Helper methods (markAsChosen, withMetadata) work correctly
// 3. Backward compatibility is preserved (defaults work)
// 4. CandidateReject bitmask operations work correctly
//
// Test organization:
// - CandidateReject bitmask tests
// - Candidate V2 field tests
// - ScoreBreakdown V2 field tests
// - SelectionDiagnostics V2 field tests
// - Integration tests
// =============================================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <limits>
#include <cmath>
#include <string>

#include "AutoBootstrapSelector.h"
#include "CandidateReject.h"

// Type aliases for convenience
using AutoCI = palvalidator::analysis::AutoCIResult<double>;
using Candidate = AutoCI::Candidate;
using MethodId = AutoCI::MethodId;
using SelectionDiagnostics = AutoCI::SelectionDiagnostics;
using ScoreBreakdown = SelectionDiagnostics::ScoreBreakdown;
using palvalidator::diagnostics::CandidateReject;
using palvalidator::diagnostics::hasRejection;
using palvalidator::diagnostics::rejectionMaskToString;

// =============================================================================
// CandidateReject Bitmask Tests
// =============================================================================

TEST_CASE("CandidateReject: Basic bitmask operations",
          "[V2][CandidateReject][Bitmask]")
{
    SECTION("Default mask is None")
    {
        CandidateReject mask = CandidateReject::None;
        REQUIRE(static_cast<std::uint32_t>(mask) == 0u);
        REQUIRE_FALSE(hasRejection(mask, CandidateReject::ScoreNonFinite));
    }
    
    SECTION("Single rejection reason works")
    {
        CandidateReject mask = CandidateReject::BcaZ0HardFail;
        REQUIRE(hasRejection(mask, CandidateReject::BcaZ0HardFail));
        REQUIRE_FALSE(hasRejection(mask, CandidateReject::BcaAccelHardFail));
        REQUIRE_FALSE(hasRejection(mask, CandidateReject::ScoreNonFinite));
    }
    
    SECTION("Multiple rejection reasons combine with OR")
    {
        CandidateReject mask = CandidateReject::BcaZ0HardFail | 
                               CandidateReject::ViolatesSupport;
        
        REQUIRE(hasRejection(mask, CandidateReject::BcaZ0HardFail));
        REQUIRE(hasRejection(mask, CandidateReject::ViolatesSupport));
        REQUIRE_FALSE(hasRejection(mask, CandidateReject::ScoreNonFinite));
        REQUIRE_FALSE(hasRejection(mask, CandidateReject::BcaAccelHardFail));
    }
    
    SECTION("Compound OR assignment works")
    {
        CandidateReject mask = CandidateReject::None;
        mask |= CandidateReject::BcaZ0HardFail;
        
        REQUIRE(hasRejection(mask, CandidateReject::BcaZ0HardFail));
        REQUIRE_FALSE(hasRejection(mask, CandidateReject::ViolatesSupport));
    }
    
    SECTION("Bitwise AND detects presence correctly")
    {
        CandidateReject mask = CandidateReject::BcaZ0HardFail;
        
        CandidateReject test1 = mask & CandidateReject::BcaZ0HardFail;
        REQUIRE(test1 == CandidateReject::BcaZ0HardFail);
        
        CandidateReject test2 = mask & CandidateReject::ViolatesSupport;
        REQUIRE(test2 == CandidateReject::None);
    }
}

TEST_CASE("CandidateReject: String conversion",
          "[V2][CandidateReject][ToString]")
{
    SECTION("None converts to empty string")
    {
        CandidateReject mask = CandidateReject::None;
        std::string text = rejectionMaskToString(mask);
        REQUIRE(text.empty());
        REQUIRE(text == "");
    }
    
    SECTION("Single reason converts correctly")
    {
        CandidateReject mask = CandidateReject::BcaZ0HardFail;
        std::string text = rejectionMaskToString(mask);
        REQUIRE(text == "BCA_Z0_EXCEEDED");
    }
    
    SECTION("Multiple reasons use semicolon separator")
    {
        CandidateReject mask = CandidateReject::BcaZ0HardFail | 
                               CandidateReject::ViolatesSupport;
        std::string text = rejectionMaskToString(mask);
        
        // All three reasons should be present
        REQUIRE(text.find("BCA_Z0_EXCEEDED") != std::string::npos);
        REQUIRE(text.find("VIOLATES_SUPPORT") != std::string::npos);
        
        // Should contain semicolons
        REQUIRE(text.find(";") != std::string::npos);
        
        // Should not contain reasons not in the mask
        REQUIRE(text.find("SCORE_NON_FINITE") == std::string::npos);
    }
    
    SECTION("All rejection reasons have string representations")
    {
        // Test each reason individually to ensure no missing strings
        std::vector<CandidateReject> reasons = {
            CandidateReject::ScoreNonFinite,
            CandidateReject::ViolatesSupport,
            CandidateReject::BcaParamsNonFinite,
            CandidateReject::BcaZ0HardFail,
            CandidateReject::BcaAccelHardFail,
            CandidateReject::PercentileTInnerFails,
            CandidateReject::PercentileTLowEffB,
        };
        
        for (auto reason : reasons) {
            std::string text = rejectionMaskToString(reason);
            REQUIRE_FALSE(text.empty());
        }
    }
}

// =============================================================================
// Candidate V2 Field Tests
// =============================================================================

TEST_CASE("Candidate V2: Backward compatibility",
          "[V2][Candidate][BackwardCompat]")
{
    SECTION("Old-style constructor works without V2 parameters")
    {
        // Create candidate using old-style constructor (without V2 params)
        Candidate candidate(
            MethodId::Percentile,
            1.05,  // mean
            0.95,  // lower
            1.15,  // upper
            0.95,  // cl
            100,   // n
            1000,  // B_outer
            0,     // B_inner
            1000,  // effective_B
            0,     // skipped_total
            0.05,  // se_boot
            0.2,   // skew_boot
            1.04,  // median_boot
            0.1,   // center_shift_in_se
            1.0,   // normalized_length
            0.5,   // ordering_penalty
            0.3,   // length_penalty
            0.1,   // stability_penalty
            0.0,   // z0
            0.0,   // accel
            0.0,   // inner_failure_rate
            0.9    // score
            // V2 params omitted - should use defaults
        );
        
        // Verify existing functionality unchanged
        REQUIRE(candidate.getMethod() == MethodId::Percentile);
        REQUIRE(candidate.getMean() == Catch::Approx(1.05));
        REQUIRE(candidate.getLower() == Catch::Approx(0.95));
        REQUIRE(candidate.getUpper() == Catch::Approx(1.15));
        REQUIRE(candidate.getScore() == Catch::Approx(0.9));
        REQUIRE(candidate.getN() == 100);
        REQUIRE(candidate.getSeBoot() == Catch::Approx(0.05));
        
        // Verify V2 defaults are applied
        REQUIRE(candidate.getCandidateId() == 0u);
        REQUIRE(candidate.getRank() == 0u);
        REQUIRE_FALSE(candidate.isChosen());
    }
}

TEST_CASE("Candidate V2: New field storage and retrieval",
          "[V2][Candidate][Fields]")
{
    SECTION("V2 fields set and retrieved correctly")
    {
        Candidate candidate(
            MethodId::BCa,
            1.05, 0.95, 1.15, 0.95, 100,
            1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.5, 0.3, 0.1, 0.02, -0.01, 0.0,
            0.9,
            // V2 params
            42,    // candidate_id
            2,     // rank
            false  // is_chosen
        );
        
        REQUIRE(candidate.getCandidateId() == 42u);
        REQUIRE(candidate.getRank() == 2u);
        REQUIRE_FALSE(candidate.isChosen());
        
        // Verify existing fields still work
        REQUIRE(candidate.getMethod() == MethodId::BCa);
        REQUIRE(candidate.getScore() == Catch::Approx(0.9));
    }
    
    SECTION("isChosen flag works for winner")
    {
        Candidate winner(
            MethodId::Percentile,
            1.05, 0.95, 1.15, 0.95, 100,
            1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.5, 0.3, 0.1, 0.0, 0.0, 0.0,
            0.55,
            10, 1, true  // is_chosen = true, rank = 1
        );
        
        REQUIRE(winner.isChosen());
        REQUIRE(winner.getRank() == 1u);
    }
    
    SECTION("Multiple candidates can have different metadata")
    {
        Candidate c1(MethodId::Basic, 1.0, 0.9, 1.1, 0.95, 100,
                     1000, 0, 1000, 0, 0.05, 0.2, 1.0, 0.1, 1.0,
                     0.4, 0.2, 0.05, 0.0, 0.0, 0.0, 0.65,
                     1, 2, false);
        
        Candidate c2(MethodId::BCa, 1.0, 0.85, 1.15, 0.95, 100,
                     1000, 0, 1000, 0, 0.05, 0.2, 1.0, 0.1, 1.0,
                     0.5, 0.3, 0.1, 0.05, -0.02, 0.0, 0.9,
                     2, 3, false);
        
        REQUIRE(c1.getCandidateId() != c2.getCandidateId());
        REQUIRE(c1.getRank() != c2.getRank());
        REQUIRE_FALSE(c1.isChosen());
        REQUIRE_FALSE(c2.isChosen());
    }
}

TEST_CASE("Candidate V2: Helper methods",
          "[V2][Candidate][Helpers]")
{
    SECTION("withScore preserves V2 fields")
    {
        Candidate original(
            MethodId::BCa,
            1.05, 0.95, 1.15, 0.95, 100,
            1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.5, 0.3, 0.1, 0.02, -0.01, 0.0,
            0.9,
            42, 2, false
        );
        
        Candidate updated = original.withScore(0.75);
        
        // New score applied
        REQUIRE(updated.getScore() == Catch::Approx(0.75));
        
        // V2 fields preserved
        REQUIRE(updated.getCandidateId() == 42u);
        REQUIRE(updated.getRank() == 2u);
        REQUIRE_FALSE(updated.isChosen());
        
        // Other fields preserved
        REQUIRE(updated.getMean() == Catch::Approx(1.05));
        REQUIRE(updated.getLower() == Catch::Approx(0.95));
        REQUIRE(updated.getMethod() == MethodId::BCa);
    }
    
    SECTION("markAsChosen sets is_chosen flag and updates rank")
    {
        Candidate loser(
            MethodId::Basic,
            1.05, 0.95, 1.15, 0.95, 100,
            1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.5, 0.3, 0.1, 0.0, 0.0, 0.0,
            1.2,
            5, 3, false  // ID=5, rank=3, not chosen
        );
        
        Candidate winner = loser.markAsChosen();
        
        REQUIRE(winner.isChosen());               // Now chosen
        REQUIRE(winner.getScore() == Catch::Approx(1.2)); // Score preserved
        
        // Original unchanged
        REQUIRE_FALSE(loser.isChosen());
        REQUIRE(loser.getRank() == 3u);
    }
    
    SECTION("withMetadata updates all metadata fields at once")
    {
        Candidate original(
            MethodId::Percentile,
            1.05, 0.95, 1.15, 0.95, 100,
            1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.5, 0.3, 0.1, 0.0, 0.0, 0.0,
            0.9
            // V2 defaults: id=0, rank=0, chosen=false
        );
        
        Candidate updated = original.withMetadata(10, 2, true);
        
        REQUIRE(updated.getCandidateId() == 10u);
        REQUIRE(updated.getRank() == 2u);
        REQUIRE(updated.isChosen());
        
        // Other fields preserved
        REQUIRE(updated.getScore() == Catch::Approx(0.9));
        REQUIRE(updated.getMethod() == MethodId::Percentile);
    }
    
    SECTION("withMetadata can mark as not chosen")
    {
        Candidate original(
            MethodId::Basic,
            1.0, 0.9, 1.1, 0.95, 100,
            1000, 0, 1000, 0,
            0.05, 0.2, 1.0, 0.1, 1.0,
            0.4, 0.2, 0.05, 0.0, 0.0, 0.0,
            0.8,
            1, 1, true  // Initially chosen
        );
        
        Candidate updated = original.withMetadata(1, 2, false);
        
        REQUIRE_FALSE(updated.isChosen());
        REQUIRE(updated.getRank() == 2u);
    }
}

// =============================================================================
// ScoreBreakdown V2 Field Tests
// =============================================================================

TEST_CASE("ScoreBreakdown V2: Backward compatibility",
          "[V2][ScoreBreakdown][BackwardCompat]")
{
    SECTION("Old-style constructor works without V2 parameters")
    {
        // Create breakdown using old-style constructor (without V2 params)
        ScoreBreakdown breakdown(
            MethodId::BCa,
            0.5,  // ordering_raw
            0.3,  // length_raw
            0.1,  // stability_raw
            0.05, // center_sq_raw
            0.02, // skew_sq_raw
            0.0,  // domain_raw
            0.8,  // ordering_norm
            0.6,  // length_norm
            0.2,  // stability_norm
            0.1,  // center_sq_norm
            0.05, // skew_sq_norm
            0.4,  // ordering_contrib
            0.18, // length_contrib
            0.02, // stability_contrib
            0.005,// center_sq_contrib
            0.001,// skew_sq_contrib
            0.0,  // domain_contrib
            0.606 // total_score
            // V2 params omitted - should use defaults
        );
        
        // Verify existing functionality unchanged
        REQUIRE(breakdown.getMethod() == MethodId::BCa);
        REQUIRE(breakdown.getOrderingRaw() == Catch::Approx(0.5));
        REQUIRE(breakdown.getLengthRaw() == Catch::Approx(0.3));
        REQUIRE(breakdown.getTotalScore() == Catch::Approx(0.606));
        
        // Verify V2 defaults are applied
        REQUIRE(breakdown.getRejectionMask() == CandidateReject::None);
        REQUIRE(breakdown.getRejectionText() == "");
        REQUIRE(breakdown.passedGates());
        REQUIRE_FALSE(breakdown.violatesSupport());
        REQUIRE(std::isnan(breakdown.getSupportLowerBound()));
        REQUIRE(std::isnan(breakdown.getSupportUpperBound()));
    }
}

TEST_CASE("ScoreBreakdown V2: Rejection tracking",
          "[V2][ScoreBreakdown][Rejection]")
{
    SECTION("Stores and retrieves rejection mask and text")
    {
        CandidateReject mask = CandidateReject::BcaZ0HardFail;
        std::string text = "BCa_Z0_EXCEEDED";
        
        ScoreBreakdown breakdown(
            MethodId::BCa,
            0.5, 0.3, 0.1, 0.05, 0.02, 0.0,
            0.8, 0.6, 0.2, 0.1, 0.05,
            0.4, 0.18, 0.02, 0.005, 0.001, 0.0,
            0.606,
            // V2 params
            mask,
            text,
            false,  // failed gates
            false,  // doesn't violate support
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        REQUIRE(breakdown.getRejectionMask() == mask);
        REQUIRE(breakdown.getRejectionText() == text);
        REQUIRE_FALSE(breakdown.passedGates());
        REQUIRE_FALSE(breakdown.violatesSupport());
    }
    
    SECTION("passed_gates correlates with rejection mask")
    {
        // No rejections = passed gates
        ScoreBreakdown passed(
            MethodId::Percentile,
            0.3, 0.2, 0.05, 0.05, 0.02, 0.0,
            0.6, 0.4, 0.1, 0.1, 0.05,
            0.3, 0.12, 0.01, 0.005, 0.001, 0.0,
            0.436,
            CandidateReject::None,
            "",
            true,  // passed gates
            false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        REQUIRE(passed.passedGates());
        REQUIRE(passed.getRejectionMask() == CandidateReject::None);
        
        // Rejections = failed gates
        ScoreBreakdown failed(
            MethodId::BCa,
            0.5, 0.4, 2.5, 0.05, 0.02, 0.0,
            1.0, 0.8, 1.0, 0.1, 0.05,
            0.5, 0.32, 2.5, 0.005, 0.001, 0.0,
            3.326,
            CandidateReject::BcaZ0HardFail,
            "BCa_Z0_EXCEEDED",
            false,  // failed gates
            false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        REQUIRE_FALSE(failed.passedGates());
        REQUIRE(hasRejection(failed.getRejectionMask(), CandidateReject::BcaZ0HardFail));
    }
}

TEST_CASE("ScoreBreakdown V2: Support validation",
          "[V2][ScoreBreakdown][Support]")
{
    SECTION("Tracks support violations for positive-only statistics")
    {
        ScoreBreakdown breakdown(
            MethodId::Percentile,
            0.5, 0.3, 0.1, 0.05, 0.02, 50.0,  // domain_raw = 50.0 (violation penalty)
            0.8, 0.6, 0.2, 0.1, 0.05,
            0.4, 0.18, 0.02, 0.005, 0.001, 50.0,
            50.606,
            CandidateReject::ViolatesSupport,
            "VIOLATES_SUPPORT",
            false,  // failed gates
            true,   // DOES violate support
            1e-9,   // support_lower (must be positive)
            std::numeric_limits<double>::infinity()  // support_upper (unbounded)
        );
        
        REQUIRE(breakdown.violatesSupport());
        REQUIRE(breakdown.getSupportLowerBound() == Catch::Approx(1e-9));
        REQUIRE(std::isinf(breakdown.getSupportUpperBound()));
        REQUIRE(breakdown.getDomainRaw() == Catch::Approx(50.0));
        REQUIRE(breakdown.getDomainContribution() == Catch::Approx(50.0));
        REQUIRE_FALSE(breakdown.passedGates());
    }
    
    SECTION("No support violation for valid intervals")
    {
        ScoreBreakdown breakdown(
            MethodId::BCa,
            0.3, 0.2, 0.1, 0.05, 0.02, 0.0,  // domain_raw = 0.0 (no violation)
            0.6, 0.4, 0.2, 0.1, 0.05,
            0.3, 0.12, 0.02, 0.005, 0.001, 0.0,
            0.446,
            CandidateReject::None,
            "",
            true,   // passed gates
            false,  // does NOT violate support
            1e-9,
            std::numeric_limits<double>::infinity()
        );
        
        REQUIRE_FALSE(breakdown.violatesSupport());
        REQUIRE(breakdown.passedGates());
        REQUIRE(breakdown.getDomainRaw() == Catch::Approx(0.0));
    }
    
    SECTION("Support bounds can be NaN for unbounded statistics")
    {
        ScoreBreakdown breakdown(
            MethodId::Basic,
            0.4, 0.25, 0.08, 0.05, 0.02, 0.0,
            0.8, 0.5, 0.16, 0.1, 0.05,
            0.4, 0.15, 0.016, 0.005, 0.001, 0.0,
            0.572,
            CandidateReject::None,
            "",
            true,
            false,
            std::numeric_limits<double>::quiet_NaN(),  // No lower bound
            std::numeric_limits<double>::quiet_NaN()   // No upper bound
        );
        
        REQUIRE_FALSE(breakdown.violatesSupport());
        REQUIRE(std::isnan(breakdown.getSupportLowerBound()));
        REQUIRE(std::isnan(breakdown.getSupportUpperBound()));
    }
}

// =============================================================================
// SelectionDiagnostics V2 Field Tests
// =============================================================================

TEST_CASE("SelectionDiagnostics V2: Backward compatibility",
          "[V2][SelectionDiagnostics][BackwardCompat]")
{
    SECTION("Old-style constructor works without tie_epsilon")
    {
        // Create diagnostics using old-style constructor (without tie_epsilon)
        SelectionDiagnostics diagnostics(
            MethodId::Percentile,
            "Percentile",
            0.8,    // chosen_score
            0.1,    // chosen_stability_penalty
            0.3,    // chosen_length_penalty
            true,   // has_bca_candidate
            false,  // bca_chosen
            true,   // bca_rejected_for_instability
            false   // bca_rejected_for_length
            // Remaining params use defaults
        );
        
        // Verify existing functionality unchanged
        REQUIRE(diagnostics.getChosenMethod() == MethodId::Percentile);
        REQUIRE(diagnostics.getChosenMethodName() == "Percentile");
        REQUIRE(diagnostics.getChosenScore() == Catch::Approx(0.8));
        REQUIRE(diagnostics.hasBCaCandidate());
        REQUIRE_FALSE(diagnostics.isBCaChosen());
        REQUIRE(diagnostics.wasBCaRejectedForInstability());
        
        // Verify V2 default is applied
        REQUIRE(diagnostics.getTieEpsilon() == Catch::Approx(1e-10));
    }
}

TEST_CASE("SelectionDiagnostics V2: Tie epsilon tracking",
          "[V2][SelectionDiagnostics][TieEpsilon]")
{
    SECTION("Custom tie epsilon is stored correctly")
    {
        std::vector<ScoreBreakdown> breakdowns;
        
        SelectionDiagnostics diagnostics(
            MethodId::BCa,
            "BCa",
            0.65,
            0.05,
            0.2,
            true, true, false, false,
            false, false,
            5,  // num_candidates
            breakdowns,
            1e-8  // custom tie_epsilon
        );
        
        REQUIRE(diagnostics.getTieEpsilon() == Catch::Approx(1e-8));
    }
    
    SECTION("Different tie epsilons for different selections")
    {
        std::vector<ScoreBreakdown> empty_breakdowns;
        
        SelectionDiagnostics d1(
            MethodId::Percentile, "Percentile",
            0.5, 0.1, 0.2,
            false, false, false, false,
            false, false, 3, empty_breakdowns,
            1e-10
        );
        
        SelectionDiagnostics d2(
            MethodId::BCa, "BCa",
            0.6, 0.15, 0.25,
            true, true, false, false,
            false, false, 4, empty_breakdowns,
            1e-12
        );
        
        REQUIRE(d1.getTieEpsilon() == Catch::Approx(1e-10));
        REQUIRE(d2.getTieEpsilon() == Catch::Approx(1e-12));
        REQUIRE(d1.getTieEpsilon() != d2.getTieEpsilon());
    }
}

TEST_CASE("SelectionDiagnostics V2: ScoreBreakdown integration",
          "[V2][SelectionDiagnostics][ScoreBreakdown]")
{
    SECTION("ScoreBreakdowns accessible with rejection info")
    {
        std::vector<ScoreBreakdown> breakdowns;
        
        // Add a breakdown with rejection info
        breakdowns.emplace_back(
            MethodId::BCa,
            0.5, 0.3, 0.1, 0.05, 0.02, 0.0,
            0.8, 0.6, 0.2, 0.1, 0.05,
            0.4, 0.18, 0.02, 0.005, 0.001, 0.0,
            0.606,
            CandidateReject::BcaZ0HardFail,
            "BCa_Z0_EXCEEDED",
            false,
            false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        SelectionDiagnostics diagnostics(
            MethodId::Percentile,
            "Percentile",
            0.5,
            0.1,
            0.2,
            true, false, true, false,
            false, false,
            2,
            breakdowns
        );
        
        REQUIRE(diagnostics.hasScoreBreakdowns());
        REQUIRE(diagnostics.getScoreBreakdowns().size() == 1u);
        
        const auto& bd = diagnostics.getScoreBreakdowns()[0];
        REQUIRE(bd.getMethod() == MethodId::BCa);
        REQUIRE(bd.getRejectionMask() == CandidateReject::BcaZ0HardFail);
        REQUIRE_FALSE(bd.passedGates());
        REQUIRE(bd.getRejectionText() == "BCa_Z0_EXCEEDED");
    }
    
    SECTION("Multiple breakdowns with different rejection reasons")
    {
        std::vector<ScoreBreakdown> breakdowns;
        
        // Winner - no rejections
        breakdowns.emplace_back(
            MethodId::Percentile,
            0.3, 0.2, 0.05, 0.05, 0.02, 0.0,
            0.6, 0.4, 0.1, 0.1, 0.05,
            0.3, 0.12, 0.01, 0.005, 0.001, 0.0,
            0.436,
            CandidateReject::None, "", true, false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        // BCa rejected for instability
        breakdowns.emplace_back(
            MethodId::BCa,
            0.5, 0.3, 2.5, 0.05, 0.02, 0.0,
            1.0, 0.8, 1.0, 0.1, 0.05,
            0.5, 0.24, 2.5, 0.005, 0.001, 0.0,
            3.246,
            CandidateReject::BcaZ0HardFail,
            "BCa_Z0_EXCEEDED",
            false, false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        // Percentile-T rejected for failures
        breakdowns.emplace_back(
            MethodId::PercentileT,
            0.4, 0.25, 0.8, 0.05, 0.02, 0.0,
            0.8, 0.5, 0.8, 0.1, 0.05,
            0.4, 0.15, 0.8, 0.005, 0.001, 0.0,
            1.356,
            CandidateReject::PercentileTInnerFails,
            "PCTT_INNER_FAILURES",
            false, false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        SelectionDiagnostics diagnostics(
            MethodId::Percentile,
            "Percentile",
            0.436,
            0.05,
            0.2,
            true, false, true, false,
            false, false,
            3,
            breakdowns
        );
        
        REQUIRE(diagnostics.getScoreBreakdowns().size() == 3u);
        
        // Check winner
        REQUIRE(diagnostics.getScoreBreakdowns()[0].passedGates());
        
        // Check BCa rejection
        REQUIRE_FALSE(diagnostics.getScoreBreakdowns()[1].passedGates());
        REQUIRE(hasRejection(diagnostics.getScoreBreakdowns()[1].getRejectionMask(),
                            CandidateReject::BcaZ0HardFail));
        
        // Check Percentile-T rejection
        REQUIRE_FALSE(diagnostics.getScoreBreakdowns()[2].passedGates());
        REQUIRE(hasRejection(diagnostics.getScoreBreakdowns()[2].getRejectionMask(),
                            CandidateReject::PercentileTInnerFails));
    }
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_CASE("V2 Integration: Full tournament workflow",
          "[V2][Integration][Tournament]")
{
    SECTION("Complete workflow with multiple candidates")
    {
        // Create multiple candidates with V2 metadata
        std::vector<Candidate> candidates;
        
        // Winner - Percentile
        candidates.emplace_back(
            MethodId::Percentile,
            1.05, 0.95, 1.15, 0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.3, 0.2, 0.05, 0.0, 0.0, 0.0,
            0.55,  // best score
            0, 1, true  // id=0, rank=1, chosen
        );
        
        // Runner-up - Basic
        candidates.emplace_back(
            MethodId::Basic,
            1.05, 0.93, 1.17, 0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.4, 0.25, 0.08, 0.0, 0.0, 0.0,
            0.73,  // second best
            1, 2, false  // id=1, rank=2, not chosen
        );
        
        // Rejected - BCa (high stability penalty)
        candidates.emplace_back(
            MethodId::BCa,
            1.05, 0.85, 1.25, 0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.2, 1.04, 0.1, 1.0,
            0.5, 0.4, 2.5, 0.8, -0.3, 0.0,  // high stability penalty
            3.4,  // rejected
            2, 3, false  // id=2, rank=3, not chosen
        );
        
        // Create score breakdowns with rejection info
        std::vector<ScoreBreakdown> breakdowns;
        
        // Winner breakdown
        breakdowns.emplace_back(
            MethodId::Percentile,
            0.3, 0.2, 0.05, 0.05, 0.02, 0.0,
            0.6, 0.4, 0.1, 0.1, 0.05,
            0.3, 0.12, 0.01, 0.005, 0.001, 0.0,
            0.55,
            CandidateReject::None, "", true, false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        // Runner-up breakdown
        breakdowns.emplace_back(
            MethodId::Basic,
            0.4, 0.25, 0.08, 0.05, 0.02, 0.0,
            0.8, 0.5, 0.16, 0.1, 0.05,
            0.4, 0.15, 0.016, 0.005, 0.001, 0.0,
            0.73,
            CandidateReject::None, "", true, false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        // Rejected BCa breakdown
        breakdowns.emplace_back(
            MethodId::BCa,
            0.5, 0.4, 2.5, 0.05, 0.02, 0.0,
            1.0, 0.8, 1.0, 0.1, 0.05,
            0.5, 0.32, 2.5, 0.005, 0.001, 0.0,
            3.4,
            CandidateReject::BcaZ0HardFail,
            "BCa_Z0_EXCEEDED",
            false,  // failed gates
            false,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        );
        
        SelectionDiagnostics diagnostics(
            MethodId::Percentile,
            "Percentile",
            0.55,
            0.05,
            0.2,
            true, false, true, false,
            false, false,
            3,
            breakdowns,
            1e-10
        );
        
        AutoCI result(
            MethodId::Percentile,
            candidates[0],
            candidates,
            diagnostics
        );
        
        // Verify winner
        REQUIRE(result.getChosenCandidate().isChosen());
        REQUIRE(result.getChosenCandidate().getRank() == 1u);
        REQUIRE(result.getChosenCandidate().getCandidateId() == 0u);
        REQUIRE(result.getChosenMethod() == MethodId::Percentile);
        
        // Verify all candidates accessible
        REQUIRE(result.getCandidates().size() == 3u);
        
        // Verify runner-up
        const auto& runner_up = result.getCandidates()[1];
        REQUIRE_FALSE(runner_up.isChosen());
        REQUIRE(runner_up.getRank() == 2u);
        REQUIRE(runner_up.getCandidateId() == 1u);
        REQUIRE(runner_up.getScore() > result.getChosenCandidate().getScore());
        
        // Verify rejected candidate
        const auto& rejected = result.getCandidates()[2];
        REQUIRE_FALSE(rejected.isChosen());
        REQUIRE(rejected.getRank() == 3u);
        REQUIRE(rejected.getCandidateId() == 2u);
        REQUIRE(rejected.getMethod() == MethodId::BCa);
        
        // Verify rejected candidate diagnostics
        const auto& rejected_bd = result.getDiagnostics().getScoreBreakdowns()[2];
        REQUIRE_FALSE(rejected_bd.passedGates());
        REQUIRE(hasRejection(rejected_bd.getRejectionMask(), 
                            CandidateReject::BcaZ0HardFail));

        REQUIRE(rejected_bd.getRejectionText().find("BCa_Z0_EXCEEDED") != std::string::npos);
        
        // Verify tie epsilon
        REQUIRE(result.getDiagnostics().getTieEpsilon() == Catch::Approx(1e-10));
    }
}

TEST_CASE("V2 Integration: Candidate ranking consistency",
          "[V2][Integration][Ranking]")
{
    SECTION("Rank correlates with score")
    {
        std::vector<Candidate> candidates;
        
        // Create candidates with scores: 0.5 (best), 0.7, 1.2 (worst)
        candidates.emplace_back(
            MethodId::Percentile,
            1.0, 0.9, 1.1, 0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.2, 1.0, 0.1, 1.0,
            0.3, 0.2, 0.0, 0.0, 0.0, 0.0,
            0.5, 0, 1, true
        );
        
        candidates.emplace_back(
            MethodId::Basic,
            1.0, 0.88, 1.12, 0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.2, 1.0, 0.1, 1.0,
            0.4, 0.3, 0.0, 0.0, 0.0, 0.0,
            0.7, 1, 2, false
        );
        
        candidates.emplace_back(
            MethodId::MOutOfN,
            1.0, 0.8, 1.2, 0.95, 100, 1000, 0, 1000, 0,
            0.05, 0.2, 1.0, 0.1, 1.0,
            0.6, 0.6, 0.0, 0.0, 0.0, 0.0,
            1.2, 2, 3, false
        );
        
        // Verify rank order matches score order
        for (size_t i = 0; i < candidates.size(); ++i) {
            REQUIRE(candidates[i].getRank() == i + 1);
            if (i > 0) {
                REQUIRE(candidates[i].getScore() > candidates[i-1].getScore());
            }
        }
        
        // Only rank 1 should be chosen
        REQUIRE(candidates[0].isChosen());
        REQUIRE_FALSE(candidates[1].isChosen());
        REQUIRE_FALSE(candidates[2].isChosen());
    }
}
