// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

/**
 * @file ExcursionTest.cpp
 *
 * @brief Unit tests for Phase 1 MFE/MAE functionality:
 *
 *   1. Terminal MFE/MAE accessors on TradingPosition (long and short).
 *   2. ExcursionTrajectoryPoint value-object invariants.
 *   3. ExcursionTrajectory construction, indexed access, terminal values,
 *      monotonicity invariant, and threshold-query semantics.
 *
 * Test style follows Catch2 v2 (TEST_CASE / SECTION / REQUIRE). The single
 * ADAPTATION NOTE at the top of the file isolates the project-specific
 * factory helpers that construct OHLC entries and TradingVolume values;
 * if the existing test suite already exposes equivalents with different
 * names, point the aliases in that section at them and the assertions
 * below should compile unchanged.
 */

#include <catch2/catch_test_macros.hpp>  // or <catch.hpp> for Catch2 v2 single-header

#include "TradingPosition.h"
#include "ExcursionTrajectory.h"
#include "DecimalConstants.h"
#include "TimeSeriesEntry.h"
#include "TimeFrame.h"
#include "TradingVolume.h"
#include "number.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <sstream>
#include <iomanip>
#include <string>
#include <tuple>
#include <vector>
#include <memory>

using namespace mkc_timeseries;
using boost::posix_time::ptime;
using boost::gregorian::date;

// ============================================================================
// ADAPTATION NOTE
// ----------------------------------------------------------------------------
// The following three helpers isolate the project's type choices:
//
//   - DecimalType == num::DefaultNumber == dec::decimal<8> (from number.h).
//   - D(double) constructs a DecimalType via num::fromString, the canonical
//     string-based path. This avoids any ambiguity over whether dec::decimal<8>
//     accepts a direct double constructor in the project's build configuration.
//   - makeBar(...) constructs an OHLCTimeSeriesEntry using the (date, O, H, L,
//     C, volume, timeframe) overload confirmed in TimeSeriesEntry.h.
//   - oneShare() constructs a one-unit TradingVolume confirmed in
//     TradingVolume.h.
//
// If the project's existing backtester test suite already exposes equivalents
// of these helpers, it's safe to swap them in here; everything below depends
// only on D(...), makeBar(...), and oneShare().
// ============================================================================

using DecimalType = num::DefaultNumber;

/**
 * Construct a DecimalType from a double by serializing through a string.
 * Uses fixed-point notation with enough decimal places to cover the 8-digit
 * scale of DefaultNumber.
 */
static DecimalType D(double v)
{
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(10) << v;
  return num::fromString<DecimalType>(oss.str());
}

/** Build an OHLCTimeSeriesEntry at daily time frame. Defaults volume to zero. */
static OHLCTimeSeriesEntry<DecimalType>
makeBar(const date& d,
        double o, double h, double l, double c,
        double vol = 0.0)
{
  return OHLCTimeSeriesEntry<DecimalType>(
      d,
      D(o), D(h), D(l), D(c),
      D(vol),
      TimeFrame::DAILY);
}

/** One unit of TradingVolume for share-denominated assets. */
static TradingVolume oneShare()
{
  return TradingVolume(1, TradingVolume::SHARES);
}

// ============================================================================
// Test fixtures and helpers (not project-specific)
// ============================================================================

/**
 * Build a closed long position with a scripted sequence of post-entry bars
 * and a given exit price/date. Bars are added in order. The entry price
 * equals the entry bar's Open. Each subsequent bar's OHLC is taken from the
 * provided vector, one bar per day starting the day after entry.
 */
struct LongTradeScript
{
  date         entryDate;
  double       entryPrice;
  double       entryBarHigh;
  double       entryBarLow;
  std::vector<std::tuple<double,double,double,double>> postEntryOHLC; // O,H,L,C
  date         exitDate;
  double       exitPrice;
};

static std::shared_ptr<TradingPositionLong<DecimalType>>
buildClosedLong(const LongTradeScript& s)
{
  auto entryBar = makeBar(s.entryDate,
                          s.entryPrice, s.entryBarHigh,
                          s.entryBarLow,  s.entryPrice);
  auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
      std::string("TEST"), D(s.entryPrice), entryBar, oneShare());

  date d = s.entryDate;
  for (const auto& ohlc : s.postEntryOHLC)
  {
    d = d + boost::gregorian::days(1);
    auto bar = makeBar(d,
                       std::get<0>(ohlc), std::get<1>(ohlc),
                       std::get<2>(ohlc), std::get<3>(ohlc));
    pos->addBar(bar);
  }
  pos->ClosePosition(s.exitDate, D(s.exitPrice));
  return pos;
}

