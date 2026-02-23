// MOutOfNPercentileBootstrapTradeTest.cpp
//
// Catch2 unit tests validating MOutOfNPercentileBootstrap<Decimal, Sampler,
// Resampler, Rng, Executor, SampleType = Trade<Decimal>> — the trade-level
// specialisation added alongside the bar-level (SampleType = Decimal) path.
//
// Coverage plan
// ─────────────
// §1  Trade class and IIDResampler<Trade<Decimal>> contract (6 tests)
// §2  Template instantiation: SampleType = Trade<Decimal> compiles (1 test)
// §3  Fixed-ratio run() — RNG path (5 tests)
// §4  Fixed-ratio run() — CRN Provider path (3 tests)
// §5  Result field semantics at trade level (4 tests)
// §6  Point-estimate consistency: trade-level vs bar-level (1 test)
// §7  CI direction reflects strategy edge (3 tests)
// §8  m_sub_override in trade units (2 tests)
// §9  Confidence-level width ordering (1 test)
// §10 m_ratio width ordering (1 test)
// §11 Copy / move semantics at trade level (2 tests)
// §12 Error paths (2 tests)
// §13 Diagnostics at trade level (2 tests)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <random>
#include <functional>
#include <type_traits>

#include "number.h"
#include "StatUtils.h"
#include "TestUtils.h"
#include "StationaryMaskResamplers.h"   // StationaryMaskValueResampler (bar-level consistency test)
#include "TradeResampling.h"            // Trade<Decimal>, TradeFlatteningAdapter
#include "MOutOfNPercentileBootstrap.h"
#include "RngUtils.h"

using palvalidator::analysis::MOutOfNPercentileBootstrap;
using mkc_timeseries::rng_utils::make_seed_seq;

// ─────────────────────────────────────────────────────────────────────────────
// Type aliases
// ─────────────────────────────────────────────────────────────────────────────
using D       = DecimalType;                         // bar-level Decimal
using TradeT  = mkc_timeseries::Trade<D>;            // Trade<Decimal>

// IID resampler for Trade<Decimal>.
// Mirrors the pattern used in the bar-level tests (IIDResamplerForMOutOfN).
// getL() returns 0 (IID — no block structure).
struct TradeIIDResampler
{
    std::size_t getL() const noexcept { return 0; }

