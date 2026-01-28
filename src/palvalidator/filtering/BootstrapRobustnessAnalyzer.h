// ============================================================================
// BOOTSTRAP ROBUSTNESS ANALYZER - Clean, Non-Invasive Implementation
// ============================================================================
// 
// This approach uses a "BootstrapRobustnessAnalyzer" class to orchestrate 
// multiple rounds of bootstrapping with different seeds, WITHOUT modifying 
// PerformanceFilter or FilteringPipeline at all.
//
// Benefits:
// - Zero modifications to existing code
// - Complete separation of concerns
// - Easy to test and maintain
// - Reusable pattern for other multi-run analyses
// ============================================================================

#ifndef BOOTSTRAP_ROBUSTNESS_ANALYZER_H
#define BOOTSTRAP_ROBUSTNESS_ANALYZER_H

#include <vector>
#include <memory>
#include <map>
#include <string>
#include <random>
#include "DateRange.h"
#include "TimeFrame.h"
#include "PalStrategy.h"
#include "PerformanceFilter.h"

namespace palvalidator::analysis
{
  using mkc_timeseries::DateRange;
  using mkc_timeseries::TimeFrame;
  using namespace mkc_timeseries;
  
  /**
   * @brief Configuration for bootstrap robustness analysis
   */
  class BootstrapConfig {
  public:
    /**
     * @brief Constructor
     * @param numSeeds Number of bootstrap seeds to test (e.g., 10)
     * @param minPassRate Minimum pass rate to accept strategy (0.0 to 1.0, e.g., 0.95)
     * @param requirePerfect If true, require 100% pass rate (overrides minPassRate)
     * @param reportDetailedResults If true, report detailed per-seed results
     */
    BootstrapConfig(unsigned int numSeeds,
                    double minPassRate,
                    bool requirePerfect = false,
                    bool reportDetailedResults = false)
      : mNumSeeds(numSeeds)
      , mMinPassRate(minPassRate)
      , mRequirePerfect(requirePerfect)
      , mReportDetailedResults(reportDetailedResults)
    {}
    
    // Getters
    unsigned int getNumSeeds() const { return mNumSeeds; }
    double getMinPassRate() const { return mMinPassRate; }
    bool getRequirePerfect() const { return mRequirePerfect; }
    bool getReportDetailedResults() const { return mReportDetailedResults; }


  private:
    unsigned int mNumSeeds;
    double mMinPassRate;
    bool mRequirePerfect;
    bool mReportDetailedResults;
  };

  /**
   * @brief Results for one strategy tested across multiple bootstrap seeds
   */
  class StrategyBootstrapResult {
  public:
    /**
     * @brief Constructor
     * @param strategy The strategy that was tested
     * @param testedSeeds Vector of seeds used for testing
     * @param passedForEachSeed Vector indicating pass/fail for each seed
     * @param passCount Number of seeds where strategy passed
     */
    StrategyBootstrapResult(
			    std::shared_ptr<mkc_timeseries::PalStrategy<Num>> strategy,
			    std::vector<uint64_t> testedSeeds,
			    std::vector<bool> passedForEachSeed,
			    int passCount)
      : mStrategy(strategy)
      , mTestedSeeds(std::move(testedSeeds))
      , mPassedForEachSeed(std::move(passedForEachSeed))
      , mPassCount(passCount)
      , mTotalTested(static_cast<int>(mTestedSeeds.size()))
      , mPassRate(mTotalTested > 0 ? mPassCount / static_cast<double>(mTotalTested) : 0.0)
      , mAccepted(false)
    {}
    
    // Getters
    std::shared_ptr<mkc_timeseries::PalStrategy<Num>> getStrategy() const { return mStrategy; }
    const std::vector<uint64_t>& getTestedSeeds() const { return mTestedSeeds; }
    const std::vector<bool>& getPassedForEachSeed() const { return mPassedForEachSeed; }
    int getPassCount() const { return mPassCount; }
    int getTotalTested() const { return mTotalTested; }
    double getPassRate() const { return mPassRate; }
    bool isAccepted() const { return mAccepted; }
    
    // Allow analyzer to set acceptance (package-private equivalent)
    void setAccepted(bool accepted) { mAccepted = accepted; }
    
  private:
    std::shared_ptr<mkc_timeseries::PalStrategy<Num>> mStrategy;
    std::vector<uint64_t> mTestedSeeds;
    std::vector<bool> mPassedForEachSeed;
    int mPassCount;
    int mTotalTested;
    double mPassRate;
    bool mAccepted;
  };

  /**
   * @brief Aggregated results from bootstrap robustness analysis
   */
  class RobustnessAnalysisResult {
  public:
    /**
     * @brief Constructor
     * @param strategyResults Individual strategy results
     * @param acceptedStrategies Strategies that met pass rate threshold
     */
    RobustnessAnalysisResult(
			     std::vector<StrategyBootstrapResult> strategyResults,
			     std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>> acceptedStrategies)
      : mStrategyResults(std::move(strategyResults))
      , mAcceptedStrategies(std::move(acceptedStrategies))
      , mTotalStrategies(static_cast<int>(mStrategyResults.size()))
      , mAcceptedCount(static_cast<int>(mAcceptedStrategies.size()))
      , mRejectedCount(mTotalStrategies - mAcceptedCount)
    {
      // Calculate distribution statistics
      mPerfectPassRate = 0;
      mHighPassRate = 0;
      mModeratePassRate = 0;
      mLowPassRate = 0;
      mVeryLowPassRate = 0;
        
      for (const auto& result : mStrategyResults) {
	double rate = result.getPassRate();
	if (rate >= 1.0) {
	  mPerfectPassRate++;
	} else if (rate >= 0.95) {
	  mHighPassRate++;
	} else if (rate >= 0.80) {
	  mModeratePassRate++;
	} else if (rate >= 0.50) {
	  mLowPassRate++;
	} else {
	  mVeryLowPassRate++;
	}
      }
    }
    
