// TradeBootstrapTest.cpp
//
// Unit tests for trade-level bootstrapping using:
//   - Trade<Decimal>                          (TradeResampling.h)
//   - IIDResampler<Trade<Decimal>>            (BiasCorrectedBootstrap.h)
//   - BCaBootStrap<..., SampleType=Trade<D>>  (BiasCorrectedBootstrap.h)
//   - BCaAnnualizer with trade-level BCa
//
// Design rationale (see Block_Bootstrapping_Issues.md / Flat_Returns_To_Trade_Returns.md):
//   The atomic unit of resampling is the Trade, not the bar. Trades are treated
//   as i.i.d. because their between-trade independence is well-founded once
//   within-trade correlation is locked inside the Trade package. IID resampling
//   on ~9 trades offers combinatorial diversity of C(17,9) = 24,310 unique
//   samples, far exceeding the ~7 effective draws available under block
//   bootstrapping on 27 bars.
//
// Coverage:
//   1. Trade class basic contract
//   2. IIDResampler<Trade<Decimal>> resampling mechanics
//   3. IIDResampler<Trade<Decimal>>::jackknife — type, size, exact values
//   4. BCaBootStrap full integration (construction, interval validity, diagnostics)
//   5. getSampleSize() reflects trade count, not bar count
//   6. Degenerate case: all identical trades
//   7. Positive / negative strategy CI direction
//   8. BCaAnnualizer compatibility with trade-level BCa
//   9. Error paths (empty sample, too-few trades)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <functional>

#include "BiasCorrectedBootstrap.h"
#include "TradeResampling.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"

using namespace mkc_timeseries;

// =============================================================================
// Test helpers
// =============================================================================

namespace
{
  using D = DecimalType;

  // ---------------------------------------------------------------------------
  // Build a Trade from a brace-initializer list of double literals.
  // ---------------------------------------------------------------------------
  Trade<D> makeTrade(std::initializer_list<double> returns)
  {
    std::vector<D> v;
    v.reserve(returns.size());
    for (double r : returns)
      v.push_back(D(std::to_string(r).c_str()));
    return Trade<D>(std::move(v));
  }

  // ---------------------------------------------------------------------------
  // Flatten a vector of trades into a single bar-return vector.
  // ---------------------------------------------------------------------------
  std::vector<D> flatten(const std::vector<Trade<D>>& trades)
  {
    std::vector<D> flat;
    flat.reserve(trades.size() * 3);
    for (const auto& t : trades)
      flat.insert(flat.end(), t.getDailyReturns().begin(), t.getDailyReturns().end());
    return flat;
  }

  // ---------------------------------------------------------------------------
  // Arithmetic mean of a flat vector.
  // ---------------------------------------------------------------------------
  D vecMean(const std::vector<D>& v)
  {
    if (v.empty()) return D(0);
    D sum(0);
    for (const auto& x : v) sum += x;
    return sum / D(static_cast<double>(v.size()));
  }

  // ---------------------------------------------------------------------------
  // Statistic for trade-level bootstrap: flatten then compute arithmetic mean.
  //
  // This satisfies BCaBootStrap<D, IIDResampler<Trade<D>>, ..., Trade<D>>::StatFn
  // which is std::function<D(const std::vector<Trade<D>>&)>.
  //
  // In production code GeoMeanStat::operator()(const std::vector<Trade<D>>&)
  // fills the same role; arithmetic mean is used here for exact verifiability.
  // ---------------------------------------------------------------------------
  D tradeMeanStat(const std::vector<Trade<D>>& trades)
  {
    return vecMean(flatten(trades));
  }

  // ---------------------------------------------------------------------------
  // Produce a realistic 9-trade population (positive expectation).
  // Median holding period = 3 bars, mirroring n_bars=27 / median_hold=3.
  // ---------------------------------------------------------------------------
  std::vector<Trade<D>> makePositiveTrades()
  {
    return {
      makeTrade({ 0.005,  0.008,  0.003}),   // 3-bar winner
      makeTrade({ 0.012, -0.002}),            // 2-bar winner
      makeTrade({-0.004,  0.001,  0.007}),   // 3-bar net winner
      makeTrade({ 0.003,  0.004,  0.002}),   // 3-bar winner
      makeTrade({-0.003, -0.002}),            // 2-bar loser
      makeTrade({ 0.010,  0.005,  0.003}),   // 3-bar winner
      makeTrade({ 0.001,  0.002}),            // 2-bar small winner
      makeTrade({-0.001,  0.006,  0.004}),   // 3-bar net winner
      makeTrade({ 0.008}),                    // 1-bar winner
    };
  }

