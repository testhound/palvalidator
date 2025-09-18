#include "MetaStrategyAnalyzer.h"
#include <algorithm>
#include <limits>
#include <numeric>
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
	  // Create unified meta-strategy
	  auto metaStrategy = createMetaStrategy(survivingStrategies, baseSecurity);
	  
	  // Execute backtesting
	  auto bt = executeBacktesting(metaStrategy, timeFrame, backtestingDates);
	  auto metaReturns = bt->getAllHighResReturns(metaStrategy.get());

	  if (metaReturns.size() < 2)
	    {
	      outputStream << "[Meta] Not enough data from unified meta-strategy.\n";
	      mMetaStrategyPassed = false;
	      return;
	    }

	  // Get number of trades from closed position history (needed for drawdown analysis)
	  const uint32_t numTrades = bt->getClosedPositionHistory().getNumPositions();
	  
	  // Block length for meta bootstrap (use meta-strategy's median hold)
	  const unsigned int metaMedianHold = bt->getClosedPositionHistory().getMedianHoldingPeriod();
	  const size_t Lmeta = std::max<size_t>(2, metaMedianHold);
   
	  // Portfolio-level cost hurdle (use meta-strategy's annualized trades)
	  const Num metaAnnualizedTrades = Num(bt->getEstimatedAnnualizedTrades());
	  
	  // Write detailed backtester results to file using proper naming convention
	  std::string performanceFileName = palvalidator::utils::createUnifiedMetaStrategyPerformanceFileName(
													      baseSecurity->getSymbol(), validationMethod);
   
	  // Write performance report with exit bar tuning
	  writePerformanceReport(bt, performanceFileName, outputStream);
	  
	  // Create TeeStream to write drawdown analysis to both console and performance file
	  std::ofstream performanceFile(performanceFileName, std::ios::app); // Append mode
	  if (performanceFile.is_open())
	    {
	      palvalidator::utils::TeeStream teeStream(outputStream, performanceFile);
	      
	      // Perform statistical analysis (including drawdown analysis)
	      // The TeeStream will write to both console and performance file simultaneously
	      performStatisticalAnalysis(metaReturns, baseSecurity, timeFrame, Lmeta,
					 metaAnnualizedTrades, survivingStrategies.size(),
					 teeStream, numTrades);
	      
	      performanceFile.close();
	    }
	  else
	    {
	      // Perform statistical analysis without performance file
	      performStatisticalAnalysis(metaReturns, baseSecurity, timeFrame, Lmeta,
					 metaAnnualizedTrades, survivingStrategies.size(),
					 outputStream, numTrades);
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

  } // namespace filtering
} // namespace palvalidator
