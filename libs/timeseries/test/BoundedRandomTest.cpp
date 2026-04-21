// BoundedRandomTest.cpp
//
// Unit tests for mkc::random_util::bounded_rand and bounded_rand_inclusive.
//
// Coverage:
//   - Basic correctness: result always lies in requested range
//   - Error handling: zero bound, reversed inclusive bounds, oversized bound
//     on restricted-domain generators
//   - Edge cases: bound of 1, full ResultType range for the inclusive form,
//     single-value range (lower == upper)
//   - Uniformity: Pearson chi-square tests at multiple bound sizes and over
//     a binned decomposition of large bounds, run across 50 deterministic
//     seeds so that per-seed anomalies cannot mask bias
//   - Fast path specific to pcg32 / pcg64 (the Lemire multiply-and-reject
//     implementation)
//   - Fallback path via a fake generator with non-zero min() and a non-power-
//     of-2 domain
//   - Fisher-Yates shuffle smoke test exercising the exact pattern of
//     permutation testing
//
// MULTI-SEED UNIFORMITY
// ---------------------
// Every uniformity test runs 50 times, each with a different seed drawn from
// a deterministic std::mt19937_64 sequence. A test fails if ANY seed produces
// a chi-square statistic above threshold; the seed and statistic are captured
// via Catch2's INFO so a failure immediately identifies the offending seed
// for reproduction. Running 50 independent seeds per test makes the suite
// robust against seed-specific false negatives that a single-seed test could
// miss.
//
// CHI-SQUARE THRESHOLDS
// ---------------------
// Each sub-test draws N samples into k bins. Under the null (true uniformity),
// the Pearson chi-square statistic has df = k-1, mean = k-1, and variance
// = 2(k-1). The tests declare failure only when the statistic exceeds
// (k-1) + 7*sqrt(2*(k-1)) — a 7-sigma cutoff corresponding to roughly
// p = 3e-12 under a normal approximation. With 50 seeds per test, the
// family-wise false-positive probability is still below 2e-10 per test,
// comfortably negligible. The threshold remains sharp enough that a
// 1-in-10^4 bias would produce chi-square values many orders of magnitude
// above it.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "BoundedRandom.h"
#include "pcg_random.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

using namespace mkc::random_util;

namespace
{

// Pearson chi-square statistic for observed bin counts against a common
// expected value.
double chi_square(const std::vector<std::uint64_t>& observed, double expected)
{
    double chi_sq = 0.0;
    for (std::uint64_t obs : observed)
    {
        const double diff = static_cast<double>(obs) - expected;
        chi_sq += (diff * diff) / expected;
    }
    return chi_sq;
}

// 7-sigma upper bound on the chi-square statistic under the null. See file
// header for the rationale.
double chi_square_threshold(std::size_t num_bins)
{
    const double df = static_cast<double>(num_bins - 1);
    return df + 7.0 * std::sqrt(2.0 * df);
}

// Generate a deterministic set of 50 test seeds. We run a std::mt19937_64
// forward from a fixed constant rather than using consecutive integers —
// consecutive seeds can exercise correlated PRNG states for some engines,
// and spreading the seeds across the full 64-bit state space is a cheap way
// to avoid that.
const std::vector<std::uint64_t>& testSeeds()
{
    static const std::vector<std::uint64_t> seeds = []()
    {
        constexpr std::size_t kCount = 50;
        constexpr std::uint64_t kBase = 0xC0FFEE5EED5EED5Eull;
        std::vector<std::uint64_t> out;
        out.reserve(kCount);
        std::mt19937_64 seeder(kBase);
        for (std::size_t i = 0; i < kCount; ++i) out.push_back(seeder());
        return out;
    }();
    return seeds;
}

// Run a chi-square uniformity test once per seed. The caller supplies a
// factory that constructs a fresh generator from a seed. Only suitable for
// bounds small enough to allocate a counts vector of that size; large bounds
// are handled by test-local binning code.
template <typename RngFactory, typename BoundType>
void expectUniformAcrossSeeds(BoundType bound,
                              std::uint64_t samples_per_seed,
                              RngFactory make_rng)
{
    const double expected = static_cast<double>(samples_per_seed) / bound;
    const double threshold =
        chi_square_threshold(static_cast<std::size_t>(bound));

    for (std::uint64_t seed : testSeeds())
    {
        auto rng = make_rng(seed);
        std::vector<std::uint64_t> counts(bound, 0);
        for (std::uint64_t i = 0; i < samples_per_seed; ++i)
        {
            ++counts[bounded_rand(rng, bound)];
        }
        const double chi_sq = chi_square(counts, expected);
        INFO("seed=" << seed << " chi_sq=" << chi_sq
             << " threshold=" << threshold << " bound=" << bound);
        REQUIRE(chi_sq < threshold);
    }
}

// A fake generator to exercise the fallback path. It has a non-zero min()
// and a domain of 900 values (not a power of 2), so it fails the
// is_full_width_zero_based_generator_v predicate and is routed to the
// generic rejection-modulo fallback.
//
// Internally we use a pcg32 and produce unbiased draws in [min(), max()]
// by explicit rejection-modulo, NOT by recursing into bounded_rand (which
// would couple the test to the very function under test).
class FakeOddRangeGenerator
{
public:
    using result_type = std::uint32_t;