    void operator()(const std::vector<TradeT>& x,
                    std::vector<TradeT>&        y,
                    std::size_t                 m,
                    std::mt19937_64&            rng) const
    {
        std::uniform_int_distribution<std::size_t> dist(0, x.size() - 1);
        for (std::size_t i = 0; i < m; ++i)
            y[i] = x[dist(rng)];
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Build a Trade from a plain brace-initializable list of per-bar returns.
static TradeT makeTrade(std::initializer_list<double> barReturns)
{
    std::vector<D> bars;
    bars.reserve(barReturns.size());
    for (double v : barReturns)
        bars.push_back(D(v));
    return TradeT(bars);
}

/// Flatten a vector of Trade<Decimal> to a flat vector<Decimal> of all bar returns.
static std::vector<D> flattenTrades(const std::vector<TradeT>& trades)
{
    std::vector<D> bars;
    for (const auto& t : trades)
        for (const auto& r : t.getDailyReturns())
            bars.push_back(r);
    return bars;
}

/// Arithmetic mean of a vector<D>.
static D vecMean(const std::vector<D>& v)
{
    double s = 0.0;
    for (const auto& x : v)
        s += num::to_double(x);
    return D(s / static_cast<double>(v.size()));
}

// Statistic: flatten all trades in a vector, then return their arithmetic mean.
// This is the canonical "trade-level mean" statistic used throughout these tests.
static auto tradeMeanStat = [](const std::vector<TradeT>& trades) -> D
{
    return vecMean(flattenTrades(trades));
};

/// 9 consistently profitable trades (3 bars each, ~3-bar median holding period).
static std::vector<TradeT> makePositiveTrades()
{
    return {
        makeTrade({ 0.003,  0.002,  0.004}),
        makeTrade({ 0.005,  0.001,  0.003}),
        makeTrade({ 0.002,  0.004,  0.003}),
        makeTrade({ 0.006,  0.001,  0.002}),
        makeTrade({ 0.003,  0.003,  0.003}),
        makeTrade({ 0.004,  0.002,  0.001}),
        makeTrade({ 0.002,  0.005,  0.002}),
        makeTrade({ 0.001,  0.003,  0.005}),
        makeTrade({ 0.004,  0.004,  0.001}),
    };
}

/// 9 consistently losing trades.
static std::vector<TradeT> makeNegativeTrades()
{
    return {
        makeTrade({-0.003, -0.002, -0.004}),
        makeTrade({-0.005, -0.001, -0.003}),
        makeTrade({-0.002, -0.004, -0.003}),
        makeTrade({-0.006, -0.001, -0.002}),
        makeTrade({-0.003, -0.003, -0.003}),
        makeTrade({-0.004, -0.002, -0.001}),
        makeTrade({-0.002, -0.005, -0.002}),
        makeTrade({-0.001, -0.003, -0.005}),
        makeTrade({-0.004, -0.004, -0.001}),
    };
}

/// 9 mixed trades designed so the flat bar mean is exactly 0.
/// 4 strongly positive trades (+0.010/bar) and 4 strongly negative (-0.010/bar)
/// cancel perfectly; the 9th trade is neutral.  The large magnitude (±1% per bar)
/// ensures the bootstrap variance is wide enough that the CI straddles zero
/// even at the small n=9 trade count used in these tests.
static std::vector<TradeT> makeMixedTrades()
{
    return {
        makeTrade({ 0.010,  0.010,  0.010}),  // strongly positive
        makeTrade({-0.010, -0.010, -0.010}),  // strongly negative
        makeTrade({ 0.010,  0.010,  0.010}),  // strongly positive
        makeTrade({-0.010, -0.010, -0.010}),  // strongly negative
        makeTrade({ 0.000,  0.000,  0.000}),  // neutral — keeps mean = 0
        makeTrade({ 0.010,  0.010,  0.010}),  // strongly positive
        makeTrade({-0.010, -0.010, -0.010}),  // strongly negative
        makeTrade({ 0.010,  0.010,  0.010}),  // strongly positive
        makeTrade({-0.010, -0.010, -0.010}),  // strongly negative
    };
    // Flat mean: 4×(+0.030) + 4×(-0.030) + 0 = 0 over 27 bars → mean/bar = 0.0
}

// ─────────────────────────────────────────────────────────────────────────────
// §1  Trade class and IIDResampler<Trade<Decimal>> contract
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Trade<Decimal>: construction and basic accessors",
          "[TradeLevel][MOutOfN][Contract]")
{
    SECTION("Trade stores per-bar returns and getDailyReturns() round-trips them")
    {
        auto t = makeTrade({0.001, -0.002, 0.003});
        const auto& bars = t.getDailyReturns();
        REQUIRE(bars.size() == 3);
        REQUIRE(num::to_double(bars[0]) == Catch::Approx( 0.001).margin(1e-12));
        REQUIRE(num::to_double(bars[1]) == Catch::Approx(-0.002).margin(1e-12));
        REQUIRE(num::to_double(bars[2]) == Catch::Approx( 0.003).margin(1e-12));
    }

    SECTION("Two trades with identical bars compare equal")
    {
        auto t1 = makeTrade({0.01, -0.02});
        auto t2 = makeTrade({0.01, -0.02});
        REQUIRE(t1 == t2);
    }

    SECTION("Two trades with different bars are not equal")
    {
        auto t1 = makeTrade({0.01, -0.02});
        auto t2 = makeTrade({0.01, -0.03});
        // Trade has operator== but not operator!= — use negation
        const bool equal = (t1 == t2);
        REQUIRE_FALSE(equal);
    }

    SECTION("flattenTrades produces correct total bar count")
    {
        auto trades = makePositiveTrades(); // 9 trades × 3 bars
        auto bars   = flattenTrades(trades);
        REQUIRE(bars.size() == 27);
    }

    SECTION("tradeMeanStat on all-identical bars equals that bar value")
    {
        std::vector<TradeT> ts = {
            makeTrade({0.005, 0.005, 0.005}),
            makeTrade({0.005, 0.005, 0.005}),
            makeTrade({0.005, 0.005, 0.005}),
        };
        const double mean = num::to_double(tradeMeanStat(ts));
        REQUIRE(mean == Catch::Approx(0.005).margin(1e-10));
    }
}

TEST_CASE("Local TradeIIDResampler: basic mechanics",
          "[TradeLevel][MOutOfN][Contract]")
{
    TradeIIDResampler res;

    SECTION("getL() returns 0 (IID — no block structure)")
    {
        REQUIRE(res.getL() == 0);
    }

    SECTION("Resampled output has exactly m_sub trades")
    {
        auto source = makePositiveTrades(); // 9 trades
        const std::size_t m_sub = 6;

        std::vector<TradeT> y(m_sub);
        std::mt19937_64 rng(42);
        res(source, y, m_sub, rng);

        REQUIRE(y.size() == m_sub);
    }

    SECTION("All resampled trades are drawn from the source set")
    {
        auto source = makePositiveTrades();
        const std::size_t m_sub = 9;

        std::vector<TradeT> y(m_sub);
        std::mt19937_64 rng(123);
        res(source, y, m_sub, rng);

        for (const auto& t : y)
        {
            const bool found = std::any_of(source.begin(), source.end(),
                                           [&](const TradeT& s){ return s == t; });
            REQUIRE(found);
        }
    }

    SECTION("Sampling with replacement: repeats can occur")
    {
        // With 1 source trade sampled m=50 times, all outputs must equal the source.
        std::vector<TradeT> single = { makeTrade({0.01, 0.02}) };
        const std::size_t m_sub = 50;
        std::vector<TradeT> y(m_sub);
        std::mt19937_64 rng(7);
        res(single, y, m_sub, rng);

        for (const auto& t : y)
            REQUIRE(t == single[0]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §2  Template instantiation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap: trade-level template instantiation",
          "[TradeLevel][MOutOfN][Instantiation]")
{
    // The primary objective of this section is to confirm that the 6-parameter
    // template with SampleType = Trade<Decimal> compiles and constructs without
    // error.  If any of the Tier 1/2/3 changes introduced a type mismatch this
    // test would fail to compile.

    using TradeMoon = MOutOfNPercentileBootstrap<
        D,
        decltype(tradeMeanStat),
        TradeIIDResampler,
        std::mt19937_64,
        concurrency::SingleThreadExecutor,
        TradeT                              // SampleType = Trade<Decimal>
    >;

    TradeIIDResampler res;

    SECTION("Constructor compiles and isAdaptiveMode() is false for fixed ratio")
    {
        TradeMoon moon(/*B=*/400, /*CL=*/0.95, /*m_ratio=*/0.75, res);
        REQUIRE_FALSE(moon.isAdaptiveMode());
        REQUIRE(moon.mratio() == Catch::Approx(0.75));
        REQUIRE(moon.B()  == 400);
        REQUIRE(moon.CL() == Catch::Approx(0.95));
    }

    SECTION("rescalesToN() reflects constructor argument")
    {
        TradeMoon moon_plain(400, 0.95, 0.75, res, /*rescale_to_n=*/false);
        TradeMoon moon_rescale(400, 0.95, 0.75, res, /*rescale_to_n=*/true);
        REQUIRE_FALSE(moon_plain.rescalesToN());
        REQUIRE(moon_rescale.rescalesToN());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §3  Fixed-ratio run() — RNG path
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): run() basic invariants",
          "[TradeLevel][MOutOfN][Run]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();   // n=9 trades

    std::seed_seq seq = make_seed_seq(0x54524144454C564ull); // "TRADELV"
    std::mt19937_64 rng(seq);

    TradeMoon moon(/*B=*/400, /*CL=*/0.95, /*m_ratio=*/0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    SECTION("Result struct B, cl, effective_B + skipped == B")
    {
        REQUIRE(result.B  == 400);
        REQUIRE(result.cl == Catch::Approx(0.95));
        REQUIRE(result.effective_B + result.skipped == result.B);
        REQUIRE(result.effective_B >= result.B / 2); // non-degenerate majority
    }

    SECTION("lower <= mean <= upper (CI is well-ordered)")
    {
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean  <= result.upper);
    }

    SECTION("All CI bounds are finite")
    {
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }

    SECTION("CI width is strictly positive")
    {
        const double w = num::to_double(result.upper - result.lower);
        REQUIRE(w > 0.0);
    }

    SECTION("skew_boot is finite")
    {
        REQUIRE(std::isfinite(result.skew_boot));
    }
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): point estimate matches statistic on original trades",
          "[TradeLevel][MOutOfN][Run][PointEstimate]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    // Compute expected theta_hat directly
    const double expected_mean = num::to_double(tradeMeanStat(trades));

    std::seed_seq seq = make_seed_seq(0xC0FFEE0000000001ull);
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    REQUIRE(num::to_double(result.mean)
            == Catch::Approx(expected_mean).margin(1e-12));
}

// ─────────────────────────────────────────────────────────────────────────────
// §4  Fixed-ratio run() — CRN Provider path
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): CRN provider path produces same result as RNG path under identical seeds",
          "[TradeLevel][MOutOfN][CRN]")
{
    // CRN provider that mimics the per-replicate seed derivation used by the RNG path
    struct DummyCRN
    {
        std::mt19937_64 make_engine(std::size_t b) const
        {
            std::seed_seq ss{
                static_cast<unsigned>(b & 0xffffffffu),
                static_cast<unsigned>((b >> 32) & 0xffffffffu),
                0xBEEFCAFEu, 0xDEAD1234u
            };
            return std::mt19937_64(ss);
        }
    };

    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();
    DummyCRN crn;

    TradeMoon moon(400, 0.95, 0.75, res);

    // Two calls with the same CRN must produce bit-identical results
    auto r1 = moon.run(trades, tradeMeanStat, crn);
    auto r2 = moon.run(trades, tradeMeanStat, crn);

    SECTION("Point estimate is identical across two CRN runs")
    {
        REQUIRE(num::to_double(r1.mean)
                == Catch::Approx(num::to_double(r2.mean)).margin(0.0));
    }

    SECTION("Lower and upper bounds are identical across two CRN runs")
    {
        REQUIRE(num::to_double(r1.lower)
                == Catch::Approx(num::to_double(r2.lower)).margin(0.0));
        REQUIRE(num::to_double(r1.upper)
                == Catch::Approx(num::to_double(r2.upper)).margin(0.0));
    }

    SECTION("effective_B and skipped are identical across two CRN runs")
    {
        REQUIRE(r1.effective_B == r2.effective_B);
        REQUIRE(r1.skipped     == r2.skipped);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §5  Result field semantics at trade level
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): Result.n is trade count, not bar count",
          "[TradeLevel][MOutOfN][Semantics]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades(); // 9 trades × 3 bars = 27 bars

    std::seed_seq seq = make_seed_seq(0x4E434F554E543031ull);
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    // n must be 9 (trade count), NOT 27 (bar count)
    REQUIRE(result.n == trades.size());
    REQUIRE(result.n == 9);
    REQUIRE(result.n != 27);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): Result.m_sub is in trade units",
          "[TradeLevel][MOutOfN][Semantics]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades(); // n=9 trades

    std::seed_seq seq = make_seed_seq(0x4D535542543031ull);
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    // With m_ratio=0.75 and n=9 trades: m_sub = floor(0.75 * 9) = 6
    const std::size_t expected_m = static_cast<std::size_t>(std::floor(0.75 * 9.0));
    REQUIRE(result.m_sub == expected_m);
    REQUIRE(result.m_sub >= 2);
    REQUIRE(result.m_sub < result.n);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): Result.L == 0 from local IID resampler",
          "[TradeLevel][MOutOfN][Semantics]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    std::seed_seq seq = make_seed_seq(0x4C4553545F4C01ull);
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    // Local TradeIIDResampler::getL() == 0 (IID — no block structure)
    REQUIRE(result.L == 0);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): computed_ratio matches configured m_ratio",
          "[TradeLevel][MOutOfN][Semantics]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    std::seed_seq seq = make_seed_seq(0x52415449304F31ull);
    std::mt19937_64 rng(seq);

    const double m_ratio = 0.75;
    TradeMoon moon(400, 0.95, m_ratio, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    // In fixed-ratio mode, computed_ratio == the configured m_ratio (not m_sub/n)
    REQUIRE(result.computed_ratio == Catch::Approx(m_ratio).margin(0.0));
}

