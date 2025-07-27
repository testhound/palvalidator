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
#include "AstResourceManager.h"
#include "PalStrategy.h"
#include "PALPatternInterpreter.h"
#include "BackTester.h"
#include "Portfolio.h"
#include "number.h"
#include "DateRange.h"

// Forward declaration for the helper class
template <class DecimalType>
class ExactPatternExpressionGenerator;

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
 * @brief Encapsulates the logic for identifying patterns, constructing ASTs,
 * running backtests, and filtering results for a specific data window.
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
     * @brief Main execution method that dispatches to the correct pattern generation logic.
     */
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> findPatterns()
    {
        // For now, this only calls findExactPatterns. In the future, it will also
        // orchestrate finding Split, Overlay, and Delay patterns.
        return findExactPatterns();
    }

private:
    /**
     * @brief Generates, backtests, and filters multi-bar exact patterns based on SearchType.
     */
    std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> findExactPatterns()
    {
        std::vector<std::pair<PALPatternPtr, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>>>> profitablePatterns;

        auto lengthRange = mConfig.getPatternLengthRange();
        unsigned int minLength = lengthRange.first;
        unsigned int maxLength = lengthRange.second;

        for (unsigned int l = minLength; l <= maxLength; ++l)
        {
            std::shared_ptr<PatternExpression> patternExpression = generateExactPatternExpressionForWindow(l);
            if (!patternExpression)
            {
                continue; // Not enough data for this length
            }

            PALPatternPtr candidatePattern = createPalPattern(patternExpression, l, 0);
            auto backtester = runBacktest(candidatePattern);

            bool meetsCriteria = meetsPerformanceCriteria(backtester);
            std::cout << "DEBUG: Pattern length " << l << " meets performance criteria: "
                      << (meetsCriteria ? "YES" : "NO") << std::endl;
            if (meetsCriteria)
            {
                PALPatternPtr finalPattern = createFinalPattern(candidatePattern, backtester);
                profitablePatterns.push_back({finalPattern, backtester});
            }
        }

        return profitablePatterns;
    }
    
    /**
     * @brief Generates a single pattern expression representing the relationships in the current window.
     */
    std::shared_ptr<PatternExpression> generateExactPatternExpressionForWindow(unsigned int length)
    {
        ExactPatternExpressionGenerator<DecimalType> generator(
            mSecurity,
            mWindowEndTime,
            length,
            mConfig.getSearchType(),
            mAstResourceManager);
        auto result = generator.generate();
        std::cout << "DEBUG: Pattern generation for length " << length << ": "
                  << (result ? "SUCCESS" : "FAILED (nullptr)") << std::endl;
        return result;
    }

    PALPatternPtr createPalPattern(std::shared_ptr<PatternExpression> patternExpression, unsigned int length, unsigned int delay)
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

    std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> runBacktest(PALPatternPtr pattern)
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

    PALPatternPtr createFinalPattern(PALPatternPtr initialPattern, std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> backtester)
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

    bool meetsPerformanceCriteria(std::shared_ptr<mkc_timeseries::BackTester<DecimalType>> backtester) const
    {
        unsigned int totalClosedTrades = backtester->getClosedPositionHistory().getNumPositions();
        if (totalClosedTrades < mPerformanceCriteria.getMinTrades()) return false;

        auto profitabilityMetrics = backtester->getProfitability();
        if (std::get<1>(profitabilityMetrics) < mPerformanceCriteria.getMinProfitability()) return false;

        if (backtester->getNumConsecutiveLosses() > mPerformanceCriteria.getMaxConsecutiveLosers()) return false;

        if (std::get<0>(profitabilityMetrics) < mPerformanceCriteria.getMinProfitFactor()) return false;

        return true;
    }

private:
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

        std::sort(components.begin(), components.end(), [](const PriceComponent& a, const PriceComponent& b) {
            return a.value < b.value;
        });

        std::shared_ptr<PatternExpression> fullExpression = nullptr;
        for (size_t i = 0; i < components.size() - 1; ++i)
        {
            auto rhs = createPriceBarReference(components[i].name, components[i].offset);
            auto lhs = createPriceBarReference(components[i+1].name, components[i+1].offset);
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

template class PatternDiscoveryTask<num::DefaultNumber>;

#endif // PATTERN_DISCOVERY_TASK_H
