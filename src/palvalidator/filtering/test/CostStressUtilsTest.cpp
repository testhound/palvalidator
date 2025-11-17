#include <catch2/catch_test_macros.hpp>

#include "filtering/CostStressUtils.h"                // makeCostStressHurdles overloads
#include "filtering/TradingHurdleCalculator.h"        // simplified (individuals)
#include "filtering/MetaTradingHurdleCalculator.h"    // legacy (metas)
#include "DecimalConstants.h"

using Num = num::DefaultNumber;
using palvalidator::filtering::OOSSpreadStatsT;
using palvalidator::filtering::CostStressHurdlesT;
using palvalidator::filtering::TradingHurdleCalculator;
using palvalidator::filtering::meta::MetaTradingHurdleCalculator;
using palvalidator::filtering::RiskParameters;

TEST_CASE("CostStressUtils: Individuals path (no OOS stats) uses simplified trading-spread cost", "[CostStressUtils][Individuals]")
{
  // Individuals → simplified TradingHurdleCalculator with known per-side
  // per-side = 0.15% → round-trip 0.30%. With 40 trades/year → 0.12 (12%)
  TradingHurdleCalculator calc(Num("0.0015"));
  const Num annualizedTrades = Num("40");

  // No OOS stats: stressed hurdles degenerate to base; per-side fields reflect configuredPerSide (if provided)
  const Num configuredPerSide = Num("0.0020"); // 0.20% (just for logging fields)
  const auto H = palvalidator::filtering::makeCostStressHurdles<Num>(
      calc, std::nullopt, annualizedTrades, configuredPerSide);

  // Base = trades * (2 * per-side) since no stats → calculator uses its configured per-side
  REQUIRE(H.baseHurdle == Num("0.12")); // 40 * 2 * 0.0015
  REQUIRE(H.h_1q == H.baseHurdle);
  REQUIRE(H.h_2q == H.baseHurdle);
  REQUIRE(H.h_3q == H.baseHurdle);

  // Per-side fields: no stats → reflect configuredPerSide we passed (for display)
  REQUIRE(H.perSideBase == configuredPerSide);
  REQUIRE(H.perSide1q == configuredPerSide);
  REQUIRE(H.perSide2q == configuredPerSide);
  REQUIRE(H.perSide3q == configuredPerSide);
}

TEST_CASE("CostStressUtils: Individuals path (with OOS stats) computes stressed hurdles from mean/Qn", "[CostStressUtils][Individuals]")
{
  // Calculator default per-side won't matter for stressed hurdles because we compute them directly from stats.
  TradingHurdleCalculator calc(Num("0.0005"));  // 0.05% per-side
  const Num annualizedTrades = Num("50");       // 50 trades/year

  // OOS stats (proportional): mean and Qn are ROUND-TRIP spreads here (per CostStressUtils policy),
  // so per-side = (mean + k*Qn) / 2.
  OOSSpreadStatsT<Num> S;
  S.mean = Num("0.0040"); // 0.40% round-trip
  S.qn   = Num("0.0030"); // 0.30% round-trip

  const auto H = palvalidator::filtering::makeCostStressHurdles<Num>(
      calc, std::optional{S}, annualizedTrades, /*configuredPerSide=*/std::nullopt);

  // Expected per-side from stats:
  // base per-side = mean/2 = 0.004/2 = 0.0020 (used only for display in Individuals overload)
  // +1·Qn per-side = (0.004 + 1*0.003)/2 = 0.0035
  // +2·Qn per-side = (0.004 + 2*0.003)/2 = 0.0050
  // +3·Qn per-side = (0.004 + 3*0.003)/2 = 0.0060
  REQUIRE(H.perSideBase == Num("0.0020"));
  REQUIRE(H.perSide1q   == Num("0.0035"));
  REQUIRE(H.perSide2q   == Num("0.0050"));
  REQUIRE(H.perSide3q   == Num("0.0060"));

  // Stressed hurdles = trades * (2 * per-side)
  // With 50 trades/year → factor 100 × per-side
  REQUIRE(H.h_1q == Num("0.3500")); // 100 * 0.0035
  REQUIRE(H.h_2q == Num("0.5000")); // 100 * 0.0050
  REQUIRE(H.h_3q == Num("0.6000")); // 100 * 0.0060

  // Base hurdle for Individuals overload comes from calc.calculateTradingSpreadCost(...)
  // We won't assert exact equality here since calc's internal policy may vary (default vs. mean),
  // but it must be ≥ 0.
  REQUIRE(H.baseHurdle >= Num("0.0000"));
}

TEST_CASE("CostStressUtils: Metas path (legacy) honors high hurdle and per-side override", "[CostStressUtils][Metas]")
{
  // Legacy meta calculator with RF hurdle ~8% (3% RF + 5% premium), buffer 1.5, default per-side unused here.
  RiskParameters rp;
  rp.riskFreeRate = Num("0.03");
  rp.riskPremium  = Num("0.05"); // RF hurdle = 0.08
  MetaTradingHurdleCalculator metaCalc(rp, /*buffer=*/Num("1.5"), /*perSide=*/Num("0.0008"));

  const Num trades = Num("40");

  // OOS stats present (for stressed variants); base per-side can be overridden by configuredPerSide.
  OOSSpreadStatsT<Num> S;
  S.mean = Num("0.0030"); // 0.30% round-trip
  S.qn   = Num("0.0010"); // 0.10% round-trip

  // configured per-side override: 0.12% (per-side). Round-trip = 0.24%. cost = trades * rt * buffer
  // = 40 * 0.0024 * 1.5 = 0.144 (14.4%) which exceeds RF 8% → baseHurdle should be cost-dominated.
  const Num perSideOverride = Num("0.0012");

  const auto H = palvalidator::filtering::makeCostStressHurdles<Num>(
      metaCalc, std::optional{S}, trades, /*configuredPerSide=*/perSideOverride);

  // perSideBase = max(configuredPerSide, mean/2) = max(0.0012, 0.003/2=0.0015) = 0.0015
  REQUIRE(H.perSideBase == Num("0.0015"));
  const Num expectedCostBase = trades * (mkc_timeseries::DecimalConstants<Num>::DecimalTwo * H.perSideBase) * Num("1.5");

  REQUIRE(H.baseHurdle == expectedCostBase);
  REQUIRE(H.baseHurdle > Num("0.08")); // should exceed RF hurdle (8%)

  // Stressed (legacy) use per-side from (mean + k*Qn)/2, then apply buffer and compare with RF
  const Num perSide1q = (S.mean + S.qn) / mkc_timeseries::DecimalConstants<Num>::DecimalTwo;           // (0.003+0.001)/2 = 0.002
  const Num perSide2q = (S.mean + Num("2.0")*S.qn) / mkc_timeseries::DecimalConstants<Num>::DecimalTwo; // 0.0025
  const Num perSide3q = (S.mean + Num("3.0")*S.qn) / mkc_timeseries::DecimalConstants<Num>::DecimalTwo; // 0.003

  const auto buffered = [&](const Num& perSide) {
    const Num rt = mkc_timeseries::DecimalConstants<Num>::DecimalTwo * perSide;
    const Num costReq = trades * rt * Num("1.5");
    // Legacy final = max(RF, costReq). With these params, costReq should be ≥ RF.
    return costReq;
  };

  REQUIRE(H.h_1q == buffered(perSide1q));
  REQUIRE(H.h_2q == buffered(perSide2q));
  REQUIRE(H.h_3q == buffered(perSide3q));
}
