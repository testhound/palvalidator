#include "MetaStrategyAnalyzer.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <iomanip>
#include "BackTester.h"
#include "Portfolio.h"
#include "DecimalConstants.h"
#include "BiasCorrectedBootstrap.h"
#include "BoundedDrawdowns.h"
#include "ParallelExecutors.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"
#include "reporting/PerformanceReporter.h"
#include "utils/OutputUtils.h"
#include "ExitPolicyJointAutoTuner.h"
#include "BacktesterStrategy.h"
#include "PalStrategy.h"
#include "TimeSeriesIndicators.h"
#include <fstream>

namespace palvalidator
{
  namespace filtering
  {
    MetaStrategyAnalyzer::MetaStrategyAnalyzer(const RiskParameters& riskParams,
    	       const Num& confidenceLevel,
    	       unsigned int numResamples)
      : mHurdleCalculator(riskParams),
	mConfidenceLevel(confidenceLevel),
	mNumResamples(numResamples),
	mMetaStrategyPassed(false)
    {
      // Note: mAnnualizedLowerBound and mRequiredReturn are not initialized here
      // They will be set when analyzeMetaStrategy() is called
    }

    void MetaStrategyAnalyzer::analyzeMetaStrategy(
    		   const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    		   std::shared_ptr<Security<Num>> baseSecurity,
    		   const DateRange& backtestingDates,
    		   TimeFrame::Duration timeFrame,
    		   std::ostream& outputStream,
    		   ValidationMethod validationMethod)
    {
      if (survivingStrategies.empty())
	{
	  outputStream << "\n[Meta] No surviving strategies to aggregate.\n";
	  mMetaStrategyPassed = false;
	  return;
	}

      outputStream << "\n[Meta] Building unified PalMetaStrategy from " << survivingStrategies.size() << " survivors...\n";

      analyzeMetaStrategyUnified(survivingStrategies, baseSecurity, backtestingDates,
				 timeFrame, outputStream, validationMethod);
    }

    void MetaStrategyAnalyzer::analyzeMetaStrategyUnified(
     const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
     std::shared_ptr<Security<Num>> baseSecurity,
     const DateRange& backtestingDates,
     TimeFrame::Duration timeFrame,
     std::ostream& outputStream,
     ValidationMethod validationMethod)
    {
      if (survivingStrategies.empty())
	{
	  outputStream << "\n[Meta] No surviving strategies to aggregate.\n";
	  mMetaStrategyPassed = false;
	  return;
	}

      outputStream << "\n[Meta] Building unified PalMetaStrategy from "
		   << survivingStrategies.size() << " survivors...\n";
      
      try
	{
	  // Create pyramid configurations
	  std::vector<PyramidConfiguration> pyramidConfigs = createPyramidConfigurations();
	  
	  // Storage for all pyramid results
	  std::vector<PyramidResults> allResults;
   
	  // Run analysis for each pyramid level
	  for (const auto& config : pyramidConfigs)
	    {
	      auto result = analyzeSinglePyramidLevel(config, survivingStrategies, baseSecurity,
						      backtestingDates, timeFrame, outputStream);
	      allResults.push_back(result);
	    }
   
	  // Write comprehensive performance file with all pyramid results
	  std::string performanceFileName = palvalidator::utils::createUnifiedMetaStrategyPerformanceFileName(
    									      baseSecurity->getSymbol(), validationMethod);
	  writeComprehensivePerformanceReport(allResults, performanceFileName, outputStream);
	  
	  // Output pyramid comparison summary
	  outputPyramidComparison(allResults, outputStream);
   
	  // Set overall meta-strategy result based on best performing pyramid level
	  mMetaStrategyPassed = false;
	  for (const auto& result : allResults)
	    {
	      if (result.getPassed())
		{
		  mMetaStrategyPassed = true;
		  // Store the best result for backward compatibility
		  mAnnualizedLowerBound = result.getAnnualizedLowerBound();
		  mRequiredReturn = result.getRequiredReturn();
		  break;
		}
	    }
	}
      catch (const std::exception& e)
	{
	  outputStream << "[Meta] Error in unified meta-strategy backtesting: " << e.what() << "\n";
	  mMetaStrategyPassed = false;
	}
    }

