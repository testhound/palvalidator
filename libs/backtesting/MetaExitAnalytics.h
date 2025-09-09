#pragma once

#include <vector>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <limits>
#include <cmath>

#include "ClosedPositionHistory.h"
#include "PositionPathAnalytics.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{

  //======================================================================
  // BarAgeSnapshot
  //======================================================================

  /**
   * @brief Per-trade, per-bar snapshot used for fine-grained diagnostics.
   *
   * Semantics/conventions:
   *  - Time indexing: t = 0 denotes the FIRST bar AFTER the entry bar.
   *  - mPnlR_TargetAtClose is the per-trade PnL at the CLOSE of bar t,
   *    expressed in R units when a valid per-trade target exists; otherwise
   *    it is left at 0 and mHasTargetR==false indicates “no R available”.
   *  - mTargetTouchedByT and mStopTouchedByT are computed using conservative,
   *    stop-first precedence and reflect whether the first touch occurred on
   *    or before t (i.e., index ≤ t).
   *
   * @tparam Decimal Fixed-point/decimal type used by the backtester for price math.
   */
  template <class Decimal>
  class BarAgeSnapshot
  {
  public:
    /**
     * @param barAge                 Bar index (0 = first bar after entry).
     * @param pnlR_TargetAtClose     PnL at CLOSE[t], in R units if available; 0 if no target.
     * @param hasTargetR             True if this trade has a valid rTarget > 0.
     * @param targetTouchedByT       True if target first-touch index ≤ t.
     * @param stopTouchedByT         True if stop first-touch index ≤ t.
     */
    BarAgeSnapshot(
        int barAge,
        const Decimal& pnlR_TargetAtClose,
        bool hasTargetR,
        bool targetTouchedByT,
        bool stopTouchedByT)
      : mBarAge(barAge)
      , mPnlR_TargetAtClose(pnlR_TargetAtClose)
      , mHasTargetR(hasTargetR)
      , mTargetTouchedByT(targetTouchedByT)
      , mStopTouchedByT(stopTouchedByT)
    {
    }

    /** @return Bar index (0 = first bar after entry). */
    int getBarAge() const
    {
      return mBarAge;
    }

    /** @return PnL at CLOSE[t], in R units when available. */
    const Decimal& getPnlR_TargetAtClose() const
    {
      return mPnlR_TargetAtClose;
    }

    /** @return True if this trade had a valid per-trade R distance (rTarget > 0). */
    bool hasTargetR() const
    {
      return mHasTargetR;
    }

    /** @return True if the target has first-touched on or before this t. */
    bool getTargetTouchedByT() const
    {
      return mTargetTouchedByT;
    }

    /** @return True if the stop has first-touched on or before this t. */
    bool getStopTouchedByT() const
    {
      return mStopTouchedByT;
    }

  private:
    int mBarAge;
    Decimal mPnlR_TargetAtClose;
    bool mHasTargetR;
    bool mTargetTouchedByT;
    bool mStopTouchedByT;
  };

  //======================================================================
  // BarAgeAggregate
  //======================================================================

  /**
   * @brief Survival- and hazard-style aggregates computed across trades at a bar age t.
   *
   * Definitions (all measured at integer bar ages t using the t=0 convention):
   *  - survival:
   *      Fraction of the initial cohort that remains “alive” at the start of bar t
   *      (i.e., neither stop nor target has first-touched before t).
   *  - fracNonPositive:
   *      Among survivors at t, fraction whose PnL at CLOSE[t] is ≤ 0 in R units
   *      (if rTarget exists), otherwise ≤ 0 in currency space.
   *  - probTargetNextBar / probStopNextBar:
   *      Among survivors at t, probability that the FIRST touch of target/stop
   *      occurs exactly at t+1 (one-step hazard proxies).
   *  - medianMfeRSoFar:
   *      Median of per-trade MFE so far (0..t) expressed in R units; NaN if insufficient data.
   */
  class BarAgeAggregate
  {
  public:
    BarAgeAggregate(
        int barAge,
        double survival,
        double fracNonPositive,
        double probTargetNextBar,
        double probStopNextBar,
        double medianMfeRSoFar)
      : mBarAge(barAge)
      , mSurvival(survival)
      , mFracNonPositive(fracNonPositive)
      , mProbTargetNextBar(probTargetNextBar)
      , mProbStopNextBar(probStopNextBar)
      , mMedianMfeRSoFar(medianMfeRSoFar)
    {
    }

    /** @return Bar index (0 = first bar after entry). */
    int getBarAge() const
    {
      return mBarAge;
    }

    /**
     * @return Fraction of the initial total trades that were still active (had not
     *         hit a stop or target) at the beginning of this bar age t.
     */
    double getSurvival() const
    {
      return mSurvival;
    }

    /** @return Among survivors at t, the fraction with non-positive PnL at CLOSE[t]. */
    double getFracNonPositive() const
    {
      return mFracNonPositive;
    }

    /** @return Among survivors at t, the probability the target first-touches at t+1. */
    double getProbTargetNextBar() const
    {
      return mProbTargetNextBar;
    }

    /** @return Among survivors at t, the probability the stop first-touches at t+1. */
    double getProbStopNextBar() const
    {
      return mProbStopNextBar;
    }

    /** @return Median MFE (in R units) accumulated up through t; NaN if none. */
    double getMedianMfeRSoFar() const
    {
      return mMedianMfeRSoFar;
    }

  private:
    int mBarAge;
    double mSurvival;
    double mFracNonPositive;
    double mProbTargetNextBar;
    double mProbStopNextBar;
    double mMedianMfeRSoFar;
  };

  //======================================================================
  // detail helpers
  //======================================================================
  namespace detail
  {

    /**
     * @brief Convert a Decimal-like value to double.
     *
     * Many Decimal implementations expose getAsDouble(); this helper centralizes
     * the conversion for consistency within the analytics.
     */
    template <class Decimal>
    static inline double toDouble(const Decimal& v)
    {
      return v.getAsDouble();
    }

    /**
     * @brief Median for vector<double>; returns NaN for an empty vector.
     *
     * Uses nth_element to compute the middle element in O(n) average time.
     * For even-sized vectors, returns the average of the two central values.
     */
    static inline double medianOrNaN(std::vector<double>& v)
    {
      if (v.empty())
      {
        return std::numeric_limits<double>::quiet_NaN();
      }

      const size_t n = v.size();
      const size_t mid = n / 2;

      std::nth_element(v.begin(), v.begin() + mid, v.end());
      double med = v[mid];

      if ((n % 2) == 0)
      {
        std::nth_element(v.begin(), v.begin() + mid - 1, v.end());
        med = 0.5 * (med + v[mid - 1]);
      }
      return med;
    }

    /**
     * @brief Per-position scan results used internally by MetaExitAnalytics.
     *
     * Conventions and contents:
     *  - barsHeld:
     *      Number of post-entry bars in the path (size of closes[]), i.e., t ranges 0..barsHeld-1.
     *  - firstTargetIdx / firstStopIdx:
     *      First-touch indices for target and stop, computed via PathStats
     *      with conservative same-bar precedence (stop-first).
     *      -1 indicates “never touched”.
     *  - hasTargetR / rTarget:
     *      Availability and magnitude of the per-trade R unit:
     *        Long : target - entry  (must be > 0)
     *        Short: entry  - target (must be > 0)
     *  - entry, isLong:
     *      Entry price and side; used for PnL/MFE sign.
     *  - closes[t]:
     *      Close price at bar t (t=0 is first bar after entry).
     *  - mfeAbsUpTo[t]:
     *      Maximum favorable excursion in currency (absolute) accumulated from start up through bar t.
     */
    template <class Decimal>
    struct PerPositionScan
    {
      int barsHeld{0};
      int firstTargetIdx{-1}; // 0-based since entry
      int firstStopIdx{-1};
      bool hasTargetR{false};
      Decimal rTarget{DecimalConstants<Decimal>::DecimalZero};
      Decimal entry{DecimalConstants<Decimal>::DecimalZero};
      int directionSign{+1};     // retained (not used for PnL)
      bool isLong{true};         // used for PnL sign
      std::vector<Decimal> closes;     // close at each bar index (0..barsHeld-1)
      std::vector<Decimal> mfeAbsUpTo; // MFE (abs currency) up to each t
    };

    /**
     * @brief Scan one closed TradingPosition into compact arrays and first-touch metadata.
     *
     * Algorithm:
     *  1) Use PathStats to obtain firstTargetIdx/firstStopIdx with conservative, stop-first
     *     precedence (for same-bar target/stop touches).
     *  2) Determine rTarget availability and magnitude directly from the position’s target price.
     *  3) Build the per-bar arrays:
     *       - Skip the entry bar so that t=0 is the first bar after entry.
     *       - For each bar, append CLOSE to closes[], and update MFE_abs_up_to[] using
     *         highs (for long) or lows (for short) relative to entry.
     *
     * @param pos Shared pointer to the closed TradingPosition to scan.
     * @return    PerPositionScan with populated arrays and metadata.
     */
    template <class Decimal>
    static PerPositionScan<Decimal>
    scanPosition(const std::shared_ptr<TradingPosition<Decimal>>& pos)
    {
      PerPositionScan<Decimal> s;

      s.entry = pos->getEntryPrice();
      s.isLong = pos->isLongPosition();
      s.directionSign = s.isLong ? +1 : -1;

      // Conservative first-touch indices via PathStats (stop-first precedence enforced)
      PathStats<Decimal> ps(*pos);
      s.firstTargetIdx = ps.getFirstTargetTouchBarIndex();
      s.firstStopIdx   = ps.getFirstStopTouchBarIndex();

      // Determine R availability and magnitude from the position's target directly
      const Decimal target = pos->getProfitTarget();
      if (target > DecimalConstants<Decimal>::DecimalZero)
      {
        s.hasTargetR = true;
        s.rTarget = s.isLong ? (target - s.entry) : (s.entry - target);
      }
      else
      {
        s.hasTargetR = false;
        s.rTarget = DecimalConstants<Decimal>::DecimalZero;
      }

      // Build per-bar close[] and MFE_abs_up_to[]; skip entry bar so t=0 is first bar after entry
      Decimal mfeAbs = DecimalConstants<Decimal>::DecimalZero;

      auto it = pos->beginPositionBarHistory();
      if (it != pos->endPositionBarHistory())
      {
        ++it; // Skip the entry bar
      }

      for (; it != pos->endPositionBarHistory(); ++it)
      {
        const auto& b = it->second;
        s.closes.push_back(b.getCloseValue());

        const Decimal fav = s.isLong
            ? (b.getHighValue() - s.entry)
            : (s.entry - b.getLowValue());

        if (fav > mfeAbs)
        {
          mfeAbs = fav;
        }
        s.mfeAbsUpTo.push_back(mfeAbs);
      }

      s.barsHeld = static_cast<int>(s.closes.size());
      return s;
    }

  } // namespace detail

  //======================================================================
  // MetaExitAnalytics
  //======================================================================

  /**
   * @brief Analytics over closed trades to support exit policy tuning.
   *
   * Responsibilities:
   *  - Transform each closed position into a bar-age path (t=0 is first bar after entry),
   *    with first-touch metadata computed conservatively via PathStats.
   *  - Produce per-trade, per-bar snapshots (BarAgeSnapshot) for diagnostics and exploratory analysis.
   *  - Aggregate across trades at each bar age to obtain survival, next-bar hazard proxies,
   *    fraction non-positive, and median MFE in R units (BarAgeAggregate).
   *
   * Notes:
   *  - This class does not alter trades; it reads existing, closed histories and
   *    computes derived statistics that seed candidate grids for auto-tuners.
   *  - Where R scaling is unavailable for a trade (no valid per-trade target),
   *    snapshot PnL_R is left at 0 and aggregates correctly fall back to currency sign
   *    for the “non-positive” classification.
   *
   * @tparam Decimal Fixed-point/decimal type used by the backtester for price math.
   */
  template <class Decimal>
  class MetaExitAnalytics
  {
  public:
    /**
     * @param closedHistory  Closed positions over which to compute analytics.
     */
    explicit MetaExitAnalytics(const ClosedPositionHistory<Decimal>& closedHistory)
      : mClosedHistory(closedHistory)
    {
    }

    /**
     * @brief Build per-trade snapshots for t = 0..(maxBars-1).
     *
     * For each closed trade:
     *  - Scan to compact arrays (detail::scanPosition).
     *  - For each t up to min(maxBars, barsHeld)-1:
     *      * Compute PnL at CLOSE[t]; convert to R if rTarget is available.
     *      * Mark whether target/stop have first-touched on or before t.
     *
     * @param maxBars  Maximum number of bar ages to emit; non-positive returns empty.
     * @return         Vector of BarAgeSnapshot entries (one per trade per bar, up to maxBars).
     */
    std::vector<BarAgeSnapshot<Decimal>> buildBarAgeSnapshots(int maxBars) const
    {
      std::vector<BarAgeSnapshot<Decimal>> out;

      if (maxBars <= 0)
      {
        return out;
      }

      for (auto it = mClosedHistory.beginTradingPositions();
           it != mClosedHistory.endTradingPositions();
           ++it)
      {
        const auto& pos = it->second;
        auto scan = detail::scanPosition<Decimal>(pos);

        const int lastT = std::min(maxBars, scan.barsHeld) - 1;

        for (int t = 0; t <= lastT; ++t)
        {
          // PnL in R_target at CLOSE of bar t
          Decimal pnlCurrency = scan.isLong
              ? (scan.closes[t] - scan.entry)   // long
              : (scan.entry - scan.closes[t]);  // short

          Decimal pnlR = DecimalConstants<Decimal>::DecimalZero;

          if (scan.hasTargetR && scan.rTarget > DecimalConstants<Decimal>::DecimalZero)
          {
            pnlR = pnlCurrency / scan.rTarget;
          }

          const bool targetByT = (scan.firstTargetIdx >= 0 && scan.firstTargetIdx <= t);
          const bool stopByT   = (scan.firstStopIdx   >= 0 && scan.firstStopIdx   <= t);

          out.emplace_back(t, pnlR, scan.hasTargetR, targetByT, stopByT);
        }
      }

      return out;
    }

    /**
     * @brief Aggregate survival/hazard and robustness statistics per bar age t.
     *
     * Procedure:
     *  1) Pre-scan all positions into compact arrays + first-touch metadata.
     *  2) For each t in [0, maxBars-1]:
     *      - survivors:
     *          Count trades with s.barsHeld > t (alive at start of bar t).
     *      - fracNonPositive:
     *          Among survivors, count non-positive PnL at CLOSE[t]:
     *            * If rTarget available: pnlR <= 0
     *            * Else: pnlCurrency <= 0
     *      - next-bar hazards:
     *          Among survivors, estimate probability that first target/stop touch occurs
     *          exactly at t+1 (i.e., firstTargetIdx == t+1, similarly for stop).
     *      - medianMfeRSoFar:
     *          Among survivors with rTarget, compute MFE_so_far(t) / rTarget and take the median
     *          (NaN if no such survivors).
     *
     * @param maxBars  Maximum number of bar ages to analyze (non-positive yields empty).
     * @return         Vector of BarAgeAggregate entries (one per bar age).
     */
    std::vector<BarAgeAggregate> summarizeByBarAge(int maxBars) const
    {
      std::vector<BarAgeAggregate> result;

      if (maxBars <= 0)
      {
        return result;
      }

      // Pre-scan all positions once
      std::vector<detail::PerPositionScan<Decimal>> scans;

      for (auto it = mClosedHistory.beginTradingPositions();
           it != mClosedHistory.endTradingPositions();
           ++it)
      {
        scans.push_back(detail::scanPosition<Decimal>(it->second));
      }

      const int totalTrades = static_cast<int>(scans.size());

      if (totalTrades == 0)
      {
        return result;
      }

      for (int t = 0; t < maxBars; ++t)
      {
        int survivors = 0;
        int nNonPositive = 0;
        int nTargetNext = 0;
        int nStopNext = 0;
        std::vector<double> mfeRSoFarDoubles;

        for (const auto& s : scans)
        {
          if (s.barsHeld <= t)
          {
            continue; // not alive at t
          }

          ++survivors;

          // ---- PnL classification at close of bar t ----
          // If R available, classify by pnlR; otherwise fallback to currency PnL sign.
          Decimal pnlCurrency = s.isLong
              ? (s.closes[t] - s.entry)
              : (s.entry - s.closes[t]);

          if (s.hasTargetR && s.rTarget > DecimalConstants<Decimal>::DecimalZero)
          {
            Decimal pnlR = pnlCurrency / s.rTarget;

            if (pnlR <= DecimalConstants<Decimal>::DecimalZero)
            {
              ++nNonPositive;
            }
          }
          else
          {
            if (pnlCurrency <= DecimalConstants<Decimal>::DecimalZero)
            {
              ++nNonPositive;
            }
          }

          // Next-bar target/stop events (first touch exactly at t+1)
          if (s.firstTargetIdx == t + 1)
          {
            ++nTargetNext;
          }
          if (s.firstStopIdx == t + 1)
          {
            ++nStopNext;
          }

          // Median MFE_R so far at t
          if (s.hasTargetR && s.rTarget > DecimalConstants<Decimal>::DecimalZero)
          {
            Decimal mfeR = s.mfeAbsUpTo[t] / s.rTarget;
            mfeRSoFarDoubles.push_back(detail::toDouble<Decimal>(mfeR));
          }
        }

        const double survival    = static_cast<double>(survivors) / static_cast<double>(totalTrades);
        const double fracNonPos  = (survivors > 0)
            ? (static_cast<double>(nNonPositive) / static_cast<double>(survivors))
            : 0.0;

        const double pTargetNext = (survivors > 0)
            ? (static_cast<double>(nTargetNext) / static_cast<double>(survivors))
            : 0.0;

        const double pStopNext   = (survivors > 0)
            ? (static_cast<double>(nStopNext) / static_cast<double>(survivors))
            : 0.0;

        std::vector<double> tmp = std::move(mfeRSoFarDoubles);
        const double medMfeR    = detail::medianOrNaN(tmp);

        result.emplace_back(t, survival, fracNonPos, pTargetNext, pStopNext, medMfeR);
      }

      return result;
    }

  private:
    const ClosedPositionHistory<Decimal>& mClosedHistory;
  };

} // namespace mkc_timeseries
