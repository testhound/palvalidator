#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>

#include "StationaryMaskResamplers.h"
#include "number.h"
#include "randutils.hpp"

using palvalidator::resampling::make_restart_mask;
using palvalidator::resampling::StationaryMaskValueResampler;
using palvalidator::resampling::StationaryMaskIndexResampler;
using palvalidator::resampling::StationaryBlockValueResampler;

// Use a concrete decimal type - adjust based on your project
using D = num::DefaultNumber;

// ============================================================================
// SECTION 1: make_restart_mask edge cases
// ============================================================================

TEST_CASE("make_restart_mask: L=1.0 produces all restarts", "[Resampler][Mask][Edge][L=1]")
{
    randutils::seed_seq_fe128 seed{999u, 888u, 777u, 666u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 500;
    const double L = 1.0;  // p = 1.0 (clamped)
    
    // Run multiple trials to verify consistency
    const int trials = 20;
    for (int trial = 0; trial < trials; ++trial)
    {
        auto mask = make_restart_mask(m, L, rng);
        REQUIRE(mask.size() == m);
        
        // With L=1.0, every position should be a restart
        for (std::size_t t = 0; t < m; ++t)
        {
            REQUIRE(mask[t] == 1u);
        }
    }
}

TEST_CASE("make_restart_mask: very large L produces rare restarts", "[Resampler][Mask][Edge][LargeL]")
{
    randutils::seed_seq_fe128 seed{111u, 222u, 333u, 444u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 10000;
    const double L = 1000.0;  // Very large mean block length
    const double p = 1.0 / L;  // p = 0.001
    
    auto mask = make_restart_mask(m, L, rng);
    REQUIRE(mask.size() == m);
    REQUIRE(mask[0] == 1u);
    
    // Count total restarts
    std::size_t restarts = 0;
    for (auto b : mask) restarts += (b ? 1 : 0);
    
    // Expected: m * p = 10
    // Standard deviation: sqrt(m * p * (1-p)) ≈ 3.16
    // Allow 6 sigma for stability: 10 ± 19
    const double expected = m * p;
    const double sigma = std::sqrt(m * p * (1.0 - p));
    REQUIRE(std::abs(static_cast<double>(restarts) - expected) < 6.0 * sigma);
}

TEST_CASE("make_restart_mask: L exactly at boundary values", "[Resampler][Mask][Edge][Boundary]")
{
    randutils::seed_seq_fe128 seed{555u, 666u, 777u, 888u};
    randutils::mt19937_rng rng(seed);

    SECTION("L = 1.0 exactly")
    {
        auto mask = make_restart_mask(100, 1.0, rng);
        // Should clamp to p = 1.0
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts == 100);
    }

    SECTION("L slightly above 1.0")
    {
        const std::size_t m = 5000;
        const double L = 1.001;
        auto mask = make_restart_mask(m, L, rng);
        
        // p ≈ 0.999, so nearly all should be restarts
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts > 0.98 * m);  // At least 98% restarts
    }

    SECTION("L = 2.0 exactly")
    {
        const std::size_t m = 2000;
        const double L = 2.0;
        auto mask = make_restart_mask(m, L, rng);
        
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        const double expected = m * 0.5;
        const double sigma = std::sqrt(m * 0.5 * 0.5);
        REQUIRE(std::abs(static_cast<double>(restarts) - expected) < 5.0 * sigma);
    }
}

// ============================================================================
// SECTION 2: StationaryMaskValueResampler edge cases and accessors
// ============================================================================

TEST_CASE("StationaryMaskValueResampler: L=1 produces IID-like behavior", "[Resampler][Value][L=1][IID]")
{
    // With L=1, every position is a restart → purely IID sampling
    const std::size_t n = 997;  // Prime to avoid artifacts
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{2025u, 1u, 1u, 1u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 5000;
    StationaryMaskValueResampler<D> res(1);  // L=1
    std::vector<D> y;
    res(x, y, m, rng);

    REQUIRE(y.size() == m);

    // Count continuations: next = (cur + 1) % n
    std::size_t continuations = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        if (curr == (prev + 1) % static_cast<int>(n))
            continuations++;
    }

    // With L=1, each pair is independent → P(continuation) = 1/n
    const double p = 1.0 / static_cast<double>(n);
    const double N = static_cast<double>(m - 1);
    const double expected = N * p;
    const double sigma = std::sqrt(N * p * (1.0 - p));
    
    // Allow 6 sigma for stability
    REQUIRE(std::abs(static_cast<double>(continuations) - expected) < 6.0 * sigma);
}

TEST_CASE("StationaryMaskValueResampler: getL() returns correct value", "[Resampler][Value][Accessor]")
{
    SECTION("L = 1")
    {
        StationaryMaskValueResampler<D> res(1);
        REQUIRE(res.getL() == 1);
    }

    SECTION("L = 5")
    {
        StationaryMaskValueResampler<D> res(5);
        REQUIRE(res.getL() == 5);
    }

    SECTION("L = 100")
    {
        StationaryMaskValueResampler<D> res(100);
        REQUIRE(res.getL() == 100);
    }
}

