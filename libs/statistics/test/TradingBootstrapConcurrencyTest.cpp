// TradingBootstrapFactoryConcurrencyTest.cpp
//
// Tests for the Executor template parameter added to all makeBCa() overloads
// in TradingBootstrapFactory.
//
// The BCaBootStrap-level executor contracts (valid intervals, exact boot_stats
// count, Efron diagnostics, degenerate collapse, etc.) are fully covered by
// BCaBootStrapConcurrencyTest.cpp. These tests focus exclusively on the
// factory layer: do the right overloads compile, do they forward the executor
// correctly, and does the CRN hierarchy remain unaffected by executor choice?
//
// Test organisation:
//
//  §F1  All four bar-level overloads compile and produce valid intervals with
//       ThreadPoolExecutor<0> (auto-sized).
//         §F1a  custom stat  + BacktesterStrategy
//         §F1b  default stat + BacktesterStrategy
//         §F1c  custom stat  + raw strategy ID
//         §F1d  default stat + raw strategy ID
//
//  §F2  CRN determinism is preserved across executor types.
//       SingleThreadExecutor vs ThreadPoolExecutor<0> with the same factory
//       call parameters must produce bit-identical bounds. The executor is a
//       pure scheduling concern; it must not enter the CRN key or affect
//       which replicate engine is assigned to replicate b.
//         §F2a  default stat + BacktesterStrategy
//         §F2b  custom stat  + raw strategy ID
//
//  §F3  ThreadPoolExecutor<N> for fixed N flows through correctly.
//       Spot-checks N=4 to cover the fixed-thread-count specialisation.
//
//  §F4  Trade-level makeBCa overloads with a non-default executor.
//         §F4a  explicit statFn + BacktesterStrategy
//         §F4b  explicit statFn + raw strategy ID

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <numeric>
#include <cmath>
#include <functional>

#include "TradingBootstrapFactory.h"
#include "BiasCorrectedBootstrap.h"
#include "ParallelExecutors.h"
#include "RngUtils.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"
#include "StatUtils.h"
#include "PalStrategy.h"

using DecimalType = num::DefaultNumber;
using D           = DecimalType;
using Eng         = randutils::mt19937_rng;
using Resamp      = StationaryBlockResampler<D, Eng>;

// ---------------------------------------------------------------------------
// Shared test fixtures
// ---------------------------------------------------------------------------

namespace
{
    // Mildly autocorrelated return series (same pattern used throughout the
    // existing factory tests for consistency). n = 200.
    static std::vector<D> buildReturns()
    {
        std::vector<D> r;
        r.reserve(200);
        for (int k = 0; k < 40; ++k)
        {
            r.push_back(createDecimal("0.004"));
            r.push_back(createDecimal("0.004"));
            r.push_back(createDecimal("-0.003"));
            r.push_back(createDecimal("-0.003"));
            r.push_back(createDecimal("0.002"));
        }
        return r;
    }

    // Trimmed-mean statistic used to exercise the custom-statFn overloads.
    static D trimmedMean(const std::vector<D>& x)
    {
        if (x.size() <= 2)
            return mkc_timeseries::StatUtils<D>::computeMean(x);
        std::vector<D> y = x;
        std::sort(y.begin(), y.end(),
                  [](const D& a, const D& b){ return num::to_double(a) < num::to_double(b); });
        y.erase(y.begin());
        y.pop_back();
        return mkc_timeseries::StatUtils<D>::computeMean(y);
    }

    // Structural validity helper.
    template<class BcaT>
    void requireValidBCaInterval(BcaT& bca)
    {
        const auto lo = bca.getLowerBound();
        const auto mu = bca.getMean();
        const auto hi = bca.getUpperBound();
        REQUIRE(std::isfinite(num::to_double(lo)));
        REQUIRE(std::isfinite(num::to_double(mu)));
        REQUIRE(std::isfinite(num::to_double(hi)));
        REQUIRE(lo <= hi);
        REQUIRE(mu >= lo);
        REQUIRE(mu <= hi);
    }

