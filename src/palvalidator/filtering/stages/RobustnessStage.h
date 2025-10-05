#pragma once

#include "filtering/FilteringTypes.h"
#include <ostream>

namespace palvalidator::analysis {
  // Forward-declare types used by the robustness stage to avoid heavy includes in the header.
  template <typename NumT> struct DivergenceResult;
  template <typename NumT> struct RobustnessResults;
  enum class RobustnessVerdict : int;
  enum class RobustnessFailReason : int;
}

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  /**
   * @brief Stage responsible for running robustness checks (split-sample, tail-risk, L-sensitivity)
   *
   * The stage delegates to palvalidator::analysis::RobustnessAnalyzer (implementation lives
   * elsewhere) and updates the provided FilteringSummary counters when a strategy fails.
   *
   * Construction takes a robustness configuration (thresholds) and a reference to the
   * global FilteringSummary used by the pipeline to aggregate results.
   */
  class RobustnessStage
  {
  public:
    RobustnessStage(const RobustnessChecksConfig& cfg, FilteringSummary& summary)
      : mCfg(cfg), mSummary(summary)
    {}

    /**
     * Execute robustness checks for the provided strategy context.
     *
     * Parameters:
     *  - ctx: analysis context (contains highResReturns, backtester, etc.)
     *  - divergence: AM/GM divergence diagnostic computed earlier
     *  - nearHurdle: whether the annualized LB is within the borderline margin
     *  - smallN: whether the sample size is considered small for robustness checks
     *  - os: output stream for logging (preserves original logging format)
     *
     * Returns:
     *  - FilterDecision::Pass() if robustness checks pass or are not required
     *  - FilterDecision::Fail(...) with appropriate type and rationale on failure
     */
    FilterDecision execute(StrategyAnalysisContext& ctx,
                           const palvalidator::analysis::DivergenceResult<Num>& divergence,
                           bool nearHurdle,
                           bool smallN,
                           std::ostream& os) const;

  private:
    const RobustnessChecksConfig& mCfg;
    FilteringSummary& mSummary;
  };

} // namespace palvalidator::filtering::stages