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
#include "BoundFutureReturns.h"
#include "ParallelExecutors.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"
#include "reporting/PerformanceReporter.h"
#include "utils/OutputUtils.h"
#include "ExitPolicyJointAutoTuner.h"
#include "BacktesterStrategy.h"
#include "PalStrategy.h"
#include "TimeSeriesIndicators.h"
#include "CostStressUtils.h"
#include <fstream>

namespace palvalidator
{
  namespace filtering
  {
    static std::size_t kMinSliceLen = 20;
    using palvalidator::filtering::makeCostStressHurdles;


    /**
     * @brief Calculates the block length for stationary bootstrap, switching between
     * median hold period (for short series) and ACF-based method (for long series).
     * @param returns Vector of returns.
     * @param medianHold Median holding period in bars.
     * @param outputStream Stream for logging which method was used.
     * @param minSizeForACF Minimum number of returns to attempt ACF calculation.
     * @param maxACFLag Maximum lag to compute for ACF.
     * @param minACFL Minimum suggested block length from ACF.
     * @param maxACFL Maximum suggested block length from ACF.
     * @return Suggested block length L (>= 2).
     */
    static std::size_t calculateBlockLengthAdaptive(const std::vector<Num>& returns,
                                                unsigned int medianHold,
                                                std::ostream& outputStream,
                                                std::size_t minSizeForACF = 100,
                                                std::size_t maxACFLag = 20,
                                                unsigned int minACFL = 2,
                                                unsigned int maxACFL = 12) // Default max L for meta
    {
        if (returns.size() < minSizeForACF)
        {
            // --- Method 1: Median Holding Period (for shorter series) ---
            std::size_t L = std::max<std::size_t>(2, static_cast<std::size_t>(medianHold));
            // Add a check to prevent excessively large L from median hold on short series
            L = std::min(L, returns.size() / 2); // Cap L at n/2 for safety
            L = std::max<std::size_t>(2, L);     // Ensure L is still at least 2

            outputStream << "      (Using block length L=" << L
                         << " based on median hold period, n=" << returns.size() << " < " << minSizeForACF << ")\n";
            return L;
        }
        else
        {
            // --- Method 2: ACF-based (for longer series) ---
            try
            {
                // Ensure maxACFLag is reasonable given series size
                std::size_t effectiveMaxLag = std::min(maxACFLag, returns.size() - 1);
                if (effectiveMaxLag < 1) {
                  throw std::runtime_error("Cannot compute ACF with effective max lag < 1");
                }

		auto logReturns = StatUtils<Num>::percentBarsToLogBars(returns);
		
                const auto acf = mkc_timeseries::StatUtils<Num>::computeACF(logReturns, effectiveMaxLag);
                unsigned int L_acf = mkc_timeseries::StatUtils<Num>::suggestStationaryBlockLengthFromACF(
                    acf, returns.size(), minACFL, maxACFL); // Use passed-in min/max L

                outputStream << "      (Using block length L=" << L_acf
                             << " based on ACF [maxLag=" << effectiveMaxLag << ", maxL=" << maxACFL
                             << "], n=" << returns.size() << " >= " << minSizeForACF << ")\n";
                return static_cast<std::size_t>(L_acf);
            }
            catch (const std::exception& e)
            {
                // Fallback to median holding period if ACF fails
                std::size_t L = std::max<std::size_t>(2, static_cast<std::size_t>(medianHold));
                // Add safety cap here too
                L = std::min(L, returns.size() / 2);
                L = std::max<std::size_t>(2, L);

                outputStream << "      Warning: ACF block length calculation failed ('" << e.what()
                             << "'). Falling back to L=" << L << " based on median hold period.\n";
                return L;
            }
        }
    }

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
    		   ValidationMethod validationMethod,
		   std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats)
    {
      if (survivingStrategies.empty())
	{
	  outputStream << "\n[Meta] No surviving strategies to aggregate.\n";
	  mMetaStrategyPassed = false;
	  return;
	}

      outputStream << "\n[Meta] Building unified PalMetaStrategy from " << survivingStrategies.size() << " survivors...\n";

      analyzeMetaStrategyUnified(survivingStrategies, baseSecurity, backtestingDates,
				 timeFrame, outputStream, validationMethod, oosSpreadStats);
    }

