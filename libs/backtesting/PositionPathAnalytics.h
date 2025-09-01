#pragma once
#include <limits>
#include <stdexcept>
#include "TradingPosition.h"

namespace mkc_timeseries
{
  template<class Decimal>
  class MfeMae
  {
  public:
    // Construct from absolute excursions only.
    explicit MfeMae(const Decimal& mfeAbs, const Decimal& maeAbs)
      : mMaximumFavorableExcursionAbsolute(nonNegative(mfeAbs)),
	mMaximumAdverseExcursionAbsolute(nonNegative(maeAbs)),
	mMaximumFavorableExcursionInTargetR(DecimalConstants<Decimal>::DecimalZero),
	mMaximumAdverseExcursionInStopR(DecimalConstants<Decimal>::DecimalZero),
	mHasTargetR(false), mHasStopR(false)
    {}

    // Construct from absolute excursions + normalized R units.
    explicit MfeMae(const Decimal& mfeAbs, const Decimal& maeAbs,
		    const Decimal& mfeR_Target, bool hasTargetR,
		    const Decimal& maeR_Stop,   bool hasStopR)
      : mMaximumFavorableExcursionAbsolute(nonNegative(mfeAbs)),
	mMaximumAdverseExcursionAbsolute(nonNegative(maeAbs)),
	mMaximumFavorableExcursionInTargetR(hasTargetR ? nonNegative(mfeR_Target)
					    : DecimalConstants<Decimal>::DecimalZero),
	mMaximumAdverseExcursionInStopR(  hasStopR   ? nonNegative(maeR_Stop)
					  : DecimalConstants<Decimal>::DecimalZero),
	mHasTargetR(hasTargetR), mHasStopR(hasStopR)
    {}

    // Construct directly from a TradingPosition path.
    explicit MfeMae(const TradingPosition<Decimal>& position)
    {
      const Decimal zero  = DecimalConstants<Decimal>::DecimalZero;
      const Decimal entry = position.getEntryPrice();
      const bool isLong   = position.isLongPosition();

      Decimal mfeAbs = zero, maeAbs = zero;
      for (auto it = position.beginPositionBarHistory(); it != position.endPositionBarHistory(); ++it)
	{
	  const auto& b = it->second;
	  const Decimal fav = isLong ? (b.getHighValue() - entry) : (entry - b.getLowValue());
	  const Decimal adv = isLong ? (entry - b.getLowValue())  : (b.getHighValue() - entry);
	  if (fav > mfeAbs)
	    mfeAbs = fav;
	  if (adv > maeAbs)
	    maeAbs = adv;
	}
      
      const Decimal target   = position.getProfitTarget();
      const Decimal stop     = position.getStopLoss();
      const bool hasTargetR  = (target > zero);
      const bool hasStopR    = (stop   > zero);

      Decimal rTarget = zero, rStop = zero;

      if (hasTargetR)
	rTarget = isLong ? (target - entry) : (entry - target);

      if (hasStopR)
	rStop   = isLong ? (entry - stop)   : (stop - entry);

      Decimal mfeR = zero,   maeR = zero;
      if (hasTargetR && rTarget > zero)
	mfeR = mfeAbs / rTarget;

      if (hasStopR   && rStop   > zero)
	maeR = maeAbs / rStop;

      // Assign after validation
      mMaximumFavorableExcursionAbsolute = nonNegative(mfeAbs);
      mMaximumAdverseExcursionAbsolute   = nonNegative(maeAbs);
      mMaximumFavorableExcursionInTargetR= hasTargetR ? nonNegative(mfeR) : zero;
      mMaximumAdverseExcursionInStopR    = hasStopR   ? nonNegative(maeR) : zero;
      mHasTargetR = hasTargetR;
      mHasStopR   = hasStopR;
    }

    // Accessors
    const Decimal& getMaximumFavorableExcursionAbsolute() const
    {
      return mMaximumFavorableExcursionAbsolute;
    }

    const Decimal& getMaximumAdverseExcursionAbsolute() const
    {
      return mMaximumAdverseExcursionAbsolute;
    }
    const Decimal& getMaximumFavorableExcursionInTargetR() const
    {
      return mMaximumFavorableExcursionInTargetR;
    }
    
    const Decimal& getMaximumAdverseExcursionInStopR() const
    {
      return mMaximumAdverseExcursionInStopR;
    }
    
    bool hasTargetR() const
    {
      return mHasTargetR;
    }
    
