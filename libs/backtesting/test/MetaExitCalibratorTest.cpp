// MetaExitCalibratorTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "MetaExitCalibrator.h"
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

static constexpr double kAbsTol_PnLR = 3e-9;

// -----------------------------------------------------------------------------
// Baseline long scenarios from earlier coverage
// -----------------------------------------------------------------------------
TEST_CASE("MetaExitCalibrator: evaluate overlay policies on synthetic long trades", "[MetaExitCalibrator][longs]")
{
  using DT = DecimalType;
  ClosedPositionHistory<DT> cph;
  TradingVolume oneShare(1, TradingVolume::SHARES);

  // -------- Trade A (Long, target works) --------
  // Entry @ 100 on 2020-01-01. Target=110, Stop=95.
  // t=0 (2020-01-02): O=101 H=112 L=98 C=110  => PnL_R = +1.0
  // t=1 (2020-01-03): O=110 H=115 L=105 C=114 => Final baseline close 114 (PnL_R=+1.4)
  auto A0 = mkBar<DT>("20200101", "100.00", "100.00", "100.00", "100.00");
  auto A1 = mkBar<DT>("20200102", "101.00", "112.00", "98.00",  "110.00");
  auto A2 = mkBar<DT>("20200103", "110.00","115.00", "105.00", "114.00");
  auto posA = std::make_shared<TradingPositionLong<DT>>("A", A0->getOpenValue(), *A0, oneShare);
  posA->setProfitTarget(createDecimal("110.00"));
  posA->setStopLoss(createDecimal("95.00"));
  posA->addBar(*A1);
  posA->addBar(*A2);
  posA->ClosePosition(A2->getDateValue(), A2->getCloseValue());
  cph.addClosedPosition(posA);

  // -------- Trade B (Long, stop & target touchable intrabar; STOP precedence) --------
  // Entry @ 118 on 2020-02-01. Target=129.80 (R=11.8), Stop=112.10.
  // t=0: O=119 H=131 L=111 C=115 => both touchable; conservative logic = STOP-first; PnL_R negative at C
  // t=1: O=115 H=120 L=114 C=117
  auto B0 = mkBar<DT>("20200201", "118.00", "118.00", "118.00", "118.00");
  auto B1 = mkBar<DT>("20200202", "119.00", "131.00", "111.00", "115.00");
  auto B2 = mkBar<DT>("20200203", "115.00", "120.00", "114.00", "117.00");
  auto posB = std::make_shared<TradingPositionLong<DT>>("B", B0->getOpenValue(), *B0, oneShare);
  posB->setProfitTarget(createDecimal("129.80"));
  posB->setStopLoss(createDecimal("112.10"));
  posB->addBar(*B1);
  posB->addBar(*B2);
  posB->ClosePosition(B2->getDateValue(), B2->getCloseValue());
  cph.addClosedPosition(posB);

  // -------- Trade C (Long, meanders negative) --------
  // Entry @ 50 on 2020-03-01. Target=55 (R=5), Stop=47.5.
  // t=0: O=50 H=51 L=49 C=49.50 => PnL_R = -0.1
  // t=1: O=49.60 H=50.00 L=48.50 C=49.00
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

  MetaExitCalibrator<DT> cal(cph);
  const auto Z = DecimalConstants<DT>::DecimalZero;

  SECTION("Failure-to-perform at K=0 exits failing trades at next bar open")
    {
      auto r = cal.evaluateFailureToPerformBars(/*K=*/0, /*thresholdR=*/Z);

      // A: +1.4R (stays to last close)
      // B: -0.254237...R (exit at t=1 open = 115)
      // C: -0.08R (exit at t=1 open = 49.60)
      const double expectedAvg = (1.4 - 0.2542372881355932 - 0.08) / 3.0; // ≈ 0.3552542373
      REQUIRE(r.getAvgPnL_R() == Approx(expectedAvg).margin(kAbsTol_PnLR));

      REQUIRE(r.getHitRate() == Approx(1.0/3.0).epsilon(1e-9));
      
      // BarsHeld: A=2, B=2, C=2 => avg 2.0
      REQUIRE(r.getAvgBarsHeld() == Approx(2.0).epsilon(1e-12));
      REQUIRE(r.getTrades() == 3);
    }
  
  SECTION("Breakeven armed at N=1: BE from second bar onward")
  {
    auto r = cal.evaluateBreakevenAfterBars(/*N=*/1, /*epsilonR=*/Z);

    // A: BE not hit at t=1 → last close 114 => +1.4 R
    // B: BE hit at t=1 open 115 <= 118 → exit @ 118 => 0.0 R
    // C: BE hit at t=1 open 49.6 <= 50 → exit @ 50  => 0.0 R
    REQUIRE(r.getAvgPnL_R() == Approx(0.4666666667).epsilon(1e-9));
    REQUIRE(r.getHitRate() == Approx(1.0/3.0).epsilon(1e-9));
    REQUIRE(r.getAvgBarsHeld() == Approx(2.0).epsilon(1e-12));
    REQUIRE(r.getTrades() == 3);
  }

  SECTION("Breakeven armed at N=0: immediate BE from first bar after entry")
  {
    auto r = cal.evaluateBreakevenAfterBars(/*N=*/0, /*epsilonR=*/Z);

    // All three trigger BE at t=0:
    // A: low 98 <= 100 → exit 100 → 0.0 R
    // B: low 111 <= 118 → exit 118 → 0.0 R
    // C: open 50 <= 50  → exit 50  → 0.0 R
    REQUIRE(r.getAvgPnL_R() == Approx(0.0));
    REQUIRE(r.getHitRate() == Approx(0.0));
    REQUIRE(r.getAvgBarsHeld() == Approx(1.0).epsilon(1e-12));
    REQUIRE(r.getTrades() == 3);
  }
}

