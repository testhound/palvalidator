// BCaAccelerationReliabilityTest.cpp
//
// Unit tests for:
//   - AccelerationReliability  (construction, accessors, thresholds)
//   - JackknifeInfluence::compute()  (all logical branches under the
//         Tier-1 materiality short-circuit + Tier-2 leave-one-out
//         sensitivity test)
//   - BCaBootStrap::getAccelerationReliability()  (integration with BCa)
//
// RELIABILITY RULE UNDER TEST
// ---------------------------
// JackknifeInfluence::compute() now returns
//       reliable = (|â| < kAccelMaterialThreshold)  OR  sensitivityOk
// where
//       â              = Σd³ / (6·(Σd²)^1.5)
//       â_without_top  = (Σd³ − top_d³) / (6·(Σd² − top_d²)^1.5)
//       relChange      = |â − â_without_top| / max(|â|, ε)
//       sensitivityOk  = (relChange ≤ kSensitivityThreshold)
//
// The legacy "single-point dominance" statistic
//       maxInfluenceFraction = max_i(|d_i³|) / Σ|d_j³|
// is still computed and exposed but no longer gates reliability.
//
// COMPUTE() INPUT CHANGE
// ----------------------
// JackknifeInfluence::compute() now takes the raw jackknife deviations
// d_i = jk_avg − jk_i, NOT the pre-cubed values. All test inputs below
// are raw d-vectors. Expected values were verified independently.
//
// Uses Catch2.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cmath>
#include <numeric>

#include "BiasCorrectedBootstrap.h"
#include "TestUtils.h"
#include "number.h"

using namespace mkc_timeseries;

// ============================================================================
// AccelerationReliability: construction and accessors
// ============================================================================

TEST_CASE("AccelerationReliability: constructor stores all 10 values correctly",
          "[AccelerationReliability]")
{
    // Exercise every stored field so an accidental member-order swap would
    // fail to compile or fail the round-trip check.
    const bool        reliable          = false;
    const double      maxFrac           = 0.7143;
    const std::size_t maxIdx            = 2;
    const std::size_t nDominant         = 1;
    const double      cancellationRatio = 0.75;
    const double      accel             = 0.185;
    const double      accelWithoutTop   = -0.042;
    const double      accelRelChange    = 1.227;
    const bool        accelMaterial     = true;
    const bool        sensitivityOk     = false;

    AccelerationReliability ar(reliable, maxFrac, maxIdx, nDominant,
                               cancellationRatio, accel, accelWithoutTop,
                               accelRelChange, accelMaterial, sensitivityOk);

    REQUIRE(ar.isReliable()               == reliable);
    REQUIRE(ar.getMaxInfluenceFraction()  == Catch::Approx(maxFrac).epsilon(1e-12));
    REQUIRE(ar.getMaxInfluenceIndex()     == maxIdx);
    REQUIRE(ar.getNDominant()             == nDominant);
    REQUIRE(ar.getCancellationRatio()     == Catch::Approx(cancellationRatio).epsilon(1e-12));
    REQUIRE(ar.getAccel()                 == Catch::Approx(accel).epsilon(1e-12));
    REQUIRE(ar.getAccelWithoutTop()       == Catch::Approx(accelWithoutTop).epsilon(1e-12));
    REQUIRE(ar.getAccelRelativeChange()   == Catch::Approx(accelRelChange).epsilon(1e-12));
    REQUIRE(ar.isAccelMaterial()          == accelMaterial);
    REQUIRE(ar.isSensitivityOk()          == sensitivityOk);
}

