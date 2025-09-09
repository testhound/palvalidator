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
   * @brief Immutable report for the joint (2-D) auto-tuner.
   *
   * This class holds the result of a joint optimization for a failure-to-perform
   * rule (at bar K) and a breakeven rule (from bar N). The key takeaway is that
   * the performance metrics (`mTrainCombined`, `mTestCombined`) reflect the
   * **interactive effect** of applying both rules simultaneously to a set of trades,
   * with the earliest exit taking precedence.
   *
   * It collects the selected pair of parameters:
   * - failureToPerformBars: The bar index K for the performance check.
   * - breakevenActivationBars: The bar index N from which the breakeven stop is armed.
   *
   * It also includes:
   * - Train/Test metrics for the combined (K, N) policy.
   * - The candidate grids that were searched for each parameter.
   *
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
   * @brief Joint auto tuner that selects an optimal (K, N) pair for a combined exit policy.
   *
   * This tuner performs a 2-dimensional grid search to find the best combination of a
   * failure-to-perform bar (K) and a breakeven activation bar (N).
   *
   * **Key Distinction:** Unlike a 1-D tuner that optimizes K and N independently, this
   * class evaluates each (K, N) pair as a single, combined policy. This is crucial
   * because the two parameters can interact; for example, an early breakeven stop (low N)
   * might make a later failure-to-perform check (high K) more effective, or vice-versa.
   * This joint search is designed to capture these interaction effects.
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
     * This is the main entry point for the joint tuning process. It executes the following steps:
     * 1) Analyzes the entire trade history to generate bar-age statistics.
     * 2) Uses heuristics to propose candidate grids for K and N based on the statistics.
     * 3) Splits the historical data into training and testing sets, with an optional embargo period.
     * 4) Performs an exhaustive grid search on the training set, evaluating every (K, N) pair
     * to find the combination that maximizes the scoring objective.
     * 5) Evaluates the single best (K, N) pair on the test set to measure out-of-sample performance.
     * 6) Returns a report containing the selected parameters and all performance metrics.
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
     * Heuristic Rationale:
     * This heuristic seeks an ideal time to check for "failure-to-perform". It looks for a
     * bar 't' where a significant portion of trades have stalled (high `fracNonPositive`)
     * but the immediate chance of hitting the profit target has diminished (low `probTargetNextBar`).
     * The first bar satisfying these conditions is used as a seed for the grid search.
     * ...
     */
    std::vector<int> proposeFailureToPerformGrid(const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Build the breakeven grid (candidate N values) from bar-age aggregates.
     *
     * Heuristic Rationale:
     * This heuristic seeks to activate a breakeven stop only after a trade has shown
     * meaningful progress. It identifies the earliest bar 't' where the median trade
     * has achieved a significant favorable move (`medianMfeRSoFar >= alphaMfeR`).
     * Arming the stop around this point aims to protect gains without being premature.
     * ...
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
