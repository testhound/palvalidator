#pragma once

#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>

#include "ClosedPositionHistory.h"
#include "MetaExitAnalytics.h"
#include "MetaExitCalibrator.h"
#include "DecimalConstants.h"
#include "ExitPolicyAutoTuner.h" // for TuningObjective, PolicyResult, ExitTunerOptions

namespace mkc_timeseries
{
  // Immutable report of a joint (failure-to-perform, breakeven) tuning run
  class JointExitTuningReportBase
  {
  public:
    JointExitTuningReportBase(
			      int failureToPerformBars,
			      int breakevenActivationBars,
			      const PolicyResult& trainCombined,
			      const PolicyResult& testCombined,
			      std::vector<int> failureToPerformGrid,
			      std::vector<int> breakevenGrid)
      : mFailureToPerformBars(failureToPerformBars)
      , mBreakevenActivationBars(breakevenActivationBars)
      , mTrainCombined(trainCombined)
      , mTestCombined(testCombined)
      , mFailureToPerformGrid(std::move(failureToPerformGrid))
      , mBreakevenGrid(std::move(breakevenGrid))
    {
    }

    int getFailureToPerformBars() const
    {
      return mFailureToPerformBars;
    }

    int getBreakevenActivationBars() const
    {
      return mBreakevenActivationBars;
    }

    const PolicyResult& getTrainCombined() const
    {
      return mTrainCombined;
    }

    const PolicyResult& getTestCombined() const
    {
      return mTestCombined;
    }

    const std::vector<int>& getFailureToPerformGrid() const
    {
      return mFailureToPerformGrid;
    }

    const std::vector<int>& getBreakevenGrid() const
    {
      return mBreakevenGrid;
    }

  private:
    int mFailureToPerformBars;
    int mBreakevenActivationBars;

    PolicyResult mTrainCombined;
    PolicyResult mTestCombined;

    std::vector<int> mFailureToPerformGrid;
    std::vector<int> mBreakevenGrid;
  };

  template<class Decimal>
  class JointExitTuningReport : public JointExitTuningReportBase
  {
  public:
    JointExitTuningReport(
			  int failureToPerformBars,
			  int breakevenActivationBars,
			  const PolicyResult& trainCombined,
			  const PolicyResult& testCombined,
			  std::vector<int> failureToPerformGrid,
			  std::vector<int> breakevenGrid)
      : JointExitTuningReportBase(
				  failureToPerformBars,
				  breakevenActivationBars,
				  trainCombined,
				  testCombined,
				  std::move(failureToPerformGrid),
				  std::move(breakevenGrid))
    {
    }
  };

  template<class Decimal>
  class ExitPolicyJointAutoTuner
  {
  public:
    ExitPolicyJointAutoTuner(const ClosedPositionHistory<Decimal>& cph,
			     const ExitTunerOptions<Decimal>& opts)
      : mCph(cph),
	mOpts(opts)
    {}

    ExitPolicyJointAutoTuner(const ClosedPositionHistory<Decimal>& cph,
			     int maxBarsToAnalyze)
      : mCph(cph),
	mOpts(ExitTunerOptions<Decimal>(maxBarsToAnalyze))
    {}
    
    JointExitTuningReport<Decimal> tuneJoint();
    
    JointExitTuningReport<Decimal> tuneExitPolicy()
    {
      return tuneJoint();
    }

  private:
    void splitTrainTest(ClosedPositionHistory<Decimal>& train,
			ClosedPositionHistory<Decimal>& test) const;

    std::vector<int> proposeFailureToPerformGrid(
						 const std::vector<BarAgeAggregate>& aggs) const;

    std::vector<int> proposeBreakevenGrid(
					  const std::vector<BarAgeAggregate>& aggs) const;

    double score(const PolicyResult& r) const;

  private:
    const ClosedPositionHistory<Decimal>& mCph;
    ExitTunerOptions<Decimal> mOpts;
  };