TEST_CASE("StationaryMaskValueResampler: explicit wraparound verification", "[Resampler][Value][Wraparound]")
{
    // Small n to force frequent wraparound
    const std::size_t n = 10;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{42u, 42u, 42u, 42u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 500;
    const std::size_t L = 15;  // Large L relative to n
    StationaryMaskValueResampler<D> res(L);
    std::vector<D> y;
    res(x, y, m, rng);

    REQUIRE(y.size() == m);

    // Verify all values are valid (in [0, n-1])
    for (const auto& v : y)
    {
        const int vi = static_cast<int>(num::to_double(v));
        REQUIRE(vi >= 0);
        REQUIRE(vi < static_cast<int>(n));
    }

    // Count wraparounds: transitions from n-1 to 0
    std::size_t wraparounds = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        if (prev == static_cast<int>(n-1) && curr == 0)
            wraparounds++;
    }

    // With large L and small n, we expect many wraparounds
    REQUIRE(wraparounds > 10);  // Sanity check
}

TEST_CASE("StationaryMaskValueResampler: very large L produces long blocks", "[Resampler][Value][LargeL]")
{
    const std::size_t n = 200;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{777u, 888u, 999u, 111u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 1000;
    const std::size_t L = 500;  // Very large mean block
    StationaryMaskValueResampler<D> res(L);
    std::vector<D> y;
    res(x, y, m, rng);

    // Count block breaks (non-contiguous transitions)
    std::size_t breaks = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        if (curr != (prev + 1) % static_cast<int>(n))
            breaks++;
    }

    // Expected breaks ≈ m/L = 2
    // With large L, we expect very few breaks
    REQUIRE(breaks < 10);  // Conservative upper bound
}

// ============================================================================
// SECTION 3: StationaryMaskIndexResampler edge cases and accessors
// ============================================================================

TEST_CASE("StationaryMaskIndexResampler: L=1 produces IID indices", "[Resampler][Index][L=1][IID]")
{
    const std::size_t n = 503;  // Prime
    randutils::seed_seq_fe128 seed{2025u, 2u, 2u, 2u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 4000;
    StationaryMaskIndexResampler res(1);  // L=1
    std::vector<std::size_t> idx;
    res(n, idx, m, rng);

    REQUIRE(idx.size() == m);

    // Count continuations
    std::size_t continuations = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        if (idx[t] == (idx[t-1] + 1) % n)
            continuations++;
    }

    // Expected: Binomial(m-1, 1/n)
    const double p = 1.0 / static_cast<double>(n);
    const double N = static_cast<double>(m - 1);
    const double expected = N * p;
    const double sigma = std::sqrt(N * p * (1.0 - p));
    
    REQUIRE(std::abs(static_cast<double>(continuations) - expected) < 6.0 * sigma);
}

TEST_CASE("StationaryMaskIndexResampler: getL() returns correct value", "[Resampler][Index][Accessor]")
{
    SECTION("L = 1")
    {
        StationaryMaskIndexResampler res(1);
        REQUIRE(res.getL() == 1);
    }

    SECTION("L = 7")
    {
        StationaryMaskIndexResampler res(7);
        REQUIRE(res.getL() == 7);
    }

    SECTION("L = 1000")
    {
        StationaryMaskIndexResampler res(1000);
        REQUIRE(res.getL() == 1000);
    }
}

TEST_CASE("StationaryMaskIndexResampler: explicit wraparound at boundary", "[Resampler][Index][Wraparound]")
{
    const std::size_t n = 8;
    randutils::seed_seq_fe128 seed{100u, 200u, 300u, 400u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 500;
    const std::size_t L = 20;  // Large L to get long blocks
    StationaryMaskIndexResampler res(L);
    std::vector<std::size_t> idx;
    res(n, idx, m, rng);

    REQUIRE(idx.size() == m);

    // All indices in range
    for (auto i : idx)
    {
        REQUIRE(i < n);
    }

    // Count wraparounds: n-1 → 0 transitions
    std::size_t wraparounds = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        if (idx[t-1] == n-1 && idx[t] == 0)
            wraparounds++;
    }

    // With small n and large blocks, expect multiple wraparounds
    REQUIRE(wraparounds > 5);
}

// ============================================================================
// SECTION 4: StationaryBlockValueResampler additional tests
// ============================================================================

TEST_CASE("StationaryBlockValueResampler: getL() accessor", "[Resampler][BlockValue][Accessor]")
{
    SECTION("L = 1")
    {
        StationaryBlockValueResampler<D> res(1);
        REQUIRE(res.getL() == 1);
    }

    SECTION("L = 10")
    {
        StationaryBlockValueResampler<D> res(10);
        REQUIRE(res.getL() == 10);
    }

    SECTION("L = 0 throws exception")
    {
        // Constructor throws for L < 1 (consistent with mask resamplers)
        REQUIRE_THROWS_AS(StationaryBlockValueResampler<D>(0), std::invalid_argument);
    }
}

