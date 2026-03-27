// AutoBootstrapSelectorTransformStabilityTest.cpp
//
// Unit tests for the BCa percentile-transform stability diagnostic added to
// AutoBootstrapSelector and AutoCIResult.
//
// These tests cover three layers, mirroring the structure of
// AutoBootstrapSelectorTest.cpp:
//
//   Layer 1 — Candidate: bcaTransformMonotone field
//     Storage, retrieval, default value, and preservation through withScore()
//     and withMetadata().
//
//   Layer 2 — summarizeBCa(): transform stability applied correctly
//     Non-monotone flag adds kBcaTransformNonMonotonePenalty (0.5) to
//     stability_penalty; monotone flag adds nothing. Accumulation with
//     existing z0/accel penalties, algorithmIsReliable AND-gate, debug
//     logging through an ostream, and bcaTransformMonotone stored on the
//     returned Candidate.
//
//   Layer 3 — Tournament selection: end-to-end effects
//     A BCa candidate whose transform mapping inverted is down-weighted by
//     0.5 raw stability (normalised to 2.0 score units), causing it to lose
//     to a clean competitor.  wasBCaRejectedForInstability() is set when BCa
//     loses with a non-monotone transform.  BCa can still win when no other
//     candidate scores lower despite the penalty.
//
// All expected penalty values are computed analytically and verified against
// the configuration constants in AutoBootstrapConfiguration.h:
//
//   kBcaTransformNonMonotonePenalty = 0.5
//   kBcaZ0SoftThreshold             = 0.25   (penalty = (excess)^2 * z0_scale)
//   kBcaASoftThreshold              = 0.10   (penalty = (excess)^2 * a_scale)
//   default z0_scale                = 20.0
//   default a_scale                 = 100.0
//   kRefStability                   = 0.25   (normaliser in scoring)
//
// Place in: libs/statistics/test/
//
// Requires:
//   - Catch2 v3
//   - AutoBootstrapSelector.h
//   - BiasCorrectedBootstrap.h  (for BcaTransformStability type)
//   - AutoBootstrapConfiguration.h
//   - number.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <limits>
#include <cmath>
#include <sstream>

#include "AutoBootstrapSelector.h"
#include "BiasCorrectedBootstrap.h"
#include "AutoBootstrapConfiguration.h"
#include "number.h"

using Decimal        = double;
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using Candidate      = Selector::Candidate;
using Result         = Selector::Result;
using ScoringWeights = Selector::ScoringWeights;
using MethodId       = Result::MethodId;

// =============================================================================
// Mock BCa engines
//
// Two variants so each failure mode can be exercised in isolation through
// summarizeBCa(), without changing the shared MockBCaEngine already used in
// AutoBootstrapSelectorTest.cpp.
// =============================================================================

// Default mock: accel reliable, transform monotone.
// Identical baseline to AutoBootstrapSelectorTest.cpp's MockBCaEngine.
struct MockBCaEngineMonotone
{
    Decimal      mean  = 0.0;
    Decimal      lower = -1.0;
    Decimal      upper =  1.0;
    double       cl    = 0.95;
    std::size_t  B     = 1000;
    std::size_t  n     = 100;
    double       z0    = 0.0;
    Decimal      accel = 0.0;
    std::vector<Decimal> stats = { -1.0, 0.0, 1.0 };

    Decimal      getMean()            const { return mean;  }
    Decimal      getLowerBound()      const { return lower; }
    Decimal      getUpperBound()      const { return upper; }
    double       getConfidenceLevel() const { return cl;    }
    unsigned int getNumResamples()    const { return static_cast<unsigned int>(B); }
    std::size_t  getSampleSize()      const { return n;     }
    double       getZ0()              const { return z0;    }
    Decimal      getAcceleration()    const { return accel; }
    const std::vector<Decimal>& getBootstrapStatistics() const { return stats; }

    mkc_timeseries::AccelerationReliability getAccelerationReliability() const
    {
        return mkc_timeseries::AccelerationReliability(true, 0.0, 0, 0, 1.0);
    }

    // Well-behaved transform: denominators safely positive, alpha1 <= alpha2.
    mkc_timeseries::BcaTransformStability getBcaTransformStability() const
    {
        return mkc_timeseries::BcaTransformStability(
            true,   // isStable   -- denominators safely away from zero
            true,   // isMonotone -- alpha1 <= alpha2
            1.0,    // denomLo
            1.0);   // denomHi
    }
};

