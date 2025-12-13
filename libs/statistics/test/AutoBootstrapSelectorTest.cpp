// AutoBootstrapSelectorTest.cpp
//
// Unit tests for AutoBootstrapSelector component classes:
//  - AutoCIResult
//  - Candidate
//  - ScoringWeights
//  - Pareto-based selection logic
//  - Efron-style ordering/length penalties under skewed bootstrap distributions
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
    }

    SECTION("Parameterized constructor sets custom weights")
    {
        ScoringWeights weights(2.0, 0.8, 0.1);
        REQUIRE(weights.getCenterShiftWeight() == 2.0);
        REQUIRE(weights.getSkewWeight()        == 0.8);
        REQUIRE(weights.getLengthWeight()      == 0.1);
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
    const double      z0              = 0.01;
    const double      accel           = 0.005;

    // Create Candidate (score left at default NaN)
    Candidate c(method, mean, lower, upper, cl, n,
                B_outer, B_inner, effective_B, skipped,
                se, skew, shift, norm_len,
                orderingPenalty, lengthPenalty,
                z0, accel);

    SECTION("Getters return correct values")
    {
        REQUIRE(c.getMethod() == method);
        REQUIRE(c.getMean()   == mean);
        REQUIRE(c.getLower()  == lower);
        REQUIRE(c.getUpper()  == upper);
        REQUIRE(c.getCl()     == cl);

        REQUIRE(c.getN()            == n);
        REQUIRE(c.getBOuter()       == B_outer);
        REQUIRE(c.getBInner()       == B_inner);
        REQUIRE(c.getEffectiveB()   == effective_B);
        REQUIRE(c.getSkippedTotal() == skipped);

        REQUIRE(c.getSeBoot()          == se);
        REQUIRE(c.getSkewBoot()        == skew);
        REQUIRE(c.getCenterShiftInSe() == shift);
        REQUIRE(c.getNormalizedLength()== norm_len);

        REQUIRE(c.getOrderingPenalty() == orderingPenalty);
        REQUIRE(c.getLengthPenalty()   == lengthPenalty);

        REQUIRE(c.getZ0()    == z0);
        REQUIRE(c.getAccel() == accel);
    }

    SECTION("Default score is NaN")
    {
        REQUIRE(std::isnan(c.getScore()));
    }
}

TEST_CASE("Candidate: Immutability and withScore",
          "[AutoBootstrapSelector][Candidate]")
{
    // Minimal candidate with simple values
    Candidate original(MethodId::Normal,
                       1.0,    // mean
                       0.9,    // lower
                       1.1,    // upper
                       0.95,   // cl
                       50,     // n
                       500,    // B_outer
                       0,      // B_inner
                       500,    // effective_B
                       0,      // skipped_total
                       0.05,   // se_boot
                       0.0,    // skew_boot
                       0.0,    // center_shift_in_se
                       1.0,    // normalized_length
                       0.01,   // ordering_penalty
                       0.02,   // length_penalty
                       0.0,    // z0
                       0.0);   // accel (score defaults to NaN)

    REQUIRE(std::isnan(original.getScore()));

    SECTION("withScore returns a new instance with updated score")
    {
        double newScore = 12.34;
        Candidate scored = original.withScore(newScore);

        // New instance has the score
        REQUIRE(scored.getScore() == newScore);

        // Other fields remained the same
        REQUIRE(scored.getMethod() == original.getMethod());
        REQUIRE(scored.getMean()   == original.getMean());
        REQUIRE(scored.getLower()  == original.getLower());
        REQUIRE(scored.getUpper()  == original.getUpper());
        REQUIRE(scored.getCl()     == original.getCl());
        REQUIRE(scored.getOrderingPenalty() == original.getOrderingPenalty());
        REQUIRE(scored.getLengthPenalty()   == original.getLengthPenalty());
    }

    SECTION("Original instance remains unchanged")
    {
        Candidate scored = original.withScore(99.9);

        // Original should still have NaN score
        REQUIRE(std::isnan(original.getScore()));
        REQUIRE(scored.getScore() == 99.9);
    }
}