TEST_CASE("StationaryBlockValueResampler: explicit wraparound with doubled buffer", "[Resampler][BlockValue][Wraparound]")
{
    // Small n to force wraparound in blocks
    const std::size_t n = 12;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{50u, 60u, 70u, 80u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 600;
    const std::size_t L = 20;  // Mean block longer than n
    StationaryBlockValueResampler<D> res(L);
    std::vector<D> y;
    res(x, y, m, rng);

    REQUIRE(y.size() == m);

    // Verify all values in range
    for (const auto& v : y)
    {
        const int vi = static_cast<int>(num::to_double(v));
        REQUIRE(vi >= 0);
        REQUIRE(vi < static_cast<int>(n));
    }

    // Count wraparound transitions
    std::size_t wraparounds = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        if (prev == static_cast<int>(n-1) && curr == 0)
            wraparounds++;
    }

    // With L > n, blocks frequently wrap around
    REQUIRE(wraparounds > 10);
}

TEST_CASE("StationaryBlockValueResampler: block extends beyond array boundary", "[Resampler][BlockValue][LongBlock]")
{
    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{1000u, 2000u, 3000u, 4000u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 300;
    const std::size_t L = 100;  // Mean block >> n
    StationaryBlockValueResampler<D> res(L);
    std::vector<D> y;
    res(x, y, m, rng);

    REQUIRE(y.size() == m);

    // All values must be valid
    for (const auto& v : y)
    {
        const double vd = num::to_double(v);
        REQUIRE(vd >= 0.0);
        REQUIRE(vd < static_cast<double>(n));
    }

    // With very large L, we expect very long contiguous sequences
    std::size_t max_contig_run = 0;
    std::size_t current_run = 1;
    
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        
        if (curr == (prev + 1) % static_cast<int>(n))
        {
            current_run++;
        }
        else
        {
            max_contig_run = std::max(max_contig_run, current_run);
            current_run = 1;
        }
    }
    max_contig_run = std::max(max_contig_run, current_run);

    // Expect at least one very long run (> n)
    REQUIRE(max_contig_run > n);
}

// ============================================================================
// SECTION 5: Cross-validation between resampler implementations
// ============================================================================

TEST_CASE("StationaryBlockValueResampler vs StationaryMaskValueResampler: similar block structure", "[Resampler][CrossValidation]")
{
    const std::size_t n = 150;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    const std::size_t m = 2000;
    const std::size_t L = 8;

    // Run both resamplers multiple times and compare statistics
    const int trials = 100;
    
    std::vector<double> block_contigs;
    std::vector<double> mask_contigs;

    for (int trial = 0; trial < trials; ++trial)
    {
        randutils::seed_seq_fe128 seed{static_cast<unsigned>(trial), 99u, 88u, 77u};
        
        // Block resampler
        randutils::mt19937_rng rng1(seed);
        StationaryBlockValueResampler<D> block_res(L);
        std::vector<D> y1;
        block_res(x, y1, m, rng1);
        
        // Mask resampler (different RNG but same statistical properties)
        randutils::mt19937_rng rng2(seed);
        rng2.engine().discard(100);  // Different stream
        StationaryMaskValueResampler<D> mask_res(L);
        std::vector<D> y2;
        mask_res(x, y2, m, rng2);
        
        // Compute contiguity fraction for each
        auto compute_contig = [&](const std::vector<D>& y) -> double {
            std::size_t adjacent = 0;
            for (std::size_t t = 1; t < y.size(); ++t)
            {
                int prev = static_cast<int>(num::to_double(y[t-1]));
                int curr = static_cast<int>(num::to_double(y[t]));
                if (curr == (prev + 1) % static_cast<int>(n))
                    adjacent++;
            }
            return static_cast<double>(adjacent) / static_cast<double>(m - 1);
        };
        
        block_contigs.push_back(compute_contig(y1));
        mask_contigs.push_back(compute_contig(y2));
    }

    // Compute means
    double block_mean = std::accumulate(block_contigs.begin(), block_contigs.end(), 0.0) / trials;
    double mask_mean = std::accumulate(mask_contigs.begin(), mask_contigs.end(), 0.0) / trials;

    // Both should be close to (1 - 1/L) ≈ 0.875
    const double expected = 1.0 - 1.0 / static_cast<double>(L);
    
    REQUIRE(block_mean == Catch::Approx(expected).margin(0.05));
    REQUIRE(mask_mean == Catch::Approx(expected).margin(0.05));
    
    // Means should be very similar to each other
    REQUIRE(std::abs(block_mean - mask_mean) < 0.03);
}