// ─────────────────────────────────────────────────────────────────────────────
// §6  Point-estimate consistency: trade-level vs bar-level
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): theta_hat agrees with bar-level bootstrap on same data",
          "[TradeLevel][MOutOfN][Consistency]")
{
    // The point estimate theta_hat is just the statistic applied to the full
    // original sample — it has nothing to do with resampling.  When the trade
    // statistic (flatten-then-mean) and the bar statistic (direct mean) are
    // applied to the same underlying bar data, they must produce the same value.

    using BarResampler = palvalidator::resampling::StationaryMaskValueResampler<D>;
    using BarSampler   = std::function<D(const std::vector<D>&)>;
    using BarMoon      = MOutOfNPercentileBootstrap<D, BarSampler, BarResampler>;

    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    auto trades = makePositiveTrades(); // 9 trades × 3 bars
    auto bars   = flattenTrades(trades); // 27 bars

    BarSampler barFn = [](const std::vector<D>& v) -> D { return vecMean(v); };

    BarResampler barRes(/*blockSize=*/3);

    std::seed_seq seqBar   = make_seed_seq(0xBA4B4C455631ull);
    std::seed_seq seqTrade = make_seed_seq(0x54524431ull);
    std::mt19937_64 rngBar(seqBar), rngTrade(seqTrade);

    BarMoon   barMoon(400, 0.95, 0.75, barRes);
    TradeMoon tradeMoon(400, 0.95, 0.75, TradeIIDResampler{});

    auto barResult   = barMoon.run(bars,   barFn,        rngBar);
    auto tradeResult = tradeMoon.run(trades, tradeMeanStat, rngTrade);

    // theta_hat is just statistic(original), so both must equal
    // the arithmetic mean of the same 27 bar values.
    REQUIRE(num::to_double(barResult.mean)
            == Catch::Approx(num::to_double(tradeResult.mean)).margin(1e-10));
}

