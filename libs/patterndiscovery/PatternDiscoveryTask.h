// PatternDiscoveryTask.h
#ifndef PATTERN_DISCOVERY_TASK_H
#define PATTERN_DISCOVERY_TASK_H

#include <memory>
#include <vector>
#include <stdexcept>
#include <string>
#include <utility>
#include <algorithm>
#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "SearchConfiguration.h"
#include "PerformanceCriteria.h"
#include "Security.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include "AstResourceManager.h"
#include "PalStrategy.h"
#include "PALPatternInterpreter.h"
#include "BackTester.h"
#include "Portfolio.h"
#include "number.h"
#include "DateRange.h"

// Forward declaration for helper classes
template <class DecimalType>
class ExactPatternExpressionGenerator;
class AstOffsetShifter;

/**
 * @brief Exception class for errors related to PatternDiscoveryTask.
 */
class PatternDiscoveryTaskException : public std::runtime_error
{
public:
    explicit PatternDiscoveryTaskException(const std::string& msg)
        : std::runtime_error(msg) {}

    ~PatternDiscoveryTaskException() noexcept override = default;
};

/**
 * @brief Encapsulates the logic for discovering patterns for a specific data window.
 */
template <class DecimalType>
class PatternDiscoveryTask
{
public:
    explicit PatternDiscoveryTask(
        const SearchConfiguration<DecimalType>& config,
        boost::posix_time::ptime windowEndTime,
        mkc_palast::AstResourceManager& astResourceManager)
    : mConfig(config),
      mWindowEndTime(windowEndTime),
      mAstResourceManager(astResourceManager),
      mSecurity(std::const_pointer_cast<mkc_timeseries::Security<DecimalType>>(config.getSecurity())),
      mTimeFrameDuration(config.getTimeFrameDuration()),
      mProfitTargetVal(config.getProfitTarget()),
      mStopLossVal(config.getStopLoss()),
      mPerformanceCriteria(config.getPerformanceCriteria()),
      mBacktestStartTime(config.getBacktestStartTime()),
      mBacktestEndTime(config.getBacktestEndTime()),
      mTaskLocalPatternCounter(0)
    {
         if (!mSecurity)
        {
            throw PatternDiscoveryTaskException("PatternDiscoveryTask: Security object cannot be null.");
        }
    }

    ~PatternDiscoveryTask() noexcept = default;

    /**
     * @brief Main execution method for finding all profitable patterns in the window.
     */
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> findPatterns()
    {
        auto profitableExactPatterns = findExactPatterns();

        if (mConfig.isSearchingForDelayPatterns())
        {
            auto profitableDelayedPatterns = findDelayedPatterns(profitableExactPatterns);
            profitableExactPatterns.insert(profitableExactPatterns.end(),
                                           profitableDelayedPatterns.begin(),
                                           profitableDelayedPatterns.end());
        }

        return profitableExactPatterns;
    }

private:
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> findExactPatterns();
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> findDelayedPatterns(
        const std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>>& basePatterns);

    std::shared_ptr<PatternExpression> generateExactPatternExpressionForWindow(unsigned int length);
    PALPatternPtr createPalPattern(std::shared_ptr<PatternExpression> patternExpression, unsigned int length, unsigned int delay);
    std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> runBacktest(PALPatternPtr pattern);
    PALPatternPtr createFinalPattern(PALPatternPtr initialPattern, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> backtester);
    bool meetsPerformanceCriteria(std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> backtester) const;
    std::shared_ptr<PatternExpression> createDelayedExpression(PatternExpression* originalExpr, unsigned int delay);

    const SearchConfiguration<DecimalType>& mConfig;
    boost::posix_time::ptime mWindowEndTime;
    mkc_palast::AstResourceManager& mAstResourceManager;
    std::shared_ptr<mkc_timeseries::Security<DecimalType>> mSecurity;
    mkc_timeseries::TimeFrame::Duration mTimeFrameDuration;
    DecimalType mProfitTargetVal;
    DecimalType mStopLossVal;
    const PerformanceCriteria<DecimalType>& mPerformanceCriteria;
    boost::posix_time::ptime mBacktestStartTime;
    boost::posix_time::ptime mBacktestEndTime;
    unsigned int mTaskLocalPatternCounter;
};


