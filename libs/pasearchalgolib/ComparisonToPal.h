#ifndef COMPARISONTOPAL_H
#define COMPARISONTOPAL_H

#include <unordered_set>
#include "PalAst.h"
#include "ComparisonsGenerator.h"

//PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern,
//		       MarketEntryExpression* entry,
//		       ProfitTargetInPercentExpression* profitTarget,
//		       StopLossInPercentExpression* stopLoss);

namespace mkc_searchalgo
{
  ///
  /// A straightforward conversion
  ///
  class ComparisonToPal
  {
  public:
    ComparisonToPal(const std::unordered_set<ComparisonEntryType>& compareBatch, bool isLongPattern, const unsigned patternIndex, const unsigned long indexDate):
      mComparisonCount(0),
      mExpectedNumberOfPatterns(compareBatch.size()),
      mIsLongPattern(isLongPattern),
      mPatternIndex(patternIndex),
      mIndexDate(indexDate)
    {
      for(const auto& comparison: compareBatch)
        addComparison(comparison);
    }

  private:
    void addComparison(const ComparisonEntryType& comparison)
    {
      std::unique_ptr<PatternExpression> newExpr = std::make_unique<GreaterThanExpr>(priceBarFactory(comparison[0], comparison[1]), priceBarFactory(comparison[2], comparison[3]));
      if (mComparisonCount == 0)
          mPalPatternExpr = std::move(newExpr);
      else
          mPalPatternExpr = std::make_unique<AndExpr>(mPalPatternExpr, newExpr);

      mComparisonCount++;
    }

    void setProfitTarget(decimal7* profitTarget)
    {
      if (mIsLongPattern)
          mProfitTarget = new LongSideProfitTargetInPercent(profitTarget);
      else
          mProfitTarget = new ShortSideProfitTargetInPercent(profitTarget);

    }

    void setStopLoss(decimal7* stopLoss)
    {
      if (mIsLongPattern)
          mStopLoss = new LongSideStopLossInPercent(stopLoss);
      else
          mStopLoss = new ShortSideStopLossInPercent(stopLoss);
    }

    MarketEntryExpression* getMarketEntry()
    {
      if (mIsLongPattern)
        return std::make_unique<LongMarketEntryOnOpen>().get();
      else
        return std::make_unique<ShortMarketEntryOnOpen>().get();
    }

    PriceActionLabPattern getPattern()
    {
      if (!isComplete())
        throw;

      PatternDescription description("", mPatternIndex, mIndexDate, nullptr, nullptr, 0, 0);
      std::unique_ptr<PatternDescription> descPtr = std::make_unique<PatternDescription>(nullptr, mPatternIndex, mIndexDate, nullptr, nullptr, 0, 0);

      PriceActionLabPattern pattern(descPtr.get(), mPalPatternExpr.get(), getMarketEntry(), mProfitTarget, mStopLoss);
    }

    bool isComplete() const { return (mExpectedNumberOfPatterns == mComparisonCount); }

    //** like a factory method
    std::unique_ptr<PriceBarReference> priceBarFactory(const unsigned int offset, const unsigned int ref)
    {
      switch (static_cast<PriceBarReference::ReferenceType>(ref))
        {
        case PriceBarReference::ReferenceType::OPEN:
          return std::make_unique<PriceBarOpen>(offset);
        case PriceBarReference::ReferenceType::HIGH:
          return std::make_unique<PriceBarHigh>(offset);
        case PriceBarReference::ReferenceType::LOW:
          return std::make_unique<PriceBarLow>(offset);
        case PriceBarReference::ReferenceType::CLOSE:
          return std::make_unique<PriceBarClose>(offset);
        }
    }

    std::unique_ptr<PatternExpression> mPalPatternExpr;
    unsigned mComparisonCount;
    unsigned mExpectedNumberOfPatterns;
    bool mIsLongPattern;
    ProfitTargetInPercentPtr mProfitTarget;
    StopLossInPercentPtr mStopLoss;
    unsigned mPatternIndex;
    unsigned long mIndexDate;
  };
}

#endif // COMPARISONTOPAL_H