// ─────────────────────────────────────────────────────────────────────────────
// §7  CI direction reflects strategy edge
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): profitable strategy has positive lower bound",
          "[TradeLevel][MOutOfN][Direction]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades(); // all returns > 0

    std::seed_seq seq = make_seed_seq(0x504F534954495645ull); // "POSITIVE"
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    // A strategy with uniformly positive trades should have a positive LB
    REQUIRE(num::to_double(result.lower) > 0.0);
    REQUIRE(num::to_double(result.mean)  > 0.0);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): losing strategy has negative upper bound",
          "[TradeLevel][MOutOfN][Direction]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makeNegativeTrades(); // all returns < 0

    std::seed_seq seq = make_seed_seq(0x4E45474154495645ull); // "NEGATIVE"
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    REQUIRE(num::to_double(result.upper) < 0.0);
    REQUIRE(num::to_double(result.mean)  < 0.0);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): mixed strategy CI straddles zero",
          "[TradeLevel][MOutOfN][Direction]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    // makeMixedTrades(): 4 positive (+0.010/bar) and 4 negative (-0.010/bar) trades
    // cancel perfectly → flat mean = 0.0.  The large magnitude ensures the bootstrap
    // distribution has substantial mass on both sides of zero.
    auto trades = makeMixedTrades();

    std::seed_seq seq = make_seed_seq(0x4D495845443031ull); // "MIXED"
    std::mt19937_64 rng(seq);

    // Use B=800 for robustness at n=9 trades.
    TradeMoon moon(800, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    // theta_hat (mean on original data) should be exactly 0
    REQUIRE(num::to_double(result.mean) == Catch::Approx(0.0).margin(1e-12));

    // CI must span zero (lower < 0, upper > 0) for a genuinely mixed strategy
    REQUIRE(num::to_double(result.lower) < 0.0);
    REQUIRE(num::to_double(result.upper) > 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// §8  m_sub_override in trade units
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): m_sub_override is in trade units",
          "[TradeLevel][MOutOfN][Override]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades(); // n=9

    SECTION("Override m_sub=5 draws 5 trades per replicate")
    {
        std::seed_seq seq = make_seed_seq(0x4F564552523031ull);
        std::mt19937_64 rng(seq);

        TradeMoon moon(400, 0.95, 0.75, res);
        const std::size_t m_override = 5;
        auto result = moon.run(trades, tradeMeanStat, rng, m_override);

        REQUIRE(result.m_sub == m_override);
        REQUIRE(result.n == trades.size());
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean  <= result.upper);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }

    SECTION("Override m_sub=2 (minimum valid) produces finite interval")
    {
        std::seed_seq seq = make_seed_seq(0x4D494E4D5331ull);
        std::mt19937_64 rng(seq);

        TradeMoon moon(400, 0.95, 0.75, res);
        auto result = moon.run(trades, tradeMeanStat, rng, /*m_sub_override=*/2);

        REQUIRE(result.m_sub == 2);
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §9  Confidence-level width ordering
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): 99% CI is wider than 95% CI",
          "[TradeLevel][MOutOfN][Width]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    struct DummyCRN {
        std::mt19937_64 make_engine(std::size_t b) const {
            std::seed_seq ss{ static_cast<unsigned>(b), 0xC0FFEE01u };
            return std::mt19937_64(ss);
        }
    } crn;

    TradeMoon moon95(400, 0.95, 0.75, res);
    TradeMoon moon99(400, 0.99, 0.75, res);

    auto r95 = moon95.run(trades, tradeMeanStat, crn);
    auto r99 = moon99.run(trades, tradeMeanStat, crn);

    const double w95 = num::to_double(r95.upper - r95.lower);
    const double w99 = num::to_double(r99.upper - r99.lower);

    REQUIRE(w99 >= w95 - 1e-12);
}

