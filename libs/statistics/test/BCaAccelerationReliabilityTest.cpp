// BCaAccelerationReliabilityTest.cpp
//
// Unit tests for:
//   - AccelerationReliability  (construction, accessors, thresholds)
//   - JackknifeInfluence::compute()  (all logical branches)
//   - BCaBootStrap::getAccelerationReliability()  (integration with BCa pipeline)
//
// All JackknifeInfluence tests use analytically computed expected values to
// ensure determinism. Expected values were verified independently in Python.
//
// The denominator for dominance fractions is Σ|d³| (total absolute cubic
// influence), NOT |Σd³| (absolute value of the net signed sum). This means:
//   - All fractions are always in [0, 1]
//   - Near-cancellation of signed cubic terms does NOT inflate fractions
//   - Near-cancellation is separately measured by the cancellation ratio
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

TEST_CASE("AccelerationReliability: constructor stores all values correctly",
          "[AccelerationReliability]")
{
    const bool        reliable          = false;
    const double      maxFrac           = 0.7143;
    const std::size_t maxIdx            = 2;
    const std::size_t nDominant         = 1;
    const double      cancellationRatio = 0.75;

    AccelerationReliability ar(reliable, maxFrac, maxIdx, nDominant, cancellationRatio);

    REQUIRE(ar.isReliable()              == reliable);
    REQUIRE(ar.getMaxInfluenceFraction() == Catch::Approx(maxFrac).epsilon(1e-12));
    REQUIRE(ar.getMaxInfluenceIndex()    == maxIdx);
    REQUIRE(ar.getNDominant()            == nDominant);
    REQUIRE(ar.getCancellationRatio()    == Catch::Approx(cancellationRatio).epsilon(1e-12));
}

TEST_CASE("AccelerationReliability: reliable=true variant stores correctly",
          "[AccelerationReliability]")
{
    // cancellationRatio=1.0 means cubic terms fully agree in sign (well-conditioned)
    AccelerationReliability ar(true, 0.25, 0, 0, 1.0);

    REQUIRE(ar.isReliable()              == true);
    REQUIRE(ar.getMaxInfluenceFraction() == Catch::Approx(0.25).epsilon(1e-12));
    REQUIRE(ar.getMaxInfluenceIndex()    == 0);
    REQUIRE(ar.getNDominant()            == 0);
    REQUIRE(ar.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("AccelerationReliability: kDominanceThreshold is 0.5",
          "[AccelerationReliability]")
{
    // Verifying the value here documents and guards against inadvertent changes.
    REQUIRE(AccelerationReliability::kDominanceThreshold ==
            Catch::Approx(0.5).epsilon(1e-15));
}

TEST_CASE("AccelerationReliability: kCancellationThreshold is 0.1",
          "[AccelerationReliability]")
{
    // Verifying the value here documents and guards against inadvertent changes.
    REQUIRE(AccelerationReliability::kCancellationThreshold ==
            Catch::Approx(0.1).epsilon(1e-15));
}

TEST_CASE("AccelerationReliability: isCancellationHeavy respects kCancellationThreshold",
          "[AccelerationReliability]")
{
    // Exactly at threshold: 0.1 is NOT < 0.1, so NOT heavy
    AccelerationReliability at_threshold(true, 0.1, 0, 0, 0.1);
    REQUIRE(at_threshold.isCancellationHeavy() == false);

    // Just below threshold: heavy cancellation
    AccelerationReliability just_below(true, 0.1, 0, 0, 0.099);
    REQUIRE(just_below.isCancellationHeavy() == true);

    // Well above threshold: not heavy
    AccelerationReliability high_ratio(true, 0.1, 0, 0, 0.9);
    REQUIRE(high_ratio.isCancellationHeavy() == false);

    // Zero ratio: maximum cancellation
    AccelerationReliability zero_ratio(true, 0.0, 0, 0, 0.0);
    REQUIRE(zero_ratio.isCancellationHeavy() == true);

    // Ratio of 1.0: no cancellation at all
    AccelerationReliability full_ratio(true, 0.1, 0, 0, 1.0);
    REQUIRE(full_ratio.isCancellationHeavy() == false);
}

// ============================================================================
// JackknifeInfluence::compute() — analytical branch coverage
// ============================================================================

TEST_CASE("JackknifeInfluence: empty input returns reliable with no signal",
          "[JackknifeInfluence]")
{
    // Empty dCubed: no jackknife information. No dominance possible.
    // cancellationRatio=1.0 by convention.
    const std::vector<double> dCubed;

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()     == false);
}

TEST_CASE("JackknifeInfluence: negligible sumAbsCubic (all zeros) returns reliable",
          "[JackknifeInfluence]")
{
    // All dCubed values zero: sumAbsCubic ≤ 1e-100.
    // â ≈ 0 → BCa degenerates to plain BC. No dominance possible.
    // cancellationRatio=1.0 by convention (no signal to cancel).
    const std::vector<double> dCubed = {0.0, 0.0, 0.0, 0.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: single near-zero value returns reliable",
          "[JackknifeInfluence]")
{
    // Single element that is effectively zero — hits the negligible sumAbsCubic path.
    const std::vector<double> dCubed = {1e-200};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: uniform positive influence is reliable",
          "[JackknifeInfluence]")
{
    // dCubed = {1, 1, 1, 1}: sumAbsCubic=4, each frac=1/4=0.25 < 0.5
    // All same sign → cancellationRatio = |4| / 4 = 1.0 (no cancellation)
    const std::vector<double> dCubed = {1.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.25).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()     == false);
}

TEST_CASE("JackknifeInfluence: one dominant observation (4/7 > 0.5) is unreliable",
          "[JackknifeInfluence]")
{
    // dCubed = {4, 1, 1, 1}: sumAbsCubic=7
    // frac[0]=4/7≈0.5714, frac[1..3]=1/7≈0.1429
    // All same sign → cancellationRatio = |7|/7 = 1.0
    const std::vector<double> dCubed = {4.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(4.0 / 7.0).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 1);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()     == false);
}

