// RandomMersenneAdditionalTests.cpp
//
// Additional test cases for RandomMersenne, extending RandomTest.cpp.
// Compile alongside RandomTest.cpp in the same test binary — Catch2 picks
// up TEST_CASEs from all linked translation units.
//
// Coverage gaps addressed (relative to RandomTest.cpp as of review):
//   A. Determinism — every deterministic-seeding path must produce identical
//      sequences from identical inputs. This is the contract that makes
//      failing permutation-test p-values debuggable.
//   B. Exception contracts — the switch to BoundedRandom.h replaced several
//      UB code paths with thrown exceptions. Lock those in so a future
//      refactor cannot silently revert.
//   C. Edge cases — boundary inputs (full uint32_t range, max-value bounds)
//      that exercise the short-circuits and threshold guards in the new
//      implementation. Old pcg_extras path had latent wrap bugs at these.
//   D. Composition chi-square — light-weight uniformity checks that verify
//      DrawNumber* wires through to bounded_rand correctly. Not intended
//      to re-test bounded_rand itself; BoundedRandomTest.cpp owns that.
//   E. Entropy seeding — default ctor and reseed/seed actually advance
//      state. Currently implicit; pinning explicitly.
//   F. Copy/move semantics — pin the current contract so SyntheticTimeSeries's
//      FIX #5 workaround does not silently become no-ops under a refactor.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestUtils.h"
#include "RandomMersenne.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using std::uint32_t;
using std::uint64_t;

namespace
{

// Pearson chi-square statistic for observed counts against a common expected
// value. Kept in an anonymous namespace to avoid symbol collision with
// RandomTest.cpp or any other test translation unit.
double chiSquareStat(const std::vector<std::uint64_t>& observed, double expected)
{
    double chi_sq = 0.0;
    for (auto c : observed)
    {
        const double d = static_cast<double>(c) - expected;
        chi_sq += d * d / expected;
    }
    return chi_sq;
}

// 7-sigma upper bound on chi-square under the null. Consistent with
// BoundedRandomTest.cpp so thresholds are comparable. At df=k-1, mean=k-1,
// variance=2(k-1); 7 sigma above the mean corresponds to roughly p=3e-12
// under a normal approximation — effectively no false positives on good code.
double chiSquareThreshold(std::size_t k)
{
    const double df = static_cast<double>(k - 1);
    return df + 7.0 * std::sqrt(2.0 * df);
}

}  // namespace

// ============================================================================
// SECTION A: Determinism — every deterministic-seed path produces reproducible
// sequences. Test each path separately because they route through different
// entry points (explicit ctor, static factory, member function, stream form,
// templated sequence form) and a regression in any one breaks reproducibility
// without affecting the others.
// ============================================================================

TEST_CASE("explicit ctor: identical uint64_t seeds produce identical sequences",
          "[RandomMersenne][Determinism]")
{
    constexpr uint64_t kSeed = 0xDEADBEEFCAFEBABEull;
    RandomMersenne a(kSeed);
    RandomMersenne b(kSeed);
    for (int i = 0; i < 2'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i << " bound=" << bound);
        REQUIRE(a.DrawNumberExclusive(bound) == b.DrawNumberExclusive(bound));
    }
}

TEST_CASE("withSeed: identical uint64_t seeds produce identical sequences",
          "[RandomMersenne][Determinism]")
{
    constexpr uint64_t kSeed = 0x123456789ABCDEF0ull;
    auto a = RandomMersenne::withSeed(kSeed);
    auto b = RandomMersenne::withSeed(kSeed);
    for (int i = 0; i < 2'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(a.DrawNumberExclusive(bound) == b.DrawNumberExclusive(bound));
    }
}

TEST_CASE("seed_u64: identical seeds applied to fresh instances produce "
          "identical sequences",
          "[RandomMersenne][Determinism]")
{
    constexpr uint64_t kSeed = 0x0F0F0F0F0F0F0F0Full;
    RandomMersenne a;
    RandomMersenne b;
    a.seed_u64(kSeed);
    b.seed_u64(kSeed);
    for (int i = 0; i < 2'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(a.DrawNumberExclusive(bound) == b.DrawNumberExclusive(bound));
    }
}

TEST_CASE("seed_u64: different seeds produce different sequences",
          "[RandomMersenne][Determinism]")
{
    // Complement to the above: verify that seed_u64 actually discriminates
    // on its argument. Protects against a stub implementation that accepts
    // a seed but ignores it.
    RandomMersenne a;
    RandomMersenne b;
    a.seed_u64(0x1111111111111111ull);
    b.seed_u64(0x2222222222222222ull);

    bool anyDifference = false;
    for (int i = 0; i < 100; ++i)
    {
        if (a.DrawNumberExclusive(1'000'000u) != b.DrawNumberExclusive(1'000'000u))
        {
            anyDifference = true;
            break;
        }
    }
    REQUIRE(anyDifference);
}

