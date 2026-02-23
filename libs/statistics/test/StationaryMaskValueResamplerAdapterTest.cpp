#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "StationaryMaskResamplers.h" 
#include "number.h"
#include "randutils.hpp"
#include "TradeResampling.h"

using mkc_timeseries::Trade;
using palvalidator::resampling::StationaryMaskValueResampler;
using palvalidator::resampling::StationaryMaskIndexResampler;

// Bring the adapter into scope (defined in StationaryMaskResamplers.h per our patch)
template <class Decimal, class Rng = randutils::mt19937_rng>
using MaskValueResamplerAdapter = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal, Rng>;

TEST_CASE("StationaryMaskValueResamplerAdapter::operator() matches value-resampler output under identical RNG", "[Resampler][Adapter][operator()]")
{
    using D = num::DefaultNumber;

    // Monotone source so we can reason about indices/values easily
    const std::size_t n = 300;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    const std::size_t m = n;   // typical bootstrap replicate = same length
    const std::size_t L = 5;   // mean block length

    // Identical RNG seeds so both paths consume the same stream
    randutils::seed_seq_fe128 seed{2025u, 10u, 31u, 99u};
    randutils::mt19937_rng rng_val(seed);
    randutils::mt19937_rng rng_adp(seed);

    // Baseline: value-resampler
    StationaryMaskValueResampler<D> valRes(L);
    std::vector<D> y_val;
    valRes(x, y_val, m, rng_val);

    // Adapter wraps value-resampler but returns by value and exposes jackknife
    MaskValueResamplerAdapter<D> adp(L);
    std::vector<D> y_adp = adp(x, m, rng_adp);

    REQUIRE(y_adp.size() == m);
    REQUIRE(y_val.size() == m);
    // With identical RNG and same implementation underneath, outputs should match exactly
    REQUIRE(y_adp == y_val);
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife on constant series",
          "[Resampler][Adapter][Jackknife]")
{
    using D = num::DefaultNumber;

    // Constant series → any delete-block jackknife mean equals the constant,
    // regardless of which block is deleted or how many pseudo-values are produced.
    const std::size_t n = 64;
    const D c = D(3.14159);
    std::vector<D> x(n, c);

    MaskValueResamplerAdapter<D> adp(/*L=*/4);

    auto meanFn = [](const std::vector<D>& v) -> D {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    // Non-overlapping Künsch jackknife: numBlocks = floor(n / L_eff)
    // n=64, L=4 → L_eff = min(4, 64-2) = 4 → numBlocks = floor(64/4) = 16
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min<std::size_t>(adp.getL(), n - minKeep); // 4
    const std::size_t numBlocks = n / L_eff;                                        // 16
    REQUIRE(jk.size() == numBlocks);

    // Every pseudo-value must equal the constant regardless of which block
    // was deleted — the mean of any subset of a constant series is that constant.
    for (const auto& v : jk)
        REQUIRE(num::to_double(v) ==
                Catch::Approx(num::to_double(c)).epsilon(1e-12));
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife with large L clamps to n-minKeep",
          "[Resampler][Adapter][Jackknife][Edge]")
{
    using D = num::DefaultNumber;

    // n=8, L=1000 → L_eff = min(1000, n - minKeep) = min(1000, 6) = 6
    // keep=2, numBlocks = floor(8/6) = 1
    //
    // Old behaviour clamped to n-1=7, giving keep=1 — degenerate: statistics
    // such as Sharpe or variance are undefined on a single observation, and
    // the test was explicitly verifying that broken single-element regime.
    // The new minKeep=2 clamp guarantees keep >= 2 for all valid inputs.
    const std::size_t n = 8;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(static_cast<int>(i))); // x = [0,1,2,3,4,5,6,7]

    MaskValueResamplerAdapter<D> adp(/*L=*/1000);

    auto meanFn = [](const std::vector<D>& v) -> D {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    // numBlocks = floor(n / L_eff) = floor(8/6) = 1
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min<std::size_t>(adp.getL(), n - minKeep); // 6
    const std::size_t numBlocks = n / L_eff;                                        // 1
    REQUIRE(jk.size() == numBlocks);

    // b=0: start=0, delete [0,1,2,3,4,5], start_keep=6
    //      tail = min(2, 8-6) = 2, head = 0 → y = [6, 7], mean = 6.5
    const double expected = 6.5;
    REQUIRE(num::to_double(jk[0]) == Catch::Approx(expected).epsilon(1e-12));
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife returns floor(n/L) finite stats and shows variation on monotone series",
          "[Resampler][Adapter][Jackknife][Sanity]")
{
    using D = num::DefaultNumber;

    // n=101, L=6 → L_eff = min(6, 101-2) = 6, numBlocks = floor(101/6) = 16
    // Odd length chosen for asymmetry; numBlocks=16 with 5 observations unused
    // (101 mod 6 = 5) — this is expected and correct for non-overlapping blocks.
    const std::size_t n = 101;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(static_cast<int>(i)));

    MaskValueResamplerAdapter<D> adp(/*L=*/6);

    auto meanFn = [](const std::vector<D>& v) -> D {
        double s = 0.0;
        for (const auto& z : v) s += num::to_double(z);
        return D(s / static_cast<double>(v.size()));
    };

    const std::vector<D> jk = adp.jackknife(x, meanFn);

    // Non-overlapping Künsch jackknife: floor(n / L_eff) = floor(101/6) = 16
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min<std::size_t>(adp.getL(), n - minKeep); // 6
    const std::size_t numBlocks = n / L_eff;                                        // 16
    REQUIRE(jk.size() == numBlocks);

    // All pseudo-values must be finite and within the data range [0, 100]
    double minv = +std::numeric_limits<double>::infinity();
    double maxv = -std::numeric_limits<double>::infinity();
    for (const auto& z : jk)
    {
        const double d = num::to_double(z);
        REQUIRE(std::isfinite(d));
        REQUIRE(d >= 0.0);
        REQUIRE(d <= 100.0);
        minv = std::min(minv, d);
        maxv = std::max(maxv, d);
    }

    // Variation: deleting different non-overlapping blocks from a monotone series
    // produces different means — earlier blocks have lower values, later blocks
    // have higher values, so pseudo-values must not all be identical.
    REQUIRE(maxv > minv);
}