struct ShortTradeScript
{
  date         entryDate;
  double       entryPrice;
  double       entryBarHigh;
  double       entryBarLow;
  std::vector<std::tuple<double,double,double,double>> postEntryOHLC;
  date         exitDate;
  double       exitPrice;
};

static std::shared_ptr<TradingPositionShort<DecimalType>>
buildClosedShort(const ShortTradeScript& s)
{
  auto entryBar = makeBar(s.entryDate,
                          s.entryPrice, s.entryBarHigh,
                          s.entryBarLow,  s.entryPrice);
  auto pos = std::make_shared<TradingPositionShort<DecimalType>>(
      std::string("TEST"), D(s.entryPrice), entryBar, oneShare());

  date d = s.entryDate;
  for (const auto& ohlc : s.postEntryOHLC)
  {
    d = d + boost::gregorian::days(1);
    auto bar = makeBar(d,
                       std::get<0>(ohlc), std::get<1>(ohlc),
                       std::get<2>(ohlc), std::get<3>(ohlc));
    pos->addBar(bar);
  }
  pos->ClosePosition(s.exitDate, D(s.exitPrice));
  return pos;
}

// Compare Decimal values with a tolerance. Decimal arithmetic is exact for
// the ratios in these tests, but if the project's Decimal type is a
// fixed-precision wrapper the comparison still survives trailing digits.
static bool approxEqual(const DecimalType& a, const DecimalType& b, double tol = 1e-6)
{
  DecimalType diff = (a > b) ? (a - b) : (b - a);
  return num::to_long_double(diff) <= tol;
}


// ============================================================================
// 1. Terminal MFE/MAE on TradingPosition (Long)
// ============================================================================

TEST_CASE("TradingPositionLong terminal MFE/MAE", "[trading_position][mfe_mae]")
{
  // Entry at 100. Bar 2 reaches high=105, low=99. Bar 3 pulls back to
  // low=97, high=103. Exit at 102.
  // Running high water mark across post-entry bars: max(105, 103) = 105
  // Running low water mark across post-entry bars:  min(99,  97)  = 97
  // Long MFE = (105 - 100)/100 * 100 = 5.0
  // Long MAE = (100 - 97)/100  * 100 = 3.0
  LongTradeScript s{
    date(2024,  1,  8), 100.0, 100.0, 100.0,  // entry-bar OHLC (high/low = entry per purist convention)
    {
      std::make_tuple(100.0, 105.0, 99.0, 104.0),
      std::make_tuple(103.0, 103.0, 97.0, 102.0)
    },
    date(2024, 1, 10), 102.0
  };
  auto pos = buildClosedLong(s);

  SECTION("MFE reflects the highest post-entry high")
  {
    REQUIRE(approxEqual(pos->getMaxFavorableExcursion(), D(5.0)));
  }
  SECTION("MAE reflects the lowest post-entry low")
  {
    REQUIRE(approxEqual(pos->getMaxAdverseExcursion(),  D(3.0)));
  }
  SECTION("MFE and MAE are non-negative")
  {
    REQUIRE(pos->getMaxFavorableExcursion() >= DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(pos->getMaxAdverseExcursion()  >= DecimalConstants<DecimalType>::DecimalZero);
  }
}

TEST_CASE("TradingPositionLong: winning trade with adverse excursion first",
          "[trading_position][mfe_mae]")
{
  // Price dips to 95 before rallying to 110. Exit at 108.
  // MFE = 10.0, MAE = 5.0 — MAE must capture the dip even though the
  // trade closes winning.
  LongTradeScript s{
    date(2024, 2, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 101.0,  95.0,  96.0),  // dip first
      std::make_tuple( 96.0, 110.0,  96.0, 108.0)   // then rally
    },
    date(2024, 2, 3), 108.0
  };
  auto pos = buildClosedLong(s);
  REQUIRE(approxEqual(pos->getMaxFavorableExcursion(), D(10.0)));
  REQUIRE(approxEqual(pos->getMaxAdverseExcursion(),   D( 5.0)));
  REQUIRE(pos->isWinningPosition());
}

