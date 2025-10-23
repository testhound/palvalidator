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
  }

  bool BacktestingStage::validateReturnCount(const StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    const auto n = ctx.highResReturns.size();
    if (n < MIN_RETURNS_FOR_BOOTSTRAP)
    {
      os << "✗ Strategy filtered out: " << (ctx.strategy ? ctx.strategy->getStrategyName() : std::string("<unknown>"))
         << " - Insufficient returns for bootstrap (" << n << " < " << MIN_RETURNS_FOR_BOOTSTRAP << ").\n";
      return false;
    }
    
    // Validate minimum trades for bootstrap analysis
    if (ctx.backtester)
    {
      const auto numTrades = ctx.backtester->getNumTrades();
      if (numTrades < MIN_TRADES_FOR_BOOTSTRAP)
      {
        os << "✗ Strategy filtered out: " << (ctx.strategy ? ctx.strategy->getStrategyName() : std::string("<unknown>"))
           << " - Insufficient trades for bootstrap (" << numTrades << " < " << MIN_TRADES_FOR_BOOTSTRAP << ").\n";
        return false;
      }
    }
    
    return true;
  }

  FilterDecision BacktestingStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    try
    {
      runBacktest(ctx);

      if (!validateReturnCount(ctx, os))
      {
        std::ostringstream ss;
        const auto numReturns = ctx.highResReturns.size();
        const auto numTrades = ctx.backtester ? ctx.backtester->getNumTrades() : 0;
        
        if (numReturns < MIN_RETURNS_FOR_BOOTSTRAP)
        {
          ss << "Insufficient returns (" << numReturns << " < " << MIN_RETURNS_FOR_BOOTSTRAP << ")";
        }
        else if (numTrades < MIN_TRADES_FOR_BOOTSTRAP)
        {
          ss << "Insufficient trades (" << numTrades << " < " << MIN_TRADES_FOR_BOOTSTRAP << ")";
        }
        else
        {
          ss << "Insufficient data for bootstrap analysis";
        }
        
        return FilterDecision::Fail(FilterDecisionType::FailInsufficientData, ss.str());
      }

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