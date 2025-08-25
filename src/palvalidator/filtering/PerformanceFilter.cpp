#include "PerformanceFilter.h"
#include <algorithm>
#include <set>
#include <iomanip>
#include "BackTester.h"
#include "Portfolio.h"
#include "DecimalConstants.h"
#include "BiasCorrectedBootstrap.h"
#include "StatUtils.h"
#include "utils/TimeUtils.h"

namespace palvalidator
{
namespace filtering
{

PerformanceFilter::PerformanceFilter(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples)
    : mHurdleCalculator(riskParams),
      mConfidenceLevel(confidenceLevel),
      mNumResamples(numResamples),
      mRobustnessConfig(),
      mFragileEdgePolicy(),
      mFilteringSummary(),
      mApplyFragileAdvice(true)
{
}

std::vector<std::shared_ptr<PalStrategy<Num>>> PerformanceFilter::filterByPerformance(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration timeFrame,
    std::ostream& outputStream)
{
    std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;
    
    // Reset summary for new filtering run
    mFilteringSummary = FilteringSummary();
    
    const Num riskFreeHurdle = mHurdleCalculator.calculateRiskFreeHurdle();


    outputStream << "\nFiltering " << survivingStrategies.size() << " surviving strategies by BCa performance...\n";
    outputStream << "Filter 1 (Statistical Viability): Annualized Lower Bound > 0\n";
    outputStream << "Filter 2 (Economic Significance): Annualized Lower Bound > (Annualized Cost Hurdle * "
                 << mHurdleCalculator.getCostBufferMultiplier() << ")\n";
    outputStream << "Filter 3 (Risk-Adjusted Return): Annualized Lower Bound > (Risk-Free Rate + Risk Premium ( "
                 << mHurdleCalculator.getRiskPremium() << ") )\n";
    outputStream << "  - Cost assumptions: $0 commission, 0.10% slippage/spread per side.\n";
    outputStream << "  - Risk-Free Rate assumption: " << (mHurdleCalculator.getRiskFreeRate() * DecimalConstants<Num>::DecimalOneHundred) << "%.\n";

    for (const auto& strategy : survivingStrategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);

            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat, timeFrame, backtestingDates);
            auto highResReturns = backtester->getAllHighResReturns(clonedStrat.get());

            if (highResReturns.size() < 20)
            {
                outputStream << "✗ Strategy filtered out: " << strategy->getStrategyName()
                           << " - Insufficient returns for bootstrap (" << highResReturns.size() << " < 20).\n";
                mFilteringSummary.incrementInsufficientCount();
                continue;
            }

            const unsigned int medianHoldBars = backtester->getClosedPositionHistory().getMedianHoldingPeriod();
            outputStream << "Strategy Median holding period = " << medianHoldBars << "\n";
            const std::size_t L = std::max<std::size_t>(2, static_cast<std::size_t>(medianHoldBars));
            StationaryBlockResampler<Num> sampler(L);

            GeoMeanStat<Num> statGeo;
            using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;
            BlockBCA bcaGeo(highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, sampler);
            BlockBCA bcaMean(highResReturns, mNumResamples, mConfidenceLevel.getAsDouble(),
                           &mkc_timeseries::StatUtils<Num>::computeMean, sampler);

            const Num lbGeoPeriod = bcaGeo.getLowerBound();
            const Num lbMeanPeriod = bcaMean.getLowerBound();

            double annualizationFactor;
            if (timeFrame == TimeFrame::INTRADAY)
            {
                annualizationFactor = calculateAnnualizationFactor(
                    timeFrame,
                    baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes());
            }
            else
            {
                annualizationFactor = calculateAnnualizationFactor(timeFrame);
            }

            BCaAnnualizer<Num> annualizerGeo(bcaGeo, annualizationFactor);
            BCaAnnualizer<Num> annualizerMean(bcaMean, annualizationFactor);

            const Num annualizedLowerBoundGeo = annualizerGeo.getAnnualizedLowerBound();
            const Num annualizedLowerBoundMean = annualizerMean.getAnnualizedLowerBound();

            // Calculate hurdles
            const Num annualizedTrades(backtester->getEstimatedAnnualizedTrades());
            const Num finalRequiredReturn = mHurdleCalculator.calculateFinalRequiredReturn(annualizedTrades);

            // Early decision on GM LB vs hurdle
            if (!passesHurdleRequirements(annualizedLowerBoundGeo, finalRequiredReturn))
            {
                outputStream << "✗ Strategy filtered out: " << strategy->getStrategyName()
                           << " (Lower Bound = "
                           << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
                           << "% <= Required Return = "
                           << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
                           << "  [Block L=" << L << "]\n\n";
                continue;
            }

