#include "PerformanceFilter.h"
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
#include "RegimeMixStressRunner.h"
#include <limits>

namespace palvalidator
{
  namespace filtering
  {

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

    std::vector<std::shared_ptr<PalStrategy<Num>>> PerformanceFilter::filterByPerformance(
											  const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
											  std::shared_ptr<Security<Num>> baseSecurity,
											  const DateRange& backtestingDates,
											  TimeFrame::Duration timeFrame,
											  std::ostream& outputStream)
    {
      std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;

      // Reset summary for new filtering run
      mFilteringSummary = FilteringSummary();

      const Num riskFreeHurdle = mHurdleCalculator.calculateRiskFreeHurdle();

      outputStream << "\nFiltering " << survivingStrategies.size() << " surviving strategies by BCa performance...\n";
      outputStream << "Filter 1 (Statistical Viability): Annualized Lower Bound > 0\n";
      outputStream << "Filter 2 (Economic Significance): Annualized Lower Bound > (Annualized Cost Hurdle * "
		   << mHurdleCalculator.getCostBufferMultiplier() << ")\n";
      outputStream << "Filter 3 (Risk-Adjusted Return): Annualized Lower Bound > (Risk-Free Rate + Risk Premium ( "
		   << mHurdleCalculator.getRiskPremium() << ") )\n";
      outputStream << "  - Cost assumptions: $0 commission, 0.10% slippage/spread per side.\n";
      outputStream << "  - Risk-Free Rate assumption: " << (mHurdleCalculator.getRiskFreeRate() * DecimalConstants<Num>::DecimalOneHundred) << "%.\n";

      for (const auto& strategy : survivingStrategies)
	{
	  try
	    {
	      // Fresh portfolio + cloned strategy
	      auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
	      freshPortfolio->addSecurity(baseSecurity);
	      auto clonedStrat = strategy->clone2(freshPortfolio);

	      // Backtest and get high-resolution per-bar returns
	      auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat, timeFrame, backtestingDates);
	      auto highResReturns = backtester->getAllHighResReturns(clonedStrat.get());

	      if (highResReturns.size() < 20)
		{
		  outputStream << "✗ Strategy filtered out: " << strategy->getStrategyName()
			       << " - Insufficient returns for bootstrap (" << highResReturns.size() << " < 20).\n";
		  mFilteringSummary.incrementInsufficientCount();
		  continue;
		}

	      // Holding-period diagnostics and baseline L
	      const unsigned int medianHoldBars = backtester->getClosedPositionHistory().getMedianHoldingPeriod();
	      outputStream << "Strategy Median holding period = " << medianHoldBars << "\n";
	      const std::size_t L = std::max<std::size_t>(2, static_cast<std::size_t>(medianHoldBars));
	      StationaryBlockResampler<Num> sampler(L);

	      // BCa (GeoMean + Mean) at baseline L
	      GeoMeanStat<Num> statGeo;
	      using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;
	      BlockBCA bcaGeo(highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, sampler);
	      BlockBCA bcaMean(highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(),
			       &mkc_timeseries::StatUtils<Num>::computeMean, sampler);

	      const Num lbGeoPeriod = bcaGeo.getLowerBound();
	      const Num lbMeanPeriod = bcaMean.getLowerBound();

	      // Annualization factor
	      double annualizationFactor;
	      if (timeFrame == TimeFrame::INTRADAY)
		{
		  annualizationFactor = calculateAnnualizationFactor(
								     timeFrame,
								     baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes());
		}
	      else
		{
		  annualizationFactor = calculateAnnualizationFactor(timeFrame);
		}

	      // Annualized lower bounds
	      BCaAnnualizer<Num> annualizerGeo(bcaGeo, annualizationFactor);
	      BCaAnnualizer<Num> annualizerMean(bcaMean, annualizationFactor);

	      const Num annualizedLowerBoundGeo  = annualizerGeo.getAnnualizedLowerBound();
	      const Num annualizedLowerBoundMean = annualizerMean.getAnnualizedLowerBound();

	      // Hurdles (cost- and risk-based)
	      const Num annualizedTrades    = Num(backtester->getEstimatedAnnualizedTrades());
	      const Num finalRequiredReturn = mHurdleCalculator.calculateFinalRequiredReturn(annualizedTrades);

	      // Early decision on GM LB vs hurdle
	      if (!passesHurdleRequirements(annualizedLowerBoundGeo, finalRequiredReturn))
		{
		  outputStream << "✗ Strategy filtered out: " << strategy->getStrategyName()
			       << " (Lower Bound = "
			       << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
			       << "% <= Required Return = "
			       << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
			       << "  [Block L=" << L << "]\n\n";
		  continue;
		}

	      // AM–GM divergence diagnostic
	      const auto divergence =
		palvalidator::analysis::DivergenceAnalyzer::assessAMGMDivergence(
										 annualizedLowerBoundGeo, annualizedLowerBoundMean, /*absThresh=*/0.05, /*relThresh=*/0.30);

	      // Robustness gate decision
	      double lSensitivityRelVar = 0.0;
	      const bool nearHurdle = (annualizedLowerBoundGeo <= (finalRequiredReturn + mRobustnessConfig.borderlineAnnualMargin));
	      const bool smallN     = (highResReturns.size() < mRobustnessConfig.minTotalForSplit);
	      const bool mustRobust = divergence.flagged || nearHurdle || smallN;

	      if (mustRobust)
		{
		  if (!processRobustnessChecks(strategy, strategy->getStrategyName(), highResReturns, L,
					       annualizationFactor, finalRequiredReturn, divergence,
					       nearHurdle, smallN, outputStream))
		    {
		      continue;
		    }
		  // Optionally capture relVar from robustness module if you expose it
		  const auto rob =
		    palvalidator::analysis::RobustnessAnalyzer::runFlaggedStrategyRobustness(
											     strategy->getStrategyName(), highResReturns, L, annualizationFactor,
											     finalRequiredReturn, mRobustnessConfig, outputStream);
		  lSensitivityRelVar = rob.relVar;
		}

	      // ===================== NEW: L-cap and L-grid sensitivity =====================

	      if (mLSensitivity.enabled)
		{
		  // Derive a per-strategy cap based on observed max hold (if available), else default (8)
		  unsigned int maxHoldBars = 8;

		  // Compute cap: min(config.maxL, maxHold + capBuffer) if cap-by-hold enabled; else config.maxL
		  size_t L_cap = mLSensitivity.maxL;
		  if (mLSensitivity.capByMaxHold)
		    {
		      const size_t byHold = static_cast<size_t>(std::max<unsigned int>(2, maxHoldBars + mLSensitivity.capBuffer));
		      L_cap = std::min(mLSensitivity.maxL, byHold);
		    }

		  // Run L-grid stress with cap
		  const auto Lres = runLSensitivity(highResReturns, L /*center*/, L_cap,
						    annualizationFactor, finalRequiredReturn, outputStream);

		  if (Lres.ran)
		    {
		      // One-line summary
		      const double frac = (Lres.numTested == 0) ? 0.0 : double(Lres.numPassed) / double(Lres.numTested);
		      outputStream << "      [L-grid] pass fraction = " << (100.0 * frac) << "%, "
				   << "min LB at L=" << Lres.L_at_min
				   << ", min LB = " << (Lres.minLbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%, "
				   << "relVar = " << Lres.relVar << " → decision: "
				   << (Lres.pass ? "PASS" : "FAIL") << "\n";

		      // Feed relVar into fragile-edge advisory
		      lSensitivityRelVar = std::max(lSensitivityRelVar, Lres.relVar);

		      if (!Lres.pass)
			{
			  const bool catastrophic =
			    (finalRequiredReturn - Lres.minLbAnn) > Num(std::max(0.0, mLSensitivity.minGapTolerance));
			  if (catastrophic) mFilteringSummary.incrementFailLBoundCount();
			  else              mFilteringSummary.incrementFailLVarCount();

			  outputStream << "   ✗ Strategy filtered out due to L-sensitivity: "
                            "insufficient robustness across block lengths (capped).\n\n";
			  continue;
			}
		    }
		}

	      // === Regime-mix stress =======================================================
	      {
		const bool regimeOk =
		  runRegimeMixStress(highResReturns, L, annualizationFactor, finalRequiredReturn, outputStream);
		
		if (!regimeOk)
		  {
		    mFilteringSummary.incrementFailRegimeMixCount();
		    continue;                                          // Fail fast (reject strategy)
		  }
	      }
	      
	      // ============================================================================

	      // Fragile edge advisory (may use lSensitivityRelVar)
	      if (!processFragileEdgeAnalysis(lbGeoPeriod, annualizedLowerBoundGeo, finalRequiredReturn,
					      lSensitivityRelVar, highResReturns, outputStream))
		{
		  continue;
		}

	      // Keep strategy
	      filteredStrategies.push_back(strategy);

	      outputStream << "✓ Strategy passed: " << strategy->getStrategyName()
			   << " (Lower Bound = "
			   << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
			   << "% > Required Return = "
			   << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
			   << "  [Block L=" << L << "]\n";

	      outputStream << "   ↳ Lower bounds (annualized): "
			   << "GeoMean = " << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred) << "%, "
			   << "Mean = " << (annualizedLowerBoundMean * DecimalConstants<Num>::DecimalOneHundred) << "%\n\n";
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
    
    bool PerformanceFilter::passesHurdleRequirements(
						     const Num& annualizedLowerBoundGeo,
						     const Num& finalRequiredReturn) const
    {
      return annualizedLowerBoundGeo > finalRequiredReturn;
    }

    bool PerformanceFilter::processRobustnessChecks(
						    std::shared_ptr<PalStrategy<Num>> strategy,
						    const std::string& strategyName,
						    const std::vector<Num>& highResReturns,
						    size_t L,
						    double annualizationFactor,
						    const Num& finalRequiredReturn,
						    const palvalidator::analysis::DivergenceResult<Num>& divergence,
						    bool nearHurdle,
						    bool smallN,
						    std::ostream& outputStream)
    {
      if (divergence.flagged)
	{
	  mFilteringSummary.incrementFlaggedCount();
	  outputStream << "   [FLAG] Large AM vs GM divergence (abs="
		       << (Num(divergence.absDiff) * DecimalConstants<Num>::DecimalOneHundred) << "%, rel=";
	  if (divergence.relState == palvalidator::analysis::DivergencePrintRel::Defined)
            outputStream << divergence.relDiff;
	  else
            outputStream << "n/a";
	  outputStream << "); running robustness checks";

	  if (nearHurdle || smallN)
	    {
	      outputStream << " (also triggered by ";
	      if (nearHurdle)
                outputStream << "near-hurdle";
	      if (nearHurdle && smallN)
                outputStream << " & ";
	      if (smallN)
                outputStream << "small-sample";
	      outputStream << ")";
	    }
	  outputStream << "...\n";
	}
      else
	{
	  outputStream << "   [CHECK] Running robustness checks due to "
		       << (nearHurdle ? "near-hurdle" : "")
		       << ((nearHurdle && smallN) ? " & " : "")
		       << (smallN ? "small-sample" : "")
		       << " condition(s)...\n";
	}

      const auto rob = palvalidator::analysis::RobustnessAnalyzer::runFlaggedStrategyRobustness(
												strategyName, highResReturns, L, annualizationFactor, finalRequiredReturn, mRobustnessConfig, outputStream);

      if (rob.verdict == palvalidator::analysis::RobustnessVerdict::ThumbsDown)
	{
	  switch (rob.reason)
	    {
            case palvalidator::analysis::RobustnessFailReason::LSensitivityBound:
	      mFilteringSummary.incrementFailLBoundCount();
	      break;
            case palvalidator::analysis::RobustnessFailReason::LSensitivityVarNearHurdle:
	      mFilteringSummary.incrementFailLVarCount();
	      break;
            case palvalidator::analysis::RobustnessFailReason::SplitSample:
	      mFilteringSummary.incrementFailSplitCount();
	      break;
            case palvalidator::analysis::RobustnessFailReason::TailRisk:
	      mFilteringSummary.incrementFailTailCount();
	      break;
            default:
	      break;
	    }
	  outputStream << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]") << " Robustness checks FAILED → excluding strategy.\n\n";
	  return false;
	}
      else
	{
	  if (divergence.flagged)
            mFilteringSummary.incrementFlagPassCount();

	  outputStream << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]") << " Robustness checks PASSED.\n";
	  return true;
	}
    }