// Non-monotone mock: accel reliable, but transform mapping inverted (alpha1 > alpha2).
// All other fields identical to MockBCaEngineMonotone.
struct MockBCaEngineNonMonotone
{
    Decimal      mean  = 0.0;
    Decimal      lower = -1.0;
    Decimal      upper =  1.0;
    double       cl    = 0.95;
    std::size_t  B     = 1000;
    std::size_t  n     = 100;
    double       z0    = 0.0;
    Decimal      accel = 0.0;
    std::vector<Decimal> stats = { -1.0, 0.0, 1.0 };

    Decimal      getMean()            const { return mean;  }
    Decimal      getLowerBound()      const { return lower; }
    Decimal      getUpperBound()      const { return upper; }
    double       getConfidenceLevel() const { return cl;    }
    unsigned int getNumResamples()    const { return static_cast<unsigned int>(B); }
    std::size_t  getSampleSize()      const { return n;     }
    double       getZ0()              const { return z0;    }
    Decimal      getAcceleration()    const { return accel; }
    const std::vector<Decimal>& getBootstrapStatistics() const { return stats; }

    mkc_timeseries::AccelerationReliability getAccelerationReliability() const
    {
        return mkc_timeseries::AccelerationReliability(true, 0.0, 0, 0, 1.0);
    }

    // Non-monotone transform: alpha1 > alpha2 — mapping inverted, bounds silently
    // swapped in calculateBCaBounds().  Denominators are themselves safe (the near-
    // singular path is geometrically impossible within the hard-gate parameter space).
    mkc_timeseries::BcaTransformStability getBcaTransformStability() const
    {
        return mkc_timeseries::BcaTransformStability(
            true,    // isStable   -- denominators OK
            false,   // isMonotone -- alpha1 > alpha2: mapping inverted
            1.05,    // denomLo
            0.92);   // denomHi
    }
};

// Unreliable-acceleration mock: jackknife dominated by a single outlier.
// Transform is well-behaved. Used to verify the AND-gate on algorithmIsReliable.
struct MockBCaEngineUnreliableAccel
{
    Decimal      mean  = 0.0;
    Decimal      lower = -1.0;
    Decimal      upper =  1.0;
    double       cl    = 0.95;
    std::size_t  B     = 1000;
    std::size_t  n     = 100;
    double       z0    = 0.0;
    Decimal      accel = 0.0;
    std::vector<Decimal> stats = { -1.0, 0.0, 1.0 };

    Decimal      getMean()            const { return mean;  }
    Decimal      getLowerBound()      const { return lower; }
    Decimal      getUpperBound()      const { return upper; }
    double       getConfidenceLevel() const { return cl;    }
    unsigned int getNumResamples()    const { return static_cast<unsigned int>(B); }
    std::size_t  getSampleSize()      const { return n;     }
    double       getZ0()              const { return z0;    }
    Decimal      getAcceleration()    const { return accel; }
    const std::vector<Decimal>& getBootstrapStatistics() const { return stats; }

    // Single observation dominates Σ|d³|: acceleration is an artifact of that outlier.
    mkc_timeseries::AccelerationReliability getAccelerationReliability() const
    {
        return mkc_timeseries::AccelerationReliability(
            false,  // isReliable       -- single observation dominant
            0.72,   // maxInfluenceFrac -- 72% of total absolute cubic influence
            3,      // maxInfluenceIdx
            1,      // nDominant
            0.85);  // cancellationRatio
    }

    // Transform is fine.
    mkc_timeseries::BcaTransformStability getBcaTransformStability() const
    {
        return mkc_timeseries::BcaTransformStability(true, true, 1.0, 1.0);
    }
};

// =============================================================================
// Layer 1 — Candidate: bcaTransformMonotone field
// =============================================================================

TEST_CASE("Candidate: bcaTransformMonotone defaults to true",
          "[AutoBootstrapSelector][Candidate][TransformStability]")
{
    // When no explicit value is passed for param 30, the default should be true.
    // This ensures all non-BCa candidates (and BCa candidates from code that
    // predates the new field) are treated as having a monotone transform by
    // convention, which is consistent with BcaTransformStability's own convention
    // for the degenerate all-equal case.
    Candidate c(MethodId::BCa,
                0.0, -1.0, 1.0, 0.95,
                100, 1000, 0, 1000, 0,
                0.10, 0.05,
                0.0,  // median_boot
                0.0, 1.0,
                0.0, 0.0, 0.0,
                0.05, 0.02,
                0.0   // inner_failure_rate — no further args; bcaTransformMonotone defaults to true
                );

    REQUIRE(c.getBcaTransformMonotone() == true);
}

