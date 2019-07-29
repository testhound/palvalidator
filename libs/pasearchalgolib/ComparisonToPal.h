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
  /// A straightforward conversion
  ///
  template <class Decimal>
  class ComparisonToPal
  {
  public:
    ComparisonToPal(const std::vector<ComparisonEntryType>& compareBatch,
                    bool isLongPattern, const unsigned patternIndex, const unsigned long indexDate,
                    decimal7* const profitTarget, decimal7* const stopLoss, std::shared_ptr<Portfolio<Decimal>>& portfolio):
      mComparisonCount(0),
      mExpectedNumberOfPatterns(compareBatch.size()),
      mIsLongPattern(isLongPattern),
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

      //std::cout << "got into ctor" << std::endl;

      for(const auto& comparison: compareBatch)
        addComparison(comparison);

      //std::cout << "added comparisons" << std::endl;
      //std::cout << "Is complete: " << isComplete() << std::endl;
      if (!isComplete())
          throw;

      mPalPattern = std::make_shared<PriceActionLabPattern>(mPatternDescription, getPatternExpression(), mMarketEntry.get(), mProfitTarget.get(), mStopLoss.get());

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

    ComparisonToPal(const ComparisonToPal<Decimal>&) = delete;

    ComparisonToPal<Decimal>& operator=(const ComparisonToPal<Decimal>&) = delete;


    ~ComparisonToPal()
    {
      //the structure of intertwound shared pointers for strategies ...
    }


    const std::shared_ptr<PalStrategy<Decimal>>& getPalStrategy() const { return mPalStrategy; }

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

    //const PriceActionLabPattern* getPattern() const { return mPalPattern.get(); }

  private:

    void addComparison(const ComparisonEntryType& comparison)
    {
      //std::cout << "enter comparison " << std::endl;
      GreaterThanExpr* newExprPtr = new GreaterThanExpr(mPriceBarFactory.getPriceBar(comparison[0], comparison[1]), mPriceBarFactory.getPriceBar(comparison[2], comparison[3]));
      //std::cout << "comparison count: " << mComparisonCount << std::endl;
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
      return new PatternDescription("", patternIndex, indexDate, nullptr, nullptr, 0, 0);
    }


//    PriceActionLabPattern* allocatePattern()
//    {
//      if (!isComplete())
//        throw;

//      return new PriceActionLabPattern(mPatternDescription.get(), mPalPatternExpr.get(), mMarketEntry.get(), mProfitTarget.get(), mStopLoss.get());
//    }

    bool isComplete() const { return ((mExpectedNumberOfPatterns == mComparisonCount) && mProfitTarget && mStopLoss && mMarketEntry && mPatternDescription); }



    unsigned mComparisonCount;
    unsigned mExpectedNumberOfPatterns;
    bool mIsLongPattern;
    PatternDescription* mPatternDescription;
    std::unique_ptr<ProfitTargetInPercentExpression> mProfitTarget;
    std::unique_ptr<StopLossInPercentExpression> mStopLoss;
    std::unique_ptr<MarketEntryExpression> mMarketEntry;
    std::vector<GreaterThanExpr*> mPalGreaterThanPatternExpressions;
    std::vector<AndExpr*> mPalAndPatternExpressions;
    std::shared_ptr<PriceActionLabPattern> mPalPattern;
    std::shared_ptr<PalStrategy<Decimal>> mPalStrategy;
    PriceBarFactory mPriceBarFactory;

  };
}

#endif // COMPARISONTOPAL_H