TEST_CASE("JackknifeInfluence: borderline frac exactly 0.5 is treated as reliable",
          "[JackknifeInfluence]")
{
    // dCubed = {1, 1}: each frac = 1/2 = 0.5 exactly.
    // Dominance condition is strictly > 0.5, so 0.5 is NOT dominant.
    // Reliable=true, nDominant=0. cancellationRatio=1.0 (same sign).
    const std::vector<double> dCubed = {1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.5).epsilon(1e-12));
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: maxInfluenceIndex identifies correct observation",
          "[JackknifeInfluence]")
{
    // dCubed = {1, 2, 8, 3}: sumAbsCubic=14, all positive
    // fracs = {1/14, 2/14, 8/14, 3/14} = {0.0714, 0.1429, 0.5714, 0.2143}
    // Dominant at index 2. cancellationRatio=1.0 (all same sign).
    const std::vector<double> dCubed = {1.0, 2.0, 8.0, 3.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceIndex()    == 2);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(8.0 / 14.0).epsilon(1e-12));
    REQUIRE(result.getNDominant()            == 1);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("JackknifeInfluence: negative dominant value detected via abs()",
          "[JackknifeInfluence]")
{
    // dCubed = {-4, 1, 1, 1}: sumAbsCubic = 4+1+1+1 = 7
    //
    // Under the CORRECT absolute-sum denominator:
    //   frac[0] = 4/7 ≈ 0.571 > 0.5  →  dominant
    //   frac[1..3] = 1/7 ≈ 0.143 < 0.5  →  not dominant
    //   → nDominant=1, reliable=false
    //
    // NOTE: the signed sum is -4+1+1+1 = -1, so:
    //   cancellationRatio = |-1| / 7 ≈ 0.143  (partial cancellation)
    //
    // This is different from the old buggy behavior which used |signed sum|=1
    // as denominator, giving fracs {4.0, 1.0, 1.0, 1.0} — all exceeding 1.0.
    // The absolute-sum denominator correctly identifies only the negative
    // observation as dominant, since it alone exceeds 50% of total abs influence.
    const std::vector<double> dCubed = {-4.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(4.0 / 7.0).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 1);
    // cancellationRatio = |-1| / 7 ≈ 0.143 — partial cancellation but not heavy
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0 / 7.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()     == false);
}

