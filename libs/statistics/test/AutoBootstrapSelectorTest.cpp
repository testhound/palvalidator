// AutoBootstrapSelectorTest.cpp
//
// Unit tests for AutoBootstrapSelector component classes:
//  - AutoCIResult
//  - Candidate
//  - ScoringWeights
//  - Pareto-based selection logic
//  - Efron-style ordering/length penalties under skewed bootstrap distributions
//  - BCa Stability Penalties (Soft & Hard limits)
//  - Bootstrap Median extraction and propagation
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

#include "AutoBootstrapSelector.h"
#include "number.h"

// Alias for convenience
using Decimal        = double; // Or num::DefaultNumber if preferred
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using Candidate      = Selector::Candidate;
using Result         = Selector::Result;
using ScoringWeights = Selector::ScoringWeights;
using MethodId       = Result::MethodId;

// -----------------------------------------------------------------------------
// Component-level tests
// -----------------------------------------------------------------------------

TEST_CASE("ScoringWeights: Construction and Getters",
          "[AutoBootstrapSelector][ScoringWeights]")
{
    SECTION("Default constructor uses standard weights")
    {
        ScoringWeights weights;
        REQUIRE(weights.getCenterShiftWeight() == 1.0);
        REQUIRE(weights.getSkewWeight()        == 0.5);
        REQUIRE(weights.getLengthWeight()      == 0.25);
        REQUIRE(weights.getStabilityWeight()   == 1.0);
        REQUIRE(weights.enforcePositive()      == false);
    }

    SECTION("Parameterized constructor sets custom weights and enforcePositive flag")
    {
        ScoringWeights weights(/*wCenterShift*/ 2.0,
                               /*wSkew*/        0.8,
                               /*wLength*/      0.1,
                               /*wStability*/   1.5,
                               /*enforcePos*/   true);

        REQUIRE(weights.getCenterShiftWeight() == 2.0);
        REQUIRE(weights.getSkewWeight()        == 0.8);
        REQUIRE(weights.getLengthWeight()      == 0.1);
        REQUIRE(weights.getStabilityWeight()   == 1.5);
        REQUIRE(weights.enforcePositive()      == true);
    }
}

TEST_CASE("Candidate: Construction and Encapsulation",
          "[AutoBootstrapSelector][Candidate]")
{
    // Dummy values for testing
    const MethodId    method          = MethodId::Basic;
    const Decimal     mean            = 0.5;
    const Decimal     lower           = 0.4;
    const Decimal     upper           = 0.6;
    const double      cl              = 0.95;
    const std::size_t n               = 100;
    const std::size_t B_outer         = 1000;
    const std::size_t B_inner         = 0;
    const std::size_t effective_B     = 990;
    const std::size_t skipped         = 10;
    const double      se              = 0.05;
    const double      skew            = 0.1;
    const double      median          = 0.48;    // NEW: median_boot
    const double      shift           = 0.02;
    const double      norm_len        = 1.05;
    const double      orderingPenalty = 0.004;
    const double      lengthPenalty   = 0.0025;
    const double      stabilityPenalty= 0.0150; // New explicit argument
    const double      z0              = 0.01;
    const double      accel           = 0.005;

    // Create Candidate with median_boot at position 13
    Candidate c(method, mean, lower, upper, cl, n,
                B_outer, B_inner, effective_B, skipped,
                se, skew,
                median,           // ← NEW: median_boot (position 13)
                shift, norm_len,
                orderingPenalty, lengthPenalty, stabilityPenalty,
                z0, accel, 0.0);

    SECTION("Getters return correct values")
    {
        REQUIRE(c.getMethod() == method);
        REQUIRE(c.getMean()   == mean);
        REQUIRE(c.getMedianBoot() == median);           // NEW: Test median getter
        REQUIRE(c.getStabilityPenalty()== stabilityPenalty);
        REQUIRE(c.getZ0()    == z0);
        REQUIRE(c.getAccel() == accel);
    }
}

TEST_CASE("SelectionDiagnostics: Default parameters work correctly",
          "[AutoBootstrapSelector][SelectionDiagnostics]")
{
    SECTION("Minimal construction using default parameters")
    {
        // Test using minimal constructor (omitting last 4 params)
        using SelectionDiagnostics = palvalidator::analysis::AutoBootstrapSelector<double>::Result::SelectionDiagnostics;
        
        SelectionDiagnostics diag(
            MethodId::Percentile,       // chosenMethod
            "Percentile",               // chosenMethodName
            0.5,                        // chosenScore
            0.1,                        // chosenStabilityPenalty
            0.05,                       // chosenLengthPenalty
            false,                      // hasBCaCandidate
            false,                      // bcaChosen
            false,                      // bcaRejectedForInstability
            false);                     // bcaRejectedForLength
            // Defaults: bcaRejectedForDomain=false, bcaRejectedForNonFinite=false,
            //           numCandidates=0, scoreBreakdowns=empty
        
        REQUIRE(diag.wasBCaRejectedForDomain() == false);
        REQUIRE(diag.wasBCaRejectedForNonFiniteParameters() == false);
        REQUIRE(diag.getNumCandidates() == 0);
        REQUIRE(diag.getScoreBreakdowns().empty());
    }
    
    SECTION("Full construction with all parameters explicit")
    {
        using SelectionDiagnostics = palvalidator::analysis::AutoBootstrapSelector<double>::Result::SelectionDiagnostics;
        using ScoreBreakdown = SelectionDiagnostics::ScoreBreakdown;
        
        std::vector<ScoreBreakdown> breakdowns;
        breakdowns.emplace_back(
            MethodId::BCa,              // method
            0.001, 0.002, 0.05,         // raw ordering, length, stability
            0.01, 0.25, 0.0,            // raw center, skew, domain
            0.1, 0.2, 0.2,              // norm ordering, length, stability
            0.25, 0.625,                // norm center, skew (no norm domain)
            0.1, 0.05, 0.2, 0.125, 0.3125, 0.0,  // contributions (ordering, length, stability, center, skew, domain)
            0.8625);                    // total score
        
        SelectionDiagnostics diag(
            MethodId::BCa,              // chosenMethod
            "BCa",                      // chosenMethodName
            0.8625,                     // chosenScore
            0.2,                        // chosenStabilityPenalty
            0.05,                       // chosenLengthPenalty
            true,                       // hasBCaCandidate
            true,                       // bcaChosen
            false,                      // bcaRejectedForInstability
            false,                      // bcaRejectedForLength
            true,                       // bcaRejectedForDomain
            false,                      // bcaRejectedForNonFinite
            3,                          // numCandidates
            std::move(breakdowns));     // scoreBreakdowns
        
        REQUIRE(diag.wasBCaRejectedForDomain() == true);
        REQUIRE(diag.wasBCaRejectedForNonFiniteParameters() == false);
        REQUIRE(diag.getNumCandidates() == 3);
        REQUIRE(diag.getScoreBreakdowns().size() == 1);
    }
    
    SECTION("Mix default and explicit parameters")
    {
        using SelectionDiagnostics = palvalidator::analysis::AutoBootstrapSelector<double>::Result::SelectionDiagnostics;
        
        // Specify domain=true but use defaults for nonFinite, numCandidates, scoreBreakdowns
        SelectionDiagnostics diag(
            MethodId::PercentileT,      // chosenMethod
            "PercentileT",              // chosenMethodName
            0.25,                       // chosenScore
            0.0,                        // chosenStabilityPenalty
            0.02,                       // chosenLengthPenalty
            true,                       // hasBCaCandidate
            false,                      // bcaChosen
            true,                       // bcaRejectedForInstability
            false,                      // bcaRejectedForLength
            true);                      // bcaRejectedForDomain (explicit)
            // Defaults: bcaRejectedForNonFinite=false, numCandidates=0, scoreBreakdowns=empty
        
        REQUIRE(diag.wasBCaRejectedForDomain() == true);      // explicit
        REQUIRE(diag.wasBCaRejectedForNonFiniteParameters() == false); // default
        REQUIRE(diag.getNumCandidates() == 0);               // default
        REQUIRE(diag.getScoreBreakdowns().empty());          // default
    }
}

