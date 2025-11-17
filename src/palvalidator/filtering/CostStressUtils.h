#pragma once

#include <optional>
#include <algorithm>
#include <ostream>

#include "FilteringTypes.h"                 // OOSSpreadStatsT / OOSSpreadStats
#include "TradingHurdleCalculator.h"       // Simplified (individual strategies)
#include "MetaTradingHurdleCalculator.h"   // Legacy (metastrategies)
#include "TimeSeries.h"                    // DecimalConstants (if not already pulled)

/*
 * This header provides:
 *  - CostStressHurdlesT: container for per-side slippages used and resulting hurdles
 *  - printCostStressConcise / printCostStressVerbose: logging helpers
 *  - cappedMeanPlusKQn: helper to cap mean+k*Qn at 3*mean (and floor at 0)
 *  - makeCostStressHurdles overloads:
 *      (1) for TradingHurdleCalculator (individual strategies)
 *      (2) for meta::MetaTradingHurdleCalculator (metastrategies; legacy high hurdle)
 */

namespace palvalidator {
namespace filtering {

// ─────────────────────────────────────────────────────────────────────────────
// Container for stress hurdles + the per-side slippages used to compute them
// ─────────────────────────────────────────────────────────────────────────────
template <typename NumT>
struct CostStressHurdlesT {
  NumT baseHurdle;   // using perSideBase
  NumT h_1q;         // using mean + 1*Qn
  NumT h_2q;         // using mean + 2*Qn
  NumT h_3q;         // using mean + 3*Qn

