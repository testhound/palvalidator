#ifndef SURVIVALPOLICY_H
#define SURVIVALPOLICY_H

#include "SteppingPolicy.h"

namespace mkc_searchalgo
{

  template <class Decimal, typename TSearchAlgoBacktester>
  class DefaultSurvivalPolicy
  {

  public:
    DefaultSurvivalPolicy(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy, Decimal survivalCriterion, Decimal targetStopRatio):
      mSurvivalCriterion(survivalCriterion),
      mTargetStopRatio(targetStopRatio),
      mProcessingPolicy(processingPolicy)
    {}

    void saveSurvivors()
    {
      //fetch results
      const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>&
          results = mProcessingPolicy->getResults();
      std::unordered_map<int, StrategyRepresentationType>&
          stratMap = mProcessingPolicy->getStrategyMap();

      for (const auto& tup: results)
        {
          const ResultStat<Decimal>& stat = std::get<0>(tup);

          if (stat.ProfitFactor > mSurvivalCriterion)
            {
              //Profitability requirement
              Decimal profRequirement = (stat.ProfitFactor)/ (stat.ProfitFactor + mTargetStopRatio);
              //Don't allow strategies with bad payoff ratio either (this means that when wrong, the market moved against them significantly)
              Decimal payoffRequirement = mTargetStopRatio * Decimal(0.95);

              if (stat.PALProfitability > profRequirement && stat.PayoffRatio > payoffRequirement)
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
    }

    std::vector<StrategyRepresentationType> getUniqueSurvivors(const std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& singlePA)
    {
      mUniqueOccurences.reserve(mSurvivors.size());
      std::vector<StrategyRepresentationType> ret;
      std::cout << "Survivors before removing duplicates: " << mSurvivors.size() << std::endl;
      Decimal sumTrades;
      for (const StrategyRepresentationType & strat: mSurvivors)
        {
            std::valarray<Decimal> occurences(DecimalConstants<Decimal>::DecimalOne, singlePA->getDateCount());
            //the multiplication
            for (unsigned int el: strat)
              occurences *= singlePA->getMappedElement(el);
            if (!findInVector(mUniqueOccurences, occurences))
              {
                mUniqueOccurences.push_back(occurences);
                ret.push_back(strat);
                sumTrades += occurences.sum() / Decimal(singlePA->getDateCount());
              }
        }
      Decimal avgTrades = (sumTrades * Decimal(singlePA->getDateCount()))  / Decimal(static_cast<unsigned int>(ret.size()));
      std::cout << "Survivors after removing duplicates: " << ret.size() << ", avg trades: " << avgTrades << std::endl;
      return ret;
    }

    const std::vector<StrategyRepresentationType>& getSurvivors() const {return mSurvivors;}
    size_t getNumSurvivors() const { return mSurvivors.size(); }

  private:
    Decimal mSurvivalCriterion;
    Decimal mTargetStopRatio;
    shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mProcessingPolicy;
    std::vector<StrategyRepresentationType> mSurvivors;
    std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>> mResults;
    std::vector<std::valarray<Decimal>> mUniqueOccurences;

  };

}


#endif // SURVIVALPOLICY_H