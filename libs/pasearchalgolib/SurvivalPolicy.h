#ifndef SURVIVALPOLICY_H
#define SURVIVALPOLICY_H

#include "SteppingPolicy.h"

namespace mkc_searchalgo
{

  template <class Decimal>
  class DefaultSurvivalPolicy
  {

  public:
    DefaultSurvivalPolicy(Decimal survivalCriterion):
      mSurvivalCriterion(survivalCriterion)
    {}

    template <class TSearchAlgoBacktester>
    std::vector<std::vector<unsigned int>> getSurvivors(const SteppingPolicyBase<Decimal, TSearchAlgoBacktester>* stepResults)
    {
      std::vector<std::vector<unsigned int>> ret;

      const std::vector<std::tuple<Decimal, unsigned int, int>>& results = stepResults->getResults();
      const std::unordered_map<int, std::unordered_map<unsigned int, std::valarray<Decimal>>>& stratMap = stepResults->getStrategyMap();
      for (const auto& tup: results)
        {
          const Decimal& pf = std::get<0>(tup);
          if (pf > mSurvivalCriterion)
            {
              int ind = std::get<2>(tup);
              const std::unordered_map<unsigned int, std::valarray<Decimal>>& mapped = stratMap[ind];
              std::vector<unsigned int> stratIds;
              stratIds.reserve(mapped.size());
              for (const std::pair<unsigned int, std::valarray<Decimal>> & pair: mapped)
                {
                  stratIds.push_back(pair.first);
                }
              ret.push_back(stratIds);
            }
        }
      return ret;
    }

  private:
    Decimal mSurvivalCriterion;

  };

}


#endif // SURVIVALPOLICY_H
