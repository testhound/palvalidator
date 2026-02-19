// GeoMeanStatTradeTests.cpp
//
// Unit tests for GeoMeanStat::operator()(const std::vector<Trade<Decimal>>& trades).
// This overload is the entry point for trade-level bootstrapping: it flattens
// the per-trade daily return sequences into a single stream and delegates to
// the existing flat-vector operator().
//
// Design goals:
//   1. Confirm that the trade overload produces exactly the same result as the
//      flat-vector overload on the same data (the fundamental contract).
//   2. Verify edge-case behaviour (empty, single-day, empty-return trades).
//   3. Confirm that options propagated through the constructor (clip_ruin, ruin_eps,
//      winsorization mode) apply identically via both overloads.
//   4. Confirm correct integration with TradeFlatteningAdapter so that the
//      bootstrap plumbing that wraps GeoMeanStat can call it via std::function.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <cmath>

#include "StatUtils.h"
#include "TradeResampling.h"
#include "TestUtils.h"        // DecimalType typedef
#include "DecimalConstants.h"
#include "number.h"           // num::to_double

using namespace mkc_timeseries;
using DC = DecimalConstants<DecimalType>;
using Stat = StatUtils<DecimalType>;
using LogPFStat = typename Stat::LogProfitFactorStat_LogPF;
using LogPFBars = typename Stat::LogProfitFactorFromLogBarsStat_LogPF;

// ---------------------------------------------------------------------------
// Internal helper: build a flat return vector by concatenating Trade returns,
// so we can independently verify what the trade overload should produce.
// ---------------------------------------------------------------------------
namespace
{
  std::vector<DecimalType> flattenTrades(const std::vector<Trade<DecimalType>>& trades)
  {
    std::vector<DecimalType> flat;
    for (const auto& t : trades)
    {
      const auto& d = t.getDailyReturns();
      flat.insert(flat.end(), d.begin(), d.end());
    }
    return flat;
  }

  /// Build a single Trade whose "daily returns" are log-growth values derived
  /// from the provided raw returns using makeLogGrowthSeries.
  Trade<DecimalType> makeLogTrade(const std::vector<DecimalType>& rawReturns,
                                   double ruin_eps = 1e-8)
  {
    return Trade<DecimalType>(StatUtils<DecimalType>::makeLogGrowthSeries(rawReturns, ruin_eps));
  }

  /// Build a vector of single-bar log-Trades from a flat raw-return vector.
  /// Each raw return becomes its own one-bar Trade holding log(max(1+r, eps)).
  std::vector<Trade<DecimalType>>
  makeOneBarLogTrades(const std::vector<DecimalType>& rawReturns,
                      double ruin_eps = 1e-8)
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(rawReturns.size());
    for (const auto& r : rawReturns)
      trades.push_back(makeLogTrade({ r }, ruin_eps));
    return trades;
  }

  /// Build a vector of single-bar Trades from a flat raw-return vector.
  std::vector<Trade<DecimalType>>
  makeOneBarTrades(const std::vector<DecimalType>& returns)
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(returns.size());
    for (const auto& r : returns)
      trades.emplace_back(std::vector<DecimalType>{ r });
    return trades;
  }
} // anonymous namespace


// =============================================================================
// TEST SUITE 1: Fundamental equivalence between overloads
//
// The core contract: operator()(trades) == operator()(flattenTrades(trades))
// for every configuration of GeoMeanStat.
// =============================================================================

TEST_CASE("GeoMeanStat trade overload: equivalence with flat-vector overload",
          "[StatUtils][GeoMean][Trade][Equivalence]")
{
  // Absolute tolerance matching the main GeoMeanStat test suite.
  constexpr double kTol = 5e-8;

  SECTION("Single-bar trades are identical to a plain flat-vector call")
  {
    // Each trade holds exactly one daily return – the degenerate case where
    // trade structure adds no complexity.
    std::vector<Trade<DecimalType>> trades = {
      Trade<DecimalType>(std::vector<DecimalType>{ DecimalType("0.10") }),
      Trade<DecimalType>(std::vector<DecimalType>{ DecimalType("-0.05") }),
      Trade<DecimalType>(std::vector<DecimalType>{ DecimalType("0.20") }),
      Trade<DecimalType>(std::vector<DecimalType>{ DecimalType("-0.10") }),
      Trade<DecimalType>(std::vector<DecimalType>{ DecimalType("0.15") }),
    };

    GeoMeanStat<DecimalType> stat;

    DecimalType via_trades = stat(trades);
    DecimalType via_flat   = stat(flattenTrades(trades));

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(via_flat)).margin(kTol));
  }

  SECTION("Multi-bar trades: flattening preserves order and count")
  {
    // Three trades, each with a different number of daily bars.
    // Trade A: 2 bars, Trade B: 3 bars, Trade C: 1 bar  →  6 total log-returns.
    Trade<DecimalType> tradeA({ DecimalType("0.01"), DecimalType("0.02") });
    Trade<DecimalType> tradeB({ DecimalType("-0.03"), DecimalType("0.04"), DecimalType("0.01") });
    Trade<DecimalType> tradeC({ DecimalType("0.05") });

    std::vector<Trade<DecimalType>> trades = { tradeA, tradeB, tradeC };

    GeoMeanStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Uniform multi-bar trades (8 bars each, realistic holding period)")
  {
    // Simulate 10 trades, each holding 8 bars – a plausible intraday profile.
    std::vector<DecimalType> bar_template = {
      DecimalType("0.002"), DecimalType("0.001"), DecimalType("-0.001"),
      DecimalType("0.003"), DecimalType("0.000"), DecimalType("-0.002"),
      DecimalType("0.001"), DecimalType("0.002")
    };

    std::vector<Trade<DecimalType>> trades;
    for (int i = 0; i < 10; ++i)
      trades.emplace_back(bar_template);   // copy-construct each trade

    GeoMeanStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Mixed wins and losses across trade boundaries")
  {
    // Arrange: one winning trade, one losing trade, one breakeven trade.
    // The boundary between trades must not affect the geometric mean.
    Trade<DecimalType> winner({ DecimalType("0.05"), DecimalType("0.03"), DecimalType("0.02") });
    Trade<DecimalType> loser ({ DecimalType("-0.04"), DecimalType("-0.02") });
    Trade<DecimalType> flat  ({ DecimalType("0.00"), DecimalType("0.00") });

    std::vector<Trade<DecimalType>> trades = { winner, loser, flat };

    GeoMeanStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }
}


// =============================================================================
// TEST SUITE 2: Edge cases
// =============================================================================

TEST_CASE("GeoMeanStat trade overload: edge cases",
          "[StatUtils][GeoMean][Trade][EdgeCases]")
{
  SECTION("Empty trade vector returns DecimalZero")
  {
    std::vector<Trade<DecimalType>> trades; // no trades at all

    GeoMeanStat<DecimalType> stat;
    DecimalType result = stat(trades);

    REQUIRE(result == DC::DecimalZero);
  }

  SECTION("Single trade, single bar")
  {
    // Geometric mean of one return r is just r.
    Trade<DecimalType> t({ DecimalType("0.07") });
    std::vector<Trade<DecimalType>> trades = { t };

    GeoMeanStat<DecimalType> stat;
    DecimalType result = stat(trades);

    // geo_mean of {0.07} = exp(log(1.07)) - 1 = 0.07
    REQUIRE(num::to_double(result) == Catch::Approx(0.07).margin(5e-8));
  }

  SECTION("Single trade with multiple constant returns equals that constant")
  {
    // If every bar has return r, the geometric mean must equal r.
    const DecimalType r("0.05");
    std::vector<DecimalType> bars(6, r);

    std::vector<Trade<DecimalType>> trades = {
      Trade<DecimalType>(bars)
    };

    GeoMeanStat<DecimalType> stat;
    DecimalType result = stat(trades);

    REQUIRE(num::to_double(result) == Catch::Approx(0.05).margin(5e-8));
  }

  SECTION("Trade built via addReturn() matches trade built from vector")
  {
    // Verify that the incremental-construction path produces the same Trade
    // as the vector-constructor path, so both reach the same operator() result.
    std::vector<DecimalType> rets = {
      DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03")
    };

    Trade<DecimalType> by_vector(rets);

    Trade<DecimalType> by_add;
    for (const auto& r : rets)
      by_add.addReturn(r);

    GeoMeanStat<DecimalType> stat;

    REQUIRE(num::to_double(stat({ by_vector })) ==
            Catch::Approx(num::to_double(stat({ by_add }))).margin(1e-12));
  }

  SECTION("Trade with all-zero returns yields geometric mean of zero")
  {
    std::vector<DecimalType> zeros(10, DecimalType("0.0"));
    std::vector<Trade<DecimalType>> trades = { Trade<DecimalType>(zeros) };

    GeoMeanStat<DecimalType> stat;
    DecimalType result = stat(trades);

    REQUIRE(num::to_double(result) == Catch::Approx(0.0).margin(5e-8));
  }
}


// =============================================================================
// TEST SUITE 3: Constructor options propagate correctly
//
// clip_ruin, ruin_eps, winsorization mode and alpha must apply identically
// via the trade overload and the flat-vector overload.
// =============================================================================

TEST_CASE("GeoMeanStat trade overload: constructor options propagate",
          "[StatUtils][GeoMean][Trade][Options]")
{
  constexpr double kTol = 5e-8;

  SECTION("clip_ruin=false throws domain_error when a bar returns -1")
  {
    // A trade containing a total-ruin bar should propagate the exception
    // thrown by the underlying flat-vector operator().
    Trade<DecimalType> t({
      DecimalType("0.05"),
      DecimalType("-1.0"),  // ruin bar
      DecimalType("0.03")
    });
    std::vector<Trade<DecimalType>> trades = { t };

    GeoMeanStat<DecimalType> strict_stat(/*clip_ruin=*/false);

    REQUIRE_THROWS_AS(strict_stat(trades), std::domain_error);
  }

  SECTION("clip_ruin=true does not throw on ruin bar and matches flat overload")
  {
    const double eps = 1e-8;
    Trade<DecimalType> t({
      DecimalType("0.05"),
      DecimalType("-1.0"),  // will be clamped to eps
      DecimalType("0.03")
    });
    std::vector<Trade<DecimalType>> trades = { t };

    GeoMeanStat<DecimalType> clip_stat(/*clip_ruin=*/true, /*ruin_eps=*/eps);

    DecimalType via_trades = clip_stat(trades);
    DecimalType via_flat   = clip_stat(flattenTrades(trades));

    REQUIRE_NOTHROW(clip_stat(trades));
    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(via_flat)).margin(kTol));
    // Result must be better than total ruin.
    REQUIRE(via_trades > DecimalType("-1.0"));
  }

  SECTION("Winsorization mode 0 (legacy): trade overload matches flat overload")
  {
    // Build 30 trades, each a single bar, so n_bars == n_trades == 30.
    // Mode 0 applies winsorization at n >= 30.
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(30);
    for (int i = 0; i < 28; ++i)
      trades.emplace_back(std::vector<DecimalType>{ DecimalType("0.005") });
    trades.emplace_back(std::vector<DecimalType>{ DecimalType("-0.45") }); // extreme low
    trades.emplace_back(std::vector<DecimalType>{ DecimalType("0.20")  }); // extreme high

    GeoMeanStat<DecimalType> stat(/*clip_ruin=*/true, /*winsor_small_n=*/true,
                                   /*alpha=*/0.02, /*ruin_eps=*/1e-8,
                                   /*adaptive_mode=*/0);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Winsorization mode 1 (smooth fade): trade overload matches flat overload")
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(26);
    for (int i = 0; i < 24; ++i)
      trades.emplace_back(std::vector<DecimalType>{ DecimalType("0.005") });
    trades.emplace_back(std::vector<DecimalType>{ DecimalType("-0.40") });
    trades.emplace_back(std::vector<DecimalType>{ DecimalType("0.18")  });

    GeoMeanStat<DecimalType> stat(true, true, 0.02, 1e-8, /*mode=*/1);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Winsorization mode 2 (always on): trade overload matches flat overload")
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(10);
    for (int i = 0; i < 8; ++i)
      trades.emplace_back(std::vector<DecimalType>{ DecimalType("0.01") });
    trades.emplace_back(std::vector<DecimalType>{ DecimalType("-0.50") });
    trades.emplace_back(std::vector<DecimalType>{ DecimalType("0.30")  });

    GeoMeanStat<DecimalType> stat(true, true, 0.02, 1e-8, /*mode=*/2);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Backward-compatible 2-arg constructor propagates clip and eps")
  {
    const double eps = 1e-6;
    Trade<DecimalType> t({
      DecimalType("0.05"), DecimalType("-1.0"), DecimalType("0.03")
    });
    std::vector<Trade<DecimalType>> trades = { t };

    // Uses the explicit GeoMeanStat(bool, double) constructor.
    GeoMeanStat<DecimalType> stat(/*clip_ruin=*/true, /*ruin_eps=*/eps);

    DecimalType via_trades = stat(trades);
    DecimalType via_flat   = stat(flattenTrades(trades));

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(via_flat)).margin(5e-8));
  }
}


