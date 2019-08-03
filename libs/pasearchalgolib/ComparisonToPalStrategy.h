#ifndef COMPARISONTOPALSTRATEGY_H
#define COMPARISONTOPALSTRATEGY_H

#include <unordered_set>
#include "PalAst.h"
#include "PalStrategy.h"
#include "ComparisonsGenerator.h"
#include "PalStrategyAlwaysOn.h"

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
  template <class Decimal, typename TPalStrategy, typename TProfitTarget, typename TStopLoss, typename TMarketEntry>
  class ComparisonToPalStrategy
  {
  public:
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
      mPalStrategy = std::make_shared<TPalStrategy>(strategyName, mPalPattern, portfolio);

    }

    ComparisonToPalStrategy(const ComparisonToPalStrategy<Decimal, TPalStrategy, TProfitTarget, TStopLoss, TMarketEntry>&) = delete;

    ComparisonToPalStrategy<Decimal, TPalStrategy, TProfitTarget, TStopLoss, TMarketEntry>& operator=(const ComparisonToPalStrategy<Decimal, TPalStrategy, TProfitTarget, TStopLoss, TMarketEntry>&) = delete;


    ~ComparisonToPalStrategy()
    {
      //the structure of intertwound shared pointers for strategies ...
    }

    const std::shared_ptr<TPalStrategy>& getPalStrategy() const { return mPalStrategy; }


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
      return new TProfitTarget(profitTarget);
    }

    StopLossInPercentExpression* allocateStopLoss(decimal7* stopLoss)
    {
      return new TStopLoss(stopLoss);
    }

    MarketEntryExpression* allocateMarketEntry()
    {
      return new TMarketEntry();
    }

    PatternDescription* allocatePatternDescription(unsigned int patternIndex, unsigned int indexDate)
    {
      return new PatternDescription("", patternIndex, indexDate, nullptr, nullptr, 0, 0);
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
    std::shared_ptr<TPalStrategy> mPalStrategy;
    PriceBarFactory mPriceBarFactory;

  };

  ///
  /// Use these "typedefs" for correct types
  ///
  template <class Decimal>
  using ComparisonToPalLongStrategy = ComparisonToPalStrategy<Decimal, PalLongStrategy<Decimal>, LongSideProfitTargetInPercent, LongSideStopLossInPercent, LongMarketEntryOnOpen>;

  template <class Decimal>
  using ComparisonToPalShortStrategy = ComparisonToPalStrategy<Decimal, PalShortStrategy<Decimal>, ShortSideProfitTargetInPercent, ShortSideStopLossInPercent, ShortMarketEntryOnOpen>;

  template <class Decimal>
  using ComparisonToPalLongStrategyAlwaysOn = ComparisonToPalStrategy<Decimal, PalLongStrategyAlwaysOn<Decimal>, LongSideProfitTargetInPercent, LongSideStopLossInPercent, LongMarketEntryOnOpen>;

  template <class Decimal>
  using ComparisonToPalShortStrategyAlwaysOn = ComparisonToPalStrategy<Decimal, PalShortStrategyAlwaysOn<Decimal>, ShortSideProfitTargetInPercent, ShortSideStopLossInPercent, ShortMarketEntryOnOpen>;


}

#endif // COMPARISONTOPALSTRATEGY_H