TEST_CASE("Candidate: bcaTransformMonotone=true stored and retrieved correctly",
          "[AutoBootstrapSelector][Candidate][TransformStability]")
{
    Candidate c(MethodId::BCa,
                1.0, 0.8, 1.2, 0.95,
                100, 1000, 0, 1000, 0,
                0.10, 0.05,
                1.0,   // median_boot
                0.0, 1.0,
                0.0, 0.0, 0.0,
                0.05, 0.02, 0.0,
                std::numeric_limits<double>::quiet_NaN(), // score
                0,     // candidate_id
                0,     // rank
                false, // is_chosen
                true,  // accelIsReliable
                true,  // algorithmIsReliable
                false, // excessiveBias
                0.0,   // excessBias
                true   // bcaTransformMonotone — param 30, explicit true
                );

    REQUIRE(c.getBcaTransformMonotone() == true);
}

TEST_CASE("Candidate: bcaTransformMonotone=false stored and retrieved correctly",
          "[AutoBootstrapSelector][Candidate][TransformStability]")
{
    // Non-monotone flag must survive the constructor and be returned as-is.
    Candidate c(MethodId::BCa,
                1.0, 0.8, 1.2, 0.95,
                100, 1000, 0, 1000, 0,
                0.10, 0.05,
                1.0,   // median_boot
                0.0, 1.0,
                0.0, 0.0, 0.5,  // stability_penalty reflects non-monotone
                0.05, 0.02, 0.0,
                std::numeric_limits<double>::quiet_NaN(),
                0, 0, false,
                true,   // accelIsReliable
                false,  // algorithmIsReliable (= accelReliable && transform_monotone = true && false)
                false,  // excessiveBias
                0.0,    // excessBias
                false   // bcaTransformMonotone — param 30, explicit false
                );

    REQUIRE(c.getBcaTransformMonotone() == false);
}

TEST_CASE("Candidate: withScore preserves bcaTransformMonotone",
          "[AutoBootstrapSelector][Candidate][TransformStability]")
{
    SECTION("Preserved when bcaTransformMonotone=true")
    {
        Candidate c(MethodId::BCa,
                    0.0, -1.0, 1.0, 0.95,
                    100, 1000, 0, 1000, 0,
                    0.10, 0.05, 0.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 0.05, 0.02, 0.0,
                    std::numeric_limits<double>::quiet_NaN(),
                    0, 0, false, true, true, false, 0.0,
                    true  // bcaTransformMonotone
                    );

        Candidate scored = c.withScore(99.0);
        REQUIRE(scored.getBcaTransformMonotone() == true);
        REQUIRE(scored.getScore() == 99.0);
    }

    SECTION("Preserved when bcaTransformMonotone=false")
    {
        Candidate c(MethodId::BCa,
                    0.0, -1.0, 1.0, 0.95,
                    100, 1000, 0, 1000, 0,
                    0.10, 0.05, 0.0, 0.0, 1.0,
                    0.0, 0.0, 0.5, 0.05, 0.02, 0.0,
                    std::numeric_limits<double>::quiet_NaN(),
                    0, 0, false, true, false, false, 0.0,
                    false  // bcaTransformMonotone
                    );

        Candidate scored = c.withScore(42.0);
        REQUIRE(scored.getBcaTransformMonotone() == false);
        REQUIRE(scored.getScore() == 42.0);
    }
}

