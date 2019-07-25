#ifndef COMPARISONTOPAL_H
#define COMPARISONTOPAL_H

#include <unordered_set>
#include "PalAst.h"
#include "PalStrategy.h"
#include "ComparisonsGenerator.h"


//PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern,
//		       MarketEntryExpression* entry,
//		       ProfitTargetInPercentExpression* profitTarget,
//		       StopLossInPercentExpression* stopLoss);

using namespace mkc_timeseries;

namespace mkc_searchalgo
{


  //**like a factory method
  static std::unique_ptr<PriceBarReference> priceBarFactory(const unsigned int offset, const unsigned int ref)
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

//  PalLongStrategy(const std::string& strategyName,
//                  std::shared_ptr<PriceActionLabPattern> pattern,
//                  std::shared_ptr<Portfolio<Decimal>> portfolio)

  ///
  /// A straightforward conversion
  ///
  template <class Decimal, size_t Dim>
  class ComparisonToPal
  {
  public:
    ComparisonToPal(const std::array<ComparisonEntryType, Dim>& compareBatch,
                    bool isLongPattern, const unsigned patternIndex, const unsigned long indexDate,
                    decimal7* const profitTarget, decimal7* const stopLoss, std::shared_ptr<Portfolio<Decimal>>& portfolio):
      mComparisonCount(0),
      mExpectedNumberOfPatterns(compareBatch.size()),
      mIsLongPattern(isLongPattern),
      mPatternDescription(allocatePatternDescription(patternIndex, indexDate)),
      mProfitTarget(allocateProfitTarget(profitTarget)),
      mStopLoss(allocateStopLoss(stopLoss)),
      mMarketEntry(allocateMarketEntry())
    {
      for(const auto& comparison: compareBatch)
        addComparison(comparison);
      mPalPattern = std::make_shared<PriceActionLabPattern>(allocatePattern());

      if (isLongPattern)
        {
          std::string strategyName= std::string("PAL Long Strategy ") + std::to_string(patternIndex);
          mPalStrategy = std::make_shared<PalLongStrategy<Decimal>>(strategyName, mPalPattern, portfolio);
        }
      else
        {
          std::string strategyName= std::string("PAL Short Strategy ") + std::to_string(patternIndex);
          mPalStrategy = std::make_shared<PalShortStrategy<Decimal>>(strategyName, mPalPattern, portfolio);
        }

    }

    ComparisonToPal(const ComparisonToPal<Decimal, Dim>&) = delete;

    ComparisonToPal<Decimal, Dim>& operator=(const ComparisonToPal<Decimal, Dim>&) = delete;


  public:

    const std::shared_ptr<PalStrategy<Decimal>>& getPalStrategy() const { return mPalStrategy; }

    //const PriceActionLabPattern* getPattern() const { return mPalPattern.get(); }

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

    ProfitTargetInPercentExpression* allocateProfitTarget(decimal7* profitTarget)
    {
      if (mIsLongPattern)
          return new LongSideProfitTargetInPercent(profitTarget);
      else
          return new ShortSideProfitTargetInPercent(profitTarget);
    }

    StopLossInPercentExpression* allocateStopLoss(decimal7* stopLoss)
    {
      if (mIsLongPattern)
          return new LongSideStopLossInPercent(stopLoss);
      else
          return new ShortSideStopLossInPercent(stopLoss);
    }

    MarketEntryExpression* allocateMarketEntry()
    {
      if (mIsLongPattern)
        return new LongMarketEntryOnOpen();
      else
        return new ShortMarketEntryOnOpen();
    }

    PatternDescription* allocatePatternDescription(unsigned int patternIndex, unsigned int indexDate)
    {
      return new PatternDescription(",", patternIndex, indexDate, nullptr, nullptr, 0, 0);
    }


    PriceActionLabPattern* allocatePattern()
    {
      if (!isComplete())
        throw;

      return new PriceActionLabPattern(mPatternDescription.get(), mPalPatternExpr.get(), mMarketEntry.get(), mProfitTarget.get(), mStopLoss.get());
    }

    bool isComplete() const { return (mPalPatternExpr && (mExpectedNumberOfPatterns == mComparisonCount) && mProfitTarget && mStopLoss && mMarketEntry && mPatternDescription); }



    unsigned mComparisonCount;
    unsigned mExpectedNumberOfPatterns;
    bool mIsLongPattern;
    std::unique_ptr<PatternDescription> mPatternDescription;
    std::unique_ptr<ProfitTargetInPercentExpression> mProfitTarget;
    std::unique_ptr<StopLossInPercentExpression> mStopLoss;
    std::unique_ptr<MarketEntryExpression> mMarketEntry;
    std::unique_ptr<PatternExpression> mPalPatternExpr;
    std::shared_ptr<PriceActionLabPattern> mPalPattern;
    std::shared_ptr<PalStrategy<Decimal>> mPalStrategy;

  };
}

#endif // COMPARISONTOPAL_H
