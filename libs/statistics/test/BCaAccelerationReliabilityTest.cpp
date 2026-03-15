// AccelerationReliabilityTest.cpp
//
// Unit tests for:
//   - AccelerationReliability  (construction, accessors, kDominanceThreshold)
//   - JackknifeInfluence::compute()  (all logical branches)
//   - BCaBootStrap::getAccelerationReliability()  (integration with BCa pipeline)
//
// All JackknifeInfluence tests use analytically computed expected values to
// ensure determinism. Expected fractions were verified independently in Python.
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
    const bool        reliable  = false;
    const double      maxFrac   = 0.7143;
    const std::size_t maxIdx    = 2;
    const std::size_t nDominant = 1;

    AccelerationReliability ar(reliable, maxFrac, maxIdx, nDominant);

    REQUIRE(ar.isReliable()              == reliable);
    REQUIRE(ar.getMaxInfluenceFraction() == Catch::Approx(maxFrac).epsilon(1e-12));
    REQUIRE(ar.getMaxInfluenceIndex()    == maxIdx);
    REQUIRE(ar.getNDominant()            == nDominant);
}

TEST_CASE("AccelerationReliability: reliable=true variant stores correctly",
          "[AccelerationReliability]")
{
    AccelerationReliability ar(true, 0.25, 0, 0);

    REQUIRE(ar.isReliable()              == true);
    REQUIRE(ar.getMaxInfluenceFraction() == Catch::Approx(0.25).epsilon(1e-12));
    REQUIRE(ar.getMaxInfluenceIndex()    == 0);
    REQUIRE(ar.getNDominant()            == 0);
}

TEST_CASE("AccelerationReliability: kDominanceThreshold is 0.5",
          "[AccelerationReliability]")
{
    // The threshold is the boundary between reliable and unreliable.
    // Verifying its value here documents and guards against inadvertent changes.
    REQUIRE(AccelerationReliability::kDominanceThreshold == Catch::Approx(0.5).epsilon(1e-15));
}

// ============================================================================
// JackknifeInfluence::compute() — analytical branch coverage
// ============================================================================

TEST_CASE("JackknifeInfluence: negligible numerator (all zeros) returns reliable",
          "[JackknifeInfluence]")
{
    // When all dCubed values are zero, |Σd³| ≤ 1e-100.
    // â ≈ 0 → BCa degenerates to plain BC.
    // Reliable by definition: no observation can "dominate" a zero sum.
    const std::vector<double> dCubed = {0.0, 0.0, 0.0, 0.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 0);
}

TEST_CASE("JackknifeInfluence: single near-zero value returns reliable",
          "[JackknifeInfluence]")
{
    // Single element that is effectively zero — still hits the negligible path.
    const std::vector<double> dCubed = {1e-200};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(result.getNDominant()            == 0);
}

TEST_CASE("JackknifeInfluence: uniform positive influence is reliable",
          "[JackknifeInfluence]")
{
    // dCubed = {1, 1, 1, 1}: sumD=4, each frac=0.25 < 0.5
    // Analytically: maxFrac=0.25, nDominant=0, reliable=true
    const std::vector<double> dCubed = {1.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.25).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 0); // first element with max frac
    REQUIRE(result.getNDominant()            == 0);
}

TEST_CASE("JackknifeInfluence: one dominant observation (4/7 > 0.5) is unreliable",
          "[JackknifeInfluence]")
{
    // dCubed = {4, 1, 1, 1}: sumD=7
    // frac[0]=4/7≈0.5714, frac[1..3]=1/7≈0.1429
    // Analytically: maxFrac=4/7, maxIdx=0, nDominant=1, reliable=false
    const std::vector<double> dCubed = {4.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(4.0 / 7.0).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 1);
}

