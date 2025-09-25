#pragma once

#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>

#include "ClosedPositionHistory.h"
#include "TradingPosition.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{

  /**
   * @brief Aggregated metrics produced by simulating an exit policy across a set of closed trades.
   *
   * Semantics:
   *  - All metrics are computed over the trades that were actually evaluated by the policy
   *    (e.g., paths with no bars after entry are skipped).
   *  - AvgPnL_R is measured in "R" multiples when a per-trade target is available; when no target
   *    is available for a trade, a robust, cross-trade fallback scale (the median rTarget across
   *    trades that have one) is used to normalize currency P&L to "R"-like units.
   *  - Hit rate counts trades with positive PnL_R as "wins".
   *  - AvgBarsHeld counts bars in the post-entry index convention where t=0 is the first bar
   *    after entry; exiting on bar index i contributes (i + 1) bars to the average.
   */
  class PolicyResult
  {
  public:
    /**
     * @param avgPnL_R    Average PnL expressed in R units (or scaled currency via fallback).
     * @param hitRate     Fraction of trades with positive PnL_R.
     * @param avgBarsHeld Average number of bars held (t=0 is first bar after entry).
     * @param trades      Number of trades included in the aggregation.
     */
    PolicyResult(double avgPnL_R, double hitRate, double avgBarsHeld, int trades)
      : mAvgPnL_R(avgPnL_R)
      , mHitRate(hitRate)
      , mAvgBarsHeld(avgBarsHeld)
      , mTrades(trades)
    {
    }

    double getAvgPnL_R() const
    {
      return mAvgPnL_R;
    }

    double getHitRate() const
    {
      return mHitRate;
    }

    double getAvgBarsHeld() const
    {
      return mAvgBarsHeld;
    }

    int getTrades() const
    {
      return mTrades;
    }

  private:
    double mAvgPnL_R;
    double mHitRate;
    double mAvgBarsHeld;
    int    mTrades;
  };

  /**
   * @brief Fill policy for failure-to-perform exits once the K-bar condition is met.
   *
   * Semantics:
   *  - CloseOfK:     Exit at the close of bar K (evaluation bar).
   *  - OpenOfKPlus1: Exit at the next bar’s open (default; realistic “market-on-open” execution).
   */
  enum class FailureExitFill
  {
    CloseOfK,
    OpenOfKPlus1
  };

  /**
   * @brief Scenario simulator for “exit overlays” on existing closed trades.
   *
   * This class applies exit policies (failure-to-perform at a chosen bar, breakeven from a chosen bar,
   * and time-exit at a chosen bar) to paths derived from previously closed trades, and summarizes the
   * resulting performance.
   *
   * Core conventions and algorithms:
   *  - Time indexing:
   *      t = 0 denotes the FIRST bar AFTER the entry bar.
   *      All arrays (open/high/low/close) are built with this convention.
   *
   *  - Failure-to-perform (F2P):
   *      * At the end of bar K (i.e., Close[K]), compute pnlCurrency:
   *          Long:  Close[K] - Entry
   *          Short: Entry    - Close[K]
   *        If a profit target exists (and thus an R distance), compute pnlR = pnlCurrency / rTarget
   *        and compare to thresholdR; otherwise compare pnlCurrency to 0.0.
   *        If the trade fails the check, exit using the configured fill:
   *          - CloseOfK:     exit at Close[K]
   *          - OpenOfKPlus1: exit at Open[K+1] (default; if K+1 does not exist, fall back to last close)
   *
   *  - Breakeven (BE):
   *      * From bar N onward (inclusive), arm a stop at:
   *          Long:  Entry + epsilonR * rTarget
   *          Short: Entry - epsilonR * rTarget
   *        Using stop-first semantics within each bar:
   *          Long:  hit if Open[t] <= stop OR Low[t] <= stop
   *          Short: hit if Open[t] >= stop OR High[t] >= stop
   *        On hit, exit at the stop level on that bar.
   *        If the trade has no valid rTarget, BE is a no-op.
   *
   *  - Time Exit (Max Hold):
   *      * If the trade is still open after the close of bar H, exit at Open[H+1] (if available);
   *        if H+1 is not available, fall back to the last close.
   *
   *  - Combined policy (F2P + BE + Time Exit):
   *      * Simulate each overlay independently on the path, then pick the earliest exit by bar index.
   *        If two overlays exit on the same bar, prefer the BE exit (consistent with stop-first precedence),
   *        then F2P, then Time Exit.
   *
   *  - Aggregation:
   *      * Results are summarized into PolicyResult over all simulated paths.
   *        PnL_R uses per-trade rTarget where available; otherwise a median rTarget across trades
   *        (when available) is used as a fallback scale to normalize currency PnL.
   *
   * @tparam Decimal Fixed-point or decimal type used for prices and arithmetic.
   *
   * @note This header is a direct extension of your existing version, adding time-exit support
   *       and a 3-argument combined evaluator. (Original reference: file you shared.) :contentReference[oaicite:0]{index=0}
   */
  template<class Decimal>
  class MetaExitCalibrator
  {
  public:
    explicit MetaExitCalibrator(const ClosedPositionHistory<Decimal>& positionHistory)
      : mClosedPositionHistory(positionHistory)
    {
    }

    // ---------------------------
    // Individual overlay evaluators
    // ---------------------------

    PolicyResult evaluateFailureToPerformBars(
      int K,
      const Decimal& thresholdR,
      FailureExitFill fill = FailureExitFill::OpenOfKPlus1) const;

    PolicyResult evaluateBreakevenAfterBars(
      int N,
      const Decimal& epsilonR) const;

    /**
     * @brief Evaluate a pure time-exit policy at bar H.
     *
     * Semantics:
     *  - If the path has at least H+1 bars, exit at Open[H+1] (i.e., after the close of H).
     *  - If the path has exactly H+0 bars (no next bar), fall back to the last close (recorded exit).
     *  - H < 0 disables time exit and returns the recorded last close.
     */
    PolicyResult evaluateTimeExitAtBars(
      int H) const;

    // ---------------------------
    // Combined evaluators
    // ---------------------------

    /**
     * @brief Evaluate the combined overlay (F2P at K and BE from N).
     * Backward-compatible overload; internally calls the 3-argument version with H = -1 (disabled).
     */
    PolicyResult evaluateCombinedPolicy(
      int K,
      int N,
      const Decimal& thresholdR,
      const Decimal& epsilonR,
      FailureExitFill fill = FailureExitFill::OpenOfKPlus1) const
    {
      return evaluateCombinedPolicy(K, N, /*H*/ -1, thresholdR, epsilonR, fill);
    }

    /**
     * @brief Evaluate the combined overlay (F2P at K, BE from N, and Time Exit at H).
     *
     * Precedence on the same bar: BE (stop-first) > F2P > Time Exit.
     * Earliest-by-bar-index wins otherwise.
     *
     * @param K           Failure-to-perform evaluation bar.
     * @param N           Breakeven activation bar.
     * @param H           Time-exit bar (max hold). Use H < 0 to disable time-exit.
     * @param thresholdR  Threshold in R units for failure-to-perform.
     * @param epsilonR    Offset in R units for breakeven.
     * @param fill        F2P fill policy (Close[K] vs Open[K+1]); default OpenOfKPlus1.
     * @return            Aggregated PolicyResult for the combined overlay.
     */
    PolicyResult evaluateCombinedPolicy(
      int K,
      int N,
      int H,
      const Decimal& thresholdR,
      const Decimal& epsilonR,
      FailureExitFill fill = FailureExitFill::OpenOfKPlus1) const;

  private:
    // ---------------------------
    // Path representation
    // ---------------------------

    class PathArrays
    {
    public:
      PathArrays(
        bool isLong,
        const Decimal& entry,
        bool hasTargetR,
        const Decimal& rTarget,
        std::vector<Decimal> open,
        std::vector<Decimal> high,
        std::vector<Decimal> low,
        std::vector<Decimal> close)
        : mIsLong(isLong)
        , mEntry(entry)
        , mHasTargetR(hasTargetR)
        , mRTarget(rTarget)
        , mOpen(std::move(open))
        , mHigh(std::move(high))
        , mLow(std::move(low))
        , mClose(std::move(close))
      {
      }

      bool isLong() const
      {
        return mIsLong;
      }

      const Decimal& entry() const
      {
        return mEntry;
      }

      bool hasTargetR() const
      {
        return mHasTargetR;
      }

      const Decimal& rTarget() const
      {
        return mRTarget;
      }

      const std::vector<Decimal>& open() const
      {
        return mOpen;
      }

      const std::vector<Decimal>& high() const
      {
        return mHigh;
      }

      const std::vector<Decimal>& low() const
      {
        return mLow;
      }

      const std::vector<Decimal>& close() const
      {
        return mClose;
      }

      int barsHeld() const
      {
        return static_cast<int>(mClose.size());
      }

    private:
      bool mIsLong;
      Decimal mEntry;
      bool mHasTargetR;
      Decimal mRTarget;
      std::vector<Decimal> mOpen, mHigh, mLow, mClose; // t = 0..T-1; t=0 is first bar after entry
    };

    // ---------------------------
    // Builders & simulators
    // ---------------------------

    PathArrays buildArrays(
      const std::shared_ptr<TradingPosition<Decimal>>& pos) const;

    std::pair<int, Decimal> simulateFailureToPerform(
      const PathArrays& p,
      int K,
      const Decimal& thresholdR,
      FailureExitFill fill) const;

    std::pair<int, Decimal> simulateBreakeven(
      const PathArrays& p,
      int N,
      const Decimal& epsilonR) const;

    /**
     * @brief Simulate time-exit (max hold) on a single path.
     *
     * If H < 0: disabled → return (last, Close[last]).
     * If H >= barsHeld:   no Open[H+1] available → return (last, Close[last]).
     * Else:               return (H+1, Open[H+1]).
     */
    std::pair<int, Decimal> simulateTimeExit(
      const PathArrays& p,
      int H) const;

    /**
     * @brief Simulate combined policy; earliest exit wins.
     * Same-bar precedence: BE > F2P > Time Exit.
     */
    std::pair<int, Decimal> simulateCombined(
      const PathArrays& p,
      int K,
      int N,
      int H,
      const Decimal& thresholdR,
      const Decimal& epsilonR,
      FailureExitFill fill) const;

    // ---------------------------
    // Aggregation
    // ---------------------------

    PolicyResult summarize(
      const std::vector<std::pair<int, Decimal>>& exits,
      const std::vector<PathArrays>& paths) const;

  private:
    const ClosedPositionHistory<Decimal>& mClosedPositionHistory;
  };

  // ===================== Implementation =====================

  template<class Decimal>
  typename MetaExitCalibrator<Decimal>::PathArrays
  MetaExitCalibrator<Decimal>::buildArrays(
    const std::shared_ptr<TradingPosition<Decimal>>& pos) const
  {
    const bool    isLong = pos->isLongPosition();
    const Decimal entry  = pos->getEntryPrice();

    // rTarget & availability from position target
    bool    hasTargetR = false;
    Decimal rTarget    = DecimalConstants<Decimal>::DecimalZero;
    const   Decimal target = pos->getProfitTarget();

    if (target > DecimalConstants<Decimal>::DecimalZero)
    {
      hasTargetR = true;
      rTarget    = isLong ? (target - entry) : (entry - target);
    }

    // Skip the entry bar; t=0 is the first bar after entry
    std::vector<Decimal> open, high, low, close;
    auto it = pos->beginPositionBarHistory();
    if (it != pos->endPositionBarHistory())
    {
      ++it;
    }

    for (; it != pos->endPositionBarHistory(); ++it)
    {
      const auto& b = it->second;
      open.push_back(b.getOpenValue());
      high.push_back(b.getHighValue());
      low.push_back(b.getLowValue());
      close.push_back(b.getCloseValue());
    }

    return PathArrays(
      isLong,
      entry,
      hasTargetR,
      rTarget,
      std::move(open),
      std::move(high),
      std::move(low),
      std::move(close));
  }

  template<class Decimal>
  std::pair<int, Decimal>
  MetaExitCalibrator<Decimal>::simulateFailureToPerform(
    const PathArrays& p,
    int K,
    const Decimal& thresholdR,
    FailureExitFill fill) const
  {
    if (K < 0 || K >= p.barsHeld())
    {
      const int last = p.barsHeld() - 1;
      return { last, p.close()[last] };
    }

    // Evaluate rule at CLOSE[K]
    const Decimal pnlCur = p.isLong()
      ? (p.close()[K] - p.entry())
      : (p.entry() - p.close()[K]);

    bool fail = false;

    if (p.hasTargetR() && p.rTarget() > DecimalConstants<Decimal>::DecimalZero)
    {
      const Decimal pnlR = pnlCur / p.rTarget();
      fail = (pnlR <= thresholdR);
    }
    else
    {
      fail = (pnlCur <= DecimalConstants<Decimal>::DecimalZero);
    }

    if (!fail)
    {
      const int last = p.barsHeld() - 1;
      return { last, p.close()[last] };
    }

    // Exit fill:
    if (fill == FailureExitFill::CloseOfK)
    {
      return { K, p.close()[K] };
    }
    else
    {
      // OpenOfKPlus1
      const int next = K + 1;

      if (next < p.barsHeld())
      {
        return { next, p.open()[next] };
      }
      else
      {
        // No next bar available; conservatively keep recorded last close
        const int last = p.barsHeld() - 1;
        return { last, p.close()[last] };
      }
    }
  }

  template<class Decimal>
  std::pair<int, Decimal>
  MetaExitCalibrator<Decimal>::simulateBreakeven(
    const PathArrays& p,
    int N,
    const Decimal& epsilonR) const
  {
    const int last = p.barsHeld() - 1;

    if (N < 0 || N >= p.barsHeld() || !p.hasTargetR() || p.rTarget() <= DecimalConstants<Decimal>::DecimalZero)
    {
      return { last, p.close()[last] };
    }

    const Decimal breakEven = p.isLong()
      ? (p.entry() + epsilonR * p.rTarget())
      : (p.entry() - epsilonR * p.rTarget());

    // Stop-first breach scanning from N onward
    for (int t = N; t <= last; ++t)
    {
      const bool hitStop = p.isLong()
        ? (p.open()[t] <= breakEven || p.low()[t]  <= breakEven)
        : (p.open()[t] >= breakEven || p.high()[t] >= breakEven);

      if (hitStop)
      {
        return { t, breakEven };
      }
    }

    return { last, p.close()[last] };
  }

  template<class Decimal>
  std::pair<int, Decimal>
  MetaExitCalibrator<Decimal>::simulateTimeExit(
    const PathArrays& p,
    int H) const
  {
    const int last = p.barsHeld() - 1;

    if (H < 0)
    {
      return { last, p.close()[last] };
    }

    // We need Open[H+1] to exit at the next session’s open
    const int next = H + 1;
    if (next < p.barsHeld())
    {
      return { next, p.open()[next] };
    }
    else
    {
      // No next bar available -> keep the recorded last close
      return { last, p.close()[last] };
    }
  }

  template<class Decimal>
  std::pair<int, Decimal>
  MetaExitCalibrator<Decimal>::simulateCombined(
    const PathArrays& p,
    int K,
    int N,
    int H,
    const Decimal& thresholdR,
    const Decimal& epsilonR,
    FailureExitFill fill) const
  {
    if (p.barsHeld() <= 0)
    {
      return { -1, DecimalConstants<Decimal>::DecimalZero };
    }

    const auto f2p = simulateFailureToPerform(p, K, thresholdR, fill);
    const auto be  = simulateBreakeven(p, N, epsilonR);
    const auto tx  = simulateTimeExit(p, H);

    // Earliest-exit-wins by bar index; tie-break: BE > F2P > Time Exit
    // Normalize invalids to "last" for comparisons (all our simulators already do)
    const int beIdx  = be.first;
    const int f2Idx  = f2p.first;
    const int txIdx  = tx.first;

    int earliest = beIdx;
    // Compare F2P vs BE
    if (f2Idx < earliest)
    {
      earliest = f2Idx;
    }
    // Compare TimeExit vs current earliest
    if (txIdx < earliest)
    {
      earliest = txIdx;
    }

    // Same-bar precedence handling
    const bool beEarliest  = (beIdx == earliest);
    const bool f2Earliest  = (f2Idx == earliest);

    if (beEarliest)
    {
      return be;
    }
    else if (f2Earliest)
    {
      return f2p;
    }
    else
    {
      return tx;
    }
  }

  template<class Decimal>
  PolicyResult
  MetaExitCalibrator<Decimal>::evaluateCombinedPolicy(
    int K,
    int N,
    int H,
    const Decimal& thresholdR,
    const Decimal& epsilonR,
    FailureExitFill fill) const
  {
    std::vector<PathArrays> paths;
    paths.reserve(128);

    for (auto it = mClosedPositionHistory.beginTradingPositions();
         it != mClosedPositionHistory.endTradingPositions();
         ++it)
    {
      auto p = buildArrays(it->second);
      if (p.barsHeld() == 0)
      {
        continue;  // skip zero-length paths (no bars after entry)
      }
      paths.push_back(std::move(p));
    }

    std::vector<std::pair<int, Decimal>> exits;
    exits.reserve(paths.size());

    for (const auto& p : paths)
    {
      exits.push_back(simulateCombined(p, K, N, H, thresholdR, epsilonR, fill));
    }

    return summarize(exits, paths);
  }

  template<class Decimal>
  PolicyResult
  MetaExitCalibrator<Decimal>::summarize(const std::vector<std::pair<int, Decimal>>& exits,
                                         const std::vector<PathArrays>& paths) const
  {
    const size_t n = exits.size();

    if (n == 0)
    {
      return PolicyResult(0.0, 0.0, 0.0, 0);
    }

    double sumPnL_R = 0.0;
    double sumBars  = 0.0;
    int    wins     = 0;

    // Median rTarget (for fallback scaling)
    std::vector<double> rts;
    rts.reserve(paths.size());

    for (const auto& p : paths)
    {
      if (p.hasTargetR() && p.rTarget() > DecimalConstants<Decimal>::DecimalZero)
      {
        rts.push_back(p.rTarget().getAsDouble());
      }
    }

    double scaleFallback = 1.0;

    if (!rts.empty())
    {
      size_t mid = rts.size() / 2;
      std::nth_element(rts.begin(), rts.begin() + mid, rts.end());
      scaleFallback = rts[mid];
    }

    for (size_t i = 0; i < n; ++i)
    {
      const auto& p      = paths[i];
      const int   idx    = exits[i].first;
      const auto  exitPx = exits[i].second;

      const double barsHeld = static_cast<double>(idx + 1); // t=0 is first day after entry
      sumBars += barsHeld;

      const Decimal pnlCur = p.isLong()
        ? (exitPx - p.entry())
        : (p.entry() - exitPx);

      double pnlR_d = 0.0;

      if (p.hasTargetR() && p.rTarget() > DecimalConstants<Decimal>::DecimalZero)
      {
        // Do the division in decimal space for precision; convert once to double.
        const Decimal pnlR_decimal = pnlCur / p.rTarget();
        pnlR_d = pnlR_decimal.getAsDouble();
      }
      else
      {
        pnlR_d = static_cast<double>(pnlCur.getAsDouble()) / scaleFallback;
      }

      sumPnL_R += pnlR_d;

      if (pnlR_d > 0.0)
      {
        ++wins;
      }
    }

    return PolicyResult(
      /*avgPnL_R*/    sumPnL_R / static_cast<double>(n),
      /*hitRate*/     static_cast<double>(wins) / static_cast<double>(n),
      /*avgBarsHeld*/ sumBars / static_cast<double>(n),
      /*trades*/      static_cast<int>(n));
  }

  // ---------------------------
  // Standalone overlay evaluators (kept as in your original, with BE/F2P)
  // ---------------------------

  template<class Decimal>
  PolicyResult
  MetaExitCalibrator<Decimal>::evaluateFailureToPerformBars(int K,
                                                            const Decimal& thresholdR,
                                                            FailureExitFill fill) const
  {
    std::vector<PathArrays> paths;

    for (auto it = mClosedPositionHistory.beginTradingPositions();
         it != mClosedPositionHistory.endTradingPositions();
         ++it)
    {
      paths.push_back(buildArrays(it->second));
    }

    std::vector<std::pair<int, Decimal>> exits;
    exits.reserve(paths.size());

    for (const auto& p : paths)
    {
      exits.push_back(simulateFailureToPerform(p, K, thresholdR, fill));
    }

    return summarize(exits, paths);
  }

  template<class Decimal>
  PolicyResult
  MetaExitCalibrator<Decimal>::evaluateBreakevenAfterBars(int N,
                                                          const Decimal& epsilonR) const
  {
    std::vector<PathArrays> paths;

    for (auto it = mClosedPositionHistory.beginTradingPositions();
         it != mClosedPositionHistory.endTradingPositions();
         ++it)
    {
      paths.push_back(buildArrays(it->second));
    }

    std::vector<std::pair<int, Decimal>> exits;
    exits.reserve(paths.size());

    for (const auto& p : paths)
    {
      exits.push_back(simulateBreakeven(p, N, epsilonR));
    }

    return summarize(exits, paths);
  }

  template<class Decimal>
  PolicyResult
  MetaExitCalibrator<Decimal>::evaluateTimeExitAtBars(int H) const
  {
    std::vector<PathArrays> paths;

    for (auto it = mClosedPositionHistory.beginTradingPositions();
         it != mClosedPositionHistory.endTradingPositions();
         ++it)
    {
      paths.push_back(buildArrays(it->second));
    }

    std::vector<std::pair<int, Decimal>> exits;
    exits.reserve(paths.size());

    for (const auto& p : paths)
    {
      exits.push_back(simulateTimeExit(p, H));
    }

    return summarize(exits, paths);
  }

} // namespace mkc_timeseries