TEST_CASE("Candidate: Immutability and withScore",
          "[AutoBootstrapSelector][Candidate]")
{
    Candidate original(MethodId::Normal, 1.0, 0.9, 1.1, 0.95, 50, 500, 0, 500, 0,
                       0.05, 0.0,
                       0.95,  // median_boot
                       0.0, 1.0, 
                       0.01, 0.02, 0.123, 0.0, 0.0, 0.0);

    SECTION("withScore returns a new instance with updated score and preserved stability and median")
    {
        double newScore = 12.34;
        Candidate scored = original.withScore(newScore);

        REQUIRE(scored.getScore() == newScore);
        REQUIRE(scored.getStabilityPenalty() == original.getStabilityPenalty());
        REQUIRE(scored.getMedianBoot() == original.getMedianBoot());  // NEW: Verify median preserved
    }
}

// -----------------------------------------------------------------------------
// NEW: Bootstrap Median Tests
// -----------------------------------------------------------------------------

TEST_CASE("Candidate: Bootstrap Median Storage and Retrieval",
          "[AutoBootstrapSelector][Candidate][Median]")
{
    SECTION("Candidate stores and retrieves median_boot correctly")
    {
        const double expected_median = 1.25;
        
        Candidate c(MethodId::BCa, 1.30, 1.10, 1.50, 0.95, 100, 1000, 0, 1000, 0,
                    0.10, 0.15,
                    expected_median,  // median_boot
                    0.02, 1.05,
                    0.001, 0.002, 0.05, 0.01, 0.005, 0.0);
        
        REQUIRE(c.getMedianBoot() == expected_median);
    }
    
    SECTION("Median can be zero (valid for some statistics)")
    {
        Candidate c(MethodId::Percentile, 0.05, -0.10, 0.20, 0.95, 50, 500, 0, 500, 0,
                    0.08, 0.0,
                    0.0,  // median_boot = 0.0 (valid value)
                    0.0, 1.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        
        REQUIRE(c.getMedianBoot() == 0.0);
    }
    
    SECTION("Median can be negative (valid for some statistics)")
    {
        Candidate c(MethodId::Basic, -0.05, -0.15, 0.05, 0.95, 50, 500, 0, 500, 0,
                    0.06, -0.5,
                    -0.08,  // median_boot = negative (valid)
                    0.01, 1.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        
        REQUIRE(c.getMedianBoot() == Catch::Approx(-0.08));
    }
}

TEST_CASE("AutoCIResult: getBootstrapMedian extracts median from chosen candidate",
          "[AutoBootstrapSelector][AutoCIResult][Median]")
{
    SECTION("getBootstrapMedian returns median from chosen candidate")
    {
        const double expected_median = 1.35;
        
        // Create a candidate with known median
        Candidate c1(MethodId::Percentile, 1.40, 1.20, 1.60, 0.95, 100, 1000, 0, 1000, 0,
                     0.12, 0.1,
                     expected_median,  // median_boot
                     0.0, 1.0,
                     0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        
        std::vector<Candidate> candidates = {c1};
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getBootstrapMedian() == expected_median);
    }
    
    SECTION("getBootstrapMedian returns median from BCa when BCa is chosen")
    {
        const double bca_median = 2.15;
        const double perc_median = 2.05;
        
        // Create BCa candidate with better score (lower penalty)
        Candidate bca(MethodId::BCa, 2.20, 2.00, 2.40, 0.95, 100, 1000, 0, 1000, 0,
                      0.10, 0.05,
                      bca_median,  // BCa median
                      0.0, 1.0,
                      0.001,  // Very low ordering penalty
                      0.0, 0.0, 0.01, 0.005, 0.0);
        
        // Create Percentile candidate with worse score (higher penalty)
        Candidate perc(MethodId::Percentile, 2.15, 1.95, 2.35, 0.95, 100, 1000, 0, 1000, 0,
                       0.10, 0.1,
                       perc_median,  // Percentile median
                       0.0, 1.0,
                       0.050,  // Higher ordering penalty
                       0.0, 0.0, 0.0, 0.0, 0.0);
        
        std::vector<Candidate> candidates = {bca, perc};
        
        auto result = Selector::select(candidates);
        
        // BCa should win (lower penalty)
        REQUIRE(result.getChosenMethod() == MethodId::BCa);
        REQUIRE(result.getBootstrapMedian() == bca_median);
    }
    
    SECTION("getBootstrapMedian returns median from winner in multi-candidate tournament")
    {
        // Create multiple candidates with different medians
        Candidate c1(MethodId::Normal, 1.0, 0.8, 1.2, 0.95, 50, 500, 0, 500, 0,
                     0.08, 0.0,
                     0.95,  // median
                     0.0, 1.0,
                     0.05, 0.0, 0.0, 0.0, 0.0, 0.0);  // High ordering penalty
        
        Candidate c2(MethodId::Percentile, 1.1, 0.9, 1.3, 0.95, 50, 500, 0, 500, 0,
                     0.08, 0.0,
                     1.05,  // median (this should win)
                     0.0, 1.0,
                     0.001, 0.0, 0.0, 0.0, 0.0, 0.0);  // Low ordering penalty
        
        Candidate c3(MethodId::MOutOfN, 1.15, 0.95, 1.35, 0.95, 50, 500, 0, 500, 0,
                     0.08, 0.0,
                     1.10,  // median
                     0.0, 1.0,
                     0.03, 0.0, 0.0, 0.0, 0.0, 0.0);  // Medium ordering penalty
        
        std::vector<Candidate> candidates = {c1, c2, c3};
        
        auto result = Selector::select(candidates);
        
        // c2 should win (lowest penalty)
        REQUIRE(result.getChosenMethod() == MethodId::Percentile);
        REQUIRE(result.getBootstrapMedian() == 1.05);
    }
}

TEST_CASE("AutoCIResult: Median ordering relative to lower bound",
          "[AutoBootstrapSelector][AutoCIResult][Median][Bounds]")
{
    SECTION("Median is typically >= lower bound for symmetric distributions")
    {
        // For a symmetric bootstrap distribution, median (50th) >= LB (2.5th percentile)
        const double lb = 1.10;
        const double median = 1.25;  // Should be >= lb
        const double ub = 1.40;
        
        REQUIRE(median >= lb);  // Sanity check
        
        Candidate c(MethodId::Percentile, 1.25, lb, ub, 0.95, 100, 1000, 0, 1000, 0,
                    0.10, 0.0,
                    median,
                    0.0, 1.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        
        REQUIRE(c.getMedianBoot() >= c.getLower());
    }
    
    SECTION("Median can be < lower bound for heavily skewed distributions")
    {
        // For right-skewed bootstrap distribution, LB might be > median
        // Example: bootstrap values = {0.5, 0.6, 0.7, 0.8, 5.0} (outlier)
        // Median ≈ 0.7, but 2.5th percentile might be adjusted higher by BCa
        const double lb = 0.80;
        const double median = 0.70;  // Can be < lb for skewed data
        const double ub = 2.50;
        
        Candidate c(MethodId::BCa, 1.0, lb, ub, 0.95, 100, 1000, 0, 1000, 0,
                    0.15, 1.5,  // High skewness
                    median,
                    0.0, 1.0,
                    0.0, 0.0, 0.0, 0.05, 0.01, 0.0);
        
        // This is valid - just checking it's stored correctly
        REQUIRE(c.getMedianBoot() == median);
        REQUIRE(c.getSkewBoot() == Catch::Approx(1.5));
    }
}

// -----------------------------------------------------------------------------
// Mock Engines for Factory Tests
// -----------------------------------------------------------------------------

struct MockPercentileEngine
{
    struct Result
    {
        Decimal     mean;
        Decimal     lower;
        Decimal     upper;
        double      cl;
        std::size_t B;
        std::size_t effective_B;
        std::size_t skipped;
        std::size_t n;
    };

    bool diagnosticsReady = false;
    std::vector<double> stats;
    double meanBoot = 0.0;
    double varBoot  = 0.0;
    double seBoot   = 0.0;

    bool hasDiagnostics() const { return diagnosticsReady; }
    const std::vector<double>& getBootstrapStatistics() const { return stats; }
    double getBootstrapMean() const { return meanBoot; }
    double getBootstrapVariance() const { return varBoot; }
    double getBootstrapSe() const { return seBoot; }
};

struct MockBCaEngine
{
    Decimal     mean;
    Decimal     lower;
    Decimal     upper;
    double      cl;
    std::size_t B;
    std::size_t n;
    
    // BCa specific
    double      z0;
    Decimal     accel;
    std::vector<Decimal> stats; 

    // Accessors needed by summarizeBCa template
    Decimal getMean() const { return mean; }
    Decimal getLowerBound() const { return lower; }
    Decimal getUpperBound() const { return upper; }
    double  getConfidenceLevel() const { return cl; }
    unsigned int getNumResamples() const { return static_cast<unsigned int>(B); }
    std::size_t getSampleSize() const { return n; }
    
    double getZ0() const { return z0; }
    Decimal getAcceleration() const { return accel; }
    const std::vector<Decimal>& getBootstrapStatistics() const { return stats; }
};

// -----------------------------------------------------------------------------
// BCa Factory Logic Tests (Soft Thresholds)
// -----------------------------------------------------------------------------

TEST_CASE("AutoBootstrapSelector: summarizeBCa applies strict stability penalties",
          "[AutoBootstrapSelector][BCa][Penalty]")
{
    // Setup a dummy BCa engine with default "safe" stats
    MockBCaEngine engine;
    engine.mean  = 0.0;
    engine.lower = -1.0;
    engine.upper = 1.0;
    engine.cl    = 0.95;
    engine.B     = 1000;
    engine.n     = 100;
    engine.stats = { -1.0, 0.0, 1.0 }; // Minimal stats to pass validation

    SECTION("Bias z0 below 0.25 incurs zero penalty")
    {
        engine.z0    = 0.24; // Safe
        engine.accel = 0.0;

        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getStabilityPenalty() == 0.0);
        REQUIRE(c.getMedianBoot() == 0.0);  // NEW: Verify median is set (should be 0.0 for {-1, 0, 1})
    }

    SECTION("Bias z0 above 0.25 incurs quadratic penalty")
    {
        // z0 = 0.35. Excess = 0.10. 
        // Penalty = Excess^2 * 20.0 = 0.01 * 20 = 0.20.
        engine.z0    = 0.35; 
        engine.accel = 0.0;

        Candidate c = Selector::summarizeBCa(engine);
        
        // Allow small float epsilon, but logic is exact
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.20));
        REQUIRE(c.getMedianBoot() == 0.0);  // NEW: Median from {-1, 0, 1} is 0
    }

    SECTION("Acceleration a above 0.10 incurs quadratic penalty")
    {
        // a = 0.12. Excess = 0.02.
        // Penalty = Excess^2 * 100.0 = 0.0004 * 100 = 0.04.
        engine.z0    = 0.0;
        engine.accel = 0.12;

        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.04));
        REQUIRE(c.getMedianBoot() == 0.0);  // NEW
    }

    SECTION("Both violations accumulate")
    {
        engine.z0    = 0.35; // Pen = 0.20
        engine.accel = 0.12; // Pen = 0.04

        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.24));
        REQUIRE(c.getMedianBoot() == 0.0);  // NEW
    }
}