    void MetaStrategyAnalyzer::analyzeMetaStrategyUnified(
     const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
     std::shared_ptr<Security<Num>> baseSecurity,
     const DateRange& backtestingDates,
     TimeFrame::Duration timeFrame,
     std::ostream& outputStream,
     ValidationMethod validationMethod,
     std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats)
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
						      backtestingDates, timeFrame, outputStream,
						      oosSpreadStats);
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

      // Don't take a position if both long and short signals fire
      metaStrategy->setSkipIfBothSidesFire(true);

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

      // Don't take a position if both long and short signals fire
      metaStrategy->setSkipIfBothSidesFire(true);

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
      
      // Pyramid Level 5: Breakeven Stop (no pyramiding)
      configs.emplace_back(5, "Breakeven Stop",
                          StrategyOptions(false, 0, 8),
                          PyramidConfiguration::BREAKEVEN_STOP);
      
      return configs;
    }

    std::size_t
    MetaStrategyAnalyzer::chooseInitialSliceCount(std::size_t n, std::size_t Lmeta) const
    {
      const std::size_t minLen = std::max<std::size_t>(kMinSliceLen, Lmeta);
      
      const std::size_t Kmax = (minLen > 0) ? (n / minLen) : 0;
      const std::size_t Ktarget = (n >= 160 ? 4 : 3);

      // Clamp K into [2, min(4, Kmax)].
      // If Kmax < 2, this will return 2, and runMultiSplitGate() will shrink/skip as needed.
      const std::size_t K = std::max<std::size_t>(
						  2,
						  std::min<std::size_t>(Ktarget, std::min<std::size_t>(4, Kmax))
						  );
 
      return K;
    }

    MetaStrategyAnalyzer::PyramidResults
    MetaStrategyAnalyzer::analyzeSinglePyramidLevel(
						    const PyramidConfiguration& config,
						    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
						    std::shared_ptr<Security<Num>> baseSecurity,
						    const DateRange& backtestingDates,
						    TimeFrame::Duration timeFrame,
						    std::ostream& outputStream,
						    std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats) const
    {
      using mkc_timeseries::DecimalConstants;

      outputStream << "\n[Meta] Pyramid Level " << config.getPyramidLevel()
		   << " (" << config.getDescription() << "):\n";

      // --- Build the meta-strategy and run backtest (unchanged paths) -----------
      std::shared_ptr<BackTester<Num>> bt;
      std::vector<Num> metaReturns;

      if (config.getFilterType() == PyramidConfiguration::ADAPTIVE_VOLATILITY_FILTER)
	{
	  auto filteredStrategy = createMetaStrategyWithAdaptiveFilter(
								       survivingStrategies, baseSecurity, config.getStrategyOptions());
	  bt = executeBacktestingWithFilter(filteredStrategy, timeFrame, backtestingDates);
	  metaReturns = bt->getAllHighResReturns(filteredStrategy.get());
	}
      else if (config.getFilterType() == PyramidConfiguration::BREAKEVEN_STOP)
	{
	  auto initialStrategy = createMetaStrategy(survivingStrategies, baseSecurity, config.getStrategyOptions());
	  auto initialBt = executeBacktesting(initialStrategy, timeFrame, backtestingDates);

	  const auto& closedPositionHistory = initialBt->getClosedPositionHistory();
	  if (closedPositionHistory.getNumPositions() > 0)
	    {
	      try
		{
		  mkc_timeseries::ExitPolicyJointAutoTuner<Num> exitTuner(closedPositionHistory, 8);
		  auto tuningReport = exitTuner.tuneExitPolicy();
		  unsigned int breakevenActivationBars =
                    static_cast<unsigned int>(tuningReport.getBreakevenActivationBars());

		  outputStream << "      Exit policy tuning completed. Breakeven activation bars: "
			       << breakevenActivationBars << "\n";

		  auto breakevenStrategy = createMetaStrategy(
							      survivingStrategies, baseSecurity, config.getStrategyOptions());
		  breakevenStrategy->addBreakEvenStop(breakevenActivationBars);

		  bt = executeBacktesting(breakevenStrategy, timeFrame, backtestingDates);
		  metaReturns = bt->getAllHighResReturns(breakevenStrategy.get());
		}
	      catch (const std::exception& e)
		{
		  outputStream << "      Warning: Exit policy tuning failed: " << e.what()
			       << ". Using standard strategy without breakeven stop.\n";
		  auto fallback = createMetaStrategy(
						     survivingStrategies, baseSecurity, config.getStrategyOptions());
		  bt = executeBacktesting(fallback, timeFrame, backtestingDates);
		  metaReturns = bt->getAllHighResReturns(fallback.get());
		}
	    }
	  else
	    {
	      outputStream << "      No closed positions available for exit policy tuning. Using standard strategy.\n";
	      auto metaStrategy = createMetaStrategy(
						     survivingStrategies, baseSecurity, config.getStrategyOptions());
	      bt = executeBacktesting(metaStrategy, timeFrame, backtestingDates);
	      metaReturns = bt->getAllHighResReturns(metaStrategy.get());
	    }
	}
      else
	{
	  auto metaStrategy = createMetaStrategy(
						 survivingStrategies, baseSecurity, config.getStrategyOptions());
	  bt = executeBacktesting(metaStrategy, timeFrame, backtestingDates);
	  metaReturns = bt->getAllHighResReturns(metaStrategy.get());
	}

      if (metaReturns.size() < 2U)
	{
	  outputStream << "      Not enough data from pyramid level " << config.getPyramidLevel() << ".\n";
	  DrawdownResults emptyDrawdown;
	  return PyramidResults(config.getPyramidLevel(), config.getDescription(),
				DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero,
				false, DecimalConstants<Num>::DecimalZero, 0, bt, emptyDrawdown,
				DecimalConstants<Num>::DecimalZero);
	}

      // --- Metrics used by both gates ------------------------------------------
      const uint32_t numTrades        = bt->getClosedPositionHistory().getNumPositions();
      const unsigned int metaMedianHold = bt->getClosedPositionHistory().getMedianHoldingPeriod();
      const std::size_t Lmeta           = calculateBlockLengthAdaptive(metaReturns, metaMedianHold, outputStream);
      const Num metaAnnualizedTrades    = Num(bt->getEstimatedAnnualizedTrades());
      const double annualizationFactor  = calculateAnnualizationFactor(timeFrame, baseSecurity);

      // --- Regular (whole-sample) BCa gate -------------------------------------
      calculatePerPeriodEstimates(metaReturns, outputStream);
      const auto bootstrapResults = performBootstrapAnalysis(metaReturns, annualizationFactor, Lmeta, outputStream);

      // Build calibrated + Qn-stressed cost hurdles (uses OOS spread stats if present)
      const std::optional<Num> configuredPerSide = mHurdleCalculator.getSlippagePerSide();
      const auto H = makeCostStressHurdles<Num>(mHurdleCalculator,
						oosSpreadStats,
						metaAnnualizedTrades,
						configuredPerSide
						);
      outputStream << "         Estimated annualized trades: "
		   << metaAnnualizedTrades << " /yr\n";

      printCostStressConcise<Num>(outputStream,
				  H,
				  bootstrapResults.lbGeoAnn,
				  "Meta",
				  oosSpreadStats,
				  false,
				  mHurdleCalculator.calculateRiskFreeHurdle());

      // Policy: require LB > base AND LB > +1·Qn
      const bool passBase = (bootstrapResults.lbGeoAnn > H.baseHurdle);
      const bool pass1Qn  = (bootstrapResults.lbGeoAnn > H.h_1q);
      const bool regularBootstrapPass = (passBase && pass1Qn);

      // --- Multi-split OOS gate (median per-slice LB > hurdle) ------------------

      const std::size_t K = chooseInitialSliceCount(metaReturns.size(), Lmeta);
      outputStream << "      Multi-split bootstrap: K=" << K
		   << ", L=" << Lmeta << ", n=" << metaReturns.size() << "\n";

      const auto ms = runMultiSplitGate(
					metaReturns,
					K,
					Lmeta,
					annualizationFactor,
					baseSecurity.get(),
					timeFrame,
					bt.get(),
					outputStream,
					oosSpreadStats);

      // Non-penalizing when not applied (too short to slice)
      const bool multiSplitPass = (!ms.applied) || ms.pass;

      // --- Final decision for this pyramid level --------------------------------
      const bool pyramidPassed = (regularBootstrapPass && multiSplitPass);

            // --- Future Returns Bound Analysis ----------------------------------------
      const auto& closedPositionHistory = bt->getClosedPositionHistory();
      outputStream << "\n";
      Num futureReturnsLowerBoundPct = performFutureReturnsBoundAnalysis(closedPositionHistory, outputStream);

      outputStream << std::endl;
      outputStream << "      Annualized Lower Bound (GeoMean, compounded): "
		   << (bootstrapResults.lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
		   << "      Annualized Lower Bound (Mean, compounded):    "
		   << (bootstrapResults.lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
		   << "      Required Return (max(cost,riskfree)): "
		   << (H.baseHurdle * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
      outputStream << "      Gates → Regular: " << (regularBootstrapPass ? "PASS" : "FAIL")
		   << ", Multi-split: " << (ms.applied ? (multiSplitPass ? "PASS" : "FAIL")
					    : "SKIPPED")
		   << "\n\n";

      if (pyramidPassed)
        outputStream << "      RESULT: ✓ Pyramid Level " << config.getPyramidLevel() << " PASSES\n";
      else
        outputStream << "      RESULT: ✗ Pyramid Level " << config.getPyramidLevel() << " FAILS\n";

      // --- Drawdown analysis (unchanged) ---------------------------------------
      auto drawdownResults = performDrawdownAnalysisForPyramid(metaReturns, numTrades, Lmeta);
      if (drawdownResults.hasResults())
	{
	  const Num qPct  = mConfidenceLevel * DecimalConstants<Num>::DecimalOneHundred;
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

      // --- Return per-level results --------------------------------------------
      return PyramidResults(config.getPyramidLevel(), config.getDescription(),
       bootstrapResults.lbGeoAnn, H.baseHurdle,
       pyramidPassed, metaAnnualizedTrades, numTrades, bt, drawdownResults,
       futureReturnsLowerBoundPct);
    }
    
    Num MetaStrategyAnalyzer::performFutureReturnsBoundAnalysis(
        const ClosedPositionHistory<Num>& closedPositionHistory,
        std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;

      // 1) Build monthly returns from closed trades
        const auto monthly =
          mkc_timeseries::buildMonthlyReturnsFromClosedPositions<Num>(closedPositionHistory);

      if (monthly.size() < 12)
      {
        outputStream << "      Future Returns Bound Analysis: Skipped "
                     << "(need at least 12 monthly returns, have "
                     << monthly.size() << " returns)\n";
        return DecimalConstants<Num>::DecimalZero;
      }

      // Get median hold from history (needed even if ACF is used)
      unsigned int medianHold = closedPositionHistory.getMedianHoldingPeriod();

      // Use specific ACF settings for monthly data (shorter max lag/L)
      const std::size_t blockLength = calculateBlockLengthAdaptive(monthly, medianHold, outputStream,
                                                                   100, // min size for ACF
                                                                   12,  // maxACFLag for monthly
                                                                   2,   // minACFL
                                                                   6);  // maxACFL for monthly
      try
	{
	  using BoundFutureReturns = mkc_timeseries::BoundFutureReturns<Num>;
	  
	  BoundFutureReturns futureReturns(monthly,  // Pass monthly returns directly
					   blockLength,
					   0.10, 0.90,  // default quantiles (10th and 90th percentile)
					   mNumResamples,  // bootstrap resamples
					   mConfidenceLevel.getAsDouble());
         
	  // Get lower bound as percentage (getLowerBound() * 100.0)
	  Num futureReturnsLowerBoundPct = futureReturns.getLowerBound() *
	    DecimalConstants<Num>::DecimalOneHundred;
        
	  outputStream << "      Future Returns Lower Bound (monthly BCa): "
		       << futureReturnsLowerBoundPct << "% "
		       << "(monthly vector size: " << monthly.size()
		       << ", block length: " << blockLength << ")\n";
	      
	  return futureReturnsLowerBoundPct;
	}
      catch (const std::exception& e)
	{
	  outputStream << "      Future Returns Bound Analysis: Failed - "
		       << e.what() << "\n";
	  return DecimalConstants<Num>::DecimalZero;
	}
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

    std::vector<Num>
    MetaStrategyAnalyzer::bootstrapReturnSlices(const std::vector<Num>& returns,
						std::size_t K,
						std::size_t blockLength,
						unsigned int numResamples,
						double confidenceLevel,
						double annualizationFactor) const
    {
      using mkc_timeseries::BCaAnnualizer;
      using mkc_timeseries::BCaBootStrap;
      using mkc_timeseries::StationaryBlockResampler;
      using mkc_timeseries::StatUtils;

      std::vector<Num> out;

      const auto slices =
	mkc_timeseries::createSliceIndicesForBootstrap(returns,
						       K,
						       std::max<std::size_t>(kMinSliceLen, blockLength));
      if (slices.empty())
	{
	  return out; // caller can decide to skip multi-split if we can't slice
	}

      out.reserve(slices.size());

      for (const auto &slice : slices)
	{
	  const auto start = slice.first;
	  const auto end   = slice.second;

	  // Extract slice
	  std::vector<Num> xs(returns.begin() + static_cast<std::ptrdiff_t>(start),
			      returns.begin() + static_cast<std::ptrdiff_t>(end));

	  // BCa on per-period geometric mean with stationary block bootstrap
	  StationaryBlockResampler<Num> sampler(blockLength);
	  GeoMeanStat<Num> statGeo;
	  using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;

	  BlockBCA bca(xs,
		       numResamples,
		       confidenceLevel,
		       statGeo,
		       sampler);

	  // Annualize the BCa bounds (correct approach)
	  BCaAnnualizer<Num> ann(bca, annualizationFactor);  // (1+r)^factor - 1
	  out.push_back(ann.getAnnualizedLowerBound());
	}

      return out;
    }

    MetaStrategyAnalyzer::MultiSplitResult
    MetaStrategyAnalyzer::runMultiSplitGate(
					    const std::vector<Num>              &metaReturns,
					    std::size_t                          K,
					    std::size_t                          Lmeta,
					    double                               annualizationFactor,
					    const mkc_timeseries::Security<Num> *baseSecurity,
					    mkc_timeseries::TimeFrame::Duration  timeFrame,
					    const mkc_timeseries::BackTester<Num>* bt,
					    std::ostream                        &os,
					    std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats
					    ) const
    {
      using mkc_timeseries::DecimalConstants;

      MultiSplitResult r;
      r.applied  = false;
      r.pass     = true;   // default non-penalizing when not applied
      r.medianLB = Num(0);
      r.minLB    = Num(0);

      // --- Enforce minimum slice length ----------------------------------------
      // Need enough bars per slice for a meaningful BCa (jackknife + block bootstrap).
      const std::size_t minLen = std::max<std::size_t>(kMinSliceLen, Lmeta);

      // Reduce K until feasible given the series length and minLen.
      std::size_t K_eff = K;
      while (K_eff > 1 && metaReturns.size() < K_eff * minLen)
	{
	  --K_eff;
	}

      if (K_eff < 2)
	{
	  os << "      [Slices] Not applied (n=" << metaReturns.size()
	     << " too short for ≥" << minLen << " bars per slice).\n";
	  return r; // not applied, non-gating
	}

      if (K_eff != K)
	{
	  os << "      [Slices] Adjusted K from " << K << " → " << K_eff
	     << " to meet min slice length ≥ " << minLen << ".\n";
	}

      // --- Per-slice BCa (annualized LB per slice) ------------------------------
      const auto sliceLBsAnn = bootstrapReturnSlices(
						     metaReturns,
						     K_eff,
						     Lmeta,
						     mNumResamples,
						     mConfidenceLevel.getAsDouble(),
						     annualizationFactor);

      if (sliceLBsAnn.size() != K_eff)
	{
	  os << "      [Slices] Not applied (insufficient length for K=" << K_eff
	     << " with min slice len " << minLen << ").\n";
	  return r; // not applied, non-gating
	}

      r.applied  = true;
      r.sliceLBs = sliceLBsAnn;

      // --- Aggregate (median/min) and compute hurdle ----------------------------
      auto lbs = r.sliceLBs;
      std::sort(lbs.begin(), lbs.end());
      r.medianLB = lbs[lbs.size() / 2];
      r.minLB    = lbs.front();

      const Num annualizedTrades = Num(bt->getEstimatedAnnualizedTrades());
      //const auto hurdles = calculateCostHurdles(annualizedTrades, os);
      //const Num required = hurdles.finalRequiredReturn;

      const std::optional<Num> configuredPerSide = mHurdleCalculator.getSlippagePerSide();

      const auto H = makeCostStressHurdles<Num>(mHurdleCalculator,
						oosSpreadStats,
						annualizedTrades,
						configuredPerSide);

      os << "         Estimated annualized trades: "
	 << annualizedTrades << " /yr\n";

      os << "      [Slices] LBs (ann, %): ";
      for (std::size_t i = 0; i < lbs.size(); ++i)
	{
	  os << (i ? ", " : "")
	     << (lbs[i] * DecimalConstants<Num>::DecimalOneHundred);
	}

      printCostStressConcise<Num>(os,
				  H,
				  r.medianLB,
				  "Slices",
				  oosSpreadStats,
				  false,
				  mHurdleCalculator.calculateRiskFreeHurdle());

      // Gate on median vs base & +1·Qn
      const bool passBase = (r.medianLB > H.baseHurdle);
      const bool pass1Qn  = (r.medianLB > H.h_1q);
      r.pass = (passBase && pass1Qn);

      if (!r.pass)
	{
	  os << "      [Slices] ✗ FAIL (median slice LB ≤ hurdle)\n";
	}
      else
	{
	  os << "      [Slices] ✓ PASS (median slice LB > hurdle)\n";
	}

      return r;
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

      outputStream << "      Costs: $0 commission; per-side slippage uses configured floor and may be calibrated by OOS spreads.\n";
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
      performanceFile << "Level | Description              | Ann. Lower Bound | Future Ret LB | Required Return | Pass/Fail | Trades/Year" << std::endl;
      performanceFile << "------|--------------------------|------------------|---------------|-----------------|-----------|------------" << std::endl;
      
      // Save original stream state to restore it later
      std::ios_base::fmtflags original_flags = performanceFile.flags();
      char original_fill = performanceFile.fill();
      std::streamsize original_precision = performanceFile.precision();

      // Set formatting for the table
      performanceFile << std::setfill(' ') << std::fixed << std::setprecision(2);

      for (const auto& result : allResults)
        {
          performanceFile << std::right << std::setw(5) << result.getPyramidLevel() << " | "
                         << std::left << std::setw(24) << result.getDescription() << " | "
                         << std::right << std::setw(15)
                         << (result.getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred).getAsDouble() << "% | "
                         << std::right << std::setw(12)
                         << result.getFutureReturnsLowerBound().getAsDouble() << "% | "
                         << std::right << std::setw(14)
                         << (result.getRequiredReturn() * DecimalConstants<Num>::DecimalOneHundred).getAsDouble() << "% | "
                         << std::right << std::setw(9) << (result.getPassed() ? "PASS" : "FAIL") << " | "
                         << std::right << std::setw(10) << result.getAnnualizedTrades().getAsDouble() << std::endl;
        }

      // Restore the original stream formatting
      performanceFile.flags(original_flags);
      performanceFile.fill(original_fill);
      performanceFile.precision(original_precision);

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
      outputStream << "      Level | Description              |      MAR | Ann. Lower Bound | Future Ret LB | Drawdown UB | Required Return | Pass/Fail\n";
      outputStream << "      ------|--------------------------|----------|------------------|---------------|-------------|-----------------|----------\n";

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

	  // Future Returns Lower Bound
	  outputStream << std::right << std::setw(12)
	        << result.getFutureReturnsLowerBound().getAsDouble() << "% | ";

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

      // Find and report best performance based on MAR ratio
      auto bestResult = std::max_element(allResults.begin(), allResults.end(),
      [](const PyramidResults& a, const PyramidResults& b) {
        // Compute MAR ratios for both
        const auto& drawdownA = a.getDrawdownResults();
        const auto& drawdownB = b.getDrawdownResults();
        
        // If either doesn't have valid drawdown results, deprioritize it
        if (!drawdownA.hasResults() || drawdownA.getUpperBound() <= DecimalConstants<Num>::DecimalZero)
          return true;  // a is worse
        if (!drawdownB.hasResults() || drawdownB.getUpperBound() <= DecimalConstants<Num>::DecimalZero)
          return false; // b is worse
        
        // Both have valid drawdowns, compare MAR ratios
        const Num marA = a.getAnnualizedLowerBound() / drawdownA.getUpperBound();
        const Num marB = b.getAnnualizedLowerBound() / drawdownB.getUpperBound();
        return marA < marB;
      });

      if (bestResult != allResults.end())
	{
	  const auto& bestDrawdown = bestResult->getDrawdownResults();
	  if (bestDrawdown.hasResults() && bestDrawdown.getUpperBound() > DecimalConstants<Num>::DecimalZero)
	    {
	      const Num bestMAR = bestResult->getAnnualizedLowerBound() / bestDrawdown.getUpperBound();
	      outputStream << "\n      Best Performance: Pyramid Level " << bestResult->getPyramidLevel()
			   << " (MAR ratio: " << std::fixed << std::setprecision(2) << bestMAR.getAsDouble() << ")\n";
	    }
	  else
	    {
	      outputStream << "\n      Best Performance: Pyramid Level " << bestResult->getPyramidLevel()
			   << " (" << std::fixed << std::setprecision(2) << (bestResult->getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred).getAsDouble()
			   << "% annualized lower bound)\n";
	    }
	  outputStream << "      Recommended Configuration: " << bestResult->getDescription() << "\n";
	}

      outputStream << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
    }
  } // namespace filtering
} // namespace palvalidator
