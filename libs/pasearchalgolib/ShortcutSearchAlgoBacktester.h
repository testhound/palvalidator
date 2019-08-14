#ifndef SHORTCUTSEARCHALGOBACKTESTER_H
#define SHORTCUTSEARCHALGOBACKTESTER_H

#include "McptConfigurationFileReader.h"
#include <string>
#include <vector>
#include <memory>
#include <stdio.h>

//#include "McptConfigurationFileReader.h"
#include "PALMonteCarloValidation.h"
#include <algorithm>

using namespace mkc_timeseries;
using std::shared_ptr;
using Decimal = num::DefaultNumber;


namespace mkc_searchalgo {

  class ShortcutBacktestException : public std::logic_error
  {
  public:
    ShortcutBacktestException(const std::string msg)
      : std::logic_error(msg)
    {}

    ~ShortcutBacktestException()
    {}

  };

  enum ShortcutBacktestMethod {PlainVanilla, Pyramiding};

  ///
  /// Backteser that uses vector multiplication based implementation
  ///
  template <class Decimal, ShortcutBacktestMethod backtestMethod> class ShortcutSearchAlgoBacktester
  {
  public:
    ShortcutSearchAlgoBacktester(const std::valarray<Decimal>& backtestResults, const std::valarray<unsigned int>& numBarsInPosition, unsigned int minTrades, bool isLong):
      mBacktestResultBase(backtestResults),
      mNumBarsInPosition(numBarsInPosition),
      mMinTrades(minTrades),
      mNumTrades(0),
      mIsLong(isLong)
    {}

    bool getIsLong() const { return mIsLong; }

    void backtest(const std::valarray<Decimal>& compareContainer)
    {
      throw ShortcutBacktestException("Backtesting logic for ShortcutBacktestMethod specified in template has not been implemented yet!");
    }

    Decimal getProfitFactor() const
    {
      if (mNumTrades < mMinTrades)
        return DecimalConstants<Decimal>::DecimalZero;

      if ((mNumWinners >= 1) and (mNumLosers >= 1))
        return (mSumWinners / num::abs(mSumLosers));
      else if (mNumWinners == 0)
        return (DecimalConstants<Decimal>::DecimalZero);
      else if (mNumLosers == 0)
        return (DecimalConstants<Decimal>::DecimalOneHundred);
      else
        throw std::logic_error(std::string("SearchBacktestPolicy:getProfitFactor - getNumPositions > 0 error"));
    }

    Decimal getAverageWinningTrade() const
    {
      if (mNumWinners >= 1)
        return (Decimal(mSumWinners) /Decimal(mNumWinners));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getAverageLosingTrade() const
    {
      if (mNumLosers >= 1)
        return (Decimal(mSumLosers) /Decimal(mNumLosers));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPayoffRatio() const
    {
      if (mNumTrades > 0)
        {
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            {
              Decimal avgLoser = num::abs(getAverageLosingTrade());
              if (avgLoser != DecimalConstants<Decimal>::DecimalZero)
                return (getAverageWinningTrade() / avgLoser);
              else
                return (getAverageWinningTrade());
            }
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getAverageWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPALProfitability() const
    {
      if (mNumTrades > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            return ((pf/denominator) * DecimalConstants<Decimal>::DecimalOneHundred);
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    unsigned int getTradeNumber() const { return mNumTrades; }

  private:
    void reset()
    {
      mNumTrades = 0;
      mNumWinners = 0;
      mNumLosers = 0;
      mSumWinners = 0;
      mSumLosers = 0;
    }

  private:
    const std::valarray<Decimal>& mBacktestResultBase;
    const std::valarray<unsigned int>& mNumBarsInPosition;
    unsigned int mMinTrades;
    unsigned int mNumTrades;
    Decimal mSumWinners;
    Decimal mSumLosers;
    unsigned int mNumWinners;
    unsigned int mNumLosers;
    bool mIsLong;
  };


  template <>
  void ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::PlainVanilla>::backtest(const std::valarray<Decimal>& occurences)
  {
    reset();
  
    if (occurences.size() != mBacktestResultBase.size())
      throw;
    //generate results for all possible entries
    std::valarray<Decimal> allResults = occurences * mBacktestResultBase;
    int nextSkipStart = -1;
    int nextSkipEnd = -1;

    /// the section that nullifies signals where a previous position would be on
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
            if (res > DecimalConstants<Decimal>::DecimalZero)
              {
                mNumWinners++;
                mSumWinners += res;
              }
            if (res < DecimalConstants<Decimal>::DecimalZero)
              {
                mNumLosers++;
                mSumLosers += res;
              }

            nextSkipStart = i + 1;
            nextSkipEnd = i + mNumBarsInPosition[i] - 1;
            //std::cout << "skip identified: start: " << nextSkipStart << ", end: " << nextSkipEnd << ", bars in position: " << mNumBarsInPosition[i] << std::endl;
          }
      }
  }

  template <>
  void ShortcutSearchAlgoBacktester<Decimal, ShortcutBacktestMethod::Pyramiding>::backtest(const std::valarray<Decimal>& occurences)
  {
    reset();

    if (occurences.size() != mBacktestResultBase.size())
      throw;

    std::valarray<Decimal> allResults = occurences * mBacktestResultBase;

    for (int i = 0; i < allResults.size(); i++)
      {
        //the signal identification procedure, without skipping
        const Decimal& res = allResults[i];
        if (res != DecimalConstants<Decimal>::DecimalZero)
          {
            mNumTrades++;
            if (res > DecimalConstants<Decimal>::DecimalZero)
              {
                mNumWinners++;
                mSumWinners += res;
              }
            if (res < DecimalConstants<Decimal>::DecimalZero)
              {
                mNumLosers++;
                mSumLosers += res;
              }
          }
      }
  }


}

#endif // SHORTCUTSEARCHALGOBACKTESTER_H