TEST_CASE("JackknifeInfluence: near-cancellation detected by cancellation ratio",
          "[JackknifeInfluence]")
{
    // dCubed = {3, 3, -5}: sumAbsCubic = 3+3+5 = 11, sumSigned = 3+3-5 = 1
    //
    // Under the CORRECT absolute-sum denominator:
    //   frac[0] = 3/11 ≈ 0.273, frac[1] = 3/11 ≈ 0.273, frac[2] = 5/11 ≈ 0.455
    //   No fraction exceeds 0.5 → reliable=true, nDominant=0
    //
    // cancellationRatio = |1| / 11 ≈ 0.091 < kCancellationThreshold=0.1
    //   → isCancellationHeavy=true
    //
    // This case illustrates the key design improvement: the old buggy denominator
    // (|signed sum| = 1) would have given fracs {3.0, 3.0, 5.0}, all exceeding 1.0,
    // and incorrectly labelled this as dominated. The corrected implementation
    // correctly identifies that no single observation dominates — the signed sum
    // is small because of cancellation among several comparable contributions,
    // which is a separate diagnostic (isCancellationHeavy) not a dominance failure.
    const std::vector<double> dCubed = {3.0, 3.0, -5.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getNDominant()            == 0);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(5.0 / 11.0).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 2);
    // cancellationRatio = |1|/11 ≈ 0.091 → heavy cancellation
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0 / 11.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()     == true);
}

TEST_CASE("JackknifeInfluence: well-conditioned positive data has cancellationRatio near 1",
          "[JackknifeInfluence]")
{
    // All same sign → signed sum equals absolute sum → ratio = 1.0
    // {5, 3, 7, 2, 4}: sumAbsCubic = 21, sumSigned = 21, ratio = 1.0
    const std::vector<double> dCubed = {5.0, 3.0, 7.0, 2.0, 4.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.getCancellationRatio() == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()  == false);
    // maxFrac = 7/21 ≈ 0.333, reliable
    REQUIRE(result.isReliable()           == true);
}

TEST_CASE("JackknifeInfluence: cancellationRatio boundary at kCancellationThreshold",
          "[JackknifeInfluence]")
{
    // Construct dCubed where |sumSigned| / sumAbsCubic = exactly 0.1
    // {10, -9, 1}: sumSigned=2, sumAbs=20, ratio=2/20=0.1
    // ratio = 0.1 is NOT < 0.1, so isCancellationHeavy=false (strict <)
    const std::vector<double> dCubed = {10.0, -9.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.getCancellationRatio() == Catch::Approx(0.1).epsilon(1e-12));
    REQUIRE(result.isCancellationHeavy()  == false);  // exactly at threshold, NOT heavy
    // frac[0] = 10/20 = 0.5 → NOT > 0.5, reliable
    REQUIRE(result.isReliable()           == true);
}