// =============================================================================
// TEST SUITE 4: Multi-bar trade specifics
//
// These tests exercise the structural property unique to the trade overload:
// that each trade may contribute multiple bars, and the concatenation order
// matters for winsorization which operates on the full flattened log-return
// sequence.
// =============================================================================

TEST_CASE("GeoMeanStat trade overload: multi-bar trade structure",
          "[StatUtils][GeoMean][Trade][MultiBar]")
{
  constexpr double kTol = 5e-8;

  SECTION("Trade order does not affect the result (commutativity of flattening geomean)")
  {
    // geometric mean of {a,b,c,d,e,f} == geometric mean of {d,e,f,a,b,c}
    // because log-sum is commutative.
    Trade<DecimalType> t1({ DecimalType("0.02"), DecimalType("0.01") });
    Trade<DecimalType> t2({ DecimalType("-0.03"), DecimalType("0.04"), DecimalType("0.01") });

    GeoMeanStat<DecimalType> stat;

    DecimalType forward  = stat({ t1, t2 });
    DecimalType reversed = stat({ t2, t1 });

    REQUIRE(num::to_double(forward) ==
            Catch::Approx(num::to_double(reversed)).margin(kTol));
  }

  SECTION("Total bar count drives winsorization, not trade count")
  {
    // Build 10 trades of 3 bars each: 30 total bars → mode-0 winsorization
    // applies at exactly n_bars=30, regardless of n_trades=10.
    std::vector<Trade<DecimalType>> trades;
    for (int i = 0; i < 8; ++i)
      trades.emplace_back(std::vector<DecimalType>{
        DecimalType("0.005"), DecimalType("0.005"), DecimalType("0.005")
      });
    // Add extremes in the last two trades
    trades.emplace_back(std::vector<DecimalType>{
      DecimalType("-0.45"), DecimalType("0.005"), DecimalType("0.005")
    });
    trades.emplace_back(std::vector<DecimalType>{
      DecimalType("0.20"), DecimalType("0.005"), DecimalType("0.005")
    });

    // Use mode 0 so we can reason about the exact winsorization threshold.
    GeoMeanStat<DecimalType> stat(true, true, 0.02, 1e-8, /*mode=*/0);

    // The trade overload must be equivalent to applying the stat to the
    // 30-element flat return stream.
    DecimalType via_trades = stat(trades);
    DecimalType via_flat   = stat(flattenTrades(trades));

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(via_flat)).margin(kTol));

    // Sanity: the extreme bars should have been winsorized, so the result
    // must be finite and within a sensible range.
    REQUIRE(std::isfinite(num::to_double(via_trades)));
  }

  SECTION("Large trade set: 50 trades of 3 bars = 150 bars, all finite")
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(50);
    for (int i = 0; i < 50; ++i)
      trades.emplace_back(std::vector<DecimalType>{
        DecimalType("0.003"), DecimalType("-0.001"), DecimalType("0.002")
      });

    GeoMeanStat<DecimalType> stat;
    DecimalType result = stat(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    // Expected: close to geometric mean of the repeating 3-bar pattern.
    DecimalType flat_result = stat(flattenTrades(trades));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(flat_result)).margin(kTol));
  }
}


// =============================================================================
// TEST SUITE 5: Integration with TradeFlatteningAdapter
//
// Verifies that the trade overload composes correctly with the adapter shim
// used by the bootstrap infrastructure, which wraps a flat-vector statistic
// into a trade-vector callable.  Both approaches must produce the same result.
// =============================================================================

TEST_CASE("GeoMeanStat trade overload: TradeFlatteningAdapter integration",
          "[StatUtils][GeoMean][Trade][Adapter]")
{
  constexpr double kTol = 5e-8;

  SECTION("TradeFlatteningAdapter wrapping GeoMeanStat flat overload matches trade overload directly")
  {
    GeoMeanStat<DecimalType> stat;

    // Wrap the flat-vector operator() in the adapter shim.
    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flat) {
        return stat(flat);
      }
    );

    std::vector<Trade<DecimalType>> trades = {
      Trade<DecimalType>({ DecimalType("0.05"), DecimalType("0.02") }),
      Trade<DecimalType>({ DecimalType("-0.03"), DecimalType("0.01"), DecimalType("0.04") }),
      Trade<DecimalType>({ DecimalType("0.00"), DecimalType("-0.01") }),
    };

    DecimalType via_adapter      = adapter(trades);
    DecimalType via_trade_overload = stat(trades);

    REQUIRE(num::to_double(via_adapter) ==
            Catch::Approx(num::to_double(via_trade_overload)).margin(kTol));
  }

  SECTION("Adapter is callable via std::function<Decimal(vector<Trade<Decimal>>)>")
  {
    // Confirms the adapter can be stored in a type-erased function – exactly
    // how the bootstrap infrastructure holds its statistic functor.
    GeoMeanStat<DecimalType> stat;

    std::function<DecimalType(const std::vector<Trade<DecimalType>>&)> fn =
      TradeFlatteningAdapter<DecimalType>(
        [&stat](const std::vector<DecimalType>& flat) { return stat(flat); }
      );

    std::vector<Trade<DecimalType>> trades = {
      Trade<DecimalType>({ DecimalType("0.10"), DecimalType("0.05") }),
      Trade<DecimalType>({ DecimalType("-0.02") }),
    };

    DecimalType result = fn(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(stat(trades))).margin(kTol));
  }
}

TEST_CASE("GeoMeanFromLogBarsStat trade overload: equivalence with flat log-bar overload",
          "[StatUtils][GeoMeanFromLogs][Trade][Equivalence]")
{
  constexpr double kTol = 5e-8;

  SECTION("Single-bar log-trades match flat call")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15")
    };

    auto trades   = makeOneBarLogTrades(rawReturns);
    auto flatLogs = flattenTrades(trades);  // already log-bars

    GeoMeanFromLogBarsStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flatLogs))).margin(kTol));
  }

  SECTION("Multi-bar log-trades: flattening preserves order and count")
  {
    // Trade A: 2 bars, Trade B: 3 bars, Trade C: 1 bar → 6 total log-bars.
    std::vector<DecimalType> rawA = { DecimalType("0.01"), DecimalType("0.02") };
    std::vector<DecimalType> rawB = { DecimalType("-0.03"), DecimalType("0.04"), DecimalType("0.01") };
    std::vector<DecimalType> rawC = { DecimalType("0.05") };

    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade(rawA),
      makeLogTrade(rawB),
      makeLogTrade(rawC)
    };

    GeoMeanFromLogBarsStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Uniform 8-bar log-trades (realistic holding period)")
  {
    std::vector<DecimalType> rawPattern = {
      DecimalType("0.002"), DecimalType("0.001"), DecimalType("-0.001"),
      DecimalType("0.003"), DecimalType("0.000"), DecimalType("-0.002"),
      DecimalType("0.001"), DecimalType("0.002")
    };

    std::vector<Trade<DecimalType>> trades;
    for (int i = 0; i < 10; ++i)
      trades.push_back(makeLogTrade(rawPattern));

    GeoMeanFromLogBarsStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Mixed winning, losing, and flat log-trades")
  {
    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade({ DecimalType("0.05"), DecimalType("0.03"), DecimalType("0.02") }),   // winner
      makeLogTrade({ DecimalType("-0.04"), DecimalType("-0.02") }),                       // loser
      makeLogTrade({ DecimalType("0.00"), DecimalType("0.00") })                          // flat
    };

    GeoMeanFromLogBarsStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }
}