  // 9 losing trades (negative expectation)
  std::vector<Trade<D>> makeNegativeTrades()
  {
    return {
      makeTrade({-0.005, -0.008, -0.003}),
      makeTrade({-0.012,  0.002}),
      makeTrade({ 0.004, -0.001, -0.007}),
      makeTrade({-0.003, -0.004, -0.002}),
      makeTrade({ 0.003,  0.002}),
      makeTrade({-0.010, -0.005, -0.003}),
      makeTrade({-0.001, -0.002}),
      makeTrade({ 0.001, -0.006, -0.004}),
      makeTrade({-0.008}),
    };
  }

  // Convenience typedef for the trade-level BCa instantiation.
  using TradeResampler = IIDResampler<Trade<D>>;
  using TradeBCa       = BCaBootStrap<D,
                                      TradeResampler,
                                      randutils::mt19937_rng,
                                      void,
                                      Trade<D>>;
} // anonymous namespace


// =============================================================================
// 1. Trade class basic contract
// =============================================================================

TEST_CASE("Trade: construction and accessors", "[Trade]")
{
  SECTION("Move-construct from vector")
  {
    Trade<D> t = makeTrade({0.01, 0.02, -0.005});
    REQUIRE(t.getDuration() == 3);
    REQUIRE(!t.empty());
    REQUIRE(num::to_double(t.getDailyReturns()[0]) == Catch::Approx(0.01));
    REQUIRE(num::to_double(t.getDailyReturns()[1]) == Catch::Approx(0.02));
    REQUIRE(num::to_double(t.getDailyReturns()[2]) == Catch::Approx(-0.005));
  }

  SECTION("Incremental construction via addReturn")
  {
    Trade<D> t;
    REQUIRE(t.empty());
    t.addReturn(D("0.005"));
    t.addReturn(D("0.010"));
    REQUIRE(t.getDuration() == 2);
    REQUIRE(!t.empty());
  }

  SECTION("Equality operator: identical trades compare equal")
  {
    Trade<D> a = makeTrade({0.01, 0.02});
    Trade<D> b = makeTrade({0.01, 0.02});
    REQUIRE(a == b);
  }

  SECTION("Equality operator: different trades compare not equal")
  {
    Trade<D> a = makeTrade({0.01, 0.02});
    Trade<D> b = makeTrade({0.01, 0.03});
    REQUIRE(!(a == b));
  }

  SECTION("operator< orders by total return")
  {
    Trade<D> loser  = makeTrade({-0.05});
    Trade<D> winner = makeTrade({ 0.05});
    REQUIRE(loser < winner);
    REQUIRE(!(winner < loser));
  }
}


// =============================================================================
// 2. IIDResampler<Trade<Decimal>> resampling mechanics
// =============================================================================

TEST_CASE("IIDResampler<Trade>: output size matches requested n", "[IIDResampler][Trade]")
{
  auto trades = makePositiveTrades();       // 9 trades
  TradeResampler resampler;
  randutils::mt19937_rng rng;

  auto sample = resampler(trades, trades.size(), rng);

  REQUIRE(sample.size() == trades.size());
}

TEST_CASE("IIDResampler<Trade>: every element in resample is a copy of an original trade",
          "[IIDResampler][Trade]")
{
  // Verify that the resampler only produces Trade objects drawn from the
  // original population (sampling with replacement, not synthesis).
  auto trades = makePositiveTrades();
  TradeResampler resampler;
  randutils::mt19937_rng rng;

  for (int trial = 0; trial < 20; ++trial)
  {
    auto sample = resampler(trades, trades.size(), rng);
    for (const auto& t : sample)
    {
      bool found = std::any_of(trades.begin(), trades.end(),
                               [&t](const Trade<D>& orig){ return orig == t; });
      REQUIRE(found);
    }
  }
}

