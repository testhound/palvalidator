#pragma once

#include "filtering/FilteringTypes.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  /**
   * @brief Stage for regime-mix stress testing
   *
   * This stage tests strategy robustness across different market regime mixes
   * by resampling returns weighted by regime labels. It executes the logic
   * from PerformanceFilter::applyRegimeMixGate() and runRegimeMixStressWithLabels().
   *
   * Gate Logic (lines 233-249 in original):
   * - Calls applyRegimeMixGate() which builds regime labels and runs stress
   * - FAIL if fewer than mixPassFrac of mixes pass the hurdle
   * - Updates: incrementFailRegimeMixCount() on failure
   */
  class RegimeMixStage
  {
  public:
    RegimeMixStage(const Num& confidenceLevel, unsigned int numResamples)
      : mConfidenceLevel(confidenceLevel)
      , mNumResamples(numResamples)
    {}

    /**
     * @brief Execute regime-mix stress analysis
     * @param ctx Strategy analysis context (contains baseSecurity, backtester, etc.)
     * @param bootstrap Bootstrap analysis results (for L and annualization factor)
     * @param hurdle Hurdle analysis results (for finalRequiredReturn)
     * @param os Output stream for logging
     * @return FilterDecision indicating pass/fail
     *
     * This method delegates to the existing helper methods in PerformanceFilter
     * to preserve exact behavior.
     */
    FilterDecision execute(
      const StrategyAnalysisContext& ctx,
      const BootstrapAnalysisResult& bootstrap,
      const HurdleAnalysisResult& hurdle,
      std::ostream& os) const;

  private:
    Num mConfidenceLevel;
    unsigned int mNumResamples;
  };

} // namespace palvalidator::filtering::stages