// -----------------------------------------------------------------------------
// Shorts: verify sign conventions and BE stop logic for short positions
// -----------------------------------------------------------------------------
TEST_CASE("MetaExitCalibrator: short positions (failure-to-perform & breakeven)", "[MetaExitCalibrator][shorts]")
{
  using DT = DecimalType;
  ClosedPositionHistory<DT> cph;
  TradingVolume oneShare(1, TradingVolume::SHARES);
  const auto Z = DecimalConstants<DT>::DecimalZero;

  // S1: Short that moves in favor quickly
  // Entry @ 200, target 190 (R=10), stop 205
  // t=0: O=199 H=201 L=188 C=190 (favorable; target touch intrabar)
  auto S10 = mkBar<DT>("20200401", "200.00","200.00","200.00","200.00");
  auto S11 = mkBar<DT>("20200402", "199.00","201.00","188.00","190.00");
  auto S12 = mkBar<DT>("20200403", "190.00","195.00","185.00","187.00");
  auto posS1 = std::make_shared<TradingPositionShort<DT>>("S1", S10->getOpenValue(), *S10, oneShare);
  posS1->setProfitTarget(createDecimal("190.00"));
  posS1->setStopLoss(createDecimal("205.00"));
  posS1->addBar(*S11);
  posS1->addBar(*S12);
  posS1->ClosePosition(S12->getDateValue(), S12->getCloseValue());
  cph.addClosedPosition(posS1);

  // S2: Short that moves against (stop touchable), meanders
  // Entry @ 300, target 285 (R=15), stop 306
  // t=0: O=301 H=308 L=294 C=307 (against; both thresholds touchable, stop-first applies)
  // t=1: O=307 H=310 L=300 C=305
  auto S20 = mkBar<DT>("20200501", "300.00","300.00","300.00","300.00");
  auto S21 = mkBar<DT>("20200502", "301.00","308.00","294.00","307.00");
  auto S22 = mkBar<DT>("20200503", "307.00","310.00","300.00","305.00");
  auto posS2 = std::make_shared<TradingPositionShort<DT>>("S2", S20->getOpenValue(), *S20, oneShare);
  posS2->setProfitTarget(createDecimal("285.00"));
  posS2->setStopLoss(createDecimal("306.00"));
  posS2->addBar(*S21);
  posS2->addBar(*S22);
  posS2->ClosePosition(S22->getDateValue(), S22->getCloseValue());
  cph.addClosedPosition(posS2);

  MetaExitCalibrator<DT> cal(cph);

  SECTION("Failure-to-perform K=0 on shorts: exits the losing short immediately")
  {
    // S1: at t=0 close=190 vs entry=200 -> pnlCur=+10 (favorable for short) => stays
    // S2: at t=0 close=307 vs entry=300 -> pnlCur=-7 (unfavorable) => exits at t=0 close
    auto r = cal.evaluateFailureToPerformBars(/*K=*/0, /*thresholdR=*/Z);

    // S1 final close (baseline last) @ 187: pnlR = (entry-exit)/R = (200-187)/10 = 1.3
    // S2 exit at t=0 close 307: pnlR = (300-307)/15 = -0.466666...
    const double expectedAvg = (1.3 - 0.4666666667) / 2.0; // ≈ 0.4166667

    REQUIRE(r.getAvgPnL_R() == Approx(expectedAvg).margin(kAbsTol_PnLR));
    REQUIRE(r.getHitRate() == Approx(0.5));

    // Bars: S1=2 (t=1 last), S2=2 (t=1 open) -> avg 2.0
    REQUIRE(r.getAvgBarsHeld() == Approx(2.0).epsilon(1e-12));
    REQUIRE(r.getTrades() == 2);
  }

  SECTION("Breakeven N=0 on shorts: BE active from t=0 with stop-first semantics")
  {
    auto r = cal.evaluateBreakevenAfterBars(/*N=*/0, /*epsilonR=*/Z);

    // BE = entry (200 for S1, 300 for S2)
    // S1: at t=0 open=199 < entry, but short BE triggers on open >= entry or high >= entry -> not on open
    //     high=201 >= 200 -> BE hit at t=0 => exit @ 200 -> PnL_R = 0
    // S2: at t=0 open=301 >= 300 -> BE hit at t=0 => exit @ 300 -> PnL_R = 0
    REQUIRE(r.getAvgPnL_R() == Approx(0.0));
    REQUIRE(r.getHitRate() == Approx(0.0));
    REQUIRE(r.getAvgBarsHeld() == Approx(1.0).epsilon(1e-12));
    REQUIRE(r.getTrades() == 2);
  }

  SECTION("Breakeven N=1 on shorts: BE armed from second bar onward")
  {
    auto r = cal.evaluateBreakevenAfterBars(/*N=*/1, /*epsilonR=*/Z);

    // S1: t=1 high=195 < entry=200 -> no BE -> final last close 187 => (200-187)/10 = 1.3
    // S2: t=1 open=307 >= 300 -> BE hit at t=1 => exit @ 300 => 0.0 R
    const double expectedAvg = (1.3 + 0.0) / 2.0; // = 0.65
    REQUIRE(r.getAvgPnL_R() == Approx(expectedAvg).epsilon(1e-9));
    // Wins: only S1
    REQUIRE(r.getHitRate() == Approx(0.5));
    // BarsHeld: S1=2, S2=2 -> avg 2
    REQUIRE(r.getAvgBarsHeld() == Approx(2.0).epsilon(1e-12));
  }
}

