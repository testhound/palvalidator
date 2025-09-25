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

namespace mkc_timeseries
{

  /**
   * @brief Objective used to score exit policies during tuning.
   *
   * Semantics:
   *  - AvgPnL_R: maximize the average PnL expressed in R units (or scaled currency via fallback).
   *  - HitRate : maximize the fraction of winning trades (PnL_R > 0).
   */
  enum class TuningObjective
    {
      AvgPnL_R,
      HitRate,
      PnLPerBar,
    };

  /**
   * @brief Immutable, ctor-only options for exit-policy tuning.
   *
   * Design:
   *  - No default constructor; one constructor with defaulted trailing parameters.
   *  - The only required argument is @p maxBarsToAnalyze (how many bar ages t to consider).
   *  - All other parameters have sensible defaults. No setters; use getters only.
   *
   * Key parameters:
   *  - maxBarsToAnalyze : analyze bar ages t = 0..(maxBarsToAnalyze-1)
   *                       where t=0 denotes the first bar after the entry bar.
   *  - trainFraction    : fraction of trades in train split (0..1]. Remainder (after embargo) is test.
   *  - embargoTrades    : number of trades to skip between train and test to reduce leakage.
   *  - thresholdR       : failure-to-perform threshold in R units (commonly 0.0).
   *  - epsilonR         : breakeven offset in R units (0.0 = exact entry price).
   *  - fracNonPosHigh   : heuristic gate for K-grid seeding (high non-positive fraction).
   *  - targetHazardLow  : heuristic gate for K-grid seeding (low probability of hitting target next bar).
   *  - alphaMfeR        : heuristic gate for N-grid seeding (min median MFE_R so far).
   *  - neighborSpan     : add ±neighborSpan around the seed bar to the candidate grid.
   *  - useFullGridIfEmpty: if seeding yields nothing, fall back to analyzing all (for K)
   *                        or a small {0,1,2} slice (for N).
   *  - objective        : scoring objective (AvgPnL_R by default).
   *
   * @tparam Decimal Fixed-point/decimal type used by the backtester for price math.
   */
  template<class Decimal>
  class ExitTunerOptions
  {
  public:
    ExitTunerOptions(int    maxBarsToAnalyze,
		     double trainFraction         = 0.70,
		     int    embargoTrades         = 5,
		     const Decimal& thresholdR    = DecimalConstants<Decimal>::DecimalZero,
		     const Decimal& epsilonR      = DecimalConstants<Decimal>::DecimalZero,
		     double fracNonPosHigh        = 0.65,
		     double targetHazardLow       = 0.20,
		     double alphaMfeR             = 0.33,
		     int    neighborSpan          = 1,
		     bool   useFullGridIfEmpty    = true,
		     TuningObjective objective    = TuningObjective::AvgPnL_R)
    : mMaxBarsToAnalyze(maxBarsToAnalyze)
    , mTrainFraction(trainFraction)
    , mEmbargoTrades(embargoTrades)
    , mThresholdR(thresholdR)
    , mEpsilonR(epsilonR)
    , mFracNonPosHigh(fracNonPosHigh)
    , mTargetHazardLow(targetHazardLow)
    , mAlphaMfeR(alphaMfeR)
    , mNeighborSpan(neighborSpan)
    , mUseFullGridIfEmpty(useFullGridIfEmpty)
    , mObjective(objective)
    {
    }

    // Return a copy with a different objective
    ExitTunerOptions withObjective(TuningObjective obj) const
    {
      ExitTunerOptions copy(*this);
      copy.mObjective = obj;
      return copy;
    }

    // Convenience you requested: set objective to PnLPerBar (returns a copy)
    ExitTunerOptions usePnLPerBar() const
    {
      return withObjective(TuningObjective::PnLPerBar);
    }
    
    /** @return number of bar ages to analyze (t in [0, maxBarsToAnalyze-1]) */
    int getMaxBarsToAnalyze() const
    {
      return mMaxBarsToAnalyze;
    }

    /** @return train fraction (0..1] */
    double getTrainFraction() const
    {
      return mTrainFraction;
    }

