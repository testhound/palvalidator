#include "BootstrapAnalysisStage.h"
#include "BiasCorrectedBootstrap.h"
#include "MOutOfNPercentileBootstrap.h"
#include "PercentileTBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"
#include "ClosedPositionHistory.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "ParallelExecutors.h"
#include "filtering/BootstrapConfig.h" 
#include "SmallNBootstrapHelpers.h"
#include "AdaptiveRatioInternal.h"
#include "Annualizer.h"
#include <sstream>
#include <cmath>


namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::BackTester;
  using mkc_timeseries::ClosedPositionHistory;
  using mkc_timeseries::Security;
  using palvalidator::bootstrap_cfg::BootstrapFactory;
  using palvalidator::analysis::detail::estimate_left_tail_index_hill;
  
  /**
 * @brief Constructs the Bootstrap Analysis Stage.
 *
 * @param confidenceLevel The target confidence level for lower bound calculations (e.g., 0.95).
 * @param numResamples The number of bootstrap replicates to generate (B).
 * @param bootstrapFactory Reference to the factory used to instantiate specific bootstrap engines
 * (BCa, Percentile-t, etc.).
 */
  BootstrapAnalysisStage::BootstrapAnalysisStage(const Num& confidenceLevel,
  				 unsigned int numResamples,
  				 BootstrapFactory& bootstrapFactory)
    : mConfidenceLevel(confidenceLevel)
    , mNumResamples(numResamples)
    , mBootstrapFactory(bootstrapFactory)
  {}

  /**
   * @brief Determines the optimal block length (L) for stationary block bootstrapping.
   *
   * @details
   * This method employs a hybrid heuristic to capture serial dependence in the return series:
   * 1. **Economic Horizon:** Uses the median holding period of the strategy. This captures
   * dependence structural to the trading logic.
   * 2. **Statistical Horizon:** Uses the Hall/Politis $N^{1/3}$ rule of thumb for optimal MSE
   * in block bootstrapping.
   *
   * The final $L$ is the maximum of these two, clamped to a safety range (min 2, max 12)
   * to prevent excessive variance reduction in small samples.
   *
   * @param ctx The analysis context containing returns and backtester data.
   * @return size_t The calculated block length.
   */
  size_t BootstrapAnalysisStage::computeBlockLength(const StrategyAnalysisContext& ctx) const
  {
    // 1) Median holding period (economic horizon)
    unsigned int medianHoldBars = 2;
    if (ctx.backtester)
      medianHoldBars = ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();

    // 2) n^(1/3) heuristic (statistical horizon), prefer already-prepared returns
    size_t n = ctx.highResReturns.size();
    size_t lCube = (n > 0) ? static_cast<size_t>(std::lround(std::pow(static_cast<double>(n), 1.0/3.0))) : 0;

    // 3) Hybrid: max of the two, with a minimum of 2
    size_t L = std::max<size_t>(2, std::max(static_cast<size_t>(medianHoldBars), lCube));

    const size_t L_cap = 12;
    L = std::min(L, L_cap);

    return L;
  }

  /**
   * @brief Calculates the scalar factor required to convert per-period returns to annualized returns.
   *
   * @details
   * This method detects the time frame of the base security. If the data is Intraday,
   * it attempts to resolve the specific minute-duration to calculate standard market-year scaling.
   * Otherwise, it defaults to standard constants (e.g., 252 for Daily).
   *
   * @note This factor is primarily used as a fallback. The `execute` method prefers
   * calculating annualization based on trade frequency ($\lambda$) if available.
   *
   * @param ctx The analysis context.
   * @return double The annualization factor.
   */
  double BootstrapAnalysisStage::computeAnnualizationFactor(const StrategyAnalysisContext& ctx) const
  {
    // Mirror original logic: use intraday minutes when appropriate
    if (ctx.timeFrame == mkc_timeseries::TimeFrame::INTRADAY)
      {
	if (ctx.baseSecurity && ctx.baseSecurity->getTimeSeries())
	  {
	    return calculateAnnualizationFactor(ctx.timeFrame,
						ctx.baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes());
	  }
      }
    return calculateAnnualizationFactor(ctx.timeFrame);
  }

  /**
   * @brief Lazily initializes the backtester and high-resolution returns if they do not yet exist.
   *
   * @details
   * The context passed from previous stages might only contain the raw Strategy and Security.
   * This method ensures a `BackTester` is instantiated, the strategy is cloned into a
   * portfolio, and `highResReturns` (Mark-to-Market returns) are populated for analysis.
   *
   * @param ctx [in,out] The analysis context. `ctx.backtester` and `ctx.highResReturns` may be modified.
   * @param os Output stream for error logging.
   * @return true If initialization was successful or already complete.
   * @return false If an exception occurred during backtesting.
   */
  bool BootstrapAnalysisStage::initializeBacktester(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    if (ctx.backtester)
    {
      return true;
    }

    try
    {
      if (!ctx.portfolio && ctx.strategy && ctx.baseSecurity)
      {
        ctx.portfolio = std::make_shared<mkc_timeseries::Portfolio<Num>>(
          ctx.strategy->getStrategyName() + " Portfolio");
        ctx.portfolio->addSecurity(ctx.baseSecurity);
        ctx.clonedStrategy = ctx.strategy->clone2(ctx.portfolio);
        ctx.backtester = mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(
          ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
        ctx.highResReturns = ctx.backtester->getAllHighResReturns(ctx.clonedStrategy.get());
      }
      return true;
    }
    catch (const std::exception& e)
    {
      os << "Warning: BootstrapAnalysisStage failed to initialize backtester: " << e.what() << "\n";
      return false;
    }
  }

  /**
   * @brief Performs statistical profiling of the return series to determine bootstrap dispatch logic.
   *
   * @details
   * This method profiles the distribution to detect "Hidden Tail Risks" and selects the
   * appropriate combination of bootstrap engines. It employs a "Dual-Check" for heavy tails:
   *
   * 1. Global Moments: Checks Skewness (> 0.9) and Excess Kurtosis (> 1.2).
   * 2. Hill Estimator: Estimates the Pareto tail index (alpha) of the losses.
   *    If alpha <= 2.0, it signals infinite variance potential and forces the robust path
   *    even if global moments appear normal.
   *
   * Conservative Dispatch Logic:
   * - Small-N (m-out-of-n, conservative duel):
   *     • Always activated if N <= 40.
   *     • Activated if 40 < N <= 60 AND Heavy Tails (via moments or Hill) are detected.
   *     • Additionally, for 60 < N <= 80, activated when the Hill estimator alone
   *       indicates very heavy tails (alpha <= 2.0).
   *
   * - BCa Geometric:
   *     • Activated whenever we are NOT in the Small-N regime.
   *       (i.e., well-behaved tails or larger N where Hill does not flag extreme heaviness).
   *
   * - Percentile-t:
   *     • Activated for N <= 80 as a universal stabilizer to correct for
   *       skewness and second-order errors.
   *
   * @param ctx The analysis context containing the return series.
   * @param os Output stream for diagnostic logging (shows Hill Alpha and Moment values).
   * @return DistributionDiagnostics Struct containing boolean flags for engine execution.
   */
  BootstrapAnalysisStage::DistributionDiagnostics
  BootstrapAnalysisStage::analyzeDistribution(const StrategyAnalysisContext& ctx,
					      std::ostream& os) const
  {
    using mkc_timeseries::StatUtils;
    using palvalidator::bootstrap_helpers::has_heavy_tails_wide;

    const std::size_t n = ctx.highResReturns.size();

    // 1. Global Moments (kept for logging / large-N context)
    const auto [skew, exkurt] =
      StatUtils<Num>::computeSkewAndExcessKurtosis(ctx.highResReturns);

    // 2. Quantile Shape (Bowley skew + tail-span, robust at small N)
    const auto qShape =
      StatUtils<Num>::computeQuantileShape(ctx.highResReturns);
    const bool heavy_via_quantiles =
      (qShape.hasStrongAsymmetry || qShape.hasHeavyTails);

    // 3. Hill Estimator (hidden tail risk: extreme left-tail heaviness)
    //    Alpha <= 2.0 implies potential infinite-variance behavior.
    const double tailIndex = estimate_left_tail_index_hill(ctx.highResReturns);
    const bool  valid_hill   = (tailIndex > 0.0);
    const bool  heavy_via_hill = valid_hill && (tailIndex <= 2.0);

    // 4. Combined Heavy-Tail flag for dispatch
    //    For consistency with conservative_smallN_lower_bound, we treat
    //    quantile-based detection as primary, with Hill as a hard override
    //    for extreme tails. Moment-based detection is informational only.
    const bool heavy_tails = heavy_via_quantiles || heavy_via_hill;

    // 5. Small-N (m-out-of-n duel)
    //    Base rule: n <= 40, or (n <= 60 && heavy_tails).
    bool run_mn =
      palvalidator::bootstrap_helpers::should_run_smallN(n, heavy_tails);

    // Conservative extension:
    // If we are in the upper-medium regime (60 < n <= 80),
    // and Hill alone flags very heavy tails, force the Small-N path.
    if (!run_mn && heavy_via_hill && n > 60 && n <= 80)
      {
	run_mn = true;
      }

    // 6. Percentile-t for small-to-medium N
    bool run_pt = (n <= 80);

    // 7. BCa Geo: only when we are not in the strict Small-N regime
    bool run_bca_geo = !run_mn;

    // 8. Logging (moments + quantile shape + Hill alpha + detector sources)

    os << "   [Bootstrap] n=" << n
       << "  skew=" << skew
       << "  exkurt=" << exkurt
       << "  bowley=" << qShape.bowleySkew
       << "  tailRatio=" << qShape.tailRatio
       << "  alpha=" << (valid_hill ? std::to_string(tailIndex) : "n/a")
       << "  heavy_tails=" << (heavy_tails ? "yes" : "no")
       << " (quant=" << heavy_via_quantiles
       << ", hill=" << heavy_via_hill << ")" // <-- 'moments' flag removed
       << "  [Flags: smallN=" << run_mn
       << ", bcaGeo=" << run_bca_geo
       << ", pt=" << run_pt << "]\n";
    
    // DistributionDiagnostics still carries skew/exkurt + combined heavy_tails
    return DistributionDiagnostics(skew, exkurt, heavy_tails,
				   run_mn, run_pt, run_bca_geo);
  }
  
  /**
   * @brief Executes the "Conservative Small-N" bootstrap pipeline.
   *
   * @details
   * This method acts as a wrapper around `conservative_smallN_lower_bound`. It:
   * 1. Configures the "Duel" between the m-out-of-n bootstrap and the BCa bootstrap.
   * 2. Enables the **TailVolStabilityPolicy** (by passing `rho_m = -1.0`), which adaptively
   * refines the subsampling ratio ($m/n$) based on the stability of the lower bound.
   * 3. Logs detailed diagnostics about the chosen resampler and effective sample size.
   *
   * @param ctx The analysis context.
   * @param confidenceLevel The target confidence level.
   * @param annualizationFactor The factor to scale results to annual figures.
   * @param blockLength The block size for resampling.
   * @param os Output stream for logging.
   * @return std::optional<SmallNResult> The result if calculation succeeds, or nullopt on failure.
   */
  std::optional<BootstrapAnalysisStage::SmallNResult>
  BootstrapAnalysisStage::runSmallNBootstrap(const StrategyAnalysisContext& ctx,
					     double confidenceLevel,
					     double annualizationFactor,
					     size_t blockLength,
					     std::ostream& os) const
  {
    using palvalidator::bootstrap_helpers::conservative_smallN_lower_bound;
    using mkc_timeseries::GeoMeanStat;

    // Pass rho_m <= 0.0 to enable TailVolStabilityPolicy (tail/vol prior + LB-stability refinement)
    // inside conservative_smallN_lower_bound. We use -1.0 here to make it explicit.
    const double rho_m = -1.0;

    // ------------------------------------------------------------------------
    // Extra conservative tweak:
    // For 60 < n <= 80, if the Hill estimator flags very heavy tails (alpha <= 2),
    // force the resampler choice inside conservative_smallN_lower_bound to use
    // block resampling (StationaryMaskValueResamplerAdapter), even if the
    // dependence heuristics might otherwise allow IID.
    // ------------------------------------------------------------------------
    std::optional<bool> heavy_tails_override = std::nullopt;

    const std::size_t n = ctx.highResReturns.size();
    if (n > 60 && n <= 80)
      {
	const double tailIndex = estimate_left_tail_index_hill(ctx.highResReturns);
	if (tailIndex > 0.0 && tailIndex <= 2.0)
	  {
	    heavy_tails_override = true; // "always block" inside conservative_smallN_lower_bound
	  }
      }

    auto smallN = conservative_smallN_lower_bound<Num, GeoMeanStat<Num>>(ctx.highResReturns,
									 blockLength,
									 annualizationFactor,
									 confidenceLevel,
									 mNumResamples,
									 rho_m, // adaptive policy enabled
									 *ctx.clonedStrategy,
									 mBootstrapFactory,
									 &os,
									 /*stage*/1,
									 /*fold*/0,
									 heavy_tails_override);

    os << "   [Bootstrap] m-out-of-n ∧ BCa (conservative small-N):"
       << "  resampler=" << (smallN.resampler_name ? smallN.resampler_name : "n/a")
       << "  m_sub=" << smallN.m_sub
       << "  L=" << smallN.L_used
       << "  effB(mn)=" << smallN.effB_mn
       << "  LB(per)=" << smallN.per_lower
       << "  LB(ann)=" << smallN.ann_lower << "\n";

    return SmallNResult(
			smallN.per_lower,
			smallN.ann_lower,
			smallN.resampler_name ? smallN.resampler_name : "n/a",
			smallN.m_sub,
			smallN.L_used,
			smallN.effB_mn);
  }

    /**
   * @brief Executes the "Conservative Small-N" bootstrap pipeline for Profit Factor.
   *
   * @details
   * This is a thin wrapper around `conservative_smallN_lower_bound`, using
   * `LogProfitFactorStat<Num>` as the statistic. It:
   *
   * 1. Configures the duel between the m-out-of-n bootstrap and the BCa bootstrap.
   * 2. Enables the TailVolStabilityPolicy (rho_m = -1.0) to adapt m/n based on
   *    the stability of the lower bound.
   * 3. Applies the same extra heavy-tail override used for geometric returns:
   *    for 60 < n <= 80 and Hill alpha <= 2.0, force block resampling.
   *
   * Note: Profit factor is dimensionless, so we do not apply any real
   *       annualization. The "annualized" bound in SmallNResult is set
   *       equal to the per-period bound.
   *
   * @param highResReturns  Mark-to-market bar-return series (out-of-sample).
   * @param clonedStrategy  Strategy instance used by the bootstrap engine.
   * @param confidenceLevel Target confidence level (e.g. 0.95).
   * @param blockLength     Stationary bootstrap block length L.
   * @param os              Output stream for logging.
   *
   * @return std::optional<SmallNResult>
   *         Populated on success; std::nullopt if the series is empty.
   */
  std::optional<BootstrapAnalysisStage::SmallNResult>
  BootstrapAnalysisStage::runSmallNProfitFactorBootstrap(
      const std::vector<Num>& highResReturns,
      const std::shared_ptr<PalStrategy<Num>>& clonedStrategy,
      double confidenceLevel,
      std::size_t blockLength,
      std::ostream& os) const
  {
    using palvalidator::bootstrap_helpers::conservative_smallN_lower_bound;
    using LogProfitFactorStat = mkc_timeseries::StatUtils<Num>::LogProfitFactorStat;

    // Enable TailVolStabilityPolicy inside conservative_smallN_lower_bound
    const double rho_m = -1.0;

    // Guard: nothing to bootstrap
    if (highResReturns.empty())
      {
        os << "   [Bootstrap] Small-N Profit Factor: empty return series; skipping.\n";
        return std::nullopt;
      }

    // ------------------------------------------------------------------------
    // Extra conservative tweak (same as runSmallNBootstrap for GeoMean):
    // For 60 < n <= 80, if the Hill estimator flags very heavy tails (alpha <= 2),
    // force the resampler choice inside conservative_smallN_lower_bound to use
    // block resampling (StationaryMaskValueResamplerAdapter).
    // ------------------------------------------------------------------------
    std::optional<bool> heavy_tails_override = std::nullopt;

    const std::size_t n = highResReturns.size();
    if (n > 60 && n <= 80)
      {
        const double tailIndex = estimate_left_tail_index_hill(highResReturns);
        if (tailIndex > 0.0 && tailIndex <= 2.0)
          {
            // "always block" inside conservative_smallN_lower_bound
            heavy_tails_override = true;
          }
      }

    // Profit factor is dimensionless → no real annualization.
    // We still pass 1.0 to satisfy the API; per_lower == ann_lower.
    const double annualizationFactor = 1.0;

    auto smallN =
      conservative_smallN_lower_bound<Num, LogProfitFactorStat>(
          highResReturns,
          blockLength,
          annualizationFactor,
          confidenceLevel,
          mNumResamples,
          rho_m,                 // adaptive policy enabled
          *clonedStrategy,
          mBootstrapFactory,
          &os,
          /*stage*/ 1,
          /*fold*/  0,
          heavy_tails_override);

    os << "   [Bootstrap] m-out-of-n ∧ BCa (conservative small-N, Profit Factor):"
       << "  resampler=" << (smallN.resampler_name ? smallN.resampler_name : "n/a")
       << "  m_sub="     << smallN.m_sub
       << "  L="         << smallN.L_used
       << "  effB(mn)="  << smallN.effB_mn
       << "  LB(stat)="  << smallN.per_lower
       << "\n";

    // Log the duel ratio if valid
    if (smallN.duel_ratio_valid)
    {
      os << "   [Bootstrap] PF duel m/n vs BCa ratio=" << smallN.duel_ratio << "\n";
    }

    // Reuse SmallNResult: interpret "period" as the log-PF statistic.
    const Num lbPer = smallN.per_lower;
    const Num lbAnn = lbPer;  // no true annualization for PF

    return SmallNResult(
        lbPer,
        lbAnn,
        smallN.resampler_name ? std::string(smallN.resampler_name) : std::string(),
        smallN.m_sub,
        static_cast<std::size_t>(smallN.L_used),
        smallN.effB_mn,
        smallN.duel_ratio,
        smallN.duel_ratio_valid);
  }

  BootstrapAnalysisStage::BCaPFResult
  BootstrapAnalysisStage::runBCaProfitFactorBootstrap(
						      const std::vector<Num>& highResReturns,
						      const std::shared_ptr<PalStrategy<Num>>& clonedStrategy,
						      double confidenceLevel,
						      size_t blockLength,
						      std::ostream& os) const
  {
    using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
    using PFStat       = mkc_timeseries::StatUtils<Num>::LogProfitFactorStat;

    BCaResampler bcaResampler(blockLength);

    std::function<Num(const std::vector<Num>&)> pfFn =
      [](const std::vector<Num>& r) { return PFStat()(r); };

    auto bcaPF = mBootstrapFactory.template makeBCa<Num, BCaResampler>(
								       highResReturns,
								       mNumResamples,
								       confidenceLevel,
								       pfFn,
								       bcaResampler,
								       *clonedStrategy,
								       /*stage*/1,
								       static_cast<uint64_t>(blockLength),
								       /*fold*/0
								       );

    const Num lbPF_BCa = bcaPF.getLowerBound();

    os << "   [Bootstrap] BCa (LogPF):"
       << "  resampler=StationaryBlockResampler"
       << "  L=" << blockLength
       << "  effB=" << mNumResamples
       << "  LB=" << lbPF_BCa << "\n";

    return BCaPFResult(lbPF_BCa);
  }
  
  /**
   * @brief Executes the Double Bootstrap (Percentile-t) method for variance stabilization.
   *
   * @details
   * Runs a nested bootstrap (Outer Loop B, Inner Loop B_in) to estimate the distribution of the
   * t-statistic. This is particularly effective for samples with moderate skewness or
   * heteroscedasticity where simple percentile methods might have poor coverage.
   *
   * @param ctx The analysis context.
   * @param confidenceLevel The target confidence level.
   * @param blockLength The block size for resampling.
   * @param os Output stream for logging.
   * @return std::optional<PercentileTResult> The result containing the studentized lower bound.
   */
  std::optional<BootstrapAnalysisStage::PercentileTResult>
  BootstrapAnalysisStage::runPercentileTBootstrap(const StrategyAnalysisContext& ctx,
						  double confidenceLevel,
						  size_t blockLength,
						  std::ostream& os) const
  {
    using PTExec = concurrency::ThreadPoolExecutor<0>;
    using ResamplerT = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    /*
      --------------------------------------------------------------------------------
      Why we run the Percentile-t Bootstrap (in addition to m-out-of-n and BCa)
      --------------------------------------------------------------------------------

      The percentile-t bootstrap provides a variance-stabilized benchmark for cases
      where the statistic of interest (e.g., geometric mean of mark-to-market returns)
      is skewed or exhibits heteroscedasticity.  It complements the other bootstrap
      methods rather than replacing them.

      Summary of roles:
      
      • m-out-of-n Bootstrap:
      - Very conservative and theoretically robust for small sample sizes (small n)
      or heavy-tailed data.
      - Protects against non-normality and dependent samples.
      - Limitation: often produces overly wide confidence intervals.

      • BCa Bootstrap (Bias-Corrected and Accelerated):
      - Corrects both bias and skewness in smooth, nearly normal statistics.
      - Works best with moderate-to-large n.
      - Limitation: requires accurate jackknife acceleration, which can be unstable
        for small samples or fat-tailed data.
	
	• Percentile-t Bootstrap:
      - "Studentizes" each bootstrap replicate by its estimated standard error.
      - Stabilizes variance and improves coverage accuracy for moderate n.
      - Bridges the gap between conservative m-out-of-n and efficient BCa.
      - Performs well with 5–15 trades or medium-size samples with mild skew.

      In BootstrapAnalysisStage we run all three methods because:
      1. m-out-of-n + BCa handle the extremes (small n, heavy tails, large n, smooth).
      2. Percentile-t acts as a precision cross-check for the middle regime.
      3. The final lower-bound estimate is derived by combining their results
      (typically the median of present bounds) to reduce sensitivity to any one
     method’s bias.
     
     In short:
     Percentile-t is the “bridge” method that validates and stabilizes inference
     when neither purely robust (m-out-of-n) nor purely efficient (BCa) assumptions
     hold—ensuring our bootstrap-based lower bounds remain both conservative and
     statistically meaningful across all sample sizes.
     --------------------------------------------------------------------------------
    */

    const std::size_t n     = ctx.highResReturns.size();
    const std::size_t B_out = std::max<std::size_t>(mNumResamples, 400);
    const std::size_t B_in  = std::max<std::size_t>(mNumResamples / 5, 100ul);
    const double rho_o = 1.0;
    const double rho_i = (n <= 24) ? 0.85 : 0.95;

    ResamplerT resampler(blockLength);
    auto [ptBoot, ptCrn] =
      mBootstrapFactory.template makePercentileT<Num,
                                                 mkc_timeseries::GeoMeanStat<Num>,
                                                 ResamplerT,
                                                 PTExec>(
							 B_out, B_in, confidenceLevel, resampler, *ctx.clonedStrategy,
							 /*stage*/1, blockLength, /*fold*/0, rho_o, rho_i);

    auto r = ptBoot.run(ctx.highResReturns, mkc_timeseries::GeoMeanStat<Num>(), ptCrn);

    os << "   [Bootstrap] Percentile-t:"
       << "  resampler=StationaryBlockResampler"
       << "  m_outer=" << r.m_outer
       << "  m_inner=" << r.m_inner
       << "  L=" << r.L
       << "  effB=" << r.effective_B
       << "  LB=" << r.lower << "\n";

    return PercentileTResult(r.lower, "StationaryBlockResampler",
			     r.m_outer, r.m_inner, r.L, r.effective_B);
  }

  /**
   * @brief Executes the Bias-Corrected and Accelerated (BCa) bootstrap for the Arithmetic Mean.
   *
   * @details
   * Unlike the other methods in this stage (which focus on Geometric Mean/CAGR), this method
   * computes the lower bound of the **Arithmetic Mean**.
   *
   * @note This is typically used for calculating risk-adjusted ratios (like Sharpe/Sortino)
   * rather than wealth accumulation, but is calculated here for completeness and reporting.
   *
   * @param ctx The analysis context.
   * @param confidenceLevel The target confidence level.
   * @param annualizationFactor Factor to annualize the mean.
   * @param blockLength Block length for the StationaryBlockResampler.
   * @param os Output stream for logging.
   * @return BCaMeanResult Struct containing per-period and annualized mean lower bounds.
   */
  BootstrapAnalysisStage::BCaMeanResult
  BootstrapAnalysisStage::runBCaMeanBootstrap(const StrategyAnalysisContext& ctx,
                                               double confidenceLevel,
                                               double annualizationFactor,
                                               size_t blockLength,
                                               std::ostream& os) const
  {
    using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
    BCaResampler bcaResampler(blockLength);

    std::function<Num(const std::vector<Num>&)> meanFn = &mkc_timeseries::StatUtils<Num>::computeMean;

    auto bcaMean = mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
                                                  mNumResamples,
                                                  confidenceLevel,
                                                  meanFn,
                                                  bcaResampler,
                                                  *ctx.clonedStrategy,
                                                  /*stage*/1, blockLength, /*fold*/0);

    const Num lbMean_BCa = bcaMean.getLowerBound();
    const Num annualizedLB = mkc_timeseries::BCaAnnualizer<Num>(bcaMean, annualizationFactor)
                               .getAnnualizedLowerBound();

    return BCaMeanResult(lbMean_BCa, annualizedLB);
  }

  /**
   * @brief Executes the Bias-Corrected and Accelerated (BCa) bootstrap for the Geometric Mean.
   *
   * @details
   * This method is used when N > 40 (where strict m-out-of-n is not required), but we still
   * want the statistical efficiency of BCa to blend with Percentile-t.
   *
   * @param ctx The analysis context.
   * @param confidenceLevel The target confidence level.
   * @param blockLength Block length for the StationaryBlockResampler.
   * @param os Output stream for logging.
   * @return BCaGeoResult Struct containing per-period lower bounds.
   */
  BootstrapAnalysisStage::BCaGeoResult
  BootstrapAnalysisStage::runBCaGeoBootstrap(const StrategyAnalysisContext& ctx,
                                              double confidenceLevel,
                                              size_t blockLength,
                                              std::ostream& os) const
  {
    using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
    using GeoStat = mkc_timeseries::GeoMeanStat<Num>;

    BCaResampler bcaResampler(blockLength);

    // Create the statistic function from GeoMeanStat
    std::function<Num(const std::vector<Num>&)> geoStatFn =
        [](const std::vector<Num>& returns) { return GeoStat()(returns); };

    // Use the BCa factory method with 2 template arguments
    auto bcaGeo = mBootstrapFactory.template makeBCa<Num, BCaResampler>(
        ctx.highResReturns,
        mNumResamples,
        confidenceLevel,
        geoStatFn,       // The statistic function
        bcaResampler,
        *ctx.clonedStrategy,
        /*stage*/1,
        static_cast<uint64_t>(blockLength),
        /*fold*/0
    );

    const Num lbGeo_BCa = bcaGeo.getLowerBound();

    // Note: We primarily need the per-period LB for blending.
    // Annualization happens later in the pipeline.

    os << "   [Bootstrap] BCa (GeoMean):"
       << "  resampler=StationaryBlockResampler"
       << "  L=" << blockLength
       << "  effB=" << mNumResamples
       << "  LB=" << lbGeo_BCa << "\n";

    return BCaGeoResult(lbGeo_BCa);
  }

  /**
   * @brief Combines lower bounds from Small-N and Percentile-t bootstraps into a single robust metric.
   *
   * @details
   * Implements a "Sign-Aware Blending" policy to balance robustness (m-out-of-n) with
   * statistical efficiency (Percentile-t):
   *
   * 1. Agreement (Same Direction): If both methods agree that the strategy is profitable
   * (both LB > 0) or unprofitable (both LB < 0), we take the **Arithmetic Mean**.
   * This smooths out the excessive variance penalty of the conservative m-out-of-n bootstrap
   * while tempering the potential instability of the Percentile-t bootstrap in small samples.
   *
   * 2. Conflict (Straddle Zero): If the methods disagree on the sign (one predicts profit,
   * the other predicts loss), we enforce strict conservatism and take the **Minimum**.
   * This ensures a strategy cannot "average its way" into passing the filter if a robust
   * method signals a genuine risk of negative expectancy.
   *
   * 3. Fallback: If only one method succeeded, its result is returned directly.
   *
   * @param smallN The result from the conservative m-out-of-n / BCa duel.
   * @param percentileT The result from the Percentile-t bootstrap.
   * @param os Output stream for logging (unused in calculation).
   * @return Num The blended per-period geometric lower bound.
   */
  Num BootstrapAnalysisStage::combineGeometricLowerBounds(
    const std::optional<SmallNResult>& smallN,
    const std::optional<PercentileTResult>& percentileT,
    const std::optional<BCaGeoResult>& bcaGeo,
    std::ostream& os) const
  {
    using mkc_timeseries::DecimalConstants;

    auto get_sign = [](const Num& v) -> int {
      if (v > DecimalConstants<Num>::DecimalZero) return 1;
      if (v < DecimalConstants<Num>::DecimalZero) return -1;
      return 0;
    };

    // 1. Determine the "Anchor" result.
    // Priority: SmallN (Conservative) > BCaGeo (Efficient)
    std::optional<Num> valAnchor;

    if (smallN.has_value()) {
        valAnchor = smallN->getLowerBoundPeriod();
    }
    else if (bcaGeo.has_value()) {
        valAnchor = bcaGeo->getLowerBoundPeriod();
    }

    // 2. Fallbacks if one side is missing
    if (!valAnchor.has_value() && !percentileT.has_value()) return DecimalConstants<Num>::DecimalZero;
    if (valAnchor.has_value() && !percentileT.has_value()) return *valAnchor;
    if (!valAnchor.has_value() && percentileT.has_value()) return percentileT->getLowerBoundPeriod();

    // 3. Smart Blending (Anchor vs Percentile-t)
    const Num v1 = *valAnchor;
    const Num v2 = percentileT->getLowerBoundPeriod();

    const int s1 = get_sign(v1);
    const int s2 = get_sign(v2);

    bool straddleZero = (s1 * s2 < 0);

    if (straddleZero)
    {
       // Conservative: Trust the pessimist if they disagree on sign.
       return (v1 < v2) ? v1 : v2;
    }
    else
    {
       // Agreement: Arithmetic Mean to smooth variance.
       return v1 + (v2 - v1) / Num(2);
    }
  }

