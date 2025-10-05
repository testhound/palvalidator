#pragma once

#include "filtering/FilteringTypes.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  /**
   * @brief Stage for fragile edge advisory analysis
   *
   * This stage analyzes strategy characteristics that may indicate fragile
   * performance (tail risk, variability, small margins). It can recommend:
   * - Keep: strategy is robust enough
   * - Downweight: reduce allocation but keep
   * - Drop: exclude from portfolio
   *
   * Gate Logic (lines 254-258 in original):
   * - Calls processFragileEdgeAnalysis() which:
   *   - Computes Q05, ES05 for tail risk
   *   - Runs analyzeFragileEdge() to get advisory
   *   - FAIL if advice.action == Drop AND mApplyFragileAdvice
   * - Outputs: "[ADVISORY]" messages with action and rationale
   * - No summary counter (advisory only)
   */
  class FragileEdgeStage
  {
  public:
    FragileEdgeStage(
      const FragileEdgePolicy& policy,
      bool applyAdvice)
      : mPolicy(policy)
      , mApplyAdvice(applyAdvice)
    {}

    /**
     * @brief Analyze fragile edge characteristics
     * @param ctx Strategy analysis context
     * @param bootstrap Bootstrap analysis results (for per-period LB)
     * @param hurdle Hurdle analysis results (for finalRequiredReturn)
     * @param lSensitivityRelVar Relative variance from L-sensitivity or robustness
     * @param os Output stream for logging
     * @return FilterDecision (may recommend drop/downweight)
     *
     * This method delegates to the existing analyzeFragileEdge() function
     * to preserve exact behavior.
     */
    FilterDecision execute(
      const StrategyAnalysisContext& ctx,
      const BootstrapAnalysisResult& bootstrap,
      const HurdleAnalysisResult& hurdle,
      double lSensitivityRelVar,
      std::ostream& os) const;

  private:
    const FragileEdgePolicy& mPolicy;
    bool mApplyAdvice;
  };

} // namespace palvalidator::filtering::stages