TEST_CASE("AutoCIResult: Construction and Accessors",
          "[AutoBootstrapSelector][AutoCIResult]")
{
    Candidate c1(MethodId::Normal,
                 1.0, 0.9, 1.1, 0.95,
                 100, 1000, 0, 1000, 0,
                 0.05, 0.1,
                 0.0,   // center shift
                 1.0,   // normalized length
                 0.01,  // ordering penalty
                 0.02,  // length penalty
                 0.0,   // z0
                 0.0);  // accel

    Candidate c2(MethodId::Basic,
                 1.0, 0.8, 1.2, 0.95,
                 100, 1000, 0, 1000, 0,
                 0.06, 0.2,
                 0.1,   // center shift
                 1.1,   // normalized length
                 0.02,  // ordering penalty
                 0.03,  // length penalty
                 0.0,
                 0.0);

    // Give them scores (e.g., ordering + length)
    c1 = c1.withScore(1.5);
    c2 = c2.withScore(0.5); // Better score in this toy example

    std::vector<Candidate> candidates = {c1, c2};

    // Simulate selection (assume c2 was chosen)
    Result result(MethodId::Basic, c2, candidates);

    SECTION("Accessors return correct data")
    {
        REQUIRE(result.getChosenMethod() == MethodId::Basic);

        const Candidate& chosen = result.getChosenCandidate();
        REQUIRE(chosen.getMethod() == MethodId::Basic);
        REQUIRE(chosen.getScore()  == 0.5);

        const auto& list = result.getCandidates();
        REQUIRE(list.size() == 2);
        REQUIRE(list[0].getMethod() == MethodId::Normal);
        REQUIRE(list[1].getMethod() == MethodId::Basic);
    }
}

TEST_CASE("AutoBootstrapSelector: dominance logic",
          "[AutoBootstrapSelector][Dominance]")
{
    // Two candidates differing only in ordering/length penalties
    Candidate a(MethodId::Normal,
                0.0, -1.0, 1.0, 0.95,
                50, 500, 0, 500, 0,
                0.1, 0.0,
                0.0, 1.0,
                0.01,  // ordering penalty (better)
                0.02,  // length penalty (better)
                0.0, 0.0);

    Candidate b(MethodId::Basic,
                0.0, -1.2, 1.2, 0.95,
                50, 500, 0, 500, 0,
                0.1, 0.0,
                0.0, 1.2,
                0.04,  // ordering penalty (worse)
                0.05,  // length penalty (worse)
                0.0, 0.0);

    SECTION("A dominates B when strictly better in at least one dimension and no worse in the other")
    {
        REQUIRE(Selector::dominates(a, b));
        REQUIRE_FALSE(Selector::dominates(b, a));
    }

    SECTION("No dominance when one is better in ordering but worse in length")
    {
        Candidate c(MethodId::Percentile,
                    0.0, -1.1, 1.1, 0.95,
                    50, 500, 0, 500, 0,
                    0.1, 0.0,
                    0.0, 1.0,
                    0.005, // better ordering
                    0.08,  // worse length
                    0.0, 0.0);

        REQUIRE_FALSE(Selector::dominates(a, c));
        REQUIRE_FALSE(Selector::dominates(c, a));
    }
}