            // Diagnostic AM–GM divergence
            const auto divergence = palvalidator::analysis::DivergenceAnalyzer::assessAMGMDivergence(
                annualizedLowerBoundGeo, annualizedLowerBoundMean, /*absThresh=*/0.05, /*relThresh=*/0.30);

            // Robustness gate
            double lSensitivityRelVar = 0.0;
            const bool nearHurdle = (annualizedLowerBoundGeo <= (finalRequiredReturn + mRobustnessConfig.borderlineAnnualMargin));
            const bool smallN = (highResReturns.size() < mRobustnessConfig.minTotalForSplit);
            const bool mustRobust = divergence.flagged || nearHurdle || smallN;

            if (mustRobust)
            {
                if (!processRobustnessChecks(strategy, strategy->getStrategyName(), highResReturns, L,
                                           annualizationFactor, finalRequiredReturn, divergence,
                                           nearHurdle, smallN, outputStream))
                {
                    continue;
                }
                // If we get here, robustness checks passed - get the relVar for fragile edge analysis
                const auto rob = palvalidator::analysis::RobustnessAnalyzer::runFlaggedStrategyRobustness(
                    strategy->getStrategyName(), highResReturns, L, annualizationFactor,
                    finalRequiredReturn, mRobustnessConfig, outputStream);
                lSensitivityRelVar = rob.relVar;
            }

            // Fragile edge advisory
            if (!processFragileEdgeAnalysis(lbGeoPeriod, annualizedLowerBoundGeo, finalRequiredReturn,
                                          lSensitivityRelVar, highResReturns, outputStream))
            {
                continue;
            }

            // Keep strategy
            filteredStrategies.push_back(strategy);

            outputStream << "✓ Strategy passed: " << strategy->getStrategyName()
                       << " (Lower Bound = "
                       << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
                       << "% > Required Return = "
                       << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
                       << "  [Block L=" << L << "]\n";

            outputStream << "   ↳ Lower bounds (annualized): "
                       << "GeoMean = " << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                       << "Mean = " << (annualizedLowerBoundMean * DecimalConstants<Num>::DecimalOneHundred) << "%\n\n";
        }
        catch (const std::exception& e)
        {
            outputStream << "Warning: Failed to evaluate strategy '" << strategy->getStrategyName()
                       << "' performance: " << e.what() << "\n";
            outputStream << "Excluding strategy from filtered results.\n";
        }
    }

    // Count survivors by direction
    auto [survivorsLong, survivorsShort] = countSurvivorsByDirection(filteredStrategies);

    // Summary
    outputStream << "BCa Performance Filtering complete: " << filteredStrategies.size()
                << "/" << survivingStrategies.size() << " strategies passed criteria.\n\n";
    outputStream << "[Summary] Flagged for divergence: " << mFilteringSummary.getFlaggedCount()
                << " (passed robustness: " << mFilteringSummary.getFlagPassCount() << ", failed: "
                << (mFilteringSummary.getFlaggedCount() >= mFilteringSummary.getFlagPassCount() ? 
                   (mFilteringSummary.getFlaggedCount() - mFilteringSummary.getFlagPassCount()) : 0) << ")\n";
    outputStream << "          Fail reasons → "
                << "L-bound/hurdle: " << mFilteringSummary.getFailLBoundCount()
                << ", L-variability near hurdle: " << mFilteringSummary.getFailLVarCount()
                << ", split-sample: " << mFilteringSummary.getFailSplitCount()
                << ", tail-risk: " << mFilteringSummary.getFailTailCount() << "\n";
    outputStream << "          Insufficient sample (pre-filter): " << mFilteringSummary.getInsufficientCount() << "\n";
    outputStream << "          Survivors by direction → Long: " << survivorsLong
                << ", Short: " << survivorsShort << "\n";

    return filteredStrategies;
}

bool PerformanceFilter::passesHurdleRequirements(
    const Num& annualizedLowerBoundGeo,
    const Num& finalRequiredReturn) const
{
    return annualizedLowerBoundGeo > finalRequiredReturn;
}