TEST_CASE("All three resamplers produce valid outputs with same L", "[Resampler][CrossValidation][AllThree]")
{
    const std::size_t n = 100;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{5000u, 6000u, 7000u, 8000u};
    
    const std::size_t m = 500;
    const std::size_t L = 6;

    // Block resampler
    randutils::mt19937_rng rng1(seed);
    StationaryBlockValueResampler<D> block_res(L);
    std::vector<D> y_block;
    block_res(x, y_block, m, rng1);

    // Mask value resampler
    randutils::mt19937_rng rng2(seed);
    StationaryMaskValueResampler<D> mask_val_res(L);
    std::vector<D> y_mask_val;
    mask_val_res(x, y_mask_val, m, rng2);

    // Mask index resampler
    randutils::mt19937_rng rng3(seed);
    StationaryMaskIndexResampler mask_idx_res(L);
    std::vector<std::size_t> idx;
    mask_idx_res(n, idx, m, rng3);
    std::vector<D> y_mask_idx; y_mask_idx.reserve(m);
    for (auto i : idx) y_mask_idx.push_back(x[i]);

    // All should have correct length
    REQUIRE(y_block.size() == m);
    REQUIRE(y_mask_val.size() == m);
    REQUIRE(y_mask_idx.size() == m);

    // All values should be in valid range
    for (std::size_t t = 0; t < m; ++t)
    {
        const double v1 = num::to_double(y_block[t]);
        const double v2 = num::to_double(y_mask_val[t]);
        const double v3 = num::to_double(y_mask_idx[t]);
        
        REQUIRE(v1 >= 0.0);
        REQUIRE(v1 < static_cast<double>(n));
        REQUIRE(v2 >= 0.0);
        REQUIRE(v2 < static_cast<double>(n));
        REQUIRE(v3 >= 0.0);
        REQUIRE(v3 < static_cast<double>(n));
    }

    // All should report same L
    REQUIRE(block_res.getL() == L);
    REQUIRE(mask_val_res.getL() == L);
    REQUIRE(mask_idx_res.getL() == L);
}

// ============================================================================
// SECTION 6: Statistical properties - block length distribution
// ============================================================================

TEST_CASE("StationaryMaskValueResampler: block lengths follow geometric distribution", "[Resampler][Value][Statistics][BlockLength]")
{
    const std::size_t n = 500;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{3333u, 4444u, 5555u, 6666u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 10000;  // Large sample
    const std::size_t L = 5;
    StationaryMaskValueResampler<D> res(L);
    std::vector<D> y;
    res(x, y, m, rng);

    // Extract block lengths
    std::vector<std::size_t> block_lengths;
    std::size_t current_block_len = 1;
    
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        
        if (curr == (prev + 1) % static_cast<int>(n))
        {
            current_block_len++;
        }
        else
        {
            block_lengths.push_back(current_block_len);
            current_block_len = 1;
        }
    }
    block_lengths.push_back(current_block_len);  // Last block

    // Compute mean block length
    double mean_len = std::accumulate(block_lengths.begin(), block_lengths.end(), 0.0) 
                      / block_lengths.size();

    // Should be approximately L
    REQUIRE(mean_len == Catch::Approx(static_cast<double>(L)).margin(0.3));

    // Check distribution shape: P(length = k) = (1-p)^(k-1) * p where p = 1/L
    // We'll check the first few length bins
    std::map<std::size_t, std::size_t> hist;
    for (auto len : block_lengths)
    {
        if (len <= 15)  // Focus on shorter blocks for cleaner statistics
            hist[len]++;
    }

    // Expected probabilities for geometric distribution
    const double p = 1.0 / static_cast<double>(L);
    const std::size_t total_blocks = block_lengths.size();
    
    // Check that length=1 occurs with approximately probability p
    if (hist[1] > 0)
    {
        double obs_p1 = static_cast<double>(hist[1]) / total_blocks;
        double exp_p1 = p;
        // Allow generous margin due to sampling variability
        REQUIRE(obs_p1 == Catch::Approx(exp_p1).margin(0.1));
    }
}

