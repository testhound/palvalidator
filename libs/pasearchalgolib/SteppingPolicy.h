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
    SimpleSteppingPolicy(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy, size_t passingStratNumPerRound):
      mProcessingPolicy(processingPolicy),
      mPassingStratNumPerRound(passingStratNumPerRound)
      {}

  private:
    Decimal getAverageRatio(const std::vector<std::tuple<Decimal, unsigned int, int>>& results)
    {
      unsigned int tradeSum{0};
      Decimal pfSum(0.0);
      for (const auto& tup: results)
        {
          tradeSum += std::get<1>(tup);
          pfSum += std::get<0>(tup);
        }
      Decimal tradeAvg = Decimal(tradeSum) / Decimal(static_cast<unsigned int>(results.size()));
      Decimal pfAvg = pfSum / Decimal(static_cast<unsigned int>(results.size()));
      std::cout << "trade avg: " << tradeAvg << std::endl;
      std::cout << "pf avg: " << pfAvg << std::endl;
      return (tradeAvg / pfAvg);
    }
  protected:
    std::vector<StrategyRepresentationType> passes()
    {
      std::vector<StrategyRepresentationType> ret;

      //fetch results
      const std::vector<std::tuple<Decimal, unsigned int, int>>&
          results = mProcessingPolicy->getResults();
      std::unordered_map<int, StrategyRepresentationType>&
          stratMap = mProcessingPolicy->getStrategyMap();

      size_t to80 = static_cast<size_t>(mPassingStratNumPerRound*0.8);
      size_t to20 = static_cast<size_t>(to80 * 0.25);
      size_t remainder = (results.size() - to80);
      size_t everyNth = remainder/to20;

      //std::cout << "sorting..." << std::endl;

      Decimal avgRatio = getAverageRatio(results);
      Decimal multiplier = Decimal(3.0);
      mProcessingPolicy->template sortResults<TSorter>(avgRatio, multiplier);

      size_t n = 0;
      for (const auto& tup: results)
        {
          if (ret.size() < to80)
            {
              int ind = std::get<2>(tup);
              // do not pass "perfect strategies
              if (std::get<0>(tup) == DecimalConstants<Decimal>::DecimalOneHundred)
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
                  // do not pass "perfect strategies
                  if (std::get<0>(tup) == DecimalConstants<Decimal>::DecimalOneHundred)
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

  };


//  ///
//  /// The stepping policy with 2 different sorters
//  ///
//  template <class Decimal, typename TSearchAlgoBacktester>
//  class SteppingPolicy
//  {
//  public:
//    SteppingPolicy(const std::shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy, size_t passingStratNumPerRound):
//      mProcessingPolicy(processingPolicy),
//      mPassingStratNumPerRound(passingStratNumPerRound)
//      {}

//  protected:

//    std::vector<StrategyRepresentationType> passes()
//    {
//      std::cout << "Stepping passes called " << std::endl;

//      std::vector<StrategyRepresentationType> ret;

//      //first 40% limit, with Profit Factor based sorting
//      size_t to40 = static_cast<size_t>(mPassingStratNumPerRound*0.4);
//      //sort
//      mProcessingPolicy->template sortResults<Sorters::PfSorter<Decimal>>();
//      //fetch results
//      const std::vector<std::tuple<Decimal, unsigned int, int>>&
//          results = mProcessingPolicy->getResults();
//      std::unordered_map<int, StrategyRepresentationType>&
//          stratMap = mProcessingPolicy->getStrategyMap();

//      for (const std::tuple<Decimal, unsigned int, int>& tup: results)
//        {
//          if (ret.size() < to40)
//            {
//              int ind = std::get<2>(tup);
//              std::cout << std::get<0>(tup) << ", " << std::get<1>(tup) << std::endl;
//              StrategyRepresentationType& strat = stratMap[ind];
//              //check for repeats (only here, as at this stage processing time is less pertinent)
//              std::sort(strat.begin(), strat.end());
//              if (!findInVector(ret, strat))
//                   ret.push_back(strat);

//            }
//          else
//            {
//              break;
//            }
//        }
//      //second 40% - 80%, with Trade Weighted Profit Factor based sorting
//      size_t to80 = to40*2;
//      size_t to20 = static_cast<size_t>(to40 * 0.5);
//      size_t remainder = (results.size() - to80);
//      size_t everyNth = remainder/to20;

//      //re-sort (this should update results)
//      std::cout << "resorting..." << std::endl;

//      mProcessingPolicy->template sortResults<Sorters::TwpfSorter<Decimal>>();

//      size_t n = 0;
//      for (const auto& tup: results)
//        {
//          if (ret.size() < to80)
//            {
//              int ind = std::get<2>(tup);
//              StrategyRepresentationType & strat = stratMap[ind];
//              //check for repeats (only here, as at this stage processing time is less pertinent)
//              std::sort(strat.begin(), strat.end());
//              if (!findInVector(ret, strat))
//                {
//                  std::cout << std::get<0>(tup) << ", " << std::get<1>(tup) << std::endl;
//                  ret.push_back(strat);
//                }
//            }
//          else    //last 20% taken at equal intervals from the last passing element to the container's last(worst) element
//            {
//              n++;
//              if (n % everyNth == 0)
//                {
//                  int ind = std::get<2>(tup);

//                  StrategyRepresentationType & strat = stratMap[ind];
//                  //check for repeats (only here, as at this stage processing time is less pertinent)
//                  std::sort(strat.begin(), strat.end());
//                  if (!findInVector(ret, strat))
//                    {
//                      if (ret.size() < mPassingStratNumPerRound)
//                        {
//                          std::cout << "last addition: " << std::get<0>(tup) << ", " << std::get<1>(tup) << std::endl;
//                          ret.push_back(strat);
//                        }
//                    }
//                  else
//                    {
//                      n--;  //try next one
//                    }

//                }
//            }
//        }

//      return ret;

//    }
//  private:
//    shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mProcessingPolicy;
//    size_t mPassingStratNumPerRound;

//  };

}


#endif // STEPPINGPOLICY_H