/**
   * @brief Logs the decision matrix used to derive the final Lower Bound.
   *
   * @details
   * This helper constructs a human-readable string explaining exactly how the
   * separate bootstrap results (Small-N, Percentile-t, BCa Geo) were combined.
   * This ensures the audit trail accurately reflects whether a "Small-N Duel"
   * or a "Standard Blend" was performed.
   *
   * @param smallN Result from the Small-N bootstrap (optional).
   * @param percentileT Result from the Percentile-t bootstrap (optional).
   * @param bcaGeo Result from the BCa Geometric bootstrap (optional).
   * @param n Sample size.
   * @param blockLength Block length used.
   * @param skew Sample skewness.
   * @param excessKurtosis Sample kurtosis.
   * @param heavyTails Boolean flag indicating if heavy tails were detected.
   * @param os Output stream.
   */
  void BootstrapAnalysisStage::logFinalPolicy(
    const std::optional<SmallNResult>& smallN,
    const std::optional<PercentileTResult>& percentileT,
    const std::optional<BCaGeoResult>& bcaGeo,
    size_t n,
    size_t blockLength,
    double skew,
    double excessKurtosis,
    bool heavyTails,
    std::ostream& os) const
  {
    namespace bhi = palvalidator::bootstrap_helpers::internal;

    constexpr bool kUseVoteOfTwoMedian = true;
    const char* policyLabel = nullptr;
    
    // Case 1: Strict Small-N Duel + Percentile-t
    if (smallN.has_value() && percentileT.has_value())
    {
      policyLabel = kUseVoteOfTwoMedian 
        ? "smallN(min of m/n,BCa) ⊕ percentile-t (median of present)"
        : "min( smallN(min of m/n,BCa), percentile-t )";
    }
    // Case 2: Option B (Normal Regime) - BCa Geo + Percentile-t Blend
    else if (bcaGeo.has_value() && percentileT.has_value())
    {
       policyLabel = "BCa(Geo) ⊕ percentile-t (blend)";
    }
    // Case 3: Individual / Edge Cases
    else if (smallN.has_value())
    {
      policyLabel = "smallN(min of m/n,BCa)";
    }
    else if (bcaGeo.has_value())
    {
      policyLabel = "BCa (Geo)";
    }
    else if (percentileT.has_value())
    {
      policyLabel = "percentile-t (geo)";
    }
    else
    {
      policyLabel = "BCa (fallback)";
    }

    // For logging, prefer the small-N resampler name if available (as it might differ, e.g., IID).
    // If small-N wasn't run, we know BCa/PT used the StationaryBlockResampler.
    const char* resamplerNameForLog = smallN.has_value() 
      ? smallN->getResamplerName().c_str() 
      : "StationaryBlockResampler";

    bhi::log_policy_line(os, policyLabel, n, blockLength, skew, excessKurtosis, 
                         heavyTails, resamplerNameForLog, std::min<std::size_t>(blockLength, 3));
  }

  /**
   * @brief Validate that context has sufficient data for bootstrap analysis
   */
  bool BootstrapAnalysisStage::validateContext(const StrategyAnalysisContext& ctx,
                                                BootstrapAnalysisResult& result,
                                                std::ostream& os) const
  {
    const std::size_t n = ctx.highResReturns.size();
    if (n < 2)
    {
      result.failureReason = "Insufficient returns (need at least 2, have " + std::to_string(n) + ")";
      os << "   [Bootstrap] Skipped (" << result.failureReason << ")\n";
      return false;
    }
    return true;
  }

  /**
   * @brief Compute annualization parameters (block length, bars/year, lambda)
   */
  BootstrapAnalysisStage::AnnualizationParams
  BootstrapAnalysisStage::computeAnnualizationParams(const StrategyAnalysisContext& ctx,
                                                      std::ostream& os) const
  {
    AnnualizationParams params;
    
    // Compute block length
    params.blockLength = computeBlockLength(ctx);
    
    // Get median holding period
    params.medianHoldBars = ctx.backtester
      ? ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod()
      : 2;
    
    os << "\n" << "Strategy Median holding period = " << params.medianHoldBars << "\n";
    
    // Base calendar factor (fallback)
    params.baseAnnFactor = computeAnnualizationFactor(ctx);
    
    // λ = trades/year from backtester (preferred)
    params.lambdaTradesPerYear = 0.0;
    if (ctx.backtester)
    {
      try
      {
        params.lambdaTradesPerYear = ctx.backtester->getEstimatedAnnualizedTrades();
      }
      catch (...)
      {
        params.lambdaTradesPerYear = 0.0;
      }
    }
    
    // Annualize M2M bar statistics using bars/year = λ × medianHoldBars
    params.barsPerYear = params.lambdaTradesPerYear * static_cast<double>(params.medianHoldBars);
    if (!(params.barsPerYear > 0.0))
    {
      params.barsPerYear = params.baseAnnFactor;
      os << "   [Bootstrap] Warning: trades/year (λ) or medianHoldBars unavailable; "
         << "falling back to base calendar factor = " << params.baseAnnFactor << "\n";
    }
    
    return params;
  }

  /**
   * @brief Execute profit factor bootstrap analysis
   */

  void BootstrapAnalysisStage::executeProfitFactorBootstrap(
  					    const StrategyAnalysisContext& ctx,
  					    const DistributionDiagnostics& diagnostics,
  					    double confidenceLevel,
  					    size_t blockLength,
  					    BootstrapAnalysisResult& result,
  					    std::ostream& os) const
  {
    using mkc_timeseries::DecimalConstants;

    if (!ctx.clonedStrategy)
      {
	os << "   [Bootstrap] Profit Factor bootstrap skipped: no clonedStrategy in context.\n";
	return;
      }

    // ALWAYS run Small-N PF bootstrap (with duel) for ratio statistics
    // Ratios are inherently unstable in bootstrap contexts, so we need the
    // duel ratio to validate reliability regardless of sample size
    auto pfSmallNResult = runSmallNProfitFactorBootstrap(
  					ctx.highResReturns,
  					ctx.clonedStrategy,
  					confidenceLevel,
  					blockLength,
  					os);

    if (!pfSmallNResult)
      {
	os << "   [Bootstrap] Profit Factor bootstrap failed to produce result.\n";
	return;
      }

    // Extract the chosen PF lower bound (already the minimum from the duel)
    const Num chosenPF_log = pfSmallNResult->getLowerBoundPeriod();
    
    // Convert LPF_stat = log(1 + PF_ratio) back to PF_ratio = exp(LPF_stat) - 1
    const Num lbProfitFactorNonLog = std::exp(chosenPF_log) - DecimalConstants<Num>::DecimalOne;
    result.lbProfitFactor = lbProfitFactorNonLog;

    os << "   [Bootstrap] Profit Factor chosen LB(stat)="
       << lbProfitFactorNonLog
       << "  [source=SmallN_Duel"
       << ", raw_log=" << chosenPF_log
       << "]\n";

    // Attach PF duel ratio to result for pipeline validation
    result.pfDuelRatioValid = false;
    result.pfDuelRatio = std::numeric_limits<double>::quiet_NaN();
    
    if (pfSmallNResult->hasDuelRatio())
    {
      result.pfDuelRatio = pfSmallNResult->getDuelRatio();
      result.pfDuelRatioValid = true;
      
      os << "   [Bootstrap] PF duel ratio (BCa vs m/n) = "
         << result.pfDuelRatio << "\n";
    }
  }

  /**
   * @brief Assemble final bootstrap results from individual components
   */
  void BootstrapAnalysisStage::assembleFinalResults(
      const std::optional<SmallNResult>& smallNResult,
      const std::optional<PercentileTResult>& percentileTResult,
      const std::optional<BCaGeoResult>& bcaGeoResult,
      const BCaMeanResult& bcaMeanResult,
      const AnnualizationParams& annParams,
      BootstrapAnalysisResult& result,
      std::ostream& os) const
  {
    // Store mean bootstrap results
    result.lbMeanPeriod = bcaMeanResult.getLowerBoundPeriod();
    result.annualizedLowerBoundMean = bcaMeanResult.getLowerBoundAnnualized();
    
    // Combine geometric lower bounds
    const Num lbGeoPer_neutral = combineGeometricLowerBounds(
      smallNResult,
      percentileTResult,
      bcaGeoResult,
      os);
    result.lbGeoPeriod = lbGeoPer_neutral;
    
    // Annualize geometric lower bound
    result.annualizedLowerBoundGeo =
      mkc_timeseries::Annualizer<Num>::annualize_one(result.lbGeoPeriod, annParams.barsPerYear);
    
    // Publish component bounds for downstream refinement
    result.lbGeoSmallNPeriod = smallNResult.has_value()
      ? std::optional<Num>(smallNResult->getLowerBoundPeriod())
      : std::nullopt;
    result.lbGeoPTPeriod = percentileTResult.has_value()
      ? std::optional<Num>(percentileTResult->getLowerBoundPeriod())
      : std::nullopt;
    
    // Store annualization metadata
    result.blockLength = annParams.blockLength;
    result.medianHoldBars = annParams.medianHoldBars;
    result.annFactorUsed = annParams.barsPerYear;
    
    result.computationSucceeded = true;
  }

  /**
   * @brief Orchestrates the entire Bootstrap Analysis process.
   *
   * @details
   * This is the main entry point for the stage. The workflow is:
   * 1. **Setup:** Initialize backtester and compute time-series properties ($N$, $L$, $\lambda$).
   * 2. **Analysis:** Profile the distribution (Skew/Kurtosis) to decide which engines to run.
   * 3. **Execution:** Run the selected engines (Small-N, Percentile-t) and the baseline BCa Mean.
   * 4. **Synthesis:** Combine the results using `combineGeometricLowerBounds` to produce a
   * single, robust Conservative Lower Bound (CLB) for the strategy's edge.
   *
   * @param ctx The analysis context.
   * @param os Output stream for logging.
   * @return BootstrapAnalysisResult The final object containing all bounds, factors, and status flags.
   */