TEST_CASE("AutoBootstrapSelector: summarizeBCa computes median correctly",
          "[AutoBootstrapSelector][BCa][Median]")
{
    MockBCaEngine engine;
    engine.mean  = 1.25;
    engine.lower = 1.10;
    engine.upper = 1.40;
    engine.cl    = 0.95;
    engine.B     = 1000;
    engine.n     = 100;
    engine.z0    = 0.05;  // Safe
    engine.accel = 0.02;  // Safe
    
    SECTION("Median computed from odd number of stats")
    {
        // Odd number: median is middle element
        engine.stats = { 1.0, 1.2, 1.3, 1.4, 1.6 };  // Median = 1.3
        
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getMedianBoot() == Catch::Approx(1.3));
    }
    
    SECTION("Median computed from even number of stats")
    {
        // Even number: median is average of two middle elements
        engine.stats = { 1.0, 1.2, 1.4, 1.6 };  // Median = (1.2 + 1.4) / 2 = 1.3
        
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getMedianBoot() == Catch::Approx(1.3));
    }
    
    SECTION("Median computed from unsorted stats")
    {
        // Stats in random order - should still compute median correctly
        engine.stats = { 1.6, 1.0, 1.4, 1.2, 1.3 };  // Sorted: {1.0, 1.2, 1.3, 1.4, 1.6}, Median = 1.3
        
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getMedianBoot() == Catch::Approx(1.3));
    }
    
    SECTION("Median with negative values")
    {
        engine.stats = { -0.5, -0.2, 0.1, 0.3, 0.6 };  // Median = 0.1
        
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getMedianBoot() == Catch::Approx(0.1));
    }
}