    bool hasStopR() const
    {
      return mHasStopR;
    }

  private:
    static Decimal nonNegative(const Decimal& v) {
      const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
      return (v < zero) ? zero : v;
    }

    Decimal mMaximumFavorableExcursionAbsolute{DecimalConstants<Decimal>::DecimalZero};
    Decimal mMaximumAdverseExcursionAbsolute{DecimalConstants<Decimal>::DecimalZero};
    Decimal mMaximumFavorableExcursionInTargetR{DecimalConstants<Decimal>::DecimalZero};
    Decimal mMaximumAdverseExcursionInStopR{DecimalConstants<Decimal>::DecimalZero};
    bool mHasTargetR{false};
    bool mHasStopR{false};
  };


  // =============== PathStats ===============
  // Immutable aggregate built either (a) from a TradingPosition, or (b) from explicit parts.
  // No default constructor.
  template<class Decimal>
    class PathStats {
  public:
    // Build from a TradingPosition path (computes timing/route + give-back + MFE/MAE).
    explicit PathStats(const TradingPosition<Decimal>& position)
      : PathStats(buildFromPosition(position)) {}

    // Build from explicit parts (useful for tests or alternate data sources).
    explicit PathStats(MfeMae<Decimal> mfeMae,
		       int firstTargetTouchBarIndex,
		       int firstStopTouchBarIndex,
		       unsigned int barsHeld,
		       const Decimal& drawdownFromMfeAbsolute,
		       const Decimal& drawdownFromMfeFraction,
		       bool targetTouchedAtOpen,
		       bool stopTouchedAtOpen)
      : mMfeMae(std::move(mfeMae)),
      mFirstTargetTouchBarIndex(firstTargetTouchBarIndex),
      mFirstStopTouchBarIndex(firstStopTouchBarIndex),
      mBarsHeld(barsHeld),
      mDrawdownFromMfeAbsolute(nonNegative(drawdownFromMfeAbsolute)),
      mDrawdownFromMfeFraction(nonNegative(drawdownFromMfeFraction)),
      mTargetTouchedAtOpen(targetTouchedAtOpen),
      mStopTouchedAtOpen(stopTouchedAtOpen) {}

    // Accessors
    const MfeMae<Decimal>& getMfeMae() const { return mMfeMae; }
    int  getFirstTargetTouchBarIndex() const { return mFirstTargetTouchBarIndex; } // -1 if never
    int  getFirstStopTouchBarIndex()   const { return mFirstStopTouchBarIndex; }   // -1 if never
    bool didTargetEverTouch()          const { return mFirstTargetTouchBarIndex >= 0; }
    bool didStopEverTouch()            const { return mFirstStopTouchBarIndex   >= 0; }
    unsigned int getBarsHeld()         const { return mBarsHeld; }
    const Decimal& getDrawdownFromMaximumFavorableExcursionAbsolute() const { return mDrawdownFromMfeAbsolute; }
    const Decimal& getDrawdownFromMaximumFavorableExcursionFraction() const { return mDrawdownFromMfeFraction; }
    // Convenience aliases for shorter method names
    const Decimal& getDrawdownFromMfeAbsolute() const { return mDrawdownFromMfeAbsolute; }
    const Decimal& getDrawdownFromMfeFraction() const { return mDrawdownFromMfeFraction; }
    bool targetTouchedAtOpen() const { return mTargetTouchedAtOpen; }
    bool stopTouchedAtOpen()   const { return mStopTouchedAtOpen; }