TEST_CASE("seed_stream: identical (seed, stream_id) produces identical sequences",
          "[RandomMersenne][Determinism]")
{
    constexpr uint64_t kSeed   = 42ull;
    constexpr uint64_t kStream = 7ull;
    RandomMersenne a;
    RandomMersenne b;
    a.seed_stream(kSeed, kStream);
    b.seed_stream(kSeed, kStream);
    for (int i = 0; i < 2'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(a.DrawNumberExclusive(bound) == b.DrawNumberExclusive(bound));
    }
}

TEST_CASE("seed_stream: same seed with different stream_ids produces "
          "different sequences",
          "[RandomMersenne][Determinism]")
{
    // pcg32's stream is controlled by the increment; different stream IDs
    // should give statistically independent sequences from the same seed.
    RandomMersenne a;
    RandomMersenne b;
    a.seed_stream(42ull, 1ull);
    b.seed_stream(42ull, 2ull);

    bool anyDifference = false;
    for (int i = 0; i < 100; ++i)
    {
        if (a.DrawNumberExclusive(1'000'000u) != b.DrawNumberExclusive(1'000'000u))
        {
            anyDifference = true;
            break;
        }
    }
    REQUIRE(anyDifference);
}

TEST_CASE("seed_seq: template method instantiates and is deterministic",
          "[RandomMersenne][Determinism]")
{
    // Exercises the templated seed_seq<It> member. Because it's a template,
    // nothing in the translation unit forces instantiation unless something
    // actually calls it — this test guarantees the signature stays compilable.
    const std::array<uint32_t, 4> kSeedData = {
        0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    RandomMersenne a;
    RandomMersenne b;
    a.seed_seq(kSeedData.begin(), kSeedData.end());
    b.seed_seq(kSeedData.begin(), kSeedData.end());
    for (int i = 0; i < 2'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(a.DrawNumberExclusive(bound) == b.DrawNumberExclusive(bound));
    }
}

// ============================================================================
// SECTION B: Exception contracts — the switch from pcg_extras::bounded_rand
// to BoundedRandom.h converted several UB paths into thrown exceptions.
// ============================================================================

TEST_CASE("DrawNumberExclusive(0) throws invalid_argument",
          "[RandomMersenne][ErrorHandling]")
{
    // Was UB under pcg_extras::bounded_rand (division by zero in the
    // threshold computation). Now throws, which is strictly better.
    RandomMersenne rng;
    REQUIRE_THROWS_AS(rng.DrawNumberExclusive(0u), std::invalid_argument);
}

TEST_CASE("DrawNumber(min, max) with min > max throws invalid_argument",
          "[RandomMersenne][ErrorHandling]")
{
    RandomMersenne rng;
    REQUIRE_THROWS_AS(rng.DrawNumber(20u, 10u), std::invalid_argument);
    REQUIRE_THROWS_AS(rng.DrawNumber(1u, 0u),   std::invalid_argument);
    REQUIRE_THROWS_AS(rng.DrawNumber(std::numeric_limits<uint32_t>::max(), 0u),
                      std::invalid_argument);
}

// ============================================================================
// SECTION C: Edge cases — boundary inputs that exercise short-circuits and
// large-bound code paths.
// ============================================================================

TEST_CASE("DrawNumber(0, UINT32_MAX) covers full uint32_t range without throwing",
          "[RandomMersenne][EdgeCase]")
{
    // Exercises the full-range short-circuit in bounded_rand_inclusive that
    // avoids width = upper - lower + 1 = 0 wrap-to-zero.
    RandomMersenne rng(12345);
    for (int i = 0; i < 1'000; ++i)
    {
        REQUIRE_NOTHROW(rng.DrawNumber(0u, std::numeric_limits<uint32_t>::max()));
    }
}

TEST_CASE("DrawNumber(UINT32_MAX) does not trigger max+1 wrap",
          "[RandomMersenne][EdgeCase]")
{
    // Under the old implementation this became
    // pcg_extras::bounded_rand(engine, UINT32_MAX + 1)
    // = pcg_extras::bounded_rand(engine, 0) — UB.
    // The new implementation routes through bounded_rand_inclusive(0, UINT32_MAX),
    // hitting the full-range short-circuit.
    RandomMersenne rng(12345);
    for (int i = 0; i < 1'000; ++i)
    {
        REQUIRE_NOTHROW(
            rng.DrawNumber(std::numeric_limits<uint32_t>::max()));
    }
}

TEST_CASE("DrawNumber(UINT32_MAX, UINT32_MAX) returns UINT32_MAX",
          "[RandomMersenne][EdgeCase]")
{
    // Equal-bounds short-circuit at the max-value boundary. The internal
    // width computation upper - lower + 1 = 1 is safe here (the full-range
    // short-circuit is the only case where the +1 would wrap).
    RandomMersenne rng;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(rng.DrawNumber(std::numeric_limits<uint32_t>::max(),
                               std::numeric_limits<uint32_t>::max())
                == std::numeric_limits<uint32_t>::max());
    }
}

TEST_CASE("DrawNumberExclusive(UINT32_MAX) produces values in [0, UINT32_MAX)",
          "[RandomMersenne][EdgeCase]")
{
    // Large-bound path through bounded_rand exercises Lemire's threshold
    // guard. Critically, the return value must be strictly less than the
    // bound — an off-by-one in the threshold would allow UINT32_MAX through.
    RandomMersenne rng(12345);
    for (int i = 0; i < 1'000; ++i)
    {
        const uint32_t v =
            rng.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
        REQUIRE(v < std::numeric_limits<uint32_t>::max());
    }
}

// ============================================================================
// SECTION D: Composition chi-square — verifies that DrawNumber* wires
// through to bounded_rand correctly. Intended to catch off-by-one, sign,
// or range-translation bugs in the wrappers. For exhaustive bounded_rand
// uniformity coverage see BoundedRandomTest.cpp.
// ============================================================================

TEST_CASE("DrawNumberExclusive: uniformity at bound 52 (composition check)",
          "[RandomMersenne][Uniformity]")
{
    constexpr uint32_t kBound = 52;
    constexpr std::uint64_t kN = 500'000;
    const double expected  = static_cast<double>(kN) / kBound;
    const double threshold = chiSquareThreshold(kBound);

    RandomMersenne rng(0xA11CE5EED5EED5EEull);
    std::vector<std::uint64_t> counts(kBound, 0);
    for (std::uint64_t i = 0; i < kN; ++i)
    {
        ++counts[rng.DrawNumberExclusive(kBound)];
    }
    const double chi_sq = chiSquareStat(counts, expected);
    INFO("chi_sq=" << chi_sq << " threshold=" << threshold);
    REQUIRE(chi_sq < threshold);
}

TEST_CASE("DrawNumber(max): uniformity over [0, 51] (composition check)",
          "[RandomMersenne][Uniformity]")
{
    // Exercises the one-argument DrawNumber(max) form, which routes through
    // bounded_rand_inclusive(0, max). Inclusive — so k = max + 1 bins.
    constexpr uint32_t kMax = 51;
    constexpr std::uint64_t kN = 500'000;
    constexpr std::size_t kBins = kMax + 1;
    const double expected  = static_cast<double>(kN) / kBins;
    const double threshold = chiSquareThreshold(kBins);

    RandomMersenne rng(0xB0B5EED0B0B5EED0ull);
    std::vector<std::uint64_t> counts(kBins, 0);
    for (std::uint64_t i = 0; i < kN; ++i)
    {
        const uint32_t v = rng.DrawNumber(kMax);
        REQUIRE(v <= kMax);
        ++counts[v];
    }
    const double chi_sq = chiSquareStat(counts, expected);
    INFO("chi_sq=" << chi_sq << " threshold=" << threshold);
    REQUIRE(chi_sq < threshold);
}

TEST_CASE("DrawNumber(min, max): uniformity over shifted interval [100, 151] "
          "(composition check)",
          "[RandomMersenne][Uniformity]")
{
    // Exercises the two-argument form AND the lower-bound translation.
    // A bug where the translation added `lower` to the wrong side of the
    // draw would produce a shifted-but-uniform distribution that passes
    // the in-range test while being visibly biased here.
    constexpr uint32_t kMin = 100;
    constexpr uint32_t kMax = 151;  // width 52
    constexpr uint32_t kWidth = kMax - kMin + 1;
    constexpr std::uint64_t kN = 500'000;
    const double expected  = static_cast<double>(kN) / kWidth;
    const double threshold = chiSquareThreshold(kWidth);

    RandomMersenne rng(0xCAFE5EED0CAFE5EEull);
    std::vector<std::uint64_t> counts(kWidth, 0);
    for (std::uint64_t i = 0; i < kN; ++i)
    {
        const uint32_t v = rng.DrawNumber(kMin, kMax);
        REQUIRE(v >= kMin);
        REQUIRE(v <= kMax);
        ++counts[v - kMin];
    }
    const double chi_sq = chiSquareStat(counts, expected);
    INFO("chi_sq=" << chi_sq << " threshold=" << threshold);
    REQUIRE(chi_sq < threshold);
}

// ============================================================================
// SECTION E: Entropy seeding — default ctor produces independent sequences,
// and reseed/seed actually advance state.
// ============================================================================

TEST_CASE("Default ctor: two independent instances produce distinct first draws",
          "[RandomMersenne][Entropy]")
{
    // Two fresh auto-seeded instances should yield different first draws.
    // Collision probability on a uint32_t draw is 2^-32 — statistically
    // indistinguishable from zero.
    RandomMersenne a;
    RandomMersenne b;
    const uint32_t ua =
        a.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
    const uint32_t ub =
        b.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
    REQUIRE(ua != ub);
}

TEST_CASE("reseed(): advances state away from prior sequence",
          "[RandomMersenne][Entropy]")
{
    RandomMersenne rng;
    const uint32_t before =
        rng.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
    rng.reseed();
    const uint32_t after =
        rng.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
    REQUIRE(before != after);
}

TEST_CASE("seed(): advances state away from prior sequence",
          "[RandomMersenne][Entropy]")
{
    // seed() and reseed() both pull from auto_seed_256 in the current
    // implementation. Tested separately to pin down both public entry
    // points — either could diverge under a future refactor.
    RandomMersenne rng;
    const uint32_t before =
        rng.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
    rng.seed();
    const uint32_t after =
        rng.DrawNumberExclusive(std::numeric_limits<uint32_t>::max());
    REQUIRE(before != after);
}

// ============================================================================
// SECTION F: Copy/move semantics — pin the current contract so a future
// refactor cannot silently change it. SyntheticTimeSeries.h FIX #5 explicitly
// works around the fact that copy duplicates RNG state; that contract must
// hold for the workaround to be necessary (and correct).
// ============================================================================

TEST_CASE("Copy constructor duplicates RNG state (copies produce identical "
          "sequences)",
          "[RandomMersenne][CopyMove]")
{
    // This is the contract that FIX #5 in SyntheticTimeSeries.h's copy
    // constructor works around. If this test ever fails (because someone
    // made RandomMersenne's copy constructor reseed), FIX #5 becomes dead
    // code and SyntheticTimeSeries's deliberate reseed-on-copy behavior
    // would need re-examination.
    RandomMersenne original(0x5EED5EED5EED5EEDull);

    // Advance state past the initial seed so we're testing mid-sequence.
    for (int i = 0; i < 10; ++i) (void) original.DrawNumberExclusive(1'000u);

    RandomMersenne copy = original;
    for (int i = 0; i < 1'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(original.DrawNumberExclusive(bound)
                == copy.DrawNumberExclusive(bound));
    }
}

TEST_CASE("Copy assignment duplicates RNG state",
          "[RandomMersenne][CopyMove]")
{
    RandomMersenne original(0xABABABABABABABABull);
    for (int i = 0; i < 10; ++i) (void) original.DrawNumberExclusive(1'000u);

    RandomMersenne copy;   // different initial state (entropy-seeded)
    copy = original;
    for (int i = 0; i < 1'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(original.DrawNumberExclusive(bound)
                == copy.DrawNumberExclusive(bound));
    }
}

TEST_CASE("Move constructor transfers state to moved-to instance",
          "[RandomMersenne][CopyMove]")
{
    // Seed two instances identically. Move one; the moved-to instance and
    // the un-moved reference should produce identical subsequent draws.
    // (Moved-from state is valid-but-unspecified and deliberately not
    // inspected.)
    RandomMersenne source   (0xABCDEFABCDEFABCDull);
    RandomMersenne reference(0xABCDEFABCDEFABCDull);

    RandomMersenne moved = std::move(source);
    for (int i = 0; i < 1'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(moved.DrawNumberExclusive(bound)
                == reference.DrawNumberExclusive(bound));
    }
}

TEST_CASE("Move assignment transfers state to moved-to instance",
          "[RandomMersenne][CopyMove]")
{
    RandomMersenne source   (0xFEDCBAFEDCBAFEDCull);
    RandomMersenne reference(0xFEDCBAFEDCBAFEDCull);

    RandomMersenne moved;  // different initial state
    moved = std::move(source);
    for (int i = 0; i < 1'000; ++i)
    {
        const uint32_t bound = 2u + static_cast<uint32_t>(i % 1000);
        INFO("iteration=" << i);
        REQUIRE(moved.DrawNumberExclusive(bound)
                == reference.DrawNumberExclusive(bound));
    }
}
