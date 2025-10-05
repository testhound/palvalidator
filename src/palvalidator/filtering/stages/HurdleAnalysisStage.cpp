#include "filtering/stages/HurdleAnalysisStage.h"
#include "filtering/CostStressUtils.h"
#include "filtering/FilteringTypes.h"
#include "BackTester.h"
#include <sstream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  HurdleAnalysisStage::HurdleAnalysisStage(const TradingHurdleCalculator& calc)
    : mHurdleCalculator(calc)
  {}

  HurdleAnalysisResult HurdleAnalysisStage::execute(const StrategyAnalysisContext& ctx,
                                                     const BootstrapAnalysisResult& bootstrap,
                                                     std::ostream& os) const
  {
    HurdleAnalysisResult R;

    // Determine annualized trades (defensive: backtester may be null)
    Num annualizedTrades = Num(0);
    if (ctx.backtester)
    {
      annualizedTrades = Num(ctx.backtester->getEstimatedAnnualizedTrades());
    }
    R.annualizedTrades = annualizedTrades;

    // Obtain configured per-side slippage if present
    std::optional<Num> configuredPerSide = mHurdleCalculator.getSlippagePerSide();

    // Build cost-stressed hurdles (may use OOS spread stats when available)
    const auto H = palvalidator::filtering::makeCostStressHurdles<Num>(
      mHurdleCalculator,
      ctx.oosSpreadStats,
      annualizedTrades,
      configuredPerSide
    );

    // Print the concise cost-stress summary (preserve original formatting)
    palvalidator::filtering::printCostStressConcise<Num>(
      os,
      H,
      bootstrap.annualizedLowerBoundGeo,
      "Strategy",
      ctx.oosSpreadStats,
      false,
      mHurdleCalculator.calculateRiskFreeHurdle()
    );

    // Fill result
    R.finalRequiredReturn = H.baseHurdle;
    R.passedBase = (bootstrap.annualizedLowerBoundGeo > H.baseHurdle);
    R.passed1Qn  = (bootstrap.annualizedLowerBoundGeo > H.h_1q);

    return R;
  }

} // namespace palvalidator::filtering::stages