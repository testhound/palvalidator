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
  class PolicyResult
  {
  public:
    PolicyResult(double avgPnL_R, double hitRate, double avgBarsHeld, int trades)
      : mAvgPnL_R(avgPnL_R),
	mHitRate(hitRate),
	mAvgBarsHeld(avgBarsHeld),
	mTrades(trades)
    {}

    double getAvgPnL_R()   const { return mAvgPnL_R; }
    double getHitRate()    const { return mHitRate; }
    double getAvgBarsHeld()const { return mAvgBarsHeld; }
    int    getTrades()     const { return mTrades; }

  
  private:
    double mAvgPnL_R;
    double mHitRate;
    double mAvgBarsHeld;
    int    mTrades;
  };

  enum class FailureExitFill
    {
      CloseOfK,      // exit at close of bar K
      OpenOfKPlus1   // exit at next bar's open (default)
    };
  
  // ===== MetaExitCalibrator (OO) =====
  template<class Decimal>
  class MetaExitCalibrator
  {
  public:
    explicit MetaExitCalibrator(const ClosedPositionHistory<Decimal>& positionHistory)
      : mClosedPositionHistory(positionHistory) {}

    // Exit at market close of bar K (0 = first bar after entry) IF PnL_R <= thresholdR (or <= 0 currency if no R)
    PolicyResult evaluateFailureToPerformBars(int K,
					      const Decimal& thresholdR,
					      FailureExitFill fill = FailureExitFill::OpenOfKPlus1) const;

    // From bar N onward, add a breakeven stop at entry + dir*(epsilonR * rTarget). (epsilonR can be 0)
    PolicyResult evaluateBreakevenAfterBars(int N, const Decimal& epsilonR) const;

  private:
    class PathArrays
    {
    public:
      PathArrays(bool isLong,
		 const Decimal& entry,
		 bool hasTargetR,
		 const Decimal& rTarget,
		 std::vector<Decimal> open,
		 std::vector<Decimal> high,
		 std::vector<Decimal> low,
		 std::vector<Decimal> close)
	: mIsLong(isLong),
	  mEntry(entry),
	  mHasTargetR(hasTargetR),
	  mRTarget(rTarget),
	  mOpen(std::move(open)),
	  mHigh(std::move(high)),
	  mLow(std::move(low)),
	  mClose(std::move(close)) {}

      bool isLong()              const { return mIsLong; }
      const Decimal& entry()     const { return mEntry; }
      bool hasTargetR()          const { return mHasTargetR; }
      const Decimal& rTarget()   const { return mRTarget; }
      const std::vector<Decimal>& open()  const { return mOpen; }
      const std::vector<Decimal>& high()  const { return mHigh; }
      const std::vector<Decimal>& low()   const { return mLow; }
      const std::vector<Decimal>& close() const { return mClose; }
      int barsHeld()             const { return static_cast<int>(mClose.size()); }

    private:
      bool mIsLong;
      Decimal mEntry;
      bool mHasTargetR;
      Decimal mRTarget;
      std::vector<Decimal> mOpen, mHigh, mLow, mClose; // t = 0..T-1; t=0 is first bar after entry
    };

    PathArrays buildArrays(const std::shared_ptr<TradingPosition<Decimal>>& pos) const;

    // Helpers that return (exitBarIdx, exitPrice)
    std::pair<int, Decimal> simulateFailureToPerform(const PathArrays& p,
						     int K,
						     const Decimal& thresholdR,
						     FailureExitFill fill) const;

    std::pair<int, Decimal> simulateBreakeven(
					      const PathArrays& p, int N, const Decimal& epsilonR) const;

    PolicyResult summarize(const std::vector<std::pair<int, Decimal>>& exits,
			   const std::vector<PathArrays>& paths) const;

  private:
    const ClosedPositionHistory<Decimal>& mClosedPositionHistory;
  };

  template<class Decimal>
  typename MetaExitCalibrator<Decimal>::PathArrays
  MetaExitCalibrator<Decimal>::buildArrays(const std::shared_ptr<TradingPosition<Decimal>>& pos) const
  {
    const bool isLong = pos->isLongPosition();
    const Decimal entry = pos->getEntryPrice();

    // rTarget & availability from position target
    bool hasTargetR = false;
    Decimal rTarget = DecimalConstants<Decimal>::DecimalZero;
    const Decimal target = pos->getProfitTarget();
    if (target > DecimalConstants<Decimal>::DecimalZero)
      {
	hasTargetR = true;
	rTarget = isLong ? (target - entry) : (entry - target);
      }

    // Skip the entry bar; t=0 is the first bar after entry
    std::vector<Decimal> open, high, low, close;
    auto it = pos->beginPositionBarHistory();
    if (it != pos->endPositionBarHistory())
      ++it;

    for (; it != pos->endPositionBarHistory(); ++it)
      {
	const auto& b = it->second;
	open.push_back(b.getOpenValue());
	high.push_back(b.getHighValue());
	low.push_back(b.getLowValue());
	close.push_back(b.getCloseValue());
      }

    return PathArrays(isLong, entry, hasTargetR, rTarget,
		      std::move(open), std::move(high), std::move(low), std::move(close));
  }

  template<class Decimal>
  std::pair<int, Decimal>
  MetaExitCalibrator<Decimal>::simulateFailureToPerform(
							const PathArrays& p, int K, const Decimal& thresholdR, FailureExitFill fill) const
  {
    if (K < 0 || K >= p.barsHeld())
      {
	const int last = p.barsHeld() - 1;
	return { last, p.close()[last] };
      }

    // Evaluate rule at CLOSE[K]
    const Decimal pnlCur = p.isLong() ? (p.close()[K] - p.entry()) : (p.entry() - p.close()[K]);
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
      { // OpenOfKPlus1
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
  MetaExitCalibrator<Decimal>::simulateBreakeven(const PathArrays& p, int N, const Decimal& epsilonR) const
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
	  ? (p.open()[t] <= breakEven || p.low()[t] <= breakEven)
	  : (p.open()[t] >= breakEven || p.high()[t] >= breakEven);

	if (hitStop)
	  return { t, breakEven };
      }

    return { last, p.close()[last] };
  }

  template<class Decimal>
  PolicyResult MetaExitCalibrator<Decimal>::summarize(const std::vector<std::pair<int, Decimal>>& exits,
						      const std::vector<PathArrays>& paths) const
  {
    const size_t n = exits.size();
    if (n == 0)
      return PolicyResult(0.0, 0.0, 0.0, 0);

    double sumPnL_R = 0.0;
    double sumBars  = 0.0;
    int    wins     = 0;

    // Median rTarget (for fallback scaling)
    std::vector<double> rts;
    rts.reserve(paths.size());
    for (const auto& p : paths)
      {
	if (p.hasTargetR() && p.rTarget() > DecimalConstants<Decimal>::DecimalZero)
	  rts.push_back(p.rTarget().getAsDouble());
      }

    double scaleFallback = 1.0;
    if (!rts.empty())
      {
	size_t mid = rts.size()/2;
	std::nth_element(rts.begin(), rts.begin()+mid, rts.end());
	scaleFallback = rts[mid];
      }

    for (size_t i = 0; i < n; ++i)
      {
	const auto& p = paths[i];
	const auto exitIdx = exits[i].first;
	const auto exitPx  = exits[i].second;

	const double barsHeld = static_cast<double>(exitIdx + 1); // t=0 is first day after entry
	sumBars += barsHeld;

	const Decimal pnlCur = p.isLong() ? (exitPx - p.entry()) : (p.entry() - exitPx);

	double pnlR_d = 0.0;
	if (p.hasTargetR() && p.rTarget() > DecimalConstants<Decimal>::DecimalZero)
	  {
	    // Use higher precision by performing division in decimal space first
	    Decimal pnlR_decimal = pnlCur / p.rTarget();
	    pnlR_d = pnlR_decimal.getAsDouble();
	  }
	else
	  {
	    pnlR_d = static_cast<double>(pnlCur.getAsDouble()) / scaleFallback;
	  }

	sumPnL_R += pnlR_d;
	if (pnlR_d > 0.0)
	  ++wins;
      }

    return PolicyResult(
			/*avgPnL_R*/   sumPnL_R / static_cast<double>(n),
			/*hitRate*/    static_cast<double>(wins) / static_cast<double>(n),
			/*avgBarsHeld*/sumBars / static_cast<double>(n),
			/*trades*/     static_cast<int>(n)
			);
  }

  template<class Decimal>
  PolicyResult MetaExitCalibrator<Decimal>::evaluateFailureToPerformBars(int K,
									 const Decimal& thresholdR,
									 FailureExitFill fill) const
  {
    std::vector<PathArrays> paths;
    for (auto it = mClosedPositionHistory.beginTradingPositions(); it != mClosedPositionHistory.endTradingPositions(); ++it) {
      paths.push_back(buildArrays(it->second));
    }

    std::vector<std::pair<int, Decimal>> exits;
    exits.reserve(paths.size());
    for (const auto& p : paths) exits.push_back(simulateFailureToPerform(p, K, thresholdR, fill));

    return summarize(exits, paths);
  }

  template<class Decimal>
  PolicyResult MetaExitCalibrator<Decimal>::evaluateBreakevenAfterBars(int N, const Decimal& epsilonR) const
  {
    std::vector<PathArrays> paths;
    for (auto it = mClosedPositionHistory.beginTradingPositions(); it != mClosedPositionHistory.endTradingPositions(); ++it) {
      paths.push_back(buildArrays(it->second));
    }

    std::vector<std::pair<int, Decimal>> exits;
    exits.reserve(paths.size());
    for (const auto& p : paths) exits.push_back(simulateBreakeven(p, N, epsilonR));

    return summarize(exits, paths);
  }
} // namespace mkc_timeseries
