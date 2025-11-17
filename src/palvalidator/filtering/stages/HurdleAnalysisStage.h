#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  /**
   * @brief Calculates the trading spread cost, which serves as the new hurdle.
   *
   * This stage uses the simplified `TradingHurdleCalculator` to determine the
   * total annualized trading spread cost. This cost is then used by the
   * `ValidationPolicy` to make a pass/fail decision.
   */
  class HurdleAnalysisStage
  {
  public:
    explicit HurdleAnalysisStage(const TradingHurdleCalculator& calc);

    /**
     * @brief Executes the hurdle calculation for the provided context.
     *
     * @param ctx The strategy analysis context.
     * @param bootstrap The bootstrap analysis result.
     * @param os The output stream for logging.
     * @return A `HurdleAnalysisResult` containing the calculated cost.
     */
    HurdleAnalysisResult execute(const StrategyAnalysisContext& ctx,
                                 std::ostream& os) const;

  private:
    const TradingHurdleCalculator& mHurdleCalculator;
  };

} // namespace palvalidator::filtering::stages