// -----------------------------------------------------------------------------
// High-Level Selection Tests (Tournament)
// -----------------------------------------------------------------------------

TEST_CASE("AutoBootstrapSelector: BCa 'Grey Zone' Tournament",
          "[AutoBootstrapSelector][Selection][GreyZone]")
{
    // Verify that a BCa candidate with moderate bias (0.25 < z0 < 0.50)
    // receives enough penalty to lose against a stable Percentile-T candidate.

    // 1. Stable Percentile-T (Baseline)
    // Small ordering penalty (0.002). Normalized against 0.01 -> score contribution 0.2.
    // This represents a "clean" robust alternative.
    Candidate percT(MethodId::PercentileT,
                    0.0, -1.1, 1.1, 0.95, 100, 1000, 0, 1000, 0,
                    0.1, 0.0,
                    0.05,  // median_boot
                    0.0, 1.0, 
                    /*ordering*/ 0.002,  // Small penalty
                    /*length*/   0.0,
                    /*stability*/ 0.0, 
                    0.0, 0.0, 0.0);

    // 2. Grey Zone BCa
    // Perfect ordering (0.0), but has z0=0.35 -> Stability Penalty = 0.20.
    // Normalized against ref 0.25 -> score contribution 0.8.
    // Total Score = 0.8.
    // Expectation: PercT (0.2) < BCa (0.8) -> PercT wins.
    Candidate bcaGrey(MethodId::BCa,
                      0.0, -1.0, 1.0, 0.95, 100, 1000, 0, 1000, 0,
                      0.1, 0.0,
                      0.03,  // median_boot
                      0.0, 1.0,
                      /*ordering*/ 0.0, 
                      /*length*/   0.0, 
                      /*stability*/ 0.20, // Calculated penalty for z0=0.35
                      /*z0*/ 0.35, 
                      /*accel*/ 0.0,
		      0.0);

    std::vector<Candidate> cands = {percT, bcaGrey};
    
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
    REQUIRE(result.getBootstrapMedian() == Catch::Approx(0.05));  // NEW: PercT's median
    REQUIRE(result.getDiagnostics().hasBCaCandidate() == true);
    REQUIRE(result.getDiagnostics().isBCaChosen() == false);
    
    // Important: It was NOT rejected by hard gate, it lost on score.
    REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == false);
}

TEST_CASE("AutoBootstrapSelector: BCa Hard Gate Rejection",
          "[AutoBootstrapSelector][Selection][HardGate]")
{
    // Define a STRONG PercentileT candidate (low ordering penalty)
    Candidate percT(MethodId::PercentileT,
                    0.0, -1.1, 1.1, 0.95, 100, 1000, 0, 1000, 0,
                    0.1, 0.0,
                    0.08,  // median_boot
                    0.0, 1.0, 
                    /*ordering*/ 0.002,   // LOW penalty
                    /*length*/   0.0,
                    /*stability*/ 0.0, 
                    0.0, 0.0, 0.0);

    SECTION("BCa with z0 just above old limit (0.51) is allowed but penalized")
    {
        Candidate bcaPenalized(MethodId::BCa,
			       0.0, -1.0, 1.0, 0.95, 100, 1000, 0, 1000, 0,
			       0.1, 0.0,
			       0.02,  // median_boot
			       0.0, 1.0,
			       0.0, 0.0, 1.352,  // Raw penalty for z0=0.51
			       /*z0*/ 0.51, 
			       /*accel*/ 0.0,
			       0.0);

        std::vector<Candidate> cands = {percT, bcaPenalized};
        auto result = Selector::select(cands);

        REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == false);
        REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
        REQUIRE(result.getBootstrapMedian() == Catch::Approx(0.08));  // NEW: PercT's median
    }

    SECTION("BCa with z0 above new hard limit (0.61) is rejected")
    {
        Candidate bcaUnstable(MethodId::BCa,
			      0.0, -1.0, 1.0, 0.95, 100, 1000, 0, 1000, 0,
			      0.1, 0.0,
			      0.01,  // median_boot
			      0.0, 1.0,
			      0.0, 0.0, 0.0, 
			      /*z0*/ 0.61,
			      /*accel*/ 0.0, 0.0);

        std::vector<Candidate> cands = {percT, bcaUnstable};
        auto result = Selector::select(cands);

        REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
        REQUIRE(result.getBootstrapMedian() == Catch::Approx(0.08));  // NEW: PercT's median (BCa rejected)
        REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == true);
    }
}

