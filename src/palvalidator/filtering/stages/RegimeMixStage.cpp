#include "filtering/stages/RegimeMixStage.h"
#include "BackTester.h"
#include "Security.h"
#include "RegimeLabeler.h"
#include "RegimeMixStress.h"
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
    try
    {
      BarAlignedSeries<Num> aligner(kRegimeVolWindow);
      tradeLabels = aligner.buildTradeAlignedLabels(oosClose, closed);
    }
    catch (const std::exception& e)
    {
      // Operational/alignment issue → do not fail strategy; just skip this gate.
      os << "   [RegimeMix] Skipped (label alignment failed: " << e.what() << ").\n";
      return FilterDecision::Pass(); // Non-gating skip
    }

    if (tradeLabels.size() != ctx.highResReturns.size())
    {
      // Labels must align 1:1 with the sparse trade-sequence returns
      os << "   [RegimeMix] Skipped (labels length "
         << tradeLabels.size() << " != returns length "
         << ctx.highResReturns.size() << ").\n";
      return FilterDecision::Pass(); // Non-gating skip
    }

    // 4) Build LongRun baseline: 1-bar ROC on in-sample close series
    auto inSampleTS = FilterTimeSeries(*ctx.baseSecurity->getTimeSeries(), ctx.inSampleDates);
    auto insampleROC = RocSeries(inSampleTS.CloseTimeSeries(), /*period=*/1);
    auto baselineRoc = insampleROC.getTimeSeriesAsVector();

    if (baselineRoc.size() < 3)
    {
      // Too short for a meaningful LongRun mix (labeler needs window+2).
      // We still proceed; stress can operate with Equal+DownFav only.
      os << "   [RegimeMix] Note: in-sample ROC short ("
         << baselineRoc.size() << " bars). LongRun may be skipped.\n";
    }

    // 5) Build target mixes: Equal + DownFav; optionally LongRun
    std::vector<RegimeMix> mixes;
    mixes.emplace_back("Equal(1/3,1/3,1/3)", std::vector<double>{1.0/3.0, 1.0/3.0, 1.0/3.0});
    mixes.emplace_back("DownFav(0.3,0.4,0.3)", std::vector<double>{0.30, 0.40, 0.30});

    if (!baselineRoc.empty())
    {
      const std::vector<double> w =
        computeLongRunMixWeights(baselineRoc, kRegimeVolWindow, /*shrinkToEqual=*/0.0);

      if (!w.empty())
      {
        mixes.emplace_back("LongRun", w);

        os << "   [RegimeMix] LongRun weights = ("
           << std::fixed << std::setprecision(2)
           << w[0] << ", " << w[1] << ", " << w[2] << ")\n";
      }
      else
      {
        os << "   [RegimeMix] LongRun baseline too short; skipping.\n";
      }
    }

    // 6) Adapt mixes & labels to the regimes actually present
    std::vector<int> compactLabels;
    std::vector<RegimeMix> adaptedMixes;

    if (!adaptMixesToPresentRegimes(tradeLabels, mixes, compactLabels, adaptedMixes, os))
    {
      // Uninformative or alignment issue → skip (non-gating)
      return FilterDecision::Pass();
    }

    // 7) Policy: require ≥ 50% of mixes to pass; min bars per regime ≈ L + 5
    const double mixPassFrac = 0.50;
    const std::size_t minBarsPerRegime = static_cast<std::size_t>(std::max<std::size_t>(2, bootstrap.blockLength + 5));
    RegimeMixConfig cfg(adaptedMixes, mixPassFrac, minBarsPerRegime);

    // 8) Execute regime-mix stress
    RegimeMixStressRunner<Num> runner(cfg,
                                      bootstrap.blockLength,
                                      mNumResamples,
                                      mConfidenceLevel.getAsDouble(),
                                      ctx.annualizationFactor,
                                      hurdle.finalRequiredReturn);

    const auto res = runner.run(ctx.highResReturns, compactLabels, os);

    if (!res.overallPass())
    {
      os << "   ✗ Regime-mix sensitivity FAIL: insufficient robustness across mixes.\n";

      // Print which mixes failed
      std::vector<std::string> failed;
      for (const auto& mx : res.perMix())
      {
        if (!mx.pass())
        {
          failed.push_back(mx.mixName());
        }
      }
      if (!failed.empty())
      {
        os << "     Failing mixes: ";
        for (std::size_t i = 0; i < failed.size(); ++i)
        {
          os << (i ? ", " : "") << failed[i];
        }
        os << "\n";
      }

      os << "   ✗ Strategy filtered out due to Regime-mix sensitivity.\n\n";
      return FilterDecision::Fail(FilterDecisionType::FailRegimeMix, "Failed regime-mix stress");
    }

    return FilterDecision::Pass();
  }

} // namespace palvalidator::filtering::stages