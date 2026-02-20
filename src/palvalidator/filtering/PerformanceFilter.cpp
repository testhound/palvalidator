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
#include "filtering/RegimeMixStressRunner.h"
#include "BarAlignedSeries.h"
#include "CostStressUtils.h"
#include "filtering/FilteringPipeline.h"
#include "TradingBootstrapFactory.h"
#include "ConfigSeeds.h"
#include <limits>
#include "version.h"

namespace palvalidator
{
  namespace filtering
  {

    using palvalidator::filtering::makeCostStressHurdles;

    constexpr std::size_t kRegimeVolWindow = 20;
    PerformanceFilter::PerformanceFilter(const Num& confidenceLevel,
  unsigned int numResamples, uint64_t masterSeed,
        std::shared_ptr<palvalidator::diagnostics::IBootstrapObserver> observer,
        bool tradeLevelBootstrapping)
    : mConfidenceLevel(confidenceLevel),
      mNumResamples(numResamples),
      mRobustnessConfig(),
      mFragileEdgePolicy(),
      mFilteringSummary(),
      mApplyFragileAdvice(true),
      mLSensitivity(),
      mBootstrapFactory(std::make_unique<BootstrapFactory>(masterSeed)),
      mObserver(std::move(observer)),
      mTradeLevelBootstrapping(tradeLevelBootstrapping)
    {
    }

    PerformanceFilter::PerformanceFilter(const Num& confidenceLevel,
	 unsigned int numResamples)
      : PerformanceFilter (confidenceLevel, numResamples, palvalidator::config::kDefaultCrnMasterSeed)
    {
    }

    // Destructor definition required for std::unique_ptr with incomplete type in header
    PerformanceFilter::~PerformanceFilter() = default;

    std::vector<std::shared_ptr<PalStrategy<Num>>>
    PerformanceFilter::filterByPerformance(const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    	   std::shared_ptr<Security<Num>> baseSecurity,
    	   const DateRange& inSampleBacktestingDates,
    	   const DateRange& oosBacktestingDates,
    	   TimeFrame::Duration timeFrame,
    	   std::ostream& outputStream,
    	   std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats)
    {
      // CRITICAL VALIDATION: Ensure we're using OOS dates for bootstrap analysis
      // This prevents accidental use of in-sample dates in the validation pipeline
      if (oosBacktestingDates.getFirstDateTime() <= inSampleBacktestingDates.getLastDateTime())
      {
        std::ostringstream errorMsg;
        errorMsg << "PerformanceFilter::filterByPerformance - FATAL: OOS dates must occur AFTER in-sample dates.\n"
                 << "  In-Sample: " << inSampleBacktestingDates.getFirstDateTime() << " to "
                 << inSampleBacktestingDates.getLastDateTime() << "\n"
                 << "  Out-of-Sample: " << oosBacktestingDates.getFirstDateTime() << " to "
                 << oosBacktestingDates.getLastDateTime();
        throw std::invalid_argument(errorMsg.str());
      }
      
      std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;

      // Reset summary for new filtering run
      mFilteringSummary = FilteringSummary();

      // Display version information first
      outputStream << "PalValidator version " << palvalidator::Version::getVersion() << "\n";
      
      outputStream << "\nFiltering " << survivingStrategies.size() << " surviving strategies by BCa performance...\n";
      outputStream << "Filter 1 (Statistical Viability): Annualized Lower Bound > 0\n";
      outputStream << "Filter 2 (Economic Significance): Annualized Lower Bound > Trading Spread Costs\n";
      outputStream << "  - Cost assumptions: $0 commission; slippage/spread per side is data-driven when available.\n";
      
      // Output bootstrapping type
      if (mTradeLevelBootstrapping) {
        outputStream << "  - Bootstrapping method: Trade level bootstrapping\n";
      } else {
        outputStream << "  - Bootstrapping method: Bar level bootstrapping\n";
      }

      // Track strategies skipped due to insufficient trades
      unsigned int skippedStrategiesCount = 0;

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

       // Check if strategy was skipped due to insufficient trades
       if (decision.decision == FilterDecisionType::FailInsufficientData) {
  ++skippedStrategiesCount;
       }

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

      // Output statistics about skipped strategies due to insufficient trades
      unsigned int totalStrategies = survivingStrategies.size();
      outputStream << "Strategy processing summary:\n";
      outputStream << "  Total strategies evaluated: " << totalStrategies << "\n";
      outputStream << "  Strategies skipped due to insufficient trades (<9): " << skippedStrategiesCount << "\n";
      outputStream << "  Strategies fully processed: " << (totalStrategies - skippedStrategiesCount) << "\n";
      outputStream << "\n";

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
      // In the new design, the TradingHurdleCalculator is simplified and used
      // directly within the pipeline, so we pass it here.
      TradingHurdleCalculator hurdleCalculator;
       return std::make_unique<FilteringPipeline>(
        hurdleCalculator,
        mConfidenceLevel,
        mNumResamples,
        mRobustnessConfig,
        mLSensitivity,
        mFragileEdgePolicy,
        mApplyFragileAdvice,
        mFilteringSummary,
  *mBootstrapFactory,
        mObserver,
        mTradeLevelBootstrapping
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