// ─────────────────────────────────────────────────────────────────────────────
// §10 m_ratio width ordering
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): smaller m_ratio produces wider CI",
          "[TradeLevel][MOutOfN][Width]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    // Use CRN so resampling draws are comparable
    struct DummyCRN {
        std::mt19937_64 make_engine(std::size_t b) const {
            std::seed_seq ss{ static_cast<unsigned>(b), 0xFACEFEEDu };
            return std::mt19937_64(ss);
        }
    } crn;

    // m_ratio=0.50 → m_sub=4 trades; m_ratio=0.88 → m_sub=7 trades
    // (9×0.50=4.5→4, 9×0.88=7.92→7)
    TradeMoon moonSmall(400, 0.95, 0.50, res);
    TradeMoon moonLarge(400, 0.95, 0.88, res);

    auto rSmall = moonSmall.run(trades, tradeMeanStat, crn);
    auto rLarge = moonLarge.run(trades, tradeMeanStat, crn);

    const double wSmall = num::to_double(rSmall.upper - rSmall.lower);
    const double wLarge = num::to_double(rLarge.upper - rLarge.lower);

    // Smaller subsample → higher variance per replicate → wider CI
    REQUIRE(wSmall >= wLarge - 1e-12);
}

// ─────────────────────────────────────────────────────────────────────────────
// §11 Copy / move semantics at trade level
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): copy constructor creates independent object",
          "[TradeLevel][MOutOfN][CopyMove]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    TradeMoon original(400, 0.95, 0.75, res);
    auto copy = original; // copy constructor

    SECTION("Copy has matching configuration")
    {
        REQUIRE(copy.B()      == original.B());
        REQUIRE(copy.CL()     == original.CL());
        REQUIRE(copy.mratio() == original.mratio());
    }

    SECTION("Diagnostics not shared: running original does not affect copy")
    {
        REQUIRE_FALSE(copy.hasDiagnostics());
        REQUIRE_FALSE(original.hasDiagnostics());

        std::seed_seq seq = make_seed_seq(0x434F5059543031ull);
        std::mt19937_64 rng(seq);
        (void)original.run(trades, tradeMeanStat, rng);

        REQUIRE(original.hasDiagnostics());
        REQUIRE_FALSE(copy.hasDiagnostics());
    }
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): move constructor transfers diagnostics",
          "[TradeLevel][MOutOfN][CopyMove]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades();

    TradeMoon original(400, 0.95, 0.75, res);

    std::seed_seq seq = make_seed_seq(0x4D4F5645543031ull);
    std::mt19937_64 rng(seq);
    (void)original.run(trades, tradeMeanStat, rng);
    REQUIRE(original.hasDiagnostics());

    auto moved = std::move(original);

    REQUIRE(moved.B()      == 400);
    REQUIRE(moved.CL()     == Catch::Approx(0.95));
    REQUIRE(moved.mratio() == Catch::Approx(0.75));
    REQUIRE(moved.hasDiagnostics()); // diagnostics transferred
}

