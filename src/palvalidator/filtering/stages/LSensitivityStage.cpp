#include "filtering/stages/LSensitivityStage.h"
#include "TradingBootstrapFactory.h"
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "StatUtils.h"
#include "SmallNBootstrapHelpers.h"
#include "Annualizer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <limits>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::StationaryBlockResampler;
  using mkc_timeseries::GeoMeanStat;
  using mkc_timeseries::BCaBootStrap;
  using mkc_timeseries::BCaAnnualizer;


  LSensitivityStage::LSensitivityStage(const palvalidator::filtering::PerformanceFilter::LSensitivityConfig& cfg,
                                       unsigned int numResamples,
                                       const Num& confidenceLevel,
                                       BootstrapFactory& bootstrapFactory)
    : mCfg(cfg)
    , mNumResamples(numResamples)
    , mConfidenceLevel(confidenceLevel)
    , mBootstrapFactory(bootstrapFactory)
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
  // Tightened for very small n: local {Lc-1, Lc, Lc+1} when n <= 32
  static std::vector<size_t> makeDefaultLGrid(size_t Lcenter, size_t n, size_t Lcap)
  {
    // Enforce a sane cap: 2 <= L <= min(Lcap, n-1)
    const size_t hardCap = std::max<size_t>(2, std::min(Lcap, (n > 0 ? n - 1 : 0)));
    const size_t Lc      = (Lcenter > 0) ? std::min(std::max<size_t>(2, Lcenter), hardCap) : 2;
    const size_t Lcube   = (n > 0) ? static_cast<size_t>(std::lround(std::pow(double(n), 1.0 / 3.0))) : 0;

    std::vector<size_t> g;

    // Small-sample branch: probe only a tight local neighborhood around center
    if (n >= 20 && n <= 32)
      {
        const size_t LcEff = (Lcenter > 0 ? Lcenter : 2);
        const size_t Lm1   = (LcEff > 2 ? LcEff - 1 : 2);
        const size_t Lp1   = (LcEff + 1 <= hardCap ? LcEff + 1 : hardCap);

        g = { std::max<size_t>(2, Lm1), std::max<size_t>(2, LcEff), std::min(hardCap, Lp1) };
      }
    else
      {
        // Broader grid for larger samples
        g = {
	  2, 3, 4, 5, 6, 8, 10,
	  (Lcenter > 0 ? Lcenter : 2),
	  2 * std::max<size_t>(2, Lcenter),
	  Lcube
        };

        // Optional local neighborhood to probe around center
        if (Lcenter > 0)
	  {
            g.push_back(Lcenter + 1);
            if (Lcenter > 2)
	      {
                g.push_back(Lcenter - 1);
	      }
	  }
      }

    // Cap to [2, hardCap]
    for (auto &L : g)
      {
        if (L < 2)
	  {
            L = 2;
	  }
        if (L > hardCap)
	  {
            L = hardCap;
	  }
      }

    // Sort, unique, and remove invalids (also require L < n)
    std::sort(g.begin(), g.end());
    g.erase(std::unique(g.begin(), g.end()), g.end());
    g.erase(std::remove_if(g.begin(), g.end(),
                           [&](size_t L)
                           {
			     return L < 2 || (n > 0 && L >= n) || L > hardCap;
                           }),
            g.end());

    // Ensure the capped center Lc is present
    if (!std::binary_search(g.begin(), g.end(), Lc))
      {
        g.insert(std::lower_bound(g.begin(), g.end(), Lc), Lc);
      }

    // Failsafe
    if (g.empty())
      {
        g.push_back(2);
      }

    return g;
  }
  
  std::vector<size_t> LSensitivityStage::buildLGrid(const StrategyAnalysisContext& ctx,
                                                      size_t hardCap,
                                                      std::ostream& os) const
  {
    const size_t n = ctx.highResReturns.size();
    std::vector<size_t> grid;

    if (!mCfg.Lgrid.empty())
    {
      // Use user-provided grid
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
      // Use default grid construction
      grid = makeDefaultLGrid(ctx.blockLength, n, hardCap);
    }

    if (grid.empty())
    {
      os << "      [L-grid] No feasible L values after capping.\n";
    }

    return grid;
  }

  Num LSensitivityStage::runSmallNBootstrapForL(const StrategyAnalysisContext& ctx,
                                                  size_t blockLength,
                                                  double annualizationFactor,
                                                  bool heavyTails,
                                                  std::ostream& os) const
  {
    namespace bh = palvalidator::bootstrap_helpers;
    using GeoStat = mkc_timeseries::GeoMeanStat<Num>;
    using mkc_timeseries::DecimalConstants;

    if (!ctx.clonedStrategy)
    {
      throw std::runtime_error("LSensitivityStage::runSmallNBootstrapForL: clonedStrategy is null");
    }

    // Consolidated small-N runner: picks IID vs Block (small L), runs m/n & BCa, returns min
    auto s = bh::conservative_smallN_lower_bound<Num, GeoStat>(
      ctx.highResReturns,
      blockLength,
      annualizationFactor,
      mConfidenceLevel.getAsDouble(),
      mNumResamples,
      /*rho_m*/ -1.0,
      *ctx.clonedStrategy,
      mBootstrapFactory,
      &os, /*stageTag*/2, 0,
      heavyTails);

    const Num lbAnn = s.ann_lower;

    os << "        L=" << blockLength << " [SmallN: "
       << (s.resampler_name ? s.resampler_name : "n/a")
       << ", m_sub=" << s.m_sub
       << ", L_small=" << s.L_used
       << "] → Ann GM LB = "
       << (lbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

    return lbAnn;
  }

  Num LSensitivityStage::runStandardBootstrapForL(const StrategyAnalysisContext& ctx,
                                                    size_t blockLength,
                                                    double annualizationFactor,
                                                    std::ostream& os) const
  {
    using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
    using GeoStat = mkc_timeseries::GeoMeanStat<Num>;
    using mkc_timeseries::DecimalConstants;

    if (!ctx.clonedStrategy)
    {
      throw std::runtime_error("LSensitivityStage::runStandardBootstrapForL: clonedStrategy is null");
    }

    // Larger-N fallback: BCa(Geo) with full stationary block resampler at L
    BCaResampler sampler(blockLength);
    std::function<Num(const std::vector<Num>&)> geoFn = GeoStat();

    auto bcaGeo = mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
                                                 mNumResamples,
                                                 mConfidenceLevel.getAsDouble(),
                                                 geoFn,
                                                 std::move(sampler),
                                                 *ctx.clonedStrategy,
                                                 /*stageTag*/2, /*L*/blockLength, /*fold*/0);

    const Num lbAnn = mkc_timeseries::BCaAnnualizer<Num>(bcaGeo, annualizationFactor)
                        .getAnnualizedLowerBound();

    os << "        L=" << blockLength << " [BCa]: Ann GM LB = "
       << (lbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

    return lbAnn;
  }

  void LSensitivityStage::computeGridStatistics(const std::vector<LGridPoint>& gridResults,
                                                 LSensitivityResultSimple& result) const
  {
    if (gridResults.empty())
    {
      return;
    }

    // Extract lower bounds for statistics
    std::vector<Num> lbs;
    lbs.reserve(gridResults.size());
    for (const auto& point : gridResults)
    {
      lbs.push_back(point.getAnnualizedLowerBound());
    }

    // Compute relative variance over annualized LBs
    auto meanFn = [](const std::vector<Num>& v) {
      Num s = Num(0);
      for (auto x : v) s += x;
      return s / Num(v.size());
    };

    const Num mu = meanFn(lbs);
    Num ss = Num(0);
    for (auto x : lbs)
    {
      const Num d = x - mu;
      ss += d * d;
    }

    const Num var = ss / Num(lbs.size());
    const Num eps = Num(1e-8);
    const Num mu2 = std::max(mu * mu, eps);
    result.relVar = (var / mu2).getAsDouble();
  }

  bool LSensitivityStage::evaluatePassCriteria(const LSensitivityResultSimple& result,
                                                const Num& finalRequiredReturn,
                                                size_t gridSize) const
  {
    if (gridSize == 0)
    {
      return false;
    }

    const double frac = static_cast<double>(result.numPassed) / static_cast<double>(gridSize);
    bool pass = (frac >= mCfg.minPassFraction);

    if (pass && mCfg.minGapTolerance > 0.0)
    {
      const Num gap = finalRequiredReturn - result.minLbAnn;
      if (gap > Num(mCfg.minGapTolerance))
      {
        pass = false;
      }
    }

    return pass;
  }

  void LSensitivityStage::logGridSummary(const std::vector<size_t>& grid,
                                          const std::vector<LGridPoint>& gridResults,
                                          const LSensitivityResultSimple& result,
                                          const Num& finalRequiredReturn,
                                          std::ostream& os) const
  {
    using mkc_timeseries::DecimalConstants;

    // Log tested L values
    os << "      [L-grid] Tested L = ";
    for (size_t i = 0; i < grid.size(); ++i)
    {
      os << grid[i];
      if (i + 1 < grid.size()) os << ", ";
    }
    os << "\n";

    // Log individual results
    for (const auto& point : gridResults)
    {
      const size_t L = point.getBlockLength();
      const Num lbAnn = point.getAnnualizedLowerBound();
      os << "        L=" << L << ": Ann GM LB = "
         << (lbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%"
         << (lbAnn > finalRequiredReturn ? "  (PASS)" : "  (FAIL)") << "\n";
    }

    // Log summary statistics
    const double frac = grid.empty() ? 0.0 : static_cast<double>(result.numPassed) / static_cast<double>(grid.size());
    os << "        → pass fraction = " << (100.0 * frac) << "%, "
       << "min LB at L=" << result.L_at_min
       << ", min LB = " << (result.minLbAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
  }

  /**
   * @brief Executes the block-length sensitivity (L-grid) bootstrap stage for a
   *        single trading strategy.
   *
   * This stage quantifies how sensitive the BCa bootstrap geometric-mean lower
   * bound is to the assumed block length (L) in the stationary bootstrap.
   * It helps validate the robustness of the strategy's statistical performance
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
  LSensitivityResultSimple
  LSensitivityStage::execute(const StrategyAnalysisContext& ctx,
                             size_t L_cap,
                             double annualizationFactor,
                             const Num& finalRequiredReturn,
                             std::ostream& os) const
  {
    using mkc_timeseries::DecimalConstants;

    LSensitivityResultSimple R;
    const size_t n = ctx.highResReturns.size();

    // --- Step 1: Validate sample size ---
    if (n < 20)
    {
      os << "      [L-grid] Skipped (n<20).\n";
      return R;
    }

    // --- Step 2: Analyze distribution characteristics ---
    const auto [skew, exkurt] =
      StatUtils<Num>::computeSkewAndExcessKurtosis(ctx.highResReturns);
    const bool heavy_tails_wide =
      palvalidator::bootstrap_helpers::has_heavy_tails_wide(skew, exkurt);

    // --- Step 3: Build L-grid ---
    const size_t hardCap = std::max<size_t>(2, std::min(L_cap, n - 1));
    const std::vector<size_t> grid = buildLGrid(ctx, hardCap, os);

    if (grid.empty())
    {
      return R;
    }

    // --- Step 4: Determine bootstrap method based on sample size ---
    const bool use_smallN = palvalidator::bootstrap_helpers::should_run_smallN(n, heavy_tails_wide);

    // --- Step 5: Initialize result tracking ---
    R.ran = true;
    R.numTested = 0;
    R.numPassed = 0;
    R.minLbAnn = Num(std::numeric_limits<double>::infinity());
    R.L_at_min = 0;

    std::vector<LGridPoint> gridResults;
    gridResults.reserve(grid.size());

    // --- Step 6: Run bootstrap for each L value ---
    for (size_t L : grid)
    {
      Num lbAnn;

      if (use_smallN)
      {
        lbAnn = runSmallNBootstrapForL(ctx, L, annualizationFactor, heavy_tails_wide, os);
      }
      else
      {
        lbAnn = runStandardBootstrapForL(ctx, L, annualizationFactor, os);
      }

      // Track results
      ++R.numTested;
      if (lbAnn < R.minLbAnn)
      {
        R.minLbAnn = lbAnn;
        R.L_at_min = L;
      }
      if (lbAnn > finalRequiredReturn)
      {
        ++R.numPassed;
      }

      gridResults.emplace_back(L, lbAnn);
    }

    // --- Step 7: Compute grid statistics ---
    computeGridStatistics(gridResults, R);

    // --- Step 8: Evaluate pass/fail criteria ---
    R.pass = evaluatePassCriteria(R, finalRequiredReturn, grid.size());

    // --- Step 9: Log summary ---
    logGridSummary(grid, gridResults, R, finalRequiredReturn, os);

    // --- Step 10: Cache result for downstream consumers (RobustnessAnalyzer) ---
    // This avoids redundant L-sensitivity computation in robustness checks
    if (R.ran) {
      const_cast<StrategyAnalysisContext&>(ctx).lgrid_result = R;
    }

    return R;
  }

} // namespace palvalidator::filtering::stages
