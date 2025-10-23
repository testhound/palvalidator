#pragma once

#include "filtering/FilteringTypes.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  class BacktestingStage
  {
  public:
    /**
     * Execute backtest for the provided context and validate returns.
     * Writes output to the provided stream using the same messages as the original code.
     */
    FilterDecision execute(StrategyAnalysisContext& ctx, std::ostream& os) const;

  private:
    static constexpr size_t MIN_RETURNS_FOR_BOOTSTRAP = 20;
    static constexpr size_t MIN_TRADES_FOR_BOOTSTRAP = 5;

    void createFreshPortfolio(StrategyAnalysisContext& ctx) const;
    void runBacktest(StrategyAnalysisContext& ctx) const;
    bool validateReturnCount(const StrategyAnalysisContext& ctx, std::ostream& os) const;
  };

} // namespace palvalidator::filtering::stages
