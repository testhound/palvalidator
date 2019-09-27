// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef STEPPINGPOLICY_H
#define STEPPINGPOLICY_H

#include "BacktestProcessor.h"

namespace mkc_searchalgo
{

  template <class T>
  static bool findInVector(const std::vector<T>& vect, const T& value)
  {
    return (std::end(vect) != std::find(std::begin(vect), std::end(vect), value));
  }

  /// valarray needs specialized handling of equality check (otherwise the operator== returns valarray of booleans)
  template <class Decimal>
  static bool findInVector(const std::vector<std::valarray<Decimal>>& vect, const std::valarray<Decimal>& value)
  {
    for (const auto& el: vect)
      {
        if ((el == value).min())
          return true;
      }
    return false;
  }


  ///
  /// The Stepping policy based on single sorter
  /// and a 80/20 (20% sampled from strategies not found at the top of the list)
  ///
  template <class Decimal, typename TSearchAlgoBacktester, typename TSorter>
  class SimpleSteppingPolicy
  {
  public:
    SimpleSteppingPolicy(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy,
                         const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& singlePA,
                         size_t passingStratNumPerRound,
                         Decimal sortMultiplier):
      mProcessingPolicy(processingPolicy),
      mPassingStratNumPerRound(passingStratNumPerRound),
      mSortMultiplier(sortMultiplier)
      {}

  private:
    Decimal getAverageRatio(const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>& results)
    {
      unsigned int tradeSum{0};
      Decimal ppSum(0.0);
      for (const auto& tup: results)
        {
          unsigned int trades = std::get<1>(tup);
          ResultStat<Decimal> stat = std::get<0>(tup);
          if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred)
            continue;
          tradeSum += trades;
          ppSum += stat.PALProfitability;
        }
      Decimal tradeAvg = Decimal(tradeSum) / Decimal(static_cast<unsigned int>(results.size()));
      Decimal ppAvg = ppSum / Decimal(static_cast<unsigned int>(results.size()));
      std::cout << "trade avg: " << tradeAvg << std::endl;
      std::cout << "PAL profitability avg: " << ppAvg << std::endl;
      return (tradeAvg / ppAvg);
    }
  protected:
    std::vector<StrategyRepresentationType> passes()
    {
      std::vector<StrategyRepresentationType> ret;

      //fetch results
      const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>&
          results = mProcessingPolicy->getResults();
      std::unordered_map<int, StrategyRepresentationType>&
          stratMap = mProcessingPolicy->getStrategyMap();

      size_t to80 = static_cast<size_t>(mPassingStratNumPerRound*0.8);
      size_t to20 = static_cast<size_t>(to80 * 0.25);
      size_t remainder = (results.size() - to80);
      size_t everyNth = remainder/to20;

      //std::cout << "sorting..." << std::endl;

      Decimal avgRatio = getAverageRatio(results);
      mProcessingPolicy->template sortResults<TSorter>(avgRatio, mSortMultiplier);

      size_t n = 0;
      for (const auto& tup: results)
        {
          if (ret.size() < to80)
            {
              int ind = std::get<2>(tup);
              // do not pass "perfect" or "useless" strategies
              const ResultStat<Decimal>& stat = std::get<0>(tup);

              if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero)
                continue;
              StrategyRepresentationType & strat = stratMap[ind];
              //check for repeats (only here, as at this stage processing time is less pertinent)
              std::sort(strat.begin(), strat.end());
              if (!findInVector(ret, strat))
                {
                  //std::cout << std::get<0>(tup) << ", " << std::get<1>(tup) << std::endl;
                  ret.push_back(strat);
                }
            }
          else    //last 20% taken at equal intervals from the last passing element to the container's last(worst) element
            {
              n++;
              if (n % everyNth == 0)
                {
                  int ind = std::get<2>(tup);
                  // do not pass "perfect" or "useless" strategies
                  const ResultStat<Decimal>& stat = std::get<0>(tup);
                  if (stat.ProfitFactor == DecimalConstants<Decimal>::DecimalOneHundred || stat.ProfitFactor == DecimalConstants<Decimal>::DecimalZero)
                    continue;

                  StrategyRepresentationType & strat = stratMap[ind];
                  //check for repeats (only here, as at this stage processing time is less pertinent)
                  std::sort(strat.begin(), strat.end());
                  if (!findInVector(ret, strat))
                    {
                      if (ret.size() < mPassingStratNumPerRound)
                        {
                          //std::cout << "last addition: " << std::get<0>(tup) << ", " << std::get<1>(tup) << std::endl;
                          ret.push_back(strat);
                        }
                    }
                  else
                    {
                      n--;  //try next one
                    }

                }
            }
        }
        return ret;
    }

  private:
    shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mProcessingPolicy;
    size_t mPassingStratNumPerRound;
    Decimal mSortMultiplier;

  };


}


#endif // STEPPINGPOLICY_H