    // Getters
    const std::vector<StrategyBootstrapResult>& getStrategyResults() const { return mStrategyResults; }
    const std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>>& getAcceptedStrategies() const { 
      return mAcceptedStrategies; 
    }
    int getTotalStrategies() const { return mTotalStrategies; }
    int getAcceptedCount() const { return mAcceptedCount; }
    int getRejectedCount() const { return mRejectedCount; }
    int getPerfectPassRateCount() const { return mPerfectPassRate; }
    int getHighPassRateCount() const { return mHighPassRate; }
    int getModeratePassRateCount() const { return mModeratePassRate; }
    int getLowPassRateCount() const { return mLowPassRate; }
    int getVeryLowPassRateCount() const { return mVeryLowPassRate; }
    
  private:
    std::vector<StrategyBootstrapResult> mStrategyResults;
    std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>> mAcceptedStrategies;
    int mTotalStrategies;
    int mAcceptedCount;
    int mRejectedCount;
    
    // Distribution statistics
    int mPerfectPassRate;      // 100%
    int mHighPassRate;         // 95-99%
    int mModeratePassRate;     // 80-94%
    int mLowPassRate;          // 50-79%
    int mVeryLowPassRate;      // <50%
  };

  /**
   * @brief Orchestrates multiple rounds of bootstrap analysis with different seeds
   * 
   * This class runs PerformanceFilter multiple times with different bootstrap seeds
   * and aggregates the results to identify robust vs. marginal strategies.
   * 
   * Key design principle: Uses PerformanceFilter as a BLACK BOX - no modifications needed.
   */
  class BootstrapRobustnessAnalyzer {
  public:
    /**
     * @brief Constructor
     * @param masterSeed The master seed for deterministic seed generation
     * @param config Configuration for bootstrap robustness testing
     */
    BootstrapRobustnessAnalyzer(uint64_t masterSeed, const BootstrapConfig& config)
      : mMasterSeed(masterSeed)
      , mConfig(config)
    {}
    
    /**
     * @brief Run bootstrap robustness analysis
     * 
     * @param survivingStrategies Strategies to test
     * @param baseSecurity Security for backtesting
     * @param inSampleBacktestingDates In-sample date range
     * @param oosBacktestingDates Out-of-sample date range
     * @param timeFrame Time frame for analysis
     * @param outputStream Output stream for logging
     * @param confidenceLevel Confidence level for bootstrap (e.g., 0.95)
     * @param numResamples Number of bootstrap resamples (e.g., 1000)
     * @param oosSpreadStats Optional spread statistics
     * 
     * @return Robustness analysis results with accepted strategies
     */
    RobustnessAnalysisResult analyze(
				     const std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>>& survivingStrategies,
				     std::shared_ptr<mkc_timeseries::Security<Num>> baseSecurity,
				     const DateRange& inSampleBacktestingDates,
				     const DateRange& oosBacktestingDates,
				     TimeFrame::Duration timeFrame,
				     std::ostream& outputStream,
				     const Num& confidenceLevel,
				     unsigned int numResamples,
				     std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats = std::nullopt
				     );
    
  private:
    uint64_t mMasterSeed;
    BootstrapConfig mConfig;
    
    /**
     * @brief Generate deterministic test seeds from master seed
     */
    std::vector<uint64_t> generateTestSeeds();

    std::pair<size_t, size_t> countSurvivorsByDirection(const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies) const;
    
    /**
     * @brief Test strategies with one specific seed
     * 
     * Simply creates a PerformanceFilter with the given seed and calls
     * filterByPerformance - completely black box usage.
     */
    std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>> testWithSeed(
										uint64_t testSeed,
										const std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>>& strategies,
										std::shared_ptr<mkc_timeseries::Security<Num>> baseSecurity,
										const DateRange& inSampleBacktestingDates,
										const DateRange& oosBacktestingDates,
										TimeFrame::Duration timeFrame,
										std::ostream& outputStream,
										const Num& confidenceLevel,
										unsigned int numResamples,
										std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats
										);
    
    /**
     * @brief Aggregate results across all seeds
     */
    RobustnessAnalysisResult aggregateResults(
					      const std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>>& allStrategies,
					      const std::vector<uint64_t>& testSeeds,
					      const std::map<uint64_t, std::vector<std::shared_ptr<mkc_timeseries::PalStrategy<Num>>>>& resultsPerSeed
					      );
    
    /**
     * @brief Report results
     */
    void reportResults(const RobustnessAnalysisResult& results, std::ostream& os) const;
  };

} // namespace palvalidator::analysis

#endif // BOOTSTRAP_ROBUSTNESS_ANALYZER_H