TEST_CASE("AccelerationReliability: reliable=true variant stores correctly",
          "[AccelerationReliability]")
{
    // Typical "everything fine" shape: small |â|, dominance not fired,
    // cancellation = 1 (cubic terms all same sign).
    AccelerationReliability ar(true,       // reliable
                               0.25,       // maxInfluenceFraction
                               0,          // maxInfluenceIndex
                               0,          // nDominant
                               1.0,        // cancellationRatio
                               0.04,       // accel
                               0.045,      // accelWithoutTop
                               0.125,      // accelRelativeChange
                               false,      // accelMaterial
                               true);      // sensitivityOk

    REQUIRE(ar.isReliable()             == true);
    REQUIRE(ar.isAccelMaterial()        == false);
    REQUIRE(ar.isSensitivityOk()        == true);
    REQUIRE(ar.getAccel()               == Catch::Approx(0.04).epsilon(1e-12));
    REQUIRE(ar.getAccelWithoutTop()     == Catch::Approx(0.045).epsilon(1e-12));
    REQUIRE(ar.getAccelRelativeChange() == Catch::Approx(0.125).epsilon(1e-12));
}

TEST_CASE("AccelerationReliability: kDominanceThreshold is 0.5 (diagnostic only)",
          "[AccelerationReliability]")
{
    // Retained for observability and nDominant counting. No longer gates
    // reliability: that is determined by the materiality short-circuit
    // plus the sensitivity test.
    REQUIRE(AccelerationReliability::kDominanceThreshold ==
            Catch::Approx(0.5).epsilon(1e-15));
}

TEST_CASE("AccelerationReliability: kCancellationThreshold is 0.1",
          "[AccelerationReliability]")
{
    REQUIRE(AccelerationReliability::kCancellationThreshold ==
            Catch::Approx(0.1).epsilon(1e-15));
}

TEST_CASE("AccelerationReliability: kAccelMaterialThreshold is 0.10",
          "[AccelerationReliability]")
{
    // Below this |â|, BCa's non-linear correction is numerically
    // negligible (denominator 1 − â·(z₀+zα) stays within [0.84, 1.16]
    // at 95% CI), so reliability is vacuously true regardless of any
    // dominance or sensitivity finding.
    REQUIRE(AccelerationReliability::kAccelMaterialThreshold ==
            Catch::Approx(0.10).epsilon(1e-15));
}

TEST_CASE("AccelerationReliability: kSensitivityThreshold is 0.30",
          "[AccelerationReliability]")
{
    // Maximum allowed relative change in â under single-observation
    // removal before â is considered unstable. Applies only when â is
    // material.
    REQUIRE(AccelerationReliability::kSensitivityThreshold ==
            Catch::Approx(0.30).epsilon(1e-15));
}

TEST_CASE("AccelerationReliability: isCancellationHeavy respects kCancellationThreshold",
          "[AccelerationReliability]")
{
    auto make = [](double cancel) {
        return AccelerationReliability(true, 0.1, 0, 0, cancel,
                                       0.0, 0.0, 0.0, false, true);
    };

    REQUIRE(make(0.1  ).isCancellationHeavy() == false);  // strict <
    REQUIRE(make(0.099).isCancellationHeavy() == true);
    REQUIRE(make(0.9  ).isCancellationHeavy() == false);
    REQUIRE(make(0.0  ).isCancellationHeavy() == true);
    REQUIRE(make(1.0  ).isCancellationHeavy() == false);
}