TEST_CASE("StationaryMaskIndexResampler: block starts are uniformly distributed", "[Resampler][Index][Statistics][Uniformity]")
{
    const std::size_t n = 100;
    randutils::seed_seq_fe128 seed{7777u, 8888u, 9999u, 1111u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 10000;
    const std::size_t L = 10;
    StationaryMaskIndexResampler res(L);
    std::vector<std::size_t> idx;
    res(n, idx, m, rng);

    // Identify block starts (where previous is not i-1)
    std::vector<std::size_t> start_indices;
    start_indices.push_back(idx[0]);  // First is always a start
    
    for (std::size_t t = 1; t < m; ++t)
    {
        if (idx[t] != (idx[t-1] + 1) % n)
        {
            start_indices.push_back(idx[t]);
        }
    }

    // Build histogram of start positions
    std::vector<std::size_t> hist(n, 0);
    for (auto start : start_indices)
    {
        hist[start]++;
    }

    // Each position should be chosen roughly equally
    const double expected_per_bin = static_cast<double>(start_indices.size()) / n;
    
    // Chi-square-like test: all bins should be within reasonable range
    for (std::size_t i = 0; i < n; ++i)
    {
        // Allow 3x deviation from expected (very generous for statistical test)
        REQUIRE(hist[i] < 3.0 * expected_per_bin);
        REQUIRE(hist[i] > 0.33 * expected_per_bin);
    }
}

TEST_CASE("make_restart_mask: restarts are independent across positions", "[Resampler][Mask][Statistics][Independence]")
{
    randutils::seed_seq_fe128 seed{1234u, 5678u, 9012u, 3456u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 1000;
    const double L = 5.0;
    const int trials = 500;

    // Count joint occurrences: both t and t+1 are restarts
    std::size_t both_restart = 0;
    std::size_t t_restart_only = 0;
    std::size_t t1_restart_only = 0;
    std::size_t neither_restart = 0;

    for (int trial = 0; trial < trials; ++trial)
    {
        auto mask = make_restart_mask(m, L, rng);
        
        // Check positions 50 and 51 (arbitrary mid-sequence positions)
        const std::size_t pos = 50;
        if (mask[pos] && mask[pos+1])
            both_restart++;
        else if (mask[pos] && !mask[pos+1])
            t_restart_only++;
        else if (!mask[pos] && mask[pos+1])
            t1_restart_only++;
        else
            neither_restart++;
    }

    // If independent, probabilities should be:
    // P(both) = p^2
    // P(t only) = p(1-p)
    // P(t+1 only) = (1-p)p
    // P(neither) = (1-p)^2
    const double p = 1.0 / L;
    const double N = static_cast<double>(trials);
    
    const double exp_both = N * p * p;
    const double exp_one = N * p * (1.0 - p);
    const double exp_neither = N * (1.0 - p) * (1.0 - p);

    // Allow generous margins for binomial variability
    REQUIRE(both_restart == Catch::Approx(exp_both).margin(0.5 * exp_both + 10));
    REQUIRE(t_restart_only == Catch::Approx(exp_one).margin(0.3 * exp_one + 10));
    REQUIRE(t1_restart_only == Catch::Approx(exp_one).margin(0.3 * exp_one + 10));
    REQUIRE(neither_restart == Catch::Approx(exp_neither).margin(0.2 * exp_neither + 10));
}

// ============================================================================
// SECTION 7: Stress tests and edge cases
// ============================================================================

TEST_CASE("StationaryMaskValueResampler: m >> n with various L values", "[Resampler][Value][Stress]")
{
    const std::size_t n = 20;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    const std::size_t m = 5000;  // 250x larger than n

    randutils::seed_seq_fe128 seed{11111u, 22222u, 33333u, 44444u};

    SECTION("L = 1")
    {
        randutils::mt19937_rng rng(seed);
        StationaryMaskValueResampler<D> res(1);
        std::vector<D> y;
        res(x, y, m, rng);
        REQUIRE(y.size() == m);
    }

    SECTION("L = n/2")
    {
        randutils::mt19937_rng rng(seed);
        StationaryMaskValueResampler<D> res(n/2);
        std::vector<D> y;
        res(x, y, m, rng);
        REQUIRE(y.size() == m);
    }

    SECTION("L = n")
    {
        randutils::mt19937_rng rng(seed);
        StationaryMaskValueResampler<D> res(n);
        std::vector<D> y;
        res(x, y, m, rng);
        REQUIRE(y.size() == m);
    }

    SECTION("L = 10*n")
    {
        randutils::mt19937_rng rng(seed);
        StationaryMaskValueResampler<D> res(10*n);
        std::vector<D> y;
        res(x, y, m, rng);
        REQUIRE(y.size() == m);
    }
}

TEST_CASE("StationaryBlockValueResampler: very short blocks with L=1", "[Resampler][BlockValue][L=1]")
{
    const std::size_t n = 100;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{99999u, 88888u, 77777u, 66666u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 1000;
    StationaryBlockValueResampler<D> res(1);  // Geometric(p=1) → always length 1
    std::vector<D> y;
    res(x, y, m, rng);

    REQUIRE(y.size() == m);

    // With L=1, blocks have geometric distribution with p=1
    // Expected block length = 1/p = 1
    // So we should see very few (ideally zero) continuations
    std::size_t continuations = 0;
    for (std::size_t t = 1; t < m; ++t)
    {
        int prev = static_cast<int>(num::to_double(y[t-1]));
        int curr = static_cast<int>(num::to_double(y[t]));
        if (curr == (prev + 1) % static_cast<int>(n))
            continuations++;
    }

    // With geometric(p=1), only blocks of length 1 occur
    // So continuations are purely coincidental (prob = 1/n)
    const double p_coincidence = 1.0 / n;
    const double expected = (m - 1) * p_coincidence;
    const double sigma = std::sqrt((m-1) * p_coincidence * (1.0 - p_coincidence));
    
    REQUIRE(std::abs(static_cast<double>(continuations) - expected) < 5.0 * sigma);
}

TEST_CASE("All resamplers: n=2 minimum case", "[Resampler][Edge][MinimumN]")
{
    const std::size_t n = 2;
    std::vector<D> x = {D(0), D(1)};
    
    randutils::seed_seq_fe128 seed{12345u, 67890u, 11111u, 22222u};
    
    const std::size_t m = 100;
    const std::size_t L = 3;

    SECTION("StationaryMaskValueResampler")
    {
        randutils::mt19937_rng rng(seed);
        StationaryMaskValueResampler<D> res(L);
        std::vector<D> y;
        res(x, y, m, rng);
        REQUIRE(y.size() == m);
        
        for (const auto& v : y)
        {
            const int vi = static_cast<int>(num::to_double(v));
            REQUIRE((vi == 0 || vi == 1));
        }
    }

    SECTION("StationaryMaskIndexResampler")
    {
        randutils::mt19937_rng rng(seed);
        StationaryMaskIndexResampler res(L);
        std::vector<std::size_t> idx;
        res(n, idx, m, rng);
        REQUIRE(idx.size() == m);
        
        for (auto i : idx)
        {
            REQUIRE((i == 0 || i == 1));
        }
    }

    SECTION("StationaryBlockValueResampler")
    {
        randutils::mt19937_rng rng(seed);
        StationaryBlockValueResampler<D> res(L);
        std::vector<D> y;
        res(x, y, m, rng);
        REQUIRE(y.size() == m);
        
        for (const auto& v : y)
        {
            const int vi = static_cast<int>(num::to_double(v));
            REQUIRE((vi == 0 || vi == 1));
        }
    }
}

TEST_CASE("make_restart_mask: handles extremely large L without underflow", 
          "[Resampler][Mask][NumericalStability]")
{
    randutils::seed_seq_fe128 seed{12345u, 67890u, 11111u, 22222u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 1000;

    SECTION("L = 1e15 (near underflow threshold)")
    {
        const double L = 1e15;
        auto mask = make_restart_mask(m, L, rng);
        
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        
        // With such large L, we expect very few or zero restarts after position 0
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        
        // Should be close to 1 (only the initial restart)
        // Allow a few restarts due to random chance
        REQUIRE(restarts <= 5);
    }

    SECTION("L = 1e16 (should trigger epsilon protection)")
    {
        const double L = 1e16;
        auto mask = make_restart_mask(m, L, rng);
        
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        
        // With epsilon protection, should be exactly one block
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts == 1);  // Only the initial restart
        
        // Verify all other positions are 0
        for (std::size_t t = 1; t < m; ++t)
        {
            REQUIRE(mask[t] == 0u);
        }
    }

    SECTION("L = 1e20 (far beyond epsilon)")
    {
        const double L = 1e20;
        auto mask = make_restart_mask(m, L, rng);
        
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        
        // Should definitely trigger one-block behavior
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts == 1);
        
        // All non-initial positions should be 0
        for (std::size_t t = 1; t < m; ++t)
        {
            REQUIRE(mask[t] == 0u);
        }
    }

    SECTION("L = max safe double")
    {
        const double L = std::numeric_limits<double>::max() / 2.0;  // Still finite
        auto mask = make_restart_mask(m, L, rng);
        
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        
        // Should produce one block
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts == 1);
    }
}

TEST_CASE("make_restart_mask: threshold behavior around epsilon boundary", 
          "[Resampler][Mask][NumericalStability][Boundary]")
{
    randutils::seed_seq_fe128 seed{99999u, 88888u, 77777u, 66666u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 10000;  // Large sample for statistics
    
    // epsilon * 10 ≈ 2.22e-15
    // 1/L = epsilon * 10 → L ≈ 4.5e14
    
    SECTION("L just below threshold (should use Bernoulli)")
    {
        const double L = 1e14;  // p = 1e-14, above threshold
        auto mask = make_restart_mask(m, L, rng);
        
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        
        // With p = 1e-14 and m = 10000, expected restarts ≈ 1 + 10000 * 1e-14 ≈ 1.0001
        // So we should see approximately 1 restart (just the initial one)
        // But Bernoulli should still be used, so might occasionally see 2
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts <= 3);  // Very unlikely to see more
    }

    SECTION("L just above threshold (should trigger epsilon protection)")
    {
        const double L = 5e15;  // p = 2e-16, below threshold
        auto mask = make_restart_mask(m, L, rng);
        
        REQUIRE(mask.size() == m);
        REQUIRE(mask[0] == 1u);
        
        // Should use epsilon protection → exactly one block
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        REQUIRE(restarts == 1);
    }
}

TEST_CASE("make_restart_mask: no undefined behavior with extreme L", 
          "[Resampler][Mask][NumericalStability][Safety]")
{
    randutils::seed_seq_fe128 seed{11111u, 22222u, 33333u, 44444u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 100;

    // These should not crash, throw unexpected exceptions, or cause UB
    
    REQUIRE_NOTHROW(make_restart_mask(m, 1e10, rng));
    REQUIRE_NOTHROW(make_restart_mask(m, 1e15, rng));
    REQUIRE_NOTHROW(make_restart_mask(m, 1e20, rng));
    REQUIRE_NOTHROW(make_restart_mask(m, 1e100, rng));
    
    // Near double max (but still finite)
    const double near_max = std::numeric_limits<double>::max() / 10.0;
    REQUIRE_NOTHROW(make_restart_mask(m, near_max, rng));
}

// ============================================================================
// Tests for Exception Throwing Fix (Issue 2)
// ============================================================================

TEST_CASE("StationaryBlockValueResampler: throws exception for L < 1", 
          "[Resampler][BlockValue][Exception]")
{
    SECTION("L = 0 throws")
    {
        REQUIRE_THROWS_AS(StationaryBlockValueResampler<D>(0), 
                          std::invalid_argument);
        
        // Verify exception message
        try
        {
            StationaryBlockValueResampler<D> res(0);
            FAIL("Expected exception was not thrown");
        }
        catch (const std::invalid_argument& e)
        {
            std::string msg = e.what();
            REQUIRE(msg.find("L must be >= 1") != std::string::npos);
        }
    }

    SECTION("Constructor with L = 1 succeeds")
    {
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(1));
        
        StationaryBlockValueResampler<D> res(1);
        REQUIRE(res.getL() == 1);
    }

    SECTION("Constructor with L > 1 succeeds")
    {
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(5));
        
        StationaryBlockValueResampler<D> res(5);
        REQUIRE(res.getL() == 5);
    }
}