TEST_CASE("TradingPositionLong: closed on entry bar has zero MFE/MAE",
          "[trading_position][mfe_mae][purist_convention]")
{
  // No post-entry bars. Under the purist convention the entry bar's own
  // high/low do not contribute — MFE and MAE are both zero even if the
  // entry bar had a large range.
  auto entryBar = makeBar(date(2024, 3, 1), 100.0, 110.0, 90.0, 100.0);
  auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
      std::string("TEST"), D(100.0), entryBar, oneShare());
  pos->ClosePosition(date(2024, 3, 1), D(100.0));

  REQUIRE(pos->getMaxFavorableExcursion() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE(pos->getMaxAdverseExcursion()  == DecimalConstants<DecimalType>::DecimalZero);
}


// ============================================================================
// 2. Terminal MFE/MAE on TradingPosition (Short)
// ============================================================================

TEST_CASE("TradingPositionShort terminal MFE/MAE", "[trading_position][mfe_mae]")
{
  // Entry at 100 short. Bar 2 reaches low=95 (favorable), high=101 (adverse).
  // Bar 3 high=103, low=97.
  // Running low water mark: min(95, 97) = 95
  // Running high water mark: max(101, 103) = 103
  // Short MFE = (100 - 95)/100 * 100 = 5.0
  // Short MAE = (103 - 100)/100 * 100 = 3.0
  ShortTradeScript s{
    date(2024, 1, 8), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 101.0,  95.0,  96.0),
      std::make_tuple( 98.0, 103.0, 97.0, 98.0)
    },
    date(2024, 1, 10), 98.0
  };
  auto pos = buildClosedShort(s);

  SECTION("Short MFE uses the lowest post-entry low")
  {
    REQUIRE(approxEqual(pos->getMaxFavorableExcursion(), D(5.0)));
  }
  SECTION("Short MAE uses the highest post-entry high")
  {
    REQUIRE(approxEqual(pos->getMaxAdverseExcursion(), D(3.0)));
  }
  SECTION("Short MFE and MAE are non-negative (direction-swap sanity check)")
  {
    REQUIRE(pos->getMaxFavorableExcursion() >= DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(pos->getMaxAdverseExcursion()  >= DecimalConstants<DecimalType>::DecimalZero);
  }
}

TEST_CASE("TradingPositionShort: price only moves up (losing short)",
          "[trading_position][mfe_mae]")
{
  // Price rises steadily: never below entry. MFE must be zero, MAE positive.
  ShortTradeScript s{
    date(2024, 4, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 102.0, 100.0, 101.0),
      std::make_tuple(101.0, 105.0, 101.0, 104.0)
    },
    date(2024, 4, 3), 104.0
  };
  auto pos = buildClosedShort(s);

  REQUIRE(pos->getMaxFavorableExcursion() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE(approxEqual(pos->getMaxAdverseExcursion(), D(5.0)));
}


// ============================================================================
// 3. ExcursionTrajectoryPoint value-object invariants
// ============================================================================

TEST_CASE("ExcursionTrajectoryPoint: valid construction and accessors",
          "[excursion_trajectory_point]")
{
  ptime t(date(2024, 1, 1), getDefaultBarTime());
  ExcursionTrajectoryPoint<DecimalType> p(3, t, D(2.5), D(1.0));
  REQUIRE(p.getBarNumber()   == 3);
  REQUIRE(p.getBarDateTime() == t);
  REQUIRE(p.getMfe()         == D(2.5));
  REQUIRE(p.getMae()         == D(1.0));
}

TEST_CASE("ExcursionTrajectoryPoint: barNumber == 0 throws",
          "[excursion_trajectory_point]")
{
  ptime t(date(2024, 1, 1), getDefaultBarTime());
  REQUIRE_THROWS_AS(
      ExcursionTrajectoryPoint<DecimalType>(0, t, D(1.0), D(1.0)),
      TradingPositionException);
}

