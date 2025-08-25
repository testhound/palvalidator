#include "MetaStrategyAnalyzer.h"
#include <algorithm>
#include <limits>
#include <numeric>
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

MetaStrategyAnalyzer::MetaStrategyAnalyzer(const RiskParameters& riskParams, const Num& confidenceLevel, unsigned int numResamples)
    : mHurdleCalculator(riskParams),
      mConfidenceLevel(confidenceLevel),
      mNumResamples(numResamples),
      mMetaStrategyPassed(false)
{
    // Note: mAnnualizedLowerBound and mRequiredReturn are not initialized here
    // They will be set when analyzeMetaStrategy() is called
}

void MetaStrategyAnalyzer::analyzeMetaStrategy(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration timeFrame,
    std::ostream& outputStream)
{
    if (survivingStrategies.empty())
    {
        outputStream << "\n[Meta] No surviving strategies to aggregate.\n";
        mMetaStrategyPassed = false;
        return;
    }

    outputStream << "\n[Meta] Building equal-weight portfolio from "
                << survivingStrategies.size() << " survivors...\n";

    // Gather per-strategy data
    auto [survivorReturns, survivorAnnualizedTrades, survivorMedianHolds, T] = 
        gatherStrategyData(survivingStrategies, baseSecurity, backtestingDates, timeFrame, outputStream);

    if (survivorReturns.empty() || T < 2)
    {
        outputStream << "[Meta] Not enough aligned data to form portfolio.\n";
        mMetaStrategyPassed = false;
        return;
    }

    // Create equal-weight portfolio
    std::vector<Num> metaReturns = createEqualWeightPortfolio(survivorReturns, T);

    // Per-period point estimates (pre-annualization)
    {
        const Num am = StatUtils<Num>::computeMean(metaReturns);
        const Num gm = GeoMeanStat<Num>{}(metaReturns);
        outputStream << "      Per-period point estimates (pre-annualization): "
                    << "Arithmetic mean =" << (am * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                    << "Geometric mean =" << (gm * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
    }

    // Annualization factor
    double annualizationFactor;
    if (timeFrame == TimeFrame::INTRADAY)
    {
        auto minutes = baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes();
        annualizationFactor = calculateAnnualizationFactor(timeFrame, minutes);
    }
    else
    {
        annualizationFactor = calculateAnnualizationFactor(timeFrame);
    }

    // Block length for meta bootstrap
    const size_t Lmeta = calculateMetaBlockLength(survivorMedianHolds);
    StationaryBlockResampler<Num> metaSampler(Lmeta);
    using BlockBCA = BCaBootStrap<Num, StationaryBlockResampler<Num>>;

    // Bootstrap portfolio series
    GeoMeanStat<Num> statGeo;
    BlockBCA metaGeo(metaReturns, mNumResamples, mConfidenceLevel.getAsDouble(), statGeo, metaSampler);
    BlockBCA metaMean(metaReturns, mNumResamples, mConfidenceLevel.getAsDouble(),
                     &mkc_timeseries::StatUtils<Num>::computeMean, metaSampler);

    const Num lbGeoPeriod = metaGeo.getLowerBound();
    const Num lbMeanPeriod = metaMean.getLowerBound();

    outputStream << "      Per-period BCa lower bounds (pre-annualization): "
                << "Geo=" << (lbGeoPeriod * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                << "Mean=" << (lbMeanPeriod * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
    outputStream << "      (Meta uses block resampling with L=" << Lmeta << ")\n";

    // Annualize portfolio BCa results
    BCaAnnualizer<Num> metaGeoAnn(metaGeo, annualizationFactor);
    BCaAnnualizer<Num> metaMeanAnn(metaMean, annualizationFactor);

    const Num lbGeoAnn = metaGeoAnn.getAnnualizedLowerBound();
    const Num lbMeanAnn = metaMeanAnn.getAnnualizedLowerBound();

    // Portfolio-level cost hurdle
    const Num portfolioAnnualizedTrades = calculatePortfolioAnnualizedTrades(survivorAnnualizedTrades, survivorReturns.size());
    const Num finalRequiredReturn = mHurdleCalculator.calculateFinalRequiredReturn(portfolioAnnualizedTrades);

    // Store results
    mAnnualizedLowerBound = lbGeoAnn;
    mRequiredReturn = finalRequiredReturn;
    mMetaStrategyPassed = (lbGeoAnn > finalRequiredReturn);

    outputStream << "\n[Meta] Portfolio of " << survivorReturns.size() << " survivors (equal-weight):\n"
                << "      Annualized Lower Bound (GeoMean): " << (lbGeoAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                << "      Annualized Lower Bound (Mean):    " << (lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
                << "      Required Return (max(cost,riskfree)): "
                << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

    if (mMetaStrategyPassed)
    {
        outputStream << "      RESULT: ✓ Metastrategy PASSES\n";
    }
    else
    {
        outputStream << "      RESULT: ✗ Metastrategy FAILS\n";
    }

    outputStream << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
}

std::tuple<std::vector<std::vector<Num>>, std::vector<Num>, std::vector<unsigned int>, size_t>
MetaStrategyAnalyzer::gatherStrategyData(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration timeFrame,
    std::ostream& outputStream)
{
    std::vector<std::vector<Num>> survivorReturns;
    survivorReturns.reserve(strategies.size());
    std::vector<Num> survivorAnnualizedTrades;
    survivorAnnualizedTrades.reserve(strategies.size());
    std::vector<unsigned int> survivorMedianHolds;
    survivorMedianHolds.reserve(strategies.size());

    size_t T = std::numeric_limits<size_t>::max();

    for (const auto& strat : strategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strat->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto cloned = strat->clone2(freshPortfolio);

            auto bt = BackTesterFactory<Num>::backTestStrategy(cloned, timeFrame, backtestingDates);
            auto r = bt->getAllHighResReturns(cloned.get());

            if (r.size() < 2)
            {
                outputStream << "  [Meta] Skipping " << strat->getStrategyName()
                           << " (insufficient returns: " << r.size() << ")\n";
                continue;
            }

            const unsigned int medHold = bt->getClosedPositionHistory().getMedianHoldingPeriod();
            survivorMedianHolds.push_back(medHold);

            T = std::min(T, r.size());
            survivorReturns.push_back(std::move(r));
            survivorAnnualizedTrades.push_back(Num(bt->getEstimatedAnnualizedTrades()));
        }
        catch (const std::exception& e)
        {
            outputStream << "  [Meta] Skipping " << strat->getStrategyName()
                       << " due to error: " << e.what() << "\n";
        }
    }

    return {survivorReturns, survivorAnnualizedTrades, survivorMedianHolds, T};
}

std::vector<Num> MetaStrategyAnalyzer::createEqualWeightPortfolio(
    const std::vector<std::vector<Num>>& survivorReturns,
    size_t minLength)
{
    const size_t n = survivorReturns.size();
    const Num w = Num(1) / Num(static_cast<int>(n));

    std::vector<Num> metaReturns(minLength, DecimalConstants<Num>::DecimalZero);
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t t = 0; t < minLength; ++t)
        {
            metaReturns[t] += w * survivorReturns[i][t];
        }
    }

    return metaReturns;
}

size_t MetaStrategyAnalyzer::calculateMetaBlockLength(const std::vector<unsigned int>& survivorMedianHolds)
{
    auto computeMedianUH = [](std::vector<unsigned int> v) -> size_t
    {
        if (v.empty()) return 2;
        const size_t m = v.size();
        const size_t mid = m / 2;
        std::nth_element(v.begin(), v.begin() + mid, v.end());
        if (m & 1U)
        {
            return std::max<size_t>(2, v[mid]);
        }
        else
        {
            auto hi = v[mid];
            std::nth_element(v.begin(), v.begin() + (mid - 1), v.begin() + mid);
            auto lo = v[mid - 1];
            return std::max<size_t>(2, (static_cast<size_t>(lo) + static_cast<size_t>(hi) + 1ULL) / 2ULL);
        }
    };

    return computeMedianUH(survivorMedianHolds);
}

Num MetaStrategyAnalyzer::calculatePortfolioAnnualizedTrades(
    const std::vector<Num>& survivorAnnualizedTrades,
    size_t numStrategies)
{
    const Num w = Num(1) / Num(static_cast<int>(numStrategies));

    Num sumTrades = DecimalConstants<Num>::DecimalZero;
    for (const auto& tr : survivorAnnualizedTrades)
        sumTrades += tr;

    return w * sumTrades;
}

} // namespace filtering
} // namespace palvalidator