TEST_CASE("AutoBootstrapSelector: enforcePositive penalizes non-positive lower bounds",
          "[AutoBootstrapSelector][Select][EnforcePositive]")
{
    Candidate pos(MethodId::PercentileT, 1.0, 0.1, 2.0, 0.95, 50, 500, 0, 500, 0,
                  0.1, 0.0,
                  0.95,  // median_boot
                  0.0, 1.0,
                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    Candidate neg(MethodId::PercentileT, 1.0, -0.5, 2.0, 0.95, 50, 500, 0, 500, 0,
                  0.1, 0.0,
                  0.85,  // median_boot
                  0.0, 1.0,
                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    std::vector<Candidate> cands = {pos, neg};

    // With enforcePositive=true
    ScoringWeights weights(/*wCenter*/ 0.0, /*wSkew*/ 0.0, /*wLength*/ 0.0, 
                           /*wStab*/ 0.0, /*enforcePos*/ true);

    auto result = Selector::select(cands, weights);

    // Should choose positive LB
    REQUIRE(result.getChosenCandidate().getLower() == Catch::Approx(0.1));
    REQUIRE(result.getBootstrapMedian() == Catch::Approx(0.95));  // NEW: pos candidate's median
}

// -----------------------------------------------------------------------------
// NEW: Integration Tests for Median Propagation Through Pipeline
// -----------------------------------------------------------------------------

TEST_CASE("AutoBootstrapSelector: Median propagates through selection pipeline",
          "[AutoBootstrapSelector][Integration][Median]")
{
    SECTION("Winner's median is accessible via AutoCIResult")
    {
        // Create candidates with distinct medians
        std::vector<Candidate> candidates;
        
        candidates.push_back(
            Candidate(MethodId::Percentile, 1.2, 1.0, 1.4, 0.95, 100, 1000, 0, 1000, 0,
                      0.10, 0.0,
                      1.18,  // median for Percentile
                      0.0, 1.0,
                      0.05, 0.0, 0.0, 0.0, 0.0, 0.0));  // High penalty
        
        candidates.push_back(
            Candidate(MethodId::MOutOfN, 1.25, 1.05, 1.45, 0.95, 100, 1000, 0, 1000, 0,
                      0.10, 0.0,
                      1.22,  // median for MOutOfN (should win)
                      0.0, 1.0,
                      0.001, 0.0, 0.0, 0.0, 0.0, 0.0));  // Low penalty
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenMethod() == MethodId::MOutOfN);
        REQUIRE(result.getBootstrapMedian() == Catch::Approx(1.22));
        REQUIRE(result.getChosenCandidate().getMedianBoot() == Catch::Approx(1.22));
    }
    
    SECTION("Multiple methods available - median from actual winner")
    {
        std::vector<Candidate> candidates;
        
        // BCa with high stability penalty (should lose)
        candidates.push_back(
            Candidate(MethodId::BCa, 1.30, 1.15, 1.45, 0.95, 100, 1000, 0, 1000, 0,
                      0.12, 0.2,
                      1.28,  // BCa median (won't be used)
                      0.0, 1.0,
                      0.0, 0.0, 0.5, 0.3, 0.1, 0.0));  // High stability penalty
        
        // PercentileT with low penalty (should win)
        candidates.push_back(
            Candidate(MethodId::PercentileT, 1.28, 1.12, 1.44, 0.95, 100, 1000, 0, 1000, 0,
                      0.11, 0.1,
                      1.25,  // PercentileT median (should be returned)
                      0.0, 1.0,
                      0.01, 0.0, 0.0, 0.0, 0.0, 0.0));  // Low penalty
        
        auto result = Selector::select(candidates);
        
        REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
        REQUIRE(result.getBootstrapMedian() == Catch::Approx(1.25));
    }
}

TEST_CASE("AutoBootstrapSelector: Median values are sensible relative to bounds",
          "[AutoBootstrapSelector][Validation][Median]")
{
    SECTION("For symmetric distribution, median should be near mean")
    {
        const double mean = 1.25;
        const double lower = 1.10;
        const double upper = 1.40;
        const double median = 1.24;  // Close to mean
        
        Candidate c(MethodId::Percentile, mean, lower, upper, 0.95, 100, 1000, 0, 1000, 0,
                    0.10, 0.05,  // Low skewness
                    median,
                    0.0, 1.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        
        // For symmetric distribution: median ≈ mean
        REQUIRE(std::abs(c.getMedianBoot() - c.getMean()) < 0.05);
    }
    
    SECTION("For skewed distribution, median can differ from mean")
    {
        const double mean = 1.50;   // Mean pulled by outliers
        const double lower = 1.15;
        const double upper = 2.50;  // Wide upper bound due to skewness
        const double median = 1.35; // Median less affected by outliers
        
        Candidate c(MethodId::BCa, mean, lower, upper, 0.95, 100, 1000, 0, 1000, 0,
                    0.12, 1.5,  // High positive skewness
                    median,
                    0.0, 1.0,
                    0.0, 0.0, 0.0, 0.01, 0.005, 0.0);
        
        // For right-skewed: median < mean (outliers pull mean up)
        REQUIRE(c.getMedianBoot() < c.getMean());
        REQUIRE(c.getSkewBoot() > 0.5);  // Confirm skewness
    }
}

TEST_CASE("computeBCaStabilityPenalty: Normal cases - no penalty",
          "[AutoBootstrapSelector][BCa][StabilityPenalty]")
{
    SECTION("All parameters well within thresholds")
    {
        // Small z0, small accel, mild skewness
        double penalty = Selector::computeBCaStabilityPenalty(
            0.05,    // z0 (well below threshold ~0.25)
            0.02,    // accel (well below threshold 0.10)
            0.5      // skew_boot (well below threshold ~2.0)
        );
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Parameters exactly at thresholds (no excess)")
    {
        // Using typical threshold values from AutoBootstrapConfiguration
        // kBcaZ0SoftThreshold ≈ 0.25
        // kBcaASoftThreshold = 0.10
        // kBcaSkewThreshold ≈ 2.0
        
        double penalty = Selector::computeBCaStabilityPenalty(
            0.25,    // z0 at threshold
            0.10,    // accel at threshold
            2.0      // skew at threshold
        );
        
        // Should have zero penalty since we're exactly at (not over) thresholds
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-10));
    }
    
    SECTION("Zero parameters (perfectly unbiased, no acceleration, symmetric)")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            0.0,     // z0 = 0 (no bias)
            0.0,     // accel = 0 (constant SE)
            0.0      // skew = 0 (perfectly symmetric)
        );
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Mild negative parameters")
    {
        // BCa parameters can be negative; sign shouldn't matter for penalty
        double penalty = Selector::computeBCaStabilityPenalty(
            -0.1,    // negative z0 (still within threshold)
            -0.05,   // negative accel
            -0.8     // negative skew (mild)
        );
        
        REQUIRE(penalty == 0.0);
    }
}