TEST_CASE("IIDResampler<Trade>: sampling with replacement produces duplicates",
          "[IIDResampler][Trade]")
{
  // With replacement, at least some bootstrap samples across many draws
  // must contain a trade appearing more than once.
  // P(no duplicate in one sample of n=9 from n=9) = 9!/9^9 ≈ 0.00036
  // Over 50 trials the probability of never seeing a duplicate is negligible.
  auto trades = makePositiveTrades();
  TradeResampler resampler;
  randutils::mt19937_rng rng;

  bool ever_saw_duplicate = false;
  for (int trial = 0; trial < 50 && !ever_saw_duplicate; ++trial)
  {
    auto sample = resampler(trades, trades.size(), rng);
    for (std::size_t i = 0; i < sample.size() && !ever_saw_duplicate; ++i)
      for (std::size_t j = i + 1; j < sample.size(); ++j)
        if (sample[i] == sample[j]) { ever_saw_duplicate = true; break; }
  }
  REQUIRE(ever_saw_duplicate);
}

TEST_CASE("IIDResampler<Trade>: getL returns 1 (no block structure)", "[IIDResampler][Trade]")
{
  TradeResampler resampler;
  REQUIRE(resampler.getL() == 1);
}

TEST_CASE("IIDResampler<Trade>: empty sample throws", "[IIDResampler][Trade][Error]")
{
  TradeResampler resampler;
  randutils::mt19937_rng rng;
  std::vector<Trade<D>> empty;
  REQUIRE_THROWS_AS(resampler(empty, 5, rng), std::invalid_argument);
}

TEST_CASE("IIDResampler<Trade>: in-place operator() fills output vector",
          "[IIDResampler][Trade]")
{
  auto trades = makePositiveTrades();
  TradeResampler resampler;
  randutils::mt19937_rng rng;

  std::vector<Trade<D>> out;
  resampler(trades, out, trades.size(), rng);

  REQUIRE(out.size() == trades.size());
  for (const auto& t : out)
    REQUIRE(!t.empty());
}


// =============================================================================
// 3. IIDResampler<Trade<Decimal>>::jackknife — type, size, and exact values
// =============================================================================

TEST_CASE("IIDResampler<Trade>::jackknife: produces n Decimal pseudo-values",
          "[IIDResampler][Trade][Jackknife]")
{
  // The jackknife return type must be std::vector<Decimal>, NOT
  // std::vector<Trade<Decimal>>. This is the critical compile-time contract:
  // the statistic maps vector<Trade<D>> -> D, so the pseudo-values are D.
  auto trades = makePositiveTrades();   // n = 9
  TradeResampler resampler;

  auto jk = resampler.jackknife(trades, tradeMeanStat);

  // Size: n pseudo-values (delete-one-trade jackknife)
  REQUIRE(jk.size() == trades.size());

  // Every pseudo-value must be a finite Decimal
  for (const auto& pv : jk)
    REQUIRE(std::isfinite(num::to_double(pv)));
}

TEST_CASE("IIDResampler<Trade>::jackknife: exact delete-one-trade values",
          "[IIDResampler][Trade][Jackknife]")
{
  // Use 3 trades with known returns to verify exact pseudo-values.
  //
  // T0 = {0.01,  0.02}         flat: [0.01,  0.02]          sum=0.030
  // T1 = {-0.01, -0.005, 0.005} flat: [-0.01, -0.005, 0.005] sum=-0.010
  // T2 = {0.015}               flat: [0.015]                 sum=0.015
  //
  // Delete T0 → flatten(T1, T2) = [-0.01, -0.005, 0.005, 0.015]  mean = 0.005/4  = 0.00125
  // Delete T1 → flatten(T0, T2) = [0.01, 0.02, 0.015]             mean = 0.045/3  = 0.015
  // Delete T2 → flatten(T0, T1) = [0.01, 0.02, -0.01, -0.005, 0.005] mean=0.02/5 = 0.004

  std::vector<Trade<D>> trades = {
    makeTrade({ 0.01,  0.02}),
    makeTrade({-0.01, -0.005,  0.005}),
    makeTrade({ 0.015}),
  };

  TradeResampler resampler;
  auto jk = resampler.jackknife(trades, tradeMeanStat);

  REQUIRE(jk.size() == 3);
  REQUIRE(num::to_double(jk[0]) == Catch::Approx(0.00125).epsilon(1e-10));
  REQUIRE(num::to_double(jk[1]) == Catch::Approx(0.015  ).epsilon(1e-10));
  REQUIRE(num::to_double(jk[2]) == Catch::Approx(0.004  ).epsilon(1e-10));
}