TEST_CASE("Candidate: withMetadata preserves bcaTransformMonotone",
          "[AutoBootstrapSelector][Candidate][TransformStability]")
{
    SECTION("Preserved when bcaTransformMonotone=true")
    {
        Candidate c(MethodId::BCa,
                    0.0, -1.0, 1.0, 0.95,
                    100, 1000, 0, 1000, 0,
                    0.10, 0.05, 0.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 0.05, 0.02, 0.0,
                    std::numeric_limits<double>::quiet_NaN(),
                    0, 0, false, true, true, false, 0.0,
                    true  // bcaTransformMonotone
                    );

        Candidate meta = c.withMetadata(7, 1, true);
        REQUIRE(meta.getBcaTransformMonotone() == true);
        REQUIRE(meta.getCandidateId() == 7);
        REQUIRE(meta.isChosen() == true);
    }

    SECTION("Preserved when bcaTransformMonotone=false")
    {
        Candidate c(MethodId::BCa,
                    0.0, -1.0, 1.0, 0.95,
                    100, 1000, 0, 1000, 0,
                    0.10, 0.05, 0.0, 0.0, 1.0,
                    0.0, 0.0, 0.5, 0.05, 0.02, 0.0,
                    std::numeric_limits<double>::quiet_NaN(),
                    0, 0, false, true, false, false, 0.0,
                    false  // bcaTransformMonotone
                    );

        Candidate meta = c.withMetadata(3, 2, false);
        REQUIRE(meta.getBcaTransformMonotone() == false);
        REQUIRE(meta.getCandidateId() == 3);
    }
}

// =============================================================================
// Layer 2 — summarizeBCa(): transform stability applied correctly
// =============================================================================

TEST_CASE("summarizeBCa: monotone transform adds no extra stability penalty",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // With z0=0, accel=0, skew=0 (symmetric {-1,0,1}): base stability penalty = 0.0
    // Monotone transform adds nothing. Total must remain 0.0.
    MockBCaEngineMonotone engine;
    engine.z0    = 0.0;
    engine.accel = 0.0;

    Candidate c = Selector::summarizeBCa(engine);

    REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("summarizeBCa: non-monotone transform adds kBcaTransformNonMonotonePenalty",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // With z0=0, accel=0, skew=0: base penalty = 0.0.
    // Non-monotone transform adds kBcaTransformNonMonotonePenalty = 0.5.
    // Total stability_penalty must be exactly 0.5.
    MockBCaEngineNonMonotone engine;
    engine.z0    = 0.0;
    engine.accel = 0.0;

    const double expected =
        AutoBootstrapConfiguration::kBcaTransformNonMonotonePenalty;  // 0.5

    Candidate c = Selector::summarizeBCa(engine);

    REQUIRE(c.getStabilityPenalty() == Catch::Approx(expected));
}

TEST_CASE("summarizeBCa: non-monotone penalty accumulates with z0 penalty",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // z0 = 0.35:
    //   excess = 0.35 - 0.25 = 0.10
    //   z0_penalty = 0.10^2 * 20.0 = 0.20
    //
    // Non-monotone: +0.5
    //
    // Total = 0.20 + 0.50 = 0.70
    MockBCaEngineNonMonotone engine;
    engine.z0    = 0.35;
    engine.accel = 0.0;

    Candidate c = Selector::summarizeBCa(engine);

    REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.70));
}

TEST_CASE("summarizeBCa: non-monotone penalty accumulates with accel penalty",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // accel = 0.12:
    //   excess = 0.12 - 0.10 = 0.02
    //   accel_penalty = 0.02^2 * 100.0 = 0.04
    //
    // Non-monotone: +0.5
    //
    // Total = 0.04 + 0.50 = 0.54
    MockBCaEngineNonMonotone engine;
    engine.z0    = 0.0;
    engine.accel = 0.12;

    Candidate c = Selector::summarizeBCa(engine);

    REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.54));
}

TEST_CASE("summarizeBCa: non-monotone penalty accumulates with both z0 and accel",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // z0 = 0.35 -> 0.20,  accel = 0.12 -> 0.04,  non-monotone -> 0.50
    // Total = 0.74
    MockBCaEngineNonMonotone engine;
    engine.z0    = 0.35;
    engine.accel = 0.12;

    Candidate c = Selector::summarizeBCa(engine);

    REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.74));
}

TEST_CASE("summarizeBCa: monotone transform does not change existing z0+accel penalty",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // Monotone engine with z0=0.35, accel=0.12 must give the same result as
    // the pre-existing test "Both violations accumulate" (0.24) — the monotone
    // flag must not silently add or subtract anything from that total.
    MockBCaEngineMonotone engine;
    engine.z0    = 0.35;
    engine.accel = 0.12;

    Candidate c = Selector::summarizeBCa(engine);

    REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.24));
}

