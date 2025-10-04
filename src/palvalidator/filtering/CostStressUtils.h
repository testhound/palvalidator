#pragma once

#include <optional>
#include <algorithm>
#include "FilteringTypes.h"          // OOSSpreadStatsT / OOSSpreadStats
#include "TradingHurdleCalculator.h" // TradingHurdleCalculator
#include "TimeSeries.h"              // DecimalConstants (if not already pulled by TradingHurdleCalculator)

namespace palvalidator
{
    namespace filtering
    {
        // Holds both the per-side slippages used and the resulting final hurdles
        // so call sites can log everything cleanly.
        template <typename NumT>
        struct CostStressHurdlesT
        {
            NumT baseHurdle;   // using perSideBase
            NumT h_1q;         // using mean + 1*Qn
            NumT h_2q;         // using mean + 2*Qn
            NumT h_3q;         // using mean + 3*Qn

            NumT perSideBase;  // per-side slippage used for base
            NumT perSide1q;
            NumT perSide2q;
            NumT perSide3q;
        };

	// -----------------------------------------------------------------------------
	// Concise cost-stress printer: shows OOS spreads and the cost hurdles only.
	// Decision rule is shown explicitly (LB > Base AND > +1·Qn).
	// -----------------------------------------------------------------------------
	template <typename NumT>
	  inline void printCostStressConcise(std::ostream& os,
					     const CostStressHurdlesT<NumT>& H,
					     const NumT& lbAnn,                                  // annualized LB
					     const char* context,                                 // "Strategy" | "Meta" | "Slices"
					     std::optional<OOSSpreadStatsT<NumT>> oosStatsOpt = std::nullopt,
					     bool showExtraHurdles = false)                       // set true to also show +2·Qn/+3·Qn
	  {
	    using mkc_timeseries::DecimalConstants;

	    const auto pct = [](const NumT& v) -> NumT
	      {
		return v * DecimalConstants<NumT>::DecimalOneHundred;
	      };

	    os << "      [CostHurdle/" << (context ? context : "Run") << "]\n";

	    if (oosStatsOpt.has_value())
	      {
		os << "         OOS spreads: mean=" << pct(oosStatsOpt->mean) << "%, "
		   << "Qn=" << pct(oosStatsOpt->qn) << "%\n";
	      }

	    os << "         Hurdles (%): Base=" << pct(H.baseHurdle) << "%, "
	       << "+1·Qn=" << pct(H.h_1q) << "%";

	    if (showExtraHurdles)
	      {
		os << ", +2·Qn=" << pct(H.h_2q) << "%, "
		   << "+3·Qn=" << pct(H.h_3q) << "%";
	      }
	    os << "\n";

	    os << "         LB (%):     " << pct(lbAnn) << "%\n"
	       << "         Gate:       "
	       << ( (lbAnn > H.baseHurdle && lbAnn > H.h_1q)
		    ? "PASS (LB > Base and > +1·Qn)"
		    : "FAIL (LB ≤ Base or ≤ +1·Qn)" )
	       << "\n";
	  }

