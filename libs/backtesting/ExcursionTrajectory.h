// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

/**
 * @file ExcursionTrajectory.h
 *
 * @brief Per-bar MFE/MAE trajectory construction and query for a trading
 *        position, computed from the position's preserved bar history.
 *
 * Provides:
 *   - ExcursionTrajectoryPoint<Decimal>: immutable value object representing
 *     running MFE and MAE at a single bar.
 *   - ExcursionTrajectory<Decimal>: sequence of trajectory points plus
 *     domain-meaningful queries (indexed access, terminal values, threshold
 *     scans).
 *
 * The trajectory is built by iterating a TradingPosition's bar history,
 * maintaining running high and low water marks initialized to the entry
 * price (the "purist" first-bar convention documented alongside the MFE/MAE
 * accessors on TradingPosition). Under this convention the trajectory point
 * at bar 1 (the entry bar) always has MFE = MAE = 0.
 *
 * Running MFE and MAE are monotonically non-decreasing across the trajectory
 * by construction, which is an invariant the threshold queries depend on.
 *
 * Threshold-query semantics ("exceeds") use strict greater-than (>). A
 * threshold equal to a trajectory value is not considered exceeded.
 */

#ifndef __EXCURSION_TRAJECTORY_H
#define __EXCURSION_TRAJECTORY_H 1

