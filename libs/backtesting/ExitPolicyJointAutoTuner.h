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
   * @brief Immutable report for the joint (K, N, H) auto-tuner.
   *
   * Captures the selected parameters for the combined policy:
   *  - failureToPerformBars (K): performance check bar (t=K).
   *  - breakevenActivationBars (N): breakeven armed from t>=N.
   *  - maxHoldBars (H): time-exit check at t=H with exit fill at Open[H+1].
   *
   * Also includes the train/test metrics for the combined policy,
   * and the candidate grids examined for each dimension.
   */
  class JointExitTuningReportBase
  {
  public:
    JointExitTuningReportBase(int failureToPerformBars,
                              int breakevenActivationBars,
                              int maxHoldBars,
                              const PolicyResult& trainCombined,
                              const PolicyResult& testCombined,
                              std::vector<int> failureToPerformGrid,
                              std::vector<int> breakevenGrid,
                              std::vector<int> maxHoldGrid)
      : mFailureToPerformBars(failureToPerformBars)
      , mBreakevenActivationBars(breakevenActivationBars)
      , mMaxHoldBars(maxHoldBars)
      , mTrainCombined(trainCombined)
      , mTestCombined(testCombined)
      , mFailureToPerformGrid(std::move(failureToPerformGrid))
      , mBreakevenGrid(std::move(breakevenGrid))
      , mMaxHoldGrid(std::move(maxHoldGrid))
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

    /** @return Selected H (time-exit bar). */
    int getMaxHoldBars() const
    {
      return mMaxHoldBars;
    }

    /** @return Train-set PolicyResult for the combined policy at (K, N, H). */
    const PolicyResult& getTrainCombined() const
    {
      return mTrainCombined;
    }

    /** @return Test-set PolicyResult for the combined policy at (K, N, H). */
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

    /** @return Candidate grid examined for time-exit H. */
    const std::vector<int>& getMaxHoldGrid() const
    {
      return mMaxHoldGrid;
    }

  private:
    int mFailureToPerformBars;
    int mBreakevenActivationBars;
    int mMaxHoldBars;

    PolicyResult mTrainCombined;
    PolicyResult mTestCombined;

    std::vector<int> mFailureToPerformGrid;
    std::vector<int> mBreakevenGrid;
    std::vector<int> mMaxHoldGrid;
  };

  /**
   * @brief Typed (templated) wrapper for JointExitTuningReportBase.
   *
   * Matches your templated API pattern while the payload itself is not Decimal-typed.
   */
  template<class Decimal>
  class JointExitTuningReport : public JointExitTuningReportBase
  {
  public:
    JointExitTuningReport(int failureToPerformBars,
                          int breakevenActivationBars,
                          int maxHoldBars,
                          const PolicyResult& trainCombined,
                          const PolicyResult& testCombined,
                          std::vector<int> failureToPerformGrid,
                          std::vector<int> breakevenGrid,
                          std::vector<int> maxHoldGrid)
      : JointExitTuningReportBase(failureToPerformBars,
                                  breakevenActivationBars,
                                  maxHoldBars,
                                  trainCombined,
                                  testCombined,
                                  std::move(failureToPerformGrid),
                                  std::move(breakevenGrid),
                                  std::move(maxHoldGrid))
    {
    }
  };

  /**
   * @brief Joint auto tuner that selects an optimal (K, N, H) triple for a combined exit policy.
   *
   * Responsibilities:
   *  1) Compute bar-age aggregates with MetaExitAnalytics::summarizeByBarAge(maxBarsToAnalyze).
   *  2) Propose candidate grids:
   *     - K (failure-to-perform grid): seed where fracNonPositive is high and probTargetNextBar is low.
   *     - N (breakeven grid): seed where medianMfeRSoFar >= alphaMfeR.
   *     - H (max-hold / time-exit grid): seed where probTargetNextBar is low and fracNonPositive is high,
   *       and also include small values near early resolution (e.g., 2–4), then ±neighborSpan.
   *       If empty and useFullGridIfEmpty, fall back to a compact default set clipped to [0..T-1].
   *  3) Deterministic train/test split with optional embargo (by entry-time order).
   *  4) 3-D grid search on the fit set (train if available, else full):
   *       Evaluate MetaExitCalibrator::evaluateCombinedPolicy(K, N, H, thresholdR, epsilonR, OpenOfKPlus1).
   *       Score by objective (AvgPnL_R or HitRate).
   *       Tie-breakers prefer earlier resolution consistent with your anomaly behavior.
   *  5) Evaluate the chosen (K, N, H) on the held-out test set; if no test set, test == train.
   *
   * Conventions:
   *  - t=0 is the first bar AFTER entry.
   *  - Failure-to-perform checks Close[K], exits at Open[K+1] by default.
   *  - Breakeven is stop-first from t>=N.
   *  - Time exit checks “still open” at t=H and exits at Open[H+1].
   */
  template<class Decimal>
  class ExitPolicyJointAutoTuner
  {
  public:
    ExitPolicyJointAutoTuner(const ClosedPositionHistory<Decimal>& cph,
                             const ExitTunerOptions<Decimal>& opts)
      : mCph(cph)
      , mOpts(opts)
    {
    }

    /**
     * @brief Convenience ctor: only maxBarsToAnalyze is required; defaults for the rest.
     */
    ExitPolicyJointAutoTuner(const ClosedPositionHistory<Decimal>& cph,
                             int maxBarsToAnalyze)
      : mCph(cph)
      , mOpts(ExitTunerOptions<Decimal>(maxBarsToAnalyze))
    {
    }

    /**
     * @brief Run the end-to-end joint tuning pipeline, returning an immutable (K, N, H) report.
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
    void splitTrainTest(ClosedPositionHistory<Decimal>& train,
                        ClosedPositionHistory<Decimal>& test) const;

    std::vector<int> proposeFailureToPerformGrid(const std::vector<BarAgeAggregate>& aggs) const;

    std::vector<int> proposeBreakevenGrid(const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Build the max-hold (time-exit) grid (candidate H values) from bar-age aggregates.
     *
     * Heuristic:
     *  - Prefer small H consistent with your fast resolution (winners at t=0–1), so seed {2,3,4}.
     *  - If analytics show a “decay zone” (fracNonPositive >= fracNonPosHigh AND
     *    probTargetNextBar <= targetHazardLow), include that t as a seed.
     *  - Expand each seed by ±neighborSpan, clamp to [0..T-1].
     *  - If still empty and useFullGridIfEmpty, fall back to a compact default {2,3,4,5,6,8}∩[0..T-1].
     */
    std::vector<int> proposeMaxHoldGrid(const std::vector<BarAgeAggregate>& aggs) const;

    /**
     * @brief Convert PolicyResult to scalar score according to the chosen objective.
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

  template<class Decimal>
  std::vector<int>
  ExitPolicyJointAutoTuner<Decimal>::proposeFailureToPerformGrid(const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());

    if (T == 0)
    {
      return {};
    }

    int tPick = -1;

    for (int t = 0; t < T; ++t)
    {
      const double fracNonPos     = aggs[t].getFracNonPositive();
      const double probTargetNext = aggs[t].getProbTargetNextBar();

      if (fracNonPos >= mOpts.getFracNonPosHigh() &&
          probTargetNext <= mOpts.getTargetHazardLow())
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
  std::vector<int>
  ExitPolicyJointAutoTuner<Decimal>::proposeMaxHoldGrid(const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());

    if (T == 0)
    {
      return {};
    }

    std::vector<int> seeds;

    // 1) Small, fast-resolution candidates (your winners cluster at t=0–1)
    for (int x : {2, 3, 4})
    {
      if (x < T)
      {
        seeds.push_back(x);
      }
    }

    // 2) Decay-aware seed where target hazard is low and non-positive fraction is high
    for (int t = 0; t < T; ++t)
    {
      const double fracNonPos     = aggs[t].getFracNonPositive();
      const double probTargetNext = aggs[t].getProbTargetNextBar();

      if (fracNonPos >= mOpts.getFracNonPosHigh() &&
          probTargetNext <= mOpts.getTargetHazardLow())
      {
        seeds.push_back(t);
        break; // first such t
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

    for (int s : seeds)
    {
      pushNeighbor(s);
    }

    if (grid.empty() && mOpts.getUseFullGridIfEmpty())
    {
      // Compact fallback emphasizing short holds; add a slightly longer probe
      for (int x : {2, 3, 4, 5, 6, 8})
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

    case TuningObjective::PnLPerBar:
      {
	const double denom = std::max(r.getAvgBarsHeld(), 1e-9);
	return r.getAvgPnL_R() / denom;
      }

      case TuningObjective::AvgPnL_R:
      default:
      {
        return r.getAvgPnL_R();
      }
    }
  }

  /**
   * @brief Run the end-to-end joint tuning pipeline, returning an immutable (K, N, H) report.
   *
   * Steps:
   *  1) Summarize bar-age behavior to seed grids.
   *  2) Split train/test deterministically with optional embargo.
   *  3) Jointly select (K, N, H) by grid search on the fit set using the configured objective.
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
    const auto maxHoldGrid          = proposeMaxHoldGrid(aggs);

    // 2) Train/Test split
    ClosedPositionHistory<Decimal> train;
    ClosedPositionHistory<Decimal> test;
    splitTrainTest(train, test);

    const bool useFull = (test.beginTradingPositions() == test.endTradingPositions());
    const ClosedPositionHistory<Decimal>& fitCph = useFull ? mCph : train;

    // 3) Joint grid search over (K, N, H)
    MetaExitCalibrator<Decimal> calFit(fitCph);

    double bestScore = -1e99;

    int bestK = failureToPerformGrid.empty() ? 0 : failureToPerformGrid.front();
    int bestN = breakevenGrid.empty()        ? 0 : breakevenGrid.front();
    int bestH = maxHoldGrid.empty()          ? std::min(8, (int)aggs.size()-1) : maxHoldGrid.front();

    PolicyResult bestTrainCombined(0.0, 0.0, 0.0, 0);

    for (int K : failureToPerformGrid)
    {
      for (int N : breakevenGrid)
      {
        for (int H : maxHoldGrid)
        {
          auto res = calFit.evaluateCombinedPolicy(
            /*K*/ K,
            /*N*/ N,
            /*H*/ H,
            mOpts.getThresholdR(),
            mOpts.getEpsilonR(),
            FailureExitFill::OpenOfKPlus1);

          const double s     = score(res);
          const int    sumKNH = K + N + H;
          const int    sumBest = bestK + bestN + bestH;

          // Tie-breakers favor earlier resolution and your anomaly profile:
          //  1) higher score
          //  2) smaller H (free capital sooner)
          //  3) smaller (K + N + H)
          //  4) higher hit-rate
          //  5) smaller K
          //  6) smaller N
          if (s > bestScore ||
              (s == bestScore && H < bestH) ||
              (s == bestScore && H == bestH && sumKNH < sumBest) ||
              (s == bestScore && H == bestH && sumKNH == sumBest && res.getHitRate() > bestTrainCombined.getHitRate()) ||
              (s == bestScore && H == bestH && sumKNH == sumBest && res.getHitRate() == bestTrainCombined.getHitRate() && K < bestK) ||
              (s == bestScore && H == bestH && sumKNH == sumBest && res.getHitRate() == bestTrainCombined.getHitRate() && K == bestK && N < bestN))
          {
            bestScore = s;
            bestK = K;
            bestN = N;
            bestH = H;
            bestTrainCombined = res;
          }
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
        bestK,
        bestN,
        bestH,
        mOpts.getThresholdR(),
        mOpts.getEpsilonR(),
        FailureExitFill::OpenOfKPlus1);
    }

    // 5) Immutable report
    return JointExitTuningReport<Decimal>(
      bestK,
      bestN,
      bestH,
      bestTrainCombined,
      testCombined,
      failureToPerformGrid,
      breakevenGrid,
      maxHoldGrid);
  }

} // namespace mkc_timeseries