TEST_CASE("ExcursionTrajectoryPoint: negative MFE throws",
          "[excursion_trajectory_point]")
{
  ptime t(date(2024, 1, 1), getDefaultBarTime());
  REQUIRE_THROWS_AS(
      ExcursionTrajectoryPoint<DecimalType>(1, t, D(-0.5), D(0.0)),
      TradingPositionException);
}

TEST_CASE("ExcursionTrajectoryPoint: negative MAE throws",
          "[excursion_trajectory_point]")
{
  ptime t(date(2024, 1, 1), getDefaultBarTime());
  REQUIRE_THROWS_AS(
      ExcursionTrajectoryPoint<DecimalType>(1, t, D(0.0), D(-0.5)),
      TradingPositionException);
}

TEST_CASE("ExcursionTrajectoryPoint: zero MFE and MAE are valid",
          "[excursion_trajectory_point]")
{
  ptime t(date(2024, 1, 1), getDefaultBarTime());
  REQUIRE_NOTHROW(
      ExcursionTrajectoryPoint<DecimalType>(1, t, D(0.0), D(0.0)));
}

TEST_CASE("ExcursionTrajectoryPoint: equality and inequality",
          "[excursion_trajectory_point]")
{
  ptime t(date(2024, 1, 1), getDefaultBarTime());
  ExcursionTrajectoryPoint<DecimalType> a(1, t, D(1.0), D(0.5));
  ExcursionTrajectoryPoint<DecimalType> b(1, t, D(1.0), D(0.5));
  ExcursionTrajectoryPoint<DecimalType> c(2, t, D(1.0), D(0.5));
  REQUIRE(a == b);
  REQUIRE(a != c);
}


// ============================================================================
// 4. ExcursionTrajectory construction and access
// ============================================================================

TEST_CASE("ExcursionTrajectory: long position, bar 1 is zero by purist convention",
          "[excursion_trajectory]")
{
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 110.0, 90.0,   // wide entry-bar range
    {
      std::make_tuple(100.0, 102.0, 99.0, 101.0)
    },
    date(2024, 1, 2), 101.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  REQUIRE(traj.size() == 2);
  REQUIRE(traj.atBar(1).getMfe() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE(traj.atBar(1).getMae() == DecimalConstants<DecimalType>::DecimalZero);
}

TEST_CASE("ExcursionTrajectory: bar-by-bar values for a long position",
          "[excursion_trajectory]")
{
  // Entry 100. Bar 2: H=102, L=99. Bar 3: H=105, L=98. Bar 4: H=104, L=95.
  // Running HWM: 100 -> 102 -> 105 -> 105
  // Running LWM: 100 ->  99 ->  98 ->  95
  // Long MFE at bars 2,3,4: 2.0, 5.0, 5.0
  // Long MAE at bars 2,3,4: 1.0, 2.0, 5.0
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 102.0,  99.0, 101.0),
      std::make_tuple(101.0, 105.0,  98.0, 100.0),
      std::make_tuple(100.0, 104.0,  95.0,  96.0)
    },
    date(2024, 1, 5), 96.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  REQUIRE(traj.size() == 4);
  REQUIRE(approxEqual(traj.atBar(2).getMfe(), D(2.0)));
  REQUIRE(approxEqual(traj.atBar(2).getMae(), D(1.0)));
  REQUIRE(approxEqual(traj.atBar(3).getMfe(), D(5.0)));
  REQUIRE(approxEqual(traj.atBar(3).getMae(), D(2.0)));
  REQUIRE(approxEqual(traj.atBar(4).getMfe(), D(5.0)));
  REQUIRE(approxEqual(traj.atBar(4).getMae(), D(5.0)));
}

TEST_CASE("ExcursionTrajectory: bar-by-bar values for a short position",
          "[excursion_trajectory]")
{
  // Entry 100 short. Bar 2: H=101, L=98. Bar 3: H=103, L=97. Bar 4: H=102, L=94.
  // Running HWM: 100 -> 101 -> 103 -> 103
  // Running LWM: 100 ->  98 ->  97 ->  94
  // Short MFE (distance below entry) at bars 2,3,4: 2.0, 3.0, 6.0
  // Short MAE (distance above entry) at bars 2,3,4: 1.0, 3.0, 3.0
  ShortTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 101.0,  98.0,  99.0),
      std::make_tuple( 99.0, 103.0,  97.0, 100.0),
      std::make_tuple(100.0, 102.0,  94.0,  95.0)
    },
    date(2024, 1, 5), 95.0
  };
  auto pos = buildClosedShort(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  REQUIRE(traj.size() == 4);
  REQUIRE(approxEqual(traj.atBar(2).getMfe(), D(2.0)));
  REQUIRE(approxEqual(traj.atBar(2).getMae(), D(1.0)));
  REQUIRE(approxEqual(traj.atBar(3).getMfe(), D(3.0)));
  REQUIRE(approxEqual(traj.atBar(3).getMae(), D(3.0)));
  REQUIRE(approxEqual(traj.atBar(4).getMfe(), D(6.0)));
  REQUIRE(approxEqual(traj.atBar(4).getMae(), D(3.0)));
}