TEST_CASE("AccelerationReliability: accessors are independent of derived flags",
          "[AccelerationReliability]")
{
    // The constructor stores flags directly; nothing is re-derived from
    // the numeric fields. Verified here by constructing an intentionally
    // inconsistent object — accessors must faithfully report what was
    // stored, even if "reliable=true" would not be derivable from the
    // numeric payload. Guards against a future refactor that accidentally
    // recomputes reliability inside the class.
    AccelerationReliability ar(true,   // reliable   (stored as-is)
                               0.90,   // maxFrac    (would trip old rule)
                               7,      // maxIdx
                               3,      // nDominant  (would also trip)
                               1.0,    // cancellationRatio
                               0.50,   // accel      (material)
                               0.00,   // accelWithoutTop
                               1.00,   // accelRelativeChange (would fail sens)
                               true,   // accelMaterial
                               false); // sensitivityOk

    REQUIRE(ar.isReliable()            == true);   // as stored
    REQUIRE(ar.isAccelMaterial()       == true);
    REQUIRE(ar.isSensitivityOk()       == false);
    REQUIRE(ar.getAccelRelativeChange() == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(ar.getMaxInfluenceFraction() == Catch::Approx(0.90).epsilon(1e-12));
}

// ============================================================================
// JackknifeInfluence::compute() — analytical branch coverage
//
// All inputs below are raw jackknife deviations d_i = jk_avg − jk_i.
// Expected values for â, â_without_top, relChange, maxFrac, and
// cancellationRatio were computed independently and are hard-coded.
// ============================================================================

TEST_CASE("JackknifeInfluence: empty input returns a trivially-reliable result",
          "[JackknifeInfluence]")
{
    auto result = JackknifeInfluence::compute(std::vector<double>{});

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.isAccelMaterial()         == false);
    REQUIRE(result.isSensitivityOk()         == true);
    REQUIRE(result.getAccel()                == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getAccelWithoutTop()      == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()     == false);
}

TEST_CASE("JackknifeInfluence: all-zero d vector returns trivially reliable",
          "[JackknifeInfluence]")
{
    // Σ|d³| ≈ 0 branch: â undefined → treated as 0 → not material → reliable.
    const std::vector<double> d = {0.0, 0.0, 0.0, 0.0};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.isAccelMaterial()         == false);
    REQUIRE(result.getAccel()                == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: single near-zero d value hits Σ|d³|≈0 path",
          "[JackknifeInfluence]")
{
    const std::vector<double> d = {1e-200};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.isAccelMaterial()         == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: uniform positive d gives non-material â=1/(6√n)",
          "[JackknifeInfluence]")
{
    // Analytic result: for d = c·1_n, â = 1/(6·√n) regardless of c.
    // n=4 → â = 1/12 ≈ 0.0833, below kAccelMaterialThreshold=0.10 → reliable.
    const std::vector<double> d = {1.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getAccel()             == Catch::Approx(1.0 / 12.0).epsilon(1e-12));
    REQUIRE(result.isAccelMaterial()      == false);
    REQUIRE(result.isReliable()           == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.25).epsilon(1e-12));
    REQUIRE(result.getNDominant()         == 0);
    REQUIRE(result.getCancellationRatio() == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: materiality short-circuit — dominance fires but |â|<threshold → reliable",
          "[JackknifeInfluence][MaterialityShortCircuit]")
{
    // THIS IS THE CENTRAL NEW BEHAVIOR: the cubic-share dominance metric
    // fires (maxFrac > 0.5), but |â| is too small for the BCa correction to
    // meaningfully affect the interval. The old rule would have rejected;
    // the new rule correctly accepts.
    //
    // d = {3, 1, 1, -1, -1, -1}
    //   Σd²  = 9+1+1+1+1+1 = 14
    //   Σd³  = 27+1+1-1-1-1 = 26
    //   Σ|d³|= 27+1+1+1+1+1 = 32
    //   â    = 26 / (6·14^1.5) ≈ 0.0827    (NOT material: |â|<0.10)
    //   maxFrac = 27/32 = 0.84375          (old rule → unreliable)
    //   reliable = !material (true)        → reliable by short-circuit.
    const std::vector<double> d = {3.0, 1.0, 1.0, -1.0, -1.0, -1.0};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getAccel()             == Catch::Approx(0.082724).margin(1e-4));
    REQUIRE(result.isAccelMaterial()      == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(27.0 / 32.0).epsilon(1e-10));
    REQUIRE(result.getMaxInfluenceIndex() == 0);
    REQUIRE(result.getNDominant()         == 1);   // legacy diagnostic still fires
    REQUIRE(result.isReliable()           == true); // but reliability is true
}