TEST_CASE("StationaryMaskValueResamplerAdapter: L=1 reports correctly", "[Resampler][Adapter][L=1][Report]")
{
    using D = num::DefaultNumber;

    // Construct adapter with L=1. After the ctor change, it should report exactly 1.
    MaskValueResamplerAdapter<D> adp(/*L=*/1);

    // These accessors are provided by the adapter (forwarding the inner resampler's config)
    REQUIRE(adp.getL() == 1);
    REQUIRE(adp.meanBlockLen() == 1);
}

TEST_CASE("StationaryMaskValueResamplerAdapter: L=1 yields IID-like no-continuation", "[Resampler][Adapter][L=1][IID]")
{
    using D = num::DefaultNumber;

    // Monotone source so we can detect block continuation via value adjacency.
    const std::size_t n = 997; // prime length to avoid trivial periodic artifacts
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    const std::size_t m = 5000; // long replicate to get a good signal
    MaskValueResamplerAdapter<D> adp(/*L=*/1);

    randutils::seed_seq_fe128 seed{2025u, 11u, 12u, 1u};
    randutils::mt19937_rng rng(seed);

    // Generate one long replicate
    std::vector<D> y = adp(x, m, rng);
    REQUIRE(y.size() == m);

    // Count how many times the resampler "continues" the previous block:
    // for monotone x, continuation means y[k] == (y[k-1] + 1) % n
    std::size_t continuations = 0;
    for (std::size_t k = 1; k < m; ++k)
    {
        const int prev = static_cast<int>(num::to_double(y[k-1]));
        const int curr = static_cast<int>(num::to_double(y[k]));
        const int cont = (prev + 1) % static_cast<int>(n);
        if (curr == cont) ++continuations;
    }

    // With L=1, every step is a restart → indices at t-1 and t are independent uniforms.
    // There's still a 1/n chance the new start equals (prev+1)%n purely by coincidence.
    // Model as Binomial(m-1, p=1/n) and allow a generous sigma band.
    const double p  = 1.0 / static_cast<double>(n);
    const double N  = static_cast<double>(m - 1);
    const double mu = N * p;
    const double sd = std::sqrt(N * p * (1.0 - p));
    // 6σ is very generous and should be plenty stable across platforms/seeds.
    REQUIRE(std::abs(static_cast<double>(continuations) - mu) <= 6.0 * sd);
}