TEST_CASE("summarizeBCa: bcaTransformMonotone stored on returned Candidate",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    SECTION("Monotone engine -> getBcaTransformMonotone() is true")
    {
        MockBCaEngineMonotone engine;
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getBcaTransformMonotone() == true);
    }

    SECTION("Non-monotone engine -> getBcaTransformMonotone() is false")
    {
        MockBCaEngineNonMonotone engine;
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getBcaTransformMonotone() == false);
    }
}

TEST_CASE("summarizeBCa: algorithmIsReliable is AND of accel reliability and transform monotone",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // algorithmIsReliable = accel_is_reliable && transform_monotone
    // Four combinations must all be correct.

    SECTION("accel reliable + monotone -> algorithmIsReliable = true")
    {
        MockBCaEngineMonotone engine;  // accel=reliable, transform=monotone
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getAccelIsReliable() == true);
        REQUIRE(c.getBcaTransformMonotone() == true);
        REQUIRE(c.getAlgorithmIsReliable() == true);
    }

    SECTION("accel reliable + non-monotone -> algorithmIsReliable = false")
    {
        MockBCaEngineNonMonotone engine;  // accel=reliable, transform=non-monotone
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getAccelIsReliable() == true);
        REQUIRE(c.getBcaTransformMonotone() == false);
        REQUIRE(c.getAlgorithmIsReliable() == false);
    }

    SECTION("accel unreliable + monotone -> algorithmIsReliable = false")
    {
        MockBCaEngineUnreliableAccel engine;  // accel=unreliable, transform=monotone
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getAccelIsReliable() == false);
        REQUIRE(c.getBcaTransformMonotone() == true);
        REQUIRE(c.getAlgorithmIsReliable() == false);
    }

    SECTION("accel unreliable + non-monotone: both false -> algorithmIsReliable = false")
    {
        // Create a mock that reports both failures simultaneously.
        struct MockBCaBothUnreliable
        {
            Decimal mean = 0.0, lower = -1.0, upper = 1.0;
            double cl = 0.95; std::size_t B = 1000, n = 100;
            double z0 = 0.0; Decimal accel = 0.0;
            std::vector<Decimal> stats = { -1.0, 0.0, 1.0 };

            Decimal getMean() const { return mean; }
            Decimal getLowerBound() const { return lower; }
            Decimal getUpperBound() const { return upper; }
            double getConfidenceLevel() const { return cl; }
            unsigned int getNumResamples() const { return static_cast<unsigned int>(B); }
            std::size_t getSampleSize() const { return n; }
            double getZ0() const { return z0; }
            Decimal getAcceleration() const { return accel; }
            const std::vector<Decimal>& getBootstrapStatistics() const { return stats; }

            mkc_timeseries::AccelerationReliability getAccelerationReliability() const
            {
                return mkc_timeseries::AccelerationReliability(false, 0.9, 0, 1, 0.9);
            }
            mkc_timeseries::BcaTransformStability getBcaTransformStability() const
            {
                return mkc_timeseries::BcaTransformStability(true, false, 1.05, 0.92);
            }
        };

        MockBCaBothUnreliable engine;
        Candidate c = Selector::summarizeBCa(engine);
        REQUIRE(c.getAccelIsReliable() == false);
        REQUIRE(c.getBcaTransformMonotone() == false);
        REQUIRE(c.getAlgorithmIsReliable() == false);
    }
}

TEST_CASE("summarizeBCa: accelIsReliable and algorithmIsReliable remain independent fields",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // When only the transform is non-monotone, accelIsReliable must remain true
    // while algorithmIsReliable becomes false. These are separate fields with
    // distinct semantics; conflating them would lose diagnostic information.
    MockBCaEngineNonMonotone engine;
    Candidate c = Selector::summarizeBCa(engine);

    // accelIsReliable captures ONLY the jackknife failure mode.
    REQUIRE(c.getAccelIsReliable() == true);
    // algorithmIsReliable captures the combined failure: accel AND transform.
    REQUIRE(c.getAlgorithmIsReliable() == false);
    // The two must differ when only one failure mode fires.
    REQUIRE(c.getAccelIsReliable() != c.getAlgorithmIsReliable());
}

