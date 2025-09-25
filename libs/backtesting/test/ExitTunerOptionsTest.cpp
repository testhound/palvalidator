// ExitTunerOptionsTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestUtils.h"
#include "ExitPolicyAutoTuner.h"   // brings in ExitTunerOptions / TuningObjective
#include "DecimalConstants.h"

using namespace mkc_timeseries;
using Catch::Approx;

TEST_CASE("ExitTunerOptions: defaulted-parameter constructor yields expected defaults",
          "[ExitTunerOptions][defaults]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  // Only required parameter: maxBarsToAnalyze
  const int maxBars = 8;
  ExitTunerOptions<DT> opts(maxBars);

  // Defaults we agreed on
  REQUIRE(opts.getMaxBarsToAnalyze() == maxBars);
  REQUIRE(opts.getTrainFraction()    == Approx(0.70).epsilon(1e-12));
  REQUIRE(opts.getEmbargoTrades()    == 5);
  REQUIRE(opts.getThresholdR()       == Z);
  REQUIRE(opts.getEpsilonR()         == Z);
  REQUIRE(opts.getFracNonPosHigh()   == Approx(0.65).epsilon(1e-12));
  REQUIRE(opts.getTargetHazardLow()  == Approx(0.20).epsilon(1e-12));
  REQUIRE(opts.getAlphaMfeR()        == Approx(0.33).epsilon(1e-12));
  REQUIRE(opts.getNeighborSpan()     == 1);
  REQUIRE(opts.getUseFullGridIfEmpty() == true);
  REQUIRE(opts.getObjective()        == TuningObjective::AvgPnL_R);
}

TEST_CASE("ExitTunerOptions: selective overrides work with default tail parameters",
          "[ExitTunerOptions][overrides]")
{
  using DT = DecimalType;
  const auto Z = DecimalConstants<DT>::DecimalZero;

  // Override a couple of knobs; keep the rest at defaults.
  ExitTunerOptions<DT> opts(
    /*maxBarsToAnalyze*/ 10,
    /*trainFraction*/    0.80,
    /*embargoTrades*/    1,
    /*thresholdR*/       Z,
    /*epsilonR*/         Z,
    /*fracNonPosHigh*/   0.65,   // default retained
    /*targetHazardLow*/  0.20,   // default retained
    /*alphaMfeR*/        0.33,   // default retained
    /*neighborSpan*/     1,      // default retained
    /*useFullGridIfEmpty*/ true, // default retained
    /*objective*/        TuningObjective::HitRate // override
  );

  REQUIRE(opts.getMaxBarsToAnalyze() == 10);
  REQUIRE(opts.getTrainFraction()    == Approx(0.80).epsilon(1e-12));
  REQUIRE(opts.getEmbargoTrades()    == 1);
  REQUIRE(opts.getObjective()        == TuningObjective::HitRate);

  // Unchanged defaults remain intact
  REQUIRE(opts.getThresholdR()       == Z);
  REQUIRE(opts.getEpsilonR()         == Z);
  REQUIRE(opts.getFracNonPosHigh()   == Approx(0.65).epsilon(1e-12));
  REQUIRE(opts.getTargetHazardLow()  == Approx(0.20).epsilon(1e-12));
  REQUIRE(opts.getAlphaMfeR()        == Approx(0.33).epsilon(1e-12));
  REQUIRE(opts.getNeighborSpan()     == 1);
  REQUIRE(opts.getUseFullGridIfEmpty() == true);
}
