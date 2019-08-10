#ifndef STEPPINGPOLICY_H
#define STEPPINGPOLICY_H

#include "BacktestProcessor.h"

namespace mkc_searchalgo
{

  template <class Decimal, typename TSearchAlgoBacktester>
  class SteppingPolicyBase
  {
  public:
    SteppingPolicyBase(const BacktestProcessor<Decimal, TSearchAlgoBacktester>& processingPolicy, size_t passingStratNumPerRound):
    mProcessingPolicy(processingPolicy),
    mPassingStratNumPerRound(passingStratNumPerRound)
    {}
    virtual std::vector<std::unordered_map<unsigned int, std::valarray<Decimal>>> passes() = 0;

  protected:
    BacktestProcessor<Decimal, TSearchAlgoBacktester> mProcessingPolicy;
    size_t mPassingStratNumPerRound;
  };

  ///
  /// The first Stepping policy (combining into double comparisons)
  ///
  template <class Decimal, typename TSearchAlgoBacktester>
  class SeedSteppingPolicy: SteppingPolicyBase<Decimal, TSearchAlgoBacktester>
  {
  public:
    SeedSteppingPolicy(const BacktestProcessor<Decimal, TSearchAlgoBacktester>& processingPolicy, size_t passingStratNumPerRound):
      SteppingPolicyBase<Decimal, TSearchAlgoBacktester> (processingPolicy, passingStratNumPerRound)
    {}

    virtual std::vector<std::unordered_map<unsigned int, std::valarray<Decimal>>> passes() override
    {
        //std::sort(this->mResults.begin(), this->mResults.end(), PfSorter<Decimal>::sort);

        this->mProcessingPolicy.template sortResults<PfSorter<Decimal>>();

        std::vector<std::unordered_map<unsigned int, std::valarray<Decimal>>> ret;
        for (const auto& tup: this->mProcessingPolicy.getResults())
         {
           int ind = std::get<2>(tup);
           ret.push_back(this->mProcessingPolicy.getStrategyMap()[ind]);
           if (ret.size() >= this->mPassingStratNumPerRound)
             break;
         }
        return ret;

    }

  };


  ///
  /// The stepping policy for all subsequent rounds
  ///
  template <class Decimal, typename TSearchAlgoBacktester>
  class SteppingPolicy: SteppingPolicyBase<Decimal, TSearchAlgoBacktester>
  {
  public:
    SteppingPolicy(const BacktestProcessor<Decimal, TSearchAlgoBacktester>& processingPolicy, size_t passingStratNumPerRound):
      SteppingPolicyBase<Decimal, TSearchAlgoBacktester> (processingPolicy, passingStratNumPerRound)
    {}

    virtual std::vector<std::unordered_map<unsigned int, std::valarray<Decimal>>> passes() override
    {
      //first 40% limit, with Profit Factor based sorting
      size_t to40 = static_cast<size_t>(this->mPassingStratNumPerRound*0.4);
      //sort
      this->mProcessingPolicy.template sortResults<PfSorter<Decimal>>();
      //fetch results
      std::vector<std::tuple<Decimal, unsigned int, int>>&
          results = this->mProcessingPolicy.getResults();
      std::unordered_map<int, std::unordered_map<unsigned int, std::valarray<Decimal>>>&
          stratMap = this->mProcessingPolicy.getStrategyMap();

      std::vector<std::unordered_map<unsigned int, std::valarray<Decimal>>> ret;
      for (const auto& tup: results)
        {
          if (ret.size() < to40)
            {
              int ind = std::get<2>(tup);
              ret.push_back(stratMap[ind]);
            }
          else
            {
              break;
            }
        }
      //second 40% - 80%, with Trade Weighted Profit Factor based sorting
      size_t to80 = to40*2;
      size_t to20 = static_cast<size_t>(to40 * 0.5);
      size_t remainder = (results.size() - to80);
      size_t everyNth = remainder/to20;

      //re-sort (this should update results)
      this->mProcessingPolicy.template sortResults<TwpfSorter<Decimal>>();

      size_t n = 0;
      for (const auto& tup: results)
        {
          if (ret.size() < to80)
            {
              int ind = std::get<2>(tup);
              const std::unordered_map<unsigned int, std::valarray<Decimal>> & strat = stratMap[ind];
              if (std::find(ret.begin(), ret.end(), strat) == ret.end())
                ret.push_back(strat);
            }
          else    //last 20% taken at equal intervals from the last passing element to the container's last(worst) element
            {
              n++;
              if (n % everyNth == 0)
                {
                  int ind = std::get<2>(tup);
                  const std::unordered_map<unsigned int, std::valarray<Decimal>> & strat = stratMap[ind];
                  if (std::find(ret.begin(), ret.end(), strat) == ret.end())
                    {
                      ret.push_back(strat);
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

  };
}


#endif // STEPPINGPOLICY_H
