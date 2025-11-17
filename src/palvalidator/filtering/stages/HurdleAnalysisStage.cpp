#include "filtering/stages/HurdleAnalysisStage.h"
#include "filtering/FilteringTypes.h"
#include "BackTester.h"
#include <sstream>
#include <iomanip>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::DecimalConstants;

  HurdleAnalysisStage::HurdleAnalysisStage(const TradingHurdleCalculator& calc)
    : mHurdleCalculator(calc)
  {}

  HurdleAnalysisResult
  HurdleAnalysisStage::execute(const StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    HurdleAnalysisResult R;

    // ── 1) Determine annualized trades λ (trades/year) ────────────────────────
    // Prefer the backtester’s estimate; do NOT assume ctx.annualizationFactor
    // is λ because the bootstrap now publishes bars/year there.
    double lambdaTradesPerYear = 0.0;
    try {
      if (ctx.backtester) {
	lambdaTradesPerYear = ctx.backtester->getEstimatedAnnualizedTrades();
      }
    } catch (...) {
      lambdaTradesPerYear = 0.0; // handled below
    }

    // Publish λ to the result struct
    Num annualizedTrades = Num(lambdaTradesPerYear);
    R.annualizedTrades = annualizedTrades;

    // ── 2) Compute the trading spread cost hurdle ─────────────────────────────
    // Uses either configured per-side slippage or OOS mean round-trip,
    // multiplied by λ (trades/year). Implementation is in TradingHurdleCalculator.
    R.finalRequiredReturn = mHurdleCalculator.calculateTradingSpreadCost(
									 annualizedTrades, ctx.oosSpreadStats);  // λ × roundTrip
    // (see TradingHurdleCalculator::calculateTradingSpreadCost) :contentReference[oaicite:1]{index=1}

    // ── 3) Derive per-side used (for logging only) ────────────────────────────
    // hurdle = λ * (2 * perSide)  ⇒  perSide = hurdle / (2 * λ)
    const Num two = mkc_timeseries::DecimalConstants<Num>::DecimalTwo;
    Num perSideUsed = mkc_timeseries::DecimalConstants<Num>::DecimalZero;

    if (annualizedTrades > mkc_timeseries::DecimalConstants<Num>::DecimalZero) {
      perSideUsed = R.finalRequiredReturn / (two * annualizedTrades);
    }
    else if (ctx.oosSpreadStats) {
      // If λ is unavailable, fall back to OOS mean (round-trip)/2 for log clarity
      perSideUsed = ctx.oosSpreadStats->mean / two;
    }

    const Num roundTrip = two * perSideUsed;

    // ── 4) Log summary (clarify that 'annualizedTrades' is λ) ─────────────────
    os << std::fixed << std::setprecision(8)
       << "   [HurdleAnalysis] Components: "
       << "tradesPerYear(λ)=" << annualizedTrades
       << " perSide=" << perSideUsed
       << " roundTrip=" << roundTrip
       << " hurdle=" << R.finalRequiredReturn
       << " (" << (R.finalRequiredReturn * 100).getAsDouble() << "%)\n";

    // This stage only computes the hurdle; pass/fail happens later in the pipeline.
    return R;
  }
} // namespace palvalidator::filtering::stages
