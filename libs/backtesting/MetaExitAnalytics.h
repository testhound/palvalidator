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
  template <class Decimal>
  class BarAgeSnapshot
  {
  public:
    BarAgeSnapshot(int barAge,
		   const Decimal& pnlR_TargetAtClose,
		   bool hasTargetR,
		   bool targetTouchedByT,
		   bool stopTouchedByT)
      : mBarAge(barAge),
	mPnlR_TargetAtClose(pnlR_TargetAtClose),
	mHasTargetR(hasTargetR),
	mTargetTouchedByT(targetTouchedByT),
	mStopTouchedByT(stopTouchedByT)
    {}

    // 0 = first bar after entry
    int getBarAge() const
    {
      return mBarAge;
    }
    const Decimal& getPnlR_TargetAtClose() const
    {
      return mPnlR_TargetAtClose;
    }

    bool hasTargetR() const
    {
      return mHasTargetR;
    }

    bool getTargetTouchedByT() const
    {
      return mTargetTouchedByT;
    }

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

  // ------------------------ BarAgeAggregate ------------------------
  class BarAgeAggregate
  {
  public:
    BarAgeAggregate(int barAge,
		    double survival,
		    double fracNonPositive,
		    double probTargetNextBar,
		    double probStopNextBar,
		    double medianMfeRSoFar)
      : mBarAge(barAge),
	mSurvival(survival),
	mFracNonPositive(fracNonPositive),
	mProbTargetNextBar(probTargetNextBar),
	mProbStopNextBar(probStopNextBar),
	mMedianMfeRSoFar(medianMfeRSoFar)
    {}

    int getBarAge() const
    {
      return mBarAge;
    }

    double getSurvival() const
    {
      return mSurvival;
    }

    double getFracNonPositive() const
    {
      return mFracNonPositive;
    }

    double getProbTargetNextBar() const
    {
      return mProbTargetNextBar;
    }

    double getProbStopNextBar() const
    {
      return mProbStopNextBar;
    }

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

  namespace detail
  {

    // Simple conversion to double for typical Decimal types
    template <class Decimal>
    static inline double toDouble(const Decimal& v)
    {
      return v.getAsDouble();
    }

    // median for vector<double>, returns NaN if empty
    static inline double medianOrNaN(std::vector<double>& v)
    {
      if (v.empty())
	return std::numeric_limits<double>::quiet_NaN();
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
      bool isLong{true};         // <-- used for PnL sign
      std::vector<Decimal> closes;     // close at each bar index (0..barsHeld-1)
      std::vector<Decimal> mfeAbsUpTo; // MFE (abs currency) up to each t
    };

    // Scan a single TradingPosition's path.
    // Uses PathStats for conservative same-bar precedence,
    // and computes hasTargetR/rTarget directly from the position's target price.
    template <class Decimal>
    static PerPositionScan<Decimal>
    scanPosition(const std::shared_ptr<TradingPosition<Decimal>>& pos)
    {
      PerPositionScan<Decimal> s;

      s.entry = pos->getEntryPrice();
      s.isLong = pos->isLongPosition();
      s.directionSign = s.isLong ? +1 : -1;

      // First-touch indices via PathStats (stop-first precedence already enforced)
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

      // Build per-bar close[] and MFE_abs_up_to[]
      // Skip the first bar (entry bar) since t=0 should be "first bar after entry"
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
	  if (fav > mfeAbs) mfeAbs = fav;
	  s.mfeAbsUpTo.push_back(mfeAbs);
	}
      s.barsHeld = static_cast<int>(s.closes.size());

      return s;
    }

  } // namespace detail

  // ------------------------ MetaExitAnalytics ------------------------
  template <class Decimal>
  class MetaExitAnalytics
  {
  public:
    explicit MetaExitAnalytics(const ClosedPositionHistory<Decimal>& closedHistory)
      : mClosedHistory(closedHistory)
    {}

    // Build per-trade snapshots for t = 0..maxBars-1 (0 = first bar after entry).
    std::vector<BarAgeSnapshot<Decimal>> buildBarAgeSnapshots(int maxBars) const
    {
      std::vector<BarAgeSnapshot<Decimal>> out;
      if (maxBars <= 0) return out;

      for (auto it = mClosedHistory.beginTradingPositions();
	   it != mClosedHistory.endTradingPositions(); ++it)
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
	      const bool stopByT   = (scan.firstStopIdx >= 0 && scan.firstStopIdx <= t);

	      out.emplace_back(t, pnlR, scan.hasTargetR, targetByT, stopByT);
	    }
	}

      return out;
    }

    // Aggregate to survival/hazard style statistics per bar age t.
    std::vector<BarAgeAggregate> summarizeByBarAge(int maxBars) const
    {
      std::vector<BarAgeAggregate> result;
      if (maxBars <= 0)
	return result;

      // Pre-scan all positions once
      std::vector<detail::PerPositionScan<Decimal>> scans;
      for (auto it = mClosedHistory.beginTradingPositions();
	   it != mClosedHistory.endTradingPositions(); ++it)
	{
	  scans.push_back(detail::scanPosition<Decimal>(it->second));
	}
      const int totalTrades = static_cast<int>(scans.size());
      if (totalTrades == 0)
	return result;

      for (int t = 0; t < maxBars; ++t)
	{
	  int survivors = 0;
	  int nNonPositive = 0;
	  int nTargetNext = 0;
	  int nStopNext = 0;
	  std::vector<double> mfeRSoFarDoubles;

	  for (const auto& s : scans)
	    {
	      if (s.barsHeld <= t) continue; // not alive at t
	      ++survivors;

	      // ---- PnL classification at close of bar t ----
	      // If R available, classify by pnlR; otherwise fallback to currency PnL sign.
	      Decimal pnlCurrency = s.isLong
		? (s.closes[t] - s.entry)
		: (s.entry - s.closes[t]);

	      if (s.hasTargetR && s.rTarget > DecimalConstants<Decimal>::DecimalZero)
		{
		  Decimal pnlR = pnlCurrency / s.rTarget;
		  if (pnlR <= DecimalConstants<Decimal>::DecimalZero) ++nNonPositive;
		}
	      else
		{
		  if (pnlCurrency <= DecimalConstants<Decimal>::DecimalZero) ++nNonPositive;
		}

	      // Next-bar target/stop events (first touch exactly at t+1)
	      if (s.firstTargetIdx == t + 1) ++nTargetNext;
	      if (s.firstStopIdx   == t + 1) ++nStopNext;

	      // Median MFE_R so far at t
	      if (s.hasTargetR && s.rTarget > DecimalConstants<Decimal>::DecimalZero)
		{
		  Decimal mfeR = s.mfeAbsUpTo[t] / s.rTarget;
		  mfeRSoFarDoubles.push_back(detail::toDouble<Decimal>(mfeR));
		}
	    }

	  const double survival    = static_cast<double>(survivors) / static_cast<double>(totalTrades);
	  const double fracNonPos  = (survivors > 0) ? (static_cast<double>(nNonPositive) / static_cast<double>(survivors)) : 0.0;
	  const double pTargetNext = (survivors > 0) ? (static_cast<double>(nTargetNext) / static_cast<double>(survivors)) : 0.0;
	  const double pStopNext   = (survivors > 0) ? (static_cast<double>(nStopNext)   / static_cast<double>(survivors)) : 0.0;

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
