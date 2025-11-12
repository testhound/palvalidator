#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/BootstrapConfig.h"
#include "TradingBootstrapFactory.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  using palvalidator::bootstrap_cfg::BootstrapFactory;

  class BootstrapAnalysisStage
  {
  public:
    BootstrapAnalysisStage(const Num& confidenceLevel,
			   unsigned int numResamples,
			   BootstrapFactory& bootstrapFactory);

    /**
     * Compute BCa bootstrap bounds and annualize results.
     * Populates and returns BootstrapAnalysisResult.
     */
    BootstrapAnalysisResult execute(StrategyAnalysisContext& ctx, std::ostream& os) const;

    /**
     * Compute block length based on median holding period.
     * Made public for pipeline coordinator access.
     */
    size_t computeBlockLength(const StrategyAnalysisContext& ctx) const;

    /**
     * Compute annualization factor based on timeframe.
     * Made public for pipeline coordinator access.
     */
    double computeAnnualizationFactor(const StrategyAnalysisContext& ctx) const;

  private:
    Num mConfidenceLevel;
    unsigned int mNumResamples;
    BootstrapFactory& mBootstrapFactory;
  };

} // namespace palvalidator::filtering::stages
