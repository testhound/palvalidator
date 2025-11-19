#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <optional>
#include "number.h"
#include "Security.h"
#include "DateRange.h"
#include "PalStrategy.h"
#include "PortfolioFilter.h"
#include "BackTester.h"
#include "TimeFrame.h"
#include "filtering/FilteringTypes.h"
#include "filtering/MetaTradingHurdleCalculator.h"
#include "filtering/CostStressUtils.h"
#include "utils/ValidationTypes.h"

// Forward declarations for external types
namespace mkc_timeseries {
  template<typename Decimal> class BackTester;
  template<typename Decimal> class ClosedPositionHistory;
}

// Forward declaration for CostStressHurdlesT
namespace palvalidator
{
  namespace filtering
  {
    template<typename Num>
    struct CostStressHurdlesT;
  }
}

namespace palvalidator
{
  namespace filtering
  {
    using namespace mkc_timeseries;
    using Num = num::DefaultNumber;
    using ValidationMethod = palvalidator::utils::ValidationMethod;

    /**
     * @brief Analyzer for meta-strategy performance using unified PalMetaStrategy approach
     *
     * This class implements meta-strategy analysis that:
     * - Combines multiple surviving strategies into a unified PalMetaStrategy
     * - Performs BCa bootstrap analysis on the unified strategy returns
     * - Calculates strategy-level cost hurdles and risk-adjusted returns
     * - Determines if the meta-strategy passes performance criteria
     */
    class MetaStrategyAnalyzer
    {
    public:
      /**
       * @brief Constructor with risk parameters and bootstrap configuration
       * @param riskParams Risk parameters including risk-free rate and premium
       * @param confidenceLevel Confidence level for BCa bootstrap analysis (e.g., 0.95)
       * @param numResamples Number of bootstrap resamples (e.g., 2000)
       */
      MetaStrategyAnalyzer(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples);

      /**
       * @brief Analyze meta-strategy performance using unified PalMetaStrategy approach
       * @param survivingStrategies Vector of strategies that survived individual filtering
       * @param baseSecurity Security to test strategies against
       * @param backtestingDates Date range for backtesting
       * @param timeFrame Time frame for analysis
       * @param outputStream Output stream for logging (typically a TeeStream)
       * @param validationMethod Validation method for reporting purposes
       */
      void analyzeMetaStrategy(
			       const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
			       std::shared_ptr<Security<Num>> baseSecurity,
			       const DateRange& backtestingDates,
			       TimeFrame::Duration timeFrame,
			       std::ostream& outputStream,
			       ValidationMethod validationMethod = ValidationMethod::Unadjusted,
			       std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats = std::nullopt);

      /**
       * @brief Check if the last analyzed meta-strategy passed performance criteria
       * @return True if meta-strategy passed, false otherwise
       */
      bool didMetaStrategyPass() const
      {
        return mMetaStrategyPassed;
      }

      /**
       * @brief Get the annualized lower bound from the last meta-strategy analysis
       * @return Annualized geometric mean lower bound for the portfolio
       */
      const Num& getAnnualizedLowerBound() const
      {
        return mAnnualizedLowerBound;
      }

      /**
       * @brief Get the required return hurdle from the last meta-strategy analysis
       * @return Required return hurdle for the portfolio
       */
      const Num& getRequiredReturn() const
      {
        return mRequiredReturn;
      }

    private:

      // Computes the observed longest losing streak and its (1 - alpha) bootstrap upper bound
      // Writes a one-line summary to `os`. Returns {observed, upperBound}.
      std::pair<int,int>
      computeLosingStreakBound(const ClosedPositionHistory<Num>& cph,
			       std::ostream& os) const;
      /**
       * @brief Analyze meta-strategy using unified PalMetaStrategy approach
       * @param survivingStrategies Vector of strategies that survived individual filtering
       * @param baseSecurity Security to test strategies against
       * @param backtestingDates Date range for backtesting
       * @param timeFrame Time frame for analysis
       * @param outputStream Output stream for logging
       */
      void analyzeMetaStrategyUnified(
				      const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
				      std::shared_ptr<Security<Num>> baseSecurity,
				      const DateRange& backtestingDates,
				      TimeFrame::Duration timeFrame,
				      std::ostream& outputStream,
				      ValidationMethod validationMethod,
				      std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats = std::nullopt);

    private:
      // Pyramiding configuration class
      class PyramidConfiguration {
      public:
          enum FilterType { NO_FILTER, ADAPTIVE_VOLATILITY_FILTER, BREAKEVEN_STOP };
          