bool PerformanceFilter::processRobustnessChecks(
    std::shared_ptr<PalStrategy<Num>> strategy,
    const std::string& strategyName,
    const std::vector<Num>& highResReturns,
    size_t L,
    double annualizationFactor,
    const Num& finalRequiredReturn,
    const palvalidator::analysis::DivergenceResult<Num>& divergence,
    bool nearHurdle,
    bool smallN,
    std::ostream& outputStream)
{
    if (divergence.flagged)
    {
        mFilteringSummary.incrementFlaggedCount();
        outputStream << "   [FLAG] Large AM vs GM divergence (abs="
                   << (Num(divergence.absDiff) * DecimalConstants<Num>::DecimalOneHundred) << "%, rel=";
        if (divergence.relState == palvalidator::analysis::DivergencePrintRel::Defined)
            outputStream << divergence.relDiff;
        else
            outputStream << "n/a";
        outputStream << "); running robustness checks";

        if (nearHurdle || smallN)
        {
            outputStream << " (also triggered by ";
            if (nearHurdle)
                outputStream << "near-hurdle";
            if (nearHurdle && smallN)
                outputStream << " & ";
            if (smallN)
                outputStream << "small-sample";
            outputStream << ")";
        }
        outputStream << "...\n";
    }
    else
    {
        outputStream << "   [CHECK] Running robustness checks due to "
                   << (nearHurdle ? "near-hurdle" : "")
                   << ((nearHurdle && smallN) ? " & " : "")
                   << (smallN ? "small-sample" : "")
                   << " condition(s)...\n";
    }

    const auto rob = palvalidator::analysis::RobustnessAnalyzer::runFlaggedStrategyRobustness(
        strategyName, highResReturns, L, annualizationFactor, finalRequiredReturn, mRobustnessConfig, outputStream);

    if (rob.verdict == palvalidator::analysis::RobustnessVerdict::ThumbsDown)
    {
        switch (rob.reason)
        {
            case palvalidator::analysis::RobustnessFailReason::LSensitivityBound:
                mFilteringSummary.incrementFailLBoundCount();
                break;
            case palvalidator::analysis::RobustnessFailReason::LSensitivityVarNearHurdle:
                mFilteringSummary.incrementFailLVarCount();
                break;
            case palvalidator::analysis::RobustnessFailReason::SplitSample:
                mFilteringSummary.incrementFailSplitCount();
                break;
            case palvalidator::analysis::RobustnessFailReason::TailRisk:
                mFilteringSummary.incrementFailTailCount();
                break;
            default:
                break;
        }
        outputStream << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]") << " Robustness checks FAILED → excluding strategy.\n\n";
        return false;
    }
    else
    {
        if (divergence.flagged)
            mFilteringSummary.incrementFlagPassCount();

        outputStream << "   " << (divergence.flagged ? "[FLAG]" : "[CHECK]") << " Robustness checks PASSED.\n";
        return true;
    }
}

bool PerformanceFilter::processFragileEdgeAnalysis(
    const Num& lbGeoPeriod,
    const Num& annualizedLowerBoundGeo,
    const Num& finalRequiredReturn,
    double lSensitivityRelVar,
    const std::vector<Num>& highResReturns,
    std::ostream& outputStream)
{
    const auto [q05, es05] = palvalidator::analysis::FragileEdgeAnalyzer::computeQ05_ES05(highResReturns, /*alpha=*/0.05);
    const auto advice = palvalidator::analysis::FragileEdgeAnalyzer::analyzeFragileEdge(
        lbGeoPeriod,                  // per-period GM LB
        annualizedLowerBoundGeo,      // annualized GM LB
        finalRequiredReturn,          // hurdle (annual)
        lSensitivityRelVar,           // relVar from robustness; 0.0 if unrun
        q05,                          // tail quantile
        es05,                         // ES05 (logged elsewhere)
        highResReturns.size(),        // n
        mFragileEdgePolicy            // thresholds
    );

    auto fragileActionToText = [](palvalidator::analysis::FragileEdgeAction a) -> const char*
    {
        switch (a)
        {
            case palvalidator::analysis::FragileEdgeAction::Keep:       return "Keep";
            case palvalidator::analysis::FragileEdgeAction::Downweight: return "Downweight";
            case palvalidator::analysis::FragileEdgeAction::Drop:       return "Drop";
            default:                                                     return "Keep";
        }
    };

    outputStream << "   [ADVISORY] Fragile edge assessment: action="
               << fragileActionToText(advice.action)
               << ", weight×=" << advice.weightMultiplier
               << " — " << advice.rationale << "\n";

    if (mApplyFragileAdvice)
    {
        if (advice.action == palvalidator::analysis::FragileEdgeAction::Drop)
        {
            outputStream << "   [ADVISORY] Apply=ON → dropping strategy per fragile-edge policy.\n\n";
            return false;
        }
        if (advice.action == palvalidator::analysis::FragileEdgeAction::Downweight)
        {
            outputStream << "   [ADVISORY] Apply=ON → (not implemented here) would downweight this strategy in meta.\n";
        }
    }

    return true;
}

std::pair<size_t, size_t> PerformanceFilter::countSurvivorsByDirection(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& filteredStrategies) const
{
    size_t survivorsLong = 0, survivorsShort = 0;
    for (const auto& s : filteredStrategies)
    {
        const auto& nm = s->getStrategyName();
        if (nm.find("Long") != std::string::npos)  ++survivorsLong;
        if (nm.find("Short") != std::string::npos) ++survivorsShort;
    }
    return {survivorsLong, survivorsShort};
}

} // namespace filtering
} // namespace palvalidator