#ifndef COMPARISONTOPALSTRATEGY_H
#define COMPARISONTOPALSTRATEGY_H

#include <unordered_set>
#include "PalAst.h"
#include "PalStrategy.h"
#include "ComparisonsGenerator.h"
#include "PalStrategyAlwaysOn.h"
#include <type_traits>

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  class PriceBarFactory {
  public:
    PriceBarFactory():
      mPriceBars()
    {
      mPriceBars.reserve(15*2);     //preallocate for speed
    }

  PriceBarReference* getPriceBar(const unsigned int offset, const unsigned int ref)
  {
    switch (static_cast<PriceBarReference::ReferenceType>(ref))
      {
      case PriceBarReference::ReferenceType::OPEN:
        mPriceBars.push_back(std::make_unique<PriceBarOpen>(offset));
        break;
      case PriceBarReference::ReferenceType::HIGH:
        mPriceBars.push_back(std::make_unique<PriceBarHigh>(offset));
        break;
      case PriceBarReference::ReferenceType::LOW:
        mPriceBars.push_back(std::make_unique<PriceBarLow>(offset));
        break;
      case PriceBarReference::ReferenceType::CLOSE:
        mPriceBars.push_back(std::make_unique<PriceBarClose>(offset));
        break;
      }
      return mPriceBars.back().get();
  }
    private:
    std::vector<std::unique_ptr<PriceBarReference>> mPriceBars;
  };



  ///
  /// A straightforward conversion from comparison to Pal-Expression based Strategy
  ///
  template <class Decimal, bool isLong, bool alwaysOn>
  class ComparisonToPalStrategy
  {
  public:
    //helpers
    using LongPalStrategyType = std::conditional_t<alwaysOn, PalLongStrategyAlwaysOn<Decimal>, PalLongStrategy<Decimal>>;
    using ShortPalStrategyType = std::conditional_t<alwaysOn, PalShortStrategyAlwaysOn<Decimal>, PalShortStrategy<Decimal>>;
    //sided typedefs
    using SidedPalStrategyType = std::conditional_t<isLong, LongPalStrategyType, ShortPalStrategyType>;
    using SidedProfitTargetType = std::conditional_t<isLong, LongSideProfitTargetInPercent, ShortSideProfitTargetInPercent>;
    using SidedStopLossType = std::conditional_t<isLong, LongSideStopLossInPercent, ShortSideStopLossInPercent>;
    using SidedMarketEntryType = std::conditional_t<isLong, LongMarketEntryOnOpen, ShortMarketEntryOnOpen>;

    ComparisonToPalStrategy(const std::vector<ComparisonEntryType>& compareBatch,
                            const unsigned patternIndex, const unsigned long indexDate,
                            decimal7* const profitTarget, decimal7* const stopLoss, const std::shared_ptr<Portfolio<Decimal>>& portfolio):
      mComparisonCount(0),
      mExpectedNumberOfPatterns(compareBatch.size()),
      mPatternDescription(allocatePatternDescription(patternIndex, indexDate)),
      mProfitTarget(allocateProfitTarget(profitTarget)),
      mStopLoss(allocateStopLoss(stopLoss)),
      mMarketEntry(allocateMarketEntry()),
      mPalGreaterThanPatternExpressions(),
      mPalAndPatternExpressions(),
      mPriceBarFactory()
    {
      mPalGreaterThanPatternExpressions.reserve(15);     //reserve 15
      mPalAndPatternExpressions.reserve(15);

      for(const auto& comparison: compareBatch)
        addComparison(comparison);

      if (!isComplete())
          throw;

      mPalPattern = std::make_shared<PriceActionLabPattern>(mPatternDescription, getPatternExpression(), mMarketEntry.get(), mProfitTarget.get(), mStopLoss.get());

      std::string strategyName= std::string("PAL Search Algo Based Strategy ") + std::to_string(patternIndex);
      mPalStrategy = std::make_shared<SidedPalStrategyType>(strategyName, mPalPattern, portfolio);

    }

    ComparisonToPalStrategy(const ComparisonToPalStrategy<Decimal, isLong, alwaysOn>&) = delete;

    ComparisonToPalStrategy<Decimal, isLong, alwaysOn>& operator=(const ComparisonToPalStrategy<Decimal, isLong, alwaysOn>&) = delete;


    ~ComparisonToPalStrategy()
    {
      //the structure of intertwound shared pointers for strategies ...
    }

    const std::shared_ptr<SidedPalStrategyType>& getPalStrategy() const { return mPalStrategy; }

    const std::shared_ptr<PriceActionLabPattern>& getPalPattern() const { return mPalPattern; }


  private:

    PatternExpression* getPatternExpression() const
    {
      if (mPalAndPatternExpressions.size() == 0 && mPalGreaterThanPatternExpressions.size() == 1)
        {
          return mPalGreaterThanPatternExpressions.back();
        }
      else
        {
          return mPalAndPatternExpressions.back();
        }
    }

    void addComparison(const ComparisonEntryType& comparison)
    {

      GreaterThanExpr* newExprPtr = new GreaterThanExpr(mPriceBarFactory.getPriceBar(comparison[0], comparison[1]), mPriceBarFactory.getPriceBar(comparison[2], comparison[3]));
      mPalGreaterThanPatternExpressions.push_back(newExprPtr);
      if (mComparisonCount > 0)
        {
          if (mPalAndPatternExpressions.size() == 0)
            mPalAndPatternExpressions.push_back(new AndExpr(mPalGreaterThanPatternExpressions.front(), mPalGreaterThanPatternExpressions.back()));
          else
            mPalAndPatternExpressions.push_back(new AndExpr(mPalAndPatternExpressions.back(), mPalGreaterThanPatternExpressions.back()));
        }
      mComparisonCount++;
    }

    ProfitTargetInPercentExpression* allocateProfitTarget(decimal7* profitTarget)
    {
      return new SidedProfitTargetType(profitTarget);
    }

    StopLossInPercentExpression* allocateStopLoss(decimal7* stopLoss)
    {
      return new SidedStopLossType(stopLoss);
    }

    MarketEntryExpression* allocateMarketEntry()
    {
      return new SidedMarketEntryType();
    }

    PatternDescription* allocatePatternDescription(unsigned int patternIndex, unsigned int indexDate)
    {
      return new PatternDescription("NonExistentFile.txt", patternIndex, indexDate, &DecimalConstants<Decimal>::DecimalZero, &DecimalConstants<Decimal>::DecimalZero, 0, 0);
    }

    bool isComplete() const { return ((mExpectedNumberOfPatterns == mComparisonCount) && mProfitTarget && mStopLoss && mMarketEntry && mPatternDescription); }

    unsigned mComparisonCount;
    unsigned mExpectedNumberOfPatterns;
    PatternDescription* mPatternDescription;
    std::unique_ptr<ProfitTargetInPercentExpression> mProfitTarget;
    std::unique_ptr<StopLossInPercentExpression> mStopLoss;
    std::unique_ptr<MarketEntryExpression> mMarketEntry;
    std::vector<GreaterThanExpr*> mPalGreaterThanPatternExpressions;
    std::vector<AndExpr*> mPalAndPatternExpressions;
    std::shared_ptr<PriceActionLabPattern> mPalPattern;
    std::shared_ptr<SidedPalStrategyType> mPalStrategy;
    PriceBarFactory mPriceBarFactory;

  };

  //"typedefs" with more explicit verbosity about the type
  template <class Decimal>
  using ComparisonToPalLongStrategyAlwaysOn = ComparisonToPalStrategy<Decimal, true, true>;

  template <class Decimal>
  using ComparisonToPalShortStrategyAlwaysOn = ComparisonToPalStrategy<Decimal, false, true>;

  template <class Decimal>
  using ComparisonToPalLongStrategy = ComparisonToPalStrategy<Decimal, true, false>;

  template <class Decimal>
  using ComparisonToPalShortStrategy = ComparisonToPalStrategy<Decimal, false, false>;


}

#endif // COMPARISONTOPALSTRATEGY_H
