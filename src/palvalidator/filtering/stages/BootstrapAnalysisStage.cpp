#include "BootstrapAnalysisStage.h"

#include <cmath>
#include <limits>
#include <optional>
#include <sstream>

#include "BackTester.h"
#include "ClosedPositionHistory.h"
#include "PalStrategy.h"

#include "Annualizer.h"
#include "StatUtils.h"

#include "StationaryMaskResamplers.h"
#include "StrategyAutoBootstrap.h"   // palvalidator::analysis::StrategyAutoBootstrap
#include "AutoBootstrapSelector.h"   // palvalidator::analysis::AutoCIResult

namespace palvalidator::filtering::stages
{
  using palvalidator::bootstrap_cfg::BootstrapFactory;
  using palvalidator::analysis::BootstrapConfiguration;
  using palvalidator::analysis::BootstrapAlgorithmsConfiguration;
  using palvalidator::analysis::StrategyAutoBootstrap;
  using palvalidator::analysis::AutoCIResult;

  using palvalidator::resampling::StationaryMaskValueResamplerAdapter;

  using mkc_timeseries::StatUtils;
  using mkc_timeseries::Annualizer;
  using mkc_timeseries::GeoMeanStat;

  namespace
  {
    // Small helper to create a quiet NaN for doubles
    inline double qnan()
    {
      return std::numeric_limits<double>::quiet_NaN();
    }

    template <class Decimal>
    const char* methodIdToString(typename AutoCIResult<Decimal>::MethodId m)
    {
      using MethodId = typename AutoCIResult<Decimal>::MethodId;
      switch (m)
        {
          case MethodId::Normal:      return "Normal";
          case MethodId::Basic:       return "Basic";
          case MethodId::Percentile:  return "Percentile";
          case MethodId::PercentileT: return "PercentileT";
          case MethodId::MOutOfN:     return "MOutOfN";
          case MethodId::BCa:         return "BCa";
          default:                    return "Unknown";
        }
    }
  } // anonymous namespace

  // ---------------------------------------------------------------------------
  // Constructor implementation
  // ---------------------------------------------------------------------------

  BootstrapAnalysisStage::BootstrapAnalysisStage(const Num& confidenceLevel,
                                                 unsigned int numResamples,
                                                 BootstrapFactory& bootstrapFactory)
    : mConfidenceLevel(confidenceLevel)
    , mNumResamples(numResamples)
    , mBootstrapFactory(bootstrapFactory)
  {
  }

  // ---------------------------------------------------------------------------
  // Helper: computeBlockLength
  // ---------------------------------------------------------------------------

  size_t BootstrapAnalysisStage::computeBlockLength(const StrategyAnalysisContext& ctx,
						    std::ostream& os) const
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

    os << "   [Bootstrap] Stationary block length L=" << L
       << " (n=" << n << ")\n";