TEST_CASE("JackknifeInfluence: borderline frac exactly 0.5 is treated as reliable",
          "[JackknifeInfluence]")
{
    // dCubed = {1, 1}: each frac=0.5 exactly.
    // Dominance condition is strictly > 0.5, so 0.5 is NOT dominant.
    // Reliable=true, nDominant=0 — boundary condition test.
    const std::vector<double> dCubed = {1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == true);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(0.5).epsilon(1e-12));
    REQUIRE(result.getNDominant()            == 0);
}

TEST_CASE("JackknifeInfluence: maxInfluenceIndex identifies correct observation",
          "[JackknifeInfluence]")
{
    // dCubed = {1, 2, 8, 3}: sumD=14
    // fracs = {1/14, 2/14, 8/14, 3/14} = {0.0714, 0.1429, 0.5714, 0.2143}
    // Dominant observation is at index 2 (frac=8/14≈0.5714)
    const std::vector<double> dCubed = {1.0, 2.0, 8.0, 3.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceIndex()    == 2);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(8.0 / 14.0).epsilon(1e-12));
    REQUIRE(result.getNDominant()            == 1);
}

TEST_CASE("JackknifeInfluence: negative dominant value detected via abs()",
          "[JackknifeInfluence]")
{
    // dCubed = {-4, 1, 1, 1}: sumD=-1, absNumD=1
    // fracs = {4/1, 1/1, 1/1, 1/1} = {4.0, 1.0, 1.0, 1.0}
    // All fracs > 0.5 → nDominant=4, maxFrac=4.0, maxIdx=0, reliable=false
    // This verifies that std::fabs() is applied to each dCubed[i] in compute().
    const std::vector<double> dCubed = {-4.0, 1.0, 1.0, 1.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(4.0).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 0);
    REQUIRE(result.getNDominant()            == 4);
}

TEST_CASE("JackknifeInfluence: canceling positive and negative terms inflate fractions",
          "[JackknifeInfluence]")
{
    // dCubed = {3, 3, -5}: sumD=1, absNumD=1
    // fracs = {3/1, 3/1, 5/1} = {3.0, 3.0, 5.0}
    // This pathological case arises when LOO cubic contributions nearly cancel.
    // Individual |d³| values can legitimately exceed |Σd³|, giving fracs > 1.
    // All three observations exceed the threshold → nDominant=3, reliable=false.
    // maxFrac=5.0 at index 2.
    const std::vector<double> dCubed = {3.0, 3.0, -5.0};

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(5.0).epsilon(1e-12));
    REQUIRE(result.getMaxInfluenceIndex()    == 2);
    REQUIRE(result.getNDominant()            == 3);
}