// ─────────────────────────────────────────────────────────────────────────────
// §12 Error paths
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): n < 3 trades throws invalid_argument",
          "[TradeLevel][MOutOfN][Error]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;

    // Only 2 trades — below the n >= 3 requirement
    std::vector<TradeT> tiny = {
        makeTrade({0.01, 0.02}),
        makeTrade({-0.01, 0.03}),
    };

    std::seed_seq seq = make_seed_seq(0x45525230313ull);
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    REQUIRE_THROWS_AS(moon.run(tiny, tradeMeanStat, rng), std::invalid_argument);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): constructor validation still applies",
          "[TradeLevel][MOutOfN][Error]")
{
    TradeIIDResampler res;

    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    // B < 400
    REQUIRE_THROWS_AS(TradeMoon(399, 0.95, 0.75, res), std::invalid_argument);

    // CL out of range
    REQUIRE_THROWS_AS(TradeMoon(400, 0.5,  0.75, res), std::invalid_argument);
    REQUIRE_THROWS_AS(TradeMoon(400, 1.0,  0.75, res), std::invalid_argument);

    // m_ratio out of (0,1)
    REQUIRE_THROWS_AS(TradeMoon(400, 0.95, 0.0,  res), std::invalid_argument);
    REQUIRE_THROWS_AS(TradeMoon(400, 0.95, 1.0,  res), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// §13 Diagnostics at trade level
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): diagnostics unavailable before run()",
          "[TradeLevel][MOutOfN][Diagnostics]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    TradeMoon moon(400, 0.95, 0.75, res);

    REQUIRE_FALSE(moon.hasDiagnostics());
    REQUIRE_THROWS_AS(moon.getBootstrapStatistics(), std::logic_error);
    REQUIRE_THROWS_AS(moon.getBootstrapMean(),       std::logic_error);
    REQUIRE_THROWS_AS(moon.getBootstrapVariance(),   std::logic_error);
    REQUIRE_THROWS_AS(moon.getBootstrapSe(),         std::logic_error);
    REQUIRE_THROWS_AS(moon.getBootstrapSkewness(),   std::logic_error);
}

