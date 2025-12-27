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