TEST_CASE("JackknifeInfluence: material â driven by one observation → unreliable",
          "[JackknifeInfluence][SensitivityTest]")
{
    // Same pattern as above but scaled so |â| ≥ 0.10.
    // d = {5, 1, 1, -1, -1, -1}
    //   Σd²  = 25+1+1+1+1+1 = 30
    //   Σd³  = 125+1+1-1-1-1 = 124
    //   â    = 124 / (6·30^1.5) ≈ 0.1258   (material)
    //   Removing d[0]=5: d_r = {1,1,-1,-1,-1}
    //     Σd²_r = 5, Σd³_r = -1
    //     â_r = -1 / (6·5^1.5) ≈ -0.0149   (sign flip)
    //   relChange = |0.1258 − (−0.0149)| / 0.1258 ≈ 1.12 > 0.30
    //   → sensitivityOk = false → unreliable.
    const std::vector<double> d = {5.0, 1.0, 1.0, -1.0, -1.0, -1.0};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getAccel()             == Catch::Approx(0.125773).margin(1e-4));
    REQUIRE(result.isAccelMaterial()      == true);
    REQUIRE(result.getAccelWithoutTop()   == Catch::Approx(-0.014907).margin(1e-4));
    REQUIRE(result.getAccelRelativeChange() == Catch::Approx(1.1185).margin(1e-3));
    REQUIRE(result.isSensitivityOk()      == false);
    REQUIRE(result.isReliable()           == false);
    REQUIRE(result.getMaxInfluenceIndex() == 0);
}

TEST_CASE("JackknifeInfluence: material â robust to top-observation removal → reliable",
          "[JackknifeInfluence][SensitivityTest]")
{
    // Smooth, all-positive d: removing the largest barely changes â.
    // d = {1.0, 0.5, 0.3}
    //   Σd²  = 1.34, Σd³ = 1.152
    //   â    = 1.152 / (6·1.34^1.5) ≈ 0.1238     (material)
    //   Removing d[0]=1.0: d_r = {0.5, 0.3}
    //     Σd²_r = 0.34, Σd³_r = 0.152
    //     â_r = 0.152 / (6·0.34^1.5) ≈ 0.1278
    //   relChange = |0.1238 − 0.1278| / 0.1238 ≈ 0.032 ≤ 0.30
    //   → sensitivityOk = true → reliable, despite maxFrac ≈ 0.87.
    //
    // This is the case that demonstrates the sensitivity test is strictly
    // better than the cubic-share proxy: "one observation holds most of
    // the cubic mass" does NOT imply "one observation drives â."
    const std::vector<double> d = {1.0, 0.5, 0.3};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getAccel()             == Catch::Approx(0.123778).margin(1e-4));
    REQUIRE(result.isAccelMaterial()      == true);
    REQUIRE(result.getAccelWithoutTop()   == Catch::Approx(0.127783).margin(1e-4));
    REQUIRE(result.getAccelRelativeChange() == Catch::Approx(0.0324).margin(5e-3));
    REQUIRE(result.isSensitivityOk()      == true);
    REQUIRE(result.isReliable()           == true);
    // Dominance metric still reports the lopsided cubic shares...
    REQUIRE(result.getMaxInfluenceFraction() > 0.5);
    // ...but is no longer a gate.
}

TEST_CASE("JackknifeInfluence: negative dominant value correctly handled via |·|",
          "[JackknifeInfluence]")
{
    // d = {-4, 1, 1, 1}
    //   Σd²  = 16+1+1+1 = 19
    //   Σd³  = -64+1+1+1 = -61
    //   â    = -61 / (6·19^1.5) ≈ -0.1228  (material)
    //   maxFrac = 64/67 ≈ 0.9552; max_idx = 0 (correctly found via |d³|).
    //   Removing d[0]=-4: d_r = {1,1,1}
    //     â_r = 3 / (6·3^1.5) ≈ 0.0962  (sign flip)
    //   relChange ≈ 1.78 > 0.30 → unreliable.
    const std::vector<double> d = {-4.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getAccel()               == Catch::Approx(-0.122758).margin(1e-4));
    REQUIRE(result.isAccelMaterial()        == true);
    REQUIRE(result.getMaxInfluenceIndex()   == 0);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(64.0 / 67.0).epsilon(1e-10));
    REQUIRE(result.getAccelWithoutTop()     == Catch::Approx(0.096225).margin(1e-4));
    REQUIRE(result.getAccelRelativeChange() == Catch::Approx(1.7839).margin(1e-3));
    REQUIRE(result.isSensitivityOk()        == false);
    REQUIRE(result.isReliable()             == false);
    // Partial cancellation: |Σd³|/Σ|d³| = 61/67 ≈ 0.910.
    REQUIRE(result.getCancellationRatio()   == Catch::Approx(61.0 / 67.0).epsilon(1e-10));
    REQUIRE(result.isCancellationHeavy()    == false);
}

