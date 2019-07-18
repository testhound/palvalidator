#ifndef COMPARISONSGENERATOR_H
#define COMPARISONSGENERATOR_H

#include <boost/circular_buffer.hpp>
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include "ComparableBar.h"

namespace mkc_searchalgo
{
  //a simplified type to represent bar to bar comparison
  using ComparisonEntryType = std::array<unsigned int, 4>;

  template <class Decimal> class ComparisonsGenerator
  {
  public:
    explicit ComparisonsGenerator(unsigned int maxlookback):
      mDateIndex(0),
      mMaxLookBack(maxlookback),
      mBarBuffer(maxlookback),  //circular buffer instantiation
      mComparisonsCount(0),
      mComparisonsBatches{{mDateIndex, std::vector<ComparisonEntryType>()}}
    {}

    const std::map<unsigned int, std::vector<ComparisonEntryType>>& getComparisons() const { return mComparisonsBatches; }

    const std::set<ComparisonEntryType>& getUniqueComparisons() const { return mUniqueComparisons; }

    const unsigned int& getComparisonsCount() const { return mComparisonsCount; }

    void addNewLastBar(const Decimal& open, const Decimal& high, const Decimal& low, const Decimal& close)
    {
      //create new last bar (0 indexed)
      ComparableBar<Decimal, 4> lastBar(open, high, low, close, 0);
      //shift up all bar offsets in circular buffer
      shiftBarsUp();
      //add to circular buffer (because we will self-compare, too)
      mBarBuffer.push_back(lastBar);
      std::cout << "bar buffer now has " << mBarBuffer.size() << " elements " << std::endl;
      for (int i = 0; i < mBarBuffer.size(); ++i)
        {
          std::cout << mBarBuffer[i] << std::endl;
        }
      runCompare();
      mDateIndex++;
      newComparisonsBatch();
    }

  private:

    template <class Comparable>
    void compareWith(const Comparable& first, const Comparable& second)
    {
      const auto& fOhlcArr = first.getOhlcArr();
      const auto& sOhlcArr = second.getOhlcArr();

      bool same = first.getOffset() == second.getOffset();
      if (same && !(first == second))
          throw;

      for (int i = 0; i < fOhlcArr.size(); ++i)
        {
          for (int c = 0; c < fOhlcArr.size(); ++c)
            {
              // self-checking case (high- and low- based comparison, or same-to-same does not make sense)
              if (same && (c == 1 || i == 1 || c == 2 || i == 2 || c == i) )
                continue;

              if (fOhlcArr[i] > sOhlcArr[c])
                {
                  addComparison(first.getOffset(), i, second.getOffset(), c);
                }
              else if (fOhlcArr[i] < sOhlcArr[c])
                {
                  addComparison(second.getOffset(), c, first.getOffset(), i);
                }
            }
        }
    }

    void addComparison(unsigned int fOffset, unsigned int fOhlcId, unsigned int sOffset, unsigned int sOhlcId)
    {
      ComparisonEntryType compEntry = {fOffset, fOhlcId, sOffset, sOhlcId};
      mComparisonsBatches[mDateIndex].push_back(compEntry);
      mComparisonsCount++;
      mUniqueComparisons.insert(compEntry);
    }
    //compare last bar with all bars (including itself)
    void runCompare()
    {
      const ComparableBar<Decimal, 4>& lastBar = mBarBuffer.back();
      for (int i = mBarBuffer.size() -1; i --> 0;)
          compareWith(lastBar, mBarBuffer[i]);

    }

    void shiftBarsUp()
    {
      for (int i = 0; i < mBarBuffer.size(); ++i)
        mBarBuffer[i].incrementOffset();
    }

    void newComparisonsBatch()
    {
      std::vector<std::array<unsigned int, 4>> newBatch(mComparisonsBatches[mDateIndex - 1]);   //deep copy vector ctor
      //increment offsets in old comparisons
      for (std::vector<ComparisonEntryType>::iterator it = newBatch.begin(); it != newBatch.end(); ++it)
        {
          (*it)[0]++;
          (*it)[2]++;
          //erase comparisons outside of lookback window
          if ( (*it)[0] > mMaxLookBack - 1 || (*it)[2] > mMaxLookBack -1 )
            newBatch.erase(it--);   //erases element and decrements iterator for correct iteration
        }
      mComparisonsBatches[mDateIndex] = newBatch;
    }

    unsigned int mDateIndex;
    unsigned int mMaxLookBack;
    unsigned int mComparisonsCount;
    boost::circular_buffer<ComparableBar<Decimal, 4>> mBarBuffer;
    std::map<unsigned int, std::vector<ComparisonEntryType>> mComparisonsBatches;
    std::set<ComparisonEntryType> mUniqueComparisons;
  };
}

#endif // COMPARISONSGENERATOR_H