TEST_CASE("IIDResampler<Trade>::jackknife: single-trade leave-one-out leaves n-1 trades",
          "[IIDResampler][Trade][Jackknife]")
{
  // Verify that deleting trade i and computing the statistic on the remaining
  // n-1 trades produces a different result than computing on all n trades.
  // This confirms the delete-one logic is actually removing a trade.
  auto trades = makePositiveTrades();   // n = 9

  // Full-population statistic
  D full_stat = tradeMeanStat(trades);

  TradeResampler resampler;
  auto jk = resampler.jackknife(trades, tradeMeanStat);

  // At least one pseudo-value must differ from the full-population statistic
  // (guaranteed unless all trades have identical flattened-mean contribution,
  //  which is impossible given our heterogeneous test data).
  bool any_differs = std::any_of(jk.begin(), jk.end(), [&](const D& pv){
    return std::abs(num::to_double(pv) - num::to_double(full_stat)) > 1e-12;
  });
  REQUIRE(any_differs);
}

TEST_CASE("IIDResampler<Trade>::jackknife: requires at least 2 trades",
          "[IIDResampler][Trade][Jackknife][Error]")
{
  TradeResampler resampler;

  SECTION("Single trade throws")
  {
    std::vector<Trade<D>> one = { makeTrade({0.01, 0.02}) };
    REQUIRE_THROWS_AS(resampler.jackknife(one, tradeMeanStat), std::invalid_argument);
  }

  SECTION("Empty vector throws")
  {
    std::vector<Trade<D>> empty;
    REQUIRE_THROWS_AS(resampler.jackknife(empty, tradeMeanStat), std::invalid_argument);
  }
}


// =============================================================================
// 4. BCaBootStrap full integration — construction, interval validity, diagnostics
// =============================================================================

TEST_CASE("BCaBootStrap<Trade>: basic construction and lazy evaluation",
          "[BCaBootStrap][Trade][Integration]")
{
  auto trades = makePositiveTrades();
  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});

  // Trigger calculation via any accessor
  D lower = bca.getLowerBound();
  D upper = bca.getUpperBound();
  D stat  = bca.getMean();

  REQUIRE(std::isfinite(num::to_double(lower)));
  REQUIRE(std::isfinite(num::to_double(upper)));
  REQUIRE(std::isfinite(num::to_double(stat)));
}

TEST_CASE("BCaBootStrap<Trade>: confidence interval ordering lower <= mean <= upper",
          "[BCaBootStrap][Trade][Integration]")
{
  auto trades = makePositiveTrades();
  TradeBCa bca(trades, 2000, 0.95, tradeMeanStat, TradeResampler{});

  REQUIRE(bca.getLowerBound() <= bca.getMean());
  REQUIRE(bca.getMean()       <= bca.getUpperBound());
}

TEST_CASE("BCaBootStrap<Trade>: 99% interval is wider than 95% interval",
          "[BCaBootStrap][Trade][Integration]")
{
  auto trades = makePositiveTrades();

  TradeBCa bca95(trades, 2000, 0.95, tradeMeanStat, TradeResampler{});
  TradeBCa bca99(trades, 2000, 0.99, tradeMeanStat, TradeResampler{});

  D width95 = bca95.getUpperBound() - bca95.getLowerBound();
  D width99 = bca99.getUpperBound() - bca99.getLowerBound();

  REQUIRE(num::to_double(width99) > num::to_double(width95));
}

TEST_CASE("BCaBootStrap<Trade>: BCa diagnostics are accessible and finite",
          "[BCaBootStrap][Trade][Diagnostics]")
{
  auto trades = makePositiveTrades();
  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});

  double z0   = bca.getZ0();
  D      accel = bca.getAcceleration();

  REQUIRE(std::isfinite(z0));
  REQUIRE(std::isfinite(num::to_double(accel)));
}