TEST_CASE("StationaryMaskValueResamplerAdapter::operator() works with Trade<Decimal>",
          "[Resampler][Adapter][Trade][operator()]")
{
    using D = num::DefaultNumber;
    
    // Create a sample of 50 trades with varying durations (1-5 bars each)
    std::vector<Trade<D>> trades;
    trades.reserve(50);
    
    for (std::size_t i = 0; i < 50; ++i) {
        std::size_t duration = 1 + (i % 5);  // 1-5 bars
        std::vector<D> returns;
        for (std::size_t j = 0; j < duration; ++j) {
            returns.push_back(D(0.01 + 0.001 * static_cast<int>(i)));
        }
        trades.emplace_back(std::move(returns));
    }
    
    const std::size_t m = 50;  // Resample to same size
    const std::size_t L = 3;
    
    randutils::seed_seq_fe128 seed{2025u, 12u, 1u, 42u};
    randutils::mt19937_rng rng(seed);
    
    // Create adapter for Trade<D> — this tests template instantiation
    MaskValueResamplerAdapter<Trade<D>> adapter(L);
    
    // Resample
    std::vector<Trade<D>> resampled = adapter(trades, m, rng);
    
    REQUIRE(resampled.size() == m);
    
    // Verify all resampled trades are valid (non-empty returns)
    for (const auto& trade : resampled) {
        REQUIRE(trade.getDuration() > 0);
        REQUIRE(!trade.getDailyReturns().empty());
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife with Trade->Decimal statistic",
          "[Resampler][Adapter][Trade][Jackknife]")
{
    using D = num::DefaultNumber;
    
    // Create constant-return trades: every trade has identical returns.
    // This allows us to predict the jackknife output precisely, similar to
    // the constant-series test for bar-level bootstrap.
    const std::size_t n = 60;
    std::vector<Trade<D>> trades;
    trades.reserve(n);
    
    const D constantReturn = D(0.05);
    for (std::size_t i = 0; i < n; ++i) {
        // Each trade: 2 bars, both with return = 0.05
        std::vector<D> returns = {constantReturn, constantReturn};
        trades.emplace_back(std::move(returns));
    }
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/3);
    
    // Statistic: mean of flattened returns (Trade<D> -> D)
    // This tests the GENERIC jackknife overload, not the traditional one,
    // because the return type (D) differs from the sample type (Trade<D>).
    auto meanFlattenedStat = [](const std::vector<Trade<D>>& sampledTrades) -> D {
        std::vector<D> flatReturns;
        for (const auto& trade : sampledTrades) {
            const auto& returns = trade.getDailyReturns();
            flatReturns.insert(flatReturns.end(), returns.begin(), returns.end());
        }
        
        if (flatReturns.empty()) return D(0);
        
        double sum = 0.0;
        for (const auto& r : flatReturns) {
            sum += num::to_double(r);
        }
        return D(sum / flatReturns.size());
    };
    
    const std::vector<D> jk = adapter.jackknife(trades, meanFlattenedStat);
    
    // Non-overlapping Künsch jackknife: numBlocks = floor(n / L_eff)
    // n=60, L=3 → L_eff = min(3, 60-2) = 3 → numBlocks = floor(60/3) = 20
    const std::size_t minKeep = 2;
    const std::size_t L_eff = std::min<std::size_t>(adapter.getL(), n - minKeep);
    const std::size_t numBlocks = n / L_eff;
    
    REQUIRE(jk.size() == numBlocks);
    REQUIRE(jk.size() == 20);
    
    // Every pseudo-value should equal the constant (0.05) because deleting
    // any block of constant-return trades still leaves all-constant returns
    for (const auto& v : jk) {
        REQUIRE(num::to_double(v) == Catch::Approx(0.05).epsilon(1e-12));
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::jackknife shows variation on heterogeneous trades",
          "[Resampler][Adapter][Trade][Jackknife][Variation]")
{
    using D = num::DefaultNumber;
    
    // Create trades with increasing returns: early trades have low returns,
    // later trades have high returns. Deleting different blocks should
    // produce different jackknife statistics — analogous to the monotone-series
    // test for bar-level bootstrap.
    const std::size_t n = 90;
    std::vector<Trade<D>> trades;
    trades.reserve(n);
    
    for (std::size_t i = 0; i < n; ++i) {
        // Return increases with trade index
        D tradeReturn = D(0.001 * static_cast<int>(i));
        std::vector<D> returns = {tradeReturn, tradeReturn};
        trades.emplace_back(std::move(returns));
    }
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/6);
    
    // Statistic: mean of flattened returns
    auto meanStat = [](const std::vector<Trade<D>>& sampledTrades) -> D {
        std::vector<D> flatReturns;
        for (const auto& trade : sampledTrades) {
            const auto& returns = trade.getDailyReturns();
            flatReturns.insert(flatReturns.end(), returns.begin(), returns.end());
        }
        
        if (flatReturns.empty()) return D(0);
        
        double sum = 0.0;
        for (const auto& r : flatReturns) sum += num::to_double(r);
        return D(sum / flatReturns.size());
    };
    
    const std::vector<D> jk = adapter.jackknife(trades, meanStat);
    
    // n=90, L=6 → L_eff = min(6, 90-2) = 6 → numBlocks = floor(90/6) = 15
    const std::size_t expectedBlocks = 15;
    REQUIRE(jk.size() == expectedBlocks);
    
    // Find min and max pseudo-values
    double minv = +std::numeric_limits<double>::infinity();
    double maxv = -std::numeric_limits<double>::infinity();
    
    for (const auto& v : jk) {
        double d = num::to_double(v);
        REQUIRE(std::isfinite(d));
        minv = std::min(minv, d);
        maxv = std::max(maxv, d);
    }
    
    // Variation: deleting early blocks (low returns) should produce higher
    // jackknife stats than deleting later blocks (high returns)
    REQUIRE(maxv > minv);
}