    /** @return number of embargoed trades between train and test */
    int getEmbargoTrades() const
    {
      return mEmbargoTrades;
    }

    /** @return failure-to-perform threshold in R units */
    const Decimal& getThresholdR() const
    {
      return mThresholdR;
    }

    /** @return breakeven offset in R units (0=entry) */
    const Decimal& getEpsilonR() const
    {
      return mEpsilonR;
    }

    /** @return heuristic gate for high fraction non-positive at bar age t */
    double getFracNonPosHigh() const
    {
      return mFracNonPosHigh;
    }

    /** @return heuristic gate for low probability of target hit next bar */
    double getTargetHazardLow() const
    {
      return mTargetHazardLow;
    }

    /** @return minimum median MFE_R so far used in N-grid seeding */
    double getAlphaMfeR() const
    {
      return mAlphaMfeR;
    }

    /** @return neighborhood half-width added around seed t in K/N grids */
    int getNeighborSpan() const
    {
      return mNeighborSpan;
    }

    /** @return whether to use fallback grids when seeding is empty */
    bool getUseFullGridIfEmpty() const
    {
      return mUseFullGridIfEmpty;
    }

    /** @return tuning objective (AvgPnL_R or HitRate) */
    TuningObjective getObjective() const
    {
      return mObjective;
    }

  private:
    int mMaxBarsToAnalyze;
    double mTrainFraction;
    int mEmbargoTrades;
    Decimal mThresholdR;
    Decimal mEpsilonR;
    double mFracNonPosHigh;
    double mTargetHazardLow;
    double mAlphaMfeR;
    int mNeighborSpan;
    bool mUseFullGridIfEmpty;
    TuningObjective mObjective;
  };

  /**
   * @brief Immutable report for the 1-D auto tuner.
   *
   * Definitions:
   *  - K = failure-to-perform evaluation bar (t=K; t=0 is first bar after entry).
   *  - N = breakeven activation bar (armed from t=N inclusive).
   *
   * Contents:
   *  - Train/Test metrics for the selected K and N (independently tuned).
   *  - Candidate grids examined for each dimension.
   *  - Friendly aliases for K and N via getFailureToPerformBars() / getBreakevenActivationBars().
   */
  class ExitTuningReportBase
  {
  public:
    ExitTuningReportBase(
			 int K,
			 int N,
			 const PolicyResult& trainK,
			 const PolicyResult& testK,
			 const PolicyResult& trainN,
			 const PolicyResult& testN,
			 std::vector<int> Kgrid,
			 std::vector<int> Ngrid)
      : mK(K)
      , mN(N)
      , mTrainK(trainK)
      , mTestK(testK)
      , mTrainN(trainN)
      , mTestN(testN)
      , mKgrid(std::move(Kgrid))
      , mNgrid(std::move(Ngrid))
    {
    }

    /** @return selected K (failure-to-perform bar) */
    int getK() const
    {
      return mK;
    }

    /** @return selected N (breakeven activation bar) */
    int getN() const
    {
      return mN;
    }

    /** @return alias for selected failure-to-perform bar (K) */
    int getFailureToPerformBars() const
    {
      return mK;
    }

    /** @return alias for selected breakeven activation bar (N) */
    int getBreakevenActivationBars() const
    {
      return mN;
    }

    /** @return train metrics for K overlay */
    const PolicyResult& getTrainK() const
    {
      return mTrainK;
    }

    /** @return test metrics for K overlay */
    const PolicyResult& getTestK() const
    {
      return mTestK;
    }

    /** @return train metrics for N overlay */
    const PolicyResult& getTrainN() const
    {
      return mTrainN;
    }

    /** @return test metrics for N overlay */
    const PolicyResult& getTestN() const
    {
      return mTestN;
    }

    /** @return candidate grid examined for K */
    const std::vector<int>& getKgrid() const
    {
      return mKgrid;
    }

    /** @return candidate grid examined for N */
    const std::vector<int>& getNgrid() const
    {
      return mNgrid;
    }

  private:
    int mK, mN;
    PolicyResult mTrainK, mTestK, mTrainN, mTestN;
    std::vector<int> mKgrid, mNgrid;
  };