/**
 * @brief Helper class to generate the combinatorial exact pattern expression for a window.
 */
template<class DecimalType>
class ExactPatternExpressionGenerator
{
public:
    ExactPatternExpressionGenerator(std::shared_ptr<mkc_timeseries::Security<DecimalType>> security,
                                    boost::posix_time::ptime windowEndTime,
                                    unsigned int length,
                                    SearchType searchType,
                                    mkc_palast::AstResourceManager& astResourceManager)
        : mSecurity(security),
          mWindowEndTime(windowEndTime),
          mLength(length),
          mSearchType(searchType),
          mAstResourceManager(astResourceManager) {}

    std::shared_ptr<PatternExpression> generate()
    {
        std::vector<PriceComponent> components;
        for (unsigned int i = 0; i < mLength; ++i)
        {
            try
            {
                auto entry = mSecurity->getTimeSeries()->getTimeSeriesEntry(mWindowEndTime, static_cast<long>(i));
                addComponentsForBar(entry, i, components);
            }
            catch (const mkc_timeseries::TimeSeriesException&)
            {
                return nullptr; // Not enough data for this length
            }
        }

        if (components.size() < 2) return nullptr;

        // Sort components by their actual value in descending order
        std::sort(components.begin(), components.end(), [](const PriceComponent& a, const PriceComponent& b) {
            return a.value > b.value;
        });

        // Chain the sorted components into a single AndExpr
        std::shared_ptr<PatternExpression> fullExpression = nullptr;
        for (size_t i = 0; i < components.size() - 1; ++i)
        {
            auto lhs = createPriceBarReference(components[i].name, components[i].offset);
            auto rhs = createPriceBarReference(components[i+1].name, components[i+1].offset);
            auto newExpr = std::make_shared<GreaterThanExpr>(lhs, rhs);

            if (fullExpression == nullptr)
                fullExpression = newExpr;
            else
                fullExpression = std::make_shared<AndExpr>(fullExpression, newExpr);
        }
        
        return fullExpression;
    }

private:
    struct PriceComponent {
        DecimalType value;
        std::string name;
        unsigned int offset;
    };

    void addComponentsForBar(const mkc_timeseries::OHLCTimeSeriesEntry<DecimalType>& entry,
                             unsigned int offset,
                             std::vector<PriceComponent>& components)
    {
        bool useOpen = false, useHigh = false, useLow = false, useClose = false;

        switch (mSearchType)
        {
            case SearchType::EXTENDED:
            case SearchType::DEEP:
            case SearchType::MIXED:
                useOpen = useHigh = useLow = useClose = true;
                break;
            case SearchType::CLOSE_ONLY:
                useClose = true;
                break;
            case SearchType::HIGH_LOW_ONLY:
                useHigh = useLow = true;
                break;
            case SearchType::OPEN_CLOSE_ONLY:
                useOpen = useClose = true;
                break;
        }

        if (useOpen) components.push_back({entry.getOpenValue(), "O", offset});
        if (useHigh) components.push_back({entry.getHighValue(), "H", offset});
        if (useLow) components.push_back({entry.getLowValue(), "L", offset});
        if (useClose) components.push_back({entry.getCloseValue(), "C", offset});
    }

    std::shared_ptr<PriceBarReference> createPriceBarReference(const std::string& name, unsigned int offset)
    {
        if (name == "O") return mAstResourceManager.getPriceOpen(offset);
        if (name == "H") return mAstResourceManager.getPriceHigh(offset);
        if (name == "L") return mAstResourceManager.getPriceLow(offset);
        if (name == "C") return mAstResourceManager.getPriceClose(offset);
        throw std::runtime_error("Unknown price component name: " + name);
    }

    std::shared_ptr<mkc_timeseries::Security<DecimalType>> mSecurity;
    boost::posix_time::ptime mWindowEndTime;
    unsigned int mLength;
    SearchType mSearchType;
    mkc_palast::AstResourceManager& mAstResourceManager;
};


// Method Implementations

