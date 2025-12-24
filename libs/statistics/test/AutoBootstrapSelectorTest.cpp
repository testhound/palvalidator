// AutoBootstrapSelectorTest.cpp
//
// Unit tests for AutoBootstrapSelector component classes:
//  - AutoCIResult
//  - Candidate
//  - ScoringWeights
//  - Pareto-based selection logic
//  - Efron-style ordering/length penalties under skewed bootstrap distributions
//  - BCa Stability Penalties (Soft & Hard limits)
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
    const double      shift           = 0.02;
    const double      norm_len        = 1.05;
    const double      orderingPenalty = 0.004;
    const double      lengthPenalty   = 0.0025;
    const double      stabilityPenalty= 0.0150; // New explicit argument
    const double      z0              = 0.01;
    const double      accel           = 0.005;

    // Create Candidate
    Candidate c(method, mean, lower, upper, cl, n,
                B_outer, B_inner, effective_B, skipped,
                se, skew, shift, norm_len,
                orderingPenalty, lengthPenalty, stabilityPenalty,
                z0, accel);

    SECTION("Getters return correct values")
    {
        REQUIRE(c.getMethod() == method);
        REQUIRE(c.getMean()   == mean);
        REQUIRE(c.getStabilityPenalty()== stabilityPenalty); // Verify new getter
        REQUIRE(c.getZ0()    == z0);
        REQUIRE(c.getAccel() == accel);
    }
}

TEST_CASE("Candidate: Immutability and withScore",
          "[AutoBootstrapSelector][Candidate]")
{
    Candidate original(MethodId::Normal, 1.0, 0.9, 1.1, 0.95, 50, 500, 0, 500, 0,
                       0.05, 0.0, 0.0, 1.0, 
                       0.01, 0.02, 0.123, 0.0, 0.0);

    SECTION("withScore returns a new instance with updated score and preserved stability")
    {
        double newScore = 12.34;
        Candidate scored = original.withScore(newScore);

        REQUIRE(scored.getScore() == newScore);
        REQUIRE(scored.getStabilityPenalty() == original.getStabilityPenalty());
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
    }

    SECTION("Acceleration a above 0.10 incurs quadratic penalty")
    {
        // a = 0.12. Excess = 0.02.
        // Penalty = Excess^2 * 100.0 = 0.0004 * 100 = 0.04.
        engine.z0    = 0.0;
        engine.accel = 0.12;

        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.04));
    }

    SECTION("Both violations accumulate")
    {
        engine.z0    = 0.35; // Pen = 0.20
        engine.accel = 0.12; // Pen = 0.04

        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.24));
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
                    0.1, 0.0, 0.0, 1.0, 
                    /*ordering*/ 0.002,  // Small penalty
                    /*length*/   0.0,
                    /*stability*/ 0.0, 
                    0.0, 0.0);

    // 2. Grey Zone BCa
    // Perfect ordering (0.0), but has z0=0.35 -> Stability Penalty = 0.20.
    // Normalized against ref 0.25 -> score contribution 0.8.
    // Total Score = 0.8.
    // Expectation: PercT (0.2) < BCa (0.8) -> PercT wins.
    Candidate bcaGrey(MethodId::BCa,
                      0.0, -1.0, 1.0, 0.95, 100, 1000, 0, 1000, 0,
                      0.1, 0.0, 0.0, 1.0,
                      /*ordering*/ 0.0, 
                      /*length*/   0.0, 
                      /*stability*/ 0.20, // Calculated penalty for z0=0.35
                      /*z0*/ 0.35, 
                      /*accel*/ 0.0);

    std::vector<Candidate> cands = {percT, bcaGrey};
    
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
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
                    0.1, 0.0, 0.0, 1.0, 
                    /*ordering*/ 0.002,   // LOW penalty
                    /*length*/   0.0,
                    /*stability*/ 0.0, 
                    0.0, 0.0);

    SECTION("BCa with z0 just above old limit (0.51) is allowed but penalized")
    {
        Candidate bcaPenalized(MethodId::BCa,
                          0.0, -1.0, 1.0, 0.95, 100, 1000, 0, 1000, 0,
                          0.1, 0.0, 0.0, 1.0,
                          0.0, 0.0, 1.352,  // Raw penalty for z0=0.51
                          /*z0*/ 0.51, 
                          /*accel*/ 0.0);

        std::vector<Candidate> cands = {percT, bcaPenalized};
        auto result = Selector::select(cands);

        REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == false);
        REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
    }

    SECTION("BCa with z0 above new hard limit (0.61) is rejected")
    {
        Candidate bcaUnstable(MethodId::BCa,
                          0.0, -1.0, 1.0, 0.95, 100, 1000, 0, 1000, 0,
                          0.1, 0.0, 0.0, 1.0,
                          0.0, 0.0, 0.0, 
                          /*z0*/ 0.61,
                          /*accel*/ 0.0);

        std::vector<Candidate> cands = {percT, bcaUnstable};
        auto result = Selector::select(cands);

        REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
        REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == true);
    }
}

TEST_CASE("AutoBootstrapSelector: enforcePositive penalizes non-positive lower bounds",
          "[AutoBootstrapSelector][Select][EnforcePositive]")
{
    Candidate pos(MethodId::PercentileT, 1.0, 0.1, 2.0, 0.95, 50, 500, 0, 500, 0,
                  0.1, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    Candidate neg(MethodId::PercentileT, 1.0, -0.5, 2.0, 0.95, 50, 500, 0, 500, 0,
                  0.1, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    std::vector<Candidate> cands = {pos, neg};

    // With enforcePositive=true
    ScoringWeights weights(/*wCenter*/ 0.0, /*wSkew*/ 0.0, /*wLength*/ 0.0, 
                           /*wStab*/ 0.0, /*enforcePos*/ true);

    auto result = Selector::select(cands, weights);

    // Should choose positive LB
    REQUIRE(result.getChosenCandidate().getLower() == Catch::Approx(0.1));
}
