#ifndef SEARCHBACKTESTPOLICY_H
#define SEARCHBACKTESTPOLICY_H

#include "McptConfigurationFileReader.h"
#include <string>
#include <vector>
#include <memory>
#include <stdio.h>

//#include "McptConfigurationFileReader.h"
#include "PALMonteCarloValidation.h"
//#include "RobustnessTester.h"
//#include "LogPalPattern.h"
//#include "LogRobustnessTest.h"
//#include "number.h"
//#include <cstdlib>
//#include "ComparisonsGenerator.h"
//#include "UniqueSinglePAMatrix.h"
#include "ComparisonsCombiner.h"
//#include <map>

using namespace mkc_timeseries;
using namespace mkc_searchalgo;
using std::shared_ptr;
using Decimal = num::DefaultNumber;


namespace mkc_searchalgo {

  ///
  /// Backtest policy that uses vector multiplication and non-pyramiding implementation of backtest
  ///
  template <class Decimal, bool isLong> class PlainVanillaShortcutBacktestPolicy
  {
  public:
    PlainVanillaShortcutBacktestPolicy(const std::valarray<Decimal>& backtestResults, const std::valarray<unsigned int>& numBarsInPosition, unsigned int minTrades):
      mBacktestResults(backtestResults),
      mNumBarsInPosition(numBarsInPosition),
      mMinTrades(minTrades),
      mNumTrades(0)
    {}

    void backtest(const std::valarray<Decimal>& occurences)
    {
      if (occurences.size() != mBacktestResults.size())
        throw;

      std::valarray<Decimal> allResults = occurences * mBacktestResults;
      int nextSkipStart = -1;
      int nextSkipEnd = -1;
      for (int i = 0; i < allResults.size(); i++)
        {
          //the skip procedure
          if (i >= nextSkipStart && i <= nextSkipEnd)
            {
              allResults[i] = DecimalConstants<Decimal>::DecimalZero;
              if (i == nextSkipEnd)
                {
                  nextSkipStart = -1;
                  nextSkipEnd = -1;
                }
            }
          //the signal identification procedure
          const Decimal& res = allResults[i];
          if (res != DecimalConstants<Decimal>::DecimalZero)
            {
              mNumTrades++;
              if (res > 0)
                {
                  mNumWinners++;
                  mSumWinners += res;
                }
              if (res < 0)
                {
                  mNumLosers++;
                  mSumLosers += res;
                }

              nextSkipStart = i + 1;
              nextSkipEnd = i + mNumBarsInPosition[i];
            }
        }
    }

    Decimal getProfitFactor() const
    {
      if (mNumTrades < mMinTrades)
        return DecimalConstants<Decimal>::DecimalOne;

      if ((mNumWinners >= 1) and (mNumLosers >= 1))
        return (mSumWinners / num::abs(mSumLosers));
      else if (mNumWinners == 0)
        return (DecimalConstants<Decimal>::DecimalZero);
      else if (mNumLosers == 0)
        return (DecimalConstants<Decimal>::DecimalOneHundred);
      else
        throw std::logic_error(std::string("SearchBacktestPolicy:getProfitFactor - getNumPositions > 0 error"));
    }

  private:
    const std::valarray<Decimal>& mBacktestResults;
    const std::valarray<unsigned int>& mNumBarsInPosition;
    unsigned int mMinTrades;
    unsigned int mNumTrades;
    Decimal mSumWinners;
    Decimal mSumLosers;
    unsigned int mNumWinners;
    unsigned int mNumLosers;
  };





}

#endif // SEARCHBACKTESTPOLICY_H