    void MetaStrategyAnalyzer::performStatisticalAnalysis(
    			  const std::vector<Num>& metaReturns,
    			  std::shared_ptr<Security<Num>> baseSecurity,
    			  TimeFrame::Duration timeFrame,
    			  size_t blockLength,
    			  const Num& annualizedTrades,
    			  size_t strategyCount,
    			  std::ostream& outputStream,
    			  uint32_t numTrades)
    {
      // Calculate per-period point estimates
      calculatePerPeriodEstimates(metaReturns, outputStream);

      // Calculate annualization factor
      double annualizationFactor = calculateAnnualizationFactor(timeFrame, baseSecurity);

      // Perform bootstrap analysis
      auto bootstrapResults = performBootstrapAnalysis(metaReturns, annualizationFactor, blockLength, outputStream);

      // Calculate cost hurdles
      auto costResults = calculateCostHurdles(annualizedTrades, outputStream);

      // Report final results and store member variables
      reportFinalResults(bootstrapResults, costResults, strategyCount, outputStream);
 
      // Perform drawdown analysis
      performDrawdownAnalysis(metaReturns, numTrades, blockLength, outputStream);
    }

    std::shared_ptr<PalMetaStrategy<Num>>
    MetaStrategyAnalyzer::createMetaStrategy(
					     const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
					     std::shared_ptr<Security<Num>> baseSecurity) const
    {
      // Create PalMetaStrategy
      auto metaPortfolio = std::make_shared<Portfolio<Num>>("Meta Portfolio");
      metaPortfolio->addSecurity(baseSecurity);
      
      auto metaStrategy = std::make_shared<PalMetaStrategy<Num>>(
          "Unified Meta Strategy", metaPortfolio);

      // Add all patterns from surviving strategies
      for (const auto& strategy : survivingStrategies)
        {
          auto pattern = strategy->getPalPattern();
          metaStrategy->addPricePattern(pattern);
        }

      return metaStrategy;
    }

    std::shared_ptr<PalMetaStrategy<Num>>
    MetaStrategyAnalyzer::createMetaStrategy(
          const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
          std::shared_ptr<Security<Num>> baseSecurity,
          const StrategyOptions& strategyOptions) const
    {
      // Create PalMetaStrategy with custom StrategyOptions
      auto metaPortfolio = std::make_shared<Portfolio<Num>>("Meta Portfolio");
      metaPortfolio->addSecurity(baseSecurity);
      
      auto metaStrategy = std::make_shared<PalMetaStrategy<Num>>(
          "Unified Meta Strategy", metaPortfolio, strategyOptions);

      // Add all patterns from surviving strategies
      for (const auto& strategy : survivingStrategies)
        {
          auto pattern = strategy->getPalPattern();
          metaStrategy->addPricePattern(pattern);
        }

      return metaStrategy;
    }

    std::shared_ptr<PalMetaStrategy<Num, AdaptiveVolatilityPortfolioFilter<Num, mkc_timeseries::SimonsHLCVolatilityPolicy>>>
    MetaStrategyAnalyzer::createMetaStrategyWithAdaptiveFilter(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const StrategyOptions& strategyOptions) const
    {
      // Create PalMetaStrategy with AdaptiveVolatilityPortfolioFilter
      auto metaPortfolio = std::make_shared<Portfolio<Num>>("Meta Portfolio with Adaptive Filter");
      metaPortfolio->addSecurity(baseSecurity);
      
      auto metaStrategy = std::make_shared<PalMetaStrategy<Num, AdaptiveVolatilityPortfolioFilter<Num, mkc_timeseries::SimonsHLCVolatilityPolicy>>>(
          "Unified Meta Strategy with Adaptive Filter", metaPortfolio, strategyOptions);

      // Add all patterns from surviving strategies
      for (const auto& strategy : survivingStrategies)
        {
          auto pattern = strategy->getPalPattern();
          metaStrategy->addPricePattern(pattern);
        }

      return metaStrategy;
    }

    std::vector<MetaStrategyAnalyzer::PyramidConfiguration>
    MetaStrategyAnalyzer::createPyramidConfigurations() const
    {
      std::vector<PyramidConfiguration> configs;
      
      // Pyramid Level 0: No pyramiding (current behavior)
      configs.emplace_back(0, "No Pyramiding", StrategyOptions(false, 0, 8));
      
      // Pyramid Level 1: 1 additional position
      configs.emplace_back(1, "1 Additional Position", StrategyOptions(true, 1, 8));
      
      // Pyramid Level 2: 2 additional positions
      configs.emplace_back(2, "2 Additional Positions", StrategyOptions(true, 2, 8));
      
      // Pyramid Level 3: 3 additional positions
      configs.emplace_back(3, "3 Additional Positions", StrategyOptions(true, 3, 8));
      
      // Pyramid Level 4: Adaptive Volatility Filter (no pyramiding)
      configs.emplace_back(4, "Volatility Filter",
                          StrategyOptions(false, 0, 8),
                          PyramidConfiguration::ADAPTIVE_VOLATILITY_FILTER);
      
      return configs;
    }

