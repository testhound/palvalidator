#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  /**
   * @brief Calculate cost- and risk-based hurdles and evaluate pass/fail
   *
   * This stage wraps the existing TradingHurdleCalculator usage and the
   * cost-stress helpers (makeCostStressHurdles / printCostStressConcise) from
   * CostStressUtils.h. It returns a HurdleAnalysisResult describing the outcome.
   */
  class HurdleAnalysisStage
  {
  public:
    explicit HurdleAnalysisStage(const TradingHurdleCalculator& calc);

    /**
     * Execute hurdle calculation for the provided context and bootstrap results.
     * Writes the same concise cost-stress output as the original implementation.
     */
    HurdleAnalysisResult execute(const StrategyAnalysisContext& ctx,
                                 const BootstrapAnalysisResult& bootstrap,
                                 std::ostream& os) const;

  private:
    const TradingHurdleCalculator& mHurdleCalculator;
  };

} // namespace palvalidator::filtering::stages