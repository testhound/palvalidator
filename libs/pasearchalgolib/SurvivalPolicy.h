#ifndef SURVIVALPOLICY_H
#define SURVIVALPOLICY_H

#include "SteppingPolicy.h"

namespace mkc_searchalgo
{

  template <class Decimal, typename TSearchAlgoBacktester>
  class DefaultSurvivalPolicy
  {

  public:
    DefaultSurvivalPolicy(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy, Decimal survivalCriterion):
      mSurvivalCriterion(survivalCriterion),
      mProcessingPolicy(processingPolicy)
    {}

    void saveSurvivors()
    {
      //fetch results
      const std::vector<std::tuple<Decimal, unsigned int, int>>&
          results = mProcessingPolicy->getResults();
      std::unordered_map<int, StrategyRepresentationType>&
          stratMap = mProcessingPolicy->getStrategyMap();

      for (const auto& tup: results)
        {
          const Decimal& pf = std::get<0>(tup);

          if (pf > mSurvivalCriterion)
            {
              int ind = std::get<2>(tup);
              StrategyRepresentationType & strat = stratMap[ind];
              //check for repeats (only here, as at this stage processing time is less pertinent)
              std::sort(strat.begin(), strat.end());
              if (!findInVector(mSurvivors, strat))
                {
                  mResults.push_back(tup);
                  mSurvivors.push_back(strat);
                }
            }
        }
    }

    const std::vector<StrategyRepresentationType>& getSurvivors() const {return mSurvivors;}
    size_t getNumSurvivors() const { return mSurvivors.size(); }

  private:
    Decimal mSurvivalCriterion;
    shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mProcessingPolicy;
    std::vector<StrategyRepresentationType> mSurvivors;
    std::vector<std::tuple<Decimal, unsigned int, int>> mResults;

  };

}


#endif // SURVIVALPOLICY_H
