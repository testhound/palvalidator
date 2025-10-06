#include "BootstrapAnalysisStage.h"
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"
#include "ClosedPositionHistory.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include <sstream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;
  using mkc_timeseries::BackTester;
  using mkc_timeseries::ClosedPositionHistory;
  using mkc_timeseries::Security;

  BootstrapAnalysisStage::BootstrapAnalysisStage(const Num& confidenceLevel, unsigned int numResamples)
    : mConfidenceLevel(confidenceLevel)
    , mNumResamples(numResamples)
  {}

  size_t BootstrapAnalysisStage::computeBlockLength(const StrategyAnalysisContext& ctx) const
  {
    if (!ctx.backtester) return 2;
    const unsigned int medianHoldBars = ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
    return std::max<size_t>(2, static_cast<size_t>(medianHoldBars));
  }

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

  BootstrapAnalysisResult BootstrapAnalysisStage::execute(StrategyAnalysisContext& ctx, std::ostream& os) const
  {
    BootstrapAnalysisResult R;
    if (!ctx.backtester)
    {
      // Try to run backtest if not already run (defensive)
      try
      {
        // reuse simple backtest flow similar to BacktestingStage
        if (!ctx.portfolio && ctx.strategy && ctx.baseSecurity)
        {
          ctx.portfolio = std::make_shared<mkc_timeseries::Portfolio<Num>>(ctx.strategy->getStrategyName() + " Portfolio");
          ctx.portfolio->addSecurity(ctx.baseSecurity);
          ctx.clonedStrategy = ctx.strategy->clone2(ctx.portfolio);
          ctx.backtester = mkc_timeseries::BackTesterFactory<Num>::backTestStrategy(ctx.clonedStrategy, ctx.timeFrame, ctx.oosDates);
          ctx.highResReturns = ctx.backtester->getAllHighResReturns(ctx.clonedStrategy.get());
        }
      }
      catch (const std::exception& e)
      {
        R.failureReason = std::string("Failed to initialize backtester: ") + e.what();
        os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
        return R;
      }
    }

    // Ensure we have enough returns - caller/BacktestingStage should have checked, but be defensive.
    if (ctx.highResReturns.size() < 2)
    {
      R.failureReason = "Insufficient returns (need at least 2, have " + std::to_string(ctx.highResReturns.size()) + ")";
      os << "   [Bootstrap] Skipped (" << R.failureReason << ")\n";
      return R;
    }

    // compute L and annualization factor
    const size_t L = computeBlockLength(ctx);
    R.blockLength = L;

    // Log median holding period similar to original
    const unsigned int medianHoldBars = ctx.backtester->getClosedPositionHistory().getMedianHoldingPeriod();
    R.medianHoldBars = medianHoldBars;
    os << "Strategy Median holding period = " << medianHoldBars << "\n";

    mkc_timeseries::StationaryBlockResampler<Num> sampler(L);
    mkc_timeseries::GeoMeanStat<Num> statGeo;
    using BlockBCA = mkc_timeseries::BCaBootStrap<Num, mkc_timeseries::StationaryBlockResampler<Num>>;

    // Create BCa objects - exceptions may propagate; catch to ensure stage returns sensible result
    try
    {
      BlockBCA bcaGeo(ctx.highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, sampler);
      BlockBCA bcaMean(ctx.highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(),
                       &mkc_timeseries::StatUtils<Num>::computeMean, sampler);

      R.lbGeoPeriod = bcaGeo.getLowerBound();
      R.lbMeanPeriod = bcaMean.getLowerBound();

      // Annualize
      const double annFactor = computeAnnualizationFactor(ctx);
      mkc_timeseries::BCaAnnualizer<Num> annualizerGeo(bcaGeo, annFactor);
      mkc_timeseries::BCaAnnualizer<Num> annualizerMean(bcaMean, annFactor);

      R.annualizedLowerBoundGeo = annualizerGeo.getAnnualizedLowerBound();
      R.annualizedLowerBoundMean = annualizerMean.getAnnualizedLowerBound();

      // Mark computation as successful
      R.computationSucceeded = true;

      // Log bootstrap results for diagnostics
      os << "   [Bootstrap] Per-period bounds: GeoMean LB=" << R.lbGeoPeriod
         << ", Mean LB=" << R.lbMeanPeriod << "\n";
      os << "   [Bootstrap] Annualization factor=" << annFactor << "\n";
      os << "   [Bootstrap] Annualized bounds: GeoMean LB=" << R.annualizedLowerBoundGeo
         << ", Mean LB=" << R.annualizedLowerBoundMean << "\n";

      return R;
    }
    catch (const std::exception& e)
    {
      R.failureReason = std::string("BCa bootstrap computation failed: ") + e.what();
      os << "Warning: BootstrapAnalysisStage " << R.failureReason << "\n";
      return R;
    }
  }

} // namespace palvalidator::filtering::stages