TEST_CASE("StationaryMaskValueResamplerAdapter::Trade type preserves trade structure",
          "[Resampler][Adapter][Trade][Structure]")
{
    using D = num::DefaultNumber;
    
    // Create distinctive trades so we can verify structure preservation.
    // Each resampled trade should match one of the originals exactly —
    // no partial trades or corrupted structures.
    std::vector<Trade<D>> trades;
    
    // Trade 0: 3-bar winner
    trades.emplace_back(std::vector<D>{D(0.02), D(0.03), D(0.01)});
    
    // Trade 1: 2-bar loser
    trades.emplace_back(std::vector<D>{D(-0.01), D(-0.02)});
    
    // Trade 2: 1-bar winner
    trades.emplace_back(std::vector<D>{D(0.05)});
    
    // Trade 3: 4-bar mixed
    trades.emplace_back(std::vector<D>{D(0.01), D(-0.01), D(0.02), D(-0.01)});
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/2);
    
    randutils::seed_seq_fe128 seed{2025u, 12u, 2u, 1u};
    randutils::mt19937_rng rng(seed);
    
    // Resample
    std::vector<Trade<D>> resampled = adapter(trades, 10, rng);
    
    REQUIRE(resampled.size() == 10);
    
    // Each resampled trade should match one of the original trades exactly.
    // Trade-level resampling preserves complete trade structure (frozen path).
    for (const auto& resampledTrade : resampled) {
        bool matchesOriginal = false;
        
        for (const auto& originalTrade : trades) {
            if (resampledTrade == originalTrade) {
                matchesOriginal = true;
                break;
            }
        }
        
        REQUIRE(matchesOriginal);
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::Trade jackknife with small sample",
          "[Resampler][Adapter][Trade][Jackknife][SmallSample]")
{
    using D = num::DefaultNumber;
    
    // Small sample: n=20 (minimum realistic size per specification).
    // Must not crash or produce degenerate results.
    const std::size_t n = 20;
    std::vector<Trade<D>> trades;
    trades.reserve(n);
    
    for (std::size_t i = 0; i < n; ++i) {
        std::vector<D> returns = {D(0.02)};
        trades.emplace_back(std::move(returns));
    }
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/3);
    
    auto countTradesStat = [](const std::vector<Trade<D>>& sampledTrades) -> D {
        return D(static_cast<double>(sampledTrades.size()));
    };
    
    const std::vector<D> jk = adapter.jackknife(trades, countTradesStat);
    
    // n=20, L=3 → L_eff = min(3, 20-2) = 3 → numBlocks = floor(20/3) = 6
    REQUIRE(jk.size() == 6);
    
    // Each pseudo-value should be n - L_eff = 20 - 3 = 17
    for (const auto& v : jk) {
        REQUIRE(num::to_double(v) == Catch::Approx(17.0).epsilon(1e-12));
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::Trade with large L clamps to n-minKeep",
          "[Resampler][Adapter][Trade][Jackknife][Edge]")
{
    using D = num::DefaultNumber;
    
    // n=10, L=1000 → L_eff = min(1000, 10-2) = 8
    // This mirrors the bar-level test but with trades.
    std::vector<Trade<D>> trades;
    for (std::size_t i = 0; i < 10; ++i) {
        trades.emplace_back(std::vector<D>{D(static_cast<double>(i))});
    }
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/1000);
    
    auto sumStat = [](const std::vector<Trade<D>>& t) -> D {
        double sum = 0.0;
        for (const auto& trade : t) {
            for (const auto& r : trade.getDailyReturns()) {
                sum += num::to_double(r);
            }
        }
        return D(sum);
    };
    
    auto jk = adapter.jackknife(trades, sumStat);
    
    // n=10, L_eff=8 → numBlocks = floor(10/8) = 1
    REQUIRE(jk.size() == 1);
    
    // The one pseudo-value deletes trades [0,1,2,3,4,5,6,7]
    // Keeps trades [8,9]
    // Sum = 8.0 + 9.0 = 17.0
    REQUIRE(num::to_double(jk[0]) == Catch::Approx(17.0).epsilon(1e-12));
}