// =============================================================================
// TEST SUITE 2: Cross-consistency with GeoMeanStat
//
// For the same raw returns, GeoMeanStat applied directly must agree with
// GeoMeanFromLogBarsStat applied to the makeLogGrowthSeries-transformed values.
// This is the foundational contract tested in the existing non-trade tests,
// now verified through the trade overload path.
// =============================================================================

TEST_CASE("GeoMeanFromLogBarsStat trade overload: matches GeoMeanStat on same raw returns",
          "[StatUtils][GeoMeanFromLogs][Trade][CrossConsistency]")
{
  constexpr double kTol = 5e-8;

  SECTION("Basic mixed returns – default constructors agree")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
      DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
      DecimalType("0.25")
    };

    // GeoMeanStat path: raw-return Trade objects
    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    GeoMeanStat<DecimalType> geoStat;
    DecimalType geoResult = geoStat(rawTrades);

    // GeoMeanFromLogBarsStat path: pre-logged Trade objects
    auto logTrades = makeOneBarLogTrades(rawReturns);

    GeoMeanFromLogBarsStat<DecimalType> logStat;
    DecimalType logResult = logStat(logTrades);

    REQUIRE(num::to_double(geoResult) ==
            Catch::Approx(num::to_double(logResult)).margin(kTol));
  }

  SECTION("Multi-bar trades: both stats agree after log transform")
  {
    std::vector<DecimalType> rawA = { DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03") };
    std::vector<DecimalType> rawB = { DecimalType("0.01"), DecimalType("0.00"), DecimalType("-0.02") };
    std::vector<DecimalType> rawC = { DecimalType("-0.03"), DecimalType("0.04") };

    // Raw-return trades for GeoMeanStat
    std::vector<Trade<DecimalType>> rawTrades = {
      Trade<DecimalType>(rawA),
      Trade<DecimalType>(rawB),
      Trade<DecimalType>(rawC)
    };

    // Pre-logged trades for GeoMeanFromLogBarsStat
    std::vector<Trade<DecimalType>> logTrades = {
      makeLogTrade(rawA),
      makeLogTrade(rawB),
      makeLogTrade(rawC)
    };

    GeoMeanStat<DecimalType>         geoStat;
    GeoMeanFromLogBarsStat<DecimalType> logStat;

    REQUIRE(num::to_double(geoStat(rawTrades)) ==
            Catch::Approx(num::to_double(logStat(logTrades))).margin(kTol));
  }

  SECTION("Near-ruin return: makeLogGrowthSeries clips; GeoMeanStat clips equivalently")
  {
    // A return of -0.999999 is near ruin.  makeLogGrowthSeries clips the growth
    // at ruin_eps; GeoMeanStat's clip_ruin=true mode applies the same floor.
    const double ruin_eps = 1e-8;

    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.999999"), DecimalType("0.20")
    };

    auto logTrades = makeOneBarLogTrades(rawReturns, ruin_eps);

    GeoMeanStat<DecimalType>           geoStat(/*clip_ruin=*/true, /*ruin_eps=*/ruin_eps);
    GeoMeanFromLogBarsStat<DecimalType> logStat(/*winsor_small_n=*/true,
                                                 /*alpha=*/0.02,
                                                 /*mode=*/1,
                                                 /*ruin_eps=*/ruin_eps);

    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    REQUIRE(num::to_double(geoStat(rawTrades)) ==
            Catch::Approx(num::to_double(logStat(logTrades))).margin(kTol));
  }
}


// =============================================================================
// TEST SUITE 3: Edge cases
// =============================================================================

TEST_CASE("GeoMeanFromLogBarsStat trade overload: edge cases",
          "[StatUtils][GeoMeanFromLogs][Trade][EdgeCases]")
{
  SECTION("Empty trade vector returns DecimalZero")
  {
    std::vector<Trade<DecimalType>> trades;
    GeoMeanFromLogBarsStat<DecimalType> stat;
    REQUIRE(stat(trades) == DC::DecimalZero);
  }

  SECTION("Single trade, single log-bar")
  {
    // log(1 + 0.07) → back-transform gives 0.07
    auto trades = makeOneBarLogTrades({ DecimalType("0.07") });
    GeoMeanFromLogBarsStat<DecimalType> stat;

    REQUIRE(num::to_double(stat(trades)) == Catch::Approx(0.07).margin(5e-8));
  }

  SECTION("Single trade with multiple constant log-bars equals that constant")
  {
    // All bars log(1 + 0.05); mean log = log(1.05); back-transform = 0.05.
    auto logTrade = makeLogTrade(std::vector<DecimalType>(6, DecimalType("0.05")));
    std::vector<Trade<DecimalType>> trades = { logTrade };

    GeoMeanFromLogBarsStat<DecimalType> stat;
    REQUIRE(num::to_double(stat(trades)) == Catch::Approx(0.05).margin(5e-8));
  }

  SECTION("All-zero raw returns: log-bars are zero, geometric mean is zero")
  {
    auto logTrade = makeLogTrade(std::vector<DecimalType>(10, DecimalType("0.0")));
    std::vector<Trade<DecimalType>> trades = { logTrade };

    GeoMeanFromLogBarsStat<DecimalType> stat;
    REQUIRE(num::to_double(stat(trades)) == Catch::Approx(0.0).margin(5e-8));
  }

  SECTION("Trade built via addReturn() matches trade built from log-bar vector")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03")
    };
    auto logBars = StatUtils<DecimalType>::makeLogGrowthSeries(rawReturns);

    Trade<DecimalType> byVector(logBars);

    Trade<DecimalType> byAdd;
    for (const auto& lb : logBars)
      byAdd.addReturn(lb);

    GeoMeanFromLogBarsStat<DecimalType> stat;

    REQUIRE(num::to_double(stat({ byVector })) ==
            Catch::Approx(num::to_double(stat({ byAdd }))).margin(1e-12));
  }
}


// =============================================================================
// TEST SUITE 4: Constructor options propagate correctly
//
// Winsorization mode and alpha must behave identically via the trade overload
// and the flat log-bar overload.
// =============================================================================

TEST_CASE("GeoMeanFromLogBarsStat trade overload: constructor options propagate",
          "[StatUtils][GeoMeanFromLogs][Trade][Options]")
{
  constexpr double kTol = 5e-8;

  // Helper: build 30 single-bar log-Trades with two extremes.
  auto make30Trades = [](double ruin_eps = 1e-8) {
    std::vector<DecimalType> raw(30, DecimalType("0.005"));
    raw[3]  = DecimalType("-0.45");
    raw[17] = DecimalType("0.20");
    return makeOneBarLogTrades(raw, ruin_eps);
  };

  SECTION("Winsorization mode 0 (legacy): trade overload matches flat overload")
  {
    auto trades = make30Trades();
    GeoMeanFromLogBarsStat<DecimalType> stat(/*winsor_small_n=*/true,
                                              /*alpha=*/0.02,
                                              /*mode=*/0);
    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Winsorization mode 1 (smooth fade): trade overload matches flat overload")
  {
    std::vector<DecimalType> raw(26, DecimalType("0.005"));
    raw[0]  = DecimalType("-0.40");
    raw[25] = DecimalType("0.18");
    auto trades = makeOneBarLogTrades(raw);

    GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, /*mode=*/1);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Winsorization mode 2 (always on): trade overload matches flat overload")
  {
    std::vector<DecimalType> raw(10, DecimalType("0.01"));
    raw[0] = DecimalType("-0.50");
    raw[9] = DecimalType("0.30");
    auto trades = makeOneBarLogTrades(raw);

    GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, /*mode=*/2);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("winsor_small_n=false disables winsorization: trade overload matches flat overload")
  {
    auto trades = make30Trades();
    GeoMeanFromLogBarsStat<DecimalType> stat(/*winsor_small_n=*/false);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).margin(kTol));
  }

  SECTION("Custom ruin_eps propagates through trade overload")
  {
    // A very aggressive ruin-clip (eps = 0.01) means growth floors at 0.01.
    // makeLogGrowthSeries with the same eps must produce the same log-bars.
    const double ruin_eps = 0.01;

    std::vector<DecimalType> rawReturns = {
      DecimalType("0.05"), DecimalType("-0.999"), DecimalType("0.03")
    };

    auto logTrades = makeOneBarLogTrades(rawReturns, ruin_eps);

    GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, 1, ruin_eps);

    DecimalType via_trades = stat(logTrades);
    DecimalType via_flat   = stat(flattenTrades(logTrades));

    REQUIRE(std::isfinite(num::to_double(via_trades)));
    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(via_flat)).margin(kTol));
  }

  SECTION("Default constructor uses mode 1: matches explicit mode-1 construction")
  {
    auto trades = make30Trades();

    GeoMeanFromLogBarsStat<DecimalType> stat_default;
    GeoMeanFromLogBarsStat<DecimalType> stat_explicit(true, 0.02, 1);

    REQUIRE(num::to_double(stat_default(trades)) ==
            Catch::Approx(num::to_double(stat_explicit(trades))).margin(1e-12));
  }
}


// =============================================================================
// TEST SUITE 5: Multi-bar trade structure
//
// Verifies structural properties unique to the trade overload: that it is
// the *total bar count* (across all trades) that drives winsorization, and
// that trade order does not affect the result.
// =============================================================================

