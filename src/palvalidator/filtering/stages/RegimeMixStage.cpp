#include "filtering/stages/RegimeMixStage.h"
#include "BackTester.h"
#include "Security.h"
#include "RegimeLabeler.h"
#include "RegimeMixStress.h"
#include "RegimeMixStationaryResampler.h"
#include "filtering/RegimeMixStressRunner.h"
#include "filtering/RegimeMixUtils.h"
#include "BarAlignedSeries.h"
#include "TimeSeriesCsvReader.h"
#include "DecimalConstants.h"
#include "Annualizer.h"
#include <algorithm>
#include <iomanip>
#include <limits>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using palvalidator::analysis::BarAlignedSeries;
  using palvalidator::analysis::RegimeMix;
  using palvalidator::analysis::RegimeMixConfig;
  using palvalidator::analysis::RegimeMixStressRunner;
  using palvalidator::analysis::VolTercileLabeler;
  using palvalidator::filtering::regime_mix_utils::computeLongRunMixWeights;
  using palvalidator::filtering::regime_mix_utils::adaptMixesToPresentRegimes;

  constexpr std::size_t kRegimeVolWindow = 20;

  FilterDecision RegimeMixStage::execute(
					 const StrategyAnalysisContext& ctx,
					 const BootstrapAnalysisResult& bootstrap,
					 const HurdleAnalysisResult& hurdle,
					 std::ostream& os) const
  {
    using mkc_timeseries::FilterTimeSeries;
    using mkc_timeseries::RocSeries;

    // Defensive checks (should not happen in normal flow)
    if (!ctx.baseSecurity || !ctx.backtester)
      {
	os << "   [RegimeMix] Skipped (null baseSecurity/backtester).\n";
	return FilterDecision::Pass(); // Non-gating skip
      }

    // 1) Build OOS close series (dense) for labeling
    auto oosInstrumentTS = FilterTimeSeries(*ctx.baseSecurity->getTimeSeries(), ctx.oosDates);
    const auto& oosClose = oosInstrumentTS.CloseTimeSeries();

    // 2) Collect OOS ClosedPositionHistory (sparse trade-sequence timestamps)
    const auto& closed = ctx.backtester->getClosedPositionHistory();

    // 3) Build trade-aligned regime labels from dense OOS closes
    std::vector<int> tradeLabels;
    try {
      palvalidator::analysis::BarAlignedSeries<Num> aligner(kRegimeVolWindow);
      tradeLabels = aligner.buildTradeAlignedLabels(oosClose, closed);
    } catch (const std::exception& e) {
      os << "   [RegimeMix] Skipped (label alignment failed: " << e.what() << ").\n";
      return FilterDecision::Pass(); // Non-gating skip
    }

    if (tradeLabels.size() != ctx.highResReturns.size())
      {
	os << "   [RegimeMix] Skipped (labels length "
	   << tradeLabels.size() << " != returns length "
	   << ctx.highResReturns.size() << ").\n";
	return FilterDecision::Pass(); // Non-gating skip
      }

    // 4) Build LongRun baseline: 1-bar ROC on in-sample close series
    auto inSampleTS = FilterTimeSeries(*ctx.baseSecurity->getTimeSeries(), ctx.inSampleDates);
    auto insampleROC = RocSeries(inSampleTS.CloseTimeSeries(), /*period=*/1);
    auto baselineRoc = insampleROC.getTimeSeriesAsVector();

    if (baselineRoc.size() < 3) {
      os << "   [RegimeMix] Note: in-sample ROC short ("
	 << baselineRoc.size() << " bars). LongRun may be skipped.\n";
    }

    // 5) Build target mixes: Equal + DownFav; optionally LongRun

    auto clip_and_normalize = [](std::vector<double> w, double floor = 0.01) {
      // Clip to [floor, 1], then renormalize to sum 1.0
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
    
    std::vector<palvalidator::analysis::RegimeMix> mixes;

    // Equal: neutral benchmark
    mixes.emplace_back("Equal(0.33,0.33,0.33)",
		       std::vector<double>{1.0/3.0, 1.0/3.0, 1.0/3.0});
    
    // MidFav (was “DownFav”): overweight middle regime
    mixes.emplace_back("MidVolFav(0.25,0.50,0.25)",
		       std::vector<double>{0.25, 0.50, 0.25});
    
    // LowVolFav: stronger tilt to low-vol but cap HighVol ≤ 1/3
    mixes.emplace_back("LowVolFav(0.50,0.35,0.15)",
		       std::vector<double>{0.50, 0.35, 0.15});

    // EvenMinusHV: evenly tilted away from HighVol (≤ 1/3)
    mixes.emplace_back("EvenMinusHV(0.35,0.35,0.30)",
		       std::vector<double>{0.35, 0.35, 0.30});

    // Optional LongRun from IS bar labels (shrunk 25% toward equal; 1% bucket floor)
    if (!baselineRoc.empty())
      {
	// Pull long-run mix, add mild shrinkage to stabilize small IS samples
	// (was 0.0 before; 0.25 is a good “forgetful” prior for n~hundreds of bars)
	std::vector<double> w = computeLongRunMixWeights(
							 baselineRoc, kRegimeVolWindow, /*shrinkToEqual=*/0.25);
	
	// Enforce a small floor and renormalize (was effectively ~2% before)
	w = clip_and_normalize(std::move(w), /*floor=*/0.01);

	// Pretty label with two decimals
	std::ostringstream name;
	name.setf(std::ios::fixed); name << std::setprecision(2);
	name << "LongRun(" << w[0] << "," << w[1] << "," << w[2] << ")";
	
	mixes.emplace_back(name.str(), w);

	os << "   [RegimeMix] LongRun weights (shrunk 25%, floored 1%): ("
	   << std::fixed << std::setprecision(2)
	   << w[0] << ", " << w[1] << ", " << w[2] << ")\n";
      }

    // 6) Adapt mixes & labels to the regimes actually present
    std::vector<int> compactLabels;
    std::vector<palvalidator::analysis::RegimeMix> adaptedMixes;

    if (!adaptMixesToPresentRegimes(tradeLabels, mixes, compactLabels, adaptedMixes, os))
      {
	return FilterDecision::Pass(); // Non-gating skip
      }

    // 7) Policy: require ≥ 50% of mixes to pass; min bars per regime ≈ L + 5
    const double mixPassFrac = 0.50;
    const std::size_t minBarsPerRegime = static_cast<std::size_t>(std::max<std::size_t>(2, bootstrap.blockLength + 5));
    palvalidator::analysis::RegimeMixConfig cfg(adaptedMixes, mixPassFrac, minBarsPerRegime);

    // 8) Execute BOTH regime-mix stresses (stationary + fixed-L)
    using NumT = Num;
    using Rng  = randutils::mt19937_rng;

    using RunnerStationary =
      palvalidator::analysis::RegimeMixStressRunner<
	NumT, Rng, palvalidator::resampling::RegimeMixStationaryResampler>;

    using RunnerFixed =
      palvalidator::analysis::RegimeMixStressRunner<
	NumT, Rng, palvalidator::resampling::RegimeMixBlockResampler>;

    // --- Annualization (NEW): bars/year via λ × medianHoldBars -------------------
    // Prefer the bootstrap-propagated factor if it’s already set to bars/year.
    // Otherwise compute it here from backtester: lambda (trades/year) × median hold.
    double annUsed = 0.0;

    // 1) Pull λ (trades/year) and median hold (bars)
    double lambdaTradesPerYear = 0.0;
    unsigned int medianHoldBars = 0;

    if (ctx.backtester) {
      try {
	lambdaTradesPerYear = ctx.backtester->getEstimatedAnnualizedTrades(); // λ
	medianHoldBars = ctx.backtester
	  ->getClosedPositionHistory()
	  .getMedianHoldingPeriod();                         // bars/trade
      } catch (...) {
	// leave defaults; we’ll fall back below if needed
      }
    }

    // 2) If the bootstrap stage already published bars/year, prefer it.
    //    (In the updated pipeline, bootstrap.annFactorUsed == barsPerYear.)
    if (bootstrap.annFactorUsed > 0.0)
      {
	annUsed = bootstrap.annFactorUsed;
      }
    else
      {
	// Compute bars/year from λ × medianHoldBars (fallback)
	const double barsPerYear = lambdaTradesPerYear * static_cast<double>(medianHoldBars);
	if (barsPerYear > 0.0)
	  {
	    annUsed = barsPerYear;
	  }
	else if (ctx.annualizationFactor > 0.0)
	  {
	    // Final fallback to whatever the context carried (legacy path)
	    annUsed = ctx.annualizationFactor;
	    os << "   [RegimeMix] Warning: λ×medianHoldBars unavailable; "
	      "falling back to ctx.annualizationFactor = " << annUsed << "\n";
	  }
	else
	  {
	    os << "   [RegimeMix] Warning: could not determine bars/year; results may be unscaled.\n";
	  }
      }

    // 3) Log clearly (no 'p' participation; this is now bars/year)
    if (annUsed > 0.0)
      {
	os << "   [RegimeMix] annualization (bars/year via λ×medianHoldBars) = "
	   << annUsed
	   << "  [λ=" << lambdaTradesPerYear
	   << ", medianHoldBars=" << medianHoldBars << "]\n";
      }
 
    ValidationPolicy policy(hurdle.finalRequiredReturn);

    RunnerStationary runnerStat(cfg,
                                bootstrap.blockLength,
                                mNumResamples,
                                mConfidenceLevel.getAsDouble(),
                                annUsed,
                                policy);

    auto resStat = runnerStat.run(ctx.highResReturns, compactLabels, os);

    RunnerFixed runnerFixed(cfg,
                            bootstrap.blockLength,
                            mNumResamples,
                            mConfidenceLevel.getAsDouble(),
                            annUsed,
                            policy);

    auto resFixed = runnerFixed.run(ctx.highResReturns, compactLabels, os);

    // 9) AND gate with small forgiveness for strong stationary pass
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
      const double hurdleDec = hurdle.finalRequiredReturn.getAsDouble(); // decimal (e.g., 0.08 for 8%)
      return 10000.0 * (lb - hurdleDec);
    }();

    const double margin_bps = 50.0; // 50 bps forgiveness threshold
    const bool strongStatPass = passStat && (stationaryMedianOverHurdle_bps >= margin_bps);

    const bool regimeMixPass = (passStat && passFixed) || (passStat && !passFixed && strongStatPass);

    // Diagnostics: count flips and max |ΔLB|
    auto to_map = [](const auto& details) {
      using DetailT = typename std::decay_t<decltype(details)>::value_type; // MixResult
      std::unordered_map<std::string, DetailT> m;
      m.reserve(details.size());
      for (const auto& d : details) m.emplace(d.mixName(), d);
      return m;
    };
    auto statMap  = to_map(resStat.perMix());
    auto fixedMap = to_map(resFixed.perMix());

    std::size_t flipCount = 0;
    NumT maxAbsDelta(0);
    for (const auto& kv : statMap)
      {
	const auto it = fixedMap.find(kv.first);
	if (it == fixedMap.end()) continue;
	const auto& sDetail = kv.second;
	const auto& fDetail = it->second;
	if (sDetail.pass() != fDetail.pass()) ++flipCount;

	const NumT delta = (sDetail.annualizedLowerBound() - fDetail.annualizedLowerBound());
	if (num::abs(delta) > maxAbsDelta) maxAbsDelta = num::abs(delta);
      }

    os << "      [RegimeMix] Gate=AND (+forgiveness "
       << margin_bps << "bps): stationary=" << (passStat ? "PASS" : "FAIL")
       << " fixed-L=" << (passFixed ? "PASS" : "FAIL")
       << " | stationary median over hurdle = " << stationaryMedianOverHurdle_bps << " bps"
       << " | flips=" << flipCount
       << " | max |ΔLB| = " << (100.0 * num::to_double(maxAbsDelta)) << "%\n";

    if (!regimeMixPass)
      {
	os << "   ✗ Regime-mix sensitivity FAIL (AND gate).";
	if (passStat && !passFixed && !strongStatPass) os << " Reason: fixed-L veto.";
	if (!passStat && passFixed)                    os << " Reason: stationary veto.";
	if (!passStat && !passFixed)                   os << " Reason: both failed.";
	os << "\n";

	// Enumerate failing mixes from the stationary run (primary)
	std::vector<std::string> failed;
	for (const auto& mx : resStat.perMix()) if (!mx.pass()) failed.push_back(mx.mixName());
	if (!failed.empty()) {
	  os << "     Failing mixes (stationary): ";
	  for (std::size_t i = 0; i < failed.size(); ++i)
	    os << (i ? ", " : "") << failed[i];
	  os << "\n";
	}

	return FilterDecision::Fail(FilterDecisionType::FailRegimeMix, "Failed regime-mix stress");
      }

    return FilterDecision::Pass();
  }  
} // namespace palvalidator::filtering::stages
