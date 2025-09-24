#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include "number.h"
#include "Security.h"
#include "DateRange.h"
#include "PalStrategy.h"
#include "PortfolioFilter.h"
#include "BackTester.h"
#include "TimeFrame.h"
#include "filtering/FilteringTypes.h"
#include "filtering/TradingHurdleCalculator.h"
#include "utils/ValidationTypes.h"

// Forward declarations
namespace mkc_timeseries {
  template<typename Decimal> class BackTester;
  template<typename Decimal> class ClosedPositionHistory;
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
			       ValidationMethod validationMethod = ValidationMethod::Unadjusted
			       );

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
				      ValidationMethod validationMethod
				      );

      /**
       * @brief Perform statistical analysis on meta-strategy returns
       * @param metaReturns Vector of portfolio returns
       * @param baseSecurity Security for annualization factor calculation
       * @param timeFrame Time frame for analysis
       * @param blockLength Block length for bootstrap resampling
       * @param annualizedTrades Annualized trades for cost hurdle calculation
       * @param strategyCount Number of strategies (for reporting)
       * @param outputStream Output stream for logging (may be TeeStream for dual output)
       * @param numTrades Number of trades for drawdown analysis
       */
      void performStatisticalAnalysis(
          const std::vector<Num>& metaReturns,
          std::shared_ptr<Security<Num>> baseSecurity,
          TimeFrame::Duration timeFrame,
          size_t blockLength,
          const Num& annualizedTrades,
          size_t strategyCount,
          std::ostream& outputStream,
          uint32_t numTrades
          );

    private:
      // Pyramiding configuration class
      class PyramidConfiguration {
      public:
          enum FilterType { NO_FILTER, ADAPTIVE_VOLATILITY_FILTER };
          
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

      // Pyramiding analysis results class
      class PyramidResults {
      public:
          PyramidResults(unsigned int pyramidLevel, const std::string& description,
                        const Num& annualizedLowerBound, const Num& requiredReturn,
                        bool passed, const Num& annualizedTrades, uint32_t numTrades,
                        std::shared_ptr<BackTester<Num>> backTester, const DrawdownResults& drawdownResults)
              : mPyramidLevel(pyramidLevel), mDescription(description),
                mAnnualizedLowerBound(annualizedLowerBound), mRequiredReturn(requiredReturn),
                mPassed(passed), mAnnualizedTrades(annualizedTrades), mNumTrades(numTrades),
                mBackTester(backTester), mDrawdownResults(drawdownResults) {}
          
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
      
      PyramidResults analyzeSinglePyramidLevel(
          const PyramidConfiguration& config,
          const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
          std::shared_ptr<Security<Num>> baseSecurity,
          const DateRange& backtestingDates,
          TimeFrame::Duration timeFrame,
          std::ostream& outputStream) const;
      
      void writeComprehensivePerformanceReport(
          const std::vector<PyramidResults>& allResults,
          const std::string& performanceFileName,
          std::ostream& outputStream) const;
      
      void outputPyramidComparison(
          const std::vector<PyramidResults>& allResults,
          std::ostream& outputStream) const;
      
      DrawdownResults performDrawdownAnalysisForPyramid(
          const std::vector<Num>& metaReturns,
          uint32_t numTrades,
          size_t blockLength) const;

      // Helper methods for performStatisticalAnalysis
      void calculatePerPeriodEstimates(
          const std::vector<Num>& metaReturns,
          std::ostream& outputStream) const;
      
      double calculateAnnualizationFactor(
          TimeFrame::Duration timeFrame,
          std::shared_ptr<Security<Num>> baseSecurity) const;
      
      struct BootstrapResults {
          Num lbGeoPeriod;
          Num lbMeanPeriod;
          Num lbGeoAnn;
          Num lbMeanAnn;
          size_t blockLength;
      };
      
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
      TradingHurdleCalculator mHurdleCalculator; ///< Calculator for trading hurdles
      Num mConfidenceLevel;                      ///< Confidence level for BCa bootstrap
      unsigned int mNumResamples;                ///< Number of bootstrap resamples
      bool mMetaStrategyPassed;                  ///< Result of last meta-strategy analysis
      Num mAnnualizedLowerBound;                 ///< Last calculated annualized lower bound
      Num mRequiredReturn;                       ///< Last calculated required return
    };

  } // namespace filtering
} // namespace palvalidator