TEST_CASE("GeoMeanFromLogBarsStat trade overload: multi-bar trade structure",
          "[StatUtils][GeoMeanFromLogs][Trade][MultiBar]")
{
  constexpr double kTol = 5e-8;

  SECTION("Trade order does not affect result (log-sum commutativity)")
  {
    auto t1 = makeLogTrade({ DecimalType("0.02"), DecimalType("0.01") });
    auto t2 = makeLogTrade({ DecimalType("-0.03"), DecimalType("0.04"), DecimalType("0.01") });

    GeoMeanFromLogBarsStat<DecimalType> stat;

    DecimalType forward  = stat({ t1, t2 });
    DecimalType reversed = stat({ t2, t1 });

    REQUIRE(num::to_double(forward) ==
            Catch::Approx(num::to_double(reversed)).margin(kTol));
  }

  SECTION("Total bar count (not trade count) drives mode-0 winsorization threshold")
  {
    // 10 trades × 3 bars = 30 total bars.  Mode 0 winsorizes at n >= 30.
    // Two trades carry extreme bars; the rest are uniform.
    std::vector<Trade<DecimalType>> trades;
    for (int i = 0; i < 8; ++i)
      trades.push_back(makeLogTrade({
        DecimalType("0.005"), DecimalType("0.005"), DecimalType("0.005")
      }));
    trades.push_back(makeLogTrade({
      DecimalType("-0.45"), DecimalType("0.005"), DecimalType("0.005")
    }));
    trades.push_back(makeLogTrade({
      DecimalType("0.20"), DecimalType("0.005"), DecimalType("0.005")
    }));

    GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, /*mode=*/0);

    DecimalType via_trades = stat(trades);
    DecimalType via_flat   = stat(flattenTrades(trades));

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(via_flat)).margin(kTol));
    REQUIRE(std::isfinite(num::to_double(via_trades)));
  }

  SECTION("Large trade set: 50 trades × 3 bars = 150 bars, finite result")
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(50);
    for (int i = 0; i < 50; ++i)
      trades.push_back(makeLogTrade({
        DecimalType("0.003"), DecimalType("-0.001"), DecimalType("0.002")
      }));

    GeoMeanFromLogBarsStat<DecimalType> stat;

    DecimalType result     = stat(trades);
    DecimalType flat_result = stat(flattenTrades(trades));

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(flat_result)).margin(kTol));
  }

  SECTION("Log-bars are NOT re-logged: calling makeLogGrowthSeries on already-logged data differs")
  {
    // This is the critical correctness contract for GeoMeanFromLogBarsStat:
    // Trade::getDailyReturns() must contain log-growth values, NOT raw returns.
    // If raw returns were passed instead, the result would differ because
    // operator() skips the log() step.
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.05"), DecimalType("-0.02"), DecimalType("0.03")
    };

    // Correct usage: pre-log the returns.
    auto correctTrades = makeOneBarLogTrades(rawReturns);
    GeoMeanFromLogBarsStat<DecimalType> logStat;
    DecimalType correct = logStat(correctTrades);

    // Incorrect usage: pass raw returns directly (as if they were log-bars).
    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });
    DecimalType incorrect = logStat(rawTrades);

    // The two results must NOT be equal: this documents the usage contract.
    REQUIRE(num::to_double(correct) != Catch::Approx(num::to_double(incorrect)).margin(1e-6));
  }
}


// =============================================================================
// TEST SUITE 6: Integration with TradeFlatteningAdapter
//
// Confirms that GeoMeanFromLogBarsStat composes correctly with the bootstrap
// adapter shim, matching results from the native trade overload.
// =============================================================================

TEST_CASE("GeoMeanFromLogBarsStat trade overload: TradeFlatteningAdapter integration",
          "[StatUtils][GeoMeanFromLogs][Trade][Adapter]")
{
  constexpr double kTol = 5e-8;

  SECTION("Adapter wrapping flat log-bar overload matches native trade overload")
  {
    GeoMeanFromLogBarsStat<DecimalType> stat;

    // The adapter wraps the flat-log-bar operator() into a trade-vector callable.
    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flatLogs) {
        return stat(flatLogs);
      }
    );

    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade({ DecimalType("0.05"), DecimalType("0.02") }),
      makeLogTrade({ DecimalType("-0.03"), DecimalType("0.01"), DecimalType("0.04") }),
      makeLogTrade({ DecimalType("0.00"), DecimalType("-0.01") })
    };

    DecimalType via_adapter       = adapter(trades);
    DecimalType via_trade_overload = stat(trades);

    REQUIRE(num::to_double(via_adapter) ==
            Catch::Approx(num::to_double(via_trade_overload)).margin(kTol));
  }

  SECTION("Adapter stored in std::function is callable and consistent")
  {
    GeoMeanFromLogBarsStat<DecimalType> stat;

    // Bootstrap infrastructure holds functors via type-erased std::function.
    std::function<DecimalType(const std::vector<Trade<DecimalType>>&)> fn =
      TradeFlatteningAdapter<DecimalType>(
        [&stat](const std::vector<DecimalType>& flatLogs) { return stat(flatLogs); }
      );

    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade({ DecimalType("0.10"), DecimalType("0.05") }),
      makeLogTrade({ DecimalType("-0.02") })
    };

    DecimalType result = fn(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(stat(trades))).margin(kTol));
  }

  SECTION("Performance contract: adapter skips log() for pre-logged trades")
  {
    // Verify that when the adapter is used with pre-logged Trades, the result
    // matches GeoMeanStat with raw-return Trades on the same underlying data,
    // proving no double-logging occurs inside GeoMeanFromLogBarsStat.
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.05"), DecimalType("-0.03"), DecimalType("0.02"),
      DecimalType("0.01"), DecimalType("-0.01")
    };

    // Raw-return Trades → GeoMeanStat
    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    GeoMeanStat<DecimalType> geoStat;
    DecimalType geoResult = geoStat(rawTrades);

    // Pre-logged Trades → GeoMeanFromLogBarsStat via adapter
    auto logTrades = makeOneBarLogTrades(rawReturns);

    GeoMeanFromLogBarsStat<DecimalType> logStat;
    TradeFlatteningAdapter<DecimalType> adapter(
      [&logStat](const std::vector<DecimalType>& flatLogs) { return logStat(flatLogs); }
    );

    DecimalType logResult = adapter(logTrades);

    REQUIRE(num::to_double(geoResult) ==
            Catch::Approx(num::to_double(logResult)).margin(kTol));
  }
}