TEST_CASE("StationaryMaskValueResamplerAdapter: Trade L=1 yields IID-like no-continuation",
          "[Resampler][Adapter][Trade][L=1][IID]")
{
    using D = num::DefaultNumber;
    
    // Create distinctive trades with unique signatures so we can detect
    // block continuation vs. independent draws.
    std::vector<Trade<D>> trades;
    for (std::size_t i = 0; i < 100; ++i) {
        D uniqueReturn = D(static_cast<double>(i));
        trades.emplace_back(std::vector<D>{uniqueReturn});
    }
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/1);
    
    randutils::seed_seq_fe128 seed{2025u, 12u, 3u, 1u};
    randutils::mt19937_rng rng(seed);
    
    // Generate long replicate to get good signal
    std::vector<Trade<D>> resampled = adapter(trades, 5000, rng);
    
    REQUIRE(resampled.size() == 5000);
    
    // With L=1, each position is an independent random draw.
    // Count sequential duplicates (same trade appears twice in a row).
    std::size_t consecutiveDuplicates = 0;
    for (std::size_t i = 1; i < resampled.size(); ++i) {
        if (resampled[i] == resampled[i-1]) {
            ++consecutiveDuplicates;
        }
    }
    
    // Expected: p = 1/100 (chance of drawing same trade by coincidence)
    // Binomial(4999, 1/100) → mean ≈ 50, sd ≈ 7
    const double p = 1.0 / 100.0;
    const double N = 4999.0;
    const double mu = N * p;
    const double sd = std::sqrt(N * p * (1.0 - p));
    
    // 6σ band, same as bar-level L=1 test
    REQUIRE(std::abs(static_cast<double>(consecutiveDuplicates) - mu) <= 6.0 * sd);
}