    // Helpers to build the PalStrategy objects used for BacktesterStrategy overloads.
    static std::shared_ptr<PriceActionLabPattern> makeLongPattern()
    {
        auto pctL = std::make_shared<D>(createDecimal("90.00"));
        auto pctS = std::make_shared<D>(createDecimal("10.00"));
        auto desc = std::make_shared<PatternDescription>(
            "TestPattern.txt", 1, 20200101, pctL, pctS, 10, 1);

        auto o5  = std::make_shared<PriceBarOpen>(5);
        auto c5  = std::make_shared<PriceBarClose>(5);
        auto gt1 = std::make_shared<GreaterThanExpr>(o5, c5);

        auto c6  = std::make_shared<PriceBarClose>(6);
        auto gt2 = std::make_shared<GreaterThanExpr>(c5, c6);
        auto pat = std::make_shared<AndExpr>(gt1, gt2);

        return std::make_shared<PriceActionLabPattern>(
            desc, pat,
            std::make_shared<LongMarketEntryOnOpen>(),
            std::make_shared<LongSideProfitTargetInPercent>(
                std::make_shared<D>(createDecimal("2.00"))),
            std::make_shared<LongSideStopLossInPercent>(
                std::make_shared<D>(createDecimal("1.00"))));
    }

    static auto makeLongStrategy()
    {
        auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("TestPortfolio");
        mkc_timeseries::StrategyOptions opts(false, 0, 0);
        return mkc_timeseries::makePalStrategy<D>("TestStrategy", makeLongPattern(), portfolio, opts);
    }

} // anonymous namespace

// ===========================================================================
// §F1 — All four bar-level overloads compile and produce valid intervals
//        with ThreadPoolExecutor<0>
// ===========================================================================

TEST_CASE("TradingBootstrapFactory[Executor] §F1a: "
          "custom stat + BacktesterStrategy — ThreadPoolExecutor<0>",
          "[Factory][makeBCa][Executor][ThreadPool]")
{
    using Exec = concurrency::ThreadPoolExecutor<0>;

    const auto returns    = buildReturns();
    auto       strategy   = makeLongStrategy();
    const uint64_t seed   = 0xAABBCCDDEEFF0011ull;
    const uint64_t stage  = 1;
    const unsigned L = 3, B = 800;

    TradingBootstrapFactory<Eng> factory(seed);

    auto bca = factory.makeBCa<D, Resamp, Exec>(
        returns, B, 0.95,
        std::function<D(const std::vector<D>&)>(trimmedMean),
        Resamp(L), *strategy, stage, L, 0u);

    requireValidBCaInterval(bca);
}

TEST_CASE("TradingBootstrapFactory[Executor] §F1b: "
          "default stat + BacktesterStrategy — ThreadPoolExecutor<0>",
          "[Factory][makeBCa][Executor][ThreadPool]")
{
    using Exec = concurrency::ThreadPoolExecutor<0>;

    const auto returns    = buildReturns();
    auto       strategy   = makeLongStrategy();
    const uint64_t seed   = 0x1122334455667788ull;
    const uint64_t stage  = 1;
    const unsigned L = 3, B = 800;

    TradingBootstrapFactory<Eng> factory(seed);

    // Convenience overload: no statFn argument — factory injects computeMean.
    auto bca = factory.makeBCa<D, Resamp, Exec>(
        returns, B, 0.95,
        Resamp(L), *strategy, stage, L, 0u);

    requireValidBCaInterval(bca);
}

TEST_CASE("TradingBootstrapFactory[Executor] §F1c: "
          "custom stat + raw strategy ID — ThreadPoolExecutor<0>",
          "[Factory][makeBCa][Executor][ThreadPool]")
{
    using Exec = concurrency::ThreadPoolExecutor<0>;

    const auto   returns    = buildReturns();
    const uint64_t seed     = 0x99AABBCCDDEEFF00ull;
    const uint64_t stratId  = 0x0102030405060708ull;
    const uint64_t stage    = 1;
    const unsigned L = 3, B = 800;

    TradingBootstrapFactory<Eng> factory(seed);

    auto bca = factory.makeBCa<D, Resamp, Exec>(
        returns, B, 0.95,
        std::function<D(const std::vector<D>&)>(trimmedMean),
        Resamp(L), stratId, stage, L, 0u);

    requireValidBCaInterval(bca);
}