#include <vector>
#include <stdexcept>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TradingPosition.h"
#include "TradingPositionException.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  using boost::posix_time::ptime;

  /**
   * @class ExcursionTrajectoryPoint
   * @brief Immutable value object carrying running MFE and MAE at a single bar.
   *
   * Invariants enforced at construction:
   *   - barNumber >= 1 (bar numbers are 1-indexed; bar 1 is the entry bar)
   *   - mfe >= 0
   *   - mae >= 0
   *
   * Construction violations throw TradingPositionException.
   */
  template <class Decimal>
  class ExcursionTrajectoryPoint
  {
  public:
    ExcursionTrajectoryPoint(unsigned int barNumber,
                             const ptime& barDateTime,
                             const Decimal& mfe,
                             const Decimal& mae)
      : mBarNumber(barNumber),
        mBarDateTime(barDateTime),
        mMfe(mfe),
        mMae(mae)
    {
      if (barNumber == 0)
        throw TradingPositionException(
            "ExcursionTrajectoryPoint: barNumber must be >= 1 (bar 1 is the entry bar)");
      if (mfe < DecimalConstants<Decimal>::DecimalZero)
        throw TradingPositionException(
            "ExcursionTrajectoryPoint: MFE must be non-negative");
      if (mae < DecimalConstants<Decimal>::DecimalZero)
        throw TradingPositionException(
            "ExcursionTrajectoryPoint: MAE must be non-negative");
    }

    ExcursionTrajectoryPoint(const ExcursionTrajectoryPoint&) = default;
    ExcursionTrajectoryPoint& operator=(const ExcursionTrajectoryPoint&) = default;
    ExcursionTrajectoryPoint(ExcursionTrajectoryPoint&&) = default;
    ExcursionTrajectoryPoint& operator=(ExcursionTrajectoryPoint&&) = default;

    unsigned int getBarNumber() const { return mBarNumber; }
    const ptime& getBarDateTime() const { return mBarDateTime; }
    const Decimal& getMfe() const { return mMfe; }
    const Decimal& getMae() const { return mMae; }

  private:
    unsigned int mBarNumber;
    ptime        mBarDateTime;
    Decimal      mMfe;
    Decimal      mMae;
  };

  template <class Decimal>
  bool operator==(const ExcursionTrajectoryPoint<Decimal>& lhs,
                  const ExcursionTrajectoryPoint<Decimal>& rhs)
  {
    return lhs.getBarNumber()   == rhs.getBarNumber()   &&
           lhs.getBarDateTime() == rhs.getBarDateTime() &&
           lhs.getMfe()         == rhs.getMfe()         &&
           lhs.getMae()         == rhs.getMae();
  }

  template <class Decimal>
  bool operator!=(const ExcursionTrajectoryPoint<Decimal>& lhs,
                  const ExcursionTrajectoryPoint<Decimal>& rhs)
  {
    return !(lhs == rhs);
  }

  /**
   * @class ExcursionTrajectory
   * @brief Ordered sequence of ExcursionTrajectoryPoint objects, one per bar
   *        of a trading position, with threshold and terminal-value queries.
   *
   * Constructed eagerly from a TradingPosition: construction is O(N) in the
   * position's bar count. All queries are O(1) or O(N) linear scan, which is
   * adequate for trajectories that are short relative to data volume.
   *
   * Works for both open and closed positions — the underlying bar history
   * is preserved across the open->closed state transition.
   */
  template <class Decimal>
  class ExcursionTrajectory
  {
  public:
    using PointVector    = std::vector<ExcursionTrajectoryPoint<Decimal>>;
    using const_iterator = typename PointVector::const_iterator;

    /**
     * Build the trajectory by iterating the position's bar history,
     * maintaining running water marks initialized to entry price (purist
     * convention), and resolving favorable/adverse direction via
     * position.isLongPosition().
     *
     * Throws TradingPositionException if the position's bar history is
     * empty (which should not occur for any position constructed through
     * the normal API, since OpenPositionHistory always inserts the entry
     * bar). The check is defensive.
     */
    explicit ExcursionTrajectory(const TradingPosition<Decimal>& position)
    {
      auto it  = position.beginPositionBarHistory();
      auto end = position.endPositionBarHistory();

      if (it == end)
        throw TradingPositionException(
            "ExcursionTrajectory: cannot build trajectory from a position with no bar history");

      const Decimal entryPrice = position.getEntryPrice();
      const bool    isLong     = position.isLongPosition();

      // Running water marks, initialized to entry price. Under the purist
      // convention the entry bar's own high and low do not contribute; only
      // subsequent bars' extremes do. This is enforced below by the
      // barNumber > 1 guard around the update step.
      Decimal highWaterMark = entryPrice;
      Decimal lowWaterMark  = entryPrice;

      unsigned int barNumber = 1;
      for (; it != end; ++it, ++barNumber)
      {
        const OpenPositionBar<Decimal>& bar = it->second;

        if (barNumber > 1)
        {
          const Decimal& barHigh = bar.getHighValue();
          const Decimal& barLow  = bar.getLowValue();
          if (barHigh > highWaterMark) highWaterMark = barHigh;
          if (barLow  < lowWaterMark)  lowWaterMark  = barLow;
        }

        Decimal mfe;
        Decimal mae;
        if (isLong)
        {
          mfe = (highWaterMark > entryPrice)
                  ? ((highWaterMark - entryPrice) / entryPrice)
                      * DecimalConstants<Decimal>::DecimalOneHundred
                  : DecimalConstants<Decimal>::DecimalZero;
          mae = (lowWaterMark < entryPrice)
                  ? ((entryPrice - lowWaterMark) / entryPrice)
                      * DecimalConstants<Decimal>::DecimalOneHundred
                  : DecimalConstants<Decimal>::DecimalZero;
        }
        else
        {
          // Short position: favorable = price drop, adverse = price rise.
          mfe = (lowWaterMark < entryPrice)
                  ? ((entryPrice - lowWaterMark) / entryPrice)
                      * DecimalConstants<Decimal>::DecimalOneHundred
                  : DecimalConstants<Decimal>::DecimalZero;
          mae = (highWaterMark > entryPrice)
                  ? ((highWaterMark - entryPrice) / entryPrice)
                      * DecimalConstants<Decimal>::DecimalOneHundred
                  : DecimalConstants<Decimal>::DecimalZero;
        }

        mPoints.emplace_back(barNumber, bar.getDateTime(), mfe, mae);
      }
    }

    ExcursionTrajectory(const ExcursionTrajectory&) = default;
    ExcursionTrajectory& operator=(const ExcursionTrajectory&) = default;
    ExcursionTrajectory(ExcursionTrajectory&&) = default;
    ExcursionTrajectory& operator=(ExcursionTrajectory&&) = default;

    // --- Structural access -------------------------------------------------

    size_t size() const { return mPoints.size(); }
    bool   empty() const { return mPoints.empty(); }

    const_iterator begin() const { return mPoints.begin(); }
    const_iterator end()   const { return mPoints.end(); }

    // --- Indexed access ----------------------------------------------------

    /**
     * Zero-indexed access into the underlying sequence. Throws std::out_of_range
     * if index >= size().
     */
    const ExcursionTrajectoryPoint<Decimal>& at(size_t index) const
    {
      if (index >= mPoints.size())
        throw std::out_of_range(
            "ExcursionTrajectory::at: index out of range");
      return mPoints[index];
    }

    /**
     * One-indexed access by bar number, matching the semantics of
     * ExcursionTrajectoryPoint::getBarNumber(). atBar(1) returns the entry
     * bar point, atBar(size()) returns the last. Throws std::out_of_range
     * for barNumber == 0 or barNumber > size().
     */
    const ExcursionTrajectoryPoint<Decimal>& atBar(unsigned int barNumber) const
    {
      if (barNumber == 0 || barNumber > mPoints.size())
        throw std::out_of_range(
            "ExcursionTrajectory::atBar: barNumber out of range (must be in [1, size()])");
      return mPoints[barNumber - 1];
    }

    // --- Terminal values ---------------------------------------------------

    /**
     * MFE at the final bar of the trajectory. Agrees with
     * TradingPosition::getMaxFavorableExcursion() on the same position.
     * Throws if trajectory is empty (which construction forbids; this check
     * is defensive for move-from states).
     */
    const Decimal& terminalMfe() const
    {
      if (mPoints.empty())
        throw std::out_of_range("ExcursionTrajectory::terminalMfe: trajectory is empty");
      return mPoints.back().getMfe();
    }

    /**
     * MAE at the final bar of the trajectory. Agrees with
     * TradingPosition::getMaxAdverseExcursion() on the same position.
     */
    const Decimal& terminalMae() const
    {
      if (mPoints.empty())
        throw std::out_of_range("ExcursionTrajectory::terminalMae: trajectory is empty");
      return mPoints.back().getMae();
    }

    // --- Threshold queries -------------------------------------------------

    /**
     * Return the bar number of the first point whose running MFE strictly
     * exceeds the given threshold (i.e. point.getMfe() > threshold).
     * Returns 0 if no point exceeds the threshold.
     *
     * "Exceeds" is strict: a trajectory whose MFE touches the threshold but
     * does not surpass it returns 0 for that threshold.
     */
    unsigned int firstBarWhereMfeExceeds(const Decimal& threshold) const
    {
      for (const auto& p : mPoints)
      {
        if (p.getMfe() > threshold)
          return p.getBarNumber();
      }
      return 0;
    }

    /**
     * Return the bar number of the first point whose running MAE strictly
     * exceeds the given threshold. Returns 0 if no point exceeds it.
     */
    unsigned int firstBarWhereMaeExceeds(const Decimal& threshold) const
    {
      for (const auto& p : mPoints)
      {
        if (p.getMae() > threshold)
          return p.getBarNumber();
      }
      return 0;
    }

  private:
    PointVector mPoints;
  };

} // namespace mkc_timeseries

#endif // __EXCURSION_TRAJECTORY_H