TEST_CASE("LogProfitFactorStat_LogPF trade overload: equivalence with flat-vector overload",
          "[StatUtils][LogPF_LogPF][Trade][Equivalence]")
{
  SECTION("Single-bar trades match flat-vector call — default parameters")
  {
    std::vector<DecimalType> returns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
      DecimalType("-0.02")
    };
    auto trades = makeOneBarTrades(returns);

    LogPFStat stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Multi-bar trades: flattening preserves order and count")
  {
    // Trade A: 2 bars, Trade B: 3 bars, Trade C: 1 bar  →  6 total bars.
    Trade<DecimalType> tA({ DecimalType("0.05"), DecimalType("0.03") });
    Trade<DecimalType> tB({ DecimalType("-0.04"), DecimalType("0.02"), DecimalType("0.01") });
    Trade<DecimalType> tC({ DecimalType("-0.02") });

    std::vector<Trade<DecimalType>> trades = { tA, tB, tC };

    LogPFStat stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Uniform 8-bar trades — realistic intraday holding period")
  {
    std::vector<DecimalType> barTemplate = {
      DecimalType("0.002"), DecimalType("0.001"), DecimalType("-0.001"),
      DecimalType("0.003"), DecimalType("0.000"), DecimalType("-0.002"),
      DecimalType("0.001"), DecimalType("0.002")
    };

    std::vector<Trade<DecimalType>> trades;
    for (int i = 0; i < 10; ++i)
      trades.emplace_back(barTemplate);

    LogPFStat stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Custom parameters: equivalence preserved with non-default constructor args")
  {
    std::vector<DecimalType> returns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.08"), DecimalType("-0.03")
    };
    auto trades = makeOneBarTrades(returns);

    LogPFStat stat(
      /*ruin_eps=*/1e-7,
      /*denom_floor=*/1e-5,
      /*prior_strength=*/0.5,
      /*stop_loss_pct=*/0.04,
      /*profit_target_pct=*/0.025,
      /*tiny_win_fraction=*/0.1,
      /*tiny_win_min_return=*/5e-5
    );

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Mixed winning and losing trades across trade boundaries")
  {
    // The boundary between trades must not affect the log(PF) calculation.
    Trade<DecimalType> winner({ DecimalType("0.05"), DecimalType("0.03"), DecimalType("0.02") });
    Trade<DecimalType> loser ({ DecimalType("-0.04"), DecimalType("-0.02") });
    Trade<DecimalType> flat  ({ DecimalType("0.00"), DecimalType("0.00") });

    std::vector<Trade<DecimalType>> trades = { winner, loser, flat };

    LogPFStat stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }
}


// =============================================================================
// TEST SUITE 2: Edge cases
// =============================================================================

TEST_CASE("LogProfitFactorStat_LogPF trade overload: edge cases",
          "[StatUtils][LogPF_LogPF][Trade][EdgeCases]")
{
  SECTION("Empty trade vector returns DecimalZero")
  {
    std::vector<Trade<DecimalType>> trades;
    LogPFStat stat;
    REQUIRE(stat(trades) == DC::DecimalZero);
  }

  SECTION("Single trade, single winning bar: log(PF) higher than single losing bar")
  {
    // With the default prior_strength, the prior's virtual losses dominate a
    // one-bar sample, so the absolute sign of log(PF) is not reliable.
    // The meaningful invariant is monotonicity: a winning bar must produce a
    // strictly higher log(PF) than a losing bar of equal magnitude.
    LogPFStat stat;

    auto win_trades  = makeOneBarTrades({ DecimalType("0.10") });
    auto loss_trades = makeOneBarTrades({ DecimalType("-0.10") });

    DecimalType win_result  = stat(win_trades);
    DecimalType loss_result = stat(loss_trades);

    REQUIRE(std::isfinite(num::to_double(win_result)));
    REQUIRE(std::isfinite(num::to_double(loss_result)));
    REQUIRE(num::to_double(win_result) > num::to_double(loss_result));
  }

  SECTION("Single trade, single losing bar: log(PF) lower than single winning bar")
  {
    // Symmetric check to the section above: a losing bar must produce a
    // strictly lower log(PF) than a winning bar of equal magnitude.
    LogPFStat stat;

    auto win_trades  = makeOneBarTrades({ DecimalType("0.05") });
    auto loss_trades = makeOneBarTrades({ DecimalType("-0.05") });

    DecimalType win_result  = stat(win_trades);
    DecimalType loss_result = stat(loss_trades);

    REQUIRE(std::isfinite(num::to_double(win_result)));
    REQUIRE(std::isfinite(num::to_double(loss_result)));
    REQUIRE(num::to_double(loss_result) < num::to_double(win_result));
  }

  SECTION("All-winning trades produce higher log(PF) than all-losing trades")
  {
    // The default prior_strength adds virtual losses that dominate small samples,
    // so even 10 all-winning bars may yield a negative log(PF) in absolute terms.
    // The reliable invariant is that all-winning > all-losing for the same n.
    std::vector<DecimalType> win_returns(10, DecimalType("0.02"));
    std::vector<DecimalType> loss_returns(10, DecimalType("-0.02"));

    auto win_trades  = makeOneBarTrades(win_returns);
    auto loss_trades = makeOneBarTrades(loss_returns);

    LogPFStat stat;
    DecimalType win_result  = stat(win_trades);
    DecimalType loss_result = stat(loss_trades);

    REQUIRE(std::isfinite(num::to_double(win_result)));
    REQUIRE(std::isfinite(num::to_double(loss_result)));
    REQUIRE(num::to_double(win_result) > num::to_double(loss_result));
  }

  SECTION("All-losing trades produce lower log(PF) than all-winning trades")
  {
    // Symmetric check: confirms the ordering is strict in both directions.
    std::vector<DecimalType> win_returns(10, DecimalType("0.02"));
    std::vector<DecimalType> loss_returns(10, DecimalType("-0.02"));

    auto win_trades  = makeOneBarTrades(win_returns);
    auto loss_trades = makeOneBarTrades(loss_returns);

    LogPFStat stat;
    DecimalType win_result  = stat(win_trades);
    DecimalType loss_result = stat(loss_trades);

    REQUIRE(std::isfinite(num::to_double(win_result)));
    REQUIRE(std::isfinite(num::to_double(loss_result)));
    REQUIRE(num::to_double(loss_result) < num::to_double(win_result));
  }

  SECTION("All-zero returns: log(PF) is zero or at the prior floor")
  {
    // Zero returns produce zero log-wins and zero log-losses.
    // The prior and floor logic governs the exact output; it must be finite.
    std::vector<DecimalType> returns(10, DecimalType("0.00"));
    auto trades = makeOneBarTrades(returns);

    LogPFStat stat;
    DecimalType result = stat(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    // Must agree with the flat-vector path.
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Near-ruin bar is clipped internally — no exception thrown")
  {
    // A bar with return = -0.999999 is near ruin.  The robust function clips
    // it at ruin_eps; the trade overload must not throw.
    Trade<DecimalType> t({
      DecimalType("0.05"), DecimalType("-0.999999"), DecimalType("0.03")
    });
    std::vector<Trade<DecimalType>> trades = { t };

    LogPFStat stat;

    REQUIRE_NOTHROW(stat(trades));
    REQUIRE(std::isfinite(num::to_double(stat(trades))));
  }

  SECTION("Trade built via addReturn() matches trade built from vector")
  {
    std::vector<DecimalType> rets = {
      DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03")
    };

    Trade<DecimalType> byVector(rets);

    Trade<DecimalType> byAdd;
    for (const auto& r : rets)
      byAdd.addReturn(r);

    LogPFStat stat;

    REQUIRE(num::to_double(stat({ byVector })) ==
            Catch::Approx(num::to_double(stat({ byAdd }))).epsilon(1e-12));
  }
}


// =============================================================================
// TEST SUITE 3: Constructor parameters propagate correctly
//
// Each named parameter stored by the functor must reach the underlying
// computeLogProfitFactorRobust_LogPF call unchanged.  We verify this by
// comparing stat(trades) against the direct function call with the same args.
// =============================================================================

TEST_CASE("LogProfitFactorStat_LogPF trade overload: constructor parameters propagate",
          "[StatUtils][LogPF_LogPF][Trade][Parameters]")
{
  // Common return fixture used across sections.
  std::vector<DecimalType> rawReturns = {
    DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
    DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
    DecimalType("-0.02")
  };

  SECTION("Default parameters: trade overload matches direct computeLogProfitFactorRobust_LogPF")
  {
    auto trades = makeOneBarTrades(rawReturns);
    LogPFStat stat;

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(rawReturns);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("Custom ruin_eps propagates through trade overload")
  {
    const double ruin_eps = 1e-5;
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat(ruin_eps);

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(
      rawReturns, ruin_eps);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("Custom denom_floor propagates through trade overload")
  {
    const double ruin_eps    = 1e-8;
    const double denom_floor = 1e-4;
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat(ruin_eps, denom_floor);

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(
      rawReturns, ruin_eps, denom_floor);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("Custom prior_strength propagates through trade overload")
  {
    const double ruin_eps       = 1e-8;
    const double denom_floor    = 1e-6;
    const double prior_strength = 2.0;
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat(ruin_eps, denom_floor, prior_strength);

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(
      rawReturns, ruin_eps, denom_floor, prior_strength);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("stop_loss_pct propagates through trade overload")
  {
    const double ruin_eps      = 1e-8;
    const double denom_floor   = 1e-6;
    const double prior_str     = 1.0;
    const double stop_loss_pct = 0.05;
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat(ruin_eps, denom_floor, prior_str, stop_loss_pct);

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(
      rawReturns, ruin_eps, denom_floor, prior_str, stop_loss_pct);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("profit_target_pct propagates through trade overload")
  {
    const double ruin_eps          = 1e-8;
    const double denom_floor       = 1e-6;
    const double prior_str         = 1.0;
    const double stop_loss_pct     = 0.05;
    const double profit_target_pct = 0.03;
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat(ruin_eps, denom_floor, prior_str, stop_loss_pct, profit_target_pct);

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(
      rawReturns, ruin_eps, denom_floor, prior_str, stop_loss_pct, profit_target_pct);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("All custom parameters propagate through trade overload")
  {
    const double ruin_eps           = 1e-7;
    const double denom_floor        = 1e-5;
    const double prior_str          = 0.5;
    const double stop_loss_pct      = 0.04;
    const double profit_target_pct  = 0.025;
    const double tiny_win_fraction  = 0.1;
    const double tiny_win_min_ret   = 5e-5;
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat(ruin_eps, denom_floor, prior_str,
                   stop_loss_pct, profit_target_pct,
                   tiny_win_fraction, tiny_win_min_ret);

    DecimalType via_trades = stat(trades);
    DecimalType direct     = Stat::computeLogProfitFactorRobust_LogPF(
      rawReturns, ruin_eps, denom_floor, prior_str,
      stop_loss_pct, profit_target_pct,
      tiny_win_fraction, tiny_win_min_ret);

    REQUIRE(num::to_double(via_trades) ==
            Catch::Approx(num::to_double(direct)).epsilon(1e-12));
  }

  SECTION("Higher prior_strength makes log(PF) more conservative via trade overload")
  {
    // Increasing prior_strength pulls log(PF) toward zero (more conservative).
    // This verifies the parameter influences the result through the trade path.
    auto trades = makeOneBarTrades(rawReturns);

    LogPFStat stat_low (1e-8, 1e-6, /*prior=*/0.5);
    LogPFStat stat_high(1e-8, 1e-6, /*prior=*/2.0);

    double result_low  = num::to_double(stat_low(trades));
    double result_high = num::to_double(stat_high(trades));

    // Both must be finite.
    REQUIRE(std::isfinite(result_low));
    REQUIRE(std::isfinite(result_high));

    // Higher prior → more conservative (result closer to zero).
    REQUIRE(result_high < result_low);
  }

  SECTION("Functor is copyable: copy and original produce identical results")
  {
    LogPFStat stat1(1e-8, 1e-6, 1.0, 0.05, 0.03);
    auto stat2 = stat1;

    auto trades = makeOneBarTrades(rawReturns);

    REQUIRE(num::to_double(stat1(trades)) ==
            Catch::Approx(num::to_double(stat2(trades))).epsilon(1e-15));
  }
}


// =============================================================================
// TEST SUITE 4: Multi-bar trade structure
//
// Structural properties unique to the trade overload: total bar count drives
// the calculation regardless of how bars are grouped into trades, and trade
// ordering does not affect the log(PF).
// =============================================================================

TEST_CASE("LogProfitFactorStat_LogPF trade overload: multi-bar trade structure",
          "[StatUtils][LogPF_LogPF][Trade][MultiBar]")
{
  SECTION("Trade order does not affect log(PF)")
  {
    // log-sum (and thus log(PF)) is commutative over the full bar stream.
    Trade<DecimalType> t1({ DecimalType("0.05"), DecimalType("0.03") });
    Trade<DecimalType> t2({ DecimalType("-0.04"), DecimalType("0.02"), DecimalType("0.01") });

    LogPFStat stat;

    DecimalType forward  = stat({ t1, t2 });
    DecimalType reversed = stat({ t2, t1 });

    REQUIRE(num::to_double(forward) ==
            Catch::Approx(num::to_double(reversed)).epsilon(1e-12));
  }

  SECTION("Regrouping bars across trade boundaries leaves log(PF) unchanged")
  {
    // Six raw returns, grouped two ways:
    //   Grouping A: one 6-bar trade.
    //   Grouping B: three 2-bar trades.
    // Both produce the same flat stream and must yield the same log(PF).
    std::vector<DecimalType> all = {
      DecimalType("0.04"), DecimalType("-0.02"),
      DecimalType("0.06"), DecimalType("-0.01"),
      DecimalType("0.03"), DecimalType("-0.03")
    };

    Trade<DecimalType> singleTrade(all);

    Trade<DecimalType> trA({ all[0], all[1] });
    Trade<DecimalType> trB({ all[2], all[3] });
    Trade<DecimalType> trC({ all[4], all[5] });

    LogPFStat stat;

    DecimalType result_one   = stat({ singleTrade });
    DecimalType result_three = stat({ trA, trB, trC });

    REQUIRE(num::to_double(result_one) ==
            Catch::Approx(num::to_double(result_three)).epsilon(1e-12));
  }

  SECTION("Total bar count (not trade count) determines log(PF)")
  {
    // 10 trades × 3 bars = 30 bars.  The log(PF) should match a single
    // 30-bar trade containing the same returns in the same order.
    std::vector<DecimalType> pattern = {
      DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03")
    };

    std::vector<DecimalType> allBars;
    for (int i = 0; i < 10; ++i)
      allBars.insert(allBars.end(), pattern.begin(), pattern.end());

    Trade<DecimalType> singleBigTrade(allBars);

    std::vector<Trade<DecimalType>> manyTrades;
    for (int i = 0; i < 10; ++i)
      manyTrades.emplace_back(pattern);

    LogPFStat stat;

    REQUIRE(num::to_double(stat(manyTrades)) ==
            Catch::Approx(num::to_double(stat({ singleBigTrade }))).epsilon(1e-12));
  }

  SECTION("Large trade set: 50 trades × 3 bars = 150 bars, finite and consistent")
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(50);
    for (int i = 0; i < 50; ++i)
      trades.emplace_back(std::vector<DecimalType>{
        DecimalType("0.005"), DecimalType("-0.002"), DecimalType("0.003")
      });

    LogPFStat stat;

    DecimalType result     = stat(trades);
    DecimalType flat_result = stat(flattenTrades(trades));

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(flat_result)).epsilon(1e-12));
  }

  SECTION("Near-ruin bar in the middle of a multi-bar trade does not throw")
  {
    // Ruin bar embedded mid-trade; surrounding bars are normal.
    Trade<DecimalType> riskTrade({
      DecimalType("0.05"),
      DecimalType("0.03"),
      DecimalType("-0.999999"),  // near-ruin, will be clipped internally
      DecimalType("0.02"),
      DecimalType("0.01")
    });

    std::vector<Trade<DecimalType>> trades = { riskTrade };
    LogPFStat stat;

    REQUIRE_NOTHROW(stat(trades));
    REQUIRE(std::isfinite(num::to_double(stat(trades))));

    // Must match the flat-vector path.
    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }
}


// =============================================================================
// TEST SUITE 5: TradeFlatteningAdapter integration
//
// The bootstrap infrastructure stores functors as
// std::function<Decimal(vector<Trade<Decimal>>)>.  Verifies that
// LogProfitFactorStat_LogPF composes correctly with TradeFlatteningAdapter
// and that both paths produce identical results.
// =============================================================================

TEST_CASE("LogProfitFactorStat_LogPF trade overload: TradeFlatteningAdapter integration",
          "[StatUtils][LogPF_LogPF][Trade][Adapter]")
{
  SECTION("Adapter wrapping flat-vector overload matches native trade overload")
  {
    LogPFStat stat;

    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flat) {
        return stat(flat);
      }
    );

    std::vector<Trade<DecimalType>> trades = {
      Trade<DecimalType>({ DecimalType("0.05"), DecimalType("0.02") }),
      Trade<DecimalType>({ DecimalType("-0.03"), DecimalType("0.01"), DecimalType("0.04") }),
      Trade<DecimalType>({ DecimalType("0.00"), DecimalType("-0.01") })
    };

    DecimalType via_adapter       = adapter(trades);
    DecimalType via_trade_overload = stat(trades);

    REQUIRE(num::to_double(via_adapter) ==
            Catch::Approx(num::to_double(via_trade_overload)).epsilon(1e-12));
  }

  SECTION("Adapter stored in std::function is callable and consistent")
  {
    LogPFStat stat;

    std::function<DecimalType(const std::vector<Trade<DecimalType>>&)> fn =
      TradeFlatteningAdapter<DecimalType>(
        [&stat](const std::vector<DecimalType>& flat) { return stat(flat); }
      );

    std::vector<Trade<DecimalType>> trades = {
      Trade<DecimalType>({ DecimalType("0.10"), DecimalType("0.05") }),
      Trade<DecimalType>({ DecimalType("-0.02") }),
    };

    DecimalType result = fn(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(stat(trades))).epsilon(1e-12));
  }

  SECTION("Adapter with custom parameters: result matches native trade overload")
  {
    LogPFStat stat(
      /*ruin_eps=*/1e-8,
      /*denom_floor=*/1e-6,
      /*prior_strength=*/1.0,
      /*stop_loss_pct=*/0.05,
      /*profit_target_pct=*/0.03
    );

    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flat) { return stat(flat); }
    );

    std::vector<DecimalType> returns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
      DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
      DecimalType("0.25"), DecimalType("-0.03"), DecimalType("0.12")
    };
    auto trades = makeOneBarTrades(returns);

    REQUIRE(num::to_double(adapter(trades)) ==
            Catch::Approx(num::to_double(stat(trades))).epsilon(1e-12));
  }

  SECTION("Empty trade vector: adapter and native overload both return zero")
  {
    LogPFStat stat;

    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flat) { return stat(flat); }
    );

    std::vector<Trade<DecimalType>> empty;

    // Native trade overload short-circuits to zero before flattening.
    // Adapter flattens to an empty vector and then calls stat({}) = zero.
    // Both paths must agree.
    REQUIRE(stat(empty) == DC::DecimalZero);
    REQUIRE(adapter(empty) == DC::DecimalZero);
  }
}

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF trade overload: equivalence with flat log-bar overload",
          "[StatUtils][LogPF_LogPF][LogBars][Trade][Equivalence]")
{
  SECTION("Single-bar log-trades match flat call — default parameters")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
      DecimalType("-0.02")
    };
    auto trades   = makeOneBarLogTrades(rawReturns);
    auto flatLogs = flattenTrades(trades);

    LogPFBars stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flatLogs))).epsilon(1e-12));
  }

  SECTION("Multi-bar log-trades: flattening preserves order and count")
  {
    // Trade A: 2 bars, Trade B: 3 bars, Trade C: 1 bar → 6 total log-bars.
    std::vector<DecimalType> rawA = { DecimalType("0.05"), DecimalType("0.03") };
    std::vector<DecimalType> rawB = { DecimalType("-0.04"), DecimalType("0.02"), DecimalType("0.01") };
    std::vector<DecimalType> rawC = { DecimalType("-0.02") };

    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade(rawA), makeLogTrade(rawB), makeLogTrade(rawC)
    };

    LogPFBars stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Uniform 8-bar log-trades — realistic intraday holding period")
  {
    std::vector<DecimalType> rawPattern = {
      DecimalType("0.002"), DecimalType("0.001"), DecimalType("-0.001"),
      DecimalType("0.003"), DecimalType("0.000"), DecimalType("-0.002"),
      DecimalType("0.001"), DecimalType("0.002")
    };

    std::vector<Trade<DecimalType>> trades;
    for (int i = 0; i < 10; ++i)
      trades.push_back(makeLogTrade(rawPattern));

    LogPFBars stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Mixed winning, losing, and flat log-trades")
  {
    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade({ DecimalType("0.05"), DecimalType("0.03"), DecimalType("0.02") }),
      makeLogTrade({ DecimalType("-0.04"), DecimalType("-0.02") }),
      makeLogTrade({ DecimalType("0.00"), DecimalType("0.00") })
    };

    LogPFBars stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Custom parameters: equivalence preserved with non-default constructor args")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.08"), DecimalType("-0.03")
    };
    auto trades = makeOneBarLogTrades(rawReturns);

    LogPFBars stat(
      /*ruin_eps=*/1e-7,
      /*denom_floor=*/1e-5,
      /*prior_strength=*/0.5,
      /*stop_loss_pct=*/0.04,
      /*profit_target_pct=*/0.025,
      /*tiny_win_fraction=*/0.1,
      /*tiny_win_min_return=*/5e-5
    );

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }
}


