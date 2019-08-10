#ifndef BACKTESTPROCESSOR_H
#define BACKTESTPROCESSOR_H

#include "OriginalSearchAlgoBacktester.h"

namespace mkc_searchalgo
{

  ///
  /// Sorts descending on Trade-Weighted Profit Factor (TWPF)
  /// (so as to keep the more active strategies for subsequent rounds)
  /// then ascending on unique id (for no collision)
  ///
  template <class Decimal>
  struct TwpfSorter
  {
    bool static sort(const std::tuple<Decimal, unsigned int, int> & lhs, const std::tuple<Decimal, unsigned int, int>& rhs)
    {
      Decimal factor1 = std::get<0>(lhs) * Decimal(std::get<1>(lhs));
      Decimal factor2 = std::get<0>(rhs) * Decimal(std::get<1>(rhs));
      if (factor1 > factor2)
        return true;
      if (factor1 < factor2)
        return false;
      //when equal
      return std::get<2>(lhs) < std::get<2>(rhs);
    }
  };

  /// Simple Profit factor sorting Desc
  template <class Decimal>
  struct PfSorter
  {
    bool static sort(const std::tuple<Decimal, unsigned int, int> & lhs, const std::tuple<Decimal, unsigned int, int>& rhs)
    {
      Decimal pf1 = std::get<0>(lhs);
      Decimal pf2 = std::get<0>(rhs);
      if (pf1 > pf2)
        return true;
      if (pf1 < pf2)
        return false;
      //when profit factors equal
      unsigned int trades1 = std::get<1>(lhs);
      unsigned int trades2 = std::get<1>(rhs);
      if (trades1 > trades2)
        return true;
      if (trades1 < trades2)
        return false;
      //when trades also equal use unique id to sort with stability
      return std::get<2>(lhs) < std::get<2>(rhs);
    }
  };


  template <class Decimal, typename TSearchAlgoBacktester>
  class BacktestProcessor
  {
  public:
    BacktestProcessor(unsigned minTrades, std::shared_ptr<TSearchAlgoBacktester>& searchAlgoBacktester):
      mUniqueId(0),
      mMinTrades(minTrades),
      mSearchAlgoBacktester(searchAlgoBacktester)
    {}

  protected:
    void processResult(const std::unordered_map<unsigned int, std::valarray<Decimal>>& compareContainer)
    {
      mSearchAlgoBacktester.backtest(compareContainer);
      const Decimal& pf = mSearchAlgoBacktester->getProfitFactor();
      unsigned int trades = mSearchAlgoBacktester->getTradeNumber();

      if (trades < mMinTrades)
        return;

      mResults.push_back(std::make_tuple<Decimal, unsigned int, int>(pf, trades, mUniqueId));
      mStratMap[mUniqueId] = compareContainer;
      mUniqueId++;
    }


  public:
    std::vector<std::tuple<Decimal, unsigned int, int>>& getResults() const { return mResults; }
    std::unordered_map<int, std::unordered_map<unsigned int, std::valarray<Decimal>>>& getStrategyMap() const { return mStratMap; }

    template <class TSorter>
    void sortResults()
    {
      std::sort(mResults.begin(), mResults.end(), TSorter::sort);
    }

  private:
    int mUniqueId;
    unsigned mMinTrades;
    shared_ptr<TSearchAlgoBacktester> mSearchAlgoBacktester;
    std::vector<std::tuple<Decimal, unsigned int, int>> mResults;
    std::unordered_map<int, std::unordered_map<unsigned int, std::valarray<Decimal>>> mStratMap;

  };


}

#endif // BACKTESTPROCESSOR_H