TEST_CASE("JackknifeInfluence: nDominant counts all observations above threshold",
          "[JackknifeInfluence]")
{
    // dCubed = {6, 5, 4, 1}: sumAbsCubic=16, all positive
    // fracs = {6/16, 5/16, 4/16, 1/16} = {0.375, 0.3125, 0.25, 0.0625}
    // None exceed 0.5 → reliable=true, nDominant=0
    {
        const std::vector<double> dCubed = {6.0, 5.0, 4.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
        REQUIRE(result.getCancellationRatio() == Catch::Approx(1.0).epsilon(1e-12));
    }

    // dCubed = {9, 9, 1}: sumAbsCubic=19
    // fracs = {9/19, 9/19, 1/19} ≈ {0.4737, 0.4737, 0.0526}
    // None exceed 0.5 → reliable=true, nDominant=0
    {
        const std::vector<double> dCubed = {9.0, 9.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // dCubed = {10, 10, 1}: sumAbsCubic=21
    // fracs = {10/21, 10/21, 1/21} ≈ {0.4762, 0.4762, 0.0476}
    // None exceed 0.5 → nDominant=0
    {
        const std::vector<double> dCubed = {10.0, 10.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // dCubed = {11, 11, 1}: sumAbsCubic=23
    // fracs = {11/23, 11/23, 1/23} ≈ {0.4783, 0.4783, 0.0435}
    // Still none > 0.5 → nDominant=0
    {
        const std::vector<double> dCubed = {11.0, 11.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // dCubed = {4, 4, -7}: sumAbsCubic=15, sumSigned=1
    // fracs = {4/15, 4/15, 7/15} ≈ {0.267, 0.267, 0.467}
    // None exceed 0.5 → reliable=true, nDominant=0
    // But cancellationRatio = |1|/15 ≈ 0.067 → isCancellationHeavy=true
    // NOTE: the old buggy denominator (|sumSigned|=1) gave fracs {4,4,7} — all > 0.5.
    // The corrected implementation correctly identifies no dominance.
    {
        const std::vector<double> dCubed = {4.0, 4.0, -7.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()           == true);
        REQUIRE(result.getNDominant()         == 0);
        REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(7.0 / 15.0).epsilon(1e-12));
        REQUIRE(result.getCancellationRatio() == Catch::Approx(1.0 / 15.0).epsilon(1e-12));
        REQUIRE(result.isCancellationHeavy()  == true);
    }
}

TEST_CASE("JackknifeInfluence: single dominant element in a longer vector",
          "[JackknifeInfluence]")
{
    // Build a vector of 20 near-equal values with one dominant outlier at index 12.
    // dCubed[0..11,13..19] = 0.01 (19 values at 0.01)
    // dCubed[12] = 10.0
    // sumAbsCubic = 19 * 0.01 + 10.0 = 10.19  (all positive, equals signed sum)
    // frac[12] = 10.0 / 10.19 ≈ 0.9814 > 0.5 → dominant
    // cancellationRatio = 10.19 / 10.19 = 1.0 (all positive, no cancellation)
    std::vector<double> dCubed(20, 0.01);
    dCubed[12] = 10.0;

    const double sumAbsCubic = 19 * 0.01 + 10.0; // = 10.19
    const double frac12      = 10.0 / sumAbsCubic;
    const double fracRest    = 0.01 / sumAbsCubic;

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceIndex()    == 12);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(frac12).epsilon(1e-10));
    REQUIRE(result.getNDominant()            == 1);
    REQUIRE(result.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-10));
    REQUIRE(result.isCancellationHeavy()     == false);

    // All non-dominant fractions are far below threshold
    REQUIRE(fracRest < 0.01);
}

// ============================================================================
// BCaBootStrap::getAccelerationReliability() — integration tests
// ============================================================================

TEST_CASE("BCaBootStrap::getAccelerationReliability: degenerate all-equal data",
          "[BCaBootStrap][AccelerationReliability]")
{
    // All returns identical → all_equal early-exit path in calculateBCaBounds().
    // â = 0, all dCubed = 0 → sumAbsCubic negligible → reliable=true.
    // cancellationRatio = 1.0 by convention.
    std::vector<DecimalType> returns(10, createDecimal("0.01"));

    BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == true);
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(rel.getNDominant()            == 0);
    REQUIRE(rel.getCancellationRatio()    == Catch::Approx(1.0).epsilon(1e-12));
    REQUIRE(rel.isCancellationHeavy()     == false);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: diffuse influence data is reliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // DESIGN RATIONALE:
    // For the arithmetic mean statistic, d[i] = (returns[i] - mean) / (n-1),
    // so the dominance fraction simplifies to:
    //
    //   frac[i] = |returns[i] - mean|³ / Σ_j |returns[j] - mean|³
    //
    // The denominator is the SUM OF ABSOLUTE VALUES of cubed deviations —
    // NOT |Σ(returns[j] - mean)³|. This ensures fracs are always in [0,1]
    // and near-cancellation does not inflate them.
    //
    // reliable=true requires max frac[i] ≤ 0.5. This is achieved by a
    // positively skewed distribution where no single observation dominates
    // the total absolute cubic influence.
    //
    // This dataset is analytically verified under the CORRECTED absolute-sum
    // denominator (Σ|d³|, not |Σd³|):
    //   8 values at 0.002–0.008, 4 values at 0.035–0.038
    //   mean ≈ 0.0155
    //
    //   dCubed signs: small-cluster observations are below the mean, so
    //   removing them raises the LOO mean → d[i] < 0 → dCubed[i] < 0.
    //   Large-cluster observations are above the mean → dCubed[i] > 0.
    //   The cubic terms have MIXED SIGNS — this is why the denominator
    //   choice matters: Σ|d³| > |Σd³|, so all fractions are smaller
    //   under the corrected denominator than under the old buggy one.
    //
    //   Verified in Python (absolute-sum denominator):
    //     sumSigned ≈ 2.04e-8, sumAbsCubic ≈ 3.57e-8
    //     fracs (i=0..7):  0.052, 0.041, 0.032, 0.024, 0.024, 0.018, 0.013, 0.009
    //     fracs (i=8..11): 0.156, 0.181, 0.209, 0.240
    //     maxFrac ≈ 0.240 at idx=11 < 0.5  →  reliable=true, nDominant=0
    //     cancellationRatio = |2.04e-8| / 3.57e-8 ≈ 0.573
    //       (partial cancellation between negative small and positive large cubics)
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
    REQUIRE(rel.getMaxInfluenceFraction()  < 0.5);
    REQUIRE(rel.getNDominant()            == 0);
    // Analytically: maxFrac ≈ 0.240 (corrected from old 0.419 which used |Σd³| as denominator)
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.240).margin(0.01));
    // Partial cancellation between negative (small returns) and positive
    // (large returns) cubic contributions → cancellationRatio ≈ 0.573
    REQUIRE(rel.getCancellationRatio()    == Catch::Approx(0.573).margin(0.05));
    REQUIRE(rel.isCancellationHeavy()     == false);  // 0.573 >> kCancellationThreshold
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: single extreme outlier is unreliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // Returns = {0.001 x4, 1.0}: one observation ~1000x larger than the rest.
    //
    // IID delete-one jackknife on mean:
    //   jk[0..3] = (4 * 0.001 + 1.0 - 0.001) / 4 = 1.003/4 = 0.25075
    //   jk[4]    = (4 * 0.001) / 4               = 0.001
    //   jk_avg                                    = 0.2008
    //
    //   d[0..3] = 0.2008 - 0.25075 = -0.04995
    //   d[4]    = 0.2008 - 0.001   =  0.1998
    //
    //   dCubed[0..3] ≈ -0.0001246 each
    //   dCubed[4]    ≈  0.007976
    //
    //   sumAbsCubic = 4 * 0.0001246 + 0.007976 = 0.008474
    //   frac[4] = 0.007976 / 0.008474 ≈ 0.941 > 0.5  →  dominant
    //
    //   cancellationRatio = |sumSigned| / sumAbsCubic
    //                     = |0.007477| / 0.008474 ≈ 0.882  (not heavy cancellation)
    //
    // Under the OLD buggy denominator (|sumSigned| = 0.007477):
    //   frac[4] = 0.007976 / 0.007477 ≈ 1.067 — exceeded 1.0, showing the bug.
    // Under the CORRECT absolute-sum denominator:
    //   frac[4] ≈ 0.941 — still dominant, correctly identified.
    std::vector<DecimalType> returns = {
        createDecimal("0.001"), createDecimal("0.001"),
        createDecimal("0.001"), createDecimal("0.001"),
        createDecimal("1.0")
    };

    BCaBootStrap<DecimalType> bca(returns, 1000, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()           == false);
    REQUIRE(rel.getMaxInfluenceIndex() == 4);      // the outlier observation
    REQUIRE(rel.getNDominant()         == 1);
    // frac ≈ 0.941: dominant, well above threshold, bounded in [0,1]
    REQUIRE(rel.getMaxInfluenceFraction() > 0.5);
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.941).epsilon(0.01));
    // Not heavy cancellation — outlier and regular observations have opposite-sign
    // cubic contributions, but the outlier's magnitude is so dominant that the
    // signed sum is still large relative to the absolute sum.
    REQUIRE(rel.getCancellationRatio()    > 0.5);
    REQUIRE(rel.isCancellationHeavy()     == false);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: isReliable is consistent with maxInfluenceFraction",
          "[BCaBootStrap][AccelerationReliability]")
{
    // Structural invariant: isReliable() == (maxInfluenceFraction <= kDominanceThreshold).
    const double threshold = AccelerationReliability::kDominanceThreshold;

    // Reliable case
    {
        std::vector<DecimalType> returns = {
            createDecimal("0.002"), createDecimal("0.003"), createDecimal("0.004"),
            createDecimal("0.005"), createDecimal("0.005"), createDecimal("0.006"),
            createDecimal("0.007"), createDecimal("0.008"),
            createDecimal("0.035"), createDecimal("0.036"),
            createDecimal("0.037"), createDecimal("0.038")
        };
        BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
        auto rel = bca.getAccelerationReliability();

        if (rel.getMaxInfluenceFraction() <= threshold)
            REQUIRE(rel.isReliable() == true);
        else
            REQUIRE(rel.isReliable() == false);
    }

    // Unreliable case (outlier-driven)
    {
        std::vector<DecimalType> returns = {
            createDecimal("0.001"), createDecimal("0.001"),
            createDecimal("0.001"), createDecimal("0.001"),
            createDecimal("1.0")
        };
        BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
        auto rel = bca.getAccelerationReliability();

        if (rel.getMaxInfluenceFraction() <= threshold)
            REQUIRE(rel.isReliable() == true);
        else
            REQUIRE(rel.isReliable() == false);
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

    REQUIRE(rel1.isReliable()              == rel2.isReliable());
    REQUIRE(rel1.isReliable()              == rel3.isReliable());
    REQUIRE(rel1.getMaxInfluenceFraction() ==
            Catch::Approx(rel2.getMaxInfluenceFraction()).epsilon(1e-15));
    REQUIRE(rel1.getMaxInfluenceFraction() ==
            Catch::Approx(rel3.getMaxInfluenceFraction()).epsilon(1e-15));
    REQUIRE(rel1.getMaxInfluenceIndex()    == rel2.getMaxInfluenceIndex());
    REQUIRE(rel1.getMaxInfluenceIndex()    == rel3.getMaxInfluenceIndex());
    REQUIRE(rel1.getNDominant()            == rel2.getNDominant());
    REQUIRE(rel1.getNDominant()            == rel3.getNDominant());
    REQUIRE(rel1.getCancellationRatio()    ==
            Catch::Approx(rel2.getCancellationRatio()).epsilon(1e-15));
    REQUIRE(rel1.getCancellationRatio()    ==
            Catch::Approx(rel3.getCancellationRatio()).epsilon(1e-15));
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: larger well-behaved dataset is reliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // n=50 with a realistic mix of small positive and negative returns.
    // No single observation should dominate the total absolute cubic influence.
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

    REQUIRE(rel.isReliable()   == true);
    REQUIRE(rel.getNDominant() == 0);
    // With 50 observations, max fraction should be well below 0.5
    REQUIRE(rel.getMaxInfluenceFraction() < 0.3);
    // cancellationRatio is finite and in [0,1]
    REQUIRE(std::isfinite(rel.getCancellationRatio()));
    REQUIRE(rel.getCancellationRatio() >= 0.0);
    REQUIRE(rel.getCancellationRatio() <= 1.0);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: StationaryBlockResampler path",
          "[BCaBootStrap][AccelerationReliability][Stationary]")
{
    // Verify the reliability diagnostic works through the StationaryBlockResampler
    // path (block jackknife, not delete-one).
    using Policy = StationaryBlockResampler<DecimalType>;

    std::vector<DecimalType> returns;
    for (int k = 0; k < 20; ++k)
    {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
    }
    // n=60, mild autocorrelation pattern

    Policy pol(3);
    BCaBootStrap<DecimalType, Policy> bca(returns, 1000, 0.95,
                                          &StatUtils<DecimalType>::computeMean,
                                          pol);
    auto rel = bca.getAccelerationReliability();

    // Block jackknife: floor(60/3)=20 pseudo-values.
    // Each block contains identical content {0.004, 0.004, -0.003}, so all
    // block means are equal → d=0 → dCubed=0 → negligible sumAbsCubic → reliable.
    REQUIRE(rel.isReliable()   == true);
    REQUIRE(rel.getNDominant() == 0);
    REQUIRE(rel.getMaxInfluenceFraction() < 0.5);

    // Diagnostic fields are self-consistent
    REQUIRE(std::isfinite(rel.getMaxInfluenceFraction()));
    REQUIRE(rel.getMaxInfluenceFraction() >= 0.0);
    REQUIRE(std::isfinite(rel.getCancellationRatio()));
    REQUIRE(rel.getCancellationRatio() >= 0.0);
    REQUIRE(rel.getCancellationRatio() <= 1.0);
}