TEST_CASE("TradingBootstrapFactory[Executor] §F1d: "
          "default stat + raw strategy ID — ThreadPoolExecutor<0>",
          "[Factory][makeBCa][Executor][ThreadPool]")
{
    using Exec = concurrency::ThreadPoolExecutor<0>;

    const auto   returns    = buildReturns();
    const uint64_t seed     = 0xFEDCBA9876543210ull;
    const uint64_t stratId  = 0xA1B2C3D4E5F60718ull;
    const uint64_t stage    = 1;
    const unsigned L = 3, B = 800;

    TradingBootstrapFactory<Eng> factory(seed);

    // Convenience overload: no statFn argument.
    auto bca = factory.makeBCa<D, Resamp, Exec>(
        returns, B, 0.95,
        Resamp(L), stratId, stage, L, 0u);

    requireValidBCaInterval(bca);
}

// ===========================================================================
// §F2 — CRN determinism is preserved across executor types.
//
//  The factory builds a CRNEngineProvider keyed by
//      masterSeed → strategyHash → stageTag → BCA → L → fold.
//  The Executor parameter is forwarded to BCaBootStrap<..., Executor> but
//  never enters the CRN key.  Therefore SingleThread and ThreadPool must
//  deliver bit-identical bounds (same per-replicate engines, same indexed
//  writes, same prop_less/prop_equal counts).
// ===========================================================================

