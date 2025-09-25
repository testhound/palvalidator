// ExitPolicyJointAutoTunerTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <algorithm>
#include <vector>

#include "ExitPolicyJointAutoTuner.h"
#include "ExitPolicyAutoTuner.h"   // for ExitTunerOptions, TuningObjective
#include "MetaExitCalibrator.h"    // to recompute metrics for verification
#include "TradingPosition.h"
#include "ClosedPositionHistory.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using Catch::Approx;

// Absolute tolerance for double <- decimal conversions
static constexpr double kAbsTol = 3e-9;

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

// -----------------------------------------------------------------------------
// Build a compact synthetic ClosedPositionHistory with mixed outcomes
// -----------------------------------------------------------------------------
template <class Decimal>
static ClosedPositionHistory<Decimal> makeSyntheticCPH()
{
  ClosedPositionHistory<Decimal> cph;
  TradingVolume oneShare(1, TradingVolume::SHARES);

  // Long A: favorable quickly; R=10
  auto A0 = mkBar<Decimal>("20200101","100.00","100.00","100.00","100.00");
  auto A1 = mkBar<Decimal>("20200102","101.00","112.00","98.00","110.00");  // t=0
  auto A2 = mkBar<Decimal>("20200103","110.00","115.00","105.00","114.00"); // t=1
  auto posA = std::make_shared<TradingPositionLong<Decimal>>("A", A0->getOpenValue(), *A0, oneShare);
  posA->setProfitTarget(createDecimal("110.00"));
  posA->setStopLoss(createDecimal("95.00"));
  posA->addBar(*A1); posA->addBar(*A2);
  posA->ClosePosition(A2->getDateValue(), A2->getCloseValue());
  cph.addClosedPosition(posA);

  // Long B: negative, target & stop touchable at t=0 (stop-first); R=11.8
  auto B0 = mkBar<Decimal>("20200201","118.00","118.00","118.00","118.00");
  auto B1 = mkBar<Decimal>("20200202","119.00","131.00","111.00","115.00"); // t=0
  auto B2 = mkBar<Decimal>("20200203","115.00","120.00","114.00","117.00"); // t=1
  auto posB = std::make_shared<TradingPositionLong<Decimal>>("B", B0->getOpenValue(), *B0, oneShare);
  posB->setProfitTarget(createDecimal("129.80"));
  posB->setStopLoss(createDecimal("112.10"));
  posB->addBar(*B1); posB->addBar(*B2);
  posB->ClosePosition(B2->getDateValue(), B2->getCloseValue());
  cph.addClosedPosition(posB);

  // Long C: meanders negative; R=5
  auto C0 = mkBar<Decimal>("20200301","50.00","50.00","50.00","50.00");
  auto C1 = mkBar<Decimal>("20200302","50.00","51.00","49.00","49.50"); // t=0
  auto C2 = mkBar<Decimal>("20200303","49.60","50.00","48.50","49.00"); // t=1
  auto posC = std::make_shared<TradingPositionLong<Decimal>>("C", C0->getOpenValue(), *C0, oneShare);
  posC->setProfitTarget(createDecimal("55.00"));
  posC->setStopLoss(createDecimal("47.50"));
  posC->addBar(*C1); posC->addBar(*C2);
  posC->ClosePosition(C2->getDateValue(), C2->getCloseValue());
  cph.addClosedPosition(posC);

  // Short S: favorable; R=10
  auto S0 = mkBar<Decimal>("20200401","200.00","200.00","200.00","200.00");
  auto S1 = mkBar<Decimal>("20200402","199.00","201.00","188.00","190.00"); // t=0
  auto S2 = mkBar<Decimal>("20200403","190.00","195.00","185.00","187.00"); // t=1
  auto posS = std::make_shared<TradingPositionShort<Decimal>>("S", S0->getOpenValue(), *S0, oneShare);
  posS->setProfitTarget(createDecimal("190.00"));
  posS->setStopLoss(createDecimal("205.00"));
  posS->addBar(*S1); posS->addBar(*S2);
  posS->ClosePosition(S2->getDateValue(), S2->getCloseValue());
  cph.addClosedPosition(posS);

  return cph;
}

// -----------------------------------------------------------------------------
// Utilities mirroring joint tuner logic for verification
// -----------------------------------------------------------------------------