// =============================================================================
// TEST SUITE 2: Cross-consistency with LogProfitFactorStat_LogPF
//
// For the same raw returns, LogProfitFactorStat_LogPF applied directly must
// agree with LogProfitFactorFromLogBarsStat_LogPF applied to pre-logged Trades
// from makeLogGrowthSeries.  This is the foundational contract verified in the
// existing flat-vector tests, now checked through the trade overload path.
// =============================================================================

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF trade overload: matches LogProfitFactorStat_LogPF on same raw returns",
          "[StatUtils][LogPF_LogPF][LogBars][Trade][CrossConsistency]")
{
  SECTION("Default parameters — basic mixed return series")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
      DecimalType("-0.02")
    };

    // Raw-return Trades for LogProfitFactorStat_LogPF
    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    LogPFStat  raw_stat;
    DecimalType via_raw = raw_stat(rawTrades);

    // Pre-logged Trades for LogProfitFactorFromLogBarsStat_LogPF
    auto logTrades = makeOneBarLogTrades(rawReturns);

    LogPFBars  log_stat;
    DecimalType via_log = log_stat(logTrades);

    REQUIRE(num::to_double(via_log) ==
            Catch::Approx(num::to_double(via_raw)).epsilon(1e-12));
  }

  SECTION("All custom parameters propagate consistently between sibling stats")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
      DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
      DecimalType("-0.02"), DecimalType("0.08")
    };

    const double ruin_eps          = 1e-8;
    const double denom_floor       = 1e-6;
    const double prior_strength    = 1.5;
    const double stop_loss_pct     = 0.05;
    const double profit_target_pct = 0.03;
    const double tiny_win_fraction = 0.05;
    const double tiny_win_min      = 1e-4;

    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    LogPFStat raw_stat(ruin_eps, denom_floor, prior_strength,
                       stop_loss_pct, profit_target_pct,
                       tiny_win_fraction, tiny_win_min);
    DecimalType via_raw = raw_stat(rawTrades);

    auto logTrades = makeOneBarLogTrades(rawReturns, ruin_eps);
    LogPFBars log_stat(ruin_eps, denom_floor, prior_strength,
                       stop_loss_pct, profit_target_pct,
                       tiny_win_fraction, tiny_win_min);
    DecimalType via_log = log_stat(logTrades);

    REQUIRE(num::to_double(via_log) ==
            Catch::Approx(num::to_double(via_raw)).epsilon(1e-12));
  }

  SECTION("Multi-bar trades: both stats agree after log transform")
  {
    std::vector<DecimalType> rawA = { DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03") };
    std::vector<DecimalType> rawB = { DecimalType("0.01"), DecimalType("0.00"), DecimalType("-0.02") };
    std::vector<DecimalType> rawC = { DecimalType("-0.03"), DecimalType("0.04") };

    std::vector<Trade<DecimalType>> rawTrades = {
      Trade<DecimalType>(rawA),
      Trade<DecimalType>(rawB),
      Trade<DecimalType>(rawC)
    };

    std::vector<Trade<DecimalType>> logTrades = {
      makeLogTrade(rawA),
      makeLogTrade(rawB),
      makeLogTrade(rawC)
    };

    LogPFStat  raw_stat;
    LogPFBars  log_stat;

    REQUIRE(num::to_double(raw_stat(rawTrades)) ==
            Catch::Approx(num::to_double(log_stat(logTrades))).epsilon(1e-12));
  }

  SECTION("Varying prior_strength: both paths agree at each level")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.05"),
      DecimalType("0.20"), DecimalType("-0.10")
    };

    const double ruin_eps = 1e-8;
    auto logTrades = makeOneBarLogTrades(rawReturns, ruin_eps);
    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    for (double prior_strength : {0.5, 1.0, 1.5, 2.0})
    {
      LogPFStat  raw_stat(ruin_eps, Stat::DefaultDenomFloor, prior_strength);
      LogPFBars  log_stat(ruin_eps, Stat::DefaultDenomFloor, prior_strength);

      REQUIRE(num::to_double(raw_stat(rawTrades)) ==
              Catch::Approx(num::to_double(log_stat(logTrades))).epsilon(1e-12));
    }
  }

  SECTION("Near-ruin bar: makeLogGrowthSeries clips; both paths agree")
  {
    const double ruin_eps = 1e-8;

    std::vector<DecimalType> rawReturns = {
      DecimalType("0.10"), DecimalType("-0.999999"), DecimalType("0.20")
    };

    auto logTrades = makeOneBarLogTrades(rawReturns, ruin_eps);

    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    LogPFStat raw_stat(ruin_eps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength, 0.05, 0.03);
    LogPFBars log_stat(ruin_eps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength, 0.05, 0.03);

    REQUIRE(num::to_double(raw_stat(rawTrades)) ==
            Catch::Approx(num::to_double(log_stat(logTrades))).epsilon(1e-12));
  }
}