          PyramidConfiguration(unsigned int level, const std::string& desc, const StrategyOptions& options, FilterType filterType = NO_FILTER)
              : mPyramidLevel(level), mDescription(desc), mStrategyOptions(options), mFilterType(filterType) {}
          
          unsigned int getPyramidLevel() const { return mPyramidLevel; }
          const std::string& getDescription() const { return mDescription; }
          const StrategyOptions& getStrategyOptions() const { return mStrategyOptions; }
          FilterType getFilterType() const { return mFilterType; }
          
      private:
          unsigned int mPyramidLevel;
          std::string mDescription;
          StrategyOptions mStrategyOptions;
          FilterType mFilterType;
      };

      // Drawdown analysis results class
      class DrawdownResults {
      public:
          DrawdownResults(bool hasResults, const Num& pointEstimate, const Num& lowerBound,
                         const Num& upperBound, const std::string& errorMessage = "")
              : mHasResults(hasResults), mPointEstimate(pointEstimate), mLowerBound(lowerBound),
                mUpperBound(upperBound), mErrorMessage(errorMessage) {}
          
          // Default constructor for failed analysis
          DrawdownResults() : mHasResults(false), mPointEstimate(DecimalConstants<Num>::DecimalZero),
                             mLowerBound(DecimalConstants<Num>::DecimalZero),
                             mUpperBound(DecimalConstants<Num>::DecimalZero), mErrorMessage("No analysis performed") {}
          
          // Getters
          bool hasResults() const { return mHasResults; }
          const Num& getPointEstimate() const { return mPointEstimate; }
          const Num& getLowerBound() const { return mLowerBound; }
          const Num& getUpperBound() const { return mUpperBound; }
          const std::string& getErrorMessage() const { return mErrorMessage; }
          
      private:
          bool mHasResults;
          Num mPointEstimate;
          Num mLowerBound;
          Num mUpperBound;
          std::string mErrorMessage;
      };

      // Bootstrap results struct (moved here for use by PyramidGateResults)
      struct BootstrapResults {
          Num lbGeoPeriod;
          Num lbMeanPeriod;
          Num lbGeoAnn;
          Num lbMeanAnn;
          size_t blockLength;
      };

      // Backtest result class for pyramid analysis
      class PyramidBacktestResult
      {
      public:
        PyramidBacktestResult(std::shared_ptr<BackTester<Num>> bt, std::vector<Num> returns)
          : mBacktester(std::move(bt)), mMetaReturns(std::move(returns))
        {}

        std::shared_ptr<BackTester<Num>> getBacktester() const { return mBacktester; }
        const std::vector<Num>& getMetaReturns() const { return mMetaReturns; }
        const ClosedPositionHistory<Num>& getClosedPositionHistory() const
        {
          return mBacktester->getClosedPositionHistory();
        }

      private:
        std::shared_ptr<BackTester<Num>> mBacktester;
        std::vector<Num> mMetaReturns;
      };

      // Validation gate results class
      class PyramidGateResults
      {
      public:
        PyramidGateResults(bool regularPass, bool multiSplitPass, bool metaSelectionPass,
                          const BootstrapResults& bootResults, const CostStressHurdlesT<Num>& hurdles,
                          double keff, std::size_t lMeta, const Num& metaAnnTrades)
          : mRegularBootstrapPass(regularPass),
            mMultiSplitPass(multiSplitPass),
            mPassMetaSelectionAware(metaSelectionPass),
            mAllGatesPassed(regularPass && multiSplitPass && metaSelectionPass),
            mBootstrapResults(bootResults),
            mHurdles(hurdles),
            mKeff(keff),
            mLMeta(lMeta),
            mMetaAnnualizedTrades(metaAnnTrades)
        {}

        // Pass/Fail Status
        bool regularBootstrapPassed() const { return mRegularBootstrapPass; }
        bool multiSplitPassed() const { return mMultiSplitPass; }
        bool passMetaSelectionAware() const { return mPassMetaSelectionAware; }
        bool allGatesPassed() const { return mAllGatesPassed; }
        
        // Key Data
        const BootstrapResults& getBootstrapResults() const { return mBootstrapResults; }
        const CostStressHurdlesT<Num>& getHurdles() const { return mHurdles; }
        double getKeff() const { return mKeff; }
        std::size_t getLMeta() const { return mLMeta; }
        const Num& getMetaAnnualizedTrades() const { return mMetaAnnualizedTrades; }

