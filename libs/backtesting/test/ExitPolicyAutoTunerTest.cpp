// ExitPolicyAutoTunerTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <algorithm>
#include <vector>

#include "ExitPolicyAutoTuner.h"
#include "MetaExitCalibrator.h"   // used to recompute metrics for verification
#include "TradingPosition.h"
#include "ClosedPositionHistory.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using Catch::Approx;

// Reasonable absolute tolerance for double <- decimal conversions
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
// Utilities to mirror tuner logic for verification
// -----------------------------------------------------------------------------
template <class Decimal>
static int argmaxK(const ClosedPositionHistory<Decimal>& cph,
                   const std::vector<int>& Kgrid,
                   TuningObjective obj,
                   const Decimal& thresholdR,
                   PolicyResult& bestOut)
{
  MetaExitCalibrator<Decimal> cal(cph);
  double bestScore = -1e99;
  int bestK = Kgrid.empty() ? 0 : Kgrid.front();
  bestOut = PolicyResult(0.0,0.0,0.0,0);

  for (int K : Kgrid) {
    auto res = cal.evaluateFailureToPerformBars(K, thresholdR, FailureExitFill::OpenOfKPlus1);
    double s = (obj == TuningObjective::HitRate) ? res.getHitRate() : res.getAvgPnL_R();
    if (s > bestScore || (s == bestScore && K < bestK) || (s == bestScore && res.getHitRate() > bestOut.getHitRate())) {
      bestScore = s; bestK = K; bestOut = res;
    }
  }
  return bestK;
}

template <class Decimal>
static int argmaxN(const ClosedPositionHistory<Decimal>& cph,
                   const std::vector<int>& Ngrid,
                   TuningObjective obj,
                   const Decimal& epsilonR,
                   PolicyResult& bestOut)
{
  MetaExitCalibrator<Decimal> cal(cph);
  double bestScore = -1e99;
  int bestN = Ngrid.empty() ? 0 : Ngrid.front();
  bestOut = PolicyResult(0.0,0.0,0.0,0);

  for (int N : Ngrid) {
    auto res = cal.evaluateBreakevenAfterBars(N, epsilonR);
    double s = (obj == TuningObjective::HitRate) ? res.getHitRate() : res.getAvgPnL_R();
    if (s > bestScore || (s == bestScore && N < bestN) || (s == bestScore && res.getHitRate() > bestOut.getHitRate())) {
      bestScore = s; bestN = N; bestOut = res;
    }
  }
  return bestN;
}

// Recreate the exact train/test split used by the tuner
template <class Decimal>
static std::pair<ClosedPositionHistory<Decimal>, ClosedPositionHistory<Decimal>>
replicateSplit(const ClosedPositionHistory<Decimal>& cph,
               double trainFraction,
               int embargoTrades)
{
  std::vector<std::shared_ptr<TradingPosition<Decimal>>> all;
  for (auto it = cph.beginTradingPositions(); it != cph.endTradingPositions(); ++it) {
    all.push_back(it->second);
  }
  const int n = static_cast<int>(all.size());
  ClosedPositionHistory<Decimal> train, test;

  const int cut = std::max(0, std::min(n, static_cast<int>(std::floor(n * trainFraction))));
  const int embargo = std::max(0, std::min(embargoTrades, n));

  for (int i = 0; i < cut; ++i) train.addClosedPosition(all[i]);
  for (int i = cut + embargo; i < n; ++i) test.addClosedPosition(all[i]);

  return {train, test};
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------
TEST_CASE("ExitPolicyAutoTuner: options construction & getters", "[ExitPolicyAutoTuner][options]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  ExitTunerOptions<DT> opts(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    0.70,
    /*embargoTrades*/    1,
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::AvgPnL_R
  );

  REQUIRE(opts.getMaxBarsToAnalyze() == 3);
  REQUIRE(opts.getTrainFraction()    == Approx(0.70));
  REQUIRE(opts.getEmbargoTrades()    == 1);
  REQUIRE(opts.getThresholdR()       == Z);
  REQUIRE(opts.getEpsilonR()         == Z);
  REQUIRE(opts.getFracNonPosHigh()   == Approx(0.65));
  REQUIRE(opts.getTargetHazardLow()  == Approx(0.20));
  REQUIRE(opts.getAlphaMfeR()        == Approx(0.33));
  REQUIRE(opts.getNeighborSpan()     == 1);
  REQUIRE(opts.getUseFullGridIfEmpty() == true);
  REQUIRE(opts.getObjective()        == TuningObjective::AvgPnL_R);
}