    MetaStrategyAnalyzer::PyramidResults
    MetaStrategyAnalyzer::analyzeSinglePyramidLevel(
        const PyramidConfiguration& config,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& backtestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream) const
    {
      outputStream << "\n[Meta] Pyramid Level " << config.getPyramidLevel()
                   << " (" << config.getDescription() << "):\n";

      // Create meta-strategy and execute backtesting based on filter type
      std::shared_ptr<BackTester<Num>> bt;
      std::vector<Num> metaReturns;
      
      if (config.getFilterType() == PyramidConfiguration::ADAPTIVE_VOLATILITY_FILTER)
        {
          // Create strategy with AdaptiveVolatilityPortfolioFilter
          auto filteredStrategy = createMetaStrategyWithAdaptiveFilter(
              survivingStrategies, baseSecurity, config.getStrategyOptions());
          bt = executeBacktestingWithFilter(filteredStrategy, timeFrame, backtestingDates);
          metaReturns = bt->getAllHighResReturns(filteredStrategy.get());
        }
      else
        {
          // Create standard strategy (existing code path)
          auto metaStrategy = createMetaStrategy(survivingStrategies, baseSecurity, config.getStrategyOptions());
          bt = executeBacktesting(metaStrategy, timeFrame, backtestingDates);
          metaReturns = bt->getAllHighResReturns(metaStrategy.get());
        }

      if (metaReturns.size() < 2)
        {
          outputStream << "      Not enough data from pyramid level " << config.getPyramidLevel() << ".\n";
          DrawdownResults emptyDrawdown;
          return PyramidResults(config.getPyramidLevel(), config.getDescription(),
                              DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero,
                              false, DecimalConstants<Num>::DecimalZero, 0, bt, emptyDrawdown);
        }

      // Get number of trades and other metrics
      const uint32_t numTrades = bt->getClosedPositionHistory().getNumPositions();
      const unsigned int metaMedianHold = bt->getClosedPositionHistory().getMedianHoldingPeriod();
      const size_t Lmeta = std::max<size_t>(2, metaMedianHold);
      const Num metaAnnualizedTrades = Num(bt->getEstimatedAnnualizedTrades());

      // Perform statistical analysis for this pyramid level
      calculatePerPeriodEstimates(metaReturns, outputStream);
      double annualizationFactor = calculateAnnualizationFactor(timeFrame, baseSecurity);
      auto bootstrapResults = performBootstrapAnalysis(metaReturns, annualizationFactor, Lmeta, outputStream);
      auto costResults = calculateCostHurdles(metaAnnualizedTrades, outputStream);

      // Determine if this pyramid level passes
      bool pyramidPassed = (bootstrapResults.lbGeoAnn > costResults.finalRequiredReturn);
      
      // Output results for this pyramid level
      outputStream << std::endl;
      outputStream << "      Annualized Lower Bound (GeoMean, compounded): "
                   << (bootstrapResults.lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                   << "      Annualized Lower Bound (Mean, compounded):    "
                   << (bootstrapResults.lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                   << "      Required Return (max(cost,riskfree)): "
                   << (costResults.finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%\n\n";

      if (pyramidPassed)
          outputStream << "      RESULT: ✓ Pyramid Level " << config.getPyramidLevel() << " PASSES\n";
      else
          outputStream << "      RESULT: ✗ Pyramid Level " << config.getPyramidLevel() << " FAILS\n";

      // Perform drawdown analysis for this pyramid level and store results
      auto drawdownResults = performDrawdownAnalysisForPyramid(metaReturns, numTrades, Lmeta);
      
      // Output drawdown results to console
      if (drawdownResults.hasResults())
        {
          const Num qPct = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;
          const Num ciPct = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;
          
          outputStream << "      Drawdown Analysis (BCa on q=" << qPct
                       << "% percentile of max drawdown over " << numTrades << " trades):\n";
          outputStream << "        Point estimate (q=" << qPct << "%ile): "
                       << (drawdownResults.getPointEstimate() * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
          outputStream << "        Two-sided " << ciPct << "% CI for that percentile: ["
                       << (drawdownResults.getLowerBound() * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                       << (drawdownResults.getUpperBound() * DecimalConstants<Num>::DecimalOneHundred) << "%]\n";
          outputStream << "        " << ciPct << "% one-sided upper bound: "
                       << (drawdownResults.getUpperBound() * DecimalConstants<Num>::DecimalOneHundred)
                       << "%  (i.e., with " << ciPct << "% confidence, the q=" << qPct
                       << "%ile drawdown does not exceed this value)\n";
        }
      else
        {
          outputStream << "      Drawdown Analysis: " << drawdownResults.getErrorMessage() << "\n";
        }

      return PyramidResults(config.getPyramidLevel(), config.getDescription(),
                          bootstrapResults.lbGeoAnn, costResults.finalRequiredReturn,
                          pyramidPassed, metaAnnualizedTrades, numTrades, bt, drawdownResults);
    }

    std::shared_ptr<BackTester<Num>> MetaStrategyAnalyzer::executeBacktesting(
        std::shared_ptr<PalMetaStrategy<Num>> metaStrategy,
        TimeFrame::Duration timeFrame,
        const DateRange& backtestingDates) const
    {
      return BackTesterFactory<Num>::backTestStrategy(metaStrategy, timeFrame, backtestingDates);
    }

    void MetaStrategyAnalyzer::performExitBarTuning(
        const ClosedPositionHistory<Num>& closedPositionHistory,
        std::ostream& outputStream,
        std::ofstream& performanceFile) const
    {
      if (closedPositionHistory.getNumPositions() > 0)
        {
          try
            {
              // Create ExitPolicyJointAutoTuner with reasonable defaults
              mkc_timeseries::ExitPolicyJointAutoTuner<Num> exitTuner(closedPositionHistory, 8);
       
              // Run the exit policy tuning
              auto tuningReport = exitTuner.tuneExitPolicy();
       
              // Write exit bar analysis results to the performance file
              performanceFile << std::endl;
              performanceFile << "=== Exit Bar Analysis ===" << std::endl;
              performanceFile << "Failure to perform exit bar: " << tuningReport.getFailureToPerformBars() << std::endl;
              performanceFile << "Breakeven bar: " << tuningReport.getBreakevenActivationBars() << std::endl;
              performanceFile << "===========================" << std::endl;
       
              outputStream << "      Exit bar analysis completed and written to performance file." << std::endl;
            }
          catch (const std::exception& e)
            {
              outputStream << "      Warning: Exit bar analysis failed: " << e.what() << std::endl;
              performanceFile << std::endl;
              performanceFile << "=== Exit Bar Analysis ===" << std::endl;
              performanceFile << "Exit bar analysis failed: " << e.what() << std::endl;
              performanceFile << "===========================" << std::endl;
            }
        }
      else
        {
          outputStream << "      Skipping exit bar analysis: No closed positions available." << std::endl;
          performanceFile << std::endl;
          performanceFile << "=== Exit Bar Analysis ===" << std::endl;
          performanceFile << "Exit bar analysis skipped: No closed positions available." << std::endl;
          performanceFile << "===========================" << std::endl;
        }
    }

    void MetaStrategyAnalyzer::writePerformanceReport(
        std::shared_ptr<BackTester<Num>> bt,
        const std::string& performanceFileName,
        std::ostream& outputStream) const
    {
      std::ofstream performanceFile(performanceFileName);
      if (performanceFile.is_open())
        {
          palvalidator::reporting::PerformanceReporter::writeBacktestReport(performanceFile, bt);
          
          // Perform exit policy joint auto-tuning analysis
          const auto& closedPositionHistory = bt->getClosedPositionHistory();
          performExitBarTuning(closedPositionHistory, outputStream, performanceFile);
          
          performanceFile.close();
          outputStream << "\n      Unified PalMetaStrategy detailed performance written to: " << performanceFileName << std::endl;
        }
      else
        {
          outputStream << "\n      Warning: Could not write performance file: " << performanceFileName << std::endl;
        }
    }

    void MetaStrategyAnalyzer::calculatePerPeriodEstimates(
        const std::vector<Num>& metaReturns,
        std::ostream& outputStream) const
    {
      const Num am = StatUtils<Num>::computeMean(metaReturns);
      const Num gm = GeoMeanStat<Num>{}(metaReturns);
      outputStream << "      Per-period point estimates (pre-annualization): "
                   << "Arithmetic mean =" << (am * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                   << "Geometric mean =" << (gm * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
    }

    double MetaStrategyAnalyzer::calculateAnnualizationFactor(
        TimeFrame::Duration timeFrame,
        std::shared_ptr<Security<Num>> baseSecurity) const
    {
      if (timeFrame == TimeFrame::INTRADAY)
        {
          auto minutes = baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes();
          return mkc_timeseries::calculateAnnualizationFactor(timeFrame, minutes);
        }
      else
        {
          return mkc_timeseries::calculateAnnualizationFactor(timeFrame);
        }
    }

    MetaStrategyAnalyzer::BootstrapResults MetaStrategyAnalyzer::performBootstrapAnalysis(
        const std::vector<Num>& metaReturns,
        double annualizationFactor,
        size_t blockLength,
        std::ostream& outputStream) const
    {
      // Block length for meta bootstrap
      StationaryBlockResampler<Num> metaSampler(blockLength);
      using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;

      // Bootstrap portfolio series
      GeoMeanStat<Num> statGeo;
      BlockBCA metaGeo(metaReturns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, metaSampler);
      BlockBCA metaMean(metaReturns, mNumResamples, mConfidenceLevel.getAsDouble(),
                        &mkc_timeseries::StatUtils<Num>::computeMean, metaSampler);

      const Num lbGeoPeriod = metaGeo.getLowerBound();
      const Num lbMeanPeriod = metaMean.getLowerBound();

      outputStream << "      Per-period BCa lower bounds (pre-annualization): "
                   << "Geo=" << (lbGeoPeriod * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                   << "Mean=" << (lbMeanPeriod * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
      outputStream << "      (Meta uses block resampling with L=" << blockLength << ")\n";

      // Annualize portfolio BCa results
      BCaAnnualizer<Num> metaGeoAnn(metaGeo, annualizationFactor);
      BCaAnnualizer<Num> metaMeanAnn(metaMean, annualizationFactor);

      const Num lbGeoAnn = metaGeoAnn.getAnnualizedLowerBound();
      const Num lbMeanAnn = metaMeanAnn.getAnnualizedLowerBound();

      return {lbGeoPeriod, lbMeanPeriod, lbGeoAnn, lbMeanAnn, blockLength};
    }

    MetaStrategyAnalyzer::CostHurdleResults MetaStrategyAnalyzer::calculateCostHurdles(
        const Num& annualizedTrades,
        std::ostream& outputStream) const
    {
      // Portfolio-level cost hurdle - show detailed calculation
      const Num riskFreeHurdle = mHurdleCalculator.calculateRiskFreeHurdle();
      const Num costBasedRequiredReturn = mHurdleCalculator.calculateCostBasedRequiredReturn(annualizedTrades);
      const Num finalRequiredReturn = mHurdleCalculator.calculateFinalRequiredReturn(annualizedTrades);
      
      // Show detailed cost hurdle breakdown
      outputStream << std::endl;
      outputStream << "      Cost Hurdle Analysis:" << std::endl;
      outputStream << "        Annualized Trades: " << annualizedTrades << " trades/year" << std::endl;
      outputStream << "        Round-trip Cost: " << (mHurdleCalculator.getSlippagePerSide() * mkc_timeseries::DecimalConstants<Num>::DecimalTwo * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "% per trade" << std::endl;
      outputStream << "        Raw Cost Hurdle: " << annualizedTrades << " × " << (mHurdleCalculator.getSlippagePerSide() * mkc_timeseries::DecimalConstants<Num>::DecimalTwo * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "% = " << (annualizedTrades * mHurdleCalculator.getSlippagePerSide() * mkc_timeseries::DecimalConstants<Num>::DecimalTwo * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
      outputStream << "        Safety Buffer: " << mHurdleCalculator.getCostBufferMultiplier() << "× multiplier" << std::endl;
      outputStream << "        Cost-Based Required Return: " << (costBasedRequiredReturn * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
      outputStream << "        Risk-Free Hurdle: " << (riskFreeHurdle * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
      outputStream << "        Final Required Return: max(" << (costBasedRequiredReturn * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%, " << (riskFreeHurdle * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%) = " << (finalRequiredReturn * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;

      return {riskFreeHurdle, costBasedRequiredReturn, finalRequiredReturn};
    }

    void MetaStrategyAnalyzer::performDrawdownAnalysis(
        const std::vector<Num>& metaReturns,
        uint32_t numTrades,
        size_t blockLength,
        std::ostream& outputStream) const
    {
      if (numTrades > 0)
        {
          try
            {
              using BoundedDrawdowns = mkc_timeseries::BoundedDrawdowns<Num, concurrency::ThreadPoolExecutor<>>;
       
              // Create thread pool executor for parallel processing
              concurrency::ThreadPoolExecutor<> executor;
       
              // Calculate drawdown bounds using BCa bootstrap with parallel execution
              // Parameters: metaReturns, mNumResamples, mConfidenceLevel, numTrades, 5000, mConfidenceLevel, blockLength
              auto drawdownResult = BoundedDrawdowns::bcaBoundsForDrawdownFractile(
                  metaReturns,
                  mNumResamples,
                  mConfidenceLevel.getAsDouble(),
                  static_cast<int>(numTrades),
                  5000,
                  mConfidenceLevel.getAsDouble(),
                  blockLength,
                  executor
                  );

              const Num qPct  = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;  // the dd percentile you targeted
              const Num ciPct = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;  // the CI level

	      outputStream << std::endl;
              outputStream << "      Drawdown Analysis (BCa on q=" << qPct
                           << "% percentile of max drawdown over " << numTrades << " trades):\n";
              outputStream << "        Point estimate (q=" << qPct << "%ile): "
                           << (drawdownResult.statistic * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
              outputStream << "        Two-sided " << ciPct << "% CI for that percentile: ["
                           << (drawdownResult.lowerBound * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                           << (drawdownResult.upperBound * DecimalConstants<Num>::DecimalOneHundred) << "%]\n";
              outputStream << "        " << ciPct << "% one-sided upper bound: "
                           << (drawdownResult.upperBound * DecimalConstants<Num>::DecimalOneHundred)
                           << "%  (i.e., with " << ciPct << "% confidence, the q=" << qPct
                           << "%ile drawdown does not exceed this value)\n";
            }
          catch (const std::exception& e)
            {
              outputStream << "      Drawdown Analysis: Failed - " << e.what() << "\n";
            }
        }
      else
        {
          outputStream << "      Drawdown Analysis: Skipped (no trades available)\n";
        }
    }

    void MetaStrategyAnalyzer::reportFinalResults(
        const BootstrapResults& bootstrapResults,
        const CostHurdleResults& costResults,
        size_t strategyCount,
        std::ostream& outputStream)
    {
      // Store results
      mAnnualizedLowerBound = bootstrapResults.lbGeoAnn;
      mRequiredReturn = costResults.finalRequiredReturn;
      mMetaStrategyPassed = (bootstrapResults.lbGeoAnn > costResults.finalRequiredReturn);

      // Output results for unified meta-strategy
      outputStream << "\n[Meta] Unified PalMetaStrategy with " << strategyCount << " patterns:\n";

      outputStream << "      Annualized Lower Bound (GeoMean, compounded): " << (bootstrapResults.lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                   << "      Annualized Lower Bound (Mean, compounded):    " << (bootstrapResults.lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                   << "      Required Return (max(cost,riskfree)): "
                   << (costResults.finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

      if (mMetaStrategyPassed)
          outputStream << "      RESULT: ✓ Unified Metastrategy PASSES\n";
      else
          outputStream << "      RESULT: ✗ Unified Metastrategy FAILS\n";
        
      outputStream << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
    }

    MetaStrategyAnalyzer::DrawdownResults MetaStrategyAnalyzer::performDrawdownAnalysisForPyramid(
        const std::vector<Num>& metaReturns,
        uint32_t numTrades,
        size_t blockLength) const
    {
      if (numTrades == 0)
        {
          return DrawdownResults(false, DecimalConstants<Num>::DecimalZero,
                               DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero,
                               "Skipped (no trades available)");
        }

      try
        {
          using BoundedDrawdowns = mkc_timeseries::BoundedDrawdowns<Num, concurrency::ThreadPoolExecutor<>>;
          concurrency::ThreadPoolExecutor<> executor;
          
          auto drawdownResult = BoundedDrawdowns::bcaBoundsForDrawdownFractile(
              metaReturns,
              mNumResamples,
              mConfidenceLevel.getAsDouble(),
              static_cast<int>(numTrades),
              5000,
              mConfidenceLevel.getAsDouble(),
              blockLength,
              executor
              );

          return DrawdownResults(true, drawdownResult.statistic, drawdownResult.lowerBound, drawdownResult.upperBound);
        }
      catch (const std::exception& e)
        {
          return DrawdownResults(false, DecimalConstants<Num>::DecimalZero,
                               DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero,
                               std::string("Failed - ") + e.what());
        }
    }

    void MetaStrategyAnalyzer::writeComprehensivePerformanceReport(
        const std::vector<PyramidResults>& allResults,
        const std::string& performanceFileName,
        std::ostream& outputStream) const
    {
      std::ofstream performanceFile(performanceFileName);
      if (!performanceFile.is_open())
        {
          outputStream << "\n      Warning: Could not write comprehensive performance file: " << performanceFileName << std::endl;
          return;
        }

      // Write header
      performanceFile << "=== Unified Meta-Strategy Pyramiding Analysis ===" << std::endl;
      performanceFile << "Generated: " << palvalidator::utils::getCurrentTimestamp() << std::endl;
      if (!allResults.empty())
        {
          performanceFile << "Patterns: " << allResults.size() << " pyramid levels analyzed" << std::endl;
        }
      performanceFile << std::endl;

      // Write detailed results for each pyramid level
      for (const auto& result : allResults)
        {
          performanceFile << "=== Pyramid Level " << result.getPyramidLevel()
                         << " (" << result.getDescription() << ") ===" << std::endl;
          
          // Write backtesting report for this pyramid level
          palvalidator::reporting::PerformanceReporter::writeBacktestReport(performanceFile, result.getBackTester());
          
          // Write statistical summary for this pyramid level
          performanceFile << std::endl;
          performanceFile << "--- Statistical Analysis Summary ---" << std::endl;
          performanceFile << "Annualized Lower Bound (GeoMean): "
                         << (result.getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
          performanceFile << "Required Return: "
                         << (result.getRequiredReturn() * DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
          performanceFile << "Annualized Trades: " << result.getAnnualizedTrades() << std::endl;
          performanceFile << "Total Trades: " << result.getNumTrades() << std::endl;
          performanceFile << "Result: " << (result.getPassed() ? "PASS" : "FAIL") << std::endl;
          
          // Write drawdown analysis for this pyramid level
          performanceFile << std::endl;
          performanceFile << "--- Drawdown Analysis ---" << std::endl;
          const auto& drawdownResults = result.getDrawdownResults();
          if (drawdownResults.hasResults())
            {
              const Num qPct = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;
              const Num ciPct = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;
              
              performanceFile << "Drawdown Analysis (BCa on q=" << qPct
                             << "% percentile of max drawdown over " << result.getNumTrades() << " trades):" << std::endl;
              performanceFile << "  Point estimate (q=" << qPct << "%ile): "
                             << (drawdownResults.getPointEstimate() * DecimalConstants<Num>::DecimalOneHundred) << "%" << std::endl;
              performanceFile << "  Two-sided " << ciPct << "% CI for that percentile: ["
                             << (drawdownResults.getLowerBound() * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                             << (drawdownResults.getUpperBound() * DecimalConstants<Num>::DecimalOneHundred) << "%]" << std::endl;
              performanceFile << "  " << ciPct << "% one-sided upper bound: "
                             << (drawdownResults.getUpperBound() * DecimalConstants<Num>::DecimalOneHundred)
                             << "%  (i.e., with " << ciPct << "% confidence, the q=" << qPct
                             << "%ile drawdown does not exceed this value)" << std::endl;
            }
          else
            {
              performanceFile << "Drawdown Analysis: " << drawdownResults.getErrorMessage() << std::endl;
            }
          
          // Perform exit bar tuning ONLY for pyramid level 0
          if (result.getPyramidLevel() == 0)
            {
              const auto& closedPositionHistory = result.getBackTester()->getClosedPositionHistory();
              performExitBarTuning(closedPositionHistory, outputStream, performanceFile);
            }
          
          performanceFile << std::endl;
        }

      // Write comparison summary
      performanceFile << "=== Pyramid Comparison Summary ===" << std::endl;
      performanceFile << "Level | Description              | Ann. Lower Bound | Required Return | Pass/Fail | Trades/Year" << std::endl;
      performanceFile << "------|--------------------------|------------------|-----------------|-----------|------------" << std::endl;
      
      for (const auto& result : allResults)
        {
          performanceFile << std::setw(5) << result.getPyramidLevel() << " | "
                         << std::setw(24) << result.getDescription() << " | "
                         << std::setw(15) << std::fixed << std::setprecision(1)
                         << (result.getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred) << "% | "
                         << std::setw(14) << std::fixed << std::setprecision(1)
                         << (result.getRequiredReturn() * DecimalConstants<Num>::DecimalOneHundred) << "% | "
                         << std::setw(9) << (result.getPassed() ? "PASS" : "FAIL") << " | "
                         << std::setw(10) << std::fixed << std::setprecision(1) << result.getAnnualizedTrades() << std::endl;
        }

      // Find and report best performance
      auto bestResult = std::max_element(allResults.begin(), allResults.end(),
          [](const PyramidResults& a, const PyramidResults& b) {
              return a.getAnnualizedLowerBound() < b.getAnnualizedLowerBound();
          });
      
      if (bestResult != allResults.end())
        {
          performanceFile << std::endl;
          performanceFile << "Best Performance: Pyramid Level " << bestResult->getPyramidLevel()
                         << " (" << (bestResult->getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred)
                         << "% annualized lower bound)" << std::endl;
          performanceFile << "Recommended Configuration: " << bestResult->getDescription() << std::endl;
        }

      performanceFile.close();
      outputStream << "\n      Comprehensive pyramiding analysis written to: " << performanceFileName << std::endl;
    }

    void MetaStrategyAnalyzer::outputPyramidComparison(
						       const std::vector<PyramidResults>& allResults,
						       std::ostream& outputStream) const
    {
      outputStream << "\n[Meta] Pyramid Analysis Summary:\n";
      outputStream << "      Level | Description              |      MAR | Ann. Lower Bound | Drawdown UB | Required Return | Pass/Fail\n";
      outputStream << "      ------|--------------------------|----------|------------------|-------------|-----------------|----------\n";

      // Save original stream state to restore it later
      std::ios_base::fmtflags original_flags = outputStream.flags();
      char original_fill = outputStream.fill();
      std::streamsize original_precision = outputStream.precision();

      // Set formatting for the entire table. It will now be respected.
      outputStream << std::setfill(' ') << std::fixed << std::setprecision(2);

      for (const auto& result : allResults)
	{
	  const auto& drawdownResults = result.getDrawdownResults();
	  const Num drawdownUB = drawdownResults.getUpperBound();

	  outputStream << "      "
		       << std::right << std::setw(5) << result.getPyramidLevel() << " | "
		       << std::left << std::setw(24) << result.getDescription() << " | ";

	  // MAR Ratio
	  if (drawdownResults.hasResults() && drawdownUB > DecimalConstants<Num>::DecimalZero)
	    {
	      const Num marRatio = result.getAnnualizedLowerBound() / drawdownUB;
	      outputStream << std::right << std::setw(8) << marRatio.getAsDouble();
	    }
	  else
	    {
	      outputStream << std::right << std::setw(8) << "N/A";
	    }
	  outputStream << " | ";

	  // Ann. Lower Bound
	  outputStream << std::right << std::setw(15)
		       << (result.getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred).getAsDouble() << "% | ";

	  // Drawdown UB
	  if (drawdownResults.hasResults())
	    {
	      outputStream << std::right << std::setw(10) << (drawdownUB * DecimalConstants<Num>::DecimalOneHundred).getAsDouble() << "% | ";
	    }
	  else
	    {
	      outputStream << std::right << std::setw(10) << "N/A" << "% | ";
	    }
      
	  // Required Return
	  outputStream << std::right << std::setw(14)
		       << (result.getRequiredReturn() * DecimalConstants<Num>::DecimalOneHundred).getAsDouble() << "% | ";

	  // Pass/Fail
	  outputStream << std::left << std::setw(9) << (result.getPassed() ? "PASS" : "FAIL") << "\n";
	}

      // Restore the original stream formatting
      outputStream.flags(original_flags);
      outputStream.fill(original_fill);
      outputStream.precision(original_precision);

      // Find and report best performance
      auto bestResult = std::max_element(allResults.begin(), allResults.end(),
					 [](const PyramidResults& a, const PyramidResults& b) {
					   return a.getAnnualizedLowerBound() < b.getAnnualizedLowerBound();
					 });

      if (bestResult != allResults.end())
	{
	  outputStream << "\n      Best Performance: Pyramid Level " << bestResult->getPyramidLevel()
		       << " (" << std::fixed << std::setprecision(2) << (bestResult->getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred).getAsDouble()
		       << "% annualized lower bound)\n";
	  outputStream << "      Recommended Configuration: " << bestResult->getDescription() << "\n";
	}

      outputStream << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
    }
  } // namespace filtering
} // namespace palvalidator