TEST_CASE("TradingBootstrapFactory[Executor] §F2a: "
          "CRN determinism preserved — default stat + BacktesterStrategy, "
          "SingleThread vs ThreadPool",
          "[Factory][makeBCa][Executor][CRN][Determinism]")
{
    using ExecST = concurrency::SingleThreadExecutor;
    using ExecTP = concurrency::ThreadPoolExecutor<0>;

    const auto returns   = buildReturns();
    auto       strategy  = makeLongStrategy();
    const uint64_t seed  = 0xDEADBEEFCAFEBABEull;
    const uint64_t stage = 1;
    const unsigned L = 3, B = 600;

    // Two independent factories sharing the same master seed.
    TradingBootstrapFactory<Eng> factoryST(seed);
    TradingBootstrapFactory<Eng> factoryTP(seed);

    auto bcaST = factoryST.makeBCa<D, Resamp, ExecST>(
        returns, B, 0.95, Resamp(L), *strategy, stage, L, 0u);

    auto bcaTP = factoryTP.makeBCa<D, Resamp, ExecTP>(
        returns, B, 0.95, Resamp(L), *strategy, stage, L, 0u);

    // Bounds must be bit-identical: same CRN key → same per-replicate engines
    // → same stat_b values written to the same indices → same bias-correction.
    REQUIRE(num::to_double(bcaST.getLowerBound()) ==
            Catch::Approx(num::to_double(bcaTP.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getUpperBound()) ==
            Catch::Approx(num::to_double(bcaTP.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getMean()) ==
            Catch::Approx(num::to_double(bcaTP.getMean())).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory[Executor] §F2b: "
          "CRN determinism preserved — custom stat + raw strategy ID, "
          "SingleThread vs ThreadPool",
          "[Factory][makeBCa][Executor][CRN][Determinism]")
{
    using ExecST = concurrency::SingleThreadExecutor;
    using ExecTP = concurrency::ThreadPoolExecutor<0>;

    const auto   returns   = buildReturns();
    const uint64_t seed    = 0xBADC0FFEE0DDF00Dull;
    const uint64_t stratId = 0xF00DFACE12345678ull;
    const uint64_t stage   = 2;
    const unsigned L = 4, B = 600;

    TradingBootstrapFactory<Eng> factoryST(seed);
    TradingBootstrapFactory<Eng> factoryTP(seed);

    auto statFn = std::function<D(const std::vector<D>&)>(trimmedMean);

    auto bcaST = factoryST.makeBCa<D, Resamp, ExecST>(
        returns, B, 0.95, statFn, Resamp(L), stratId, stage, L, 0u);

    auto bcaTP = factoryTP.makeBCa<D, Resamp, ExecTP>(
        returns, B, 0.95, statFn, Resamp(L), stratId, stage, L, 0u);

    REQUIRE(num::to_double(bcaST.getLowerBound()) ==
            Catch::Approx(num::to_double(bcaTP.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getUpperBound()) ==
            Catch::Approx(num::to_double(bcaTP.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getMean()) ==
            Catch::Approx(num::to_double(bcaTP.getMean())).epsilon(0));
}

// ===========================================================================
// §F3 — ThreadPoolExecutor<N> for fixed N flows through correctly.
// ===========================================================================

TEST_CASE("TradingBootstrapFactory[Executor] §F3: "
          "ThreadPoolExecutor<4> — valid interval and CRN-matches SingleThread",
          "[Factory][makeBCa][Executor][ThreadPool][FixedN]")
{
    using ExecST = concurrency::SingleThreadExecutor;
    using ExecT4 = concurrency::ThreadPoolExecutor<4>;

    const auto   returns   = buildReturns();
    const uint64_t seed    = 0x1234ABCD5678EF90ull;
    const uint64_t stratId = 0xBEEFCAFEDEADBABEull;
    const uint64_t stage   = 1;
    const unsigned L = 3, B = 700;

    TradingBootstrapFactory<Eng> factoryST(seed);
    TradingBootstrapFactory<Eng> factoryT4(seed);

    auto bcaST = factoryST.makeBCa<D, Resamp, ExecST>(
        returns, B, 0.95, Resamp(L), stratId, stage, L, 0u);

    auto bcaT4 = factoryT4.makeBCa<D, Resamp, ExecT4>(
        returns, B, 0.95, Resamp(L), stratId, stage, L, 0u);

    // Structural validity for the fixed-N pool.
    requireValidBCaInterval(bcaT4);

    // CRN determinism: same key → bit-identical bounds regardless of N.
    REQUIRE(num::to_double(bcaST.getLowerBound()) ==
            Catch::Approx(num::to_double(bcaT4.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getUpperBound()) ==
            Catch::Approx(num::to_double(bcaT4.getUpperBound())).epsilon(0));
}

// ===========================================================================
// §F4 — Trade-level makeBCa overloads with a non-default executor.
//
//  The trade-level overloads (SampleType = Trade<Decimal>) were updated in the
//  same refactor.  These tests confirm they compile correctly with a non-default
//  Executor and that CRN determinism still holds.
// ===========================================================================

TEST_CASE("TradingBootstrapFactory[Executor] §F4a: "
          "trade-level, explicit statFn + BacktesterStrategy — ThreadPoolExecutor<0>",
          "[Factory][makeBCa][Executor][TradeLevelBCa][ThreadPool]")
{
    using TradeT    = mkc_timeseries::Trade<D>;
    using TradeResamp = IIDResampler<TradeT, Eng>;
    using ExecST    = concurrency::SingleThreadExecutor;
    using ExecTP    = concurrency::ThreadPoolExecutor<0>;

    // Build a small synthetic trade vector.  Each trade holds one daily return
    // (the simplest valid Trade — a single-element return sequence).
    std::vector<TradeT> trades;
    for (int i = 0; i < 50; ++i)
    {
        double pnl = (i % 5 == 0) ? -0.008 : 0.003;
        trades.emplace_back(TradeT(std::vector<D>{ D(pnl) }));
    }

    // Trade-level statistic: mean of the first (only) daily return per trade.
    // getDailyReturns() returns the internal return sequence; accumulate over
    // all bars in all trades to get the total mean P&L.
    auto tradeStatFn = std::function<D(const std::vector<TradeT>&)>(
        [](const std::vector<TradeT>& tv) -> D {
            double sum = 0.0;
            std::size_t n = 0;
            for (const auto& t : tv)
            {
                for (const auto& r : t.getDailyReturns())
                {
                    sum += num::to_double(r);
                    ++n;
                }
            }
            return n > 0 ? D(sum / static_cast<double>(n)) : D(0.0);
        });

    auto strategy    = makeLongStrategy();
    const uint64_t seed  = 0xCAFEF00DBAADF00Dull;
    const uint64_t stage = 3;
    const unsigned L = 1, B = 500;

    TradingBootstrapFactory<Eng> factoryST(seed);
    TradingBootstrapFactory<Eng> factoryTP(seed);

    TradeResamp sampler;

    auto bcaST = factoryST.makeBCa<D, TradeResamp, ExecST>(
        trades, B, 0.95, tradeStatFn, sampler, *strategy, stage, L, 0u);

    auto bcaTP = factoryTP.makeBCa<D, TradeResamp, ExecTP>(
        trades, B, 0.95, tradeStatFn, sampler, *strategy, stage, L, 0u);

    // Both must produce structurally valid intervals.
    requireValidBCaInterval(bcaST);
    requireValidBCaInterval(bcaTP);

    // CRN determinism: executor must not affect the key or the bounds.
    REQUIRE(num::to_double(bcaST.getLowerBound()) ==
            Catch::Approx(num::to_double(bcaTP.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getUpperBound()) ==
            Catch::Approx(num::to_double(bcaTP.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getMean()) ==
            Catch::Approx(num::to_double(bcaTP.getMean())).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory[Executor] §F4b: "
          "trade-level, explicit statFn + raw strategy ID — ThreadPoolExecutor<0>",
          "[Factory][makeBCa][Executor][TradeLevelBCa][ThreadPool]")
{
    using TradeT      = mkc_timeseries::Trade<D>;
    using TradeResamp = IIDResampler<TradeT, Eng>;
    using ExecST      = concurrency::SingleThreadExecutor;
    using ExecTP      = concurrency::ThreadPoolExecutor<0>;

    std::vector<TradeT> trades;
    for (int i = 0; i < 50; ++i)
    {
        double pnl = (i % 5 == 0) ? -0.008 : 0.003;
        trades.emplace_back(TradeT(std::vector<D>{ D(pnl) }));
    }

    auto tradeStatFn = std::function<D(const std::vector<TradeT>&)>(
        [](const std::vector<TradeT>& tv) -> D {
            double sum = 0.0;
            std::size_t n = 0;
            for (const auto& t : tv)
            {
                for (const auto& r : t.getDailyReturns())
                {
                    sum += num::to_double(r);
                    ++n;
                }
            }
            return n > 0 ? D(sum / static_cast<double>(n)) : D(0.0);
        });

    const uint64_t seed    = 0x0F1E2D3C4B5A6978ull;
    const uint64_t stratId = 0x8899AABBCCDDEEFFull;
    const uint64_t stage   = 3;
    const unsigned L = 1, B = 500;

    TradingBootstrapFactory<Eng> factoryST(seed);
    TradingBootstrapFactory<Eng> factoryTP(seed);

    TradeResamp sampler;

    auto bcaST = factoryST.makeBCa<D, TradeResamp, ExecST>(
        trades, B, 0.95, tradeStatFn, sampler, stratId, stage, L, 0u);

    auto bcaTP = factoryTP.makeBCa<D, TradeResamp, ExecTP>(
        trades, B, 0.95, tradeStatFn, sampler, stratId, stage, L, 0u);

    requireValidBCaInterval(bcaST);
    requireValidBCaInterval(bcaTP);

    REQUIRE(num::to_double(bcaST.getLowerBound()) ==
            Catch::Approx(num::to_double(bcaTP.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getUpperBound()) ==
            Catch::Approx(num::to_double(bcaTP.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bcaST.getMean()) ==
            Catch::Approx(num::to_double(bcaTP.getMean())).epsilon(0));
}