TEST_CASE("BCaBootStrap<Trade>: getBootstrapStatistics returns B Decimal values",
          "[BCaBootStrap][Trade][Diagnostics]")
{
  auto trades = makePositiveTrades();
  unsigned int B = 500;
  TradeBCa bca(trades, B, 0.95, tradeMeanStat, TradeResampler{});

  const auto& boot = bca.getBootstrapStatistics();

  REQUIRE(boot.size() == B);

  for (const auto& s : boot)
    REQUIRE(std::isfinite(num::to_double(s)));
}

TEST_CASE("BCaBootStrap<Trade>: getConfidenceLevel and getNumResamples round-trip",
          "[BCaBootStrap][Trade][Diagnostics]")
{
  auto trades = makePositiveTrades();
  unsigned int B  = 1200;
  double       cl = 0.90;
  TradeBCa bca(trades, B, cl, tradeMeanStat, TradeResampler{});

  REQUIRE(bca.getConfidenceLevel() == Catch::Approx(cl));
  REQUIRE(bca.getNumResamples()    == B);
}


// =============================================================================
// 5. getSampleSize() reflects trade count, not bar count
// =============================================================================

TEST_CASE("BCaBootStrap<Trade>: getSampleSize returns number of trades, not bars",
          "[BCaBootStrap][Trade][SampleSize]")
{
  // This is the key semantic test. The input has 9 trades and a total of
  // 9*3=27 bars (approximate). getSampleSize() must return 9 (trade count),
  // not 27 (bar count). The bootstrap resamples over trades, not bars.
  auto trades = makePositiveTrades();   // 9 trades
  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});

  REQUIRE(bca.getSampleSize() == trades.size());  // 9, not ~27
  REQUIRE(bca.getSampleSize() == 9);
}

TEST_CASE("BCaBootStrap<Trade>: getSampleSize matches input vector size exactly",
          "[BCaBootStrap][Trade][SampleSize]")
{
  // Varying trade count: getSampleSize must always equal the number of
  // Trade objects passed in, regardless of how many bars each contains.

  SECTION("5 trades of varying duration")
  {
    std::vector<Trade<D>> trades = {
      makeTrade({0.01}),
      makeTrade({0.01, 0.02}),
      makeTrade({0.01, 0.02, 0.03}),
      makeTrade({0.01, 0.02, 0.03, 0.04}),
      makeTrade({0.01, 0.02, 0.03, 0.04, 0.05}),
    };
    TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});
    REQUIRE(bca.getSampleSize() == 5);
  }

  SECTION("2 trades (minimum)")
  {
    std::vector<Trade<D>> trades = {
      makeTrade({0.01, 0.02}),
      makeTrade({-0.01}),
    };
    TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});
    REQUIRE(bca.getSampleSize() == 2);
  }
}


// =============================================================================
// 6. Degenerate case: all identical trades
// =============================================================================

TEST_CASE("BCaBootStrap<Trade>: all-identical-trades triggers degenerate handling",
          "[BCaBootStrap][Trade][EdgeCase]")
{
  // When every trade is identical the bootstrap distribution collapses to a
  // point mass. BCaBootStrap must detect this and return lower == upper == mean
  // with z0 = 0 and acceleration = 0.
  Trade<D> identical = makeTrade({0.005, 0.010, -0.002});
  std::vector<Trade<D>> trades(9, identical);

  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});

  D lower = bca.getLowerBound();
  D upper = bca.getUpperBound();
  D mean  = bca.getMean();

  // All three must be equal (degenerate distribution)
  REQUIRE(num::to_double(lower) == Catch::Approx(num::to_double(mean)).epsilon(1e-10));
  REQUIRE(num::to_double(upper) == Catch::Approx(num::to_double(mean)).epsilon(1e-10));

  // BCa diagnostics must be benign
  REQUIRE(bca.getZ0() == Catch::Approx(0.0));
  REQUIRE(num::to_double(bca.getAcceleration()) == Catch::Approx(0.0));
}


// =============================================================================
// 7. Positive / negative strategy CI direction
// =============================================================================