TEST_CASE("summarizeBCa: non-monotone transform writes to debug log stream",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa][Logging]")
{
    // When a non-monotone transform is detected, summarizeBCa() should write a
    // diagnostic message to the optional ostream, including the penalty value
    // and the denominator values from getBcaTransformStability().
    MockBCaEngineNonMonotone engine;
    engine.z0    = 0.0;
    engine.accel = 0.0;

    std::ostringstream log;
    Selector::summarizeBCa(engine, ScoringWeights(), &log);

    const std::string output = log.str();
    REQUIRE_FALSE(output.empty());
    REQUIRE(output.find("Non-monotone transform") != std::string::npos);
    REQUIRE(output.find("denom_lo") != std::string::npos);
    REQUIRE(output.find("denom_hi") != std::string::npos);
}

TEST_CASE("summarizeBCa: monotone transform writes nothing extra to debug log",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa][Logging]")
{
    // A monotone transform produces no extra log output.  The existing z0/accel
    // logging may still fire if those parameters exceed their soft thresholds,
    // but the transform-specific message must be absent.
    MockBCaEngineMonotone engine;
    engine.z0    = 0.0;
    engine.accel = 0.0;

    std::ostringstream log;
    Selector::summarizeBCa(engine, ScoringWeights(), &log);

    const std::string output = log.str();
    REQUIRE(output.find("Non-monotone transform") == std::string::npos);
}

TEST_CASE("summarizeBCa: non-monotone penalty magnitude equals kBcaTransformNonMonotonePenalty",
          "[AutoBootstrapSelector][BCa][TransformStability][summarizeBCa]")
{
    // Pin the exact constant value so that a future change to
    // kBcaTransformNonMonotonePenalty is immediately caught here.
    REQUIRE(AutoBootstrapConfiguration::kBcaTransformNonMonotonePenalty ==
            Catch::Approx(0.5).epsilon(1e-12));

    // And verify that the penalty on the returned Candidate matches.
    MockBCaEngineNonMonotone engine;
    engine.z0    = 0.0;
    engine.accel = 0.0;

    Candidate c = Selector::summarizeBCa(engine);
    REQUIRE(c.getStabilityPenalty() ==
            Catch::Approx(AutoBootstrapConfiguration::kBcaTransformNonMonotonePenalty));
}

// =============================================================================
// Layer 3 — Tournament selection: end-to-end effects
// =============================================================================

TEST_CASE("Tournament: BCa with non-monotone transform is down-weighted",
          "[AutoBootstrapSelector][BCa][TransformStability][Tournament]")
{
    // Non-monotone BCa: stability_penalty = 0.5 (only the transform penalty).
    //   stability_norm  = 0.5 / 0.25  = 2.0
    //   stability_contrib = 1.0 * 2.0 = 2.0
    //   Total tournament score ≈ 2.0
    //
    // Clean BCa (monotone): stability_penalty = 0.0.
    //   score ≈ 0.0
    //
    // Clean BCa must win.
    Candidate bcaClean(MethodId::BCa,
                       0.0, -1.0, 1.0, 0.95,
                       100, 1000, 0, 1000, 0,
                       0.10, 0.05,
                       0.0,   // median
                       0.0, 1.0,
                       0.0, 0.0, 0.0,    // ordering, length, stability (clean)
                       0.05, 0.02, 0.0,
                       std::numeric_limits<double>::quiet_NaN(),
                       0, 0, false,
                       true,  // accelIsReliable
                       true,  // algorithmIsReliable
                       false, 0.0,
                       true   // bcaTransformMonotone
                       );

    Candidate bcaNonMonotone(MethodId::BCa,
                              0.0, -1.0, 1.0, 0.95,
                              100, 1000, 0, 1000, 0,
                              0.10, 0.05,
                              0.0,   // median
                              0.0, 1.0,
                              0.0, 0.0, 0.5,   // stability_penalty = kBcaTransformNonMonotonePenalty
                              0.05, 0.02, 0.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              0, 0, false,
                              true,  // accelIsReliable
                              false, // algorithmIsReliable (accel ok, transform not)
                              false, 0.0,
                              false  // bcaTransformMonotone
                              );

    std::vector<Candidate> cands = {bcaNonMonotone, bcaClean};
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::BCa);
    REQUIRE(result.getChosenCandidate().getBcaTransformMonotone() == true);
}