	// Pretty printer for cost-stress context and decision
	template <typename NumT>
	  inline void printCostStressVerbose(std::ostream& os,
					     const CostStressHurdlesT<NumT>& H,
					     const NumT& lbAnn,                  // annualized LB used for gating
					     const char*  context,               // e.g. "Strategy" or "Slices"
					     std::optional<NumT> configuredPerSide = std::nullopt,
					     std::optional<OOSSpreadStatsT<NumT>> oosStatsOpt = std::nullopt)
	  {
	    using mkc_timeseries::DecimalConstants;

	    const auto pct = [](const NumT& v) -> NumT
	      {
		return v * DecimalConstants<NumT>::DecimalOneHundred;
	      };

	    os << "      [CostStress/" << (context ? context : "Run") << "]\n"
	       << "         Inputs (per-side slippage, %):\n";

	    if (configuredPerSide.has_value())
	      {
		os << "           • Configured:       " << pct(*configuredPerSide) << "%\n";
	      }
	    else
	      {
		os << "           • Configured:       (n/a)\n";
	      }

	    // If OOS spread stats are available, print them (proportions shown as %)
	    if (oosStatsOpt.has_value())
	      {
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
	       << ( (lbAnn > H.baseHurdle && lbAnn > H.h_1q) ? "PASS (LB > Base and > +1·Qn)"
		    : "FAIL (LB ≤ Base or ≤ +1·Qn)" )
	       << "\n";
	  }

        // Soft cap to curb pathological spikes: cap (mean + k*Qn) at 3*mean (and floor at 0)
        template <typename NumT>
        inline NumT cappedMeanPlusKQn(const NumT& mean, const NumT& qn, double k)
        {
            const NumT cap = NumT("3.0") * mean;
            NumT v = mean + NumT(k) * qn;
            if (v > cap) v = cap;
            if (v < NumT(0)) v = NumT(0);
            return v;
        }

        // Build calibrated baseline + Qn-stressed cost hurdles.
        //
        // Parameters:
        //   calc               : TradingHurdleCalculator to use for computations
        //   statsOpt           : optional OOS spread stats (mean, Qn) [proportional units]
        //   annualizedTrades   : trades per year as NumT
        //   configuredPerSide  : optional configured per-side slippage (if you can fetch it);
        //                        if std::nullopt, baseline will default to mean/2 when stats are present
        //
        // Policy:
        //   base per-side = max(configuredPerSide, mean/2)     [if stats present]
        //   stress per-side = (mean + k*Qn)/2, k in {1,2,3}    [capped at 3*mean]
        //
        // If statsOpt is std::nullopt:
        //   - baseHurdle uses calc.calculateFinalRequiredReturn(annualizedTrades)
        //   - perSideBase = configuredPerSide.value_or(NumT(0))
        //   - stressed hurdles equal base (diagnostic only)
        template <typename NumT>
        inline CostStressHurdlesT<NumT>
        makeCostStressHurdles(const TradingHurdleCalculator& calc,
                              const std::optional<OOSSpreadStatsT<NumT>>& statsOpt,
                              const NumT& annualizedTrades,
                              std::optional<NumT> configuredPerSide = std::nullopt)
        {
            using mkc_timeseries::DecimalConstants;

            CostStressHurdlesT<NumT> R{};

            if (!statsOpt.has_value())
            {
                R.baseHurdle = calc.calculateFinalRequiredReturn(annualizedTrades);
                R.h_1q = R.h_2q = R.h_3q = R.baseHurdle;

                // If you don’t have a getter for the configured per-side, pass it in via configuredPerSide.
                R.perSideBase = configuredPerSide.value_or(NumT(0));
                R.perSide1q = R.perSide2q = R.perSide3q = R.perSideBase;
                return R;
            }

            const auto& S = *statsOpt;

            const NumT half = DecimalConstants<NumT>::DecimalOne
                              / DecimalConstants<NumT>::DecimalTwo; // 0.5

            const NumT perSideFromMean = S.mean * half;

            NumT perSideBase = perSideFromMean;
            if (configuredPerSide.has_value())
            {
                perSideBase = std::max(*configuredPerSide, perSideFromMean);
            }

            const NumT s1 = cappedMeanPlusKQn(S.mean, S.qn, 1.0);
            const NumT s2 = cappedMeanPlusKQn(S.mean, S.qn, 2.0);
            const NumT s3 = cappedMeanPlusKQn(S.mean, S.qn, 3.0);

            const NumT perSide1q = s1 * half;
            const NumT perSide2q = s2 * half;
            const NumT perSide3q = s3 * half;

            R.perSideBase = perSideBase;
            R.perSide1q   = perSide1q;
            R.perSide2q   = perSide2q;
            R.perSide3q   = perSide3q;

            R.baseHurdle  = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSideBase);
            R.h_1q        = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSide1q);
            R.h_2q        = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSide2q);
            R.h_3q        = calc.calculateFinalRequiredReturnWithPerSideSlippage(annualizedTrades, perSide3q);

            return R;
        }
    }
}
