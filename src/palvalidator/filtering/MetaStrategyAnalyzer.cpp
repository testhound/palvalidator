#include "MetaStrategyAnalyzer.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <iomanip>
#include <map>
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
#include "MetaSelectionBootstrap.h"
#include "MetaLosingStreakBootstrapBound.h"
#include "Annualizer.h"
#include "StationaryMaskResamplers.h"
#include "filtering/RegimeMixUtils.h"
#include "filtering/RegimeMixStressRunner.h"
#include "RegimeMixStationaryResampler.h"
#include "BarAlignedSeries.h"
#include "RegimeLabeler.h"
#include "filtering/PositionSizingCalculator.h"
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
	  std::size_t n = returns.size();
	  std::size_t L = 0;

	  if (n < 50)
	    {
	      // Very short: trust the median hold
	      L = std::max<std::size_t>(2, static_cast<std::size_t>(medianHold));
	    }
	  else
	    {
	      // Medium-length: heuristic n^(1/3)
	      L = static_cast<std::size_t>(std::floor(std::pow(static_cast<double>(n), 1.0/3.0)));
	      
	      // Blend with median hold if that’s materially higher
	      L = std::max<std::size_t>(L, static_cast<std::size_t>(medianHold));
	    }

	  // Safety caps
	  L = std::min(L, n / 2);
	  L = std::max<std::size_t>(2, L);

	  outputStream << "      (Using block length L=" << L
		       << " based on "
		       << (n < 50 ? "median hold period" : "n^(1/3) heuristic")
		       << ", n=" << n << " < " << minSizeForACF << ")\n";
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

    /**
     * @brief Orchestrates the complete meta-strategy validation pipeline.
     *
     * Objective:
     * This is the public entry point. It serves as a wrapper that:
     * 1. Checks if there are any surviving strategies to analyze.
     * 2. Delegates the heavy lifting to `analyzeMetaStrategyUnified` to build
     * and stress-test the Unified PalMetaStrategy.
     *
     * Arguments:
     * survivingStrategies: List of individual strategies that passed initial filtering.
     * baseSecurity:        The underlying asset (e.g., SPY) used for backtesting context.
     * backtestingDates:    The specific date range (IS/OOS) to run the validation on.
     * timeFrame:           Bar duration (Daily, Intraday) for the backtest.
     * outputStream:        Where logs and results are printed.
     * validationMethod:    The type of statistical validation used (e.g., Romano-Wolf), for reporting.
     * oosSpreadStats:      (Optional) Realized OOS spread data for calibrating cost hurdles.
     * inSampleDates:       Required for the Regime Mix "Long Run" baseline calculation.
     */
    void MetaStrategyAnalyzer::analyzeMetaStrategy(
    		   const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    		   std::shared_ptr<Security<Num>> baseSecurity,
    		   const DateRange& oosBacktestingDates,
    		   TimeFrame::Duration timeFrame,
    		   std::ostream& outputStream,
    		   ValidationMethod validationMethod,
     std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats,
     const DateRange& inSampleDates)
    {
      // CRITICAL VALIDATION: Ensure oosBacktestingDates occur after inSampleDates
      // This is a fundamental requirement for proper out-of-sample validation
      if (oosBacktestingDates.getFirstDateTime() <= inSampleDates.getLastDateTime())
      {
        std::ostringstream errorMsg;
        errorMsg << "MetaStrategyAnalyzer::analyzeMetaStrategy - FATAL: OOS dates must occur AFTER in-sample dates.\n"
                 << "  In-Sample: " << inSampleDates.getFirstDateTime() << " to "
                 << inSampleDates.getLastDateTime() << "\n"
                 << "  Out-of-Sample: " << oosBacktestingDates.getFirstDateTime() << " to "
                 << oosBacktestingDates.getLastDateTime() << "\n"
                 << "  This validation ensures the meta-strategy analysis uses only out-of-sample data.";
        throw std::invalid_argument(errorMsg.str());
      }
      
      if (survivingStrategies.empty())
	{
	  outputStream << "\n[Meta] No surviving strategies to aggregate.\n";
	  mMetaStrategyPassed = false;
	  return;
	}

      outputStream << "\n[Meta] Building unified PalMetaStrategy from " << survivingStrategies.size() << " survivors...\n";

      analyzeMetaStrategyUnified(survivingStrategies, baseSecurity, oosBacktestingDates,
				 timeFrame, outputStream, validationMethod, oosSpreadStats, inSampleDates);
    }

    /**
     * @brief Executes the Unified Meta-Strategy analysis across all pyramid levels.
     *
     * Objective:
     * This method runs the core experiment:
     * 1. Constructs multiple versions of the portfolio (Level 0 = No Pyramiding,
     * Level 1 = 1 Add-on, etc.).
     * 2. Runs a full backtest and validation suite (`analyzeSinglePyramidLevel`)
     * for each configuration.
     * 3. Generates comprehensive performance reports.
     * 4. Selects the "Best" configuration (canonical passer) based on risk-adjusted
     * metrics (MAR ratio) to update the analyzer's final state.
     *
     * Arguments:
     * (Same as analyzeMetaStrategy)
     */
    void MetaStrategyAnalyzer::analyzeMetaStrategyUnified(const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    			  std::shared_ptr<Security<Num>> baseSecurity,
    			  const DateRange& oosBacktestingDates,
    			  TimeFrame::Duration timeFrame,
    			  std::ostream& outputStream,
    			  ValidationMethod validationMethod,
    			  std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats,
    			  const DateRange& inSampleDates)
    {
      if (survivingStrategies.empty())
	{
	  outputStream << "\n[Meta] No surviving strategies to aggregate.\n";
	  mMetaStrategyPassed = false;
	  return;
	}

      outputStream << "\n[Meta] Building unified PalMetaStrategy from "
		   << survivingStrategies.size() << " survivors...\n";

      // We determine the policy for the entire session before running any levels.
      mEffectiveSlippageFloor = determineEffectiveSlippageFloor(survivingStrategies,
								mHurdleCalculator.getSlippagePerSide(),
								oosSpreadStats,
								outputStream);

      try
	{
	  // 1) Create all pyramid configurations and collect results
	  std::vector<PyramidConfiguration> pyramidConfigs = createPyramidConfigurations();
	  std::vector<PyramidResults> allResults;
	  allResults.reserve(pyramidConfigs.size());

	  for (const auto& config : pyramidConfigs)
	    {
	      auto result = analyzeSinglePyramidLevel(
						      config,
						      survivingStrategies,
						      baseSecurity,
						      oosBacktestingDates,
						      timeFrame,
						      outputStream,
						      oosSpreadStats,
						      inSampleDates);
	      allResults.push_back(std::move(result));
	    }

	  // 2) Persist reports (unchanged)
	  const std::string performanceFileName =
	    palvalidator::utils::createUnifiedMetaStrategyPerformanceFileName(
									      baseSecurity->getSymbol(), validationMethod);

	  writeComprehensivePerformanceReport(allResults, performanceFileName, outputStream);
	  outputPyramidComparison(allResults, baseSecurity, outputStream);

	  // 3) Choose the canonical "best" passing configuration
	  //
	  //    Primary key  : conservative MAR = (annualized LB GeoMean) / (drawdown UB)
	  //    Fallback key : highest annualized LB (when drawdown UB missing/invalid)
	  //    Tiebreaker   : larger (LB - requiredReturn)
	  //
	  //    This aligns the stored state with what you recommend in the printed summary.

	  const PyramidResults* best = selectBestPassingConfiguration(allResults, outputStream);

	  mMetaStrategyPassed = (best != nullptr);

	  if (!mMetaStrategyPassed)
	    {
	      // No passer → keep legacy zeros for members
	      // (writeComprehensivePerformanceReport already reflects per-level outcomes)
	      return;
	    }

	  // 4) Update canonical members so downstream reads match the recommendation
	  mAnnualizedLowerBound = best->getAnnualizedLowerBound();
	  mRequiredReturn       = best->getRequiredReturn();
	}
      catch (const std::exception& e)
	{
	  outputStream << "[Meta] Error in unified meta-strategy backtesting: " << e.what() << "\n";
	  mMetaStrategyPassed = false;
	}
    }

    /**
     * @brief Factory method to build a standard PalMetaStrategy.
     *
     * Objective:
     * Combines multiple individual `PalStrategy` instances into a single
     * `PalMetaStrategy` container.
     *
     * Configuration:
     * - Sets the portfolio name to "Unified Meta Strategy".
     * - Configures `setSkipIfBothSidesFire(true)` to prevent simultaneous Long/Short
     * conflicts (neutralizing exposure rather than doubling it).
     *
     * Arguments:
     * survivingStrategies: The components to add to the portfolio.
     * baseSecurity:        The asset these strategies trade.
     *
     * Returns:
     * A shared pointer to the fully configured meta-strategy.
     */
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

    /**
     * @brief Factory method to build a PalMetaStrategy with custom execution options.
     *
     * Objective:
     * Similar to the standard factory, but allows injecting `StrategyOptions`.
     * This is primarily used to configure **Pyramiding** (e.g., allowing
     * multiple positions in the same direction).
     *
     * Arguments:
     * survivingStrategies: The components.
     * baseSecurity:        The asset.
     * strategyOptions:     Configuration object containing pyramiding limits
     * (e.g., `maxOpenPositions`).
     */
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

    /**
     * @brief Factory method for a Volatility-Filtered Meta-Strategy.
     *
     * Objective:
     * Creates a `PalMetaStrategy` wrapped with an `AdaptiveVolatilityPortfolioFilter`.
     * This is used specifically for Pyramid Level 4 ("Volatility Filter"), which
     * dynamically reduces exposure when market volatility (Simons HLC) exceeds
     * historical norms.
     *
     * Arguments:
     * survivingStrategies: The components.
     * baseSecurity:        The asset.
     * strategyOptions:     Execution options.
     *
     * Returns:
     * A shared pointer to the filtered strategy instance.
     */
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

    /**
     * @brief Defines the set of portfolio configurations to test.
     *
     * Objective:
     * Acts as the "Experiment Design" generator. It creates a list of scenarios
     * to stress-test the portfolio's scalability:
     * - Level 0: Base case (1 position max).
     * - Level 1: 2 positions max (1 add-on).
     * - Level 2: 3 positions max (2 add-ons).
     * - Level 3: 4 positions max (3 add-ons).
     *
     * (Optional #ifdefs exist for Volatility Filters and Breakeven Stops).
     *
     * Returns:
     * A vector of `PyramidConfiguration` objects, each defining a specific
     * backtest scenario.
     */
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