// -----------------------------------------------------------------------------
// Missing targets: fallback classification + no breakeven overlay possible
// -----------------------------------------------------------------------------
TEST_CASE("MetaExitCalibrator: missing targets (fallback PnL sign, BE disabled)", "[MetaExitCalibrator][no-target]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;
  TradingVolume oneShare(1, TradingVolume::SHARES);

  ClosedPositionHistory<DT> cph;

  // T1: Long WITH target; positive outcome (to provide scaleFallback = median rTarget = 10)
  // Entry 100, target 110 (R=10)
  auto T10 = mkBar<DT>("20200601","100.00","100.00","100.00","100.00");
  auto T11 = mkBar<DT>("20200602","99.00","102.00","98.00","101.00");  // t=0 close +1
  auto T12 = mkBar<DT>("20200603","110.00","113.00","109.00","112.00"); // t=1 close +12
  auto posT1 = std::make_shared<TradingPositionLong<DT>>("T1", T10->getOpenValue(), *T10, oneShare);
  posT1->setProfitTarget(createDecimal("110.00"));
  posT1->setStopLoss(createDecimal("95.00"));
  posT1->addBar(*T11);
  posT1->addBar(*T12);
  posT1->ClosePosition(T12->getDateValue(), T12->getCloseValue());
  cph.addClosedPosition(posT1);

  // T2: Long WITHOUT target; negative early, more negative later
  auto T20 = mkBar<DT>("20200610","50.00","50.00","50.00","50.00");
  auto T21 = mkBar<DT>("20200611","50.00","50.20","49.00","49.00"); // t=0 close -1
  auto T22 = mkBar<DT>("20200612","49.00","49.50","47.00","48.00"); // t=1 close -2
  auto posT2 = std::make_shared<TradingPositionLong<DT>>("T2", T20->getOpenValue(), *T20, oneShare);
  // no profit target set; stop optional (won't matter for this test)
  posT2->addBar(*T21);
  posT2->addBar(*T22);
  posT2->ClosePosition(T22->getDateValue(), T22->getCloseValue());
  cph.addClosedPosition(posT2);

  MetaExitCalibrator<DT> cal(cph);

  SECTION("Failure-to-perform K=0 uses currency sign when R unavailable")
  {
    auto r = cal.evaluateFailureToPerformBars(/*K=*/0, /*thresholdR=*/Z);

    // T1 stays to last close: pnlR = (112-100)/10 = +1.2
    // T2 exits at t=0 close 49: no R → fallback scale by median rTarget (=10 from T1) → -1/10 = -0.1
    const double expectedAvg = (1.2 - 0.1) / 2.0; // = 0.55
    REQUIRE(r.getAvgPnL_R() == Approx(expectedAvg).epsilon(1e-9));
    REQUIRE(r.getHitRate() == Approx(0.5));

    // BarsHeld: T1=2, T2=2 -> avg 2.0
    REQUIRE(r.getAvgBarsHeld() == Approx(2.0).epsilon(1e-12));
  }

  SECTION("Breakeven is disabled if no R_target (overlay is a no-op for that trade)")
  {
    auto r = cal.evaluateBreakevenAfterBars(/*N=*/0, /*epsilonR=*/Z);

    // T1: BE from t=0 → open 99 <= 100 -> exit @100 → 0.0R
    // T2: no target → overlay no-op → last close 48; fallback scaling uses median R (=10) from T1
    // AvgPnL_R = (0.0 + (-2/10)) / 2 = -0.1
    REQUIRE(r.getAvgPnL_R() == Approx(-0.1).epsilon(1e-9));
    REQUIRE(r.getHitRate() == Approx(0.0));
    // BarsHeld: T1=1 (t=0), T2=2 (t=1) -> avg 1.5
    REQUIRE(r.getAvgBarsHeld() == Approx(1.5).epsilon(1e-12));
  }
}