TEST_CASE("AutoBootstrapSelector: Pareto selection",
          "[AutoBootstrapSelector][Select]")
{
    // Construct a few synthetic candidates
    Candidate normal(MethodId::Normal,
                     1.0, 0.9, 1.1, 0.95,
                     30, 400, 0, 400, 0,
                     0.10, 0.0,
                     0.0, 1.0,
                     0.030, // ordering penalty
                     0.020, // length penalty
                     0.0, 0.0);

    Candidate bca(MethodId::BCa,
                  1.0, 0.9, 1.1, 0.95,
                  30, 400, 0, 400, 0,
                  0.10, 0.1,
                  0.0, 1.0,
                  0.010, // better ordering
                  0.010, // better length
                  0.05,  0.01); // z0, accel just for flavor

    Candidate percentile(MethodId::Percentile,
                         1.0, 0.85, 1.15, 0.95,
                         30, 400, 0, 400, 0,
                         0.10, 0.2,
                         0.0, 1.3,
                         0.050, // worst ordering
                         0.090, // worst length
                         0.0, 0.0);

    std::vector<Candidate> cands = {normal, bca, percentile};

    SECTION("Selector chooses BCa as non-dominated with best ordering/length")
    {
        auto result = Selector::select(cands);

        REQUIRE(result.getChosenMethod() == MethodId::BCa);

        const Candidate& chosen = result.getChosenCandidate();
        REQUIRE(chosen.getOrderingPenalty() <= normal.getOrderingPenalty());
        REQUIRE(chosen.getOrderingPenalty() <= percentile.getOrderingPenalty());
        REQUIRE(chosen.getLengthPenalty()   <= normal.getLengthPenalty());
        REQUIRE(chosen.getLengthPenalty()   <= percentile.getLengthPenalty());

        // Scores should be finite (ordering + length) in the returned candidates
        for (const auto& c : result.getCandidates())
        {
            REQUIRE(std::isfinite(c.getScore()));
        }
    }

    SECTION("Tie-breaking among frontier candidates uses ordering, then length, then method preference")
    {
        // Create two methods with identical penalties to test the methodPreference fallback
        Candidate bca_tie(MethodId::BCa,
                          1.0, 0.9, 1.1, 0.95,
                          30, 400, 0, 400, 0,
                          0.10, 0.1,
                          0.0, 1.0,
                          0.020, // same ordering as t
                          0.020, // same length as t
                          0.05,  0.01);

        Candidate t_method(MethodId::PercentileT,
                           1.0, 0.9, 1.1, 0.95,
                           30, 400, 0, 400, 0,
                           0.10, 0.1,
                           0.0, 1.0,
                           0.020, // same ordering
                           0.020, // same length
                           0.0,   0.0);

        std::vector<Candidate> ties = {bca_tie, t_method};

        auto result = Selector::select(ties);

        // BCa has higher preference rank than PercentileT, so it should be chosen
        REQUIRE(result.getChosenMethod() == MethodId::BCa);
    }
}

// -----------------------------------------------------------------------------
// Skewed bootstrap distribution mock & tests
// -----------------------------------------------------------------------------

// A minimal mock engine that satisfies the interface required by
// AutoBootstrapSelector::summarizePercentileLike.
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