      private:
        bool mRegularBootstrapPass;
        bool mMultiSplitPass;
        bool mPassMetaSelectionAware;
        bool mAllGatesPassed;

        BootstrapResults mBootstrapResults;
        CostStressHurdlesT<Num> mHurdles;
        double mKeff;
        std::size_t mLMeta;
        Num mMetaAnnualizedTrades;
      };

      // Risk analysis results class
      class PyramidRiskResults
      {
      public:
        PyramidRiskResults(DrawdownResults ddResults, Num futureLB, int obsStreak, int ubStreak)
          : mDrawdownResults(std::move(ddResults)),
            mFutureReturnsLowerBoundPct(futureLB),
            mObservedLosingStreak(obsStreak),
            mLosingStreakUpperBound(ubStreak)
        {}

        const DrawdownResults& getDrawdownResults() const { return mDrawdownResults; }
        const Num& getFutureReturnsLowerBoundPct() const { return mFutureReturnsLowerBoundPct; }
        int getObservedLosingStreak() const { return mObservedLosingStreak; }
        int getLosingStreakUpperBound() const { return mLosingStreakUpperBound; }

      private:
        DrawdownResults mDrawdownResults;
        Num mFutureReturnsLowerBoundPct;
        int mObservedLosingStreak;
        int mLosingStreakUpperBound;
      };

      struct MultiSplitResult
      {
        bool                 applied;     // true if slices were created (K valid)
        bool                 pass;        // true if median slice LB > hurdle
        Num                  medianLB;    // annualized
        Num                  minLB;       // annualized
        std::vector<Num>     sliceLBs;    // annualized LBs per slice, size == K when applied
      };

      // Multi-split OOS gate: bootstrap per-slice LBs and compare median against hurdle.
      // Uses bootstrapReturnSlices(...) (already a member) and your existing hurdle calculator.
      MultiSplitResult runMultiSplitGate(const std::vector<Num>              &metaReturns,
					 std::size_t                          K,
					 std::size_t                          Lmeta,
					 double                               annualizationFactor,
					 const mkc_timeseries::Security<Num> *baseSecurity,
					 mkc_timeseries::TimeFrame::Duration  timeFrame,
					 const mkc_timeseries::BackTester<Num>* bt,
					 std::ostream                        &os,
					 std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats) const;
      
      // Pyramiding analysis results class
      class PyramidResults
      {
      public:
	PyramidResults(unsigned int pyramidLevel, const std::string& description,
		       const Num& annualizedLowerBound, const Num& requiredReturn,
		       bool passed, const Num& annualizedTrades, uint32_t numTrades,
		       std::shared_ptr<BackTester<Num>> backTester, const DrawdownResults& drawdownResults,
		       const Num& futureReturnsLowerBound,
		       int observedLosingStreak,
		       int losingStreakUpperBound)
	  : mPyramidLevel(pyramidLevel), mDescription(description),
	    mAnnualizedLowerBound(annualizedLowerBound), mRequiredReturn(requiredReturn),
	    mPassed(passed), mAnnualizedTrades(annualizedTrades), mNumTrades(numTrades),
	    mBackTester(backTester),
	    mDrawdownResults(drawdownResults),
	    mFutureReturnsLowerBound(futureReturnsLowerBound),
	    mObservedLosingStreak(observedLosingStreak),
	    mLosingStreakUpperBound(losingStreakUpperBound)
	{}
          
	// Getters
	unsigned int getPyramidLevel() const { return mPyramidLevel; }
	const std::string& getDescription() const { return mDescription; }
	const Num& getAnnualizedLowerBound() const { return mAnnualizedLowerBound; }
	const Num& getRequiredReturn() const { return mRequiredReturn; }
	bool getPassed() const { return mPassed; }
	const Num& getAnnualizedTrades() const { return mAnnualizedTrades; }
	uint32_t getNumTrades() const { return mNumTrades; }
	std::shared_ptr<BackTester<Num>> getBackTester() const { return mBackTester; }
	const DrawdownResults& getDrawdownResults() const { return mDrawdownResults; }
	const Num& getFutureReturnsLowerBound() const { return mFutureReturnsLowerBound; }
	int getObservedLosingStreak() const { return mObservedLosingStreak; }
	int getLosingStreakUpperBound() const { return mLosingStreakUpperBound; }
          
