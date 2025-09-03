#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "MetaExitAnalytics.h"
#include "PositionPathAnalytics.h"
#include "TradingPosition.h"
#include "ClosedPositionHistory.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using Catch::Approx;

template <class Decimal>
static std::shared_ptr<OHLCTimeSeriesEntry<Decimal>>
mkBar(const std::string& yyyymmdd,
      const std::string& o,
      const std::string& h,
      const std::string& l,
      const std::string& c)
{
  return createTimeSeriesEntry(yyyymmdd, o, h, l, c, "0");
}

TEST_CASE("MetaExitAnalytics::summarizeByBarAge with synthetic trades", "[MetaExitAnalytics][summarizeByBarAge]")
{
  using DT = DecimalType;
  ClosedPositionHistory<DT> cph;

  TradingVolume oneShare(1, TradingVolume::SHARES);

  // ------------- Trade A (Long) -------------
  // Entry @ 100 on 2020-01-01. Target=110, Stop=95.
  // Bar t=0 (2020-01-02): high 112 -> target touch at t=1; close 110 (PnL_R=+1.0)
  // Bar t=1 (2020-01-03): high 115; close 114
  auto A0 = mkBar<DT>("20200101", "100.00", "100.00", "100.00", "100.00");
  auto A1 = mkBar<DT>("20200102", "101.00", "112.00", "98.00",  "110.00");
  auto A2 = mkBar<DT>("20200103", "110.00","115.00", "105.00", "114.00");

  auto posA = std::make_shared<TradingPositionLong<DT>>("A", A0->getOpenValue(), *A0, oneShare);
  posA->setProfitTarget(createDecimal("110.00"));
  posA->setStopLoss(createDecimal("95.00"));
  posA->addBar(*A1);
  posA->addBar(*A2);
  // Close on last bar close price
  posA->ClosePosition(A2->getDateValue(), A2->getCloseValue());
  cph.addClosedPosition(posA);

  // ------------- Trade B (Long, both stop & target touch at t=1; STOP precedence) -------------
  // Entry @ 118 on 2020-02-01. Target=129.80 (10%), Stop=112.10 (~5%).
  // t=0 (2020-02-02): high 131, low 111 -> both touch at t=1; STOP wins; close 115 (PnL_R negative).
  // t=1 (2020-02-03): mild recovery; close 117
  auto B0 = mkBar<DT>("20200201", "118.00", "118.00", "118.00", "118.00");
  auto B1 = mkBar<DT>("20200202", "119.00", "131.00", "111.00", "115.00");
  auto B2 = mkBar<DT>("20200203", "115.00", "120.00", "114.00", "117.00");

  auto posB = std::make_shared<TradingPositionLong<DT>>("B", B0->getOpenValue(), *B0, oneShare);
  // price targets as absolutes
  posB->setProfitTarget(createDecimal("129.80"));
  posB->setStopLoss(createDecimal("112.10"));
  posB->addBar(*B1);
  posB->addBar(*B2);
  posB->ClosePosition(B2->getDateValue(), B2->getCloseValue());
  cph.addClosedPosition(posB);

  // ------------- Trade C (Long, meanders negative; no target/stop touch early) -------------
  // Entry @ 50 on 2020-03-01. Target=55, Stop=47.5.
  // t=0 (2020-03-02): high 51, low 49, close 49.5 => PnL_R negative (-0.5/5 = -0.10)
  // t=1 (2020-03-03): close 49.0
  auto C0 = mkBar<DT>("20200301", "50.00", "50.00", "50.00", "50.00");
  auto C1 = mkBar<DT>("20200302", "50.00", "51.00", "49.00", "49.50");
  auto C2 = mkBar<DT>("20200303", "49.60", "50.00", "48.50", "49.00");

  auto posC = std::make_shared<TradingPositionLong<DT>>("C", C0->getOpenValue(), *C0, oneShare);
  posC->setProfitTarget(createDecimal("55.00"));
  posC->setStopLoss(createDecimal("47.50"));
  posC->addBar(*C1);
  posC->addBar(*C2);
  posC->ClosePosition(C2->getDateValue(), C2->getCloseValue());
  cph.addClosedPosition(posC);

  // ---------------- Run analytics ----------------
  MetaExitAnalytics<DT> mex(cph);

  SECTION("Snapshots basic sanity") {
    auto snaps = mex.buildBarAgeSnapshots(3);
    // We added 3 trades with at least 2 bars each → at t=0 and t=1 we get 3 snapshots each → total >= 6
    REQUIRE(snaps.size() >= 6);
  }

  SECTION("Aggregates at t=0 match expectations") {
    auto aggs = mex.summarizeByBarAge(3);
    REQUIRE(aggs.size() >= 1);
    const auto& t0 = aggs[0];

    // Survival at t=0 = all trades alive at the first bar after entry
    REQUIRE(t0.getSurvival() == Approx(1.0)); // 3/3

    // Expected non-positive share at t=0:
    // Trade A close=110 vs entry=100, R=10 → +1.0 (positive)
    // Trade B close=115 vs entry=118, R=11.8 → negative
    // Trade C close=49.5 vs entry=50, R=5 → negative
    REQUIRE(t0.getFracNonPositive() == Approx(2.0/3.0).epsilon(1e-9));

    // Next-bar hazards from t=0 → t=1:
    // A: firstTargetIdx = 1 → counts toward targetNext
    // B: firstStopIdx   = 1 → counts toward stopNext (stop-first precedence)
    // C: neither → counts to neither
    REQUIRE(t0.getProbTargetNextBar() == Approx(1.0/3.0).epsilon(1e-9));
    REQUIRE(t0.getProbStopNextBar()   == Approx(1.0/3.0).epsilon(1e-9));

    // Median MFE_R so far at t=0:
    // A: (high 112 - 100)/10 = 1.2
    // B: (high 131 - 118)/11.8 ≈ 1.1016949
    // C: (high 51 - 50)/5 = 0.2
    // Median is the middle value ≈ 1.1016949
    REQUIRE(t0.getMedianMfeRSoFar() == Approx(1.1016949).epsilon(1e-6));
  }

  SECTION("Aggregates at t=1 match expectations") {
    auto aggs = mex.summarizeByBarAge(3);
    REQUIRE(aggs.size() >= 2);
    const auto& t1 = aggs[1];

    // All three trades still have at least 2 bars → survive
    REQUIRE(t1.getSurvival() == Approx(1.0));

    // At t=1, first touches already happened for A and B at t=1; so next-bar hazards should be ~0
    REQUIRE(t1.getProbTargetNextBar() == Approx(0.0));
    REQUIRE(t1.getProbStopNextBar() == Approx(0.0));

    // MFE_R so far at t=1:
    // A: MFE abs up to t1 = max(112-100, 115-100) = 15 → /10 = 1.5
    // B: max(131-118, 120-118)=13 → /11.8 ≈ 1.1016949
    // C: max(51-50, 50-50)=1 → /5 = 0.2
    // Median remains ≈ 1.1016949
    REQUIRE(t1.getMedianMfeRSoFar() == Approx(1.1016949).epsilon(1e-6));
  }
}