TEST_CASE("AutoBootstrapSelector: ordering penalty is smaller for quantile-aligned CI under skewed bootstrap",
          "[AutoBootstrapSelector][OrderingPenalty][Skewed]")
{
    // Right-skewed small-sample bootstrap distribution (n=10)
    // Many small/near-zero returns, a couple of outliers on the right.
    std::vector<double> theta_star = {
        -0.5, -0.4, -0.3, -0.2, -0.1,
         0.0,  0.1,  0.2,  1.5,  2.0
    };

    const std::size_t m = theta_star.size();
    double sum = std::accumulate(theta_star.begin(), theta_star.end(), 0.0);
    double mean_boot = sum / static_cast<double>(m);

    double var_boot = 0.0;
    for (double v : theta_star)
    {
        const double d = v - mean_boot;
        var_boot += d * d;
    }
    var_boot /= static_cast<double>(m - 1);
    double se_boot = std::sqrt(var_boot);

    MockPercentileEngine engine;
    engine.diagnosticsReady = true;
    engine.stats    = theta_star;
    engine.meanBoot = mean_boot;
    engine.varBoot  = var_boot;
    engine.seBoot   = se_boot;

    // Choose CL = 0.60 => alpha = 0.40 => alphaL=0.20, alphaU=0.80
    // For m=10, the ideal endpoints are statistic #2 (20%) and #8 (80%)
    // Using the sorted theta_star:
    //   sorted = [-0.5,-0.4,-0.3,-0.2,-0.1,0,0.1,0.2,1.5,2.0]
    // - "Well-aligned" CI uses lower=-0.4 (2nd), upper=0.2 (8th)
    // - "Misaligned" CI uses lower=-0.5 (1st, F=0.1), upper=1.5 (9th, F=0.9)

    const double CL = 0.60;
    const std::size_t B = 100;   // just a bookkeeping value
    const std::size_t n = 20;    // pretend strategy sample size

    MockPercentileEngine::Result goodRes{
        /*mean*/ 0.0,     // not used in ordering penalty directly
        /*lower*/ -0.4,
        /*upper*/ 0.2,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    MockPercentileEngine::Result badRes{
        /*mean*/ 0.0,
        /*lower*/ -0.5,
        /*upper*/ 1.5,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    Candidate good = Selector::summarizePercentileLike(MethodId::Percentile, engine, goodRes);
    Candidate bad  = Selector::summarizePercentileLike(MethodId::Percentile, engine, badRes);

    // Sanity: both see the same bootstrap distribution
    REQUIRE(good.getSeBoot() == Catch::Approx(se_boot));
    REQUIRE(bad.getSeBoot()  == Catch::Approx(se_boot));

    // Check that the "good" CI, whose endpoints line up with empirical
    // 20% and 80% quantiles, has a smaller ordering penalty.
    REQUIRE(good.getOrderingPenalty() < bad.getOrderingPenalty());
}

TEST_CASE("AutoBootstrapSelector: length penalty increases as CI deviates from ideal length",
          "[AutoBootstrapSelector][LengthPenalty][Skewed]")
{
    // Use the same skewed bootstrap distribution as above,
    // but this time focus on length distortions.
    std::vector<double> theta_star = {
        -0.5, -0.4, -0.3, -0.2, -0.1,
         0.0,  0.1,  0.2,  1.5,  2.0
    };

    const std::size_t m = theta_star.size();
    double sum = std::accumulate(theta_star.begin(), theta_star.end(), 0.0);
    double mean_boot = sum / static_cast<double>(m);

    double var_boot = 0.0;
    for (double v : theta_star)
    {
        const double d = v - mean_boot;
        var_boot += d * d;
    }
    var_boot /= static_cast<double>(m - 1);
    double se_boot = std::sqrt(var_boot);

    MockPercentileEngine engine;
    engine.diagnosticsReady = true;
    engine.stats    = theta_star;
    engine.meanBoot = mean_boot;
    engine.varBoot  = var_boot;
    engine.seBoot   = se_boot;

    // We'll construct two CIs with the same center = 0, same CL, but
    // different lengths relative to the ideal normal-theory length.
    const double CL = 0.95;
    const double alpha = 1.0 - CL;
    const double z = mkc_timeseries::NormalDistribution::inverseNormalCdf(
        1.0 - 0.5 * alpha);
    const double ideal_len = 2.0 * z * se_boot;

    const std::size_t B = 200;
    const std::size_t n = 25;

    // "Good" CI: perfectly normal-theory length, symmetric around 0
    const double half_good = 0.5 * ideal_len;
    MockPercentileEngine::Result goodRes{
        /*mean*/ 0.0,
        /*lower*/ -half_good,
        /*upper*/  half_good,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    // "Too long" CI: twice the ideal length, but still centered at 0
    const double half_long = ideal_len; // length = 2 * ideal_len
    MockPercentileEngine::Result longRes{
        /*mean*/ 0.0,
        /*lower*/ -half_long,
        /*upper*/  half_long,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    Candidate good = Selector::summarizePercentileLike(MethodId::Percentile, engine, goodRes);
    Candidate bad  = Selector::summarizePercentileLike(MethodId::Percentile, engine, longRes);

    // Both candidates use the same bootstrap distribution
    REQUIRE(good.getSeBoot() == Catch::Approx(se_boot));
    REQUIRE(bad.getSeBoot()  == Catch::Approx(se_boot));

    // "Good" CI should have normalized_length ~ 1, "bad" ~ 2.

    REQUIRE(good.getNormalizedLength() < bad.getNormalizedLength());
    REQUIRE(good.getLengthPenalty()    < bad.getLengthPenalty());

    // Therefore length penalty ( (norm_len - 1)^2 ) should be smaller for the "good" CI.
    REQUIRE(good.getLengthPenalty() < bad.getLengthPenalty());
}

// -----------------------------------------------------------------------------
// High-level test: Pareto selection on skewed bootstrap distribution
// -----------------------------------------------------------------------------

TEST_CASE("AutoBootstrapSelector: select picks best-aligned CI on skewed bootstrap",
          "[AutoBootstrapSelector][Select][Skewed]")
{
    // Same skewed bootstrap distribution as above
    std::vector<double> theta_star = {
        -0.5, -0.4, -0.3, -0.2, -0.1,
         0.0,  0.1,  0.2,  1.5,  2.0
    };

    const std::size_t m = theta_star.size();
    double sum = std::accumulate(theta_star.begin(), theta_star.end(), 0.0);
    double mean_boot = sum / static_cast<double>(m);

    double var_boot = 0.0;
    for (double v : theta_star)
    {
        const double d = v - mean_boot;
        var_boot += d * d;
    }
    var_boot /= static_cast<double>(m - 1);
    double se_boot = std::sqrt(var_boot);

    MockPercentileEngine engine;
    engine.diagnosticsReady = true;
    engine.stats    = theta_star;
    engine.meanBoot = mean_boot;
    engine.varBoot  = var_boot;
    engine.seBoot   = se_boot;

    // Use CL = 0.60 so the "ideal" quantiles are 20% and 80%.
    const double CL = 0.60;
    const std::size_t B = 100;
    const std::size_t n = 20;

    // Sorted theta_star:
    // [-0.5,-0.4,-0.3,-0.2,-0.1,0,0.1,0.2,1.5,2.0]

    // Candidate 1 ("good"): quantile-aligned
    // lower = -0.4 (2nd), upper = 0.2 (8th)
    MockPercentileEngine::Result goodRes{
        /*mean*/ 0.0,
        /*lower*/ -0.4,
        /*upper*/  0.2,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    // Candidate 2 ("too narrow"): symmetric around 0 but shorter than "good"
    MockPercentileEngine::Result narrowRes{
        /*mean*/ 0.0,
        /*lower*/ -0.15,
        /*upper*/  0.15,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    // Candidate 3 ("misaligned and very long"): full range
    // lower = -0.5 (F=0.1), upper = 2.0 (F=1.0)
    MockPercentileEngine::Result badRes{
        /*mean*/ 0.0,
        /*lower*/ -0.5,
        /*upper*/  2.0,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    Candidate c_good   = Selector::summarizePercentileLike(MethodId::Percentile, engine, goodRes);
    Candidate c_narrow = Selector::summarizePercentileLike(MethodId::Percentile, engine, narrowRes);
    Candidate c_bad    = Selector::summarizePercentileLike(MethodId::Percentile, engine, badRes);

    // Basic sanity: all are using the same underlying bootstrap stats
    REQUIRE(c_good.getSeBoot()   == Catch::Approx(se_boot));
    REQUIRE(c_narrow.getSeBoot() == Catch::Approx(se_boot));
    REQUIRE(c_bad.getSeBoot()    == Catch::Approx(se_boot));

    // The quantile-aligned CI ("good") should have the smallest ordering penalty.
    REQUIRE(c_good.getOrderingPenalty() < c_narrow.getOrderingPenalty());
    REQUIRE(c_good.getOrderingPenalty() < c_bad.getOrderingPenalty());

    // Length penalties should be non-negative
    REQUIRE(c_good.getLengthPenalty()   >= 0.0);
    REQUIRE(c_narrow.getLengthPenalty() >= 0.0);
    REQUIRE(c_bad.getLengthPenalty()    >= 0.0);

    std::vector<Candidate> cands = {c_good, c_narrow, c_bad};

    auto result = Selector::select(cands);

    // The selector should prefer the quantile-aligned CI ("good")
    REQUIRE(result.getChosenMethod() == MethodId::Percentile);

    const Candidate& chosen = result.getChosenCandidate();

    REQUIRE(chosen.getLower() == Catch::Approx(-0.4));
    REQUIRE(chosen.getUpper() == Catch::Approx(0.2));

    // Confirm that "good" is not dominated by either of the others,
    // but "bad" is dominated by at least one.
    REQUIRE_FALSE(Selector::dominates(c_narrow, c_good));
    REQUIRE_FALSE(Selector::dominates(c_bad,    c_good));

    bool bad_is_dominated = Selector::dominates(c_good,   c_bad) ||
                            Selector::dominates(c_narrow, c_bad);
    REQUIRE(bad_is_dominated);
}

TEST_CASE("AutoBootstrapSelector: BCa stability and selection behavior",
          "[AutoBootstrapSelector][BCa][Stability]")
{
    // Reuse the same skewed bootstrap distribution from previous tests
    std::vector<double> theta_star = {
        -0.5, -0.4, -0.3, -0.2, -0.1,
         0.0,  0.1,  0.2,  1.5,  2.0
    };

    const std::size_t m = theta_star.size();
    double sum = std::accumulate(theta_star.begin(), theta_star.end(), 0.0);
    double mean_boot = sum / static_cast<double>(m);

    double var_boot = 0.0;
    for (double v : theta_star)
    {
        const double d = v - mean_boot;
        var_boot += d * d;
    }
    var_boot /= static_cast<double>(m - 1);
    double se_boot = std::sqrt(var_boot);

    MockPercentileEngine engine;
    engine.diagnosticsReady = true;
    engine.stats    = theta_star;
    engine.meanBoot = mean_boot;
    engine.varBoot  = var_boot;
    engine.seBoot   = se_boot;

    const double CL = 0.60;
    const std::size_t B = 100;
    const std::size_t n = 20;

    // Create reference candidates from previous test
    MockPercentileEngine::Result goodRes{
        /*mean*/ 0.0,
        /*lower*/ -0.4,
        /*upper*/  0.2,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    MockPercentileEngine::Result narrowRes{
        /*mean*/ 0.0,
        /*lower*/ -0.15,
        /*upper*/  0.15,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    MockPercentileEngine::Result badRes{
        /*mean*/ 0.0,
        /*lower*/ -0.5,
        /*upper*/  2.0,
        /*cl*/ CL,
        /*B*/ B,
        /*effective_B*/ m,
        /*skipped*/ 0,
        /*n*/ n
    };

    Candidate c_good   = Selector::summarizePercentileLike(MethodId::Percentile, engine, goodRes);
    Candidate c_narrow = Selector::summarizePercentileLike(MethodId::Percentile, engine, narrowRes);
    Candidate c_bad    = Selector::summarizePercentileLike(MethodId::Percentile, engine, badRes);

    SECTION("BCa-first selection overrides ordering/length when BCa stable")
    {
        // BCa endpoints do not matter; only z0, accel, and length penalty do
        Candidate bca(MethodId::BCa,
                      0.0, -0.4, 0.2, CL,
                      n, B, 0, m, 0,
                      se_boot, 0.1,
                      0.0, 1.0,
                      /*ordering*/ 999.0, // huge penalties but irrelevant
                      /*length*/ 0.05,
                      /*z0*/ 0.02,
                      /*accel*/ 0.01);

        std::vector<Candidate> cands = {c_good, c_narrow, c_bad, bca};

        auto result = Selector::select(cands);

        REQUIRE(result.getChosenMethod() == MethodId::BCa);
        REQUIRE(result.getChosenCandidate().getStabilityPenalty() <= 0.1);
    }

    SECTION("Unstable BCa does NOT win; selector falls back to percentile geometry")
    {
        Candidate unstable_bca(MethodId::BCa,
                               0.0, -0.4, 0.2, CL,
                               n, B, 0, m, 0,
                               se_boot, 0.1,
                               0.0, 1.0,
                               0.0,  // ordering (ignored)
                               0.0,  // length penalty (ignored at first)
                               /*z0*/ 1.5,    // very large => unstable
                               /*accel*/ 0.25);

        // Should push BCa stability_penalty >> threshold
        REQUIRE(unstable_bca.getStabilityPenalty() > 0.1);

        std::vector<Candidate> cands = {c_good, c_narrow, c_bad, unstable_bca};

        auto result = Selector::select(cands);

        REQUIRE(result.getChosenMethod() == MethodId::Percentile);
    }
}