  /**
   * @brief Typed (templated) wrapper for ExitTuningReportBase (kept for API symmetry).
   */
  template<class Decimal>
  class ExitTuningReport : public ExitTuningReportBase
  {
  public:
    ExitTuningReport(
		     int K,
		     int N,
		     const PolicyResult& trainK,
		     const PolicyResult& testK,
		     const PolicyResult& trainN,
		     const PolicyResult& testN,
		     std::vector<int> Kgrid,
		     std::vector<int> Ngrid)
      : ExitTuningReportBase(
			     K,
			     N,
			     trainK,
			     testK,
			     trainN,
			     testN,
			     std::move(Kgrid),
			     std::move(Ngrid))
    {
    }
  };

  /**
   * @brief One-dimensional auto tuner for exit overlays (K for failure-to-perform, N for breakeven).
   *
   * Responsibilities:
   *  1) Summarize bar-age behavior via MetaExitAnalytics::summarizeByBarAge(maxBarsToAnalyze).
   *  2) Seed candidate grids for K and N:
   *     - K-grid: choose a seed t where fracNonPositive >= fracNonPosHigh AND
   *               probTargetNextBar <= targetHazardLow; expand by ±neighborSpan.
   *               If empty and useFullGridIfEmpty, fall back to full [0..T-1].
   *     - N-grid: choose the earliest t where medianMfeRSoFar >= alphaMfeR; expand by ±neighborSpan.
   *               If empty and useFullGridIfEmpty, fall back to a small {0,1,2}∩[0..T-1].
   *  3) Deterministic train/test split (by entry-time order):
   *     - Let n be number of trades; cut = floor(n * trainFraction).
   *     - Train = [0, cut); embargo = [cut, cut+embargoTrades); Test = [cut+embargo, n).
   *  4) Select K and N independently on the fit set (train or full if no test):
   *     - For K: evaluateFailureToPerformBars(K, thresholdR, OpenOfKPlus1).
   *     - For N: evaluateBreakevenAfterBars(N, epsilonR).
   *     - Score using the objective; tie-break with smaller K/N, then higher hit-rate.
   *  5) Compute test-set metrics for the chosen K and N via MetaExitCalibrator on the held-out set.
   *
   * Conventions:
   *  - t=0 denotes the first bar after entry throughout.
   *  - Failure-to-perform is evaluated at Close[K]; default execution fills at Open[K+1].
   *  - Breakeven uses stop-first semantics; see MetaExitCalibrator documentation.
   */
  template<class Decimal>
  class ExitPolicyAutoTuner
  {
  public:
    /**
     * @brief Construct with explicit options.
     *
     * @param cph  ClosedPositionHistory over which to tune.
     * @param opts Tuning options (immutable).
     */
    ExitPolicyAutoTuner(
			const ClosedPositionHistory<Decimal>& cph,
			const ExitTunerOptions<Decimal>& opts)
      : mClosedPositionHistory(cph)
      , mOpts(opts)
    {
    }

    /**
     * @brief Convenience constructor: only @p maxBarsToAnalyze is required; other opts use defaults.
     */
    ExitPolicyAutoTuner(
			const ClosedPositionHistory<Decimal>& cph,
			int maxBarsToAnalyze)
      : mClosedPositionHistory(cph)
      , mOpts(ExitTunerOptions<Decimal>(maxBarsToAnalyze))
    {
    }

    /**
     * @brief Run the end-to-end 1-D tuning pipeline.
     *
     * Steps:
     *  1) Compute bar-age aggregates (analytics).
     *  2) Build K/N candidate grids from aggregates and options.
     *  3) Split train/test deterministically with optional embargo.
     *  4) Select K and N independently on the fit set, scoring by objective.
     *  5) Evaluate chosen K/N on the test set (or full set if no test).
     *
     * @return ExitTuningReport with selections, grids, and train/test metrics.
     */
    ExitTuningReport<Decimal> tune();

  private:
    /**
     * @brief Deterministic train/test split by entry-time order with optional embargo.
     *
     * @param train Output train set.
     * @param test  Output test set.
     */
    void splitTrainTest(
			ClosedPositionHistory<Decimal>& train,
			ClosedPositionHistory<Decimal>& test) const;