template <class DecimalType>
std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>>
PatternDiscoveryTask<DecimalType>::findExactPatterns()
{
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> profitablePatterns;
    auto lengthRange = mConfig.getPatternLengthRange();

    for (unsigned int l = lengthRange.first; l <= lengthRange.second; ++l)
    {
        auto patternExpression = generateExactPatternExpressionForWindow(l);
        if (!patternExpression) continue;

        auto candidatePattern = createPalPattern(patternExpression, l, 0);
        auto backtester = runBacktest(candidatePattern);

        if (meetsPerformanceCriteria(backtester))
        {
            auto finalPattern = createFinalPattern(candidatePattern, backtester);
            profitablePatterns.push_back({finalPattern, backtester});
        }
    }
    return profitablePatterns;
}

template <class DecimalType>
std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>>
PatternDiscoveryTask<DecimalType>::findDelayedPatterns(
    const std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>>& basePatterns)
{
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> profitableDelayedPatterns;

    for (const auto& basePatternPair : basePatterns)
    {
        auto basePattern = basePatternPair.first;
        unsigned int baseLength = basePattern->getMaxBarsBack() + 1;

        for (unsigned int d = mConfig.getMinDelayBars(); d <= mConfig.getMaxDelayBars(); ++d)
        {
            auto delayedExpression = createDelayedExpression(basePattern->getPatternExpression().get(), d);
            if (!delayedExpression) continue;
            
            auto candidateDelayedPattern = createPalPattern(delayedExpression, baseLength, d);
            auto backtester = runBacktest(candidateDelayedPattern);

            if (meetsPerformanceCriteria(backtester))
            {
                auto finalPattern = createFinalPattern(candidateDelayedPattern, backtester);
                profitableDelayedPatterns.push_back({finalPattern, backtester});
            }
        }
    }

    return profitableDelayedPatterns;
}

template<class DecimalType>
std::shared_ptr<PatternExpression>
PatternDiscoveryTask<DecimalType>::createDelayedExpression(PatternExpression* originalExpr, unsigned int delay)
{
    class AstOffsetShifter : public PalCodeGenVisitor {
    public:
        AstOffsetShifter(unsigned int delay, mkc_palast::AstResourceManager& rm) : mDelay(delay), mResourceManager(rm) {}

        void generateCode() override {} // Not used for this visitor

        void visit(PriceBarOpen* open) override { mCurrentRef = mResourceManager.getPriceOpen(open->getBarOffset() + mDelay); }
        void visit(PriceBarHigh* high) override { mCurrentRef = mResourceManager.getPriceHigh(high->getBarOffset() + mDelay); }
        void visit(PriceBarLow* low) override { mCurrentRef = mResourceManager.getPriceLow(low->getBarOffset() + mDelay); }
        void visit(PriceBarClose* close) override { mCurrentRef = mResourceManager.getPriceClose(close->getBarOffset() + mDelay); }
        
        void visit(GreaterThanExpr* expr) override {
            expr->getLHS()->accept(*this);
            auto lhs = mCurrentRef;
            expr->getRHS()->accept(*this);
            auto rhs = mCurrentRef;
            mCurrentExpr = std::make_shared<GreaterThanExpr>(lhs, rhs);
        }
        
        void visit(AndExpr* expr) override {
            expr->getLHS()->accept(*this);
            auto lhs = mCurrentExpr;
            expr->getRHS()->accept(*this);
            auto rhs = mCurrentExpr;
            mCurrentExpr = std::make_shared<AndExpr>(lhs, rhs);
        }

        // Unused visitor methods
        void visit(VolumeBarReference*) override {}
        void visit(Roc1BarReference*) override {}
        void visit(IBS1BarReference*) override {}
        void visit(IBS2BarReference*) override {}
        void visit(IBS3BarReference*) override {}
        void visit(MeanderBarReference*) override {}
        void visit(VChartHighBarReference*) override {}
        void visit(VChartLowBarReference*) override {}
        void visit(LongMarketEntryOnOpen*) override {}
        void visit(ShortMarketEntryOnOpen*) override {}
        void visit(LongSideProfitTargetInPercent*) override {}
        void visit(ShortSideProfitTargetInPercent*) override {}
        void visit(LongSideStopLossInPercent*) override {}
        void visit(ShortSideStopLossInPercent*) override {}
        void visit(PatternDescription*) override {}
        void visit(PriceActionLabPattern*) override {}

        std::shared_ptr<PatternExpression> mCurrentExpr;
        std::shared_ptr<PriceBarReference> mCurrentRef;
    private:
        unsigned int mDelay;
        mkc_palast::AstResourceManager& mResourceManager;
    };

    AstOffsetShifter shifter(delay, mAstResourceManager);
    originalExpr->accept(shifter);
    return shifter.mCurrentExpr;
}