TEST_CASE("ExitPolicyAutoTuner: end-to-end tuning with full data (train==test)", "[ExitPolicyAutoTuner][full]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  ExitTunerOptions<DT> opts(
    /*maxBarsToAnalyze*/ 3,
    /*trainFraction*/    1.0,   // use all for train; test == train in report
    /*embargoTrades*/    0,
    /*thresholdR*/       Z,     // failure to perform if PnL_R <= 0
    /*epsilonR*/         Z,     // pure breakeven
    /*fracNonPosHigh*/   0.65,
    /*targetHazardLow*/  0.20,
    /*alphaMfeR*/        0.33,
    /*neighborSpan*/     1,
    /*useFullGridIfEmpty*/ true,
    /*objective*/        TuningObjective::AvgPnL_R
  );

  ExitPolicyAutoTuner<DT> tuner(cph, opts);
  auto report = tuner.tune();

  // Report sanity
  REQUIRE(!report.getKgrid().empty());
  REQUIRE(!report.getNgrid().empty());
  REQUIRE(report.getK() >= 0);
  REQUIRE(report.getN() >= 0);

  // Verify chosen K and N are argmax over the same grids & objective
  PolicyResult bestK_train(0.0,0.0,0.0,0), bestN_train(0.0,0.0,0.0,0);
  const int expectedK = argmaxK(cph, report.getKgrid(), opts.getObjective(), opts.getThresholdR(), bestK_train);
  const int expectedN = argmaxN(cph, report.getNgrid(), opts.getObjective(), opts.getEpsilonR(), bestN_train);
  REQUIRE(report.getK() == expectedK);
  REQUIRE(report.getN() == expectedN);

  // And the train/test metrics should match the calibrator’s for those K/N (train==test here)
  MetaExitCalibrator<DT> cal(cph);
  const auto kAll = cal.evaluateFailureToPerformBars(report.getK(), opts.getThresholdR(), FailureExitFill::OpenOfKPlus1);
  const auto nAll = cal.evaluateBreakevenAfterBars(report.getN(), opts.getEpsilonR());

  REQUIRE(report.getTrainK().getAvgPnL_R() == Approx(kAll.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(report.getTrainK().getHitRate()  == Approx(kAll.getHitRate()).epsilon(1e-12));
  REQUIRE(report.getTrainK().getAvgBarsHeld() == Approx(kAll.getAvgBarsHeld()).epsilon(1e-12));

  REQUIRE(report.getTrainN().getAvgPnL_R() == Approx(nAll.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(report.getTrainN().getHitRate()  == Approx(nAll.getHitRate()).epsilon(1e-12));
  REQUIRE(report.getTrainN().getAvgBarsHeld() == Approx(nAll.getAvgBarsHeld()).epsilon(1e-12));

  REQUIRE(report.getTestK().getAvgPnL_R() == Approx(kAll.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(report.getTestN().getAvgPnL_R() == Approx(nAll.getAvgPnL_R()).margin(kAbsTol));
}

TEST_CASE("ExitPolicyAutoTuner: objective controls selection (AvgPnL_R vs HitRate)", "[ExitPolicyAutoTuner][objective]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  auto cph = makeSyntheticCPH<DT>();

  // First run: optimize AvgPnL_R
  ExitTunerOptions<DT> optsAvg(
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
  ExitPolicyAutoTuner<DT> tunerAvg(cph, optsAvg);
  auto repAvg = tunerAvg.tune();

  // Second run: optimize HitRate
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
  ExitPolicyAutoTuner<DT> tunerHit(cph, optsHit);
  auto repHit = tunerHit.tune();

  // Independently compute argmax under each objective using the SAME grids
  PolicyResult tmp(0.0,0.0,0.0,0);
  const int kAvg = argmaxK(cph, repAvg.getKgrid(), TuningObjective::AvgPnL_R, optsAvg.getThresholdR(), tmp);
  const int kHit = argmaxK(cph, repHit.getKgrid(), TuningObjective::HitRate,  optsHit.getThresholdR(), tmp);
  const int nAvg = argmaxN(cph, repAvg.getNgrid(), TuningObjective::AvgPnL_R, optsAvg.getEpsilonR(), tmp);
  const int nHit = argmaxN(cph, repHit.getNgrid(), TuningObjective::HitRate,  optsHit.getEpsilonR(), tmp);

  REQUIRE(repAvg.getK() == kAvg);
  REQUIRE(repHit.getK() == kHit);
  REQUIRE(repAvg.getN() == nAvg);
  REQUIRE(repHit.getN() == nHit);
}

TEST_CASE("ExitPolicyAutoTuner: train/test split with embargo (exact verification)", "[ExitPolicyAutoTuner][split]")
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

  ExitPolicyAutoTuner<DT> tuner(cph, opts);
  auto report = tuner.tune();

  // Recreate the exact split used by the tuner and recompute test-side metrics
  auto [train, test] = replicateSplit(cph, opts.getTrainFraction(), opts.getEmbargoTrades());
  MetaExitCalibrator<DT> calTest(test);

  const auto kTest = calTest.evaluateFailureToPerformBars(report.getK(),
                                                          opts.getThresholdR(),
                                                          FailureExitFill::OpenOfKPlus1);
  const auto nTest = calTest.evaluateBreakevenAfterBars(report.getN(), opts.getEpsilonR());

  // Now assert the report’s test metrics match the recomputed ones exactly
  REQUIRE(report.getTestK().getAvgPnL_R()    == Approx(kTest.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(report.getTestK().getHitRate()     == Approx(kTest.getHitRate()).epsilon(1e-12));
  REQUIRE(report.getTestK().getAvgBarsHeld() == Approx(kTest.getAvgBarsHeld()).epsilon(1e-12));
  REQUIRE(report.getTestK().getTrades()      == kTest.getTrades());

  REQUIRE(report.getTestN().getAvgPnL_R()    == Approx(nTest.getAvgPnL_R()).margin(kAbsTol));
  REQUIRE(report.getTestN().getHitRate()     == Approx(nTest.getHitRate()).epsilon(1e-12));
  REQUIRE(report.getTestN().getAvgBarsHeld() == Approx(nTest.getAvgBarsHeld()).epsilon(1e-12));
  REQUIRE(report.getTestN().getTrades()      == nTest.getTrades());
}
