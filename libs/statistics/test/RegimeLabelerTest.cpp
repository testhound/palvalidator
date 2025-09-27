// RegimeLabelerTest.cpp
//
// Unit tests for VolTercileLabeler (RegimeLabeler) using Catch2.
// Mirrors style and includes from the BCa test suite.
//
// Labels:
//  - 0: LowVol, 1: MidVol, 2: HighVol
//
// Dependencies:
//  - RegimeLabeler.h
//  - TestUtils.h, number.h (for DecimalType/createDecimal)
//  - Catch2

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <random>

#include "RegimeLabeler.h"
#include "TestUtils.h"   // DecimalType, createDecimal
#include "number.h"

using palvalidator::analysis::VolTercileLabeler;

TEST_CASE("VolTercileLabeler: constructor validation", "[RegimeLabeler][VolTercile]")
{
    using D = DecimalType;

    SECTION("Window must be >= 2")
    {
        REQUIRE_THROWS_AS(VolTercileLabeler<D>(0), std::invalid_argument);
        REQUIRE_THROWS_AS(VolTercileLabeler<D>(1), std::invalid_argument);
        REQUIRE_NOTHROW(VolTercileLabeler<D>(2));
    }
}

TEST_CASE("VolTercileLabeler: input size validation", "[RegimeLabeler][VolTercile]")
{
    using D = DecimalType;

    const std::size_t win = 4;
    VolTercileLabeler<D> labeler(win);

    SECTION("Throws if series too short for rolling window")
    {
        std::vector<D> tooShort = { createDecimal("0.01"), createDecimal("-0.01"), createDecimal("0.02"), createDecimal("0.00") };
        // Need at least mWindow + 2 points by design:
        REQUIRE_THROWS_AS(labeler.computeLabels(tooShort), std::invalid_argument);
    }

    SECTION("Accepts minimal workable length n >= mWindow + 2")
    {
        std::vector<D> ok = {
            createDecimal("0.01"), createDecimal("0.02"), createDecimal("0.00"),
            createDecimal("-0.01"), createDecimal("0.005"), createDecimal("-0.004")
        }; // n = 6, win=4 -> ok
        REQUIRE_NOTHROW(labeler.computeLabels(ok));
    }
}

TEST_CASE("VolTercileLabeler: size, domain and warmup fill-forward", "[RegimeLabeler][VolTercile]")
{
    using D = DecimalType;

    const std::size_t win = 4;
    VolTercileLabeler<D> labeler(win);

    // 12 bars, with some mild variation
    std::vector<D> r = {
        createDecimal("0.002"), createDecimal("-0.001"), createDecimal("0.003"), createDecimal("0.000"),
        createDecimal("-0.004"), createDecimal("0.006"), createDecimal("-0.002"), createDecimal("0.001"),
        createDecimal("0.004"), createDecimal("-0.003"), createDecimal("0.002"), createDecimal("0.005")
    };

    auto z = labeler.computeLabels(r);

    REQUIRE(z.size() == r.size());

    // Domain {0,1,2}
    for (int zi : z)
    {
        REQUIRE((zi == 0 || zi == 1 || zi == 2));
    }

    // Warmup: first (win-1) labels should match label at (win-1)
    for (std::size_t i = 0; i < win - 1; ++i)
    {
        REQUIRE(z[i] == z[win - 1]);
    }
}