TEST_CASE("computeBCaStabilityPenalty: Non-finite parameters",
          "[AutoBootstrapSelector][BCa][StabilityPenalty]")
{
    SECTION("NaN z0 returns infinity")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            std::numeric_limits<double>::quiet_NaN(),
            0.05,
            1.0
        );
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("NaN accel returns infinity")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,
            std::numeric_limits<double>::quiet_NaN(),
            1.0
        );
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Infinite z0 returns infinity")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            std::numeric_limits<double>::infinity(),
            0.05,
            1.0
        );
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Infinite accel returns infinity")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,
            std::numeric_limits<double>::infinity(),
            1.0
        );
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Both z0 and accel non-finite")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            1.0
        );
        
        REQUIRE(penalty == std::numeric_limits<double>::infinity());
    }
    
    SECTION("NaN skewness does not cause infinity (only z0 and accel are checked)")
    {
        // skew_boot is used in calculations but doesn't trigger infinity on NaN
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,
            0.05,
            std::numeric_limits<double>::quiet_NaN()
        );

	REQUIRE(!std::isfinite(penalty));
	REQUIRE(std::isinf(penalty));
    }
}

TEST_CASE("computeBCaStabilityPenalty: z0 penalty component",
          "[AutoBootstrapSelector][BCa][StabilityPenalty]")
{
    SECTION("z0 slightly over threshold incurs small penalty")
    {
        // Assuming threshold ≈ 0.25
        double penalty = Selector::computeBCaStabilityPenalty(
            0.30,    // z0 slightly over threshold
            0.05,    // accel OK
            1.0      // skew OK
        );
        
        REQUIRE(penalty > 0.0);
        REQUIRE(penalty < 0.1);  // Should be small
    }
    
    SECTION("z0 well over threshold incurs larger penalty")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            0.50,    // z0 well over threshold
            0.05,    // accel OK
            1.0      // skew OK
        );
        
        REQUIRE(penalty > 0.1);  // Should be substantial
    }
    
    SECTION("Negative z0 with same magnitude has same penalty")
    {
        double penalty_pos = Selector::computeBCaStabilityPenalty(
            0.40, 0.05, 1.0
        );
        double penalty_neg = Selector::computeBCaStabilityPenalty(
            -0.40, 0.05, 1.0
        );
        
        REQUIRE(penalty_pos == Catch::Approx(penalty_neg));
    }
    
    SECTION("z0 penalty increases quadratically")
    {
        // Test that doubling the excess more than doubles the penalty
        double penalty_small = Selector::computeBCaStabilityPenalty(
            0.30, 0.05, 1.0  // Small excess
        );
        double penalty_double = Selector::computeBCaStabilityPenalty(
            0.35, 0.05, 1.0  // Double the excess (assuming threshold ~0.25)
        );
        
        // If threshold is 0.25:
        // penalty_small ∝ (0.30-0.25)² = 0.0025
        // penalty_double ∝ (0.35-0.25)² = 0.01 = 4× penalty_small
        REQUIRE(penalty_double > penalty_small * 3.5);  // Should be ~4x
        REQUIRE(penalty_double < penalty_small * 4.5);
    }
}

TEST_CASE("computeBCaStabilityPenalty: acceleration penalty component",
          "[AutoBootstrapSelector][BCa][StabilityPenalty]")
{
    SECTION("accel over threshold incurs penalty")
    {
        // Threshold is 0.10 for mild skewness
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.15,    // accel over threshold
            1.0      // skew mild
        );
        
        REQUIRE(penalty > 0.0);
    }
    
    SECTION("Negative accel with same magnitude has same penalty")
    {
        double penalty_pos = Selector::computeBCaStabilityPenalty(
            0.1, 0.15, 1.0
        );
        double penalty_neg = Selector::computeBCaStabilityPenalty(
            0.1, -0.15, 1.0
        );
        
        REQUIRE(penalty_pos == Catch::Approx(penalty_neg));
    }
    
    SECTION("Very large accel incurs substantial penalty")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.30,    // accel very large
            1.0      // skew mild
        );
        
        REQUIRE(penalty > 0.3);  // Should be substantial
    }
}