TEST_CASE("JackknifeInfluence: nDominant counts all observations above threshold",
          "[JackknifeInfluence]")
{
    // dCubed = {6, 5, 4, 1}: sumD=16
    // fracs = {6/16, 5/16, 4/16, 1/16} = {0.375, 0.3125, 0.25, 0.0625}
    // None exceed 0.5 → reliable=true, nDominant=0
    {
        const std::vector<double> dCubed = {6.0, 5.0, 4.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // dCubed = {9, 9, 1}: sumD=19
    // fracs = {9/19, 9/19, 1/19} ≈ {0.4737, 0.4737, 0.0526}
    // None exceed 0.5 → reliable=true, nDominant=0
    {
        const std::vector<double> dCubed = {9.0, 9.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // dCubed = {10, 10, 1}: sumD=21
    // fracs = {10/21, 10/21, 1/21} ≈ {0.4762, 0.4762, 0.0476}
    // Still none exceed 0.5 (10/21 ≈ 0.476) → nDominant=0
    {
        const std::vector<double> dCubed = {10.0, 10.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // dCubed = {11, 11, 1}: sumD=23
    // fracs = {11/23, 11/23, 1/23} ≈ {0.4783, 0.4783, 0.0435}
    // Still none > 0.5 → nDominant=0
    {
        const std::vector<double> dCubed = {11.0, 11.0, 1.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == true);
        REQUIRE(result.getNDominant() == 0);
    }

    // To get two dominant observations we need canceling.
    // dCubed = {4, 4, -7}: sumD=1, absNumD=1
    // fracs = {4, 4, 7} — all three are dominant
    // Two "positive" observations are dominant along with the negative one.
    {
        const std::vector<double> dCubed = {4.0, 4.0, -7.0};
        auto result = JackknifeInfluence::compute(dCubed);
        REQUIRE(result.isReliable()   == false);
        REQUIRE(result.getNDominant() == 3); // all three exceed threshold
    }
}

TEST_CASE("JackknifeInfluence: single dominant element in a longer vector",
          "[JackknifeInfluence]")
{
    // Build a vector of 20 near-equal values with one dominant outlier at index 12.
    // dCubed[0..11,13..19] = 0.01 (20 baseline values, but only 19 at 0.01)
    // dCubed[12] = 10.0
    // sumD = 19 * 0.01 + 10.0 = 10.19
    // frac[12] = 10.0 / 10.19 ≈ 0.9814 > 0.5 → dominant
    // All others: 0.01/10.19 ≈ 0.000981 < 0.5
    std::vector<double> dCubed(20, 0.01);
    dCubed[12] = 10.0;

    const double sumD    = 19 * 0.01 + 10.0; // = 10.19
    const double frac12  = 10.0 / sumD;       // ≈ 0.9814
    const double fracRest = 0.01 / sumD;      // ≈ 0.000981

    auto result = JackknifeInfluence::compute(dCubed);

    REQUIRE(result.isReliable()              == false);
    REQUIRE(result.getMaxInfluenceIndex()    == 12);
    REQUIRE(result.getMaxInfluenceFraction() == Catch::Approx(frac12).epsilon(1e-10));
    REQUIRE(result.getNDominant()            == 1);

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
    // â = 0, Σd³ = 0 → negligible numerator → reliable=true.
    // Verifies the early-exit path populates m_accelReliability correctly.
    std::vector<DecimalType> returns(10, createDecimal("0.01"));

    BCaBootStrap<DecimalType> bca(returns, 500, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()              == true);
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(rel.getNDominant()            == 0);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: diffuse influence data is reliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // DESIGN RATIONALE:
    // For the arithmetic mean statistic, d[i] = (returns[i] - mean) / (n-1),
    // so the influence fraction simplifies to:
    //
    //   frac[i] = |returns[i] - mean|³ / |Σ(returns[j] - mean)³|
    //
    // reliable=true requires max frac[i] ≤ 0.5.
    //
    // "Well-distributed" balanced data is NOT sufficient — when positive and
    // negative cubed deviations nearly cancel, the denominator collapses toward
    // zero and individual fractions explode well above 1.0.
    //
    // The necessary condition is a CONSISTENT positive (or negative) third
    // central moment with no single observation dominating it. This is achieved
    // by a positively skewed distribution: many small values clustered below
    // the mean, a smaller number of moderate values above.
    //
    // This dataset is analytically verified:
    //   8 values at 0.002–0.008, 4 values at 0.035–0.038
    //   mean ≈ 0.0155, positive third moment ≈ 2.04e-8
    //
    //   fracs (verified in Python):
    //     small cluster (i=0..7): 0.090, 0.072, 0.056, 0.043, 0.043, 0.032, 0.023, 0.016
    //     large cluster (i=8..11): 0.272, 0.317, 0.365, 0.419
    //   maxFrac ≈ 0.419 < 0.5  →  reliable=true, nDominant=0
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
    // Analytically: maxFrac ≈ 0.419
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(0.419).margin(0.01));
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: single extreme outlier is unreliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // Returns = {0.001 x4, 1.0}: one trade that is ~1000x larger than the rest.
    //
    // IID delete-one jackknife on mean:
    //   jk[0..3] = (4 * 0.001 + 1.0 - 0.001) / 4 = 1.003/4 = 0.25075
    //   jk[4]    = (4 * 0.001) / 4               = 0.001
    //   jk_avg                                    = 0.2008
    //
    //   d[0..3] = 0.2008 - 0.25075 = -0.04995
    //   d[4]    = 0.2008 - 0.001   =  0.1998
    //
    //   dCubed[0..3] ≈ -0.0001246  (each)
    //   dCubed[4]    ≈  0.0079760
    //
    //   sumD ≈ 0.007476, frac[4] = 0.007976/0.007476 ≈ 1.067 > 0.5
    //
    // → observation 4 dominates → unreliable.
    std::vector<DecimalType> returns = {
        createDecimal("0.001"), createDecimal("0.001"),
        createDecimal("0.001"), createDecimal("0.001"),
        createDecimal("1.0")
    };

    BCaBootStrap<DecimalType> bca(returns, 1000, 0.95);
    auto rel = bca.getAccelerationReliability();

    REQUIRE(rel.isReliable()           == false);
    REQUIRE(rel.getMaxInfluenceIndex() == 4);      // the outlier trade
    REQUIRE(rel.getNDominant()         == 1);
    // frac ≈ 1.067: well above threshold, not a borderline case
    REQUIRE(rel.getMaxInfluenceFraction() > 0.5);
    REQUIRE(rel.getMaxInfluenceFraction() == Catch::Approx(1.0667).epsilon(0.01));
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: isReliable is consistent with maxInfluenceFraction",
          "[BCaBootStrap][AccelerationReliability]")
{
    // Structural invariant: isReliable() == (maxInfluenceFraction <= kDominanceThreshold).
    // Test both reliable and unreliable cases.
    const double threshold = AccelerationReliability::kDominanceThreshold;

    // Reliable case: positively skewed data with consistent third moment
    // (same dataset as the diffuse influence test — analytically verified)
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
    // BCaBootStrap uses lazy evaluation (ensureCalculated). The result must be
    // identical on repeated accessor calls — no recomputation should occur.
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
    REQUIRE(rel1.getMaxInfluenceFraction() == Catch::Approx(rel2.getMaxInfluenceFraction()).epsilon(1e-15));
    REQUIRE(rel1.getMaxInfluenceFraction() == Catch::Approx(rel3.getMaxInfluenceFraction()).epsilon(1e-15));
    REQUIRE(rel1.getMaxInfluenceIndex()    == rel2.getMaxInfluenceIndex());
    REQUIRE(rel1.getMaxInfluenceIndex()    == rel3.getMaxInfluenceIndex());
    REQUIRE(rel1.getNDominant()            == rel2.getNDominant());
    REQUIRE(rel1.getNDominant()            == rel3.getNDominant());
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: larger well-behaved dataset is reliable",
          "[BCaBootStrap][AccelerationReliability]")
{
    // n=50 with a realistic mix of small positive and negative returns.
    // No single trade should dominate the jackknife cubic sum.
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
    // With 50 balanced observations, max fraction should be well below 0.5
    REQUIRE(rel.getMaxInfluenceFraction() < 0.3);
}

TEST_CASE("BCaBootStrap::getAccelerationReliability: StationaryBlockResampler path",
          "[BCaBootStrap][AccelerationReliability][Stationary]")
{
    // Verify the reliability diagnostic works correctly through the
    // StationaryBlockResampler path (block jackknife, not delete-one).
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

    // The block jackknife produces floor(60/3)=20 pseudo-values.
    // With 20 balanced blocks of similar size, no single block should dominate.
    REQUIRE(rel.isReliable()   == true);
    REQUIRE(rel.getNDominant() == 0);
    REQUIRE(rel.getMaxInfluenceFraction() < 0.5);

    // Sanity: diagnostic fields are self-consistent
    REQUIRE(std::isfinite(rel.getMaxInfluenceFraction()));
    REQUIRE(rel.getMaxInfluenceFraction() >= 0.0);
}