  private:
  static PathStats buildFromPosition(const TradingPosition<Decimal>& position)
  {
    using DC = DecimalConstants<Decimal>;
    const Decimal zero = DC::DecimalZero;
    const Decimal one  = DC::DecimalOne;
    const Decimal hund = Decimal(100);

    const Decimal entry = position.getEntryPrice();
    const bool    isLong = position.isLongPosition();

    // Raw values cached on the position (may be ABSOLUTE or PERCENT)
    const Decimal rawTarget = position.getProfitTarget();
    const Decimal rawStop   = position.getStopLoss();

    if (rawTarget <= zero)
      throw std::invalid_argument("PathStats::buildFromPosition requires profit target to be set (cannot be zero or negative)");
    if (rawStop <= zero)
      throw std::invalid_argument("PathStats::buildFromPosition requires stop loss to be set (cannot be zero or negative)");

    // Heuristic: live backtester stores PERCENTS (e.g., 10, 5). Tests that build synthetic
    // positions usually set ABSOLUTE prices near entry (e.g., 110, 95 for entry 100).
    // If both cached values are "much smaller" than entry, treat them as percents.
    Decimal target = rawTarget;
    Decimal stop   = rawStop;
    if (entry > zero) {
      const Decimal threshold = entry * Decimal("0.8"); // 80% of entry
      const bool looksPercent = (rawTarget < threshold) && (rawStop < threshold);
      if (looksPercent) {
        const Decimal pctTarget = rawTarget / hund;
        const Decimal pctStop   = rawStop   / hund;
        target = isLong ? entry * (one + pctTarget) : entry * (one - pctTarget);
        stop   = isLong ? entry * (one - pctStop)   : entry * (one + pctStop);
      }
    }

    // First-touch bookkeeping and drawdown-from-MFE
    int  firstTargetIdx = -1;
    int  firstStopIdx   = -1;
    bool tgtAtOpen      = false;
    bool stpAtOpen      = false;

    Decimal mfeAbs = zero;  // peak favorable excursion vs entry
    int barIdx = -1;

    for (auto it = position.beginPositionBarHistory(); it != position.endPositionBarHistory(); ++it) {
      ++barIdx;
      const auto& b = it->second;
      const Decimal o = b.getOpenValue();
      const Decimal h = b.getHighValue();
      const Decimal l = b.getLowValue();

      // Update peak favorable excursion vs entry
      const Decimal fav = isLong ? (h - entry) : (entry - l);
      if (fav > mfeAbs) mfeAbs = fav;

      // ---- STOP precedence on the same bar ----
      if (firstStopIdx < 0) {
        const bool stopTouchedThisBar = isLong ? (o <= stop || l <= stop)
                                               : (o >= stop || h >= stop);
        if (stopTouchedThisBar) {
          firstStopIdx = barIdx;
          stpAtOpen    = isLong ? (o <= stop) : (o >= stop);
        }
      }

      if (firstTargetIdx < 0) {
        // If stop already touched this bar, target cannot claim this bar
        bool stopTouchedThisBar = (firstStopIdx == barIdx);
        if (!stopTouchedThisBar) {
          stopTouchedThisBar = isLong ? (o <= stop || l <= stop)
                                      : (o >= stop || h >= stop);
        }

        const bool targetTouchedThisBar = isLong ? (o >= target || h >= target)
                                                 : (o <= target || l <= target);
        if (!stopTouchedThisBar && targetTouchedThisBar) {
          firstTargetIdx = barIdx;
          tgtAtOpen      = isLong ? (o >= target) : (o <= target);
        }
      }
    }

    // Terminal price for drawdown-from-MFE
    Decimal terminal;
    if (position.isPositionClosed()) {
      terminal = position.getExitPrice();
    } else {
      auto lastIt = position.endPositionBarHistory(); --lastIt;
      terminal = lastIt->second.getCloseValue();
    }

    Decimal favorableAtEnd = isLong ? (terminal - entry) : (entry - terminal);
    if (favorableAtEnd < zero) favorableAtEnd = zero;

    const Decimal ddAbs  = (mfeAbs > favorableAtEnd) ? (mfeAbs - favorableAtEnd) : zero;
    const Decimal ddFrac = (mfeAbs > zero) ? (ddAbs / mfeAbs) : zero;

    // Let MfeMae compute its own R-units from the position (works for both modes)
    MfeMae<Decimal> mfeMae(position);

    return PathStats(mfeMae,
                     firstTargetIdx,
                     firstStopIdx,
                     position.getNumBarsInPosition(),
                     ddAbs,
                     ddFrac,
                     tgtAtOpen,
                     stpAtOpen);
  }
    
    static Decimal nonNegative(const Decimal& v)
    {
      const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
      return (v < zero) ? zero : v;
    }

    MfeMae<Decimal> mMfeMae;
    int  mFirstTargetTouchBarIndex{-1};
    int  mFirstStopTouchBarIndex{-1};
    unsigned int mBarsHeld{0};
    Decimal mDrawdownFromMfeAbsolute{DecimalConstants<Decimal>::DecimalZero};
    Decimal mDrawdownFromMfeFraction{DecimalConstants<Decimal>::DecimalZero};
    bool mTargetTouchedAtOpen{false};
    bool mStopTouchedAtOpen{false};
  };
} // namespace mkc_timeseries