TEST_CASE("Tournament: BCa non-monotone loses to clean PercentileT",
          "[AutoBootstrapSelector][BCa][TransformStability][Tournament]")
{
    // Clean PercentileT: small ordering penalty, score ≈ 0.2.
    // BCa non-monotone: stability_penalty = 0.5, score ≈ 2.0.
    // PercentileT must win.
    Candidate percT(MethodId::PercentileT,
                    0.0, -1.1, 1.1, 0.95,
                    100, 1000, 100, 1000, 0,
                    0.10, 0.05,
                    0.08,  // median
                    0.0, 1.0,
                    0.002,  // small ordering penalty
                    0.0, 0.0,
                    0.0, 0.0, 0.0);

    Candidate bcaNonMonotone(MethodId::BCa,
                              0.0, -1.0, 1.0, 0.95,
                              100, 1000, 0, 1000, 0,
                              0.10, 0.05,
                              0.0,   // median
                              0.0, 1.0,
                              0.0, 0.0, 0.5,   // non-monotone penalty only
                              0.05, 0.02, 0.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              0, 0, false,
                              true,  // accelIsReliable
                              false, // algorithmIsReliable
                              false, 0.0,
                              false  // bcaTransformMonotone
                              );

    std::vector<Candidate> cands = {bcaNonMonotone, percT};
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
    REQUIRE(result.getDiagnostics().hasBCaCandidate() == true);
    REQUIRE(result.getDiagnostics().isBCaChosen() == false);
}

TEST_CASE("Tournament: wasBCaRejectedForInstability set when BCa has non-monotone transform and loses",
          "[AutoBootstrapSelector][BCa][TransformStability][Tournament][Diagnostics]")
{
    // BCa non-monotone does not win (PercentileT wins on score).
    // analyzeBcaRejection() checks !getBcaTransformMonotone() and must set
    // rejected_for_instability = true, surfacing it through
    // wasBCaRejectedForInstability().
    Candidate percT(MethodId::PercentileT,
                    0.0, -1.1, 1.1, 0.95,
                    100, 1000, 100, 1000, 0,
                    0.10, 0.05,
                    0.08,
                    0.0, 1.0,
                    0.002, 0.0, 0.0,
                    0.0, 0.0, 0.0);

    Candidate bcaNonMonotone(MethodId::BCa,
                              0.0, -1.0, 1.0, 0.95,
                              100, 1000, 0, 1000, 0,
                              0.10, 0.05,
                              0.0,
                              0.0, 1.0,
                              0.0, 0.0, 0.5,
                              0.05, 0.02, 0.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              0, 0, false,
                              true, false, false, 0.0,
                              false  // bcaTransformMonotone
                              );

    std::vector<Candidate> cands = {bcaNonMonotone, percT};
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
    REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == true);
}

TEST_CASE("Tournament: wasBCaRejectedForInstability false when monotone BCa loses on score only",
          "[AutoBootstrapSelector][BCa][TransformStability][Tournament][Diagnostics]")
{
    // A BCa candidate that passes all stability checks but loses on score
    // (e.g., because a PercentileT has a much lower ordering penalty) should
    // NOT have the instability flag set. This verifies the flag is specifically
    // tied to the non-monotone transform, not to losing the tournament in general.
    Candidate percT(MethodId::PercentileT,
                    0.0, -1.1, 1.1, 0.95,
                    100, 1000, 100, 1000, 0,
                    0.10, 0.05,
                    0.08,
                    0.0, 1.0,
                    0.001, 0.0, 0.0,   // very low ordering penalty
                    0.0, 0.0, 0.0);

    // BCa with moderate z0 penalty (loses on score, not on stability flag).
    Candidate bcaMonotone(MethodId::BCa,
                           0.0, -1.0, 1.0, 0.95,
                           100, 1000, 0, 1000, 0,
                           0.10, 0.05,
                           0.0,
                           0.0, 1.0,
                           0.0, 0.0, 0.20,  // z0=0.35 penalty, no non-monotone
                           0.35, 0.0, 0.0,
                           std::numeric_limits<double>::quiet_NaN(),
                           0, 0, false,
                           true, true, false, 0.0,
                           true  // bcaTransformMonotone — no transform issue
                           );

    std::vector<Candidate> cands = {bcaMonotone, percT};
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::PercentileT);
    REQUIRE(result.getDiagnostics().hasBCaCandidate() == true);
    // BCa lost purely on score, not because the transform flag fired.
    REQUIRE(result.getDiagnostics().wasBCaRejectedForInstability() == false);
}

