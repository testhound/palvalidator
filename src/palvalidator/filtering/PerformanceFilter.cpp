#include "PerformanceFilter.h"
#include <array>
#include <algorithm>
#include <set>
#include <iomanip>
#include "BackTester.h"
#include "Portfolio.h"
#include "DecimalConstants.h"
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"
#include "RegimeLabeler.h"
#include "RegimeMixStress.h"
#include "RegimeMixStressRunner.h"
#include "BarAlignedSeries.h"
#include "CostStressUtils.h"
#include "filtering/FilteringPipeline.h"
#include <limits>

namespace palvalidator
{
  namespace filtering
  {

    using palvalidator::filtering::makeCostStressHurdles;

    constexpr std::size_t kRegimeVolWindow = 20;
    
    PerformanceFilter::PerformanceFilter(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples)
      : mHurdleCalculator(riskParams),
	mConfidenceLevel(confidenceLevel),
	mNumResamples(numResamples),
	mRobustnessConfig(),
	mFragileEdgePolicy(),
	mFilteringSummary(),
	mApplyFragileAdvice(true)
    {
    }

    std::vector<std::shared_ptr<PalStrategy<Num>>>
    PerformanceFilter::filterByPerformance(const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
					   std::shared_ptr<Security<Num>> baseSecurity,
					   const DateRange& inSampleBacktestingDates,
					   const DateRange& oosBacktestingDates,
					   TimeFrame::Duration timeFrame,
					   std::ostream& outputStream,
					   std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats)
    {
      std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;

      // Reset summary for new filtering run
      mFilteringSummary = FilteringSummary();

      outputStream << "\nFiltering " << survivingStrategies.size() << " surviving strategies by BCa performance...\n";
      outputStream << "Filter 1 (Statistical Viability): Annualized Lower Bound > 0\n";
      outputStream << "Filter 2 (Economic Significance): Annualized Lower Bound > (Annualized Cost Hurdle * "
		   << mHurdleCalculator.getCostBufferMultiplier() << ")\n";
      outputStream << "Filter 3 (Risk-Adjusted Return): Annualized Lower Bound > (Risk-Free Rate + Risk Premium ( "
		   << mHurdleCalculator.getRiskPremium() << ") )\n";
      outputStream << "  - Cost assumptions: $0 commission; slippage/spread per side uses configured floor"
		   << " and may be calibrated by OOS spreads when available.\n";
      outputStream << "  - Risk-Free Rate assumption: " << (mHurdleCalculator.getRiskFreeRate() * DecimalConstants<Num>::DecimalOneHundred) << "%.\n";

      for (const auto& strategy : survivingStrategies)
	{
	  try
	    {
	      // Create analysis context for this strategy
	      StrategyAnalysisContext ctx(
					  strategy,
					  baseSecurity,
					  inSampleBacktestingDates,
					  oosBacktestingDates,
					  timeFrame,
					  oosSpreadStats
					  );

	      // Execute pipeline (handles all gates with fail-fast)
	      auto pipeline = createPipeline();
	      auto decision = pipeline->executeForStrategy(ctx, outputStream);

	      // Update summary based on decision type
	      updateSummaryForDecision(decision);

	      // Keep strategy if passed all gates
	      if (decision.passed())
		{
		  filteredStrategies.push_back(strategy);
		}
	    }
	  catch (const std::exception& e)
	    {
	      outputStream << "Warning: Failed to evaluate strategy '" << strategy->getStrategyName()
			   << "' performance: " << e.what() << "\n";
	      outputStream << "Excluding strategy from filtered results.\n";
	    }
	}

      // Count survivors by direction
      auto [survivorsLong, survivorsShort] = countSurvivorsByDirection(filteredStrategies);

      // Summary
      outputStream << "BCa Performance Filtering complete: " << filteredStrategies.size()
		   << "/" << survivingStrategies.size() << " strategies passed criteria.\n\n";
      outputStream << "[Summary] Flagged for divergence: " << mFilteringSummary.getFlaggedCount()
		   << " (passed robustness: " << mFilteringSummary.getFlagPassCount() << ", failed: "
		   << (mFilteringSummary.getFlaggedCount() >= mFilteringSummary.getFlagPassCount() ?
		       (mFilteringSummary.getFlaggedCount() - mFilteringSummary.getFlagPassCount()) : 0) << ")\n";
      outputStream << "          Fail reasons → "
		   << "L-bound/hurdle: " << mFilteringSummary.getFailLBoundCount()
		   << ", L-variability near hurdle: " << mFilteringSummary.getFailLVarCount()
		   << ", regime-mix: " << mFilteringSummary.getFailRegimeMixCount()     // NEW
		   << ", split-sample: " << mFilteringSummary.getFailSplitCount()
		   << ", tail-risk: " << mFilteringSummary.getFailTailCount() << "\n";
      outputStream << "          Insufficient sample (pre-filter): " << mFilteringSummary.getInsufficientCount() << "\n";
      outputStream << "          Survivors by direction → Long: " << survivorsLong
		   << ", Short: " << survivorsShort << "\n";

      return filteredStrategies;
    }
    

    std::pair<size_t, size_t> PerformanceFilter::countSurvivorsByDirection(
    					   const std::vector<std::shared_ptr<PalStrategy<Num>>>& filteredStrategies) const
    {
      size_t survivorsLong = 0, survivorsShort = 0;
      for (const auto& s : filteredStrategies)
 {
   const auto& nm = s->getStrategyName();
   if (nm.find("Long") != std::string::npos)  ++survivorsLong;
   if (nm.find("Short") != std::string::npos) ++survivorsShort;
 }
      return {survivorsLong, survivorsShort};
    }

    std::unique_ptr<FilteringPipeline> PerformanceFilter::createPipeline()
    {
      return std::make_unique<FilteringPipeline>(
        mHurdleCalculator,
        mConfidenceLevel,
        mNumResamples,
        mRobustnessConfig,
        mLSensitivity,
        mFragileEdgePolicy,
        mApplyFragileAdvice,
        mFilteringSummary
      );
    }

    void PerformanceFilter::updateSummaryForDecision(const FilterDecision& decision)
    {
      switch (decision.decision)
      {
        case FilterDecisionType::FailInsufficientData:
          mFilteringSummary.incrementInsufficientCount();
          break;
        case FilterDecisionType::FailHurdle:
          mFilteringSummary.incrementFailLBoundCount();
          break;
        case FilterDecisionType::FailRobustness:
          // RobustnessStage already updated summary via helper
          break;
        case FilterDecisionType::FailLSensitivity:
          // L-sensitivity: pipeline determines type and updates directly
          // Check the rationale to determine if catastrophic or variability
          if (decision.rationale.find("catastrophic") != std::string::npos)
          {
            mFilteringSummary.incrementFailLBoundCount();
          }
          else
          {
            mFilteringSummary.incrementFailLVarCount();
          }
          break;
        case FilterDecisionType::FailRegimeMix:
          mFilteringSummary.incrementFailRegimeMixCount();
          break;
        case FilterDecisionType::FailFragileEdge:
          // No summary counter for advisory
          break;
        case FilterDecisionType::Pass:
          // No action needed
          break;
      }
    }

  } // namespace filtering
} // namespace palvalidator