// Exact split replication (same policy as tuner)
template <class Decimal>
static std::pair<ClosedPositionHistory<Decimal>, ClosedPositionHistory<Decimal>>
replicateSplit(const ClosedPositionHistory<Decimal>& cph,
               double trainFraction,
               int embargoTrades)
{
  std::vector<std::shared_ptr<TradingPosition<Decimal>>> all;
  for (auto it = cph.beginTradingPositions(); it != cph.endTradingPositions(); ++it)
  {
    all.push_back(it->second);
  }

  const int n = static_cast<int>(all.size());
  ClosedPositionHistory<Decimal> train;
  ClosedPositionHistory<Decimal> test;

  const int cut = std::max(0, std::min(n, static_cast<int>(std::floor(n * trainFraction))));
  const int embargo = std::max(0, std::min(embargoTrades, n));

  for (int i = 0; i < cut; ++i)
  {
    train.addClosedPosition(all[i]);
  }
  for (int i = cut + embargo; i < n; ++i)
  {
    test.addClosedPosition(all[i]);
  }

  return {train, test};
}

// Arg-max over (K, N, H) with same tie-breakers as tuner; supports all objectives
template <class Decimal>
static std::tuple<int,int,int,PolicyResult>
argmaxFNH(const ClosedPositionHistory<Decimal>& cph,
          const std::vector<int>& failureToPerformGrid,
          const std::vector<int>& breakevenGrid,
          const std::vector<int>& maxHoldGrid,
          TuningObjective obj,
          const Decimal& thresholdR,
          const Decimal& epsilonR)
{
  MetaExitCalibrator<Decimal> cal(cph);

  auto score = [&](const PolicyResult& r) -> double
  {
    switch (obj)
    {
      case TuningObjective::HitRate:
        return r.getHitRate();

      case TuningObjective::PnLPerBar:
      {
        const double denom = std::max(r.getAvgBarsHeld(), 1e-9);
        return r.getAvgPnL_R() / denom;
      }

      case TuningObjective::AvgPnL_R:
      default:
        return r.getAvgPnL_R();
    }
  };

  double bestScore = -1e99;
  int bestF = failureToPerformGrid.empty() ? 0 : failureToPerformGrid.front();
  int bestB = breakevenGrid.empty() ? 0 : breakevenGrid.front();
  int bestH = maxHoldGrid.empty() ? 0 : maxHoldGrid.front();
  PolicyResult bestRes(0.0,0.0,0.0,0);

  for (int F : failureToPerformGrid)
  {
    for (int B : breakevenGrid)
    {
      for (int H : maxHoldGrid)
      {
        auto res = cal.evaluateCombinedPolicy(F, B, H, thresholdR, epsilonR, FailureExitFill::OpenOfKPlus1);
        const double s = score(res);

        const int sumFBH   = F + B + H;
        const int sumBest  = bestF + bestB + bestH;

        if (s > bestScore ||
            (s == bestScore && H < bestH) ||
            (s == bestScore && H == bestH && sumFBH < sumBest) ||
            (s == bestScore && H == bestH && sumFBH == sumBest && res.getHitRate() > bestRes.getHitRate()) ||
            (s == bestScore && H == bestH && sumFBH == sumBest && res.getHitRate() == bestRes.getHitRate() && F < bestF) ||
            (s == bestScore && H == bestH && sumFBH == sumBest && res.getHitRate() == bestRes.getHitRate() && F == bestF && B < bestB))
        {
          bestScore = s;
          bestF = F; bestB = B; bestH = H;
          bestRes = res;
        }
      }
    }
  }

  return {bestF, bestB, bestH, bestRes};
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------
TEST_CASE("ExitPolicyJointAutoTuner: end-to-end joint tuning with full data (train==test)",
          "[ExitPolicyJointAutoTuner][full]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  ExitTunerOptions<DT> opts(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    1.0,   // all in train; test==train in report
    /*embargoTrades*/    0,
    /*thresholdR*/       Z,     // F2P if PnL_R <= 0 at K
    /*epsilonR*/         Z,     // pure breakeven
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::AvgPnL_R
  );

  ExitPolicyJointAutoTuner<DT> jtuner(cph, opts);
  auto jreport = jtuner.tuneJoint();

  // Grid sanity incl. H
  REQUIRE(!jreport.getFailureToPerformGrid().empty());
  REQUIRE(!jreport.getBreakevenGrid().empty());
  REQUIRE(!jreport.getMaxHoldGrid().empty());

  REQUIRE(jreport.getFailureToPerformBars()    >= 0);
  REQUIRE(jreport.getBreakevenActivationBars() >= 0);
  REQUIRE(jreport.getMaxHoldBars()             >= 0);

  // Ensure chosen H is one of the candidates searched
  const auto& Hgrid = jreport.getMaxHoldGrid();
  REQUIRE(std::find(Hgrid.begin(), Hgrid.end(), jreport.getMaxHoldBars()) != Hgrid.end());

  // Independent arg-max under SAME grids & objective (K,N,H)
  auto [expF, expB, expH, bestTrain] =
      argmaxFNH<DT>(cph,
                    jreport.getFailureToPerformGrid(),
                    jreport.getBreakevenGrid(),
                    jreport.getMaxHoldGrid(),
                    opts.getObjective(),
                    opts.getThresholdR(),
                    opts.getEpsilonR());

  REQUIRE(jreport.getFailureToPerformBars()    == expF);
  REQUIRE(jreport.getBreakevenActivationBars() == expB);
  REQUIRE(jreport.getMaxHoldBars()             == expH);

  // Train/Test metrics should match calibrator on full history (train==test here)
  MetaExitCalibrator<DT> calAll(cph);
  const auto combinedAll =
      calAll.evaluateCombinedPolicy(expF, expB, expH, opts.getThresholdR(), opts.getEpsilonR(),
                                    FailureExitFill::OpenOfKPlus1);

  REQUIRE(jreport.getTrainCombined().getAvgPnL_R()    == Approx(combinedAll.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(jreport.getTrainCombined().getHitRate()     == Approx(combinedAll.getHitRate()).epsilon(1e-12));
  REQUIRE(jreport.getTrainCombined().getAvgBarsHeld() == Approx(combinedAll.getAvgBarsHeld()).epsilon(1e-12));

  REQUIRE(jreport.getTestCombined().getAvgPnL_R()     == Approx(combinedAll.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(jreport.getTestCombined().getHitRate()      == Approx(combinedAll.getHitRate()).epsilon(1e-12));
  REQUIRE(jreport.getTestCombined().getAvgBarsHeld()  == Approx(combinedAll.getAvgBarsHeld()).epsilon(1e-12));
}

TEST_CASE("ExitPolicyJointAutoTuner: train/test split with embargo (exact verification, K/N/H)",
          "[ExitPolicyJointAutoTuner][split]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  ExitTunerOptions<DT> opts(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    0.5,  // split dataset
    /*embargoTrades*/    1,    // skip one between train and test
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::AvgPnL_R
  );

  ExitPolicyJointAutoTuner<DT> jtuner(cph, opts);
  auto jreport = jtuner.tuneJoint();

  // Recreate exact split and recompute test-side metrics with (K,N,H)
  auto [train, test] = replicateSplit(cph, opts.getTrainFraction(), opts.getEmbargoTrades());
  MetaExitCalibrator<DT> calTest(test);

  const int F = jreport.getFailureToPerformBars();
  const int B = jreport.getBreakevenActivationBars();
  const int H = jreport.getMaxHoldBars();

  const auto combinedTest =
      calTest.evaluateCombinedPolicy(F, B, H, opts.getThresholdR(), opts.getEpsilonR(),
                                     FailureExitFill::OpenOfKPlus1);

  REQUIRE(jreport.getTestCombined().getAvgPnL_R()     == Approx(combinedTest.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(jreport.getTestCombined().getHitRate()      == Approx(combinedTest.getHitRate()).epsilon(1e-12));
  REQUIRE(jreport.getTestCombined().getAvgBarsHeld()  == Approx(combinedTest.getAvgBarsHeld()).epsilon(1e-12));
  REQUIRE(jreport.getTestCombined().getTrades()       == combinedTest.getTrades());
}

TEST_CASE("ExitPolicyJointAutoTuner: objective controls selection with H (AvgPnL_R vs HitRate)",
          "[ExitPolicyJointAutoTuner][objective]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  // --- Optimize AvgPnL_R ---
  ExitTunerOptions<DT> optsAvg(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    1.0,   // train == test on report
    /*embargoTrades*/    0,
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::AvgPnL_R
  );

  ExitPolicyJointAutoTuner<DT> jtAvg(cph, optsAvg);
  auto repAvg = jtAvg.tuneJoint();

  // Independent 3-D arg-max under AvgPnL_R on the SAME grids
  auto [expF_A, expB_A, expH_A, bestTrain_A] =
      argmaxFNH<DT>(cph,
                    repAvg.getFailureToPerformGrid(),
                    repAvg.getBreakevenGrid(),
                    repAvg.getMaxHoldGrid(),
                    TuningObjective::AvgPnL_R,
                    optsAvg.getThresholdR(),
                    optsAvg.getEpsilonR());

  REQUIRE(repAvg.getFailureToPerformBars()    == expF_A);
  REQUIRE(repAvg.getBreakevenActivationBars() == expB_A);
  REQUIRE(repAvg.getMaxHoldBars()             == expH_A);

  // Calibrator recomputation on full data should match report train/test
  {
    MetaExitCalibrator<DT> calAll(cph);
    const auto combAll =
        calAll.evaluateCombinedPolicy(expF_A, expB_A, expH_A,
                                      optsAvg.getThresholdR(),
                                      optsAvg.getEpsilonR(),
                                      FailureExitFill::OpenOfKPlus1);

    REQUIRE(repAvg.getTrainCombined().getAvgPnL_R()     == Catch::Approx(combAll.getAvgPnL_R()).margin(kAbsTol));
    REQUIRE(repAvg.getTrainCombined().getHitRate()      == Catch::Approx(combAll.getHitRate()).epsilon(1e-12));
    REQUIRE(repAvg.getTrainCombined().getAvgBarsHeld()  == Catch::Approx(combAll.getAvgBarsHeld()).epsilon(1e-12));

    REQUIRE(repAvg.getTestCombined().getAvgPnL_R()      == Catch::Approx(combAll.getAvgPnL_R()).margin(kAbsTol));
    REQUIRE(repAvg.getTestCombined().getHitRate()       == Catch::Approx(combAll.getHitRate()).epsilon(1e-12));
    REQUIRE(repAvg.getTestCombined().getAvgBarsHeld()   == Catch::Approx(combAll.getAvgBarsHeld()).epsilon(1e-12));
  }

  // --- Optimize HitRate ---
  ExitTunerOptions<DT> optsHit(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    1.0,
    /*embargoTrades*/    0,
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::HitRate
  );

  ExitPolicyJointAutoTuner<DT> jtHit(cph, optsHit);
  auto repHit = jtHit.tuneJoint();

  // Independent 3-D arg-max under HitRate on the SAME grids
  auto [expF_H, expB_H, expH_H, bestTrain_H] =
      argmaxFNH<DT>(cph,
                    repHit.getFailureToPerformGrid(),
                    repHit.getBreakevenGrid(),
                    repHit.getMaxHoldGrid(),
                    TuningObjective::HitRate,
                    optsHit.getThresholdR(),
                    optsHit.getEpsilonR());

  REQUIRE(repHit.getFailureToPerformBars()    == expF_H);
  REQUIRE(repHit.getBreakevenActivationBars() == expB_H);
  REQUIRE(repHit.getMaxHoldBars()             == expH_H);

  // Calibrator recomputation on full data should match report train/test
  {
    MetaExitCalibrator<DT> calAll(cph);
    const auto combAll =
        calAll.evaluateCombinedPolicy(expF_H, expB_H, expH_H,
                                      optsHit.getThresholdR(),
                                      optsHit.getEpsilonR(),
                                      FailureExitFill::OpenOfKPlus1);

    REQUIRE(repHit.getTrainCombined().getAvgPnL_R()     == Catch::Approx(combAll.getAvgPnL_R()).margin(kAbsTol));
    REQUIRE(repHit.getTrainCombined().getHitRate()      == Catch::Approx(combAll.getHitRate()).epsilon(1e-12));
    REQUIRE(repHit.getTrainCombined().getAvgBarsHeld()  == Catch::Approx(combAll.getAvgBarsHeld()).epsilon(1e-12));

    REQUIRE(repHit.getTestCombined().getAvgPnL_R()      == Catch::Approx(combAll.getAvgPnL_R()).margin(kAbsTol));
    REQUIRE(repHit.getTestCombined().getHitRate()       == Catch::Approx(combAll.getHitRate()).epsilon(1e-12));
    REQUIRE(repHit.getTestCombined().getAvgBarsHeld()   == Catch::Approx(combAll.getAvgBarsHeld()).epsilon(1e-12));
  }
}

TEST_CASE("ExitPolicyJointAutoTuner: PnLPerBar objective chooses (K,N,H) consistent with throughput",
          "[ExitPolicyJointAutoTuner][objective][PnLPerBar]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  // Optimize using the new PnLPerBar objective
  ExitTunerOptions<DT> optsPPB(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    1.0,   // train == test on report
    /*embargoTrades*/    0,
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::PnLPerBar
  );

  ExitPolicyJointAutoTuner<DT> jtPPB(cph, optsPPB);
  auto repPPB = jtPPB.tuneJoint();

  // Independent 3-D arg-max under PnLPerBar on the SAME grids
  auto [expF_P, expB_P, expH_P, bestTrain_P] =
      argmaxFNH<DT>(cph,
                    repPPB.getFailureToPerformGrid(),
                    repPPB.getBreakevenGrid(),
                    repPPB.getMaxHoldGrid(),
                    TuningObjective::PnLPerBar,
                    optsPPB.getThresholdR(),
                    optsPPB.getEpsilonR());

  // The chosen triple must match the independent arg-max
  REQUIRE(repPPB.getFailureToPerformBars()    == expF_P);
  REQUIRE(repPPB.getBreakevenActivationBars() == expB_P);
  REQUIRE(repPPB.getMaxHoldBars()             == expH_P);

  // Verify report metrics match calibrator on full data (train==test here)
  {
    MetaExitCalibrator<DT> calAll(cph);
    const auto combAll =
        calAll.evaluateCombinedPolicy(expF_P, expB_P, expH_P,
                                      optsPPB.getThresholdR(),
                                      optsPPB.getEpsilonR(),
                                      FailureExitFill::OpenOfKPlus1);

    REQUIRE(repPPB.getTrainCombined().getAvgPnL_R()     == Catch::Approx(combAll.getAvgPnL_R()).margin(kAbsTol));
    REQUIRE(repPPB.getTrainCombined().getHitRate()      == Catch::Approx(combAll.getHitRate()).epsilon(1e-12));
    REQUIRE(repPPB.getTrainCombined().getAvgBarsHeld()  == Catch::Approx(combAll.getAvgBarsHeld()).epsilon(1e-12));

    REQUIRE(repPPB.getTestCombined().getAvgPnL_R()      == Catch::Approx(combAll.getAvgPnL_R()).margin(kAbsTol));
    REQUIRE(repPPB.getTestCombined().getHitRate()       == Catch::Approx(combAll.getHitRate()).epsilon(1e-12));
    REQUIRE(repPPB.getTestCombined().getAvgBarsHeld()   == Catch::Approx(combAll.getAvgBarsHeld()).epsilon(1e-12));
  }

  // Sanity: H is from the H-grid and within plausible bounds
  const auto& Hgrid = repPPB.getMaxHoldGrid();
  REQUIRE(!Hgrid.empty());
  REQUIRE(std::find(Hgrid.begin(), Hgrid.end(), repPPB.getMaxHoldBars()) != Hgrid.end());
  REQUIRE(repPPB.getMaxHoldBars() >= 0);
  REQUIRE(repPPB.getMaxHoldBars() <= 2); // our synthetic paths have 2 post-entry bars
}

TEST_CASE("ExitPolicyJointAutoTuner: MaxHold H is valid and within grid",
          "[ExitPolicyJointAutoTuner][H-basic]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  ExitTunerOptions<DT> opts(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    1.0,
    /*embargoTrades*/    0,
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::AvgPnL_R
  );

  ExitPolicyJointAutoTuner<DT> jt(cph, opts);
  auto rep = jt.tuneJoint();

  const auto& Hgrid = rep.getMaxHoldGrid();

  REQUIRE(!Hgrid.empty());
  REQUIRE(std::find(Hgrid.begin(), Hgrid.end(), rep.getMaxHoldBars()) != Hgrid.end());

  // With maxBarsToAnalyze=3 and our synthetic paths having 2 post-entry bars,
  // the valid bar-age indices are 0..2. Ensure H is within that range.
  REQUIRE(rep.getMaxHoldBars() >= 0);
  REQUIRE(rep.getMaxHoldBars() <= 2);
}