TEST_CASE("VolTercileLabeler: separates low/mid/high volatility terciles on synthetic data", "[RegimeLabeler][VolTercile]")
{
    using D = DecimalType;

    // Build a series with three distinct absolute-volatility regimes:
    //  - first 40 bars: very small |r|
    //  - middle 40 bars: medium |r|
    //  - last 40 bars: large |r|
    std::vector<D> r; r.reserve(120);

    auto push_noisy_block = [&](double base, double noise, int count)
    {
        std::mt19937_64 gen(1234567 ^ static_cast<unsigned long long>(base * 1e6));
        std::uniform_real_distribution<double> u(-noise, noise);
        for (int i = 0; i < count; ++i)
        {
            // Randomly flip sign but keep |r| near base
            double sgn = (i % 2 == 0 ? 1.0 : -1.0);
            r.push_back(createDecimal(std::to_string(sgn * (base + u(gen)))));
        }
    };

    push_noisy_block(0.001, 0.0002, 40); // low
    push_noisy_block(0.010, 0.0020, 40); // mid
    push_noisy_block(0.050, 0.0050, 40); // high

    const std::size_t win = 20; // longer than your hold, but fine for regime inference
    VolTercileLabeler<D> labeler(win);
    auto z = labeler.computeLabels(r);

    REQUIRE(z.size() == r.size());

    // Count labels by segment (ignoring first win-1 warmup if desired)
    auto count_in_segment = [&](int label, std::size_t start, std::size_t len)
    {
        std::size_t end = std::min(start + len, z.size());
        int cnt = 0;
        for (std::size_t i = start; i < end; ++i)
        {
            if (z[i] == label) ++cnt;
        }
        return cnt;
    };

    // Expect the low segment to be mostly label 0,
    // the mid to be mostly 1, and the high to be mostly 2.
    // Allow some slack due to rolling + noise.
    int low0  = count_in_segment(0, 0, 40);
    int mid1  = count_in_segment(1, 40, 40);
    int high2 = count_in_segment(2, 80, 40);

    REQUIRE(low0  >= 24);   // ≥60% of low segment labeled LowVol
    REQUIRE(mid1  >= 24);   // ≥60% MidVol
    REQUIRE(high2 >= 24);   // ≥60% HighVol
}

TEST_CASE("VolTercileLabeler: ties (all magnitudes equal) yield consistent labeling", "[RegimeLabeler][VolTercile]")
{
    using D = DecimalType;

    // All |r| equal -> q1 == q2, code maps (<= q1) to 0, so all zeros expected
    std::vector<D> r(30, createDecimal("0.005")); // constant
    const std::size_t win = 10;

    VolTercileLabeler<D> labeler(win);
    auto z = labeler.computeLabels(r);

    REQUIRE(z.size() == r.size());
    for (int zi : z)
    {
        REQUIRE(zi == 0);
    }
}

TEST_CASE("VolTercileLabeler: scale invariance", "[RegimeLabeler][VolTercile]")
{
    using D = DecimalType;

    // Any positive scaling of returns magnitudes should produce identical labels,
    // since terciles of |r| are scale-invariant under monotone scaling.
    std::vector<D> r1 = {
        createDecimal("0.002"), createDecimal("-0.001"), createDecimal("0.003"),
        createDecimal("-0.002"), createDecimal("0.0005"), createDecimal("0.004"),
        createDecimal("-0.001"), createDecimal("0.0035"), createDecimal("-0.0025"),
        createDecimal("0.0015"), createDecimal("0.0022"), createDecimal("-0.0013"),
        createDecimal("0.0041"), createDecimal("-0.0031"), createDecimal("0.0007"),
        createDecimal("0.0027"), createDecimal("-0.0022"), createDecimal("0.0032"),
        createDecimal("0.0009"), createDecimal("-0.0017"), createDecimal("0.0040"),
        createDecimal("-0.0030"), createDecimal("0.0010"), createDecimal("0.0020")
    };

    // Scale by 10×
    std::vector<D> r2; r2.reserve(r1.size());
    for (auto &v : r1)
    {
        r2.push_back(D("10.0") * v);
    }

    const std::size_t win = 6;
    VolTercileLabeler<D> L(win);

    auto z1 = L.computeLabels(r1);
    auto z2 = L.computeLabels(r2);

    REQUIRE(z1.size() == z2.size());
    for (std::size_t i = 0; i < z1.size(); ++i)
    {
        REQUIRE(z1[i] == z2[i]);
    }
}