      private:
	unsigned int mPyramidLevel;
	std::string mDescription;
	Num mAnnualizedLowerBound;
	Num mRequiredReturn;
	bool mPassed;
	Num mAnnualizedTrades;
	uint32_t mNumTrades;
	std::shared_ptr<BackTester<Num>> mBackTester;
	DrawdownResults mDrawdownResults;
	Num mFutureReturnsLowerBound;
	int mObservedLosingStreak{0};
	int mLosingStreakUpperBound{0};
      };

      // Helper methods for analyzeMetaStrategyUnified
      std::shared_ptr<PalMetaStrategy<Num>> createMetaStrategy(
          const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
          std::shared_ptr<Security<Num>> baseSecurity) const;

      std::shared_ptr<PalMetaStrategy<Num>> createMetaStrategy(
          const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
          std::shared_ptr<Security<Num>> baseSecurity,
          const StrategyOptions& strategyOptions) const;
      
      std::shared_ptr<PalMetaStrategy<Num, AdaptiveVolatilityPortfolioFilter<Num, mkc_timeseries::SimonsHLCVolatilityPolicy>>>
      createMetaStrategyWithAdaptiveFilter(
          const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
          std::shared_ptr<Security<Num>> baseSecurity,
          const StrategyOptions& strategyOptions) const;
      
      std::shared_ptr<BackTester<Num>> executeBacktesting(
          std::shared_ptr<PalMetaStrategy<Num>> metaStrategy,
          TimeFrame::Duration timeFrame,
          const DateRange& backtestingDates) const;
      
      template<typename FilterType>
      std::shared_ptr<BackTester<Num>> executeBacktestingWithFilter(
          std::shared_ptr<PalMetaStrategy<Num, FilterType>> metaStrategy,
          TimeFrame::Duration timeFrame,
          const DateRange& backtestingDates) const
      {
        return BackTesterFactory<Num>::backTestStrategy(metaStrategy, timeFrame, backtestingDates);
      }
      
      void performExitBarTuning(
          const ClosedPositionHistory<Num>& closedPositionHistory,
          std::ostream& outputStream,
          std::ofstream& performanceFile) const;
      
      void writePerformanceReport(
          std::shared_ptr<BackTester<Num>> bt,
          const std::string& performanceFileName,
          std::ostream& outputStream) const;

      // Pyramiding-specific helper methods
      std::vector<PyramidConfiguration> createPyramidConfigurations() const;

      // Choose an initial number of slices K for multi-split OOS gating.
      // Policy:
      //  - minLen = max(20, Lmeta)
      //  - Kmax   = floor(n / minLen)
      //  - Ktarget = 4 if n >= 160 else 3
      //  - K = clamp(Ktarget, 2, min(4, Kmax))
      //
      // Note: runMultiSplitGate(...) will further reduce K if needed (or skip when infeasible).
      std::size_t chooseInitialSliceCount(std::size_t n, std::size_t Lmeta) const;

      std::vector<Num>
      bootstrapReturnSlices(const std::vector<Num>& returns,
			  std::size_t K,
			  std::size_t blockLength,
			  unsigned int numResamples,
			  double confidenceLevel,
			    double annualizationFactor) const;

			   // New helper methods for analyzeSinglePyramidLevel refactoring
			   PyramidBacktestResult runPyramidBacktest(
			       const PyramidConfiguration& config,
			       const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
			       std::shared_ptr<Security<Num>> baseSecurity,
			       const DateRange& backtestingDates,
			       TimeFrame::Duration timeFrame,
			       std::ostream& outputStream) const;

			   PyramidGateResults runPyramidValidationGates(
			       const std::vector<Num>& metaReturns,
			       std::shared_ptr<BackTester<Num>> bt,
			       const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
			       std::shared_ptr<Security<Num>> baseSecurity,
			       const DateRange& backtestingDates,
			       TimeFrame::Duration timeFrame,
			       std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats,
			       std::ostream& outputStream) const;

			   PyramidRiskResults runPyramidRiskAnalysis(
			       const std::vector<Num>& metaReturns,
			       const ClosedPositionHistory<Num>& closedHistory,
			       std::size_t lMeta,
			       std::ostream& outputStream) const;

			   void logPyramidValidationResults(
			       const PyramidGateResults& gates,
			       const PyramidRiskResults& risk,
			       unsigned int pyramidLevel,
			       std::ostream& outputStream) const;

			   void logDrawdownAnalysis(
			       const DrawdownResults& drawdownResults,
			       uint32_t numTrades,
			       std::ostream& outputStream) const;
			 