    return L;
  }

  double
  BootstrapAnalysisStage::computeAnnualizationFactor(const StrategyAnalysisContext& ctx) const
  {
    // 1) If the coordinator already set an annualizationFactor, trust it.
    if (ctx.annualizationFactor > 0.0)
      {
	return ctx.annualizationFactor;
      }

    // 2) Delegate to centralized logic that knows how to handle intraday vs others.
    //    We pass the underlying time series if we have one; the helper will:
    //      - for INTRADAY: extract minutesPerBar and use it
    //      - otherwise: ignore the series and use the TimeFrame-based factor
    auto tsPtr = (ctx.baseSecurity && ctx.baseSecurity->getTimeSeries())
      ? ctx.baseSecurity->getTimeSeries()
      : decltype(ctx.baseSecurity->getTimeSeries()){}; // nullptr / empty
    
    return mkc_timeseries::computeAnnualizationFactorForSeries(ctx.timeFrame,
							       tsPtr);
}
  
  // ---------------------------------------------------------------------------
  // AnnualizationParams: small struct describing what execute() needs
  // ---------------------------------------------------------------------------

  BootstrapAnalysisStage::AnnualizationParams
  BootstrapAnalysisStage::computeAnnualizationParams(const StrategyAnalysisContext& ctx,
                                                     std::ostream& os) const
  {
    AnnualizationParams params{};

    params.medianHoldBars =
      (ctx.backtester)
        ? ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod()
        : 2;

    os << "\nStrategy Median holding period = "
       << params.medianHoldBars << "\n";

    // Base calendar factor (e.g. 252)
    params.baseAnnFactor = computeAnnualizationFactor(ctx);

    // λ = trades/year from backtester (preferred if available)
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
    params.barsPerYear =
      params.lambdaTradesPerYear * static_cast<double>(params.medianHoldBars);

    if (!(params.barsPerYear > 0.0))
      {
        params.barsPerYear = params.baseAnnFactor;
        os << "   [Bootstrap] Warning: trades/year (λ) or medianHoldBars unavailable; "
           << "falling back to base calendar factor = "
           << params.baseAnnFactor << "\n";
      }

    return params;
  }

  // ---------------------------------------------------------------------------
  // Helper: ensure backtester exists
  // ---------------------------------------------------------------------------

  bool
  BootstrapAnalysisStage::initializeBacktester(StrategyAnalysisContext& ctx,
                                               std::ostream& os) const
  {
    if (ctx.backtester)
      {
        return true;
      }

    if (!ctx.clonedStrategy)
      {
        os << "   [Bootstrap] Error: no clonedStrategy in context; "
              "cannot initialize backtester.\n";
        return false;
      }

    // Create a concrete backtester implementation instead of the abstract base class
    ctx.backtester = std::make_shared<mkc_timeseries::DailyBackTester<Num>>();
    ctx.backtester->addStrategy(ctx.clonedStrategy);
    try
      {
        ctx.backtester->backtest();
      }
    catch (const std::exception& e)
      {
        os << "   [Bootstrap] Error: backtester run failed: "
           << e.what() << "\n";
        ctx.backtester.reset();
        return false;
      }

    return true;
  }

  // ---------------------------------------------------------------------------
  // Helper: BCa for arithmetic mean (kept intact)
  // ---------------------------------------------------------------------------

  BootstrapAnalysisStage::BCaMeanResult
  BootstrapAnalysisStage::runBCaMeanBootstrap(const StrategyAnalysisContext& ctx,
                                              double confidenceLevel,
                                              double annualizationFactor,
                                              std::size_t blockLength,
                                              std::ostream& os) const
  {
    using BCaResampler = mkc_timeseries::StationaryBlockResampler<Num>;
    BCaResampler bcaResampler(blockLength);

    std::function<Num(const std::vector<Num>&)> meanFn =
      &mkc_timeseries::StatUtils<Num>::computeMean;

    auto bcaMean =
      mBootstrapFactory.makeBCa<Num>(ctx.highResReturns,
                                     mNumResamples,
                                     confidenceLevel,
                                     meanFn,
                                     bcaResampler,
                                     *ctx.clonedStrategy,
                                     /*stage*/ 1,
                                     blockLength,
                                     /*fold*/ 0);

    const Num lbMean_BCa = bcaMean.getLowerBound();
    const Num annualizedLB =
      mkc_timeseries::BCaAnnualizer<Num>(bcaMean, annualizationFactor)
        .getAnnualizedLowerBound();

    os << "   [Bootstrap] BCa (Mean):"
       << "  L=" << blockLength
       << "  effB=" << mNumResamples
       << "  LB(per)=" << lbMean_BCa
       << "  LB(ann)=" << annualizedLB
       << "\n";

    return BCaMeanResult(lbMean_BCa, annualizedLB);
  }

  // ---------------------------------------------------------------------------
  // Auto-bootstrap helper for geometric mean (CAGR)
  // ---------------------------------------------------------------------------
  Num
  BootstrapAnalysisStage::runAutoGeoBootstrap(const StrategyAnalysisContext& ctx,
					      double                        confidenceLevel,
					      std::size_t                   blockLength,
					      BootstrapAnalysisResult&      out,
					      std::ostream&                 os) const
  {
    using Decimal   = Num;
    using Stat      = mkc_timeseries::StatUtils<Decimal>;
    using Sampler   = mkc_timeseries::GeoMeanFromLogBarsStat<Decimal>;
    using Resampler = StationaryMaskValueResamplerAdapter<Decimal>;
    using AutoCI    = AutoCIResult<Decimal>;
    using Candidate = typename AutoCI::Candidate;
    using MethodId  = typename AutoCI::MethodId;

    if (!ctx.clonedStrategy)
      {
	throw std::runtime_error("runAutoGeoBootstrap: clonedStrategy is null.");
      }

    if (ctx.highResReturns.empty())
      {
	os << "   [Bootstrap] AutoCI (GeoMean): skipped (n == 0).\n";
	return mkc_timeseries::DecimalConstants<Decimal>::DecimalZero;
      }

    const std::uint64_t stageTag = 1;
    const std::uint64_t fold     = 0;

    BootstrapConfiguration cfg(
			       mNumResamples,
			       blockLength,
			       confidenceLevel,
			       stageTag,
			       fold);

    BootstrapAlgorithmsConfiguration algos(
					   /*Normal*/      true,
					   /*Basic*/       true,
					   /*Percentile*/  true,
					   /*MOutOfN*/     true,
					   /*PercentileT*/ true,
					   /*BCa*/         true);

    // Precompute log(1 + r) once, with ruin-aware clipping.
    const double ruin_eps = Stat::DefaultRuinEps;   // or 1e-8 to match GeoMeanStat defaults
    const auto&  rawReturns = ctx.highResReturns;

    std::vector<Decimal> logBars =
      Stat::makeLogGrowthSeries(rawReturns, ruin_eps);

    // StrategyAutoBootstrap will default-construct GeoMeanFromLogBarsStat,
    // which uses the same winsorization defaults as GeoMeanStat.
    StrategyAutoBootstrap<Decimal, Sampler, Resampler> autoGeo(
							       mBootstrapFactory,
							       *ctx.clonedStrategy,
							       cfg,
							       algos);

    os << "   [Bootstrap] AutoCI (GeoMean): running composite bootstrap engines"
       << " on precomputed log-bars...\n";

    AutoCI result = autoGeo.run(logBars, &os);

    const Candidate& chosen = result.getChosenCandidate();
    const Decimal    lbPer  = chosen.getLower();

    // Diagnostics into BootstrapAnalysisResult (unchanged)
    out.geoAutoCIChosenMethod      = methodIdToString<Decimal>(result.getChosenMethod());
    out.geoAutoCIChosenScore       = chosen.getScore();
    out.geoAutoCIStabilityPenalty  = chosen.getStabilityPenalty();
    out.geoAutoCILengthPenalty     = chosen.getLengthPenalty();

    const auto& candidates = result.getCandidates();
    bool hasBCa = false;
    for (const auto& c : candidates)
      {
	if (c.getMethod() == MethodId::BCa)
	  {
	    hasBCa = true;
	    break;
	  }
      }
    out.geoAutoCIHasBCaCandidate = hasBCa;
    out.geoAutoCIBCaChosen       = (chosen.getMethod() == MethodId::BCa);
    out.geoAutoCINumCandidates   = candidates.size();

    os << "   [Bootstrap] AutoCI (GeoMean):"
       << " method=" << out.geoAutoCIChosenMethod
       << "  LB(per)=" << lbPer
       << "  CL=" << chosen.getCl()
       << "  n=" << chosen.getN()
       << "  B_eff=" << chosen.getEffectiveB()
       << "  score=" << out.geoAutoCIChosenScore
       << "  stab_penalty=" << out.geoAutoCIStabilityPenalty
       << "  len_penalty=" << out.geoAutoCILengthPenalty
       << "  hasBCa=" << (out.geoAutoCIHasBCaCandidate ? "true" : "false")
       << "  BCaChosen=" << (out.geoAutoCIBCaChosen ? "true" : "false")
       << "\n";

    return lbPer;
  }

  /**
   * @brief Run automatic bootstrap confidence interval selection for profit factor.
   *
   * @details
   * This helper drives a StrategyAutoBootstrap instance configured for a
   * robust log profit-factor statistic. It uses stationary-block bootstrap
   * resampling of the strategy’s high-resolution returns and runs a suite
   * of interval construction methods (Normal, Basic, Percentile, m-out-of-n,
   * Percentile-t, BCa). The method then selects and records the resulting
   * confidence intervals and point estimates in the provided
   * BootstrapAnalysisResult.
   *
   * High-level workflow:
   *   1. Validate the analysis context:
   *        • Require a non-null clonedStrategy.
   *        • Require at least two highResReturns; otherwise, log a message
   *          and return std::nullopt.
   *
   *   2. Derive a stop-loss prior for the profit-factor statistic:
   *        • Attempt a dynamic_pointer_cast to PalStrategy<Decimal>.
   *        • If successful and a pattern exists, read the pattern stop loss
   *          as a Decimal (for example 2.55 for 2.55%).
   *        • Convert that to a decimal fraction using PercentNumber, then to
   *          a double stopLossPct and pass it into the profit-factor functor.
   *        • If casting fails or no pattern is available, fall back to a
   *          zero stopLossPct (no explicit stop-loss prior).
   *
   *   3. Configure the bootstrap engine:
   *        • Build a BootstrapConfiguration with the requested number of
   *          resamples (mNumResamples), the supplied blockLength, the
   *          confidenceLevel, and a stageTag/fold identifier.
   *        • Enable the desired interval algorithms via
   *          BootstrapAlgorithmsConfiguration (Normal, Basic, Percentile,
   *          m-out-of-n, Percentile-t, BCa).
   *        • Construct the profit-factor statistic functor (for example
   *          StatUtils<Decimal>::LogProfitFactorStat configured with
   *          compression, ruin_eps, denom_floor, prior_strength, and the
   *          stopLossPct derived above).
   *        • Instantiate StrategyAutoBootstrap with the configured statistic
   *          and a StationaryMaskValueResamplerAdapter as the resampling
   *          mechanism.
   *
   *   4. Execute the auto-bootstrap:
   *        • Run the auto-bootstrap procedure to obtain an AutoCIResult
   *          containing candidate intervals for the chosen statistic.
   *        • Select the preferred interval(s) and populate the output
   *          BootstrapAnalysisResult structure, including point estimate and
   *          confidence bounds.
   *
   * The method returns the chosen profit-factor point estimate as an
   * std::optional<Num>. When bootstrapping is skipped due to insufficient
   * data, it returns std::nullopt and logs a short message to the
   * provided output stream.
   *
   * @param ctx
   *     StrategyAnalysisContext holding the cloned strategy instance and its
   *     precomputed high-resolution return series (ctx.highResReturns).
   *
   * @param confidenceLevel
   *     Target confidence level for the intervals (for example 0.90, 0.95).
   *
   * @param blockLength
   *     Stationary-block bootstrap mean block length to use when resampling
   *     the highResReturns series.
   *
   * @param out
   *     BootstrapAnalysisResult that will be populated with the chosen point
   *     estimate, confidence intervals, and method diagnostics for the
   *     profit-factor statistic.
   *
   * @param os
   *     Output stream used for logging progress and any reasons why the
   *     bootstrap was skipped (for example, too few data points).
   *
   * @return
   *     An optional profit-factor point estimate (Num). On success, this is
   *     typically the statistic evaluated on the original series. If the
   *     bootstrap is skipped (for example, ctx.highResReturns.size() < 2),
   *     returns std::nullopt.
   */
  std::optional<Num>
  BootstrapAnalysisStage::runAutoProfitFactorBootstrap(
						       const StrategyAnalysisContext& ctx,
						       double                        confidenceLevel,
						       std::size_t                   blockLength,
						       BootstrapAnalysisResult&      out,
						       std::ostream&                 os) const
  {
    using Decimal    = Num;
    using PFStat     = typename mkc_timeseries::StatUtils<Decimal>::LogProfitFactorFromLogBarsStat;
    using Resampler  = StationaryMaskValueResamplerAdapter<Decimal>;
    using AutoCI     = AutoCIResult<Decimal>;
    using Candidate  = typename AutoCI::Candidate;
    using MethodId   = typename AutoCI::MethodId;
    using Stat       = mkc_timeseries::StatUtils<Decimal>;

    if (!ctx.clonedStrategy)
      {
	throw std::runtime_error(
				 "runAutoProfitFactorBootstrap: clonedStrategy is null.");
      }

    if (ctx.highResReturns.size() < 2)
      {
	os << "   [Bootstrap] AutoCI (PF): skipped (n < 2).\n";
	return std::nullopt;
      }

    // -----------------------------------------------------------------------
    // Precompute log-growth series once for the entire bootstrap run
    // logBars[i] = log( max(1 + r_i, ruin_eps) )
    // -----------------------------------------------------------------------
    const double ruin_eps = Stat::DefaultRuinEps;
    const auto&  rawReturns = ctx.highResReturns;

    std::vector<Decimal> logBars =
      Stat::makeLogGrowthSeries(rawReturns, ruin_eps);

    // -----------------------------------------------------------------------
    // Retrieve Strategy Stop Loss for Robust "Zero-Loss" Handling
    // -----------------------------------------------------------------------
    double stopLossPct = 0.0;

    // Try to cast to PalStrategy to access the single pattern.
    // If it's a PalMetaStrategy, this cast will fail (nullptr), and we default to 0.0.
    auto palStrategy =
      std::dynamic_pointer_cast<mkc_timeseries::PalStrategy<Decimal>>(ctx.clonedStrategy);
    if (palStrategy)
      {
	auto pattern = palStrategy->getPalPattern();
	if (pattern)
	  {
	    // 1. Get Stop Loss from pattern (e.g., 2.55 for 2.55%)
	    Decimal stopDec = pattern->getStopLossAsDecimal();

	    // 2. Use PercentNumber factory to convert to decimal fraction (0.0255).
	    //    This handles the division by 100 internally.
	    auto stopPn =
	      mkc_timeseries::PercentNumber<Decimal>::createPercentNumber(stopDec);

	    // 3. Extract as double for StatUtils
	    stopLossPct = num::to_double(stopPn.getAsPercent());
	  }
      }

    // -----------------------------------------------------------------------
    // Configure Bootstrap Engines
    // -----------------------------------------------------------------------
    const std::uint64_t stageTag = 2;
    const std::uint64_t fold     = 0;

    BootstrapConfiguration cfg(
			       mNumResamples,
			       blockLength,
			       confidenceLevel,
			       stageTag,
			       fold);

    BootstrapAlgorithmsConfiguration algos(
					   /*Normal*/      true,
					   /*Basic*/       true,
					   /*Percentile*/  true,
					   /*MOutOfN*/     true,
					   /*PercentileT*/ true,
					   /*BCa*/         true);

    PFStat stat(
		Stat::DefaultCompress,
		Stat::DefaultRuinEps,
		Stat::DefaultDenomFloor,
		Stat::DefaultPriorStrength,
		stopLossPct);

    // Pass the configured 'stat' object to StrategyAutoBootstrap
    StrategyAutoBootstrap<Decimal, PFStat, Resampler> autoPF(
							     mBootstrapFactory,
							     *ctx.clonedStrategy,
							     cfg,
							     algos,
							     stat);

    os << "   [Bootstrap] AutoCI (PF): running composite bootstrap engines"
       << " on precomputed log-bars"
       << " (StopLoss assumption=" << (stopLossPct * 100.0) << "%)...\n";

    // NOTE: We now bootstrap the log-growth series (logBars) instead of raw returns.
    AutoCI result = autoPF.run(logBars, &os);

    const Candidate& chosen  = result.getChosenCandidate();
    const Decimal    lbLogPF = chosen.getLower();

    // Convert LPF_stat = log(1 + PF_ratio) back to PF_ratio = exp(LPF_stat) - 1
    using mkc_timeseries::DecimalConstants;
    const Decimal lbPF =
      std::exp(lbLogPF) - DecimalConstants<Decimal>::DecimalOne;

    // Populate PF AutoCI diagnostics
    out.pfAutoCIChosenMethod      = methodIdToString<Decimal>(result.getChosenMethod());
    out.pfAutoCIChosenScore       = chosen.getScore();
    out.pfAutoCIStabilityPenalty  = chosen.getStabilityPenalty();
    out.pfAutoCILengthPenalty     = chosen.getLengthPenalty();

    const auto& candidates = result.getCandidates();
    bool hasBCa = false;
    for (const auto& c : candidates)
      {
	if (c.getMethod() == MethodId::BCa)
	  {
	    hasBCa = true;
	    break;
	  }
      }
    out.pfAutoCIHasBCaCandidate = hasBCa;
    out.pfAutoCIBCaChosen       = (chosen.getMethod() == MethodId::BCa);
    out.pfAutoCINumCandidates   = candidates.size();

    os << "   [Bootstrap] AutoCI (PF):"
       << " method=" << out.pfAutoCIChosenMethod
       << "  LB(PF)=" << lbPF
       << "  LB(logPF)=" << lbLogPF
       << "  CL=" << chosen.getCl()
       << "  n=" << chosen.getN()
       << "  B_eff=" << chosen.getEffectiveB()
       << "  score=" << out.pfAutoCIChosenScore
       << "  stab_penalty=" << out.pfAutoCIStabilityPenalty
       << "  len_penalty=" << out.pfAutoCILengthPenalty
       << "  hasBCa=" << (out.pfAutoCIHasBCaCandidate ? "true" : "false")
       << "  BCaChosen=" << (out.pfAutoCIBCaChosen ? "true" : "false")
       << "\n";

    return lbPF;
  }
  
  // ---------------------------------------------------------------------------
  // execute() – top-level stage orchestration
  // ---------------------------------------------------------------------------

  BootstrapAnalysisResult
  BootstrapAnalysisStage::execute(StrategyAnalysisContext& ctx,
                                  std::ostream& os) const
  {
    BootstrapAnalysisResult result{};

    // Initialize default / sentinel values
    result.lbGeoPeriod              = Num(0);
    result.annualizedLowerBoundGeo  = Num(0);
    result.lbMeanPeriod             = Num(0);
    result.annualizedLowerBoundMean = Num(0);
    result.lbProfitFactor           = std::nullopt;
    result.annFactorUsed            = qnan();
    result.blockLength              = 0;
    result.medianHoldBars           = 0;
    result.pfDuelRatioValid         = false;
    result.pfDuelRatio              = qnan();

    os << "\n==================== Bootstrap Analysis Stage ====================\n";

    if (ctx.highResReturns.size() < 2)
      {
        os << "   [Bootstrap] Skipping: insufficient highResReturns (n < 2).\n";
        return result;
      }

    if (!initializeBacktester(ctx, os))
      {
        os << "   [Bootstrap] Skipping: backtester initialization failed.\n";
        return result;
      }

    // 1) Annualization params (shared across metrics)
    AnnualizationParams annParams = computeAnnualizationParams(ctx, os);
    result.annFactorUsed          = annParams.barsPerYear;
    result.medianHoldBars         = annParams.medianHoldBars;

    // 2) Block length for stationary bootstrap
    const std::size_t blockLength = computeBlockLength(ctx, os);
    result.blockLength            = blockLength;

    const double confLevel = num::to_double(mConfidenceLevel);

    // 3) Arithmetic mean via BCa
    const auto bcaMean =
      runBCaMeanBootstrap(ctx, confLevel, annParams.barsPerYear, blockLength, os);
    result.lbMeanPeriod             = bcaMean.getLowerBoundPeriod();
    result.annualizedLowerBoundMean = bcaMean.getLowerBoundAnnualized();

    // 4) Geometric mean (CAGR) via StrategyAutoBootstrap
    try
      {
        const Num lbGeoPer =
          runAutoGeoBootstrap(ctx, confLevel, blockLength, result, os);
        result.lbGeoPeriod             = lbGeoPer;
        result.annualizedLowerBoundGeo =
          Annualizer<Num>::annualize_one(lbGeoPer, annParams.barsPerYear);
      }
    catch (const std::exception& e)
      {
        os << "   [Bootstrap] ERROR: AutoCI (GeoMean) failed: "
           << e.what() << "\n";
        // Leave geometric fields at default; mean and PF (if any) still published.
      }

    // 5) Profit Factor via StrategyAutoBootstrap
    try
      {
        auto pfLBOpt =
          runAutoProfitFactorBootstrap(ctx, confLevel, blockLength, result, os);
        if (pfLBOpt.has_value())
          {
            result.lbProfitFactor   = pfLBOpt;
            result.pfDuelRatioValid = false;              // no duel anymore
            result.pfDuelRatio      = qnan();             // unused
          }
      }
    catch (const std::exception& e)
      {
        os << "   [Bootstrap] ERROR: AutoCI (PF) failed: "
           << e.what() << "\n";
      }

    result.computationSucceeded = true;

    os << "==================== End Bootstrap Analysis Stage ===============\n\n";

    return result;
  }

} // namespace palvalidator::filtering::stages