  NumT perSideBase;  // per-side slippage used for base
  NumT perSide1q;
  NumT perSide2q;
  NumT perSide3q;
};

// ─────────────────────────────────────────────────────────────────────────────
// Concise printer
// Decision line is expressed as: LB > Base AND > +1·Qn
// (Callers can optionally pass a risk-free hurdle to show which driver dominates.)
// ─────────────────────────────────────────────────────────────────────────────
template <typename NumT>
inline void printCostStressConcise(std::ostream& os,
                                   const CostStressHurdlesT<NumT>& H,
                                   const NumT& lbAnn,                       // annualized LB
                                   const char* context,                     // "Strategy" | "Meta" | "Slices"
                                   std::optional<OOSSpreadStatsT<NumT>> oosStatsOpt = std::nullopt,
                                   bool showExtraHurdles = false,
                                   std::optional<NumT> riskFreeHurdle = std::nullopt)
{
  using mkc_timeseries::DecimalConstants;
  const auto pct = [](const NumT& v) -> NumT {
    return v * DecimalConstants<NumT>::DecimalOneHundred;
  };

  os << "      [CostHurdle/" << (context ? context : "Run") << "]\n";

  if (oosStatsOpt.has_value()) {
    os << "         OOS spreads: mean=" << pct(oosStatsOpt->mean) << "%, "
       << "Qn=" << pct(oosStatsOpt->qn) << "%\n";
  }

  os << "         Hurdles (%): Base=" << pct(H.baseHurdle) << "%, "
     << "+1·Qn=" << pct(H.h_1q) << "%";
  if (showExtraHurdles) {
    os << ", +2·Qn=" << pct(H.h_2q) << "%, "
       << "+3·Qn=" << pct(H.h_3q) << "%";
  }
  os << "\n";

  os << "         LB (%):     " << pct(lbAnn) << "%\n"
     << "         Gate:       "
     << ((lbAnn > H.baseHurdle && lbAnn > H.h_1q)
         ? "PASS (LB > Base and > +1·Qn)"
         : "FAIL (LB ≤ Base or ≤ +1·Qn)") << "\n";

  if (riskFreeHurdle.has_value()) {
    const bool rfDominates = (*riskFreeHurdle >= H.baseHurdle);
    os << "         Driver:     "
       << (rfDominates ? "Risk-free (RF dominates)" : "Costs (cost-based dominates)")
       << "  [RF=" << pct(*riskFreeHurdle) << "%]\n";
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Verbose printer
// ─────────────────────────────────────────────────────────────────────────────
template <typename NumT>
inline void printCostStressVerbose(std::ostream& os,
                                   const CostStressHurdlesT<NumT>& H,
                                   const NumT& lbAnn,                  // annualized LB used for gating
                                   const char*  context,               // e.g. "Strategy" or "Slices"
                                   std::optional<NumT> configuredPerSide = std::nullopt,
                                   std::optional<OOSSpreadStatsT<NumT>> oosStatsOpt = std::nullopt)
{
  using mkc_timeseries::DecimalConstants;
  const auto pct = [](const NumT& v) -> NumT {
    return v * DecimalConstants<NumT>::DecimalOneHundred;
  };

  os << "      [CostStress/" << (context ? context : "Run") << "]\n"
     << "         Inputs (per-side slippage, %):\n";

  if (configuredPerSide.has_value()) {
    os << "           • Configured:       " << pct(*configuredPerSide) << "%\n";
  } else {
    os << "           • Configured:       (n/a)\n";
  }

  if (oosStatsOpt.has_value()) {
    os << "         OOS spreads (%, proportional):\n"
       << "           • mean:          " << pct(oosStatsOpt->mean) << "%\n"
       << "           • Qn (robust):   " << pct(oosStatsOpt->qn)   << "%\n";
  }

  os << "           • Calibrated Base: " << pct(H.perSideBase) << "%  (max(configured, mean/2))\n"
     << "           • +1·Qn Stress:    " << pct(H.perSide1q)   << "%  ( (mean+Qn)/2 )\n"
     << "           • +2·Qn Stress:    " << pct(H.perSide2q)   << "%  ( (mean+2·Qn)/2 )\n"
     << "           • +3·Qn Stress:    " << pct(H.perSide3q)   << "%  ( (mean+3·Qn)/2 )\n"
     << "         Hurdles (final required return, %):\n"
     << "           • Base:            " << pct(H.baseHurdle) << "%\n"
     << "           • +1·Qn:          " << pct(H.h_1q)       << "%\n"
     << "           • +2·Qn:          " << pct(H.h_2q)       << "%\n"
     << "           • +3·Qn:          " << pct(H.h_3q)       << "%\n"
     << "         Result:\n"
     << "           • Annualized LB:   " << pct(lbAnn) << "%\n"
     << "           • Decision:        "
     << ((lbAnn > H.baseHurdle && lbAnn > H.h_1q) ? "PASS (LB > Base and > +1·Qn)"
                                                 : "FAIL (LB ≤ Base or ≤ +1·Qn)")
     << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: cap (mean + k*Qn) at 3*mean, floor at 0
// ─────────────────────────────────────────────────────────────────────────────
template <typename NumT>
inline NumT cappedMeanPlusKQn(const NumT& mean, const NumT& qn, double k)
{
  const NumT cap = NumT("3.0") * mean;
  NumT v = mean + NumT(k) * qn;
  if (v > cap) v = cap;
  if (v < NumT(0)) v = NumT(0);
  return v;
}

// ============================================================================
// Overload #1: INDIVIDUALS (simplified TradingHurdleCalculator)
// Uses ONLY calculateTradingSpreadCost(...). No RF/premium or buffers.
// ============================================================================

template <typename NumT>
inline CostStressHurdlesT<NumT>
makeCostStressHurdles(const TradingHurdleCalculator& calc,
                      const std::optional<OOSSpreadStatsT<NumT>>& statsOpt,
                      const NumT& annualizedTrades,
                      std::optional<NumT> configuredPerSide = std::nullopt)
{
  using mkc_timeseries::DecimalConstants;

  CostStressHurdlesT<NumT> R{};

  // Base hurdle: simplified calculator may internally use default per-side and/or OOS mean.
  R.baseHurdle = calc.calculateTradingSpreadCost(annualizedTrades, statsOpt);

  if (!statsOpt.has_value()) {
    // No OOS stats → stressed hurdles degenerate to base (diagnostic only)
    R.h_1q = R.h_2q = R.h_3q = R.baseHurdle;

    // We may still wish to display what "configured per-side" was (if any)
    R.perSideBase = configuredPerSide.value_or(NumT(0));
    R.perSide1q = R.perSide2q = R.perSide3q = R.perSideBase;
    return R;
  }

  // With stats: define stressed per-side slippages off (mean + k*Qn)/2
  const auto& S = *statsOpt;

  const NumT perSideFromMean = S.mean / DecimalConstants<NumT>::DecimalTwo; // mean/2
  const NumT perSideBase = configuredPerSide.has_value()
                         ? std::max(*configuredPerSide, perSideFromMean)
                         : perSideFromMean;

  const NumT perSide1q = cappedMeanPlusKQn(S.mean, S.qn, 1.0) / DecimalConstants<NumT>::DecimalTwo;
  const NumT perSide2q = cappedMeanPlusKQn(S.mean, S.qn, 2.0) / DecimalConstants<NumT>::DecimalTwo;
  const NumT perSide3q = cappedMeanPlusKQn(S.mean, S.qn, 3.0) / DecimalConstants<NumT>::DecimalTwo;

  // Convert per-side → round-trip and multiply by trades/year to get stressed hurdles
  const NumT two = DecimalConstants<NumT>::DecimalTwo;
  const auto hurdleFromPerSide = [&](const NumT& perSide) {
    return annualizedTrades * (two * perSide);
  };

  R.perSideBase = perSideBase;
  R.perSide1q   = perSide1q;
  R.perSide2q   = perSide2q;
  R.perSide3q   = perSide3q;

  R.h_1q = hurdleFromPerSide(R.perSide1q);
  R.h_2q = hurdleFromPerSide(R.perSide2q);
  R.h_3q = hurdleFromPerSide(R.perSide3q);

  return R;
}

// ============================================================================
// Overload #2: METAS (legacy MetaTradingHurdleCalculator)
// Restores legacy higher bar: max(RF+premium, buffered costs), and supports
// per-side slippage & Qn stresses like before.
// ============================================================================

template <typename NumT>
inline CostStressHurdlesT<NumT>
makeCostStressHurdles(const meta::MetaTradingHurdleCalculator& calc,
                      const std::optional<OOSSpreadStatsT<NumT>>& statsOpt,
                      const NumT& annualizedTrades,
                      std::optional<NumT> configuredPerSide = std::nullopt)
{
  using mkc_timeseries::DecimalConstants;

  CostStressHurdlesT<NumT> R{};

  if (!statsOpt.has_value()) {
    // Legacy: final = max(RF+premium, buffered costs with calculator's default per-side)
    R.baseHurdle = calc.calculateFinalRequiredReturn(annualizedTrades);
    R.h_1q = R.h_2q = R.h_3q = R.baseHurdle;

    R.perSideBase = configuredPerSide.value_or(NumT(0));
    R.perSide1q = R.perSide2q = R.perSide3q = R.perSideBase;
    return R;
  }

  // With stats → legacy calibration from mean/Qn with configured override for base.
  const auto& S = *statsOpt;

  const NumT perSideFromMean = S.mean / DecimalConstants<NumT>::DecimalTwo; // mean/2
  const NumT perSideBase = configuredPerSide.has_value()
                         ? std::max(*configuredPerSide, perSideFromMean)
                         : perSideFromMean;

  const NumT perSide1q = cappedMeanPlusKQn(S.mean, S.qn, 1.0) / DecimalConstants<NumT>::DecimalTwo;
  const NumT perSide2q = cappedMeanPlusKQn(S.mean, S.qn, 2.0) / DecimalConstants<NumT>::DecimalTwo;
  const NumT perSide3q = cappedMeanPlusKQn(S.mean, S.qn, 3.0) / DecimalConstants<NumT>::DecimalTwo;

  R.perSideBase = perSideBase;
  R.perSide1q   = perSide1q;
  R.perSide2q   = perSide2q;
  R.perSide3q   = perSide3q;

  // Legacy final required return using explicit per-side slippage
  R.baseHurdle = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSideBase);
  R.h_1q       = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSide1q);
  R.h_2q       = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSide2q);
  R.h_3q       = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSide3q);

  return R;
}

} // namespace filtering
} // namespace palvalidator