			   PyramidResults analyzeSinglePyramidLevel(
          const PyramidConfiguration& config,
          const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
          std::shared_ptr<Security<Num>> baseSecurity,
          const DateRange& backtestingDates,
          TimeFrame::Duration timeFrame,
          std::ostream& outputStream,
	  std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats = std::nullopt) const;
      
      // Runs selection-aware outer bootstrap that replays meta construction.
      // Returns true if the annualized LB from the selection-aware CI exceeds the hurdle.
      bool runSelectionAwareMetaGate(
				     const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
				     std::shared_ptr<mkc_timeseries::Security<Num>> baseSecurity,
				     const mkc_timeseries::DateRange& backtestingDates,
				     mkc_timeseries::TimeFrame::Duration timeFrame,
				     std::size_t Lmeta,
				     double annualizationFactor,
				     const mkc_timeseries::BackTester<Num>* bt,  // for annualized trades in hurdle
				     std::ostream& os,
				     std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats
				     ) const;

      void writeComprehensivePerformanceReport(
          const std::vector<PyramidResults>& allResults,
          const std::string& performanceFileName,
          std::ostream& outputStream) const;
      
      void outputPyramidComparison(
          const std::vector<PyramidResults>& allResults,
          std::ostream& outputStream) const;
      
      /**
       * @brief Select the best passing pyramid configuration based on MAR ratio and performance metrics
       *
       * Selection criteria (in priority order):
       * 1. Primary: Conservative MAR ratio (annualized LB / drawdown UB) - higher is better
       * 2. Fallback: Highest annualized lower bound (when drawdown UB missing/invalid)
       * 3. Tiebreaker: Larger margin (LB - requiredReturn)
       *
       * @param allResults All pyramid analysis results
       * @param outputStream Output stream for logging the selection
       * @return Pointer to the best passing configuration, or nullptr if none passed
       */
      const PyramidResults* selectBestPassingConfiguration(
          const std::vector<PyramidResults>& allResults,
          std::ostream& outputStream) const;
      
      DrawdownResults performDrawdownAnalysisForPyramid(
          const std::vector<Num>& metaReturns,
          uint32_t numTrades,
          size_t blockLength) const;
      
      /**
       * @brief Perform future returns bound analysis on closed position history
       * @param closedPositionHistory History of closed positions
       * @param outputStream Output stream for logging
       * @return Lower bound percentage for future returns (0 if analysis fails or insufficient data)
       */
      Num performFutureReturnsBoundAnalysis(
          const ClosedPositionHistory<Num>& closedPositionHistory,
          std::ostream& outputStream) const;

      // Helper methods for performStatisticalAnalysis
      void calculatePerPeriodEstimates(
          const std::vector<Num>& metaReturns,
          std::ostream& outputStream) const;
      
      double calculateAnnualizationFactor(
          TimeFrame::Duration timeFrame,
          std::shared_ptr<Security<Num>> baseSecurity) const;
      
      BootstrapResults performBootstrapAnalysis(
          const std::vector<Num>& metaReturns,
          double annualizationFactor,
          size_t blockLength,
          std::ostream& outputStream) const;
      
      struct CostHurdleResults {
          Num riskFreeHurdle;
          Num costBasedRequiredReturn;
          Num finalRequiredReturn;
      };
      
      CostHurdleResults calculateCostHurdles(
          const Num& annualizedTrades,
          std::ostream& outputStream) const;
      
      void performDrawdownAnalysis(
          const std::vector<Num>& metaReturns,
          uint32_t numTrades,
          size_t blockLength,
          std::ostream& outputStream) const;
      
      void reportFinalResults(
          const BootstrapResults& bootstrapResults,
          const CostHurdleResults& costResults,
          size_t strategyCount,
          std::ostream& outputStream);

    private:
      palvalidator::filtering::meta::MetaTradingHurdleCalculator mHurdleCalculator; ///< Calculator for trading hurdles
      Num mConfidenceLevel;                      ///< Confidence level for BCa bootstrap
      unsigned int mNumResamples;                ///< Number of bootstrap resamples
      bool mMetaStrategyPassed;                  ///< Result of last meta-strategy analysis
      Num mAnnualizedLowerBound;                 ///< Last calculated annualized lower bound
      Num mRequiredReturn;                       ///< Last calculated required return
    };

  } // namespace filtering
} // namespace palvalidator