    /**
     * @brief Build the K candidate grid from bar-age aggregates.
     *
     * Heuristic:
     *  - Choose a seed t where fracNonPositive >= fracNonPosHigh AND
     *    probTargetNextBar <= targetHazardLow; add ±neighborSpan around t.
     *  - If empty and useFullGridIfEmpty is true, use [0..T-1].
     */
    std::vector<int> proposeKGrid(
				  const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Build the N candidate grid from bar-age aggregates.
     *
     * Heuristic:
     *  - Choose the earliest t where medianMfeRSoFar >= alphaMfeR; add ±neighborSpan.
     *  - If empty and useFullGridIfEmpty is true, use {0,1,2}∩[0..T-1].
     */
    std::vector<int> proposeNGrid(
				  const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Select best K on the fit set (train or full), using the configured objective.
     *
     * Evaluation:
     *  - Uses MetaExitCalibrator::evaluateFailureToPerformBars(K, thresholdR, OpenOfKPlus1).
     *  - Tie-breakers: higher score, then smaller K, then higher hit-rate.
     *
     * @param train  Fit set (train or full CPH).
     * @param Kgrid  Candidates.
     * @param bestTrain [out] best PolicyResult on the fit set.
     * @return the selected K.
     */
    int selectBestK(
		    const ClosedPositionHistory<Decimal>& train,
		    const std::vector<int>& Kgrid,
		    PolicyResult& bestTrain) const;

    /**
     * @brief Select best N on the fit set (train or full), using the configured objective.
     *
     * Evaluation:
     *  - Uses MetaExitCalibrator::evaluateBreakevenAfterBars(N, epsilonR).
     *  - Tie-breakers: higher score, then smaller N, then higher hit-rate.
     *
     * @param train  Fit set (train or full CPH).
     * @param Ngrid  Candidates.
     * @param bestTrain [out] best PolicyResult on the fit set.
     * @return the selected N.
     */
    int selectBestN(
		    const ClosedPositionHistory<Decimal>& train,
		    const std::vector<int>& Ngrid,
		    PolicyResult& bestTrain) const;

    /**
     * @brief Convert a PolicyResult to a scalar score based on @p mOpts.getObjective().
     *
     * @param r PolicyResult to score.
     * @return  scalar score (AvgPnL_R or HitRate).
     */
    double score(
		 const PolicyResult& r) const;

  private:
    const ClosedPositionHistory<Decimal>& mClosedPositionHistory;
    ExitTunerOptions<Decimal> mOpts;
  };

  // ===================== Implementation =====================

  template<class Decimal>
  void ExitPolicyAutoTuner<Decimal>::splitTrainTest(
						    ClosedPositionHistory<Decimal>& train,
						    ClosedPositionHistory<Decimal>& test) const
  {
    std::vector<std::shared_ptr<TradingPosition<Decimal>>> all;

    for (auto it = mClosedPositionHistory.beginTradingPositions();
	 it != mClosedPositionHistory.endTradingPositions();
	 ++it)
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

  template<class Decimal>
  std::vector<int> ExitPolicyAutoTuner<Decimal>::proposeKGrid(
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
	const double fnp = aggs[t].getFracNonPositive();
	const double pT1 = aggs[t].getProbTargetNextBar();

	if (tPick < 0 && fnp >= mOpts.getFracNonPosHigh() && pT1 <= mOpts.getTargetHazardLow())
	  {
	    tPick = t;
	  }

	const double s = fnp - pT1;

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
  std::vector<int> ExitPolicyAutoTuner<Decimal>::proposeNGrid(
							      const std::vector<BarAgeAggregate>& aggs) const
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

  /**
   * @brief Convert a PolicyResult to a scalar score based on @p mOpts.getObjective().
   *
   * @param r PolicyResult to score.
   * @return  scalar score (AvgPnL_R or HitRate).
   */

  template<class Decimal>
  double ExitPolicyAutoTuner<Decimal>::score(const PolicyResult& r) const
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

  template<class Decimal>
  int ExitPolicyAutoTuner<Decimal>::selectBestK(
						const ClosedPositionHistory<Decimal>& train,
						const std::vector<int>& Kgrid,
						PolicyResult& bestTrain) const
  {
    if (Kgrid.empty())
      {
	bestTrain = PolicyResult(0.0, 0.0, 0.0, 0);
	return 0;
      }

    MetaExitCalibrator<Decimal> cal(train);
    double bestScore = -1e99;
    int bestK = Kgrid.front();
    bestTrain = PolicyResult(0.0, 0.0, 0.0, 0);

    for (int K : Kgrid)
      {
	auto res = cal.evaluateFailureToPerformBars(K, mOpts.getThresholdR(), FailureExitFill::OpenOfKPlus1);
	const double s = score(res);

	if (s > bestScore ||
	    (s == bestScore && K < bestK) ||
	    (s == bestScore && res.getHitRate() > bestTrain.getHitRate()))
	  {
	    bestScore = s;
	    bestK = K;
	    bestTrain = res;
	  }
      }

    return bestK;
  }

  template<class Decimal>
  int ExitPolicyAutoTuner<Decimal>::selectBestN(const ClosedPositionHistory<Decimal>& train,
						const std::vector<int>& Ngrid,
						PolicyResult& bestTrain) const
  {
    if (Ngrid.empty())
      {
	bestTrain = PolicyResult(0.0, 0.0, 0.0, 0);
	return 0;
      }

    MetaExitCalibrator<Decimal> cal(train);
    double bestScore = -1e99;
    int bestN = Ngrid.front();
    bestTrain = PolicyResult(0.0, 0.0, 0.0, 0);

    for (int N : Ngrid)
      {
	auto res = cal.evaluateBreakevenAfterBars(N, mOpts.getEpsilonR());
	const double s = score(res);

	if (s > bestScore ||
	    (s == bestScore && N < bestN) ||
	    (s == bestScore && res.getHitRate() > bestTrain.getHitRate()))
	  {
	    bestScore = s;
	    bestN = N;
	    bestTrain = res;
	  }
      }

    return bestN;
  }

  template<class Decimal>
  ExitTuningReport<Decimal> ExitPolicyAutoTuner<Decimal>::tune()
  {
    // 1) Summaries to seed candidate grids
    MetaExitAnalytics<Decimal> mex(mClosedPositionHistory);
    const auto aggs = mex.summarizeByBarAge(mOpts.getMaxBarsToAnalyze());

    const auto Kgrid = proposeKGrid(aggs);
    const auto Ngrid = proposeNGrid(aggs);

    // 2) Train/Test split
    ClosedPositionHistory<Decimal> train;
    ClosedPositionHistory<Decimal> test;
    splitTrainTest(train, test);
    const bool useFull = (test.beginTradingPositions() == test.endTradingPositions());

    // 3) Fit: choose best K and N independently on train (or full if no test)
    PolicyResult bestTrainK(0.0, 0.0, 0.0, 0);
    PolicyResult bestTrainN(0.0, 0.0, 0.0, 0);

    const int K = selectBestK(useFull ? mClosedPositionHistory : train, Kgrid, bestTrainK);
    const int N = selectBestN(useFull ? mClosedPositionHistory : train, Ngrid, bestTrainN);

    // 4) Test metrics on held-out set (or reuse train when test is empty)
    PolicyResult testK(0.0, 0.0, 0.0, 0);
    PolicyResult testN(0.0, 0.0, 0.0, 0);

    if (useFull)
      {
	testK = bestTrainK;
	testN = bestTrainN;
      }
    else
      {
	MetaExitCalibrator<Decimal> calTest(test);
	testK = calTest.evaluateFailureToPerformBars(K, mOpts.getThresholdR(), FailureExitFill::OpenOfKPlus1);
	testN = calTest.evaluateBreakevenAfterBars(N, mOpts.getEpsilonR());
      }

    // 5) Immutable report
    return ExitTuningReport<Decimal>(K,
				     N,
				     bestTrainK,
				     testK,
				     bestTrainN,
				     testN,
				     Kgrid,
				     Ngrid);
  }
} // namespace mkc_timeseries
