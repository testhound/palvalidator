#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include "number.h"
#include "BackTester.h"
#include "Security.h"
#include "DateRange.h"
#include "PalStrategy.h"
#include "TimeFrame.h"
#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"
#include "analysis/RobustnessAnalyzer.h"
#include "analysis/DivergenceAnalyzer.h"
#include "analysis/FragileEdgeAnalyzer.h"
#include "RegimeMixStress.h"

namespace palvalidator
{
  namespace filtering
  {
    // Forward declaration
    class FilteringPipeline;

    using namespace mkc_timeseries;
    using Num = num::DefaultNumber;

    /**
     * @brief Performance-based filtering of trading strategies using BCa bootstrap analysis
     * 
     * This class implements comprehensive performance filtering that includes:
     * - Statistical viability checks (annualized lower bound > 0)
     * - Economic significance tests (returns exceed cost hurdles)
     * - Risk-adjusted return requirements (returns exceed risk-free rate + premium)
     * - Robustness analysis for flagged strategies
     * - Fragile edge advisory analysis
     */
    class PerformanceFilter
    {
    public:
      struct LSensitivityConfig
      {
	// Grid of mean block lengths to test. If empty, a default, cap-aware grid is built
	// for short-horizon strategies: {2,3,4,5,6,8,10, Lcenter, 2*Lcenter},
	//clipped to [2, min(L_cap, n-1)].

	std::vector<size_t> Lgrid;

	// Minimum fraction of grid points for which the annualized GM lower bound must
	// exceed the final required return (economic hurdle) to pass the stress.
	// Example: 0.8 means: at least 80% of tested L values must pass.

	double minPassFraction = 0.8;
	// If true, also enforce that the *minimum* annualized GM lower bound over the grid
	// must not be catastrophically below the hurdle by more than this absolute margin.
	// Set to 0 to disable. Interpreted in *return units* (e.g., 0.0025 = 0.25%).

	double minGapTolerance = 0.0;
	// Master switch
	bool enabled = true;
	size_t maxL = 10;        // hard upper cap for block length search
	size_t capBuffer = 2;    // add a small buffer to (observed) max holding period
	bool   capByMaxHold = true; // if true, cap = min(maxL, maxHold + capBuffer)
      };

      void setLSensitivityConfig(const LSensitivityConfig& cfg) { mLSensitivity = cfg; }
      /**
       * @brief Constructor with risk parameters and bootstrap configuration
       * @param riskParams Risk parameters including risk-free rate and premium
       * @param confidenceLevel Confidence level for BCa bootstrap analysis (e.g., 0.95)
       * @param numResamples Number of bootstrap resamples (e.g., 2000)
       */
      PerformanceFilter(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples);

      /**
       * @brief Filter strategies based on BCa bootstrap performance analysis
       * @param survivingStrategies Vector of strategies that survived Monte Carlo validation
       * @param baseSecurity Security to test strategies against
       * @param backtestingDates Date range for backtesting
       * @param timeFrame Time frame for analysis
       * @param outputStream Output stream for logging (typically a TeeStream)
       * @return Vector of strategies that passed performance filtering
       */
      std::vector<std::shared_ptr<PalStrategy<Num>>> filterByPerformance(
									 const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
									 std::shared_ptr<Security<Num>> baseSecurity,
									 const DateRange& inSampleBacktestingDates,
									 const DateRange& oosBacktestingDates,
									 TimeFrame::Duration timeFrame,
									 std::ostream& outputStream,
									 std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats = std::nullopt
									 );

      /**
       * @brief Get the filtering summary from the last run
       * @return Summary statistics of the filtering process
       */
      const FilteringSummary& getFilteringSummary() const
      {
        return mFilteringSummary;
      }

    private:

      /**
       * @brief Count survivors by direction (Long/Short)
       * @param filteredStrategies Vector of filtered strategies
       * @return Pair of (Long count, Short count)
       */
      std::pair<size_t, size_t> countSurvivorsByDirection(
      	  const std::vector<std::shared_ptr<PalStrategy<Num>>>& filteredStrategies
      	  ) const;

      /**
       * @brief Create a FilteringPipeline configured with current settings
       * @return Unique pointer to configured pipeline
       */
      std::unique_ptr<FilteringPipeline> createPipeline();

      /**
       * @brief Update filtering summary based on filter decision type
       * @param decision Filter decision from pipeline
       *
       * Maps FilterDecisionType to appropriate summary increment calls.
       * Note: Some stages (RobustnessStage, LSensitivityStage) update
       * summary directly, so not all decision types require action here.
       */
      void updateSummaryForDecision(const FilterDecision& decision);

    private:
      TradingHurdleCalculator mHurdleCalculator;     ///< Calculator for trading hurdles
      Num mConfidenceLevel;                          ///< Confidence level for BCa bootstrap
      unsigned int mNumResamples;                    ///< Number of bootstrap resamples
      RobustnessChecksConfig mRobustnessConfig;      ///< Configuration for robustness checks
      FragileEdgePolicy mFragileEdgePolicy;          ///< Policy for fragile edge analysis
      FilteringSummary mFilteringSummary;            ///< Summary of filtering results
      bool mApplyFragileAdvice;                      ///< Whether to apply fragile edge advice
      LSensitivityConfig mLSensitivity;
    };

  } // namespace filtering
} // namespace palvalidator