TEST_CASE("JackknifeInfluence: heavy cancellation with â≈0 is reliable via short-circuit",
          "[JackknifeInfluence]")
{
    // d = {3, 3, -3, -3, 0.01}
    //   Σd³  ≈ 0 (27+27-27-27 plus negligible)
    //   Σ|d³|= 108.000001
    //   cancellationRatio ≈ 0 → isCancellationHeavy=true
    //   â ≈ 0 → NOT material → reliable (materiality short-circuit).
    //
    // Note: relChange is numerically astronomical here because the denom
    // clamps to 1e-6 while the numerator is small but non-zero. This is
    // harmless — the short-circuit dominates. The test below pins that
    // behavior: even with a huge relChange, reliable=true when â is tiny.
    const std::vector<double> d = {3.0, 3.0, -3.0, -3.0, 0.01};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(std::fabs(result.getAccel())    < 1e-6);
    REQUIRE(result.isAccelMaterial()        == false);
    REQUIRE(result.isCancellationHeavy()    == true);
    REQUIRE(result.isReliable()             == true);  // short-circuit wins
}

TEST_CASE("JackknifeInfluence: maxInfluenceIndex identifies top |d³| with mixed signs",
          "[JackknifeInfluence]")
{
    // d = {1, -2, 3, -0.5}: |d³| = {1, 8, 27, 0.125}. Max at idx=2.
    //   Σd²  = 1+4+9+0.25 = 14.25
    //   Σd³  = 1-8+27-0.125 = 19.875
    //   Σ|d³|= 36.125
    //   â    = 19.875 / (6·14.25^1.5) ≈ 0.0615 — not material, reliable.
    const std::vector<double> d = {1.0, -2.0, 3.0, -0.5};

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getMaxInfluenceIndex()   == 2);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(27.0 / 36.125).epsilon(1e-10));
    REQUIRE(result.isAccelMaterial()        == false);
    REQUIRE(result.isReliable()             == true);
}

TEST_CASE("JackknifeInfluence: nDominant counts cubic-share hits for diagnostics",
          "[JackknifeInfluence]")
{
    // nDominant is diagnostic-only under the new rule; this test pins
    // that it still counts correctly for logging/observability.

    // No dominance: fracs {9/19, 9/19, 1/19}.
    {
        const std::vector<double> d = {std::cbrt(9.0), std::cbrt(9.0), std::cbrt(1.0)};
        auto r = JackknifeInfluence::compute(d);
        REQUIRE(r.getNDominant() == 0);
    }

    // Exactly one dominant contributor: fracs {10/12, 1/12, 1/12}.
    {
        const std::vector<double> d = {std::cbrt(10.0), std::cbrt(1.0), std::cbrt(1.0)};
        auto r = JackknifeInfluence::compute(d);
        REQUIRE(r.getNDominant() == 1);
    }
}

TEST_CASE("JackknifeInfluence: cancellationRatio boundary at kCancellationThreshold",
          "[JackknifeInfluence]")
{
    // Construct d where |Σd³| / Σ|d³| = exactly 0.1.
    // d = {a, b, c} with d³ = {10, -9, 1} → sumSigned=2, sumAbs=20, ratio=0.1.
    const std::vector<double> d = {
        std::cbrt( 10.0),
        std::cbrt( -9.0),
        std::cbrt(  1.0)
    };

    auto result = JackknifeInfluence::compute(d);

    REQUIRE(result.getCancellationRatio() == Catch::Approx(0.1).epsilon(1e-10));
    REQUIRE(result.isCancellationHeavy()  == false);  // strict <
}

