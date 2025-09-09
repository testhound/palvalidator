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
#include "ExitPolicyAutoTuner.h" // TuningObjective, PolicyResult, ExitTunerOptions

namespace mkc_timeseries
{

  /**
   * @brief Immutable report for the joint tuner (failure-to-perform + breakeven).
   *
   * This collects the selected pair of bar-ages:
   *  - failureToPerformBars: bar index K at which the failure-to-perform test is evaluated
   *                          (t=0 is the first bar after entry).
   *  - breakevenActivationBars: bar index N from which breakeven becomes active (inclusive).
   *
   * It also includes:
   *  - Train/Test metrics for the combined policy at (K, N), measured via MetaExitCalibrator
   *    with earliest-exit-wins and (by default) “next bar’s open” fills for failure-to-perform.
   *  - The candidate grids examined for each dimension (useful for diagnostics).
   *
   * Design:
   *  - Ctor-only, no default constructor, no setters.
   *  - Getters expose the chosen bars, grids, and the aggregated PolicyResult metrics.
   */
  class JointExitTuningReportBase
  {
  public:
    JointExitTuningReportBase(int failureToPerformBars,
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

    /** @return Selected K (failure-to-perform evaluation bar). */
    int getFailureToPerformBars() const
    {
      return mFailureToPerformBars;
    }

    /** @return Selected N (breakeven activation bar). */
    int getBreakevenActivationBars() const
    {
      return mBreakevenActivationBars;
    }

    /** @return Train-set PolicyResult for the combined policy at (K, N). */
    const PolicyResult& getTrainCombined() const
    {
      return mTrainCombined;
    }

    /** @return Test-set PolicyResult for the combined policy at (K, N). */
    const PolicyResult& getTestCombined() const
    {
      return mTestCombined;
    }

    /** @return Candidate grid examined for failure-to-perform K. */
    const std::vector<int>& getFailureToPerformGrid() const
    {
      return mFailureToPerformGrid;
    }

    /** @return Candidate grid examined for breakeven activation N. */
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

  /**
   * @brief Typed (templated) wrapper for JointExitTuningReportBase.
   *
   * This preserves your templated API pattern while the payload itself
   * (PolicyResult and integer grids) is not dependent on Decimal at the type level.
   */
  template<class Decimal>
  class JointExitTuningReport : public JointExitTuningReportBase
  {
  public:
    JointExitTuningReport(int failureToPerformBars,
			  int breakevenActivationBars,
			  const PolicyResult& trainCombined,
			  const PolicyResult& testCombined,
			  std::vector<int> failureToPerformGrid,
			  std::vector<int> breakevenGrid)
      : JointExitTuningReportBase(failureToPerformBars,
				  breakevenActivationBars,
				  trainCombined,
				  testCombined,
				  std::move(failureToPerformGrid),
				  std::move(breakevenGrid))
    {
    }
  };

  /**
   * @brief Joint auto tuner that selects (failure-to-perform K, breakeven N) together.
   *
   * Responsibilities:
   *  1) Compute bar-age aggregates using MetaExitAnalytics::summarizeByBarAge(maxBarsToAnalyze).
   *  2) Propose candidate grids:
   *     - Failure-to-perform grid (K):
   *         Heuristic seed: first t where fracNonPositive >= fracNonPosHigh AND
   *         probTargetNextBar <= targetHazardLow; then add ±neighborSpan around it.
   *         If empty and useFullGridIfEmpty, fall back to [0..T-1].
   *     - Breakeven grid (N):
   *         Heuristic seed: earliest t where medianMfeRSoFar >= alphaMfeR; then add ±neighborSpan.
   *         If empty and useFullGridIfEmpty, fall back to {0,1,2}∩[0..T-1].
   *  3) Deterministic train/test split:
   *         Let n be number of trades in time order; cut = floor(n * trainFraction).
   *         Train = [0, cut); embargo = [cut, cut+embargoTrades); Test = [cut+embargo, n).
   *  4) Joint grid search on the fit set (train if non-empty, else full):
   *         Evaluate MetaExitCalibrator::evaluateCombinedPolicy(K, N, thresholdR, epsilonR, OpenOfKPlus1).
   *         Score by objective (AvgPnL_R or HitRate).
   *         Tie-breakers: higher score, then smaller (K+N), then higher hit-rate, then smaller K.
   *  5) Test metrics:
   *         Re-evaluate the chosen (K, N) on the held-out test set; if no test set, test==train.
   *
   * Conventions:
   *  - Time indexing: t=0 is the first bar AFTER the entry bar.
   *  - Failure-to-perform:
   *      Evaluated at Close[K]; default fill is Open[K+1] (OpenOfKPlus1).
   *      If K is exhausted for a path, overlay is a no-op (keeps recorded exit).
   *  - Breakeven:
   *      Armed from N onward; stop-first semantics inside each bar.
   *      If the path has no valid rTarget, breakeven is a no-op.
   */
  template<class Decimal>
  class ExitPolicyJointAutoTuner
  {
  public:
    /**
     * @brief Construct with explicit options.
     *
     * @param cph   ClosedPositionHistory over which to tune.
     * @param opts  ExitTunerOptions (immutable); only maxBarsToAnalyze is required by type,
     *              the rest have reasonable defaults.
     */
    ExitPolicyJointAutoTuner(const ClosedPositionHistory<Decimal>& cph,
			     const ExitTunerOptions<Decimal>& opts)
      : mCph(cph),
      mOpts(opts)
    {
    }

    /**
     * @brief Convenience constructor: only maxBarsToAnalyze is required; defaults for the rest.
     */
    ExitPolicyJointAutoTuner(const ClosedPositionHistory<Decimal>& cph,
			     int maxBarsToAnalyze)
      : mCph(cph),
      mOpts(ExitTunerOptions<Decimal>(maxBarsToAnalyze))
    {
    }

    /**
     * @brief Run the end-to-end joint tuning pipeline, returning an immutable report.
     *
     * Steps:
     *  1) Summarize bar-age behavior to seed grids.
     *  2) Split train/test deterministically with optional embargo.
     *  3) Jointly select (K, N) by grid search on the fit set using the configured objective.
     *  4) Recompute test metrics on held-out data (or reuse train if no test).
     */
    JointExitTuningReport<Decimal> tuneJoint();

    /**
     * @brief Alias of tuneJoint() for symmetry with other “exit policy” APIs.
     */
    JointExitTuningReport<Decimal> tuneExitPolicy()
    {
      return tuneJoint();
    }

  private:
    /**
     * @brief Deterministic train/test split by entry-time order with optional embargo.
     *
     * @param train Output train container.
     * @param test  Output test container.
     */
    void splitTrainTest(ClosedPositionHistory<Decimal>& train,
			ClosedPositionHistory<Decimal>& test) const;

    /**
     * @brief Build the failure-to-perform grid (candidate K values) from bar-age aggregates.
     *
     * Heuristic:
     *  - Choose a seed t where:
     *      fracNonPositive >= fracNonPosHigh AND
     *      probTargetNextBar <= targetHazardLow
     *    then add ±neighborSpan around the seed.
     *  - If empty and useFullGridIfEmpty is true, fall back to [0..T-1].
     */
    std::vector<int> proposeFailureToPerformGrid(const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Build the breakeven grid (candidate N values) from bar-age aggregates.
     *
     * Heuristic:
     *  - Choose the earliest t where:
     *      medianMfeRSoFar >= alphaMfeR
     *    then add ±neighborSpan around the seed.
     *  - If empty and useFullGridIfEmpty is true, fall back to {0,1,2}∩[0..T-1].
     */
    std::vector<int> proposeBreakevenGrid(const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Convert PolicyResult to scalar score according to the chosen objective.
     *
     * @param r PolicyResult to score.
     * @return  r.getAvgPnL_R() when objective==AvgPnL_R; otherwise r.getHitRate().
     */
    double score(const PolicyResult& r) const;

  private:
    const ClosedPositionHistory<Decimal>& mCph;
    ExitTunerOptions<Decimal> mOpts;
  };

  // ===================== Inline / Template Implementations =====================

  template<class Decimal>
  void ExitPolicyJointAutoTuner<Decimal>::splitTrainTest(ClosedPositionHistory<Decimal>& train,
							 ClosedPositionHistory<Decimal>& test) const
  {
    std::vector<std::shared_ptr<TradingPosition<Decimal>>> all;

    for (auto it = mCph.beginTradingPositions();
	 it != mCph.endTradingPositions();
	 ++it)
      {
	all.push_back(it->second);
      }

    const int n = static_cast<int>(all.size());

    if (n == 0)
      {
	return;
      }

    const int cut     = std::max(0, std::min(n, static_cast<int>(std::floor(n * mOpts.getTrainFraction()))));
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
  
  /**
   * @brief Build the failure-to-perform grid (candidate K values) from bar-age aggregates.
   *
   * Heuristic:
   *  - Choose a seed t where:
   *      fracNonPositive >= fracNonPosHigh AND
   *      probTargetNextBar <= targetHazardLow
   *    then add ±neighborSpan around the seed.
   *  - If empty and useFullGridIfEmpty is true, fall back to [0..T-1].
   */
  template<class Decimal>
  std::vector<int>
  ExitPolicyJointAutoTuner<Decimal>::proposeFailureToPerformGrid(const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());

    if (T == 0)
      {
	return {};
      }

    int    tPick     = -1;
    double bestScore = -1e9;

    for (int t = 0; t < T; ++t)
      {
	const double fracNonPos       = aggs[t].getFracNonPositive();
	const double probTargetNext   = aggs[t].getProbTargetNextBar();

	if (tPick < 0 &&
	    fracNonPos     >= mOpts.getFracNonPosHigh() &&
	    probTargetNext <= mOpts.getTargetHazardLow())
	  {
	    tPick = t;
	  }

	// Secondary signal used to break ties in seeding when no first-pass pick found
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
	  const int x = t + d;

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

  /**
   * @brief Build the breakeven grid (candidate N values) from bar-age aggregates.
   *
   * Heuristic:
   *  - Choose the earliest t where:
   *      medianMfeRSoFar >= alphaMfeR
   *    then add ±neighborSpan around the seed.
   *  - If empty and useFullGridIfEmpty is true, fall back to {0,1,2}∩[0..T-1].
   */
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
	  const int x = t + d;

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

  template<class Decimal>
  double ExitPolicyJointAutoTuner<Decimal>::score(const PolicyResult& r) const
  {
    switch (mOpts.getObjective())
      {
      case TuningObjective::HitRate:
	{
	  return r.getHitRate();
	}
      case TuningObjective::AvgPnL_R:
      default:
	{
	  return r.getAvgPnL_R();
	}
      }
  }

  /**
   * @brief Run the end-to-end joint tuning pipeline, returning an immutable report.
   *
   * Steps:
   *  1) Summarize bar-age behavior to seed grids.
   *  2) Split train/test deterministically with optional embargo.
   *  3) Jointly select (K, N) by grid search on the fit set using the configured objective.
   *  4) Recompute test metrics on held-out data (or reuse train if no test).
   */
  template<class Decimal>
  JointExitTuningReport<Decimal> ExitPolicyJointAutoTuner<Decimal>::tuneJoint()
  {
    // 1) Seed grids from analytics
    MetaExitAnalytics<Decimal> mex(mCph);
    const auto aggs = mex.summarizeByBarAge(mOpts.getMaxBarsToAnalyze());

    const auto failureToPerformGrid = proposeFailureToPerformGrid(aggs);
    const auto breakevenGrid        = proposeBreakevenGrid(aggs);

    // 2) Train/Test split
    ClosedPositionHistory<Decimal> train;
    ClosedPositionHistory<Decimal> test;
    splitTrainTest(train, test);

    const bool useFull = (test.beginTradingPositions() == test.endTradingPositions());
    const ClosedPositionHistory<Decimal>& fitCph = useFull ? mCph : train;

    // 3) Joint grid search over (failureToPerformBars, breakevenActivationBars)
    MetaExitCalibrator<Decimal> calFit(fitCph);

    double bestScore = -1e99;

    int bestFailureToPerformBars  = failureToPerformGrid.empty() ? 0 : failureToPerformGrid.front();
    int bestBreakevenActivationBars = breakevenGrid.empty()      ? 0 : breakevenGrid.front();

    PolicyResult bestTrainCombined(0.0, 0.0, 0.0, 0);

    for (int failureToPerformBars : failureToPerformGrid)
      {
	for (int breakevenActivationBars : breakevenGrid)
	  {
	    auto res = calFit.evaluateCombinedPolicy(
						     /*K*/ failureToPerformBars,
						     /*N*/ breakevenActivationBars,
						     mOpts.getThresholdR(),
						     mOpts.getEpsilonR(),
						     FailureExitFill::OpenOfKPlus1);

	    const double s   = score(res);
	    const int    sum = failureToPerformBars + breakevenActivationBars;
	    const int    bestSum = bestFailureToPerformBars + bestBreakevenActivationBars;

	    // Tie-breakers:
	    //  1) higher score
	    //  2) smaller (K + N)
	    //  3) higher hit-rate
	    //  4) smaller K (failureToPerformBars)
	    if (s > bestScore ||
		(s == bestScore && sum < bestSum) ||
		(s == bestScore && sum == bestSum && res.getHitRate() > bestTrainCombined.getHitRate()) ||
		(s == bestScore && sum == bestSum && res.getHitRate() == bestTrainCombined.getHitRate() &&
		 failureToPerformBars < bestFailureToPerformBars))
	      {
		bestScore = s;
		bestFailureToPerformBars  = failureToPerformBars;
		bestBreakevenActivationBars = breakevenActivationBars;
		bestTrainCombined = res;
	      }
	  }
      }

    // 4) Test metrics on held-out data (or reuse train metrics if no test split)
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
