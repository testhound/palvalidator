#ifndef BACKTESTPROCESSOR_H
#define BACKTESTPROCESSOR_H

#include "Sorters.h"

namespace mkc_searchalgo
{
  using StrategyRepresentationType = std::vector<unsigned int>;

  template <class Decimal, typename TSearchAlgoBacktester>
  class BacktestProcessor
  {
  public:
    BacktestProcessor(unsigned minTrades, std::shared_ptr<TSearchAlgoBacktester>& searchAlgoBacktester, const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& uniques):
      mUniqueId(0),
      mMinTrades(minTrades),
      mSearchAlgoBacktester(searchAlgoBacktester),
      mResults(),
      mStratMap(),
      mUniques(uniques)
    {}

    void processResult(const StrategyRepresentationType & compareContainer)
    {
      //init to ones
      std::valarray<Decimal> occurences(DecimalConstants<Decimal>::DecimalOne, mUniques->getDateCount());
      //the multiplication
      for (unsigned int el: compareContainer)
        occurences *= mUniques->getMappedElement(el);

      mSearchAlgoBacktester->backtest(occurences);
      const Decimal& pf = mSearchAlgoBacktester->getProfitFactor();
      unsigned int trades = mSearchAlgoBacktester->getTradeNumber();
      if (trades < mMinTrades)
        return;
      mResults.emplace_back(pf, trades, mUniqueId);
      mStratMap[mUniqueId] = compareContainer;
      mUniqueId++;
    }


    const std::vector<std::tuple<Decimal, unsigned int, int>>& getResults() const
    { return mResults; }


    std::unordered_map<int, StrategyRepresentationType>& getStrategyMap()
    { return mStratMap;}


    template <class TSorter>
    void sortResults()
    {
      std::sort(mResults.begin(), mResults.end(), TSorter::sort);
    }

    template <class TSorter>
    void sortResults(Decimal ratio, Decimal multiplier)
    {
      std::cout << "sortResults called with: " << ratio << ", " << multiplier << std::endl;
      std::sort(mResults.begin(), mResults.end(), TSorter(ratio, multiplier));
    }

    void clearAll()
    {
      mResults.clear();
      mResults.shrink_to_fit();
      mStratMap.clear();
    }

  private:

    int mUniqueId;
    unsigned mMinTrades;
    std::shared_ptr<TSearchAlgoBacktester> mSearchAlgoBacktester;
    std::vector<std::tuple<Decimal, unsigned int, int>> mResults;
    std::unordered_map<int, StrategyRepresentationType> mStratMap;
    const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& mUniques;

  };


}

#endif // BACKTESTPROCESSOR_H
