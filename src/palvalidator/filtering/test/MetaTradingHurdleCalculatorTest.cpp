#include <catch2/catch_test_macros.hpp>

#include "filtering/MetaTradingHurdleCalculator.h"
#include "filtering/FilteringTypes.h"
#include "DecimalConstants.h"

using palvalidator::filtering::meta::MetaTradingHurdleCalculator;
using palvalidator::filtering::RiskParameters;
using Num = num::DefaultNumber;

TEST_CASE("MetaTradingHurdleCalculator: RF+premium dominates when costs are small", "[MetaHurdle]")
{
  RiskParameters rp;
  rp.riskFreeRate = Num("0.03");   // 3%
  rp.riskPremium  = Num("0.05");   // 5%  → RF hurdle = 8%

  // Cost params: per-side 0.10%, buffer 1.5
  const Num perSide = Num("0.001");
  const Num buffer  = Num("1.5");

  MetaTradingHurdleCalculator calc(rp, buffer, perSide);

  // Low trading activity so cost-based hurdle is below 8%:
  // round-trip = 2*0.001 = 0.002;  trades=10 → 0.002*10 = 0.02;  *1.5 = 0.03 (3%)
  const Num annualizedTrades = Num("10");
  const Num rf_hurdle = calc.calculateRiskFreeHurdle(); // 0.08
  const Num cost_req  = calc.calculateCostBasedRequiredReturn(annualizedTrades); // 0.03
  const Num final_req = calc.calculateFinalRequiredReturn(annualizedTrades);

  REQUIRE(rf_hurdle == Num("0.08"));
  REQUIRE(cost_req  == Num("0.03"));
  REQUIRE(final_req == rf_hurdle); // max(0.08, 0.03) == 0.08
}

TEST_CASE("MetaTradingHurdleCalculator: Costs dominate when activity is higher", "[MetaHurdle]")
{
  RiskParameters rp;
  rp.riskFreeRate = Num("0.03");
  rp.riskPremium  = Num("0.05"); // 8%

  // per-side 0.10%, buffer 1.5
  MetaTradingHurdleCalculator calc(rp, Num("1.5"), Num("0.001"));

  // Higher trading → cost hurdle above 8%:
  // 2*0.001 = 0.002; trades=50 → 0.002*50 = 0.10; *1.5 = 0.15 (15%)
  const Num annualizedTrades = Num("50");
  const Num final_req = calc.calculateFinalRequiredReturn(annualizedTrades);

  REQUIRE(final_req == Num("0.15"));
  REQUIRE(final_req > Num("0.08"));
}

TEST_CASE("MetaTradingHurdleCalculator: Per-side override path", "[MetaHurdle]")
{
  RiskParameters rp;
  rp.riskFreeRate = Num("0.02");
  rp.riskPremium  = Num("0.04"); // 6%

  MetaTradingHurdleCalculator calc(rp, Num("1.5"), Num("0.0008")); // default per-side unused here

  const Num trades = Num("40");
  // Explicit per-side = 0.12% → round-trip 0.24%; *40 = 0.096; *1.5 = 0.144 (14.4%)
  const Num perSideOverride = Num("0.0012");
  const Num cost_req = calc.calculateFinalRequiredReturnWithPerSideSlippage(trades, perSideOverride);
  REQUIRE(cost_req == Num("0.144"));
  REQUIRE(cost_req > calc.calculateRiskFreeHurdle()); // > 6%
}