TEST_CASE("BCaBootStrap<Trade>: consistently profitable strategy has positive CI lower bound",
          "[BCaBootStrap][Trade][Strategy]")
{
  // All trades are winners. The 95% lower bound should be positive,
  // reflecting that the strategy is statistically profitable.
  std::vector<Trade<D>> winners = {
    makeTrade({0.010, 0.005}),
    makeTrade({0.020}),
    makeTrade({0.008, 0.012, 0.003}),
    makeTrade({0.015, 0.007}),
    makeTrade({0.005, 0.009, 0.006}),
    makeTrade({0.018}),
    makeTrade({0.006, 0.004, 0.011}),
    makeTrade({0.014, 0.008}),
    makeTrade({0.009, 0.013}),
  };

  TradeBCa bca(winners, 2000, 0.95, tradeMeanStat, TradeResampler{});

  REQUIRE(num::to_double(bca.getLowerBound()) > 0.0);
  REQUIRE(num::to_double(bca.getUpperBound()) > 0.0);
}

TEST_CASE("BCaBootStrap<Trade>: consistently losing strategy has negative CI upper bound",
          "[BCaBootStrap][Trade][Strategy]")
{
  // All trades are losers. The 95% upper bound should be negative.
  auto losers = makeNegativeTrades();

  TradeBCa bca(losers, 2000, 0.95, tradeMeanStat, TradeResampler{});

  REQUIRE(num::to_double(bca.getUpperBound()) < 0.0);
  REQUIRE(num::to_double(bca.getLowerBound()) < 0.0);
}

TEST_CASE("BCaBootStrap<Trade>: mixed strategy CI contains zero",
          "[BCaBootStrap][Trade][Strategy]")
{
  // Half winners, half losers — the CI should straddle zero.
  std::vector<Trade<D>> mixed = {
    makeTrade({ 0.020,  0.010}),
    makeTrade({ 0.015}),
    makeTrade({ 0.012,  0.008, 0.005}),
    makeTrade({ 0.018,  0.009}),
    makeTrade({ 0.011,  0.007}),
    makeTrade({-0.020, -0.010}),
    makeTrade({-0.015}),
    makeTrade({-0.012, -0.008, -0.005}),
    makeTrade({-0.018}),
  };

  TradeBCa bca(mixed, 2000, 0.95, tradeMeanStat, TradeResampler{});

  D lower = bca.getLowerBound();
  D upper = bca.getUpperBound();

  // Interval must straddle zero for a balanced mix
  REQUIRE(num::to_double(lower) < 0.0);
  REQUIRE(num::to_double(upper) > 0.0);
}


// =============================================================================
// 8. BCaAnnualizer compatibility with trade-level BCa
// =============================================================================

TEST_CASE("BCaAnnualizer: accepts trade-level BCaBootStrap (5-param instantiation)",
          "[BCaAnnualizer][Trade][Integration]")
{
  // BCaAnnualizer must compile and run correctly against a BCaBootStrap
  // instantiated with SampleType = Trade<D>. It reads only Decimal accessors
  // (getMean, getLowerBound, getUpperBound) so SampleType is irrelevant to it.
  auto trades = makePositiveTrades();
  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});

  REQUIRE_NOTHROW(BCaAnnualizer<D>(bca, 252.0));

  BCaAnnualizer<D> ann(bca, 252.0);

  D ann_mean  = ann.getAnnualizedMean();
  D ann_lower = ann.getAnnualizedLowerBound();
  D ann_upper = ann.getAnnualizedUpperBound();

  REQUIRE(std::isfinite(num::to_double(ann_mean)));
  REQUIRE(std::isfinite(num::to_double(ann_lower)));
  REQUIRE(std::isfinite(num::to_double(ann_upper)));
}

TEST_CASE("BCaAnnualizer: annualized trade-level mean is larger than daily mean (positive strategy)",
          "[BCaAnnualizer][Trade][Integration]")
{
  auto trades = makePositiveTrades();
  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});
  BCaAnnualizer<D> ann(bca, 252.0);

  D daily_mean      = bca.getMean();
  D annualized_mean = ann.getAnnualizedMean();

  // For a positive daily mean, (1+r)^252 - 1  >  r
  if (num::to_double(daily_mean) > 0.0)
    REQUIRE(num::to_double(annualized_mean) > num::to_double(daily_mean));
}