// =============================================================================
// TEST SUITE 3: Edge cases
// =============================================================================

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF trade overload: edge cases",
          "[StatUtils][LogPF_LogPF][LogBars][Trade][EdgeCases]")
{
  SECTION("Empty trade vector returns DecimalZero")
  {
    std::vector<Trade<DecimalType>> trades;
    LogPFBars stat;
    REQUIRE(stat(trades) == DC::DecimalZero);
  }

  SECTION("Single winning log-bar produces higher log(PF) than single losing log-bar")
  {
    // With the default prior, a one-bar sample is dominated by virtual pseudo-losses,
    // so the absolute sign is unreliable.  The meaningful invariant is ordering:
    // a winning log-bar must yield strictly higher log(PF) than a losing log-bar.
    LogPFBars stat;

    auto win_trades  = makeOneBarLogTrades({ DecimalType("0.10") });
    auto loss_trades = makeOneBarLogTrades({ DecimalType("-0.10") });

    DecimalType win_result  = stat(win_trades);
    DecimalType loss_result = stat(loss_trades);

    REQUIRE(std::isfinite(num::to_double(win_result)));
    REQUIRE(std::isfinite(num::to_double(loss_result)));
    REQUIRE(num::to_double(win_result) > num::to_double(loss_result));
  }

  SECTION("All-winning log-trades produce higher log(PF) than all-losing log-trades")
  {
    // For n bars: relative ordering is reliable even when absolute sign is not.
    std::vector<DecimalType> win_returns(10, DecimalType("0.02"));
    std::vector<DecimalType> loss_returns(10, DecimalType("-0.02"));

    auto win_trades  = makeOneBarLogTrades(win_returns);
    auto loss_trades = makeOneBarLogTrades(loss_returns);

    LogPFBars stat;
    DecimalType win_result  = stat(win_trades);
    DecimalType loss_result = stat(loss_trades);

    REQUIRE(std::isfinite(num::to_double(win_result)));
    REQUIRE(std::isfinite(num::to_double(loss_result)));
    REQUIRE(num::to_double(win_result) > num::to_double(loss_result));
  }

  SECTION("Large wins dominate the prior: log(PF) is positive and finite")
  {
    // With sufficiently large wins the numerator dominates the prior's
    // virtual losses, so the absolute sign IS reliable here.
    // This mirrors the existing flat-vector "Large returns" stability test.
    std::vector<DecimalType> rawReturns = {
      DecimalType("2.0"), DecimalType("-0.5"),
      DecimalType("1.5"), DecimalType("-0.3")
    };
    auto trades = makeOneBarLogTrades(rawReturns, 1e-10);

    LogPFBars stat(1e-10, 1e-8, 1.0, 0.5, 0.3);
    DecimalType result = stat(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) > 0.0);
  }

  SECTION("All-zero raw returns: log-bars are zero; result is finite and matches flat")
  {
    // log(1 + 0) = 0 for every bar. Both win and loss sums are zero.
    // The prior and floor logic determines the output; it must be finite.
    auto logTrade = makeLogTrade(std::vector<DecimalType>(10, DecimalType("0.0")));
    std::vector<Trade<DecimalType>> trades = { logTrade };

    LogPFBars stat;
    DecimalType result = stat(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Trade built via addReturn() matches trade built from log-bar vector")
  {
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03")
    };
    auto logBars = Stat::makeLogGrowthSeries(rawReturns);

    Trade<DecimalType> byVector(logBars);

    Trade<DecimalType> byAdd;
    for (const auto& lb : logBars)
      byAdd.addReturn(lb);

    LogPFBars stat;

    REQUIRE(num::to_double(stat({ byVector })) ==
            Catch::Approx(num::to_double(stat({ byAdd }))).epsilon(1e-12));
  }

  SECTION("Near-ruin log-bar (clipped by makeLogGrowthSeries): no exception thrown")
  {
    // makeLogGrowthSeries has already clamped growth at ruin_eps; the log-bar
    // is a large negative finite number — not -inf.  The trade overload must
    // remain finite and not throw.
    auto trades = makeOneBarLogTrades(
      { DecimalType("0.05"), DecimalType("-0.999999"), DecimalType("0.03") }
    );

    LogPFBars stat;
    REQUIRE_NOTHROW(stat(trades));
    REQUIRE(std::isfinite(num::to_double(stat(trades))));
  }
}


