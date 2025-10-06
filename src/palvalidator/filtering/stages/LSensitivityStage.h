#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/PerformanceFilter.h" // for LSensitivityConfig
#include <ostream>
#include <vector>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  struct LSensitivityResultSimple
  {
    bool ran{false};
    bool pass{false};
    size_t numTested{0};
    size_t numPassed{0};
    size_t L_at_min{0};
    Num minLbAnn{Num(0)};
    double relVar{0.0};
  };

  class LSensitivityStage
  {
  public:
    // Use the LSensitivityConfig nested in PerformanceFilter and pass bootstrap params
    explicit LSensitivityStage(const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& cfg,
                               unsigned int numResamples,
                               const Num& confidenceLevel);

    /**
     * Run L-grid sensitivity check using the logic adapted from PerformanceFilter::runLSensitivity.
     * Returns a simple result struct summarizing outcomes.
     * @param L_cap Cap for maximum block length (computed from maxHold + buffer if enabled)
     */
    LSensitivityResultSimple execute(const StrategyAnalysisContext& ctx,
                                     size_t L_cap,
                                     double annualizationFactor,
                                     const Num& finalRequiredReturn,
                                     std::ostream& os) const;

  private:
    const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& mCfg;
    unsigned int mNumResamples;
    Num mConfidenceLevel;
  };

} // namespace palvalidator::filtering::stages