TEST_CASE("Tournament: BCa with non-monotone transform can still win if competitors score worse",
          "[AutoBootstrapSelector][BCa][TransformStability][Tournament]")
{
    // Even with the 0.5 non-monotone penalty, BCa wins when every competitor
    // has a worse score.  The penalty is a soft down-weight, not a hard veto.
    //
    // BCa non-monotone: stability_penalty = 0.5
    //   stability_norm  = 0.5 / 0.25 = 2.0
    //   stability_contrib ≈ 2.0   (with wStability=1.0)
    //   Total score     ≈ 2.0
    //
    // Percentile with heavy ordering penalty: ordering_penalty = 1.0
    //   ordering_norm  = 1.0 / 0.01 = 100.0
    //   ordering_contrib ≈ 100.0  (with wOrdering=1.0)
    //   Total score    ≈ 100.0
    //
    // BCa (score 2.0) must beat Percentile (score 100.0).
    Candidate bcaNonMonotone(MethodId::BCa,
                              0.0, -1.0, 1.0, 0.95,
                              100, 1000, 0, 1000, 0,
                              0.10, 0.05,
                              0.0,
                              0.0, 1.0,
                              0.0, 0.0, 0.5,   // non-monotone penalty
                              0.05, 0.02, 0.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              0, 0, false,
                              true, false, false, 0.0,
                              false  // bcaTransformMonotone
                              );

    Candidate percBad(MethodId::Percentile,
                      0.0, -1.5, 1.5, 0.95,
                      100, 1000, 0, 1000, 0,
                      0.10, 0.05,
                      0.0,
                      0.0, 1.0,
                      1.0, 0.0, 0.0,    // very large ordering penalty
                      0.0, 0.0, 0.0);

    std::vector<Candidate> cands = {bcaNonMonotone, percBad};
    auto result = Selector::select(cands);

    REQUIRE(result.getChosenMethod() == MethodId::BCa);
    REQUIRE(result.getDiagnostics().isBCaChosen() == true);
}

TEST_CASE("Tournament: non-monotone BCa outscores stable BCa only when stable z0/accel are much worse",
          "[AutoBootstrapSelector][BCa][TransformStability][Tournament]")
{
    // Non-monotone BCa with clean z0/accel (stability_penalty = 0.5).
    // Stable-transform BCa with z0=0.50 (stability_penalty = 0.20 + large = 1.352).
    //
    // z0=0.50: excess = 0.25, penalty = (0.25)^2 * 20.0 = 1.25.
    //   Plus skew multiplier at default (skew=0, multiplier=1): total = 1.25.
    //
    // Non-monotone (0.5) < stable-but-high-z0 (1.25).
    // Non-monotone BCa should win this head-to-head.
    Candidate bcaHighZ0(MethodId::BCa,
                         0.0, -1.0, 1.0, 0.95,
                         100, 1000, 0, 1000, 0,
                         0.10, 0.05,
                         0.0,
                         0.0, 1.0,
                         0.0, 0.0, 1.25,   // z0=0.50 penalty
                         0.50, 0.0, 0.0,
                         std::numeric_limits<double>::quiet_NaN(),
                         0, 0, false,
                         true, true, false, 0.0,
                         true  // bcaTransformMonotone -- stable transform
                         );

    Candidate bcaNonMonotone(MethodId::BCa,
                              0.0, -1.0, 1.0, 0.95,
                              100, 1000, 0, 1000, 0,
                              0.10, 0.05,
                              0.0,
                              0.0, 1.0,
                              0.0, 0.0, 0.5,   // only non-monotone penalty
                              0.05, 0.0, 0.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              0, 0, false,
                              true, false, false, 0.0,
                              false  // bcaTransformMonotone
                              );

    std::vector<Candidate> cands = {bcaHighZ0, bcaNonMonotone};
    auto result = Selector::select(cands);

    // The non-monotone BCa has a lower raw stability penalty (0.5 vs 1.25)
    // so its normalised score is lower and it should win.
    REQUIRE(result.getChosenMethod() == MethodId::BCa);
    REQUIRE(result.getChosenCandidate().getBcaTransformMonotone() == false);
    REQUIRE(result.getChosenCandidate().getStabilityPenalty() ==
            Catch::Approx(0.5));
}