TEST_CASE("StationaryBlockValueResampler: consistent exception behavior with mask resamplers", 
          "[Resampler][BlockValue][Consistency]")
{
    using palvalidator::resampling::StationaryMaskValueResampler;
    using palvalidator::resampling::StationaryMaskIndexResampler;

    // All three resamplers should throw for L < 1
    
    SECTION("All throw for L = 0")
    {
        REQUIRE_THROWS_AS(StationaryBlockValueResampler<D>(0), 
                          std::invalid_argument);
        REQUIRE_THROWS_AS(StationaryMaskValueResampler<D>(0), 
                          std::invalid_argument);
        REQUIRE_THROWS_AS(StationaryMaskIndexResampler(0), 
                          std::invalid_argument);
    }

    SECTION("All succeed for L = 1")
    {
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(1));
        REQUIRE_NOTHROW(StationaryMaskValueResampler<D>(1));
        REQUIRE_NOTHROW(StationaryMaskIndexResampler(1));
    }

    SECTION("All report correct L value")
    {
        const std::size_t L = 7;
        
        StationaryBlockValueResampler<D> r1(L);
        StationaryMaskValueResampler<D> r2(L);
        StationaryMaskIndexResampler r3(L);
        
        REQUIRE(r1.getL() == L);
        REQUIRE(r2.getL() == L);
        REQUIRE(r3.getL() == L);
    }
}