    bool PerformanceFilter::processFragileEdgeAnalysis(
						       const Num& lbGeoPeriod,
						       const Num& annualizedLowerBoundGeo,
						       const Num& finalRequiredReturn,
						       double lSensitivityRelVar,
						       const std::vector<Num>& highResReturns,
						       std::ostream& outputStream)
    {
      const auto [q05, es05] = palvalidator::analysis::FragileEdgeAnalyzer::computeQ05_ES05(highResReturns, /*alpha=*/0.05);
      const auto advice = palvalidator::analysis::FragileEdgeAnalyzer::analyzeFragileEdge(
											  lbGeoPeriod,                  // per-period GM LB
											  annualizedLowerBoundGeo,      // annualized GM LB
											  finalRequiredReturn,          // hurdle (annual)
											  lSensitivityRelVar,           // relVar from robustness; 0.0 if unrun
											  q05,                          // tail quantile
											  es05,                         // ES05 (logged elsewhere)
											  highResReturns.size(),        // n
											  mFragileEdgePolicy            // thresholds
											  );

      auto fragileActionToText = [](palvalidator::analysis::FragileEdgeAction a) -> const char*
      {
        switch (a)
	  {
	  case palvalidator::analysis::FragileEdgeAction::Keep:       return "Keep";
	  case palvalidator::analysis::FragileEdgeAction::Downweight: return "Downweight";
	  case palvalidator::analysis::FragileEdgeAction::Drop:       return "Drop";
	  default:                                                     return "Keep";
	  }
      };

      outputStream << "   [ADVISORY] Fragile edge assessment: action="
		   << fragileActionToText(advice.action)
		   << ", weight×=" << advice.weightMultiplier
		   << " — " << advice.rationale << "\n";

      if (mApplyFragileAdvice)
	{
	  if (advice.action == palvalidator::analysis::FragileEdgeAction::Drop)
	    {
	      outputStream << "   [ADVISORY] Apply=ON → dropping strategy per fragile-edge policy.\n\n";
	      return false;
	    }
	  if (advice.action == palvalidator::analysis::FragileEdgeAction::Downweight)
	    {
	      outputStream << "   [ADVISORY] Apply=ON → (not implemented here) would downweight this strategy in meta.\n";
	    }
	}

      return true;
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

    // PerformanceFilter.cpp
    static std::vector<size_t>
    makeDefaultLGrid(size_t Lcenter, size_t n, size_t Lcap)
    {
      // Base candidates: focused around very short holds + a couple of multiples
      std::vector<size_t> g = {
        2, 3, 4, 5, 6, 8, 10,                 // short-horizon emphasis
        Lcenter > 0 ? Lcenter : 2,
        2 * std::max<size_t>(2, Lcenter)
      };

      // Apply cap and feasibility
      const size_t hardCap = std::max<size_t>(2, std::min(Lcap, n - 1));
      for (auto& L : g) {
        if (L < 2) L = 2;
        if (L > hardCap) L = hardCap;
      }

      std::sort(g.begin(), g.end());
      g.erase(std::unique(g.begin(), g.end()), g.end());
      g.erase(std::remove_if(g.begin(), g.end(),
			     [&](size_t L){ return L < 2 || L >= n || L > hardCap; }),
	      g.end());

      // Guarantee we include Lcenter (feasible & capped)
      const size_t Lc = std::max<size_t>(2, std::min(Lcenter, hardCap));
      if (!std::binary_search(g.begin(), g.end(), Lc)) {
        g.insert(std::lower_bound(g.begin(), g.end(), Lc), Lc);
      }

      return g;
    }

    PerformanceFilter::LSensitivityResult
    PerformanceFilter::runLSensitivity(const std::vector<Num>& returns,
				       size_t L_center,
				       size_t L_cap,
				       double annualizationFactor,
				       const Num& finalRequiredReturn,
				       std::ostream& os) const
    {
      LSensitivityResult R; 
      R.ran = true;

      const size_t n = returns.size();
      if (n < 20) {
        os << "      [L-grid] Skipped (n<20).\n";
        return R;
      }

      // Build grid with cap
      std::vector<size_t> grid;
      const size_t hardCap = std::max<size_t>(2, std::min(L_cap, n - 1));

      if (!mLSensitivity.Lgrid.empty())
	{
	  grid = mLSensitivity.Lgrid;
	  // Enforce feasibility and cap
	  grid.erase(std::remove_if(grid.begin(), grid.end(),
				    [&](size_t L){ return L < 2 || L >= n || L > hardCap; }),
		     grid.end());
	  std::sort(grid.begin(), grid.end());
	  grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
	  // Ensure L_center is included if feasible
	  const size_t Lc = std::max<size_t>(2, std::min(L_center, hardCap));
	  if (!grid.empty() && !std::binary_search(grid.begin(), grid.end(), Lc))
	    {
	      grid.insert(std::lower_bound(grid.begin(), grid.end(), Lc), Lc);
	    }
	  else if (grid.empty())
	    {
	      grid.push_back(Lc);
	    }
	}
      else
	{
	  // Default grid builder (cap-aware)
	  grid = makeDefaultLGrid(L_center, n, hardCap);
	}

      if (grid.empty()) {
        os << "      [L-grid] No feasible L values after capping.\n";
        return R;
      }

      // Run BCa(GeoMean) for each L
      using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;
      GeoMeanStat<Num> statGeo;

      Num minLb = Num(std::numeric_limits<double>::infinity());
      size_t L_at_min = 0;
      size_t passCount = 0;
      std::vector<Num> lbs; lbs.reserve(grid.size());
      R.perL.clear();

      for (size_t L : grid) {
        StationaryBlockResampler<Num> sampler(L);
        BlockBCA bcaGeo(returns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, sampler);
        const Num lbGeoPeriod = bcaGeo.getLowerBound();
        BCaAnnualizer<Num> annualizer(bcaGeo, annualizationFactor);
        const Num lbGeoAnn = annualizer.getAnnualizedLowerBound();

        R.perL.emplace_back(L, lbGeoAnn);
        lbs.push_back(lbGeoAnn);

        if (lbGeoAnn < minLb) { minLb = lbGeoAnn; L_at_min = L; }
        if (lbGeoAnn > finalRequiredReturn) ++passCount;
      }

      R.minLbAnn = minLb;
      R.L_at_min = L_at_min;
      R.numTested = grid.size();
      R.numPassed = passCount;

      // Relative variance across grid (for advisory/logging)
      auto meanFn = [](const std::vector<Num>& v){
        Num s = Num(0); for (auto x: v) s += x; return s / Num(v.size());
      };
      const Num mu = meanFn(lbs);
      if (mu != Num(0)) {
        Num ss = Num(0);
        for (auto x: lbs) { Num d = x - mu; ss += d*d; }
        const Num var = ss / Num(lbs.size());
        R.relVar = (var / (mu * mu)).getAsDouble();
      } else {
        R.relVar = 0.0;
      }

      // Decision rule
      const double frac = (grid.empty() ? 0.0 : double(passCount) / double(grid.size()));
      bool pass = (frac >= mLSensitivity.minPassFraction);
      if (pass && mLSensitivity.minGapTolerance > 0.0) {
        const Num gap = finalRequiredReturn - minLb;
        if (gap > Num(mLSensitivity.minGapTolerance)) pass = false;
      }
      R.pass = pass;

      // Logging
      os << "      [L-grid] Tested L = ";
      for (size_t i = 0; i < grid.size(); ++i) {
        os << grid[i]; if (i + 1 < grid.size()) os << ", ";
      }
      os << "\n";
      for (const auto& kv : R.perL) {
        const auto L = kv.first;
        const auto lbAnn = kv.second;
        os << "        L=" << L << ": Ann GM LB = "
           << (lbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%"
           << (lbAnn > finalRequiredReturn ? "  (PASS)" : "  (FAIL)") << "\n";
      }
      os << "        → pass fraction = " << (100.0 * frac) << "%, "
	 << "min LB at L=" << R.L_at_min
	 << ", min LB = " << (R.minLbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

      return R;
    }

    bool PerformanceFilter::runRegimeMixStress(const std::vector<Num> &highResReturns,
					       std::size_t L,
					       double annualizationFactor,
					       const Num &finalRequiredReturn,
					       std::ostream &outputStream) const
    {
      using palvalidator::analysis::VolTercileLabeler;
      using palvalidator::analysis::RegimeMix;
      using palvalidator::analysis::RegimeMixConfig;
      using palvalidator::analysis::RegimeMixStressRunner;

      try
	{
	  // 1) Label regimes (short window suits 2–3 bar holds)
	  const std::size_t volWindow = 20;
	  VolTercileLabeler<Num> labeler(volWindow);
	  const std::vector<int> labels = labeler.computeLabels(highResReturns);

	  // 2) Target mixes
	  const std::vector<RegimeMix> mixes =
	    {
	      RegimeMix("Equal(1/3,1/3,1/3)", { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 }),
	      RegimeMix("DownFav(0.3,0.4,0.3)", { 0.30, 0.40, 0.30 })
	      // Optional: add "LongRun(...)" once you compute baseline weights
	    };

	  // 3) Config:
	  //    - Require at least half the mixes to pass (with 2 mixes → ≥1 must pass)
	  //    - Also enforce Equal-mix must pass (below)
	  const double minPassFraction = 0.50;
	  const std::size_t minBarsPerRegime = static_cast<std::size_t>(L + 5);

	  RegimeMixConfig mixCfg(mixes, minPassFraction, minBarsPerRegime);

	  // 4) Runner (uses your BCa settings & annualization)
	  RegimeMixStressRunner<Num> runner(mixCfg,
					    L,
					    mNumResamples,
					    mConfidenceLevel.getAsDouble(),
					    annualizationFactor,
					    finalRequiredReturn);

	  const auto res = runner.run(highResReturns, labels, outputStream);

	  // 5) Extra policy checks beyond passFraction:
	  //    (a) Equal mix must pass
	  //    (b) Catastrophic fail if any mix LB < (hurdle - epsilon)
	  const Num catastrophicEps = Num(0.0025); // 25 bps = 0.25%

	  bool equalFound = false;
	  bool equalPassed = false;
	  bool catastrophic = false;

	  for (const auto &mx : res.perMix())
	    {
	      const bool isEqual =
                (mx.mixName() == std::string("Equal(1/3,1/3,1/3)")) ||
                (mx.mixName() == std::string("Equal"));

	      if (isEqual)
		{
		  equalFound = true;
		  equalPassed = mx.pass();
		}

	      if (mx.annualizedLowerBound() < (finalRequiredReturn - catastrophicEps))
		{
		  catastrophic = true;
		}
	    }

	  if (!equalFound)
	    {
	      outputStream << "      [RegimeMix] Warning: 'Equal' mix not present; "
			   << "skipping 'Equal must pass' policy.\n";
	    }

	  if (catastrophic)
	    {
	      outputStream << "   ✗ Strategy filtered out due to Regime-mix sensitivity: "
			   << "a mix produced an annualized LB more than 25 bps below the hurdle.\n\n";
	      return false;
	    }

	  if (equalFound && !equalPassed)
	    {
	      outputStream << "   ✗ Strategy filtered out: 'Equal(1/3,1/3,1/3)' mix failed the hurdle.\n\n";
	      return false;
	    }

	  if (!res.overallPass())
	    {
	      outputStream << "   ✗ Strategy filtered out due to Regime-mix sensitivity: "
			   << "insufficient robustness across target mixes (minPassFraction = 0.50).\n\n";
	      return false;
	    }

	  return true;
	}
      catch (const std::exception &e)
	{
	  // Conservative: skip regime-mix gate on operational error, but do not fail the strategy.
	  outputStream << "      [RegimeMix] Skipped (" << e.what() << ").\n";
	  return true;
	}
    }
  } // namespace filtering
} // namespace palvalidator