// -----------------------------------------------------------------------------
// Extreme K/N beyond bars-held: overlay becomes no-op (keeps last recorded exit)
// -----------------------------------------------------------------------------
TEST_CASE("MetaExitCalibrator: extreme K/N beyond bars-held are no-ops", "[MetaExitCalibrator][bounds]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;
  TradingVolume oneShare(1, TradingVolume::SHARES);

  ClosedPositionHistory<DT> cph;

  // One-bar trade (t=0 only)
  // Entry 100, target 110 (R=10), t=0 close=105
  auto E0 = mkBar<DT>("20200701","100.00","100.00","100.00","100.00");
  auto E1 = mkBar<DT>("20200702","104.00","106.00","102.00","105.00");
  auto posE = std::make_shared<TradingPositionLong<DT>>("E", E0->getOpenValue(), *E0, oneShare);
  posE->setProfitTarget(createDecimal("110.00"));
  posE->setStopLoss(createDecimal("95.00"));
  posE->addBar(*E1);
  posE->ClosePosition(E1->getDateValue(), E1->getCloseValue());
  cph.addClosedPosition(posE);

  MetaExitCalibrator<DT> cal(cph);

  SECTION("K much larger than barsHeld: failure rule does nothing")
  {
    auto r = cal.evaluateFailureToPerformBars(/*K=*/5, /*thresholdR=*/Z);
    // Last close 105 → pnlR = (105-100)/10 = 0.5
    REQUIRE(r.getAvgPnL_R() == Approx(0.5).epsilon(1e-12));
    REQUIRE(r.getHitRate() == Approx(1.0));
    REQUIRE(r.getAvgBarsHeld() == Approx(1.0).epsilon(1e-12));
    REQUIRE(r.getTrades() == 1);
  }

  SECTION("N much larger than barsHeld: BE rule does nothing")
  {
    auto r = cal.evaluateBreakevenAfterBars(/*N=*/5, /*epsilonR=*/Z);
    REQUIRE(r.getAvgPnL_R() == Approx(0.5).epsilon(1e-12));
    REQUIRE(r.getHitRate() == Approx(1.0));
    REQUIRE(r.getAvgBarsHeld() == Approx(1.0).epsilon(1e-12));
    REQUIRE(r.getTrades() == 1);
  }
}