// ============================================================================
// BCaBootStrap::getAccelerationReliability() — integration tests
//
// Expected numerical values below are for the arithmetic-mean statistic
// (IID delete-one jackknife) and were verified independently.
// ============================================================================

TEST_CASE("BCaBootStrap::getAccelerationReliability: degenerate all-equal data",
          "[BCaBootStrap][AccelerationReliability]")
{
    // all_equal early-exit path in calculateBCaBounds(): â=0, reliable=true.
    std::vector<DecimalType> returns(10, createDecimal("0.01"));

    BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == true);
    REQUIRE(rel.isAccelMaterial()         == false);
    REQUIRE(rel.getAccel()                == Catch::Approx(0.0).margin(1e-12));
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(rel.getNDominant()            == 0);
    REQUIRE(rel.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(rel.isCancellationHeavy()     == false);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: diffuse-influence data is reliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // 8 values at 0.002–0.008, 4 values at 0.035–0.038. No single
    // observation dominates. Analytically:
    //   mean ≈ 0.0155
    //   â ≈ +0.0327 (not material)
    //   maxFrac ≈ 0.2398 (idx=11, the largest return)
    //   cancellationRatio ≈ 0.5729
    //   relChange ≈ 0.2039 (below 0.30 threshold; also moot since not material)
    std::vector<DecimalType> returns = {
        createDecimal("0.002"), createDecimal("0.003"), createDecimal("0.004"),
        createDecimal("0.005"), createDecimal("0.005"), createDecimal("0.006"),
        createDecimal("0.007"), createDecimal("0.008"),
        createDecimal("0.035"), createDecimal("0.036"),
        createDecimal("0.037"), createDecimal("0.038")
    };

    BCaBootStrap<DecimalType> bca(returns, 1000, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == true);
    REQUIRE(rel.isAccelMaterial()         == false);
    REQUIRE(rel.getAccel()                == Catch::Approx(0.0327).margin(0.002));
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.240).margin(0.01));
    REQUIRE(rel.getNDominant()            == 0);
    REQUIRE(rel.getCancellationRatio()    == Catch::Approx(0.573).margin(0.05));
    REQUIRE(rel.isCancellationHeavy()     == false);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: single extreme outlier is unreliable",
          "[BCaBootStrap][AccelerationReliability][SensitivityTest]")
{
    // Returns = {0.001×4, 1.0}: outlier dwarfs everything.
    //   d[0..3] = -0.04995, d[4] = +0.1998
    //   â ≈ +0.1118   (MATERIAL, |â| ≥ 0.10)
    //   maxFrac ≈ 0.9412 at idx=4
    //   Removing d[4]: â_without_top ≈ -0.0833  (sign flips)
    //   relChange ≈ 1.75 > 0.30 → sensitivityOk=false → unreliable.
    //
    // Confirms that a genuine single-point outlier with material â is
    // still correctly rejected by the new rule.
    std::vector<DecimalType> returns = {
        createDecimal("0.001"), createDecimal("0.001"),
        createDecimal("0.001"), createDecimal("0.001"),
        createDecimal("1.0")
    };

    BCaBootStrap<DecimalType> bca(returns, 1000, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == false);
    REQUIRE(rel.isAccelMaterial()         == true);
    REQUIRE(rel.isSensitivityOk()         == false);
    REQUIRE(rel.getAccel()                == Catch::Approx( 0.1118).margin(1e-3));
    REQUIRE(rel.getAccelWithoutTop()      == Catch::Approx(-0.0833).margin(1e-3));
    REQUIRE(rel.getAccelRelativeChange()  > 1.0);  // ≈ 1.75
    REQUIRE(rel.getMaxInfluenceIndex()    == 4);
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.941).epsilon(0.01));
    REQUIRE(rel.getNDominant()            == 1);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: materiality short-circuit fixes user's reported case",
          "[BCaBootStrap][AccelerationReliability][MaterialityShortCircuit]")
{
    // Real-data analog of the bug the user reported: n=6, a single
    // moderately large return drives the cubic-share metric over 0.5,
    // but the resulting |â| is well below the material threshold, so
    // BCa's correction is negligible and the interval is fine.
    //
    // Returns = {-0.02, -0.01, 0, 0.01, 0.02, 0.05}
    //   mean ≈ 0.00833
    //   â ≈ +0.0447         (NOT material)
    //   maxFrac ≈ 0.70 at idx=5  (old rule → REJECTED; new rule → accepted)
    //
    // Under the OLD fixed-0.5 dominance gate this would have been flagged
    // "dominant jackknife observation" just like the user's production
    // case with a = -0.047. Under the new rule, the materiality
    // short-circuit correctly reports reliable.
    std::vector<DecimalType> returns = {
        createDecimal("-0.02"), createDecimal("-0.01"), createDecimal("0.0"),
        createDecimal("0.01"),  createDecimal("0.02"),  createDecimal("0.05")
    };

    BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
    auto rel = bca.getAccelerationReliability();

    // The key assertions: maxFrac would have tripped the old rule...
    REQUIRE(rel.getMaxInfluenceFraction() > AccelerationReliability::kDominanceThreshold);
    REQUIRE(rel.getNDominant()            >= 1);
    // ...but |â| is below the material threshold...
    REQUIRE(std::fabs(rel.getAccel()) < AccelerationReliability::kAccelMaterialThreshold);
    REQUIRE(rel.isAccelMaterial()         == false);
    // ...so the new rule correctly reports reliable.
    REQUIRE(rel.isReliable()              == true);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: new invariant — reliable iff (!material OR sensitivityOk)",
          "[BCaBootStrap][AccelerationReliability]")
{
    // Under the new rule, this logical invariant must hold by construction.
    // Verified on both a well-behaved and an outlier dataset.

    auto check_invariant = [](const AccelerationReliability& rel) {
        const bool expected = (!rel.isAccelMaterial()) || rel.isSensitivityOk();
        REQUIRE(rel.isReliable() == expected);
    };

    // Reliable dataset
    {
        std::vector<DecimalType> returns = {
            createDecimal("0.002"), createDecimal("0.003"), createDecimal("0.004"),
            createDecimal("0.005"), createDecimal("0.005"), createDecimal("0.006"),
            createDecimal("0.007"), createDecimal("0.008"),
            createDecimal("0.035"), createDecimal("0.036"),
            createDecimal("0.037"), createDecimal("0.038")
        };
        BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
        check_invariant(bca.getAccelerationReliability());
    }

    // Unreliable dataset
    {
        std::vector<DecimalType> returns = {
            createDecimal("0.001"), createDecimal("0.001"),
            createDecimal("0.001"), createDecimal("0.001"),
            createDecimal("1.0")
        };
        BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
        check_invariant(bca.getAccelerationReliability());
    }
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: result is stable across repeated calls",
          "[BCaBootStrap][AccelerationReliability]")
{
    std::vector<DecimalType> returns = {
        createDecimal("0.01"), createDecimal("0.02"), createDecimal("-0.01"),
        createDecimal("0.03"), createDecimal("-0.02"), createDecimal("0.015"),
        createDecimal("0.00"), createDecimal("0.025"), createDecimal("-0.005"),
        createDecimal("0.018")
    };

    BCaBootStrap<DecimalType> bca(returns, 1000, 0.95);

    auto rel1 = bca.getAccelerationReliability();
    auto rel2 = bca.getAccelerationReliability();
    auto rel3 = bca.getAccelerationReliability();

    // Core reliability + legacy diagnostics
    REQUIRE(rel1.isReliable()              == rel2.isReliable());
    REQUIRE(rel1.isReliable()              == rel3.isReliable());
    REQUIRE(rel1.getMaxInfluenceFraction() ==
            Catch::Approx(rel2.getMaxInfluenceFraction()).epsilon(1e-15));
    REQUIRE(rel1.getMaxInfluenceIndex()    == rel2.getMaxInfluenceIndex());
    REQUIRE(rel1.getNDominant()            == rel2.getNDominant());
    REQUIRE(rel1.getCancellationRatio()    ==
            Catch::Approx(rel2.getCancellationRatio()).epsilon(1e-15));

    // New fields must also be stable
    REQUIRE(rel1.getAccel()                ==
            Catch::Approx(rel2.getAccel()).epsilon(1e-15));
    REQUIRE(rel1.getAccelWithoutTop()      ==
            Catch::Approx(rel3.getAccelWithoutTop()).epsilon(1e-15));
    REQUIRE(rel1.getAccelRelativeChange()  ==
            Catch::Approx(rel2.getAccelRelativeChange()).epsilon(1e-15));
    REQUIRE(rel1.isAccelMaterial()         == rel2.isAccelMaterial());
    REQUIRE(rel1.isSensitivityOk()         == rel3.isSensitivityOk());
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: larger well-behaved dataset is reliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // n=50 with a realistic mix of small positive and negative returns.
    // Analytically: â ≈ -0.035 (not material), maxFrac ≈ 0.159.
    std::vector<DecimalType> returns;
    returns.reserve(50);
    for (int i = 0; i < 50; ++i)
    {
        if (i % 5 == 0)
            returns.push_back(createDecimal("-0.020") + DecimalType(i) / DecimalType(5000));
        else
            returns.push_back(createDecimal("0.008")  + DecimalType(i) / DecimalType(10000));
    }

    BCaBootStrap<DecimalType> bca(returns, 1000, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == true);
    REQUIRE(rel.isAccelMaterial()         == false);
    REQUIRE(rel.getNDominant()            == 0);
    REQUIRE(rel.getMaxInfluenceFraction() < 0.3);

    // The "a_wo" and "relChange" fields are computed from the (n-1)-sized
    // jackknife with the top contributor removed; still finite and sensible.
    REQUIRE(std::isfinite(rel.getAccelWithoutTop()));
    REQUIRE(std::isfinite(rel.getAccelRelativeChange()));

    // Sanity on dominance/cancellation diagnostics
    REQUIRE(rel.getCancellationRatio() >= 0.0);
    REQUIRE(rel.getCancellationRatio() <= 1.0);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: StationaryBlockResampler path",
          "[BCaBootStrap][AccelerationReliability][Stationary]")
{
    // Verify the reliability diagnostic works through the stationary-block
    // jackknife (delete-block, not delete-one). With L=3 and n=60 the
    // block means are all equal (each block contains {0.004, 0.004, -0.003}),
    // so d ≈ 0 → â ≈ 0 → not material → reliable.
    using Policy = StationaryBlockResampler<DecimalType>;

    std::vector<DecimalType> returns;
    for (int k = 0; k < 20; ++k)
    {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
    }

    Policy pol(3);
    BCaBootStrap<DecimalType, Policy> bca(returns, 1000, 0.95,
                                          &StatUtils<DecimalType>::computeMean,
                                          pol);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == true);
    REQUIRE(rel.isAccelMaterial()         == false);
    REQUIRE(rel.getNDominant()            == 0);
    REQUIRE(rel.getMaxInfluenceFraction() < 0.5);

    // Diagnostic fields self-consistent
    REQUIRE(std::isfinite(rel.getMaxInfluenceFraction()));
    REQUIRE(rel.getMaxInfluenceFraction() >= 0.0);
    REQUIRE(std::isfinite(rel.getCancellationRatio()));
    REQUIRE(rel.getCancellationRatio() >= 0.0);
    REQUIRE(rel.getCancellationRatio() <= 1.0);
}
