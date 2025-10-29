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


  LSensitivityStage::LSensitivityStage(const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& cfg,
                                       unsigned int numResamples,
                                       const Num& confidenceLevel)
    : mCfg(cfg)
    , mNumResamples(numResamples)
    , mConfidenceLevel(confidenceLevel)
  {}

  /**
   * @brief Builds the default grid of block lengths (L values) used in the
   *        BCa bootstrap sensitivity analysis.
   *
   * The grid determines which mean block lengths (L) are tested during the
   * "L-sensitivity" stage of the filtering pipeline. Each candidate L value
   * represents the expected block size of dependence in the stationary
   * bootstrap, and the results are compared to evaluate the robustness of
   * the geometric-mean lower bound across different assumptions of serial
   * dependence.
   *
   * ### Construction Rules
   * - Begins with a fixed base set: {2, 3, 4, 5, 6, 8, 10}.
   * - Inserts the computed @p Lcenter (typically the hybrid block length chosen
   *   from the median holding period or n^(1/3)).
   * - Adds the doubled value (2 × Lcenter) and optionally ±1 around center.
   * - Includes @p n^(1/3) as a theoretical MSE-optimal heuristic.
   * - Deduplicates, sorts, and caps each element so that:
   *   - 2 ≤ L ≤ min(Lcap, n-1)
   *   - Duplicate or invalid values are removed.
   *
   * The resulting vector is used by LSensitivityStage::execute to iterate over
   * multiple block-length configurations and measure the variation of BCa
   * bootstrap estimates with respect to L.
   *
   * @param Lcenter  The central or nominal block length (typically hybrid).
   * @param n        Number of observations (vector size of returns).
   * @param Lcap     Maximum allowed block length for this stage.
   * @return std::vector<size_t> Sorted and deduplicated list of candidate L values.
   */
  static std::vector<size_t> makeDefaultLGrid(size_t Lcenter, size_t n, size_t Lcap)
  {
    const size_t hardCap = std::max<size_t>(2, std::min(Lcap, n - 1));
    const size_t Lcube = (n > 0) ? static_cast<size_t>(std::lround(std::pow(double(n), 1.0/3.0))) : 0;

    std::vector<size_t> g = {
      2,3,4,5,6,8,10,
      (Lcenter > 0 ? Lcenter : 2),
      2 * std::max<size_t>(2, Lcenter),
      Lcube
    };

    // Optional “neighborhood” around center:
    if (Lcenter > 0)
      {
      g.push_back(Lcenter + 1);
      if (Lcenter > 2)
	g.push_back(Lcenter - 1);
    }

    for (auto& L : g)
      {
	if (L < 2)
	  L = 2;
	if (L > hardCap)
	  L = hardCap;
      }

    std::sort(g.begin(), g.end());
    g.erase(std::unique(g.begin(), g.end()), g.end());
    g.erase(std::remove_if(g.begin(), g.end(),
			   [&](size_t L){ return L < 2 || L >= n || L > hardCap; }),
	    g.end());

    const size_t Lc = std::max<size_t>(2, std::min(Lcenter, hardCap));

    if (Lc >= 2 && !std::binary_search(g.begin(), g.end(), Lc)) {
      g.insert(std::lower_bound(g.begin(), g.end(), Lc), Lc);
    }
    return g;
  }

  /**
   * @brief Executes the block-length sensitivity (L-grid) bootstrap stage for a
   *        single trading strategy.
   *
   * This stage quantifies how sensitive the BCa bootstrap geometric-mean lower
   * bound is to the assumed block length (L) in the stationary bootstrap.
   * It helps validate the robustness of the strategy’s statistical performance
   * under varying assumptions about trade-to-trade dependence.
   *
   * ### Algorithm
   * 1. Retrieve the high-resolution percent returns from the backtest context.
   * 2. Construct a candidate L grid via makeDefaultLGrid().
   * 3. For each L in the grid:
   *    - Run a BCa bootstrap using `GeoMeanStat` (log(1+r)-based statistic).
   *    - Record the annualized geometric-mean lower bound and confidence
   *      interval width.
   * 4. Compute summary diagnostics across the grid:
   *    - Median, min, max, and relative variance of the geometric-mean lower
   *      bounds.
   *    - Pass/fail ratios and robustness flags.
   * 5. Store the aggregated results in the analysis context for later use
   *    (e.g., divergence analysis and meta-filtering).
   *
   * ### Notes
   * - Uses percent returns as input; `GeoMeanStat` converts to log(1+r)
   *   internally.
   * - The BCa bootstrap sampler is stationary with mean block length L.
   * - The grid typically spans 2–12 for small-N strategies (20–35 trades).
   * - Designed to detect if performance significance depends strongly on L,
   *   indicating fragile or regime-specific dependence.
   *
   * @param ctx  StrategyAnalysisContext containing backtest results, returns,
   *             and configuration for this strategy.
   */
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
      if (lbGeoAnn < minLb)
	{
	  minLb = lbGeoAnn;
	  L_at_min = L;
	}

      if (lbGeoAnn > finalRequiredReturn)
	++passCount;

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
    if (mu != Num(0))
      {
	Num ss = Num(0);
	for (auto x: lbs)
	  {
	    Num d = x - mu;
	    ss += d*d;
	  }

	const Num var = ss / Num(lbs.size());
	R.relVar = (var / (mu * mu)).getAsDouble();
      }
    else
      {
	R.relVar = 0.0;
      }

    const double frac = (grid.empty() ? 0.0 : double(passCount) / double(grid.size()));
    bool pass = (frac >= mCfg.minPassFraction);

    if (pass && mCfg.minGapTolerance > 0.0)
      {
	const Num gap = finalRequiredReturn - minLb;
	if (gap > Num(mCfg.minGapTolerance))
	  pass = false;
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