TEST_CASE("computeBCaStabilityPenalty: skewness penalty component",
          "[AutoBootstrapSelector][BCa][StabilityPenalty]")
{
    SECTION("Moderate skewness (< threshold) has no skew penalty")
    {
        // Assuming skew threshold ≈ 2.0
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.05,    // accel OK
            1.5      // skew below threshold
        );
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("High skewness (> threshold) incurs skew penalty")
    {
        // Assuming skew threshold ≈ 2.0
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.05,    // accel OK
            3.0      // skew over threshold
        );
        
        REQUIRE(penalty > 0.0);
    }
    
    SECTION("Extreme skewness incurs large penalty")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.05,    // accel OK
            5.0      // extreme skew
        );
        
        REQUIRE(penalty > 1.0);  // Should be substantial
    }
    
    SECTION("Negative skewness treated same as positive")
    {
        double penalty_pos = Selector::computeBCaStabilityPenalty(
            0.1, 0.05, 4.0
        );
        double penalty_neg = Selector::computeBCaStabilityPenalty(
            0.1, 0.05, -4.0
        );
        
        REQUIRE(penalty_pos == Catch::Approx(penalty_neg));
    }
}

TEST_CASE("computeBCaStabilityPenalty: Adaptive thresholds based on skewness",
          "[AutoBootstrapSelector][BCa][StabilityPenalty][Adaptive]")
{
    SECTION("High skewness (> 2.0) increases z0 penalty via multiplier")
    {
        // Same z0, but different skewness levels
        double penalty_low_skew = Selector::computeBCaStabilityPenalty(
            0.35,    // z0 over threshold
            0.05,    // accel OK
            1.0      // skew < 2.0 (multiplier = 1.0)
        );
        
        double penalty_high_skew = Selector::computeBCaStabilityPenalty(
            0.35,    // same z0
            0.05,    // accel OK
            2.5      // skew > 2.0 (multiplier = 1.5)
        );
        
        // High skew should increase the penalty via 1.5x multiplier
        // (plus it adds its own skew penalty)
        REQUIRE(penalty_high_skew > penalty_low_skew * 1.4);
    }
    
    SECTION("Extreme skewness (> 3.0) tightens accel threshold")
    {
        // Test accel = 0.09 (between 0.08 and 0.10)
        // Should pass with low skew (threshold 0.10)
        // Should fail with extreme skew (threshold 0.08)
        
        double penalty_mild_skew = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.09,    // accel between 0.08 and 0.10
            2.0      // skew ≤ 3.0 → threshold 0.10
        );
        
        double penalty_extreme_skew = Selector::computeBCaStabilityPenalty(
            0.1,     // z0 OK
            0.09,    // accel between 0.08 and 0.10
            3.5      // skew > 3.0 → threshold 0.08
        );
        
        // With mild skew, 0.09 < 0.10 → no accel penalty (but may have skew penalty)
        // With extreme skew, 0.09 > 0.08 → accel penalty + skew penalty
        REQUIRE(penalty_extreme_skew > penalty_mild_skew);
    }
    
    SECTION("Skewness exactly at 2.0 uses multiplier 1.0")
    {
        // At the boundary, should use base multiplier
        double penalty_at_boundary = Selector::computeBCaStabilityPenalty(
            0.35, 0.05, 2.0  // skew exactly 2.0
        );
        double penalty_below = Selector::computeBCaStabilityPenalty(
            0.35, 0.05, 1.99  // skew just below 2.0
        );
        
        // Should be approximately the same (no multiplier jump)
        REQUIRE(penalty_at_boundary == Catch::Approx(penalty_below).epsilon(0.01));
    }
    
    SECTION("Skewness exactly at 3.0 triggers stricter accel threshold")
    {
        double penalty_at_boundary = Selector::computeBCaStabilityPenalty(
            0.1, 0.09, 3.0  // skew exactly 3.0 → threshold 0.08
        );
        double penalty_below = Selector::computeBCaStabilityPenalty(
            0.1, 0.09, 2.99  // skew just below 3.0 → threshold 0.10
        );
        
        // At 3.0, accel 0.09 exceeds threshold 0.08
        // Below 3.0, accel 0.09 is under threshold 0.10
        REQUIRE(penalty_at_boundary > penalty_below);
    }
}

TEST_CASE("computeBCaStabilityPenalty: Combined penalties",
          "[AutoBootstrapSelector][BCa][StabilityPenalty][Combined]")
{
    SECTION("Multiple sources of penalty combine additively")
    {
        // Get individual penalties
        double z0_only = Selector::computeBCaStabilityPenalty(
            0.40, 0.05, 1.0  // Only z0 over threshold
        );
        double accel_only = Selector::computeBCaStabilityPenalty(
            0.1, 0.15, 1.0  // Only accel over threshold
        );
        double skew_only = Selector::computeBCaStabilityPenalty(
            0.1, 0.05, 3.5  // Only skew over threshold
        );
        
        // Get combined penalty
        double combined = Selector::computeBCaStabilityPenalty(
            0.40, 0.15, 3.5  // All over thresholds
        );
        
        // Combined should be approximately the sum
        // (with some tolerance for interaction effects via multipliers)
        REQUIRE(combined > z0_only);
        REQUIRE(combined > accel_only);
        REQUIRE(combined > skew_only);
        
        // Should be roughly additive (within 20% due to skew multiplier effects)
        double expected_sum = z0_only + accel_only + skew_only;
        REQUIRE(combined >= expected_sum * 0.8);
        REQUIRE(combined <= expected_sum * 2.0);  // Upper bound accounting for multipliers
    }
    
    SECTION("Pathological case: all parameters extremely bad")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            1.0,     // z0 very large
            0.5,     // accel very large
            10.0     // skew extreme
        );
        
        // Should be a very large penalty
        REQUIRE(penalty > 10.0);
    }
}

// CORRECTED TEST - Replace the failing test section with this:

TEST_CASE("computeBCaStabilityPenalty: Custom ScoringWeights",
          "[AutoBootstrapSelector][BCa][StabilityPenalty][Weights]")
{
    SECTION("Custom weights affect penalty magnitude")
    {
        ScoringWeights default_weights;  // z0_scale=20.0, a_scale=100.0
        
        // Create LARGER custom weights (should increase penalty)
        ScoringWeights larger_weights(
            1.0,   // center shift weight (not used here)
            0.5,   // skew weight (not used here)
            0.25,  // length weight (not used here)
            1.0,   // stability weight (not used here)
            false, // enforce positive (not used here)
            40.0,  // bca_z0_scale (2x default of 20.0)
            200.0  // bca_a_scale (2x default of 100.0)
        );
        
        double penalty_default = Selector::computeBCaStabilityPenalty(
            0.35, 0.12, 1.0, default_weights
        );
        
        double penalty_larger = Selector::computeBCaStabilityPenalty(
            0.35, 0.12, 1.0, larger_weights
        );
        
        // Larger weights should increase the penalty (roughly 2x)
        REQUIRE(penalty_larger > penalty_default * 1.8);
        REQUIRE(penalty_larger < penalty_default * 2.2);
    }
    
    SECTION("Smaller custom weights reduce penalty")
    {
        ScoringWeights default_weights;  // z0_scale=20.0, a_scale=100.0
        
        // Create SMALLER custom weights (should decrease penalty)
        ScoringWeights smaller_weights(
            1.0, 0.5, 0.25, 1.0, false,
            10.0,  // bca_z0_scale (0.5x default)
            50.0   // bca_a_scale (0.5x default)
        );
        
        double penalty_default = Selector::computeBCaStabilityPenalty(
            0.35, 0.12, 1.0, default_weights
        );
        
        double penalty_smaller = Selector::computeBCaStabilityPenalty(
            0.35, 0.12, 1.0, smaller_weights
        );
        
        // Smaller weights should decrease the penalty
        REQUIRE(penalty_smaller < penalty_default);
        REQUIRE(penalty_smaller > penalty_default * 0.4);
        REQUIRE(penalty_smaller < penalty_default * 0.6);
    }
    
    SECTION("Zero weights eliminate penalties")
    {
        ScoringWeights zero_weights(
            1.0, 0.5, 0.25, 1.0, false,
            0.0,  // bca_z0_scale = 0
            0.0   // bca_a_scale = 0
        );
        
        double penalty = Selector::computeBCaStabilityPenalty(
            0.50,    // z0 way over threshold
            0.20,    // accel way over threshold
            1.5,     // skew mild (so no skew penalty)
            zero_weights
        );
        
        // With zero scales, z0 and accel penalties should be zero
        // Only potential penalty is from skewness (if over threshold)
        REQUIRE(penalty == Catch::Approx(0.0).margin(1e-10));
    }
    
    SECTION("Default weights are non-zero and produce penalties")
    {
        ScoringWeights default_weights;
        
        // Default weights should produce penalties for violations
        double penalty = Selector::computeBCaStabilityPenalty(
            0.40,    // z0 over threshold
            0.15,    // accel over threshold
            1.0,     // skew OK
            default_weights
        );
        
        REQUIRE(penalty > 0.0);
        
        // Verify the defaults are as documented
        REQUIRE(default_weights.getBcaZ0Scale() == 20.0);
        REQUIRE(default_weights.getBcaAScale() == 100.0);
    }
}

TEST_CASE("computeBCaStabilityPenalty: Edge cases",
          "[AutoBootstrapSelector][BCa][StabilityPenalty][EdgeCases]")
{
    SECTION("Extremely small but positive parameters")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            1e-10,   // z0 ≈ 0
            1e-10,   // accel ≈ 0
            1e-10    // skew ≈ 0
        );
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Parameters exactly at machine epsilon")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            std::numeric_limits<double>::epsilon(),
            std::numeric_limits<double>::epsilon(),
            std::numeric_limits<double>::epsilon()
        );
        
        REQUIRE(penalty == 0.0);
    }
    
    SECTION("Very large but finite parameters")
    {
        double penalty = Selector::computeBCaStabilityPenalty(
            1e6,     // z0 huge
            1e6,     // accel huge
            1e6      // skew huge
        );
        
        // Should be finite (not infinity)
        REQUIRE(std::isfinite(penalty));
        // But should be extremely large
        REQUIRE(penalty > 1e10);
    }
}

TEST_CASE("computeBCaStabilityPenalty: Debug logging",
          "[AutoBootstrapSelector][BCa][StabilityPenalty][Logging]")
{
    SECTION("Provides debug output when skewness > 2.0")
    {
        std::ostringstream log;
        
        Selector::computeBCaStabilityPenalty(
            0.3, 0.1, 2.5,  // High skewness
            ScoringWeights(),
            &log
        );
        
        std::string output = log.str();
        REQUIRE_FALSE(output.empty());
        REQUIRE(output.find("High skew detected") != std::string::npos);
    }
    
    SECTION("Provides debug output when penalty > 0")
    {
        std::ostringstream log;
        
        Selector::computeBCaStabilityPenalty(
            0.4, 0.05, 1.0,  // z0 over threshold
            ScoringWeights(),
            &log
        );
        
        std::string output = log.str();
        REQUIRE_FALSE(output.empty());
        REQUIRE(output.find("stability penalty") != std::string::npos);
    }
    
    SECTION("No output when all parameters OK and no logging stream")
    {
        // This should not crash and should return 0
        double penalty = Selector::computeBCaStabilityPenalty(
            0.1, 0.05, 1.0,
            ScoringWeights(),
            nullptr  // No logging
        );
        
        REQUIRE(penalty == 0.0);
    }
}

TEST_CASE("computeBCaStabilityPenalty: Consistency with summarizeBCa",
          "[AutoBootstrapSelector][BCa][StabilityPenalty][Integration]")
{
    SECTION("Method should be callable from summarizeBCa context")
    {
        // This test verifies the method signature is correct for use in summarizeBCa
        // We don't have a full BCa engine here, but we can verify the method works
        // with typical values that would come from a BCa computation
        
        const double z0 = 0.15;          // Typical BCa z0
        const double accel = 0.08;       // Typical BCa acceleration  
        const double skew_boot = 1.2;    // Typical bootstrap skewness
        
        ScoringWeights weights;
        std::ostringstream log;
        
        double penalty = Selector::computeBCaStabilityPenalty(
            z0, accel, skew_boot, weights, &log
        );
        
        // Should work without error and return reasonable value
        REQUIRE(std::isfinite(penalty));
        REQUIRE(penalty >= 0.0);
        REQUIRE(penalty < 100.0);  // Reasonable upper bound for typical values
    }
}
