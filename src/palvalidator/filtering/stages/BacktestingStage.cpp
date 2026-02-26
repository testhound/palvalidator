#include "filtering/stages/BacktestingStage.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include <sstream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::BackTester;
  using mkc_timeseries::Portfolio;
  using mkc_timeseries::Security;
  using mkc_timeseries::PalStrategy;

  void BacktestingStage::createFreshPortfolio(StrategyAnalysisContext& ctx) const
  {
    // Create a fresh portfolio for isolated backtesting
    const std::string name = ctx.strategy ? ctx.strategy->getStrategyName() + " Portfolio" : "Strategy Portfolio";
    ctx.portfolio = std::make_shared<Portfolio<Num>>(name);
    if (ctx.baseSecurity)
    {
      ctx.portfolio->addSecurity(ctx.baseSecurity);
    }
  }

  void BacktestingStage::runBacktest(StrategyAnalysisContext& ctx) const
  {
    if (!ctx.strategy)
      throw std::invalid_argument("BacktestingStage::runBacktest - null strategy");

    createFreshPortfolio(ctx);
    ctx.clonedStrategy = ctx.strategy->clone2(ctx.portfolio);
    ctx.backtester = mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
    ctx.highResReturns = ctx.backtester->getAllHighResReturns(ctx.clonedStrategy.get());

    // Apply slippage to trade returns for more accurate bootstrapping
    bool applyTradeCosts = true;

    // Exempt return related to limit orders from slippage
    bool exemptLimitOrders = true;

    ctx.tradeLevelReturns = ctx.backtester->getClosedTradeLevelReturns(ctx.clonedStrategy.get(),
								       applyTradeCosts,
								       mkc_timeseries::DecimalConstants<Num>::DefaultEquitySlippage,
								       exemptLimitOrders);
  }


  FilterDecision BacktestingStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    try
    {
      runBacktest(ctx);

      return FilterDecision::Pass();
    }
    catch (const std::exception& e)
    {
      os << "Warning: BacktestingStage failed for strategy '"
         << (ctx.strategy ? ctx.strategy->getStrategyName() : std::string("<unknown>"))
         << "': " << e.what() << "\n";
      return FilterDecision::Fail(FilterDecisionType::FailInsufficientData, std::string("Backtest error: ") + e.what());
    }
  }

} // namespace palvalidator::filtering::stages