TEST_CASE("MOutOfNPercentileBootstrap (trade-level): diagnostics consistent with Result after run()",
          "[TradeLevel][MOutOfN][Diagnostics]")
{
    using TradeMoon = MOutOfNPercentileBootstrap<
        D, decltype(tradeMeanStat), TradeIIDResampler,
        std::mt19937_64, concurrency::SingleThreadExecutor, TradeT>;

    TradeIIDResampler res;
    auto trades = makePositiveTrades(); // 9 trades

    std::seed_seq seq = make_seed_seq(0x4449414731ull);
    std::mt19937_64 rng(seq);

    TradeMoon moon(400, 0.95, 0.75, res);
    auto result = moon.run(trades, tradeMeanStat, rng);

    REQUIRE(moon.hasDiagnostics());

    const auto& stats  = moon.getBootstrapStatistics();
    const double mean_b = moon.getBootstrapMean();
    const double var_b  = moon.getBootstrapVariance();
    const double se_b   = moon.getBootstrapSe();

    SECTION("Statistics vector size matches effective_B")
    {
        REQUIRE(stats.size() == result.effective_B);
    }

    SECTION("All diagnostic statistics are finite")
    {
        REQUIRE(std::isfinite(mean_b));
        REQUIRE(std::isfinite(var_b));
        REQUIRE(std::isfinite(se_b));
        REQUIRE(std::isfinite(moon.getBootstrapSkewness()));
    }

    SECTION("se_b == sqrt(var_b) exactly")
    {
        REQUIRE(se_b == Catch::Approx(std::sqrt(var_b)).margin(1e-12));
    }

    SECTION("Mean recomputed from raw statistics matches getBootstrapMean()")
    {
        double sum = 0.0;
        for (double v : stats) sum += v;
        const double recomputed = sum / static_cast<double>(stats.size());
        REQUIRE(mean_b == Catch::Approx(recomputed).margin(1e-12));
    }

    SECTION("All raw bootstrap statistics are finite")
    {
        for (double v : stats)
            REQUIRE(std::isfinite(v));
    }
}
