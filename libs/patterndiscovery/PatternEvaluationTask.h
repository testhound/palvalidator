// PatternEvaluationTask.h
#ifndef PATTERN_EVALUATION_TASK_H
#define PATTERN_EVALUATION_TASK_H

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "SearchConfiguration.h"
#include "PatternTemplate.h"
#include "PricePatternFactory.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PerformanceCriteria.h"

using namespace mkc_timeseries;

class PatternEvaluationTaskException : public std::runtime_error
{
public:
    explicit PatternEvaluationTaskException(const std::string& msg)
        : std::runtime_error(msg) {}
};


/**
 * @brief Evaluates a single PatternTemplate for both long and short profitability.
 */
template <class DecimalType>
class PatternEvaluationTask
{
public:
    explicit PatternEvaluationTask(const SearchConfiguration<DecimalType>& config,
				   const PatternTemplate& patternTemplate,
				   PricePatternFactory<DecimalType>& patternFactory)
    : mConfig(config),
      mTemplate(patternTemplate),
      mPatternFactory(patternFactory),
      mSecurity(std::const_pointer_cast<Security<DecimalType>>(config.getSecurity()))
    {
        if (!mSecurity)
        {
            throw PatternEvaluationTaskException("Security object cannot be null.");
        }
    }

    /**
     * @brief Evaluates the template for both long and short scenarios.
     * @return A vector of profitable PriceActionLabPattern objects (can be 0, 1, or 2).
     */
    std::vector<PALPatternPtr> evaluateAndBacktest()
    {
        std::vector<PALPatternPtr> profitablePatterns;

        auto patternExpression = mPatternFactory.createPatternExpressionFromTemplate(mTemplate);
        if (!patternExpression)
        {
            return profitablePatterns;
        }

 DateRange backTestDates(mConfig.getBacktestStartTime(), mConfig.getBacktestEndTime());
 
        // --- Test 1: Long Direction ---
        auto longPattern = mPatternFactory.createLongPalPattern(patternExpression, mConfig, mTemplate.getName());
        auto longStrategy = makePalStrategy<DecimalType>(longPattern->getFileName(),
        longPattern,
        mSecurity);

        auto longBacktester = BackTesterFactory<DecimalType>::backTestStrategy(longStrategy,
        	       mConfig.getTimeFrameDuration(),
        	       backTestDates);

        if (meetsPerformanceCriteria(*longBacktester))
        {
            profitablePatterns.push_back(mPatternFactory.createFinalPattern(longPattern, *longBacktester));
        }

        // --- Test 2: Short Direction ---
        auto shortPattern = mPatternFactory.createShortPalPattern(patternExpression, mConfig, mTemplate.getName());
        auto shortStrategy = makePalStrategy<DecimalType>(shortPattern->getFileName(),
         shortPattern,
         mSecurity);

        auto shortBacktester = BackTesterFactory<DecimalType>::backTestStrategy(shortStrategy,
        		mConfig.getTimeFrameDuration(),
        		backTestDates);

        if (meetsPerformanceCriteria(*shortBacktester))
        {
            profitablePatterns.push_back(mPatternFactory.createFinalPattern(shortPattern, *shortBacktester));
        }

        return profitablePatterns;
    }

private:

    bool meetsPerformanceCriteria(const mkc_timeseries::BackTester<DecimalType>& b) const
    {
        const auto& c = mConfig.getPerformanceCriteria();
	
        if (b.getNumTrades() < c.getMinTrades())
	  return false;

        if (std::get<1>(b.getProfitability()) < c.getMinProfitability())
	  return false;

        if (std::get<0>(b.getProfitability()) < c.getMinProfitFactor())
	  return false;

        if (b.getNumConsecutiveLosses() > c.getMaxConsecutiveLosers())
	  return false;

        return true;
    }


private:
    const SearchConfiguration<DecimalType>& mConfig;
    const PatternTemplate& mTemplate;
    PricePatternFactory<DecimalType>& mPatternFactory;
    std::shared_ptr<Security<DecimalType>> mSecurity;
};

#endif // PATTERN_EVALUATION_TASK_H