  template<class Decimal>
  void ExitPolicyJointAutoTuner<Decimal>::splitTrainTest(
							 ClosedPositionHistory<Decimal>& train,
							 ClosedPositionHistory<Decimal>& test) const
  {
    std::vector<std::shared_ptr<TradingPosition<Decimal>>> all;
    for (auto it = mCph.beginTradingPositions(); it != mCph.endTradingPositions(); ++it)
      {
	all.push_back(it->second);
      }

    const int n = static_cast<int>(all.size());
    if (n == 0)
      {
	return;
      }

    const int cut = std::max(0, std::min(n, static_cast<int>(std::floor(n * mOpts.getTrainFraction()))));
    const int embargo = std::max(0, std::min(mOpts.getEmbargoTrades(), n));

    for (int i = 0; i < cut; ++i)
      {
	train.addClosedPosition(all[i]);
      }
    for (int i = cut + embargo; i < n; ++i)
      {
	test.addClosedPosition(all[i]);
      }
  }

  // -------------------- Grid proposals (same heuristics as 1-D tuner) --------------------
  template<class Decimal>
  std::vector<int> ExitPolicyJointAutoTuner<Decimal>::proposeFailureToPerformGrid(
										  const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());
    if (T == 0)
      {
	return {};
      }

    int tPick = -1;
    double bestScore = -1e9;

    for (int t = 0; t < T; ++t)
      {
	const double fracNonPos = aggs[t].getFracNonPositive();
	const double probTargetNext = aggs[t].getProbTargetNextBar();

	if (tPick < 0 &&
	    fracNonPos >= mOpts.getFracNonPosHigh() &&
	    probTargetNext <= mOpts.getTargetHazardLow())
	  {
	    tPick = t;
	  }

	const double s = fracNonPos - probTargetNext;
	if (s > bestScore)
	  {
	    bestScore = s;
	  }
      }

    std::vector<int> grid;

    auto pushNeighbor = [&](int t)
    {
      for (int d = -mOpts.getNeighborSpan(); d <= mOpts.getNeighborSpan(); ++d)
	{
	  int x = t + d;
	  if (0 <= x && x < T)
	    {
	      grid.push_back(x);
	    }
	}
    };

    if (tPick >= 0)
      {
	pushNeighbor(tPick);
      }

    if (grid.empty() && mOpts.getUseFullGridIfEmpty())
      {
	grid.resize(T);
	std::iota(grid.begin(), grid.end(), 0);
      }