TEST_CASE("ExcursionTrajectory: at() and atBar() agree",
          "[excursion_trajectory]")
{
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 101.0, 99.0, 100.0),
      std::make_tuple(100.0, 103.0, 98.0, 101.0)
    },
    date(2024, 1, 3), 101.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  for (size_t i = 0; i < traj.size(); ++i)
  {
    REQUIRE(traj.at(i) == traj.atBar(static_cast<unsigned int>(i + 1)));
  }
}

TEST_CASE("ExcursionTrajectory: at() and atBar() out-of-range throw",
          "[excursion_trajectory]")
{
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    { std::make_tuple(100.0, 101.0, 99.0, 100.0) },
    date(2024, 1, 2), 100.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  REQUIRE_THROWS_AS(traj.at(traj.size()), std::out_of_range);
  REQUIRE_THROWS_AS(traj.atBar(0), std::out_of_range);
  REQUIRE_THROWS_AS(traj.atBar(static_cast<unsigned int>(traj.size() + 1)),
                    std::out_of_range);
}


// ============================================================================
// 5. ExcursionTrajectory invariants
// ============================================================================

TEST_CASE("ExcursionTrajectory: running MFE and MAE are monotonically non-decreasing",
          "[excursion_trajectory][invariants]")
{
  // Deliberately choppy price action to stress the invariant.
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 101.0,  99.0, 100.0),
      std::make_tuple(100.0, 104.0,  96.0,  98.0),
      std::make_tuple( 98.0, 102.0,  97.0, 100.0),   // no new extreme on either side
      std::make_tuple(100.0, 107.0,  93.0,  94.0),
      std::make_tuple( 94.0,  96.0,  92.0,  95.0)
    },
    date(2024, 1, 7), 95.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  REQUIRE(traj.size() >= 2);
  for (size_t i = 1; i < traj.size(); ++i)
  {
    REQUIRE(traj.at(i).getMfe() >= traj.at(i - 1).getMfe());
    REQUIRE(traj.at(i).getMae() >= traj.at(i - 1).getMae());
  }
}


// ============================================================================
// 6. Terminal values agree with TradingPosition accessors
// ============================================================================

TEST_CASE("ExcursionTrajectory: terminal values agree with TradingPosition accessors",
          "[excursion_trajectory][agreement]")
{
  SECTION("long position")
  {
    LongTradeScript s{
      date(2024, 1, 1), 100.0, 100.0, 100.0,
      {
        std::make_tuple(100.0, 105.0, 99.0, 104.0),
        std::make_tuple(103.0, 103.0, 97.0, 102.0)
      },
      date(2024, 1, 3), 102.0
    };
    auto pos = buildClosedLong(s);
    ExcursionTrajectory<DecimalType> traj(*pos);

    REQUIRE(approxEqual(traj.terminalMfe(), pos->getMaxFavorableExcursion()));
    REQUIRE(approxEqual(traj.terminalMae(), pos->getMaxAdverseExcursion()));
  }

  SECTION("short position")
  {
    ShortTradeScript s{
      date(2024, 2, 1), 100.0, 100.0, 100.0,
      {
        std::make_tuple(100.0, 101.0, 95.0, 96.0),
        std::make_tuple( 98.0, 103.0, 97.0, 98.0)
      },
      date(2024, 2, 3), 98.0
    };
    auto pos = buildClosedShort(s);
    ExcursionTrajectory<DecimalType> traj(*pos);

    REQUIRE(approxEqual(traj.terminalMfe(), pos->getMaxFavorableExcursion()));
    REQUIRE(approxEqual(traj.terminalMae(), pos->getMaxAdverseExcursion()));
  }
}


