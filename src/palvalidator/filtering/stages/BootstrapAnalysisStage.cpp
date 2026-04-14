#include "BootstrapAnalysisStage.h"

#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <atomic>
#include "number.h"
#include "BackTester.h"
#include "ClosedPositionHistory.h"
#include "PalStrategy.h"

#include "Annualizer.h"
#include "StatUtils.h"

#include "diagnostics/IBootstrapObserver.h"

#include "StationaryMaskResamplers.h"
#include "TradeResampling.h"
#include "StrategyAutoBootstrap.h"
#include "CandidateReject.h"
#include "AutoBootstrapSelector.h"
#include "BootstrapException.h"

namespace {
    std::atomic<uint64_t> g_nextRunID{1};
}

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
  using mkc_timeseries::IIDResampler;
  using mkc_timeseries::Trade;

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

    // -----------------------------------------------------------------------
    // makeLogGrowthSeriesForTrades
    //
    // Converts a vector<Trade<Decimal>> whose getDailyReturns() holds raw
    // mark-to-market returns into a new vector<Trade<Decimal>> whose
    // getDailyReturns() holds pre-computed log-growth values of the form
    //   log( max(1 + r_i, ruin_eps) )
    //
    // This is the trade-level analogue of StatUtils::makeLogGrowthSeries and
    // satisfies the contract required by GeoMeanFromLogBarsStat::operator()
    // and LogProfitFactorFromLogBarsStat_LogPF::operator() when called with
    // a vector<Trade<Decimal>>: both assume getDailyReturns() contains
    // pre-computed log-bars, not raw returns.
    //
    // By pre-computing once here, bootstrap iterations only flatten and sum;
    // no further log() calls occur inside the stat functors.
    //
    // NOTE: Not every statistic requires log-transformed input. Callers are
    // responsible for deciding whether to pre-transform their data series
    // before passing it to the bootstrap engine. This helper exists solely
    // for statistics whose contracts require pre-computed log-bars.
    // -----------------------------------------------------------------------
    template <class Decimal>
    std::vector<mkc_timeseries::Trade<Decimal>>
    makeLogGrowthSeriesForTrades(
        const std::vector<mkc_timeseries::Trade<Decimal>>& rawTrades,
        double ruin_eps = mkc_timeseries::StatUtils<Decimal>::DefaultRuinEps)
    {
      std::vector<mkc_timeseries::Trade<Decimal>> logTrades;
      logTrades.reserve(rawTrades.size());
      for (const auto& trade : rawTrades)
        {
          // Reuse the scalar makeLogGrowthSeries on each trade's raw returns.
          std::vector<Decimal> logBars =
            mkc_timeseries::StatUtils<Decimal>::makeLogGrowthSeries(
                trade.getDailyReturns(), ruin_eps);
          logTrades.emplace_back(std::move(logBars));
        }
      return logTrades;
    }

    // -----------------------------------------------------------------------
    // hasBCaCandidate
    //
    // Returns true if any candidate in the AutoCIResult used the BCa method.
    // Centralised to avoid the identical four-line scan that previously
    // appeared verbatim in each of the four auto-bootstrap helpers.
    // -----------------------------------------------------------------------
    template <class Decimal>
    bool hasBCaCandidate(const AutoCIResult<Decimal>& result)
    {
      using MethodId = typename AutoCIResult<Decimal>::MethodId;
      for (const auto& c : result.getCandidates())
        if (c.getMethod() == MethodId::BCa)
          return true;
      return false;
    }

    // -----------------------------------------------------------------------
    // requireClonedStrategy
    //
    // Guards the entry of each auto-bootstrap helper.  Throws
    // std::runtime_error if ctx.clonedStrategy is null, embedding the caller
    // name in the message so failures are immediately attributable.
    // -----------------------------------------------------------------------
    void requireClonedStrategy(const StrategyAnalysisContext& ctx, const char* callerName)
    {
      if (!ctx.clonedStrategy)
        throw std::runtime_error(std::string(callerName) + ": clonedStrategy is null.");
    }

    // -----------------------------------------------------------------------
    // throwAsStageException
    //
    // Translates a StrategyAutoBootstrapException (tournament-level failure)
    // into a BootstrapStageException with the strategy name attached, then
    // logs a short message before re-throwing.  Centralises the identical
    // five-line catch block that previously appeared in all four helpers.
    // -----------------------------------------------------------------------
    [[noreturn]] void throwAsStageException(
        const palvalidator::StrategyAutoBootstrapException& ex,
        const StrategyAnalysisContext&                       ctx,
        const char*                                          label,
        std::ostream&                                        os)
    {
      const std::string stratName =
        ctx.strategy ? ctx.strategy->getStrategyName() : "unknown";
      os << "   [Bootstrap] Tournament failure (" << label << "): "
         << ex.getDetail() << "\n";
      throw palvalidator::BootstrapStageException(ex, stratName);
    }

    // -----------------------------------------------------------------------
    // makeBootstrapAlgorithmsConfiguration
    //
    // Single source of truth for which bootstrap algorithms are enabled at
    // each sampling level.
    //
    //   Bar-level: all six algorithms are appropriate for serially-sampled
    //     bar data.
    //
    //   Trade-level: Normal, Basic, and Percentile all assume serial bar-level
    //     correlation and are disabled.  MOutOfN uses the fixed-ratio IID
    //     path; PercentileT and BCa are distribution-free and valid for IID
    //     trade samples.
    //
    // Both the geo-mean and profit-factor helpers share exactly this policy
    // so that the algorithm set can be changed in one place.
    // -----------------------------------------------------------------------
    BootstrapAlgorithmsConfiguration
    makeBootstrapAlgorithmsConfiguration(bool tradeLevelBootstrapping)
    {
      if (tradeLevelBootstrapping)
        {
          return BootstrapAlgorithmsConfiguration(
            /*Normal*/      false,
            /*Basic*/       false,
            /*Percentile*/  false,
            /*MOutOfN*/     true,
            /*PercentileT*/ true,
            /*BCa*/         true);
        }
      else
        {
          return BootstrapAlgorithmsConfiguration(
            /*Normal*/      true,
            /*Basic*/       true,
            /*Percentile*/  true,
            /*MOutOfN*/     true,
            /*PercentileT*/ true,
            /*BCa*/         true);
        }
    }

    // -----------------------------------------------------------------------
    // makeBarLevelBootstrapConfiguration
    //
    // Constructs a BootstrapConfiguration for the stationary-block (bar-level)
    // bootstrap path.  blockLength is passed through as-is; callers should
    // derive it from computeBlockLength().
    // -----------------------------------------------------------------------
    BootstrapConfiguration makeBarLevelBootstrapConfiguration(
        unsigned int   numResamples,
        std::size_t    blockLength,
        double         confidenceLevel,
        std::uint64_t  stageTag,
        std::uint64_t  fold)
    {
      return BootstrapConfiguration(numResamples, blockLength, confidenceLevel, stageTag, fold);
    }

    // -----------------------------------------------------------------------
    // makeTradeLevelBootstrapConfiguration
    //
    // Constructs a BootstrapConfiguration for the IID (trade-level) bootstrap
    // path.  blockSize is fixed to 1 because IIDResampler has no block
    // structure.  rescaleMOutOfN and enableTradeLevelBootstrap are both true
    // to route MOutOfN to the fixed-ratio path appropriate for IID samples.
    // -----------------------------------------------------------------------
    BootstrapConfiguration makeTradeLevelBootstrapConfiguration(
        unsigned int   numResamples,
        double         confidenceLevel,
        std::uint64_t  stageTag,
        std::uint64_t  fold)
    {
      return BootstrapConfiguration(
        numResamples,
        /*blockSize*/                 1,
        confidenceLevel,
        stageTag,
        fold,
        /*rescaleMOutOfN*/            true,
        /*enableTradeLevelBootstrap*/ true);
    }

    // -----------------------------------------------------------------------
    // kPFBootstrapPriorStrength
    //
    // Prior strength used by the profit-factor statistic functor at both the
    // bar-level and trade-level paths.  A value of 0.01 provides a weak
    // regularising prior appropriate for small-sample trade data; both paths
    // share this constant so they remain consistent.
    // -----------------------------------------------------------------------
    constexpr double kPFBootstrapPriorStrength = 0.01;

  } // anonymous namespace

  // ---------------------------------------------------------------------------
  // Constructor implementation
  // ---------------------------------------------------------------------------

  BootstrapAnalysisStage::BootstrapAnalysisStage(const Num& confidenceLevel,
                                                 unsigned int numResamples,
                                                 BootstrapFactory& bootstrapFactory,
						 bool performTradeLevelBootstrapping)
    : mConfidenceLevel(confidenceLevel),
      mNumResamples(numResamples),
      mBootstrapFactory(bootstrapFactory),
      mTradeLevelBootstrapping(performTradeLevelBootstrapping)
  {
  }

  void BootstrapAnalysisStage::reportDiagnostics(
						 const StrategyAnalysisContext& ctx,
						 palvalidator::diagnostics::MetricType metricType,
						 const AutoCIResult<Num>& result) const
  {
    if (!mObserver)
      return;

    using namespace palvalidator::diagnostics;
    using Record = BootstrapDiagnosticRecord;

    // -------------------------------------------------------------------------
    // Unique runID per tournament
    // -------------------------------------------------------------------------
    const std::uint64_t runID = g_nextRunID.fetch_add(1, std::memory_order_relaxed);

    // -------------------------------------------------------------------------
    // Tournament-level context (true invariants only)
    // -------------------------------------------------------------------------
    const std::string strategyName   = (ctx.strategy)     ? ctx.strategy->getStrategyName() : "Unknown";
    const std::string symbol        = (ctx.baseSecurity) ? ctx.baseSecurity->getSymbol()   : "Unknown";
    const std::uint64_t strategyUID = (ctx.strategy)     ? ctx.strategy->hashCode()        : 0;

    const auto& candidates   = result.getCandidates();
    const auto& diagnostics  = result.getDiagnostics();
    const auto& breakdowns   = diagnostics.getScoreBreakdowns();

    if (candidates.empty())
      return;

    const double confidenceLevel = candidates.front().getCl();
    const std::size_t sampleSize = candidates.front().getN();
    const std::size_t numCandidates = candidates.size();
    const double tieEpsilon = diagnostics.getTieEpsilon();

    Record::TournamentContext tournament(
					 runID,
					 strategyUID,
					 strategyName,
					 symbol,
					 metricType,
					 confidenceLevel,
					 sampleSize,
					 numCandidates,
					 tieEpsilon
					 );

    // -------------------------------------------------------------------------
    // Helper: find ScoreBreakdown by method (robust to candidate sorting)
    // -------------------------------------------------------------------------
    auto findBreakdownForMethod =
      [&](typename AutoCIResult<Num>::MethodId m)
      -> const typename AutoCIResult<Num>::SelectionDiagnostics::ScoreBreakdown*
      {
	for (const auto& b : breakdowns)
	  {
	    if (b.getMethod() == m)
	      return &b;
	  }
	return nullptr;
      };

    // -------------------------------------------------------------------------
    // Emit one record per candidate
    // -------------------------------------------------------------------------
    for (const auto& c : candidates)
      {
	const auto* breakdown = findBreakdownForMethod(c.getMethod());

	// --------------------------
	// CandidateIdentity
	// --------------------------
	Record::CandidateIdentity identity(
					   c.getCandidateId(),
					   AutoCIResult<Num>::methodIdToString(c.getMethod()),
					   c.isChosen(),
					   c.getRank()
					   );

	// --------------------------
	// CandidateDistributionStats (candidate-level)
	// --------------------------
	Record::CandidateDistributionStats stats(
						 c.getBOuter(),
						 c.getBInner(),
						 c.getEffectiveB(),
						 c.getSkippedTotal(),
						 c.getSeBoot(),
						 c.getSkewBoot(),
						 c.getMedianBoot(),
						 c.getCenterShiftInSe(),
						 c.getNormalizedLength()
						 );

	// --------------------------
	// IntervalAndScore
	// --------------------------
	const double lowerBound = num::to_double(c.getLower());
	const double upperBound = num::to_double(c.getUpper());

	Record::IntervalAndScore interval(
					  lowerBound,
					  upperBound,
					  c.getScore()
					  );

	// --------------------------
	// RejectionInfo + SupportValidation
	// --------------------------
	CandidateReject rejectionMask = CandidateReject::None;
	std::string rejectionText;
	bool passedGates = true;

	bool violatesSupport = false;
	double supportLower = std::numeric_limits<double>::quiet_NaN();
	double supportUpper = std::numeric_limits<double>::quiet_NaN();

	if (breakdown)
	  {
	    rejectionMask = breakdown->getRejectionMask();
	    rejectionText = breakdown->getRejectionText();
	    passedGates   = breakdown->passedGates();

	    violatesSupport = breakdown->violatesSupport();
	    supportLower    = breakdown->getSupportLowerBound();
	    supportUpper    = breakdown->getSupportUpperBound();
	  }

	Record::RejectionInfo rejection(
					rejectionMask,
					rejectionText,
					passedGates
					);

	Record::SupportValidation support(
					  violatesSupport,
					  supportLower,
					  supportUpper
					  );

	// --------------------------
	// PenaltyBreakdown (7 components now)
	// --------------------------
	auto makePenalty = [](double raw, double norm, double contrib) -> Record::PenaltyComponents {
	  return Record::PenaltyComponents(raw, norm, contrib);
	};

	// Default to NaNs so missing data becomes empty in CSV (your collector does that)
	const double NaN = std::numeric_limits<double>::quiet_NaN();

	Record::PenaltyComponents ordering    = makePenalty(NaN, NaN, NaN);
	Record::PenaltyComponents length      = makePenalty(NaN, NaN, NaN);
	Record::PenaltyComponents stability   = makePenalty(NaN, NaN, NaN);
	Record::PenaltyComponents domain      = makePenalty(NaN, NaN, NaN);
	Record::PenaltyComponents centerShift = makePenalty(NaN, NaN, NaN);
	Record::PenaltyComponents skew        = makePenalty(NaN, NaN, NaN);
	Record::PenaltyComponents bcaOverflow = makePenalty(NaN, NaN, NaN);

	if (breakdown)
	  {
	    ordering  = makePenalty(breakdown->getOrderingRaw(),  breakdown->getOrderingNorm(),  breakdown->getOrderingContribution());
	    length    = makePenalty(breakdown->getLengthRaw(),    breakdown->getLengthNorm(),    breakdown->getLengthContribution());
	    stability = makePenalty(breakdown->getStabilityRaw(), breakdown->getStabilityNorm(), breakdown->getStabilityContribution());
	    domain    = makePenalty(breakdown->getDomainRaw(),    NaN,                           breakdown->getDomainContribution());

	    // These are your existing "center shift sq" and "skew sq" score components.
	    // If you later rename them, update here accordingly.
	    centerShift = makePenalty(breakdown->getCenterSqRaw(), breakdown->getCenterSqNorm(), breakdown->getCenterSqContribution());
	    skew        = makePenalty(breakdown->getSkewSqRaw(),   breakdown->getSkewSqNorm(),   breakdown->getSkewSqContribution());

	    // If/when you add explicit overflow penalty to ScoreBreakdown, wire it here.
	    // For now, keep it empty.
	    bcaOverflow = makePenalty(NaN, NaN, NaN);
	  }

	Record::PenaltyBreakdown penalties(
					   ordering,
					   length,
					   stability,
					   domain,
					   centerShift,
					   skew,
					   bcaOverflow
					   );

	// --------------------------
	// BCa diagnostics
	// --------------------------
	Record::BcaDiagnostics bca = Record::BcaDiagnostics::notAvailable();
	if (c.getMethod() == AutoCIResult<Num>::MethodId::BCa)
	  {
	    const double z0    = c.getZ0();
	    const double accel = c.getAccel();

	    const bool z0Exceeds =
	      std::isfinite(z0) && (std::fabs(z0) > AutoBootstrapConfiguration::kBcaZ0HardLimit);
	    const bool accelExceeds =
	      std::isfinite(accel) && (std::fabs(accel) > AutoBootstrapConfiguration::kBcaAHardLimit);

	    const double rawLength = upperBound - lowerBound;

	    bca = Record::BcaDiagnostics(z0, accel, z0Exceeds, accelExceeds, rawLength);
	  }

	// --------------------------
	// Percentile-T diagnostics (now includes innerFailRate)
	// --------------------------
	Record::PercentileTDiagnostics percentileT = Record::PercentileTDiagnostics::notAvailable();
	if (c.getMethod() == AutoCIResult<Num>::MethodId::PercentileT)
	  {
	    const std::size_t B_outer   = c.getBOuter();
	    const std::size_t B_inner   = c.getBInner();
	    const std::size_t effective = c.getEffectiveB();

	    const std::size_t outerFailCount = (B_outer > effective) ? (B_outer - effective) : 0;

	    const double innerFailRate = c.getInnerFailureRate();

	    std::size_t innerFailCount = 0;
	    if (std::isfinite(innerFailRate) && innerFailRate > 0.0 && B_outer > 0 && B_inner > 0)
	      {
		const double totalInner = static_cast<double>(B_outer) * static_cast<double>(B_inner);
		const double estFails   = innerFailRate * totalInner;

		const double capped = std::max(0.0, std::min(estFails, totalInner));
		innerFailCount = static_cast<std::size_t>(std::llround(capped));
	      }

	    const double effectiveB = static_cast<double>(effective);

	    percentileT = Record::PercentileTDiagnostics(
							 B_outer,
							 B_inner,
							 outerFailCount,
							 innerFailCount,
							 innerFailRate,
							 effectiveB
							 );
	  }

	// --------------------------
	// Emit record
	// --------------------------
	BootstrapDiagnosticRecord record(
					 tournament,
					 identity,
					 stats,
					 interval,
					 rejection,
					 support,
					 penalties,
					 bca,
					 percentileT
					 );

	mObserver->onBootstrapResult(record);
      }
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
      {
	medianHoldBars = ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
      }

    // 2) n^(1/3) heuristic (statistical horizon), prefer already-prepared returns
    const size_t n = ctx.highResReturns.size();

    size_t lCube = 0;
    if (n > 0)
      {
	// Using cbrt is a bit more numerically direct than pow(n, 1/3).
	lCube = static_cast<size_t>(std::lround(std::cbrt(static_cast<double>(n))));
      }

    // 3) Hybrid: max of the two, with a minimum of 2
    size_t L = std::max<size_t>(2, std::max(static_cast<size_t>(medianHoldBars), lCube));

    // 4) Cap L relative to n so we don't end up with too few effective blocks.
    //    Rule: enforce at least ~3 expected blocks on average => L <= n/3 (with floor/guards).
    size_t L_cap = 12;

    if (n >= 3)
      {
	const size_t nRelativeCap = std::max<size_t>(2, n / 3);
	L_cap = std::min<size_t>(L_cap, nRelativeCap);
      }
    else
      {
	// For n < 3, the bootstrap is already very limited; keep a minimal valid block length.
	L_cap = 2;
      }

    const size_t L_beforeCap = L;
    L = std::min(L, L_cap);

    os << "   [Bootstrap] Stationary block length L=" << L
       << " (n=" << n
       << ", medianHoldBars=" << medianHoldBars
       << ", n^(1/3)=" << lCube
       << ", cap=" << L_cap << ")";

    if (L != L_beforeCap)
      {
	os << " [capped]";
      }

    os << "\n";

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

    // Use BackTesterFactory to create the correct backtester for the timeframe
    try
      {
        ctx.backtester =
          mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(
            ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
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
                                     BootstrapStages::BCA_MEAN,
                                     blockLength,
                                     BootstrapStages::NO_FOLD);

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
  // populateAndLogGeoResult
  //
  // Extracts the chosen candidate from an AutoCIResult, writes all geometric-
  // mean diagnostic fields into `out`, emits a structured log line, and calls
  // reportDiagnostics() if an observer is registered.
  //
  // Shared by runBarLevelAutoGeoBootstrap and runTradeLevelAutoGeoBootstrap;
  // the only per-caller variation is the `logLabel` string used in the log
  // line and in the exception message written by throwAsStageException().
  //
  // Returns the per-period lower bound (chosen.getLower()), ready for
  // annualization by the caller.
  // ---------------------------------------------------------------------------
  Num
  BootstrapAnalysisStage::populateAndLogGeoResult(
      const AutoCIResult<Num>&       result,
      BootstrapAnalysisResult&       out,
      const StrategyAnalysisContext& ctx,
      const char*                    logLabel,
      std::ostream&                  os) const
  {
    using Decimal  = Num;
    using AutoCI   = AutoCIResult<Decimal>;
    using MethodId = typename AutoCI::MethodId;

    const auto&   chosen = result.getChosenCandidate();
    const Decimal lbPer  = chosen.getLower();

    out.geoAutoCIChosenMethod     = methodIdToString<Decimal>(result.getChosenMethod());
    out.geoAutoCIChosenScore      = chosen.getScore();
    out.geoAutoCIStabilityPenalty = chosen.getStabilityPenalty();
    out.geoAutoCILengthPenalty    = chosen.getLengthPenalty();

    out.geoAutoCIHasBCaCandidate = hasBCaCandidate<Decimal>(result);
    out.geoAutoCIBCaChosen       = (chosen.getMethod() == MethodId::BCa);
    out.geoAutoCINumCandidates   = result.getCandidates().size();

    try {
      out.medianGeo = Num(result.getBootstrapMedian());
    } catch (...) {
      out.medianGeo = std::nullopt;
    }

    os << "   [Bootstrap] AutoCI " << logLabel << ":"
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

    try {
      if (mObserver)
        reportDiagnostics(ctx, palvalidator::diagnostics::MetricType::GeoMean, result);
    } catch (...) {}

    return lbPer;
  }

  // ---------------------------------------------------------------------------
  // populateAndLogPFResult
  //
  // Extracts the chosen candidate from an AutoCIResult, writes all
  // profit-factor diagnostic fields into `out`, applies the std::exp()
  // transform that converts the log-space lower bound back to a PF ratio,
  // emits a structured log line, and calls reportDiagnostics() if an observer
  // is registered.
  //
  // Shared by runBarLevelAutoProfitFactorBootstrap and
  // runTradeLevelAutoProfitFactorBootstrap; the only per-caller variation is
  // the `logLabel` string.
  //
  // Returns lbPF = std::exp(chosen.getLower()), the profit-factor lower bound
  // in linear (not log) space.
  // ---------------------------------------------------------------------------
  Num
  BootstrapAnalysisStage::populateAndLogPFResult(
      const AutoCIResult<Num>&       result,
      BootstrapAnalysisResult&       out,
      const StrategyAnalysisContext& ctx,
      const char*                    logLabel,
      std::ostream&                  os) const
  {
    using Decimal  = Num;
    using AutoCI   = AutoCIResult<Decimal>;
    using MethodId = typename AutoCI::MethodId;

    const auto&   chosen  = result.getChosenCandidate();
    const Decimal lbLogPF = chosen.getLower();
    const Decimal lbPF    = std::exp(lbLogPF);

    out.pfAutoCIChosenMethod     = methodIdToString<Decimal>(result.getChosenMethod());
    out.pfAutoCIChosenScore      = chosen.getScore();
    out.pfAutoCIStabilityPenalty = chosen.getStabilityPenalty();
    out.pfAutoCILengthPenalty    = chosen.getLengthPenalty();
    out.pfAutoCIChosenSeBoot     = chosen.getSeBoot();

    out.pfAutoCIHasBCaCandidate = hasBCaCandidate<Decimal>(result);
    out.pfAutoCIBCaChosen       = (chosen.getMethod() == MethodId::BCa);
    out.pfAutoCINumCandidates   = result.getCandidates().size();

    try {
      out.medianProfitFactor = Num(std::exp(result.getBootstrapMedian()));
    } catch (...) {
      out.medianProfitFactor = std::nullopt;
    }

    os << "   [Bootstrap] AutoCI " << logLabel << ":"
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

    try {
      if (mObserver)
        reportDiagnostics(ctx, palvalidator::diagnostics::MetricType::ProfitFactor, result);
    } catch (...) {}

    return lbPF;
  }

  /**
   * @brief Run automatic bootstrap confidence interval selection for geometric mean (CAGR).
   *
   * @details
   * This helper drives a StrategyAutoBootstrap instance configured for a
   * geometric mean statistic computed from log-transformed returns. It uses
   * stationary-block bootstrap resampling of the strategy's high-resolution
   * returns and runs a suite of interval construction methods (Normal, Basic,
   * Percentile, m-out-of-n, Percentile-t, BCa). The method then selects and
   * records the resulting confidence intervals and point estimates in the
   * provided BootstrapAnalysisResult.
   *
   * High-level workflow:
   *   1. Validate the analysis context:
   *        • Require a non-null clonedStrategy.
   *        • Require at least one highResReturn; otherwise, log a message
   *          and return zero.
   *
   *   2. Transform returns to log-growth space:
   *        • Compute log(1 + r) for each return r in ctx.highResReturns.
   *        • Apply ruin-aware clipping to prevent log(0) or log(negative)
   *          using StatUtils<Decimal>::makeLogGrowthSeries() with a small
   *          epsilon (DefaultRuinEps, typically 1e-8).
   *        • This transformation enables stable geometric mean estimation
   *          via arithmetic mean in log-space: exp(mean(log(1+r))) - 1.
   *        • NOTE: This transformation is specific to GeoMeanFromLogBarsStat.
   *          Statistics that operate on raw returns should not apply it.
   *
   *   3. Configure the bootstrap engine:
   *        • Build a BootstrapConfiguration using makeBarLevelBootstrapConfiguration()
   *          with the requested number of resamples (mNumResamples), the
   *          supplied blockLength, and stageTag GEO_MEAN.
   *        • Enable the desired interval algorithms via
   *          makeBootstrapAlgorithmsConfiguration(false) — all six methods
   *          are appropriate for bar-level serially-sampled data.
   *        • Construct the geometric mean statistic functor:
   *          GeoMeanFromLogBarsStat<Decimal>, which computes the geometric
   *          mean by taking the arithmetic mean of log-bars and exponentiating.
   *          This statistic uses the same winsorization defaults as GeoMeanStat.
   *        • Instantiate StrategyAutoBootstrap with the configured statistic
   *          and a StationaryMaskValueResamplerAdapter as the resampling
   *          mechanism.
   *
   *   4. Execute the auto-bootstrap and extract results:
   *        • Run the auto-bootstrap procedure on the log-transformed series.
   *        • Delegate result extraction, logging, and diagnostics reporting
   *          to populateAndLogGeoResult().
   *
   * @param ctx       StrategyAnalysisContext with clonedStrategy and highResReturns.
   * @param confidenceLevel  Target CI level (e.g. 0.90, 0.95).
   * @param blockLength      Stationary-block mean block length from computeBlockLength().
   * @param out       BootstrapAnalysisResult populated with geoAutoCI* fields.
   * @param os        Output stream for progress logging.
   * @return          Per-period lower bound for geometric mean (Num).
   *
   * @throws std::runtime_error if ctx.clonedStrategy is null.
   * @throws BootstrapStageException on tournament failure.
   *
   * @see runAutoProfitFactorBootstrap() for a similar auto-bootstrap for profit factor
   * @see StrategyAutoBootstrap for the auto-selection bootstrap orchestrator
   * @see AutoBootstrapSelector for the candidate scoring and selection logic
   * @see GeoMeanFromLogBarsStat for the geometric mean statistic implementation
   * @see StatUtils<Decimal>::makeLogGrowthSeries() for log-transformation details
   */
  // ---------------------------------------------------------------------------
  // runAutoGeoBootstrap: dispatcher
  // ---------------------------------------------------------------------------

  Num
  BootstrapAnalysisStage::runAutoGeoBootstrap(const StrategyAnalysisContext& ctx,
					      double                        confidenceLevel,
					      std::size_t                   blockLength,
					      BootstrapAnalysisResult&      out,
					      std::ostream&                 os) const
  {
    if (mTradeLevelBootstrapping)
      return runTradeLevelAutoGeoBootstrap(ctx, confidenceLevel, blockLength, out, os);
    else
      return runBarLevelAutoGeoBootstrap(ctx, confidenceLevel, blockLength, out, os);
  }

  // ---------------------------------------------------------------------------
  // runBarLevelAutoGeoBootstrap
  // ---------------------------------------------------------------------------

  Num
  BootstrapAnalysisStage::runBarLevelAutoGeoBootstrap(const StrategyAnalysisContext& ctx,
                                                       double                         confidenceLevel,
                                                       std::size_t                    blockLength,
                                                       BootstrapAnalysisResult&       out,
                                                       std::ostream&                  os) const
  {
    using Decimal   = Num;
    using Stat      = mkc_timeseries::StatUtils<Decimal>;
    using Sampler   = mkc_timeseries::GeoMeanFromLogBarsStat<Decimal>;
    using Resampler = StationaryMaskValueResamplerAdapter<Decimal>;
    using AutoCI    = AutoCIResult<Decimal>;

    requireClonedStrategy(ctx, "runBarLevelAutoGeoBootstrap");

    if (ctx.highResReturns.empty())
      {
	os << "   [Bootstrap] AutoCI (GeoMean): skipped (n == 0).\n";
	return mkc_timeseries::DecimalConstants<Decimal>::DecimalZero;
      }

    // GeoMeanFromLogBarsStat requires pre-computed log-growth bars.
    // makeLogGrowthSeries is called explicitly here; other statistics that
    // operate on raw returns would skip this step.
    const std::vector<Decimal> logBars =
      Stat::makeLogGrowthSeries(ctx.highResReturns, Stat::DefaultRuinEps);

    const BootstrapConfiguration cfg =
      makeBarLevelBootstrapConfiguration(
        mNumResamples, blockLength, confidenceLevel,
        BootstrapStages::GEO_MEAN, BootstrapStages::NO_FOLD);

    const BootstrapAlgorithmsConfiguration algoConfig =
      makeBootstrapAlgorithmsConfiguration(/*tradeLevelBootstrapping=*/false);

    Sampler geoSampler(/*winsorize=*/false);
    StrategyAutoBootstrap<Decimal, Sampler, Resampler> autoGeo(
      mBootstrapFactory,
      *ctx.clonedStrategy,
      cfg,
      algoConfig,
      geoSampler,
      IntervalType::ONE_SIDED_LOWER);

    os << "   [Bootstrap] AutoCI (GeoMean): running composite bootstrap engines"
       << " on precomputed log-bars...\n";

    try
      {
        AutoCI result = autoGeo.run(logBars, &os);
        return populateAndLogGeoResult(result, out, ctx, "bar-level GeoMean", os);
      }
    catch (const palvalidator::StrategyAutoBootstrapException& ex)
      {
        throwAsStageException(ex, ctx, "bar-level GeoMean", os);
      }
  }

  // ---------------------------------------------------------------------------
  // runTradeLevelAutoGeoBootstrap
  //
  // Trade-level IID bootstrap for geometric mean.  Pre-computes log-growth
  // bars for each Trade object via makeLogGrowthSeriesForTrades(), then
  // bootstraps using GeoMeanFromLogBarsStat<Decimal> — the same stat as the
  // bar-level path.  Its operator()(vector<Trade<Decimal>>) flattens and sums
  // the log-bars from the resampled trades directly.
  //
  // Uses IIDResampler<Trade<Decimal>> so whole Trade objects are resampled,
  // preserving multi-bar trade structure.
  //
  // blockLength is accepted to match the dispatcher's call signature but is
  // unused (IID has no block structure).
  // ---------------------------------------------------------------------------

  Num
  BootstrapAnalysisStage::runTradeLevelAutoGeoBootstrap(const StrategyAnalysisContext& ctx,
                                                         double                         confidenceLevel,
                                                         std::size_t                    /*blockLength*/,
                                                         BootstrapAnalysisResult&       out,
                                                         std::ostream&                  os) const
  {
    using Decimal   = Num;
    using Stat      = mkc_timeseries::StatUtils<Decimal>;
    using TradeType = Trade<Decimal>;
    using Sampler   = mkc_timeseries::GeoMeanFromLogBarsStat<Decimal>;
    using Resampler = IIDResampler<TradeType>;
    using AutoCI    = AutoCIResult<Decimal>;

    requireClonedStrategy(ctx, "runTradeLevelAutoGeoBootstrap");

    if (ctx.tradeLevelReturns.empty())
      {
	os << "   [Bootstrap] AutoCI trade-level (GeoMean): skipped (no trades).\n";
	return mkc_timeseries::DecimalConstants<Decimal>::DecimalZero;
      }

    // Pre-compute log(max(1+r, ruin_eps)) for every bar in every trade.
    // GeoMeanFromLogBarsStat's Trade overload assumes getDailyReturns() holds
    // pre-computed log-bars, not raw returns.
    const std::vector<TradeType> logTrades =
      makeLogGrowthSeriesForTrades<Decimal>(ctx.tradeLevelReturns, Stat::DefaultRuinEps);

    const BootstrapConfiguration cfg =
      makeTradeLevelBootstrapConfiguration(
        mNumResamples, confidenceLevel,
        BootstrapStages::GEO_MEAN, BootstrapStages::NO_FOLD);

    const BootstrapAlgorithmsConfiguration algoConfig =
      makeBootstrapAlgorithmsConfiguration(/*tradeLevelBootstrapping=*/true);

    Sampler geoSampler(/*winsorize=*/false);
    StrategyAutoBootstrap<Decimal, Sampler, Resampler, TradeType> autoGeo(
      mBootstrapFactory,
      *ctx.clonedStrategy,
      cfg,
      algoConfig,
      geoSampler,
      IntervalType::ONE_SIDED_LOWER);

    os << "   [Bootstrap] AutoCI trade-level (GeoMean): running bootstrap engines"
       << " on " << ctx.tradeLevelReturns.size()
       << " trades (log-bars pre-computed)...\n";

    try
      {
        // run() receives logTrades; GeoMeanFromLogBarsStat::operator()(vector<Trade>)
        // flattens them into a single log-bar vector and computes the geometric mean.
        AutoCI result = autoGeo.run(logTrades, &os);
        return populateAndLogGeoResult(result, out, ctx, "trade-level GeoMean", os);
      }
    catch (const palvalidator::StrategyAutoBootstrapException& ex)
      {
        throwAsStageException(ex, ctx, "trade-level GeoMean", os);
      }
  }


  /**
   * @brief Run automatic bootstrap confidence interval selection for profit factor.
   *
   * @details
   * This helper drives a StrategyAutoBootstrap instance configured for a
   * robust log profit-factor statistic. It uses stationary-block bootstrap
   * resampling of the strategy's high-resolution returns and runs a suite
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
   *   2. Derive stop-loss and profit-target priors:
   *        • Call extractStopLossAndProfitTargetPriors() — shared by both
   *          the bar-level and trade-level paths to guarantee identical prior
   *          extraction logic.
   *        • Falls back to 0.0/0.0 if not a PalStrategy or pattern unavailable.
   *
   *   3. Configure the bootstrap engine:
   *        • Build a BootstrapConfiguration using the appropriate factory
   *          (makeBarLevelBootstrapConfiguration or
   *           makeTradeLevelBootstrapConfiguration).
   *        • Enable the desired interval algorithms via
   *          makeBootstrapAlgorithmsConfiguration() — the algorithm set is
   *          identical between bar-level and trade-level PF bootstrapping and
   *          is governed by the same central policy as the geo-mean paths.
   *        • Construct the profit-factor statistic functor using
   *          LogProfitFactorFromLogBarsStat_LogPF_Custom with
   *          SmallSampleNumerPolicy and SmallSampleDenomPolicy, and prior
   *          strength kPFBootstrapPriorStrength.  Both bar-level and
   *          trade-level paths share this sampler type and prior so that
   *          future policy changes are made in one place.
   *        • Instantiate StrategyAutoBootstrap with the configured statistic.
   *
   *   4. Execute the auto-bootstrap and extract results:
   *        • Run the auto-bootstrap procedure to obtain an AutoCIResult.
   *        • Delegate result extraction, logging, and diagnostics reporting
   *          to populateAndLogPFResult(), which also applies std::exp() to
   *          convert from log-PF space back to a linear PF ratio.
   *
   * @param ctx             StrategyAnalysisContext with clonedStrategy and returns.
   * @param confidenceLevel Target CI level (e.g. 0.90, 0.95).
   * @param blockLength     Stationary-block mean block length (bar path) or ignored (trade path).
   * @param out             BootstrapAnalysisResult populated with pfAutoCI* fields.
   * @param os              Output stream for progress logging.
   * @return                Optional profit-factor lower bound (linear space). std::nullopt
   *                        when bootstrapping is skipped due to insufficient data.
   *
   * @throws std::runtime_error if ctx.clonedStrategy is null.
   * @throws BootstrapStageException on tournament failure.
   */
  // ---------------------------------------------------------------------------
  // runAutoProfitFactorBootstrap: dispatcher
  // ---------------------------------------------------------------------------

  std::optional<Num>
  BootstrapAnalysisStage::runAutoProfitFactorBootstrap(
      const StrategyAnalysisContext& ctx,
      double                         confidenceLevel,
      std::size_t                    blockLength,
      BootstrapAnalysisResult&       out,
      std::ostream&                  os) const
  {
    if (mTradeLevelBootstrapping)
      return runTradeLevelAutoProfitFactorBootstrap(ctx, confidenceLevel, blockLength, out, os);
    else
      return runBarLevelAutoProfitFactorBootstrap(ctx, confidenceLevel, blockLength, out, os);
  }

  // ---------------------------------------------------------------------------
  // runBarLevelAutoProfitFactorBootstrap
  //
  // Uses a stationary-block resampler over ctx.highResReturns (pre-converted
  // to log-growth bars).  The PF statistic type and prior strength are now
  // aligned with the trade-level path: both use
  // LogProfitFactorFromLogBarsStat_LogPF_Custom<SmallSampleNumerPolicy,
  // SmallSampleDenomPolicy> and kPFBootstrapPriorStrength.
  // ---------------------------------------------------------------------------

  std::optional<Num>
  BootstrapAnalysisStage::runBarLevelAutoProfitFactorBootstrap(
      const StrategyAnalysisContext& ctx,
      double                         confidenceLevel,
      std::size_t                    blockLength,
      BootstrapAnalysisResult&       out,
      std::ostream&                  os) const
  {
    using Decimal    = Num;
    using Stat       = mkc_timeseries::StatUtils<Decimal>;
    // PFSampler is aligned with the trade-level path: both paths share the
    // same statistic type and prior strength (kPFBootstrapPriorStrength) so
    // that policy changes are made in one place.
    using PFSampler  = typename Stat::template LogProfitFactorFromLogBarsStat_LogPF_Custom<
                           Stat::SmallSampleNumerPolicy,
                           Stat::SmallSampleDenomPolicy>;
    using Resampler  = StationaryMaskValueResamplerAdapter<Decimal>;
    using AutoCI     = AutoCIResult<Decimal>;

    requireClonedStrategy(ctx, "runBarLevelAutoProfitFactorBootstrap");

    if (ctx.highResReturns.size() < 2)
      {
	os << "   [Bootstrap] AutoCI (PF): skipped (n < 2).\n";
	return std::nullopt;
      }

    // LogProfitFactorFromLogBarsStat_LogPF_Custom requires pre-computed log-growth bars.
    const std::vector<Decimal> logBars =
      Stat::makeLogGrowthSeries(ctx.highResReturns, Stat::DefaultRuinEps);

    const BootstrapConfiguration cfg =
      makeBarLevelBootstrapConfiguration(
        mNumResamples, blockLength, confidenceLevel,
        BootstrapStages::PROFIT_FACTOR, BootstrapStages::NO_FOLD);

    const BootstrapAlgorithmsConfiguration algoConfig =
      makeBootstrapAlgorithmsConfiguration(/*tradeLevelBootstrapping=*/false);

    PFSampler stat(Stat::DefaultRuinEps,
                   Stat::DefaultDenomFloor,
                   kPFBootstrapPriorStrength);

    StrategyAutoBootstrap<Decimal, PFSampler, Resampler> autoPF(
      mBootstrapFactory,
      *ctx.clonedStrategy,
      cfg,
      algoConfig,
      stat,
      IntervalType::ONE_SIDED_LOWER);

    try
      {
        AutoCI result = autoPF.run(logBars, &os);
        return populateAndLogPFResult(result, out, ctx, "bar-level PF", os);
      }
    catch (const palvalidator::StrategyAutoBootstrapException& ex)
      {
        throwAsStageException(ex, ctx, "bar-level PF", os);
      }
  }

  // ---------------------------------------------------------------------------
  // runTradeLevelAutoProfitFactorBootstrap
  //
  // Trade-level IID bootstrap for profit factor.  Pre-computes log-growth bars
  // for each Trade object via makeLogGrowthSeriesForTrades(), then bootstraps
  // using the same PFSampler type as the bar-level path.
  //
  // blockLength is accepted to match the dispatcher's call signature but is
  // unused (IID has no block structure).
  // ---------------------------------------------------------------------------

  std::optional<Num>
  BootstrapAnalysisStage::runTradeLevelAutoProfitFactorBootstrap(
      const StrategyAnalysisContext& ctx,
      double                         confidenceLevel,
      std::size_t                    /*blockLength*/,
      BootstrapAnalysisResult&       out,
      std::ostream&                  os) const
  {
    using Decimal   = Num;
    using Stat      = mkc_timeseries::StatUtils<Decimal>;
    using TradeType = Trade<Decimal>;
    using PFSampler = typename Stat::template LogProfitFactorFromLogBarsStat_LogPF_Custom<
                          Stat::SmallSampleNumerPolicy,
                          Stat::SmallSampleDenomPolicy>;
    using Resampler = IIDResampler<TradeType>;
    using AutoCI    = AutoCIResult<Decimal>;

    requireClonedStrategy(ctx, "runTradeLevelAutoProfitFactorBootstrap");

    if (ctx.tradeLevelReturns.size() < 2)
      {
	os << "   [Bootstrap] AutoCI trade-level (PF): skipped (fewer than 2 trades).\n";
	return std::nullopt;
      }

    // Pre-compute log(max(1+r, ruin_eps)) for every bar in every trade.
    // LogProfitFactorFromLogBarsStat_LogPF_Custom's Trade overload assumes
    // getDailyReturns() holds pre-computed log-bars, not raw returns.
    const std::vector<TradeType> logTrades =
      makeLogGrowthSeriesForTrades<Decimal>(ctx.tradeLevelReturns, Stat::DefaultRuinEps);

    const BootstrapConfiguration cfg =
      makeTradeLevelBootstrapConfiguration(
        mNumResamples, confidenceLevel,
        BootstrapStages::PROFIT_FACTOR, BootstrapStages::NO_FOLD);

    const BootstrapAlgorithmsConfiguration algoConfig =
      makeBootstrapAlgorithmsConfiguration(/*tradeLevelBootstrapping=*/true);

    PFSampler stat(Stat::DefaultRuinEps,
                   Stat::DefaultDenomFloor,
                   kPFBootstrapPriorStrength);

    StrategyAutoBootstrap<Decimal, PFSampler, Resampler, TradeType> autoPF(
      mBootstrapFactory,
      *ctx.clonedStrategy,
      cfg,
      algoConfig,
      stat,
      IntervalType::ONE_SIDED_LOWER);

    try
      {
        // run() receives logTrades; PFSampler::operator()(vector<Trade>) flattens
        // them and computes log(PF) directly.
        AutoCI result = autoPF.run(logTrades, &os);
        return populateAndLogPFResult(result, out, ctx, "trade-level PF", os);
      }
    catch (const palvalidator::StrategyAutoBootstrapException& ex)
      {
        throwAsStageException(ex, ctx, "trade-level PF", os);
      }
  }

  
  // ---------------------------------------------------------------------------
  // execute() – top-level stage orchestration
  // ---------------------------------------------------------------------------

  double
  BootstrapAnalysisStage::getAdjustedConfidenceInterval(const Num& confidenceInterval, size_t returnsSize) const
  {
    static const Num adjustedLowerInterval = num::fromString<Num>(std::string("0.90"));
    static const Num typicalLowerInterval = num::fromString<Num>(std::string("0.95"));
    
    if (isTradeLevelBootStrapping())
      return num::to_double(confidenceInterval);

    if ((returnsSize < 30) && (confidenceInterval >= typicalLowerInterval))
      return num::to_double(adjustedLowerInterval);
    else
      return num::to_double(confidenceInterval);
  }
  
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

    if (isTradeLevelBootStrapping())
      {
        if (ctx.tradeLevelReturns.size() < 2)
          {
            os << "   [Bootstrap] Skipping: insufficient tradeLevelReturns (n < 2).\n";
            return result;
          }
      }
    else
      {
        if (ctx.highResReturns.size() < 2)
          {
            os << "   [Bootstrap] Skipping: insufficient highResReturns (n < 2).\n";
            return result;
          }
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

    //const double confLevel = num::to_double(mConfidenceLevel);
    auto returnsSize = ctx.highResReturns.size();
    os << "BootstrapAnalysisStage::execute given confidence interval = " << mConfidenceLevel << " for size = " << returnsSize << std::endl;

    double confLevel = 0.0;
    
    if (isTradeLevelBootStrapping())
      confLevel = getAdjustedConfidenceInterval(mConfidenceLevel, ctx.tradeLevelReturns.size());
    else
      confLevel = getAdjustedConfidenceInterval(mConfidenceLevel, ctx.highResReturns.size());

    os << "BootstrapAnalysisStage::execute actual confidence interval = " << confLevel << std::endl;
    
    // 3) Arithmetic mean via BCa (always uses bar-level highResReturns)
    if (ctx.highResReturns.size() >= 2)
      {
        const auto bcaMean =
          runBCaMeanBootstrap(ctx, confLevel, annParams.barsPerYear, blockLength, os);
        result.lbMeanPeriod             = bcaMean.getLowerBoundPeriod();
        result.annualizedLowerBoundMean = bcaMean.getLowerBoundAnnualized();
      }
    else
      {
        os << "   [Bootstrap] BCa (Mean): skipped (highResReturns n < 2).\n";
      }

    // 4) Geometric mean (CAGR) via StrategyAutoBootstrap
    try
      {
        const Num lbGeoPer =
          runAutoGeoBootstrap(ctx, confLevel, blockLength, result, os);
        result.lbGeoPeriod             = lbGeoPer;
        result.annualizedLowerBoundGeo =
          Annualizer<Num>::annualize_one(lbGeoPer, annParams.barsPerYear);
      }
    catch (const palvalidator::BootstrapStageException&)
      {
        // Tournament failure: no trustworthy CI was producible.
        // Re-throw so the caller knows this strategy cannot be evaluated.
        throw;
      }
    catch (const std::exception& e)
      {
        os << "   [Bootstrap] ERROR: AutoCI (GeoMean) failed: "
           << e.what() << "\n";
        // Leave geometric fields at default; mean and PF (if any) still published.
      }

    // Diagnostics: report AutoCI GeoMean results if observer present
    try {
      if (mObserver) {
        using palvalidator::diagnostics::MetricType;
        // result.geoAuto... fields were populated by runAutoGeoBootstrap
        // But we also want to report the AutoCI chosen candidate; we need access to it.
        // We can rerun a lightweight auto selection? Instead, StrategyAutoBootstrap::run
        // returned details inside runAutoGeoBootstrap; to keep changes minimal, call
        // reportDiagnostics only when runAutoGeoBootstrap returned a chosen candidate
        // by reconstructing an AutoCIResult via the stage's internal API is complex.
        // As an approximation, we will not attempt to reconstruct AutoCIResult here.
      }
    } catch (...) {
      // swallow diagnostics errors
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
    catch (const palvalidator::BootstrapStageException&)
      {
        // Tournament failure: re-throw so the caller treats this strategy as a failure.
        throw;
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