template <class DecimalType>
std::shared_ptr<PatternExpression>
PatternDiscoveryTask<DecimalType>::generateExactPatternExpressionForWindow(unsigned int length)
{
    ExactPatternExpressionGenerator<DecimalType> generator(
        mSecurity,
        mWindowEndTime,
        length,
        mConfig.getSearchType(),
        mAstResourceManager);
    return generator.generate();
}

template <class DecimalType>
PALPatternPtr
PatternDiscoveryTask<DecimalType>::createPalPattern(std::shared_ptr<PatternExpression> patternExpression, unsigned int length, unsigned int delay)
{
    unsigned int patternIndex = ++mTaskLocalPatternCounter;
    unsigned long indexDate = mWindowEndTime.date().day_number();
    std::string patternFileName = mSecurity->getSymbol() + "_L" + std::to_string(length) + "_D" + std::to_string(delay);

    auto patternDesc = std::make_shared<PatternDescription>(
        patternFileName.c_str(), patternIndex, indexDate,
        mAstResourceManager.getDecimalNumber(0), mAstResourceManager.getDecimalNumber(0), 0, 0);

    auto marketEntry = mAstResourceManager.getLongMarketEntryOnOpen();
    auto profitTargetExpr = mAstResourceManager.getLongProfitTarget(mAstResourceManager.getDecimalNumber(num::toString(mProfitTargetVal)));
    auto stopLossExpr = mAstResourceManager.getLongStopLoss(mAstResourceManager.getDecimalNumber(num::toString(mStopLossVal)));

    return mAstResourceManager.createPattern(patternDesc, patternExpression, marketEntry, profitTargetExpr, stopLossExpr);
}

template <class DecimalType>
std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>
PatternDiscoveryTask<DecimalType>::runBacktest(PALPatternPtr pattern)
{
    auto palStrategy = mkc_timeseries::makePalStrategy<DecimalType>(
        pattern->getPatternDescription()->getFileName(),
        pattern,
        mSecurity);

    return mkc_timeseries::BackTesterFactory<DecimalType>::backTestStrategy(
        palStrategy,
        mTimeFrameDuration,
        mkc_timeseries::DateRange(mBacktestStartTime, mBacktestEndTime));
}

template <class DecimalType>
PALPatternPtr
PatternDiscoveryTask<DecimalType>::createFinalPattern(PALPatternPtr initialPattern, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> backtester)
{
    unsigned int numTrades = backtester->getClosedPositionHistory().getNumPositions();
    DecimalType winRate = std::get<1>(backtester->getProfitability());
    unsigned int consecutiveLosses = backtester->getNumConsecutiveLosses();
    auto initialDesc = initialPattern->getPatternDescription();

    auto finalDesc = std::make_shared<PatternDescription>(
        initialDesc->getFileName().c_str(), initialDesc->getpatternIndex(), initialDesc->getIndexDate(),
        mAstResourceManager.getDecimalNumber(num::toString(winRate)), mAstResourceManager.getDecimalNumber(0),
        numTrades, consecutiveLosses);

    return mAstResourceManager.createPattern(
        finalDesc, initialPattern->getPatternExpression(), initialPattern->getMarketEntry(),
        initialPattern->getProfitTarget(), initialPattern->getStopLoss());
}

template <class DecimalType>
bool
PatternDiscoveryTask<DecimalType>::meetsPerformanceCriteria(std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> backtester) const
{
    unsigned int totalClosedTrades = backtester->getClosedPositionHistory().getNumPositions();
    if (totalClosedTrades < mPerformanceCriteria.getMinTrades()) return false;

    auto profitabilityMetrics = backtester->getProfitability();
    if (std::get<1>(profitabilityMetrics) < mPerformanceCriteria.getMinProfitability()) return false;

    if (backtester->getNumConsecutiveLosses() > mPerformanceCriteria.getMaxConsecutiveLosers()) return false;

    if (std::get<0>(profitabilityMetrics) < mPerformanceCriteria.getMinProfitFactor()) return false;

    return true;
}

template class PatternDiscoveryTask<num::DefaultNumber>;

#endif // PATTERN_DISCOVERY_TASK_H