// ============================================================================
// 7. Threshold queries
// ============================================================================

TEST_CASE("ExcursionTrajectory: firstBarWhereMfeExceeds semantics",
          "[excursion_trajectory][threshold]")
{
  // MFE trajectory: 0.0, 2.0, 5.0, 5.0 (computed earlier test case).
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 102.0,  99.0, 101.0),
      std::make_tuple(101.0, 105.0,  98.0, 100.0),
      std::make_tuple(100.0, 104.0,  95.0,  96.0)
    },
    date(2024, 1, 5), 96.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  SECTION("threshold never exceeded returns 0")
  {
    REQUIRE(traj.firstBarWhereMfeExceeds(D(10.0)) == 0);
  }
  SECTION("strict > semantics: threshold equal to MFE does not count")
  {
    // bar 3 MFE == 5.0 exactly; threshold 5.0 should not be considered exceeded
    REQUIRE(traj.firstBarWhereMfeExceeds(D(5.0)) == 0);
  }
  SECTION("threshold below first non-zero MFE returns first such bar")
  {
    REQUIRE(traj.firstBarWhereMfeExceeds(D(1.0)) == 2);
  }
  SECTION("threshold between bar 2 and bar 3 MFE returns bar 3")
  {
    REQUIRE(traj.firstBarWhereMfeExceeds(D(3.0)) == 3);
  }
  SECTION("threshold of zero returns the first bar with any MFE")
  {
    REQUIRE(traj.firstBarWhereMfeExceeds(DecimalConstants<DecimalType>::DecimalZero) == 2);
  }
}

TEST_CASE("ExcursionTrajectory: firstBarWhereMaeExceeds semantics",
          "[excursion_trajectory][threshold]")
{
  // MAE trajectory: 0.0, 1.0, 2.0, 5.0.
  LongTradeScript s{
    date(2024, 1, 1), 100.0, 100.0, 100.0,
    {
      std::make_tuple(100.0, 102.0,  99.0, 101.0),
      std::make_tuple(101.0, 105.0,  98.0, 100.0),
      std::make_tuple(100.0, 104.0,  95.0,  96.0)
    },
    date(2024, 1, 5), 96.0
  };
  auto pos = buildClosedLong(s);
  ExcursionTrajectory<DecimalType> traj(*pos);

  REQUIRE(traj.firstBarWhereMaeExceeds(D(10.0)) == 0);
  REQUIRE(traj.firstBarWhereMaeExceeds(D(5.0))  == 0); // strict
  REQUIRE(traj.firstBarWhereMaeExceeds(D(0.5))  == 2);
  REQUIRE(traj.firstBarWhereMaeExceeds(D(1.5))  == 3);
  REQUIRE(traj.firstBarWhereMaeExceeds(D(4.0))  == 4);
}


// ============================================================================
// 8. Trajectory on an open (not yet closed) position
// ============================================================================

TEST_CASE("ExcursionTrajectory: can be built from an open position",
          "[excursion_trajectory][open_position]")
{
  // The MFE-conditional exit-rule extension (Phase 2, if adopted) needs
  // trajectory access during a trade, not only after close. Verify that
  // the capability works before ClosePosition is called.
  auto entryBar = makeBar(date(2024, 1, 1), 100.0, 100.0, 100.0, 100.0);
  auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
      std::string("TEST"), D(100.0), entryBar, oneShare());
  pos->addBar(makeBar(date(2024, 1, 2), 100.0, 102.0, 99.0, 101.0));
  pos->addBar(makeBar(date(2024, 1, 3), 101.0, 105.0, 98.0, 104.0));

  REQUIRE(pos->isPositionOpen());

  ExcursionTrajectory<DecimalType> traj(*pos);
  REQUIRE(traj.size() == 3);
  REQUIRE(approxEqual(traj.atBar(3).getMfe(), D(5.0)));
  REQUIRE(approxEqual(traj.atBar(3).getMae(), D(2.0)));
  REQUIRE(approxEqual(traj.terminalMfe(), pos->getMaxFavorableExcursion()));
  REQUIRE(approxEqual(traj.terminalMae(), pos->getMaxAdverseExcursion()));
}
