#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "StopLoss.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

TEST_CASE ("ProfitTarget operations", "[ProfitTarget]")
{
  using namespace dec;

  NullStopLoss<DecimalType> noStopLoss;
  DecimalType stop1(fromString<DecimalType>("117.4165"));
  DecimalType stop2(fromString<DecimalType>("117.3659"));
  LongStopLoss<DecimalType> longStopLoss1(stop1);
  ShortStopLoss<DecimalType> shortStopLoss1(stop2);

  SECTION ("StopLoss constructor tests 1");
  {
    REQUIRE (longStopLoss1.getStopLoss() == stop1);
    REQUIRE (shortStopLoss1.getStopLoss() == stop2);
    REQUIRE_THROWS (noStopLoss.getStopLoss());
  }

  SECTION ("StopLoss constructor tests 2");
  {
    DecimalType entry1(fromString<DecimalType>("117.00"));
    DecimalType stopReference(fromString<DecimalType>("116.5203"));

    PercentNumber<DecimalType> percStop1 = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.41"));

    LongStopLoss<DecimalType> stopPrice2 (entry1, percStop1);

    REQUIRE (stopPrice2.getStopLoss() == stopReference);
  }

  SECTION ("StopLoss constructor tests 3");
  {
    DecimalType entry1(fromString<DecimalType>("117.00"));
    DecimalType stopReference(fromString<DecimalType>("117.4797"));

    PercentNumber<DecimalType> percStop1 = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.41"));

    ShortStopLoss<DecimalType> stopPrice2 (entry1, percStop1);
    REQUIRE (stopPrice2.getStopLoss() == stopReference);
  }

  SECTION ("NullStopLoss attributes");
  {
    REQUIRE (noStopLoss.isNullStopLoss() == true);
    REQUIRE (noStopLoss.isLongStopLoss() == false);
    REQUIRE (noStopLoss.isShortStopLoss() == false);
  }

  SECTION ("LongStopLoss attributes");
  {
    REQUIRE (longStopLoss1.isNullStopLoss() == false);
    REQUIRE (longStopLoss1.isLongStopLoss() == true);
    REQUIRE (longStopLoss1.isShortStopLoss() == false);
  }

  SECTION ("ShortStopLoss attributes");
  {
    REQUIRE (shortStopLoss1.isNullStopLoss() == false);
    REQUIRE (shortStopLoss1.isLongStopLoss() == false);
    REQUIRE (shortStopLoss1.isShortStopLoss() == true);
  }
}