    std::sort(grid.begin(), grid.end());
    grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
    return grid;
  }

  template<class Decimal>
  std::vector<int>
  ExitPolicyJointAutoTuner<Decimal>::proposeBreakevenGrid(const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());
    if (T == 0)
      {
	return {};
      }

    int tPick = -1;

    for (int t = 0; t < T; ++t)
      {
	const double medMfeR = aggs[t].getMedianMfeRSoFar();
	if (!std::isnan(medMfeR) && medMfeR >= mOpts.getAlphaMfeR())
	  {
	    tPick = t;
	    break;
	  }
      }

    std::vector<int> grid;

    auto pushNeighbor = [&](int t)
    {
      for (int d = -mOpts.getNeighborSpan(); d <= mOpts.getNeighborSpan(); ++d)
	{
	  int x = t + d;
	  if (0 <= x && x < T)
	    {
	      grid.push_back(x);
	    }
	}
    };

    if (tPick >= 0)
      {
	pushNeighbor(tPick);
      }

    if (grid.empty() && mOpts.getUseFullGridIfEmpty())
      {
	for (int x : {0, 1, 2})
	  {
	    if (x < T)
	      {
		grid.push_back(x);
	      }
	  }
      }

    std::sort(grid.begin(), grid.end());
    grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
    return grid;
  }

  // -------------------- Scoring --------------------
  template<class Decimal>
  double ExitPolicyJointAutoTuner<Decimal>::score(const PolicyResult& r) const
  {
    switch (mOpts.getObjective())
      {
      case TuningObjective::HitRate:
	return r.getHitRate();
      case TuningObjective::AvgPnL_R:
      default:
	return r.getAvgPnL_R();
      }
  }

  // -------------------- Orchestrator (joint grid search) --------------------
  template<class Decimal>
  JointExitTuningReport<Decimal> ExitPolicyJointAutoTuner<Decimal>::tuneJoint()
  {
    // 1) Seed grids from analytics
    MetaExitAnalytics<Decimal> mex(mCph);
    const auto aggs = mex.summarizeByBarAge(mOpts.getMaxBarsToAnalyze());

    const auto failureToPerformGrid = proposeFailureToPerformGrid(aggs);
    const auto breakevenGrid = proposeBreakevenGrid(aggs);

    // 2) Train/Test split
    ClosedPositionHistory<Decimal> train;
    ClosedPositionHistory<Decimal> test;
    splitTrainTest(train, test);

    const bool useFull = (test.beginTradingPositions() == test.endTradingPositions());
    const ClosedPositionHistory<Decimal>& fitCph = useFull ? mCph : train;

    // 3) Grid search over (failureToPerformBars, breakevenActivationBars)
    MetaExitCalibrator<Decimal> calFit(fitCph);
    double bestScore = -1e99;

    int bestFailureToPerformBars = failureToPerformGrid.empty() ? 0 : failureToPerformGrid.front();
    int bestBreakevenActivationBars = breakevenGrid.empty() ? 0 : breakevenGrid.front();
    PolicyResult bestTrainCombined(0.0, 0.0, 0.0, 0);

    for (int failureToPerformBars : failureToPerformGrid)
      {
	for (int breakevenActivationBars : breakevenGrid)
	  {
	    auto res = calFit.evaluateCombinedPolicy(
						     /*failureToPerformBars*/ failureToPerformBars,
						     /*breakevenActivationBars*/ breakevenActivationBars,
						     mOpts.getThresholdR(),
						     mOpts.getEpsilonR(),
						     FailureExitFill::OpenOfKPlus1);

	    const double s = score(res);

	    // Tie-breaks: higher score, then smaller (F+B), then higher hit-rate, then smaller F
	    const int fbSum = failureToPerformBars + breakevenActivationBars;
	    const int bestSum = bestFailureToPerformBars + bestBreakevenActivationBars;

	    if (s > bestScore ||
		(s == bestScore && fbSum < bestSum) ||
		(s == bestScore && fbSum == bestSum && res.getHitRate() > bestTrainCombined.getHitRate()) ||
		(s == bestScore && fbSum == bestSum && res.getHitRate() == bestTrainCombined.getHitRate() &&
		 failureToPerformBars < bestFailureToPerformBars))
	      {
		bestScore = s;
		bestFailureToPerformBars = failureToPerformBars;
		bestBreakevenActivationBars = breakevenActivationBars;
		bestTrainCombined = res;
	      }
	  }
      }

    // 4) Test metrics
    PolicyResult testCombined(0.0, 0.0, 0.0, 0);

    if (useFull)
      {
	testCombined = bestTrainCombined;
      }
    else
      {
	MetaExitCalibrator<Decimal> calTest(test);
	testCombined = calTest.evaluateCombinedPolicy(
						      bestFailureToPerformBars,
						      bestBreakevenActivationBars,
						      mOpts.getThresholdR(),
						      mOpts.getEpsilonR(),
						      FailureExitFill::OpenOfKPlus1);
      }

    // 5) Immutable report
    return JointExitTuningReport<Decimal>(
					  bestFailureToPerformBars,
					  bestBreakevenActivationBars,
					  bestTrainCombined,
					  testCombined,
					  failureToPerformGrid,
					  breakevenGrid);
  }
} // namespace mkc_timeseries

