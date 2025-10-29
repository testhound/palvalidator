#include "filtering/stages/RegimeMixStage.h"
#include "BackTester.h"
#include "Security.h"
#include "RegimeLabeler.h"
#include "RegimeMixStress.h"
#include "RegimeMixStationaryResampler.h"
#include "RegimeMixStressRunner.h"
#include "BarAlignedSeries.h"
#include "TimeSeriesCsvReader.h"
#include "DecimalConstants.h"
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

  constexpr std::size_t kRegimeVolWindow = 20;

  // Helper: Build long-run mix weights (from legacy lines 615-652)
  static std::vector<double>
  computeLongRunMixWeights(const std::vector<Num>& baselineReturns,
                           std::size_t volWindow,
                           double shrinkToEqual)
  {
    if (baselineReturns.size() < volWindow + 2)
      {
	// Fallback: equal weights if baseline is too short
	return {1.0/3.0, 1.0/3.0, 1.0/3.0};
      }

    VolTercileLabeler<Num> labeler(volWindow);
    const std::vector<int> z = labeler.computeLabels(baselineReturns);

    std::array<double, 3> cnt{0.0, 0.0, 0.0};
    for (int zi : z)
      {
	if (zi >= 0 && zi <= 2) cnt[static_cast<std::size_t>(zi)] += 1.0;
      }
    const double n = std::max(1.0, cnt[0] + cnt[1] + cnt[2]);
    std::array<double, 3> p{cnt[0]/n, cnt[1]/n, cnt[2]/n};

    // Shrink toward equal to avoid over-committing
    const double lam = std::clamp(shrinkToEqual, 0.0, 1.0);
    std::array<double, 3> w{
      (1.0 - lam) * p[0] + lam * (1.0/3.0),
      (1.0 - lam) * p[1] + lam * (1.0/3.0),
      (1.0 - lam) * p[2] + lam * (1.0/3.0)
    };

    // Clip tiny buckets and renormalize
    const double eps = 0.02; // min 2% mass per bucket
    for (double& v : w) v = std::max(v, eps);
    const double s = w[0] + w[1] + w[2];
    return {w[0]/s, w[1]/s, w[2]/s};
  }

  // Helper: Adapt mixes to present regimes (from legacy lines 870-967)
  static bool adaptMixesToPresentRegimes(
					 const std::vector<int>& tradeLabels,
					 const std::vector<RegimeMix>& mixesIn,
					 std::vector<int>& labelsOut,
					 std::vector<RegimeMix>& mixesOut,
					 std::ostream& os)
  {
    // 1) Detect which of {0,1,2} appear and build old→new id map
    std::array<int, 3> present{0, 0, 0};
    for (int z : tradeLabels)
      {
	if (0 <= z && z <= 2)
	  {
	    present[static_cast<std::size_t>(z)] = 1;
	  }
      }

    std::array<int, 3> old2new{-1, -1, -1};
    int next = 0;
    for (int s = 0; s < 3; ++s)
      {
	if (present[static_cast<std::size_t>(s)] == 1)
	  {
	    old2new[static_cast<std::size_t>(s)] = next++;
	  }
      }
    const int Sobs = next;

    // If fewer than 2 regimes present, the stress is uninformative → skip (non-gating)
    if (Sobs < 2)
      {
	os << "   [RegimeMix] Skipped (only " << Sobs
	   << " regime present in OOS trades; mix stress uninformative).\n";
	return false;
      }

    // 2) Remap labels to compact 0..Sobs-1
    labelsOut.clear();
    labelsOut.reserve(tradeLabels.size());
    for (int z : tradeLabels)
      {
	if (!(0 <= z && z <= 2))
	  {
	    os << "   [RegimeMix] Skipped (unexpected label " << z << ").\n";
	    return false;
	  }
	const int m = old2new[static_cast<std::size_t>(z)];
	if (m < 0)
	  {
	    os << "   [RegimeMix] Skipped (label remap failed).\n";
	    return false;
	  }
	labelsOut.push_back(m);
      }

    // 3) Adapt each mix's 3 weights to observed regimes and renormalize
    mixesOut.clear();
    mixesOut.reserve(mixesIn.size());

    for (const auto& mx : mixesIn)
      {
	const std::string& nm = mx.name();
	const std::vector<double>& w3 = mx.weights();

	std::vector<double> wS(static_cast<std::size_t>(Sobs), 0.0);
	double sum = 0.0;

	for (int old = 0; old < 3; ++old)
	  {
	    const int nw = old2new[static_cast<std::size_t>(old)];
	    if (nw >= 0)
	      {
		const double w = (old < static_cast<int>(w3.size())) ? w3[static_cast<std::size_t>(old)] : 0.0;
		wS[static_cast<std::size_t>(nw)] += w;
		sum += w;
	      }
	  }

	if (sum <= 0.0)
	  {
	    // Fallback to equal within observed regimes
	    const double eq = 1.0 / static_cast<double>(Sobs);
	    std::fill(wS.begin(), wS.end(), eq);
	  }
	else
	  {
	    for (double& v : wS) v /= sum;
	  }

	mixesOut.emplace_back(nm, wS);
      }

    return true;
  }

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

    RunnerStationary runnerStat(cfg,
				bootstrap.blockLength,
				mNumResamples,
				mConfidenceLevel.getAsDouble(),
				ctx.annualizationFactor,
				hurdle.finalRequiredReturn);

    auto resStat = runnerStat.run(ctx.highResReturns, compactLabels, os);

    RunnerFixed runnerFixed(cfg,
			    bootstrap.blockLength,
			    mNumResamples,
			    mConfidenceLevel.getAsDouble(),
			    ctx.annualizationFactor,
			    hurdle.finalRequiredReturn);

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
