// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef SURVIVALPOLICY_H
#define SURVIVALPOLICY_H

#include "SteppingPolicy.h"

namespace mkc_searchalgo
{


  template <class Decimal, typename TSearchAlgoBacktester>
  class MutualInfoSurvivalPolicy
  {

  public:
    MutualInfoSurvivalPolicy(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy,
                            std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& singlePA,
                          Decimal survivalCriterion, Decimal targetStopRatio, unsigned int maxConsecutiveLosersLimit,
                          const Decimal& palSafetyFactor):
      mSurvivalCriterion(survivalCriterion),
      mTargetStopRatio(targetStopRatio),
      mProcessingPolicy(processingPolicy),
      mMaxConsecutiveLosersLimit(maxConsecutiveLosersLimit),
      mPalProfitabilitySafetyFactor(palSafetyFactor),
      mMutualizer(processingPolicy, singlePA, "Survival")
    {}

    void filterSurvivors()
    {
      //fetch results
      const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>&
          results = mProcessingPolicy->getResults();
      std::unordered_map<int, StrategyRepresentationType>&
          stratMap = mProcessingPolicy->getStrategyMap();

      for (const auto& tup: results)
        {
          const ResultStat<Decimal>& stat = std::get<0>(tup);
          if (stat.MaxLosers > mMaxConsecutiveLosersLimit)
            continue;
          if (stat.ProfitFactor > mSurvivalCriterion)
            {
              //Profitability requirement
              Decimal profRequirement = (mSurvivalCriterion)/ (mSurvivalCriterion +  mPalProfitabilitySafetyFactor * mTargetStopRatio);
//              if (stat.PALProfitability < stat.WinPercent)
//                std::cout << "!!!Strategy found with Pal prof: " << stat.PALProfitability << " and win %: " << stat.WinPercent << std::endl;

              if (stat.PALProfitability > profRequirement && stat.WinPercent > profRequirement)
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

    std::vector<StrategyRepresentationType> getUniqueSurvivors()
    {
      std::cout << "Sorting survivors." << std::endl;
      std::sort(mResults.begin(), mResults.end(), Sorters::PALProfitabilitySorter<Decimal>::sort);
      std::cout << "Surival MaxRelMinRed Algorithm..." << std::endl;
      mMutualizer.getMaxRelMinRed2(mResults, mResults.size(), 0.0, 2.0, 0.5);
      return mMutualizer.getSelectedStrategies();
    }

    std::vector<ResultStat<Decimal>> getUniqueStatistics()
    {
      return mMutualizer.getSelectedStatistics();
    }

    const std::vector<StrategyRepresentationType>& getSurvivors() const {return mSurvivors;}

    size_t getNumSurvivors() const { return mSurvivors.size(); }

    void clearRound()
    {
      mSurvivors.clear();
      mSurvivors.shrink_to_fit();
      mResults.clear();
      mResults.shrink_to_fit();
    }

  private:
    Decimal mSurvivalCriterion;
    Decimal mTargetStopRatio;
    shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mProcessingPolicy;
    std::vector<StrategyRepresentationType> mSurvivors;
    std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>> mResults;
    //std::vector<std::valarray<Decimal>> mUniqueOccurences;
    unsigned int mMaxConsecutiveLosersLimit;
    Decimal mPalProfitabilitySafetyFactor;
    ValarrayMutualizer<Decimal, TSearchAlgoBacktester> mMutualizer;

  };

  template <class Decimal, typename TSearchAlgoBacktester>
  class DefaultSurvivalPolicy
  {

  public:
    DefaultSurvivalPolicy(const shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>>& processingPolicy,
                          std::shared_ptr<UniqueSinglePAMatrix<Decimal, std::valarray<Decimal>>>& singlePA,
                        Decimal survivalCriterion, Decimal targetStopRatio, unsigned int maxConsecutiveLosersLimit,
                        const Decimal& palSafetyFactor):
      mSurvivalCriterion(survivalCriterion),
      mTargetStopRatio(targetStopRatio),
      mProcessingPolicy(processingPolicy),
      mMaxConsecutiveLosersLimit(maxConsecutiveLosersLimit),
      mPalProfitabilitySafetyFactor(palSafetyFactor)
    {}

    void filterSurvivors()
    {
      //fetch results
      const std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>>&
          results = mProcessingPolicy->getResults();
      std::unordered_map<int, StrategyRepresentationType>&
          stratMap = mProcessingPolicy->getStrategyMap();

      for (const auto& tup: results)
        {
          const ResultStat<Decimal>& stat = std::get<0>(tup);
          if (stat.MaxLosers > mMaxConsecutiveLosersLimit)
            continue;
          if (stat.ProfitFactor > mSurvivalCriterion)
            {
              //Profitability requirement
              Decimal profRequirement = (mSurvivalCriterion)/ (mSurvivalCriterion +  mPalProfitabilitySafetyFactor * mTargetStopRatio);
              //Don't allow strategies with bad payoff ratio either (this means that when wrong, the market moved against them significantly)
//              std::cout << "MaxLosers: " << stat.MaxLosers << ", Limit: " <<  mMaxConsecutiveLosersLimit <<  ", PF: " << stat.ProfitFactor << ", Payoff: " << stat.PayoffRatio << ", profRequirement: " << profRequirement <<
//                        << ", PALProf: " << stat.PALProfitability << ", criterion: " << mSurvivalCriterion << ", Safety: " << mPalProfitabilitySafetyFactor << ", TS ratio: " << mTargetStopRatio << std::endl;
              if (stat.PALProfitability > profRequirement)
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

    std::vector<StrategyRepresentationType> getUniqueSurvivors()
    {
      std::vector<StrategyRepresentationType> ret;
      std::cout << "Survivors before removing duplicates: " << mSurvivors.size() << std::endl;
      for (StrategyRepresentationType& strat: mSurvivors)
        {
          std::sort(strat.begin(), strat.end());

            if (!findInVector(ret, strat))
                ret.push_back(strat);

        }
      std::cout << "Survivors after removing duplicates: " << ret.size() <<  std::endl;
      return ret;
    }

    const std::vector<StrategyRepresentationType>& getSurvivors() const {return mSurvivors;}

    size_t getNumSurvivors() const { return mSurvivors.size(); }

    void clearRound()
    {
      mSurvivors.clear();
      mSurvivors.shrink_to_fit();
      mResults.clear();
      mResults.shrink_to_fit();
    }

  private:
    Decimal mSurvivalCriterion;
    Decimal mTargetStopRatio;
    shared_ptr<BacktestProcessor<Decimal, TSearchAlgoBacktester>> mProcessingPolicy;
    std::vector<StrategyRepresentationType> mSurvivors;
    std::vector<std::tuple<ResultStat<Decimal>, unsigned int, int>> mResults;
    //std::vector<std::valarray<Decimal>> mUniqueOccurences;
    unsigned int mMaxConsecutiveLosersLimit;
    Decimal mPalProfitabilitySafetyFactor;

  };

}


#endif // SURVIVALPOLICY_H