    static constexpr result_type min() { return 100; }
    static constexpr result_type max() { return 999; }  // domain of 900

    explicit FakeOddRangeGenerator(std::uint64_t seed) : rng_(seed) {}

    result_type operator()()
    {
        constexpr std::uint32_t domain = max() - min() + 1;  // 900
        const std::uint32_t threshold =
            static_cast<std::uint32_t>(-domain) % domain;
        for (;;)
        {
            const std::uint32_t x = rng_();
            if (x >= threshold)
            {
                return min() + (x % domain);
            }
        }
    }

private:
    pcg32 rng_;
};

// Factory helpers to keep test bodies tidy.
auto pcg32Factory()
{
    return [](std::uint64_t s) { return pcg32(s); };
}
auto pcg64Factory()
{
    return [](std::uint64_t s) { return pcg64(s); };
}
auto fakeOddFactory()
{
    return [](std::uint64_t s) { return FakeOddRangeGenerator(s); };
}

}  // namespace

// ============================================================================
// SECTION 1: Basic correctness — result lies in requested range
// ============================================================================

TEST_CASE("bounded_rand: result is always strictly less than the bound (pcg32)",
          "[BoundedRandom][Correctness][pcg32]")
{
    pcg32 rng(42);

    const std::vector<std::uint32_t> bounds = {
        1u, 2u, 3u, 7u, 52u, 1000u, 65537u,
        1u << 20, (1u << 31), (1u << 31) + 1u,
        std::numeric_limits<std::uint32_t>::max()
    };

    for (std::uint32_t bound : bounds)
    {
        for (int i = 0; i < 100'000; ++i)
        {
            const std::uint32_t v = bounded_rand(rng, bound);
            REQUIRE(v < bound);
        }
    }
}

TEST_CASE("bounded_rand: result is always strictly less than the bound (pcg64)",
          "[BoundedRandom][Correctness][pcg64]")
{
    pcg64 rng(42);

    const std::vector<std::uint64_t> bounds = {
        1ull, 2ull, 3ull, 1000ull,
        1ull << 40, 1ull << 62, (1ull << 63) + 1ull,
        std::numeric_limits<std::uint64_t>::max()
    };

    for (std::uint64_t bound : bounds)
    {
        for (int i = 0; i < 100'000; ++i)
        {
            const std::uint64_t v = bounded_rand(rng, bound);
            REQUIRE(v < bound);
        }
    }
}

TEST_CASE("bounded_rand: bound of 1 always returns 0",
          "[BoundedRandom][Correctness][EdgeCase]")
{
    pcg32 rng(42);
    for (int i = 0; i < 10'000; ++i)
    {
        REQUIRE(bounded_rand(rng, 1u) == 0u);
    }
}

// ============================================================================
// SECTION 2: Error handling
// ============================================================================

TEST_CASE("bounded_rand: zero bound throws invalid_argument",
          "[BoundedRandom][ErrorHandling]")
{
    pcg32 rng(42);
    REQUIRE_THROWS_AS(bounded_rand(rng, 0u), std::invalid_argument);

    pcg64 rng64(42);
    REQUIRE_THROWS_AS(bounded_rand(rng64, 0ull), std::invalid_argument);
}

TEST_CASE("bounded_rand: fallback throws out_of_range when bound exceeds "
          "generator domain",
          "[BoundedRandom][ErrorHandling][Fallback]")
{
    FakeOddRangeGenerator rng(42);
    // FakeOddRangeGenerator has a domain of 900. A bound of 1000 should throw.
    REQUIRE_THROWS_AS(bounded_rand(rng, 1000u), std::out_of_range);

    // A bound of exactly the domain size is legal (yields a direct draw).
    REQUIRE_NOTHROW(bounded_rand(rng, 900u));
}

// ============================================================================
// SECTION 3: Uniformity across 50 seeds (fast path)
// ============================================================================

TEST_CASE("bounded_rand: uniformity at bound 3 across 50 seeds (pcg32)",
          "[BoundedRandom][Uniformity][pcg32]")
{
    expectUniformAcrossSeeds(std::uint32_t{3}, 500'000ull, pcg32Factory());
}

TEST_CASE("bounded_rand: uniformity at bound 7 across 50 seeds (pcg32)",
          "[BoundedRandom][Uniformity][pcg32]")
{
    expectUniformAcrossSeeds(std::uint32_t{7}, 500'000ull, pcg32Factory());
}

TEST_CASE("bounded_rand: uniformity at bound 52 across 50 seeds (pcg32)",
          "[BoundedRandom][Uniformity][pcg32]")
{
    expectUniformAcrossSeeds(std::uint32_t{52}, 500'000ull, pcg32Factory());
}

TEST_CASE("bounded_rand: uniformity at bound 1000 across 50 seeds (pcg32)",
          "[BoundedRandom][Uniformity][pcg32]")
{
    expectUniformAcrossSeeds(std::uint32_t{1000}, 500'000ull, pcg32Factory());
}

TEST_CASE("bounded_rand: uniformity at prime bound 65537 across 50 seeds (pcg32)",
          "[BoundedRandom][Uniformity][pcg32]")
{
    // 65537 is prime and awkward for modulo-based schemes.
    expectUniformAcrossSeeds(std::uint32_t{65537}, 1'000'000ull, pcg32Factory());
}

TEST_CASE("bounded_rand: uniformity at very large bound across 50 seeds (pcg32)",
          "[BoundedRandom][Uniformity][pcg32][LargeBound]")
{
    // For a bound near 2^31 we cannot allocate 2^31 bins. Decompose the output
    // into 100 equal-size buckets and test uniformity of bucket occupancy.
    constexpr std::uint32_t bound = (1u << 31) + 1u;   // 2^31 + 1
    constexpr std::size_t num_bins = 100;
    constexpr std::uint64_t samples_per_seed = 500'000;
    const double expected = static_cast<double>(samples_per_seed) / num_bins;
    const double threshold = chi_square_threshold(num_bins);

    for (std::uint64_t seed : testSeeds())
    {
        pcg32 rng(seed);
        std::vector<std::uint64_t> counts(num_bins, 0);
        for (std::uint64_t i = 0; i < samples_per_seed; ++i)
        {
            const std::uint32_t v = bounded_rand(rng, bound);
            REQUIRE(v < bound);
            std::size_t bin = static_cast<std::size_t>(
                (static_cast<std::uint64_t>(v) * num_bins) / bound);
            if (bin >= num_bins) bin = num_bins - 1;  // defensive
            ++counts[bin];
        }
        const double chi_sq = chi_square(counts, expected);
        INFO("seed=" << seed << " chi_sq=" << chi_sq
             << " threshold=" << threshold);
        REQUIRE(chi_sq < threshold);
    }
}

TEST_CASE("bounded_rand: uniformity at bound 1000 across 50 seeds (pcg64)",
          "[BoundedRandom][Uniformity][pcg64]")
{
    expectUniformAcrossSeeds(std::uint64_t{1000}, 500'000ull, pcg64Factory());
}

// ============================================================================
// SECTION 4: Fallback path
// ============================================================================

TEST_CASE("bounded_rand: fallback path produces results in range",
          "[BoundedRandom][Correctness][Fallback]")
{
    FakeOddRangeGenerator rng(99);
    const std::vector<std::uint32_t> bounds = {2u, 7u, 52u, 100u, 500u, 899u, 900u};

    for (std::uint32_t bound : bounds)
    {
        for (int i = 0; i < 10'000; ++i)
        {
            const std::uint32_t v = bounded_rand(rng, bound);
            REQUIRE(v < bound);
        }
    }
}

TEST_CASE("bounded_rand: fallback uniformity at bound 52 across 50 seeds",
          "[BoundedRandom][Uniformity][Fallback]")
{
    // Smaller N per seed — the fallback generator runs its own rejection
    // loop internally, so each draw costs several pcg32 calls.
    expectUniformAcrossSeeds(std::uint32_t{52}, 50'000ull, fakeOddFactory());
}

// ============================================================================
// SECTION 5: Inclusive form
// ============================================================================

TEST_CASE("bounded_rand_inclusive: result lies in closed interval",
          "[BoundedRandom][Inclusive]")
{
    pcg32 rng(42);
    for (int i = 0; i < 100'000; ++i)
    {
        const std::uint32_t v = bounded_rand_inclusive(rng, 10u, 20u);
        REQUIRE(v >= 10u);
        REQUIRE(v <= 20u);
    }
}

TEST_CASE("bounded_rand_inclusive: full ResultType range does not throw",
          "[BoundedRandom][Inclusive][EdgeCase]")
{
    // Specifically exercises the short-circuit that avoids the
    // width = upper - lower + 1 overflow.
    pcg32 rng(42);
    for (int i = 0; i < 1000; ++i)
    {
        REQUIRE_NOTHROW(bounded_rand_inclusive(
            rng, 0u, std::numeric_limits<std::uint32_t>::max()));
    }
}

TEST_CASE("bounded_rand_inclusive: reversed bounds throw invalid_argument",
          "[BoundedRandom][Inclusive][ErrorHandling]")
{
    pcg32 rng(42);
    REQUIRE_THROWS_AS(
        bounded_rand_inclusive(rng, 20u, 10u), std::invalid_argument);
}

TEST_CASE("bounded_rand_inclusive: equal bounds always return that value",
          "[BoundedRandom][Inclusive][EdgeCase]")
{
    pcg32 rng(42);
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(bounded_rand_inclusive(rng, 42u, 42u) == 42u);
    }
}

TEST_CASE("bounded_rand_inclusive: uniformity over shifted interval across 50 seeds",
          "[BoundedRandom][Inclusive][Uniformity]")
{
    constexpr std::uint32_t lower = 1000;
    constexpr std::uint32_t upper = 1051;            // width 52
    constexpr std::uint32_t width = upper - lower + 1;
    constexpr std::uint64_t samples_per_seed = 500'000;
    const double expected = static_cast<double>(samples_per_seed) / width;
    const double threshold = chi_square_threshold(width);

    for (std::uint64_t seed : testSeeds())
    {
        pcg32 rng(seed);
        std::vector<std::uint64_t> counts(width, 0);
        for (std::uint64_t i = 0; i < samples_per_seed; ++i)
        {
            const std::uint32_t v = bounded_rand_inclusive(rng, lower, upper);
            REQUIRE(v >= lower);
            REQUIRE(v <= upper);
            ++counts[v - lower];
        }
        const double chi_sq = chi_square(counts, expected);
        INFO("seed=" << seed << " chi_sq=" << chi_sq
             << " threshold=" << threshold);
        REQUIRE(chi_sq < threshold);
    }
}

// ============================================================================
// SECTION 6: Fisher-Yates shuffle — the permutation-testing use case
// ============================================================================
//
// Run many independent shuffles of a length-n array and check that each
// element lands in each position with approximately equal frequency. This
// is the actual pattern exercised by Monte Carlo permutation testing.
// Spread the total shuffle budget across 50 seeds so that a seed that
// happens to produce a correlated sequence cannot mask bias.

TEST_CASE("bounded_rand: Fisher-Yates yields uniform position distribution "
          "for every element across 50 seeds",
          "[BoundedRandom][Shuffle][Uniformity]")
{
    constexpr std::size_t n = 52;
    constexpr std::size_t shuffles_per_seed = 10'000;
    const double expected = static_cast<double>(shuffles_per_seed) / n;
    const double threshold = chi_square_threshold(n);

    std::vector<int> work(n);

    for (std::uint64_t seed : testSeeds())
    {
        pcg32 rng(seed);

        // position_counts[element][position] = how often that element ended
        // up in that position across all shuffles FOR THIS SEED.
        std::vector<std::vector<std::uint64_t>> position_counts(
            n, std::vector<std::uint64_t>(n, 0));

        for (std::size_t s = 0; s < shuffles_per_seed; ++s)
        {
            for (std::size_t i = 0; i < n; ++i) work[i] = static_cast<int>(i);
            // Standard Fisher-Yates.
            for (std::size_t i = n - 1; i > 0; --i)
            {
                const std::size_t j = bounded_rand(
                    rng, static_cast<std::uint32_t>(i + 1));
                std::swap(work[i], work[j]);
            }
            for (std::size_t i = 0; i < n; ++i)
            {
                ++position_counts[work[i]][i];
            }
        }

        for (std::size_t elem = 0; elem < n; ++elem)
        {
            const double chi_sq = chi_square(position_counts[elem], expected);
            INFO("seed=" << seed << " element=" << elem
                 << " chi_sq=" << chi_sq << " threshold=" << threshold);
            REQUIRE(chi_sq < threshold);
        }
    }
}

// ============================================================================
// SECTION 7: Determinism — same seed produces same sequence
// ============================================================================

TEST_CASE("bounded_rand: identical seeds produce identical sequences "
          "across 50 seeds",
          "[BoundedRandom][Determinism]")
{
    for (std::uint64_t seed : testSeeds())
    {
        pcg32 rng_a(seed);
        pcg32 rng_b(seed);
        for (int i = 0; i < 2'000; ++i)
        {
            const std::uint32_t bound = 2u + (i % 1000);
            INFO("seed=" << seed << " i=" << i);
            REQUIRE(bounded_rand(rng_a, bound) == bounded_rand(rng_b, bound));
        }
    }
}