TEST_CASE("BCaAnnualizer: annualized trade-level CI maintains lower <= mean <= upper ordering",
          "[BCaAnnualizer][Trade][Integration]")
{
  auto trades = makePositiveTrades();
  TradeBCa bca(trades, 1000, 0.95, tradeMeanStat, TradeResampler{});
  BCaAnnualizer<D> ann(bca, 252.0);

  REQUIRE(ann.getAnnualizedLowerBound() <= ann.getAnnualizedMean());
  REQUIRE(ann.getAnnualizedMean()       <= ann.getAnnualizedUpperBound());
}


// =============================================================================
// 9. Error paths
// =============================================================================

TEST_CASE("BCaBootStrap<Trade>: empty trade vector throws at construction",
          "[BCaBootStrap][Trade][Error]")
{
  std::vector<Trade<D>> empty;
  REQUIRE_THROWS_AS(
    TradeBCa(empty, 1000, 0.95, tradeMeanStat, TradeResampler{}),
    std::invalid_argument);
}

TEST_CASE("BCaBootStrap<Trade>: fewer than 100 resamples throws at construction",
          "[BCaBootStrap][Trade][Error]")
{
  auto trades = makePositiveTrades();
  REQUIRE_THROWS_AS(
    TradeBCa(trades, 99, 0.95, tradeMeanStat, TradeResampler{}),
    std::invalid_argument);
}

TEST_CASE("BCaBootStrap<Trade>: invalid confidence level throws at construction",
          "[BCaBootStrap][Trade][Error]")
{
  auto trades = makePositiveTrades();

  SECTION("confidence_level = 0.0")
  {
    REQUIRE_THROWS_AS(
      TradeBCa(trades, 1000, 0.0, tradeMeanStat, TradeResampler{}),
      std::invalid_argument);
  }

  SECTION("confidence_level = 1.0")
  {
    REQUIRE_THROWS_AS(
      TradeBCa(trades, 1000, 1.0, tradeMeanStat, TradeResampler{}),
      std::invalid_argument);
  }

  SECTION("confidence_level > 1.0")
  {
    REQUIRE_THROWS_AS(
      TradeBCa(trades, 1000, 1.5, tradeMeanStat, TradeResampler{}),
      std::invalid_argument);
  }
}

TEST_CASE("BCaBootStrap<Trade>: null statistic function throws at construction",
          "[BCaBootStrap][Trade][Error]")
{
  auto trades = makePositiveTrades();
  std::function<D(const std::vector<Trade<D>>&)> null_stat;  // empty

  REQUIRE_THROWS_AS(
    TradeBCa(trades, 1000, 0.95, null_stat, TradeResampler{}),
    std::invalid_argument);
}


// =============================================================================
// 10. Consistency: trade-level and bar-level bootstrap agree on same data
// =============================================================================

TEST_CASE("BCaBootStrap<Trade> vs BCaBootStrap<Decimal>: statistics are consistent on same data",
          "[BCaBootStrap][Trade][Consistency]")
{
  // Build the flat bar vector that corresponds to our trade population so we
  // can compare trade-level and bar-level bootstrap estimates. They are not
  // identical (different resamplers, different n), but their point estimates
  // (theta_hat) must agree exactly because both compute the same arithmetic
  // mean statistic on the same underlying numbers.
  auto trades = makePositiveTrades();
  std::vector<D> bars = flatten(trades);

  // Trade-level BCa
  TradeBCa trade_bca(trades, 2000, 0.95, tradeMeanStat, TradeResampler{});

  // Bar-level BCa (IIDResampler<Decimal>, the original default)
  auto bar_mean_fn = [](const std::vector<D>& v) -> D { return vecMean(v); };
  BCaBootStrap<D> bar_bca(bars, 2000, 0.95, bar_mean_fn);

  // Point estimates must be identical: both compute mean of the same numbers.
  REQUIRE(num::to_double(trade_bca.getMean()) ==
          Catch::Approx(num::to_double(bar_bca.getMean())).epsilon(1e-10));

  // Both intervals must be valid (ordering holds)
  REQUIRE(trade_bca.getLowerBound() <= trade_bca.getUpperBound());
  REQUIRE(bar_bca.getLowerBound()   <= bar_bca.getUpperBound());
}
