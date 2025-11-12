#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/PerformanceFilter.h" // for LSensitivityConfig
#include "filtering/BootstrapConfig.h"
#include <ostream>
#include <vector>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using palvalidator::bootstrap_cfg::BootstrapFactory;

  // Note: LSensitivityResultSimple is now defined in FilteringTypes.h
  // to avoid circular dependencies and allow reuse by RobustnessAnalyzer

  class LSensitivityStage
  {
  public:
    // Use the LSensitivityConfig nested in PerformanceFilter and pass bootstrap params
    explicit LSensitivityStage(const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& cfg,
                               unsigned int numResamples,
                               const Num& confidenceLevel,
                               BootstrapFactory& bootstrapFactory);

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
    // Helper classes for organizing results
    class LGridPoint
    {
    public:
      LGridPoint(size_t blockLength, const Num& annualizedLowerBound)
        : mBlockLength(blockLength)
        , mAnnualizedLowerBound(annualizedLowerBound)
      {}

      size_t getBlockLength() const { return mBlockLength; }
      const Num& getAnnualizedLowerBound() const { return mAnnualizedLowerBound; }

    private:
      size_t mBlockLength;
      Num mAnnualizedLowerBound;
    };

    /**
     * Build the L-grid for sensitivity testing
     */
    std::vector<size_t> buildLGrid(const StrategyAnalysisContext& ctx,
                                    size_t hardCap,
                                    std::ostream& os) const;

    /**
     * Run bootstrap for a single L value using small-N conservative method
     */
    Num runSmallNBootstrapForL(const StrategyAnalysisContext& ctx,
                                size_t blockLength,
                                double annualizationFactor,
                                bool heavyTails,
                                std::ostream& os) const;

    /**
     * Run bootstrap for a single L value using standard BCa method
     */
    Num runStandardBootstrapForL(const StrategyAnalysisContext& ctx,
                                  size_t blockLength,
                                  double annualizationFactor,
                                  std::ostream& os) const;

    /**
     * Compute statistics across all L-grid results
     */
    void computeGridStatistics(const std::vector<LGridPoint>& gridResults,
                                LSensitivityResultSimple& result) const;

    /**
     * Determine pass/fail based on grid statistics and thresholds
     */
    bool evaluatePassCriteria(const LSensitivityResultSimple& result,
                               const Num& finalRequiredReturn,
                               size_t gridSize) const;

    /**
     * Log summary of L-grid results
     */
    void logGridSummary(const std::vector<size_t>& grid,
                        const std::vector<LGridPoint>& gridResults,
                        const LSensitivityResultSimple& result,
                        const Num& finalRequiredReturn,
                        std::ostream& os) const;

    const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& mCfg;
    unsigned int mNumResamples;
    Num mConfidenceLevel;
    BootstrapFactory& mBootstrapFactory;
  };

} // namespace palvalidator::filtering::stages