// --- BEGIN REGENERATED BootstrapAnalysisStage::execute METHOD ---

  /**
   * @brief Orchestrates the entire Bootstrap Analysis process.
   *
   * @details
   * This is the main entry point for the stage. The workflow is:
   * 1. **Setup:** Initialize backtester and validate context
   * 2. **Parameter Computation:** Compute annualization parameters
   * 3. **Distribution Analysis:** Profile distribution to decide which methods to run
   * 4. **Bootstrap Execution:** Run selected bootstrap methods
   * 5. **Result Assembly:** Combine results into final output
   *
   * @param ctx The analysis context.
   * @param os Output stream for logging.
   * @return BootstrapAnalysisResult The final object containing all bounds, factors, and status flags.
   */
  BootstrapAnalysisResult
  BootstrapAnalysisStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    BootstrapAnalysisResult result;

    // Step 1: Initialize backtester and validate context
    if (!initializeBacktester(ctx, os))
    {
      result.failureReason = "Failed to initialize backtester";
      return result;
    }

    if (!validateContext(ctx, result, os))
    {
      return result;
    }

    // Step 2: Compute annualization parameters
    const auto annParams = computeAnnualizationParams(ctx, os);
    const double confidenceLevel = mConfidenceLevel.getAsDouble();

    try
    {
      // Step 3: Analyze distribution and determine which methods to run
      const auto diagnostics = analyzeDistribution(ctx, os);
      os << "   [Bootstrap] L=" << annParams.blockLength << "\n";

      // Step 4: Run bootstrap methods based on diagnostics
      std::optional<SmallNResult> smallNResult;
      if (diagnostics.shouldRunSmallN())
      {
	os << "   [Bootstrap] Executing m/n + BCa geomean duel = " << std::endl << std::endl;
        smallNResult = runSmallNBootstrap(ctx, confidenceLevel,
                                          annParams.barsPerYear,
                                          annParams.blockLength, os);
      }

      std::optional<BCaGeoResult> bcaGeoResult;
      if (diagnostics.shouldRunBCaGeo())
      {
	os << "   [Bootstrap] Executing BCa standalone bootstrap = " << std::endl << std::endl;
        bcaGeoResult = runBCaGeoBootstrap(ctx, confidenceLevel,
                                          annParams.blockLength, os);
      }

      std::optional<PercentileTResult> percentileTResult;
      if (diagnostics.shouldRunPercentileT())
      {
	os << "   [Bootstrap] Executing percentile bootstrap = " << std::endl << std::endl;
        percentileTResult = runPercentileTBootstrap(ctx, confidenceLevel,
                                                    annParams.blockLength, os);
      }

      // Always run BCa mean bootstrap for compatibility
      const auto bcaMeanResult = runBCaMeanBootstrap(ctx, confidenceLevel,
                                                     annParams.barsPerYear,
                                                     annParams.blockLength, os);

      os << "   [Bootstrap] Executing profit factor duel = " << std::endl << std::endl;

      // Step 5: Execute profit factor bootstrap
      executeProfitFactorBootstrap(ctx, diagnostics, confidenceLevel,
                                   annParams.blockLength, result, os);

      // Step 6: Assemble final results
      assembleFinalResults(smallNResult, percentileTResult, bcaGeoResult,
                          bcaMeanResult, annParams, result, os);

      // Log final policy with actual diagnostics
      logFinalPolicy(smallNResult, percentileTResult, bcaGeoResult,
                    ctx.highResReturns.size(), annParams.blockLength,
                    diagnostics.getSkew(), diagnostics.getExcessKurtosis(),
                    diagnostics.hasHeavyTails(), os);

      // Log annualization parameters
      os << "   [Bootstrap] Annualization factor (bars/year via λ×medianHoldBars) = "
         << annParams.barsPerYear
         << "  [λ=" << annParams.lambdaTradesPerYear
         << ", medianHoldBars=" << annParams.medianHoldBars << "]\n";

      return result;
    }
    catch (const std::exception& e)
    {
      result.failureReason = std::string("Bootstrap computation failed: ") + e.what();
      os << "Warning: BootstrapAnalysisStage " << result.failureReason << "\n";
      return result;
    }
  }
} // namespace palvalidator::filtering::stages