// =============================================================================
// TEST SUITE 4: Constructor parameters propagate correctly
//
// Every parameter stored by the functor must reach computeLogPF_FromSums
// unchanged.  We verify by comparing stat(trades) against stat(flat_log_bars)
// with identical constructor arguments (the flat-vector overload is the
// canonical reference for the log-bar path).
// We also cross-check against LogProfitFactorStat_LogPF to validate the full
// pipeline: raw returns → makeLogGrowthSeries → log-bar Trade → stat(trades).
// =============================================================================

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF trade overload: constructor parameters propagate",
          "[StatUtils][LogPF_LogPF][LogBars][Trade][Parameters]")
{
  // Common fixture used across sections.
  std::vector<DecimalType> rawReturns = {
    DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
    DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
    DecimalType("-0.02")
  };
  const double ruin_eps = 1e-8;

  SECTION("Default parameters: trade overload matches flat log-bar overload")
  {
    auto trades = makeOneBarLogTrades(rawReturns, ruin_eps);
    LogPFBars stat;

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Custom stop_loss_pct propagates through trade overload")
  {
    LogPFBars stat1(ruin_eps, 1e-6, 1.0, /*stop_loss=*/0.03, 0.02);
    LogPFBars stat2(ruin_eps, 1e-6, 1.0, /*stop_loss=*/0.05, 0.02);
    LogPFBars stat3(ruin_eps, 1e-6, 1.0, /*stop_loss=*/0.10, 0.02);

    auto trades = makeOneBarLogTrades(rawReturns, ruin_eps);

    // Each stat must agree with its own flat-vector call.
    REQUIRE(num::to_double(stat1(trades)) ==
            Catch::Approx(num::to_double(stat1(flattenTrades(trades)))).epsilon(1e-12));
    REQUIRE(num::to_double(stat2(trades)) ==
            Catch::Approx(num::to_double(stat2(flattenTrades(trades)))).epsilon(1e-12));
    REQUIRE(num::to_double(stat3(trades)) ==
            Catch::Approx(num::to_double(stat3(flattenTrades(trades)))).epsilon(1e-12));

    // Different stop losses must produce different results.
    REQUIRE(num::to_double(stat1(trades)) != num::to_double(stat2(trades)));
    REQUIRE(num::to_double(stat2(trades)) != num::to_double(stat3(trades)));
  }

  SECTION("Custom profit_target_pct propagates — tiny-win regime shows ordering")
  {
    // Use tiny wins so the numerator floor dominates (from existing flat tests).
    std::vector<DecimalType> tinyWinReturns = {
      DecimalType("0.0001"), DecimalType("-0.05"),
      DecimalType("-0.10"),  DecimalType("-0.02")
    };
    auto trades = makeOneBarLogTrades(tinyWinReturns, ruin_eps);

    LogPFBars stat1(ruin_eps, 1e-6, 1.0, 0.05, /*profit_target=*/0.02);
    LogPFBars stat2(ruin_eps, 1e-6, 1.0, 0.05, /*profit_target=*/0.03);
    LogPFBars stat3(ruin_eps, 1e-6, 1.0, 0.05, /*profit_target=*/0.05);

    // Each stat must agree with its own flat call.
    REQUIRE(num::to_double(stat1(trades)) ==
            Catch::Approx(num::to_double(stat1(flattenTrades(trades)))).epsilon(1e-12));
    REQUIRE(num::to_double(stat2(trades)) ==
            Catch::Approx(num::to_double(stat2(flattenTrades(trades)))).epsilon(1e-12));
    REQUIRE(num::to_double(stat3(trades)) ==
            Catch::Approx(num::to_double(stat3(flattenTrades(trades)))).epsilon(1e-12));

    // Higher profit_target → higher numer_floor → higher log(PF).
    REQUIRE(num::to_double(stat1(trades)) < num::to_double(stat2(trades)));
    REQUIRE(num::to_double(stat2(trades)) < num::to_double(stat3(trades)));
  }

  SECTION("Higher prior_strength makes log(PF) strictly lower (more pessimistic)")
  {
    // Confirmed from flat-vector tests: higher prior → strictly lower log(PF),
    // regardless of the absolute sign.  Do NOT use |result_high| < |result_low|.
    auto trades = makeOneBarLogTrades(rawReturns, ruin_eps);

    LogPFBars stat_low (ruin_eps, 1e-6, /*prior=*/0.5);
    LogPFBars stat_mid (ruin_eps, 1e-6, /*prior=*/1.0);
    LogPFBars stat_high(ruin_eps, 1e-6, /*prior=*/2.0);

    double result_low  = num::to_double(stat_low(trades));
    double result_mid  = num::to_double(stat_mid(trades));
    double result_high = num::to_double(stat_high(trades));

    REQUIRE(std::isfinite(result_low));
    REQUIRE(std::isfinite(result_mid));
    REQUIRE(std::isfinite(result_high));

    // Strict ordering: lower prior → less pessimistic → higher log(PF).
    REQUIRE(result_high < result_mid);
    REQUIRE(result_mid  < result_low);
  }

  SECTION("All custom parameters: trade overload matches flat log-bar overload")
  {
    const double denom_floor       = 1e-5;
    const double prior_strength    = 0.5;
    const double stop_loss_pct     = 0.04;
    const double profit_target_pct = 0.025;
    const double tiny_win_fraction = 0.1;
    const double tiny_win_min_ret  = 5e-5;

    auto trades = makeOneBarLogTrades(rawReturns, ruin_eps);

    LogPFBars stat(ruin_eps, denom_floor, prior_strength,
                   stop_loss_pct, profit_target_pct,
                   tiny_win_fraction, tiny_win_min_ret);

    REQUIRE(num::to_double(stat(trades)) ==
            Catch::Approx(num::to_double(stat(flattenTrades(trades)))).epsilon(1e-12));
  }

  SECTION("Functor is copyable: copy and original produce identical results")
  {
    LogPFBars stat1(ruin_eps, 1e-6, 1.0, 0.05, 0.03);
    auto stat2 = stat1;

    auto trades = makeOneBarLogTrades(rawReturns, ruin_eps);

    REQUIRE(num::to_double(stat1(trades)) ==
            Catch::Approx(num::to_double(stat2(trades))).epsilon(1e-15));
  }
}


// =============================================================================
// TEST SUITE 5: Multi-bar trade structure
//
// Structural properties unique to the trade overload: total bar count drives
// win/loss accumulation regardless of how bars are grouped into trades, and
// trade ordering does not affect log(PF).  Includes the critical usage-contract
// test confirming that raw returns must not be passed in place of log-bars.
// =============================================================================

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF trade overload: multi-bar trade structure",
          "[StatUtils][LogPF_LogPF][LogBars][Trade][MultiBar]")
{
  SECTION("Trade order does not affect log(PF)")
  {
    // Sum of positive log-bars and sum of negative log-bars are each
    // commutative, so reordering trades cannot change log(PF).
    auto t1 = makeLogTrade({ DecimalType("0.05"), DecimalType("0.03") });
    auto t2 = makeLogTrade({ DecimalType("-0.04"), DecimalType("0.02"), DecimalType("0.01") });

    LogPFBars stat;

    DecimalType forward  = stat({ t1, t2 });
    DecimalType reversed = stat({ t2, t1 });

    REQUIRE(num::to_double(forward) ==
            Catch::Approx(num::to_double(reversed)).epsilon(1e-12));
  }

  SECTION("Regrouping bars across trade boundaries leaves log(PF) unchanged")
  {
    // Six raw returns grouped two ways:
    //   Grouping A: one 6-bar trade.
    //   Grouping B: three 2-bar trades.
    // Both flatten to the same log-bar sequence.
    std::vector<DecimalType> all = {
      DecimalType("0.04"), DecimalType("-0.02"),
      DecimalType("0.06"), DecimalType("-0.01"),
      DecimalType("0.03"), DecimalType("-0.03")
    };

    Trade<DecimalType> singleTrade(Stat::makeLogGrowthSeries(all));

    std::vector<DecimalType> sub1 = { all[0], all[1] };
    std::vector<DecimalType> sub2 = { all[2], all[3] };
    std::vector<DecimalType> sub3 = { all[4], all[5] };

    std::vector<Trade<DecimalType>> splitTrades = {
      makeLogTrade(sub1), makeLogTrade(sub2), makeLogTrade(sub3)
    };

    LogPFBars stat;

    REQUIRE(num::to_double(stat({ singleTrade })) ==
            Catch::Approx(num::to_double(stat(splitTrades))).epsilon(1e-12));
  }

  SECTION("Total bar count (not trade count) determines log(PF)")
  {
    // 10 trades × 3 bars = 30 total log-bars.  The log(PF) must match a single
    // 30-bar trade containing the same returns in the same order.
    std::vector<DecimalType> pattern = {
      DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.03")
    };

    std::vector<DecimalType> allRaw;
    for (int i = 0; i < 10; ++i)
      allRaw.insert(allRaw.end(), pattern.begin(), pattern.end());

    Trade<DecimalType> singleBigTrade(Stat::makeLogGrowthSeries(allRaw));

    std::vector<Trade<DecimalType>> manyTrades;
    for (int i = 0; i < 10; ++i)
      manyTrades.push_back(makeLogTrade(pattern));

    LogPFBars stat;

    REQUIRE(num::to_double(stat(manyTrades)) ==
            Catch::Approx(num::to_double(stat({ singleBigTrade }))).epsilon(1e-12));
  }

  SECTION("Large trade set: 50 trades × 3 bars = 150 log-bars, finite and consistent")
  {
    std::vector<Trade<DecimalType>> trades;
    trades.reserve(50);
    for (int i = 0; i < 50; ++i)
      trades.push_back(makeLogTrade({
        DecimalType("0.005"), DecimalType("-0.002"), DecimalType("0.003")
      }));

    LogPFBars stat;

    DecimalType result      = stat(trades);
    DecimalType flat_result = stat(flattenTrades(trades));

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(flat_result)).epsilon(1e-12));
  }

  SECTION("Log-bars are NOT re-logged: passing raw returns gives a different result")
  {
    // Critical usage-contract test: getDailyReturns() must contain log-growth
    // values, not raw percent returns.  If raw returns were passed, the stat
    // would interpret them as log-bars, producing a different (incorrect) result.
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.05"), DecimalType("-0.02"), DecimalType("0.03")
    };

    // Correct usage: pre-log the returns.
    auto correctTrades = makeOneBarLogTrades(rawReturns);
    LogPFBars stat;
    DecimalType correct = stat(correctTrades);

    // Incorrect usage: pass raw returns directly as if they were log-bars.
    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });
    DecimalType incorrect = stat(rawTrades);

    // The two results must differ, documenting the usage contract.
    REQUIRE(num::to_double(correct) != Catch::Approx(num::to_double(incorrect)).margin(1e-6));
  }
}


// =============================================================================
// TEST SUITE 6: TradeFlatteningAdapter integration
//
// Confirms that LogProfitFactorFromLogBarsStat_LogPF composes correctly with
// TradeFlatteningAdapter and that both paths produce identical results.
// Also verifies the performance contract: no double-logging occurs, so
// adapter(logTrades) agrees with LogProfitFactorStat_LogPF(rawTrades).
// =============================================================================

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF trade overload: TradeFlatteningAdapter integration",
          "[StatUtils][LogPF_LogPF][LogBars][Trade][Adapter]")
{
  SECTION("Adapter wrapping flat log-bar overload matches native trade overload")
  {
    LogPFBars stat;

    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flatLogs) {
        return stat(flatLogs);
      }
    );

    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade({ DecimalType("0.05"), DecimalType("0.02") }),
      makeLogTrade({ DecimalType("-0.03"), DecimalType("0.01"), DecimalType("0.04") }),
      makeLogTrade({ DecimalType("0.00"), DecimalType("-0.01") })
    };

    DecimalType via_adapter        = adapter(trades);
    DecimalType via_trade_overload = stat(trades);

    REQUIRE(num::to_double(via_adapter) ==
            Catch::Approx(num::to_double(via_trade_overload)).epsilon(1e-12));
  }

  SECTION("Adapter stored in std::function is callable and consistent")
  {
    LogPFBars stat;

    std::function<DecimalType(const std::vector<Trade<DecimalType>>&)> fn =
      TradeFlatteningAdapter<DecimalType>(
        [&stat](const std::vector<DecimalType>& flatLogs) { return stat(flatLogs); }
      );

    std::vector<Trade<DecimalType>> trades = {
      makeLogTrade({ DecimalType("0.10"), DecimalType("0.05") }),
      makeLogTrade({ DecimalType("-0.02") })
    };

    DecimalType result = fn(trades);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) ==
            Catch::Approx(num::to_double(stat(trades))).epsilon(1e-12));
  }

  SECTION("Performance contract: adapter on pre-logged trades agrees with raw-return sibling stat")
  {
    // Verifies no double-logging: LogProfitFactorFromLogBarsStat_LogPF via
    // the adapter on pre-logged Trades must agree with LogProfitFactorStat_LogPF
    // called directly on raw-return Trades of the same underlying data.
    std::vector<DecimalType> rawReturns = {
      DecimalType("0.05"), DecimalType("-0.03"), DecimalType("0.02"),
      DecimalType("0.01"), DecimalType("-0.01")
    };

    std::vector<Trade<DecimalType>> rawTrades;
    for (const auto& r : rawReturns)
      rawTrades.emplace_back(std::vector<DecimalType>{ r });

    LogPFStat  raw_stat;
    DecimalType via_raw = raw_stat(rawTrades);

    auto logTrades = makeOneBarLogTrades(rawReturns);

    LogPFBars log_stat;
    TradeFlatteningAdapter<DecimalType> adapter(
      [&log_stat](const std::vector<DecimalType>& flatLogs) { return log_stat(flatLogs); }
    );

    DecimalType via_adapter = adapter(logTrades);

    REQUIRE(num::to_double(via_raw) ==
            Catch::Approx(num::to_double(via_adapter)).epsilon(1e-12));
  }

  SECTION("Empty trade vector: adapter and native overload both return zero")
  {
    LogPFBars stat;

    TradeFlatteningAdapter<DecimalType> adapter(
      [&stat](const std::vector<DecimalType>& flatLogs) { return stat(flatLogs); }
    );

    std::vector<Trade<DecimalType>> empty;

    REQUIRE(stat(empty) == DC::DecimalZero);
    REQUIRE(adapter(empty) == DC::DecimalZero);
  }
}
