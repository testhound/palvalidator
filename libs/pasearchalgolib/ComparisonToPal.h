#ifndef COMPARISONTOPAL_H
#define COMPARISONTOPAL_H

#include "PalAst.h"

//PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern,
//		       MarketEntryExpression* entry,
//		       ProfitTargetInPercentExpression* profitTarget,
//		       StopLossInPercentExpression* stopLoss);

namespace mkc_searchalgo
{
  using ComparisonEntryType = std::array<unsigned int, 4>;

  ///
  /// A straightforward conversion
  ///
  template <unsigned Dim> class ComparisonToPal
  {
  public:
    ComparisonToPal(const unsigned expectedNumberOfPatterns, bool isLongPattern, const unsigned patternIndex, const unsigned long indexDate):
      mComparisonCount(0),
      mExpectedNumberOfPatterns(expectedNumberOfPatterns),
      mIsLongPattern(isLongPattern),
      mPatternIndex(patternIndex),
      mIndexDate(indexDate)
    {}

    void addComparison(const std::array<unsigned, Dim> comparison)
    {
      std::unique_ptr<PatternExpression> newExpr = std::make_unique<GreaterThanExpr>(priceBarFactory(comparison[0], comparison[1]), priceBarFactory(comparison[2], comparison[3]));
      if (mComparisonCount == 0)
          mPalPatternExpr = newExpr;
      else
          mPalPatternExpr = AndExpr(mPalPatternExpr, newExpr);

      mComparisonCount++;
    }

    void setProfitTarget(decimal7* profitTarget)
    {
      if (mIsLongPattern)
          mProfitTarget = LongSideProfitTargetInPercent(profitTarget);
      else
          mProfitTarget = ShortSideProfitTargetInPercent(profitTarget);

    }

    void setStopLoss(decimal7* stopLoss)
    {
      if (mIsLongPattern)
          mStopLoss = LongSideStopLossInPercent(stopLoss);
      else
          mProfitTarget = ShortSideStopLossInPercent(stopLoss);
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
      std::unique_ptr<PatternDescription> descPtr = std::make_unique<PatternDescription>("", mPatternIndex, mIndexDate, nullptr, nullptr, 0, 0);

      PriceActionLabPattern pattern(descPtr, mPalPatternExpr, mIsLongPattern? LongMarketEntryOnOpen(): ShortMarketEntryOnOpen(), mProfitTarget, mStopLoss);
    }

  private:

    bool isComplete() const { return (mExpectedNumberOfPatterns == mComparisonCount); }

    //** like a factory method
    std::unique_ptr<PriceBarReference> priceBarFactory(const unsigned int offset, const PriceBarReference::ReferenceType ref)
    {
      switch (ref)
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
