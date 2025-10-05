#include "filtering/stages/LSensitivityStage.h"
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::StationaryBlockResampler;
  using mkc_timeseries::GeoMeanStat;
  using mkc_timeseries::BCaBootStrap;
  using mkc_timeseries::BCaAnnualizer;

  static std::vector<size_t> makeDefaultLGrid(size_t Lcenter, size_t n, size_t Lcap)
  {
    std::vector<size_t> g = {2,3,4,5,6,8,10, Lcenter > 0 ? Lcenter : 2, 2 * std::max<size_t>(2, Lcenter)};
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
    const size_t Lc = std::max<size_t>(2, std::min(Lcenter, hardCap));
    if (!std::binary_search(g.begin(), g.end(), Lc)) {
      g.insert(std::lower_bound(g.begin(), g.end(), Lc), Lc);
    }
    return g;
  }

  LSensitivityStage::LSensitivityStage(const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& cfg,
                                       unsigned int numResamples,
                                       const Num& confidenceLevel)
    : mCfg(cfg)
    , mNumResamples(numResamples)
    , mConfidenceLevel(confidenceLevel)
  {}

  LSensitivityResultSimple LSensitivityStage::execute(const StrategyAnalysisContext& ctx,
                                                      size_t L_cap,
                                                      double annualizationFactor,
                                                      const Num& finalRequiredReturn,
                                                      std::ostream& os) const
  {
    LSensitivityResultSimple R;
    const size_t n = ctx.highResReturns.size();
    if (n < 20) {
      os << "      [L-grid] Skipped (n<20).\n";
      return R;
    }

    const size_t hardCap = std::max<size_t>(2, std::min(L_cap, n - 1));
    std::vector<size_t> grid;
    if (!mCfg.Lgrid.empty())
    {
      grid = mCfg.Lgrid;
      grid.erase(std::remove_if(grid.begin(), grid.end(),
                                [&](size_t L){ return L < 2 || L >= n || L > hardCap; }),
                 grid.end());
      std::sort(grid.begin(), grid.end());
      grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
      const size_t Lc = std::max<size_t>(2, std::min(static_cast<size_t>(ctx.blockLength), hardCap));
      if (!grid.empty() && !std::binary_search(grid.begin(), grid.end(), Lc))
        grid.insert(std::lower_bound(grid.begin(), grid.end(), Lc), Lc);
      else if (grid.empty())
        grid.push_back(Lc);
    }
    else
    {
      grid = makeDefaultLGrid(ctx.blockLength, n, hardCap);
    }

    if (grid.empty()) {
      os << "      [L-grid] No feasible L values after capping.\n";
      return R;
    }

    GeoMeanStat<Num> statGeo;
    Num minLb = Num(std::numeric_limits<double>::infinity());
    size_t L_at_min = 0;
    size_t passCount = 0;
    std::vector<Num> lbs; lbs.reserve(grid.size());
    R.ran = true;

    std::vector<std::pair<size_t, Num>> perL;
    for (size_t L : grid) {
      StationaryBlockResampler<Num> sampler(L);
      BCaBootStrap<Num, StationaryBlockResampler<Num>> bcaGeo(ctx.highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, sampler);
      const Num lbGeoPeriod = bcaGeo.getLowerBound();
      BCaAnnualizer<Num> annualizer(bcaGeo, annualizationFactor);
      const Num lbGeoAnn = annualizer.getAnnualizedLowerBound();
      R.numTested++;
      if (lbGeoAnn < minLb) { minLb = lbGeoAnn; L_at_min = L; }
      if (lbGeoAnn > finalRequiredReturn) ++passCount;
      lbs.push_back(lbGeoAnn);
      perL.emplace_back(L, lbGeoAnn);
    }

    R.minLbAnn = minLb;
    R.L_at_min = L_at_min;
    R.numPassed = passCount;

    // compute relVar
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

    const double frac = (grid.empty() ? 0.0 : double(passCount) / double(grid.size()));
    bool pass = (frac >= mCfg.minPassFraction);
    if (pass && mCfg.minGapTolerance > 0.0) {
      const Num gap = finalRequiredReturn - minLb;
      if (gap > Num(mCfg.minGapTolerance)) pass = false;
    }
    R.pass = pass;

    os << "      [L-grid] Tested L = ";
    for (size_t i = 0; i < grid.size(); ++i) { os << grid[i]; if (i + 1 < grid.size()) os << ", "; }
    os << "\n";

    for (const auto& kv : perL) {
      const auto L = kv.first;
      const auto lbAnn = kv.second;
      os << "        L=" << L << ": Ann GM LB = "
         << (lbAnn * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%"
         << (lbAnn > finalRequiredReturn ? "  (PASS)" : "  (FAIL)") << "\n";
    }

    os << "        → pass fraction = " << (100.0 * frac) << "%, "
       << "min LB at L=" << R.L_at_min
       << ", min LB = " << (R.minLbAnn * mkc_timeseries::DecimalConstants<Num>::DecimalOneHundred) << "%\n";

    return R;
  }

} // namespace palvalidator::filtering::stages