// ============================================================================
// Integration tests: verify fixes don't break normal operation
// ============================================================================

TEST_CASE("Numerical stability fix: normal L values work correctly", 
          "[Resampler][Mask][Integration]")
{
    randutils::seed_seq_fe128 seed{55555u, 66666u, 77777u, 88888u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 2000;

    SECTION("Small L values (L = 2)")
    {
        const double L = 2.0;
        auto mask = make_restart_mask(m, L, rng);
        
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        const double expected = m * 0.5;
        const double sigma = std::sqrt(m * 0.5 * 0.5);
        
        REQUIRE(std::abs(static_cast<double>(restarts) - expected) < 5.0 * sigma);
    }

    SECTION("Medium L values (L = 10)")
    {
        const double L = 10.0;
        auto mask = make_restart_mask(m, L, rng);
        
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        const double expected = m * 0.1;
        const double sigma = std::sqrt(m * 0.1 * 0.9);
        
        REQUIRE(std::abs(static_cast<double>(restarts) - expected) < 5.0 * sigma);
    }

    SECTION("Large but reasonable L values (L = 100)")
    {
        const double L = 100.0;
        auto mask = make_restart_mask(m, L, rng);
        
        std::size_t restarts = std::count(mask.begin(), mask.end(), 1u);
        const double expected = m * 0.01;
        const double sigma = std::sqrt(m * 0.01 * 0.99);
        
        REQUIRE(std::abs(static_cast<double>(restarts) - expected) < 5.0 * sigma);
    }
}

TEST_CASE("Exception throwing fix: normal operation unaffected", 
          "[Resampler][BlockValue][Integration]")
{
    const std::size_t n = 100;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{12121u, 23232u, 34343u, 45454u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 500;

    SECTION("L = 1 produces valid output")
    {
        StationaryBlockValueResampler<D> res(1);
        std::vector<D> y;
        
        REQUIRE_NOTHROW(res(x, y, m, rng));
        REQUIRE(y.size() == m);
        
        // All values in range
        for (const auto& v : y)
        {
            const double vd = num::to_double(v);
            REQUIRE(vd >= 0.0);
            REQUIRE(vd < static_cast<double>(n));
        }
    }

    SECTION("L = 5 produces valid output")
    {
        StationaryBlockValueResampler<D> res(5);
        std::vector<D> y;
        
        REQUIRE_NOTHROW(res(x, y, m, rng));
        REQUIRE(y.size() == m);
        
        // All values in range
        for (const auto& v : y)
        {
            const double vd = num::to_double(v);
            REQUIRE(vd >= 0.0);
            REQUIRE(vd < static_cast<double>(n));
        }
    }

    SECTION("Large L = 1000 produces valid output")
    {
        StationaryBlockValueResampler<D> res(1000);
        std::vector<D> y;
        
        REQUIRE_NOTHROW(res(x, y, m, rng));
        REQUIRE(y.size() == m);
        
        // All values in range
        for (const auto& v : y)
        {
            const double vd = num::to_double(v);
            REQUIRE(vd >= 0.0);
            REQUIRE(vd < static_cast<double>(n));
        }
    }
}

// ============================================================================
// Regression tests: ensure fixes don't introduce new bugs
// ============================================================================

TEST_CASE("Numerical stability: deterministic behavior preserved", 
          "[Resampler][Mask][Regression]")
{
    // Very large L should produce deterministic "one block" behavior
    randutils::seed_seq_fe128 seed1{99u, 88u, 77u, 66u};
    randutils::seed_seq_fe128 seed2{99u, 88u, 77u, 66u};
    
    randutils::mt19937_rng rng1(seed1);
    randutils::mt19937_rng rng2(seed2);

    const std::size_t m = 1000;
    const double L = 1e20;  // Triggers epsilon protection

    auto mask1 = make_restart_mask(m, L, rng1);
    auto mask2 = make_restart_mask(m, L, rng2);

    // Should be identical (deterministic)
    REQUIRE(mask1 == mask2);
    
    // Should both be "one block"
    REQUIRE(std::count(mask1.begin(), mask1.end(), 1u) == 1);
    REQUIRE(std::count(mask2.begin(), mask2.end(), 1u) == 1);
}

TEST_CASE("Exception throwing: no impact on valid L values", 
          "[Resampler][BlockValue][Regression]")
{
    // Verify that adding validation doesn't change behavior for valid inputs
    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{77777u, 88888u, 99999u, 11111u};
    randutils::mt19937_rng rng1(seed);
    randutils::mt19937_rng rng2(seed);

    const std::size_t m = 300;
    const std::size_t L = 5;

    StationaryBlockValueResampler<D> res(L);
    std::vector<D> y1, y2;

    res(x, y1, m, rng1);
    res(x, y2, m, rng2);

    // With identical seeds, output should be identical
    REQUIRE(y1 == y2);
    REQUIRE(y1.size() == m);
}

// ============================================================================
// Documentation tests: verify examples from fix documentation
// ============================================================================

TEST_CASE("Documentation example: epsilon threshold at ~4.5e14", 
          "[Resampler][Mask][Documentation]")
{
    randutils::seed_seq_fe128 seed{10101u, 20202u, 30303u, 40404u};
    randutils::mt19937_rng rng(seed);

    const std::size_t m = 1000;
    
    // epsilon * 10 ≈ 2.22e-15
    // Threshold L ≈ 1 / (2.22e-15) ≈ 4.5e14
    
    SECTION("L = 1e14 (well below threshold)")
    {
        auto mask = make_restart_mask(m, 1e14, rng);
        // Should use Bernoulli (may have > 1 restart, though unlikely)
        // Not guaranteed, but very likely to be == 1 with this p
        REQUIRE(mask[0] == 1u);
    }

    SECTION("L = 1e15 (above threshold)")
    {
        auto mask = make_restart_mask(m, 1e15, rng);
        // Should trigger epsilon protection
        REQUIRE(std::count(mask.begin(), mask.end(), 1u) == 1);
    }
}

TEST_CASE("Documentation example: migration from clamping to exception", 
          "[Resampler][BlockValue][Documentation]")
{
    SECTION("Old code with L=0 no longer works")
    {
        // OLD: StationaryBlockValueResampler<D> res(0);  // Silently clamped to 1
        // NEW: Must use L=1 explicitly
        
        REQUIRE_THROWS_AS(StationaryBlockValueResampler<D>(0), 
                          std::invalid_argument);
        
        // Correct usage:
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(1));
    }

    SECTION("Valid L values work as before")
    {
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(1));
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(5));
        REQUIRE_NOTHROW(StationaryBlockValueResampler<D>(100));
    }
}