TEST_CASE("StationaryMaskValueResamplerAdapter::Trade jackknife delegates to generic overload",
          "[Resampler][Adapter][Trade][Jackknife][Overload]")
{
    using D = num::DefaultNumber;
    
    // This test explicitly verifies that the GENERIC jackknife overload
    // (the template<class StatFunc> version) is selected when the statistic
    // returns a different type than the sample type.
    
    std::vector<Trade<D>> trades;
    for (std::size_t i = 0; i < 30; ++i) {
        trades.emplace_back(std::vector<D>{D(0.01), D(0.02)});
    }
    
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/3);
    
    // Lambda: Trade<D> -> D (different return type)
    // This MUST call the generic overload, not the traditional one.
    auto lambda = [](const std::vector<Trade<D>>& t) -> D {
        return D(static_cast<double>(t.size()));
    };
    
    // This should compile and call the generic overload via SFINAE
    auto jk = adapter.jackknife(trades, lambda);
    
    // Verify result type is std::vector<D>, not std::vector<Trade<D>>
    static_assert(std::is_same<decltype(jk), std::vector<D>>::value,
                  "Jackknife should return std::vector<D> for Trade->D statistic");
    
    // n=30, L=3 → numBlocks = 10
    REQUIRE(jk.size() == 10);
    
    // Each pseudo-value should be 30 - 3 = 27
    for (const auto& v : jk) {
        REQUIRE(num::to_double(v) == Catch::Approx(27.0).epsilon(1e-12));
    }
}

TEST_CASE("StationaryMaskValueResamplerAdapter::Trade simulates computeFromTrades pattern",
          "[Resampler][Adapter][Trade][Integration]")
{
    using D = num::DefaultNumber;
    
    // Simulate the real-world pattern where a statistic class has both
    // operator()(const vector<D>&) and computeFromTrades(const vector<Trade<D>>&).
    // This is how production code will use trade-level bootstrap.
    
    struct MockLogProfitFactorStat
    {
        D operator()(const std::vector<D>& flatReturns) const
        {
            // Simplified: just compute mean of returns
            if (flatReturns.empty()) return D(0);
            double sum = 0.0;
            for (const auto& r : flatReturns) sum += num::to_double(r);
            return D(sum / flatReturns.size());
        }
        
        D computeFromTrades(const std::vector<Trade<D>>& trades) const
        {
            std::vector<D> flatReturns;
            for (const auto& trade : trades) {
                const auto& returns = trade.getDailyReturns();
                flatReturns.insert(flatReturns.end(), returns.begin(), returns.end());
            }
            return (*this)(flatReturns);  // Delegate to operator()
        }
    };
    
    // Create sample trades
    std::vector<Trade<D>> trades;
    for (std::size_t i = 0; i < 27; ++i) {
        std::vector<D> returns = {D(0.02), D(0.03)};
        trades.emplace_back(std::move(returns));
    }
    
    MockLogProfitFactorStat stat;
    MaskValueResamplerAdapter<Trade<D>> adapter(/*L=*/3);
    
    // Lambda wrapping computeFromTrades (real-world pattern)
    auto tradeStat = [&stat](const std::vector<Trade<D>>& t) -> D {
        return stat.computeFromTrades(t);
    };
    
    auto jk = adapter.jackknife(trades, tradeStat);
    
    // n=27, L=3 → numBlocks = 9
    REQUIRE(jk.size() == 9);
    
    // All pseudo-values should be (0.02 + 0.03) / 2 = 0.025
    for (const auto& v : jk) {
        REQUIRE(num::to_double(v) == Catch::Approx(0.025).epsilon(1e-12));
    }
}
