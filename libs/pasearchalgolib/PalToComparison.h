#ifndef PALTOCOMPARISON_H
#define PALTOCOMPARISON_H

#include <unordered_set>
#include "PalAst.h"
#include "PalStrategy.h"
#include "ComparisonsGenerator.h"
#include "PalStrategyAlwaysOn.h"
#include <type_traits>

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  static unsigned int getBarReferenceId(PriceBarReference::ReferenceType ref)
  {
    switch (ref)
      {
      case PriceBarReference::ReferenceType::OPEN:
        return 0;
      case PriceBarReference::ReferenceType::HIGH:
        return 1;
      case PriceBarReference::ReferenceType::LOW:
        return 2;
      case PriceBarReference::ReferenceType::CLOSE:
        return 3;
      }
      throw;
  }



  class PalToComparison
  {
  public:
    PalToComparison (PatternExpression *expression)
    {
      unwindExpression(expression);
      std::sort(mComparisons.begin(), mComparisons.end());
    }

    PalToComparison(const PalToComparison& rhs): mComparisons(rhs.mComparisons)
    {}

    PalToComparison& operator =(const PalToComparison& ) = delete;

    bool operator < (const PalToComparison& rhs) const
    {
      if (mComparisons.size() < rhs.mComparisons.size())
        return true;
      if (mComparisons.size() > rhs.mComparisons.size())
        return false;
      return (this->getIntRepresentation() < rhs.getIntRepresentation());
    }

    int getIntRepresentation() const
    {
      int ret = 0;
      for (const ComparisonEntryType& comp: mComparisons)
        {
            ret += comp[0] * 1000 + comp[1] * 100 + comp[2] * 10 + comp[3];
        }
      return ret;
    }

    bool operator == (const PalToComparison& rhs)
    {
      return (mComparisons == rhs.mComparisons);
    }

    const std::vector<ComparisonEntryType>& getComparisons() const { return mComparisons; }


  private:

    void unwindExpression(PatternExpression * expression)
    {
      if (AndExpr *pAnd = dynamic_cast<AndExpr*>(expression))
        {
          unwindExpression (pAnd->getLHS());
          unwindExpression (pAnd->getRHS());
        }
      else if (GreaterThanExpr *pGreaterThan = dynamic_cast<GreaterThanExpr*>(expression))
        {
          unsigned int offset1 = pGreaterThan->getLHS()->getBarOffset();
          unsigned int ref1 = getBarReferenceId(pGreaterThan->getLHS()->getReferenceType());
          unsigned int offset2 = pGreaterThan->getRHS()->getBarOffset();
          unsigned int ref2 = getBarReferenceId(pGreaterThan->getRHS()->getReferenceType());
          mComparisons.push_back(ComparisonEntryType{offset1, ref1, offset2, ref2});
        }
      else
        throw PalPatternInterpreterException ("PALPatternInterpreter::evaluateExpression Illegal PatternExpression");
    }


    std::vector<ComparisonEntryType> mComparisons;
  };



}

#endif // PALTOCOMPARISON_H