#ifdef ADDITIONAL_METASTRATEGIES
      // Pyramid Level 4: Adaptive Volatility Filter (no pyramiding)
      configs.emplace_back(4, "Volatility Filter",
                          StrategyOptions(false, 0, 8),
                          PyramidConfiguration::ADAPTIVE_VOLATILITY_FILTER);
      
      // Pyramid Level 5: Breakeven Stop (no pyramiding)
      configs.emplace_back(5, "Breakeven Stop",
                          StrategyOptions(false, 0, 8),
                          PyramidConfiguration::BREAKEVEN_STOP);
#endif
      
      return configs;
    }

    /**
     * @brief Executes the Selection-Aware Bootstrap (Gate #3).
     *
     * Objective:
     * Corrects for the "Lucky Survivor" bias. Since the components were pre-selected
     * based on performance, a standard bootstrap is biased upwards.
     * This method:
     * 1. Reconstructs the exact "meta-strategy construction process" inside
     * the bootstrap loop.
     * 2. Resamples component returns *before* aggregation.
     * 3. Verifies if the portfolio's edge persists even after accounting for
     * selection bias.
     *
     * Arguments:
     * survivingStrategies: The pool of components to resample.
     * backtestingDates:    Simulation window.
     * Lmeta:               Block length derived from the portfolio's serial dependence.
     * annualizationFactor: The effective 'K' factor for scaling returns.
     * bt:                  Backtester (used for trade frequency stats).
     * oosSpreadStats:      Used to set the hurdle bar.
     *
     * Returns:
     * True if the bias-corrected Lower Bound > Hurdle.
     */
    bool MetaStrategyAnalyzer::runSelectionAwareMetaGate(
    			 const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    			 std::shared_ptr<mkc_timeseries::Security<Num>> /* baseSecurity */,
    			 const mkc_timeseries::DateRange& oosBacktestingDates,
    			 mkc_timeseries::TimeFrame::Duration timeFrame,
    			 std::size_t Lmeta,
    			 double annualizationFactor,
    			 const mkc_timeseries::BackTester<Num>* bt,
    			 std::ostream& os,
    			 std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats
    			 ) const
    {
      using NumT   = Num;
      using Rng    = randutils::mt19937_rng;
      using PTime  = boost::posix_time::ptime;

      // ─────────────────────────────────────────────────────────────────────────────
      // 1) Gather (ptime, return) per-strategy and build the UNION timestamp index
      // ─────────────────────────────────────────────────────────────────────────────
      std::vector<std::vector<std::pair<PTime, NumT>>> series_with_dates;
      series_with_dates.reserve(survivingStrategies.size());

      std::map<PTime, std::size_t> union_index;               // ordered union → column index
      for (const auto& strat : survivingStrategies)
 {
   auto cloned = strat->cloneForBackTesting();
   auto single = mkc_timeseries::BackTesterFactory<NumT>::backTestStrategy(
    						  cloned, timeFrame, oosBacktestingDates);

	  auto ts = single->getAllHighResReturnsWithDates(cloned.get()); // vector<pair<ptime,Num>>
	  if (ts.size() >= 2)
	    {
	      for (const auto& pr : ts) union_index.emplace(pr.first, 0U);
	      series_with_dates.emplace_back(std::move(ts));
	    }
	}

      if (series_with_dates.empty())
	{
	  os << "      [MetaSel] Skipped (no component series available)\n";
	  return true; // non-penalizing skip
	}

      // Stamp contiguous indices 0..T-1 onto the union map
      {
	std::size_t col = 0;
	for (auto& kv : union_index) kv.second = col++;
      }
      const std::size_t T = union_index.size();

      // ─────────────────────────────────────────────────────────────────────────────
      // 2) Encode presence via parallel indicator rows; build dense (2*C)×T matrix
      //    Row 0..C-1   : values aligned to union (0 if missing at that bar)
      //    Row C..2C-1  : indicators (1 if present, else 0) aligned to union
      // ─────────────────────────────────────────────────────────────────────────────
      const std::size_t C = series_with_dates.size();
      std::vector<std::vector<NumT>> componentMatrix;
      componentMatrix.resize(2 * C, std::vector<NumT>(T, mkc_timeseries::DecimalConstants<NumT>::DecimalZero));

      for (std::size_t s = 0; s < C; ++s)
	{
	  auto& valuesRow = componentMatrix[s];
	  auto& indicRow  = componentMatrix[C + s];

	  for (const auto& [pt, r] : series_with_dates[s])
	    {
	      const auto it = union_index.find(pt);
	      if (it == union_index.end()) continue;
	      const std::size_t j = it->second;
	      valuesRow[j] = r;
	      indicRow [j] = mkc_timeseries::DecimalConstants<NumT>::DecimalOne; // 1 = present
	    }
	}

      // Safety: if union had < 2 bars after pruning, skip (can happen with pathological inputs)
      if (T < 2)
	{
	  os << "      [MetaSel] Skipped (insufficient union length)\n";
	  return true;
	}

      // ─────────────────────────────────────────────────────────────────────────────
      // 3) Configure selection-aware bootstrap (generic CL = 0.95) and builder
      // ─────────────────────────────────────────────────────────────────────────────
      const std::size_t outerB = 2000;
      const double cl          = mConfidenceLevel.getAsDouble(); // keep 0.95 for this gate
      const std::size_t Lmean  = Lmeta;
      const double ppy         = annualizationFactor;

      palvalidator::analysis::MetaSelectionBootstrap<NumT, Rng> msb(outerB, cl, Lmean, ppy);

      // Builder: for each bar t, average only components that are present at t:
      // meta[t] = sum_k values[k][t] / sum_k indicators[k][t], with k over 0..C-1.
      auto builder_date_aligned =
	[C](const std::vector<std::vector<NumT>>& mats) -> std::vector<NumT>
	{
	  if (mats.empty()) return {};
	  const std::size_t Tloc = mats[0].size();
	  if (Tloc < 2 || mats.size() < 2 * C) return {};

	  std::vector<NumT> meta;
	  meta.resize(Tloc, mkc_timeseries::DecimalConstants<NumT>::DecimalZero);

	  for (std::size_t t = 0; t < Tloc; ++t)
	    {
	      NumT num = mkc_timeseries::DecimalConstants<NumT>::DecimalZero;
	      NumT den = mkc_timeseries::DecimalConstants<NumT>::DecimalZero;

	      for (std::size_t k = 0; k < C; ++k)
		{
		  const NumT w = mats[C + k][t];  // indicator (0 or 1)
		  if (w != mkc_timeseries::DecimalConstants<NumT>::DecimalZero)
		    {
		      num += mats[k][t];            // value already zero when missing
		      den += w;                     // count present components
		    }
		}

	      meta[t] = (den != mkc_timeseries::DecimalConstants<NumT>::DecimalZero)
		? (num / den)
		: mkc_timeseries::DecimalConstants<NumT>::DecimalZero;
	    }
	  return meta;
	};

      Rng rng;
      auto msbRes = msb.run(componentMatrix, builder_date_aligned, rng); // <— Matrix is std::vector<std::vector<Num>>

      // ─────────────────────────────────────────────────────────────────────────────
      // 4) Hurdles and logging (unchanged)
      // ─────────────────────────────────────────────────────────────────────────────
      const auto H = makeCostStressHurdles<Num>(
						mHurdleCalculator, oosSpreadStats,
						Num(bt->getEstimatedAnnualizedTrades()),
						mEffectiveSlippageFloor);

      const bool passBase = (msbRes.lbAnnualized > H.baseHurdle);
      const bool pass1Qn  = (msbRes.lbAnnualized > H.h_1q);
      const bool pass     = (passBase && pass1Qn);

      os << "      [MetaSel] Selection-aware bootstrap (date-aligned): "
	 << "Ann GM LB=" << (100.0 * num::to_double(msbRes.lbAnnualized)) << "% "
	 << (pass ? "(PASS)" : "(FAIL)")
	 << " vs Base=" << (100.0 * num::to_double(H.baseHurdle)) << "%, "
	 << "+1·Qn=" << (100.0 * num::to_double(H.h_1q)) << "% "
	 << "@ CL=" << (100.0 * msbRes.cl) << "%, B=" << msbRes.B
	 << ", L~" << Lmean << "\n";

      return pass;
    }

    
    /**
     * @brief Executes the Regime Mix Stress Test (Gate #5).
     *
     * Objective:
     * Ensures the portfolio is "All-Weather" and robust to volatility changes.
     * It tests the portfolio against specific market regimes:
     *
     * 1. **Data Alignment:** Maps irregular portfolio returns to the daily
     * volatility regime of the base asset (Low, Mid, High).
     *
     * 2. **Mix Stress:** Resamples returns weighted by regime scenarios
     * (e.g., "Low Volatility Favored", "High Volatility Favored").
     *
     * 3. **Gating:** Fails the portfolio if it cannot maintain a positive
     * expectancy in ANY reasonable market regime.
     *
     * Arguments:
     * bt:                  Backtester (source of high-res portfolio returns).
     * baseSecurity:        Source of volatility data (Close prices).
     * annualizationFactor: For scaling the stress-test LBs.
     * requiredReturn:      The cost hurdle to clear.
     * inSampleDates:       Used to compute the asset's "Long Run" baseline regime mix.
     *
     * Returns:
     * A `RegimeMixResult` containing pass/fail status and a list of any failing mixes.
     */
    MetaStrategyAnalyzer::RegimeMixResult
    MetaStrategyAnalyzer::runRegimeMixGate(
        const std::shared_ptr<BackTester<Num>>& bt,
        const std::shared_ptr<Security<Num>>& baseSecurity,
        const DateRange& oosBacktestingDates,
        double annualizationFactor,
        const Num& requiredReturn,
        std::size_t blockLength,
        std::ostream& outputStream,
        const DateRange& inSampleDates) const
    {
      /**
       * @brief The "All-Weather" Robustness Validator.
       *
       * @section summary Executive Summary
       * This stage transforms validation from a simple "performance check" into a robustness proof.
       * It acts as an "All-Weather" stress test: instead of asking "How does this strategy perform
       * on average?" (standard bootstrap), it asks "How does this strategy perform if the future
       * market environment is structurally different from the past?"
       *
       * It is arguably the most rigorous filter in the pipeline because it destroys "lucky timing"
       * bias. A strategy cannot hide poor performance in High Volatility periods simply because
       * those periods were rare in the backtest history.
       *
       * @section technical Technical Implementation
       * The logic relies on **Regime-Conditional Resampling**:
       *
       * 1. **Dynamic Labeling (VolTercileLabeler):**
       * Defines regimes using **Relative Volatility** (top 33% of the asset's own history)
       * rather than arbitrary thresholds (e.g., VIX > 20). This makes the test robust across
       * different asset classes.
       *
       * 2. **Sparse-to-Dense Alignment (BarAlignedSeries):**
       * Maps continuous market regimes to sparse, irregular trade timestamps. This ensures
       * that a trade executed on a Tuesday is strictly evaluated against Tuesday's specific
       * volatility regime.
       *
       * 3. **Weighted Bootstrapping (RegimeMixStressRunner):**
       * Creates synthetic equity curves by forcing the bootstrapper to pick trades from
       * specific regimes based on probability weights (e.g., forcing 30% of trades to
       * come from the "High Vol" bucket).
       *
       * @section mixes Tested Scenarios
       * The stage evaluates five distinct market textures:
       * - **Equal (0.33, 0.33, 0.33):** Removes historical frequency bias.
       * - **MidVolFav (0.25, 0.50, 0.25):** Simulates a mean-reverting, grinding market.
       * - **LowVolFav (0.50, 0.35, 0.15):** Simulates a calm, bull-market environment.
       * - **EvenMinusHV (0.35, 0.35, 0.30):** A balanced view capping extreme volatility exposure.
       * - **LongRun:** Uses the actual historical distribution as a baseline reality check.
       *
       * @section value Strategic Value
       * - **Detects "Hidden Beta":** Exposes strategies that hold implicit "Short Volatility"
       * positions by failing them in Equal or High Vol weighted mixes.
       * - **Validates Portfolio Effect:** Proves that diversification is structural, allowing
       * the portfolio to pass all regimes even if individual components are specialists.
       * - **Prevents Curve Fitting:** Makes it mathematically difficult to overfit a strategy
       * to work in three different volatility regimes simultaneously unless the edge is genuine.
       */

      using mkc_timeseries::FilterTimeSeries;
      using mkc_timeseries::RocSeries;
      using palvalidator::analysis::BarAlignedSeries;
      using palvalidator::analysis::RegimeMix;
      using palvalidator::analysis::RegimeMixConfig;
      using palvalidator::analysis::RegimeMixStressRunner;
      using palvalidator::analysis::VolTercileLabeler;
      using palvalidator::filtering::regime_mix_utils::computeLongRunMixWeights;
      using palvalidator::filtering::regime_mix_utils::adaptMixesToPresentRegimes;

      constexpr std::size_t kRegimeVolWindow = 20;

      outputStream << "\n      [Meta Regime Mix] Starting regime mix stress testing...\n";

      // Step A: Data Preparation & Alignment
      // Get meta-strategy returns with dates from the closed position history
      const auto& closedHistory = bt->getClosedPositionHistory();
      auto metaReturnsWithDates = closedHistory.getHighResBarReturnsWithDates();
      
      if (metaReturnsWithDates.size() < 2)
      {
        outputStream << "      [Meta Regime Mix] Skipped (insufficient returns with dates)\n";
        return RegimeMixResult(true, 0.0, {}); // Non-gating skip
      }

      // Extract just the returns for the regime mix runners
      std::vector<Num> metaReturns;
      metaReturns.reserve(metaReturnsWithDates.size());
      for (const auto& [date, ret] : metaReturnsWithDates)
      {
        metaReturns.push_back(ret);
      }

      // Build OOS close series for regime labeling
      auto oosInstrumentTS = FilterTimeSeries(*baseSecurity->getTimeSeries(), oosBacktestingDates);
      const auto& oosClose = oosInstrumentTS.CloseTimeSeries();

      // Calculate regime labels for the base security
      std::vector<Num> oosCloseReturns;
      const auto entries = oosClose.getEntriesCopy();
      if (entries.size() < 2)
      {
        outputStream << "      [Meta Regime Mix] Skipped (insufficient OOS close data)\n";
        return RegimeMixResult(true, 0.0, {}); // Non-gating skip
      }

      oosCloseReturns.reserve(entries.size() - 1);
      for (std::size_t i = 1; i < entries.size(); ++i)
      {
        const Num c0 = entries[i - 1].getValue();
        const Num c1 = entries[i].getValue();
        if (c0 == Num(0))
        {
          outputStream << "      [Meta Regime Mix] Skipped (zero close price encountered)\n";
          return RegimeMixResult(true, 0.0, {}); // Non-gating skip
        }
        oosCloseReturns.push_back((c1 - c0) / c0);
      }

      if (oosCloseReturns.size() < kRegimeVolWindow + 2)
      {
        outputStream << "      [Meta Regime Mix] Skipped (insufficient data for volatility window)\n";
        return RegimeMixResult(true, 0.0, {}); // Non-gating skip
      }

      // Label the OOS bars by volatility terciles
      VolTercileLabeler<Num> labeler(kRegimeVolWindow);
      std::vector<int> oosBarLabels = labeler.computeLabels(oosCloseReturns);

      // Build timestamp -> label map
      std::map<boost::posix_time::ptime, int> dateToLabel;
      for (std::size_t i = 1; i < entries.size() && (i - 1) < oosBarLabels.size(); ++i)
      {
        dateToLabel[entries[i].getDateTime()] = oosBarLabels[i - 1];
      }

      // Align meta-strategy returns to regime labels
      std::vector<int> metaLabels;
      metaLabels.reserve(metaReturnsWithDates.size());
      for (const auto& [date, ret] : metaReturnsWithDates)
      {
        auto it = dateToLabel.find(date);
        if (it != dateToLabel.end())
        {
          metaLabels.push_back(it->second);
        }
        else
        {
          // If we can't find the exact date, use the closest previous label
          // This handles cases where meta returns might be on different timestamps
          auto lower = dateToLabel.lower_bound(date);
          if (lower != dateToLabel.begin())
          {
            --lower;
            metaLabels.push_back(lower->second);
          }
          else if (!dateToLabel.empty())
          {
            metaLabels.push_back(dateToLabel.begin()->second);
          }
          else
          {
            metaLabels.push_back(1); // Default to mid volatility
          }
        }
      }

      if (metaLabels.size() != metaReturns.size())
      {
        outputStream << "      [Meta Regime Mix] Skipped (label/return size mismatch: "
                     << metaLabels.size() << " vs " << metaReturns.size() << ")\n";
        return RegimeMixResult(true, 0.0, {}); // Non-gating skip
      }

      // Step B: Define the Mixes
      std::vector<RegimeMix> mixes;

      // Equal: neutral benchmark
      mixes.emplace_back("Equal(0.33,0.33,0.33)",
                         std::vector<double>{1.0/3.0, 1.0/3.0, 1.0/3.0});
      
      // MidVolFav: overweight middle regime
      mixes.emplace_back("MidVolFav(0.25,0.50,0.25)",
                         std::vector<double>{0.25, 0.50, 0.25});
      
      // LowVolFav: stronger tilt to low-vol
      mixes.emplace_back("LowVolFav(0.50,0.35,0.15)",
                         std::vector<double>{0.50, 0.35, 0.15});

      // EvenMinusHV: evenly tilted away from HighVol
      mixes.emplace_back("EvenMinusHV(0.35,0.35,0.30)",
                         std::vector<double>{0.35, 0.35, 0.30});

      // HighVolFav: Stress test for crisis periods / regime shifts
      // 50% High Volatility, 30% Mid, 20% Low
      mixes.emplace_back("HighVolFav(0.20,0.30,0.50)",
			 std::vector<double>{0.20, 0.30, 0.50});

      // LongRun: Calculate from in-sample data
      auto inSampleTS = FilterTimeSeries(*baseSecurity->getTimeSeries(), inSampleDates);
      auto insampleROC = RocSeries(inSampleTS.CloseTimeSeries(), /*period=*/1);
      auto baselineRoc = insampleROC.getTimeSeriesAsVector();

      if (!baselineRoc.empty())
      {
        auto clip_and_normalize = [](std::vector<double> w, double floor = 0.01) {
          for (auto& v : w)
            v = std::max(v, floor);
          double s = 0.0;
          for (double v : w)
            s += v;
          if (s <= 0.0)
            return std::vector<double>{1.0/3.0, 1.0/3.0, 1.0/3.0};
          for (auto& v : w)
            v /= s;
          return w;
        };

        std::vector<double> w = computeLongRunMixWeights(
          baselineRoc, kRegimeVolWindow, /*shrinkToEqual=*/0.25);
        
        w = clip_and_normalize(std::move(w), /*floor=*/0.01);

        std::ostringstream name;
        name.setf(std::ios::fixed); name << std::setprecision(2);
        name << "LongRun(" << w[0] << "," << w[1] << "," << w[2] << ")";
        
        mixes.emplace_back(name.str(), w);

        outputStream << "      [Meta Regime Mix] LongRun weights (shrunk 25%, floored 1%): ("
                     << std::fixed << std::setprecision(2)
                     << w[0] << ", " << w[1] << ", " << w[2] << ")\n";
      }

      // Step C: Adapt mixes to present regimes
      std::vector<int> compactLabels;
      std::vector<RegimeMix> adaptedMixes;

      if (!adaptMixesToPresentRegimes(metaLabels, mixes, compactLabels, adaptedMixes, outputStream))
      {
        return RegimeMixResult(true, 0.0, {}); // Non-gating skip
      }

      // Step D: Execute the runners
      const double mixPassFrac = 0.50;
      const std::size_t minBarsPerRegime = static_cast<std::size_t>(std::max<std::size_t>(2, blockLength + 5));
      RegimeMixConfig cfg(adaptedMixes, mixPassFrac, minBarsPerRegime);

      using NumT = Num;
      using Rng  = randutils::mt19937_rng;

      using RunnerStationary =
        RegimeMixStressRunner<NumT, Rng, palvalidator::resampling::RegimeMixStationaryResampler>;

      using RunnerFixed =
        RegimeMixStressRunner<NumT, Rng, palvalidator::resampling::RegimeMixBlockResampler>;

      palvalidator::filtering::ValidationPolicy policy(requiredReturn);

      RunnerStationary runnerStat(cfg, blockLength, mNumResamples,
                                   mConfidenceLevel.getAsDouble(),
                                   annualizationFactor, policy);

      auto resStat = runnerStat.run(metaReturns, compactLabels, outputStream);

      RunnerFixed runnerFixed(cfg, blockLength, mNumResamples,
                              mConfidenceLevel.getAsDouble(),
                              annualizationFactor, policy);

      auto resFixed = runnerFixed.run(metaReturns, compactLabels, outputStream);

      // Step E: Gating Logic
      const bool passStat  = resStat.overallPass();
      const bool passFixed = resFixed.overallPass();

      // Compute median of stationary annualized LBs across mixes (bps above hurdle)
      auto stationaryMedianOverHurdle_bps = [&]() -> double
      {
        const auto& perMix = resStat.perMix();
        if (perMix.empty()) return -1e9;
        std::vector<NumT> lbs;
        lbs.reserve(perMix.size());
        for (const auto& d : perMix) lbs.push_back(d.annualizedLowerBound());
        std::sort(lbs.begin(), lbs.end(), [](const NumT& a, const NumT& b){ return a < b; });
        const NumT medianLB = lbs[lbs.size() / 2];

        const double lb = num::to_double(medianLB);
        const double hurdleDec = requiredReturn.getAsDouble();
        return 10000.0 * (lb - hurdleDec);
      }();

      const double margin_bps = 50.0; // 50 bps forgiveness threshold
      const bool strongStatPass = passStat && (stationaryMedianOverHurdle_bps >= margin_bps);

      const bool regimeMixPass = (passStat && passFixed) || (passStat && !passFixed && strongStatPass);

      // Collect failing mixes and find minimum LB
      std::vector<std::string> failingMixes;
      double minAnnualizedLB = std::numeric_limits<double>::max();

      for (const auto& mx : resStat.perMix())
      {
        const double lb = num::to_double(mx.annualizedLowerBound());
        minAnnualizedLB = std::min(minAnnualizedLB, lb);
        if (!mx.pass())
        {
          failingMixes.push_back(mx.mixName());
        }
      }

      outputStream << "      [Meta Regime Mix] Gate=AND (+forgiveness "
                   << margin_bps << "bps): stationary=" << (passStat ? "PASS" : "FAIL")
                   << " fixed-L=" << (passFixed ? "PASS" : "FAIL")
                   << " | stationary median over hurdle = " << stationaryMedianOverHurdle_bps << " bps\n";

      if (!regimeMixPass)
      {
        outputStream << "      [Meta Regime Mix] ✗ FAIL (AND gate).";
        if (passStat && !passFixed && !strongStatPass) outputStream << " Reason: fixed-L veto.";
        if (!passStat && passFixed)                    outputStream << " Reason: stationary veto.";
        if (!passStat && !passFixed)                   outputStream << " Reason: both failed.";
        outputStream << "\n";

        if (!failingMixes.empty())
        {
          outputStream << "      Failing mixes (stationary): ";
          for (std::size_t i = 0; i < failingMixes.size(); ++i)
            outputStream << (i ? ", " : "") << failingMixes[i];
          outputStream << "\n";
        }
      }
      else
      {
        outputStream << "      [Meta Regime Mix] ✓ PASS\n";
      }

      return RegimeMixResult(regimeMixPass, minAnnualizedLB, failingMixes);
    }

    std::optional<Num> MetaStrategyAnalyzer::determineEffectiveSlippageFloor(
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        const std::optional<Num>& currentConfiguredSlippage,
        const std::optional<palvalidator::filtering::OOSSpreadStats>& oosSpreadStats,
        std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;

      // 1. Calculate Average Profit Target
      Num sumTargets = DecimalConstants<Num>::DecimalZero;
      size_t count = 0;
      
      for (const auto& strat : survivingStrategies) {
          if (strat && strat->getPalPattern()) {
              sumTargets = sumTargets + strat->getPalPattern()->getProfitTargetAsDecimal();
              count++;
          }
      }
      
      Num avgTarget = (count > 0) ? (sumTargets / Num(count)) : Num("0.01");

      // 2. Check Threshold (0.75% / 0.0075)
      // If avg target < 0.75%, we assume this is a Low Volatility / Micro-Target strategy.
      const bool isLowVolStrategy = (avgTarget < Num("0.75"));
      
      // 3. Determine Floor
      if (isLowVolStrategy) 
      {
          // Case A: We have actual spread stats (Preferred)
          if (oosSpreadStats.has_value()) 
          {
              outputStream << "      [Auto-Tune] Detected Micro-Target Strategy (Avg Target: " 
                           << (avgTarget * DecimalConstants<Num>::DecimalOneHundred) 
                           << "%).\n"
                           << "      [Auto-Tune] Policy: Removing 10bps fixed floor. Using actual OOS spread statistics (" 
                           << (oosSpreadStats->mean * DecimalConstants<Num>::DecimalOneHundred) << "%).\n";
              
              // Return 0.0. 
              // This causes makeCostStressHurdles to use std::max(0.0, actual_spread/2), 
              // effectively letting the actual spread drive the cost.
              return Num("0.0"); 
          }
          // Case B: No stats available (Fallback)
          else 
          {
              const Num lowVolFallback = Num("0.0002"); // 2 bps
              outputStream << "      [Auto-Tune] Detected Micro-Target Strategy (Avg Target: " 
                           << (avgTarget * DecimalConstants<Num>::DecimalOneHundred) 
                           << "%).\n"
                           << "      [Auto-Tune] Warning: No OOS spread stats available. Lowering floor to 2 bps ("
                           << (lowVolFallback * DecimalConstants<Num>::DecimalOneHundred) << "%).\n";
              return lowVolFallback;
          }
      }
      
      // Default: Return original configured value (usually 10 bps)
      return currentConfiguredSlippage;
    }

    /**
     * @brief Determines the number of time-slices (K) for the Multi-Split Gate.
     *
     * Objective:
     * Calculates a heuristic $K$ for the OOS consistency check (Gate #4).
     * It balances the need for granularity (more slices = better consistency check)
     * with statistical significance (too many slices = insufficient sample size $n$ per slice).
     *
     * Logic:
     * - Target $K=4$ for long datasets ($n \ge 160$).
     * - Target $K=3$ for shorter datasets.
     * - Clamps $K$ so that each slice has at least `kMinSliceLen` bars.
     *
     * Arguments:
     * n:     Total number of returns.
     * Lmeta: The calculated block length (used to determine minimum slice length).
     *
     * Returns:
     * The integer $K$ (slice count).
     */
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

    /**
     * @brief Executes the backtest simulation for a specific pyramid configuration.
     *
     * Objective:
     * Acts as the execution engine for `analyzeSinglePyramidLevel`. It handles
     * the logic branching for different configuration types:
     * - **Standard:** Runs a normal backtest with the specified pyramiding limits.
     * - **Adaptive Filter:** Wraps the strategy in a volatility filter before running.
     * - **Breakeven:** Performs an initial pass to tune exit parameters, adds a
     * breakeven stop, and re-runs the backtest.
     *
     * Arguments:
     * config:              The configuration (Level ID, Type, Options).
     * survivingStrategies: Components.
     * baseSecurity:        The asset.
     * backtestingDates:    Simulation window.
     * timeFrame:           Bar duration.
     * outputStream:        Logging stream.
     *
     * Returns:
     * A `PyramidBacktestResult` containing the `BackTester` instance and the raw returns.
     */
    MetaStrategyAnalyzer::PyramidBacktestResult
    MetaStrategyAnalyzer::runPyramidBacktest(
        const PyramidConfiguration& config,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& oosBacktestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream) const
    {
      std::shared_ptr<BackTester<Num>> bt;
      std::vector<Num> metaReturns;

      if (config.getFilterType() == PyramidConfiguration::ADAPTIVE_VOLATILITY_FILTER)
      {
        auto filteredStrategy = createMetaStrategyWithAdaptiveFilter(
            survivingStrategies, baseSecurity, config.getStrategyOptions());
        bt = executeBacktestingWithFilter(filteredStrategy, timeFrame, oosBacktestingDates);
        metaReturns = bt->getAllHighResReturns(filteredStrategy.get());
      }
      else if (config.getFilterType() == PyramidConfiguration::BREAKEVEN_STOP)
      {
        auto initialStrategy = createMetaStrategy(survivingStrategies, baseSecurity, config.getStrategyOptions());
        auto initialBt = executeBacktesting(initialStrategy, timeFrame, oosBacktestingDates);

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

            bt = executeBacktesting(breakevenStrategy, timeFrame, oosBacktestingDates);
            metaReturns = bt->getAllHighResReturns(breakevenStrategy.get());
          }
          catch (const std::exception& e)
          {
            outputStream << "      Warning: Exit policy tuning failed: " << e.what()
                        << ". Using standard strategy without breakeven stop.\n";
            auto fallback = createMetaStrategy(
                survivingStrategies, baseSecurity, config.getStrategyOptions());
            bt = executeBacktesting(fallback, timeFrame, oosBacktestingDates);
            metaReturns = bt->getAllHighResReturns(fallback.get());
          }
        }
        else
        {
          outputStream << "      No closed positions available for exit policy tuning. Using standard strategy.\n";
          auto metaStrategy = createMetaStrategy(
              survivingStrategies, baseSecurity, config.getStrategyOptions());
          bt = executeBacktesting(metaStrategy, timeFrame, oosBacktestingDates);
          metaReturns = bt->getAllHighResReturns(metaStrategy.get());
        }
      }
      else
      {
        auto metaStrategy = createMetaStrategy(
            survivingStrategies, baseSecurity, config.getStrategyOptions());
        bt = executeBacktesting(metaStrategy, timeFrame, oosBacktestingDates);
        metaReturns = bt->getAllHighResReturns(metaStrategy.get());
      }

      return PyramidBacktestResult(bt, metaReturns);
    }

    /**
     * @brief The Central Validator: Runs all 5 statistical gates for a single pyramid level.
     *
     * Objective:
     * This is the "Gatekeeper". It orchestrates the sequential validation checks:
     * 1. **Metrics Calculation:** Computes block length (L), K-factor, and participation.
     * 2. **Regular Gate:** Standard BCa Bootstrap vs. Cost Hurdle.
     * 3. **Selection-Aware Gate:** Checks for selection bias.
     * 4. **Multi-Split Gate:** Checks for OOS consistency (time slicing).
     * 5. **Regime Mix Gate:** Checks for volatility robustness.
     *
     * Arguments:
     * metaReturns:         The raw return series of the portfolio.
     * bt:                  The backtester instance.
     * survivingStrategies: The components (needed for Selection-Aware gate).
     * oosSpreadStats:      Realized spread data for cost calibration.
     * inSampleDates:       Passed down to Regime Mix gate.
     *
     * Returns:
     * A `PyramidGateResults` object summarizing the outcome of all 5 gates.
     */
    MetaStrategyAnalyzer::PyramidGateResults
    MetaStrategyAnalyzer::runPyramidValidationGates(
        const std::vector<Num>& metaReturns,
        std::shared_ptr<BackTester<Num>> bt,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& oosBacktestingDates,
        TimeFrame::Duration timeFrame,
        std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats,
        std::ostream& outputStream,
        const DateRange& inSampleDates) const
    {
      using mkc_timeseries::DecimalConstants;

      // Calculate metrics used by all gates
      const unsigned int metaMedianHold = bt->getClosedPositionHistory().getMedianHoldingPeriod();
      const std::size_t Lmeta = calculateBlockLengthAdaptive(metaReturns, metaMedianHold, outputStream);
      const Num metaAnnualizedTrades = Num(bt->getEstimatedAnnualizedTrades());
      const double annualizationFactor = calculateAnnualizationFactor(timeFrame, baseSecurity);

      const double Keff = mkc_timeseries::computeEffectiveAnnualizationFactor(metaAnnualizedTrades,
                                                                               metaMedianHold,
                                                                               annualizationFactor,
                                                                               &outputStream);

      const double p = (annualizationFactor > 0.0) ? (Keff / annualizationFactor) : 1.0;
      if (p > 1.2 || p < 0.01) {
        outputStream << "      [Meta] Warning: participation p=" << p
                    << " looks unusual; verify estimated annualized trades / median hold.\n";
      }

      // Regular (whole-sample) BCa gate
      calculatePerPeriodEstimates(metaReturns, outputStream);
      const auto bootstrapResults = performBootstrapAnalysis(metaReturns, Keff, Lmeta, outputStream);

      // Build calibrated + Qn-stressed cost hurdles
      
      const auto H = makeCostStressHurdles<Num>(mHurdleCalculator,
                                                oosSpreadStats,
                                                metaAnnualizedTrades,
                                                mEffectiveSlippageFloor);
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
      const bool pass1Qn = (bootstrapResults.lbGeoAnn > H.h_1q);
      const bool regularBootstrapPass = (passBase && pass1Qn);

      // Selection-aware gate
      const bool passMetaSelectionAware =
        runSelectionAwareMetaGate(survivingStrategies,
                                  baseSecurity,
                                  oosBacktestingDates,
                                  timeFrame,
                                  Lmeta,
                                  Keff,
                                  bt.get(),
                                  outputStream,
                                  oosSpreadStats);

      // Multi-split OOS gate
      const std::size_t K = chooseInitialSliceCount(metaReturns.size(), Lmeta);
      outputStream << "      Multi-split bootstrap: K=" << K
                  << ", L=" << Lmeta << ", n=" << metaReturns.size() << "\n";

      const auto ms = runMultiSplitGate(
          metaReturns,
          K,
          Lmeta,
          Keff,
          baseSecurity.get(),
          timeFrame,
          bt.get(),
          outputStream,
          oosSpreadStats);

      // Non-penalizing when not applied (too short to slice)
      const bool multiSplitPass = (!ms.applied) || ms.pass;

      // Regime Mix Gate
      outputStream << "\n";
      const auto regimeResult = runRegimeMixGate(
          bt,
          baseSecurity,
          oosBacktestingDates,
          Keff,
          H.baseHurdle,
          Lmeta,
          outputStream,
          inSampleDates
      );

      return PyramidGateResults(regularBootstrapPass, multiSplitPass, passMetaSelectionAware,
                                bootstrapResults, H, Keff, Lmeta, metaAnnualizedTrades, regimeResult);
    }

    /**
     * @brief Aggregates post-validation risk metrics.
     *
     * Objective:
     * Once a strategy passes validation, this method calculates the "Safety" metrics
     * used for monitoring and sizing:
     * 1. **Future Monthly Bound:** The VaR-like lower bound for a single month.
     * 2. **Losing Streak Bound:** The statistical upper bound for consecutive losses.
     * 3. **Drawdown Analysis:** The BCa confidence intervals for Max Drawdown.
     *
     * Arguments:
     * metaReturns:   Portfolio return series.
     * closedHistory: Trade list (for streak analysis).
     * lMeta:         Block length.
     * outputStream:  Logging stream.
     *
     * Returns:
     * A `PyramidRiskResults` object containing the computed risk metrics.
     */
    MetaStrategyAnalyzer::PyramidRiskResults
    MetaStrategyAnalyzer::runPyramidRiskAnalysis(
        const std::vector<Num>& metaReturns,
        const ClosedPositionHistory<Num>& closedHistory,
        std::size_t lMeta,
        std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;

      // Future Returns Bound Analysis
      outputStream << "\n";
      Num futureReturnsLowerBoundPct = performFutureReturnsBoundAnalysis(closedHistory, outputStream);

      // Max consecutive losses (trade-level) bound
      const auto [observedLosingStreak, losingStreakUpperBound] =
        computeLosingStreakBound(closedHistory, outputStream);

      // Drawdown analysis
      const uint32_t numTrades = closedHistory.getNumPositions();
      auto drawdownResults = performDrawdownAnalysisForPyramid(metaReturns, numTrades, lMeta);

      return PyramidRiskResults(drawdownResults, futureReturnsLowerBoundPct,
                                observedLosingStreak, losingStreakUpperBound);
    }

    /**
     * @brief Logging helper for the validation gates.
     *
     * Objective:
     * Formats and prints the pass/fail status of the 5 gates (Regular, Multi-Split,
     * MetaSel, RegimeMix) and the primary performance metrics to the console.
     *
     * Arguments:
     * gates:        Results from `runPyramidValidationGates`.
     * risk:         Results from `runPyramidRiskAnalysis`.
     * pyramidLevel: Integer ID of the current level (0, 1, etc.).
     * outputStream: Logging stream.
     */
    void MetaStrategyAnalyzer::logPyramidValidationResults(
        const PyramidGateResults& gates,
        const PyramidRiskResults& risk,
        unsigned int pyramidLevel,
        std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;

      outputStream << "\n";
      outputStream << "      Annualized Lower Bound (GeoMean, compounded): "
                  << (gates.getBootstrapResults().lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                  << "      Annualized Lower Bound (Mean, compounded):    "
                  << (gates.getBootstrapResults().lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                  << "      Required Return (max(cost,riskfree)): "
                  << (gates.getHurdles().baseHurdle * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                  << "      Max Consecutive Losing Trades (Upper Bound): "
                  << risk.getLosingStreakUpperBound() << " trades\n";
      outputStream << "      Gates → Regular: " << (gates.regularBootstrapPassed() ? "PASS" : "FAIL")
                  << ", Multi-split: " << (gates.multiSplitPassed() ? "PASS" : "FAIL")
                  << ", MetaSel: " << (gates.passMetaSelectionAware() ? "PASS" : "FAIL")
                  << ", RegimeMix: " << (gates.regimeMixPassed() ? "PASS" : "FAIL")
                  << "\n";
      
      // Show regime mix details if it failed
      const auto& regimeMixResult = gates.getRegimeMixResult();
      if (!regimeMixResult.passed && !regimeMixResult.failingMixes.empty())
      {
        outputStream << "      Regime Mix Failing Scenarios: ";
        for (std::size_t i = 0; i < regimeMixResult.failingMixes.size(); ++i)
          outputStream << (i ? ", " : "") << regimeMixResult.failingMixes[i];
        outputStream << "\n";
        outputStream << "      Minimum Annualized LB across mixes: "
                    << (regimeMixResult.minAnnualizedLB * 100.0) << "%\n";
      }
      outputStream << "\n";
      
      if (gates.allGatesPassed())
        outputStream << "      RESULT: ✓ Pyramid Level " << pyramidLevel << " PASSES\n";
      else
        outputStream << "      RESULT: ✗ Pyramid Level " << pyramidLevel << " FAILS\n";
    }

    void MetaStrategyAnalyzer::logDrawdownAnalysis(
        const DrawdownResults& drawdownResults,
        uint32_t numTrades,
        std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;

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
    }

    /**
     * @brief Worker method: Runs the full analysis lifecycle for ONE configuration.
     *
     * Objective:
     * Encapsulates the entire workflow for a specific pyramid level (e.g., Level 1):
     * 1. **Backtest:** Runs the strategy with the specific config options.
     * 2. **Validate:** Calls `runPyramidValidationGates` to test robustness.
     * 3. **Risk:** Calls `runPyramidRiskAnalysis` (Drawdowns, Future Bounds).
     * 4. **Log:** Writes detailed logs for this specific level.
     * 5. **Package:** Returns a `PyramidResults` object for comparison.
     *
     * Arguments:
     * config:              The specific setup (Level 0, 1, 2, etc.).
     * survivingStrategies: The components.
     * outputStream:        For real-time logging.
     *
     * Returns:
     * A `PyramidResults` object containing all metrics and pass/fail status.
     */
    PyramidResults
    MetaStrategyAnalyzer::analyzeSinglePyramidLevel(
        const PyramidConfiguration& config,
        const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
        std::shared_ptr<Security<Num>> baseSecurity,
        const DateRange& oosBacktestingDates,
        TimeFrame::Duration timeFrame,
        std::ostream& outputStream,
        std::optional<palvalidator::filtering::OOSSpreadStats> oosSpreadStats,
        const DateRange& inSampleDates) const
    {
      using mkc_timeseries::DecimalConstants;

      outputStream << "\n[Meta] Pyramid Level " << config.getPyramidLevel()
                  << " (" << config.getDescription() << "):\n";
      
      // --- Step 1: Run Backtest ---
      auto btResult = runPyramidBacktest(config, survivingStrategies, baseSecurity,
                                        oosBacktestingDates, timeFrame, outputStream);
      
      if (btResult.getMetaReturns().size() < 2U)
      {
        outputStream << "      Not enough data from pyramid level " << config.getPyramidLevel() << ".\n";
        DrawdownResults emptyDrawdown;
        return PyramidResults(config.getPyramidLevel(), config.getDescription(),
                              DecimalConstants<Num>::DecimalZero, DecimalConstants<Num>::DecimalZero,
                              false, DecimalConstants<Num>::DecimalZero, 0,
                              btResult.getBacktester(), emptyDrawdown,
                              DecimalConstants<Num>::DecimalZero, 0, 0);
      }
      
      // --- Step 2: Run All Validation Gates ---
      auto gates = runPyramidValidationGates(
          btResult.getMetaReturns(),
          btResult.getBacktester(),
          survivingStrategies,
          baseSecurity,
          oosBacktestingDates,
          timeFrame,
          oosSpreadStats,
          outputStream,
          inSampleDates
      );
      
      // --- Step 3: Run All Risk Analyses ---
      auto risk = runPyramidRiskAnalysis(
          btResult.getMetaReturns(),
          btResult.getClosedPositionHistory(),
          gates.getLMeta(),
          outputStream
      );
      
      // --- Step 4: Log Results ---
      logPyramidValidationResults(gates, risk, config.getPyramidLevel(), outputStream);
      logDrawdownAnalysis(risk.getDrawdownResults(),
                         btResult.getClosedPositionHistory().getNumPositions(),
                         outputStream);

      if (gates.allGatesPassed()) 
	{
	  // Only run sensitivity check if the strategy is a passer
	  performBlockLengthSensitivity(btResult.getMetaReturns(), 
					gates.getLMeta(), 
					gates.getKeff(), 
					gates.getHurdles().baseHurdle, // or finalRequiredReturn
					outputStream
					);
	}      
      // --- Step 5: Return Final Aggregated Result ---
      return PyramidResults(
          config.getPyramidLevel(),
          config.getDescription(),
          gates.getBootstrapResults().lbGeoAnn,
          gates.getHurdles().baseHurdle,
          gates.allGatesPassed(),
          gates.getMetaAnnualizedTrades(),
          btResult.getClosedPositionHistory().getNumPositions(),
          btResult.getBacktester(),
          risk.getDrawdownResults(),
          risk.getFutureReturnsLowerBoundPct(),
          risk.getObservedLosingStreak(),
          risk.getLosingStreakUpperBound()
      );
    }

    /**
     * @brief Estimates the statistical upper bound for consecutive losing trades.
     *
     * Objective:
     * Uses a specialized bootstrap (`MetaLosingStreakBootstrapBound`) to estimate
     * the "Worst Case" losing streak.
     * - Instead of just reporting the observed streak (e.g., 3 losses in a row),
     * it resamples the trade sequence to find the likely maximum streak
     * at 95% confidence (e.g., "We are 95% sure it won't exceed 6 losses").
     *
     * Arguments:
     * cph: The history of closed positions (W/L sequence).
     * os:  Output stream for logging.
     *
     * Returns:
     * A pair {Observed Streak, Bootstrap Upper Bound}.
     */
    std::pair<int,int>
    MetaStrategyAnalyzer::computeLosingStreakBound(const ClosedPositionHistory<Num>& cph,
    		   std::ostream& os) const
    {
      using mkc_timeseries::MetaLosingStreakBootstrapBound;
      using mkc_timeseries::StationaryTradeBlockSampler;
      using ExecT = concurrency::ThreadPoolExecutor<>;
      using RngT  = randutils::mt19937_rng;

      ExecT exec;   // thread pool executor
      RngT  rng;    // high-quality auto-seeded

      typename MetaLosingStreakBootstrapBound<Num,
    	      StationaryTradeBlockSampler<Num>,
    	      ExecT,
    	      RngT>::Options opts;
      opts.B               = mNumResamples;                 // align with your bootstrap budget
      opts.alpha           = 1.0 - mConfidenceLevel.getAsDouble();// use same CL as rest of analysis
      opts.sampleFraction  = 1.0;                           // full m-out-of-n by default
      opts.treatZeroAsLoss = false;

      MetaLosingStreakBootstrapBound<Num,
         StationaryTradeBlockSampler<Num>,
         ExecT,
         RngT> bounder(exec, rng, opts);

      const int observed = bounder.observedStreak(cph);
      int upper    = bounder.computeUpperBound(cph);

      // Safety belt: empirical upper bound should never be < observed
      if (upper < observed)
	upper = observed;

      os << "      Losing-streak bound @ " << (mConfidenceLevel * 100)
  << "% CL: observed=" << observed
  << ", upper bound=" << upper << " (trades)\n";

      return {observed, upper};
    }
    
    std::shared_ptr<BackTester<Num>> MetaStrategyAnalyzer::executeBacktesting(
        std::shared_ptr<PalMetaStrategy<Num>> metaStrategy,
        TimeFrame::Duration timeFrame,
        const DateRange& oosBacktestingDates) const
    {
      return BackTesterFactory<Num>::backTestStrategy(metaStrategy, timeFrame, oosBacktestingDates);
    }

    /**
     * @brief Estimates the "Worst Case" future monthly return (VaR-like metric).
     *
     * Objective:
     * Answers the question: "How bad could a single month get?"
     * 1. Aggregates trade-level returns into **Monthly Returns**.
     * 2. Calculates an adaptive block length (L) for monthly dependence.
     * 3. Runs a BCa Bootstrap to estimate the **5th Percentile** (Left Tail)
     * of the monthly return distribution.
     * 4. Returns the lower bound of that 5th percentile estimate.
     *
     * This serves as a "Monitoring Bound"—if live monthly returns dip below this,
     * the strategy is likely broken.
     *
     * Returns:
     * The lower bound % (as a decimal).
     */
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

      // 2) Pick block length (adaptive: median-hold for very short; n^(1/3) for medium; ACF for long)
      const unsigned int medianHold = closedPositionHistory.getMedianHoldingPeriod();
      const std::size_t blockLength = calculateBlockLengthAdaptive(
								   monthly,          // returns
								   medianHold,       // median holding period (bars → months here)
								   outputStream,     // logs which path was chosen
								   100,              // min size for ACF on monthly
								   12,               // maxACFLag for monthly
								   2,                // min L from ACF
								   12                 // max L from ACF (monthly)
								   );

      try
	{
	  // 3) Construct the bounder (stationary block bootstrap + BCa)
	  using BoundFutureReturnsT = mkc_timeseries::BoundFutureReturns<Num>;
	  const double cl = 0.99;
	  const double pL = 0.05; // lower-tail quantile (5th percentile) for monitoring
	  const double pU = 0.90; // upper (not used for gating here, but standard pair)
	  const std::size_t B = mNumResamples;

	  BoundFutureReturnsT bfr(monthly,                // monthly return series
				  blockLength,            // L
				  pL, pU,                 // lower/upper quantiles
				  B,                      // bootstrap resamples
				  cl);                    // confidence level

	  // 4) Operational lower bound (BCa lower endpoint at pL)
	  const Num lb = bfr.getLowerBound();

	  // Utility: print a percentage with 4 decimals
	  auto pct = [](const Num& x) {
	    std::ostringstream ss;
	    ss.setf(std::ios::fixed);
	    ss << std::setprecision(4)
	       << (x * DecimalConstants<Num>::DecimalOneHundred) << "%";
	    return ss.str();
	  };

	  // 5) Layperson-friendly report — put Lower Bound and L front and center
	  const std::size_t n = monthly.size();

	  // 5) Layperson-friendly report — indented to match the rest of the pipeline
	  const std::string INDENT = "      ";  // 6 spaces, matches other report sections

	  outputStream << "\n" << INDENT << "=== Future Monthly Return Bound (Monitoring) ===\n";
	  outputStream << INDENT << "Lower Bound (monthly, " << std::lround(100 * cl)
		       << "% confidence): " << pct(lb)
		       << "    [Block length L = " << blockLength << "]\n";

	  outputStream << INDENT << "What this means: With about " << std::lround(100 * cl)
		       << "% confidence, any future month is expected to be no worse than "
		       << pct(lb) << ".\n";

	  outputStream << INDENT << "How we estimated it: We used a block bootstrap with L = "
		       << blockLength
		       << " to respect typical month-to-month dependence.\n"
		       << INDENT
		       << "We then looked at the "
		       << std::lround(100 * pL)
		       << "th percentile of monthly returns and applied a BCa confidence interval.\n"
		       << INDENT
		       << "The number shown above is the **lower endpoint** of that interval (a conservative bound).\n";

	  outputStream << INDENT << "Data used: " << n
		       << " monthly returns"
		       << "  |  Bootstrap resamples: " << B
		       << "  |  Confidence level: " << std::lround(100 * cl) << "%\n";

	  outputStream << INDENT << "Interpretation guide:\n"
		       << INDENT << " • If this bound is well above 0%, downside months are usually mild.\n"
		       << INDENT << " • If it’s near/below 0%, expect occasional negative months of that size.\n"
		       << INDENT << " • Larger L assumes stronger serial dependence; smaller L assumes less.\n";

	  // 6) Return as a percent (matches prior behavior in your PyramidResults table)
	  return lb * DecimalConstants<Num>::DecimalOneHundred;
	}
      catch (const std::exception& e)
	{
	  outputStream << "      Future Returns Bound Analysis: Failed - " << e.what() << "\n";
	  return DecimalConstants<Num>::DecimalZero;
	}
    }

    /**
     * @brief Analyzes trade history to suggest optimal exit parameters.
     *
     * Objective:
     * Uses the `ExitPolicyJointAutoTuner` to analyze the holding period distribution
     * of winning vs. losing trades. It identifies:
     * 1. **Breakeven Activation:** The optimal bar count to move stops to breakeven.
     * 2. **Time Stop:** The optimal bar count to force an exit (if applicable).
     *
     * This is used primarily for the "Breakeven Stop" pyramid configuration (Level 5)
     * and for reporting tuning suggestions in Level 0.
     *
     * Arguments:
     * closedPositionHistory: The list of trades to analyze.
     * outputStream:          Logging stream.
     * performanceFile:       The file stream where the detailed report is written.
     */
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

    /**
     * @brief Generates a detailed trade-by-trade report file.
     *
     * Objective:
     * Delegates to `PerformanceReporter` to write the raw backtest artifacts
     * (equity curve, trade list) to a CSV/Text file. Also appends the
     * Exit Bar Tuning analysis.
     *
     * Arguments:
     * bt:                  The backtester instance.
     * performanceFileName: Path to the output file.
     * outputStream:        Logging stream (for error reporting).
     */
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

    /**
     * @brief Computes basic point estimates for returns.
     *
     * Objective:
     * Calculates the simple Arithmetic Mean and Geometric Mean of the return series
     * *before* any bootstrapping or annualization. Used for sanity checking the
     * data distribution.
     *
     * Arguments:
     * metaReturns:  Vector of period returns.
     * outputStream: Logging stream.
     */
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

    /**
     * @brief Determines the scaling factor for annualization.
     *
     * Objective:
     * Returns the number of bars in a trading year based on the timeframe.
     * - **Daily:** Returns ~252.
     * - **Intraday:** Calculates `252 * (MinutesPerDay / BarDuration)`.
     *
     * Arguments:
     * timeFrame:    The bar duration enum.
     * baseSecurity: Used to retrieve the trading session length (minutes per day).
     *
     * Returns:
     * The float scaling factor.
     */
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

    /**
     * @brief Executes the Regular (Gate #2) BCa Bootstrap.
     *
     * Objective:
     * Runs the standard Bias-Corrected and Accelerated (BCa) Block Bootstrap
     * on the entire OOS return series.
     * - Computes Lower Bounds for both Geometric Mean and Arithmetic Mean.
     * - Annualizes the results using the provided factor.
     *
     * Arguments:
     * metaReturns:         Return series.
     * annualizationFactor: The K-factor (or 252) for scaling.
     * blockLength:         Stationary block length $L$.
     * outputStream:        Logging stream.
     *
     * Returns:
     * A `BootstrapResults` struct containing period and annualized bounds.
     */
    MetaStrategyAnalyzer::BootstrapResults MetaStrategyAnalyzer::performBootstrapAnalysis(
											  const std::vector<Num>& metaReturns,
											  double annualizationFactor,
											  size_t blockLength,
											  std::ostream& outputStream) const
    {
      // Block length for meta bootstrap
      using ResamplerT = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;
      ResamplerT metaSampler(blockLength);
      using BlockBCA = BCaBootStrap<Num, ResamplerT>;

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

    void MetaStrategyAnalyzer::performBlockLengthSensitivity(
							     const std::vector<Num>& metaReturns,
							     std::size_t calculatedL,
							     double annualizationFactor,
							     const Num& hurdle,
							     std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;

      // Define multipliers to stress-test the block length
      std::vector<double> multipliers = {0.5, 1.5, 2.0};
    
      outputStream << "\n" 
		   << "      === Block Length Sensitivity Audit ===\n"
		   << "      (Checking robustness against L variation)\n";

      // Print the baseline (current result)
      outputStream << "      Baseline (L=" << calculatedL << "): Included in analysis above.\n" << std::endl;

      for (double mult : multipliers)
	{
	  // Calculate new L, ensuring it is at least 2 and at most n/2
	  std::size_t newL = static_cast<std::size_t>(calculatedL * mult);
	  newL = std::max<std::size_t>(2, newL);
	  newL = std::min<std::size_t>(metaReturns.size() / 2, newL);

	  // Skip if effective L didn't change (e.g., small L rounding)
	  if (newL == calculatedL) continue; 

	  // Run Bootstrap with new L
	  auto results = performBootstrapAnalysis(metaReturns, annualizationFactor, newL, outputStream);
        
	  // Check vs Hurdle
	  bool pass = (results.lbGeoAnn > hurdle);
        
	  outputStream << "      Sensitivity L=" << std::left << std::setw(4) << newL 
		       << " (" << std::fixed << std::setprecision(1) << mult << "x): "
		       << "LB=" << (results.lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "% "
		       << (pass ? "[PASS]" : "[FAIL]") 
		       << "\n" << "\n";
	}
      outputStream << "      ======================================\n\n";
    }
    
    /**
     * @brief Helper for Multi-Split: Bootstraps specific sub-segments.
     *
     * Objective:
     * Cuts the return series into $K$ slices and runs an independent BCa bootstrap
     * on each slice.
     *
     * Arguments:
     * returns:             The full return series.
     * K:                   Number of slices.
     * blockLength:         Block length $L$.
     * numResamples:        Bootstrap iterations per slice.
     * confidenceLevel:     Confidence level (e.g., 0.95).
     * annualizationFactor: Scaling factor.
     *
     * Returns:
     * A vector of annualized Lower Bounds (one per slice).
     */
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
	  
	  GeoMeanStat<Num> statGeo;
	  using ResamplerT = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;
	  ResamplerT sampler(blockLength);
	  using BlockBCA = BCaBootStrap<Num, ResamplerT>;

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

    /**
     * @brief Executes the Multi-Split / Time-Slicing Gate (Gate #4).
     *
     * Objective:
     * Tests if the strategy's edge is consistent across time.
     * 1. Divides OOS history into $K$ slices.
     * 2. Bootstraps each slice independently.
     * 3. **Pass Condition:** The *Median* lower bound of the slices must exceed
     * the cost hurdle.
     *
     * This prevents a strategy from passing based on one "lucky year" that
     * obscures poor performance in other years.
     *
     * Arguments:
     * metaReturns:  Portfolio returns.
     * K:            Initial slice count target.
     * Lmeta:        Block length.
     * annualizationFactor: Scaling factor.
     * bt:           Backtester (for trade counts).
     * oosSpreadStats: For cost hurdle calibration.
     *
     * Returns:
     * A `MultiSplitResult` containing the LBs per slice and pass/fail status.
     */
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

      const auto H = makeCostStressHurdles<Num>(mHurdleCalculator,
						oosSpreadStats,
						annualizedTrades,
						mEffectiveSlippageFloor);

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

      // 1. Consistency: The "Typical" period must clear the high hurdle
      const bool passConsistency = (r.medianLB > H.baseHurdle) && (r.medianLB > H.h_1q);

      // 2. Survival: The "Worst" period must clear the floor
      //    (e.g., -5% annualized, or even 0.0 depending on your risk tolerance)
      const Num survivalFloor = Num("-0.05"); // -5% annualized tolerance
      const bool passSurvival = (r.minLB > survivalFloor);

      r.pass = (passConsistency && passSurvival);

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

    /**
     * @brief Computes the "Required Return" thresholds.
     *
     * Objective:
     * Calculates the hurdles a strategy must clear to be viable:
     * 1. **Risk-Free:** Must beat the risk-free rate.
     * 2. **Cost-Based:** Must cover estimated slippage/commissions + a safety buffer.
     * 3. **Final:** `max(RiskFree, CostBased)`.
     *
     * Logs a detailed breakdown of how the hurdle was derived.
     *
     * Arguments:
     * annualizedTrades: The turnover rate (trades/year).
     * outputStream:     Logging stream.
     *
     * Returns:
     * A `CostHurdleResults` struct.
     */
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

    /**
     * @brief Standalone helper for BCa Max Drawdown analysis.
     *
     * Objective:
     * Calculates the confidence intervals for the Maximum Drawdown statistic.
     * Used primarily when a simple console print is needed (not part of the
     * structured `PyramidResults` object).
     *
     * Arguments:
     * metaReturns:  Return series.
     * numTrades:    Trade count (sample size $n$).
     * blockLength:  Block length $L$.
     * outputStream: Logging stream.
     */
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
              auto drawdownResult = BoundedDrawdowns::bcaBoundsForDrawdownFractile(metaReturns,
										   mNumResamples,
										   mConfidenceLevel.getAsDouble(),
										   static_cast<int>(numTrades),
										   5000,
										   mConfidenceLevel.getAsDouble(),
										   blockLength,
										   executor);

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

    /**
     * @brief Finalizes the analysis state and prints the verdict.
     *
     * Objective:
     * Updates the member variables `mMetaStrategyPassed`, `mAnnualizedLowerBound`,
     * and `mRequiredReturn` based on the results of the primary gate.
     * Prints the final "PASS" or "FAIL" banner for the Unified Strategy.
     *
     * Arguments:
     * bootstrapResults: Outcome of the primary bootstrap.
     * costResults:      The calculated hurdles.
     * strategyCount:    Number of components in the portfolio.
     * outputStream:     Logging stream.
     */
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

    /**
     * @brief Structured BCa Max Drawdown analysis for Pyramid results.
     *
     * Objective:
     * Identical to `performDrawdownAnalysis` mathematically, but returns a
     * `DrawdownResults` object instead of printing to the stream. This object
     * is stored in the `PyramidResults` for later comparison (MAR calculation).
     *
     * Returns:
     * A `DrawdownResults` object containing Point Estimate, CI Lower/Upper bounds.
     */
    DrawdownResults MetaStrategyAnalyzer::performDrawdownAnalysisForPyramid(
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

    /**
     * @brief Generates the master report file for all pyramid levels.
     *
     * Objective:
     * Creates a single, detailed text file containing:
     * 1. Trade logs for every level (0, 1, 2...).
     * 2. Statistical summaries for every level.
     * 3. Drawdown analysis for every level.
     * 4. The comparison table summary.
     *
     * Arguments:
     * allResults:          Vector of results for all levels.
     * performanceFileName: Output file path.
     * outputStream:        Logging stream (for error reporting).
     */
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
          performanceFile << "Max Consecutive Losing Trades (Upper Bound): "
                         << result.getLosingStreakUpperBound() << " trades" << std::endl;
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
      performanceFile << "Level | Description              | Ann. Lower Bound | Future Ret LB | Max Loss Streak UB | Required Return | Pass/Fail | Trades/Year" << std::endl;
      performanceFile << "------|--------------------------|------------------|---------------|---------------------|-----------------|-----------|------------" << std::endl;
      
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
                         << std::right << std::setw(18)
                         << result.getLosingStreakUpperBound() << " | "
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

    /**
     * @brief Selects the single best portfolio configuration (Canonical Passer).
     *
     * Objective:
     * After testing all levels (0, 1, 2, 3...), this method picks the winner.
     * Selection Logic:
     * 1. **Filter:** Only consider levels that PASSED all gates.
     * 2. **Primary Metric:** **MAR Ratio** (Annualized Return / Max Drawdown).
     * We prioritize efficiency (return per unit of risk) over raw profit.
     * 3. **Tie-Breaker:** If MARs are invalid or equal, prioritize raw
     * Annualized Lower Bound.
     *
     * This ensures the system recommends the most robust, risk-efficient
     * version of the portfolio, not just the one with the highest leverage.
     *
     * Returns:
     * A pointer to the winning `PyramidResults` object (or nullptr if none passed).
     */
    const PyramidResults*
    MetaStrategyAnalyzer::selectBestPassingConfiguration(
        const std::vector<PyramidResults>& allResults,
        std::ostream& outputStream) const
    {
      using mkc_timeseries::DecimalConstants;
      
      // Helper lambdas for selection criteria
      auto hasValidDD = [](const PyramidResults& r) -> bool {
        const auto& dd = r.getDrawdownResults();
        return dd.hasResults() && dd.getUpperBound() > DecimalConstants<Num>::DecimalZero;
      };

      auto conservativeMAR = [&](const PyramidResults& r) -> Num {
        if (!hasValidDD(r)) return DecimalConstants<Num>::DecimalZero;
        return r.getAnnualizedLowerBound() / r.getDrawdownResults().getUpperBound();
      };

      auto margin = [&](const PyramidResults& r) -> Num {
        return r.getAnnualizedLowerBound() - r.getRequiredReturn();
      };

      // Filter to passers
      std::vector<const PyramidResults*> passers;
      passers.reserve(allResults.size());
      for (const auto& r : allResults)
        if (r.getPassed()) passers.push_back(&r);

      if (passers.empty())
      {
        return nullptr;
      }

      // Rank passers by MAR (if available), else by LB; then by margin
      const PyramidResults* best = passers.front();
      bool bestHasValidDD = hasValidDD(*best);
      Num  bestMAR        = bestHasValidDD ? conservativeMAR(*best)
        : DecimalConstants<Num>::DecimalZero;
      Num  bestLB         = best->getAnnualizedLowerBound();
      Num  bestMargin     = margin(*best);

      for (std::size_t i = 1; i < passers.size(); ++i)
      {
        const PyramidResults* cand = passers[i];
        const bool candValidDD = hasValidDD(*cand);
        const Num  candMAR     = candValidDD ? conservativeMAR(*cand)
          : DecimalConstants<Num>::DecimalZero;
        const Num  candLB      = cand->getAnnualizedLowerBound();
        const Num  candMargin  = margin(*cand);

        bool better = false;

        if (bestHasValidDD || candValidDD)
        {
          // Prefer valid MAR; if both valid, compare MAR; if only candidate valid, candidate wins
          if (!bestHasValidDD && candValidDD) {
            better = true;
          } else if (bestHasValidDD && candValidDD) {
            if (candMAR > bestMAR) better = true;
            else if (candMAR == bestMAR && candMargin > bestMargin) better = true;
            else if (candMAR == bestMAR && candMargin == bestMargin && candLB > bestLB) better = true;
          }
        }
        else
        {
          // Neither has valid DD → compare LB; tie-break by margin
          if (candLB > bestLB) better = true;
          else if (candLB == bestLB && candMargin > bestMargin) better = true;
        }

        if (better)
        {
          best           = cand;
          bestHasValidDD = candValidDD;
          bestMAR        = candMAR;
          bestLB         = candLB;
          bestMargin     = candMargin;
        }
      }

      // Log the chosen configuration
      outputStream << "      [Meta] Chosen configuration → Level "
                   << best->getPyramidLevel()
                   << " (" << best->getDescription() << "), "
                   << "Ann LB=" << (best->getAnnualizedLowerBound() * DecimalConstants<Num>::DecimalOneHundred)
                   << "%, Required=" << (best->getRequiredReturn() * DecimalConstants<Num>::DecimalOneHundred)
                   << "%";
      if (bestHasValidDD) {
        outputStream << ", MAR=" << (bestMAR.getAsDouble());
      }
      outputStream << "\n";

      return best;
    }

    /**
     * @brief Prints the summary comparison table to the console.
     *
     * Objective:
     * Displays the ASCII table summarizing the key metrics (LB, MAR, Drawdown, Pass/Fail)
     * for all analyzed pyramid levels.
     * Also identifies and prints the "Best Performance" recommendation based on
     * the MAR ratio.
     *
     * Arguments:
     * allResults:   Vector of results for all levels.
     * outputStream: The console/log stream.
     */
    void MetaStrategyAnalyzer::outputPyramidComparison(const std::vector<PyramidResults>& allResults,
						       std::shared_ptr<Security<Num>> baseSecurity,
						       std::ostream& outputStream) const
    {
      outputStream << "\n[Meta] Pyramid Analysis Summary:\n";
      outputStream << "      Level | Description              |      MAR | Ann. Lower Bound | Future Ret LB | Max Loss Streak UB | Drawdown UB | Required Return | Pass/Fail\n";
      outputStream << "      ------|--------------------------|----------|------------------|---------------|---------------------|-------------|-----------------|----------\n";

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

	  // Max Consecutive Losing Trades Upper Bound
	  outputStream << std::right << std::setw(18)
	        << result.getLosingStreakUpperBound() << " | ";

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

	  palvalidator::filtering::PositionSizingCalculator<Num>::recommendSizing(baseSecurity, 
										  *bestResult, 
										  outputStream,
										  0.20); // You can make this 0.20 configurable or a constant
	  
	}

      outputStream << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
    }
  } // namespace filtering
} // namespace palvalidator
